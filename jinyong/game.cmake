# SPDX-License-Identifier: MIT
# Full real engine, not a replacement rules demo. Generated files preserve the
# pinned upstream and the explicit portability patches beside this file.
set(JY_ENGINE_NAMES charset jymain luafun mainmap piccache sdlfun)
set(JY_ENGINE_SOURCES)
foreach(name IN LISTS JY_ENGINE_NAMES)
  list(APPEND JY_ENGINE_SOURCES "${JY_WORK}/src/${name}.c")
endforeach()
file(GLOB JY_LUA_SOURCES CONFIGURE_DEPENDS "${JY_WORK}/lua/*.c")
list(FILTER JY_LUA_SOURCES EXCLUDE REGEX "/(lua|luac|print)\\.c$")
set(JY_ZLIB_NAMES adler32 crc32 deflate inflate inffast inftrees trees zutil compress uncompr)
set(JY_ZLIB_SOURCES)
foreach(name IN LISTS JY_ZLIB_NAMES)
  list(APPEND JY_ZLIB_SOURCES "${CMAKE_CURRENT_LIST_DIR}/deps/zlib/${name}.c")
endforeach()
set(JY_ASYNCIFY_REMOVE "pxa_app_start,pxa_app_on_event,pxa_app_stop,jy_front_drive,jy_host_dispatch,jy_coroutine_begin,jy_coroutine_resume,jy_coroutine_cancel,jy_drive,jy_stack_enter,jy_stack_pointer")
