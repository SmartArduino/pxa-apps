// SPDX-License-Identifier: MIT
// Included after physics and player damage helpers in core.cpp.
void flying_enemy_ai(State &s, Actor &m, uint32_t ms, float dt) {
  // One 704 ms wingbeat cycle, sampled without a transcendental call per tick.
  static constexpr int8_t wave[64] = {
      0,2,4,6,8,10,12,14,16,17,18,19,20,21,22,22,
      22,22,22,21,20,19,18,17,16,14,12,10,8,6,4,2,
      0,-2,-4,-6,-8,-10,-12,-14,-16,-17,-18,-19,-20,-21,-22,-22,
      -22,-22,-22,-21,-20,-19,-18,-17,-16,-14,-12,-10,-8,-6,-4,-2};
  const float dx = s.player.x - m.x, bob = wave[(m.anim / 11) & 63];
  const int direction = dx > 0 ? 1 : -1;
  m.face = dx > 0;
  if (m.type == LanternMoth && s.clock >= 25000 && s.clock <= 240000 &&
      ix(m.y) <= s.heights[std::clamp(ix(m.x), 0, s.width - 1)] + 8) {
    m.vx = float(-direction * 48);
    m.vy = -72;
    m.face = m.vx > 0;
    if (m.y <= enemies[LanternMoth].height + 1) m.hp = 0;
  } else if (m.type == LanternMoth) {
    if (m.timer >= 2400) m.timer %= 2400;
    if (m.timer < 1300) {
      m.vx = float(direction * 30);
      m.vy = std::clamp((s.player.y - 32 - m.y) * .8f + bob, -65.0f, 65.0f);
    } else if (m.timer < 1600) {
      m.vx *= .8f;
      m.vy *= .8f;
    } else if (m.timer - ms < 1600) {
      auto aim = starsoil_aim::unit(dx, s.player.y - 12 - (m.y - 9), m.face);
      m.vx = aim.x * 125;
      m.vy = aim.y * 125;
    }
  } else if (m.type == ThornHornet) {
    m.vx = std::clamp((std::fabs(dx) - 110) * direction * .8f, -38.0f, 38.0f);
    m.vy = std::clamp((s.player.y - 36 - m.y) * .9f + bob, -65.0f, 65.0f);
    if (m.timer >= 1500) m.vx *= .25f;
    if (m.timer >= 1800) {
      m.timer = 0;
      if (std::fabs(dx) < 240 && std::fabs(m.y - s.player.y) < 160 &&
          target_ray(s, ix(m.x), ix(m.y - 12), m.x, m.y - 12)) {
        for (auto &shot : s.hostile_shots) {
          if (shot.life) continue;
          auto aim = starsoil_aim::unit(dx, s.player.y - m.y, m.face);
          shot = {m.x, m.y - 12, aim.x * 120, aim.y * 120, 2200, 9, 1};
          break;
        }
      }
    }
  } else {
    m.vx = float(direction * 38);
    m.vy = std::clamp((s.player.y - 16 - m.y) * .9f + bob, -90.0f, 90.0f);
  }
  flying_physics(s, m, dt);
}

void ground_enemy_ai(State &s, Actor &m, float dt) {
  const float dx = s.player.x - m.x;
  const int direction = dx > 0 ? 1 : -1;
  if (m.type == DuneBeetle) {
    if (m.timer >= 1900) m.timer %= 1900;
    if (m.timer < 1250) m.face = dx > 0;
    m.vx = m.timer < 1000 ? float(direction * 22)
           : m.timer < 1250 ? 0.0f : (m.face ? 112.0f : -112.0f);
    // A short jump clears one-block steps, while a wall stops the charge.
    if (m.grounded && m.vx &&
        !body_clear(s, m.x + (m.vx > 0 ? 3 : -3), m.y, 20, 14)) m.vy = -135;
  } else {
    m.face = dx > 0;
    if (m.type == Mossling && m.grounded && m.timer > 900) {
      int x = ix(m.x + direction * 9), y = ix(m.y - 12);
      if (closed_door(tile(s, x, y)) && toggle_door(s, x, y, true)) {
        m.timer = 0;
        m.vx = float(direction * 32);
        notice(s, "苔木精推开了木门。");
      }
    }
    const int period = m.type == Warden ? 440 : m.type == FrostSlime ? 1100 : 900;
    if (m.grounded && m.timer > period) {
      m.vy = m.type == Warden ? -240 : m.type == FrostSlime ? -220 : -145;
      m.vx = float(direction * (m.type == Warden ? 85 : m.type == FrostSlime ? 24 : 32));
      m.timer = 0;
    }
  }
  physics(s, m, dt, float(mob_width(m.type)), float(mob_height(m.type)));
}

void update_enemies(State &s, uint32_t ms, float dt) {
  for (auto &m : s.mobs) {
    if (m.hp <= 0) continue;
    if (m.type <= NoEnemy || m.type >= EnemyCount ||
        (m.type != Warden && (std::fabs(m.x - s.player.x) > 600 ||
                             std::fabs(m.y - s.player.y) > 480))) {
      m.hp = 0;
      continue;
    }
    m.timer = uint16_t(std::min(65535u, unsigned(m.timer) + ms));
    m.anim += ms;
    if (flying_enemy(m.type)) flying_enemy_ai(s, m, ms, dt);
    else ground_enemy_ai(s, m, dt);
    const auto &def = enemies[m.type];
    if (m.hp > 0 && std::fabs(m.x - s.player.x) < (def.width + 10) * .5f &&
        m.y > s.player.y - 24 && m.y - def.height < s.player.y)
      hurt(s, def.damage);
  }
  for (auto &shot : s.hostile_shots) {
    if (!shot.life) continue;
    shot.life = ms >= shot.life ? 0 : shot.life - ms;
    const float dx = shot.vx * dt, dy = shot.vy * dt;
    const int steps = std::max(1, int(std::ceil(std::max(std::fabs(dx), std::fabs(dy)) / 3)));
    for (int i = 0; i < steps && shot.life; ++i) {
      shot.x += dx / steps;
      shot.y += dy / steps;
      if (solid(tile(s, ix(shot.x), ix(shot.y)))) {
        shot.life = 0;
      } else if (std::fabs(shot.x - s.player.x) < 6 &&
                 shot.y > s.player.y - 24 && shot.y < s.player.y) {
        hurt(s, shot.damage);
        shot.life = 0;
      }
    }
  }
}
