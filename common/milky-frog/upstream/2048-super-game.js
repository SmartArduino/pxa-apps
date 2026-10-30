/* 奶娃2048 —— 棋盘核心逻辑 */
(function (global) {
  'use strict';

  const { TIERS, TIER_BY_V, MAX_V, GOAL_V, SIZE, DIRS, unlockTier, setBest } = global.NL;

  const board = [];        // board[r][c] = tile | null
  let nextId = 1;
  let score = 0;
  let won = false;         // 是否已经合成过 2048
  let over = false;
  let undoSnapshot = null; // 一步撤销

  function emptyBoard() {
    const b = [];
    for (let r = 0; r < SIZE; r++) b.push(new Array(SIZE).fill(null));
    return b;
  }

  function makeTile(v, r, c) {
    return { id: nextId++, v, r, c, isNew: false, merged: false };
  }

  function reset() {
    board.length = 0;
    for (let r = 0; r < SIZE; r++) board.push(new Array(SIZE).fill(null));
    nextId = 1;
    score = 0;
    won = false;
    over = false;
    undoSnapshot = null;
    spawn();
    spawn();
  }

  /* 快照/恢复（撤销用） */
  function snapshot() {
    return {
      cells: board.map((row) => row.map((t) => (t ? { ...t } : null))),
      nextId, score, won, over
    };
  }
  function restore(s) {
    board.length = 0;
    s.cells.forEach((row) => board.push(row.map((t) => (t ? { ...t } : null))));
    nextId = s.nextId;
    score = s.score;
    won = s.won;
    over = s.over;
  }

  function emptyCells() {
    const out = [];
    for (let r = 0; r < SIZE; r++) for (let c = 0; c < SIZE; c++) if (!board[r][c]) out.push({ r, c });
    return out;
  }

  /* 新方块 90% 出 2，10% 出 4 */
  function spawn() {
    const cells = emptyCells();
    if (!cells.length) return null;
    const p = cells[Math.floor(Math.random() * cells.length)];
    const t = makeTile(Math.random() < 0.9 ? 2 : 4, p.r, p.c);
    t.isNew = true;
    board[p.r][p.c] = t;
    unlockTier(t.v);
    return t;
  }

  /* 每个方向上「看向哪一格」的步长：方块要往落点方向挤，所以看的也是落点那一侧 */
  const STEPS = {
    left:  { dr: 0,  dc: -1 },
    right: { dr: 0,  dc: 1 },
    up:    { dr: -1, dc: 0 },
    down:  { dr: 1,  dc: 0 }
  };

  /**
   * 遍历顺序：先按「非移动轴」分组，再沿移动轴从落点边缘往回扫。
   * 例：left = 第 0 行左→右、第 1 行左→右……
   * 这样每行最靠落点的那格先处理，后面挤过来的方块自然排在它后面。
   */
  function buildOrder(dir) {
    const cells = [];
    for (let a = 0; a < SIZE; a++) {
      for (let b = 0; b < SIZE; b++) {
        let r, c;
        if (dir === 'left') { r = a; c = b; }
        else if (dir === 'right') { r = a; c = SIZE - 1 - b; }
        else if (dir === 'up') { r = b; c = a; }
        else { r = SIZE - 1 - b; c = a; }   // down
        cells.push({ r, c });
      }
    }
    return cells;
  }

  const inside = (r, c) => r >= 0 && r < SIZE && c >= 0 && c < SIZE;

  function canMergeNeighbours() {
    for (let r = 0; r < SIZE; r++) {
      for (let c = 0; c < SIZE; c++) {
        const t = board[r][c];
        if (!t) continue;
        if (c + 1 < SIZE && board[r][c + 1] && board[r][c + 1].v === t.v) return true;
        if (r + 1 < SIZE && board[r + 1][c] && board[r + 1][c].v === t.v) return true;
      }
    }
    return false;
  }

  function isOver() {
    return emptyCells().length === 0 && !canMergeNeighbours();
  }

  function canMove() {
    return !isOver();
  }

  /**
   * 执行一次移动。
   * @returns {null | {moved:boolean, gained:number, merged:Array, spawned:object|null,
   *                   newTiers:number[], justWon:boolean}}
   * 返回 null 表示这个方向走不了（调用方据此忽略）。
   */
  function move(dir) {
    const step = STEPS[dir];
    if (!step) return null;
    if (isOver()) { over = true; return null; }

    // 先记下旧的撤销点：本次若走不动，不能把玩家上一步的撤销机会吃掉
    const prevSnapshot = undoSnapshot;
    undoSnapshot = snapshot();

    for (let r = 0; r < SIZE; r++) {
      for (let c = 0; c < SIZE; c++) if (board[r][c]) { board[r][c].isNew = false; board[r][c].merged = false; }
    }

    let moved = false;
    let gained = 0;
    const merged = [];
    const newTiers = [];

    for (const { r: r0, c: c0 } of buildOrder(dir)) {
      const t = board[r0][c0];
      if (!t) continue;

      // 先朝落点方向滑到不能再滑
      let nr = r0, nc = c0;
      let fr = r0 + step.dr, fc = c0 + step.dc;
      while (inside(fr, fc) && !board[fr][fc]) {
        nr = fr; nc = fc;
        fr += step.dr; fc += step.dc;
      }
      if (nr !== r0 || nc !== c0) moved = true;

      const ahead = inside(fr, fc) ? board[fr][fc] : null;
      // ahead.merged 是关键：本次移动里刚合出来的方块不能再被合一次
      if (ahead && ahead.v === t.v && !ahead.merged) {
        ahead.v = Math.min(ahead.v * 2, MAX_V);
        ahead.merged = true;
        board[r0][c0] = null;
        gained += ahead.v;
        merged.push(ahead);
        if (unlockTier(ahead.v)) newTiers.push(ahead.v);
        if (ahead.v === GOAL_V) won = true;
        moved = true;
      } else {
        board[r0][c0] = null;
        board[nr][nc] = t;
        t.r = nr; t.c = nc;
      }
    }

    if (!moved) {
      undoSnapshot = prevSnapshot;
      return null;
    }

    score += gained;
    setBest(score);
    const spawned = spawn();
    over = isOver();

    return {
      moved, gained, merged, spawned, newTiers,
      justWon: won && merged.some((t) => t.v === GOAL_V)
    };
  }

  function undo() {
    if (!undoSnapshot) return null;
    const prevScore = score;
    restore(undoSnapshot);
    undoSnapshot = null;
    return { score, prevScore };
  }

  global.NLGame = {
    board, reset, move, undo, spawn,
    // 暴露快照/恢复：撤销内部在用，测试与 AI 回溯也靠它
    _snapshot: snapshot,
    _restore: restore,
    canUndo: () => !!undoSnapshot,
    canMove,
    get score() { return score; },
    get won() { return won; },
    get over() { return over; },
    get bestTile() {
      let m = 0;
      for (let r = 0; r < SIZE; r++) for (let c = 0; c < SIZE; c++) if (board[r][c]) m = Math.max(m, board[r][c].v);
      return m;
    },
    liveTiles() {
      const out = [];
      for (let r = 0; r < SIZE; r++) for (let c = 0; c < SIZE; c++) if (board[r][c]) out.push(board[r][c]);
      return out;
    }
  };
})(globalThis);   // 同 data.js，便于 Node 直接 require 做逻辑自测
