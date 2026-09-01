#include "kit_usb_msc.h"
#include "kit_storage.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "tinyusb.h"
#include "tusb.h"
#include "tusb_msc_storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "KIT_USB_MSC";

// Mesmos pinos do slot em kit_storage (SDMMC 1-bit) — ver docs/hardware/microsd.md.
#define KIT_SD_PIN_CLK 2
#define KIT_SD_PIN_CMD 1
#define KIT_SD_PIN_D0  3

static bool s_active = false;
static volatile bool s_host_mounted = false;
static sdmmc_card_t *s_card = NULL;
static uint32_t s_card_mb = 0;

static void msc_mount_changed_cb(tinyusb_msc_event_t *event)
{
    s_host_mounted = event->mount_changed_data.is_mounted;
    ESP_LOGI(TAG, "Computador %s o cartão.", s_host_mounted ? "montou" : "ejetou");
}

// Reabre o cartão como bloco cru (sem VFS) para o TinyUSB. Devolve o handle
// em *out. O host SDMMC deve estar livre (kit_storage_sd_unmount já rodou).
static esp_err_t raw_sd_open(sdmmc_card_t **out)
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = KIT_SD_PIN_CLK;
    slot.cmd = KIT_SD_PIN_CMD;
    slot.d0  = KIT_SD_PIN_D0;
    slot.d1 = GPIO_NUM_NC;
    slot.d2 = GPIO_NUM_NC;
    slot.d3 = GPIO_NUM_NC;
    slot.cd = SDMMC_SLOT_NO_CD;
    slot.wp = SDMMC_SLOT_NO_WP;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_err_t err = (*host.init)();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sdmmc_host_init: %s", esp_err_to_name(err));
        return err;
    }
    err = sdmmc_host_init_slot(host.slot, &slot);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sdmmc_host_init_slot: %s", esp_err_to_name(err));
        sdmmc_host_deinit();
        return err;
    }

    sdmmc_card_t *card = calloc(1, sizeof(sdmmc_card_t));
    if (!card) {
        sdmmc_host_deinit();
        return ESP_ERR_NO_MEM;
    }
    err = sdmmc_card_init(&host, card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sdmmc_card_init: %s", esp_err_to_name(err));
        free(card);
        sdmmc_host_deinit();
        return err;
    }

    *out = card;
    return ESP_OK;
}

static void raw_sd_close(void)
{
    if (s_card) {
        free(s_card);
        s_card = NULL;
    }
    sdmmc_host_deinit();
}

kit_err_t kit_usb_msc_enter(void)
{
    if (s_active) return KIT_OK;

    if (!kit_storage_sd_is_mounted()) {
        ESP_LOGW(TAG, "Nenhum cartão montado — nada a expor.");
        return KIT_ERR_NOT_FOUND;
    }

    // 1. O KIT larga o cartão: desmonta o FATFS e libera o host SDMMC.
    kit_storage_sd_unmount();

    // 2. Reabre o cartão como bloco cru.
    esp_err_t err = raw_sd_open(&s_card);
    if (err != ESP_OK) {
        kit_storage_sd_mount();   // devolve o cartão ao KIT
        return KIT_ERR_STORAGE;
    }

    s_card_mb = (uint32_t)(((uint64_t)s_card->csd.capacity * s_card->csd.sector_size) / (1024 * 1024));
    ESP_LOGI(TAG, "Cartão cru OK: %llu setores de %u B (%lu MB).",
             (unsigned long long)s_card->csd.capacity,
             (unsigned)s_card->csd.sector_size, (unsigned long)s_card_mb);

    // 3. Entrega o cartão ao TinyUSB MSC. O storage começa "não montado no
    //    firmware", ou seja, já exposto ao host — não precisa unmount aqui.
    const tinyusb_msc_sdmmc_config_t msc_cfg = {
        .card = s_card,
        .callback_mount_changed = msc_mount_changed_cb,
    };
    err = tinyusb_msc_storage_init_sdmmc(&msc_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_msc_storage_init_sdmmc: %s", esp_err_to_name(err));
        raw_sd_close();
        kit_storage_sd_mount();
        return KIT_ERR_STORAGE;
    }

    // 4. Sobe o stack USB como device MSC. Isso assume o PHY interno
    //    (GPIO19/20) — o USB-Serial/JTAG some até o próximo reboot.
    const tinyusb_config_t tusb_cfg = { 0 };
    err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tinyusb_driver_install: %s", esp_err_to_name(err));
        tinyusb_msc_storage_deinit();
        raw_sd_close();
        kit_storage_sd_mount();
        return KIT_ERR_STORAGE;
    }

    // O cabo já estava enumerado como USB-Serial/JTAG. Dá um tempo pra task
    // do TinyUSB subir e então pede o pull-up de D+ (reenumeração como MSC).
    vTaskDelay(pdMS_TO_TICKS(150));
    tud_connect();

    s_active = true;
    s_host_mounted = false;
    ESP_LOGW(TAG, "Modo pen drive ATIVO — conecte o KIT no computador.");
    return KIT_OK;
}

void kit_usb_msc_exit_reboot(void)
{
    ESP_LOGW(TAG, "Saindo do modo pen drive — reiniciando.");
    // Não desmonta nada de propósito: o reboot devolve o barramento USB ao
    // console e o kit_storage remonta o cartão do zero na inicialização.
    esp_restart();
}

bool kit_usb_msc_is_active(void)      { return s_active; }
bool kit_usb_msc_host_connected(void) { return s_host_mounted; }
uint32_t kit_usb_msc_card_mb(void)    { return s_card_mb; }
bool kit_usb_msc_usb_ready(void)      { return s_active && tud_mounted(); }
