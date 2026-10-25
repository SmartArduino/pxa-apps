/* The QuakeC VM ignores unused arguments; C/Wasm function pointers do not. */
#pragma once
void pxa_quake_call_pain(unsigned function_index, int attacker, float damage);
