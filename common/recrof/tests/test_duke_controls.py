#!/usr/bin/env python3
"""Exercise real Duke adapter/keyboard code for simultaneous actions and taps."""
import argparse
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
apps = Path(__file__).resolve().parents[3]
root = apps.parents[1]
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
port = (apps / 'duke3d/port.c').read_text()
keyboard = (apps / 'duke3d/vendor/Game/src/keyboard.c').read_text()


def function(source, name):
    start = re.search(r'^\w[^\n]*\b' + name + r'\([^;]*?\)\s*\{', source, re.M).start()
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


source = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "keymap.h"
#include "controller.h"
#include "pointer_motion.h"
#include "pxa_ui.h"
uint8_t KB_KeyDown_bit[(MAXKEYBOARDSCAN + 7) / 8];
kb_scancode KB_LastScan;
static int keyIsWaiting;
static uint8_t scq[256], downq[256], keys_this_update[256];
static unsigned kh, kt;
static bool menu;
static bool weapon_select_held,weapon_select_used;
static uint8_t pending_action_taps;
static const uint8_t action_tap_codes[]={sc_Space,sc_P,sc_O};
static float mx,my;
static port_pointer_motion_t pointer_motion;
#define MODE_GAME 4
#define MODE_MENU 1
#define MODE_TYPE 16
static struct {int gm;} ps[1];
static int myconnectindex;
static struct {int recstat;} ud;
static int pending_fire;
static void port_game_reset_motion(void);
static void port_game_key(int key,int down);
static void port_poll_input(void){if(pending_fire){pending_fire=0;port_game_key(200,1);}}
'''
for name in ['inputSetMenuMode', 'inputIsMenuMode', 'inputPostKeyEvent',
             'post_action_tap', 'finish_action_sample',
             'inputBeginKeyUpdate', 'inputGetKeyEvent', 'port_game_key',
             'port_game_reset_motion']:
    source += function(port, name) + '\n'
handler = function(keyboard, 'keyhandler')
handler = handler.split('#elif ESP_PLATFORM\n', 1)[1].split('#else\n', 1)[0]
source += 'static void keyhandler(void){\n' + handler + '}\n'
source += r'''
static port_controller_t weapon_pad;
static uint8_t active_keys[256];
static void reset(int in_menu){
  port_game_reset_motion();
  kh=kt=0;memset(KB_KeyDown_bit,0,sizeof(KB_KeyDown_bit));
  menu=in_menu;ps[0].gm=in_menu?MODE_MENU:MODE_GAME;
  keyIsWaiting=0;pending_fire=0;
  memset(&weapon_pad,0,sizeof(weapon_pad));memset(active_keys,0,sizeof(active_keys));
}
static void buttons(uint32_t bits,int connected){
  uint8_t next[256];
  if(!connected)port_game_reset_motion();
  port_controller_state(&weapon_pad,0,connected,bits,menu,0,next);
  /* Match runtime.c's ascending aggregate key dispatch. */
  for(int k=0;k<256;++k)if(active_keys[k]!=next[k])port_game_key(k,next[k]);
  memcpy(active_keys,next,sizeof(active_keys));
}
static void no_actions(void){
  assert(!KB_KeyPressed(sc_LeftControl)&&!KB_KeyPressed(sc_X)&&!KB_KeyPressed(sc_Space));
}
int main(void){
  port_controller_t pad={0};uint8_t keys[256];
  reset(0);
  port_controller_state(&pad,0,1,PXA_CONTROLLER_UP|PXA_CONTROLLER_LEFT|
      PXA_CONTROLLER_R|PXA_CONTROLLER_L,0,0,keys);
  for(int k=0;k<256;++k)if(keys[k])port_game_key(k,1);
  port_game_key('e',1); /* Touch USE remains immediate. */
  keyhandler();
  /* All actions reach the actual engine keyboard state in one update. */
  assert(KB_KeyPressed(sc_UpArrow)&&KB_KeyPressed(sc_Q));
  assert(KB_KeyPressed(sc_LeftControl)&&KB_KeyPressed(sc_X)&&KB_KeyPressed(sc_Space));
  assert(kh==kt);
  for(int k=0;k<256;++k)if(keys[k])port_game_key(k,0);
  port_game_key('e',0);
  keyhandler();
  for(unsigned k=0;k<sizeof(KB_KeyDown_bit);++k)assert(!KB_KeyDown_bit[k]);
  /* A short fire tap remains down for one poll, then releases. */
  port_game_key(200,1);port_game_key(200,0);
  keyhandler();assert(KB_KeyPressed(sc_LeftControl)&&kh!=kt);
  keyhandler();assert(!KB_KeyPressed(sc_LeftControl)&&kh==kt);
  /* Input still pending in the runtime is picked up before this poll. */
  pending_fire=1;keyhandler();assert(!pending_fire&&KB_KeyPressed(sc_LeftControl));
  reset(1);
  inputPostKeyEvent(sc_DownArrow,1);inputPostKeyEvent(sc_Enter,1);
  keyhandler();assert(KB_KeyPressed(sc_DownArrow)&&!KB_KeyPressed(sc_Enter));
  keyhandler();assert(KB_KeyPressed(sc_Enter));

  /* SELECT+R reaches the engine's next-weapon scancode without firing,
   * jumping or using. Full-state refreshes and holds do not repeat it. */
  reset(0);buttons(PXA_CONTROLLER_SELECT,1);keyhandler();no_actions();
  buttons(PXA_CONTROLLER_SELECT|PXA_CONTROLLER_R,1);keyhandler();
  assert(KB_KeyPressed(sc_P));no_actions();
  /* Rendering may poll many times before the next fixed game update. */
  for(int i=0;i<12;++i){keyhandler();assert(KB_KeyPressed(sc_P));}
  finish_action_sample();keyhandler();assert(!KB_KeyPressed(sc_P));
  for(int i=0;i<12;++i){buttons(PXA_CONTROLLER_SELECT|PXA_CONTROLLER_R,1);keyhandler();assert(!KB_KeyPressed(sc_P));no_actions();}
  /* Modifier-first release suppresses fire until the shoulder is released. */
  buttons(PXA_CONTROLLER_R,1);keyhandler();no_actions();
  buttons(0,1);keyhandler();no_actions();
  buttons(PXA_CONTROLLER_R,1);keyhandler();assert(KB_KeyPressed(sc_LeftControl));
  buttons(0,1);keyhandler();no_actions();

  reset(0);buttons(PXA_CONTROLLER_SELECT|PXA_CONTROLLER_L,1);keyhandler();
  assert(KB_KeyPressed(sc_O));no_actions();finish_action_sample();keyhandler();assert(!KB_KeyPressed(sc_O));
  /* Shoulder-first release does not produce a lone SELECT use. */
  buttons(PXA_CONTROLLER_SELECT,1);keyhandler();buttons(0,1);keyhandler();no_actions();
  buttons(PXA_CONTROLLER_L,1);keyhandler();assert(KB_KeyPressed(sc_X));
  buttons(0,1);keyhandler();no_actions();

  /* A lone SELECT creates exactly one complete use tap on release. */
  reset(0);buttons(PXA_CONTROLLER_SELECT,1);keyhandler();no_actions();
  buttons(0,1);keyhandler();assert(KB_KeyPressed(sc_Space));
  for(int i=0;i<12;++i){keyhandler();assert(KB_KeyPressed(sc_Space));}
  finish_action_sample();keyhandler();no_actions();assert(kh==kt);

  /* Background/reset, disconnection and menus cancel a pending SELECT. */
  reset(0);buttons(PXA_CONTROLLER_SELECT,1);keyhandler();
  port_game_reset_motion();buttons(0,1);keyhandler();no_actions();
  reset(0);buttons(PXA_CONTROLLER_SELECT,1);keyhandler();
  buttons(0,0);keyhandler();no_actions();
  reset(0);buttons(PXA_CONTROLLER_SELECT|PXA_CONTROLLER_R,1);keyhandler();
  assert(KB_KeyPressed(sc_P));port_game_reset_motion();buttons(0,1);keyhandler();
  assert(!KB_KeyPressed(sc_P));no_actions();
  reset(0);buttons(PXA_CONTROLLER_SELECT,1);keyhandler();
  ps[0].gm=MODE_MENU;keyhandler();buttons(0,1);keyhandler();no_actions();
  buttons(PXA_CONTROLLER_SELECT,1);keyhandler();assert(KB_KeyPressed(sc_Enter));
  buttons(0,1);keyhandler();assert(!KB_KeyPressed(sc_Enter));
  puts("Duke: simultaneous actions, taps, weapon chords, release order, cancellation and menus pass");
}
'''
(out / 'controls.c').write_text(source)
subprocess.run(['cc', '-std=c11', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-attributes',
                '-DPORT_DUKE_CONTROLLER=1',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                '-I' + str(apps / 'duke3d'), '-I' + str(apps / 'common/recrof'),
                '-I' + str(root / 'deps/pxa-system/sdk/guest-c/include'),
                str(out / 'controls.c'), str(apps / 'common/recrof/controller.c'),
                '-o', str(out / 'controls')], check=True)
subprocess.run([str(out / 'controls')], check=True)
