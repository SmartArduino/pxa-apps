# SDL guest game runtime

SDL2 software surfaces/renderers, SDL_mixer WAV/Vorbis, image decoding and optional software GL/font rendering live in each application guest. The firmware/Host does not link them. The bridge uses the PXA native main loop, pinned RGB565 mapped surfaces, controller events, app-private filesystem, signed resource reads and PCM playback. Building requires SDK 2.1 with the archive resource compiler.

`Build.cmake` compiles SDL without OS threads, GPU/window dependencies, ambient filesystem or networking. SDL's actual virtual controller supports existing game menus and gameplay. Touch overlays use the same controls and hide after controller input. The first touch after controller use reveals the controls and is consumed; subsequent touches operate buttons or the game UI. `SDL_Delay` and event waits yield cooperatively while servicing PCM. Desktop framebuffer calls are not measurements of hardware LCD completion.

`media/` becomes one deterministic uncompressed USTAR Blob (`assets/data.tar`); only its path/offset directory index enters WASM. Reads stream through the bounded guest cache and do not expand the whole archive into RAM. Application writes override packaged paths inside the private `/data` namespace. The archive is a resource container, not a writable mounted filesystem; no extraction step or arbitrary host paths are needed.

Audio first attempts native S16 PCM. Hosts exposing only the graph/session API use a mono resampling fallback; backend state reports actual accepted samples separately from decoded frames. Sound settings remain in each game's own configuration. SDL timer/event wrappers must avoid calling the public wrapped entry points recursively.

Neverputt additionally uses the MIT PortableGL programmable rasterizer with a limited fixed-function adapter in `gl.c` and cached stb_truetype labels. Its profile uses RGB565/D16, RGBA textures, alpha blending, matrix stacks, lighting and VBOs. FBO/shader/HMD/mirror/clip-plane features are not advertised; packaged courses use the basic ball and disabled reflections/shadows. 3D worlds can use a half-size viewport and an in-place RGB565 resolve before drawing the full-resolution HUD; this needs no second scene buffer. The pinned PortableGL source includes a documented RGB565 blend correction.

Source revisions and local adaptations are recorded in `vendor/*/UPSTREAM.json`; original copyright/license files are retained. The guest bridge is GPL-3.0-or-later. These dependencies must not be linked into the system or Host.

Regression tools are in `../../tests/sdl-games`: `gl_adapter.sh` verifies colors, alpha and texturing; `simulator.py` runs the actual PXA product simulator, injects controls, checks fatal errors/PCM and retains raw screenshots in an explicitly selected ignored directory. Review gameplay screenshots as well as the script result.
