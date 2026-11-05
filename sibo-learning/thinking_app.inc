// SPDX-License-Identifier: MIT
// Members of App. These pages reuse the bounded native UI and personal snapshot.
  thinking::State math;
  State<std::string> math_summary{""}, sudoku_status{""}, cards_status{""}, fraction_status{""};
  State<bool> fraction_review{false};
  std::array<Label,16> sudoku_labels;
  struct MathColor {State<uint32_t> value{0};};
  struct MathWidth {State<int32_t> value{0};};
  std::array<MathColor,16> sudoku_colors;
  std::array<Flag,16> sudoku_editable;
  std::array<Label,4> card_labels;
  std::array<Flag,4> card_shown;
  std::array<Label,2> fraction_labels;
  std::array<MathWidth,24> fraction_widths;
  std::array<Flag,24> fraction_cells, fraction_paint;
  unsigned sudoku_cell=0;
  int card_first=-1,card_second=-1;
  Screen math_reset_target=Screen::sudoku;
  void math_update() {
    math_summary.set("数独独立完成 "+std::to_string(math.sudoku_alone)+" · 提示完成 "+std::to_string(math.sudoku_help)+
      "\n24点独立完成 "+std::to_string(math.cards_alone)+" · 提示完成 "+std::to_string(math.cards_help));
    for(unsigned i=0;i<16;++i) {
      auto &s=math.sudoku;
      auto label=s.cells[i]?std::to_string(s.cells[i]):"·";
      sudoku_labels[i].value.set(i==sudoku_cell?"["+label+"]":label);
      sudoku_colors[i].value.set(s.conflict(i)?0xffc9bdff:i==sudoku_cell?0xb9e0d3ff:s.given[i]?mint:white);
      sudoku_editable[i].value.set(!s.given[i]);
    }
    sudoku_status.set(math.sudoku.complete()?"完成啦！每行、列和宫都不重复。":
      "选第 "+std::to_string(sudoku_cell/4+1)+" 行、第 "+std::to_string(sudoku_cell%4+1)+" 列，再填数字。");
    auto b=math.cards.current();
    for(unsigned i=0;i<4;++i) {
      card_shown[i].value.set(i<b.count);
      card_labels[i].value.set(i<b.count?std::string(int(i)==card_first?"① ":int(i)==card_second?"② ":"")+b.cards[i].value.text():"");
    }
    cards_status.set(b.won()?"得到24啦！"+b.cards[0].expression+" = 24":
      b.count==1?"结果是 "+b.cards[0].value.text()+"，可以撤销再试。":
      card_second>=0?"已选①和②：减法、除法按选择顺序。":card_first>=0?"再选一张卡。":"先选两张卡，再选运算。每张卡只用一次。");
    auto &f=math.fractions;
    fraction_review.set(f.answered);
    const bool compact=layout.height<260;
    fraction_status.set("第 "+std::to_string(std::min(5u,f.done+(f.answered?0:1)))+" / 5 题 · 答对 "+std::to_string(f.score)+
      (compact?"":f.answered?"\n已提交，可观察分数条和解析。":"\n两个整体一样大，哪边涂色更多？"));
    for(unsigned row=0;row<2;++row) {
      auto n=row?f.rn:f.n,d=row?f.rd:f.d;
      fraction_labels[row].value.set(std::string(row?"右边 ":"左边 ")+std::to_string(n)+" / "+std::to_string(d));
      auto bar_width=compact?(layout.content_width-8)/2:layout.content_width;
      auto usable=bar_width-(d-1)*2;
      for(unsigned i=0;i<12;++i){fraction_widths[row*12+i].value.set(int((i+1)*usable/d-i*usable/d));fraction_cells[row*12+i].value.set(i<d);fraction_paint[row*12+i].value.set(i<n);}
    }
  }
  void math_open(Screen value) {
    player.stop();cancel();message("");
    bool changed=false;
    if(value==Screen::sudoku && !math.sudoku.given[0] &&
        std::all_of(math.sudoku.given.begin(),math.sudoku.given.end(),[](auto n){return n==0;})) {
      math.sudoku.generate(math.seed);changed=true;
    }
    if(value==Screen::twentyfour && !math.cards.original[0]){math.cards.generate(math.seed);changed=true;}
    if(value==Screen::sudoku) {
      for(unsigned i=0;i<16;++i)if(!math.sudoku.given[i]){sudoku_cell=i;break;}
    }
    card_first=card_second=-1;math_update();show(value);if(changed)save();
  }
  void math_new(Screen value) {math_reset_target=value;show(Screen::math_reset);message("");}
  void math_reset() {
    if(math_reset_target==Screen::sudoku)math.sudoku.generate(math.seed);
    else math.cards.generate(math.seed);
    math_open(math_reset_target);save();
  }
  void sudoku_write(unsigned n,bool hint=false) {
    auto changed=hint?math.sudoku.hint(sudoku_cell):math.sudoku.fill(sudoku_cell,n);
    if(!changed){message("选择一个可填写的格子，再试一下。");return;}
    math.credit_sudoku();math_update();
    message(math.sudoku.conflict(sudoku_cell)?"这个数字在同行、同列或同宫重复。可清除后重试。":hint?"已填入一格提示；本局记为提示练习。":"");save();
  }
  void cards_select(unsigned index) {
    if(index>=math.cards.current().count)return;
    if(card_first==int(index)){card_first=-1;card_second=-1;}
    else if(card_first<0){card_first=int(index);card_second=-1;}
    else if(card_second==int(index))card_second=-1;
    else card_second=int(index);
    math_update();message("");
  }
  void cards_apply(thinking::Op op) {
    if(card_first<0||card_second<0){read("先选两张不同的卡，再做运算。", "选择两张卡",Screen::twentyfour);return;}
    if(!math.cards.apply({unsigned(card_first),unsigned(card_second),op})) {
      read("不能除以零，请换一种运算。", "换一种运算",Screen::twentyfour);return;
    }
    card_first=card_second=-1;math.credit_cards();math_update();message("");save();
    auto result=math.cards.current();
    if(result.won())read("得到24啦！\n"+result.cards[0].expression+" = 24", "完成24点",Screen::twentyfour);
  }
  void cards_hint() {
    auto b=math.cards.current();
    if(b.count==1){read(b.won()?"已经得到24啦！":"这一步没有得到24，可以撤销再试。", "本题提示",Screen::twentyfour);return;}
    std::array<thinking::Move,3> path{};
    if(!thinking::solve(b,path)){read("当前组合没有解，请撤销一步或重来。", "本题提示",Screen::twentyfour);return;}
    math.cards.hinted=true;const auto m=path[0];
    const char *sign[]={" + "," − "," × "," ÷ "};
    auto tip="试试 "+b.cards[m.a].value.text()+sign[unsigned(m.op)]+b.cards[m.b].value.text()+"；只提示这一步。本局会计为提示练习。";
    save();read(std::move(tip),"下一步提示",Screen::twentyfour);
  }
  void fraction_answer(int comparison) {
    auto &f=math.fractions;
    if(!f.answer(comparison)){message("这道题已经提交，点击下一题继续。");return;}
    math_update();save();fraction_explain(f.relation()==comparison?"答对了！":"再看一看：");
  }
  void fraction_explain(std::string prefix="") {
    const auto &f=math.fractions;
    if(!f.answered)return;
    const auto right=f.relation();
    auto explanation=prefix+(right<0?"左边小于右边。":right>0?"左边大于右边。":"两边一样多。")+
      "\n通分："+std::to_string(f.n*f.rd)+"/"+std::to_string(f.d*f.rd)+" 与 "+std::to_string(f.rn*f.d)+"/"+std::to_string(f.d*f.rd);
    read(std::move(explanation),"分数比较",Screen::fractions);
  }
  void fraction_next() {
    auto &f=math.fractions;
    if(!f.answered){message("先比较两边并提交答案。");return;}
    if(f.done==5){f.done=0;f.score=0;}
    f.question(math.seed);math_update();message("");save();
  }
