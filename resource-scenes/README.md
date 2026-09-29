# Resource Scenes

An actual Core v1 application demonstrating file resources and atomic bindings.
Twenty 256×256 source PNGs are compiled to PXR INDEX8 files, with one RGB565
palette. Each scene displays four textures. The Guest contains no texture
pixel arrays, palette arrays, decoder buffers, or per-frame uploads.

The app queries its palette's cache state (Assets 1.1), then loads the palette
and prepares its required map before loading four textures asynchronously.
This avoids pinning a full scene while waiting for extra map verification
memory under a tight global budget. Music and sound preparation can overlap
the map phase; texture loading and scene switches run with music playing.
`pxa_asset_scene.h` tracks the four texture
requests and their handles in caller-owned storage. Once ready, it
binds the texture batch and closes the Guest handles; the render context owns
its own references. After four clock ticks it unbinds the old textures,
submits a clear loading frame, and loads the next scene. It completes 100
scenes, closes its render context, and logs completion. The background worker
and final cache reclamation are owned by the Host.

Each visible scene optionally prefetches the first texture of the next scene.
Prefetch completion creates no Guest handle and leaves an evictable cache entry;
normal LOAD obtains handles before the next binding. Low memory can reject the
optional warming without failing the scene. Normal scene release cancels its
pending requests and drains late results. At component stop, Core revokes all
requests/handles before the Guest stop callback, where imports are forbidden.
The example does not poll status
every frame or create a separate request scheduler.

The source PNGs and resource manifest are reproducible with
`python3 generate_resources.py`. Normal packaging uses the checked-in PNGs;
Pillow is needed by the existing offline texture compiler. Regeneration is
not part of runtime startup.

From the workspace root:

```sh
./tools/dev.sh sim --board pai-touch resource-scenes
```

For automated verification, build the package with the normal package tool,
then build and run the simulator's `pxsys_resources_test` target:

```sh
cmake -S deps/pxa-system/simulator/desktop -B build/simulator/pai-touch
cmake --build build/simulator/pai-touch --target pxsys_resources_test -j4
build/simulator/pai-touch/pxsys_resources_test \
  /absolute/path/to/pxa-resource-scenes \
  "$PWD/local/pxa-apps/.dev-signing/publisher-public.der"
```

The test uses dummy SDL, real WAMR/AOT, a 512 KiB texture-object budget, and
1 ms injected delay per resource read. It checks pixels from all four textures
in every scene and exits normally after the app closes its context. Cache
metadata, thread stacks, framebuffer storage, AOT storage and Guest memory are
additional allocations, not included in the 512 KiB number.

The integration test requires 99 accepted prefetch requests, checks that any
load failures were optional prefetch failures, and validates all 100 scenes.
Run with `PXA_RESOURCE_EXTERNAL_BYTES=300000` to demonstrate optional warming
failure and foreground recovery under the global shared budget.

Append `exit-loading` to the test command to stop the actual app while its first
texture is loading. This mode defaults to 20 ms per read, observes active
external loading after the loading frame, and verifies normal shutdown with
zero remaining shared-budget charges. It does not wait for a scene to finish.

The helper holds one handle per item until the atomic bind, even with a smaller
request window. Host handle quotas still apply. It releases only Guest handles;
the application explicitly unbinds textures before retiring a scene. Failed
groups drain before the sample's bounded retry on a later clock tick.

Current scope includes desktop/ESP file resources, prefetch, status and the
scene SDK plus prepared PCM effects and looping Ogg music. Full UI decoding
budgets and shared-storage underrun/recovery validation remain part of the
parent refactor goal. This sample does not claim those parts
are finished or predict ESP playback/rendering speed.


Assets 1.2 的地图示例使用 `assets/map.bin`，由 `generate_resources.py` 可复现生成
8193 字节的原始数据；这些字节不会编译进 AOT。Guest 使用完整 64 位 token 异步读取，
逐事件检查内容，按实际长度推进并显式验证 EOF，不保留地图数组或事件内指针。
单次最多 4064 字节，结果含信封/状态/偏移后最多 4096 字节，按请求范围直接读取，不做运行时 SHA。
读取与纹理加载共用 Host worker。正常模式完成地图后继续 100 次场景切换；
产品测试的 `exit-reading` 模式在读取结果缓冲已分配且 I/O 注入延迟期间退出，验证回收。

Assets 1.3 / Audio 0.7 示例异步加载 `assets/click.pcm`，每个场景准备完成后按句柄
触发一次音效，共 100 次；最后再播放一次并立即关闭 Guest 音效句柄，检查
消费者引用清理。播放不读取文件、不把 PCM 放入 Guest 内存。`exit-sound-loading`
在最终 PCM 对象已分配、worker 仍在延迟读取时退出，检查取消、排空和预算归零。
同时循环播放 `assets/tone.ogg`，在匹配播放实例的 READY 后推进场景，结束时提交 STOP，
收到同一实例的 STOPPED 后关闭会话和上下文。这个 Ogg 是生成的 1 秒正弦音，循环
用于验证并发播放与通知；当前只对资源读取注入延迟，仍不代表共享存储欠载验收完成。
