# 摇摇骰子

独立 PXA 游戏，包 ID `pxa-shake-dice`。启动直接进入骰盅，只有骰子玩法。

- 可选择1～6颗六面骰子，真实立体翻滚和落定点数；轻摇设备、确认或触摸骰盅均可掷点。
- 显示实际点数合计，保存最近8次的每颗点数和累计次数。切换数量仅改变下一次投掷，不改写已有结果。结果、数量和摇动开关可在重启后恢复。
- 三按键使用↑↓选择底部动作、确认执行；投掷完成后自动回到“摇一摇”。“更多”提供记录、摇动开关和退出；记录可翻页。触屏可直接点击骰盅、按钮。
- 联网与SD卡均不需要。开启摇动时按公开 Sensor 接口请求加速度与角速度权限；连续摇动经过冷却检测，角速度参与翻滚幅度。菜单、记录页与后台释放订阅，后台落定正在进行的投掷并保存一次。
- 小屏、横屏、大屏触摸设备按实际空间调整骰子排列；使用原生 GameRender 和随包字形图集。

在 `pxa-projects` 构建：

```sh
tools/app.sh build shake-dice --board doit-gamekit-1 --target esp32s3 --aot-only \
  --source-root "$PWD/local/pxa-apps" --output "$PWD/local/app-output/shake-dice"
```

模拟器使用 `--target simulator`。资源生成与可复用测试见 [共用支持代码](../common/motion-games/README.md)。代码、原创图标及 PXA SDK 使用 MIT；字形使用 SIL OFL 1.1，完整声明随包保存在 `assets/NOTICE.txt`。
