"""
kit_cli.packager - Empacotamento, validação e inspeção de pacotes .kit.
"""

import hashlib
import json
import zipfile
from pathlib import Path
from typing import Dict, List, Optional, Tuple
from kit_cli.validator import validate_manifest_dict, validate_manifest_file


def calculate_sha256(file_path: Path) -> str:
    """Calcula o hash SHA-256 de um arquivo binário."""
    sha = hashlib.sha256()
    with open(file_path, "rb") as f:
        while chunk := f.read(65536):
            sha.update(chunk)
    return sha.hexdigest()


def pack_tool_directory(source_dir: Path, output_file: Optional[Path] = None) -> Tuple[bool, str]:
    """
    Empacota um diretório de Tool em um arquivo .kit (ZIP padronizado com checksum).
    """
    source_dir = Path(source_dir)
    manifest_path = source_dir / "manifest.json"

    # 1. Valida o manifesto
    valid, errors = validate_manifest_file(manifest_path)
    if not valid:
        return False, f"Manifesto inválido:\n" + "\n".join(f"  - {e}" for e in errors)

    with open(manifest_path, "r", encoding="utf-8") as f:
        manifest = json.load(f)

    tool_id = manifest["id"]
    entry_point = source_dir / manifest["entry_point"]
    icon_file = source_dir / manifest["icon"]

    # 2. Checa arquivos obrigatórios
    if not entry_point.exists():
        return False, f"Binário de entrada '{manifest['entry_point']}' não encontrado no diretório."

    if not icon_file.exists():
        return False, f"Arquivo de ícone '{manifest['icon']}' não encontrado no diretório."

    # 3. Calcula checksum do executável
    elf_hash = calculate_sha256(entry_point)
    manifest["checksum"] = f"sha256:{elf_hash}"

    # 4. Define o caminho de saída
    if output_file is None:
        output_file = source_dir.parent / f"{tool_id}.kit"
    else:
        output_file = Path(output_file)

    # 5. Gera o arquivo .kit (ZIP)
    try:
        with zipfile.ZipFile(output_file, "w", zipfile.ZIP_DEFLATED) as zf:
            # Escreve o manifesto atualizado com o checksum
            zf.writestr("manifest.json", json.dumps(manifest, indent=2, ensure_ascii=False))

            # Escreve o checksum.sha256 em texto puro
            zf.writestr("checksum.sha256", elf_hash)

            # Adiciona o executável
            zf.write(entry_point, arcname=manifest["entry_point"])

            # Adiciona o ícone
            zf.write(icon_file, arcname=manifest["icon"])

            # Adiciona a pasta de assets se existir
            assets_dir = source_dir / "assets"
            if assets_dir.exists() and assets_dir.is_dir():
                for asset in assets_dir.rglob("*"):
                    if asset.is_file():
                        arcname = str(asset.relative_to(source_dir))
                        zf.write(asset, arcname=arcname)

        return True, f"Pacote criado com sucesso em: {output_file}"
    except Exception as e:
        return False, f"Erro ao criar o pacote .kit: {e}"


def inspect_kit_package(kit_path: Path) -> Tuple[bool, Optional[dict], str]:
    """Inspeciona e valida o conteúdo de um pacote .kit."""
    kit_path = Path(kit_path)
    if not kit_path.exists():
        return False, None, f"Arquivo não encontrado: {kit_path}"

    if not zipfile.is_zipfile(kit_path):
        return False, None, f"O arquivo '{kit_path.name}' não é um pacote .kit válido (formato ZIP corrompido)."

    try:
        with zipfile.ZipFile(kit_path, "r") as zf:
            namelist = zf.namelist()
            if "manifest.json" not in namelist:
                return False, None, "Pacote .kit inválido: 'manifest.json' ausente na raiz."

            with zf.open("manifest.json") as f:
                manifest = json.load(f)

            valid, errors = validate_manifest_dict(manifest)
            if not valid:
                return False, manifest, "Manifesto inválido:\n" + "\n".join(f"  - {e}" for e in errors)

            # Valida hash do executável se presente
            entry_point = manifest.get("entry_point")
            if entry_point in namelist:
                with zf.open(entry_point) as elf_f:
                    data = elf_f.read()
                    actual_hash = hashlib.sha256(data).hexdigest()
                    expected_hash = manifest.get("checksum", "").replace("sha256:", "")
                    if expected_hash and actual_hash != expected_hash:
                        return False, manifest, f"Aviso de integridade: Hash SHA-256 do executável não coincide!\nEsperado: {expected_hash}\nObtido:   {actual_hash}"

            return True, manifest, "Pacote válido e íntegro."
    except Exception as e:
        return False, None, f"Erro ao inspecionar pacote: {e}"
