# Galaxy RP Ultimate: server Lua scripts

Server admins can add quests, jobs, world events, scripted interactions and minigames with Lua
scripts, without rebuilding the mod. Scripts run on the server only; players need nothing new.

This guide covers **API version 3**:
- version 1: events, commands, timers, tasks, messages, server information, player information
  and storage;
- version 2: the world -- entities, zones, effects, sounds, music, screen shakes, and the map's
  `target_lua` entity;
- version 3: players and NPCs -- what a player handle tells, teleporting, healing, items, player
  effects and skills, spawning and controlling NPCs, and the death, knockdown and damage events.

Credits, XP and login events come in a later version. A plugin written for version 1 or 2 runs
unchanged.

## Getting started

1. Make a folder `lua/plugins/<name>/` in the GalaxyRP folder, with a `plugin.lua` in it.
   - The folder name is the plugin's name: lower-case letters, digits and `_`, up to 32 characters.
2. In game, an admin with the **Scripts** power runs `/scripts enable <name>`. From rcon, use
   `lua_enable <name>`.
3. To keep the plugin on after a restart, add its name to `rp_lua_plugins` in
   `galaxyrp_server.cfg`, separated from the others by spaces.

`lua/examples/` has plugins to copy and change:
- `welcome`: greets new players;
- `announcements`: timed chat messages;
- `playtime`: a `/playtime` command with per-character storage;
- `terminal` (API 2): a console players use with the Use key;
- `meteors` (API 2): `/meteors <seconds>`, a meteor shower world event;
- `bounty` (API 3): `/bounty`, a "kill three NPCs" quest;
- `race` (API 3): `/race`, a checkpoint race with a countdown and records.

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
| `rp_lua_maxents` | 32 | Entities and zones one plugin may have at once |
| `rp_lua_spawnkb` | 512 | KB of game memory all scripts' `World.spawn` may use per map (freed when the map changes) |
| `rp_lua_newfx` | 8 | New effect files scripts may add per map (a map has only 64 effect slots) |
| `rp_lua_maxnpcs` | 8 | NPCs one plugin may have at once, corpses included |
| `rp_lua_items` | (empty) | Not a limit but a list: the item class names scripts may give players |

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
| `/scripts stats` | `lua_stats` | Calls, time, timers, entities and storage per plugin |
| — | `lua_exec <code>` | Run a line of Lua (only with `rp_lua_allowexec 1`) |

The console removes double quotes and splits commands at `;`. So in `lua_exec`, write strings with
single quotes or `[[...]]`, and don't use `;`.

## The API

### Plugin

```lua
Plugin.register{ name = "Bank job", version = "1.2", api = 2 }
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
| `use` | `entity`, `activator`, `other` | API 2. An entity with the targetname given is used (see below). Return `false` to stop the use |
| `map_event` | `name`, `activator`, `entity` | API 2. A `target_lua` was used (see below) |

- After a reload, players already on the server get no `client_begin`. Use `Server.players()` in
  `init` to pick them up.
- Chat from a silenced player raises no event.
- `use` needs a filter: `Events.on("use", fn, { targetname = "vault_door" })`. `map_event` may
  have one: `Events.on("map_event", fn, { name = "vault_alarm" })`, or none for every event.
- `activator` and `other` are player handles for players, entity handles for anything else, or nil.

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

API version 3 adds more fields, and a few things scripts can do to players: see
[Players and NPCs](#players-and-npcs-api-version-3).

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

## The world (API version 2)

### Entity handles

`World.spawn`, `World.find`, `World.by_number` and the events give **entity handles**. Like player
handles, the same entity is always the same handle, so it can be a table key. Once the entity is
gone, using its handle is an error; check `entity.valid` first when unsure.

| Field or method | Meaning |
|---|---|
| `valid` | The entity still exists |
| `id` | Entity number (logical entities, which clients never see, are 1024 and up) |
| `classname`, `targetname`, `target` | Its names, or nil |
| `origin`, `angles` | Vectors |
| `is_logical` | Not sent to clients |
| `owner` | The plugin that spawned it, or nil |
| `health` | Its health, or nil if it cannot be damaged |
| `ent:key(name)` | One of its keys, as the map or the Entity System gave it, or nil |
| `ent:use(player)` | Uses it, as a button would. With a player, locks judge that player; with none, it acts as the map's own logic |

Only the plugin's **own** entities (the ones it spawned) can also be:

| Method | What it does |
|---|---|
| `ent:free()` | Removes it |
| `ent:move(to, ms)` | Moves it to `to` over `ms` milliseconds (0: at once). Brush entities (`func_*`) and models (`misc_model*`) only |
| `ent:rotate(angles, ms)` | Turns it the same way |
| `ent:on_use(fn)` | `fn(ent, activator, other)` runs when it is used. It also becomes usable with the Use key. Return `false` to stop its own use (a `misc_model_breakable` breaks when used, unless spawnflag 64) |
| `ent:on_pain(fn)` | `fn(ent, attacker, damage)` when it is hurt (entities that can be damaged) |
| `ent:on_die(fn)` | `fn(ent, attacker)` when it is destroyed, before its own breaking |

`nil` instead of a function removes the callback. A map's entities can be read and used, never
removed, moved or given callbacks: that stays with the Entity System.

**Vectors** are tables `{ x = 1, y = 2, z = 3 }`; `{ 1, 2, 3 }` works too.

### World

```lua
local crate, why = World.spawn("misc_model_breakable", {
    model = "models/map_objects/kejim/cargo_small.md3",
    origin = { x = 100, y = 0, z = 0 }, health = 50, spawnflags = 1 })
