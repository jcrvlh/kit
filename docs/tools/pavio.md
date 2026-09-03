# Pavio

**Pavio** é um mini-jogo de mesa: o KIT acende um pavio de tempo **escondido** e
mostra uma **sílaba grande no centro**. Cada jogador fala em voz alta uma palavra
que contenha a sílaba e **passa o aparelho adiante** — antes que exploda. Quando
o pavio acaba, **BUM**: quem estiver com o KIT na mão perdeu a rodada.

> **Tool do catálogo**, não built-in do Core. Vive em
> [`kit-tools`](https://github.com/jcrvlh/kit-tools/tree/main/tools/io.github.jcrvlh.pavio)
> como `io.github.jcrvlh.pavio` (pacote `.kit`, carregado do cartão microSD
> pelo [`kit_tool_loader`](../architecture/tools.md)). Nasceu como componente
> built-in `kit_pavio` para iteração rápida e saiu do Core quando estabilizou
> — a lógica já só usava `kit_api`, então a conversão foi `tool_init`/
> `tool_destroy` + a ação principal ligada ao toque e a `register_shake_callback`.
>
> O Core mantém: o ícone geométrico da Home (`TOOL_ICON_PAVIO` — uma bomba redonda
> com pavio e faísca), o motor do "pavio queimando" (`api->audio->fuse`), o SFX
> `KIT_SFX_PAVIO_BOOM` e os símbolos LVGL `lv_obj_set_style_translate_x/y` e
> `lv_obj_invalidate` na tabela do `kit_tool_loader`.

Card **vermelho** na grade da Home, marcada como **mini-jogo** (`is_game`). Texto sobre o vermelho = paper (`KIT_COLOR_ON_COLOR`).

A regra social — **não repetir palavra já dita na rodada** — o KIT **não
verifica**. É o "confere no papel" do Bingo: o KIT dá o palco, a mesa fiscaliza.

---

## Tela

Titlebar fixa + `lv_tileview` horizontal de **3 páginas**
(`AJUSTE ◄──► JOGO ◄──► COMO JOGA`, começa no JOGO) + botão primário fixo no
rodapé. Durante a rodada e a explosão o arraste entre páginas fica travado (o
palco é a única página).

### Página 0 — AJUSTE

Corpo rolável na vertical, três seletores de pílula (só rótulo de campo em
`kit_mono_16` apagado, sem textos de apoio):

| Campo | Opções | Efeito |
|---|---|---|
| **PAVIO** | `CURTO` · `MÉDIO` · `LONGO` | Faixa de tempo do pavio. O tempo real é **sorteado dentro da faixa a cada rodada** e nunca aparece. Curto ≈ 12–28 s, Médio ≈ 22–48 s, Longo ≈ 40–75 s. |
| **SÍLABA** | `TROCA` · `FIXA` | `TROCA` a cada passe (mais fácil) · `FIXA` na rodada inteira (fica difícil conforme as palavras acabam). |
| **BARALHO** | `FÁCIL` · `TUDO` | `FÁCIL` = sílabas comuns (`BA`, `CO`, `TE`…). `TUDO` inclui os encontros difíceis (`TRA`, `LHO`, `NHA`, `ÇÃO`, `ÕES`…). |

Tudo persiste no Storage (`pv_faixa` / `pv_silaba` / `pv_deck`) e volta ao
reabrir.

### Página 1 — JOGO

O palco (área tocável) com só três coisas: o rótulo `SÍLABA` (`kit_mono_16`
apagado), a **sílaba** em `kit_display_72` **CAIXA ALTA** (a fonte cobre
`A-Z Ã Ç Õ` — o baralho não usa outros acentos) e o botão. Sem texto de ajuda.

Atrás da sílaba, uma **banda vermelha** (`s_flash`) e uma **moldura** (`s_ring`)
que pulsam com o pavio. Antes de acender, mostra só `PRONTO` apagado.

Botão no rodapé, na cor da Tool: `ACENDER PAVIO` (ocioso) → `PASSEI` (em jogo).

### Página 2 — COMO JOGA

Corpo rolável, na mesma pegada da Tool **Fora** (do catálogo): cabeçalho
`COMO JOGA` em `kit_mono_26` e as regras num **único rótulo `kit_sans_22`** (caixa
normal, "paper", quebrando linha) — os três passos numerados e o "explodiu na sua
mão? perdeu". Sem em-dash `—` (glifo ausente nas fontes do KIT). É o único lugar
com texto explicativo; o resto da Tool é enxuto.

### Overlay — EXPLODIU

Cobre o palco em **vermelho cheio**: `BUM` em `kit_display_72`, `PERDEU A RODADA`
em mono, e o botão contornado `NOVA RODADA`.

---

## Execução

`kit_pavio_action()` é a ação principal e é **sensível ao estado**:

| Estado | Botão / toque no palco / **PWR** / **chacoalhar** |
|---|---|
| **OCIOSO** | Acende o pavio: sorteia o tempo, sorteia a 1ª sílaba, vai pro JOGO. |
| **EM JOGO** | Passa o KIT: no modo `TROCA` sorteia a próxima sílaba; toca um clique curto. |
| **EXPLODIU** | Começa uma rodada nova (volta pro ocioso). |

Todas as entradas fazem a mesma coisa (via
[`kit_runtime`](../architecture/runtime.md),
`kit_runtime_set_tool_primary_action`). Enquanto o pavio está **acendendo**
(~0,75 s inicial) o passe é ignorado.

### O pavio — um timer + o motor de áudio

- **`s_burn`** (período **fixo**, 100 ms): recalcula a `intensidade`
  (`1 - fração restante`; `now >= deadline` → `explode()`), empurra ela como
  **tensão 0–255** pro Runtime via `api->audio->fuse(tensão)`, e anima o pulso —
  a **cor** do `s_flash` (uma banda **opaca**: sobe do preto até `PV_FLASH_HOT`
  com uma batida que só afunda até `PV_FLASH_DIP`, nunca perto do preto), a
  espessura do `s_ring`, a cor da sílaba (paper → branco) e um **tremor** dela
  via `translate_x/y`.

  A banda é opaca (não `bg_opa` variável) e o anel só é reescrito quando **muda
  de degrau** de 2 px: o *bbox* do anel é quase o palco inteiro, então mexer
  nele a 10 Hz invalidava a tela toda e o *flush* do buffer parcial (40 linhas)
  não fechava num quadro só — as bandas apareciam em tons diferentes, o
  "misturando com o preto". Idem a sílaba: clareia em degraus, não repinta o
  glifo todo tique.
- **O tique mora no áudio.** `fuse()` liga um "pavio queimando" que é gerado na
  própria task de áudio: um `render_tone` curto + `render_silence` até o próximo,
  dimensionados pela tensão corrente. Como o compasso é dado pelo DMA do codec
  (não por um `lv_timer`), o ritmo é **constante** mesmo com a placa repintando o
  pulso — antes o tique saía de um `lv_timer` que acelerava e, sob carga de
  render, chegava atrasado (era o "travando"). A curva de aceleração (segura o
  começo pro suspense, desaba no fim) é feita no motor; a Tool só manda o número.
  Amplitude `~11800–14500` (alta, mas com folga pro fundo de escala 32767) e
  silêncio ativo entre os tiques — **não estoura, não estala**. `fuse(-1)` apaga (feito em
  `stop_timers()`, que cobre `explode()`, `new_round()` e a saída da Tool).

> **Por que um timer só agora.** O buffer do display é parcial (40 linhas) e
> repintar a banda a 18 Hz no fim arriscaria `task_wdt` — por isso o desenho
> segue a ritmo **fixo** (100 ms), não acelera. O som é que acelera, e isso
> agora vive fora do LVGL. Nada de `transform_scale`/`rotation` nem `opa`
> intermediário em container (força layer buffer — regra da board); o tremor usa
> `translate`, que não cria layer.

### A explosão

`explode()` derruba o timer, apaga o pavio (`fuse(-1)`), mostra o overlay e toca
**`KIT_SFX_PAVIO_BOOM`**
(um SFX só: estalo agudo `render_tone(2600, 30, 11000)` + uma cascata descendo de
~2200 a ~640 Hz com `render_silence` entre os passos — todo o registro que a
corneta reproduz, nada de sub-grave). O timer `s_boom` fica só com o _strobe_ do
overlay (poucos toques, como o "fim" do Timer) — nenhum `beep()`.

`kit_power.keep_awake(true)` fica ligado em `EM JOGO` e `EXPLODIU` (um pavio
longo passa do tempo de repouso da tela); volta a `false` no ocioso e ao sair.

---

## Baralho de sílabas

Fixo no firmware (`SIL_FACIL[]` ~42 + `SIL_DIFICIL[]` ~32 em
`kit_pavio.c`). Só glifos de `kit_display_72` (` - 0-9 A-Z Ã Ç Õ`), CAIXA
ALTA. Sorteio pela [Random API](../api/random.md) / TRNG; nunca repete a sílaba
imediatamente anterior.

---

## Navegação

- **BOOT** e o chip de voltar da titlebar saem pela API (`system->exit`).
- Sem estado de rodada persistente: ao reabrir, começa em `PRONTO` (o ajuste
  volta do Storage).

---

## Ciclo de vida
 
| Função | Efeito |
|---|---|
| `tool_init(ctx)` | Monta a tela, registra callback de shake e carrega. |
| `tool_destroy()` | Derruba os `lv_timer`, solta o `keep_awake`, o callback de shake e deleta a tela. |
