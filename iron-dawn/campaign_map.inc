// SPDX-License-Identifier: MIT
// Map coordinates increase east (x) and south (y).
 struct Deployment { Kind kind; Vec position; };

 void deploy(int team, std::initializer_list<Deployment> roster) {
  for (auto [kind, p] : roster) add(kind, team, p);
 }

 void patch(Terrain type, int x, int y, int width, int height) {
  for (int yy = y; yy < y + height; ++yy)
   for (int xx = x; xx < x + width; ++xx)
    terrain[yy * N + xx] = type;
 }

 void landform(Terrain type, int x, int y, int rx, int ry) {
  for (int yy = std::max(1, y - ry); yy <= std::min(side - 2, y + ry); ++yy) {
   for (int xx = std::max(1, x - rx); xx <= std::min(side - 2, x + rx); ++xx) {
    int dx = xx - x, dy = yy - y;
    if (dx * dx * ry * ry + dy * dy * rx * rx > rx * rx * ry * ry) continue;
    // Shelling leaves scattered impact marks within the damaged area.
    if (type == crater && unsigned(xx * 97 + yy * yy * 59 + xx * yy * 31) % 11 < 7) continue;
    terrain[yy * N + xx] = type;
   }
  }
 }

 // Waypoints make an orthogonal road. Only an intentional crossing can span water.
 void route(std::initializer_list<Vec> points, bool crossing = false) {
  Vec prior = *points.begin();
  for (Vec end : points) {
   int left = int(std::min(prior.x, end.x)), right = int(std::max(prior.x, end.x));
   int top = int(std::min(prior.y, end.y)), bottom = int(std::max(prior.y, end.y));
   for (int y = top; y <= bottom + 1; ++y) {
    for (int x = left; x <= right + 1; ++x) {
     auto& tile = terrain[y * N + x];
     if (tile == water) {
      if (crossing) tile = bridge;
     } else if (tile != bridge) tile = road;
    }
   }
   prior = end;
  }
 }

 void deposit(int x, int y, int width = 3, int height = 3) {
  patch(ground, x, y, width, height);
  for (int yy = y; yy < y + height; ++yy)
   for (int xx = x; xx < x + width; ++xx)
    ore[yy * N + xx] = 185 + random() % 70;
 }

 void rail_line(int x, int y, int length) {
  for (int yy = y; yy < y + length; ++yy) {
   auto& tile = terrain[yy*N+x];
   if (tile != bridge && tile != water && tile != road) tile = railbed;
  }
 }

 void waterway(int x, int width, int bend) {
  for (int y = 1; y < side - 1; ++y) {
   int bank = x + (y < bend ? 0 : y < bend + 8 ? 1 : 2);
   patch(water, bank, y, width, 1);
  }
 }

 void init_campaign() {
  const auto& map = campaign_maps[chapter];
  side = map.side;
  extra.map_rules = 1;
  rng = 83711 + chapter * 7919;
  credits = map.credits;
  next_wave = map.first_wave;
  terrain.fill(water);
  patch(ground, 1, 1, side - 2, side - 2);

  switch (chapter) {
   case 0: // Red Sand: an exposed haul road and a sheltered quarry route.
    landform(ridge, 16, 13, 4, 7); landform(ridge, 24, 24, 4, 7);
    landform(ridge, 5, 8, 3, 5); landform(crater, 16, 25, 4, 3);
    landform(crater, 29, 13, 4, 3);
    route({{7,22},{11,22},{11,19},{27,19},{27,10},{32,10}});
    route({{10,29},{19,29},{19,33},{31,33},{31,18}});
    deposit(3,30); deposit(19,15); deposit(28,27);
    deploy(0, {{hq,{7,25}}, {power,{4,25}}, {refinery,{7,29}}, {barracks,{10,25}},
               {harvester,{6,31}}, {tank,{10,21}}, {tank,{12,22}}, {rifle,{9,22}}, {rocket,{11,23}}});
    deploy(1, {{hq,{28,8}}, {power,{30,5}}, {barracks,{25,6}}, {turret,{26,13}},
               {tank,{23,16}}, {rifle,{26,15}}, {rocket,{31,9}}});
    deploy(2, {{supply,{15,21}}, {supply,{26,31}}});
    break;
   case 1: // Silent River: bridges lead from the eastern levee to the stranded relay.
    landform(woodland, 14, 15, 7, 8); landform(woodland, 26, 31, 4, 9);
    landform(woodland, 36, 13, 6, 6); landform(water, 10, 35, 5, 4);
    landform(ridge, 29, 17, 3, 3); landform(ridge, 13, 5, 4, 2);
    waterway(19, 4, 15);
    route({{6,10},{14,10}}); route({{14,10},{37,10}}, true);
    route({{9,24},{33,24}}, true); route({{33,24},{33,36},{39,36}});
    route({{9,39},{30,39}}, true); route({{9,24},{9,39}});
    deposit(37,31); deposit(28,15); deposit(14,30);
    deploy(0, {{hq,{36,24}}, {power,{39,24}}, {refinery,{38,28}}, {barracks,{36,20}},
               {harvester,{38,31}}, {tank,{32,24}}, {tank,{33,26}}, {rifle,{32,22}},
               {rocket,{34,23}}, {factory,{38,17}}, {power,{41,20}}, {artillery,{35,25}}, {turret,{31,26}}});
    deploy(1, {{hq,{7,10}}, {power,{4,12}}, {turret,{10,18}}, {tank,{10,21}},
               {tank,{15,17}}, {rifle,{17,11}}, {rocket,{6,20}}});
    deploy(2, {{relay,{14,24}}, {supply,{27,23}}, {supply,{16,36}}});
    deploy(0, {{shipyard,{26,27}}, {patrol_boat,{23.5f,29.5f}}, {landing_craft,{24.5f,30.5f}}});
    deploy(1, {{patrol_boat,{20.5f,12.5f}}});
    break;
   case 2: // Grid district: two substations on separate service roads.
    landform(ridge, 22, 24, 6, 9); landform(water, 40, 11, 4, 6);
    landform(ruins, 11, 17, 7, 4); landform(ruins, 37, 31, 6, 6);
    landform(crater, 20, 39, 7, 4);
    route({{22,11},{10,11},{10,37},{26,37},{26,43}});
    route({{24,11},{34,11},{34,35},{38,35},{38,43},{26,43}});
    route({{6,24},{16,24}}); route({{33,25},{43,25}});
    deposit(14,4); deposit(7,29); deposit(38,16);
    deploy(0, {{hq,{23,7}}, {power,{25,4}}, {refinery,{19,5}}, {barracks,{19,9}},
               {harvester,{16,7}}, {tank,{21,12}}, {tank,{23,13}}, {rifle,{19,12}},
               {rocket,{24,11}}, {factory,{28,8}}, {power,{29,4}}, {artillery,{26,12}}, {turret,{28,13}}});
    deploy(1, {{hq,{26,40}}, {power,{10,25}}, {power,{38,24}}, {factory,{30,43}},
               {turret,{11,22}}, {turret,{36,22}}, {turret,{27,36}}, {tank,{12,26}},
               {tank,{36,27}}, {artillery,{28,39}}});
    deploy(2, {{supply,{13,15}}, {supply,{33,17}}});
    break;
   case 3: // Blackpine: two passes approach the eastern quarry and its factory.
    landform(woodland, 13, 24, 10, 10); landform(woodland, 39, 40, 11, 9);
    landform(woodland, 40, 10, 10, 7); landform(ridge, 24, 13, 7, 10);
    landform(ridge, 28, 33, 7, 9); landform(ridge, 19, 44, 6, 5);
    landform(ridge, 43, 23, 6, 3);
    route({{9,37},{15,37},{15,23},{34,23},{34,15},{45,15}});
    route({{15,37},{20,37},{20,43},{39,43},{39,32},{48,32}});
    route({{34,23},{34,27},{38,27},{38,32}});
    deposit(4,46); deposit(16,14); deposit(36,35); deposit(47,41);
    deploy(0, {{hq,{9,40}}, {power,{5,40}}, {refinery,{8,45}}, {barracks,{12,42}},
               {harvester,{7,47}}, {tank,{13,37}}, {tank,{15,38}}, {rifle,{13,39}},
               {rocket,{15,36}}, {factory,{8,35}}, {power,{4,36}}, {artillery,{11,37}}, {turret,{15,40}}});
    deploy(1, {{hq,{45,11}}, {power,{48,14}}, {factory,{46,32}}, {turret,{40,30}},
               {turret,{45,35}}, {turret,{35,22}}, {tank,{37,24}}, {tank,{43,31}}, {rocket,{47,27}}});
    deploy(2, {{relay,{19,23}}, {relay,{37,33}}, {supply,{16,30}}, {supply,{31,43}}});
    deploy(0, {{helipad,{5,33}}, {rotorcraft,{6,33}}});
    break;
   case 4: // Rail corridor: the convoy turns west across a drainage canal.
    landform(ruins, 47, 24, 7, 6); landform(ruins, 17, 39, 8, 6);
    landform(crater, 50, 39, 6, 6); landform(woodland, 21, 19, 8, 9);
    landform(woodland, 10, 45, 5, 7); landform(ridge, 29, 28, 3, 5);
    waterway(32, 3, 24);
    route({{52,13},{46,13},{46,37},{38,37}});
    route({{19,37},{38,37}}, true); route({{19,37},{19,49},{7,49}});
    route({{21,14},{44,14}}, true); route({{16,52},{48,52}}, true);
    rail_line(42, 4, 50); rail_line(44, 4, 50);
    deposit(53,4); deposit(39,28); deposit(23,43); deposit(8,33);
    deploy(0, {{hq,{51,10}}, {power,{55,11}}, {refinery,{52,7}}, {barracks,{48,8}},
               {harvester,{54,5}}, {tank,{46,16}}, {tank,{48,16}}, {rifle,{45,17}},
               {rocket,{47,18}}, {factory,{55,15}}, {power,{55,7}}, {artillery,{49,14}}, {turret,{44,13}}});
    deploy(1, {{hq,{10,15}}, {power,{13,12}}, {factory,{10,10}}, {turret,{43,28}},
               {turret,{25,35}}, {turret,{11,45}}, {tank,{48,29}}, {tank,{25,32}}, {rocket,{18,44}}});
    deploy(2, {{relay,{43,23}}, {relay,{27,39}}, {supply,{48,26}}, {supply,{21,40}}});
    {
     constexpr Vec trucks[] = {{49,18},{52,18},{50,21}};
     for (int i = 0; i < 3; ++i) mission.convoy[i] = int16_t(add(harvester, 2, trucks[i]));
    }
    break;
   case 5: // Gray Tower: avenues separate the city substations and central core.
    for (int y : {10, 23, 35, 46}) {
     for (int x : {9, 22, 36, 47}) patch(ruins, x, y, 6, 5);
    }
    landform(woodland, 40, 46, 6, 5); landform(woodland, 23, 21, 6, 5);
    landform(water, 12, 43, 5, 8); landform(ridge, 31, 29, 3, 7);
    route({{55,54},{56,54},{56,17},{8,17},{8,8}});
    route({{9,31},{27,31},{27,8},{13,8}});
    route({{19,53},{19,44},{57,44}});
    route({{40,52},{40,26},{49,26},{49,17}});
    deposit(56,56); deposit(35,49); deposit(51,35); deposit(23,26);
    deploy(0, {{hq,{53,53}}, {power,{58,50}}, {refinery,{54,57}}, {barracks,{49,54}},
               {harvester,{57,57}}, {tank,{51,48}}, {tank,{54,48}}, {rifle,{49,49}},
               {rocket,{52,47}}, {factory,{57,54}}, {power,{61,53}}, {artillery,{52,51}}, {turret,{56,48}}});
    deploy(1, {{hq,{12,10}}, {power,{19,23}}, {power,{44,14}}, {factory,{9,8}},
               {turret,{13,15}}, {turret,{21,20}}, {turret,{43,18}}, {turret,{29,38}},
               {tank,{44,30}}, {tank,{18,27}}, {rocket,{39,34}}, {artillery,{25,14}}});
    deploy(2, {{relay,{46,43}}, {relay,{37,28}}, {relay,{24,32}}, {supply,{49,37}}, {supply,{23,37}}});
    deploy(0, {{commander,{51,50}}});
    deploy(1, {{aa_tower,{15,14}}, {rotorcraft,{30,17}}, {helipad,{34,14}}});
    break;
   case harbor_chapter:
    landform(woodland, 9, 25, 8, 11);landform(ridge, 12, 18, 6, 5);
    landform(ruins, 12, 49, 8, 7);landform(water, 51, 59, 11, 6);
    landform(woodland, 53, 29, 7, 8);
    patch(water, 22, 1, 17, side-2);
    landform(ground, 30, 25, 5, 6);
    patch(road, 29, 24, 4, 3);
    route({{44,13},{43,13},{43,40},{18,40},{18,52},{10,52}}, true);
    route({{17,37},{17,40}});route({{40,17},{45,17}});
    deposit(48,5);deposit(43,31);deposit(8,34);
    deploy(0, {{hq,{44,9}}, {power,{41,8}}, {refinery,{46,6}}, {barracks,{48,10}},
               {harvester,{48,5}}, {tank,{42,13}}, {tank,{44,14}}, {rifle,{41,17}},
               {rocket,{42,18}}, {factory,{51,11}}, {power,{50,3}}, {artillery,{45,13}},
               {shipyard,{40,18}}, {landing_craft,{38.5f,18.5f}}, {patrol_boat,{37.5f,17.5f}},
               {patrol_boat,{37.5f,20.5f}}, {helipad,{47,16}}, {rotorcraft,{47,16}},
               {commander,{41,15}}, {turret,{45,19}}, {power,{54,7}}});
    deploy(1, {{hq,{12,52}}, {power,{8,53}}, {factory,{15,50}},
               {turret,{18,44}}, {aa_tower,{14,47}}, {tank,{13,34}}, {rocket,{17,48}},
               {artillery,{10,49}}, {patrol_boat,{26.5f,34.5f}}});
    deploy(2, {{relay,harbor_beacon_site}, {relay,harbor_switch_site}, {supply,{45,34}}, {supply,{9,40}}});
    break;
   case lighthouse_chapter:
    init_lighthouse();
    break;
  }
  say(map.arrival, 9);
  power_update();
  reveal();
 }

 bool authored_map() const {return !extra.mode && side == campaign_maps[chapter].side;}
 Vec base_position() const {
  int id = nearest({side*.5f,side*.5f}, hq, 0);
  return id >= 0 ? e[id].p : authored_map() ? campaign_maps[chapter].home : Vec{6,9};
 }

 Vec deployment_view() const {
  if (!authored_map()) return {8,10};
  auto home = base_position();
  auto front = campaign_maps[chapter].front;
  return {(home.x + front.x)*.5f, (home.y + front.y)*.5f};
 }

 Vec convoy_stop(unsigned step) const {
  constexpr Vec current[] = {{46,23},{38,37},{19,38},{7,49}};
  constexpr Vec legacy[] = {{17,18},{26,18},{35,29},{37,40}};
  return (authored_map() ? current : legacy)[std::min(step, 3u)];
 }

 Vec convoy_station(unsigned gate) const {
  constexpr Vec stations[] = {{43,23},{27,39}};
  return stations[gate - 1];
 }

 bool convoy_gate(unsigned step) const {
  if (step != 1 && step != 2) return true;
  if (!authored_map()) return captured_nodes() >= int(step);
  auto station = convoy_station(step);
  int id = nearest(station, relay, 0);
  return id >= 0 && dist(station, e[id].p) < .5f;
 }

 bool active_convoy(int id) const {
  return id >= 0 && e[id].hp > 0 && e[id].team == 2 && e[id].kind == harvester;
 }

 int convoy_checkpoint() const {
  if (chapter != 4 || !authored_map()) return -1;
  for (unsigned step = 1; step <= 2; ++step) {
   if (!convoy_gate(step)) return nearest(convoy_station(step), relay, 2);
  }
  return -1;
 }

 Vec objective_position() const {
  auto home = base_position();
  if (extra.mode) return home;
  int id = -1;
  switch (chapter) {
   case 1:
    id = nearest(home, relay, 2);
    if (id < 0) id = nearest(home, relay, 0);
    break;
   case 2:
    id = nearest(home, power, 1);
    break;
   case 3:
    id = mission.stage ? nearest(home, factory, 1) : nearest(home, relay, 2);
    break;
   case 4:
    id = authored_map() ? convoy_checkpoint() : nearest(home, relay, 2);
    if (id < 0) {
     for (int truck : mission.convoy) if (active_convoy(truck)) return e[truck].p;
     return convoy_stop(3);
    }
    break;
   case 5:
    if (mission.stage == 0) id = nearest(home, relay, 2);
    if (mission.stage == 1) id = nearest(home, power, 1);
    break;
   case harbor_chapter:
    if (mission.stage!=harbor_shutdown) return harbor_beacon_site;
    break;
   case lighthouse_chapter:
    return mission.stage==restore_grid?island_grid_site:lighthouse_site;
  }
  if (id < 0) id = nearest(home, hq, 1);
  return id < 0 ? home : e[id].p;
 }
