#include "duke3d.h"
#include "port.h"
#if PXA_GAME_RENDER_BLIT
#include "pxa_game_render_spans.h"
extern uint64_t port_game_render_context;
#endif

/* Report actual engine state, so accepted input events cannot masquerade as
 * verified movement. Called by the existing five-second statistics hook. */
void port_duke_input_stats(void) {
  if (myconnectindex < 0 || myconnectindex >= MAXPLAYERS) return;
  const struct player_struct *player = &ps[myconnectindex];
#if PXA_GAME_RENDER_BLIT
  uint8_t native[64] = {0};
  if (port_game_render_context)
    pxa_io(port_game_render_context, PXA_GAME_RENDER_IO_SPAN_TELEMETRY,
           native, sizeof(native));
  port_log("PXA DUKE INPUT x=%ld y=%ld z=%ld angle=%d mode=%u up=%d down=%d hb=%llu hr=%llu hx=%llu",
      (long)player->posx, (long)player->posy, (long)player->posz,
      player->ang, (unsigned)player->gm,
      KB_KeyPressed(sc_UpArrow), KB_KeyPressed(sc_DownArrow),
      (unsigned long long)pxa_load_u64(native),
      (unsigned long long)pxa_load_u64(native + 8),
      (unsigned long long)pxa_load_u64(native + 40));
#else
  port_log("PXA DUKE INPUT x=%ld y=%ld z=%ld angle=%d mode=%u up=%d down=%d",
      (long)player->posx, (long)player->posy, (long)player->posz,
      player->ang, (unsigned)player->gm,
      KB_KeyPressed(sc_UpArrow), KB_KeyPressed(sc_DownArrow));
#endif
}
