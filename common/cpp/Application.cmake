include_guard(GLOBAL)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)

# Manual callback components use the payload-only C++ SDK without reserving a
# Context, coroutine pool or request table. Existing wire codecs and message
# IDs remain source-compatible; each application owns its bounded state.
function(pxa_add_callback_component target)
    cmake_parse_arguments(APP "ALLOCATOR" "COMPONENT_ID" "SOURCES" ${ARGN})
    if(APP_UNPARSED_ARGUMENTS OR NOT APP_COMPONENT_ID OR NOT APP_SOURCES)
        message(FATAL_ERROR "pxa_add_callback_component requires COMPONENT_ID and SOURCES")
    endif()
    set(_allocator_option NO_ALLOCATOR)
    if(APP_ALLOCATOR)
        set(_allocator_option)
    endif()
    pxa_add_component(${target} CPP NO_TRANSLATIONS ${_allocator_option}
        COMPONENT_ID "${APP_COMPONENT_ID}" SOURCES ${APP_SOURCES}
        INCLUDE_DIRS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
    target_compile_definitions(${target} PRIVATE PXA_GUEST_CPP_TRANSPORT=1)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/i18n/messages.yaml")
        pxa_add_translations(${target} LANGUAGE c SOURCE i18n/messages.yaml)
    endif()
endfunction()
