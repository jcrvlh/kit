/**
 * @file fora_game.c
 * @brief Lógica pura do jogo FORA (sem LVGL).
 *
 * Todas as funções dependem apenas da Random API do KIT para
 * aleatoriedade. Nenhuma função acessa display, input ou áudio.
 */

#include "fora_game.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* -----------------------------------------------------------------------
 * Helpers internos
 * ----------------------------------------------------------------------- */

/** Fallback para rand() quando a Random API não está disponível (stubs). */
static int32_t rng_range(const kit_random_api_t *rng, int32_t min, int32_t max)
{
    if (rng) return rng->range(min, max);
    if (max <= min) return min;
    return min + (rand() % (max - min + 1));
}

/* -----------------------------------------------------------------------
 * Reset
 * ----------------------------------------------------------------------- */

void fora_game_reset(fora_state_t *s)
{
    s->fora_player    = -1;
    s->word_index     = -1;
    s->word_category  = -1;
    s->phase          = FORA_PHASE_CONFIG;
    s->current_player = 0;
    s->current_round  = 0;
    s->current_pair   = 0;
    s->num_pairs      = 0;
    s->has_prev_pairs = false;
    s->voted_player   = -1;
    s->guess_correct  = -1;
    s->guess_chosen   = -1;
    s->fora_won       = false;

    memset(s->pairs_from,      0, sizeof(s->pairs_from));
    memset(s->pairs_to,        0, sizeof(s->pairs_to));
    memset(s->prev_pairs_from, 0, sizeof(s->prev_pairs_from));
    memset(s->prev_pairs_to,   0, sizeof(s->prev_pairs_to));
    memset(s->guess_words,     0, sizeof(s->guess_words));
}

/* -----------------------------------------------------------------------
 * Seleção de palavra e FORA
 * ----------------------------------------------------------------------- */

void fora_game_select_word(fora_state_t *s, const kit_random_api_t *rng)
{
    /* Resolve a categoria (MIX = sorteia uma) */
    if (s->category_index == FORA_MIX_INDEX) {
        s->word_category = rng_range(rng, 0, FORA_CATEGORY_COUNT - 1);
    } else {
        s->word_category = s->category_index;
    }

    /* Sorteia a palavra dentro da categoria */
    const fora_category_t *cat = &FORA_CATEGORIES[s->word_category];
    s->word_index = rng_range(rng, 0, cat->count - 1);

    /* Sorteia quem é o FORA */
    s->fora_player = rng_range(rng, 0, s->num_players - 1);

    printf("[FORA] Categoria: %s, Palavra: %s, FORA: Jogador %d\n",
           cat->name, cat->words[s->word_index], s->fora_player + 1);
}

/* -----------------------------------------------------------------------
 * Geração de pares (cadeia hamiltoniana via Fisher-Yates)
 * ----------------------------------------------------------------------- */

/**
 * Gera uma permutação circular aleatória: cada jogador pergunta para
 * exatamente um outro jogador, e cada jogador responde exatamente uma vez.
 *
 * Algorítmo:
 * 1. Cria array [0, 1, 2, ..., n-1]
 * 2. Fisher-Yates shuffle
 * 3. perm[i] → perm[(i+1) % n] cria a cadeia
 *
 * Isso garante que ninguém pergunta para si mesmo.
 */
static void generate_chain(fora_state_t *s, const kit_random_api_t *rng)
{
    int n = s->num_players;
    int perm[FORA_MAX_PLAYERS];

    /* Inicializa */
    for (int i = 0; i < n; i++) perm[i] = i;

    /* Fisher-Yates shuffle */
    for (int i = n - 1; i > 0; i--) {
        int j = rng_range(rng, 0, i);
        int tmp = perm[i];
        perm[i] = perm[j];
        perm[j] = tmp;
    }

    /* Monta a cadeia: perm[i] pergunta para perm[(i+1) % n] */
    s->num_pairs = n;
    for (int i = 0; i < n; i++) {
        s->pairs_from[i] = perm[i];
        s->pairs_to[i]   = perm[(i + 1) % n];
    }
}

/**
 * Verifica se os pares atuais são idênticos aos anteriores.
 * Retorna true se TODOS os pares são iguais (mesma direção).
 */
static bool pairs_equal(const fora_state_t *s)
{
    if (!s->has_prev_pairs) return false;

    for (int i = 0; i < s->num_pairs; i++) {
        bool found = false;
        for (int j = 0; j < s->num_pairs; j++) {
            if (s->pairs_from[i] == s->prev_pairs_from[j] &&
                s->pairs_to[i]   == s->prev_pairs_to[j]) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

void fora_game_generate_pairs(fora_state_t *s, const kit_random_api_t *rng)
{
    /* Tenta gerar pares diferentes dos da rodada anterior (até 10 tentativas). */
    int attempts = 0;
    do {
        generate_chain(s, rng);
        attempts++;
    } while (pairs_equal(s) && attempts < 10);

    if (attempts > 1) {
        printf("[FORA] Pares gerados após %d tentativas\n", attempts);
    }
}

void fora_game_save_prev_pairs(fora_state_t *s)
{
    memcpy(s->prev_pairs_from, s->pairs_from, sizeof(s->pairs_from));
    memcpy(s->prev_pairs_to,   s->pairs_to,   sizeof(s->pairs_to));
    s->has_prev_pairs = true;
}

/* -----------------------------------------------------------------------
 * Votação
 * ----------------------------------------------------------------------- */

bool fora_game_vote_correct(const fora_state_t *s)
{
    return s->voted_player == s->fora_player;
}

/* -----------------------------------------------------------------------
 * Chute final
 * ----------------------------------------------------------------------- */

void fora_game_generate_guess(fora_state_t *s, const kit_random_api_t *rng)
{
    const fora_category_t *cat = &FORA_CATEGORIES[s->word_category];

    /* A posição correta é aleatória */
    s->guess_correct = rng_range(rng, 0, FORA_GUESS_OPTIONS - 1);

    /* Preenche as 4 opções */
    for (int i = 0; i < FORA_GUESS_OPTIONS; i++) {
        if (i == s->guess_correct) {
            s->guess_words[i] = s->word_index;
        } else {
            /* Sorteia uma palavra falsa que não repita */
            int w;
            bool ok;
            do {
                w = rng_range(rng, 0, cat->count - 1);
                ok = (w != s->word_index);
                /* Verifica se não é duplicata das outras opções */
                for (int j = 0; j < i && ok; j++) {
                    if (s->guess_words[j] == w) ok = false;
                }
            } while (!ok);
            s->guess_words[i] = w;
        }
    }

    s->guess_chosen = -1;
}

bool fora_game_check_guess(fora_state_t *s, int chosen)
{
    s->guess_chosen = chosen;
    bool correct = (chosen == s->guess_correct);
    s->fora_won = correct;
    return correct;
}

/* -----------------------------------------------------------------------
 * Acessors
 * ----------------------------------------------------------------------- */

const char *fora_game_get_word(const fora_state_t *s)
{
    if (s->word_category < 0 || s->word_index < 0) return "???";
    return FORA_CATEGORIES[s->word_category].words[s->word_index];
}

const char *fora_game_get_category_name(const fora_state_t *s)
{
    if (s->word_category < 0) return "MIX";
    return FORA_CATEGORIES[s->word_category].name;
}

const char *fora_game_get_guess_word(const fora_state_t *s, int option)
{
    if (option < 0 || option >= FORA_GUESS_OPTIONS) return "???";
    return FORA_CATEGORIES[s->word_category].words[s->guess_words[option]];
}
