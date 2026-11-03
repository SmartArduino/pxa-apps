/* SPDX-License-Identifier: MIT */
.text
.globaltype __stack_pointer, i32
.functype jy_engine_entry (i32) -> ()
.globl jy_stack_enter
.type jy_stack_enter,@function
jy_stack_enter:
 .functype jy_stack_enter (i32, i32) -> (i32)
 .local i32, i32
 global.get __stack_pointer
 local.set 2
 local.get 1
 global.set __stack_pointer
 local.get 0
 call jy_engine_entry
 global.get __stack_pointer
 local.set 3
 local.get 2
 global.set __stack_pointer
 local.get 3
 end_function
.globl jy_stack_pointer
.type jy_stack_pointer,@function
jy_stack_pointer:
 .functype jy_stack_pointer () -> (i32)
 global.get __stack_pointer
 end_function
