# Gravação do Firmware (Flash)

Como gravar o firmware compilado na placa Waveshare ESP32-S3 Touch AMOLED 1.8.

---

## ⚡ Conexão USB

1. Conecte a placa ao computador utilizando um cabo USB-C de boa qualidade.
2. Verifique a porta serial atribuída:
   - **Linux:** `/dev/ttyUSB0` ou `/dev/ttyACM0`
   - **macOS:** `/dev/cu.usbmodem...` ou `/dev/cu.wchusbserial...`
   - **Windows:** `COM3`, `COM4`, etc.

---

## 🚀 Comando de Gravação

Grave o firmware e abra o monitor serial imediatamente:

```bash
cd firmware
idf.py -p /dev/ttyACM0 flash monitor
```

*(Substitua `/dev/ttyACM0` pela porta serial correta do seu sistema).*

Para sair do monitor serial do ESP-IDF, pressione `Ctrl + ]`.
