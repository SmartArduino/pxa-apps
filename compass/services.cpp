// SPDX-License-Identifier: MIT
#include "../common/pocket-play/service-hooks.hpp"
#include "model.hpp"
#include "sensors.hpp"
namespace {
compass_sensor::Service<compass::Model> service(compass::device, "Compass",
                                                "magnetic samples ready", 100);
}
namespace game {
bool service_event(const pxa_event_t &e, bool foreground, uint64_t now) {
  return service.event(e, foreground, now);
}
void service_update(bool foreground, uint64_t now) {
  service.update(foreground, now);
}
} // namespace game
