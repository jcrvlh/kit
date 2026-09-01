# Timer Tool

A **Timer Tool** é o cronômetro e a contagem regressiva do KIT num só lugar.
Tool interna (built-in no Core), componente `kit_timer`, id `com.kit.timer`,
despachada pelo [`kit_tool_manager`](../architecture/tools.md). Card verde na
grade da Home (ícone `TOOL_ICON_TIMER`, já existente no launcher).

---

## Navegação

`lv_tileview` horizontal de 2 páginas (indicador de 2 pontos na titlebar), no
mesmo idioma da Dice / Coin:

```
AJUSTE  ◄──►  RELÓGIO
```

A página inicial é o **RELÓGIO**. O botão `◂` da titlebar sai da Tool
(`system->exit`); o botão físico **BOOT** também.

---

## Tela RELÓGIO

Deliberadamente mínima: o mostrador `MM:SS` grande (`kit_display_120`, Archivo
Black — a fonte só tem `0–9 - +`, então os "dois pontos" são **dois quadrados
desenhados**, no idioma Bauhaus de formas em vez de glifos) e dois botões no
rodapé:

| Botão | Papel |
|-------|-------|
| **PARAR** (contorno) | Zera: volta para `00:00` (cronômetro) ou para o tempo configurado (regressivo). Esmaecido/inerte quando não há o que zerar. |
| **COMEÇAR** (verde, primário) | Alterna **COMEÇAR → PAUSAR → CONTINUAR**. |

Enquanto conta, o "dois pontos" pisca (~1,6 Hz). Uma etiqueta discreta no topo
diz o modo atual (`CRONÔMETRO` / `REGRESSIVO`).

O botão físico **PWR** e o gesto de **chacoalhar** fazem exatamente a mesma
coisa que o botão **COMEÇAR** (`kit_timer_toggle`, via `kit_runtime`).

---

## Tela AJUSTE

- **MODO** — `CRONÔMETRO` (conta para cima a partir de `00:00`) ou `REGRESSIVO`
  (conta para baixo). Trocar de modo zera uma contagem em andamento.
- Só no **REGRESSIVO**:
  - **TEMPOS FIXOS · MIN** — pílulas de um toque: `3 · 5 · 10 · 15 · 30`.
  - **OU DEFINA** — duas rodas (`lv_roller`, a mesma da Coin Tool): minutos
    `00–99` e segundos `00–59`. Mexer numa roda limpa o destaque do preset.

A configuração (`modo` + `segundos`) é salva na hora
([Storage API](../api/storage.md), chaves `timer_mode` e `timer_secs`) e
recuperada ao reabrir a Tool. **Não há persistência de uma contagem em
andamento** — sair da Tool zera.

---

## Durante a contagem — tela viva, mas econômica

Enquanto o estado é `RUNNING` ou `PAUSED`, a Tool liga o **keep-awake**
(`kit_power->keep_awake(true)`), e o laço de inatividade do
[`kit_runtime`](../architecture/runtime.md) passa a **ignorar** o repouso da
tela e o desligamento automático — a contagem nunca é interrompida por um
timeout do sistema.

Para poupar a bateria do AMOLED sem apagar, a **própria Tool** escurece o
painel para o mínimo (`kit_display->set_brightness(5)`) depois de **~15 s sem
toque**, e só enquanto está de fato contando (`RUNNING`). Qualquer toque na tela
devolve o brilho salvo do usuário (capturado no início da Tool via
`kit_display->get_brightness()`). Ao pausar, parar, zerar ou ao sair da Tool o
brilho normal é restaurado e o keep-awake é desligado.

---

## Fim do tempo

Quando a contagem regressiva chega a `00:00`, roda a animação de fim
(sem áudio por enquanto) — mesma família visual da tela **CARREGANDO**:

- um overlay cobre a tela toda com um **disco central + ícone de timer**
  (anel + botão em cima + dois ponteiros, formas geométricas no mesmo idioma do
  card da Home) e a palavra **`TEMPO`** embaixo, mais a dica `TOQUE PARA PARAR`;
- o **fundo pisca verde ↔ preto** a cada ~420 ms (`lv_timer`); em cada fase os
  elementos do miolo trocam de cor para manter alto contraste (fundo verde →
  miolo preto; fundo preto → miolo verde);
- **um toque em qualquer lugar da tela para** — fecha o overlay e volta ao
  estado ocioso (mostrador de volta no tempo configurado).

Sem `transform` e sem redesenho por frame — só a troca de cor do overlay a
~2 Hz, bem abaixo do custo de render da board.

---

## Timers internos

| `lv_timer` | Período | Vida | Papel |
|------------|---------|------|-------|
| contagem | 1 s | criado ao iniciar, morto ao pausar/parar/zerar | incrementa/decrementa `MM:SS` |
| animação | 200 ms | toda a Tool | pisca o "dois pontos" + entra/sai do brilho reduzido |
| fim | ~420 ms | só durante a tela de fim | alterna o fundo verde ↔ preto |

> **Ghost de texto ao trocar o rótulo.** O botão primário
> (`COMEÇAR → PAUSAR → CONTINUAR`) muda de comprimento e o LVGL, nesta board,
> não repinta sozinho toda a área do texto anterior → glifos "sobram" (parecia
> `RECOMEÇAR`). Fix: `lv_obj_invalidate(s_go_btn)` depois de trocar o texto —
> repinta o botão inteiro. **Não** travar a largura do rótulo: caixa estreita +
> `LV_LABEL_LONG_CLIP` **corta** o texto (tentado com o mostrador — o
> `kit_display_120` avança ~80 px/dígito e qualquer corte vira "linha" no
> número). O mostrador `MM:SS` fica em largura automática: `"%02d"` tem sempre
> 2 dígitos de mesmo avanço, então a caixa nunca muda de tamanho.
>
> O "dois pontos" pisca por **`bg_opa` nos dois quadrados** (não `opa` no
> container — `opa` intermediário força layer buffer). O botão `PARAR` esmaece
> por `border_opa` + `text_opa`, pelo mesmo motivo.

---

## Desvios registrados em relação à especificação original

1. **Mostrador `MM:SS` (minutos `00–99`), sem horas.** A tela de 368 px não
   comporta 6 dígitos em `kit_display_120` de forma legível, e `99:59` cobre de
   sobra timers de jogo de tabuleiro. A roda de ajuste é minutos + segundos.
2. **Cronômetro sem centésimos** — mostra só `MM:SS`, igual ao regressivo, para
   os dois modos terem a mesma leitura.
3. **`PARAR` também zera** — não há um botão "resetar" separado; parar já
   devolve o mostrador ao ponto de partida.
4. **Sem persistência de sessão** — apenas a configuração (modo + tempo) fica
   salva; uma contagem em andamento é perdida ao sair.
