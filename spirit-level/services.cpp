// SPDX-License-Identifier: CC0-1.0
#include "model.hpp"
#ifdef PXA_POCKET_SERVICES
#include "../common/pocket-imu/service.hpp"
#include "../common/pocket-play/service-hooks.hpp"
namespace {
pocket_imu::Service<level::Model> service(level::device, "Level",
                                          "gravity ready", 20);
}
namespace game {
bool service_event(const pxa_event_t &e, bool foreground, uint64_t now) {
  return service.event(e, foreground, now);
}
void service_update(bool foreground, uint64_t now) {
  service.update(foreground, now);
}
} // namespace game
#endif
