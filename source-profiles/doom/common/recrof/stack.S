/* Separate the suspended engine's C stack from PXA event callbacks.
 * Asyncify saves Wasm locals; pointers to C stack objects must remain valid
 * while the Host invokes other exports, including its malloc/free bridge. */
.globaltype __stack_pointer, i32
.text
.globl port_stack_get
.type port_stack_get,@function
port_stack_get:
    .functype port_stack_get () -> (i32)
    global.get __stack_pointer
    end_function
.globl port_stack_set
.type port_stack_set,@function
port_stack_set:
    .functype port_stack_set (i32) -> ()
    local.get 0
    global.set __stack_pointer
    end_function
