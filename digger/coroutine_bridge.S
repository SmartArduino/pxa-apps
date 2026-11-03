/* SPDX-License-Identifier: MIT. Independent Wasm C-stack switch. */
.text
.globaltype __stack_pointer, i32
.functype digger_engine_entry () -> ()
.globl digger_stack_enter
.type digger_stack_enter,@function
digger_stack_enter:
 .functype digger_stack_enter (i32) -> (i32)
 .local i32, i32
 global.get __stack_pointer
 local.set 1
 local.get 0
 global.set __stack_pointer
 call digger_engine_entry
 global.get __stack_pointer
 local.set 2
 local.get 1
 global.set __stack_pointer
 local.get 2
 end_function
