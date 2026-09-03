# Veto

**Veto** é um mini-jogo de mesa: uma pessoa segura o KIT, vê a **palavra-alvo**
grande no centro e a **descreve** para o time adivinhar — sem dizer a palavra
nem nenhuma das **proibidas** listadas abaixo dela (nem pedaços ou traduções).
Acertou, toca no **JOINHA (👍)** e cai a próxima carta; falou uma proibida, toca no
**DISLIKE (👎)** e a **cigarra** dispara. Quando o **TEMPO** acaba, o KIT mostra quantas
ela acertou — mas quem valida cada palpite é a roda.

> **Tool do catálogo**, não built-in do Core. Vive em
> [`kit-tools`](https://github.com/jcrvlh/kit-tools/tree/main/tools/io.github.jcrvlh.veto)
> como `io.github.jcrvlh.veto` (pacote `.kit`, carregado do cartão microSD
> pelo [`kit_tool_loader`](../architecture/tools.md)). Nasceu como componente
> built-in `kit_veto` para iteração rápida e saiu do Core quando estabilizou.
>
> O Core mantém: o ícone geométrico da Home (`TOOL_ICON_VETO` — a carta com palavra-alvo
> e marcadores quadrados vermelhos) e o mapa `"veto"`.

Card **amarelo** na grade da Home (`TOOL_ICON_VETO`), marcada como **mini-jogo** (`is_game`). Texto sobre o amarelo = preto (`KIT_COLOR_ON_YELLOW`).

Filosofia do Bingo: **o KIT sorteia e cronometra, a mesa confere.** O placar da
vez (acertos / erros / pulos) fica **escondido** enquanto ela corre — só a barra
de tempo aparece — e é revelado no overlay de TEMPO. Nada de placar acumulado
nem times no aparelho.

---

## Tela

Titlebar fixa + `lv_tileview` horizontal de **3 páginas**
(`AJUSTE ◄──► JOGO ◄──► COMO JOGA`, começa no JOGO) + rodapé fixo. Durante a vez
o arraste entre páginas fica travado (o JOGO é a única página).

### Página 0 — AJUSTE

Corpo rolável na vertical, seletores de pílula (rótulo de campo em
`kit_mono_16` apagado):

| Campo | Opções | Efeito |
|---|---|---|
| **TEMPO** | `60S` · `90S` · `120S` · `OFF` | Duração da vez. `OFF` = modo livre sem relógio; a vez encerra na faixa `ENCERRAR VEZ` no topo do palco. |
| **PROIBIDAS** | `2` · `3` | Quantas palavras proibidas mostrar (2 = mais fácil). Vale na próxima carta. |
| **BARALHO** | `FÁCIL` · `TUDO` | `FÁCIL` = coisas concretas (`CACHORRO`, `PRAIA`, `PIZZA`). `TUDO` inclui as abstratas (`SAUDADE`, `INVEJA`, `DESTINO`). |
| **PULOS** | Linha 1: `1` · `2` · `3`<br>Linha 2: `LIVRES` · `OFF` | Limite de `PULAR` por vez. `OFF` esconde o botão; esgotado, ele se recolhe da barra. |

E o botão contornado vermelho **REINICIAR BARALHO** (dois toques) — reembaralha
o monte sem repetição.

Tudo persiste no Storage (`vt_tempo` / `vt_proib` / `vt_deck` / `vt_pulos`). O
progresso do monte fica só na RAM (uma sessão de festa é uma sentada só).

### Página 1 — JOGO

O palco (**não tocável** — sem toque acidental) com:

- a **barra de tempo** amarela sob a titlebar, drenando da esquerda pra direita
  ao longo da vez, com o `M:SS` em `kit_mono_16` à direita. Nos últimos 10 s a
  barra pisca e o relógio acende; nos últimos 5 s toca `KIT_SFX_TIMER_TICK` a
  cada segundo. No modo `OFF` a barra some e no lugar dela fica a faixa
  contornada `ENCERRAR VEZ`;
- a **palavra-alvo** em `kit_display_44` **CAIXA ALTA** amarela centralizada com
  espaço generoso;
- as **proibidas** em **sequência horizontal** (`row-wrap`) com marcador quadrado
  vermelho (`KIT_ICON_SQUARE`) e tipografia `kit_mono_26` — máxima legibilidade
  e destaque no display AMOLED.

Antes de começar, o palco mostra só `TOQUE EM COMEÇAR` apagado.

**Barra de Ações Unificada** (rodapé fixo):
- **DISLIKE (👎)**: botão contornado vermelho com ícone A8;
- **PULAR**: botão intermediário contornado (`s_skip_btn`) com rótulo `PULAR` (oculta quando em `OFF` ou esgotado);
- **JOINHA (👍)**: botão amarelo cheio com ícone A8 preto (`s_go_btn`), ocupando maior largura e espelhado no botão físico **PWR**. No ocioso, expande para largura total com o texto `COMEÇAR`.

### Página 2 — COMO JOGA

Corpo rolável: cabeçalho em `kit_mono_26` + as regras num único `lv_label`
`kit_sans_22` que quebra linha.

### Overlay — TEMPO

Cobre o palco em **amarelo cheio**: `TEMPO` (ou `FIM`, se encerrada no `OFF`)
em `kit_display_72`, o placar da vez `ACERTOS n` / `ERROS n` / `PULOS n` em mono
26 preto (`kit_mono_26`), e o botão contornado preto **PASSAR A VEZ**. O alarme (`KIT_SFX_TIMER_DONE`)
re-toca a cada ~3,4 s com um _strobe_ suave do fundo.

---

## Execução

`kit_veto_action()` é a ação principal e é **sensível ao estado**:

| Estado | Botão primário / **PWR** |
|---|---|
| **OCIOSO** | `COMEÇAR`: zera o placar, puxa a 1ª carta, arranca o relógio. |
| **EM JOGO** | `JOINHA (👍)`: `acertos++`, som de acerto, próxima carta. |
| **TEMPO** | `PASSAR A VEZ`: volta ao ocioso (próxima pessoa descreve). |

**DISLIKE (👎)** (`erros++` + a cigarra + próxima carta) e **PULAR** (`pulos++`, a carta
volta ao monte numa posição aleatória à frente; respeita o limite de PULOS) são
botões próprios na mesma barra — não entram na ação principal.

O **chacoalhar não é ligado** nesta Tool: quem descreve gesticula muito e um
"chacoalhão" viraria acerto acidental.

`kit_power.keep_awake(true)` fica ligado em jogo e no overlay (uma vez de 120 s
passa do tempo de repouso da tela); volta a `false` no ocioso e ao sair.
