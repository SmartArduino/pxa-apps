// SPDX-License-Identifier: CC0-1.0
#include "chess.hpp"
#include <string.h>
#include "third_party/smallchesslib/smallchesslib.h"

namespace chess {
namespace {
const char pieces[] = ".PNBRQKpnbrqk";
int piece_id(char c) {
    for (int i = 0; i < 13; ++i) if (pieces[i] == c) return i;
    return -1;
}
bool belongs(char p, bool w) { return p != '.' && (p >= 'A' && p <= 'Z') == w; }
uint8_t byte(char c) { return static_cast<uint8_t>(c); }
uint32_t checksum(const uint8_t *p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 16777619u;
    return h;
}
void put32(uint8_t *p, uint32_t v) { for (unsigned i = 0; i < 4; ++i) p[i] = uint8_t(v >> (i * 8)); }
uint32_t get32(const uint8_t *p) {
    uint32_t v = 0; for (unsigned i = 0; i < 4; ++i) v |= uint32_t(p[i]) << (8 * i); return v;
}
void identity(State &s, uint8_t out[POSITION_BYTES]) {
    for (unsigned i = 0; i < 32; ++i)
        out[i] = uint8_t(piece_id(s.board[i * 2]) | (piece_id(s.board[i * 2 + 1]) << 4));
    out[32] = byte(s.board[64]);
    // An en-passant field changes repetition identity only when a legal capture exists.
    const unsigned ep = out[32] & 15;
    if (ep < 8) {
        bool possible = false;
        unsigned row = white(s) ? 4 : 3;
        for (int dx = -1; dx <= 1; dx += 2) {
            int f = int(ep) + dx;
            if (f >= 0 && f < 8) {
                unsigned from = row * 8 + unsigned(f), to = (white(s) ? 5 : 2) * 8 + ep;
                if (s.board[from] == (white(s) ? 'P' : 'p') && SCL_boardMoveIsLegal(s.board, from, to)) possible = true;
            }
        }
        if (!possible) out[32] |= 15;
    }
    out[33] = white(s) ? 0 : 1;
}
bool dead(const State &s) {
    unsigned minors = 0, knights = 0; int bishop_color = -1;
    for (unsigned i = 0; i < 64; ++i) {
        const char p = char(s.board[i] | 32);
        if (p == 'p' || p == 'r' || p == 'q') return false;
        if (p == 'n') { ++knights; ++minors; }
        if (p == 'b') {
            int color = int((i / 8 + i % 8) & 1);
            if (bishop_color >= 0 && bishop_color != color) return false;
            bishop_color = color; ++minors;
        }
    }
    return minors <= 1 || (knights == 0 && bishop_color >= 0);
}
void update_result(State &s) {
    const uint8_t p = SCL_boardGetPosition(s.board);
    if (p == SCL_POSITION_MATE) s.result = white(s) ? Result::BLACK_WIN : Result::WHITE_WIN;
    else if (p == SCL_POSITION_STALEMATE) s.result = Result::STALEMATE;
    else if (dead(s)) s.result = Result::DEAD;
    else if (byte(s.board[66]) >= 100) s.result = Result::FIFTY_MOVE;
    else {
        unsigned repeats = 0;
        for (unsigned i = 0; i < s.history_count; ++i)
            if (!memcmp(s.history[i], s.history[s.history_count - 1], POSITION_BYTES)) ++repeats;
        s.result = repeats >= 3 ? Result::REPETITION : Result::PLAYING;
    }
}
bool valid_board(char *b) {
    unsigned kings[2] = {}, counts[2] = {}, pawns[2] = {}, king_sq[2] = {};
    for (unsigned i = 0; i < 64; ++i) {
        if (piece_id(b[i]) < 0) return false;
        if (b[i] == '.') continue;
        unsigned side = b[i] >= 'a' ? 1 : 0;
        ++counts[side];
        if (b[i] == 'P' || b[i] == 'p') {
            if (i < 8 || i >= 56) return false;
            ++pawns[side];
        }
        if (b[i] == 'K' || b[i] == 'k') { ++kings[side]; king_sq[side] = i; }
    }
    if (kings[0] != 1 || kings[1] != 1 || counts[0] > 16 || counts[1] > 16 || pawns[0] > 8 || pawns[1] > 8) return false;
    int dx = int(king_sq[0] % 8) - int(king_sq[1] % 8), dy = int(king_sq[0] / 8) - int(king_sq[1] / 8);
    if (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) return false;
    if (b[67] != 0 || b[68] != 0 || byte(b[66]) > 100) return false;
    uint8_t flags = byte(b[64]);
    if ((flags & 0x10) && (b[4] != 'K' || b[7] != 'R')) return false;
    if ((flags & 0x20) && (b[4] != 'K' || b[0] != 'R')) return false;
    if ((flags & 0x40) && (b[60] != 'k' || b[63] != 'r')) return false;
    if ((flags & 0x80) && (b[60] != 'k' || b[56] != 'r')) return false;
    unsigned ep = flags & 15;
    if (ep > 7 && ep != 15) return false;
    if (ep < 8) {
        const bool w = SCL_boardWhitesTurn(b);
        unsigned pawn = (w ? 4 : 3) * 8 + ep;
        if (b[pawn] != (w ? 'p' : 'P') || b[(w ? 5 : 2) * 8 + ep] != '.' || b[(w ? 6 : 1) * 8 + ep] != '.' || b[66] != 0) return false;
    }
    // A legal position can check the active side, but never the side that just moved.
    return !SCL_boardCheck(b, !SCL_boardWhitesTurn(b));
}
int material(char p) {
    switch (p | 32) { case 'p': return 100; case 'n': return 320; case 'b': return 330; case 'r': return 500; case 'q': return 900; case 'k': return 20000; default: return 0; }
}
int evaluate(State &s, unsigned from, unsigned to) {
    char board[BOARD_BYTES]; memcpy(board, s.board, BOARD_BYTES);
    bool side = white(s);
    SCL_boardMakeMove(board, uint8_t(from), uint8_t(to), 'q');
    int score = 0;
    for (unsigned i = 0; i < 64; ++i) {
        char p = board[i]; if (p == '.') continue;
        int v = material(p), file = i % 8, row = i / 8;
        int center = 6 - (file > 3 ? file - 3 : 4 - file) - (row > 3 ? row - 3 : 4 - row);
        if ((p | 32) != 'k') v += center * 3;
        if ((p | 32) == 'p') v += (belongs(p, true) ? row : 7 - row) * 4;
        score += belongs(p, side) ? v : -v;
    }
    // Cheap tactical caution; this is intentionally a beginner, one-ply AI.
    if (SCL_boardSquareAttacked(board, uint8_t(to), !side)) score -= material(board[to]) * 3 / 4;
    if (SCL_boardCheck(board, !side)) score += 24;
    if ((s.board[from] | 32) == 'k' && (int(from) - int(to) == 2 || int(to) - int(from) == 2)) score += 35;
    return score;
}
}

bool white(const State &s) { return !(byte(s.board[65]) & 1); }
bool check(State &s) { return SCL_boardCheck(s.board, white(s)); }
void start(State &s) {
    memset(&s, 0, sizeof(s)); SCL_boardInit(s.board); s.history_count = 1; identity(s, s.history[0]); s.result = Result::PLAYING;
}
bool position(State &s, const char board[BOARD_BYTES]) {
    State tmp{}; memcpy(tmp.board, board, BOARD_BYTES);
    if (!valid_board(tmp.board)) return false;
    tmp.ply = byte(tmp.board[65]); tmp.history_count = 1; identity(tmp, tmp.history[0]); update_result(tmp); s = tmp; return true;
}
bool legal(State &s, unsigned from, unsigned to) {
    return s.result == Result::PLAYING && from < 64 && to < 64 && SCL_boardMoveIsLegal(s.board, uint8_t(from), uint8_t(to));
}
void targets(State &s, unsigned from, uint8_t out[8]) {
    memset(out, 0, 8);
    if (s.result == Result::PLAYING && from < 64 && belongs(s.board[from], white(s))) SCL_boardGetMoves(s.board, uint8_t(from), out);
}
bool promotes(const State &s, unsigned from, unsigned to) {
    return from < 64 && to < 64 && (s.board[from] == 'P' || s.board[from] == 'p') && (to < 8 || to >= 56);
}
bool play(State &s, Move move) {
    if ((move.promotion != 'q' && move.promotion != 'r' && move.promotion != 'b' && move.promotion != 'n') || !legal(s, move.from, move.to)) return false;
    SCL_boardMakeMove(s.board, move.from, move.to, move.promotion); ++s.ply;
    if (s.board[66] == 0) s.history_count = 0;
    if (s.history_count < HISTORY_CAPACITY) { identity(s, s.history[s.history_count]); ++s.history_count; }
    update_result(s); return true;
}
size_t serialize(const State &s, uint8_t *out, size_t capacity) {
    size_t size = 12 + BOARD_BYTES + size_t(s.history_count) * POSITION_BYTES + 4;
    if (!out || s.history_count == 0 || s.history_count > HISTORY_CAPACITY || capacity < size) return 0;
    memcpy(out, "PCH1", 4); put32(out + 4, s.ply); out[8] = s.history_count; out[9] = out[10] = out[11] = 0;
    memcpy(out + 12, s.board, BOARD_BYTES); memcpy(out + 12 + BOARD_BYTES, s.history, size_t(s.history_count) * POSITION_BYTES);
    put32(out + size - 4, checksum(out, size - 4)); return size;
}
bool restore(State &s, const uint8_t *data, size_t size) {
    if (!data || size < 12 + BOARD_BYTES + POSITION_BYTES + 4 || memcmp(data, "PCH1", 4) || data[9] || data[10] || data[11]) return false;
    unsigned n = data[8];
    if (!n || n > HISTORY_CAPACITY || size != 12 + BOARD_BYTES + n * POSITION_BYTES + 4 || get32(data + size - 4) != checksum(data, size - 4)) return false;
    State tmp{}; tmp.ply = get32(data + 4); tmp.history_count = uint8_t(n); memcpy(tmp.board, data + 12, BOARD_BYTES);
    if (!valid_board(tmp.board) || byte(tmp.board[65]) != uint8_t(tmp.ply) || n > unsigned(byte(tmp.board[66])) + 1 || n > uint64_t(tmp.ply) + 1) return false;
    memcpy(tmp.history, data + 12 + BOARD_BYTES, n * POSITION_BYTES);
    for (unsigned j = 0; j < n; ++j) {
        for (unsigned i = 0; i < 32; ++i) if ((tmp.history[j][i] & 15) > 12 || (tmp.history[j][i] >> 4) > 12) return false;
        if (tmp.history[j][33] > 1 || ((tmp.history[j][32] & 15) > 7 && (tmp.history[j][32] & 15) != 15)) return false;
    }
    uint8_t current[POSITION_BYTES]; identity(tmp, current);
    if (memcmp(current, tmp.history[n - 1], POSITION_BYTES)) return false;
    update_result(tmp); s = tmp; return true;
}
bool ai_step(State &s, Ai &ai) {
    if (ai.done) return true;
    if (s.result != Result::PLAYING || ++ai.calls > 320 || ai.candidates >= 256) { ai.done = true; return true; }
    if (ai.to < 64) {
        while (ai.to < 64 && !(ai.moves[ai.to / 8] & (1u << (ai.to % 8)))) ++ai.to;
        if (ai.to < 64) {
            int score = evaluate(s, ai.from - 1, ai.to); ++ai.candidates;
            if (score > ai.best_score) { ai.best_score = score; ai.best = {uint8_t(ai.from - 1), ai.to, 'q'}; }
            ++ai.to; return false;
        }
    }
    while (ai.from < 64 && !belongs(s.board[ai.from], white(s))) ++ai.from;
    if (ai.from == 64) { ai.done = true; return true; }
    targets(s, ai.from, ai.moves); ++ai.from; ai.to = 0; return false;
}
}
