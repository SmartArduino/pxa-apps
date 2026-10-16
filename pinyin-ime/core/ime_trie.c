/*
 * SPDX-FileCopyrightText: 2026 lvgl_pinyin_ime contributors
 * SPDX-License-Identifier: Apache-2.0
 *
 * Index over the T9 syllable table.
 *
 * The 9-key front end repeatedly asks two questions while the user types:
 *
 *   - "which syllables can 94 still grow into?"  -> ime_trie_children()
 *   - "is 946 a prefix of any syllable?"         -> ime_trie_is_prefix()
 *
 * A generated 288-byte directory selects one/two-digit ranges in constant time.
 * Remaining digits are read from the canonical spellings: there is no second
 * string table, pointer relocation, allocated index or startup work.
 * The scan order is stabilised by sorting the matches by syllable length, which
 * is the order the 9-key candidate row wants.
 *
 * The module is named "trie" because that is its place in the architecture
 * (docs/architecture.md); the implementation is the smallest thing that
 * answers the queries correctly.
 */

#include "ime_trie.h"

#include "generated/ime_t9_syllables.h"

#include <string.h>

static bool s_inited;

bool ime_trie_init(void)
{
    s_inited = true;
    return true;
}

void ime_trie_deinit(void)
{
    s_inited = false;
}

bool ime_trie_is_ready(void)
{
    return s_inited;
}

size_t ime_trie_syllable_count(void)
{
    return IME_T9_SYLLABLE_COUNT;
}

const char *ime_trie_syllable(size_t index)
{
    if (index >= IME_T9_SYLLABLE_COUNT) {
        return NULL;
    }
    return ime_t9_syllables[index].syllable;
}

#if defined(__GNUC__)
__attribute__((noinline))
#endif
bool ime_trie_match_tail(const ime_t9_syllable_t *row, const char *digits, size_t length)
{
#if defined(__clang__)
#pragma clang loop unroll(disable)
#endif
    for (size_t i = 2; i < length; ++i) {
        if (ime_t9_letter_digits[row->syllable[i] - 'a'] != digits[i]) return false;
    }
    return true;
}

ime_t9_range_t ime_trie_prefix_range(const char *prefix, size_t length)
{
    ime_t9_range_t empty = {IME_T9_SYLLABLE_COUNT, IME_T9_SYLLABLE_COUNT};
    if (!prefix || !length || length > IME_T9_SYLLABLE_MAX_DIGITS) return empty;
#if defined(__clang__)
#pragma clang loop unroll(disable)
#endif
    for (size_t i = 0; i < length; ++i) {
        if (prefix[i] < '2' || prefix[i] > '9') return empty;
    }
    size_t index = length == 1 ? 64 + prefix[0] - '2' :
        (prefix[0] - '2') * 8 + prefix[1] - '2';
    ime_t9_range_t range = ime_t9_prefixes[index];
    if (length <= 2) return range;
    while (range.begin < range.end &&
           (ime_t9_syllables[range.begin].length < length ||
            !ime_trie_match_tail(&ime_t9_syllables[range.begin], prefix, length))) ++range.begin;
    uint16_t end = range.begin;
    while (end < range.end && ime_t9_syllables[end].length >= length &&
           ime_trie_match_tail(&ime_t9_syllables[end], prefix, length)) ++end;
    range.end = end;
    return range;
}

bool ime_trie_is_prefix(const char *prefix)
{
    if (!prefix) return false;
    ime_t9_range_t range = ime_trie_prefix_range(prefix, strlen(prefix));
    return range.begin < range.end;
}

size_t ime_trie_children(const char *prefix, ime_trie_match_t *out, size_t max)
{
    if (prefix == NULL || prefix[0] == '\0' || out == NULL || max == 0) {
        return 0;
    }

    size_t len = strlen(prefix);
    size_t count = 0;
    ime_t9_range_t range = ime_trie_prefix_range(prefix, len);
    for (size_t i = range.begin; i < range.end && count < max; i++) {

        out[count].syllable = ime_t9_syllables[i].syllable;
        out[count].length = ime_t9_syllables[i].length;
        out[count].is_exact = (ime_t9_syllables[i].length == len);
        count++;
    }

    /* Shortest spelling first: it is both the most likely and the most compact
     * to show in the 9-key row. Insertion sort, at most a handful of entries. */
    for (size_t i = 1; i < count; i++) {
        ime_trie_match_t item = out[i];
        size_t j = i;
        while (j > 0 && out[j - 1].length > item.length) {
            out[j] = out[j - 1];
            j--;
        }
        out[j] = item;
    }

    return count;
}

bool ime_trie_exact(const char *digits, size_t *out_index)
{
    if (digits == NULL || digits[0] == '\0') {
        return false;
    }

    size_t len = strlen(digits);
    ime_t9_range_t range = ime_trie_prefix_range(digits, len);
    size_t index = range.begin;
    if (index == range.end || ime_t9_syllables[index].length != len) return false;
    if (out_index != NULL) *out_index = index;
    return true;
}
