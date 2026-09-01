"""
kit_cli.main — Ponto de entrada CLI para gerenciamento de Tools do KIT.

Migrado para Click (v8+) com output via Rich para melhor DX.
"""

import sys
from pathlib import Path

import click
from rich.console import Console
from rich.table import Table

from kit_cli import __version__
from kit_cli.validator import validate_manifest_file
from kit_cli.packager import pack_tool_directory, inspect_kit_package
from kit_cli.scaffolder import scaffold_new_tool
from kit_cli.builder import build_tool
from kit_cli.flasher import flash_tool

console = Console()


@click.group()
@click.version_option(__version__, prog_name="kit-cli")
def cli():
    """KIT Tool SDK CLI — Crie, valide, compile, empacote e instale Tools para o KIT."""
    pass


# -----------------------------------------------------------------------
# new — Criar nova Tool
# -----------------------------------------------------------------------

@cli.command()
@click.argument("name")
@click.option("--id", "tool_id", default=None, help="ID exclusivo da Tool (ex: com.autor.tool).")
@click.option("--dir", "target_dir", default=".", help="Diretório de destino (padrão: .).")
def new(name: str, tool_id: str, target_dir: str):
    """Cria um novo projeto de Tool a partir de um template."""
    dir_path = Path(target_dir).resolve()
    final_id = tool_id or f"com.kit.{name.lower().replace(' ', '_')}"

    console.print(f"📦 Criando nova Tool: [bold]{name}[/bold] ({final_id})...")
    ok, msg = scaffold_new_tool(name, final_id, dir_path)
    if ok:
        console.print(f"[green]✅ {msg}[/green]")
    else:
        console.print(f"[red]❌ {msg}[/red]")
        sys.exit(1)


# -----------------------------------------------------------------------
# validate — Validar manifesto ou pacote
# -----------------------------------------------------------------------

@cli.command()
@click.argument("target", default=".", required=False)
def validate(target: str):
    """Valida conformidade do manifesto ou pacote .kit."""
    target_path = Path(target).resolve()

    if target_path.is_file() and target_path.name.endswith(".kit"):
        console.print(f"🔍 Validando pacote: [bold]{target_path.name}[/bold]...")
        ok, manifest, msg = inspect_kit_package(target_path)
        if not ok:
            console.print(f"[red]❌ {msg}[/red]")
            sys.exit(1)
        console.print("[green]✅ Pacote .kit válido e íntegro![/green]")
        return

    manifest_path = target_path / "manifest.json" if target_path.is_dir() else target_path
    console.print(f"🔍 Validando manifesto: [bold]{manifest_path}[/bold]...")
    valid, errors = validate_manifest_file(manifest_path)

    if not valid:
        console.print("[red]❌ Erros encontrados no manifesto:[/red]")
        for err in errors:
            console.print(f"  [red]•[/red] {err}")
        sys.exit(1)

    console.print("[green]✅ Manifesto válido de acordo com a especificação v1![/green]")


# -----------------------------------------------------------------------
# build — Compilar a Tool
# -----------------------------------------------------------------------

@cli.command()
@click.argument("dir", default=".", required=False)
@click.option("--target", "build_target", default="auto",
              type=click.Choice(["auto", "native", "xtensa"], case_sensitive=False),
              help="Target: auto (detecta ESP-IDF), native (desktop/stubs), xtensa (cross-compile).")
def build(dir: str, build_target: str):
    """Compila a Tool (desktop ou cross-compile Xtensa)."""
    source_dir = Path(dir).resolve()
    console.print(f"🔨 Compilando Tool em [bold]{source_dir}[/bold] (target: {build_target})...")

    ok, msg = build_tool(source_dir, build_target)
    if ok:
        console.print(f"[green]✅ {msg}[/green]")
    else:
        console.print(f"[red]❌ {msg}[/red]")
        sys.exit(1)


# -----------------------------------------------------------------------
# pack — Empacotar em .kit
# -----------------------------------------------------------------------

@cli.command()
@click.argument("dir", default=".", required=False)
@click.option("-o", "--output", default=None, help="Caminho do arquivo .kit de saída.")
def pack(dir: str, output: str):
    """Empacota a Tool em um arquivo .kit (ZIP com checksum SHA-256)."""
    source_dir = Path(dir).resolve()
    output_file = Path(output).resolve() if output else None

    console.print(f"📦 Empacotando Tool em [bold]{source_dir}[/bold]...")
    ok, msg = pack_tool_directory(source_dir, output_file)
    if ok:
        console.print(f"[green]✅ {msg}[/green]")
    else:
        console.print(f"[red]❌ {msg}[/red]")
        sys.exit(1)


# -----------------------------------------------------------------------
# info — Inspecionar pacote .kit
# -----------------------------------------------------------------------

@cli.command()
@click.argument("file")
def info(file: str):
    """Exibe informações e metadados de um arquivo .kit."""
    kit_file = Path(file).resolve()
    console.print(f"📦 Inspecionando pacote: [bold]{kit_file.name}[/bold]...")

    ok, manifest, msg = inspect_kit_package(kit_file)
    if not ok or not manifest:
        console.print(f"[red]❌ {msg}[/red]")
        sys.exit(1)

    table = Table(title=f"🧰 {manifest.get('name')} v{manifest.get('version')}", show_lines=True)
    table.add_column("Campo", style="cyan", no_wrap=True)
    table.add_column("Valor", style="white")

    table.add_row("ID", manifest.get("id"))
    table.add_row("Versão", f"{manifest.get('version')} (code: {manifest.get('version_code')})")
    table.add_row("Autor", manifest.get("author"))
    table.add_row("Descrição", manifest.get("description"))
    table.add_row("Runtime", f">={manifest.get('min_runtime')} <={manifest.get('max_runtime')}")
    table.add_row("Arquitetura", manifest.get("arch"))
    table.add_row("API Level", str(manifest.get("api_level", "N/A")))
    table.add_row("Permissões", ", ".join(manifest.get("permissions", [])))
    table.add_row("Checksum", manifest.get("checksum", "N/A"))

    console.print(table)


# -----------------------------------------------------------------------
# flash — Enviar .kit via serial
# -----------------------------------------------------------------------

@cli.command()
@click.argument("file")
@click.option("--port", default=None, help="Porta serial (ex: /dev/ttyUSB0). Auto-detecta se omitida.")
@click.option("--baud", default=921600, help="Velocidade da serial (padrão: 921600).")
def flash(file: str, port: str, baud: int):
    """Envia um arquivo .kit para o KIT via serial."""
    kit_file = Path(file).resolve()
    console.print(f"⚡ Enviando [bold]{kit_file.name}[/bold] via serial...")

    ok, msg = flash_tool(kit_file, port, baud)
    if ok:
        console.print(f"[green]✅ {msg}[/green]")
    else:
        console.print(f"[red]❌ {msg}[/red]")
        sys.exit(1)


def main():
    cli()


if __name__ == "__main__":
    main()
