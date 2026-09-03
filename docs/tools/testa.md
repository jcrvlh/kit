# Testa

**Testa** é um mini-jogo de mesa estilo *Heads Up!*: uma pessoa segura o KIT
encostado na **testa**, com a tela virada para a roda. A roda **dá dicas** —
falando, cantando ou gesticulando, como combinarem — e a pessoa tenta
**adivinhar** a palavra em voz alta. **Acertou:** inclina o KIT para baixo (tela
para o chão). **Passar:** inclina para cima (tela para o teto). Quando o
**TEMPO** acaba, o KIT mostra quantas ela fez — quem valida cada palpite é a
roda (filosofia do Bingo).

> **Tool do catálogo**, não built-in do Core. Vive em
> [`kit-tools`](https://github.com/jcrvlh/kit-tools/tree/main/tools/io.github.jcrvlh.testa)
> como `io.github.jcrvlh.testa` (pacote `.kit`, carregado do cartão microSD pelo
> [`kit_tool_loader`](../architecture/tools.md)). Nasceu como componente
> built-in `kit_testa` para iteração rápida e saiu do Core quando estabilizou.
>
> O Core mantém: o ícone geométrico da Home (`TOOL_ICON_TESTA` — cabeça + o KIT
> na testa + setas de inclinar) e o mapa `"testa"` em `icon_from_name`.

Card **amarelo** na grade da Home, marcada como **mini-jogo** (`is_game`). Texto
sobre o amarelo = preto (`KIT_COLOR_ON_YELLOW`).

> **Não confundir com a [Mímica](mimica.md).** Lá quem segura o KIT **atua** a
> palavra por gestos e vê a tela. Aqui quem segura **adivinha**, com o aparelho
> na testa, e inclina para acertar/passar.

---

## Gesto de inclinar

Exige **Runtime ≥ 0.2.0** (`min_runtime` `"0.2.0"` no manifest). A Tool registra
`ctx->api->imu->register_tilt_callback(cb, ud)`; o Runtime só faz o *polling* do
gesto enquanto há um callback registrado (`kit_imu_poll_tilt()` devolve
`KIT_TILT_NONE` de graça sem consumidor).

O aparelho fica ~vertical na testa (eixo normal à tela ~0 g). Virar a tela para
o chão dispara `KIT_TILT_DOWN`; para o teto, `KIT_TILT_UP`. Dispara **uma vez
por inclinada** — só rearma quando volta a ~vertical. Limiares (ângulo de
gatilho, debounce, filtro de solavanco) são `#define` calibráveis no firmware
(`kit_imu.c`), não pela Tool.

O **chacoalhar não é usado** (a roda gesticula muito). Sem botão físico PWR
(só built-ins têm `primary_action`) — a mecânica é o gesto; o PWR de "rede de
segurança" que existia na versão built-in saiu na migração.

---

## Telas

Titlebar fixa + `lv_tileview` de 3 páginas (começa no **JOGO**):

- **AJUSTE** — `TEMPO` (`60S / 90S / 120S / OFF`) e `BARALHO`. Só os ajustes vão
  pro Storage (`te_tempo`, `te_deck`).
- **JOGO** — o nome do baralho, a **palavra** grande virada pra roda
  (`kit_display_44`; frase longa quebra em linha), a barra de tempo e a dica do
  gesto. Sem botões de acerto/passe. A contagem `3 · 2 · 1` aparece grande no
  centro e a 1ª palavra só surge no fim dela. Entre as cartas, um **flash** de
  tela cheia (verde `ACERTOU` / vermelho `PASSOU`, ~900 ms).
- **COMO JOGA** — as regras, corpo rolável em `kit_sans_28`.

Tempo esgotado → **overlay** amarelo `TEMPO` com `ACERTOS n / PASSOU n` e o botão
**PASSAR A VEZ**. O alarme (`KIT_SFX_TIMER_DONE`) re-toca a cada ~3,4 s; um toque
em qualquer lugar do overlay **cala o alarme**, mas a vez só passa no botão.

---

## Baralhos

Fixos na Tool (`src/main.c`), CAIXA ALTA, curadoria pró-adivinhação-verbal
(nomes próprios e cultura pop são bem-vindos, ao contrário da Mímica):

| Baralho | ~n | Exemplos |
|---|---|---|
| `FILMES & SÉRIES` | ~65 | TITANIC, AVENIDA BRASIL, ROUND 6 |
| `CELEBRIDADES` | ~70 | NEYMAR, JULIETTE, JÔ SOARES |
| `ANIMAIS` | ~75 | ORNITORRINCO, CAPIVARA, TAMANDUÁ |
| `GÍRIAS` | ~60 | MITO, PLOT TWIST, PAGAR MICO |
| `OBJETOS & LUGARES` | ~75 | LIQUIDIFICADOR, RODOVIÁRIA, RODA-GIGANTE |
| `MIX` | todos | embaralha as cartas de todos os temas |

`MIX` é um baralho **virtual** (índice `MIX_IDX`) — `cur_card()` / `cur_deck_n()`
mapeiam o índice para o tema certo, sem duplicar dados. Saco sem reposição
(Fisher-Yates); `PASSAR` devolve a carta ao monte numa posição aleatória à
frente. Anel `s_recent[8]` evita repetir uma carta recém-vista logo após o
reembaralho.

---

## Ciclo de vida

| Função | Efeito |
|---|---|
| `tool_init(ctx)` | Guarda `ctx->api`, registra o callback de inclinar, monta a tela (`accent` amarelo). |
| `tool_destroy()` | Remove o callback, derruba os `lv_timer`, solta o `keep_awake` e os objetos LVGL. |

Permissões: `display`, `input`, `random`, `storage`, `audio`, `imu`.
