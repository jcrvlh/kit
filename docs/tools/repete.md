# Repete

**Repete** é um jogo de memória de mesa no estilo "Genius" / Simon, com as **três
primitivas do KIT**. O KIT acende uma sequência de formas — círculo (azul),
triângulo (amarelo), quadrado (vermelho) — e o jogador **repete tocando nas
formas**. Acertou a sequência inteira, ela ganha mais uma forma; errou uma,
acabou.

> **Tool do catálogo**, não built-in. Vive em
> [`kit-tools`](https://github.com/jcrvlh/kit-tools/tree/main/tools/io.github.jcrvlh.repete)
> como `io.github.jcrvlh.repete` (pacote `.kit`, carregado do cartão pelo
> [`kit_tool_loader`](../architecture/tools.md)). Nasceu direto no catálogo.
>
> O Core não ganhou nada específico: usa só a superfície de runtime que já
> existia (`lv_image_*`, `lv_obj_set_style_image_recolor{,_opa}`,
> `lv_obj_set_style_translate_x/y`, `lv_timer_*`). `home_icon` = `triangle`
> (`TOOL_ICON_TRIANGLE`, já no launcher). `min_runtime` `0.3.1`.

Card **verde** na grade da Home, marcada como **mini-jogo** (`kind: game`). Texto
sobre o verde = paper (`KIT_COLOR_ON_COLOR`).

Cada forma tem **cor e som fixos**: círculo `KIT_COLOR_BLUE` / 330 Hz (grave),
triângulo `KIT_COLOR_YELLOW` / 523 Hz (médio), quadrado `KIT_COLOR_RED` / 784 Hz
(agudo) — no playback e no toque do jogador.

---

## As formas

Os glifos `KIT_ICON_CIRCLE/TRIANGLE/SQUARE` só existem até `kit_display_44`, e o
triângulo do Font Awesome é um *caret* (não equilátero). Então as três formas são
**máscaras A8 embutidas no `.so`** (padrão da Tool Veto): `scripts/make_shapes.py`
gera `src/shapes.h` com um `lv_image_dsc_t` estático por forma — círculo r=44,
triângulo **equilátero** de lado 88, quadrado 88×88, todas no mesmo box de 88 px.
A cor vem de `image_recolor` em runtime (`recolor_opa` 100 %): forma da cor na
tecla apagada, `ON_*` na tecla acesa.

---

## Tela

Titlebar fixa (chip de voltar + `REPETE` em `kit_mono_26` + 3 pontos de página) +
`lv_tileview` horizontal de **3 páginas** (`AJUSTE ◄──► JOGO ◄──► COMO JOGA`,
começa no JOGO).

### Página 0 — AJUSTE

Um seletor: **MODO**. Três chips (`kit_mono_20`), dois por linha + um embaixo,
tudo alinhado à esquerda:

| Modo | Efeito |
|---|---|
| **Clássico** | Sequência +1 por rodada, velocidade fixa. |
| **Velocidade** | Igual, mas o playback acelera a cada rodada — `lit` 420 → 150 ms, `gap` 170 → 80 ms, com piso. |
| **Inverso** | Repetir a sequência **de trás pra frente**. |

Persiste no Storage (`repete_mode`) e volta ao reabrir. Trocar de modo reinicia
o placar da tela e mostra o recorde daquele modo.

### Página 1 — JOGO

Coluna de **altura cheia** com `flex_align` `SPACE_EVENLY` (nada de grupo
`SIZE_CONTENT` centralizado com offset — os irmãos acabavam cortando as peças) e
`pad_bottom` reservando a faixa do botão:

- **Status** em `kit_mono_26` — `MODO CLASSICO` (ocioso), `RODADA N` / `OLHE`
  (playback), `SUA VEZ`, `CERTO` / `MUITO BEM` (marco), `ERROU` / `NOVO RECORDE`.
- **As 3 teclas** de forma (104 px, raio 22), `keyrow` com
  `LV_OBJ_FLAG_OVERFLOW_VISIBLE` (a tecla acesa sobe 4 px + ganha borda). Acesa =
  fundo na cor da forma; apagada = `KIT_COLOR_SURFACE` com a forma na cor.
- **Placar** em `kit_mono_20` apagado — `RECORDE N` (ocioso) ou
  `VOCE FEZ N  ·  RECORDE M` no fim.
- **Botão** no rodapé, verde: `COMECAR` → `...` (durante o jogo, desabilitado) →
  `JOGAR DE NOVO`.

### Página 2 — COMO JOGA

Corpo rolável na vertical (a flag `LV_OBJ_FLAG_SCROLLABLE` é readicionada — o
`plain_box` do container a remove), cabeçalho `COMO JOGA` em `kit_mono_26` + as
regras num único `kit_sans_28`, passos numerados. Só ASCII (as fontes do KIT não
têm em-dash).

---

## Execução

Máquina de estados `IDLE → PLAYBACK → INPUT → OVER`, tudo em `lv_timer`:

| Estado | O que roda |
|---|---|
| **PLAYBACK** | `pb_tick_cb` alterna fase acesa (`lit_ms`) / apagada (`gap_ms`), acende `s_seq[i]` + `beep`. Input travado. Ao fim → `INPUT`. |
| **INPUT** | Toque numa tecla: compara com `s_seq[pos]` (`pos` invertido no modo Inverso). Acertou tudo → rodada concluída; errou → `game_over`. |
| **Rodada concluída** | Recorde por modo salvo (`repete_hi0/1/2`). A cada 5 rodadas: pisca as 3 formas + `KIT_SFX_CONFIRM`. Sorteia a próxima forma (`random->range(0,2)`), `s_round++`, volta pro `PLAYBACK`. |
| **OVER** | `game_over`: mostra a forma certa acesa, `KIT_SFX_ESTOURO_POP`, **treme a tela** (`shake_tick_cb` deslocando o tileview no eixo X). |

A sequência é sorteada pelo TRNG e **não persiste** — reabrir a Tool começa do
zero; só o recorde de cada modo sobrevive.

`KIT_SFX_TOOL_OPEN` ao apertar COMECAR. `MAX_SEQ` 100 (chegou lá = "MEMORIA
PERFEITA", encerra vitorioso).

---

## Navegação

- Chip de voltar da titlebar → `system->exit()`.
- Sem estado de partida persistente.

---

## Ciclo de vida

| Função | Efeito |
|---|---|
| `tool_init(ctx)` | Carrega prefs, monta a tela, abre no JOGO. `KIT_ERR_NOT_SUPPORTED` se não houver `random`. |
| `tool_destroy()` | Derruba os 4 `lv_timer` (playback, blip, gap, shake), deleta a tela, zera todo ponteiro estático. |
