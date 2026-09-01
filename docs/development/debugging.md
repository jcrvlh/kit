# Depuração e Diagnósticos

Guia para análise de logs, rastreamento de falhas e diagnósticos no KIT.

---

## 📜 Logs do Sistema

O KIT utiliza as macros de log padronizadas do ESP-IDF:
- `ESP_LOGI(TAG, ...)`: Mensagens informativas.
- `ESP_LOGW(TAG, ...)`: Avisos e alertas não-críticos.
- `ESP_LOGE(TAG, ...)`: Erros de execução ou falhas de hardware.

---

## 🔍 Análise de Crash Dumps

Em caso de exceção de hardware (ex: `LoadProhibited`, `InstructionFetchError` ou estouro de pilha):
1. O ESP-IDF imprime o **Backtrace** na porta serial:
   ```text
   Backtrace:0x42001a2b:0x3fca0110 0x420023c4:0x3fca0130 ...
   ```
2. O monitor serial (`idf.py monitor`) decodifica automaticamente os endereços para nomes de funções e linhas de código fonte caso a tabela de símbolos (`.elf`) esteja presente.
