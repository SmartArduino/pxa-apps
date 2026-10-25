/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void game2048_set_random(uint32_t (*source)(void));
uint8_t findTarget(uint8_t array[4], uint8_t x, uint8_t stop);
bool slideArray(uint8_t array[4], uint32_t *score);
void rotateBoard(uint8_t board[4][4]);
bool moveUp(uint8_t board[4][4], uint32_t *score);
bool moveLeft(uint8_t board[4][4], uint32_t *score);
bool moveDown(uint8_t board[4][4], uint32_t *score);
bool moveRight(uint8_t board[4][4], uint32_t *score);
uint8_t countEmpty(uint8_t board[4][4]);
bool gameEnded(uint8_t board[4][4]);
void addRandom(uint8_t board[4][4]);
void initBoard(uint8_t board[4][4]);
#ifdef __cplusplus
}
#endif
