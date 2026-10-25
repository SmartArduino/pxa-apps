/* Preserve each generated callback's real C signature. GPL-2.0-or-later. */
#include "quakedef.h"
#include "cprogs_def.h"
#include "callbacks.h"

void pxa_quake_call_pain(unsigned function_index, int attacker, float damage) {
  int has_arguments;
  switch (function_index) {
#define PAIN_NOARGS(fn) case getFunctionIndex(fn): has_arguments = 0; break
#define PAIN_ARGS(fn) case getFunctionIndex(fn): has_arguments = 1; break
    PAIN_NOARGS(qcc_SUB_Null);
    PAIN_NOARGS(qcc_dog_pain);
    PAIN_NOARGS(qcc_fd_secret_use);
    PAIN_NOARGS(qcc_player_pain);
    PAIN_NOARGS(qcc_shalrath_pain);
    PAIN_ARGS(qcc_Wiz_Pain);
    PAIN_ARGS(qcc_army_pain);
    PAIN_ARGS(qcc_demon1_pain);
    PAIN_ARGS(qcc_enf_pain);
    PAIN_ARGS(qcc_fish_pain);
    PAIN_ARGS(qcc_hknight_pain);
    PAIN_ARGS(qcc_knight_pain);
    PAIN_ARGS(qcc_ogre_pain);
    PAIN_ARGS(qcc_sham_pain);
    PAIN_ARGS(qcc_zombie_pain);
#undef PAIN_ARGS
#undef PAIN_NOARGS
    default: Host_Error("Unsupported pain callback %u", function_index); return;
  }
  /* Retain indirect dispatch so native optimization cannot duplicate the
   * monster animation bodies inside this adapter. */
  if (has_arguments)
    ((void (*)(int, float))getFunction(function_index))(attacker, damage);
  else
    ((void (*)(void))getFunction(function_index))();
}
