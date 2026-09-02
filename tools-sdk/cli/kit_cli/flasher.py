"""
kit_cli.flasher — Envio de pacotes .kit para o KIT via porta Serial (Protocolo nativo kit_comms).

Fluxo:
1. Valida o pacote .kit (integridade + manifesto).
2. Auto-detecta a porta serial ou usa a porta fornecida.
3. Extrai o pacote em memória (zipfile).
4. Comunica-se com a thread `kit_comms` no firmware do KIT:
   - KIT_TOOL_BEGIN <tool_id>
   - Para cada arquivo: KIT_FILE_WRITE <filename> <size> -> Binário
   - KIT_TOOL_COMMIT
"""

import os
import tempfile
import zipfile
import json
import time
from pathlib import Path
from typing import Optional, Tuple

try:
    import serial
except ImportError:
    serial = None

from kit_cli.packager import inspect_kit_package


def _find_serial_port() -> Optional[str]:
    """Tenta auto-detectar a porta serial do KIT (ESP32-S3)."""
    import glob
    # macOS
    ports = glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/cu.usbserial*")
    # Linux
    ports += glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*")

    if ports:
        return sorted(ports)[0]
    return None


def flash_tool(kit_file: Path, port: Optional[str] = None, baud: int = 921600) -> Tuple[bool, str]:
    """
    Envia um pacote .kit para o KIT usando o protocolo serial nativo.
    """
    if not serial:
        return False, (
            "A biblioteca 'pyserial' é necessária para a transferência.\n"
            "Instale com: pip install pyserial"
        )

    kit_file = Path(kit_file)

    # 1. Valida o pacote
    if not kit_file.exists():
        return False, f"Arquivo não encontrado: {kit_file}"

    ok, manifest, msg = inspect_kit_package(kit_file)
    if not ok:
        return False, f"Pacote inválido: {msg}"

    tool_id = manifest.get("id", "unknown")

    # 2. Detecta porta serial
    if not port:
        port = _find_serial_port()
        if not port:
            return False, (
                "Porta serial não detectada. Conecte o KIT via USB e tente novamente,\n"
                "ou especifique a porta com --port (ex: --port /dev/cu.usbmodem123)."
            )

    try:
        # 3. Extrai para temp e abre conexão
        with tempfile.TemporaryDirectory(prefix="kit_flash_") as tmpdir:
            tmppath = Path(tmpdir)
            with zipfile.ZipFile(kit_file, "r") as zf:
                zf.extractall(tmppath)
            
            # Abre conexão serial
            ser = serial.Serial(port, baud, timeout=2.0)
            
            def send_cmd(cmd: str) -> bool:
                ser.write((cmd + "\n").encode('utf-8'))
                ser.flush()
                # Aguarda OK
                start = time.time()
                while time.time() - start < 5.0:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line == "OK":
                        return True
                    if line.startswith("ERR"):
                        raise Exception(f"Firmware erro: {line}")
                raise Exception("Timeout esperando resposta do firmware.")

            # Inicia
            send_cmd(f"KIT_TOOL_BEGIN {tool_id}")

            # Itera arquivos
            for root, _, files in os.walk(tmppath):
                for file in files:
                    filepath = Path(root) / file
                    relpath = filepath.relative_to(tmppath).as_posix()
                    size = filepath.stat().st_size
                    
                    # Ignora pastas (se houver no as_posix) ou lixo
                    if size == 0:
                        continue

                    # Prepara firmware
                    send_cmd(f"KIT_FILE_WRITE {relpath} {size}")

                    # Envia binário chunked
                    with open(filepath, "rb") as f:
                        while chunk := f.read(2048):
                            ser.write(chunk)
                    
                    # Aguarda OK final da gravação do arquivo
                    start = time.time()
                    while time.time() - start < 5.0:
                        line = ser.readline().decode('utf-8', errors='ignore').strip()
                        if line == "OK":
                            break
                        if line.startswith("ERR"):
                            raise Exception(f"Erro salvando arquivo: {line}")

            # Finaliza
            send_cmd("KIT_TOOL_COMMIT")
            
            ser.close()
            return True, f"Tool '{tool_id}' instalada com sucesso na porta {port}!"

    except serial.SerialException as e:
        return False, f"Erro na porta serial {port}: {e}"
    except Exception as e:
        return False, f"Erro durante transferência: {e}"
