# Linguagem Visual da Interface — "Brutalist Bauhaus"

Sistema de design do Launcher do KIT: identidade, paleta, tipografia, ícones,
componentes e regras de layout para a tela AMOLED de 368 × 448.

O sistema vive em código:

| Arquivo | Conteúdo |
|---|---|
| [`firmware/components/kit_fonts/include/kit_theme.h`](../../firmware/components/kit_fonts/include/kit_theme.h) | Tokens de cor, área de toque e macros de ícone |
| [`firmware/components/kit_fonts/include/kit_fonts.h`](../../firmware/components/kit_fonts/include/kit_fonts.h) | Fontes tipográficas |
| [`firmware/components/kit_launcher/src/kit_launcher.c`](../../firmware/components/kit_launcher/src/kit_launcher.c) | Telas e componentes montados em LVGL v9 |

> Escopo atual: Introdução (onboarding do 1º boot), Home (slideshow de Tools),
> Ajustes (Tela → Brilho / Repouso da tela · Som · Wi-Fi · Armazenamento ·
> Modo pen drive · Atualizar firmware · Bateria → nível / Desligar sozinho ·
> Repetir introdução · Sobre · Restaurar padrão de fábrica), a linha **Testes**
> (diagnóstico, dentro de Sobre) e
> as Tools built-in **Dados** (`kit_dice`), **Garrafa** (`kit_bottle`),
> **Moeda** (`kit_decisor`, id `com.kit.coin`), **Timer** (`kit_timer`),
> **Quem Vai Primeiro** (`kit_primeiro`), **Sortear Times** (`kit_times`),
> **Bingo** (`kit_bingo`), **Placar** (`kit_placar`) e
> **Soundbox** (`kit_soundbox`).
> As Tools do catálogo (**Quebra-Gelo**, **Pavio**, **Adedonha**, **Veto**,
> **Mímica** `io.github.jcrvlh.mimica`, **Testa** `io.github.jcrvlh.testa`,
> **Telefonema** `io.github.jcrvlh.telefonema`, **Estouro**
> `io.github.jcrvlh.estouro`, **Vira Certo**, **Tarot**, **Fora** …) seguem a
> mesma linguagem — Telefonema e Estouro são a referência atual do padrão descrito
> em [🎮 Padrão de Tool](#-padrão-de-tool-ajuste--jogo--como-joga), abaixo.

---

## 🎯 Princípios

1. **Preto AMOLED é o material.** Fundo `#000000` absoluto em todas as telas —
   pixels desligados, contraste infinito, menor consumo. Nada de "cinza escuro"
   como fundo.
2. **As três primitivas são a marca.** Quadrado vermelho, círculo azul,
   triângulo amarelo (Bauhaus). Cada uma carrega um significado fixo e reaparece
   como ícone, badge e logo.
3. **A tipografia carrega a hierarquia, não a cor.** Monoespaçada em CAIXA ALTA
   com _tracking_ largo para rótulos e títulos; display pesada para números e
   wordmark. A cor entra com parcimônia — **uma superfície colorida por vez**.
4. **Tela pequena, dedo real.** Todo alvo tocável tem no mínimo 56 px de lado
   (80 px nos principais) e ainda ganha área de clique invisível ao redor.
   Conteúdo longo rola na vertical em vez de espremer.
5. **Sem "wrap boxes".** Nada de cartão decorativo em volta de texto ou controle
   só para agrupar — numa tela de 1.8" a moldura arredondada só rouba espaço.
   Agrupa-se com espaçamento e tipografia. Contêiner só quando é superfície de
   tela cheia ou alvo de toque real.

---

## 🎨 Paleta

Definida em `kit_theme.h`. Quatro neutros + quatro primárias Bauhaus.

### Neutros

| Token | Hex | Uso |
|---|---|---|
| `KIT_COLOR_BG` | `#000000` | Fundo AMOLED (preto absoluto) |
| `KIT_COLOR_SURFACE` | `#171719` | Linhas de lista, botões secundários, chips |
| `KIT_COLOR_SURFACE_ALT` | `#222226` | Superfície elevada (badge dentro da linha) |
| `KIT_COLOR_LINE` | `#2C2C2E` | Fios, bordas, contorno tracejado |
| `KIT_COLOR_TEXT` | `#EFEADD` | Texto principal (off-white quente, "paper") |
| `KIT_COLOR_TEXT_MUTED` | `#6E6C66` | Rótulos apagados, legendas |

### Primárias Bauhaus

| Token | Hex | Forma | Significado |
|---|---|---|---|
| `KIT_COLOR_RED` | `#C6472F` | Quadrado | Erro / ação destrutiva |
| `KIT_COLOR_BLUE` | `#2C3CC4` | Círculo | Informação |
| `KIT_COLOR_YELLOW` | `#E9B23C` | Triângulo | Ação primária |
| `KIT_COLOR_GREEN` | `#45A05B` | — | Conexão / sucesso |

### Texto sobre superfícies cheias

| Token | Hex | Uso |
|---|---|---|
| `KIT_COLOR_ON_YELLOW` | `#000000` | Texto sobre botão amarelo (contraste alto) |
| `KIT_COLOR_ON_COLOR` | `#EFEADD` | Texto sobre vermelho / azul / verde |

### Nomes legados

`KIT_COLOR_ACCENT`, `KIT_COLOR_PANEL`, `KIT_COLOR_PANEL_ALT`, `KIT_COLOR_BORDER`,
`KIT_COLOR_DANGER`, `KIT_COLOR_ON_BRIGHT`, `KIT_COLOR_ACCENT_DIM` continuam
definidos como _alias_ da nova paleta, só para o `kit_tool_manager` compilar sem
mudança. Não usar em código novo — serão removidos quando aquela tela for
redesenhada.

---

## 🔤 Tipografia

Fontes bitmap LVGL (bpp 4) geradas a partir de TTFs livres. Cada uma cobre
Latin + Latin-1 (acentuação PT: ç ã õ é ê í ó ú ü …) mais um conjunto mínimo de
7 ícones FontAwesome. Declaradas em `kit_fonts.h`.

| Fonte | Base | Tam. | Papel |
|---|---|---|---|
| `kit_mono_26` | Space Mono **Bold** | 26 px | Títulos de tela, wordmark `KIT` da barra de status |
| `kit_mono_20` | Space Mono **Bold** | 20 px | Rótulos de botão, legenda "NENHUMA TOOL" |
| `kit_mono_16` | Space Mono Regular | 16 px | Tabela de especificações, `MIN` / `MAX` |
| `kit_sans_22` | Archivo **Bold** | 22 px | Rótulos das linhas de Ajustes (caixa normal) |
| `kit_sans_28` | Archivo **Bold** | 28 px | Frases da Introdução (caixa normal) |
| `kit_display_44` | Archivo **Black** | 44 px | Wordmark `KIT`, número grande do Brilho, formas/ícones grandes |
| `kit_display_72` | Archivo **Black** | 72 px | Só ` - 0-9 A-Z Ã Ç Õ` — rótulo do resultado da Moeda (CARA/COROA/…) e número da pessoa em Sortear Times |
| `kit_display_120` | Archivo **Black** | ~85 px | Só `0-9 - +` — número protagonista (Dados, Bingo, contador do Estouro) |

> As fontes Montserrat+FA antigas (`kit_font_12/14/18/20/24`), usadas só pela
> Test Tool no visual pré-Bauhaus, foram removidas. Do Montserrat embutido do
> LVGL ficou só o tamanho 14 (o `LV_FONT_DEFAULT`).

### Regras de uso

* **Mono = sempre CAIXA ALTA** com `letter_space` de 2–4 px (`lv_obj_set_style_text_letter_space`).
* **Sans (Archivo) = caixa normal**, só nos rótulos de linha — é o único texto proporcional da interface.
* **Display = só números e o wordmark.** Nunca frases.
* Não há peso itálico nem uma quarta família. Se precisar de ênfase, é tamanho ou cor, não uma fonte nova.

### Como (re)gerar as fontes

Ferramenta: [`lv_font_conv`](https://github.com/lvgl/lv_font_conv) via `npx` (não precisa instalar).
TTFs: Space Mono e Archivo (OFL, Google Fonts), Archivo Black (OFL).

```sh
TEXT="0x20-0x7F,0xA0-0xFF,0x2022"
# signal, plus, square, caret-up, caret-left, caret-right, circle
ICONS="61458,61543,61640,61656,61657,61658,61713"
C="--no-compress --no-prefilter --bpp 4 --format lvgl --force-fast-kern-format"

npx lv_font_conv@1.5.3 $C --size 26 --font SpaceMono-Bold.ttf    -r $TEXT --font fa.woff -r $ICONS -o kit_mono_26.c
npx lv_font_conv@1.5.3 $C --size 20 --font SpaceMono-Bold.ttf    -r $TEXT --font fa.woff -r $ICONS -o kit_mono_20.c
npx lv_font_conv@1.5.3 $C --size 16 --font SpaceMono-Regular.ttf -r $TEXT --font fa.woff -r $ICONS -o kit_mono_16.c
npx lv_font_conv@1.5.3 $C --size 22 --font Archivo-Bold.woff     -r $TEXT -o kit_sans_22.c
npx lv_font_conv@1.5.3 $C --size 28 --font Archivo-Bold.woff     -r $TEXT -o kit_sans_28.c
npx lv_font_conv@1.5.3 $C --size 44 --font ArchivoBlack-Regular.ttf -r 0x20-0x7F --font fa.woff -r $ICONS -o kit_display_44.c

# número gigante da Dice Tool — só dígitos e sinais, sem ícones (arquivo enxuto)
npx lv_font_conv@1.5.3 $C --size 120 --font ArchivoBlack-Regular.ttf -r 0x2B,0x2D,0x30-0x39 -o kit_display_120.c
```

Depois: trocar o bloco de include gerado por `#include "lvgl.h"`, copiar os `.c`
para `firmware/components/kit_fonts/src/`, e atualizar `CMakeLists.txt` +
`kit_fonts.h`. `fa.woff` = `FontAwesome5-Solid+Brands+Regular.woff` do repositório do LVGL.

---

## ▲ Formas & ícones

As formas **não** são objetos desenhados — são glifos FontAwesome coloridos
(um `lv_label` com uma cor), embutidos nas fontes. Macros UTF-8 em `kit_theme.h`:

| Macro | Glifo (FA5) | Onde aparece |
|---|---|---|
| `KIT_ICON_SQUARE` | `0xF0C8` quadrado | Marca / badge / erro (vermelho) |
| `KIT_ICON_CIRCLE` | `0xF111` círculo | Marca / badge / info (azul) |
| `KIT_ICON_TRIANGLE` | `0xF0D8` caret-up | Marca / badge / ação (amarelo) |
| `KIT_ICON_BARS` | `0xF012` signal | Badge de "Testar som" (verde) |
| `KIT_ICON_BACK` | `0xF0D9` caret-left | Botão voltar |
| `KIT_ICON_CHEVRON` | `0xF0DA` caret-right | Fim de linha de lista |
| `KIT_ICON_PLUS` | `0xF067` mais | Botão "Adicionar Tool" |

Tamanho da forma = tamanho da fonte do label. Badges usam `kit_display_44`;
chevron usa `kit_mono_26`.

---

## 🧱 Componentes

Todos montados por _helpers_ em `kit_launcher.c`. Raio de canto sempre generoso
(estilo watchOS/Bauhaus arredondado), borda 0 salvo indicação.

### Chip de canto — `make_chip()`
Quadrado `64 × 64`, `KIT_COLOR_SURFACE`, raio 18. Contém um glifo centralizado
(voltar). `ext_click_area` +12.

### Titlebar — `make_titlebar()`
Faixa fixa de `KIT_TITLEBAR` (88 px) no topo das sub-telas: chip de voltar à
esquerda + título em `kit_mono_26` CAIXA ALTA. Sempre visível — é o retorno
primário, o botão `VOLTAR` do rodapé é secundário.

### Botão — `make_button(..., primary)`
Largura = conteúdo (`KIT_CONTENT` = 336), altura **80 px** (`KIT_BTN_H` =
`KIT_TOUCH_TARGET_COMFORTABLE`), raio = altura/2 (pílula), sombra 0,
`ext_click_area` +8. Rótulo em `kit_mono_20`.

| Variante | Fundo | Borda | Texto |
|---|---|---|---|
| `primary` | `KIT_COLOR_YELLOW` | — | `KIT_COLOR_ON_YELLOW` |
| contornado | transparente | 2 px `KIT_COLOR_TEXT` | `KIT_COLOR_TEXT` |

Em contexto de sucesso/erro o `primary` troca de cor (verde `CONECTAR`,
vermelho `TENTAR NOVAMENTE`) — mesma forma.

### Linha de lista — `make_row()`
Filha de um corpo flex. `336 × 88`, raio 24, `ext_click_area` +6.

* **Badge** `52 × 52` raio 14, fundo `KIT_COLOR_SURFACE_ALT`, com a forma em `kit_display_44`.
* **Rótulo** em `kit_sans_22`.
* **Chevron** `KIT_ICON_CHEVRON` à direita.
* **Estado selecionado:** inverte — fundo `KIT_COLOR_TEXT`, rótulo/chevron pretos, badge escurece.

### Slider — Brilho
Trilho `304 × 22`, raio 11. `MAIN` = `SURFACE`, `INDICATOR` = `YELLOW`,
`KNOB` = `TEXT` (branco), `KNOB` circular com `pad` 13. `ext_click_area` **+24** —
a faixa de toque é bem maior que o trilho visível.

### Tabela de especificações — `make_spec()`
Linha de `336 × 46`, transparente, fio de 1 px `KIT_COLOR_LINE` embaixo.
Rótulo à esquerda (`kit_mono_16`, apagado, CAIXA ALTA), valor à direita
(`kit_mono_16`, `TEXT`, alinhado à direita).

### Corpo rolável — `make_scroll_body()`
Contêiner flex-coluna abaixo da titlebar, rolagem vertical (`LV_DIR_VER`,
scrollbar `AUTO`), `pad` lateral `KIT_PAD` (16), `pad_row` 12. Recebe um
`bottom_reserve` quando há botão fixo no rodapé. Usado em **Ajustes** e **Sobre**.

### Roleta de arraste — número / letra que gira
Uma caixa `SURFACE` com o valor grande no meio (`kit_display_72`); **arrastar ↕
gira** (pra cima aumenta, 24 px por passo, com wrap), **tocar = +1**. Um
micro-arraste (< 12 px) ainda conta como toque. Durante o arraste o container que
rola e o `lv_tileview` têm o `scroll_dir` congelado pra a roleta não virar
rolagem/troca de página. Handlers `PRESSED/PRESSING/RELEASED/PRESS_LOST/CLICKED`,
`lv_indev_get_vect()` pro delta. Usos: sigla do **Placar/Fora** (`kit_ui_sigla`,
3 letras) e o tempo MM:SS do **Timer** (`time_wheel_t` + `make_wheel_pair`).

### Modo Ampulheta — Timer
Ligado no AJUSTE (padrão desligado, persistido). É uma ampulheta: com o KIT **de
cabeça pra baixo** o timer corre e a tela gira 180°
(`kit_display_set_rotation_impl` — o `lvgl_flush_cb` inverte pixels + janela,
`kit_input` espelha o toque); **qualquer outra posição pausa** e guarda o
restante (tag `PAUSADO`; `PRONTO` antes do 1º giro). A tela **fica** a 180°
depois de pausar (só volta a 0° ao desligar o modo ou sair da Tool) — pra ler o
valor pausado do mesmo lado. A posição vem do
**acelerômetro** (`kit_imu_poll_orientation`, direção da gravidade — não o
giroscópio, que só mede rotação e deriva). Quando o modo está ligado não há
timer manual (COMEÇAR/PWR/chacoalhar ficam inertes) e a tela fica acesa (senão o
acelerômetro desliga). 90° (KIT de lado) não gira nessa tela — o CO5300 não faz
swap_xy.

---

## 👆 Área de toque

Definida em `kit_theme.h`:

| Constante | Valor | Regra |
|---|---|---|
| `KIT_TOUCH_TARGET_MIN` | **56 px** | Mínimo absoluto de qualquer alvo interativo |
| `KIT_TOUCH_TARGET_COMFORTABLE` | **80 px** | Padrão para botões e linhas principais |

Além do tamanho físico, todo alvo chama `lv_obj_set_ext_click_area()` para
esticar a zona de clique **para fora** das bordas visíveis (chip +12, botão +8,
linha +6, slider +24). O dedo pode errar a borda e ainda acerta.

Quando o conteúdo não cabe com fonte grande, a tela **rola** — nunca se reduz
tipografia nem alvo para encaixar.

---

## 🔘 Botões físicos

Tratados em [`kit_runtime`](../architecture/runtime.md) (`poll_system_buttons`,
~a cada 200 ms), não na UI.

| Botão | Ligação | Toque curto |
|---|---|---|
| **PWR** | tecla PWRON do AXP2101 (IRQ `INTSTS2` bit 3) | **Na Home:** liga/desliga o painel AMOLED (`kit_display_set_on_impl`) e o touch do LVGL — com a tela apagada, ela volta com o PWR ou com um toque na tela (leitura crua do CST820 em `poll_wake_touch`, ~a cada 80 ms; o toque que acorda é consumido e não chega à UI). **Dentro de uma Tool:** dispara a _ação principal_ da Tool (`kit_runtime_set_tool_primary_action`) — nos Dados, rola. Toque longo = desliga o sistema (hardware). |
| **Chacoalhar** | acelerômetro QMI8658 (`kit_imu`) | Dentro de uma Tool, agitar o aparelho (`|a| > 2,2 g`) dispara a mesma _ação principal_ que o PWR. Polling a ~60 ms, só enquanto há Tool ativa. |
| **BOOT** | GPIO0, ativo-baixo | Volta para a Home fechando qualquer sub-tela (`kit_launcher_go_home`), ou sai da Tool ativa (`kit_system_exit_impl`). |

---

## 🎮 Padrão de Tool (Ajuste · Jogo · Como Joga)

A partir do Telefonema e do Estouro (catálogo), consolidando o que já valia
pra Pavio/Placar/Veto/Mímica: este é o formato **padrão pra qualquer Tool
nova** (built-in ou catálogo) que tenha ajuste e/ou uma regra pra explicar —
não se reinventa layout por Tool. O formato "enxuto" de página única (Quem
Vai Primeiro, Quebra-Gelo, Moeda) continua válido, mas só **enquanto a Tool
não tiver nem ajuste nem regra que precise de explicação**; no momento em que
qualquer um dos dois aparecer, ela migra pra este padrão.

### 1. Três páginas, sempre nesta ordem

`lv_tileview` horizontal de 3 tiles: **AJUSTE** (esquerda) ◄──► **JOGO**
(centro) ◄──► **COMO JOGA** (direita). A Tool **sempre abre no JOGO**
(`lv_tileview_set_tile_by_index(tv, 1, 0, LV_ANIM_OFF)`) — é o conteúdo
protagonista; nunca abre na configuração nem na regra. A titlebar fixa mostra
3 pontos de página (o ativo cresce e pega a cor da Tool) pra deixar claro
onde a pessoa está e que dá pra arrastar.

Ref.: `kit_pavio`, `kit_placar`, `io.github.jcrvlh.veto`,
`io.github.jcrvlh.mimica`, `io.github.jcrvlh.telefonema`,
`io.github.jcrvlh.estouro`.

### 2. AJUSTE: alvo de toque grande, nunca espremido

Era aqui que mais aparecia controle pequeno demais pra caber tudo numa tela
de 1,8". Padrão corrigido em Telefonema/Estouro:

* **Chips/pílulas de opção:** altura mínima **80–84 px**
  (`KIT_TOUCH_TARGET_COMFORTABLE`, não os 54 px dos chips mais antigos da
  Moeda/Dados) e **no máximo 2 por linha** — com mais de 2 opções, quebra em
  outra linha em vez de espremer 3–4 na mesma (rótulo ilegível, dedo erra o
  alvo). Ref.: `build_chip_grid()` em `io.github.jcrvlh.telefonema`
  (grade `SEM`/`POUCOS`/`PADRÃO`/`MUITOS` em duas linhas de dois).
* **Stepper numérico `[ − valor + ]`:** botões quadrados **≥ 76–80 px**
  (`E_STEP = 80` em `io.github.jcrvlh.estouro`, mesmo padrão do `PESSOAS` da
  Sortear Times), valor central em fonte grande (`kit_display_44`) — nunca
  em mono.
* No AJUSTE especificamente, mirar o **COMFORTABLE** (80 px) como piso, não
  como teto — a regra geral de `KIT_TOUCH_TARGET_MIN` (56 px, ver
  [👆 Área de toque](#-área-de-toque)) é o mínimo absoluto pro resto da
  interface, não a meta aqui.

### 3. COMO JOGA é obrigatória em qualquer mini-jogo

Toda Tool com mecânica que precise de explicação (mini-jogo, ferramenta com
regra não óbvia) tem a terceira página **COMO JOGA** — a regra nunca fica só
no README do catálogo. Formato fixo, referência `io.github.jcrvlh.mimica`
(`build_page_help()`):

* Título `COMO JOGA` em `kit_mono_26` CAIXA ALTA.
* Corpo em **`kit_sans_28`** — não mono, não `kit_mono_16`. É a única tela da
  Tool onde um parágrafo comprido em caixa normal é lido de verdade; mono em
  bloco longo cansa a vista. `lv_label_set_long_mode(LV_LABEL_LONG_WRAP)`,
  rola na vertical, passos numerados (`1.`, `2.`, `3.`…), sem "wrap box" ao
  redor do texto.
* Sem botão fixo no rodapé — o botão de ação é filho só do tile do JOGO (ver
  §4), então esta página usa a altura inteira pra ler.

### 4. O botão de ação é filho do tile do JOGO, não da tela

Bug encontrado e corrigido no Telefonema: criar o botão fixo (`COMEÇAR` /
ação principal) como filho de `s_screen` faz ele flutuar sobre — ou deixar
uma tarja preta atrás — das outras páginas do `lv_tileview`, porque a faixa
reservada pra ele fica fora da altura calculada de cada tile. O botão tem que
nascer dentro do tile do JOGO (`build_game_page()`), ancorado no rodapé
**daquele tile só**; assim ele só existe ali, e AJUSTE/COMO JOGA ocupam a
tela inteira sem sobra.

### 5. O protagonista da tela é sempre fonte grande

Generalizando o que corrigiu o Telefonema, o Estouro já fazia e a Mímica
também: o elemento **protagonista** da página do JOGO — a palavra sorteada,
o número, o resultado, o texto de referência — vai em fonte grande
(`kit_display_44` ou maior; `kit_display_120` pra número isolado tipo o
contador do Estouro), **nunca em `kit_mono_26`** só por ser "o padrão de
título". `kit_mono_26` fica pro status secundário (`FIQUE ATENTO...`,
`TOQUE EM COMEÇAR`) — o que precisa ser lido de relance, a um braço de
distância da mesa, é sempre o protagonista, não a legenda.

* **Telefonema** — a referência do toque certo (`ESSE É O TOQUE CERTO`) e o
  resultado (`342 MS` / `CEDO DEMAIS!`) subiram de `kit_mono_26` pra
  `kit_display_44` depois de reportado como "fonte pequena demais".
* **Estouro** — o contador de jogadores no palco vai em `kit_display_120`; o
  valor do stepper no AJUSTE, em `kit_display_44`.
* **Mímica** — a palavra a ser atuada vai em `kit_display_44` (ou
  `kit_sans_28` se for longa demais pra caber) — nunca em mono, mesmo sendo
  texto e não número.
* Lembrete de kerning: `kit_display_44` distorce palavras longas (ver nota
  em [🔤 Tipografia](#-tipografia)) — se o conteúdo grande for uma **frase**
  e não um número/palavra curta, prefira quebrar em duas linhas ou cair pra
  `kit_sans_28` a forçar tudo numa linha só.

---

## 📱 Telas

| Tela | Função | Layout | Navegação |
|---|---|---|---|
| **Splash** | "INICIANDO" ao ligar | Fixo | Some sozinha (~1,5 s) → Introdução (1º boot) ou Home |
| **Introdução** | 4 telas no 1º boot: marca → o que é → o que tem dentro → pronto | Overlay preto: coluna central + botão-pílula fixo no rodapé | Abre com o SFX `WELCOME` · `COMEÇAR`/`VEM VER`/`CONTINUAR` avança · `COMEÇAR` verde no fim toca `ONBOARD_DONE`, grava a flag `onboarded` e vai pra Home · BOOT sai e grava a flag |
| **Duas dicas** (coach-mark) | Ensina os 2 gestos sem botão da Home | Overlay preto sobre a Home: título + 2 linhas (rastro de setas + frase) + `ENTENDI` no rodapé | Só aparece 1× logo após o `COMEÇAR` da Introdução · `ENTENDI` ou BOOT fecha |
| **Home** | Launcher / slideshow de Tools | Barra de status (wordmark + ícone de Wi-Fi + bateria) + `lv_tileview` horizontal ("VER TODOS" + até 3 recentes) + pontos | Abre na Tool mais recente · arrasta na horizontal (→ direita cai na visão geral) · slide/card → Tool · deslizar pra cima → Ajustes (ou o card na seção SISTEMA de "VER TODOS") |
| **Ajustes** | Lista de configurações | Titlebar + corpo rolável | Linhas → Tela / Som / Wi-Fi / Armazenamento / Modo pen drive / Atualizar firmware / Bateria / Repetir introdução / Sobre o KIT / Restaurar padrão de fábrica |
| **Restaurar** | Confirmação de restaurar padrão de fábrica | Titlebar `RESTAURAR` + aviso rolável + botões | `RESTAURAR AGORA` (vermelho) apaga a NVS inteira (ajustes + redes Wi-Fi + recordes) e reinicia · `CANCELAR` · Tools do cartão continuam |
| **Tela** | Sub-lista | Titlebar + corpo rolável | Linhas → Brilho / Repouso da tela |
| **Brilho** | Controle de brilho do AMOLED | Fixo | Slider 10–100 % · Titlebar ← ou `VOLTAR` |
| **Repouso da tela** | Tempo sem toque até apagar a tela | Titlebar + lista de opção | 15 s / 30 s / 1 min / 2 min / 5 min / Nunca — toque grava e volta |
| **Som** | Liga/desliga SFX + volume | Titlebar + linha de switch + slider (`MIN`/`MAX`) | Ao soltar o slider, toca uma nota no volume escolhido |
| **Wi-Fi** | Rádio, rede conectada, redes salvas | Titlebar + linha de switch + specs (rede / IP) + lista de redes + `CONFIGURAR REDE` | Sub-telas: **Configurar** (portal, titlebar `CONFIGURAR`) e **Esquecer** (confirmação) |
| **Armazenamento** | Espaço da memória interna e do cartão | Titlebar + specs + botões | `FORMATAR CARTÃO` (→ sub-tela `FORMATAR`) / `PROCURAR CARTÃO` / `RECARREGAR TOOLS` |
| **Modo pen drive** | Expõe o microSD ao PC via USB MSC | Titlebar `PEN DRIVE` + coluna central + `ATIVAR` / `SAIR` | `SAIR` reinicia o KIT (confirmação de "ejetou?" antes) |
| **Atualizar firmware** | Verifica e aplica OTA | Titlebar `FIRMWARE` + specs (versão atual / disponível) + botão | Tela de progresso durante download/aplicação |
| **Bateria** | Nível, estado e desligamento automático | Titlebar + specs (`NÍVEL` / `ESTADO`) + linha `Desligar sozinho` | — |
| **Desligar sozinho** | Tempo sem uso até o aparelho desligar | Titlebar `DESLIGAR` + lista de opção | 2 min / 5 min / 10 min / 30 min / Nunca — não vale na tomada |
| **Sobre** | Ficha do dispositivo + diagnóstico | Titlebar `SOBRE` + logo + specs + linha `Testes` + `VOLTAR` fixo | `Testes` abre o diagnóstico de subsistemas · Titlebar ← ou `VOLTAR` |
| **Testes** | Diagnóstico de subsistemas (dentro de Sobre) | Cabeçalho + linhas de status (verde OK / vermelho falha) + `SAIR` | `SAIR` (vermelho) ou BOOT → Home |
| **Catálogo** | Instala/remove Tools por Wi-Fi (Home → VER TODOS → SISTEMA) | Titlebar `CATÁLOGO` + `lv_tileview` de 2 páginas: lista de Tools (barra `TOOLS X/16` + botão `ATUALIZAR TODAS (N)` quando há updates + selo `INSTALAR`/`ATUALIZAR`/`INSTALADA`) e **SOBRE O LIMITE** (explica o teto de 16 Tools do catálogo); ou aviso `IR PARA WI-FI` | Toque numa Tool → detalhe · arrasta pro lado → limite · Titlebar ← → Home |
| **Dados** (`kit_dice`) | Rolagem de dados | Titlebar + `lv_tileview` de 3 páginas + `ROLAR` fixo | Arrasta na horizontal · Titlebar ← ou BOOT → Home |
| **Timer** (`kit_timer`) | Cronômetro ↑ / regressivo ↓ | Titlebar + `lv_tileview` de 2 páginas (Ajuste / Relógio) + `COMEÇAR`/`PARAR` fixos. Ajuste: atalhos 3/5/10/15/30 min + par de **roletas de arraste** MM:SS + **Modo Ampulheta**. Relógio: MM:SS em `kit_display_120` | PWR/chacoalhar = COMEÇAR/PAUSAR · Titlebar ← ou BOOT → Home |
| **Quem Vai Primeiro** (`kit_primeiro`) | Sorteia uma característica pra decidir quem começa | Titlebar + palco tocável + `SORTEAR` fixo | Titlebar ← ou BOOT → Home |
| **Sortear Times** (`kit_times`) | Divide a mesa em times equilibrados | Titlebar + `lv_tileview` de 2 páginas (Ajuste / Sorteio) + `SORTEAR` fixo; overlay de revelação um a um | Arrasta na horizontal · Titlebar ← ou BOOT → Home |
| **Bingo** (`kit_bingo`) | Globo de bingo digital 1–75 / 1–90 com painel de chamadas | Titlebar + `lv_tileview` de 3 páginas (Ajuste / Globo / Chamadas) + `SORTEAR` fixo | Arrasta na horizontal · Titlebar ← ou BOOT → Home |
| **Quebra-Gelo** (`io.github.jcrvlh.*`, catálogo) | Sorteia uma pergunta quebra-gelo pra roda responder | Titlebar + palco tocável + `SORTEAR` fixo (formato enxuto) | Titlebar ← ou BOOT → Home |
| **Pavio** (`kit_pavio`) | Mini-jogo: fale uma palavra com a sílaba e passe o KIT antes de explodir | Titlebar + `lv_tileview` de 3 páginas (Ajuste / Jogo / Como joga) + botão `ACENDER PAVIO`/`PASSEI` fixo; overlay vermelho `BUM` | Arrasta na horizontal (travado em rodada) · Titlebar ← ou BOOT → Home |
| **Placar** (`kit_placar`) | Placar de mesa: 2–4 colunas, toque = +1 e segurar = −1, meta opcional | Titlebar + `lv_tileview` de 3 páginas (Ajuste / Placar / Como usa) + `ZERAR` (dois toques) fixo; overlay na cor do jogador `VENCEU`. Sigla: stepper `◄ JOGADOR N ►` + 3 caixas — toque avança a letra, arraste gira como roleta (padrão do Fora) | Arrasta na horizontal (travado só no `VENCEU`) · Titlebar ← ou BOOT → Home |
| **Veto** (`io.github.jcrvlh.veto`) | Mini-jogo: descreva a palavra-alvo sem dizer as 3 proibidas; o KIT cronometra e toca a cigarra, a mesa confere | Titlebar + `lv_tileview` de 3 páginas (Ajuste / Jogo / Como joga) + barra de tempo amarela + botões `DISLIKE (👎)`/`PULAR`/`JOINHA (👍)` na mesma linha; overlay amarelo `TEMPO` com o placar da vez | Arrasta na horizontal (travado em vez) · Titlebar ← ou BOOT → Home |
| **Mímica** (`io.github.jcrvlh.mimica`, catálogo) | Mini-jogo: atue a palavra por gestos, sem falar; o KIT cronometra, a mesa confere | Titlebar + `lv_tileview` de 3 páginas (Ajuste / Jogo / Como joga) + barra de tempo azul + linha de ações `PULAR` (contornado) + `ACERTOU` (cheio) no rodapé; preparo de 3 s; overlay azul `TEMPO` com o placar da vez | Arrasta na horizontal (travado em vez) · Titlebar ← ou BOOT → Home |
| **Feedback** | Confirmação transitória (ex: carga iniciada) | Overlay colorido | Some sozinha (~1,7 s) |

As sub-telas são _overlays_ de tela cheia (`make_overlay(bg)`) criados como
filhos do `s_launcher_screen` sob demanda e destruídos no retorno.

**Splash** — fundo preto, a logo (trio + wordmark `KIT`) e `INICIANDO` em mono
caixa alta. Sem botão.

**Home** — barra de status fixa (wordmark `KIT` + **ícone de Wi-Fi** + indicador
de bateria) sobre um **slideshow** de Tools: um `lv_tileview` horizontal (`build_home` →
`home_build_deck`) com o slide **"VER TODOS"** primeiro (índice 0) seguido de um
slide por Tool recente. A Home **abre na Tool mais recente** (índice 1): deslizar
da esquerda para a direita cai direto na visão geral sem passar pelas outras
recentes; deslizar para a esquerda percorre as demais. Pontos de página no
rodapé (traço claro = ativo). **Deslizar pra cima** em qualquer ponto da Home
abre os **Ajustes** (`home_gesture_cb` no `s_launcher_screen`, `LV_EVENT_GESTURE`
+ `LV_DIR_TOP`); o card "Ajustes" na grade "VER TODOS" continua valendo.

* **Slides de Tool** (até 3) — as **3 mais usadas recentemente**, a mais recente
  primeiro. A ordem é persistida em NVS (`kit_config`, chaves `home_mru0`…`mru2`)
  e sobe pro topo toda vez que a Tool é aberta (`home_mru_touch`); o deck
  reconstrói ao voltar pra Home. Cada slide é uma carta cheia na cor da Tool
  (`KIT_CONTENT` de largura), raio 30: badge `52 × 52` com o ícone geométrico no
  topo esquerdo, número da posição na recência (`01`…`03`) em `kit_display_72` a
  30 % no topo direito, rótulo em `kit_sans_22` e a dica `TOQUE PARA ABRIR`
  (`kit_mono_16`) no rodapé. Tocar abre a Tool; indisponível = carta a
  `LV_OPA_40` + dica "EM BREVE". O slideshow é **só recência** — fixar uma Tool
  **não** a coloca aqui (as fixadas vivem na seção FIXADOS da grade).
* **Slide "VER TODOS"** — a **grade completa** de cards de Tool, 2 colunas,
  `162 × 118`, raio 20 (número da posição em `kit_mono_26` a 40 %, badge `42 × 42`,
  rótulo `kit_sans_22`), rolando na vertical. A grade é dividida em seções por
  cabeçalhos que ocupam a linha inteira do flex-wrap (`make_grid_header`,
  `kit_mono_16` apagado). Cada Tool entra em **uma** seção só, nesta precedência:
  **FIXADOS** (lista curada — flag por-id em NVS `pin<hash>`, toque longo → FIXAR)
  > **NOVOS** (Tools do catálogo instaladas e ainda não abertas — flag
  `nu_<hash>`, migram pra sua categoria no 1º uso) > **FERRAMENTAS**
  (`is_game == false`) / **MINI-JOGOS** (`is_game == true`); e **SISTEMA** no fim
  (os cards **Ajustes** e **Catálogo**, sempre em cinza). O card **Catálogo**
  ganha um ponto âmbar (`tile_corner_dot`, igual ao de firmware novo no
  **Ajustes**) quando uma checagem em background acha Tool instalada com versão
  nova. Card disponível abre a Tool; indisponível emite um _toast_ "EM BREVE".
  **Toque longo** num card abre uma folha de ações (`s_toolmgr_screen`), corpo
  rolável: descrição da Tool (`kit_sans_28`, como a página COMO JOGA) + botões em
  fluxo — **FIXAR NA HOME** / **DESAFIXAR** pra qualquer Tool;
  **ATUALIZAR** (só quando o catálogo confirma versão nova; senão **REINSTALAR**)
  e **DESINSTALAR** (vermelho) só pras Tools do catálogo. Sem CANCELAR — a
  titlebar ← fecha. Desinstalar esquece as flags `nu_`/`pin` da Tool (reinstalar
  volta pra NOVOS).

O diagnóstico (antiga Test Tool) agora é a linha **Testes** dentro de **Sobre**.

**Duas dicas** (`home_hints_show`) — coach-mark que entra **uma vez**, logo depois
do `COMEÇAR` verde da Introdução (chamado no fim de `onboarding_finish_cb`, já com
a Home montada atrás). Overlay preto: título `DUAS DICAS` (`kit_mono_20` apagado)
e duas linhas `hint_row` — uma calha de 64 px com um **rastro de 3 setas** (glifo
caret repetido em `kit_mono_26` amarelo, opacidade 30→60→100 na direção do gesto:
`»` pra direita, `⌃` empilhado pra cima) e a frase em `kit_sans_22`. Botão
`ENTENDI` (amarelo) no rodapé; BOOT também fecha. Não há flag própria — só se
alcança por `onboarding_finish_cb`, então repetir a Introdução mostra de novo.

**Toast** (`show_toast(msg)`) — `lv_label` com fundo `KIT_COLOR_TEXT`, texto
preto em `kit_mono_20`, raio 16, no rodapé; some sozinho (~1,4 s).

**Dados** (`kit_dice`) — titlebar (chip ← + `DADOS` + 3 pontos de página) sobre
um `lv_tileview` de **3 páginas** (arrasta na horizontal) e o botão `ROLAR` fixo
no rodapé (`76` de altura, `kit_mono_26`, na cor da Tool). O ponto ativo cresce e
fica na cor da Tool. Rolar (botão, PWR ou chacoalhar) leva sempre à página 1.
A saída é feita pela API (`system->exit`).

* **Página 0 — Ajuste** (rola na vertical): seletor de dado (`D4`…`D100`) em
  chips `80 × 54` (`kit_mono_26`) num grid de 4; `QUANTIDADE` e `MODIFICADOR`,
  uma linha cada, rótulo à esquerda e `[ - valor + ]` à direita — botões
  `56 × 56` e valor em `kit_display_44` de largura fixa.
* **Página 1 — Resultado** (a inicial): o **número sorteado** em `kit_display_120`
  — fonte bitmap dedicada de ~85 px (Archivo Black, só `0-9 - +`) — ancorado no
  **centro exato entre a base da titlebar e o topo do botão `ROLAR`** (`D_NUM_OFFSET`,
  padding de cima = padding de baixo, ~90 px cada). A notação (`3D6+2`) flutua
  14 px acima dele (`align_to OUT_TOP`), a dica `ROLAR · PWR · CHACOALHAR` (antes
  de rolar) e as faces individuais `kit_mono_26` (depois) 14 px abaixo
  (`align_to OUT_BOTTOM`) — o número não se mexe com os parâmetros. Antes da
  primeira rolagem o número mostra um `-` apagado. _Fonte de verdade em vez de
  `transform_scale`: o transform faz o LVGL alocar um layer por frame no
  CO5300/PSRAM e estoura o task watchdog._
* **Página 2 — Histórico** (rola na vertical): até 6 rolagens, cada linha com a
  notação à esquerda e o total à direita (o mais recente na cor da Tool).

**Quem Vai Primeiro** (`kit_primeiro`) — titlebar (chip ← + `PRIMEIRO`) sobre um
**palco** tocável e o botão `SORTEAR` fixo no rodapé (vermelho, texto claro,
`kit_mono_26`).
Página única — sem `lv_tileview`, sem ajuste, sem histórico, sem persistência (o
formato enxuto da Garrafa). No palco, a característica sorteada em `kit_mono_26`
**CAIXA ALTA** centralizada, quebrando em até quatro linhas, **sem "wrap box"**;
`A PESSOA QUE` acima e `COMEÇA O JOGO` abaixo (`kit_mono_16` apagado, aparecem no
primeiro sorteio). Antes do primeiro sorteio o palco mostra só `TOQUE EM SORTEAR`
apagado. Sortear (botão, toque no palco, PWR ou chacoalhar) embaralha entre as
57 características fixas por ~0,7 s num **único `lv_timer`** e trava na sorteada
(escolhida antes, via Random API; nunca repete) — cor vermelha + 1 bipe. _Frase vai
em mono, nunca em `kit_display_*` (essa é só pra números e o wordmark), mesmo
sendo o elemento protagonista da tela._ A saída é feita pela API (`system->exit`).

**Quebra-Gelo** (catálogo) — nasceu built-in e **migrou pro catálogo**; continua
sendo a referência do **formato enxuto de página única**. **Mesma estrutura da
Quem Vai Primeiro**: titlebar (chip ← + `QUEBRA-GELO`) + palco tocável + botão
`SORTEAR` fixo no rodapé (azul). Sem `lv_tileview`/ajuste/histórico/persistência.
No palco, a pergunta sorteada em `kit_mono_26` **CAIXA ALTA** centralizada
(quebrando em várias linhas, sem "wrap box"); `PERGUNTA` acima e `PASSE ADIANTE`
abaixo (`kit_mono_16` apagado). Baralho de ~95 perguntas quebra-gelo leves;
sortear (botão/toque/PWR/chacoalhar) embaralha num **único `lv_timer`** e trava
na sorteada (azul, nunca repete a anterior) + 1 bipe. Card azul (`TOOL_ICON_ASK`
— balão de fala com reticências, reusável por `home_icon`).

**Sortear Times** (`kit_times`) — titlebar (chip ← + `TIMES`) + `lv_tileview`
horizontal de 2 páginas (`AJUSTE ◄──► SORTEIO`, começa no SORTEIO) + botão
`SORTEAR` fixo no rodapé (azul). **AJUSTE**: `PESSOAS` (4–16, botões `-`/`+`
+ número `kit_display_44`) e `TIMES` (2/3/4, pílulas). **SORTEIO**: no ocioso só
`N TIMES` / `N PESSOAS` (`kit_mono_26`) e `TOQUE PARA SORTEAR` — sem "wrap box".
O resultado é **sempre revelado um a um**: um overlay de tela cheia na cor do
time por pessoa — rótulo `PESSOA` pequeno + número `X` **grande em
`kit_display_72`** (é o que muda a cada toque, mesmo quando duas pessoas seguidas
caem no mesmo time e a cor de fundo não denuncia) + `DE N` + `TIME` + nome do
time em **`kit_mono_26`**. Divisão sempre equilibrada (Fisher-Yates via Random
API); config em Storage (`times_people` / `times_count`), sem histórico. Animação
= um único `lv_timer` (55 ms/tick, suspense curto) + bipes.

> **`kit_display_44` distorce palavras.** Essa fonte foi gerada **com** kerning e
> os pares do Archivo Black se sobrepõem — palavras de várias letras (`VERMELHO`,
> `AMARELO`) saem distorcidas (o mesmo bug que a Moeda teve com a
> `kit_display_72` antiga, resolvido regerando com `--no-kerning`). Regra: palavra
> grande vai em **`kit_display_72`** (única Archivo Black `--no-kerning`, cobre
> `A-Z Ã Ç Õ 0-9 - espaço`) ou em **mono**; `kit_display_44` só para glifos,
> wordmark e números soltos.

**Bingo** (`kit_bingo`) — titlebar (chip ← + `BINGO`) + `lv_tileview`
horizontal de 3 páginas (`AJUSTE ◄──► GLOBO ◄──► CHAMADAS`, começa no GLOBO) +
botão `SORTEAR` fixo no rodapé (verde). **AJUSTE**: `FAIXA` (`1-75` / `1-90`,
pílulas) e `REINICIAR SORTEIO` (botão contornado vermelho, dois toques para
confirmar). **GLOBO**: número sorteado grande em `kit_display_120` (só dígitos —
`FIM` cai em `kit_display_72`), a letra da coluna `B/I/N/G/O` em `kit_mono_26`
acima (só no 1–75), linha `ANTERIOR` + contador `N / TOTAL` — sem "wrap box".
**CHAMADAS**: toggle `LISTA` / `GRADE` (persistido, padrão LISTA). **LISTA** = só
os números já sorteados, agrupados por letra (1–75) ou dezena (1–90) em
`kit_mono_26` — a visão de conferência pensada pra tela de 1,8"; a linha do
último sorteado fica na cor da Tool. **GRADE** = o painel inteiro da faixa num
**`lv_table`** (célula desenhada, não objeto — uma grade de 75–90 `lv_obj`
estourava o layer do LVGL e travava a placa; a cor de cada célula sai do hook
`LV_EVENT_DRAW_TASK_ADDED`), sorteadas na cor da Tool com o número em **preto**
(contraste), o último com um anel de `border`. Sorteio sem reposição via Random
API; animação = um único `lv_timer` (60 ms/tick) que só troca o texto do número +
1 bipe no lock. A **rodada persiste** em Storage (`bingo_range` / `bingo_drawn` /
`bingo_view`) — trocar a faixa ou tocar `REINICIAR` zera.

> **Grade de muitas células = `lv_table`, nunca uma árvore de `lv_obj`.** Montar
> 75–90 células como linhas flex + célula + label estourava o layout do LVGL no
> CO5300/PSRAM (render > 5 s → `task_wdt` em loop, placa travada ao abrir a
> Tool) — a mesma classe do gotcha de `transform`/layer, por contagem de objeto.
> O `lv_table` desenha as células direto; a aparência de cada uma sai do hook
> `LV_EVENT_DRAW_TASK_ADDED` (`lv_draw_task_get_fill_dsc` / `_label_dsc` /
> `_border_dsc`).

**Cor da Tool.** Cada Tool adota como cor principal a cor do seu slide/card na
Home — o `kit_tool_manager` passa essa cor no `*_start()` e ela vai no botão
primário e nos acentos da Tool. **Repetição de cor entre Tools é aceita** — a
paleta Bauhaus só tem quatro primárias. Roster atual:

| Cor | Tools |
|---|---|
| **Vermelho** | Dados, Quem Vai Primeiro, Quebra-Gelo, Pavio, Telefonema, Fora |
| **Azul** | Garrafa, Sortear Times, Vira Certo, Mímica, Adedonha, Tarot |
| **Amarelo** | Moeda, Estouro, Testa, Veto (texto preto por cima) |
| **Verde** | Timer, Bingo, Placar |

O Placar ainda usa as quatro primárias de uma vez — uma por jogador — como
identidade de cada coluna.

**Indicador de bateria** — desenhado (corpo + terminal + barra de nível), com
`NN%` ao lado (`kit_mono_16`). Verde carregando, vermelho ≤ 15 %, "paper" no
resto. Atualiza a cada 2 s (`batt_tick_cb`).

**Feedback** (`show_feedback(bg, icone, rótulo)`) — fundo na cor de contexto,
disco preto central com o ícone na cor do fundo, rótulo em mono caixa alta.
Usado pelo evento de carga iniciada (verde + raio + `CARREGANDO`), pela
associação de Wi-Fi (verde + sinal + `CONECTADO`) e pelo aviso de bateria baixa
(amarelo + triângulo + `BATERIA BAIXA`, dispara uma vez ao cair a ≤ 20 %
descarregando; rearma acima de 25 % ou ao ligar na tomada).

**Sobre** — a logo (trio + wordmark `KIT`), a tabela de specs
(`DISPOSITIVO` de `kit_power_get_device_id()`, `FIRMWARE`, `RUNTIME`, `HARDWARE`,
`FLASH`, `LICENÇA`), a linha **`Testes`** (`make_row`, abre o diagnóstico) e a
assinatura `JCRVLH EXPERIMENT` no fim. O triângulo da logo usa o glifo
`KIT_ICON_PLAY` rotacionado 90° — mais encorpado que o caret, do mesmo tamanho
do quadrado e do círculo. Esta é a logo oficial.

**Brilho** — badge amarelo com o círculo, slider, valor em `kit_display_44`
amarelo, `MIN`/`MAX`, botão `VOLTAR`. O slider aplica ao vivo via DCS 0x51 no
CO5300 (com o enquadramento QSPI correto — ver [display.md](../hardware/display.md));
ao soltar o dedo o valor é gravado em NVS (`kit_config`) e reaplicado no boot.

**Repouso da tela / Desligar sozinho** — cada uma é uma titlebar + `make_scroll_body`
com uma legenda curta e uma lista de opções (`make_row`, badge círculo). A opção
atual aparece selecionada (badge verde + linha invertida). Tocar numa opção grava
em `kit_config` e fecha. O `kit_runtime` lê a inatividade do LVGL
(`lv_display_get_inactive_time`) a cada ~1 s: passando o tempo de repouso apaga
o painel **e o touch do LVGL** (o botão PWR ou um toque na tela acordam — ver a tabela de botões físicos);
passando o tempo de desligamento chama `kit_power_shutdown()` (AXP2101), exceto
ligado na tomada.

**Tela / Som / Bateria** — linhas de Ajustes que abrem uma sub-lista, não uma
tela de controle direto. **Tela** agrupa `Brilho` e `Repouso da tela`; **Bateria**
mostra os specs `NÍVEL`/`ESTADO` e a linha `Desligar sozinho`; **Som** é a
exceção — traz o próprio switch de liga/desliga e o slider de volume
(`MIN`/`MAX`) na mesma tela, no padrão da linha de Wi-Fi.

**Wi-Fi / Armazenamento / Modo pen drive / Atualizar firmware / Catálogo** — as
telas de sistema mais novas, todas no mesmo molde (titlebar + `make_scroll_body`,
`make_spec` para status, `make_button`/`make_row` para ações). O detalhe de
comportamento (portal `KIT-XXXX`, verificação diária de OTA, SHA-256, ejeção do
cartão) fica no **Manual do Usuário** — aqui só o esqueleto visual:

* **Wi-Fi** (`WI-FI`) — linha de switch (mesma pegada da de `Som`), `make_spec`
  com rede e IP quando conectado, lista de redes salvas (`make_row`, badge
  círculo) e o botão `CONFIGURAR REDE`. Sub-telas **Configurar** (`CONFIGURAR`,
  o portal) e **Esquecer** (`ESQUECER`, confirmação de dois toques). O **ícone de
  Wi-Fi da barra de status** (`KIT_ICON_BARS`) segue `kit_network_get_state()`:
  escondido (rádio off), apagado (ligado sem rede), amarelo (associando), azul
  (portal aberto), verde (conectado).
* **Armazenamento** (`ARMAZENAMENTO`) — `make_spec` com o espaço livre da flash
  interna e do cartão + contagem de Tools no cartão; botões `FORMATAR CARTÃO`
  (→ sub-tela `FORMATAR`, confirmação), `PROCURAR CARTÃO`, `RECARREGAR TOOLS`.
* **Modo pen drive** (`PEN DRIVE`) — coluna central explicando o modo + botão
  `ATIVAR`; ativo, vira aviso + `SAIR` (com confirmação "ejetou no PC?"). `SAIR`
  reinicia o KIT — é a única saída (USB único, sem console enquanto monta).
* **Atualizar firmware** (`FIRMWARE`) — `make_spec` com `VERSÃO ATUAL` e, quando
  há, a disponível; botão de ação; tela de progresso (`s_fwupdate_busy`) durante
  download e aplicação. Um _toast_ + um ponto no card `Ajustes` avisam quando a
  verificação de fundo acha versão nova.
* **Catálogo** (`CATÁLOGO`, em Home → VER TODOS → SISTEMA) — sem rede, mostra
  `IR PARA WI-FI`; com rede, lista as Tools com selo `INSTALAR` / `ATUALIZAR` /
  `INSTALADA` e um estado de "Buscando catálogo…". Toque numa Tool abre o detalhe
  (instalar / remover). Instala direto no cartão microSD. Quando há Tools
  instaladas com versão nova, um botão `ATUALIZAR TODAS (N)` no topo da lista
  baixa uma a uma (o `catalog_poll_cb` encadeia). Uma checagem em background
  (`kit_runtime`, ~40 s após o boot, 1×/dia) acende um _toast_ + ponto âmbar no
  card `Catálogo`, como o do firmware.
* **Aviso de bateria baixa** — `show_banner` põe uma tarja curta na **top layer**
  do LVGL (aparece por cima da Home **e** de uma Tool em andamento), alinhada ao
  topo. Dois níveis, cada um 1× por descarga: `≤ 20 %` amarelo `BATERIA N%`,
  `≤ 10 %` vermelho `BATERIA FRACA`. Rearma ao carregar.

**Testes** (`kit_tool_manager`, aberto pela linha `Testes` em Sobre) — tela cheia
própria (não é overlay do Launcher). Cabeçalho + `DIAGNOSTICO DO SISTEMA`, um
`make_scroll_body` com uma linha por subsistema no padrão da tabela de specs
(chave `kit_mono_16` apagada à esquerda, valor à direita: verde = OK, vermelho =
falha — tela, toque, PMIC, RTC, áudio, IMU, cartão, aleatório) e uma pílula
vermelha `SAIR` fixa no rodapé. A linha `TOUCH` mostra X/Y/contagem a cada toque
e a `RANDOM` sorteia um novo valor do TRNG. Sai pela API (`system->exit`) / BOOT.

---

## 🚧 Pendências

* **Telas de sistema entregues** desde a primeira redação: Wi-Fi (+ portal +
  esquecer rede), Armazenamento (+ formatar), Modo pen drive, Atualizar firmware
  e Catálogo. Falta ainda uma **tela dedicada de erro de instalação** de Tool
  (hoje é _toast_). Os _alias_ de cor legados (`KIT_COLOR_ACCENT` etc.) foram
  removidos — todo o código usa os tokens da paleta nova.
* **Ícones das Tools** são composições de `lv_obj` (retângulos/círculos), não
  glifos — não há um glifo FA dedicado por Tool nas fontes. Exceções que usam
  glifo: o triângulo do card "Sorteio" (`KIT_ICON_TRIANGLE`) e a seta do card
  "Primeiro" (`KIT_ICON_CHEVRON` apontando para um disco). O card "Times"
  (`TOOL_ICON_TEAMS`) é um quadrado dividido em dois (metade cheia, metade
  contorno). O card "Bingo" (`TOOL_ICON_BINGO`) são quatro pontos; o "Quebra-Gelo"
  (`TOOL_ICON_ASK`) é um balão de fala com três pontinhos; o "Pavio"
  (`TOOL_ICON_PAVIO`) é uma bomba redonda com um pavio curto e uma faísca; o
  "Veto" (`TOOL_ICON_VETO`) é uma carta com a palavra-alvo em cima e as
  proibidas (ponto + traço) abaixo; a "Mímica" (`TOOL_ICON_MIMICA`) é uma figura
  gesticulando (cabeça + tronco + braços erguidos). O `TOOL_ICON_ADEDONHA` (folha de cartela com
  três linhas) fica no Core mesmo a Adedonha tendo saído pro catálogo — uma Tool
  do cartão pode reusá-lo por `home_icon`.
* **Grade de Tools** — a seção FERRAMENTAS/MINI-JOGOS lista as built-in
  (`HOME_TOOLS_BUILTIN`: Dados, Garrafa, Moeda, Timer, Quem Vai Primeiro, Sortear
  Times, Bingo, Placar) mais as do catálogo já instaladas; SISTEMA fecha com
  Ajustes e Catálogo. O campo `available` (`is_game` à parte) continua existindo
  para uma Tool em desenvolvimento aparecer esmaecida + "EM BREVE".

---

## 🔗 Referências

* [Display AMOLED CO5300 & Renderização](../hardware/display.md)
* [Runtime & botões físicos](../architecture/runtime.md) ·
  [Rede / Wi-Fi](../architecture/networking.md) ·
  [Atualização OTA](../architecture/ota.md)
* Referência visual (proposta aprovada): artefato "KIT Interface Redesign"
