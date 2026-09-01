# Especificação de Tools no KIT

Uma **Tool** é uma aplicação autocontida, instalável e executada pelo KIT Runtime.

---

## 🧩 Built-in vs. Catálogo

* **Built-in no Core:** conjunto oficial mínimo compilado no firmware
  (`firmware/components/kit_*`) e despachado pelo `kit_tool_manager` a partir do
  `id`. Não passa por instalação de pacote.
* **Tools do catálogo:** distribuídas como pacotes `.kit` e instaladas em
  `/tools/<id>/` via Wi-Fi ou web-installer. Aberto a contribuições da comunidade.

A especificação abaixo trata das Tools do catálogo. Ver
[Catálogo Comunitário de Tools](registry.md).

---

## 🎯 Requisitos de uma Tool

1. **Desacoplamento:** Não deve referenciar drivers ou registradores internos do ESP32-S3. Toda a comunicação ocorre via `kit_api.h`.
2. **Desempenho Gráfico:** A interface deve ser desenhada com LVGL v9 para garantir 60 FPS e fluidez em telas AMOLED.
3. **Respeito aos Recursos:** Ocupar memória PSRAM apenas enquanto estiver em execução e liberar todos os recursos ao ser encerrada.
4. **Empacotamento Padronizado:** Entregue como um arquivo único `.kit` contendo manifesto, binário ELF, ícone e assets.
