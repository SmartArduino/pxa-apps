// SPDX-License-Identifier: MIT
#pragma once
#include "model.hpp"

inline bool build_at(iron::World& world,int kind,iron::Vec site){
 if(world.build_active())return world.place(kind,site);
 if(world.can_place(kind,site))world.start_construction(kind);
 return false;
}
