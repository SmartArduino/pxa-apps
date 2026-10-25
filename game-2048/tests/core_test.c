/* SPDX-License-Identifier: MIT */
#include "../core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t rng = 1;
static uint32_t random_value(void) { rng = rng * 1664525u + 1013904223u; return rng; }
static unsigned mass(const uint8_t *row) {
    unsigned sum = 0;
    for (int i = 0; i < 4; ++i) if (row[i]) sum += 1u << row[i];
    return sum;
}
static void merge_cases(void) {
    static const uint8_t cases[][9] = {
        {1,1,1,1, 2,2,0,0, 8},
        {1,1,2,0, 2,2,0,0, 4},
        {2,0,2,2, 3,2,0,0, 8},
        {3,3,3,3, 4,4,0,0, 32},
        {1,2,3,4, 1,2,3,4, 0},
        {0,0,0,0, 0,0,0,0, 0}
    };
    for (unsigned i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        uint8_t row[4]; memcpy(row, cases[i], 4);
        uint32_t score = 10;
        bool moved = slideArray(row, &score);
        assert(!memcmp(row, cases[i]+4, 4));
        assert(score == 10u + cases[i][8]);
        assert(moved == (memcmp(cases[i], cases[i]+4, 4) != 0));
    }
    uint8_t extreme[4] = {31,31,0,0};
    uint32_t score = 0;
    assert(slideArray(extreme, &score) && extreme[0] == 32 && score == 0);
}
static void row_properties(void) {
    for (unsigned bits = 0; bits < 4096; ++bits) {
        uint8_t row[4], before[4];
        for (int i = 0; i < 4; ++i) row[i] = (bits >> (3*i)) & 7;
        memcpy(before, row, 4);
        const unsigned initial_mass = mass(row);
        uint32_t score = 0;
        bool changed = slideArray(row, &score);
        assert(mass(row) == initial_mass);
        assert(changed == (memcmp(row,before,4) != 0));
        bool zero = false;
        for (int i = 0; i < 4; ++i) {
            if (!row[i]) zero = true;
            else assert(!zero);
        }
    }
}
static void directions_and_end(void) {
    uint8_t board[4][4] = {{0}}, saved[4][4];
    board[1][2] = 1; memcpy(saved, board, 16);
    uint32_t score = 0;
    assert(moveUp(board,&score) && board[1][0] == 1);
    memcpy(board,saved,16); assert(moveDown(board,&score) && board[1][3] == 1);
    memcpy(board,saved,16); assert(moveLeft(board,&score) && board[0][2] == 1);
    memcpy(board,saved,16); assert(moveRight(board,&score) && board[3][2] == 1);
    memcpy(board,saved,16);
    for(int i=0;i<4;++i) rotateBoard(board);
    assert(!memcmp(board,saved,16));
    for(int x=0;x<4;++x) for(int y=0;y<4;++y) board[x][y]=1+(x+y)%2;
    memcpy(saved,board,16); assert(gameEnded(board)); assert(!memcmp(board,saved,16));
    board[1][0]=board[0][0]; memcpy(saved,board,16);
    assert(!gameEnded(board)); assert(!memcmp(board,saved,16));
    board[1][0]=2; board[0][1]=board[0][0]; memcpy(saved,board,16);
    assert(!gameEnded(board)); assert(!memcmp(board,saved,16));
    board[0][0]=0; assert(!gameEnded(board));
}
static void spawning(void) {
    game2048_set_random(random_value);
    for (int n=0;n<1000;++n) {
        uint8_t board[4][4]; initBoard(board);
        assert(countEmpty(board)==14);
        for(int x=0;x<4;++x) for(int y=0;y<4;++y) assert(board[x][y]<=2);
        while(countEmpty(board)) {
            uint8_t previous=countEmpty(board);
            addRandom(board); assert(countEmpty(board)==previous-1);
        }
        uint8_t saved[4][4]; memcpy(saved,board,16);
        addRandom(board); assert(!memcmp(board,saved,16));
    }
}
int main(void) {
    merge_cases(); row_properties(); directions_and_end(); spawning();
    puts("2048 core: fixtures, 4096 rows, direction/end invariants, 1000 spawn boards passed");
    return 0;
}
