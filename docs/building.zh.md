# 构建

1. 安装 [xmake](https://xmake.io/zh/)、clang-cl 与 VS2022+

2. 构建模组

```bash
# 服务端（BDS）
xmake f -y -p windows -a x64 -m release --target_type=server
xmake

# 客户端（GDK / LeviLamina 客户端）
xmake f -y -p windows -a x64 -m release --target_type=client
xmake
```

# 项目结构

| 路径 | 内容 |
| --- | --- |
| `src/` | 模组主体：配置、指令、格式化、扩展信息构建、i18n。会编进**两端**的产物 |
| `src-server/` | BDS 侧：采样循环、文字通道、`/insight gui` 表单 |
| `src-client/` | GDK 客户端侧：覆盖层、面板绘制、物品图标、配置界面、本地存档读取 |
| `lang/` | 语言表（`en.json`、`zh_cn.json`） |
| `assets/` | 图标与 README 用到的截图 |
| `xmake.lua` | 一个 target、两种构建（`--target_type=server` / `client`） |

共享代码在 `src/` 里；只有某一端才能做的事用 `INSIGHT_TARGET_CLIENT` / `INSIGHT_TARGET_SERVER`
隔开，所以同一份 `src/` 能产出服务端插件与客户端模组两个产物。

参与开发前该看的构建要求、改动前要跑的检查、提交信息格式，都写在
[CONTRIBUTING.md](../CONTRIBUTING.md)。
