# Galaxy RP Ultimate: server Lua scripts

Server admins can add quests, jobs, world events, scripted interactions and minigames with Lua
scripts, without rebuilding the mod. Scripts run on the server only; players need nothing new.

This guide covers **API version 1**. It has events, commands, timers, tasks, messages, server
information, player information and storage. Spawning things in the world, changing players and
giving rewards come in later versions.

## Getting started

1. Make a folder `lua/plugins/<name>/` in the GalaxyRP folder, with a `plugin.lua` in it.
   - The folder name is the plugin's name: lower-case letters, digits and `_`, up to 32 characters.
2. In game, an admin with the **Scripts** power runs `/scripts enable <name>`. From rcon, use
   `lua_enable <name>`.
3. To keep the plugin on after a restart, add its name to `rp_lua_plugins` in
   `galaxyrp_server.cfg`, separated from the others by spaces.

`lua/examples/` has three plugins to copy and change:
- `welcome`: greets new players;
- `announcements`: timed chat messages;
- `playtime`: a `/playtime` command with per-character storage.

**Map scripts** are files in `lua/maps/<map name>/` (for example `lua/maps/mp/ffa5/doors.lua`).
- They run only on that map, and only when `rp_lua_mapscripts` is 1.
- Each file runs like its own plugin, called `map:<file>`.

Scripts are read **only from loose files** in the GalaxyRP folder. A script inside a pk3 is never
run, so no script can end up in a pk3 that players download.

## How scripts run

- **Per map.** Every map starts a fresh Lua state and runs every plugin again. Variables do not
  survive a map change; use `Storage` for anything that must. `/scripts reload all` does the same
  mid-map.
- **Own globals.** Each plugin has its own globals: two plugins can both have a variable called
  `count`. The shared libraries and the API tables are read-only.
- **Removed from Lua:** `io`, `os`, `package`, `debug`, `require`, `load`, `loadfile`, `dofile`,
  `collectgarbage`, `rawget`, `rawset`, `rawequal`, `rawlen` and `string.dump`. Precompiled Lua
  files are refused.
- **Splitting a plugin into files:** `include("other.lua")` runs another file from the plugin's own
  folder, once.
- **`print(...)`** writes to the server console and the server log.

### Limits

All limits are cvars in `galaxyrp_server.cfg`.

| Cvar | Default | What it limits |
|---|---|---|
| `rp_lua_budget` | 1000000 | Instructions one call into a script may run |
| `rp_lua_budgetms` | 50 | Milliseconds one call may run |
| `rp_lua_framems` | 15 | Milliseconds of timers, tasks and `frame` handlers per server frame; the rest run next frame |
| `rp_lua_memmb` | 64 | Memory for all scripts together, in MB |
| `rp_lua_maxerrors` | 20 | Errors in a minute before a plugin is switched off |
| `rp_lua_maxvalue` | 65536 | Bytes in one stored value |
| `rp_lua_maxstore` | 2097152 | Bytes one plugin may keep in storage |

- **A script over the limit** stops with an error: "script ran too long". It cannot catch that error
  and carry on.
- **Errors** are written to the server log. Online admins with the Scripts power see them, at most
  once per plugin every 30 seconds.
- **A plugin that keeps failing** is switched off until it is reloaded or enabled again.

### Admin commands

In game you need the **Scripts** admin power (number 29, see `/adminlist 29`). From rcon you need
nothing extra.

| In game | Rcon | What it does |
|---|---|---|
| `/scripts` | `lua_list` | Plugins, map scripts and script commands |
| `/scripts reload [plugin\|all]` | `lua_reload [plugin\|all]` | Load a plugin again, or all of them |
| `/scripts enable <plugin>` | `lua_enable <plugin>` | Start a plugin until the server restarts |
| `/scripts disable <plugin>` | `lua_disable <plugin>` | Stop a plugin until the server restarts |
| `/scripts errors` | `lua_errors` | The last 50 errors |
| `/scripts stats` | `lua_stats` | Calls, time, timers and storage per plugin |
| — | `lua_exec <code>` | Run a line of Lua (only with `rp_lua_allowexec 1`) |

The console removes double quotes and splits commands at `;`. So in `lua_exec`, write strings with
single quotes or `[[...]]`, and don't use `;`.

## The API

### Plugin

```lua
Plugin.register{ name = "Bank job", version = "1.2", api = 1 }
Plugin.name()   -- "bankjob": the folder name
```

- `register` is optional. It sets what `/scripts` shows.
- A plugin that asks for an `api` newer than the server has is refused, with a clear error.

### Events

```lua
local id = Events.on("chat", function(player, mode, text, target) ... end)
Events.off(id)
```

