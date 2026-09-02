#include "kit_display.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_co5300.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "KIT_DISPLAY";

// Fonte de tick do LVGL v9: sem isso, lv_tick_get() nunca avança e nenhum
// timer interno do LVGL (incluindo o polling de touch do kit_input) roda
// mais de uma vez após a inicialização.
static uint32_t lvgl_tick_get_cb(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static lv_display_t *s_disp = NULL;
static esp_lcd_panel_handle_t s_panel_handle = NULL;
static esp_lcd_panel_io_handle_t s_io_handle = NULL;
static uint8_t s_brightness = 80;
static bool s_display_on = true;

// Tamanho do buffer de desenho: 368 x 40 linhas em RGB565 (2 bytes por pixel)
#define BUFFER_LINES 40
#define BUFFER_SIZE (KIT_DISPLAY_WIDTH * BUFFER_LINES * sizeof(lv_color16_t))

static uint8_t *s_buf1 = NULL;
static uint8_t *s_buf2 = NULL;

// O CO5300 em QSPI não recebe o comando DCS "cru": o driver esp_lcd_co5300
// envia todo comando como um endereço de 32 bits — (0x02 << 24) | (cmd << 8) —
// e é assim que o painel espera. As chamadas a esp_lcd_panel_io_tx_param() aqui
// (0x51 brilho, 0x53 control display, 0x55 CABC) estavam passando o byte do
// comando direto, então nunca chegavam ao painel — era por isso que o slider
// de brilho não tinha efeito nenhum.
#define CO5300_QSPI_CMD(cmd)  ((int)((0x02UL << 24) | ((uint32_t)(cmd) << 8)))

static esp_err_t co5300_write_cmd(uint8_t cmd, const uint8_t *param, size_t len)
{
    if (!s_io_handle) return ESP_ERR_INVALID_STATE;
    return esp_lcd_panel_io_tx_param(s_io_handle, CO5300_QSPI_CMD(cmd), param, len);
}

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    if (disp) {
        lv_display_flush_ready(disp);
    }
    return false;
}

// O CO5300 exige que a janela de endereçamento (CASET/RASET) comece em pixel
// par e termine em ímpar — ou seja, x e largura pares. Se o LVGL manda uma área
// com x1 ímpar (ou largura ímpar), o painel embaralha o fluxo QSPI e o conteúdo
// sai "rasgado"/cortado na diagonal. Como o LVGL invalida a caixa exata de cada
// label, isso aparecia quando um texto mudava de largura: o "%d%%" do brilho e
// do volume ao passar de 99% para 100%, o contador "PESSOA N" do Times, etc.
// Aqui esticamos a área invalidada para fora até coordenadas pares/ímpares.
static void lvgl_round_area_cb(lv_event_t *e)
{
    lv_area_t *area = lv_event_get_invalidated_area(e);
    area->x1 &= ~1;
    area->y1 &= ~1;
    area->x2 |= 1;
    area->y2 |= 1;
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if (s_panel_handle) {
        uint32_t len = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
        lv_draw_sw_rgb565_swap(px_map, len);
        esp_err_t e = esp_lcd_panel_draw_bitmap(s_panel_handle, area->x1, area->y1,
                                                area->x2 + 1, area->y2 + 1, px_map);
        // Se a transferência nem chegou a ser enfileirada (fila cheia / disputa
        // de barramento sob carga de Wi-Fi), o callback de "done" NUNCA vai vir e
        // o LVGL ficaria preso pra sempre em wait_for_flushing. Libera na mão.
        if (e != ESP_OK) {
            ESP_LOGW(TAG, "draw_bitmap falhou (%s) — liberando o flush", esp_err_to_name(e));
            lv_display_flush_ready(disp);
        }
    } else {
        lv_display_flush_ready(disp);
    }
}


