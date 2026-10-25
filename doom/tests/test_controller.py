#!/usr/bin/env python3
"""Check the controller through the real DOOM key and mouse adapters."""
import argparse
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
doom = Path(__file__).resolve().parents[1]
apps = doom.parent
root = apps.parents[1]
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
port = (doom / 'doom.c').read_text()


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
#include <stdio.h>
#include <string.h>
#include "vendor/PureDOOM.h"
#include "controller.h"
#include "pxa_ui.h"
player_t players[MAXPLAYERS];
int consoleplayer;
static int messageToPrint, messageNeedsInput;
static int held[256], initialized=1, menu, last_key, released, mouse_x, mouse_y;
int port_game_in_menu(void){return menu;}
void doom_key_down(doom_key_t key){last_key=key;}
void doom_key_up(doom_key_t key){released=key;}
void doom_mouse_move(int x,int y){mouse_x=x;mouse_y=y;}
'''
source += function(port, 'port_game_key') + '\n'
source += function(port, 'port_game_motion') + '\n'
source += r'''
static void key(uint32_t button,int expected){
  port_controller_t pad={0};uint8_t keys[256];last_key=released=-1;
  port_controller_state(&pad,0,1,button,menu,0,keys);
  for(int k=0;k<256;++k)if(keys[k])port_game_key(k,1);
  assert(last_key==expected);
  /* Release the engine key even if a menu opens before button-up. */
  int old_menu=menu;menu=1;
  for(int k=0;k<256;++k)if(keys[k])port_game_key(k,0);
  assert(released==expected);menu=old_menu;
}
int main(void){
  key(PXA_CONTROLLER_UP,DOOM_KEY_UP_ARROW);key(PXA_CONTROLLER_DOWN,DOOM_KEY_DOWN_ARROW);
  key(PXA_CONTROLLER_LEFT,DOOM_KEY_COMMA);key(PXA_CONTROLLER_RIGHT,DOOM_KEY_PERIOD);
  key(PXA_CONTROLLER_R,DOOM_KEY_CTRL);key(PXA_CONTROLLER_L,DOOM_KEY_SPACE);
  key(PXA_CONTROLLER_B,DOOM_KEY_TAB);key(PXA_CONTROLLER_SELECT,DOOM_KEY_SHIFT);
  players[0].readyweapon=wp_pistol;players[0].weaponowned[wp_shotgun]=1;
  key(PXA_CONTROLLER_X,DOOM_KEY_3);
  port_controller_t pad={0};uint8_t keys[256];
  port_controller_state(&pad,0,1,PXA_CONTROLLER_Y,0,0,keys);
  for(int k=0;k<256;++k)assert(!keys[k]);
  port_controller_motion_t move=port_controller_advance(&pad,100000,0);
  port_game_motion(move.x,move.y);assert(mouse_x==-120&&mouse_y==0);
  port_controller_state(&pad,0,1,PXA_CONTROLLER_A,0,0,keys);
  move=port_controller_advance(&pad,100000,0);
  port_game_motion(move.x,move.y);assert(mouse_x==120&&mouse_y==0);
  menu=1;key(PXA_CONTROLLER_A,DOOM_KEY_ENTER);key(PXA_CONTROLLER_B,DOOM_KEY_ESCAPE);
  messageToPrint=messageNeedsInput=1;key(PXA_CONTROLLER_A,DOOM_KEY_Y);key(PXA_CONTROLLER_B,DOOM_KEY_N);
  puts("DOOM: move/strafe, view, fire/use, weapon/map/run, menus and releases pass");
}
'''
(out / 'controller.c').write_text(source)
subprocess.run(['cc', '-std=c11', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-attributes',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-DPORT_DOOM_CONTROLLER=1',
                '-I' + str(doom), '-I' + str(apps / 'common/recrof'),
                '-I' + str(root / 'deps/pxa-system/sdk/guest-c/include'),
                str(out / 'controller.c'), str(apps / 'common/recrof/controller.c'),
                '-o', str(out / 'controller')], check=True)
subprocess.run([str(out / 'controller')], check=True)
