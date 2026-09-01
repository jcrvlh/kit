# Compilação e Build do KIT Core

Instruções para compilar o firmware principal do KIT.

---

## ⚙️ Processo de Compilação

1. Abra o terminal no diretório `firmware`:
```bash
cd firmware
```

2. Defina o alvo do microcontrolador para o ESP32-S3:
```bash
idf.py set-target esp32s3
```

3. (Opcional) Ajuste opções de menuconfig se necessário:
```bash
idf.py menuconfig
```

4. Inicie o processo de compilação:
```bash
idf.py build
```

Ao final, os binários de bootloader, tabela de partição e o binário principal `kit_core.bin` estarão disponíveis na pasta `firmware/build/`.
