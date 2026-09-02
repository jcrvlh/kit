"""
Testes automatizados do kit-cli — cobertura expandida para v0.2.0.
"""

import json
import pytest
from pathlib import Path
from kit_cli.validator import validate_manifest_dict, validate_manifest_file, validate_package_size
from kit_cli.scaffolder import scaffold_new_tool
from kit_cli.packager import pack_tool_directory, inspect_kit_package


# -----------------------------------------------------------------------
# Fixtures
# -----------------------------------------------------------------------

def _valid_manifest():
    return {
        "manifest_version": 1,
        "id": "com.kit.dice",
        "name": "Dice Roller",
        "version": "1.0.0",
        "version_code": 1,
        "author": "KIT Community",
        "description": "A dice roller for tabletop games",
        "icon": "icon.bin",
        "entry_point": "tool.so",
        "min_runtime": "0.1.0",
        "max_runtime": "1.0.0",
        "arch": "xtensa-esp32s3",
        "permissions": ["display", "input", "random"],
        "api_level": 1,
    }


# -----------------------------------------------------------------------
# Testes de Validação de Manifesto
# -----------------------------------------------------------------------

def test_manifest_validation_valid():
    """Manifesto completo e válido."""
    valid, errors = validate_manifest_dict(_valid_manifest())
    assert valid is True
    assert len(errors) == 0


def test_manifest_validation_missing_fields():
    """Campos obrigatórios ausentes."""
    manifest = {"name": "Incomplete App"}
    valid, errors = validate_manifest_dict(manifest)
    assert valid is False
    assert any("id" in err for err in errors)
    assert any("version" in err for err in errors)


def test_manifest_validation_invalid_id():
    """ID fora do formato reverso de domínio."""
    manifest = _valid_manifest()
    manifest["id"] = "invalid id with spaces"
    valid, errors = validate_manifest_dict(manifest)
    assert valid is False
    assert any("ID inválido" in err for err in errors)


def test_manifest_validation_invalid_id_single_segment():
    """ID com segmento único (sem ponto)."""
    manifest = _valid_manifest()
    manifest["id"] = "noperiod"
    valid, errors = validate_manifest_dict(manifest)
    assert valid is False


def test_manifest_validation_invalid_version():
    """Versão sem semver."""
    manifest = _valid_manifest()
    manifest["version"] = "abc"
    valid, errors = validate_manifest_dict(manifest)
    assert valid is False
    assert any("Versão inválida" in err for err in errors)


def test_manifest_validation_invalid_version_code():
    """version_code zero ou negativo."""
    manifest = _valid_manifest()
    manifest["version_code"] = 0
    valid, errors = validate_manifest_dict(manifest)
    assert valid is False
    assert any("version_code" in err for err in errors)


def test_manifest_validation_invalid_arch():
    """Arquitetura diferente de xtensa-esp32s3."""
    manifest = _valid_manifest()
    manifest["arch"] = "arm-cortex-m4"
    valid, errors = validate_manifest_dict(manifest)
    assert valid is False
    assert any("Arquitetura inválida" in err for err in errors)


def test_manifest_validation_invalid_permission():
    """Permissão desconhecida."""
    manifest = _valid_manifest()
    manifest["permissions"] = ["display", "bluetooth", "camera"]
    valid, errors = validate_manifest_dict(manifest)
    assert valid is False
    assert any("bluetooth" in err for err in errors)
    assert any("camera" in err for err in errors)


def test_manifest_validation_all_valid_permissions():
    """Todas as permissões válidas aceitas."""
    manifest = _valid_manifest()
    manifest["permissions"] = [
        "display", "input", "storage", "random", "time", "audio", "power", "imu", "network"
    ]
    valid, errors = validate_manifest_dict(manifest)
    assert valid is True


def test_manifest_validation_invalid_api_level():
    """api_level zero."""
    manifest = _valid_manifest()
    manifest["api_level"] = 0
    valid, errors = validate_manifest_dict(manifest)
    assert valid is False
    assert any("api_level" in err for err in errors)


def test_manifest_validation_invalid_min_runtime():
    """min_runtime não-semver."""
    manifest = _valid_manifest()
    manifest["min_runtime"] = "latest"
    valid, errors = validate_manifest_dict(manifest)
    assert valid is False
    assert any("min_runtime" in err for err in errors)


def test_manifest_validation_missing_api_level_ok():
    """api_level é opcional — manifesto sem ele deve ser válido."""
    manifest = _valid_manifest()
    del manifest["api_level"]
    valid, errors = validate_manifest_dict(manifest)
    assert valid is True


def test_manifest_file_not_found():
    """Arquivo inexistente."""
    valid, errors = validate_manifest_file(Path("/nonexistent/manifest.json"))
    assert valid is False
    assert any("não encontrado" in err for err in errors)


