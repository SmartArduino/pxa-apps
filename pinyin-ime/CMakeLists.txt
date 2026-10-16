cmake_minimum_required(VERSION 3.20)
project(pxa_pinyin_ime LANGUAGES C CXX)
list(APPEND CMAKE_MODULE_PATH "${PXA_CMAKE_MODULE_DIR}")
include(PxaGuest)
pxa_add_app(pxa_main COMPONENT_ID main SOURCES main.cpp)
set(decoder_sources dictio dictlist dicttrie lpicache matrixsearch mystdlib ngram
    pinyinime searchutility spellingtable spellingtrie splparser sync userdict utf16char)
set(decoder_files)
foreach(name IN LISTS decoder_sources)
    list(APPEND decoder_files "third_party/libgooglepinyin/src/${name}.cpp")
endforeach()
pxa_add_component(pxa_provider CPP COMPONENT_ID provider SOURCES
    provider.cpp port/dictionary.cpp core/ime_engine_port.cpp core/ime_t9.c core/ime_trie.c generated/ime_t9_syllables.c
    ${decoder_files})
target_include_directories(pxa_provider PRIVATE . include third_party/libgooglepinyin/include)
# This read-only Provider has no writable user-dictionary capability. Avoid
# constructing a UserDict only to fail its empty-path open; other lib users
# can omit SYSTEM_DICT_ONLY when they implement persistence.
target_compile_definitions(pxa_provider PRIVATE PINYINIME_RUNTIME_ONLY PINYINIME_SYSTEM_DICT_ONLY)
# The shipped model already grows to 19 pages after initialization. Start only
# this on-demand Component at that measured size to avoid ESP realloc copies
# during decoding. This neither pins the 4 MiB limit nor enlarges the UI app.
target_link_options(pxa_provider PRIVATE -Wl,--initial-memory=1245184)
set_source_files_properties(${decoder_files} PROPERTIES COMPILE_OPTIONS
    "-include;${CMAKE_CURRENT_SOURCE_DIR}/port/guest_stdio.h;-w")
