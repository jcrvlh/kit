"""
kit_cli.builder — Compilação de Tools (desktop com stubs ou cross-compile Xtensa).
"""
from __future__ import annotations

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


XTENSA_CC = "xtensa-esp32s3-elf-gcc"
XTENSA_STRIP = "xtensa-esp32s3-elf-strip"


def _has_idf() -> bool:
    """Verifica se o toolchain Xtensa está no PATH (via export.sh do ESP-IDF)."""
    return shutil.which(XTENSA_CC) is not None


def _find_lvgl(sdk_root: Path) -> Path | None:
    """Localiza os headers do LVGL para o build da Tool.

    Ordem: $KIT_LVGL_DIR → managed_components do firmware (após um build IDF) →
    None. A Tool só usa handles opacos do LVGL, então a config (lv_conf.h) do
    diretório encontrado não afeta o binário: basta ser a mesma versão do
    firmware (9.5.x).
    """
    env = os.environ.get("KIT_LVGL_DIR")
    if env and (Path(env) / "lvgl.h").exists():
        return Path(env)
    cand = sdk_root.parent / "firmware" / "managed_components" / "lvgl__lvgl"
    if (cand / "lvgl.h").exists():
        return cand
    return None


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
    if not shutil.which(XTENSA_CC):
        return False, (
            f"'{XTENSA_CC}' não está no PATH. Rode '. $IDF_PATH/export.sh'.\n"
            "Ou use '--target native' para compilar a lógica em modo desktop."
        )

    lvgl = _find_lvgl(sdk_root)
    if lvgl is None:
        return False, (
            "Headers do LVGL não encontrados. Defina $KIT_LVGL_DIR apontando "
            "para um checkout do lvgl/lvgl 9.5.x, ou rode um build do firmware "
            "uma vez (popula firmware/managed_components/lvgl__lvgl)."
        )

    srcs = sorted(str(p) for p in (source_dir / "src").glob("*.c"))
    if not srcs:
        return False, f"Nenhum .c em '{source_dir / 'src'}'."

    out = source_dir / "tool.so"
    build_dir.mkdir(parents=True, exist_ok=True)

    # Espelha tools-sdk/examples/hello_sd/build.sh (recipe validada em HW):
    # PIC + -shared + sem libc/CRT, só tool_init/tool_destroy no .dynsym.
    cflags = [
        "-std=gnu17", "-Os", "-mlongcalls",
        "-fPIC", "-shared", "-nostdlib", "-nostartfiles",
        "-ffunction-sections", "-fdata-sections", "-fvisibility=hidden",
        "-Wall", "-Wextra",
        f"-I{sdk_root / 'include'}",
        f"-I{lvgl}", f"-I{lvgl / 'src'}",
        "-DLV_CONF_SKIP=1",
        # O firmware liga o widget QR do LVGL (CONFIG_LV_USE_QRCODE, em
        # sdkconfig.defaults) e exporta lv_qrcode_* na tabela de símbolos das
        # Tools. Sem este define, o protótipo fica atrás de `#if LV_USE_QRCODE`
        # e a Tool não compila a chamada. Ver tools-sdk/docs/tool_lvgl_runtime.md.
        "-DLV_USE_QRCODE=1",
    ]
    ldflags = ["-Wl,--gc-sections", "-Wl,--strip-all", "-Wl,--allow-shlib-undefined"]

    try:
        r = subprocess.run([XTENSA_CC, *cflags, *srcs, *ldflags, "-o", str(out)],
                           capture_output=True, text=True, cwd=str(source_dir))
        if r.returncode != 0:
            return False, f"Compilação Xtensa falhou:\n{r.stderr}"

        if shutil.which(XTENSA_STRIP):
            subprocess.run(
                [XTENSA_STRIP, "--strip-unneeded",
                 "--remove-section=.comment", "--remove-section=.got.loc",
                 "--remove-section=.dynamic", "--remove-section=.xt.lit",
                 "--remove-section=.xt.prop", "--remove-section=.xtensa.info",
                 str(out)],
                capture_output=True, text=True,
            )

        return True, f"Build Xtensa concluído: {out}"

    except FileNotFoundError as e:
        return False, f"Ferramenta ausente: {e}"
    except Exception as e:  # noqa: BLE001
        return False, f"Erro durante o build Xtensa: {e}"
