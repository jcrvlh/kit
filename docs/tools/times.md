# Sortear Times

A Tool **Sortear Times** divide a mesa em times equilibrados sem digitação: o
usuário escolhe **quantas pessoas** e **quantos times**, e o KIT lida as
posições numeradas (`01..N`). Cada um se conta pela roda da mesa — "você é a 1,
você é a 2…" — e acha o seu número.

É a realização do "sorteio de times" da **Fase 2 (Game Night)** do roadmap do
projeto. Tool interna (built-in no Core), componente
`kit_times`, id `com.kit.times`, despachada pelo
[`kit_tool_manager`](../architecture/tools.md). Card **azul** na grade da Home
(ícone `TOOL_ICON_TEAMS` — um quadrado dividido em dois) — a mesma cor da
Bottle Tool, por escolha do usuário (a paleta Bauhaus só tem
quatro primárias). Texto sobre o azul = paper (`KIT_COLOR_ON_COLOR`).

---

## Telas

`lv_tileview` horizontal de **2 páginas** (`AJUSTE ◄──► SORTEIO`), no mesmo
idioma da [Timer Tool](timer.md). Começa no **SORTEIO** (dois pontos de página, o
da direita aceso ao abrir).

### 0 · AJUSTE (rola na vertical)

| Campo | Controle | Faixa | Padrão |
|---|---|---|---|
| `PESSOAS` | botões `-` / `+` + número grande (`kit_display_44`) | 4 a 16 | **4** |
| `TIMES` | 3 pílulas segmentadas | 2 · 3 · 4 | **2** |

Pílula selecionada = fundo na cor da Tool. `TIMES` nunca passa de `PESSOAS`
(com 4 pessoas as três opções valem; o guard só age em cenários futuros).

### 1 · SORTEIO (o palco)

- **Titlebar** fixa: chip de voltar (`KIT_ICON_BACK`) + título `TIMES`
  (`kit_mono_26`) + os dois pontos de página.
- **Estado ocioso:** só tipografia, **sem "wrap box"** — `N TIMES` / `N PESSOAS`
  em `kit_mono_26` no centro e a dica `TOQUE PARA SORTEAR` (`kit_mono_16`
  apagada) abaixo.
- **Botão `SORTEAR`** fixo no rodapé, na cor da Tool (`kit_mono_26`).

---

## Revelação — um a um

O resultado é **sempre** revelado pessoa por pessoa. O KIT passa de mão em mão;
cada toque (ou PWR) mostra, num **overlay de tela cheia na cor do time**, o time
da vez:

```
PESSOA               (rótulo, kit_mono_16)
3                    (kit_display_72 — o número que muda a cada toque)
DE 10                (kit_mono_16)

TIME                 (rótulo, kit_mono_16)
AZUL                 (kit_mono_26)

TOQUE PARA A PRÓXIMA (kit_mono_16)
```

`PESSOA X` é o elemento que mais muda a cada toque — duas pessoas seguidas podem
cair no mesmo time e aí a cor de fundo **não** denuncia a troca. Por isso o
número vai **grande em `kit_display_72`**, com `PESSOA` pequeno em cima e `DE N`
embaixo. Texto em `KIT_COLOR_ON_YELLOW` (preto) sobre amarelo e
`KIT_COLOR_ON_COLOR` (paper) sobre as outras cores. Cada avanço sai **1 bipe
curto** (`beep(760, 25)`).

Ao passar da última pessoa: tela preta, `PRONTO` (`kit_display_72`, azul, a cor
da Tool) + `TOQUE PARA FECHAR` — o toque seguinte fecha o overlay e volta ao estado
ocioso.

> ### Por que `kit_display_72` e não `kit_display_44`
>
> O nome do time (`VERMELHO`, `AMARELO`, …) e o número da pessoa **não** podem ir
> em `kit_display_44`: essa fonte foi gerada **com** kerning e os pares de
> kerning do Archivo Black **se sobrepõem**, distorcendo palavras de várias
> letras (o mesmo bug que a [Decisor Tool](decisor.md) teve antes de regerar a
> `kit_display_72`). A convenção do projeto: palavra grande vai em
> **`kit_display_72`** (única Archivo Black gerada com `--no-kerning`, cobre
> `A-Z Ã Ç Õ 0-9 - espaço`) ou em **mono**; `kit_display_44` só para glifos,
> wordmark e números soltos. Aqui: número da pessoa e `PRONTO` em
> `kit_display_72`; nome do time em `kit_mono_26` (palavra = mono, como a
> [Quem Vai Primeiro](primeiro.md)).

---

## Execução e animação

A ação principal (`kit_times_draw`) pode ser disparada por:

- botão **`SORTEAR`** no rodapé;
- **toque em qualquer lugar do palco**;
- botão físico **PWR** e o gesto de **chacoalhar** (via
  [`kit_runtime`](../architecture/runtime.md),
  `kit_runtime_set_tool_primary_action`).

Durante a revelação, `kit_times_draw` **avança para a próxima pessoa** (igual a
tocar na tela). Fora dela, dispara um novo sorteio. Sem efeito se um sorteio já
estiver em curso.

A divisão é **sempre equilibrada**: `⌈N/T⌉` para os primeiros `N mod T` times,
`⌊N/T⌋` para o resto (diferença ≤ 1 pessoa). O embaralhamento é um
**Fisher-Yates** das posições `1..N` alimentado pela [Random API](../api/random.md)
(TRNG de hardware).

Ao tocar `SORTEAR`, um **único `lv_timer`** (`DRAW_TICK_MS` 55 ms × `DRAW_TICKS`
5) segura um suspense curto, dá **1 bipe** (`beep(900, 35)`) e abre o overlay na
pessoa 1. Só troca de texto/atributo por tick — **nada de** `transform_scale` /
`transform_rotation` (estoura o render no CO5300/PSRAM).

---

## Persistência

Via [Storage API](../api/storage.md) (`set_i32` / `get_i32`):

| Chave | Conteúdo |
|---|---|
| `times_people` | número de pessoas (4–16) |
| `times_count` | número de times (2–4) |

**Sem histórico** — um sorteio de times se usa na hora ou refaz.

---

## Navegação

- **BOOT** volta para a Home fechando a Tool (`kit_system_exit_impl`).
- O chip de voltar da titlebar sai pela API (`system->exit`).

---

## Ciclo de vida

| Função | Efeito |
|---|---|
| `kit_times_start(accent)` | Monta as duas páginas + o overlay e carrega. `accent` 0 → azul padrão. |
| `kit_times_draw()` | Ação principal — sorteia, ou avança a revelação. Ligada ao PWR/chacoalhar pelo Runtime. |
| `kit_times_destroy()` | Derruba o `lv_timer` e os objetos LVGL. |
