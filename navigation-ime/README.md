# 基础拼音键盘

为 Flash 空间有限的 PXA 设备提供全键盘英文和拼音逐字输入。密码框直接提交字符，不生成拼音候选。通过系统输入法设置选择后，供原生输入框使用。数字和符号面板由系统键盘提供。若系统默认九键，请在键盘设置中切换到全键。

输入法组件使用固定大小的工作区；字表复用相邻 `pinyin-ime-lite/engine.hpp` 及其生成字表。自行移动源码时需一并保留该依赖。许可证及上游声明完整保存在 `assets/LICENSE.txt`。

在工作区根目录构建：

```sh
tools/app.sh build navigation-ime --target esp32s3 --aot-only
```

`tests/provider_test.cpp` 覆盖英文、密码、拼音选字、分页、退格和提交事件。
