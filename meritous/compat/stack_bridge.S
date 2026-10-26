/* SPDX-License-Identifier: GPL-3.0-or-later
 * Thin Wasm ABI bridge. No C frame is allocated while changing stacks.
 * Partition the linker's reserved 4 MiB auxiliary stack: the upper 128 KiB
 * belongs to the frontend; the remaining lower region belongs to the game.
 * Keep both inside WAMR's auxiliary-stack bounds. Between exports the default
 * remains the frontend, including Host-invoked Guest malloc/free.
 */
    .globaltype __stack_pointer, i32
    .functype mt_app_start (i32, i32) -> (i32)
    .functype mt_run_game () -> ()

    .globl pxa_app_start
    .type pxa_app_start,@function
pxa_app_start:
    .functype pxa_app_start (i32, i32) -> (i32)
    i32.const mt_game_stack_pointer
    global.get __stack_pointer
    i32.const 131072
    i32.sub
    i32.store 0
    local.get 0
    local.get 1
    call mt_app_start
    end_function

    .globl mt_call_game
    .type mt_call_game,@function
mt_call_game:
    .functype mt_call_game () -> ()
    .local i32
    global.get __stack_pointer
    local.set 0
    i32.const mt_game_stack_pointer
    i32.load 0
    global.set __stack_pointer
    call mt_run_game
    local.get 0
    global.set __stack_pointer
    end_function

    .globl mt_get_stack_pointer
    .type mt_get_stack_pointer,@function
mt_get_stack_pointer:
    .functype mt_get_stack_pointer () -> (i32)
    global.get __stack_pointer
    end_function
