#include "duke3d.h"
#include "port.h"

/* Report actual engine state, so accepted input events cannot masquerade as
 * verified movement. Called by the existing five-second statistics hook. */
void port_duke_input_stats(void) {
  if (myconnectindex < 0 || myconnectindex >= MAXPLAYERS) return;
  const struct player_struct *player = &ps[myconnectindex];
  port_log("PXA DUKE INPUT x=%ld y=%ld z=%ld angle=%d mode=%u up=%d down=%d",
      (long)player->posx, (long)player->posy, (long)player->posz,
      player->ang, (unsigned)player->gm,
      KB_KeyPressed(sc_UpArrow), KB_KeyPressed(sc_DownArrow));
}
