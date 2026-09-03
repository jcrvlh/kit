#include "kit_comms.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

// Forward declaration for tool manager catalog reload
extern void kit_tool_manager_reload_catalog(void);

static const char *TAG = "KIT_COMMS";

#define LINE_BUF_SIZE 256
#define RX_BUF_SIZE 2048

static char s_current_tool_id[64] = {0};

static void send_reply(const char *msg)
{
    printf("%s\n", msg);
    fflush(stdout);
}

static void ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        mkdir(path, 0777);
    }
}

static void comms_task(void *arg)
{
    ESP_LOGI(TAG, "Serial Comms listener iniciado na porta stdin.");

    char line_buf[LINE_BUF_SIZE];
    uint8_t rx_buf[RX_BUF_SIZE];

    while (1) {
        // Leitura de linha com fgets()
        if (fgets(line_buf, sizeof(line_buf), stdin) == NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Remove \r e \n
        line_buf[strcspn(line_buf, "\r\n")] = 0;

        if (strlen(line_buf) == 0) {
            continue;
        }

        // 1. KIT_TOOL_BEGIN <tool_id>
        if (strncmp(line_buf, "KIT_TOOL_BEGIN ", 15) == 0) {
            char *tool_id = line_buf + 15;
            if (strlen(tool_id) == 0 || strlen(tool_id) >= sizeof(s_current_tool_id)) {
                send_reply("ERR: Invalid tool_id");
                continue;
            }
            strncpy(s_current_tool_id, tool_id, sizeof(s_current_tool_id) - 1);
            
            // Garante o diretório raiz e o diretório da tool
            ensure_dir("/sdcard/tools");
            char path[256];
            snprintf(path, sizeof(path), "/sdcard/tools/%s", s_current_tool_id);
            ensure_dir(path);
            
            ESP_LOGI(TAG, "Iniciando recebimento da Tool: %s", s_current_tool_id);
            send_reply("OK");
        }
        // 2. KIT_FILE_WRITE <filename> <size>
        else if (strncmp(line_buf, "KIT_FILE_WRITE ", 15) == 0) {
            if (s_current_tool_id[0] == '\0') {
                send_reply("ERR: No tool session active");
                continue;
            }

            char filename[64];
            int size = 0;
            if (sscanf(line_buf + 15, "%63s %d", filename, &size) != 2 || size < 0) {
                send_reply("ERR: Invalid arguments");
                continue;
            }

            char path[256];
            snprintf(path, sizeof(path), "/sdcard/tools/%s/%s", s_current_tool_id, filename);

            ESP_LOGI(TAG, "Recebendo arquivo %s (%d bytes)", path, size);
            
            // Manda OK para que o host comece a enviar o binário
            send_reply("OK");

            FILE *f = fopen(path, "wb");
            if (!f) {
                ESP_LOGE(TAG, "Erro ao abrir %s", path);
                send_reply("ERR: Cannot open file");
                continue;
            }

            int remaining = size;
            bool error = false;

            while (remaining > 0) {
                int to_read = (remaining > RX_BUF_SIZE) ? RX_BUF_SIZE : remaining;
                // fread bloqueia até ler to_read bytes ou EOF
                int n = fread(rx_buf, 1, to_read, stdin);
                if (n > 0) {
                    fwrite(rx_buf, 1, n, f);
                    remaining -= n;
                } else if (n <= 0 && ferror(stdin)) {
                    error = true;
                    break;
                }
                // Yield para evitar watchdog trigger em transferências longas
                vTaskDelay(pdMS_TO_TICKS(1));
            }

            fclose(f);

            if (error) {
                send_reply("ERR: Transfer failed");
            } else {
                send_reply("OK");
            }
        }
        // 3. KIT_TOOL_COMMIT
        else if (strcmp(line_buf, "KIT_TOOL_COMMIT") == 0) {
            ESP_LOGI(TAG, "Tool %s commitada.", s_current_tool_id);
            s_current_tool_id[0] = '\0';
            
            // Recarrega o catálogo para atualizar a home
            kit_tool_manager_reload_catalog();
            
            send_reply("OK");
        }
        else {
            // Outras saídas ou sujeira no terminal (pula)
        }
    }
}

kit_err_t kit_comms_init(void)
{
    // Aumentar o tamanho do buffer de stdin para transferências mais rápidas (opcional, mas recomendado)
    setvbuf(stdin, NULL, _IOFBF, RX_BUF_SIZE * 2);

    xTaskCreatePinnedToCore(
        comms_task, 
        "kit_comms", 
        8192, 
        NULL, 
        3, 
        NULL, 
        1  // Preferencialmente no core 1, já que o LVGL está no core 1 ou 0
    );

    return KIT_OK;
}
