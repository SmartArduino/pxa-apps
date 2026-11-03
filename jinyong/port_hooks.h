/* SPDX-License-Identifier: MIT */
#ifndef JY_PORT_HOOKS_H
#define JY_PORT_HOOKS_H
#include <stdlib.h>
#include "protected_bridge.h"
/* Do not include lua.h here: force-including it before loslib_c is defined
 * would suppress Lua's per-translation-unit luaconf temporary-file macros. */
struct lua_State;
void jy_port_lua_state(struct lua_State *);
void jy_port_config(struct lua_State *);
void jy_port_lua_result(struct lua_State *,const char *,int);
void jy_port_key_result(int);
void jy_port_debug(const char *);
/* The original exit callers finish cleanup before reaching this boundary. */
#define exit(code) jy_finish((code))
#endif
