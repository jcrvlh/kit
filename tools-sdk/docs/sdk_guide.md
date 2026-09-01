# Guia de Desenvolvimento do SDK

Bem-vindo ao Guia de Desenvolvimento do SDK do KIT! Este documento explica os conceitos fundamentais para criar sua própria Tool e empacotá-la usando a CLI oficial.

## 1. O que é uma Tool?

Diferente de sistemas embarcados tradicionais, onde todo o código é compilado em um único bloco monolítico, o **KIT** adota uma arquitetura modular.
O firmware base gerencia o hardware (display, bateria, boot) e expõe uma tabela de funções (a API). 
Sua Tool é compilada separadamente como um executável `.elf` independente e o KIT a carrega dinamicamente da memória Flash para a RAM quando o usuário a abre na interface.

Isso significa que:
1. Você não precisa compilar o firmware inteiro.
2. Se a sua Tool travar, o KIT a isola e volta para a Home (segurança e robustez).
3. Você pode compartilhar seu arquivo `.kit` e qualquer um pode instalar via Serial.

## 2. A Tabela de API (`kit_api_table_t`)

O coração do SDK é o arquivo `kit_tool_api.h`. Ele define a tabela de ponteiros de função que sua Tool recebe ao iniciar (em `tool_init`).

```c
kit_err_t tool_init(kit_tool_ctx_t *ctx) {
    // Pegue a API do contexto
    const kit_api_table_t *api = ctx->api;
    
    // Use o hardware (ex: bipe)
    if (api->audio) {
        api->audio->beep(1000, 100);
    }
    
    return KIT_OK;
}
```

**Importante:** Você só terá acesso aos módulos que solicitar no `manifest.json`. Se pedir só permissão de `audio`, o ponteiro `api->display` será `NULL`. Sempre valide antes de usar.

## 3. Padrão de Projeto Recomendado

Para manter sua Tool fluida no hardware limitado do ESP32-S3:

1. **Estado Global Local**: Mantenha o estado da sua Tool em variáveis globais estáticas (`static`). Ex: `static lv_obj_t *tela;` ou `static meu_estado_t estado;`.
2. **Callbacks Limpos**: As funções registradas como callback (ex: `on_input` da Input API ou `on_shake` da IMU) rodam no contexto da main task do Runtime. **Nunca bloqueie essas funções** (não use laços infinitos ou delays longos). Processe rapidamente e use a UI (LVGL) para reagir assincronamente.
3. **Persistência Assertiva**: Salve seus dados usando a Storage API no momento exato em que algo importante mudar. Não dependa exclusivamente de salvar no `tool_destroy`, pois se o KIT for forçado a reiniciar (botão), a Tool pode não ter tempo de finalizar.

## 4. Testes e Compilação Desktop (Stubs)

O KIT SDK tem um recurso poderoso: **Stubs de Linkagem**. Ele fornece implementações simuladas ("falsas") das APIs de hardware, permitindo que você compile e teste a lógica da sua Tool diretamente no seu PC, sem LVGL e sem o hardware real.

```bash
# Compila para o desktop (Mac/Linux/Windows)
kit-cli build . --target native

# Executa o simulador stub (imprime logs no terminal)
./build/tool.elf
```

- A **Random API** stub usará a função `rand()` do C.
- A **Storage API** stub salva em um hashmap de memória RAM (apenas durante a execução).
- A **Audio API** stub imprime um `printf` simulando o bipe.
- O **Display** stub retorna `NULL` (a UI não será criada).

Use os stubs para testar regras de negócio, algoritmos (ex: gerar números aleatórios, regras de combate em um RPG) e depurar com `printf` rapidamente.

## 5. Compilação Xtensa e Empacotamento

Quando a lógica estiver pronta, gere o binário real:

```bash
# Exportar variáveis do ESP-IDF
source $IDF_PATH/export.sh

# Compilar para o hardware real (ESP32-S3)
kit-cli build . --target xtensa
```

Isso criará o `tool.elf` verdadeiro. Para empacotar com o manifesto, a CLI cria um `.kit`:

```bash
# Valida as permissões e dependências
kit-cli validate .

# Empacota em .kit (com checksum SHA-256 interno)
kit-cli pack . -o minha_tool.kit
```

## 6. Flash e Instalação

Envie para o hardware através da USB. A CLI usa o `parttool.py` internamente para injetar o `.kit` na partição `littlefs` do ESP32-S3.

```bash
kit-cli flash minha_tool.kit --port /dev/ttyUSB0
```

Seu app aparecerá instantaneamente no menu do KIT. Boa sorte!
