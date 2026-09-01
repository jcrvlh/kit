"""
kit_cli.builder — Compilação de Tools (desktop com stubs ou cross-compile Xtensa).
"""

import os
import shutil
import subprocess
from pathlib import Path
from typing import Tuple


def _find_sdk_root() -> Path:
    """Encontra o diretório raiz do tools-sdk subindo a partir deste arquivo."""
    current = Path(__file__).resolve().parent  # kit_cli/
    # Sobe: kit_cli → cli → tools-sdk
    sdk_root = current.parent.parent
    if (sdk_root / "include" / "kit_tool_api.h").exists():
        return sdk_root
    # Fallback: tenta encontrar via variável de ambiente
    env_path = os.environ.get("KIT_SDK_PATH")
    if env_path:
        return Path(env_path)
    return sdk_root


def _has_idf() -> bool:
    """Verifica se o ESP-IDF está configurado no ambiente."""
    return bool(os.environ.get("IDF_PATH")) and shutil.which("xtensa-esp32s3-elf-gcc")


def _detect_target(requested: str) -> str:
    """Resolve 'auto' para 'xtensa' ou 'native'."""
    if requested != "auto":
        return requested
    return "xtensa" if _has_idf() else "native"


def build_tool(source_dir: Path, target: str = "auto") -> Tuple[bool, str]:
    """
    Compila uma Tool.

    Args:
        source_dir: Diretório do projeto da Tool (deve conter CMakeLists.txt).
        target: 'auto' (detecta ESP-IDF), 'native' (desktop), 'xtensa' (cross-compile).

    Returns:
        (success, message)
    """
    source_dir = Path(source_dir)

    if not (source_dir / "CMakeLists.txt").exists():
        return False, f"CMakeLists.txt não encontrado em '{source_dir}'."

    resolved = _detect_target(target)
    build_dir = source_dir / "build"
    sdk_root = _find_sdk_root()

    if resolved == "native":
        return _build_native(source_dir, build_dir, sdk_root)
    elif resolved == "xtensa":
        return _build_xtensa(source_dir, build_dir, sdk_root)
    else:
        return False, f"Target desconhecido: '{resolved}'."


def _build_native(source_dir: Path, build_dir: Path, sdk_root: Path) -> Tuple[bool, str]:
    """Compila com o compilador nativo (gcc/clang) linkando contra stubs."""
    try:
        # Configura CMake
        cmake_args = [
            "cmake",
            "-B", str(build_dir),
            "-S", str(source_dir),
            f"-DCMAKE_PREFIX_PATH={sdk_root}",
        ]

        result = subprocess.run(
            cmake_args,
            capture_output=True,
            text=True,
            cwd=str(source_dir),
        )

        if result.returncode != 0:
            return False, f"CMake configure falhou:\n{result.stderr}"

        # Build
        result = subprocess.run(
            ["cmake", "--build", str(build_dir)],
            capture_output=True,
            text=True,
            cwd=str(source_dir),
        )

        if result.returncode != 0:
            return False, f"Compilação falhou:\n{result.stderr}"

        return True, f"Build nativo (desktop) concluído em: {build_dir}"

    except FileNotFoundError:
        return False, "cmake não encontrado. Instale o CMake 3.16+."
    except Exception as e:
        return False, f"Erro durante o build: {e}"


def _build_xtensa(source_dir: Path, build_dir: Path, sdk_root: Path) -> Tuple[bool, str]:
    """Compila com toolchain Xtensa (ESP-IDF) para gerar tool.elf."""
    idf_path = os.environ.get("IDF_PATH")
    if not idf_path:
        return False, (
            "ESP-IDF não encontrado. Configure $IDF_PATH e execute:\n"
            "  source $IDF_PATH/export.sh\n\n"
            "Ou use '--target native' para compilar em modo desktop."
        )

    toolchain_file = Path(idf_path) / "tools" / "cmake" / "toolchain-esp32s3.cmake"
    if not toolchain_file.exists():
        # Tenta localizar via componentes
        toolchain_file = None

    try:
        cmake_args = [
            "cmake",
            "-B", str(build_dir),
            "-S", str(source_dir),
            f"-DCMAKE_PREFIX_PATH={sdk_root}",
        ]

        if toolchain_file:
            cmake_args.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}")
        else:
            # Usa o compilador Xtensa diretamente
            cmake_args.extend([
                "-DCMAKE_C_COMPILER=xtensa-esp32s3-elf-gcc",
                "-DCMAKE_SYSTEM_NAME=Generic",
                "-DCMAKE_SYSTEM_PROCESSOR=xtensa",
            ])

        result = subprocess.run(
            cmake_args,
            capture_output=True,
            text=True,
            cwd=str(source_dir),
        )

        if result.returncode != 0:
            return False, f"CMake configure (Xtensa) falhou:\n{result.stderr}"

        result = subprocess.run(
            ["cmake", "--build", str(build_dir)],
            capture_output=True,
            text=True,
            cwd=str(source_dir),
        )

        if result.returncode != 0:
            return False, f"Compilação Xtensa falhou:\n{result.stderr}"

        return True, f"Build Xtensa (ESP32-S3) concluído em: {build_dir}"

    except FileNotFoundError:
        return False, "cmake não encontrado. Instale o CMake 3.16+."
    except Exception as e:
        return False, f"Erro durante o build Xtensa: {e}"
