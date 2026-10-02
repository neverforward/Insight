![Insight](https://socialify.git.ci/neverforward/Insight/image?custom_description=A+lightweight+info+display+mod+for+Bedrock+Edition+that+shows+you+exactly+what+you+are+looking+at.&description=1&font=Inter&forks=1&issues=1&logo=https%3A%2F%2Fraw.githubusercontent.com%2Fneverforward%2FInsight%2Frefs%2Fheads%2Fmain%2Fassets%2Ficon.svg&name=1&owner=1&pattern=Plus&pulls=1&stargazers=1&theme=Auto)

[![](https://img.shields.io/badge/English-informational?style=for-the-badge)](README.md)
![](https://img.shields.io/badge/简体中文-inactive?style=for-the-badge)
![GitHub License](https://img.shields.io/github/license/neverforward/Insight?style=for-the-badge)
![GitHub Tag](https://img.shields.io/github/v/tag/neverforward/Insight?style=for-the-badge)
![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/neverforward/Insight/build.yml?style=for-the-badge)
![GitHub commit activity](https://img.shields.io/github/commit-activity/y/neverforward/Insight?style=for-the-badge)

一个 Minecraft 基岩版（LeviLamina 26.51.*）的**信息显示**模组：实时显示玩家准星所指方块的信息（名称、类型、坐标、距离、容器内容、方块状态……）。

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

`<选项>` 输入时游戏会给出补全候选，就是配置里的那些名字（`display.name`、`entity.health`、
`maxWidth`、`extras.chest`…）。`display.health` 已不存在
（血量现在是 `entity.health`），也从来没有 `display.dimension` 这项。取值写错会提示可用取值，例如：

```
/insight set channel actionbar
/insight set maxDistance 24
/insight set display.distance true
/insight set entity.health false
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
- 每行显示当前值，点击行展开内联编辑器；左列不再是长长的一列，而是分成五个标签页——
  **通用**、**方块显示行**、**实体信息**、**扩展信息**、**快捷键**。**方块显示行**里按开关所属的行
  分组：**标题行**（名称、朝向）、**详情行**（类型 ID、翻译键）、**坐标行**（坐标、距离）、
  **状态行**（光照、自发光）、**扩展信息**（扩展信息行 → `display.extras`）。**实体信息**里有
  **实体信息**（`entityEnabled`），以及同样分组的实体开关：**标题行**（名称、朝向）、
  **详情行**（类型 ID、翻译键）、**坐标行**（坐标、距离）、**状态行**（生命值），最后是
  **扩展信息**（扩展信息行 → `entity.extras`）。**扩展信息**标签页里有 **扩展信息**（`extras.enabled`）、
  **扩展：所有方块**、**扩展：容器**、**扩展：方块实体**、**扩展：红石**、**扩展：方块状态**，
  **快捷键**里是两个按键绑定。右列仍是面板的实时预览与外观设置；改动立即保存，底部会提示结果；
- 界面打开期间游戏不会收到键鼠输入。


```jsonc
{
    "version": 6,                  // 结构版本；升级模组时旧配置会自动合并，version <= 4 的
                                   // format / entityFormat 模板会分别转成 display / entity
                                   // 两组开关（见下文）

    "enabled": true,               // 总开关
    "enabledByDefault": true,      // 玩家默认开启；玩家可用 /insight 单独切换

    "maxDistance": 16.0,           // 射线最大距离（格）
    "intervalTicks": 4,            // 采样间隔（20 tick = 1 秒；4 = 0.2s）
    "passThroughLiquids": true,    // 视线是否穿透水/岩浆
    "showEmpty": false,            // 看向空气/超距时是否仍显示
    "emptyText": "",               // showEmpty=true 时显示的文本

    "display": {                   // 方块面板显示哪些内容，每部分一个开关（取代旧的文本模板）
        "name": true,                  // 标题行：本地化的方块名
        "facing": true,                // 标题行：名称后用括号附上的方块朝向 / 轴，如 石头§7(北)；方块没有我们认得的状态时跳过
        "identifier": true,            // 详情行：方块类型 id，如 minecraft:stone
        "translationKey": false,       // 详情行：紧跟类型 id 的括号内方块翻译键，如 minecraft:stone(tile.stone.stone)
        "position": true,              // 坐标行：x, y, z
        "distance": false,             // 坐标行：与方块的距离（格）
        "light": true,                 // 状态行：该位置的光照，如 光照 12
        "emission": true,              // 状态行：方块自身发出的光，如 自发光 15
        "extras": true                 // 方块专属扩展信息行（还受 extras.enabled 约束）
    },

    "entity": {                    // 实体面板显示哪些内容（是否显示实体由 entityEnabled 决定）
        "name": true,                  // 标题行：玩家真实名字 / 命名牌 / 本地化的实体名
        "facing": true,                // 标题行：实体自身的偏航角换算成的方位，放在名称后的括号里
        "identifier": true,            // 详情行：实体类型 id，如 minecraft:zombie
        "translationKey": false,       // 详情行：名称所解析自的键，如 minecraft:zombie(entity.zombie)
        "position": true,              // 坐标行：实体的 x, y, z
        "distance": false,             // 坐标行：与实体的距离（格）
        "health": true,                // 状态行：当前/最大血量；没有血量的实体（物品、投掷物、画）不显示
        "extras": true                 // 实体专属扩展信息行（与方块侧共用同一套 extras.* 适配器，同样受 extras.enabled 约束）
    },

    "colors": {                    // 面板每个部分的颜色：每处一个格式码，不含 "§"（"c" 即 §c）；
                                   // 空串（或 none / off / default）表示该处用纯文本色。方块面板、
                                   // 实体面板与扩展信息行共用这一套，配色一处改全局生效。
        "name": "f",               // 目标名称
        "facing": "7",             // 名称后括号里的朝向
        "identifier": "7",         // 类型 ID 行
        "translationKey": "7",     // 类型 ID 后括号里的翻译键
        "x": "c",                  // 坐标：x（红）
        "y": "a",                  // 坐标：y（绿）
        "z": "b",                  // 坐标：z（青）
        "distance": "7",           // 坐标后括号里的距离
        "label": "7",              // "标签 取值" 行：标签
        "value": "f",              // "标签 取值" 行：取值
        "health": "c"              // 实体血量
    },

    "extras": {                    // 扩展信息适配器，方块与实体面板共用（display.extras / entity.extras 打开时才显示）
        "enabled": true,           // 两侧所有适配器的总开关
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
        "transitionTime": 0.1,     // 面板开关的淡入淡出、以及换目标时面板尺寸过渡的秒数，0 = 立即
        "hideOverlayInGui": true,  // 打开背包/箱子等界面时隐藏面板
        "overlayOnRemote": "on",   // on=联机也画本地面板；off=联机时隐藏
        "language": "zh_cn",       // zh_cn、en，或 auto（客户端报告了已支持的语言时才跟随）
        "keyOpenConfig": 73,       // 打开配置界面的快捷键（虚拟键码，0 = 不绑定）
        "keyToggleShow": 75        // 开关信息显示的快捷键（虚拟键码，0 = 不绑定）
    }
}
```

> 如果需要详细日志（方块的每个状态、容器槽位判定、活塞/陶罐诊断等）要将 `PreLoaderConfig.json` 中的 `logLevel` 改为 `5`

### display 开关（方块）

面板不再由文本模板拼出：每一部分都有自己的开关，由模组自己组装，因此不可能拼出错误布局
（不会有残留占位符、悬空分隔符或空行）。客户端上这块面板由游戏自带的 UI 渲染器绘制
（引擎字体、用引擎物品渲染器画主体图标、圆角底板），并按 `client.transitionTime` 淡入淡出、
换目标时做尺寸过渡。方块与实体不再共用一套开关：下面的 `display.*` 描述**方块**面板，
`entity.*` 描述**实体**面板。开关都是布尔值：写 `true` / `false`（`on` / `off`、`1` / `0`
同样可用），与其它开关的解析方式一致。

| 开关 | 默认值 | 显示内容 |
| --- | --- | --- |
| `display.name` | `true` | 本地化的方块名，即标题行 |
| `display.facing` | `true` | 名称后紧跟次要色括号里的方块朝向 / 轴，如 `石头§7(北)`；方块没有我们认得的状态时跳过 |
| `display.identifier` | `true` | 方块类型 id，如 `minecraft:stone`，即详情行 |
| `display.translationKey` | `false` | 紧跟类型 id、同样用次要色的括号内方块翻译键，如 `§7minecraft:stone(tile.stone.stone)` |
| `display.position` | `true` | `x, y, z`，即坐标行 |
| `display.distance` | `false` | 与方块的距离（格），跟在坐标后面 |
| `display.light` | `true` | 该位置的光照，显示为 `光照 12` |
| `display.emission` | `true` | 方块自身发出的光，显示为 `自发光 15` |
| `display.extras` | `true` | 方块专属扩展信息行（即 `extras.*` 各适配器，还受 `extras.enabled` 约束） |

### entity 开关（实体）

`entityEnabled` 决定是否瞄准并显示实体；下面的开关只决定指向实体后这块面板里有什么。

| 开关 | 默认值 | 显示内容 |
| --- | --- | --- |
| `entity.name` | `true` | 玩家真实名字 / 命名牌 / 本地化的实体名，即标题行 |
| `entity.facing` | `true` | 实体**自身偏航角**换算出的方位，放在名称后次要色的括号里 |
| `entity.identifier` | `true` | 实体类型 id，如 `minecraft:zombie`，即详情行 |
| `entity.translationKey` | `false` | 名称所解析自的键，紧跟类型 id、同样用次要色的括号，如 `§7minecraft:zombie(entity.zombie)` |
| `entity.position` | `true` | 实体的 `x, y, z`，即坐标行 |
| `entity.distance` | `false` | 与实体的距离（格），跟在坐标后面 |
| `entity.health` | `true` | `当前/最大` 血量，如 `§7生命值 §f12/20`；没有血量的实体（物品、投掷物、画）不显示 |
| `entity.extras` | `true` | 实体专属扩展信息行（箱子/漏斗矿车、运输船等容器实体、画、穿戴的装备）；与方块侧共用同一套 `extras.*` 适配器，同样受 `extras.enabled` 约束 |

### 颜色

面板每个部分都有自己的颜色，取自上面的 `colors` 分组。取值是**去掉 `§` 的格式码**——就是玩家在
`§` 后面打的那个字符，所以 `"c"` 表示 `§c`；空串（或 `none` / `off` / `default`）表示该处用纯文本
色，也就是 `client.textColor`。

除了常见的十六色，基岩版还多出一批材料色：

| 码 | 颜色 | 码 | 颜色 | 码 | 颜色 | 码 | 颜色 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `0` | 黑色 | `4` | 深红色 | `8` | 深灰色 | `c` | 红色 |
| `1` | 深蓝色 | `5` | 深紫色 | `9` | 蓝色 | `d` | 淡紫色 |
| `2` | 深绿色 | `6` | 金色 | `a` | 绿色 | `e` | 黄色 |
| `3` | 暗水蓝色 | `7` | 灰色 | `b` | 水蓝色 | `f` | 白色 |

| 码 | 基岩版颜色 | 码 | 基岩版颜色 | 码 | 基岩版颜色 |
| --- | --- | --- | --- | --- | --- |
| `g` | Minecoin 金色 | `n` | 铜锭色 | `t` | 青金石色 |
| `h` | 石英色 | `p` | 金锭色 | `u` | 紫水晶色 |
| `i` | 铁锭色 | `q` | 绿宝石色 | `v` | 树脂色 |
| `j` | 下界合金色 | `s` | 钻石色 | `w` | 组队蓝色 |
| `m` | 红石色 | | | | |

`colors.label` 与 `colors.value` 是所有「标签 取值」行用的两处，因此扩展信息行也归它们管：箱子的
`物品 12/27` 就是一处标签加一处取值，没有单独的设置项。本文档里出现的 `§7`、`§f` 等写法都是默认值。

这些开关按顺序拼出的行。方块面板：

```
<名称>§7(<朝向>)                                           ← 标题行（display.name、display.facing）
§7<类型 ID>(<翻译键>)                                      ← 详情行（display.identifier、display.translationKey）
<x, y, z> §7<距离>                                         ← 坐标行（display.position、display.distance）
§7<标签> §f<取值> ……                                       ← 状态行：光照、自发光
<扩展信息行>                                               ← 每个适配器一行
```

实体面板：

```
<名称>§7(<朝向>)                                           ← 标题行（entity.name、entity.facing）
§7<类型 ID>(<翻译键>)                                      ← 详情行（entity.identifier、entity.translationKey）
<x, y, z> §7<距离>                                         ← 坐标行（entity.position、entity.distance）
§7<标签> §f<当前>/<最大>                                     ← 状态行：生命值（entity.health）
<扩展信息行>                                               ← 每个适配器一行
```

- 朝向挂在第 1 行的**名称**后面，用次要色（`§7`）的括号括起——方块是它自己的朝向状态，实体是
  它自身的偏航角；开关关掉或目标没有朝向时整段都不出现；
- 翻译键在第 2 行紧跟在**类型 id** 后面，同样是次要色的括号；
- 第 3 行是默认文字颜色的坐标，后面跟 `§7` 的距离，中间只隔一个空格；
- 第 4 行是带标签的状态，每项形如 `§7<标签> §f<取值>`、项与项之间只隔一个空格：方块面板显示
  光照与方块自身发出的光（`光照 12 自发光 15`），实体面板显示生命值（`生命值 12/20`）——
  实体没有光照类数据；
- 旧时用来分隔每个格式部分的 `·` 没有了——面板各部分只用单个空格连接。唯一剩下的 `·` 在容器
  的扩展信息行里，位于槽位数量与物品总数之间（`物品 12/27 §7· §f35`）；
- 维度不再显示。

关掉的开关、以及目标本身没有的部分（拿不到朝向状态的方块的朝向、本身没有血量的实体类型）
都会跳过，空行会被丢掉，所以布局永远不会错位。`showEmpty` 打开且没有指向任何目标时仍然使用
`emptyText`；它不再有占位符。

颜色：面板自己的行已经带好 `§` 颜色码（次要部分 `§7`——朝向、类型 id、翻译键、距离与状态标签
——和取值 `§f`；扩展信息行同样用这两种，另外用深灰 `§8` 画截断的物品列表 `...`、空唱片 `-`
这类占位符号），不需要自己上色。唯一由你写的文本是 `emptyText`，它按你写的原样绘制——`&` 不是
颜色码，想上色就直接写 `§` 码。

#### 升级旧配置

结构版本低于 5 的 `config.json` 是用旧的 `format` / `entityFormat` 文本模板描述面板的。在配置库
把新的默认值合并进来、重写文件之前，模组会先读一次这两个模板并逐组转成开关：旧方块模板
`format` 决定 `display.*` 各开关，旧实体模板 `entityFormat` 决定 `entity.*` 各开关，模板里用到的
占位符对应的开关保持打开，没用到的开关关掉。所以旧版默认 `format`（用了 `{blockName}`、
`{blockType}`、`{x} {y} {z}` 和 `{extras}`）会得到 `display.name`、`display.identifier`、
`display.position` 和 `display.extras` 打开、方块组其余关闭；实体模板里的占位符按同样的规则
对应到各自的 `entity.*` 开关，例如 `{entityName}` → `entity.name`、`{entityType}` → `entity.identifier`、
`{x} {y} {z}` → `entity.position`、`{health}` / `{maxHealth}` → `entity.health`、`{extras}` →
`entity.extras`，升级的玩家看到的还是原来那块面板。文件里根本没有的模板会让它那一组保持
默认值，而不是把所有开关都关掉。迁移只在版本低于 5 的文件上执行，结果立即保存。逐方块的
`overrides` 没有对应关系，仍然会被丢弃；`{dim}` 也没有开关可对应——维度不再显示——同样被忽略。

#### 额外信息（extras）

扩展信息行在面板自己的开关打开时才会拼进去——方块是 `display.extras`，实体是 `entity.extras`——
而 `extras.enabled` 总管**两侧**的适配器，关掉它就同时静音方块与实体的扩展信息。实体面板并不是
另一套适配器：它复用同一批 `extras.*` 开关（箱子/漏斗矿车这类容器实体是 `extras.chest`，画是
`extras.painting`，玩家、盔甲架和穿装备生物身上的装备是 `extras.misc`）。下面每个适配器都有
自己的开关：

| 开关 | 显示内容 |
| --- | --- |
| `extras.chest` | 容器占用：`物品 12/27`（物品总数另以 `·` 跟在后面，如 `物品 12/27 §7· §f35`）——箱子/陷阱箱/桶/漏斗/投掷器/发射器/铜箱子/潜影盒/合成器/雕纹书架/讲台/陶罐，以及箱子矿车/漏斗矿车/运输船 |
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