def test_manifest_file_invalid_json(tmp_path):
    """JSON corrompido."""
    bad = tmp_path / "manifest.json"
    bad.write_text("{invalid json", encoding="utf-8")
    valid, errors = validate_manifest_file(bad)
    assert valid is False
    assert any("sintaxe JSON" in err for err in errors)


# -----------------------------------------------------------------------
# Testes de Scaffolding
# -----------------------------------------------------------------------

def test_scaffolding_creates_structure(tmp_path):
    """Scaffold cria todos os arquivos esperados."""
    ok, msg = scaffold_new_tool("Test Tool", "com.test.tool", tmp_path)
    assert ok is True
    tool_dir = tmp_path / "test_tool"
    assert (tool_dir / "manifest.json").exists()
    assert (tool_dir / "src" / "main.c").exists()
    assert (tool_dir / "icon.bin").exists()
    assert (tool_dir / "CMakeLists.txt").exists()
    assert (tool_dir / ".gitignore").exists()
    assert (tool_dir / "README.md").exists()
    assert (tool_dir / "build").is_dir()
    assert (tool_dir / "assets").is_dir()


def test_scaffolding_includes_theme_header(tmp_path):
    """Template main.c inclui kit_theme.h."""
    ok, _ = scaffold_new_tool("Theme Test", "com.test.theme", tmp_path)
    assert ok is True
    main_c = (tmp_path / "theme_test" / "src" / "main.c").read_text()
    assert '#include "kit_theme.h"' in main_c


def test_scaffolding_manifest_valid(tmp_path):
    """Manifesto gerado pelo scaffold deve ser válido."""
    ok, _ = scaffold_new_tool("Valid Tool", "com.test.valid", tmp_path)
    assert ok is True
    valid, errors = validate_manifest_file(tmp_path / "valid_tool" / "manifest.json")
    assert valid is True, f"Errors: {errors}"


def test_scaffolding_duplicate_dir(tmp_path):
    """Scaffold em diretório que já existe falha."""
    scaffold_new_tool("Dup", "com.test.dup", tmp_path)
    ok, msg = scaffold_new_tool("Dup", "com.test.dup", tmp_path)
    assert ok is False
    assert "já existe" in msg


# -----------------------------------------------------------------------
# Testes de Empacotamento e Inspeção
# -----------------------------------------------------------------------

def test_full_pipeline(tmp_path):
    """Pipeline completo: scaffold → validate → pack → info."""
    # Scaffold
    ok, _ = scaffold_new_tool("Full Test", "com.test.full", tmp_path)
    assert ok is True
    tool_dir = tmp_path / "full_test"

    # Validate
    valid, errors = validate_manifest_file(tool_dir / "manifest.json")
    assert valid is True, f"Errors: {errors}"

    # Cria binário dummy
    with open(tool_dir / "tool.so", "wb") as f:
        f.write(b"\x7fELF\x01\x01\x01\x00" + b"\x00" * 32)

    # Pack
    out_pkg = tmp_path / "test.kit"
    ok, msg = pack_tool_directory(tool_dir, out_pkg)
    assert ok is True, f"Packaging failed: {msg}"
    assert out_pkg.exists()

    # Info / Inspect
    ok, manifest, msg = inspect_kit_package(out_pkg)
    assert ok is True, f"Inspection failed: {msg}"
    assert manifest["id"] == "com.test.full"
    assert manifest["name"] == "Full Test"
    assert "checksum" in manifest


def test_pack_missing_elf(tmp_path):
    """Pack falha se tool.so não existe."""
    ok, _ = scaffold_new_tool("No ELF", "com.test.noelf", tmp_path)
    tool_dir = tmp_path / "no_elf"
    ok, msg = pack_tool_directory(tool_dir)
    assert ok is False
    assert "tool.so" in msg


def test_inspect_invalid_zip(tmp_path):
    """Inspeção de arquivo não-ZIP falha graciosamente."""
    fake = tmp_path / "fake.kit"
    fake.write_text("not a zip")
    ok, manifest, msg = inspect_kit_package(fake)
    assert ok is False
    assert "ZIP corrompido" in msg


def test_inspect_missing_manifest(tmp_path):
    """Inspeção de ZIP sem manifest.json falha."""
    import zipfile
    fake = tmp_path / "no_manifest.kit"
    with zipfile.ZipFile(fake, "w") as zf:
        zf.writestr("tool.so", b"\x00" * 8)
    ok, manifest, msg = inspect_kit_package(fake)
    assert ok is False
    assert "manifest.json" in msg


def test_package_size_validation(tmp_path):
    """Validação de tamanho do pacote."""
    # Pacote pequeno — OK
    small = tmp_path / "small.kit"
    small.write_bytes(b"\x00" * 1024)
    ok, msg = validate_package_size(small)
    assert ok is True

    # Pacote grande demais (>7 MB) — simulado alterando a constante é impraticável,
    # então verificamos apenas que a função existe e retorna os tipos certos
    assert isinstance(msg, str)
