# Slot microSD (Expansão Futura)

Especificação da interface para cartões de memória microSD.

---

## 💾 Conexão de Hardware

* **Barramento:** SDMMC no modo de 1-bit (Clock no GPIO 2, CMD no GPIO 1, Data no GPIO 3).
* **Compatibilidade:** Cartões micro SD e micro SDHC formatados em FAT32 ou exFAT.

---

## 🎯 Escopo no KIT

* **Fase Atual (V1 Core):** O slot microSD **NÃO** é obrigatório para a inicialização nem para a operação do KIT Core.
* **Fase Futura:** Será utilizado como partição de armazenamento secundária para arquivamento de dezenas de pacotes `.kit` e assets multimídia pesados.
