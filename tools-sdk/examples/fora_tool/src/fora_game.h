/**
 * @file fora_game.h
 * @brief Estado e lógica do jogo FORA (sem dependência de LVGL).
 *
 * Toda a lógica de seleção de palavra, geração de pares, votação e
 * chute final é separada da interface gráfica. Isso permite:
 * - Testar via stubs no desktop;
 * - Reusar a lógica se a UI for refeita;
 * - Manter o main.c focado apenas na apresentação.
 */

#pragma once

#include "kit_tool_api.h"
#include "fora_words.h"

/* -----------------------------------------------------------------------
 * Constantes
 * ----------------------------------------------------------------------- */

#define FORA_MIN_PLAYERS    3
#define FORA_MAX_PLAYERS    12
#define FORA_MAX_ROUNDS     2
#define FORA_GUESS_OPTIONS  4

/* -----------------------------------------------------------------------
 * Fases do jogo (máquina de estados)
 * ----------------------------------------------------------------------- */

typedef enum {
    FORA_PHASE_CONFIG,          /**< Configuração (jogadores, categoria, rodadas). */
    FORA_PHASE_DISTRIBUTE,      /**< "PASSE O KIT — JOGADOR X" (tela neutra). */
    FORA_PHASE_REVEAL,          /**< Jogador vê sua palavra (ou FORA). */
    FORA_PHASE_ALL_READY,       /**< "TODOS PRONTOS?" antes da primeira rodada. */
    FORA_PHASE_QUESTION,        /**< "JOGADOR X PERGUNTE PARA JOGADOR Y". */
    FORA_PHASE_ROUND_END,       /**< "RODADA X CONCLUÍDA". */
    FORA_PHASE_VOTE,            /**< "QUEM RECEBEU A MAIORIA?" (lista de jogadores). */
    FORA_PHASE_VOTE_REVEAL,     /**< Animação de suspense + revelação do voto. */
    FORA_PHASE_FORA_ESCAPED,    /**< Maioria errou — o FORA venceu. */
    FORA_PHASE_FINAL_GUESS,     /**< Chute final (4 opções). */
    FORA_PHASE_GUESS_RESULT,    /**< Resultado do chute (acertou/errou). */
    FORA_PHASE_RESULT,          /**< Resumo final + NOVA PARTIDA / SAIR. */
} fora_phase_t;

/* -----------------------------------------------------------------------
 * Estado completo do jogo
 * ----------------------------------------------------------------------- */

typedef struct {
    /* Configuração */
    int num_players;                     /**< 3–12. */
    int category_index;                  /**< Índice em FORA_CATEGORIES, ou FORA_MIX_INDEX. */
    int num_rounds;                      /**< 1 ou 2. */

    /* Estado secreto */
    int fora_player;                     /**< Índice do jogador FORA (0-based). */
    int word_index;                      /**< Índice da palavra na categoria. */
    int word_category;                   /**< Categoria real (para MIX resolve-se aqui). */

    /* Fase atual */
    fora_phase_t phase;
    int current_player;                  /**< Jogador da distribuição (0-based). */
    int current_round;                   /**< Rodada atual (0-based). */
    int current_pair;                    /**< Par de perguntas atual na rodada. */

    /* Pares de perguntas — cadeia hamiltoniana */
    int pairs_from[FORA_MAX_PLAYERS];    /**< Quem pergunta. */
    int pairs_to[FORA_MAX_PLAYERS];      /**< Para quem. */
    int num_pairs;                       /**< Número de pares na rodada. */

    /* Pares da rodada anterior (para evitar repetição) */
    int prev_pairs_from[FORA_MAX_PLAYERS];
    int prev_pairs_to[FORA_MAX_PLAYERS];
    bool has_prev_pairs;

    /* Votação */
    int voted_player;                    /**< Jogador escolhido pela maioria (-1 = nenhum). */

    /* Chute final */
    int guess_words[FORA_GUESS_OPTIONS]; /**< Índices de palavras (na categoria). */
    int guess_correct;                   /**< Posição da correta (0–3). */
    int guess_chosen;                    /**< Opção escolhida pelo FORA (-1 = ainda não). */

    /* Resultado */
    bool fora_won;                       /**< true = FORA venceu. */
} fora_state_t;

/* -----------------------------------------------------------------------
 * Protótipos
 * ----------------------------------------------------------------------- */

/**
 * Reseta o estado para uma nova partida com as configurações atuais.
 * Não altera num_players, category_index ou num_rounds.
 */
void fora_game_reset(fora_state_t *s);

/**
 * Sorteia a palavra secreta e o jogador FORA usando a Random API.
 * Preenche: fora_player, word_index, word_category.
 */
void fora_game_select_word(fora_state_t *s, const kit_random_api_t *rng);

/**
 * Gera os pares de perguntas para a rodada atual.
 * Usa Fisher-Yates shuffle para criar uma permutação circular.
 * Se has_prev_pairs, tenta evitar repetição (até 10 tentativas).
 * Preenche: pairs_from[], pairs_to[], num_pairs.
 */
void fora_game_generate_pairs(fora_state_t *s, const kit_random_api_t *rng);

/**
 * Salva os pares atuais como pares anteriores (para evitar repetição
 * na segunda rodada).
 */
void fora_game_save_prev_pairs(fora_state_t *s);

/**
 * Verifica se o voto da maioria acertou o FORA.
 * @param s Estado do jogo (voted_player já definido).
 * @return true se o votado é o FORA.
 */
bool fora_game_vote_correct(const fora_state_t *s);

/**
 * Gera as 4 opções do chute final.
 * Uma é a palavra correta, três são falsas da mesma categoria.
 * A posição da correta é aleatória.
 * Preenche: guess_words[], guess_correct.
 */
void fora_game_generate_guess(fora_state_t *s, const kit_random_api_t *rng);

/**
 * Verifica se o chute do FORA acertou.
 * @param s Estado do jogo.
 * @param chosen Índice da opção escolhida (0–3).
 * @return true se acertou a palavra correta.
 */
bool fora_game_check_guess(fora_state_t *s, int chosen);

/**
 * Retorna a palavra secreta da partida atual (string CAIXA ALTA).
 */
const char *fora_game_get_word(const fora_state_t *s);

/**
 * Retorna o nome da categoria (string CAIXA ALTA).
 */
const char *fora_game_get_category_name(const fora_state_t *s);

/**
 * Retorna a palavra de uma opção do chute final (string CAIXA ALTA).
 */
const char *fora_game_get_guess_word(const fora_state_t *s, int option);
