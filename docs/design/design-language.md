# Linguagem Visual da Interface — "Brutalist Bauhaus"

Sistema de design do Launcher do KIT: identidade, paleta, tipografia, ícones,
componentes e regras de layout para a tela AMOLED de 368 × 448.

O sistema vive em código:

| Arquivo | Conteúdo |
|---|---|
| [`firmware/components/kit_fonts/include/kit_theme.h`](../../firmware/components/kit_fonts/include/kit_theme.h) | Tokens de cor, área de toque e macros de ícone |
| [`firmware/components/kit_fonts/include/kit_fonts.h`](../../firmware/components/kit_fonts/include/kit_fonts.h) | Fontes tipográficas |
| [`firmware/components/kit_launcher/src/kit_launcher.c`](../../firmware/components/kit_launcher/src/kit_launcher.c) | Telas e componentes montados em LVGL v9 |

> Escopo atual: Home (slideshow de Tools), Ajustes (+ Brilho, Repouso da tela,
> Desligar sozinho), Sobre, a **Test Tool** (`kit_tool_manager`) e as Tools
> **Dados** (`kit_dice`), **Garrafa** (`kit_bottle`), **Decisor** (`kit_decisor`),
> **Timer** (`kit_timer`), **Quem Vai Primeiro** (`kit_primeiro`),
> **Sortear Times** (`kit_times`), **Globo de Bingo** (`kit_bingo`) e
> **Quebra-Gelo** (`kit_quebragelo`).

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
| `kit_display_44` | Archivo **Black** | 44 px | Wordmark `KIT`, número grande do Brilho, formas/ícones grandes |
| `kit_display_72` | Archivo **Black** | 72 px | Só ` - 0-9 A-Z Ã Ç Õ` — rótulo do resultado da Decisor Tool |
| `kit_display_120` | Archivo **Black** | ~85 px | Só `0-9 - +` — número do resultado da Dice Tool |

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
(voltar) ou um anel (Ajustes na Home). `ext_click_area` +12.

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
| **PWR** | tecla PWRON do AXP2101 (IRQ `INTSTS2` bit 3) | **Na Home:** liga/desliga o painel AMOLED (`kit_display_set_on_impl`) e o touch do LVGL — com a tela apagada, ela volta com o PWR ou com um toque na tela (leitura crua do CST820 em `poll_wake_touch`, ~a cada 80 ms; o toque que acorda é consumido e não chega à UI). **Dentro de uma Tool:** dispara a _ação principal_ da Tool (`kit_runtime_set_tool_primary_action`) — na Dice Tool, rola os dados. Toque longo = desliga o sistema (hardware). |
| **Chacoalhar** | acelerômetro QMI8658 (`kit_imu`) | Dentro de uma Tool, agitar o aparelho (`|a| > 2,2 g`) dispara a mesma _ação principal_ que o PWR. Polling a ~60 ms, só enquanto há Tool ativa. |
| **BOOT** | GPIO0, ativo-baixo | Volta para a Home fechando qualquer sub-tela (`kit_launcher_go_home`), ou sai da Tool ativa (`kit_system_exit_impl`). |

---

## 📱 Telas

| Tela | Função | Layout | Navegação |
|---|---|---|---|
| **Splash** | "INICIANDO" ao ligar | Fixo | Some sozinha (~1,4 s) → Home |
| **Home** | Launcher / slideshow de Tools | Barra de status + `lv_tileview` horizontal (4 recentes + "VER TODOS") + pontos | Arrasta na horizontal · slide/card → Tool · chip-anel (topo dir.) → Ajustes |
| **Ajustes** | Lista de configurações | Titlebar + corpo rolável | Linhas → Brilho / Repouso da tela / Desligar sozinho / Test Tool / Testar som / Sobre |
| **Brilho** | Controle de brilho do AMOLED | Fixo | Titlebar ← ou `VOLTAR` |
| **Repouso da tela** | Tempo sem toque até apagar a tela | Titlebar + lista de opção | Toque numa opção grava e volta |
| **Desligar sozinho** | Tempo sem uso até o aparelho desligar | Titlebar + lista de opção | Toque numa opção grava e volta |
| **Test Tool** | Diagnóstico de subsistemas | Cabeçalho + linhas de status + `SAIR` | `SAIR` (vermelho) ou BOOT → Home |
| **Sobre** | Especificações do dispositivo | Titlebar + corpo rolável + `VOLTAR` fixo | Titlebar ← ou `VOLTAR` |
| **Dados** (`kit_dice`) | Rolagem de dados | Titlebar + `lv_tileview` de 3 páginas + `ROLAR` fixo | Arrasta na horizontal · Titlebar ← ou BOOT → Home |
| **Quem Vai Primeiro** (`kit_primeiro`) | Sorteia uma característica pra decidir quem começa | Titlebar + palco tocável + `SORTEAR` fixo | Titlebar ← ou BOOT → Home |
| **Sortear Times** (`kit_times`) | Divide a mesa em times equilibrados | Titlebar + `lv_tileview` de 2 páginas (Ajuste / Sorteio) + `SORTEAR` fixo; overlay de revelação um a um | Arrasta na horizontal · Titlebar ← ou BOOT → Home |
| **Globo de Bingo** (`kit_bingo`) | Globo de bingo digital 1–75 / 1–90 com painel de chamadas | Titlebar + `lv_tileview` de 3 páginas (Ajuste / Globo / Chamadas) + `SORTEAR` fixo | Arrasta na horizontal · Titlebar ← ou BOOT → Home |
| **Quebra-Gelo** (`kit_quebragelo`) | Sorteia uma pergunta quebra-gelo pra roda responder | Titlebar + palco tocável + `SORTEAR` fixo | Titlebar ← ou BOOT → Home |
| **Feedback** | Confirmação transitória (ex: carga iniciada) | Overlay colorido | Some sozinha (~1,7 s) |

