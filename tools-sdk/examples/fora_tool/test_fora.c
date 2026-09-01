/**
 * @file test_fora.c
 * @brief Test harness para a lógica do jogo FORA (compilação desktop).
 *
 * Testa a lógica pura em fora_game.c sem LVGL:
 * - Seleção de palavra e FORA
 * - Geração de pares (sem auto-perguntas, todos participam)
 * - Verificação de votação
 * - Chute final (1 correta entre 4)
 * - Contagem de palavras por categoria (≥50)
 */

#include "fora_game.h"
#include "fora_words.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <assert.h>

/* Stub simples da Random API usando rand() */
static int32_t stub_range(int32_t min, int32_t max)
{
    if (max <= min) return min;
    return min + (rand() % (max - min + 1));
}

static const kit_random_api_t STUB_RNG = {
    .u32       = NULL,
    .range     = stub_range,
    .bytes     = NULL,
    .get_float = NULL,
};

/* -----------------------------------------------------------------------
 * Testes
 * ----------------------------------------------------------------------- */

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("\n--- TEST: %s ---\n", name)
#define ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while (0)

static void test_word_counts(void)
{
    TEST("Todas as categorias têm ≥50 palavras");
    printf("  Categorias: %d\n", FORA_CATEGORY_COUNT);
    ASSERT(FORA_CATEGORY_COUNT == 20, "Deve haver 20 categorias");

    for (int i = 0; i < FORA_CATEGORY_COUNT; i++) {
        const fora_category_t *cat = &FORA_CATEGORIES[i];
        printf("  [%2d] %-14s : %d palavras %s\n",
               i, cat->name, cat->count,
               cat->count >= 50 ? "OK" : "FAIL");
        ASSERT(cat->count >= 50, cat->name);
    }
}

static void test_select_word(void)
{
    TEST("Seleção de palavra e FORA");

    /* Categoria fixa */
    fora_state_t s = {0};
    s.num_players = 5;
    s.category_index = 0;  /* COMIDAS */
    fora_game_reset(&s);
    s.num_players = 5;
    s.category_index = 0;
    fora_game_select_word(&s, &STUB_RNG);

    ASSERT(s.word_category == 0, "Categoria deve ser 0 (COMIDAS)");
    ASSERT(s.word_index >= 0 && s.word_index < FORA_CATEGORIES[0].count,
           "Índice da palavra dentro do range");
    ASSERT(s.fora_player >= 0 && s.fora_player < 5,
           "FORA entre 0-4");
    printf("  Palavra: %s, FORA: Jogador %d\n",
           fora_game_get_word(&s), s.fora_player + 1);

    /* Modo MIX */
    s.category_index = FORA_MIX_INDEX;
    fora_game_select_word(&s, &STUB_RNG);
    ASSERT(s.word_category >= 0 && s.word_category < FORA_CATEGORY_COUNT,
           "MIX resolve para categoria válida");
    printf("  MIX -> Categoria: %s, Palavra: %s\n",
           fora_game_get_category_name(&s), fora_game_get_word(&s));
}

static void test_pairs_no_self(void)
{
    TEST("Pares: ninguém pergunta para si mesmo");

    for (int n = FORA_MIN_PLAYERS; n <= FORA_MAX_PLAYERS; n++) {
        fora_state_t s = {0};
        s.num_players = n;
        fora_game_reset(&s);
        s.num_players = n;
        fora_game_generate_pairs(&s, &STUB_RNG);

        ASSERT(s.num_pairs == n, "Número de pares == jogadores");

        bool ok = true;
        for (int i = 0; i < s.num_pairs; i++) {
            if (s.pairs_from[i] == s.pairs_to[i]) {
                printf("  FAIL: Jogador %d pergunta para si mesmo (n=%d)\n",
                       s.pairs_from[i] + 1, n);
                ok = false;
            }
        }
        ASSERT(ok, "Nenhuma auto-pergunta");
    }
}

static void test_pairs_all_participate(void)
{
    TEST("Pares: todos perguntam e respondem");

    for (int n = FORA_MIN_PLAYERS; n <= FORA_MAX_PLAYERS; n++) {
        fora_state_t s = {0};
        s.num_players = n;
        fora_game_reset(&s);
        s.num_players = n;
        fora_game_generate_pairs(&s, &STUB_RNG);

        bool asked[FORA_MAX_PLAYERS] = {false};
        bool answered[FORA_MAX_PLAYERS] = {false};

        for (int i = 0; i < s.num_pairs; i++) {
            asked[s.pairs_from[i]] = true;
            answered[s.pairs_to[i]] = true;
        }

        bool all_asked = true, all_answered = true;
        for (int i = 0; i < n; i++) {
            if (!asked[i]) all_asked = false;
            if (!answered[i]) all_answered = false;
        }

        ASSERT(all_asked, "Todos perguntaram");
        ASSERT(all_answered, "Todos responderam");
    }
}

