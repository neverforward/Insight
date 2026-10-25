# Commands

The command is `/insight` on the server and **`/cliinsight`** in the client build. Joining a world
merges the server's command list into the client registry and lets client mods register on top of it,
so an unprefixed client-side `/insight` would collide with a server-side one when both installs are
present - the `cli` prefix keeps them apart (the same convention as LeviLamina's own `/levilamina` vs
`/clilevilamina`). The tree below is identical on both sides.

```
/insight toggle          switch the display on/off (server: your own switch, kept in the mod's data
                         directory; client: the overlay switch, saved to the config file)
/insight on | off        same as above
/insight status          show the current state (switch/interval/distance/channel, or anchor/display/language/extras)
/insight gui             open the configuration: the client's own screen, or a form the server
                         pushes to the player who asked
/insight reload          re-read the configuration file (OP / operator)
/insight set <option> <value>   change a setting in game and save it (OP / operator)
```

`<option>` is completed by the game: the candidates are exactly the configuration names
(`display.name`, `entity.health`, `maxWidth`, `extras.chest`, ...). `display.health` no longer exists (health is
`entity.health` now) and there is no `display.dimension` at all. A bad value reports the accepted
ones, for example:

```
/insight set channel actionbar
/insight set maxDistance 24
/insight set display.distance true
/insight set entity.health false
/insight set extras.chest false
```

On the server each player's switch is stored in a key-value database inside the mod's data
directory (`plugins/Insight/data/players/`), so it survives server restarts; an entry is only
written when a player changes the switch away from its **default** value.
