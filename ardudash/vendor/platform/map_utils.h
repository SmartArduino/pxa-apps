#pragma once

#include "obstacles.h"
#include "game_global_constants.h"
#include "map_segments.h"

void create_obstacle(
    Obstacle* o, 
    int16_t start_x, int16_t start_y, uint8_t w, uint8_t h, 
    uint8_t obstacle_id,
    uint8_t* created_cnt
){
    ObstacleType type_to_create = static_cast<ObstacleType>(obstacle_id);

    if (type_to_create == ObstacleType::UNKNOWN) return;
    o->_type = type_to_create;
    o->bounds.x = start_x * OBJECT_SIZE;
    o->bounds.y = start_y * OBJECT_SIZE;
    o->bounds.width = w * OBJECT_SIZE;
    o->bounds.height = h * OBJECT_SIZE;
    (*created_cnt)++;
}

int8_t parse_vertical_segment_map_to_obstacles(
    Obstacle obstacles[SEGMENT_W], 
    const uint8_t segment_map[SEGMENT_H][SEGMENT_W]
) {
    // int8_t start = 0;
    // int8_t height;
    // uint8_t created_cnt = 0;

    // int8_t obstacle_start_y = 0;
    int8_t obstacle_height = 0;
    uint8_t created_cnt = 0;

    uint8_t tile;
    uint8_t prev_tile;

    for (int8_t x = 0; x < SEGMENT_W; x++) {
        obstacle_height = 1;
        prev_tile = static_cast<uint8_t>(ObstacleType::UNKNOWN);
        
        for (int8_t y = SEGMENT_H - 1; y >= 0; y--) {
            tile = (uint8_t)pgm_read_byte(&segment_map[y][x]);

            if (prev_tile == tile) {
                obstacle_height++;
            } else {
                create_obstacle(
                    &obstacles[created_cnt], 
                    x, -y + SEGMENT_H - 1, 1, obstacle_height,
                    prev_tile,
                    &created_cnt
                );
                obstacle_height = 1;
            }

            prev_tile = tile;
        }

        // add last obstacle
        create_obstacle(
            &obstacles[created_cnt], 
            x, SEGMENT_H, 1, obstacle_height,
            prev_tile,
            &created_cnt
        );
    }

    return created_cnt;
}

int8_t parse_horizontal_segment_map_to_obstacles(
    Obstacle obstacles[SEGMENT_W], 
    const uint8_t segment_map[SEGMENT_H][SEGMENT_W]
){
    int8_t obstacle_start_x = 0;
    int8_t obstacle_width = 0;
    uint8_t created_cnt = 0;

    uint8_t tile;
    uint8_t prev_tile;

    for (int8_t y = SEGMENT_H - 1; y >= 0; y--) {
        obstacle_start_x = 0;
        obstacle_width = 0;
        prev_tile = static_cast<uint8_t>(ObstacleType::UNKNOWN);

        for(int8_t x = 0; x < SEGMENT_W; x++) {
            tile = (uint8_t)pgm_read_byte(&segment_map[y][x]);

            if (prev_tile == tile) {
                obstacle_width++;
            } else {
                create_obstacle(
                    &obstacles[created_cnt], 
                    obstacle_start_x, -y + SEGMENT_H, obstacle_width, 1, 
                    prev_tile, 
                    &created_cnt
                );
                obstacle_start_x = x;
                obstacle_width = 1;
            }

            prev_tile = tile;
        }
        // create last obstacle
        create_obstacle(
            &obstacles[created_cnt], 
            obstacle_start_x, -y + SEGMENT_H, obstacle_width, 1, 
            prev_tile, 
            &created_cnt
        );
    }

    return created_cnt;
}

int8_t parse_segment_to_obstacles(Obstacle obstacles[SEGMENT_W], int8_t segment_idx) {
    const Segment* seg = &SEGMENTS_METADATA[segment_idx];
    if (seg->load_orientation == SegmentLoadOrientation::HORIZONTAL){
        return parse_horizontal_segment_map_to_obstacles(obstacles, SEGMENT_MAPS[segment_idx]);
    } else {
        return parse_vertical_segment_map_to_obstacles(obstacles, SEGMENT_MAPS[segment_idx]);
    }
}

uint8_t next_segment(int8_t prev_segment_idx = -1) {
    if (prev_segment_idx == -1) {
        return 0;
    }
    uint8_t next_segment_idx = random(0, NEXT_LEVEL_OPTIONS_CNT);
    // return 1;
    return SEGMENT_LINKS[prev_segment_idx][next_segment_idx];
}