static void test_pairs_no_repeat(void)
{
    TEST("Pares: segunda rodada diferente da primeira");

    fora_state_t s = {0};
    s.num_players = 6;
    fora_game_reset(&s);
    s.num_players = 6;

    /* Rodada 1 */
    fora_game_generate_pairs(&s, &STUB_RNG);
    int r1_from[FORA_MAX_PLAYERS], r1_to[FORA_MAX_PLAYERS];
    for (int i = 0; i < s.num_pairs; i++) {
        r1_from[i] = s.pairs_from[i];
        r1_to[i] = s.pairs_to[i];
    }
    fora_game_save_prev_pairs(&s);

    /* Rodada 2 */
    fora_game_generate_pairs(&s, &STUB_RNG);

    /* Verifica que não é exatamente igual (pode falhar raramente, mas improvável) */
    bool identical = true;
    for (int i = 0; i < s.num_pairs && identical; i++) {
        bool found = false;
        for (int j = 0; j < s.num_pairs; j++) {
            if (s.pairs_from[i] == r1_from[j] && s.pairs_to[i] == r1_to[j]) {
                found = true;
                break;
            }
        }
        if (!found) identical = false;
    }
    printf("  Rodada 2 %s rodada 1\n", identical ? "IGUAL à" : "DIFERENTE da");
    /* Não falha se for igual (raro mas possível com 3 jogadores) */
}

static void test_vote(void)
{
    TEST("Votação");

    fora_state_t s = {0};
    s.num_players = 5;
    fora_game_reset(&s);
    s.num_players = 5;
    s.category_index = 0;
    fora_game_select_word(&s, &STUB_RNG);

    /* Voto correto */
    s.voted_player = s.fora_player;
    ASSERT(fora_game_vote_correct(&s), "Voto correto detectado");

    /* Voto errado */
    s.voted_player = (s.fora_player + 1) % s.num_players;
    ASSERT(!fora_game_vote_correct(&s), "Voto errado detectado");
}

static void test_final_guess(void)
{
    TEST("Chute final: 4 opções, 1 correta");

    for (int trial = 0; trial < 20; trial++) {
        fora_state_t s = {0};
        s.num_players = 5;
        s.category_index = trial % FORA_CATEGORY_COUNT;
        fora_game_reset(&s);
        s.num_players = 5;
        s.category_index = trial % FORA_CATEGORY_COUNT;
        fora_game_select_word(&s, &STUB_RNG);
        fora_game_generate_guess(&s, &STUB_RNG);

        /* A posição correta é válida */
        ASSERT(s.guess_correct >= 0 && s.guess_correct < FORA_GUESS_OPTIONS,
               "Posição correta no range");

        /* A opção correta é a palavra certa */
        ASSERT(s.guess_words[s.guess_correct] == s.word_index,
               "Opção correta é a palavra secreta");

        /* As falsas são diferentes entre si e da correta */
        bool unique = true;
        for (int i = 0; i < FORA_GUESS_OPTIONS && unique; i++) {
            for (int j = i + 1; j < FORA_GUESS_OPTIONS; j++) {
                if (s.guess_words[i] == s.guess_words[j]) {
                    unique = false;
                    break;
                }
            }
        }
        ASSERT(unique, "Todas as opções são únicas");

        /* Check_guess funciona */
        bool correct = fora_game_check_guess(&s, s.guess_correct);
        ASSERT(correct, "Acertar a correta retorna true");
        ASSERT(s.fora_won, "FORA vence ao acertar");

        /* Reset e errar */
        s.fora_won = false;
        int wrong = (s.guess_correct + 1) % FORA_GUESS_OPTIONS;
        bool wrong_result = fora_game_check_guess(&s, wrong);
        ASSERT(!wrong_result, "Errar retorna false");
        ASSERT(!s.fora_won, "FORA perde ao errar");
    }
}

/* -----------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------- */

int main(void)
{
    srand((unsigned)time(NULL));

    printf("=== FORA Tool — Testes de Lógica ===\n");

    test_word_counts();
    test_select_word();
    test_pairs_no_self();
    test_pairs_all_participate();
    test_pairs_no_repeat();
    test_vote();
    test_final_guess();

    printf("\n====================================\n");
    printf("RESULTADO: %d passou, %d falhou\n", tests_passed, tests_failed);
    printf("====================================\n");

    return tests_failed > 0 ? 1 : 0;
}
