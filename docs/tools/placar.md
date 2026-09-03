# Placar

**Placar** é um placar de mesa para qualquer jogo: de 2 a 4 jogadores lado a
lado, cada um numa coluna grande com a pontuação em fonte de display. Um **toque**
na coluna soma 1; **segurar** subtrai 1. Passo fixo, sem digitar.

Tool interna (built-in no Core), componente `kit_placar`, id `com.kit.placar`,
despachada pelo [`kit_tool_manager`](../architecture/tools.md). Card **verde** na
grade da Home (`TOOL_ICON_PLACAR` — três colunas de placar em alturas
diferentes), marcada como **ferramenta** (não `is_game`).

A partida em andamento **sobrevive a fechar/reabrir a Tool** — pontuação, nº de
jogadores, meta e iniciais persistem no Storage.

---

## Tela

Titlebar fixa + `lv_tileview` horizontal de **3 páginas**
(`AJUSTE ◄──► PLACAR ◄──► COMO USA`, começa no PLACAR). O arraste entre páginas
só trava enquanto o overlay **VENCEU** está na tela.

### Página 0 — AJUSTE

Corpo rolável na vertical:

| Campo | Opções | Efeito |
|---|---|---|
| **JOGADORES** | `2` · `3` · `4` | Quantas colunas o placar mostra. Trocar não apaga a pontuação de quem sai de vista. |
| **INICIAIS** (opcional) | seletor `JOGADOR N` + 3 caixas de letra + `APAGAR` | Padrão da Tool [Fora](https://github.com/jcrvlh/kit-tools): escolhe o jogador, toca cada caixa para **girar a letra** (`vazio → A → … → Z → vazio`). Sem inicial, a coluna mostra `#1`..`#4` na cor do jogador. |
| **META** | `SEM` · `3` · `5` · `10` · `21` · `50` (pílulas em **2 linhas** de 3, altura 68 — botão grande) | Meta **desligada por padrão**. Com meta, cada coluna ganha uma barra de progresso e, ao bater, aparece o overlay `VENCEU`. |

Tudo persiste no Storage (`pl_players` / `pl_meta` / `pl_names`) e volta ao
reabrir.

### Página 1 — PLACAR

O palco: 2 a 4 colunas em `flex` (as escondidas saem do layout). Cada coluna:

- a **inicial** (ou `#N`) em `kit_mono_20`, na cor Bauhaus do jogador — as quatro
  primárias, **sem repetir mesmo com 4 jogadores**: `#1` vermelho, `#2` verde,
  `#3` amarelo, `#4` azul;
- a **pontuação** em fonte de display — `kit_display_120` (2 jogadores) →
  `kit_display_72` (3) → `kit_display_44` (4), caindo um degrau com 3 dígitos ou
  número negativo (aí em `kit_mono_*`, que tem o glifo `-`);
- uma **barra de progresso** da meta no rodapé (só com meta ligada).

**Toque** na coluna = **+1** (`LV_EVENT_SHORT_CLICKED`); **segurar** = **−1**
(`LV_EVENT_LONG_PRESSED`). A coluna dá um **brilho curto** (`bg_opa` na própria
coluna, sem criar layer) — é o feedback rápido. O som usa os **SFX prontos** do
`kit_audio` (envelope de 2 ms, dois tons, amplitude baixa), não um `beep()` cru
(que clicado em série estoura): `KIT_SFX_CLICK` no `+1`, `KIT_SFX_BACK` no `−1`
(um par "avança / volta"), **rate-limitado** a um a cada 60 ms. Faixa `−99 … 999`.

Botão **ZERAR** contornado no rodapé — **dois toques** (o 1º arma e mostra
`TOCA DE NOVO` por 2,5 s; o 2º zera a pontuação). Nº de jogadores, meta e
iniciais ficam.

### Página 2 — COMO USA

Corpo rolável, seis passos — título em `kit_mono_26`, corpo em `kit_mono_20`
(fonte grande, sem glifos fora do range Latin-1 das fontes mono: nada de `→` nem
`−`, que não renderizam). Passos: toque = +1 · segure = -1 · meta opcional ·
iniciais opcionais · zerar com dois toques · a partida não some.

### Overlay — VENCEU

Cobre o palco na **cor do jogador** que bateu a meta: `VENCEU` em
`kit_display_72`, a inicial/`JOGADOR N` em mono, e o botão contornado
`NOVA PARTIDA` (zera e sai do aviso). **Toque fora do botão** = fecha o aviso e
**segue jogando** com a pontuação intacta. O aviso sai **uma vez por jogador**
(`s_announced[]`) — um `ZERAR` (ou trocar a meta) rearma.

---

## Persistência

| Chave | Tipo | Conteúdo |
|---|---|---|
| `pl_players` | i32 | 2–4 |
| `pl_meta` | i32 | índice em `{0,10,21,50,100}` (0 = sem meta) |
| `pl_names` | str | 4 siglas separadas por `/` (ex.: `ANA//JO/`) |
| `pl_score` | str | 4 inteiros separados por `/` (ex.: `7/3/0/0`) — **a partida em andamento** |

Ao abrir, a pontuação carregada que já estiver na meta **não** dispara o aviso
(entra em `s_announced` de saída).

---

## Navegação e energia

- **BOOT** e o chip de voltar da titlebar saem pela API (`system->exit`).
- Sem `primary_action` — o PWR físico não mexe no placar (evita somar ponto pra
  "ninguém" com N jogadores).
- `kit_power.keep_awake(true)` fica ligado a Tool inteira: um placar fica na mesa
  a partida toda e não pode deixar a tela dormir. Volta a `false` ao sair.

---

## Ciclo de vida

| Função | Efeito |
|---|---|
| `kit_placar_start(accent)` | Monta a tela, carrega o Storage e liga o `keep_awake`. `accent` 0 → verde. |
| `kit_placar_destroy()` | Derruba os `lv_timer` (brilho / ZERAR), solta o `keep_awake` e os objetos LVGL. |

---

## Futuro: mover para o catálogo

Como a Pavio, nasce built-in para iteração rápida. A lógica já só usa `kit_api`
(`audio`, `storage`, `power`, `system`) — a conversão para pacote `.kit` do
[catálogo](registry.md) é o manifest + `tool_init/destroy` chamando
`kit_placar_start/destroy`.