if not crate then print("no crate: " .. why) end

World.find{ classname = "func_door", targetname = "gate", near = pos, radius = 512 }  -- up to 64, nearest first
World.by_number(42)
World.effect("env/med_explode2", pos [, direction])
World.sound("sound/weapons/explosions/cargoexplode.wav", pos_or_entity_or_player)
World.music("music/hoth2/hoth2_explore.mp3")     -- "" stops the music
World.shake(pos, radius, intensity, ms)           -- returns how many players it shook
World.distance(a, b)
```

- **`World.spawn(class, keys)`** places an entity exactly as `/entadd` does, with the same checks.
  It returns the handle, or `nil` and the reason (a full map, a model the server does not have,
  `rp_lua_maxents`, `rp_lua_spawnkb`). Keys are strings, numbers, booleans or vectors.
- **Classes scripts cannot spawn:** NPCs and NPC or vehicle spawners, player spawn points,
  `misc_bsp`, weather, `worldspawn` and `target_lua`. `/enthelp <class>` describes every class.
- **A plugin's entities go with it:** when it is disabled, reloaded or switched off by errors.
  They never reach entity files: `/entsave` leaves them out, and `/entedit`, `/entcopy`, `/entcut`,
  `/entrotate` and `/entlockset` refuse them. `/entremove` can still remove one. `/entlist` marks
  them `script:<plugin>`.
- **Effects:** an effect the map already uses plays freely. A new one takes one of the map's 64
  effect slots until the map changes, so at most `rp_lua_newfx` new ones per map.
- **Effects, sounds and shakes** are refused (they return `false` and why) past 64 a server frame
  for all scripts together. Shakes keep `/shakescreen`'s limits: radius up to 1500, intensity 1 to
  10, 1000 to 5000 ms.
- **Inside a callback** (`on_use`, `on_pain`, `on_die`, `on_enter`, `on_leave`, and the `use` and
  `map_event` events), `free()` waits for the next server frame: the game is in the middle of
  something with that entity.

### Zones

```lua
local zone = World.zone{ mins = { x = -64, y = -64, z = 0 }, maxs = { x = 64, y = 64, z = 128 } }
zone:on_enter(function(zone, player) Msg.center(player, "You enter the cantina.") end)
zone:on_leave(function(zone, player) Msg.center(player, "You leave the cantina.") end)
```

- **What a zone is:** an invisible box, up to 8192 units a side, that counts as one of the plugin's
  entities.
- **Enter** fires once, when a player first touches it.
- **Leave** fires half a second after they stop touching it, at once when they die or become a
  spectator, and when they disconnect (their handle still works then).
- Spectators are never in a zone.
- **NPCs** (API 3): a zone made with `npcs = true` sees NPCs too:
  `World.zone{ mins = ..., maxs = ..., npcs = true }`. `on_enter` and `on_leave` then get an
  entity handle for an NPC (nil on leave if the NPC was removed). A zone follows up to 32 NPCs at
  once. An NPC leaves the same way a player does: half a second after it stops touching the zone,
  or at once when it dies or is removed.

### Map events: `target_lua`

Mappers (and admins with `/entadd`) can place a `target_lua` entity with an `event` key:
lower-case letters, digits and `_`, up to 32. Whenever it is used -- by a button, a trigger, a
relay, an ICARUS script or `/entuse` -- the scripts get a `map_event`:

```lua
Events.on("map_event", function(name, player, ent)
    Msg.broadcast("^1The vault alarm is ringing!")
end, { name = "vault_alarm" })
```

### Admin uses

`/entuse`, `/enttrigger` and `/targetuse` run the scripts' `on_use` and `use` handlers too, but a
script cannot stop them (nor what they set off), just as locks cannot. A door opened by
`/enttrigger` or the level 5 stun baton is not reported to scripts.

## Players and NPCs (API version 3)

### Player fields

| Field | Meaning |
|---|---|
| `alive` | Connected, playing (not spectating), not dead and not knocked down |
| `downed` | Knocked down by the Death System, or paralyzed by an admin |
| `health`, `max_health` | Health now, and its maximum |
| `armor`, `max_armor` | Shield now, and its maximum (the Max Shield skill's on a character) |
| `origin`, `yaw` | Where they stand (a vector) and which way they face (degrees) |
| `account_id` | The account's id, or nil when logged out |
| `character_name` | The character's name, or nil |
| `level`, `xp`, `credits`, `skillpoints` | The character's, or nil when not playing one. Read-only |
| `riding` | On a vehicle |
| `minigame` | `"Duel Tournament"`, `"Melee Battle"` or nil |
| `effect` | `"walkthrough"`, `"ghost"`, `"holo"`, `"hidden"` or nil |
| `player:skill(n)` | Skill `n`'s level, its maximum and its name, numbered as `/skillup` numbers them; nil when not on a character |

### Player actions

Each returns `false` and the reason when it is refused, so a script can tell the player why.

| Method | What it does | Refused when |
|---|---|---|
| `player:teleport(pos [, yaw])` | Moves them there, with the teleport effect | Not alive, riding a vehicle, in a mini-game or a duel, admin-protected, or the spot is blocked: a wall, the floor, or someone standing there. Nobody is ever telefragged |
| `player:heal(n)` | Adds up to `n` health, never above the maximum. Returns how much it added | Not alive (a script does not revive a knocked-down player) |
| `player:set_armor(n)` | Sets their shield, kept between 0 and the maximum. Returns the new value | Not alive |
| `player:give(item)` | One item, by its class name: a weapon with its ammo, ammo, or a holdable item, as picking it up would give it | The item is not listed in `rp_lua_items` (none are by default); not alive; in a mini-game; they cannot take it now (full on that ammo, already have that item, in a duel) |
| `player:set_effect(name)` | `"walkthrough"`, `"ghost"`, `"holo"` or `"hidden"`, as `/admwalkthrough`, `/admghost`, `/admholo` and `/admhidden` give them; nil takes it off. The player is not told | Spectating; admin-protected |
| `player:skill_up(n [, count])` | Raises skill `n` by `count` (1 if left out), spending the player's own skillpoints, as `/skillup` does: they see the same message and it is saved. Returns how many levels it added (fewer than `count` when the maximum is nearer) | Not on a character; no such skill (or number 39, no longer in use); already at its maximum; not enough skillpoints |
| `player:skill_down(n [, count])` | Lowers it and gives the skillpoints back, as `/skilldown` does | Not on a character; already at 0 |
| `player:play_sound(file)` | A sound only this player hears, wherever they are | The file is not on the server; too many effects and sounds this frame |

- **Admin Protect:** an admin with the Admin Protect power and the setting on cannot be teleported or
  given an effect by scripts, as by other admins.
- **Effects:** the last one wins. An admin's `/admghost` replaces a script's effect and the other way
  round. When a plugin is disabled or reloaded, the effects it set that nobody has changed since are
  taken off. A map change, logging in or out and changing character take every effect off, as always.
- **The log:** every teleport, item, effect and skill change is written to the server log
  (`Lua: <plugin> teleported <player> to (x y z)`).
- **Allowed items:** `rp_lua_items` in `galaxyrp_server.cfg`, e.g.
  `set rp_lua_items "weapon_blaster ammo_blaster item_medpac"`.

Names, teams, admin powers, credits and XP stay read-only. Killing or reviving a player is not
possible from a script.

### NPCs

```lua
local npc, why = World.npc("stormtrooper", pos, { yaw = 90, team = "enemy" })
if not npc then print("no NPC: " .. why) end
npc:on_death(function(npc, killer) Msg.broadcast("The trooper fell.") end)
```

**`World.npc(type, pos [, options])`** spawns one NPC, the way `/npc spawn` does, at `pos`.
- Options: `yaw`; `team` (`"enemy"`, `"player"`, `"neutral"` or `"free"`); `effect` (`"holo"`,
  `"ghost"` or `"walkthrough"`); `vehicle = true` for a vehicle type (see `/list vehicles`).
- It returns the NPC's entity handle, or `nil` and the reason: an unknown type (see `/list npcs`),
  NPCs switched off (`g_allowNPC 0`), the server near its entity limit, or the plugin's limit.
- **The limit:** `rp_lua_maxnpcs` (8) NPCs per plugin. An NPC counts until it is removed, its corpse
  included (corpses go after `rp_npc_corpse_time`). NPCs count towards `rp_lua_maxents` too.
- The NPC is the plugin's own entity: it goes when the plugin is disabled or reloaded, and never
  reaches entity files. Admins' `/npc` commands work on it as on any NPC.

**Any NPC's handle** (a map's too) also has:

| Field | Meaning |
|---|---|
| `is_npc`, `is_vehicle` | What it is (false for anything that is not an NPC) |
| `npc_type` | e.g. `"stormtrooper"` |
| `team` | `"enemy"`, `"player"`, `"neutral"` or `"free"` |
| `health`, `max_health`, `alive` | `alive` is false once it is dead, even while its corpse lies there |
| `spawner` | The targetname of the map's or Entity System's spawner that made it (while that spawner exists), or nil |
| `effect` | `"walkthrough"`, `"ghost"`, `"holo"` or nil |
| `frozen` | Frozen by `/npc freeze`, a spawner's `npcfreeze` or a script |

The plugin's **own** NPCs can also be:

| Method | What it does | Refused when |
|---|---|---|
| `npc:kill()` | Kills it as `/npc kill` does: it dies and leaves a corpse | Already dead; a vehicle someone is riding |
| `npc:free()` | Removes it at once | — |
| `npc:teleport(pos [, yaw])` | Moves it, with the same checks as a player's teleport | Dead; riding a vehicle; the spot is blocked; it has not finished spawning (the frame it was made) |
| `npc:freeze()`, `npc:unfreeze()` | Stops it, or lets it go, as `/npc freeze` does | A vehicle, or riding one |
| `npc:set_effect(name)` | `"walkthrough"`, `"ghost"`, `"holo"` or nil, as `/npc effect` does | A vehicle, or riding one |
| `npc:on_death(fn)` | `fn(npc, killer)` when it dies | — |

- An NPC has `on_death` and the `damage` event instead of `on_pain` and `on_die`.
- Giving NPCs orders (follow, guard, go somewhere) comes later.

### Events

| Event | Arguments | Filter (optional) | Notes |
|---|---|---|---|
| `player_death` | `player`, `killer`, `mod` | — | A real death, not a knockdown |
| `player_downed` | `player`, `attacker` | — | The Death System knocked them down. Not raised for an admin's `/paralyze` |
| `npc_death` | `npc`, `killer`, `mod` | `{ npc_type = "...", spawner = "..." }` | Any NPC, the map's included |
| `damage` | `target`, `attacker`, `amount`, `mod` | `{ target = "players" }` or `{ target = "npcs" }` | After a hit took health from a player or NPC. `target.health` is already the new health |

- `killer` and `attacker` are a player handle, an entity handle, or nil (the world, a fall, a
  suicide, or an NPC's own `kill()`).
- `mod` is the means of death, e.g. `"MOD_SABER"`, `"MOD_BLASTER"`, `"MOD_FALLING"`.
- These events, and `on_death`, only tell: what a handler returns changes nothing. Inside them,
  `free()` and `kill()` wait for the next server frame.
- `damage` runs on every hit, so keep its handlers short; the usual limits apply.
