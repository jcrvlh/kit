# Quem Vai Primeiro

A Tool **Quem Vai Primeiro** resolve a discussão de toda mesa de jogo: _quem
começa?_ Sorteia uma característica de uma lista fixa — "é mais alta", "comeu
feijão por último", "chegou por último" — e mostra o texto **grande no centro da
tela**. Quem se encaixa começa o jogo.

É a realização da ideia "Quem começa?" da entrada **Random Tool (Sorteio)** do
roadmap do projeto. Tool interna (built-in no Core), componente
`kit_primeiro`, id `com.kit.primeiro`, despachada pelo
[`kit_tool_manager`](../architecture/tools.md). Card amarelo na grade da Home
(ícone `TOOL_ICON_FIRST` — uma seta apontando para um disco). Texto sobre a cor
da Tool = preto (`KIT_COLOR_ON_YELLOW`).

---

## Tela

Página **única** — sem `lv_tileview`, sem ajuste, sem histórico, sem
persistência. O mesmo formato enxuto da Bottle Tool: a graça
é só o sorteio.

- **Titlebar** fixa: chip de voltar (`KIT_ICON_BACK`) + título `PRIMEIRO`
  (`kit_mono_26`, caixa alta).
- **Palco** (toda a área entre a titlebar e o botão, tocável): a característica
  sorteada em `kit_mono_26` **CAIXA ALTA**, centralizada, quebrando em até quatro
  linhas — **sem "wrap box"** em volta (regra da linguagem visual). Acima,
  `A PESSOA QUE`; abaixo, `COMEÇA O JOGO`, ambos em `kit_mono_16` apagado.
- Antes do primeiro sorteio o palco mostra só a dica `TOQUE EM SORTEAR` (apagada);
  os rótulos `A PESSOA QUE` / `COMEÇA O JOGO` aparecem no primeiro sorteio.
- **Botão `SORTEAR`** fixo no rodapé, na cor da Tool (amarelo, texto preto), `kit_mono_26`.

> **Frase = mono, nunca display.** A tipografia display (`kit_display_*`) é só
> para números e o wordmark. Uma característica é uma frase, então vai em
> `kit_mono_26` caixa alta, mesmo sendo o elemento protagonista da tela.

---

## Execução e animação

A ação principal (`kit_primeiro_draw`) pode ser disparada por:

- botão **`SORTEAR`** no rodapé;
- **toque em qualquer lugar do palco**;
- botão físico **PWR** e o gesto de **chacoalhar** (via
  [`kit_runtime`](../architecture/runtime.md),
  `kit_runtime_set_tool_primary_action`).

Todas fazem exatamente a mesma coisa. Não há efeito se um sorteio já estiver em
curso.

Fluxo: `ocioso → botão/toque/PWR/chacoalhar → embaralhar (~0,7 s) → característica
→ aguarda novo sorteio`. A característica é sorteada **antes** da animação
([Random API](../api/random.md) / TRNG) e **nunca repete a anterior**. Durante o
embaralhar a frase troca a cada tick e fica apagada; ao travar, fica **amarela**
(cor da Tool) e sai **1 bipe** (`kit_audio`, `beep(900, 35)`) — igual à Garrafa.

A animação segue **exatamente** o padrão validado da Dice/Coin Tool: **um único
`lv_timer`** curto (`P_DRAW_TICK_MS` 55 ms), só um `lv_label_set_text` por tick,
e no último tick trava na sorteada, se apaga com `lv_timer_delete` e revela —
tudo no mesmo callback. **Nada de** `transform_scale` / `transform_rotation`: o
layer transformado animado estoura o render no CO5300/PSRAM e reinicia a board.

---

## Lista de características

Fixa no firmware (`TRAITS[]` em `kit_primeiro.c`), **57 entradas**, todas em caixa
alta. Pares opostos entram de propósito ("é mais alta" / "é mais baixa", "chegou
primeiro" / "chegou por último") para dar mais variação. Exemplos:

```
É MAIS ALTA · É MAIS NOVA · ACORDOU MAIS CEDO HOJE · COMEU FEIJÃO POR ÚLTIMO
FAZ ANIVERSÁRIO PRIMEIRO · MORA MAIS LONGE DAQUI · TEM O NOME MAIS CURTO
ESTÁ COM MENOS BATERIA NO CELULAR · RIU POR ÚLTIMO · CALÇA O PÉ MAIOR …
```

---

## Navegação

- **BOOT** volta para a Home fechando a Tool (`kit_system_exit_impl`).
- O chip de voltar da titlebar sai pela API (`system->exit`).

Sem estados persistentes: ao reabrir, a Tool começa do zero (`TOQUE EM SORTEAR`).

---

## Ciclo de vida

| Função | Efeito |
|---|---|
| `kit_primeiro_start(accent)` | Monta a tela e carrega. `accent` 0 → amarelo padrão. |
| `kit_primeiro_draw()` | Ação principal — sorteia. Ligada ao PWR/chacoalhar pelo Runtime. |
| `kit_primeiro_destroy()` | Derruba o `lv_timer` e os objetos LVGL. |
