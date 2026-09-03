# Mímica

**Mímica** é um mini-jogo de mesa: uma pessoa segura o KIT, vê a **palavra ou
frase** grande no centro (com a **categoria** acima) e a **representa por
gestos** — sem falar, sem fazer som, sem mexer a boca formando a palavra, sem
apontar objeto da sala, sem soletrar no ar. O time dela adivinha. Acertou, toca
em **ACERTOU** e cai a próxima carta; travou, toca em **PULAR**.
Quando o **TEMPO** acaba, o KIT mostra quantas ela fez — mas quem valida cada
palpite é a roda.

> **Tool do catálogo**, não built-in do Core. Vive em
> [`kit-tools`](https://github.com/jcrvlh/kit-tools/tree/main/tools/io.github.jcrvlh.mimica)
> como `io.github.jcrvlh.mimica` (pacote `.kit`, carregado do cartão microSD
> pelo [`kit_tool_loader`](../architecture/tools.md)). Nasceu como componente
> built-in `kit_mimica` para iteração rápida e saiu do Core quando estabilizou.
>
> O Core mantém: o ícone geométrico da Home (`TOOL_ICON_MIMICA` — uma figura
> gesticulando) e o mapa `"mimica"` em `icon_from_name`.

Card **azul** na grade da Home, marcada como **mini-jogo** (`is_game`). Texto
sobre o azul = paper (`KIT_COLOR_ON_COLOR`).

> **Estrutura e ajustes espelham a [Veto](veto.md)** (mesma família de mini-jogo
> de mesa cronometrado). Diferenças: aqui não há palavras proibidas nem botão de
> falta — só `ACERTOU` e `PULAR` — e a carta tem uma **categoria** no lugar das
> proibidas. Antes do relógio arrancar há um **preparo de 3 s**.

Filosofia do Bingo: **o KIT sorteia e cronometra, a mesa confere.** O placar da
vez (acertos / pulos) fica **escondido** enquanto ela corre — só a barra de
tempo aparece — e é revelado no overlay de TEMPO.

> **Não confundir com a [Testa](testa.md) (Heads Up).** Ali quem segura o KIT
> **adivinha** com o aparelho na testa e inclina pra acertar/passar. Aqui quem
> segura **atua** e vê a palavra.

---

## Tela

Titlebar fixa + `lv_tileview` horizontal de **3 páginas**
(`AJUSTE ◄──► JOGO ◄──► COMO JOGA`, começa no JOGO). Durante o preparo e a vez o
arraste entre páginas fica travado.

### Página 0 — AJUSTE

Corpo rolável, seletores de pílula (rótulo de campo em `kit_mono_20` apagado,
pílulas de 66 px em `kit_mono_26`):

| Campo | Opções | Efeito |
|---|---|---|
| **TEMPO** | `60S` · `90S` · `120S` · `OFF` | Duração da vez. `OFF` = sem relógio; a vez encerra na faixa `ENCERRAR VEZ` no topo do palco. |
| **CATEGORIA** | `MOSTRA` · `ESCONDE` | Se o kicker acima da palavra mostra a categoria (`AÇÃO`, `OBJETO`, `ANIMAL`…) ou só `ATUE`. |
| **BARALHO** | `FÁCIL` · `TUDO` | `FÁCIL` = concreto (ações, objetos, animais, profissões, lugares, esportes). `TUDO` acrescenta personagens, expressões (`CHUTAR O BALDE`) e emoções (`COM CIÚMES`). |
| **PULOS** | `1` · `2` · `3` · `LIVRES` · `OFF` | Limite de `PULAR` por vez (2 linhas de pílulas). `OFF` esconde o botão; com limite, ele mostra quantos restam e some ao esgotar. |

E o botão contornado vermelho **REINICIAR BARALHO** (dois toques,
`TOCAR DE NOVO PARA EMBARALHAR`) + a nota `EMBARALHA O MONTE. AS CARTAS NÃO SÃO
SALVAS: AO REABRIR, PODEM REPETIR.`

Tudo persiste no Storage (`mi_tempo` / `mi_categ` / `mi_deck` / `mi_pulos`). O
progresso do monte fica só na RAM.

### Página 1 — JOGO

O palco (**não tocável**) com:

- a **barra de tempo** azul (trilho `KIT_COLOR_SURFACE`, largura cheia) sob a
  titlebar, drenando da esquerda pra direita, com o `M:SS` em `kit_mono_16` à
  direita. Nos últimos 10 s a barra **pisca** (opacidade) e o relógio acende;
  nos últimos 5 s toca `KIT_SFX_TIMER_TICK` a cada segundo. No modo `OFF` a
  barra some e no lugar dela fica a faixa contornada `ENCERRAR VEZ`;
- o **kicker** em `kit_mono_16` apagado: a categoria (se `MOSTRA`), `ATUE` (se
  `ESCONDE`), `PREPARE-SE 3 · 2 · 1` no preparo, `MÍMICA` no ocioso;
- a **palavra / frase** em `kit_display_44` **CAIXA ALTA** (< 10 caracteres) ou
  `kit_sans_28` que quebra linha (frase longa), em azul.

Ao apertar **COMEÇAR**, um **preparo de 3 s** (`PREPARE-SE 3 · 2 · 1` no kicker)
dá tempo de ler e planejar antes de o relógio arrancar. Um toque no botão
primário durante o preparo pula a contagem.

No ocioso o palco mostra só `TOQUE EM COMEÇAR` apagado.

**Linha de ações** (fixa no rodapé, `74` de altura): **PULAR** contornado
(`kit_mono_20`, some quando o limite esgota) + **ACERTOU** cheio azul
(`kit_mono_26`), lado a lado — `flex_grow` 1 : 2. No ocioso é um único
**COMEÇAR** azul.

### Página 2 — COMO JOGA

Corpo rolável: cabeçalho `COMO JOGA` em `kit_mono_26` + as regras num **único
rótulo `kit_sans_28`** (caixa normal, quebra linha) — os passos numerados e o
"quem decide é a mesa".

### Overlay — TEMPO

Cobre o palco em **azul cheio**: `TEMPO` (ou `FIM`, se encerrada no `OFF`) em
`kit_display_72`, o placar da vez `ACERTOS n` / `PULOS n` em `kit_mono_26` (duas
linhas), e o botão contornado **PASSAR A VEZ**. O alarme (`KIT_SFX_TIMER_DONE`)
re-toca a cada ~3,4 s com um _strobe_ do fundo (azul ↔ `#4551E0`); um toque em
qualquer lugar do overlay cala o alarme.

---

## Execução

A ação principal é **sensível ao estado** (botão primário no rodapé da página
JOGO):

| Estado | Botão primário |
|---|---|
| **OCIOSO** | `COMEÇAR`: zera o placar da vez, puxa a 1ª carta, roda o preparo de 3 s. |
| **PREPARO** | Pula a contagem e começa a vez agora. |
| **EM JOGO** | `ACERTOU`: `acertos++`, som de acerto, próxima carta. |
| **TEMPO** | `PASSAR A VEZ`: volta ao ocioso. |

**PULAR** (`pulos++`, a carta volta ao monte numa posição aleatória à frente;
respeita o limite) é um botão próprio — não entra na ação principal.

Como Tool do catálogo, a Mímica não recebe o botão físico **PWR** (só built-ins
têm `primary_action`) nem registra callback de **chacoalhar** — a mecânica é
toque: quem atua gesticula muito e um "chacoalhão" viraria `ACERTOU` acidental.

`kit_power.keep_awake(true)` fica ligado do preparo até o overlay; volta a
`false` no ocioso e ao sair.

### A barra de tempo

Um único `lv_timer` de 200 ms (`clock_cb`) recalcula a fração restante, ajusta a
**largura** de `s_bar_fill` (`lv_obj_set_width` — barato, sem layer), escreve o
`M:SS` só quando o segundo vira, e no fim (`now >= deadline`) chama `time_up()`.
No modo `OFF` não há `lv_timer` — a vez só encerra pela faixa `ENCERRAR VEZ`.

---

## Baralho

Fixo na Tool (`DECK[]` em `src/main.c`, ~124 cartas `{ texto, categoria }`).
Categorias FÁCIL: `AÇÃO` `OBJETO` `ANIMAL` `PROFISSÃO` `LUGAR` `ESPORTE`; `TUDO`
acrescenta `PERSONAGEM` `EXPRESSÃO` `EMOÇÃO` (`card_hard()` = categoria ≥
`CAT_PERSONAGEM`).

Saco embaralhado sem reposição (Fisher-Yates via [Random API](../api/random.md) /
TRNG); reembaralha ao esgotar e nunca abre repetindo a última carta. `PULAR`
(`deck_requeue`) devolve a carta ao monte numa posição sorteada à frente sem
crescer o array. Trocar o `BARALHO` ou tocar `REINICIAR BARALHO` embaralha na
hora.

Curadoria: personagens e figuras ficam em domínio público / conhecimento comum
(`CHAPEUZINHO VERMELHO`, `PINÓQUIO`, `BRANCA DE NEVE`, `PIRATA`, `ROBÔ`) — nada
de marca registrada. **Sem em-dash `—`** (glifo ausente nas fontes do KIT).

---

## Áudio

| Evento | Som |
|---|---|
| `COMEÇAR` / início da vez | `KIT_SFX_CONFIRM` |
| preparo (3 · 2 · 1) | `beep()` curto por contagem |
| `ACERTOU` | `KIT_SFX_VETO_HIT` — duas notas subindo depressa, curtas |
| `PULAR` | `KIT_SFX_CLICK` |
| últimos 5 s | `KIT_SFX_TIMER_TICK` (1×/s) |
| `TEMPO` | `KIT_SFX_TIMER_DONE`, re-tocado a cada ~3,4 s no overlay |
| `REINICIAR BARALHO` | `beep()` de confirmação |

---

## Navegação

- **BOOT** e o chip de voltar da titlebar saem pela API (`system->exit`).
- Sem estado de vez persistente: ao reabrir, começa em `TOQUE EM COMEÇAR` (o
  ajuste volta do Storage).

---

## Ciclo de vida

| Função | Efeito |
|---|---|
| `tool_init(ctx)` | Guarda `ctx->api`, monta a tela e carrega (`accent` azul). |
| `tool_destroy()` | Derruba os `lv_timer`, solta o `keep_awake` e os objetos LVGL. |

Só usa `kit_api` (áudio por `beep`/`sfx`, `random`, `time`, `power`, `storage`,
`system`, `display`, `input`) — sem `imu`. A ação principal liga num botão da
própria Tool, não no PWR.
