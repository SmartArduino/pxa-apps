/*
 * SPDX-FileCopyrightText: 2026 lvgl_pinyin_ime contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Generated prefix ranges over the immutable, digit-sorted T9 syllable table.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "generated/ime_t9_syllables.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One syllable that matches a digit prefix. */
typedef struct {
    const char *syllable; /**< full pinyin, for example "xiang" */
    uint8_t length;       /**< spelling/digit count */
    bool is_exact;        /**< true when the digits match the prefix exactly */
} ime_trie_match_t;

/** Prepare the index. Idempotent. */
bool ime_trie_init(void);

/** Release the index. */
void ime_trie_deinit(void);

/** True once ime_trie_init() ran. */
bool ime_trie_is_ready(void);

/** Number of syllables in the table. */
size_t ime_trie_syllable_count(void);

/** Syllable string at an absolute table index, or NULL. */
const char *ime_trie_syllable(size_t index);

/** Matching half-open range; empty if invalid. No allocation or startup index. */
ime_t9_range_t ime_trie_prefix_range(const char *prefix, size_t length);

/** Compare the remaining digits after the range already matched the first two.
 * Caller supplies a row from that range and length <= row->length. */
bool ime_trie_match_tail(const ime_t9_syllable_t *row, const char *digits, size_t length);

/** True when @p prefix can still be extended into a syllable. */
bool ime_trie_is_prefix(const char *prefix);

/**
 * Syllables that start with @p prefix, shortest spelling first.
 *
 * @param out  caller owned array
 * @param max  capacity of @p out
 * @return number of matches written
 */
size_t ime_trie_children(const char *prefix, ime_trie_match_t *out, size_t max);

/**
 * Find the syllable whose key sequence is exactly @p digits.
 *
 * @param out_index  receives the table index
 */
bool ime_trie_exact(const char *digits, size_t *out_index);

#ifdef __cplusplus
}
#endif
