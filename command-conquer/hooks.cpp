#include "function.h"
#include "port.h"

void Cnc_Stand_By_Begin() {port_loading_stage("Loading game artwork");}
void Cnc_Stand_By_End() {port_loading_stage("Preparing main menu");}
void Pxa_Load_Mission() {port_loading_restart("Loading mission");}
void Pxa_Load_Phase(const char *phase) {port_loading_stage(phase);}
#ifdef CNC_GAME_RA
void Pxa_Shape_Slot_Stats(unsigned &,unsigned &);
#endif

extern "C" void port_game_stats(void) {
#ifdef CNC_GAME_RA
  unsigned hits,misses;Pxa_Shape_Slot_Stats(hits,misses);
  port_log("PXA SHP slot_hits=%u misses=%u",hits,misses);
#endif
  /* Frame advances after simulation logic, unlike presentation during menus
   * and loading. Keep this in the existing five-second statistics callback;
   * acceptance also requires advancing frames and a visible actual map. */
  port_log("PXA RTS active=%d frame=%d initializing=%d player=%d scenario=%.48s",
           !!GameActive, Frame, ScenarioInit, PlayerPtr != nullptr,
           Scen.ScenarioName);
}
