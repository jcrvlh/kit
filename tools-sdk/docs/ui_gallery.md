# Galeria de componentes de UI — `kit_ui.h`

Widgets prontos no padrão **Brutalist Bauhaus** pra reaproveitar entre Tools,
em vez de cada uma recolar `build_titlebar` / `build_chip_grid` / `sync_dots`.

- **Fonte de verdade do visual:** [`docs/design/design-language.md`](../../docs/design/design-language.md)
  (§ "Padrão de Tool"). Esta galeria é o **companheiro em código** dela.
- **Superfície LVGL garantida:** [`tool_lvgl_runtime.md`](tool_lvgl_runtime.md).
- **Header:** [`../include/kit_ui.h`](../include/kit_ui.h) — header-only, sem
  build extra (o `kit-cli build --target xtensa` só compila `src/*.c`).

> **v1 cobre Tier 1 + Tier 2 + os dois widgets de referência (grade de chips e
> seletor de sigla).** Overlay de tela cheia, tabela de highscore, stepper e
> barra de status ficam pra v2 — ver [Roadmap](#roadmap).

---

## Como usar

`kit_ui.h` inclui `kit_tool_api.h` + `kit_theme.h` + `kit_fonts.h`. Toda a
implementação fica atrás de `#ifndef KIT_SDK_STUBS` — no build nativo (CI /
lógica) o header é inerte, igual à UI da Tool.

```c
#include "kit_ui.h"

static const kit_api_table_t *s_api;
static kit_ui_shell_t s_shell;
static kit_ui_chips_t s_dur;

static const char *const DUR[] = { "15S", "30S", "60S" };

static void on_page(int page, void *u) { (void)u; /* re-sync ao trocar de aba */ }
static void on_dur (int idx, void *u)  { (void)u; s_dur_idx = idx; save_prefs(); }

KIT_TOOL_EXPORT kit_err_t tool_init(kit_tool_ctx_t *ctx)
{
    s_api = ctx->api;
    kit_ui_bind(s_api);                       // liga áudio / rng / keep_awake / exit

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(KIT_COLOR_BG), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    kit_ui_shell_begin(&s_shell, scr, "QUADRADO", KIT_COLOR_BLUE, 3);
    kit_ui_shell_tiles(&s_shell, on_page, NULL);

    // AJUSTE
    kit_ui_label(s_shell.tiles[0], "TEMPO DA PARTIDA", KIT_COLOR_TEXT_MUTED, &kit_mono_16, 2);
    kit_ui_chips(&s_dur, s_shell.tiles[0], DUR, 3, s_dur_idx, KIT_COLOR_BLUE, on_dur, NULL);

    // JOGO — sua UI aqui (o botão de ação é filho de s_shell.tiles[1])
    build_game(s_shell.tiles[1]);

    // COMO JOGA
    kit_ui_help_page(s_shell.tiles[2], "COMO JOGA", RULES);

    kit_ui_shell_open(&s_shell, 1);           // abre no JOGO
    lv_screen_load(scr);
    return KIT_OK;
}

KIT_TOOL_EXPORT void tool_destroy(void)
{
    // zere TODA struct kit_ui_* junto com os ponteiros LVGL (erro nº9)
    s_shell = (kit_ui_shell_t){0};
    s_dur   = (kit_ui_chips_t){0};
    s_api = NULL;
}
```

### Regras que valem pra todos

| # | Regra |
|---|---|
| 1 | `kit_ui_bind(api)` **antes** de qualquer helper de áudio/rng/energia/exit. |
| 2 | As structs (`kit_ui_shell_t`, `kit_ui_chips_t`, `kit_ui_sigla_t`, `kit_ui_action_t`) são **suas** — declare `static`, zere no `tool_destroy`. |
| 3 | Arrays passados (`labels` da grade de chips) precisam durar tanto quanto a tela: `static const char *const[]`. |
| 4 | O `on_select` / `on_page` roda **depois** de o visual e o `kit_ui_click()` já terem acontecido — faça ali só persistência/efeito colateral. |
| 5 | Budget de objetos LVGL: o pool de 64 KB é dividido com o Launcher. A grade de chips gasta ~4 objetos por opção; com muitas opções, prefira um stepper (v2). |

---

## Tier 1 — primitivas

Substituem o vocabulário que **toda** Tool reescreve (`add_label`/`lbl`,
`plain_box`/`pane`, `rect`, `tap`, `flex`, `sfx_*`, `coin`, `rnd`).

| Função | Assinatura | Faz |
|---|---|---|
| `kit_ui_bind` | `void (const kit_api_table_t *api)` | Liga a API pros helpers abaixo. |
| `kit_ui_on` | `uint32_t (uint32_t accent)` | Cor de texto sobre superfície cheia (`ON_YELLOW` no amarelo, `ON_COLOR` no resto). |
| `kit_ui_box` | `lv_obj_t *(lv_obj_t *parent)` | Container de layout invisível, sem scroll. |
| `kit_ui_label` | `lv_obj_t *(parent, txt, color, font, letter_space)` | Label numa chamada. |
| `kit_ui_text` | `lv_obj_t *(parent, txt, color, font, width)` | Bloco de texto que quebra em `width`, centrado. |
| `kit_ui_rect` | `lv_obj_t *(parent, w, h, bg, radius)` | Retângulo arredondado opaco. |
| `kit_ui_tap` | `lv_obj_t *(o, cb, code)` | CLICKABLE + feedback + `ext_click_area(8)` + `CLICKED` cb com `code` inteiro. |
| `kit_ui_flex` | `void (o, flow, main_align, pad_row, pad_col)` | Flex numa chamada (cruzado/track = CENTER). |
| `kit_ui_sfx` / `kit_ui_beep` | `void (kit_sfx_t)` / `void (hz, ms)` | Áudio via a API ligada. |
| `kit_ui_click` / `kit_ui_confirm` | `void (void)` | `KIT_SFX_CLICK` / `KIT_SFX_CONFIRM`. |
| `kit_ui_miss` | `void (void)` | Erro sem estridência: A4→D4 curtas (o padrão do Quadrado). |
| `kit_ui_rnd` / `kit_ui_coin` | `int (lo, hi)` / `bool (void)` | RNG inteiro via `api->random` (o loader não resolve `rand`). |
| `kit_ui_keep_awake` | `void (bool)` | `api->power->keep_awake`. |
| `kit_ui_exit` | `void (void)` | Encerra a Tool (não retorna). |

Origem: `io.github.jcrvlh.fora` (`pane`/`rect`/`tap`/`flex`/`lbl`),
`io.github.jcrvlh.bolaquadrado` (`sfx_miss`, `coin`).

---

## Tier 2 — esqueleto Ajuste / Jogo / Como Joga

### `kit_ui_shell_t` — titlebar + tileview

```
┌─────────────────────────────────────────┐
│  ┌──┐                                    │   titlebar (88 px)
│  │ ◄│  QUADRADO              ● ──  ·      │   chip ← + título mono_26 + N dots
│  └──┘                        (ativo cresce e pega a cor)
├─────────────────────────────────────────┤
│                                         │
│   AJUSTE  ◄────►  JOGO  ◄────►  COMO     │   lv_tileview horizontal
│                                  JOGA    │   tiles[0]   tiles[1]   tiles[2]
│                                         │
└─────────────────────────────────────────┘
```

| Função | Assinatura | Faz |
|---|---|---|
| `kit_ui_shell_begin` | `void (sh, screen, title, accent, page_count)` | Monta a titlebar (chip ← = sai da Tool, título, dots). |
| `kit_ui_shell_tiles` | `void (sh, on_page, user)` | Cria o `lv_tileview` e `page_count` tiles → `sh->tiles[i]`. `on_page` pode ser NULL. |
| `kit_ui_shell_open` | `void (sh, page)` | Pula pro tile (sem anim) + sincroniza dots + `on_page`. **Abra sempre no 1 (JOGO).** |
| `kit_ui_shell_active` | `int (sh)` | Índice do tile ativo. |
| `kit_ui_shell_lock` | `void (sh, locked)` | `locked`: prende no JOGO, desliga swipe, esconde os dots. Pra Tool que trava a navegação durante a partida (origem: `set_swipe()` do Fora). |

`on_page(idx, user)` roda a cada troca de página (depois dos dots) — use pra
re-sincronizar a aba que entrou (ex: recarregar highscores).

- **Origem:** `build_titlebar` (12 Tools, idêntico) + `build_tileview` +
  `sync_dots` (`io.github.jcrvlh.telefonema` tem a versão com o dot que cresce).
- **Cor:** `accent` é a cor do card da Tool na Home — some no botão de ação e
  no dot ativo, nada mais.

### `kit_ui_help_page` — página COMO JOGA

```c
void kit_ui_help_page(lv_obj_t *tile, const char *heading, const char *body);
```

Cabeçalho `kit_mono_26` CAIXA ALTA + corpo `kit_sans_28` `LONG_WRAP`, rola na
vertical, sem wrap box. `body` aceita `\n`; numere os passos (`1.`, `2.`…).
Origem: `build_page_help` (`io.github.jcrvlh.mimica` / `.bolaquadrado`).

### `kit_ui_action_t` — botão de ação do rodapé

```c
void kit_ui_action_button(kit_ui_action_t *a, lv_obj_t *game_tile, uint32_t accent, lv_event_cb_t cb);
void kit_ui_action_set (kit_ui_action_t *a, const char *label);   // COMEÇAR / SALVAR / JOGAR DE NOVO
void kit_ui_action_show(kit_ui_action_t *a, bool show);            // esconde durante a rodada
```

Pílula na cor da Tool, ancorada no rodapé. **`game_tile` tem que ser
`sh->tiles[1]`**, não a screen — senão flutua sobre AJUSTE/COMO JOGA (erro nº5
do design-language). Origem: `build_game_action_btn`
(`io.github.jcrvlh.bolaquadrado`).

---

## Tier 2 — ⭐ Grade de chips (seletor segmentado)

O controle da página AJUSTE. Copiado **idêntico** de Telefonema → Vira Certo →
Quadrado; agora é uma chamada.

```
 TEMPO DA PARTIDA
┌───────────────┐ ┌───────────────┐
│      15S      │ │   ▉ 30S ▉     │   ativo = cor da Tool + texto ON_*
└───────────────┘ └───────────────┘   84 px de altura, no máx. 2 por linha
┌───────────────┐
│      60S      │                     3ª opção quebra pra linha nova
└───────────────┘
```

```c
void kit_ui_chips(kit_ui_chips_t *c, lv_obj_t *parent,
                  const char *const *labels, int count, int initial,
                  uint32_t accent, void (*on_select)(int idx, void *user), void *user);

void kit_ui_chips_select(kit_ui_chips_t *c, int idx);   // só visual+estado (sem som/callback)
```

- **Estado:** `c->selected`.
- `on_select` roda no toque, depois do visual e do `kit_ui_click()`.
- `kit_ui_chips_select` é seguro no init e em re-syncs (ex: quando outro ajuste
  muda o recorde exibido).
- **Máx. 8 opções** (`KIT_UI_MAX_CHIPS`). Muitas opções → stepper (v2).
- **Origem:** `build_chip_grid()` + `sync_chip_selection()` de
  `io.github.jcrvlh.telefonema`.

---

## Tier 3 (referência) — ⭐ Seletor de SIGLA

Entrada de 3 iniciais (highscore, nome de jogador). **Toque** numa caixa
avança uma letra; **arraste** pra cima/baixo gira como roleta; **REDEFINIR**
volta pra `AAA`.

```
   ┌────┐  ┌────┐  ┌────┐
   │    │  │    │  │    │     92 × 96, kit_display_72
   │ A  │  │ B  │  │ C  │     arraste ↕ = roleta  ·  toque = +1 letra
   │    │  │    │  │    │
   └────┘  └────┘  └────┘
        ┌─────────────┐
        │  REDEFINIR  │       volta pra AAA
        └─────────────┘
```

```c
typedef struct {
    bool        allow_blank;     // letra pode ser ' ' (mostra "-"); ciclo ' '→A…Z→' '
    const char *reset_label;     // NULL → "REDEFINIR" (ou "APAGAR" se allow_blank)
    void      (*on_change)(const char *letters, void *user);   // a cada letra + no reset
    void       *user;
} kit_ui_sigla_opts_t;

void kit_ui_sigla(kit_ui_sigla_t *s, lv_obj_t *parent, uint32_t accent,
                  const char *initial, const kit_ui_sigla_opts_t *opts);   // opts pode ser NULL
void kit_ui_sigla_set(kit_ui_sigla_t *s, const char *letters);   // pré-preencher (não dispara on_change)
const char *kit_ui_sigla_get(kit_ui_sigla_t *s);                 // "ABC" / "AB " — pra persistir
void kit_ui_sigla_show(kit_ui_sigla_t *s, bool show);            // ex: só quando entra no top-5
void kit_ui_sigla_scroll_lock(kit_ui_sigla_t *s, lv_obj_t *obj, lv_dir_t restore);  // 1 por eixo a congelar
void kit_ui_sigla_feed_touch(kit_ui_sigla_t *s, const kit_input_event_t *ev);
```

- **`allow_blank`** (Fora): a sigla é opcional — jogador sem sigla = "JOGADOR N".
  Sem `on_change` (bolaquadrado): passa `NULL` em `opts` e lê `_get()` só quando
  o SALVAR é apertado.

### O arraste precisa do toque bruto

O SDK não expõe widget de rolagem, então a roleta lê o stream de toque do
input. A Tool registra **um** callback (`api->input->register_callback` — um
por Tool) e repassa cada evento:

```c
static void on_touch(const kit_input_event_t *ev, void *u) {
    (void)u;
    kit_ui_sigla_feed_touch(&s_sigla, ev);   // ignora tudo enquanto nenhuma caixa é arrastada
}
// no tool_init, depois de montar a UI:
if (s_api->input) s_api->input->register_callback(on_touch, NULL);
// no tool_destroy:
if (s_api && s_api->input) s_api->input->register_callback(NULL, NULL);
```

As caixas não encadeiam scroll pro pai (chain HOR+VER removidos), então o
arraste já não vaza pra nenhum ancestral. Como reforço, chame
`kit_ui_sigla_scroll_lock()` uma vez por container que ainda possa se mexer
durante o toque — cada um com a direção pra restaurar ao soltar:

```c
kit_ui_sigla_scroll_lock(&s_sigla, pagina_ajuste, LV_DIR_VER);   // a página rola na vertical
kit_ui_sigla_scroll_lock(&s_sigla, s_shell.tv,    LV_DIR_HOR);   // o tileview desliza na horizontal
```

**Tolerância:** um micro-arraste (< 12 px) ainda conta como toque; acima
disso, o toque do soltar é ignorado (foi arraste). Saltos de coordenada de
uma amostra só — o lixo que chega quando o dedo sai da tela — são
descartados, então soltar não troca a letra sem querer.

- **Persistência é da Tool:** guarde `kit_ui_sigla_get()` no `storage` com suas
  chaves e pré-preencha com `kit_ui_sigla_set()` na próxima abertura.
- **Origem:** `io.github.jcrvlh.bolaquadrado` (`build_game_result` +
  `on_touch` + `step_letter`). A versão só-toque (sem roleta) do
  `io.github.jcrvlh.fora` fica obsoleta — Fora migra pra este seletor.

---

## Roadmap

| Componente | Origem | Nota |
|---|---|---|
| Overlay de tela cheia | `io.github.jcrvlh.estouro` (piscante) / `.testa` (passar a vez) | painel na cor da Tool cobrindo o tileview entre rodadas |
| Tabela de highscore | `io.github.jcrvlh.bolaquadrado` | top-5, `rank+sigla / pontos`, #1 em accent, seções por modo |
| Stepper `[ ‹ valor › ]` | `io.github.jcrvlh.fora` (`stepper`) / `.estouro` (`dim_step` no limite) | mais leve que a grade de chips |
| Barra de status | `io.github.jcrvlh.bolaquadrado` / `.viracerto` | placar / tempo / recorde num `SPACE_BETWEEN` |
| Contagem 3·2·1 | `io.github.jcrvlh.telefonema` / `.viracerto` | preroll |

### Migração das Tools existentes

Só as **novas** usam `kit_ui.h` daqui pra frente. As 13 existentes migram
quando forem mexidas.

- **`io.github.jcrvlh.fora`** — feita (adota `kit_ui_shell` + `kit_ui_shell_lock`
  + `kit_ui_help_page` + `kit_ui_sigla` no modo `allow_blank`, no lugar das 3
  caixas de letra manuais). Mantém os `stepper()` próprios de propósito
  (orçamento de objetos LVGL — chip grid estouraria). Build native + xtensa OK,
  197 testes de lógica passam; falta validar no HW.

### Screenshots

PNGs 1:1 de cada componente vão em `docs/marketing/` pela pipeline da skill
`kit-marketing-assets` quando cada um for reconstruído em HTML — pendente.
