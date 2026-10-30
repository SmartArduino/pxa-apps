/* Simulation time drives every bonus; a paused game never consumes a bonus. */
(function (root) {
  'use strict';
  const W = 480, H = 854, FLOOR = 805;
  const TYPES = {
    plain: { points: 1, weight: 55 }, ice: { points: 2, weight: 12 },
    rich: { points: 2, weight: 12 }, double: { points: 2, weight: 12 }, coin: { points: 10, weight: 9 }
  };
  const clamp = (v, min, max) => Math.max(min, Math.min(max, v));
  class Engine {
    constructor({ random = Math.random, onEvent = () => {} } = {}) {
      this.random = random;
      this.onEvent = onEvent;
      this.reset();
      this.state.phase = 'menu';
    }
    reset() {
      this.state = {
        phase: 'playing', time: 0, score: 0, hearts: 3, combo: 0, maxCombo: 0,
        catches: 0, misses: 0, size: 2, spawned: 0, spawnIn: .7, items: [],
        player: { x: W / 2, target: W / 2, vx: 0, squash: 1, hurtUntil: 0 },
        buffs: { ice: 0, iceMultiplier: 1, rich: 0, double: 0, burst: 0 },
        laughUntil: 0, bestCleared: 0, keyDirection: 0
      };
      this.onEvent({ type: 'start' });
    }
    emit(type, data = {}) { this.onEvent({ type, ...data }); }
    size() { return clamp(this.state.size + (this.active('rich') ? 1 : 0), 0, 6); }
    active(key) { return this.state.buffs[key] > this.state.time; }
    multiplier() { return (this.state.combo >= 5 ? 4 : this.state.combo >= 3 ? 2 : 1) * (this.active('double') ? 2 : 1); }
    dimensions() { const n = this.size(); return { height: 132 + n * 8, width: 70 + n * 8, catchWidth: 68 + n * 10 }; }
    moveTo(x) { this.state.player.target = clamp(x, 0, W); }
    pause() { if (this.state.phase === 'playing') { this.state.phase = 'paused'; this.state.keyDirection = 0; } }
    resume() { if (this.state.phase === 'paused') this.state.phase = 'playing'; }
    spawn() {
      const s = this.state;
      let type = 'plain';
      if (s.spawned >= 3) {
        let pick = this.random() * 100;
        for (const [key, def] of Object.entries(TYPES)) { pick -= def.weight; if (pick <= 0) { type = key; break; } }
      }
      const speed = Math.min(150 + s.score * 3.6, 455) * (s.time < 15 ? .7 : 1) * (this.active('burst') ? .85 : 1);
      s.items.push({ type, x: 34 + this.random() * (W - 68), y: -32, vy: speed * (.93 + this.random() * .14),
        vx: this.random() < .22 ? (this.random() - .5) * 58 : 0,
        rotation: (this.random() - .5) * .3, spin: (this.random() - .5) * .9,
        safe: s.time < 15, radius: 23, age: 0 });
      s.spawned++;
    }
    catchItem(item) {
      const s = this.state;
      const points = TYPES[item.type].points * this.multiplier();
      s.score += points; s.catches++; s.combo++; s.maxCombo = Math.max(s.maxCombo, s.combo);
      s.player.squash = .84;
      if (item.type === 'ice') { s.buffs.ice = s.time + 5; s.buffs.iceMultiplier = 1.5; }
      if (item.type === 'double') s.buffs.double = s.time + 5;
      if (item.type === 'rich') s.buffs.rich = s.time + 5;
      if (item.type === 'coin') {
        if (s.hearts < 3) { s.hearts++; this.emit('heal', { x: item.x, y: item.y }); }
        else {
          s.buffs.iceMultiplier = this.active('ice') ? Math.max(s.buffs.iceMultiplier, 1.15) : 1.15;
          s.buffs.ice = Math.max(s.buffs.ice, s.time + 3);
          this.emit('fullHeart', { x: item.x, y: item.y });
        }
      }
      if (s.catches % 3 === 0 && s.size < 6) { s.size++; this.emit('grow'); }
      this.emit('catch', { item, points });
      if (s.combo === 3) this.emit('combo', { multiplier: 2 });
      if (s.combo === 5) this.burst();
    }
    burst() {
      const s = this.state;
      const cleared = s.items.splice(0);
      // The caught fifth item has already been removed; it is never counted twice.
      const points = cleared.reduce((total, item) => total + TYPES[item.type].points * this.multiplier(), 0);
      s.score += points; s.bestCleared = Math.max(s.bestCleared, cleared.length);
      s.buffs.burst = s.time + 4; s.laughUntil = s.time + 4.3;
      this.emit('burst', { cleared, points });
    }
    missItem(item) {
      const s = this.state;
      if (item.safe || s.time < 15) { this.emit('safeMiss', { item }); return; }
      s.combo = 0; s.buffs.rich = 0; s.hearts--; s.misses++;
      s.size = Math.max(0, s.size - 1); s.player.hurtUntil = s.time + .8;
      this.emit('miss', { item });
      if (s.hearts <= 0) { s.phase = 'gameover'; s.keyDirection = 0; this.emit('gameover'); }
    }
    update(dt) {
      const s = this.state;
      if (s.phase !== 'playing' || !Number.isFinite(dt) || dt <= 0) return;
      dt = Math.min(dt, .05); s.time += dt;
      const p = s.player, dim = this.dimensions();
      const speedBySize = [1.25, 1.15, 1, .92, .85, .78, .72][this.size()];
      const cap = 430 * speedBySize * (this.active('ice') ? s.buffs.iceMultiplier : 1) * (this.active('burst') ? 1.3 : 1);
      if (s.keyDirection) p.target = clamp(p.target + s.keyDirection * cap * dt, 0, W);
      const desired = clamp((p.target - p.x) * 11, -cap, cap);
      p.vx += clamp(desired - p.vx, -3500 * dt, 3500 * dt);
      p.x = clamp(p.x + p.vx * dt, dim.width / 2 + 7, W - dim.width / 2 - 7);
      p.squash += (1 - p.squash) * Math.min(1, dt * 12);
      s.spawnIn -= dt;
      if (s.spawnIn <= 0 && s.items.length < 12) {
        this.spawn(); s.spawnIn = Math.max(.39, 1.13 - s.score * .003) * (this.active('burst') ? 1.18 : 1);
      }
      // Snapshot permits a clear-screen event to remove every remaining item safely.
      for (const item of [...s.items]) {
        const index = s.items.indexOf(item);
        if (index < 0 || s.phase !== 'playing') continue;
        item.age += dt; item.rotation += item.spin * dt;
        const steps = Math.max(1, Math.ceil((Math.abs(item.vy) + Math.abs(item.vx)) * dt / 12));
        let caught = false;
        for (let i = 0; i < steps; i++) {
          item.x += item.vx * dt / steps; item.y += item.vy * dt / steps;
          if (item.x < 25 || item.x > W - 25) { item.vx *= -1; item.x = clamp(item.x, 25, W - 25); }
          const nearX = clamp(item.x, p.x - dim.catchWidth / 2, p.x + dim.catchWidth / 2);
          const nearY = clamp(item.y, 793 - dim.height * .87, 789);
          if ((item.x - nearX) ** 2 + (item.y - nearY) ** 2 <= (item.radius * .83) ** 2) { caught = true; break; }
        }
        if (caught) { s.items.splice(index, 1); this.catchItem(item); }
        else if (item.y + item.radius > FLOOR) { s.items.splice(index, 1); this.missItem(item); }
      }
    }
  }
  const api = { Engine, TYPES, W, H, FLOOR, clamp };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.NaiwaEngine = api;
})(typeof window !== 'undefined' ? window : globalThis);
