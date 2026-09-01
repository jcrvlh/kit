# Adicionando Tools

O KIT já vem com as Tools oficiais (Dados, Garrafa, Moeda, Timer, Primeiro,
Times, Bingo, Quebra-Gelo). Para colocar outras, há dois caminhos.

---

## Tools leves — pelo computador, sem cartão

O jeito mais rápido para Tools pequenas (só código, sem muita imagem ou som).
Elas ficam na memória interna do KIT, que é pequena (cerca de 7 MB no total).

1. Ligue o KIT no computador com um cabo USB-C **de dados**.
2. Abra o **instalador de Tools** no Chrome, Edge ou Opera.
3. Conecte e arraste o arquivo `.kit`.

---

## Tools pesadas — precisam de um cartão microSD

Tools com muitas imagens, áudio ou assets grandes **não cabem na memória
interna** — elas moram num cartão microSD que você espeta na placa. Serve
qualquer micro SD / SDHC / SDXC em **FAT32 ou exFAT** (cartões de 64 GB ou
mais já vêm em exFAT, que funciona direto).

### Opção A: Modo pen drive (sem tirar o cartão)

1. **Ajustes → Modo pen drive → Ativar.**
2. O cartão do KIT aparece no computador como um pen drive. Copie os
   arquivos `.kit` para a raiz do cartão ou para a pasta `tools/`.
3. **Ejete o cartão no computador.** Isso é importante: sair sem ejetar pode
   corromper os arquivos.
4. Toque em **Sair** no KIT e confirme. Ele reinicia e as Tools novas
   aparecem na Home.

Enquanto o modo pen drive está ligado, o KIT fica em espera e o cabo USB
**não** funciona como console nem para o instalador.

> Para sair, use sempre o botão **Sair** na tela (o KIT reinicia
> corretamente). Não desligue na tomada nem use o botão físico com o disco
> ainda montado no computador.

### Opção B: cartão direto no computador

Tire o cartão, ponha num leitor no computador, copie os `.kit` para a raiz
(ou para `tools/`), recoloque no KIT e ligue. Ele descompacta e valida no
próximo boot.

---

## Ver o espaço e formatar o cartão

**Ajustes → Armazenamento** mostra o espaço livre da memória interna e do
cartão, e quantas Tools do cartão estão instaladas.

- **Formatar cartão** — apaga tudo e recria a estrutura que o KIT espera
  (a pasta `tools/`). Cartões grandes saem em exFAT.
- **Procurar cartão** — monta um cartão que você espetou depois de ligar.
- **Recarregar Tools** — relê o cartão sem precisar reiniciar.

---

## Ferramentas × mini-jogos

Na tela **TUDO** da Home as Tools aparecem em seções: **Ferramentas**
(utilitários rápidos — dados, moeda, timer) em cima, **Mini-jogos**
(têm rodada e progressão — o Bingo, por exemplo) no meio, e **Sistema**
(o atalho para os Ajustes) embaixo.
