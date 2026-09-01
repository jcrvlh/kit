# Segurança e Integridade

O modelo de segurança do KIT foi projetado para garantir a integridade do sistema operacional e proteger o dispositivo contra códigos maliciosos ou pacotes corrompidos.

---

## 🔒 Camadas de Proteção

### 1. Validação de Pacotes de Tool (`.kit`)
- **Checksum SHA-256:** O `manifest.json` contém o hash SHA-256 de `tool.so` e de cada asset. O Tool Manager valida os hashes antes de qualquer extração ou carregamento, e reconfere o hash de `tool.so` antes da relocação em PSRAM.
- **Assinatura Ed25519:** Pacotes distribuídos pelo [catálogo](../tools/registry.md) incluem `signature.bin` — assinatura Ed25519 sobre os bytes canônicos do `manifest.json`, que por sua vez cobre `tool.so` e os assets. O firmware embute as chaves públicas de release do projeto e recusa instalar pacotes cuja assinatura não confira.
- **Trilhas de confiança:** *Oficial* e *Comunidade* (ambas assinadas e revisadas) instalam sem atrito; *Sideload* (arquivo `.kit` local, fora do catálogo) só com **Modo Desenvolvedor** ligado e confirmação explícita.

### 2. Recursos Nativos de Hardware (ESP32-S3)
- **Secure Boot V2:** Suporte nativo à verificação por chave pública gravada em eFuses do microcontrolador, garantindo que apenas binários de firmware autorizados possam ser executados pelo bootloader.
- **Flash Encryption:** Criptografia transparente AES-256 da memória Flash, protegendo dados confidenciais (credenciais de Wi-Fi em NVS) contra leitura física.

### 3. Proteção em Execução
- **Controle de Permissões Declarativo:** As Tools declaram no `manifest.json` quais APIs necessitam (`display`, `input`, `random`, `audio`, etc.). O Runtime restringe os ponteiros passados no contexto da Tool às APIs autorizadas.
