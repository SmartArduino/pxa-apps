#include "unity.h"

extern void test_init(void);
extern void test_move(void);
extern void test_combat(void);
extern void test_floor(void);
extern void test_interact(void);
extern void test_progress(void);

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_init);
    RUN_TEST(test_move);
    RUN_TEST(test_combat);
    RUN_TEST(test_floor);
    RUN_TEST(test_interact);
    RUN_TEST(test_progress);

    return UNITY_END();
}
