# Adedonha

**Adedonha** é um mini-jogo de mesa "nome, lugar, objeto" (também Stop! ou
Adedanha). Fiel à filosofia do Bingo — **o KIT sorteia, o papel confere**: o
KIT sorteia a **cartela** (as categorias da rodada) e as **letras**, conta o
tempo e toca o alarme. Ninguém pontua no aparelho — cada um confere e soma no
papel.

> **Tool do catálogo**, não built-in do Core. Vive em
> [`kit-tools`](https://github.com/jcrvlh/kit-tools/tree/main/tools/io.github.jcrvlh.adedonha)
> como `io.github.jcrvlh.adedonha` (pacote `.kit`, carregado do cartão microSD
> pelo [`kit_tool_loader`](../architecture/tools.md)). Nasceu como componente
> built-in `kit_adedonha` para iteração rápida e saiu do Core quando estabilizou
> — a lógica já só usava `kit_api`, então a conversão foi `tool_init`/
> `tool_destroy` + a ação principal ligada a `register_shake_callback`.
>
> O Core mantém: o ícone geométrico da Home (`TOOL_ICON_ADEDONHA` — uma folha de
> cartela com três linhas; `home_icon` `"order"` até o validador do SDK ganhar
> `"adedonha"`), os 4 SFX (`KIT_SFX_ADEDONHA_*`) e os símbolos LVGL
> `lv_obj_clean` / `lv_obj_set_style_border_color` / `_border_opa` / `lv_qrcode_*`
> na tabela do `kit_tool_loader`.
>
> A partir da **v1.1.1** exige **runtime ≥ 0.3.1** — a versão que exporta
> `lv_qrcode_*` (QR da página CARTELA) **e** `strcpy` (que o GCC sintetiza no
> `.so`; sem ele o `dlopen` falhava com `Can't find symbol strcpy`). Ver
> [tool_lvgl_runtime.md](../../tools-sdk/docs/tool_lvgl_runtime.md).

Card **azul**, mini-jogo. Baralho de 43 categorias, licença CC0.

---

## Fluxo

1. **Sorteia a cartela uma vez** → todos copiam as categorias como colunas (ou
   apontam a câmera no **QR** da página CARTELA e imprimem as folhas prontas).
2. **Sorteia uma letra** (botão, toque no palco ou **chacoalhando** — a Tool
   externa não tem a ação do PWR) → o tempo começa a correr.
3. Todos preenchem uma palavra por coluna com aquela letra, até **o tempo
   acabar** (alarme) ou alguém apertar **STOP**.
4. Conferem e pontuam no papel. **Sorteia a próxima letra** na mesma cartela.

---

## Tela

Titlebar + `lv_tileview` de **3 páginas** (`AJUSTE ◄──► JOGO ◄──► CARTELA`,
começa no JOGO) + botão sensível ao estado fixo no rodapé.

### Página 0 — AJUSTE

| Campo | Opções | Efeito |
|---|---|---|
| **CATEGORIAS** | `4` · `6` · `8` | Quantas a cartela sorteia. Trocar **pede uma cartela nova**. |
| **TEMPO** | `30S` · `1MIN` · `2MIN` · `OFF` | Duração da rodada. `OFF` mostra um cronômetro **subindo** e a rodada só acaba no STOP (teto de segurança de 15 min). |
| **LETRAS** | `FÁCEIS` · `TODAS` | `FÁCEIS` = 16 letras que dá pra preencher a cartela inteira. `TODAS` = A–Z. Trocar zera o saco. |

Mais `NOVA CARTELA` (um toque) e `REINICIAR LETRAS` (dois toques). **Só os
ajustes** persistem no Storage (`ad_cats` / `ad_tempo` / `ad_letras`) — a cartela
e o saco de letras **começam do zero a cada abertura**.

### Página 1 — JOGO

Palco tocável: a **letra** em `kit_display_72` **CAIXA ALTA**, o **relógio
`MM:SS`** em `kit_display_44`, e o botão. Um **anel** engrossa e pisca nos
últimos 10 s. Estados: `SORTEAR CARTELA` → `SORTEAR LETRA` (roleta A–Z, **sem
reposição**) → `STOP` → próxima letra. Saco vazio → reembaralha sozinho.

**Tempo esgotado** → overlay azul cheio `TEMPO`, em **dois estágios**: o **1º
toque cala o alarme** (a dica vira `CONFIRAM NO PAPEL` e aparece o botão); o 2º
(ou o botão `PRÓXIMA LETRA`) sorteia a próxima letra. O alarme
(`KIT_SFX_ADEDONHA_TIMEUP`) re-toca a cada ~3,4 s com um _strobe_ do fundo até o
1º toque.

### Página 2 — CARTELA

Antes do primeiro sorteio, o **"como joga"** no padrão da Mímica — um corpo
único em `kit_sans_28` (caixa normal, quebra linha), não mais quatro linhas de
mono apagado em caixa alta.

Depois do sorteio: a **lista numerada das categorias** em `kit_sans_28` +, no
fim, um **QR** (fundo branco, exceção ao preto AMOLED) pro **gerador de folhas
web** — [`web-installer/adedonha.html`](../../web-installer/adedonha.html),
publicado em `jcrvlh.github.io/kit/adedonha.html`. A URL leva os _slugs_ das
categorias da rodada (`?c=nome,animal,fruta,…` — mesma normalização nos dois
lados: minúscula ASCII, sem acento nem espaço). A página monta **uma folha A4
por pessoa** (coluna da LETRA + uma coluna por categoria + PONTOS, N rodadas em
branco) e imprime; aberta sem `?c=` ela traz um seletor das 43 categorias. Um
alternador **CATEGORIAS / PREENCHER** deixa anotar as respostas no próprio
celular (rodada = letra + resposta por categoria + pontos, com total), salvo em
`localStorage`. A nota do QR puxa pro **papel e caneta** — a web fica como
alternativa, e ela também imprime.

---

## Identidade sonora

Quatro SFX próprios, renderizados pelo Core (`kit_audio.c`):

| SFX | Momento |
|---|---|
| `KIT_SFX_ADEDONHA_CARD` | sortear a cartela — folhear cartas desacelerando + "tap" |
| `KIT_SFX_ADEDONHA_LETTER` | letra travou — folheio → carimbo + duas notas "VALENDO!" |
| `KIT_SFX_ADEDONHA_STOP` | STOP — buzina amigável descendo + assento grave |
| `KIT_SFX_ADEDONHA_TIMEUP` | tempo esgotado — klaxon de game-show bi-tom + resolução grave |

Os tiques dos últimos 5 s reusam `KIT_SFX_TIMER_TICK`.

---

## Ciclo de vida (`src/main.c` no `kit-tools`)

| Função | Efeito |
|---|---|
| `tool_init(ctx)` | Salva `ctx->api`, carrega os ajustes, registra o `on_shake`, monta a tela e carrega. Começa sempre em `SORTEIE A CARTELA`. |
| `tool_destroy()` | Derruba os `lv_timer` (animação, relógio, alarme), desregistra o shake, solta o `keep_awake`, deleta a própria tela e zera o `s_api`. |

Aritmética inteira de propósito no relógio e no pulso do anel — o `elf_loader`
não exporta `__divsf3` (float) nem `__udivdi3` (divisão de 64 bits).
