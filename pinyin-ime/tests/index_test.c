#include "core/ime_trie.h"
#include "core/ime_t9.h"
#include "generated/ime_t9_syllables.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static void row_digits(const ime_t9_syllable_t *row, char *out) {
    static const char keys[] = "22233344455566677778889999";
    for (size_t i=0;i<row->length;++i) out[i]=keys[row->syllable[i]-'a'];
    out[row->length]=0;
}
static void check(const char *prefix, size_t capacity) {
    ime_trie_match_t expected[32], actual[32];
    size_t length = strlen(prefix), first = IME_T9_SYLLABLE_COUNT, count = 0;
    for (size_t i = 0; i < IME_T9_SYLLABLE_COUNT; ++i) {
        const ime_t9_syllable_t *row = &ime_t9_syllables[i];
        char digits[IME_T9_SYLLABLE_MAX_DIGITS+1]; row_digits(row,digits);
        if (strncmp(digits, prefix, length)) continue;
        if (first == IME_T9_SYLLABLE_COUNT) first = i;
        if (count < capacity) expected[count++] = (ime_trie_match_t){row->syllable, row->length, row->length == length};
    }
    ime_t9_range_t range = ime_trie_prefix_range(prefix, length);
    assert(range.begin == range.end ? first == IME_T9_SYLLABLE_COUNT : range.begin == first);
    assert(ime_trie_is_prefix(prefix) == (first != IME_T9_SYLLABLE_COUNT));
    for (size_t i = 1; i < count; ++i) {
        ime_trie_match_t item = expected[i]; size_t j = i;
        while (j && expected[j-1].length > item.length) {
            expected[j] = expected[j-1]; --j;
        }
        expected[j] = item;
    }
    assert(ime_trie_children(prefix, actual, capacity) == count);
    for (size_t i = 0; i < count; ++i) {
        assert(!strcmp(expected[i].syllable, actual[i].syllable));
        assert(expected[i].length == actual[i].length);
        assert(expected[i].is_exact == actual[i].is_exact);
    }
    size_t index = SIZE_MAX;
    bool exact = first != IME_T9_SYLLABLE_COUNT && ime_t9_syllables[first].length == length;
    assert(ime_trie_exact(prefix, &index) == exact);
    if (exact) assert(index == first);
}

int main(void) {
    assert(ime_trie_syllable_count() == IME_T9_SYLLABLE_COUNT);
    for (size_t i = 0; i < IME_T9_SYLLABLE_COUNT; ++i) {
        char digits[IME_T9_SYLLABLE_MAX_DIGITS+1]; row_digits(&ime_t9_syllables[i],digits);
        if (i) {
            char previous[IME_T9_SYLLABLE_MAX_DIGITS+1]; row_digits(&ime_t9_syllables[i-1],previous);
            assert(strcmp(previous,digits)<=0);
        }
        assert(ime_t9_is_valid(ime_t9_syllables[i].syllable));
        char prefix[16]; strcpy(prefix, digits);
        for (size_t length = strlen(prefix); length; --length) {
            prefix[length] = 0;
            check(prefix, 1); check(prefix, 6); check(prefix, 32);
        }
    }
    check("111", 6); check("999999", 6);
    assert(!ime_trie_is_prefix(NULL) && !ime_trie_is_prefix(""));
    assert(!ime_trie_exact(NULL, NULL) && !ime_trie_exact("", NULL));
    assert(!ime_trie_children("2", NULL, 6));
    puts("T9 index: every generated prefix, exact lookup, capacity and stable reading order passed");
}
