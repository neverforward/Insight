# 配置
除了用聊天栏指令，还可以用界面改配置

## 配置界面（仅客户端）

使用`/cliinsight gui`或快捷键打开（默认 <kbd>I</kbd>）
- 两个按键都会出现在游戏自带的按键设置里、可在那里改键——配置里的 `keyOpenConfig` /
  `keyToggleShow` 只是默认值（Windows 虚拟键码，`0` 表示不绑定）；
- 每行显示当前值，点击行展开内联编辑器

## 配置菜单（服务端）

使用 `/insight gui` 打开配置表单

## 默认配置文件

```jsonc
{
    "version": 10,                  // 结构版本；升级模组时旧配置会自动合并，version <= 4 的
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
        "healthStyle": "hearts",       // 血量（以及盔甲行）怎么画，仅客户端：bar 条形 | bar+number 条形+数值 | hearts 心形 | hearts+number 心形+数值 | number 数值
        "heartsThreshold": 40.0,       // 血量（点）；最大血量超过它的实体仍然显示数字
        "heartsPerRow": 10,            // 每行红心数；再多的行从上一行上方开始，并压住上一行的 1/2
        "healthDecimals": false,       // 数字形式的血量保留一位小数，如 19.5/20
        "armor": true,                 // 盔甲值用原版盔甲贴图画（仅客户端），阈值与行规则同红心
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
        "chest": true,             // 容器占用：客户端直接画物品图标，服务端仍是 物品 12/27
        "chestIcons": true,        // 用物品图标代替「物品 12/27」（仅客户端）
        "chestIconsMerge": true,   // 合并同类项：同一种物品只画一个图标，堆叠数量相加
        "chestIconsPerRow": 9,     // 每行放几个图标
        "chestIconLimit": 18,      // 最多画几个图标（0 = 全画，放不下的用一个小 "+n" 记数）
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

    "entityExtras": {              // 只对实体有意义的额外信息行（同样受 extras.enabled 总开关控制）
        "owner": true,             // 已驯服动物：主人是谁
        "effects": true,           // 当前状态效果，占一行
        "effectsLimit": 4,         // 这一行最多放几个（0 = 全部）
        "breeding": true,          // 恋爱 / 繁殖冷却计时
        "conversion": true,        // 僵尸村民：还需多久转化
        "petArmor": "all",         // 宠物护甲（身体槽）：all | damaged | off
        "horse": true,             // 马：移动速度、跳跃力、是否装鞍
        "armorStand": true,        // 盔甲架：姿势序号
        "villager": true,          // 村民：职业与交易等级
        "waxed": true              // 铜傀儡：是否已涂蜡（仅服务端；该属性不同步给客户端）
    },

    "entityEnabled": true,         // 准星指向实体时也显示

    "server": {
        "channel": "actionbar"     // none | actionbar | tip | jukebox | system | chat
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

## entity 开关（实体）

`entityEnabled` 决定是否瞄准并显示实体，下面的开关只决定指向实体后这块面板里有什么。

| 开关 | 默认值 | 显示内容 |
| --- | --- | --- |
| `entity.name` | `true` | 玩家真实名字 / 命名牌 / 本地化的实体名，即标题行 |
| `entity.facing` | `true` | 实体**自身偏航角**换算出的方位，放在名称后次要色的括号里 |
| `entity.identifier` | `true` | 实体类型 id，如 `minecraft:zombie`，即详情行 |
| `entity.translationKey` | `false` | 名称所解析自的键，紧跟类型 id、同样用次要色的括号，如 `§7minecraft:zombie(entity.zombie)` |
| `entity.position` | `true` | 实体的 `x, y, z`，即坐标行 |
| `entity.distance` | `false` | 与实体的距离（格），跟在坐标后面 |
| `entity.health` | `true` | `当前/最大` 血量，如 `§7生命值 §f12/20`；没有血量的实体（物品、投掷物、画）不显示 |
| `entity.healthStyle` | `hearts` | 血量与盔甲行怎么画（仅客户端，见下文）：`bar` 条形 / `bar+number` 条形+数值 / `hearts` 心形 / `hearts+number` 心形+数值 / `number` 数值 |
| `entity.heartsThreshold` | `40` | 血量（点）；`hearts` 模式下**最大**血量不超过它的实体画心，超过的仍显示数字 |
| `entity.heartsPerRow` | `10` | 一行放几颗心，再多的行从上一行上方开始；`bar` 模式下是条形的宽度（以红心计） |
| `entity.healthDecimals` | `false` | `number` 模式保留一位小数，如 `19.5/20` |
| `entity.armor` | `true` | 是否显示盔甲行，样式与血量相同；`number` 模式下形如 `§7盔甲 §f15` |
| `entity.extras` | `true` | 实体专属扩展信息行（箱子/漏斗矿车、运输船等容器实体、画、穿戴的装备）；一部分与方块侧共用 `extras.*` 适配器，实体自己的在 `entityExtras`，两者都受 `extras.enabled` 约束 |

## 颜色
颜色跟随原版色码

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

## 血量与盔甲行

- **`bar` 条形**  客户端由模组绘制 服务端采用`█`和`▌`绘制
- **`hearts`** 心形（仅客户端）
- **`number` 数值**。
- **`bar+number` 条形+数值** 
 - **`hearts+number` 心形+数值（仅客户端）**


### 额外信息（extras）

| 开关 | 显示内容 |
| --- | --- |
| `extras.chest` | 容器内容——箱子/陷阱箱/桶/漏斗/投掷器/发射器/铜箱子/潜影盒/合成器/雕纹书架/讲台/陶罐，以及箱子矿车/漏斗矿车/运输船。**客户端**会用游戏自己的物品图标把东西画出来（`extras.chestIcons`）：每个非空格子一个图标、大小就是游戏自己 GUI 里那个物品尺寸（看起来和背包里的一致），堆叠数量按原版习惯用小字写在图标角上，`extras.chestIconsMerge` 会把同类项合成一个图标、数量相加（6 把剑显示成一把剑带 `6`，5 组 64 个草方块显示成 `320`），而不是一个槽位一个图标；这一行**永远排在面板最后**（不管方块还报告了什么），所以图标一定贴在框的底部；每行 `extras.chestIconsPerRow` 个、最多画 `extras.chestIconLimit` 个（0 = 全画，放不下的用一个小 `+n` 记数）；空容器没东西可画，客户端干脆不显示这一行（服务端仍显示原来那行，它只能传文字）。**服务端**通道只能传文字，所以那边仍是 `物品 12/27`（物品总数另以 `·` 跟在后面，如 `物品 12/27 §7· §f35`） |
| `extras.furnace` | 熔炉/高炉/烟熏炉：`槽位 1/3`，烧制时另有 `剩余时间 12s` 与 `烹饪进度 40%`，以及下面那条容器行。`烹饪进度` 是当前这件烧到了多少——刚放进去 0%、烧完 100%；`剩余时间` 是后面每一件各一个完整的烧制时间，再加上当前这件的剩余。这两行只在真正拥有该机器的世界里读取：服务端总是有，本地存档里的客户端也有（见《读取本地存档》）；联机时的客户端只有槽位行与容器行，因为网络副本里的计时器是残缺、成批下发的。里面的东西在**界面打开时**跟着变（读界面自己的容器模型，引擎会往里填实时内容；方块实体里只有一份不完整的副本）；从没开过界面的熔炉只能显示方块实体里那份 |
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

实体还有自己的一组开关，放在 `entityExtras` 里（同样受 `extras.enabled` 总开关控制）。每一条都
只在该实体确实有内容时才出现：

| 开关 | 内容 |
| --- | --- |
| `entityExtras.owner` | 已驯服动物的主人：`主人 Steve`（玩家主人用真名，其它用名牌；主人不在场则不显示） |
| `entityExtras.effects` | 当前状态效果挤在一行，每条是「名字 等级 剩余时间」：`状态效果 速度 II 0:12 · 力量 0:30`。名字用游戏自己的效果名，等级只在二级及以上才写罗马数字——和原版效果列表的写法一致。`entityExtras.effectsLimit` 是这一行最多放几个（多出来的记成 `· +n`，`0` 表示全放）。这一行**只在服务端显示**：客户端最多只会收到其它生物最近的一个效果，显示出来就是一个效果、随后整行消失 |
| `entityExtras.breeding` | 繁殖计时：喂食后的恋爱计时或繁殖后的冷却显示 `繁殖 0:30`。这些计数是服务端状态，客户端只会被告知"是否在恋爱"，所以客户端显示 `繁殖 恋爱中` |
| `entityExtras.conversion` | 僵尸村民转化时间：`转化中 2:31`，取自游戏自己在转化期间跑的转化倒计时（`TransformationComponent`）。这个倒计时不会同步给客户端，所以服务端给时间、客户端只能给状态 |
| `entityExtras.petArmor` | 身体槽的护甲（狼铠、鹦鹉螺铠、地毯等），它不属于四条人形护甲槽：`宠物护甲 狼铠 12/25`。`all` 有就显示、`damaged` 只显示已经磨损的、`off` 不显示 |
| `entityExtras.horse` | 马、驴、骡、骷髅马/僵尸马。马还会多一行外观，顺序就是游戏自己的贴图顺序：`毛色 白色 · 花纹 白色圆点`（没有花纹时只显示毛色）。所有坐骑再给一行玩家能判断的量——`速度 7.34 方块/秒 · 跳跃高度 3.25 方块`（移动速度属性 ×42.16，跳跃力代入游戏跳跃用的三次式换算成高度），装了鞍再加 `· 已装鞍` |
| `entityExtras.armorStand` | 盔甲架不是默认姿势时显示姿势序号：`姿势 5`。API 只给序号不给姿势名，所以显示数字 |
| `entityExtras.villager` | 村民的交易信息：`职业 图书管理员 · 等级 2`。职业名直接用游戏自己的 `entity.villager.*` 文本，跟随玩家语言；等级就是交易界面上的等级（原始 tier 从 0 起算） |
| `entityExtras.waxed` | 铜傀儡是否已涂蜡：`已涂蜡 是` / `已涂蜡 否`。游戏把该属性当作仅服务端数据（故意不同步给客户端），所以这一行只在服务端出现；客户端不显示，而不是把一只已涂蜡的铜傀儡说成"否" |


## 数据完整度
- 客户端+本地存档：**高**
- 客户端+远程数据：**低**
- 服务端：**高**
