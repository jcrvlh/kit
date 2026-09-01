# Globo de Bingo

A Tool **Globo de Bingo** é um globo de bingo digital: sorteia números sem
repetir e guarda o **painel de chamadas** para conferir a cartela. O KIT sorteia,
o papel confere.

É a realização da entrada **Bingo Tool** da **Fase 2 (Game Night)** do roadmap do
projeto. Tool interna (built-in no Core), componente
`kit_bingo`, id `com.kit.bingo`, despachada pelo
[`kit_tool_manager`](../architecture/tools.md). Card **verde** na grade da Home
(ícone `TOOL_ICON_BINGO` — quatro pontos). Texto sobre o verde = paper
(`KIT_COLOR_ON_COLOR`).

---

## Telas

`lv_tileview` horizontal de **3 páginas** (`AJUSTE ◄──► GLOBO ◄──► CHAMADAS`), no
mesmo idioma da [Sortear Times](times.md) e da Dice Tool.
Começa no **GLOBO** (três pontos de página, o do meio aceso ao abrir).

### 0 · AJUSTE (rola na vertical)

| Campo | Controle | Opções | Padrão |
|---|---|---|---|
| `FAIXA` | 2 pílulas segmentadas | `1-75` · `1-90` | **1-75** |
| `RODADA` | botão `REINICIAR SORTEIO` (contornado, vermelho) | — | — |

- **1-75** é o bingo americano: mostra a **letra da coluna** (B 1–15, I 16–30,
  N 31–45, G 46–60, O 61–75). **1-90** é o bingão: só o número.
- **`REINICIAR SORTEIO`** pede **dois toques** (o rótulo vira `TOCAR DE NOVO PARA
  ZERAR` e o botão fica vermelho cheio por 4 s). Zera o painel de chamadas e volta
  o globo pro começo. **Trocar a faixa também zera.**

### 1 · GLOBO (o palco)

- **Titlebar** fixa: chip de voltar (`KIT_ICON_BACK`) + título `BINGO`
  (`kit_mono_26`) + os três pontos de página.
- **Número sorteado** grande em `kit_display_120` (só dígitos — a fonte não tem
  letras), na cor da Tool. A **letra da coluna** (`kit_mono_26`, cor da Tool)
  aparece acima no modo 1–75. Antes do primeiro sorteio: um `-` apagado.
- Linha **`ANTERIOR · G 52`** (`kit_mono_16` apagado) — o penúltimo número, pra
  quem chegou atrasado marcar. Some quando há menos de dois sorteios.
- Contador **`12 / 75`** e a dica (`SORTEAR · PWR · CHACOALHAR` no início,
  `TOQUE PARA SORTEAR` depois).
- Quando a faixa acaba: o número vira **`FIM`** (`kit_display_72`), a dica vira
  `FAIXA COMPLETA · REINICIE NO AJUSTE` e o botão `SORTEAR` fica esmaecido e
  inerte.
- **Botão `SORTEAR`** fixo no rodapé, na cor da Tool (`kit_mono_26`).

### 2 · CHAMADAS (o painel, rola na vertical)

Cabeçalho `SORTEADOS 12 / 75` (`kit_mono_16` apagado) + um toggle de **duas
visões** (pílulas `LISTA` / `GRADE`), persistido em Storage (`bingo_view`).
Padrão: **LISTA**.

#### LISTA — a visão de conferência da tela de 1,8"

Mostra **só os números que já saíram**, agrupados e em corpo grande
(`kit_mono_26`) — é bem mais fácil de bater o olho do que caçar as células
acesas numa grade de 90.

- **1-75:** 5 linhas, uma por letra (`B` `I` `N` `G` `O` em `kit_mono_26` na cor
  da Tool), com os sorteados daquela coluna em ordem crescente ao lado.
- **1-90:** 9 linhas por dezena (prefixo `01` `11` … `81` em `kit_mono_16`
  apagado).
- Linha **sem nenhum número** = `·` apagado. Linha que **contém o último
  sorteado** fica inteira na cor da Tool.

