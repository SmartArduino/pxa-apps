/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Wasm ABI stack switch, with no C frame during a stack change.
 * The linked 1 MiB stack is partitioned: frontend uses its top 128 KiB,
 * game uses the remainder. Keep frontend active between Host exports.
 */
    .globaltype __stack_pointer, i32
    .functype br_app_start (i32, i32) -> (i32)
    .functype br_run_game () -> ()

    .globl pxa_app_start
    .type pxa_app_start,@function
pxa_app_start:
    .functype pxa_app_start (i32, i32) -> (i32)
    i32.const br_game_stack_pointer
    global.get __stack_pointer
    i32.const 131072
    i32.sub
    i32.store 0
    local.get 0
    local.get 1
    call br_app_start
    end_function

    .globl br_call_game
    .type br_call_game,@function
br_call_game:
    .functype br_call_game () -> ()
    .local i32
    global.get __stack_pointer
    local.set 0
    i32.const br_game_stack_pointer
    i32.load 0
    global.set __stack_pointer
    call br_run_game
    local.get 0
    global.set __stack_pointer
    end_function

    .globl br_save_game_stack
    .type br_save_game_stack,@function
br_save_game_stack:
    .functype br_save_game_stack () -> ()
    i32.const br_game_stack_pointer
    global.get __stack_pointer
    i32.store 0
    end_function
