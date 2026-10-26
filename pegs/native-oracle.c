/* SPDX-License-Identifier: MIT. Tests call the unchanged original C engine. */
#include <stdarg.h>
#include "../common/retro-games/puzzles/vendor/pegs.c"

void fatal(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(2);
}

static int peg_count(const game_state *state) {
    int count = 0;
    for (int i = 0; i < state->w * state->h; i++)
        count += state->grid[i] == GRID_PEG;
    return count;
}

int main(int argc, char **argv) {
    game_params *params = default_params();
    if (argc == 2 && !strcmp(argv[1], "--test")) {
        params->w = 3; params->h = 1;
        assert(!validate_desc(params, "PPH"));
        game_state *state = new_game(NULL, params, "PPH");
        assert(peg_count(state) == 2 && game_status(state) == 0);
        assert(!execute_move(state, "0,0-1,0"));
        assert(!execute_move(state, "0,0-2,1"));
        game_state *next = execute_move(state, "0,0-2,0");
        assert(next && peg_count(next) == 1 && game_status(next) == 1);
        assert(peg_count(state) == 2 && state->grid[2] == GRID_HOLE);
        free_game(next); free_game(state); free_params(params);
        puts("PASS: original legal jump, illegal jumps, unchanged source state and one-peg victory");
        return 0;
    }
    if (argc < 2 || validate_desc(params, argv[1])) return 3;
    game_state *state = new_game(NULL, params, argv[1]);
    for (int i = 2; i < argc; i++) {
        game_state *next = execute_move(state, argv[i]);
        if (!next) return 4;
        free_game(state); state = next;
    }
    for (int i = 0; i < state->w * state->h; i++)
        putchar(state->grid[i] == GRID_PEG ? 'P' : state->grid[i] == GRID_HOLE ? 'H' : 'O');
    printf("\n%d %d\n", peg_count(state), game_status(state));
    free_game(state); free_params(params);
    return 0;
}
