set(EMU_SYSTEM pce)
set(EMU_PCE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/pce-go")
set(EMU_ENGINE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/core.c"
    "${EMU_PCE}/pce-go.c" "${EMU_PCE}/pce.c" "${EMU_PCE}/h6280.c"
    "${EMU_PCE}/gfx.c" "${EMU_PCE}/psg.c")
set(EMU_ENGINE_INCLUDES "${EMU_PCE}")
set(EMU_ENGINE_DEFINITIONS)
