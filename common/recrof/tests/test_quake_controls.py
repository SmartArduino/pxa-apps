#!/usr/bin/env python3
"""Exercise controller -> port -> real Quake key bindings and command emission."""
import argparse
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
apps = Path(__file__).resolve().parents[3]
root = apps.parents[1]
quake = apps / 'quake'
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
port = (quake / 'port.c').read_text()
keys = (quake / 'vendor/quake/keys.c').read_text()
menu = (quake / 'vendor/quake/menu.c').read_text()


def function(source, name):
    start = re.search(r'^\w[^\n]*\b' + name + r'\([^;]*?\)\s*\{', source, re.M).start()
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


bindings = re.search(r'const char \*const bindnames\[\]\[2\]\s*=\s*\{.*?\n\};', menu, re.S).group()
shifts = re.search(r'const char keyshift\[256\]\s*=\s*\{.*?\n\};', keys, re.S).group()
source = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "controller.h"
#include "pxa_ui.h"
typedef unsigned char byte;
typedef int qboolean;
#include "keys.h"
#define WIN32 0
#define USE_MORE_STACK 1
#define USE_KEY_QBOOLEANS 1
#define Q_strcmp strcmp
byte keybindings[256];
byte key_repeats[256];
uint8_t key_lastpress;
int key_count=1,shift_down,con_forcedup;
keydest_t key_dest;
int keydown[256],consolekeys[256],menubound[256];
static struct {struct {int demoplayback;} cls;} globals,*_g=&globals;
static struct {int k,d;} keyq[128];
static unsigned kh,kt;
static char emitted[8192];
static int unbound,menu_key;
static void port_game_reset_motion(void){}
static int pending_fire;
static void port_game_key(int key,int down);
static void port_poll_input(void){if(pending_fire){pending_fire=0;port_game_key(200,1);}}
static void Cbuf_AddText(const char *s){assert(strlen(emitted)+strlen(s)<sizeof(emitted));strcat(emitted,s);}
static void Con_Printf(const char *fmt,...){if(strstr(fmt,"unbound"))++unbound;}
static char *Key_KeynumToString(int k){(void)k;return "key";}
static void M_ToggleMenu_f(void){key_dest=key_menu;}
static void M_Keydown(int k){menu_key=k;}
static void Key_Message(int k){(void)k;}
static void Key_Console(int k){(void)k;}
static void Sys_Error(const char *s){(void)s;abort();}
'''
source += bindings + '\n#define NUMCOMMANDS (sizeof(bindnames)/sizeof(bindnames[0]))\n'
for name in ['findKeyCommandIndex', 'getBindname', 'getKeybindName']:
    source += function(menu, name) + '\n'
source += shifts + '\n'
for name in ['Key_SetBinding', 'Key_Event']:
    source += function(keys, name) + '\n'
for name in ['port_game_key', 'Sys_SendKeyEvents']:
    source += function(port, name) + '\n'
source += r'''
static void exercise(uint32_t button,const char *command,int engine_key){
  port_controller_t pad={0};uint8_t held[256],released[256];
  memset(keybindings,0,sizeof(keybindings));emitted[0]=0;unbound=0;key_dest=key_game;
  assert(port_controller_state(&pad,0,1,button,0,0,held));
  for(int k=0;k<256;++k)if(held[k])port_game_key(k,1);
  Sys_SendKeyEvents();
  char wanted[128];snprintf(wanted,sizeof(wanted),"%s %i\n",command,engine_key);
  if(strcmp(emitted,wanted))fprintf(stderr,"press button=%u wanted=[%s] actual=[%s]\n",button,wanted,emitted);
  assert(!strcmp(emitted,wanted));assert(unbound==0);
  /* Releasing after entering a menu must release the original game key. */
  key_dest=key_menu;emitted[0]=0;
  port_controller_state(&pad,0,1,0,1,0,released);
  for(int k=0;k<256;++k)if(held[k]&&!released[k])port_game_key(k,0);
  Sys_SendKeyEvents();
  snprintf(wanted,sizeof(wanted),"-%s %i\n",command+1,engine_key);
  /* Legacy signed-char keyshift can repeat the same harmless release. */
  assert(!strncmp(emitted,wanted,strlen(wanted)));
  assert(!emitted[strlen(wanted)] || !strcmp(emitted+strlen(wanted),wanted));
}
int main(void){
  exercise(PXA_CONTROLLER_R,"+attack",K_MOUSE1);
  exercise(PXA_CONTROLLER_L,"+jump",K_SPACE);
  exercise(PXA_CONTROLLER_LEFT,"+moveleft",',');
  exercise(PXA_CONTROLLER_RIGHT,"+moveright",'.');
  exercise(PXA_CONTROLLER_UP,"+forward",K_UPARROW);
  exercise(PXA_CONTROLLER_DOWN,"+back",K_DOWNARROW);
  exercise(PXA_CONTROLLER_SELECT,"+use",'e');
  /* Input queued during rendering is applied in this engine update. */
  emitted[0]=0;key_dest=key_game;pending_fire=1;Sys_SendKeyEvents();
  assert(!pending_fire && !strcmp(emitted,"+attack 200\n"));
  port_game_key(200,0);Sys_SendKeyEvents();
  emitted[0]=0;key_dest=key_game;port_game_key('q',1);Sys_SendKeyEvents();
  assert(!strcmp(emitted,"impulse 10\n"));port_game_key('q',0);Sys_SendKeyEvents();
  /* A stale user config must not redirect the physical fire button. */
  emitted[0]=0;Key_SetBinding(K_MOUSE1,"+jump");port_game_key(200,1);Sys_SendKeyEvents();
  assert(!strcmp(emitted,"+attack 200\n"));port_game_key(200,0);Sys_SendKeyEvents();
  /* Menu confirmation stays ENTER and does not fire. */
  emitted[0]=0;menu_key=0;key_dest=key_menu;port_game_key(200,1);Sys_SendKeyEvents();
  assert(menu_key==K_ENTER);assert(!emitted[0]);port_game_key(200,0);Sys_SendKeyEvents();
  for(unsigned i=1;i<=NUMCOMMANDS;++i)assert(getBindname(i));
  assert(!getBindname(0)&&!getBindname(NUMCOMMANDS+1));
  puts("Quake physical controls emit attack, strafing, movement, jump, weapon and use commands; menu and releases pass");
}
'''
(out / 'controls.c').write_text(source)
subprocess.run(['cc', '-std=c11', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-attributes',
                '-Wno-char-subscripts', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                '-I' + str(quake / 'vendor/quake'), '-I' + str(apps / 'common/recrof'),
                '-I' + str(root / 'deps/pxa-system/sdk/guest-c/include'),
                str(out / 'controls.c'), str(apps / 'common/recrof/controller.c'),
                '-o', str(out / 'controls')], check=True)
subprocess.run([str(out / 'controls')], check=True)
