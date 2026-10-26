#pragma once

#include <Arduboy2.h>
#include "debug_helper.h"
#include "bitmaps.h"

constexpr static uint8_t PLAYER_JUMP_LENGTH = 16;
constexpr static uint8_t JUMP_ANIM_LEN = 6;
constexpr static uint8_t DEATH_ANIM_LEN = 5;
constexpr static int8_t PLAYER_JUMP_Y[PLAYER_JUMP_LENGTH] = {
    -6, -5, -4, -3, -3, -2, -2, -1, 1, 2, 2, 3, 3, 4, 5, 6
};

struct Player {
  static constexpr uint8_t FALL_SPEED = 5;

  Player() = default;

  constexpr Player(int16_t x, int16_t y, uint8_t width, uint8_t height): 
      x(x), y(y), width(width), height(height), 
      is_jumping(false),
      is_dead(false),
      is_grounded(true),
      jump_idx(0),
      animation_idx(0) {}

  constexpr Player(const Rect& bounds): 
    x(bounds.x), y(bounds.y), width(bounds.width), height(bounds.height), 
    is_jumping(false),
    is_dead(false),
    is_grounded(true),
    jump_idx(0),
    animation_idx(0) {}

  constexpr Rect rect() {
    return Rect(
      this->x,
      this->y,
      this->width,
      this->height
    );
  }

  void draw(const Arduboy2& a) {
    if (is_dead)
      Sprites::drawSelfMasked(x-2, y-2, player_death_sprite, animation_idx);
    else
      Sprites::drawSelfMasked(x-2, y-2, player_sprite, animation_idx);
      
  }

  void start_jump() {
    if (is_grounded) {
      this->reset_jump();
      is_jumping = true;
      is_grounded = false;
    }
  }

  void ground() {
    if (is_grounded) return;
    is_grounded = true;
    is_jumping = false;
    this->reset_jump();
  }

  void update(bool do_update = true) {
    if (!do_update) return;
    
    if (this->is_jumping && jump_idx <= PLAYER_JUMP_LENGTH) {
      this->y += PLAYER_JUMP_Y[jump_idx++];
      animation_idx = (animation_idx + 1) % JUMP_ANIM_LEN;
    } else {
      is_grounded = false;
      y += FALL_SPEED;
      animation_idx = 0;
    }
  }

  void reset_jump() {
    animation_idx = 0;
    jump_idx = 0;
  }

  void dead() {
    is_dead = true;
    animation_idx = 0;
  }

  bool play_death_animation(bool update) {
    if(!update) return false;

    animation_idx = animation_idx + 1;
    if (animation_idx >= DEATH_ANIM_LEN) {
      return true;
      animation_idx = 0;
    }
    return false;
  }

  int16_t x;
  int16_t y;
  uint8_t width;
  uint8_t height;

  uint8_t jump_idx;
  uint8_t animation_idx;

  bool is_jumping;
  bool is_grounded;
  bool is_dead;
};