![Insight](https://socialify.git.ci/neverforward/Insight/image?custom_description=A+lightweight+info+display+mod+for+Bedrock+Edition+that+shows+you+exactly+what+you+are+looking+at.&description=1&font=Inter&forks=1&issues=1&logo=https%3A%2F%2Fraw.githubusercontent.com%2Fneverforward%2FInsight%2Frefs%2Fheads%2Fmain%2Fassets%2Ficon.svg&name=1&owner=1&pattern=Plus&pulls=1&stargazers=1&theme=Auto)

[![](https://img.shields.io/badge/English-informational?style=for-the-badge)
](README.md)![](https://img.shields.io/badge/简体中文-inactive?style=for-the-badge)
![GitHub License](https://img.shields.io/github/license/neverforward/Insight?style=for-the-badge)
 ![GitHub Tag](https://img.shields.io/github/v/tag/neverforward/Insight?style=for-the-badge)
![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/neverforward/Insight/build.yml?style=for-the-badge)
![GitHub commit activity](https://img.shields.io/github/commit-activity/y/neverforward/Insight?style=for-the-badge)

一个 Minecraft 基岩版（LeviLamina 26.20.*）的**信息显示**模组：实时显示玩家准星所指方块的信息（名称、类型、坐标、距离、容器内容、方块状态……）。

## 安装

- 服务端: `lip install github.com/neverforward/Insight`
- 客户端: `lip install github.com/neverforward/Insight#client`

首次启动会自动生成配置文件 `plugins/Insight/config/config.json`（服务端）或
`mods/Insight/config/config.json`（客户端），包含全部默认值。执行
`/insight reload`（需 OP）即可热重载；也可以直接在游戏里用 `/insight set <选项> <值>` 修改。

## 指令

服务端命令是 `/insight`，客户端是 `/cliinsight`。进入世界时客户端会把服务端的命令表
并进本地命令表、再让客户端模组叠加注册，所以客户端若也叫 `/insight`，在两端都装了 Insight 时会互相顶掉；
加 `cli` 前缀就能同时可用。
两边的子命令完全一致。

```
/insight toggle          开/关显示（服务端：你自己的开关，存在模组数据目录；客户端：总开关，写入配置文件）
/insight on | off        同上
/insight status          查看当前状态（开关/间隔/距离/频道，或锚点/显示/语言/extras）
/insight gui             打开配置界面（仅客户端；服务端暂无界面）
/insight reload          重新读取配置文件（需要 OP/管理员）
/insight set <选项> <值>   游戏内直接改配置并保存（需要 OP/管理员）
```

`<选项>` 输入时游戏会给出补全候选，就是配置里的那些名字（`format`、`maxWidth`、
`extras.chest`…）。取值写错会提示可用取值，例如：

```
/insight set channel actionbar
/insight set maxDistance 24
/insight set format {blockName} {x} {y} {z}
/insight set extras.chest false
```

服务端每位玩家的开关保存在模组数据目录下的键值数据库里（`plugins/Insight/data/players/`），
服务器重启后依然有效；只有在玩家把开关改成**非默认值**时才写入记录。

## 配置

### 配置界面（仅客户端）

客户端除了聊天栏指令，还可以用界面改配置：

- 用 `/cliinsight gui` 或快捷键打开（默认 <kbd>I</kbd>）；
- 另一个快捷键（默认 <kbd>K</kbd>）开关信息显示；
- 两个按键都会出现在游戏自带的按键设置里、可在那里改键——配置里的 `keyOpenConfig` /
  `keyToggleShow` 只是默认值（Windows 虚拟键码，`0` 表示不绑定）；
- 每行显示当前值，点击行展开内联编辑器；右列是面板的实时预览与外观设置；改动立即保存，
  底部会提示结果；
- 界面打开期间游戏不会收到键鼠输入。


```jsonc
{
    "version": 3,                  // 结构版本；升级模组时旧配置会自动合并，无需手动迁移

    "enabled": true,               // 总开关
    "enabledByDefault": true,      // 玩家默认开启；玩家可用 /insight 单独切换

    "maxDistance": 16.0,           // 射线最大距离（格）
    "intervalTicks": 4,            // 采样间隔（20 tick = 1 秒；4 = 0.2s）
    "passThroughLiquids": true,    // 视线是否穿透水/岩浆
    "showEmpty": false,            // 看向空气/超距时是否仍显示
    "emptyText": "",               // showEmpty=true 时显示的文本

    "format": "{blockName}\n§7{blockType} §8· §7{x}, {y}, {z}\n{extras}",  // 显示格式

    "overrides": [                 // 按方块类型覆盖显示格式（类型 id 子串匹配，首个命中生效）
        { "match": "minecraft:chest", "format": "{blockName}\n§e箱子\n{extras}" }
    ],

    "extras": {                    // 方块附加信息（显示在 {extras} 处）
        "enabled": true,           // 总开关
        "hardness": true,          // 所有方块的破坏时间
        "blastResistance": true,   // 爆炸抗性
        "chest": true,             // 容器占用：物品 12/27
        "bookshelf": true,         // 錾制书架里的书
        "shelf": true,             // 展示架内的物品
        "lectern": true,           // 讲台的书与页码
        "pot": true,               // 陶罐的物品与陶片
        "brewing": true,           // 酿造台槽位
        "furnace": true,           // 熔炉 / 高炉 / 烟熏炉槽位
        "jukebox": true,           // 唱片机正在播放的唱片
        "sign": true,              // 告示牌文本
        "banner": true,            // 旗帜图案
        "itemFrame": true,         // 物品展示框内的物品
        "flowerPot": true,         // 花盆里的植物
        "painting": true,          // 挂的是哪一幅画
        "piston": true,            // 活塞状态
        "redstone": true,          // 红石线 / 压力板 / 拉杆等的强度
        "repeater": true,          // 中继器挡位与信号
        "comparator": true,        // 比较器信号
        "dispenser": true,         // 发射器 / 投掷器状态
        "candle": true,            // 蜡烛根数与是否点燃
        "respawnAnchor": true,     // 重生锚充能等级
        "misc": true               // 其余方块状态
    },

    "entityEnabled": true,         // 准星指向实体时也显示
    "entityFormat": "{entityName}\n§7{entityType} §8· §7{health}/{maxHealth}",

    "server": {
        "channel": "actionbar"     // none | actionbar | tip | popup | jukebox | system | chat
    },

    "client": {
        "showOverlay": true,
        "anchor": "top_center",    // top_left/top_center/top_right/middle_left/center/middle_right/bottom_left/bottom_center/bottom_right
        "offsetX": 0.0,            // 屏幕宽度比例的水平偏移（正=从锚点向屏幕内侧）
        "offsetY": 0.0,            // 屏幕高度比例的纵向偏移（正=从锚点向屏幕内侧）
        "fontSize": 1.0,           // 字号倍率
        "background": true,        // 文字后的半透明黑底
        "backgroundAlpha": 0.45,   // 面板透明度 0~1
        "shadow": true,            // 文字阴影
        "textColor": "ffffff",     // 无颜色代码时的文字颜色（RRGGBB）
        "maxWidth": 0.0,           // 最大面板宽度占屏比，0 = 不限（超长自动换行）
        "hideOverlayInGui": true,  // 打开背包/箱子等界面时隐藏面板
        "overlayOnRemote": "on",   // on=联机也画本地面板；off=联机时隐藏
        "language": "zh_cn",       // zh_cn、en，或 auto（客户端报告了已支持的语言时才跟随）
        "keyOpenConfig": 73,       // 打开配置界面的快捷键（虚拟键码，0 = 不绑定）
        "keyToggleShow": 75        // 开关信息显示的快捷键（虚拟键码，0 = 不绑定）
    }
}
```

> 如果需要详细日志（方块的每个状态、容器槽位判定、活塞/陶罐诊断等）要将 `PreLoaderConfig.json` 中的 `logLevel` 改为 `4`

### format 占位符

| 占位符 | 含义 | 示例 |
| --- | --- | --- |
| `{blockType}` | 方块类型 id | `minecraft:stone` |
| `{blockName}` | 本地化方块名 | `石头` / `Stone` |
| `{blockKey}` | 翻译键 | `tile.stone.stone` |
| `{x}` `{y}` `{z}` | 方块整型坐标 | `10` |
| `{dist}` | 距离（格） | `3.5` |
| `{dim}` | 维度 | `overworld` |
| `{direction}` | 方块朝向（该方块没有朝向状态时为空） | `北` / `north`、`上` / `up` |
| `{light}` | 该位置的光照等级 0–15（不可用时为空） | `12` |
| `{emission}` | 方块**自身发出**的光照 0–15 | `15`（萤石）/ `0`（石头） |
| `{extras}` | 方块专属信息行（多行自动拼接，无数据时为空） | `物品 12/27` |

颜色：直接写 `§` 码，或写 `&` 码（`&a &l &r`…），`&&` 表示字面 `&`。
服务端频道里 `§` 颜色码原生生效；客户端面板支持 `§0-9a-f` 颜色与 `§r` 重置，
`§l/k/m/n/o` 这类修饰码在面板上不生效。

#### 额外信息（extras）

在 `format` 里放 `{extras}` 即可把额外信息行拼进面板（放中间或末尾都行）。

| 开关 | 显示内容 |
| --- | --- |
| `extras.chest` | 容器占用：`物品 12/27`（箱子/陷阱箱/桶/漏斗/投掷器/发射器/铜箱子/潜影盒/合成器/雕纹书架/讲台/陶罐，以及箱子矿车/漏斗矿车/运输船） |
| `extras.furnace` | 熔炉/高炉/烟熏炉：`槽位 1/3` |
| `extras.brewing` | 酿造台：`槽位 x/5` |
| `extras.redstone` | 比较器、红石线/中继器、压力板、标靶：`信号强度 12` |
| `extras.misc` | 方块状态与方块实体类信息，见下表 |

下面每一行都归与它同名的那个开关管（`extras.hardness`、`extras.banner`、`extras.sign`、
`extras.furnace`、`extras.brewing`……）。`extras.misc` 是普通方块状态的总开关，同时管那些没有自己开关的
方块实体行（蜂巢、信标、营火、床、附魔台）。它们各自显示的内容：

- 门 / 活板门 / 栅栏门：`状态 开/关`（充能时追加一行 `充能 是`）
- 按钮：`状态 按下/弹起`；拉杆：`状态 开/关`；绊线钩：`连接`、`充能`
- 侦测器：`充能 是/否`
- 活塞 / 粘性活塞：`状态 已伸出/未伸出`（动画中为 `伸出中` / `收回中`）；活塞臂：`活塞 普通/粘性` + `状态 已伸出`
- 合成器：`状态 合成中/空闲` 与 `已触发 是/否`（合成器自己的两个状态，基岩版里第二个叫 `triggered_bit`），都归 `extras.misc` 管，另有从方块实体读出的 `禁用槽位 n`
- 海泡菜：`数量 n`
- 蛋糕：`剩余 n/7`；堆肥桶：`堆肥 n/8`
- 熔炉 / 高炉 / 烟熏炉：`剩余时间 Ns`（当前正在烧的这个物品剩余的）与 `烹饪进度 N%`，只报当前这一个，并且会自己接着往下走、物品烧完自动切到下一个。引擎只在打开容器界面时才把方块实体明细发给客户端，所以客户端是拿最后收到的值按时间往下推：燃料烧完、漏斗在没人看着时补料这类变化会让它偏，重新打开熔炉即可校正
- 酿造台：`酿造进度 N%`（同样在两次打开界面之间自己往下走）与 `燃料 n/m`
- 蜂巢 / 蜂箱：`蜜蜂 n/3`（另有来自方块状态的 `蜂蜜等级 n/5`）
- 信标：`信标等级 n`
- 营火：每个正在烤的物品一行 `烤制中 <物品> Ns/30s`（剩余时间，同样自己往下走）
- 床：`被占用 是/否`
- 附魔台：`附魔等级 n`（书架功率，上限 15）
- 旗帜：`图案 n`；陶罐：`物品 <物品> ×n` 与 `陶片 n/4`；展示架：`物品 n/3`；物品展示框：`展示 <物品>` 与 `旋转角度 N°`
- 讲台：`书 <物品>`、`页码 p/total`；告示牌：`文本 <正面>` 与 `文本（背面） <背面>`
- 任何带装备的实体（盔甲架、玩家、穿装备的生物）：`头盔` / `胸甲` / `护腿` / `靴子`、`主手`、`副手`

**数据完整度**：容器与方块实体类信息在**服务端和本地单机**最完整；客户端连远程服务器时读不到
别人容器的内容，这类行会留空而不会报错。烧制类计时（熔炉、酿造台、营火）由客户端按最后一次
收到的值本地推算，所以界面关着时也会继续走，收到真实值就自动校正。暂未覆盖：命令方块、刷怪笼、作物生长阶段，以及
音符盒（26.40 的 API 拿不到音高与音色，宁可删掉也不显示猜出来的值）。

## 语言

- 方块名、容器内物品名跟随**玩家自己的语言**：直接查引擎的本地化表（它已合并原版与玩家启用的
  所有资源包），查不到时回退英文，最后回退类型 id。
- 面板标签与指令反馈提供 `lang/en.json` 与 `lang/zh_cn.json`（**文件名即 locale 码，小写**）。
  在 `lang/` 下再加一个 `<locale>.json` 即可新增语言；缺条目时回退英文，不会显示空白。

## 构建

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

产物输出到 `bin/Insight/`，整个目录即为上面「安装」里要放置的内容。

# 许可证

MIT © neverforward
