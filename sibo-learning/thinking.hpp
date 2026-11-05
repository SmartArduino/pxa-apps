// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
namespace sibo::thinking {
// Independent bounded models: no downloaded question banks or platform code.
using Grid = std::array<uint8_t,16>;
uint32_t next(uint32_t &seed);
bool legal(const Grid &g, unsigned cell, unsigned value);
unsigned solutions(Grid g, Grid *answer=nullptr);
struct Sudoku {
  Grid given{}, cells{};
  unsigned hints=0;
  bool credited=false;
  void generate(uint32_t &seed);
  bool conflict(unsigned cell) const;
  bool complete() const;
  bool fill(unsigned cell,unsigned value);
  bool hint(unsigned cell);
};
struct Rational {
  int64_t n=0,d=1;
  Rational(int64_t numerator=0,int64_t denominator=1);
  std::string text() const;
  bool operator==(const Rational &b) const {return n==b.n && d==b.d;}
};
enum class Op:uint8_t {add,sub,mul,div};
struct Move {unsigned a=0,b=1;Op op=Op::add;};
struct Card {Rational value;std::string expression;};
struct Board {
  std::array<Card,4> cards{};
  unsigned count=4;
  bool apply(Move move);
  bool won() const {return count==1 && cards[0].value==Rational(24);}
};
Board board(const std::array<uint8_t,4> &values);
bool solve(Board b,std::array<Move,3> &moves);
struct TwentyFour {
  std::array<uint8_t,4> original{};
  std::array<Move,3> moves{};
  unsigned depth=0;
  bool hinted=false,credited=false;
  Board current() const;
  void generate(uint32_t &seed);
  bool apply(Move move);
  bool undo();
};
struct Fractions {
  unsigned n=1,d=2,rn=2,rd=3,done=0,score=0;
  bool answered=false;
  int relation() const;
  void question(uint32_t &seed);
  bool answer(int comparison);
};
struct State {
  uint32_t seed=0x715ab13;
  Sudoku sudoku;
  TwentyFour cards;
  Fractions fractions;
  unsigned sudoku_alone=0,sudoku_help=0,cards_alone=0,cards_help=0;
  void credit_sudoku();
  void credit_cards();
  std::string encode() const;
  bool decode(std::string_view bytes);
};
} // namespace sibo::thinking
