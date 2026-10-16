#include "core/ime_t9.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static void digits(const char *text) {
    ime_t9_reset();
    for (;*text;++text) assert(ime_t9_push_digit(*text));
    assert(ime_t9_count()>0 && ime_t9_count()<=IME_T9_MAX_PINYIN);
}
static int contains(const char *text) {
    for(size_t i=0;i<ime_t9_count();++i) if (!strcmp(ime_t9_pinyin(i),text)) return 1;
    return 0;
}
int main(void) {
    ime_t9_init(); digits("32"); assert(!contains("ea") && !contains("e'a"));
    digits("64"); assert(contains("ni")); digits("64426"); assert(contains("ni'hao"));
    // A complete first syllable plus an incomplete second must stay typeable.
    digits("649"); assert(ime_t9_len()==3);
    digits("64");
    for (size_t i=0;i<ime_t9_count();++i)
        if (!strcmp(ime_t9_pinyin(i),"ni")) assert(ime_t9_select(i));
    assert(ime_t9_split()); assert(!strcmp(ime_t9_selected_pinyin(),"ni'"));
    assert(!ime_t9_split());
    assert(ime_t9_push_digit('4') && ime_t9_push_digit('2') && ime_t9_push_digit('6'));
    assert(contains("ni'hao"));
    for(size_t i=0;i<ime_t9_count();++i) assert(!strncmp(ime_t9_pinyin(i),"ni'",3));
    ime_t9_backspace(); ime_t9_backspace(); ime_t9_backspace();
    assert(!strcmp(ime_t9_selected_pinyin(),"ni'"));
    ime_t9_backspace(); assert(ime_t9_len()==2 && !strcmp(ime_t9_selected_pinyin(),"ni"));
    ime_t9_backspace(); assert(ime_t9_len()==1);
    assert(ime_t9_restore("hao") && !strcmp(ime_t9_digits(),"426"));
    assert(!strcmp(ime_t9_selected_pinyin(),"hao"));
    assert(ime_t9_push_digit('6') && ime_t9_len()==4);
    assert(ime_t9_restore("ni'hao") && !strcmp(ime_t9_digits(),"64426"));
    assert(!strcmp(ime_t9_selected_pinyin(),"ni'hao"));
    assert(ime_t9_push_digit('6'));
    for(size_t i=0;i<ime_t9_count();++i) assert(!strncmp(ime_t9_pinyin(i),"ni'",3));
    assert(ime_t9_restore("ni'") && !strcmp(ime_t9_selected_pinyin(),"ni'"));
    ime_t9_backspace(); assert(!strcmp(ime_t9_selected_pinyin(),"ni"));
    assert(!ime_t9_restore("a'wrong") && !strcmp(ime_t9_selected_pinyin(),"ni"));
    digits("645"); assert(ime_t9_count() > 1);
    assert(ime_t9_select(ime_t9_count()-1));
    char selected[IME_T9_MAX_PINYIN_LEN+1]; strcpy(selected,ime_t9_selected_pinyin());
    assert(!ime_t9_push_digit('5'));
    assert(!strcmp(selected,ime_t9_selected_pinyin()) && !strcmp(ime_t9_digits(),"645"));
    for (unsigned trial=0;trial<1000;++trial) {
        ime_t9_reset();
        for(unsigned i=0;i<IME_T9_MAX_DIGITS;++i) (void)ime_t9_push_digit('7');
        assert(ime_t9_len()<=IME_T9_MAX_DIGITS && ime_t9_count()<=IME_T9_MAX_PINYIN);
        while(ime_t9_len()) ime_t9_backspace();
        assert(!ime_t9_count());
    }
    ime_t9_deinit(); puts("T9: exact readings, multi-syllable prefix, bounded ambiguous input and reset passed");
}
