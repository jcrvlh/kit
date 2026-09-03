"""
kit_cli.validator — Validador de conformidade e integridade de Tools para o KIT.
"""

import json
import re
from pathlib import Path
from typing import Dict, List, Tuple

VALID_PERMISSIONS = {
    "display",
    "input",
    "storage",
    "random",
    "time",
    "audio",
    "power",
    "imu",
    "network",
}

ID_REGEX = re.compile(r"^[a-z0-9_]+(\.[a-z0-9_]+)+$")
SEMVER_REGEX = re.compile(r"^\d+\.\d+\.\d+$")
ACCENT_REGEX = re.compile(r"^#?[0-9a-fA-F]{6}$")

# Ícones geométricos que o Launcher sabe desenhar para o card da Home.
VALID_HOME_ICONS = {
    "card", "dice", "spin", "coin", "triangle",
    "bingo", "order", "timer", "first", "teams", "ask",
    "pavio", "adedonha", "placar", "veto", "mimica", "testa",
}

# Tipo da Tool na listagem da Home: ferramenta ou mini-jogo.
VALID_KINDS = {"tool", "game"}

# Tamanho máximo de um pacote .kit (7 MB — tamanho da partição tools)
MAX_PACKAGE_BYTES = 7 * 1024 * 1024


def validate_manifest_dict(data: dict) -> Tuple[bool, List[str]]:
    """Valida a estrutura do dicionário de manifesto contra a especificação v1."""
    errors = []

    # Campos obrigatórios
    required_fields = [
        "manifest_version",
        "id",
        "name",
        "version",
        "version_code",
        "min_runtime",
        "max_runtime",
        "author",
        "description",
        "icon",
        "entry_point",
        "arch",
        "permissions",
    ]

    for field in required_fields:
        if field not in data:
            errors.append(f"Campo obrigatório ausente: '{field}'")

    if errors:
        return False, errors

    # Validação dos tipos e valores
    if data["manifest_version"] != 1:
        errors.append(f"Versão de manifesto não suportada: {data['manifest_version']} (esperado: 1)")

    if not ID_REGEX.match(data["id"]):
        errors.append(f"ID inválido '{data['id']}'. Deve seguir o formato reverso de domínio (ex: com.autor.tool)")

    if not SEMVER_REGEX.match(str(data["version"])):
        errors.append(f"Versão inválida '{data['version']}'. Deve seguir Semantic Versioning (ex: 1.0.0)")

    if not isinstance(data["version_code"], int) or data["version_code"] < 1:
        errors.append("O campo 'version_code' deve ser um número inteiro maior ou igual a 1.")

    if data["arch"] != "xtensa-esp32s3":
        errors.append(f"Arquitetura inválida '{data['arch']}'. Deve ser 'xtensa-esp32s3'")

    if not isinstance(data["permissions"], list):
        errors.append("O campo 'permissions' deve ser uma lista de strings.")
    else:
        for perm in data["permissions"]:
            if perm not in VALID_PERMISSIONS:
                errors.append(f"Permissão desconhecida ou inválida: '{perm}'. Válidas: {sorted(VALID_PERMISSIONS)}")

    # Validação do api_level (opcional mas recomendado)
    if "api_level" in data:
        if not isinstance(data["api_level"], int) or data["api_level"] < 1:
            errors.append("O campo 'api_level' deve ser um número inteiro maior ou igual a 1.")

    # Campos opcionais de aparência na Home
    if "kind" in data and data["kind"] not in VALID_KINDS:
        errors.append(
            f"'kind' inválido: '{data['kind']}'. Válidos: {sorted(VALID_KINDS)}"
        )

    if "accent" in data and not (
        isinstance(data["accent"], str) and ACCENT_REGEX.match(data["accent"])
    ):
        errors.append("O campo 'accent' deve ser uma cor hex '#RRGGBB'.")

    if "home_icon" in data and data["home_icon"] not in VALID_HOME_ICONS:
        errors.append(
            f"'home_icon' inválido: '{data['home_icon']}'. Válidos: {sorted(VALID_HOME_ICONS)}"
        )

    # Validação de semver para min_runtime e max_runtime
    for field in ("min_runtime", "max_runtime"):
        val = str(data.get(field, ""))
        if val and not SEMVER_REGEX.match(val):
            errors.append(f"'{field}' inválido '{val}'. Deve seguir Semantic Versioning (ex: 1.0.0)")

    return len(errors) == 0, errors


def validate_manifest_file(manifest_path: Path) -> Tuple[bool, List[str]]:
    """Valida um arquivo manifest.json no disco."""
    if not manifest_path.exists():
        return False, [f"Arquivo não encontrado: {manifest_path}"]

    try:
        with open(manifest_path, "r", encoding="utf-8") as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        return False, [f"Erro de sintaxe JSON no manifesto: {e}"]
    except Exception as e:
        return False, [f"Falha ao ler o manifesto: {e}"]

    return validate_manifest_dict(data)


def validate_package_size(kit_path: Path) -> Tuple[bool, str]:
    """Verifica se o pacote .kit está dentro do limite de tamanho."""
    size = kit_path.stat().st_size
    if size > MAX_PACKAGE_BYTES:
        mb = size / (1024 * 1024)
        return False, f"Pacote excede o limite de 7 MB ({mb:.1f} MB). A partição tools tem exatamente 7 MB."
    return True, f"Tamanho OK: {size / 1024:.1f} KB"
