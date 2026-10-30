/* 奶娃2048 —— 常量 / 存档 / DOM 工具 */
(function (global) {
  'use strict';

  // 数值阶梯：数值 -> 奶娃造型、配色
  const TIERS = [
    { v: 2,    name: '迷你奶娃',   img: 'assets/tiles/tile-2.webp',    bg: '#ffeeb0', fg: '#7a4a21' },
    { v: 4,    name: '奶油奶娃',   img: 'assets/tiles/tile-4.webp',    bg: '#ffe08a', fg: '#7a4a21' },
    { v: 8,    name: '微笑奶娃',   img: 'assets/tiles/tile-8.webp',    bg: '#ffd75e', fg: '#7a4a21' },
    { v: 16,   name: '苹果奶娃',   img: 'assets/tiles/tile-16.webp',   bg: '#ffc94d', fg: '#7a4a21' },
    { v: 32,   name: '奶茶奶娃',   img: 'assets/tiles/tile-32.webp',   bg: '#ffb93b', fg: '#7a4a21' },
    { v: 64,   name: '白大褂奶娃', img: 'assets/tiles/tile-64.webp',   bg: '#ffa62e', fg: '#7a4a21' },
    { v: 128,  name: '厨师奶娃',   img: 'assets/tiles/tile-128.webp',  bg: '#ff9422', fg: '#fff3d0' },
    { v: 256,  name: '奶友人奶娃', img: 'assets/tiles/tile-256.webp',  bg: '#f97f1f', fg: '#fff3d0' },
    { v: 512,  name: '朋克奶娃',   img: 'assets/tiles/tile-512.webp',  bg: '#ef6a1c', fg: '#fff3d0' },
    { v: 1024, name: '绅士奶娃',   img: 'assets/tiles/tile-1024.webp', bg: '#e2541a', fg: '#fff3d0' },
    { v: 2048, name: '女王奶娃',   img: 'assets/tiles/tile-2048.webp', bg: '#c93a17', fg: '#ffe9a8', royal: true },
    { v: 4096, name: '公主奶娃',   img: 'assets/tiles/tile-4096.webp', bg: '#8b46d6', fg: '#ffe9a8', royal: true }
  ];

  const TIER_BY_V = new Map(TIERS.map((t) => [t.v, t]));
  const MAX_V = TIERS[TIERS.length - 1].v;
  const GOAL_V = 2048;

  const SIZE = 4;
  const DIRS = {
    left:  { x: -1, y: 0 },
    right: { x: 1,  y: 0 },
    up:    { x: 0, y: -1 },
    down:  { x: 0, y: 1 }
  };

  /* ---------- 存档 ---------- */
  const SAVE_KEY = 'nailong2048.save.v2';
  const DEFAULT_SAVE = {
    best: 0,
    unlocked: [2, 4],
    // 背景音乐默认关闭：这个 BGM 循环很吵，玩家想听自己在设置里开
    settings: { bgm: 0, sfx: 100 }
  };

  function loadSave() {
    try {
      const raw = localStorage.getItem(SAVE_KEY);
      if (!raw) return structuredClone(DEFAULT_SAVE);
      const s = JSON.parse(raw);
      return {
        best: Number(s.best) || 0,
        unlocked: Array.isArray(s.unlocked) && s.unlocked.length ? s.unlocked : [2, 4],
        settings: {
          bgm: clampNum(s.settings?.bgm, 0, 100, 0),
          sfx: clampNum(s.settings?.sfx, 0, 100, 100)
        }
      };
    } catch (e) {
      return structuredClone(DEFAULT_SAVE);
    }
  }

  let save = loadSave();

  function persist() {
    try { localStorage.setItem(SAVE_KEY, JSON.stringify(save)); } catch (e) { /* 隐私模式下忽略 */ }
  }

  function clampNum(v, lo, hi, dflt) {
    const n = Number(v);
    return Number.isFinite(n) ? Math.min(hi, Math.max(lo, n)) : dflt;
  }

  function unlockTier(v) {
    if (save.unlocked.includes(v)) return false;
    save.unlocked.push(v);
    persist();
    return true;
  }

  function setBest(n) {
    if (n > save.best) { save.best = n; persist(); return true; }
    return false;
  }

  function setSetting(k, v) {
    save.settings[k] = clampNum(v, 0, 100, save.settings[k]);
    persist();
  }

  function clearSave() {
    save = structuredClone(DEFAULT_SAVE);
    try { localStorage.removeItem(SAVE_KEY); } catch (e) { /* 忽略 */ }
  }

  /* ---------- DOM 工具 ---------- */
  const $ = (sel, root) => (root || document).querySelector(sel);
  const $$ = (sel, root) => Array.from((root || document).querySelectorAll(sel));

  function el(tag, cls, text) {
    const n = document.createElement(tag);
    if (cls) n.className = cls;
    if (text != null) n.textContent = text;
    return n;
  }

  function show(node) { node.classList.remove('hidden'); }
  function hide(node) { node.classList.add('hidden'); }

  /* 数字滚动动画：让分数变化看得见 */
  function bump(node) {
    node.classList.remove('bump');
    void node.offsetWidth;   // 强制回流以重启动画
    node.classList.add('bump');
  }

  global.NL = {
    TIERS, TIER_BY_V, MAX_V, GOAL_V, SIZE, DIRS,
    get save() { return save; },
    persist, unlockTier, setBest, setSetting, clearSave,
    $, $$, el, show, hide, bump
  };
})(globalThis);   // 用 globalThis 而非 window，方便在 Node 里直接 require 做逻辑自测