#### GRADE — o painel inteiro da faixa

**Um único widget `lv_table`** — as células são desenhadas, não são objetos
(montar 75–90 `lv_obj` aninhados estourava o layer do LVGL no CO5300 e **travava
a placa ao abrir a Tool**). A aparência de cada célula sai do hook
`LV_EVENT_DRAW_TASK_ADDED` (`table_draw_cb`), que lê `s_in[]` ao vivo:

- **sorteado:** fundo na cor da Tool, número em **preto** (`KIT_COLOR_BG`) — na
  tela de 1,8" o preto sobre o verde tem contraste bem melhor que o paper;
- **não sorteado:** fundo `KIT_COLOR_SURFACE`, número apagado;
- **último sorteado:** `border` de 3 px em `KIT_COLOR_TEXT` (os demais têm um fio
  de 1 px em `KIT_COLOR_LINE` de grade).
- **1-75:** 5 colunas sob os rótulos `B I N G O` (`kit_mono_20`), 15 linhas.
- **1-90:** 9 colunas (1–10, 11–20, …, 81–90), 10 linhas, sem rótulo.

As duas visões são repovoadas ao trocar a faixa e ao reiniciar; cada sorteio
`lv_obj_invalidate` a tabela (o hook repinta) e recompõe os rótulos da LISTA.

---

## Execução e animação

A ação principal (`kit_bingo_draw`) pode ser disparada por:

- botão **`SORTEAR`** no rodapé;
- **toque em qualquer lugar do palco** do globo;
- botão físico **PWR** e o gesto de **chacoalhar** (via
  [`kit_runtime`](../architecture/runtime.md),
  `kit_runtime_set_tool_primary_action`).

O sorteio é **sem reposição**: o número é escolhido antes da animação, entre os
que ainda não saíram, pela [Random API](../api/random.md) (TRNG de hardware).
Sem efeito se um sorteio já estiver em curso ou se a faixa tiver acabado (nesse
caso, 1 bipe grave).

Ao disparar, um **único `lv_timer`** (`SHUF_TICK_MS` 60 ms × `SHUF_TICKS` 10)
embaralha o texto do número por ~0,6 s e trava no sorteado — cor da Tool + **1
bipe** (`beep(880, 35)`). Só troca de texto por tick — **nada de**
`transform_scale` / `transform_rotation` (estoura o render no CO5300/PSRAM).

---

## Persistência

Via [Storage API](../api/storage.md):

| Chave | Tipo | Conteúdo |
|---|---|---|
| `bingo_range` | `i32` | faixa (75 ou 90) |
| `bingo_drawn` | `str` | números sorteados, em ordem, separados por vírgula |
| `bingo_view` | `i32` | visão da página CHAMADAS (0 = LISTA, 1 = GRADE) |

**A rodada persiste** — diferente da [Sortear Times](times.md), que não tem
histórico. Um jogo de bingo dura e o usuário pode sair da Tool no meio (ver as
horas, atender). Ao reabrir, faixa e painel voltam como estavam. O risco de
começar a próxima rodada com o painel sujo fica coberto pelo contador sempre
visível e pelo `REINICIAR` no Ajuste (e trocar a faixa zera).

---

## Navegação

- **BOOT** volta para a Home fechando a Tool (`kit_system_exit_impl`).
- O chip de voltar da titlebar sai pela API (`system->exit`).

---

## Ciclo de vida

| Função | Efeito |
|---|---|
| `kit_bingo_start(accent)` | Monta as três páginas, carrega o Storage e a tela. `accent` 0 → verde padrão. |
| `kit_bingo_draw()` | Ação principal — sorteia o próximo número. Ligada ao PWR/chacoalhar pelo Runtime. |
| `kit_bingo_destroy()` | Derruba os `lv_timer` e os objetos LVGL. |

---

## Fora de escopo

Sem cartelas na tela, sem locução de voz, sem "chance de bingo". O escopo da
Fase 2 pede "globo digital 1–75/1–90 com histórico dos números sorteados" — nada além.
