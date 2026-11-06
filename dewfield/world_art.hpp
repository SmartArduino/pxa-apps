// SPDX-License-Identifier: MIT
#pragma once
// Included inside namespace art. All maps share the original compiled atlas.
inline const sprites::Sprite &villager_sprite(int id, int dir = 2) {
  const sprites::Sprite *people[4][4] = {
      {&sprites::villager_0_2, &sprites::villager_0_1, &sprites::villager_0_0,
       &sprites::villager_0_3},
      {&sprites::villager_1_2, &sprites::villager_1_1, &sprites::villager_1_0,
       &sprites::villager_1_3},
      {&sprites::villager_2_2, &sprites::villager_2_1, &sprites::villager_2_0,
       &sprites::villager_2_3},
      {&sprites::villager_3_2, &sprites::villager_3_1, &sprites::villager_3_0,
       &sprites::villager_3_3}};
  return *people[id % 4][dir & 3];
}
inline const sprites::Sprite &villager_walking_sprite(int id, int dir,
                                                      int frame) {
  if (!frame)
    return villager_sprite(id, dir);
  const sprites::Sprite *all[4][4][4] = {
      {{&sprites::villager_0_2_walk0, &sprites::villager_0_2_walk1,
        &sprites::villager_0_2_walk2, &sprites::villager_0_2_walk3},
       {&sprites::villager_0_1_walk0, &sprites::villager_0_1_walk1,
        &sprites::villager_0_1_walk2, &sprites::villager_0_1_walk3},
       {&sprites::villager_0_0_walk0, &sprites::villager_0_0_walk1,
        &sprites::villager_0_0_walk2, &sprites::villager_0_0_walk3},
       {&sprites::villager_0_3_walk0, &sprites::villager_0_3_walk1,
        &sprites::villager_0_3_walk2, &sprites::villager_0_3_walk3}},
      {{&sprites::villager_1_2_walk0, &sprites::villager_1_2_walk1,
        &sprites::villager_1_2_walk2, &sprites::villager_1_2_walk3},
       {&sprites::villager_1_1_walk0, &sprites::villager_1_1_walk1,
        &sprites::villager_1_1_walk2, &sprites::villager_1_1_walk3},
       {&sprites::villager_1_0_walk0, &sprites::villager_1_0_walk1,
        &sprites::villager_1_0_walk2, &sprites::villager_1_0_walk3},
       {&sprites::villager_1_3_walk0, &sprites::villager_1_3_walk1,
        &sprites::villager_1_3_walk2, &sprites::villager_1_3_walk3}},
      {{&sprites::villager_2_2_walk0, &sprites::villager_2_2_walk1,
        &sprites::villager_2_2_walk2, &sprites::villager_2_2_walk3},
       {&sprites::villager_2_1_walk0, &sprites::villager_2_1_walk1,
        &sprites::villager_2_1_walk2, &sprites::villager_2_1_walk3},
       {&sprites::villager_2_0_walk0, &sprites::villager_2_0_walk1,
        &sprites::villager_2_0_walk2, &sprites::villager_2_0_walk3},
       {&sprites::villager_2_3_walk0, &sprites::villager_2_3_walk1,
        &sprites::villager_2_3_walk2, &sprites::villager_2_3_walk3}},
      {{&sprites::villager_3_2_walk0, &sprites::villager_3_2_walk1,
        &sprites::villager_3_2_walk2, &sprites::villager_3_2_walk3},
       {&sprites::villager_3_1_walk0, &sprites::villager_3_1_walk1,
        &sprites::villager_3_1_walk2, &sprites::villager_3_1_walk3},
       {&sprites::villager_3_0_walk0, &sprites::villager_3_0_walk1,
        &sprites::villager_3_0_walk2, &sprites::villager_3_0_walk3},
       {&sprites::villager_3_3_walk0, &sprites::villager_3_3_walk1,
        &sprites::villager_3_3_walk2, &sprites::villager_3_3_walk3}},
  };
  return *all[id % 4][dir & 3][(frame - 1) & 3];
}
inline void world_bed(Raster &r, int x, int y) {
  r.rect(x - 17, y - 35, 34, 34, Ink);
  r.rect(x - 15, y - 34, 30, 32, rgb(166, 112, 61));
  r.rect(x - 13, y - 30, 26, 28, Cream);
  r.rect(x - 10, y - 29, 20, 7, rgb(240, 213, 161));
  r.rect(x - 13, y - 19, 26, 17, rgb(131, 150, 104));
  r.rect(x - 11, y - 18, 2, 15, rgb(163, 173, 118));
  r.rect(x - 16, y - 3, 3, 3, Ink);
  r.rect(x + 13, y - 3, 3, 3, Ink);
}
inline void world_table(Raster &r, int x, int y) {
  r.rect(x - 18, y - 18, 36, 15, Ink);
  r.rect(x - 16, y - 17, 32, 11, rgb(189, 137, 72));
  r.rect(x - 15, y - 16, 30, 2, Gold);
  r.rect(x - 14, y - 3, 3, 3, Ink);
  r.rect(x + 11, y - 3, 3, 3, Ink);
  r.rect(x - 4, y - 20, 8, 5, Cream);
  r.rect(x - 2, y - 19, 4, 2, rgb(112, 139, 88));
}