| Event | Arguments | Notes |
|---|---|---|
| `init` | — | After every plugin has loaded (and after `/scripts reload`) |
| `shutdown` | — | Map end or reload |
| `unload` | — | This plugin only, before it is disabled or reloaded |
| `frame` | `time` | Every server frame; counts against `rp_lua_framems` |
| `client_connect` | `player`, `first_time` | `first_time` is false on map changes. Notification only |
| `client_begin` | `player` | The player is in the game (also after every map change) |
| `client_spawn` | `player` | The player (re)spawned |
| `client_disconnect` | `player` | The player is leaving; the handle stops working after this |
| `chat` | `player`, `mode`, `text`, `target` | `mode` is `"say"`, `"ooc"`, `"team"`, `"tell"` or `"ally"`. `target` is the player for `"tell"`, otherwise nil. Return `false` to stop the message; handlers after that one are not called |
| `command` | `player`, `name`, `args` | After any script command ran |

- After a reload, players already on the server get no `client_begin`. Use `Server.players()` in
  `init` to pick them up.
- Chat from a silenced player raises no event.

### Commands

```lua
Commands.register("roll20", function(player, args)
    Msg.chat(player, "You rolled " .. math.random(20))
end, { help = "Rolls a 20-sided die", logged_in = false, admin = nil })
```

- **Names:** 2 to 24 lower-case letters, digits or `_`.
- **Refused names:** any name the game already uses, including commands the game client handles
  itself. Script commands can never replace a built-in command. A name another plugin registered
  is refused too.
- **`args`:** the words after the command, as strings.
- **`logged_in = true`:** only logged-in players can use the command.
- **`admin = <number>`:** only admins with that power can use it (the numbers are in `/adminlist`).

### Timers and tasks

```lua
local id = Timer.after(5000, function() ... end)   -- once, in 5 s
local id = Timer.every(60000, function() ... end)  -- every minute (at least 50 ms)
Timer.cancel(id)

Task.start(function(player)
    Msg.center(player, "The door is opening...")
    Task.wait(3000)
    Msg.center(player, "Go!")
end, player)
```

- **Limits:** each plugin can have 256 timers and 64 tasks.
- **When they run:** on the next server frame at the earliest. They end with the map.
- **Tasks** can `Task.wait(ms)` as often as they like; `Task.cancel(id)` stops one.

### Messages

```lua
Msg.print(player, text)        -- console
Msg.center(player, text)       -- middle of the screen
Msg.chat(player, text)         -- chat area
Msg.broadcast(text)            -- everyone's chat
Msg.broadcast_center(text)     -- everyone's screen
```

- Colour codes (`^1`...`^7`) work.
- Text is cut to 900 characters, and `"` becomes `'`.

### Server

```lua
Server.time()          -- milliseconds since the map started
Server.realtime()      -- seconds since 1970 (UTC)
Server.date("date")    -- "2026-10-17"; also "time", "datetime", "weekday" (1 = Monday) and "hour"
Server.map()           -- "mp/ffa5"
Server.cvar("sv_hostname")
Server.players()       -- every player in the game
```

`Server.cvar` reads only `sv_hostname`, `g_gametype`, `mapname`, `timelimit`, `fraglimit`,
`sv_maxclients`, `g_motd` and `rp_*` cvars.

### Players

A player handle is the same value for the whole connection, so it can be a table key. After the
player disconnects, using it is an error. Check `player.valid` first when unsure.

| Field | Meaning |
|---|---|
| `id` | Client number |
| `name` / `name_raw` | Name without / with colour codes |
| `team` | `"free"`, `"red"`, `"blue"` or `"spectator"` |
| `is_bot` | true for bots |
| `logged_in` | Logged in to an account |
| `rpg` | Playing a character |
| `character_id` | The character's id, or nil |
| `valid` | Still connected |
| `player:has_admin(n)` | Has admin power `n` |

API version 1 cannot change players. That comes later.

### Storage

```lua
Storage.set("winner", "Kyle")              -- this plugin's own data
Storage.get("winner")                      -- "Kyle"
Storage.set("winner", nil)                 -- deletes it

if player.character_id then
    local store = Storage.char(player)     -- this character's data, for this plugin
    store:set("quest", { step = 2, kills = 5 })
    local q = store:get("quest")
end
```

- **Where it lives:** `GalaxyRP/database/lua.db`, written every 30 seconds and at map end. Each
  plugin sees only its own data.
- **Keys:** 1 to 64 characters.
- **Values:** nil, true/false, numbers, strings, and tables of them up to 16 deep, without loops.
- **Characters:** a character's data is deleted when the character is deleted. `Storage.char` needs
  a player who is playing a character.
