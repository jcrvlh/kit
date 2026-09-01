# Modo de Recuperação (Recovery Mode)

O **Modo de Recuperação** é uma camada de segurança independente gravada na partição de fábrica (`factory`) do KIT.

---

## 🆘 Quando o Modo de Recuperação é Acionado

1. **Falha Crítica de Boot:** Se o firmware principal falhar repetidamente durante a inicialização, o bootloader reverte automaticamente para o Factory Recovery.
2. **Acionamento Manual:** Ligue o dispositivo mantendo o dedo pressionado no centro do display por 5 segundos durante a animação de boot inicial.

---

## 🛠️ Opções no Modo de Recuperação

* **Reinstalar Runtime:** Efetua o download da versão estável mais recente do KIT via Wi-Fi.
* **Formatar Armazenamento (/tools):** Apaga Tools corrompidas mantendo as configurações do sistema intactas.
* **Modo USB Flash:** Prepara o microcontrolador para receber um novo firmware completo via cabo USB.
