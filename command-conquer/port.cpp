#include "port.h"
#include <stdio.h>
int cnc_game_main(int, char **);
extern "C" void port_game_main() {
  port_loading_stage("Checking game files");
#ifdef CNC_GAME_RA
  const char *required = "/data/REDALERT.MIX";
#else
  const char *required = "/data/CONQUER.MIX";
#endif
  FILE *f = fopen(required, "rb");
  if (!f) {
#ifdef CNC_GAME_RA
    f = fopen("/data/LOCAL.MIX", "rb");
#else
    f = fopen("/data/DEMO.MIX", "rb");
#endif
  }
  if (!f)
    port_error("Missing game MIX archives.\nImport game data with the included "
               "tools.");
  fclose(f);
#ifdef PORT_LOADING_TRACE
  port_log("PXA LOAD required archives checked");
#endif
#ifdef CNC_GAME_RA
  const char *config_name = "/data/redalert.ini";
  char name[] = "/data/vanillara";
#else
  const char *config_name = "/data/conquer.ini";
  char name[] = "/data/vanillatd";
#endif
  f = fopen(config_name, "rb");
  if (f)
    fclose(f);
  else {
    f = fopen(config_name, "wb");
    if (!f)
      port_error("Cannot create game settings.");
    fputs("[Video]\nDOSMode=false\n[Intro]\nPlayIntro=false\n[Options]"
          "\nSlowPalette=false\n",
          f);
    fclose(f);
  }
  char *argv[] = {name, nullptr};
#ifdef PORT_LOADING_TRACE
  port_log("PXA LOAD settings ready; entering engine");
#endif
  port_loading_stage("Preparing game engine");
  int status = cnc_game_main(1, argv);
  port_log("Game returned %d", status);
  port_error("Game closed.");
}
