cmake_minimum_required(VERSION 3.20)
project(pxa_command_conquer_sd C CXX)
set(PXA_RECROF_ENGINE_STACK_BYTES 65536)
set(PXA_RECROF_CALLBACK_STACK_BYTES 16384)
add_subdirectory(../command-conquer game-engine)
target_compile_definitions(pxa_main PRIVATE PORT_EXTERNAL_GAME_DATA=1 PORT_COOPERATE_BUDGET_US=32000
    PORT_TICK_US=8000 PORT_CONTINUATION_BYTES=16384)
