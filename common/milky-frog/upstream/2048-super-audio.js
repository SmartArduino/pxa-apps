/* 奶娃2048 —— 音频
 * 奶娃笑声（laugh-1~4）负责「合成」，冰块音（ice-1~3）负责「大事发生」，
 * andy-1 是解锁新奶娃的特殊音效。
 * 变调沿用原项目「体积 × 质量」的思路：块越大声音越低沉。 */
(function (global) {
  'use strict';

  const { save } = global.NL;

  const LAUGH = ['assets/laugh-1.mp3', 'assets/laugh-2.mp3', 'assets/laugh-3.mp3', 'assets/laugh-4.mp3'];
  const ICE = ['assets/ice-1.mp3', 'assets/ice-2.mp3', 'assets/ice-3.mp3'];
  const SPECIAL = 'assets/andy-1.mp3';

  const bgm = new Audio('assets/bgm.mp3');
  bgm.loop = true;
  bgm.preload = 'auto';

  let unlocked = false;
  let muted = false;

  function make(src) {
    const a = new Audio(src);
    a.preload = 'auto';
    return a;
  }
  const laugh = LAUGH.map(make);
  const ice = ICE.map(make);
  const special = make(SPECIAL);
  const all = [...laugh, ...ice, special];

  function applyVolumes() {
    const s = save.settings;
    bgm.volume = muted ? 0 : s.bgm / 100;
    laugh.forEach((a) => { a.volume = muted ? 0 : s.sfx / 100; });
    ice.forEach((a) => { a.volume = muted ? 0 : s.sfx / 100; });
    special.volume = muted ? 0 : s.sfx / 100;
  }

  /** 播放一个音效；gain 用来让同一个素材在不同场合有轻重之分 */
  function play(a, rate, gain) {
    if (!unlocked || muted) return;
    try {
      a.pause();
      a.currentTime = 0;
      a.playbackRate = rate;
      a.volume = Math.min(1, save.settings.sfx / 100 * (gain == null ? 1 : gain));
      a.play().catch(() => { /* 浏览器自动播放策略拦截时静默忽略 */ });
    } catch (e) { /* 忽略 */ }
  }

  /** 合成音效：按数量级切换奶娃笑声，越大越低沉 */
  function playMerge(v) {
    if (v <= 8) { play(laugh[0], 1.75); return; }
    if (v <= 32) { play(laugh[1], 1.35); return; }
    if (v <= 128) { play(laugh[2], 1.0); return; }
    if (v <= 512) { play(laugh[3], 0.85); return; }
    // 大块合成改用冰块音，听起来像「咔嚓」冻住了
    play(ice[v >= 2048 ? 2 : 1], v >= 2048 ? 0.8 : 1.0);
  }

  /** 新方块冒出来：一声很轻的冰块碰撞 */
  function playSpawn() { play(ice[0], 1.5, 0.28); }

  /** 走不动了：轻轻磕一下 */
  function playBlocked() { play(ice[0], 1.9, 0.2); }

  /** 解锁新奶娃：奶娃出场 */
  function playUnlock() { play(special, 1); }

  /** 安迪音效：沿用原作「每局保证播一次」的语义，每局第一次合成时响 */
  function playAndy() { play(special, 1); }

  /** 重开 / 撤销 */
  function playRestart() { play(laugh[1], 1.15, 0.8); }

  function playLose() { play(laugh[3], 0.45, 1); }

  function playWin() {
    play(ice[2], 0.9, 1);
    setTimeout(() => play(laugh[0], 1.0, 1), 130);
    setTimeout(() => play(special, 1, 1), 300);
  }

  function startBgm() {
    // 音量为 0 时干脆不播，省得白解码
    if (!unlocked || muted || save.settings.bgm <= 0) return;
    if (bgm.paused) bgm.play().catch(() => { /* 忽略 */ });
  }
  function pauseBgm() { bgm.pause(); }
  function toggleMute(force) {
    muted = force != null ? force : !muted;
    applyVolumes();
    if (muted) pauseBgm(); else startBgm();
    return muted;
  }
  const isMuted = () => muted;

  /* 浏览器要求首次交互后才能出声 */
  function unlock() {
    if (unlocked) return;
    unlocked = true;
    applyVolumes();
    startBgm();
  }

  function setVolume(kind, v) {
    global.NL.setSetting(kind, v);
    applyVolumes();
    if (kind === 'bgm') { if (v <= 0) pauseBgm(); else startBgm(); }
  }

  applyVolumes();

  global.NLAudio = {
    unlock, startBgm, pauseBgm, toggleMute, isMuted, setVolume,
    playMerge, playSpawn, playBlocked, playUnlock, playAndy, playRestart, playLose, playWin
  };
})(window);
