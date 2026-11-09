# 随行地图

PXA C++ 应用，采用全屏地图、悬浮搜索、底部路线卡片和独立转向提示。
支持高德地点搜索、步行/骑行/驾车算路、地图拖动与缩放、分步路书、
位置跟随、连续偏航后重新规划与到达判断。未配置 Key 时可直接体验模拟导航。
模拟道路与位置始终带有明确标识，不代表实际可通行路线。

## 使用

设备需要选定系统输入法才能填写设置和搜索。若尚未安装，可使用同仓库的
`navigation-ime` 基础拼音键盘，在系统输入法设置中选中并使用全键布局。

1. 首页直接点击「模拟导航」体验地图、位置移动和转向提示。
2. 在高德开放平台创建 **Web 服务**类型 Key，点击地图右上角设置按钮，
   在「地图服务」中输入，在底部保存。「搜索城市」可选；有同名地点时应
   填写城市并在搜索结果中确认具体地点。「定位来源」可切换 Wi-Fi 和网络转发。
3. 点击顶部搜索栏选择目的地，或点击底部「路线」选择起点和终点。
   路线规划页支持使用当前位置出发，也可搜索 `经度,纬度`（GCJ-02）。
4. 点击出行方式切换步行、骑行、驾车，规划路线后点击「开始导航」。
5. 切到后台会停止定位和导航；返回后需手动重新开始。

拖动地图可浏览周边，松手后加载底图；右侧加减按钮缩放，定位按钮回到最近
有效位置。路线卡片中的路线图标恢复全览，路书图标查看每段行程。
导航时，小屏点击底部的「路书 ›」进入路书；返回键先退出路书，再结束导航。
结束导航后保留路线，可再次开始；到达后可重新规划，模拟行程可点击「再走一次」。

Gen4 等三键设备可用上／下键循环切换焦点，确认键执行选中操作。蓝色焦点框
标明当前控件；搜索结果会随焦点自动滚动。输入框按确认键打开系统键盘，在
键盘中选择「完成」返回应用。每页都有可聚焦的返回按钮，首页左上角可退出；
不需要额外的返回键。地图缩放、定位、路线规划、路书与设置均支持此方式。
触摸设备仍可直接点击控件和拖动地图。

界面使用与轻量应用商城相近的米白与深绿配色，按显示密度适配，并避开系统安全区。
可用高度不足 400dp 时使用紧凑卡片，较高的屏幕显示更多说明和路书内容。模拟导航始终标注「示意地图」与
「模拟导航」；未能下载真实底图时不会用示意道路冒充真实地图。

地图与算路直接使用官方 HTTPS 接口，不使用未公开的高德瓦片地址。
Key 保存在 PXA 应用私有 KV 中，不写入包内。旧固件可能记录请求 URL，应使用不记录完整 URL 的固件；私有 KV 不等同于加密存储。
高德的免费配额、使用条件与服务授权以实际账号控制台为准。

## 指南针

在设置页右上角打开指南针。设备有可用方位来源时，地图也会显示指北针入口，
支持触摸和三键操作。系统绝对方位优先使用 `orientation.heading`。原始磁场使用
`magnetic-field.0` 或 `.1`，配合 `motion.acceleration` 做倾斜补偿，
`motion.angular-velocity` 可提供陀螺仪辅助。固件需通过 PXA Sensor 服务公开这些
语义通道，应用按需申请对应 `sensor.read` 权限。

首次使用原始磁力计时选择「六面校准」，远离磁铁和金属，将设备正反、上下、
左右缓慢翻转。覆盖六个面后继续转动，显示采样合格时保存。取消保留原校准。
两个磁力计分别保存校准，所选来源会保存到本机。系统方位模式由宿主负责校准。
运动坐标与 Android 一致：X 向原生右沿、Y 向原生上沿、Z 向屏幕外。
原始磁力计模式使用新版 `CMP2` 标定；旧坐标的标定不读取，更新后需重新校准。
方向沿用设备物理上沿参考，竖握时平滑过渡到机背朝向，读数以磁北为基准。

