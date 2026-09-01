# Guia de Contribuição para o KIT

Obrigado pelo interesse em contribuir com o **KIT**! Este documento orienta o processo de desenvolvimento e envio de contribuições.

---

## 🧭 Princípios do Projeto

1. **Modularidade Rigorosa:** O KIT Runtime não deve conter código específico de nenhuma Tool de jogo. Tools utilizam exclusivamente a API do KIT.
2. **Abstração de Hardware:** Nenhum componente fora da camada de HAL/Drivers deve interagir diretamente com registradores, barramentos I2C/SPI ou pinos GPIO específicos.
3. **Resiliência e Tolerância a Falhas:** Uma falha em uma Tool nunca deve travar ou comprometer o Runtime do KIT.
4. **Documentação Atualizada:** Toda mudança de comportamento, API ou formato deve atualizar a documentação correspondente em `docs/` no mesmo Pull Request. Decisões arquiteturais relevantes devem ser justificadas na descrição do PR.

---

## 🛠️ Ambiente de Desenvolvimento

* **Framework:** ESP-IDF v5.3 ou superior.
* **Target:** ESP32-S3 (`idf.py set-target esp32s3`).
* **Formatador de Código:** `clang-format` com estilo baseado em LLVM / Espressif.

### Como compilar:
```bash
cd firmware
idf.py build
```

---

## 📝 Como Enviar Contribuições

1. Faça um Fork do repositório.
2. Crie uma branch para a sua alteração: `git checkout -b feature/nome-da-funcionalidade`.
3. Escreva código claro, com comentários explicativos em português ou inglês consistente.
4. Garanta que o projeto compila sem alertas críticos: `idf.py build`.
5. Envie um Pull Request detalhado explicando as motivações e o que foi testado.

---

## ⚖️ Licença das Contribuições

Ao submeter contribuições para este projeto, você concorda que seu trabalho será licenciado sob os termos da [GNU General Public License v3.0](LICENSE).
