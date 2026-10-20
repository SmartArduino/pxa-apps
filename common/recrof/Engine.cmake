include_guard(GLOBAL)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)
set(PXA_RECROF_DIR "${CMAKE_CURRENT_LIST_DIR}")
function(pxa_recrof_component)
    option(PXA_RECROF_NATIVE_PCM "Exact source PCM format; requires Audio 0.10 and native main loop" OFF)
    option(PXA_RECROF_MAIN_LOOP "Core 2.1 native event port; signed main-loop component required" OFF)
    option(PXA_RECROF_HOST_BLIT "Experimental GameRender indexed frame resolve" OFF)
    option(PXA_RECROF_BULK_READ_COPY "Use WASM memory.copy for large file/cache reads" OFF)
    option(PXA_RECROF_PACKED_GAME_CODEC "Use layout-proven bulk encoding of GameRender commands" OFF)
    cmake_parse_arguments(ENGINE "RTS" "TITLE" "SOURCES;INCLUDES;DEFINITIONS" ${ARGN})
    find_program(PXA_WASM_OPT wasm-opt HINTS "$ENV{PXA_BINARYEN_DIR}/bin"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.tools/binaryen-version_130/bin"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../../recrof-ports-tools/binaryen-version_130/bin")
    if(NOT PXA_WASM_OPT)
        message(FATAL_ERROR "Binaryen 130 wasm-opt is required; set PXA_BINARYEN_DIR or run common/recrof/tools/setup.sh")
    endif()
    set_source_files_properties("${PXA_RECROF_DIR}/stack.S" PROPERTIES LANGUAGE C
        COMPILE_FLAGS "-x assembler-with-cpp")
    # Protocol parsing is cold relative to the software renderer. O3's header
    # inlining bloats the event callback and its PSRAM instruction working set.
    set_source_files_properties("${PXA_RECROF_DIR}/runtime.c"
        "${PXA_RECROF_DIR}/filesystem.c" PROPERTIES COMPILE_OPTIONS -Os)
    pxa_add_component(pxa_main COMPONENT_ID main NO_TRANSLATIONS
        SOURCES ${ENGINE_SOURCES} "${PXA_RECROF_DIR}/runtime.c"
        "${PXA_RECROF_DIR}/filesystem.c" "${PXA_RECROF_DIR}/font8x8.c"
        "${PXA_RECROF_DIR}/external.c"
        "${PXA_RECROF_DIR}/controller.c"
        "${PXA_RECROF_DIR}/framebuffer.c" "${PXA_RECROF_DIR}/stack.S"
        INCLUDE_DIRS ${ENGINE_INCLUDES} "${PXA_RECROF_DIR}" "${PXA_RECROF_DIR}/compat"
        DEFINITIONS ${ENGINE_DEFINITIONS} "PORT_TITLE=\"${ENGINE_TITLE}\"")
    if(PXA_RECROF_HOST_BLIT)
        target_compile_definitions(pxa_main PRIVATE PXA_GAME_RENDER_BLIT=1)
    endif()
    if(PXA_RECROF_NATIVE_PCM)
        if(NOT PXA_RECROF_MAIN_LOOP)
            message(FATAL_ERROR "PXA_RECROF_NATIVE_PCM requires PXA_RECROF_MAIN_LOOP")
        endif()
        target_compile_definitions(pxa_main PRIVATE PORT_NATIVE_PCM=1)
    endif()
    if(PXA_RECROF_MAIN_LOOP)
        target_compile_definitions(pxa_main PRIVATE PORT_NATIVE_MAIN_LOOP=1)
    endif()
    if(PXA_RECROF_BULK_READ_COPY)
        target_compile_definitions(pxa_main PRIVATE PXA_RECROF_BULK_READ_COPY=1)
    endif()
    if(PXA_RECROF_PACKED_GAME_CODEC)
        target_compile_definitions(pxa_main PRIVATE PXA_GAME_RENDER_WASM_PACKED_CODEC=1)
    endif()
    if(ENGINE_RTS)
        target_compile_definitions(pxa_main PRIVATE PORT_RTS=1)
        target_compile_options(pxa_main PRIVATE -Os)
        set_source_files_properties("${PXA_RECROF_DIR}/framebuffer.c"
            PROPERTIES COMPILE_OPTIONS -O3)
        set(asyncify_script "${PXA_RECROF_DIR}/tools/asyncify_rts.py")
    endif()
    target_compile_options(pxa_main PRIVATE -w -fsigned-char -fno-strict-aliasing
        -Wno-error=int-conversion -Wno-error=incompatible-pointer-types
        -Wno-error=implicit-function-declaration -Wno-error=implicit-int
        -Wno-error=return-type -fno-optimize-sibling-calls
        "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti;-Wno-narrowing>")
    file(READ "${PXA_GUEST_SDK_DIR}/pxa-imports.txt" imports)
    if(PXA_RECROF_MAIN_LOOP)
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/port-imports.txt" "${imports}\n")
    else()
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/port-imports.txt"
            "${imports}\nunwind\nstop_unwind\nrewind_stack\nstop_rewind\n")
    endif()
    if(NOT DEFINED PXA_RECROF_ENGINE_STACK_BYTES)
        set(PXA_RECROF_ENGINE_STACK_BYTES 262144)
    endif()
    if(NOT DEFINED PXA_RECROF_CALLBACK_STACK_BYTES)
        set(PXA_RECROF_CALLBACK_STACK_BYTES 262144)
    endif()
    foreach(bytes IN ITEMS PXA_RECROF_ENGINE_STACK_BYTES PXA_RECROF_CALLBACK_STACK_BYTES)
        math(EXPR remainder "${${bytes}} % 16")
        if(${${bytes}} LESS 16384 OR NOT remainder EQUAL 0)
            message(FATAL_ERROR "${bytes} must be at least 16384 and aligned to 16")
        endif()
    endforeach()
    math(EXPR total_stack "${PXA_RECROF_ENGINE_STACK_BYTES} + ${PXA_RECROF_CALLBACK_STACK_BYTES}")
    target_compile_definitions(pxa_main PRIVATE
        PORT_ENGINE_STACK_BYTES=${PXA_RECROF_ENGINE_STACK_BYTES}
        PORT_CALLBACK_STACK_BYTES=${PXA_RECROF_CALLBACK_STACK_BYTES})
    target_link_options(pxa_main PRIVATE -Wl,-z,stack-size=${total_stack}
        "-Wl,--allow-undefined-file=${CMAKE_CURRENT_BINARY_DIR}/port-imports.txt")
    foreach(fn fopen fclose fread fwrite fseek ftell rewind fflush feof ferror clearerr
        fgetc getc fputc putc fgets fputs fprintf printf puts putchar vprintf open close read write lseek
        stat access mkdir unlink remove rename opendir closedir readdir getcwd chdir fileno
        setvbuf nanosleep usleep sleep exit _Exit abort getenv perror system __wasi_fd_fdstat_get __wasi_environ_sizes_get __wasi_environ_get)
        target_link_options(pxa_main PRIVATE "-Wl,--wrap=${fn}")
    endforeach()
    if(NOT ENGINE_RTS)
        set(asyncify_script "${PXA_RECROF_DIR}/tools/asyncify.py")
    endif()
    if(PXA_RECROF_MAIN_LOOP)
        # Keep the same optimization policy as the callback build while
        # removing continuation instrumentation and its entire import surface.
        add_custom_command(TARGET pxa_main POST_BUILD
            COMMAND "${PXA_WASM_OPT}" "$<TARGET_FILE:pxa_main>"
                -O2 --always-inline-max-function-size=0
                --one-caller-inline-max-function-size=0 --flexible-inline-max-function-size=0
                -g --enable-bulk-memory --enable-sign-ext
                --enable-nontrapping-float-to-int --enable-mutable-globals
                -o "$<TARGET_FILE:pxa_main>.native"
            COMMAND "${CMAKE_COMMAND}" -E rename
                "$<TARGET_FILE:pxa_main>.native" "$<TARGET_FILE:pxa_main>"
            VERBATIM)
        return()
    endif()
    set_property(TARGET pxa_main APPEND PROPERTY LINK_DEPENDS "${asyncify_script}")
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    get_filename_component(wasi_bin "${CMAKE_C_COMPILER}" DIRECTORY)
    get_filename_component(wasi_root "${wasi_bin}" DIRECTORY)
    add_custom_command(TARGET pxa_main POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${asyncify_script}"
            "$<TARGET_FILE:pxa_main>" "${PXA_WASM_OPT}" "${wasi_bin}/llvm-nm"
            "${wasi_root}/share/wasi-sysroot/lib/wasm32-wasip1/libc.a"
        VERBATIM)
endfunction()
