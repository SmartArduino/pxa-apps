// SPDX-License-Identifier: MIT
#include "../thinking.hpp"
#include <cassert>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <set>
#include <utility>
using namespace sibo::thinking;
// Independent brute-force row/column/box oracle, not the production solver.
unsigned count_grid(Grid &g,unsigned at=0) {
  if(at==16)return 1;
  if(g[at])return count_grid(g,at+1);
  unsigned found=0;
  for(unsigned n=1;n<=4;++n) {
    bool valid=true;
    for(unsigned i=0;i<16;++i)if(g[i]==n) {
      if(i/4==at/4||i%4==at%4)valid=false;
      if((i/4)/2==(at/4)/2 && (i%4)/2==(at%4)/2)valid=false;
    }
    if(valid){g[at]=n;found+=count_grid(g,at+1);g[at]=0;}
  }
  return found;
}
using Pair=std::pair<int64_t,int64_t>;
Pair normalized(int64_t a,int64_t b) {
 if(b<0){a=-a;b=-b;}auto g=std::gcd(a,b);return {a/g,b/g};
}
bool oracle24(std::array<uint8_t,4> values) {
 std::array<std::set<Pair>,16> dp;
 for(unsigned mask=1;mask<16;++mask) {
  if(!(mask&(mask-1))) {unsigned i=0;while((1u<<i)!=mask)++i;dp[mask].insert({values[i],1});continue;}
  for(unsigned a=(mask-1)&mask;a;a=(a-1)&mask) {
   auto b=mask^a;if(!b)continue;
   for(auto [an,ad]:dp[a])for(auto [bn,bd]:dp[b]) {
    dp[mask].insert(normalized(an*bd+bn*ad,ad*bd));
    dp[mask].insert(normalized(an*bd-bn*ad,ad*bd));
    dp[mask].insert(normalized(an*bn,ad*bd));
    if(bn)dp[mask].insert(normalized(an*bd,ad*bn));
   }
  }
 }
 return dp[15].count({24,1});
}
int main() {
 uint32_t seed=1;
 for(unsigned trial=0;trial<512;++trial) {
  Sudoku s;s.generate(seed);auto g=s.given;
  assert(count_grid(g)==1);
  assert(std::count(g.begin(),g.end(),0)>=8);
  Grid answer{};assert(solutions(s.given,&answer)==1);
  for(unsigned i=0;i<16;++i) {
   if(s.given[i]){assert(!s.fill(i,0));continue;}
   assert(!s.fill(i,5));s.fill(i,answer[i]);
  }
  assert(s.complete());
 }
 Sudoku s;s.generate(seed);unsigned empty=0;while(s.given[empty])++empty;
 unsigned wrong=1;while(legal(s.cells,empty,wrong))++wrong;
 assert(wrong<=4 && s.fill(empty,wrong) && s.conflict(empty));assert(s.hint(empty));assert(!s.conflict(empty));
 State sudoku_score;sudoku_score.sudoku=s;
 Grid completed{};assert(solutions(s.given,&completed)==1);
 for(unsigned i=0;i<16;++i)sudoku_score.sudoku.fill(i,completed[i]);
 sudoku_score.credit_sudoku();assert(sudoku_score.sudoku_help==1 && sudoku_score.sudoku_alone==0);
 assert(sudoku_score.sudoku.fill(empty,0));sudoku_score.credit_sudoku();
 assert(sudoku_score.sudoku.fill(empty,completed[empty]));sudoku_score.credit_sudoku();
 assert(sudoku_score.sudoku_help==1 && sudoku_score.sudoku_alone==0);
 sudoku_score.sudoku.generate(seed);assert(solutions(sudoku_score.sudoku.given,&completed)==1);
 for(unsigned i=0;i<16;++i)sudoku_score.sudoku.fill(i,completed[i]);
 sudoku_score.credit_sudoku();assert(sudoku_score.sudoku_help==1 && sudoku_score.sudoku_alone==1);
 unsigned cases=0,solvable=0;
 for(uint8_t a=1;a<=9;++a)for(uint8_t b=a;b<=9;++b)for(uint8_t c=b;c<=9;++c)for(uint8_t d=c;d<=9;++d) {
  const std::array<uint8_t,4> cards{a,b,c,d};std::array<Move,3> path{};
  bool expected=oracle24(cards),got=solve(board(cards),path);assert(got==expected);++cases;
  if(got){auto active=board(cards);for(auto m:path)assert(active.apply(m));assert(active.won());++solvable;}
 }
 std::array<Move,3> path{};assert(solve(board({3,3,8,8}),path));
 auto b=board({1,1,2,3});assert(b.apply({0,1,Op::sub}));
 auto zero=b.count-1;assert(!b.apply({0,zero,Op::div}));assert(!b.apply({0,0,Op::add}));
 State state;state.sudoku=s;state.cards.original={3,3,8,8};
 for(auto m:path)assert(state.cards.apply(m));state.credit_cards();assert(state.cards_alone==1);
 state.cards.undo();assert(state.cards.apply(path[2]));state.credit_cards();assert(state.cards_alone==1);
 for(unsigned d=2;d<=12;++d)for(unsigned n=0;n<=d;++n)for(unsigned rd=2;rd<=12;++rd)for(unsigned rn=0;rn<=rd;++rn) {
  Fractions f;f.n=n;f.d=d;f.rn=rn;f.rd=rd;
  const int a=int(n*rd),b=int(rn*d),relation=a<b?-1:a>b?1:0;
  assert(f.relation()==relation && f.answer(relation) && f.score==1 && f.done==1);
  assert(!f.answer(relation) && f.done==1);
 }
 auto encoded=state.encode();assert(encoded.size()<512);State loaded;assert(loaded.decode(encoded));assert(loaded.encode()==encoded);
 auto prior=loaded.encode();assert(!loaded.decode(encoded+" bad"));assert(!loaded.decode(std::string(513,'1')));assert(!loaded.decode("2 1"));assert(loaded.encode()==prior);
 State invalid;invalid.fractions.answered=true;
 assert(!loaded.decode(invalid.encode()) && loaded.encode()==prior);
 invalid=State{};invalid.sudoku.hints=1;
 assert(!loaded.decode(invalid.encode()) && loaded.encode()==prior);
 for(unsigned i=0;i<encoded.size();++i) {auto truncated=encoded.substr(0,i);State check;assert(!check.decode(truncated));}
 for(unsigned trial=0;trial<64;++trial){State v;v.sudoku.generate(seed);v.cards.generate(seed);v.fractions.question(seed);auto data=v.encode();State parsed;assert(parsed.decode(data)&&parsed.encode()==data);}
 std::cout<<"PASS: 512 independently unique Sudoku puzzles; "<<cases<<" exhaustive 24-point multisets ("<<solvable<<" solvable); rational replay, zero division, duplicate credit, fraction grid and bounded persistence\n";
}
