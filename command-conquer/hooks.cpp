#include "function.h"
#include "port.h"

void Cnc_Stand_By_Begin() {port_loading_stage("Loading game artwork");}
void Cnc_Stand_By_End() {port_loading_stage("Preparing main menu");}

extern "C" void port_game_stats(void) {
  /* Frame advances after simulation logic, unlike presentation during menus
   * and loading. Keep this in the existing five-second statistics callback;
   * acceptance also requires advancing frames and a visible actual map. */
  port_log("PXA RTS active=%d frame=%d initializing=%d player=%d scenario=%.48s",
           !!GameActive, Frame, ScenarioInit, PlayerPtr != nullptr,
           Scen.ScenarioName);
}