As sub-telas são _overlays_ de tela cheia (`make_overlay(bg)`) criados como
filhos do `s_launcher_screen` sob demanda e destruídos no retorno.

**Splash** — fundo preto, a logo (trio + wordmark `KIT`) e `INICIANDO` em mono
caixa alta. Sem botão.

**Home** — barra de status fixa (`KIT` + indicador de bateria + chip-anel) sobre
um **slideshow** de Tools: um `lv_tileview` horizontal (`build_home` →
`home_build_deck`) com um slide por Tool + um último slide **"VER TODOS"**.
Arrasta-se na horizontal; pontos de página no rodapé (traço claro = ativo).

* **Slides de Tool** (até 4) — os **4 mais usados recentemente**, o mais recente
  primeiro. A ordem é persistida em NVS (`kit_config`, chaves `home_mru0`…`mru3`)
  e sobe pro topo toda vez que a Tool é aberta (`home_mru_touch`); o deck
  reconstrói ao voltar pra Home. Cada slide é uma carta cheia na cor da Tool
  (`KIT_CONTENT` de largura), raio 30: badge `52 × 52` com o ícone geométrico no
  topo esquerdo, número da posição no slideshow (`01`…`04`) em `kit_display_72` a
  30 % no topo direito, rótulo em `kit_sans_22` e a dica `TOQUE PARA ABRIR`
  (`kit_mono_16`) no rodapé. Tocar abre a Tool; indisponível = carta a `LV_OPA_40`
  + dica "EM BREVE".
* **Slide "VER TODOS"** — cabeçalho `TODAS AS TOOLS` (`kit_mono_16` apagado) e a
  **grade completa** de cards de Tool, 2 colunas, `162 × 118`, raio 20 (número da
  posição em `kit_mono_26` a 40 %, badge `42 × 42`, rótulo `kit_sans_22`), rolando
  na vertical. Card disponível abre a Tool; indisponível emite um _toast_
  "EM BREVE".

O Test Tool continua em Ajustes.

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
**palco** tocável e o botão `SORTEAR` fixo no rodapé (amarelo, texto preto,
`kit_mono_26`).
Página única — sem `lv_tileview`, sem ajuste, sem histórico, sem persistência (o
formato enxuto da Garrafa). No palco, a característica sorteada em `kit_mono_26`
**CAIXA ALTA** centralizada, quebrando em até quatro linhas, **sem "wrap box"**;
`A PESSOA QUE` acima e `COMEÇA O JOGO` abaixo (`kit_mono_16` apagado, aparecem no
primeiro sorteio). Antes do primeiro sorteio o palco mostra só `TOQUE EM SORTEAR`
apagado. Sortear (botão, toque no palco, PWR ou chacoalhar) embaralha entre as
57 características fixas por ~0,7 s num **único `lv_timer`** e trava na sorteada
(escolhida antes, via Random API; nunca repete) — cor amarela + 1 bipe. _Frase vai
em mono, nunca em `kit_display_*` (essa é só pra números e o wordmark), mesmo
sendo o elemento protagonista da tela._ A saída é feita pela API (`system->exit`).

