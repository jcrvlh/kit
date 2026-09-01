"""
kit_cli.scaffolder — Gerador de novos projetos de Tools (Boilerplate).
"""

import json
from pathlib import Path
from typing import Tuple


def scaffold_new_tool(name: str, tool_id: str, target_dir: Path) -> Tuple[bool, str]:
    """Cria a estrutura de pastas e arquivos padrão para uma nova Tool."""
    project_dir = target_dir / name.lower().replace(" ", "_")
    if project_dir.exists():
        return False, f"O diretório '{project_dir}' já existe."

    try:
        project_dir.mkdir(parents=True, exist_ok=True)
        (project_dir / "src").mkdir(exist_ok=True)
        (project_dir / "assets").mkdir(exist_ok=True)
        (project_dir / "build").mkdir(exist_ok=True)

        # 1. Cria o manifest.json
        manifest = {
            "manifest_version": 1,
            "id": tool_id,
            "name": name,
            "version": "1.0.0",
            "version_code": 1,
            "min_runtime": "0.1.0",
            "max_runtime": "1.0.0",
            "author": "KIT Developer",
            "description": f"Tool {name} para a plataforma KIT.",
            "icon": "icon.bin",
            "entry_point": "tool.elf",
            "arch": "xtensa-esp32s3",
            "permissions": ["display", "input", "random", "storage"],
            "api_level": 1,
            "assets": [],
        }

        with open(project_dir / "manifest.json", "w", encoding="utf-8") as f:
            json.dump(manifest, f, indent=2, ensure_ascii=False)

        # 2. Cria o arquivo src/main.c
        main_c_content = f'''/**
 * @file main.c
 * @brief {name} — Tool para o KIT.
 */

#include "kit_tool_api.h"
#include "kit_theme.h"
#include <stdio.h>

static const kit_api_table_t *s_api = NULL;
static lv_obj_t *s_screen = NULL;

/**
 * Callback de toque.
 */
static void on_input(const kit_input_event_t *event, void *user_data)
{{
    (void)user_data;
    if (!s_api || event->type != KIT_INPUT_TAP) return;

    /* Exemplo: bipe ao tocar */
    if (s_api->audio) {{
        s_api->audio->beep(1500, 50);
    }}
}}

/**
 * Ponto de entrada da Tool.
 */
kit_err_t tool_init(kit_tool_ctx_t *ctx)
{{
    if (!ctx || !ctx->api) return KIT_ERR_INVALID_ARG;
    s_api = ctx->api;

    /* Obtém a tela raiz da Tool */
    s_screen = s_api->display->get_screen();

    /*
     * Criação de interface em LVGL v9:
     *
     * lv_obj_t *label = lv_label_create(s_screen);
     * lv_label_set_text(label, "{name.upper()}");
     * lv_obj_set_style_text_font(label, &kit_mono_26, 0);
     * lv_obj_set_style_text_color(label, lv_color_hex(KIT_COLOR_TEXT), 0);
     * lv_obj_set_style_text_letter_space(label, 3, 0);
     * lv_obj_center(label);
     */

    /* Registra callback de entrada */
    if (s_api->input) {{
        s_api->input->register_callback(on_input, NULL);
    }}

    return KIT_OK;
}}

/**
 * Destrutor da Tool.
 */
void tool_destroy(void)
{{
    s_screen = NULL;
    s_api = NULL;
}}
'''
        with open(project_dir / "src" / "main.c", "w", encoding="utf-8") as f:
            f.write(main_c_content)

        # 3. Cria o CMakeLists.txt do projeto
        cmake_content = f"""cmake_minimum_required(VERSION 3.16)
project({tool_id} C)

set(CMAKE_C_STANDARD 11)

add_executable(tool.elf
    src/main.c
    ${{CMAKE_CURRENT_SOURCE_DIR}}/../../stubs/stub_main.c
)

target_link_libraries(tool.elf PRIVATE kit_stubs)
"""
        with open(project_dir / "CMakeLists.txt", "w", encoding="utf-8") as f:
            f.write(cmake_content)

        # 4. Cria arquivo de ícone placeholder
        with open(project_dir / "icon.bin", "wb") as f:
            f.write(b"\x00" * 128)

        # 5. Cria o .gitignore
        gitignore_content = """build/
*.o
*.elf
*.kit
.DS_Store
"""
        with open(project_dir / ".gitignore", "w", encoding="utf-8") as f:
            f.write(gitignore_content)

        # 6. Cria o README do projeto
        readme_content = f"""# {name} — KIT Tool

ID: `{tool_id}`
Versão: `1.0.0`

## 🛠️ Como Desenvolver e Empacotar

1. Escreva a lógica no arquivo `src/main.c`.
2. Adicione recursos visuais na pasta `assets/`.
3. Compile, valide e gere o pacote `.kit`:

```bash
# Compilar (desktop — testes sem hardware)
kit-cli build . --target native

# Validar manifesto
kit-cli validate .

# Empacotar
kit-cli pack .

# Enviar para o KIT via serial
kit-cli flash {name.lower().replace(' ', '_')}.kit
```

## 📖 Referência

- [Guia do SDK](https://github.com/jcrvlh/kit/tree/main/tools-sdk/docs/sdk_guide.md)
- [Referência de API](https://github.com/jcrvlh/kit/tree/main/tools-sdk/docs/api_reference.md)
- [Especificação do Manifesto](https://github.com/jcrvlh/kit/tree/main/tools-sdk/docs/manifest_spec.md)
"""
        with open(project_dir / "README.md", "w", encoding="utf-8") as f:
            f.write(readme_content)

        return True, f"Projeto '{name}' criado com sucesso em: {project_dir}"
    except Exception as e:
        return False, f"Falha ao criar o projeto: {e}"
