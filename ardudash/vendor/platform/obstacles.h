#pragma once

#include <Arduboy2.h>

#include "bitmaps.h"
#include "game_global_constants.h"
#include "player.h"
#include "rects_collision.h"

enum class ObstacleType : uint8_t {
  UNKNOWN = 0,  // special technical obstacle for parsing
  PLATFORM = 1,
  BLOCK = 2,
  SAW = 3,
  COUNT = 4,
  FLOOR = 5,    // special technical obstacle
};

struct Obstacle {
  static constexpr uint8_t MOVE_SPEED = 3;
  static constexpr uint8_t SAW_ANIM_LEN = 3;

  Obstacle() {
    this->bounds = Rect(0, 0, 0, 0);
    this->_type = ObstacleType::BLOCK;
    this->enabled = false;
    this->anim_idx = 0; // only for saw at the moment
  }

  Obstacle(
      const Rect& bounds,
      ObstacleType _type,
      int16_t init_offset = 0,
      bool enabled = false) : bounds(bounds),
                              _type(_type),
                              enabled(enabled) {
    this->set_x_offset(init_offset);
    this->anim_idx = 0;
  }

  Obstacle(
      int16_t x, int16_t y, uint8_t width, uint8_t height,
      ObstacleType _type,
      int16_t init_offset = 0,
      bool enabled = false) : Obstacle(Rect(x, y, width, height), _type, init_offset, enabled) {}

  void update(bool do_update = true, bool update_anim = false) {
    if(!do_update) return;
    
    bounds.x -= MOVE_SPEED;
    if (update_anim) anim_idx = (anim_idx + 1) % SAW_ANIM_LEN;
  }

  void set_x_offset(int16_t x_offset = 0) {
    bounds.x += x_offset;
  }

  static void block_interact(Player& player, const Rect& bounds, const Arduboy2& a) {
    uint8_t w = 0, h = 0;

    switch (collide_rects(player.rect(), bounds)) {
      case CollisionType::TOP:
      case CollisionType::TOP_LEFT:
        w = player.width;
        h = bounds.y + bounds.height - player.y;
        player.y = bounds.y + bounds.height;
        break;
      case CollisionType::BOTTOM:
      case CollisionType::BOTTOM_LEFT:
        player.ground();
        player.y = bounds.y - player.height;
        break;
      case CollisionType::LEFT:
        h = player.height;
        w = bounds.x + bounds.width - player.x;

        player.x = bounds.x + bounds.width;
        break;
      case CollisionType::RIGHT:
        player.dead();
        // h = player.height;
        // w = player.x + player.width - bounds.x;
        // player.x = bounds.x - player.width;

        // DO_DEBUG(
        //   a.setCursor(0, 8);
        //   a.print("R\n");
        //   a.print(w);
        //   a.print(" ");
        //   a.print(h);
        // );
        break;
      case CollisionType::TOP_RIGHT:
        w = player.x + player.width - bounds.x;
        h = bounds.y + bounds.height - player.y;
        if (h > player.height / 3) {
          player.dead();
          // player.x = bounds.x - player.width;
          
          DO_DEBUG(
            a.setCursor(0, 8);
            a.print("TR\n");
            a.print(w);
            a.print(" ");
            a.print(h);
          );
        }
        break;
      case CollisionType::BOTTOM_RIGHT:
        w = player.x + player.width - bounds.x;
        h = player.y + player.height - bounds.y;
        if (h > player.height / 3) {
          if(w >= player.width / 2) {
            player.y = bounds.y - player.height;
          } else {
            player.dead();
          }
        
          // player.x = bounds.x - player.width;
          
          // DO_DEBUG(
          //   a.setCursor(0, 8);
          //   a.print("BR\n");
          //   a.print(w);
          //   a.print(" ");
          //   a.print(h);
          // );
        }
        break;
      default:
        break;
    }
  }

  static void floor_interact(Player& player, const Rect& bounds, const Arduboy2& a) {
    if (player.y + player.height >= bounds.y) {
      player.ground();
      player.y = bounds.y - player.height;
    }
  }

  void interact(Player& player, const Arduboy2& a) {
    switch (this->_type) {
      case ObstacleType::BLOCK:
        Obstacle::block_interact(player, bounds, a);
        break;
      case ObstacleType::PLATFORM:
        // TODO: works a bit funny if the blayer collides with platform upfront, 
        // but as long as platform are high in the air (1 block above the bottom block), works fine
        Obstacle::block_interact(player, Rect(bounds.x, bounds.y, bounds.width, 7), a);
        break;
      case ObstacleType::SAW:
        // TODO: bugged as well on long obstacles of type SAW, player seems to be just passing trough
        // until the end
        // probably need better collision model
        if (Arduboy2::collide(player.rect(), Rect(bounds.x+4, bounds.y+8, bounds.width - 8, bounds.height - 8))) {
          player.dead();
        }
        // Obstacle::block_interact(player, Rect(bounds.x+3, bounds.y+3, 10, 13), a);
        break;
      case ObstacleType::FLOOR:
        Obstacle::floor_interact(player, bounds, a);
        break;
      default:
        break;
    }
  }

  void to_world_y(int16_t y_offset) {
    this->bounds.y = y_offset - this->bounds.y;
  }

  void draw(const Arduboy2& a) {
    switch (this->_type) {
      case ObstacleType::FLOOR:
        a.drawFastHLine(bounds.x, bounds.y, bounds.width, WHITE);
        break;
      case ObstacleType::SAW:
        for (int16_t y = bounds.y; y < bounds.y + bounds.height; y += OBJECT_SIZE) {
          for (int16_t x = bounds.x; x < bounds.x + bounds.width; x += OBJECT_SIZE) {
            Sprites::drawSelfMasked(x, y, obstacles_sprites, anim_idx + 2);
          }
        }
        break;
      default:
        for (int16_t y = bounds.y; y < bounds.y + bounds.height; y += OBJECT_SIZE) {
          for (int16_t x = bounds.x; x < bounds.x + bounds.width; x += OBJECT_SIZE) {
            Sprites::drawSelfMasked(x, y, obstacles_sprites, static_cast<uint8_t>(_type)-1);
          }
        }
        break;
    }
  }

  Rect bounds;
  ObstacleType _type;
  uint8_t anim_idx = 0;
  bool enabled;
};

struct BlockObstacle : Obstacle {
  BlockObstacle(int16_t x, int16_t y, uint8_t width, uint8_t height, uint8_t init_offset = 0) : Obstacle(x, y, width, height, ObstacleType::BLOCK, init_offset) {}
};