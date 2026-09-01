# Decisor Tool

A **Decisor Tool** é a moeda digital do KIT: transforma uma decisão binária numa
"virada de moeda" rápida, física e satisfatória. Tool interna (built-in no Core),
componente `kit_decisor`, id `com.kit.decisor`, despachada pelo
[`kit_tool_manager`](../architecture/tools.md). Card verde na grade da Home.

---

## Modos

| Modo | Opção A | Opção B | Observação |
|------|---------|---------|------------|
| **MOEDA** | `CARA` | `COROA` | Padrão. |
| **SIM / NÃO** | `SIM` | `NÃO` | Mesma linguagem visual da MOEDA. |
| **CUSTOM** | rótulo A | rótulo B | Dois rótulos de **1 a 3 caracteres**, `A–Z` e `0–9`. |

Os rótulos custom são definidos por um **teclado de rodas** (`lv_roller`): três
tumblers lado a lado, um por caractere, com uma posição em branco (`·`). O limite
de 3 caracteres é garantido pela própria estrutura — não há como digitar um
quarto. Um contador `n/3` e a prévia `[ PIZ ]` acompanham a edição. O mesmo
teclado é reutilizado para o rótulo A e o B. A página de Config também traz
_presets_ de um toque (`A / B`, `EU/TU`, `PIZ/HAM`).

---

## Peso de decisão

Cada opção tem um peso de **10 % a 90 %** (passo de 10; B = 100 − A). O peso é
aplicado **exclusivamente ao mecanismo de aleatoriedade**
([Random API](../api/random.md) / TRNG): `random->range(1,1000) <= peso_A*10`.

O peso **nunca** altera a animação — o giro da moeda é idêntico em 50/50 e em
90/10, para não induzir o usuário a acreditar que os resultados são igualmente
prováveis nem, ao contrário, a "ler" o resultado antes da hora. Quando o peso é
diferente de 50/50, um chip discreto `PESO 70·30` aparece na página principal e a
página de Config mostra `PIZ 70 · 30 HAM`.

---

## Melhor de

`UMA VEZ` (padrão), `MELHOR DE 3`, `MELHOR DE 5`, `MELHOR DE 7`.

Nos modos "melhor de", cada rodada é disparada pelo usuário (um toque em
`SORTEAR` / PWR / chacoalhar por rodada), o placar aparece ao vivo na página
principal e a partida
**encerra assim que uma opção atinge a maioria** — sem necessariamente jogar
todas as rodadas (ex.: `PIZ` vence 3 × 0 em "melhor de 5" e a partida acaba). A
tela de resultado final mostra `PIZ` / `3 × 1` / `VENCEU`.

---

## Execução e animação

A ação principal (`kit_decisor_flip`) pode ser disparada por:

- botão **`SORTEAR`** no rodapé;
- botão físico **PWR** e o gesto de **chacoalhar** (via `kit_runtime`).

Todas fazem exatamente a mesma coisa. Sortear de qualquer página leva para a
página principal.

> **Sem gesto no palco.** As primeiras versões também disparavam por
> *arrastar para cima* / *tocar* no palco, detectado pela própria Tool a partir
> do delta Y entre `LV_EVENT_PRESSED` e `LV_EVENT_RELEASED`. Isso foi **removido**:
> o palco era um `lv_obj` clicável do tamanho do tile, e chamar
> `lv_tileview_set_tile_by_index()` de dentro do handler de toque reentrava no
> processamento de scroll do próprio tileview e **travava a Tool**. Quem dispara
> agora é só o botão `SORTEAR` (mesmo padrão da Dice).

Fluxo: `IDLE → botão/PWR/chacoalhar → embaralhar (~0,55 s) → resultado → aguarda
nova execução`. **Não há moeda literal** — a decisão é o nome grandão. No estado
ocioso a página mostra as duas opções (`SIM  ·  NÃO`); ao sortear, esse texto
vira o rótulo do resultado (`kit_display_72`, Archivo Black, só `A–Z 0–9 - Ã Ç Õ`,
**sem kerning** para os pares de letras não se sobreporem) e **embaralha** —
alterna entre as duas opções e trava no vencedor (sorteado **antes** da
animação), nos últimos ticks já mostrando o vencedor.

A animação segue **exatamente** o desenho da Dice Tool, que roda liso nesta
board: **um único `lv_timer`** curto (poucos ticks a 70 ms); no último tick ele
trava no vencedor, se apaga com `lv_timer_delete` e revela o resultado — **tudo
no mesmo callback**. Sem segundo timer / "hold" de suspense, sem bipe no meio, e
só um `lv_label_set_text` por tick (nada de `transform_scale`/`transform_rotation`
— o layer transformado animado estoura o render no CO5300/PSRAM e o task watchdog
reinicia a board). O encadeamento anterior (timer de embaralhar → segundo timer
one-shot de suspense → `kit_audio->beep` no callback) era a causa raiz do
travamento ao sortear.

O resultado ocupa a maior parte da tela; nos modos "melhor de" a tela final
empilha `PIZ` / `3 × 1` / `VENCEU`.

---

## Navegação e persistência

`lv_tileview` horizontal de 3 páginas (indicador de 3 pontos na titlebar), no
mesmo idioma da Dice Tool:

```
CONFIG  ◄──►  PRINCIPAL  ◄──►  HISTÓRICO
```

- **Config** — modo, melhor de, peso, rótulos custom. Toda alteração é salva na
  hora ([Storage API](../api/storage.md), chaves `dec_mode`, `dec_bestof`,
  `dec_weight`, `dec_lblA`, `dec_lblB`) e zera uma partida em andamento.
- **Histórico** — até 30 decisões (`hist.txt` no diretório privado da Tool),
  roláveis, mais recente no topo. `LIMPAR` pede confirmação antes de apagar.
- Ao reabrir a Tool, a última configuração é recuperada.

---

## Estados visuais

Primeira abertura (MOEDA / UMA VEZ / 50-50) · modo MOEDA · modo SIM/NÃO · CUSTOM
sem configuração (botão `SORTEAR` esmaecido, dica "defina as opções") · CUSTOM
configurado · digitando rótulo (rodas) · configuração de peso · MELHOR DE 3/5/7
(placar) · execução · animação · resultado · rodada sem vencedor ainda ("mais
uma") · histórico vazio · histórico preenchido · confirmação de limpeza.

---

## Desvios registrados em relação à especificação original

1. **`SIM / TALVEZ` removido dos exemplos de CUSTOM** — "TALVEZ" tem 6 caracteres
   e o limite de 3 caracteres é um critério de aceitação. O limite prevalece; o
   exemplo era ilustrativo. (`CARA`/`COROA` continuam válidos: o limite de 3 vale
   só para o modo CUSTOM; aqueles são os rótulos fixos do modo MOEDA.)
2. **Rodadas do "melhor de" são disparadas pelo usuário** (um toque em `SORTEAR`,
   PWR ou chacoalhar por rodada), não automaticamente em sequência — preserva a
   sensação de "uma virada física por vez". A especificação não definia o ritmo.
3. **Divulgação do peso na página principal** — além da página de Config exigida
   pela especificação, o chip `PESO x·y` aparece na tela principal sempre que o
   peso ≠ 50/50.
4. **Rótulos custom em caixa alta, `A–Z` e `0–9`, de 1 a 3 caracteres** — uma roda
   em branco não contribui com caractere.
