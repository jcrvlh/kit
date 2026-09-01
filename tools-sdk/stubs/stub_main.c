/**
 * @file stub_main.c
 * @brief Driver main() para execução de Tools no modo stub (desktop).
 *
 * Este arquivo fornece um ponto de entrada `main()` que simula o Runtime
 * do KIT em desktop: cria um contexto stub, chama tool_init() e
 * tool_destroy(), permitindo testar a lógica da Tool sem hardware.
 */

#include "kit_tool_api.h"
#include "kit_stubs.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=== KIT SDK Stub Runner ===\n\n");

    kit_tool_ctx_t ctx = kit_stub_create_context(NULL);

    printf("Iniciando Tool...\n");
    kit_err_t err = tool_init(&ctx);

    if (err != KIT_OK) {
        printf("tool_init falhou com código %d\n", (int)err);
        return 1;
    }

    printf("\nTool inicializada com sucesso.\n");
    printf("(No modo stub, a Tool executa sem UI — verifique os logs acima.)\n\n");

    printf("Encerrando Tool...\n");
    tool_destroy();

    printf("\n=== Fim ===\n");
    return 0;
}
