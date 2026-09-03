# Quebra-Gelo

A Tool **Quebra-Gelo** sorteia uma pergunta leve e criativa de um baralho fixo e
mostra o texto **grande no centro da tela**. Todo mundo na roda responde — bom
pra começar uma noite de jogos, uma reunião ou um jantar.

> **Tool do catálogo**, não built-in do Core. Vive em
> [`kit-tools`](https://github.com/jcrvlh/kit-tools/tree/main/tools/io.github.jcrvlh.quebragelo)
> como `io.github.jcrvlh.quebragelo` (pacote `.kit`, carregado do cartão microSD
> pelo [`kit_tool_loader`](../architecture/tools.md)). Nasceu como componente
> built-in `kit_quebragelo` para iteração rápida e saiu do Core quando estabilizou
> — a lógica já só usava `kit_api`, então a conversão foi `tool_init`/
> `tool_destroy` + a ação principal ligada ao toque e a `register_shake_callback`.
>
> O Core mantém: o ícone geométrico da Home (`TOOL_ICON_ASK` — um balão de fala
> com reticências) e o mapa `"ask"`.

Card **azul** na grade da Home, classificada como **ferramenta** (`"kind":"tool"`) —
aparece junto das Tools, não na seção de mini-jogos. Texto sobre o azul = paper
(`KIT_COLOR_ON_COLOR`).

---

## Tela

Página **única** — sem `lv_tileview`, sem ajuste, sem histórico, sem
persistência. O mesmo formato enxuto da [Quem Vai Primeiro](primeiro.md) e da
Bottle Tool: a graça é só o sorteio.

- **Titlebar** fixa: chip de voltar (`KIT_ICON_BACK`) + título `QUEBRA-GELO`
  (`kit_mono_26`, caixa alta).
- **Palco** (toda a área entre a titlebar e o botão, tocável): a pergunta
  sorteada em `kit_mono_26` **CAIXA ALTA**, centralizada, quebrando em várias
  linhas — **sem "wrap box"** em volta (regra da linguagem visual). Acima,
  `PERGUNTA`; abaixo, `PASSE ADIANTE`, ambos em `kit_mono_16` apagado.
- Antes do primeiro sorteio o palco mostra só a dica `TOQUE EM SORTEAR`
  (apagada); os rótulos aparecem no primeiro sorteio.
- **Botão `SORTEAR`** fixo no rodapé, na cor da Tool (azul), `kit_mono_26`.

> **Pergunta = mono, nunca display.** A tipografia display (`kit_display_*`) é só
> para números e o wordmark. Uma pergunta é uma frase, então vai em `kit_mono_26`
> caixa alta, mesmo sendo o elemento protagonista da tela.

---

## Execução e animação

A ação principal (`kit_quebragelo_draw`) pode ser disparada por:

- botão **`SORTEAR`** no rodapé;
- **toque em qualquer lugar do palco**;
- botão físico **PWR** e o gesto de **chacoalhar** (via
  [`kit_runtime`](../architecture/runtime.md),
  `kit_runtime_set_tool_primary_action`).

Todas fazem exatamente a mesma coisa. Não há efeito se um sorteio já estiver em
curso.

Fluxo: `ocioso → botão/toque/PWR/chacoalhar → embaralhar (~0,7 s) → pergunta →
aguarda novo sorteio`. A pergunta é sorteada **antes** da animação
([Random API](../api/random.md) / TRNG) e **nunca repete a anterior**. Durante o
embaralhar a frase troca a cada tick e fica apagada; ao travar, fica **azul**
(cor da Tool) e sai **1 bipe** (`kit_audio`, `beep(900, 35)`).

A animação segue **exatamente** o padrão validado da Dice/Coin/Primeiro: **um
único `lv_timer`** curto (`Q_DRAW_TICK_MS` 55 ms), só um `lv_label_set_text` por
tick, e no último tick trava na sorteada, se apaga com `lv_timer_delete` e revela
— tudo no mesmo callback. **Nada de** `transform_scale` / `transform_rotation`.

---

## Baralho de perguntas

Fixo no firmware (`QUESTIONS[]` em `kit_quebragelo.c`), **~95 entradas**, todas em
caixa alta e curtas o bastante para caber em poucas linhas de `kit_mono_26`.
Mistura hipotéticas ("se você fosse um eletrodoméstico, qual seria?"),
preferências ("qual comida você comeria todo dia sem enjoar?"), memórias ("qual
foi sua maior gafe recente?") e criativas ("que som você faria se fosse um
desenho animado?"). Nada pesado — é quebra-gelo. Exemplos:

```
QUAL SUPERPODER INÚTIL VOCÊ GOSTARIA DE TER?
QUAL MÚSICA GRUDA NA SUA CABEÇA COM MAIS FACILIDADE?
SE VOCÊ ABRISSE UM RESTAURANTE, QUAL SERIA O PRATO?
QUAL OBJETO INÚTIL VOCÊ NUNCA CONSEGUE JOGAR FORA?
QUAL SERIA O SEU NOME ARTÍSTICO? …
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
| `tool_init(ctx)` | Monta a tela, registra callback de shake e carrega. |
| `tool_destroy()` | Derruba o `lv_timer`, solta o callback de shake e deleta a tela. |
