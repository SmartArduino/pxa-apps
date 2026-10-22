# 有界 ROM 银行缓存

此模块属于模拟器应用层，复用 PXA FS `read-at`；不依赖具体 CPU、显示或文件系统实现。普通 CPU/PPU 读访问映射后的内存，换银行或访问尚未驻留的地址时才读文件。ROM 可大于空闲 RAM，SRAM、VRAM、工作 RAM 仍须实际分配。

`rom_source_t` 的读取回调必须填满指定范围并返回 0，失败返回非零；禁止保留输出指针。`size` 是可读范围，`checksum` 由流式加载器提供。格式转换可包装回调，不能改变源文件。NES 的身份校验沿用去掉 iNES 头和 trainer 后的 CRC，其他前端沿用原文件 CRC。

| 内核 | 页大小 | 默认最大缓存 | 映射保护 |
| --- | ---: | ---: | --- |
| NES | 2 KiB | 256 KiB | 每个 CPU/PPU 映射独立引用，可共享同一页 |
| GB/GBC | 4 KiB | 256 KiB | 八个 CPU ROM 窗口；首次访问才填充对应银行子页 |
| SMS/GG | 4 KiB | 512 KiB | CPU 的各个 1 KiB 窗口引用对应缓存页 |
| PCE | 8 KiB | 512 KiB | 八个 MPR 窗口；SF2 变更后重新映射 |
| SNES | 4 KiB | 512 KiB | 指令取码、DMA、C4 分别锁定；HDMA 按扫描线复制最多四字节 |
| MD | 4 KiB | 512 KiB | FAME 取码；短 VDP DMA 直接读页，跨页只复制实际请求区间；SSF2 更新物理银行映射 |

GB/GBC 与五个新前端优先尝试整块驻留，并临时保留各核心的初始化空间；实际放不下时才启用缓存，避免能放进 RAM 的游戏承担分页成本。NES 保留 1 MiB 驻留阈值，兼顾较大 CHR-RAM mapper 的额外分配，分配失败也会进入分页路径。上表不包括页索引、引用表、CRC 表和小型格式转换缓冲。页表按文件页数分配，数据缓存不随 ROM 文件长度增长。

`rom_cache_get(cache, offset, owner)` 替换该 owner 的映射并返回页内指针。活动 owner 的页不得淘汰；`rom_cache_unpin()` 释放引用。同一页可被多个 owner 共享，不能按银行号代替 owner。`UINT32_MAX` 返回临时指针，只能使用到下一次可能缺页的访问之前。调用者不得越过页边界使用指针，必须拆分访问或另行构造跨页指令窗口。

未锁定页按银行访问顺序淘汰。`hits/misses/evictions/bytes` 是累计计数；不在每次模拟读字节时更新 LRU。数据文件、缓存与读回调 context 在内核关闭前均须有效；关闭顺序为内核、缓存、源文件。PXA 前端保留独立的 ROM 句柄，加载电池存档不会覆盖它。

缺页失败或所有可用槽均被锁定时设置粘性错误，返回独立的只读 open-bus 页，不暴露未初始化或已淘汰内容。前端在帧边界检查错误并停机，禁止将错误帧状态写回已有存档；重开游戏会创建新的缓存。设备统计另记录 FS 的累计读取时间和最大单次读取耗时，便于区分内核负载与存储停顿。

`tests/paging_test.c` 位于各应用及公共前端测试目录，使用独立构造的银行数据检查映射、淘汰和锁定，并比较驻留与分页路径的 RGB565/PCM。NES 另检查分页存档回放，SNES 检查扩展映射和多通道 HDMA，MD 检查 SMD、SSF2 和 DMA。原生吞吐不能代替目标设备帧率；设备采样使用公共前端的 `tests/device_perf.py`，所有运行产物写入工作区忽略的 `local/artifacts/`。

私有 Flash 沿用旧版 `OPEN`，Host 的 FS 服务在同一 owner 调用内保存位置、seek/read、恢复位置；读取失败也恢复游标。SD 的 scoped 句柄继续使用原生 `read_at`，不增加该回退开销。旧 Host 不支持私有句柄定位读取时，Flash 的整块驻留仍可运行，大 ROM 分页需升级 Host。

`tests/pxa_read_at_probe.c` 是独立测试身份使用的设备探针，需要 `fs` 和 `log` 服务；它仅在自身私有目录创建 4 KiB 文件，检查定位读取、短读、取消和顺序游标保持。成功日志为 `ROM PRIVATE READ_AT PROBE PASS`。不要借用已有游戏身份运行该探针。

`tests/pxa_read_at_benchmark.c` 用于独立测试身份的 SD 读取分析，需要 FS `app-media-root`、`read-at` 及 log 服务。在该测试 App 自己的 SD 媒体目录放置至少 8 MiB 的 `probe.bin`，程序以 ABBA 顺序交替把顺序游标置于文件末尾与开头，读取相同的 4 KiB 偏移集合，检查 CRC 与游标保持并记录耗时。此测量不能代替模拟器帧率测试。

ROM 文件仍须符合所在介质的剩余空间和应用配额；分页只降低运行内存，不能扩大 Flash 存储容量。
