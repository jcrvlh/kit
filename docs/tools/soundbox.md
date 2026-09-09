# Soundbox

**Soundbox** é uma mesa de sons de mão: uma grade de até 9 pads; cada **toque**
toca um `.wav` do cartão microSD. Um som novo **corta** o anterior — sem
polifonia, no ritmo de uma soundbox de zoeira.

Tool interna (built-in no Core), componente `kit_soundbox`, id
`com.kit.soundbox`, despachada pelo [`kit_tool_manager`](../architecture/tools.md).
Card **verde** na grade da Home (`TOOL_ICON_SOUNDBOX` — uma grade 3×3 de
quadrados), marcada como **ferramenta** (não `is_game`).

Depende do Runtime **0.7.0** (`api->audio->play_sample` / `stop_sample`).

---

## Bancos de som

Os sons moram em `/sdcard/soundbox/<banco>/`. Cada subpasta é um **banco** — um
conjunto de pads. A página `BANCOS` lista os que a Tool achou no cartão; o
usuário escolhe qual está na grade.

### `banco.json` (opcional)

Na raiz da pasta do banco. Tudo é opcional — sem o arquivo, os `*.wav` entram em
ordem alfabética com o nome do arquivo como rótulo e a cor do banco (verde).

```json
{
  "nome": "Meus Sons",
  "cor": "verde",
  "pads": [
    { "arquivo": "buzina.wav", "rotulo": "BUZINA", "cor": "vermelho" }
  ]
}
```

| Campo | Efeito |
|---|---|
| `nome` | Nome de exibição do banco (senão, o nome da pasta). |
| `cor` | `vermelho` · `azul` · `amarelo` · `verde`. Cor do banco na lista e cor padrão dos pads. |
| `pads[]` | Ordem da grade. `arquivo` (obrigatório) precisa existir na pasta; `rotulo` (senão, o nome do arquivo em CAIXA ALTA); `cor` do pad (senão, herda a do banco). |

Arquivos `.wav` da pasta que não estão em `pads[]` entram depois, em ordem
alfabética. Entradas de `pads[]` cujo arquivo não existe são ignoradas. Teto de
**9 pads** (o resto é descartado). Banco sem nenhum `.wav` não aparece na lista.

### Formato do áudio

**WAV PCM 16-bit, mono, 16 kHz** (ou 8 kHz — reamostrado 2×; estéreo é rebaixado
pra mono). Outros formatos são recusados pelo firmware. Os bancos são montados no
**conversor web** ([`web-installer/soundbox.html`](../../web-installer/soundbox.html),
publicado em `jcrvlh.github.io/kit/soundbox.html`): ele valida cada arquivo, diz
por que não serve, converte, deixa nomear/colorir/reordenar os pads e entrega o
banco de duas formas — **GRAVAR NO CARTÃO** escreve `soundbox/<nome>/` (áudios +
`banco.json`) direto no cartão pela File System Access API (Chrome/Edge desktop,
com o KIT em Modo pen drive), ou um `.zip` da mesma pasta pra descompactar na
raiz do cartão.

---

## Tela

Titlebar fixa + `lv_tileview` horizontal de **3 páginas**
(`BANCOS ◄──► PADS ◄──► ADICIONAR SONS`, começa em **PADS**).

### Página 0 — BANCOS (volume + seletor)

Corpo rolável (`scroll_box` — `plain_box` tira o flag `SCROLLABLE`), tudo à
esquerda menos o stepper de volume (centralizado):

- **VOLUME** — stepper `[ − N% + ]` (±10, botões 66 px, valor em
  `kit_display_44`). Mexe no volume **global** do KIT
  (`api->audio->set_volume` aplica ao vivo + `kit_config_set_volume` persiste),
  igual à tela Ajustes › Som — só acessível sem sair da Tool.
- **BANCOS** — lista de chips (um por banco): nome + `N SOMS`, borda esquerda
  na cor do banco. **Toque** escolhe o banco, regenera a grade e volta pros
  `PADS`. Sem nenhum banco, um texto manda deslizar pra ADICIONAR SONS.

### Página 1 — PADS

O palco. Nome do banco no topo (na cor dele) + grade **3×3** de pads. Cada pad
é um botão na sua cor, rótulo centralizado (`kit_mono_20`, wrap). O toque
dispara em `LV_EVENT_SHORT_CLICKED` — deslizar pra trocar de página ou rolar
**não** toca o som sem querer. Chama
`api->audio->play_sample("/sdcard/soundbox/<banco>/<arquivo>")` e dá um **flash
branco** curto (`FLASH_MS`, 140 ms). Menos de 9 sons → só as primeiras células
aparecem.

### Página 2 — ADICIONAR SONS

Corpo rolável, à esquerda menos o QR (centralizado):

- Título + **5 passos** em linguagem leiga + **QR code** (`lv_qrcode`) pro
  conversor. Sem URL escrita embaixo do QR.
- **SONS DE EXEMPLO** — créditos (todos do Pixabay). Título na cor da Tool.

As fontes do KIT cobrem só Latin-1 — nada de em-dash nos textos.

## Banco de exemplo

`web-installer/soundbox-exemplo/` (deployado junto com o site): 8 sons do
Pixabay já no formato do KIT + `banco.json` + `CREDITOS.txt`. O conversor
oferece carregá-lo direto ("banco de exemplo"), e o `docs/tools/soundbox.md`
lista os créditos. Não é embutido no firmware (partição apertada) — vem pelo
cartão como qualquer outro banco.

---

## Navegação e energia

- **BOOT** e o chip de voltar da titlebar saem pela API (`system->exit`).
- `primary_action` = `kit_soundbox_replay` — o **PWR físico** retoca o último pad
  acionado.
- A Tool **só lê** o cartão; nunca escreve.

---

## Persistência

| Chave (NVS `kit_sys`) | Tipo | Conteúdo |
|---|---|---|
| `sb_bank` | u8 | Índice do banco escolhido (clampado ao nº de bancos no `start`). |

O scan do cartão roda a cada `kit_soundbox_start` — bancos e pads não são
guardados em NVS, só relidos.

---

## Ciclo de vida

| Função | Efeito |
|---|---|
| `kit_soundbox_start(accent)` | Varre `/sdcard/soundbox/`, monta a tela, abre em `PADS` no banco salvo. `accent` 0 → verde. |
| `kit_soundbox_replay()` | Retoca o último pad (PWR físico). |
| `kit_soundbox_destroy()` | `stop_sample()`, derruba o `lv_timer` do flash e os objetos LVGL. O catálogo de bancos em RAM fica pro próximo `start`. |

---

## Futuro: mover para o catálogo

Nasce built-in porque enumera `/sdcard/soundbox/` (fora do sandbox
`/tools/<id>/data/` do SDK). A conversão pra pacote `.kit` do
[catálogo](registry.md) espera o SDK expor listagem de diretório; a lógica de UI
já só usa `kit_api` (`audio`, `system`). Um **pack de sons padrão** embutido nos
`assets/` também fica pra essa etapa.