kit_err_t kit_display_init(void)
{
    ESP_LOGI(TAG, "Inicializando Display AMOLED Waveshare 1.8\" CO5300 (368x448 QSPI)...");

    // 1. Configuração do barramento SPI em modo QSPI (Quad-SPI)
    spi_bus_config_t buscfg = CO5300_PANEL_BUS_QSPI_CONFIG(
        KIT_LCD_SCLK,
        KIT_LCD_SDIO0,
        KIT_LCD_SDIO1,
        KIT_LCD_SDIO2,
        KIT_LCD_SDIO3,
        BUFFER_SIZE
    );
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar barramento QSPI: %s", esp_err_to_name(ret));
        return KIT_FAIL;
    }

    // 2. Aloca buffers de desenho em PSRAM. A RAM interna (~232 KB DIRAM) é
    //    disputada por Wi-Fi (~45 KB), TLS e, principalmente, a relocação do
    //    .so das Tools (o ELF loader precisa de um bloco contíguo em IRAM). Pôr
    //    59 KB de framebuffer aqui deixava o catálogo (mbedtls_ssl_setup ->
    //    -0x7F00 ALLOC_FAILED) e o "abrir Tool" sem bloco grande o bastante.
    //    O DMA do QSPI lê da PSRAM sem problema; o risco de disputa com o Wi-Fi
    //    é tratado no lvgl_flush_cb (checa o retorno e não trava o LVGL).
    s_buf1 = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_buf2 = heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_buf1 || !s_buf2) {
        ESP_LOGE(TAG, "Falha ao alocar buffers de renderização em PSRAM!");
        return KIT_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Buffers de renderização alocados em PSRAM: 2x %d bytes", (int)BUFFER_SIZE);

    // 3. Inicializa LVGL v9
    lv_init();
    lv_tick_set_cb(lvgl_tick_get_cb);

    // 4. Cria e configura o display no LVGL
    s_disp = lv_display_create(KIT_DISPLAY_WIDTH, KIT_DISPLAY_HEIGHT);
    if (!s_disp) {
        ESP_LOGE(TAG, "Falha ao registrar display no LVGL!");
        return KIT_ERR_NO_MEM;
    }

    lv_display_set_buffers(s_disp, s_buf1, s_buf2, BUFFER_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, lvgl_flush_cb);
    lv_display_add_event_cb(s_disp, lvgl_round_area_cb, LV_EVENT_INVALIDATE_AREA, NULL);

    // 5. Configuração do Painel IO do CO5300 via QSPI
    esp_lcd_panel_io_spi_config_t io_config = CO5300_PANEL_IO_QSPI_CONFIG(
        KIT_LCD_CS,
        notify_lvgl_flush_ready,
        s_disp
    );
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &s_io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar Painel IO QSPI: %s", esp_err_to_name(ret));
        return KIT_FAIL;
    }

    // 6. Configuração e inicialização do driver do painel CO5300
    co5300_vendor_config_t vendor_config = {
        .init_cmds = NULL,
        .init_cmds_size = 0,
        .flags = {
            .use_qspi_interface = 1,
        },
    };

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = KIT_LCD_RST,
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };

    ret = esp_lcd_new_panel_co5300(s_io_handle, &panel_config, &s_panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar painel CO5300: %s", esp_err_to_name(ret));
        return KIT_FAIL;
    }

    // Reset, init e ativação da tela
    esp_lcd_panel_reset(s_panel_handle);
    esp_lcd_panel_init(s_panel_handle);
    esp_lcd_panel_set_gap(s_panel_handle, 16, 0); // Offset X padrão CO5300 1.8"
    esp_lcd_panel_disp_on_off(s_panel_handle, true);

    // CO5300: habilita o controle de brilho por software antes de qualquer
    // 0x51. O "Write Display Brightness" (0x51) só surte efeito com o bit
    // BCTRL ligado via "Write CTRL Display" (0x53); e o CABC (0x55) precisa
    // ficar desligado, senão o painel sobrepõe o valor pedido pelo slider.
    // Sem isto o 0x51 era aceito mas ignorado — o brilho nunca mudava.
    if (s_io_handle) {
        uint8_t ctrl_display = 0x2C; // BCTRL | DD | BL
        co5300_write_cmd(0x53, &ctrl_display, 1);
        uint8_t cabc_off = 0x00;
        co5300_write_cmd(0x55, &cabc_off, 1);
    }

    // Aplica o brilho padrão (o init do driver liga o painel no brilho máximo)
    kit_display_set_brightness_impl(s_brightness);

    ESP_LOGI(TAG, "Display AMOLED CO5300 inicializado e pronto para renderização.");
    return KIT_OK;
}

uint32_t kit_display_process(void)
{
    return lv_timer_handler();
}

lv_obj_t *kit_display_get_screen_impl(void)
{
    return lv_screen_active();
}

kit_err_t kit_display_refresh_impl(void)
{
    if (s_disp) {
        lv_refr_now(s_disp);
    }
    return KIT_OK;
}

kit_err_t kit_display_set_brightness_impl(uint8_t percentage)
{
    if (percentage > 100) percentage = 100;
    s_brightness = percentage;
    ESP_LOGI(TAG, "Ajustando brilho do AMOLED para %d%%", percentage);

    if (s_io_handle) {
        // "Write Display Brightness" (0x51) do CO5300, enviado com o
        // enquadramento QSPI correto (ver co5300_write_cmd).
        uint8_t dcs_val = (uint8_t)((percentage * 255) / 100);
        esp_err_t ret = co5300_write_cmd(0x51, &dcs_val, 1);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Falha ao enviar brilho (0x51) para o painel: %s", esp_err_to_name(ret));
            return KIT_FAIL;
        }
    }
    return KIT_OK;
}

uint8_t kit_display_get_brightness_impl(void)
{
    return s_brightness;
}

kit_err_t kit_display_set_on_impl(bool on)
{
    s_display_on = on;
    if (s_panel_handle) {
        esp_lcd_panel_disp_on_off(s_panel_handle, on);
    }
    ESP_LOGI(TAG, "Painel AMOLED %s", on ? "ligado" : "desligado");
    return KIT_OK;
}

bool kit_display_is_on_impl(void)
{
    return s_display_on;
}

