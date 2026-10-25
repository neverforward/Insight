# 指令

模组在服务端的命令是 `/insight`，客户端是 `/cliinsight`。
两边的子命令完全一致。

```
/insight toggle          开/关显示（服务端：你自己的开关，存在模组数据目录；客户端：总开关，写入配置文件）
/insight on | off        同上
/insight status          查看当前状态（开关/间隔/距离/频道，或锚点/显示/语言/extras）
/insight gui             打开配置：客户端是自己的界面，服务端则给发命令的玩家推一个表单
/insight reload          重新读取配置文件（需要 OP/管理员）
/insight set <选项> <值>   游戏内直接改配置并保存（需要 OP/管理员）
```

取值写错会提示可用取值，例如：

```
/insight set channel actionbar
/insight set maxDistance 24
/insight set display.distance true
/insight set entity.health false
/insight set extras.chest false
```

服务端每位玩家的开关保存在模组数据目录下的键值数据库里（`plugins/Insight/data/players/`），
服务器重启后依然有效；只有在玩家把开关改成**非默认值**时才写入记录。
