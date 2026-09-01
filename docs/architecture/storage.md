# Estratégia de Armazenamento e Particionamento

O KIT utiliza **16 MB de memória Flash QSPI** e **8 MB de memória PSRAM Octal**.

---

## 🗺️ Tabela de Partições (16 MB Flash)

A tabela de partições (`partitions.csv`) foi projetada para suportar atualizações seguras de firmware (OTA com rollback duplo), imagem de recuperação de fábrica e espaço dedicado para instalação de dezenas de Tools em LittleFS.

```csv
# Name,      Type, SubType,  Offset,    Size,       Flags
nvs,         data, nvs,      0x9000,    0x6000,     # 24 KB (WiFi, configurações base)
otadata,     data, ota,      0xF000,    0x2000,     # 8 KB (Controle de boot OTA)
phy_init,    data, phy,      0x11000,   0x1000,     # 4 KB (Calibração RF)
factory,     app,  factory,  0x20000,   0x200000,   # 2.0 MB (Imagem de Recovery/Fábrica)
ota_0,       app,  ota_0,    0x220000,  0x300000,   # 3.0 MB (Slot de Firmware A)
ota_1,       app,  ota_1,    0x520000,  0x300000,   # 3.0 MB (Slot de Firmware B)
tools,       data, littlefs, 0x820000,  0x700000,   # 7.0 MB (Armazenamento de Tools)
config,      data, nvs,      0xF20000,  0x10000,    # 64 KB (Configurações de Tools)
coredump,    data, coredump, 0xF30000,  0x10000,    # 64 KB (Relatórios de Crash)
```

**Total Alocado:** ~15.3 MB / 16.0 MB

---

## 📂 Estrutura do Sistema de Arquivos LittleFS (`/tools`)

A partição `tools` é formatada em **LittleFS**, oferecendo wear-leveling dinâmico e proteção total contra perda de energia (*power-loss safe*).

```text
/tools
├── .installed_registry.json     # Índice rápido de Tools instaladas
├── com.kit.dice/
│   ├── manifest.json
│   ├── tool.elf
│   ├── icon.bin
│   ├── assets/
│   │   └── dice_faces.bin
│   └── data/                    # Espaço de dados persistentes da Tool
│       └── stats.json
└── com.kit.bingo/
    ├── manifest.json
    ├── tool.elf
    └── ...
```