只有陀螺仪时，页面会提示缺少指北来源。方位或配对磁场、重力数据超过
1.5 秒未更新时隐藏读数。进入后台会释放传感器订阅，返回后重新检测。
指南针计算和校准复用同仓库 `compass/model.hpp`，按其 MIT 许可使用。

## 定位来源

**免费 Wi-Fi 定位**：需要固件的 `org.doit.wifi-location` 0.1 扩展及 `wifi.scan`
权限。工作区的 pai-touch 固件可编入该扩展；旧固件、其他固件和标准模拟器可能
没有它，应用会提示更新固件或使用网络转发。扩展返回附近热点 BSSID、RSSI 和
实际扫描时间，不暴露密码。用户获取位置或开始导航后才扫描与请求网络定位。
应用向 [beaconDB](https://beacondb.net/) 发送最多十个热点的 BSSID、SSID 与 RSSI、
禁用 IP 回退，并标明
客户端 User-Agent。beaconDB 是实验服务；没有覆盖时定位失败，不以城市中心
冒充当前位置。无定位 Key 费用不等于导航精度或服务可用性保证。

导航期间 Wi-Fi 通常每 15 秒查询一次；网络位置转发每秒查询一次。首次扫描进行中会自动重试，手动与周期定位均不会锁住地图操作。Wi-Fi 更新频率与
精度不足以替代所有场景下的 GNSS，特别是高速行驶和密集路口。

**网络位置转发 / GPS 接入**：设置一个可以 GET 的 HTTP(S) URL，返回：

```json
{"lat":22.540000,"lng":114.050000,"accuracy":10,"age_ms":500,"datum":"wgs84","sample_id":"sensor-session:42"}
```

`accuracy` 为误差半径（米），`age_ms` 为服务端获取该测量后经过的毫秒数，必须
按测量时间计算，不能按每次请求重置。可选的 `sample_id` 应标识一次实际测量，
重复转发同一次测量时保持不变；应用会过滤重复标识和网络延迟造成的近似重复时间。`datum` 只接受 `wgs84` 或 `gcj02`。
WGS84 通过高德官方坐标转换接口转换后再绘制。手机或未来 GPS 适配器可提供
此接口；应用内部统一通过 `Fix` 更新导航，因此无需更改路线匹配算法。
此版本尚不包含板载 UART GPS 驱动或手机配套程序。HTTP 仅建议用于可信局域网。

超过 30 秒、重复时间戳、未知精度或误差大于 80 米的位置不会推进路线或触发
偏航重规划。尚未确认的偏航保留原路线剩余距离；连续三次有效位置离路线过远才确认偏航；自动重新算路间隔至少
60 秒。到达需两次误差不超过 25 米的有效测量，同时接近路线终点。上述阈值
是首版应用策略，不构成实际定位精度的承诺。

## 电脑 Wi-Fi 转发

`tools/wifi_bridge.py` 使用 Linux NetworkManager 实际扫描附近热点，并向 beaconDB
查询位置，供同一局域网内、尚未安装扫描扩展的设备使用。它表示电脑的位置；
电脑应与设备处于同一地点。需要 `nmcli` 与 Python 3，无第三方 Python 依赖。
不会用 IP 城市位置填补缺失覆盖；无覆盖时返回 HTTP 503。测量标识不变时，
读取不会重置位置的年龄。热点名称和 BSSID 不写入运行日志。

```sh
artifact_dir=local/artifacts/navigation-bridge/$(date +%Y%m%d-%H%M%S)
mkdir -p "$artifact_dir"
python3 local/pxa-apps/navigation/tools/wifi_bridge.py --interface wlp1s0 \
  --bind 192.168.1.10 --token-file "$artifact_dir/token" --endpoint-file "$artifact_dir/endpoint.txt"
```

将绑定地址替换为电脑的局域网 IP。在应用「定位来源」中选择网络转发，填入
`endpoint.txt` 中的地址并保存。令牌文件权限为 0600，服务只提供带令牌的读取
接口，不开放上传或目录浏览；仅用于可信局域网。停止工具后设备会提示转发不可用。
`--once` 可只查询一次；没有可用位置时返回非零退出码。

## 构建与检查

在 PXA ESP 工作区根目录执行，保留独立应用仓库的 `common/`：

```sh
# Flash 空间较小的设备使用单架构包。
tools/app.sh build navigation --target esp32s3 --aot-only --output local/app-output/navigation-device
# 模拟器另行构建，避免把 x86_64 代码装入设备。
tools/app.sh build navigation --target simulator --aot-only --output local/app-output/navigation-simulator
tools/dev.sh sim navigation
```

应用通过独立的 Wi-Fi 扩展 SDK 编译，默认位置是工作区
`extensions/wifi-location/sdk`。扩展是可选能力，不写入应用的必需服务清单。
其他源码布局可通过 `PXA_WIFI_LOCATION_SDK` 指定其 SDK 目录。

`tests/model_test.cpp` 覆盖接口解析、路线匹配、精度与时效、偏航和到达判断；
`tests/camera_location_test.cpp` 检查缩放预览、位图与路线对齐、视野记录与测量去重；
`tests/design_test.cpp` 检查安全区布局、触控边界、文字截断和小屏路线取景；
`tests/simulator.py` 驱动真实产品模拟器，覆盖不同密度、导航、路书阅读、地图
拖动、原生输入和设置。`tests/http_simulator.py` 用本地接口夹具联调搜索、结果
滚动、算路、底图解码、手动与后台定位等待期间的操作、连续缩放、断网重试与视野恢复。截图均由真实模拟器生成。测试输出放在工作区
`local/artifacts/navigation/<时间戳>/`，不纳入应用源码或商城素材。

`PXA_NAV_TEST_ORIGIN` 仅用于将高德请求指向测试 HTTP 服务。设置该变量生成
的包不得发布为正式包；正式构建需移除该变量，默认地址为高德官方 HTTPS。

绘制按需提交，连续事件合并到下一帧；不变的画面不重复刷新。移动位置时使用局部
更新，路线绘制点按缩放级别缓存。周期位置请求在后台执行，保持地图可拖动。
启动先显示可操作界面，再异步下载底图；网络速度不计入本地界面初始化。
底图按物理显示像素请求，拖动后的更新在后台合并；新手势会淘汰过时的请求结果。
缩放立即预览旧底图并同步重绘路线，下载完成后替换清晰底图；连续点击会合并到
最后的视野。失败时可点击重试。真实地图的中心与缩放在操作停顿后保存，重开
恢复最近视野；模拟地图不写入该记录。

当前限制：最多 128 段、4096 坐标点；超限需分段规划。地图为按需更新的静态
底图与本地路线叠加，拖动后按需请求新底图，暂不提供多点缩放、3D 地图、
实时路况或语音播报。Wi-Fi 定位需要兼容固件和当地服务覆盖；GPS 适配器需要另行接入。

接口文档：[路线规划](https://developer.amap.com/api/webservice/guide/api/newroute)、
[静态地图](https://developer.amap.com/api/webservice/guide/api/staticmaps)、
[坐标转换](https://developer.amap.com/api/webservice/guide/api/convert)。

应用自有源码为 MIT；Wi-Fi 扩展 SDK 为 Apache-2.0；地图与服务数据遵守各提供方的条款。

三键回归使用 Gen4 的实际 `Mz79Navigation`，在独立模拟器中验证焦点循环、
五条搜索结果的滚动选择、算路、缩放、路书、系统键盘与退出。测试 Host 使用
内置英文解码器，正式包仍使用设备选择的系统输入法。所有设置和地图响应均为
测试数据，输出目录应位于 `local/artifacts/`：

```sh
artifact_dir=local/artifacts/navigation-keys/$(date +%Y%m%d-%H%M%S)
python3 local/pxa-apps/navigation/tests/build_gen4_simulator.py \
  --workspace "$PWD" --gen4 /path/to/mz79-firmware --output "$artifact_dir/runner"
PXA_NAV_TEST_ORIGIN=http://127.0.0.1:18787 tools/app.sh build navigation \
  --target simulator --aot-only --output "$artifact_dir/fixture"
python3 local/pxa-apps/navigation/tests/keys_simulator.py \
  --package "$artifact_dir/fixture/pxa-navigation" \
  --runner "$artifact_dir/runner/pxsys_product_simulator" --output "$artifact_dir/keys"
```
