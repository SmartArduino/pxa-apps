// SPDX-License-Identifier: MIT
// Render within ScreenView::render; native widgets keep touch and key focus.
  if(kind==Screen::thinking) {
    auto f=stage(kind),b=p.scroll(p.body(f));
    p.text(b,"离线动手玩",Font::title,green);
    p.text(b,"不联网、不下载题库。进度自动保存，提示练习单独统计。",Font::caption,muted);
    p.button(b,"4×4 数独",[&a]{a.math_open(Screen::sudoku);},true);
    p.button(b,"24点挑战",[&a]{a.math_open(Screen::twentyfour);});
    p.button(b,"分数实验室",[&a]{a.math_open(Screen::fractions);});
    p.text(b,a.math_summary,Font::caption,muted);notice(b);
    auto title=std::string_view("数学动手玩");header(f,title);
  }
  if(kind==Screen::math_reset) {
    auto f=stage(kind),b=p.scroll(p.body(f));
    p.text(b,"换题会替换当前练习进度，已完成统计仍保留。",Font::body,ink);
    p.button(b,"继续原题",[&a]{a.math_open(a.math_reset_target);},true);
    p.button(b,"确认换题",[&a]{a.math_reset();});
    auto title=std::string_view("换一道题");header(f,title);
  }
  if(kind==Screen::sudoku) {
    auto f=stage(kind),b=p.scroll(p.body(f));
    const bool compact=a.layout.height<220;
    if(!compact)p.text(b,"每行、每列、每个2×2宫填1–4，各用一次。浅绿格是题目。",Font::caption,muted);
    auto grid=p.box(b,false,7);
    auto band=grid;
    for(unsigned r=0;r<4;++r) {
      if(r%2==0)band=p.box(grid,false,3);
      auto row=p.box(band,true,7);
      for(unsigned group=0;group<2;++group) {
        auto pair=p.box(row,true,3);
        p.ok &= p.tx.logical_px(pair,protocol::width,int((a.layout.content_width-7)/2));
        for(unsigned c=0;c<2;++c) {
          auto i=r*4+group*2+c;
          auto n=p.button(pair,a.sudoku_labels[i].value,[&a,i]{a.sudoku_cell=i;a.math_update();a.message("");});
          p.ok &= p.tx.logical_px(n,protocol::width,int((a.layout.content_width-13)/4)) &&
            p.tx.logical_px(n,protocol::height,compact?24:34) && p.tx.template logical_px<protocol::min_height>(n,compact?24:34);
          p.padding(n,1,1,1,1);
          p.enabled(n,a.sudoku_editable[i].value);
          p.ok &= math_background(p.tx,n,a.sudoku_colors[i].value.get()) &&
            page.template bind<uint32_t,math_background>(a.sudoku_colors[i].value,n);
        }
      }
    }
    p.text(b,a.sudoku_status,Font::caption,green);
    auto numbers=p.box(b,true,4);
    for(unsigned i=1;i<=4;++i) {
      auto label=std::to_string(i);auto n=p.button(numbers,label,[&a,i]{a.sudoku_write(i);},true);
      p.ok &= p.tx.logical_px(n,protocol::width,int((a.layout.content_width-12)/4));p.padding(n,2,6,2,6);
    }
    p.button(b,"清除这一格",[&a]{a.sudoku_write(0);});
    p.button(b,"提示这一格",[&a]{a.sudoku_write(0,true);});
    p.button(b,"换一道数独",[&a]{a.math_new(Screen::sudoku);});notice(b);
    if(compact)p.button(b,"数独玩法",[&a]{a.read("每行、每列、每个2×2宫填1–4，各用一次。浅绿格是题目。先选一个空格，再选数字。", "数独玩法",Screen::sudoku);});
    auto title=std::string_view("4×4 数独");header(f,title);
  }
  if(kind==Screen::twentyfour) {
    auto f=stage(kind),b=p.scroll(p.body(f));
    p.text(b,a.cards_status,Font::body,green);
    for(unsigned i=0;i<4;++i) {
      auto n=p.button(b,a.card_labels[i].value,[&a,i]{a.cards_select(i);});p.visible(n,a.card_shown[i].value);
    }
    auto ops=p.box(b,true,4);
    const char *symbols[]={"+","−","×","÷"};
    for(unsigned i=0;i<4;++i) {
      auto label=std::string_view(symbols[i]);auto n=p.button(ops,label,[&a,i]{a.cards_apply(thinking::Op(i));},true);
      p.ok &= p.tx.logical_px(n,protocol::width,int((a.layout.content_width-12)/4));p.padding(n,2,6,2,6);
    }
    p.button(b,"交换①②",[&a]{if(a.card_first>=0&&a.card_second>=0){std::swap(a.card_first,a.card_second);a.math_update();}else a.read("先选择两张不同的卡，再交换运算顺序。","选择两张卡",Screen::twentyfour);});
    p.button(b,"撤销一步",[&a]{if(a.math.cards.undo()){a.card_first=a.card_second=-1;a.math_update();a.message("");a.save();}});
    p.button(b,"提示下一步",[&a]{a.cards_hint();});
    p.button(b,"本题重来",[&a]{a.math.cards.depth=0;a.card_first=a.card_second=-1;a.math_update();a.message("本题提示和完成标记保留。");a.save();});
    p.button(b,"换一道24点",[&a]{a.math_new(Screen::twentyfour);});notice(b);
    auto title=std::string_view("24点挑战");header(f,title);
  }
  if(kind==Screen::fractions) {
    auto f=stage(kind),b=p.scroll(p.body(f));
    const bool compact=a.layout.height<260;
    p.text(b,a.fraction_status,Font::caption,green);
    auto comparisons=compact?p.box(b,true,8):b;
    for(unsigned row=0;row<2;++row) {
      auto column=compact?p.box(comparisons,false,3):b;
      if(compact)p.ok &= p.tx.logical_px(column,protocol::width,int((a.layout.content_width-8)/2));
      p.text(column,a.fraction_labels[row].value,compact?Font::caption:Font::title,ink);
      auto bar=p.box(column,true,2);
      for(unsigned i=0;i<12;++i) {
        auto n=p.box(bar);p.visible(n,a.fraction_cells[row*12+i].value);
        p.ok &= p.tx.logical_px(n,protocol::height,compact?14:24) &&
          pixel_width(p.tx,n,a.fraction_widths[row*12+i].value.get()) &&
          math_fraction_color(p.tx,n,a.fraction_paint[row*12+i].value.get()) &&
          page.template bind<int32_t,pixel_width>(a.fraction_widths[row*12+i].value,n) &&
          page.template bind<bool,math_fraction_color>(a.fraction_paint[row*12+i].value,n);
      }
    }
    p.button(b,"左边更少 <",[&a]{a.fraction_answer(-1);},true);
    p.button(b,"两边一样 =",[&a]{a.fraction_answer(0);});
    p.button(b,"左边更多 >",[&a]{a.fraction_answer(1);});
    auto review=p.button(b,"再看解析",[&a]{a.fraction_explain();});p.enabled(review,a.fraction_review);
    p.button(b,"下一题 / 新一轮",[&a]{a.fraction_next();});notice(b);
    auto title=std::string_view("分数实验室");header(f,title);
  }