**Quebra-Gelo** (`kit_quebragelo`) — **mesma estrutura da Quem Vai Primeiro**:
titlebar (chip ← + `QUEBRA-GELO`) + palco tocável + botão `SORTEAR` fixo no
rodapé (azul). Página única, sem `lv_tileview`/ajuste/histórico/persistência. No
palco, a pergunta sorteada em `kit_mono_26` **CAIXA ALTA** centralizada
(quebrando em várias linhas, sem "wrap box"); `PERGUNTA` acima e `PASSE ADIANTE`
abaixo (`kit_mono_16` apagado). Baralho fixo de ~95 perguntas quebra-gelo leves;
sortear (botão/toque/PWR/chacoalhar) embaralha num **único `lv_timer`** e trava
na sorteada (azul, nunca repete a anterior) + 1 bipe. Card azul (`TOOL_ICON_ASK`
— balão de fala com reticências).

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
> `AMARELO`) saem distorcidas (o mesmo bug que a Decisor Tool teve com a
> `kit_display_72` antiga, resolvido regerando com `--no-kerning`). Regra: palavra
> grande vai em **`kit_display_72`** (única Archivo Black `--no-kerning`, cobre
> `A-Z Ã Ç Õ 0-9 - espaço`) ou em **mono**; `kit_display_44` só para glifos,
> wordmark e números soltos.

**Globo de Bingo** (`kit_bingo`) — titlebar (chip ← + `BINGO`) + `lv_tileview`
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
paleta Bauhaus só tem quatro primárias: hoje **vermelho** = Dados, **azul** =
Garrafa, Sortear Times e Quebra-Gelo, **amarelo** = Moeda e Quem Vai Primeiro
(texto preto por cima), **verde** = Timer e Globo de Bingo.

**Indicador de bateria** — desenhado (corpo + terminal + barra de nível), com
`NN%` ao lado (`kit_mono_16`). Verde carregando, vermelho ≤ 15 %, "paper" no
resto. Atualiza a cada 2 s (`batt_tick_cb`).

**Feedback** (`show_feedback(bg, icone, rótulo)`) — fundo na cor de contexto,
disco preto central com o ícone na cor do fundo, rótulo em mono caixa alta.
Usado pelo evento de carga iniciada: verde + raio + `CARREGANDO`.

**Sobre** — a logo (trio + wordmark `KIT`), a tabela de specs (`DISPOSITIVO`
vem de `kit_power_get_device_id()`) e a assinatura `JCRVLH EXPERIMENT` no fim.
O triângulo da logo usa o glifo `KIT_ICON_PLAY` rotacionado 90° — mais encorpado
que o caret, do mesmo tamanho do quadrado e do círculo. Esta é a logo oficial.

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

**Test Tool** (`kit_tool_manager`) — tela cheia própria (não é overlay do
Launcher). Cabeçalho `TEST TOOL` + `DIAGNOSTICO DO SISTEMA`, um `make_scroll_body`
com uma linha por subsistema no padrão da tabela de specs (chave `kit_mono_16`
apagada à esquerda, valor à direita: verde = OK, vermelho = falha) e uma pílula
vermelha `SAIR` fixa no rodapé. A linha `TOUCH` mostra X/Y/contagem a cada toque
e a `RANDOM` sorteia um novo valor do TRNG. A saída também sai pela API
(`system->exit`) / botão BOOT.

---

## 🚧 Pendências

* **Telas ainda não implementadas** (existem só nos esboços): Boas-vindas,
  Wi-Fi, Sistema / armazenamento, erro de instalação. A paleta e os componentes
  deste documento já foram dimensionados para cobri-las. Os _alias_ de cor
  legados (`KIT_COLOR_ACCENT` etc.) foram removidos — todo o código usa os
  tokens da paleta nova.
* **Ícones das Tools** são composições de `lv_obj` (retângulos/círculos), não
  glifos — não há um glifo FA dedicado por Tool nas fontes. Exceções que usam
  glifo: o triângulo do card "Sorteio" (`KIT_ICON_TRIANGLE`) e a seta do card
  "Primeiro" (`KIT_ICON_CHEVRON` apontando para um disco). O card "Times"
  (`TOOL_ICON_TEAMS`) é um quadrado dividido em dois (metade cheia, metade
  contorno). O card "Bingo" (`TOOL_ICON_BINGO`) são quatro pontos; o "Quebra-Gelo"
  (`TOOL_ICON_ASK`) é um balão de fala com três pontinhos.
* **Grade de Tools** mostra só as Tools já implementadas (hoje: Dados, Garrafa,
  Moeda, Timer, Quem Vai Primeiro, Sortear Times, Globo de Bingo, Quebra-Gelo). Cada nova Tool da Fase 2 entra em
  `HOME_TOOLS` quando fica pronta. O campo `available` continua existindo para o
  caso de uma Tool em desenvolvimento (aparece esmaecida + "EM BREVE").
* **Shake to Roll** (chacoalhar para rolar, na Dice Tool) depende de um driver
  do IMU QMI8658, ainda não escrito.

---

## 🔗 Referências

* [Display AMOLED CO5300 & Renderização](../hardware/display.md)
* Referência visual (proposta aprovada): artefato "KIT Interface Redesign"
