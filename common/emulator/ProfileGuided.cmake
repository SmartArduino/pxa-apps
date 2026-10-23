# Optional Clang instrumentation profiles. No instrumentation in Guest builds.
option(EMU_PGO_GENERATE "Collect native core execution counts" OFF)
set(EMU_PGO_PROFILE "" CACHE FILEPATH "LLVM indexed profile for this core")

if(EMU_NATIVE_TEST)
    set(emu_pgo_target emu_core)
    set(emu_pgo_profile "${EMU_PGO_PROFILE}")
else()
    set(emu_pgo_target pxa_main)
    # Read on every package configure, including when the variable is unset.
    # A cached build must not silently retain the previous package's profile.
    set(emu_pgo_profile "$ENV{PXA_EMU_PGO_PROFILE}")
    if(EMU_PGO_GENERATE)
        message(FATAL_ERROR "Collect profiles with EMU_NATIVE_TEST, not in a Guest package")
    endif()
endif()

if(EMU_PGO_GENERATE OR emu_pgo_profile)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "Emulator instrumentation profiles require Clang")
    endif()
    if(EMU_PGO_GENERATE AND emu_pgo_profile)
        message(FATAL_ERROR "Choose profile collection or profile use, not both")
    endif()
endif()

if(EMU_PGO_GENERATE)
    target_compile_options(${emu_pgo_target} PRIVATE -fprofile-instr-generate)
    target_link_options(${emu_pgo_target} INTERFACE -fprofile-instr-generate)
elseif(emu_pgo_profile)
    get_filename_component(emu_pgo_profile "${emu_pgo_profile}" ABSOLUTE)
    if(NOT EXISTS "${emu_pgo_profile}" OR IS_DIRECTORY "${emu_pgo_profile}")
        message(FATAL_ERROR "Profile does not exist: ${emu_pgo_profile}")
    endif()
    # The filename in the compile command changes when profile bytes change,
    # even if the caller replaced the file at the same path. This invalidates
    # cached objects; ordinary incremental builds retain them.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${emu_pgo_profile}")
    file(SHA256 "${emu_pgo_profile}" emu_pgo_sha256)
    set(emu_pgo_copy "${CMAKE_CURRENT_BINARY_DIR}/profiles/${emu_pgo_sha256}.profdata")
    configure_file("${emu_pgo_profile}" "${emu_pgo_copy}" COPYONLY)
    target_compile_options(${emu_pgo_target} PRIVATE
        "-fprofile-instr-use=${emu_pgo_copy}" -Werror=profile-instr-out-of-date)
    message(STATUS "Emulator profile SHA256: ${emu_pgo_sha256}")
endif()
