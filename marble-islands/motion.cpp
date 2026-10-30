// SPDX-License-Identifier: CC0-1.0
#include "motion.hpp"
#ifdef PXA_POCKET_SERVICES
#include "../common/pocket-play/service-hooks.hpp"
#include "motion-service.hpp"
namespace {
marble_motion::Service service(marble_motion::device);
}
namespace game {
bool service_event(const pxa_event_t &event, bool foreground, uint64_t now) {
  return service.event(event, foreground, now);
}
void service_update(bool foreground, uint64_t now) {
  service.update(foreground, now);
}
} // namespace game
#endif
