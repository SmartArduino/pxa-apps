// SPDX-License-Identifier: MIT
#include "../carrot_visual.hpp"
#include <cassert>
#include <cstdio>
int main(){
  using carrot::CarrotVisual;
  assert(CarrotVisual::damageStage(10,10)==0);
  assert(CarrotVisual::damageStage(9,10)==1);
  assert(CarrotVisual::damageStage(8,10)==1);
  assert(CarrotVisual::damageStage(7,10)==1);
  assert(CarrotVisual::damageStage(6,10)==2);
  assert(CarrotVisual::damageStage(5,10)==2);
  assert(CarrotVisual::damageStage(4,10)==2);
  assert(CarrotVisual::damageStage(3,10)==2);
  assert(CarrotVisual::damageStage(2,10)==3);
  assert(CarrotVisual::damageStage(1,10)==3);
  assert(CarrotVisual::damageStage(0,10)==3);
  CarrotVisual carrot;carrot.reset(10);
  assert(carrot.pose(0)==0&&carrot.pose(3450)==1);
  carrot.tap();assert(carrot.pose(0)==3);
  carrot.tick(.3f,10);assert(carrot.jump()>7);
  carrot.tick(.32f,10);assert(carrot.pose(0)==0&&carrot.jump()==0);
  carrot.tick(.02f,7);assert(carrot.pose(0)==2);
  carrot.tap();assert(carrot.pose(0)==2); // A pet must not hide incoming damage.
  carrot.tick(.5f,7);assert(carrot.pose(0)==0);
  carrot.tap();assert(carrot.pose(0)==3);
  carrot.reset(10);assert(carrot.pose(0)==0&&carrot.shake()==0);
  std::puts("Carrot: health thresholds, blink, pet, hit priority and retry reset passed");
}
