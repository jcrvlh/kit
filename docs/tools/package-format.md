# Formato do Pacote de Distribuição (`.kit`)

O arquivo `.kit` é o contêiner de distribuição de uma Tool para o usuário final.

---

## 🗜️ Estrutura do Pacote (Arquivo ZIP)

Um pacote `.kit` é um arquivo ZIP padrão contendo:

```text
meu-jogo.kit
├── manifest.json         # Metadados, requisitos e hashes SHA-256 dos artefatos
├── tool.elf              # Executável Xtensa relocável
├── icon.bin              # Imagem do ícone para o Launcher (LVGL image format)
├── signature.bin         # Assinatura Ed25519 (64 B) do manifest.json — só em pacotes do catálogo
└── assets/               # Diretório opcional de recursos estáticos
    ├── sprites.bin
    └── sound_roll.pcm
```

O `manifest.json` carrega o campo `checksum` com o hash SHA-256 de `tool.elf` e um
hash por asset. A `signature.bin` assina os bytes canônicos do `manifest.json`,
fechando a cadeia de confiança sobre o pacote inteiro. Pacotes sem `signature.bin`
só são instaláveis na trilha **Sideload** (Modo Desenvolvedor).

---

## 🔒 Processo de Instalação pelo Tool Manager

1. O arquivo `.kit` é copiado para uma área temporária.
2. O Tool Manager extrai `manifest.json` e, se presente, `signature.bin`.
3. **Autenticidade:** se houver `signature.bin`, a assinatura Ed25519 é verificada
   contra as chaves públicas de release embutidas no firmware. Falha → instalação
   abortada. Sem `signature.bin` → só prossegue com Modo Desenvolvedor ligado.
4. **Integridade:** é calculada a soma SHA-256 de `tool.elf` e de cada asset e
   comparada com os hashes declarados no `manifest.json`.
5. **Compatibilidade:** `min_runtime <= runtime_version <= max_runtime` e `arch`.
6. O pacote é extraído para `/tools/<tool_id>/`.
7. O Tool Manager adiciona o registro da Tool ao `.installed_registry.json`.
8. O Launcher atualiza a lista de ícones na tela principal.

O hash de `tool.elf` é reconferido a cada carregamento, antes da relocação em PSRAM.
