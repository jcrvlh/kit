/**
 * @file fora_words.h
 * @brief Declarações do banco de palavras para a Tool FORA.
 *
 * 20 categorias com 50+ palavras cada, todas em CAIXA ALTA (UTF-8).
 * Os dados estão definidos em fora_words.c.
 */

#pragma once

/* -----------------------------------------------------------------------
 * Estrutura de Categoria
 * ----------------------------------------------------------------------- */

typedef struct {
    const char *name;            /**< Nome da categoria em CAIXA ALTA. */
    const char *const *words;    /**< Array de palavras (CAIXA ALTA, UTF-8). */
    int count;                   /**< Número de palavras no array. */
} fora_category_t;

/* -----------------------------------------------------------------------
 * Dados (definidos em fora_words.c)
 * ----------------------------------------------------------------------- */

/** Número total de categorias. */
#define FORA_CATEGORY_COUNT  20

/** Tabela de categorias. */
extern const fora_category_t FORA_CATEGORIES[FORA_CATEGORY_COUNT];

/** Índice especial para o modo MIX (sorteia categoria aleatória). */
#define FORA_MIX_INDEX       (-1)
