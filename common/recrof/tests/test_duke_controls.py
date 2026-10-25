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
#include "pxa_ui.h"
uint8_t KB_KeyDown_bit[(MAXKEYBOARDSCAN + 7) / 8];
kb_scancode KB_LastScan;
static int keyIsWaiting;
static uint8_t scq[256], downq[256], keys_this_update[256];
static unsigned kh, kt;
static bool menu;
#define MODE_GAME 4
#define MODE_MENU 8
#define MODE_TYPE 16
static struct {int gm;} ps[1];
static int myconnectindex;
static struct {int recstat;} ud;
static int pending_fire;
static void port_game_reset_motion(void){}
static void port_game_key(int key,int down);
static void port_poll_input(void){if(pending_fire){pending_fire=0;port_game_key(200,1);}}
'''
for name in ['inputSetMenuMode', 'inputIsMenuMode', 'inputPostKeyEvent',
             'inputBeginKeyUpdate', 'inputGetKeyEvent', 'port_game_key']:
    source += function(port, name) + '\n'
handler = function(keyboard, 'keyhandler')
handler = handler.split('#elif ESP_PLATFORM\n', 1)[1].split('#else\n', 1)[0]
source += 'static void keyhandler(void){\n' + handler + '}\n'
source += r'''
static void reset(int in_menu){
  kh=kt=0;memset(KB_KeyDown_bit,0,sizeof(KB_KeyDown_bit));
  menu=in_menu;ps[0].gm=in_menu?MODE_MENU:MODE_GAME;
  keyIsWaiting=0;pending_fire=0;
}
int main(void){
  port_controller_t pad={0};uint8_t keys[256];
  reset(0);
  port_controller_state(&pad,0,1,PXA_CONTROLLER_UP|PXA_CONTROLLER_LEFT|
      PXA_CONTROLLER_R|PXA_CONTROLLER_L|PXA_CONTROLLER_SELECT,0,0,keys);
  for(int k=0;k<256;++k)if(keys[k])port_game_key(k,1);
  keyhandler();
  /* All actions reach the actual engine keyboard state in one update. */
  assert(KB_KeyPressed(sc_UpArrow)&&KB_KeyPressed(sc_Q));
  assert(KB_KeyPressed(sc_LeftControl)&&KB_KeyPressed(sc_X)&&KB_KeyPressed(sc_Space));
  assert(kh==kt);
  for(int k=0;k<256;++k)if(keys[k])port_game_key(k,0);
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
  puts("Duke: simultaneous movement/fire/jump, quick taps, fresh input and ordered menus pass");
}
'''
(out / 'controls.c').write_text(source)
subprocess.run(['cc', '-std=c11', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-attributes',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                '-I' + str(apps / 'duke3d'), '-I' + str(apps / 'common/recrof'),
                '-I' + str(root / 'deps/pxa-system/sdk/guest-c/include'),
                str(out / 'controls.c'), str(apps / 'common/recrof/controller.c'),
                '-o', str(out / 'controls')], check=True)
subprocess.run([str(out / 'controls')], check=True)
