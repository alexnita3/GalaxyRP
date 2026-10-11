/*
===========================================================================
DAJ_RP: [Entity Help] the text /enthelp prints (g_enthelp.c): one entry per spawn-table class, per topic, and per
item classname that has something of its own to say. Written from the multiplayer code as it is (the keys each spawn
function reads, their defaults, the spawnflags it tests), not from the editor's QUAKED comments.

Format (rpEntHelp_t): name, flags (RP_EH_TOPIC / RP_EH_MAPONLY / RP_EH_GAMETYPE / RP_EH_REMOVED -- the last for a class
that removes itself as it spawns whatever its keys: tests_maponly checks it against the code), note (why it is not for
the Entity System, or the gametypes it stays in), text, spawnflags ("value|NAME|meaning" lines), keys ("key|default|meaning"
lines), example (what follows /entadd), also (names to read too). No string may hold a double quote, a ';', a '^' or a
tab: they go out inside print commands. A spawnflag or key line stays under a print (RP_EH_PRINT_MAX). Keep it in step
with the code: the tests (tests_enthelp) check that every class has an entry and that every example spawns.
===========================================================================
*/

static const rpEntHelp_t rp_ent_help[] = {
	{ "ammo_all", 0, NULL,
		"Gives some of every ammo: 50 blaster packs, 50 power cells, 50 metallic bolts and 2 rockets (in siege 100, 100, 100 and 5, "
		"plus 2 of each explosive the player carries), up to the rp_max_ caps. count does not change it, rp_add_ammo_scale does not "
		"scale it, and it can always be picked up.",
		NULL,
		NULL,
		"ammo_all",
		"items common" },
	{ "ammo_detpack", 0, NULL,
		"Det pack ammo: 3 (or count), scaled by rp_add_ammo_scale and capped by rp_max_detpack_ammo. Picking it up also gives the "
		"det pack weapon once the player has any det packs.",
		NULL,
		NULL,
		"ammo_detpack",
		"items common weapon_det_pack" },
	{ "ammo_force", RP_EH_MAPONLY, "It gives nothing: pickups never add Force ammo.",
		"Force ammo from the item table. Taking it adds nothing, since no pickup fills Force ammo, so placed it is only an empty "
		"pickup.",
		NULL,
		NULL,
		NULL,
		"items common" },
	{ "ammo_thermal", 0, NULL,
		"Thermal detonator ammo: 4 (or count), scaled by rp_add_ammo_scale and capped by rp_max_thermal_ammo. Picking it up also "
		"gives the thermal detonator weapon once the player has any detonators.",
		NULL,
		NULL,
		"ammo_thermal",
		"items common weapon_thermal" },
	{ "ammo_tripmine", 0, NULL,
		"Trip mine ammo: 3 (or count), scaled by rp_add_ammo_scale and capped by rp_max_tripmine_ammo. Picking it up also gives the "
		"trip mine weapon once the player has any mines.",
		NULL,
		NULL,
		"ammo_tripmine",
		"items common weapon_trip_mine" },
	{ "common", RP_EH_TOPIC, NULL,
		"The keys and rules every class shares. /entadd <classname> <key> <value> ... places an entity where you stand (or at the "
		"/entorigin point), /entaddaim on the surface you aim at. /entedit <id> lists its keys, /entedit <id> <key> <value> [more "
		"pairs] sets them and respawns it in place, and the value zykremovekey removes a key, with no id it acts on the entity you "
		"aim at. Key names ignore case, an entity holds at most 64 pairs, its classname cannot be changed, and the map's own "
		"entities (M in /entlist) are not edited: /entcopy them.",
		NULL,
		"classname||The class. /entedit cannot change it: /entremove it and /entadd the new class.\n"
		"origin|where you stand|Position x y z. /entadd uses the /entorigin point when one is set, /entaddaim the surface aimed at "
		"(a typed origin is ignored).\n"
		"angle|0|Facing as one yaw in degrees, the same as angles 0 <angle> 0. Handy on the command line.\n"
		"angles|0 0 0|Pitch yaw roll in degrees, typed in quotes. With none given, /entadd takes the /entorigin angles when set.\n"
		"spawnflags|0|The sum of the flag values wanted (1 and 4 make 5). Each class gives the numbers its own meaning.\n"
		"targetname||The name others use it by (their target keys, scripts). What being used does is up to the class.\n"
		"target||When the entity fires its targets (the class decides when), every entity whose targetname matches is used. Loops "
		"are cut off and logged.\n"
		"target2||target2 to target6: more targets that only some classes fire, each at its own moment.\n"
		"targetshadername||With targetshadernewname: when it fires its targets, every surface with this shader shows the new one "
		"instead.\n"
		"script_targetname||The name ICARUS scripts find it by. Puts it in a networked slot.\n"
		"spawnscript||Script run when it spawns, named under scripts/ (added if missing). Any key ending in script puts the entity "
		"in a networked slot.\n"
		"usescript||Script run each time it is used, for the classes that run it (most do).\n"
		"painscript||Script run when it is damaged (breakables, NPCs).\n"
		"deathscript||Script run when damage kills it.\n"
		"angerscript||angerscript, attackscript, awakescript, blockedscript, delayscript, fleescript, lostenemyscript, "
		"mindtrickscript, victoryscript, ffirescript, ffdeathscript: NPC scripts only.\n"
		"parm1||parm1 to parm16: values ICARUS scripts read from the entity, up to 63 characters each.\n"
		"notsingle|0|1: not spawned in the single player gametype.\n"
		"notteam|0|1: not spawned in team gametypes (team, siege, ctf, cty).\n"
		"notfree|0|1: not spawned in the others (ffa, holocron, jedimaster, duel, powerduel, single).\n"
		"gametype||Spawned only when the value contains the current gametype's name (ffa holocron jedimaster duel powerduel single "
		"team siege ctf cty), e.g. ffa,team. powerduel also lets it spawn in duel.\n"
		"nological|0|1: keeps a class that would be logical in a networked slot (only matters with logical entities on).\n"
		"team||Entities with the same team value act together: movers move as one, led by one of them (which takes over a "
		"targetname), items show one at a time. Triggers never join. A placed entity joins its team when it spawns.\n"
		"soundSet||An ambient sound set from sound/sound.txt, for the classes that play one. A name the clients do not have is "
		"refused, since it would disconnect them.\n"
		"lock||An account lock: only logged-in players whose account is on that lock's list (/entlockadd) may use, touch or "
		"trigger it, a vehicle by its pilot. NPCs never may. Timers and map logic with no player behind them, /entuse, /enttrigger "
		"and the upgraded Stun Baton always pass, and so does what they set off, later too. A lock name is 1 to 31 letters, digits, "
		"_ or -, and none means no lock. In a mover team one locked member locks them all. A trigger fired with the Use or fire "
		"button refuses when the button is pressed, not when walked into. trigger_hurt, trigger_space, trigger_shipboundary, "
		"trigger_hyperspace and target_kill ignore it: a lock would only spare the players not on its list. /entlockset, on the "
		"entity aimed at or an id, sets it without a respawn, and also locks the map's own doors.\n"
		"delay||No common meaning: the classes that read it say what it does.\n"
		"wait||No common meaning: the classes that read it say what it does (often a reset or respawn time).\n"
		"random||No common meaning: usually a random amount added to or taken from wait.",
		NULL,
		NULL },
	{ "dispenser", RP_EH_TOPIC, NULL,
		"The ammo and shield floor units and the three model power converters (ammo, shield, health) work alike. "
		"A player holds Use on one: every 100 ms it gives what the player lacks and takes it from its charge (count), unless nodrain is set. "
		"Left alone it regains 1 charge every chargerate ms, up to count. An empty one gives nothing, plays its empty sound and does not recharge while a player who needs it keeps holding Use. "
		"A draining one shows its charge as a fill bar. It loops a sound while giving and plays a done sound when it stops.",
		NULL,
		"count|200|Its full charge, which it starts with. Each class says what one use costs.\n"
		"chargerate|100|Milliseconds per point of charge regained while nobody is using it.\n"
		"nodrain|0|1: never drains, gives without end and shows no fill bar.\n"
		"model||An .md3 to show instead of its own: as given, or under models/ with .md3 added. The Entity System refuses one not on the server, not an .md3 or with no model slot (a map's own shows its default model and logs why).",
		NULL,
		"common misc_ammo_floor_unit misc_shield_floor_unit misc_model_ammo_power_converter misc_model_shield_power_converter misc_model_health_power_converter" },
	{ "emplaced_eweb", 0, NULL,
		"Single player's E-Web. In GalaxyRP it spawns exactly as an emplaced_gun: the same turret chair model, health, keys and spawnflags.",
		"1|CANRESPAWN|Comes back after it is destroyed (4 seconds plus count ms later), with 320 health instead of 800.\n"
		"1024|NO_DROP|Stays at its origin height instead of dropping onto the floor below.",
		"angle||The direction it points: the base facing the gunner turns from.\n"
		"constraint|60|Degrees the gunner can turn it to each side of its base facing, 1 to 180.\n"
		"count|600|With CANRESPAWN: milliseconds added to the 4 second respawn delay.",
		"emplaced_eweb angle 90",
		"common emplaced_gun" },
	{ "emplaced_gun", 0, NULL,
		"A mounted heavy gun on a turret chair. A player standing behind it, facing the way it points, within 64 units and not crouching, mounts it with the use key and fires it, and the use key again gets off. It has 800 health (320 with CANRESPAWN) and a health bar under the crosshair. Destroyed, it flashes for 3 seconds, then explodes (80 damage in 128 units) and smokes, and stays wrecked unless CANRESPAWN.",
		"1|CANRESPAWN|Comes back after it is destroyed (4 seconds plus count ms later), with 320 health instead of 800.\n"
		"1024|NO_DROP|Stays at its origin height instead of dropping onto the floor below.",
		"angle||The direction it points: the base facing the gunner turns from.\n"
		"constraint|60|Degrees the gunner can turn it to each side of its base facing, 1 to 180.\n"
		"count|600|With CANRESPAWN: milliseconds added to the 4 second respawn delay.",
		"emplaced_gun angle 90",
		"common emplaced_eweb" },
	{ "func_bobbing", 0, NULL,
		"A brush or .md3 model that bobs up and down (or along X or Y) from spawn, in a smooth sine motion. A player or NPC in its way is killed outright unless CRUSH_THROUGH is set. Using it (targetname, PLAYER_USE, the Stun Baton) stops it where it is, and the next use carries on from the same place in its cycle.",
		"1|X_AXIS|Bobs along the X axis instead of Z.\n"
		"2|Y_AXIS|Bobs along the Y axis instead of Z.\n"
		"4|START_OFF|Starts still, at the place in the cycle phase gives: the first use starts it.",
		"height|32|How far it bobs each way, in units.\n"
		"speed|4|Seconds for one full cycle.\n"
		"phase|0|Where in the cycle it starts, 0 to 1.\n"
		"dmg|2|With CRUSH_THROUGH: damage to a player in its way (without it, a blocker is killed).",
		"func_bobbing model *1 height 16 speed 3",
		"common mover" },
	{ "func_breakable", 0, NULL,
		"A brush or .md3 model that breaks into chunks when its health runs out or when it is used, fires its target and is removed. Bryar pistol, DEMP2 and plain melee do not hurt it (heavy melee does). One with 10 health or less, or THIN, also breaks when a player runs into it hard. Its spawnflags are not the other movers': 128 is PLAYER_USE and there is no INACTIVE.",
		"1|INVINCIBLE|No default health: with no health key it takes no damage and only breaks when used.\n"
		"8|THIN|Breaks easily when a player runs into it, like glass.\n"
		"16|SABERONLY|Only sabers damage it.\n"
		"32|HEAVY_WEAP|Only heavy weapons damage it: rockets, explosives, repeater alt fire, concussion, sabers, crushing and the like.\n"
		"64|USE_NOT_BREAK|Using it fires its target instead of breaking it. Damage still breaks it.\n"
		"128|PLAYER_USE|Players can use it with the use key.\n"
		"2048|NO_EXPLOSION|No explosion effect when it breaks (the chunks still fly).",
		"health|10|Hit points. With INVINCIBLE and no health it cannot be damaged.\n"
		"material|0|Chunks: 0 metal, 1 glass, 2 sparks only, 3 metal and sparks, 4 brown stone, 5 tan stone, 6 glass and metal, 7 metal2, 8 none, 9 grey stone, 10 metal3, 11 yellow crate, 12 grate, 13 rope, 14 red crate, 15 white metal, 16 snowy rock. Stone ones also chip when hit.\n"
		"radius|1|Multiplies the number of chunks. Use this, not numchunks, which it overwrites.\n"
		"chunksize|1|Multiplies the size of the chunks.\n"
		"playfx||An effect played where it breaks. One starting with * is a weather command and is refused (a map's own plays no effect instead, logged).\n"
		"splashDamage|0|Damage of an explosion when it breaks (needs splashRadius too).\n"
		"splashRadius|0|Radius of that explosion, in units.\n"
		"delay|0|Whole seconds between being destroyed or used and breaking.\n"
		"target||Fired when it breaks.\n"
		"paintarget||Fired when a player damages it without breaking it.\n"
		"wait|0|Milliseconds before paintarget can fire again. -1: paintarget fires once only.\n"
		"showhealth|0|Non-0: a health bar shows under the crosshair of a player aiming at it.",
		"func_breakable model *1 health 50 material 4",
		"common mover func_glass" },
	{ "func_button", 0, NULL,
		"A brush or .md3 model that, touched by a player or NPC, slides in the direction of its angle by its own size minus lip, fires its target as it starts, waits, and goes back. With health it is pressed by shooting it instead of touching. Being used by a trigger or another entity presses it the same way.",
		"2|FORCE_PUSH|Force Push also presses it.",
		"angle||Direction it moves when pressed (-1 up, -2 down).\n"
		"speed|40|Units per second.\n"
		"wait|1|Seconds before it goes back. -1: stays pressed for good.\n"
		"lip|4|Units of it left showing at the end of its move.\n"
		"health|0|Non-0: shooting it presses it (it takes no real damage), and touching does not.\n"
		"delay|0|Milliseconds between being used and moving (not seconds as on a door).\n"
		"target||Fired as it starts moving.",
		"func_button model *1 angle 90 target door1",
		"common mover func_door" },
	{ "func_door", 0, NULL,
		"A brush or .md3 model that slides in the direction of its angle by its own size minus lip, waits, and closes again. With no targetname, health, PLAYER_USE or FORCE_ACTIVATE it opens by itself when someone walks up (its trigger reaches 120 units out on each side of its thinnest axis). With a targetname it opens only when used. Blocked, it hurts the blocker by dmg and goes back.",
		"1|START_OPEN|Spawns open and closes when used (its two positions are swapped).\n"
		"2|FORCE_ACTIVATE|Opened and closed only by Force Push or Pull aimed at it while it is at rest.\n"
		"4|CRUSHER|Does not go back when blocked: it keeps hurting what blocks it.\n"
		"8|TOGGLE|Stays open until used again (wait is not used).\n"
		"16|LOCKED|Starts locked, its shader on the first frame: walking up does nothing until it is used once, which unlocks it. A non-TOGGLE door then also drops its targetname and opens by walking up.",
		"angle||Direction it opens in (-1 up, -2 down).\n"
		"speed|400|Units per second.\n"
		"wait|2|Seconds before it closes. -1: stays open for good.\n"
		"lip|8|Units of it left showing when open.\n"
		"dmg|2|Damage to what blocks it. Negative: none.\n"
		"delay|0|Whole seconds between being used and moving.\n"
		"health|0|Non-0: shooting it opens it (it takes no real damage), and it gets no walk-up trigger.\n"
		"targetname||Using it opens it, and it gets no walk-up trigger.\n"
		"teamallow|0|Team (1 red, 2 blue) whose players still open it by walking up while it is LOCKED.\n"
		"vehopen|0|Non-0: vehicles and their riders can open it by walking up.",
		"func_door model *1 angle -1 wait 4",
		"common mover func_button" },
	{ "func_glass", 0, NULL,
		"A brush or .md3 model of breakable glass. Damage that takes its health breaks it into shards, a player running or jumping into it breaks it too, and so does using it. It fires its target when it breaks and is removed. PLAYER_USE has no effect on it.",
		"1|NO_DAMAGE|Cannot be damaged: only using it breaks it.",
		"health|1|Damage it takes to break.\n"
		"maxshards|0|Most shards it breaks into. 0 leaves it to the client.\n"
		"target||Fired when it breaks.",
		"func_glass model *1 health 10",
		"common mover func_breakable" },
	{ "func_goodie_panel", 0, NULL,
		"A brush or .md3 panel a player uses with the use key (PLAYER_USE is not needed). If they carry a goodie key, one is used up, it plays the pass sound, fires its target and is spent for good. Without one it plays the fail sound and fires target2. It does not move. A player gets goodie keys (up to 5) from a dead NPC whose message is goodie, and loses them on death or disconnect.",
		"128|INACTIVE|Cannot be used until a target_activate activates it.",
		"target||Fired when a player with a goodie key uses it.\n"
		"target2||Fired when a player without one uses it.",
		"func_goodie_panel model *1 target door1",
		"common mover func_security_panel" },
	{ "func_group", RP_EH_MAPONLY | RP_EH_REMOVED, "func_group is a map-editor grouping of brushes, nothing in the game.",
		"A map-editor grouping: its brushes become part of the world when the map is compiled, so the game has nothing to spawn. It frees itself at once.",
		NULL,
		NULL,
		NULL,
		"common" },
	{ "func_pendulum", 0, NULL,
		"A brush or .md3 model that swings back and forth from spawn, pivoting on its origin (a map's pendulum needs an origin brush at the pivot). Its swing time follows its length below the pivot (its mins height, at least 8) and gravity. A player or NPC in its way is killed outright unless CRUSH_THROUGH is set. Using it (targetname, PLAYER_USE, the Stun Baton) stops it where it is in its swing, and the next use carries on from there.",
		"4|START_OFF|Starts still, at the point of its swing phase gives: the first use starts it.",
		"speed|30|Degrees it swings to each side.\n"
		"phase|0|Where in the swing it starts, 0 to 1.\n"
		"angles||Its orientation, also for an .md3 model (angles2 is not used). It swings around its own roll axis, so the yaw picks the plane it swings in.\n"
		"dmg|2|With CRUSH_THROUGH: damage to a player in its way (without it, a blocker is killed).",
		"func_pendulum model *1 speed 45",
		"common mover" },
	{ "func_plat", 0, NULL,
		"A brush or .md3 lift. It spawns lowered, height units below where it is placed, and rises to its placed position when a player steps onto it at the bottom (with a targetname, only when used). At the top it waits 1 second, longer while a live player stands on it, then goes back down. Blocked, it hurts the blocker by dmg and reverses. /spawnplatform makes one under your feet with a catwalk model: it stays put, or with a height given it rises that far, lowered to fit under a ceiling.",
		"4096|NO_RIDER_WAIT|Goes down after its 1 second even with a live player standing on it.",
		"height||How far it travels, in units. Default: its model height minus lip.\n"
		"lip|8|Taken off the model height for the default height.\n"
		"speed|200|Units per second.\n"
		"dmg|2|Damage to what blocks it.\n"
		"delay|0|Milliseconds between being used and moving.\n"
		"targetname||Only using it moves it: it gets no step-on trigger.\n"
		"wait||Not used: the wait at the top is always 1 second.",
		"func_plat model *1 height 128",
		"common mover" },
	{ "func_rotating", 0, NULL,
		"A brush or .md3 model that spins around its origin (a map's one needs an origin brush), around Z unless X_AXIS or Y_AXIS is set. Blocked, it stops until the way is clear, unless IMPACT. Using it (targetname, PLAYER_USE, the Stun Baton) stops it where it is, and the next use starts it again, with its soundSet's start, loop and stop sounds. One with a targetname starts still unless START_ON, one without spins from spawn. With health it can be broken instead (the func_breakable keys apply): it always spins, and using it breaks it.",
		"1|START_ON|With a targetname: spins from spawn (without it, the first use starts it). One without a targetname always does.\n"
		"2|RADAR|Shown on the radar, as Siege asteroids are.\n"
		"4|X_AXIS|Spins around the X axis (roll) instead of Z.\n"
		"8|Y_AXIS|Spins around the Y axis (pitch) instead of Z.\n"
		"16|IMPACT|Hurts anything it touches while spinning by dmg (10000 when not given) instead of being blocked.",
		"speed|100|Degrees per second.\n"
		"spinangles||Degrees per second on pitch, yaw and roll at once, in place of speed and the axis flags.\n"
		"trdelta||As spinangles, but then the axis chosen by the flags is set to speed.\n"
		"dmg|2|Damage with IMPACT (10000 when not given) or CRUSH_THROUGH.\n"
		"model2scale|0|Size of model2 in percent (0 or 100 normal, up to 1023).\n"
		"health|0|Non-0: it can be broken, as a func_breakable.",
		"func_rotating model *1 speed 45",
		"common mover func_breakable" },
	{ "func_security_panel", 0, NULL,
		"A brush or .md3 panel a player uses with the use key (PLAYER_USE is not needed). If they carry the security key its message names (or it has no message), the key is taken, it plays the pass sound, fires its target and is spent for good. Otherwise it plays the fail sound and fires target2. It does not move. A player carries one security key at a time, from an item_security_key or a dead NPC, and loses it on death or disconnect.",
		"128|INACTIVE|Cannot be used until a target_activate activates it.",
		"message||Name of the security key it needs. Empty: anyone opens it (and loses any key they carry).\n"
		"target||Fired when it is opened.\n"
		"target2||Fired when it is used without the key.",
		"func_security_panel model *1 message bluekey target door1",
		"common mover item_security_key misc_security_panel" },
	{ "func_static", 0, NULL,
		"A brush or .md3 model that just sits there. Using it fires its target, and with SWITCH_SHADER flips its shader frame, so it can serve as a usable prop or a Force-pushed switch. Unlike most movers it faces by angles, also with an .md3 model, and a brush model turns with them too.",
		"1|F_PUSH|Force Push aimed at it uses it.\n"
		"2|F_PULL|Force Pull aimed at it uses it.\n"
		"4|SWITCH_SHADER|Each use flips its shader animation between frames 0 and 1.\n"
		"2048|BROADCAST|Sent to every client wherever they are (for something huge).",
		"angles||Its orientation (pitch yaw roll), also for an .md3 model (angles2 is not used).\n"
		"model2scale|0|Size of model2 in percent (0 or 100 normal, up to 1023).\n"
		"target||Fired each time it is used.",
		"func_static model *1",
		"common mover" },
	{ "func_timer", 0, NULL,
		"An invisible timer that, while on, fires its target over and over, every wait seconds give or take random. Using it switches it on and off (it fires at once when switched on), and its targets see whoever switched it on as the activator.",
		"1|START_ON|Runs from spawn.",
		"wait|1|Seconds between firings, at least 0.1.\n"
		"random|1|Up to this many seconds added or taken off each time, at most wait minus 0.1 (so by default it fires every 0.1 to 1.9 seconds).\n"
		"target||What it fires.",
		"func_timer target lamp1 wait 5 random 0 spawnflags 1",
		"common" },
	{ "func_train", 0, NULL,
		"A brush or .md3 model that travels along a chain of path_corners: target names the first, each corner targets the next, and a corner targeting an earlier one makes a loop. It starts at the first corner and runs at once, unless it has a targetname and no START_ON: then it waits at its own spot until used. A brush train needs an origin brush, the point that follows the path. Blocked, it waits until the way is clear.",
		"1|START_ON|Runs from spawn even with a targetname.\n"
		"2|TOGGLE|Does nothing: using a running train does not stop it.\n"
		"4|BLOCK_STOPS|Sets dmg to 0 (a blocked train always waits).",
		"target||The first path_corner. Required: it is not spawned without one.\n"
		"speed|100|Units per second, unless a path_corner sets its own.\n"
		"dmg|2|With CRUSH_THROUGH: damage to a player in its way.",
		"func_train model *1 target p1",
		"common mover path_corner" },
	{ "func_usable", 0, NULL,
		"A brush or .md3 model that each use switches between there (solid and visible) and gone. Going, it fires its target, coming back, its target2. With endframe it instead steps its shader animation one frame per use and fires its target. With ALWAYS_ON it never switches: a use only fires its target. With health, hits use it.",
		"1|START_OFF|Starts gone.\n"
		"8|ALWAYS_ON|Never switches: a use fires its target, and it cannot be used again until wait has passed (never, without wait).\n"
		"16|BLOCKCHECK|Coming back, it waits until nobody stands inside it.",
		"target||Fired when it goes (and on every use with endframe or ALWAYS_ON).\n"
		"target2||Fired when it comes back.\n"
		"wait|0|With ALWAYS_ON: seconds before it can be used again. 0: once only.\n"
		"health|0|Non-0: every hit uses it, and the hit that takes the last of its health uses it a last time and ends that.\n"
		"endframe|0|Frames of its shader animation minus 1: each use steps one frame, back to 0 after the last.",
		"func_usable model *1 targetname wall1",
		"common mover func_wall" },
	{ "func_wall", 0, NULL,
		"A brush or .md3 model that each use switches between there (solid and visible) and gone, also opening or closing the map's area portal it sits in, unless it started gone.",
		"1|START_OFF|Starts gone (and then never touches area portals).",
		NULL,
		"func_wall model *1 targetname wall1",
		"common mover func_usable" },
	{ "fx_rain", 0, NULL,
		"Rain over the whole map for every player, from the moment it spawns. Only the lowest of the flags 1, 2, 4 and 8 counts. It lasts until the map changes, even if the entity is removed. Refused once /admweather has taken over this map's weather: use /admweather then. Refused, and removed, when another weather entity already adds all of its weather (it would add nothing), or when the map's weather would hold more than 6 weather commands, 5 particle effects (rain, snow, sand, fog, dust) or 10 winds, the map's own included. A map's own follows the same rules: one left out is logged.",
		"1|LIGHT|Light drizzle.\n"
		"2|MEDIUM|Ordinary rain, also what no spawnflags gives.\n"
		"4|HEAVY|Downpour with heavy fog.\n"
		"8|ACID|Acid rain.\n"
		"32|MISTY_FOG|Adds drifting misty fog (alone: fog and no rain).",
		NULL,
		"fx_rain spawnflags 4",
		"common fx_snow fx_wind" },
	{ "fx_runner", 0, NULL,
		"Plays an effect at its spot, over and over every delay ms, along its angles (straight up by default) or toward its target. With a targetname, using it switches it on and off, or plays it once with ONESHOT. Refused if the effect file is not on the server. "
		"An fxFile starting with * is weather instead (fxFile *rain, or *spacedust 500 typed in quotes): the same as zyk_weather's message, under the same rules, and the fx_runner itself then plays nothing.",
		"1|STARTOFF|Starts off: using it switches it on.\n"
		"2|ONESHOT|Plays only when used, once per use.\n"
		"4|DAMAGE|Each time it plays, does splashDamage within splashRadius (at most every 100 ms).",
		"fxFile||The effect, under effects/ with .efx optional, for example env/small_fire. Starting with *: weather (see zyk_weather).\n"
		"delay|200|Milliseconds between plays.\n"
		"random|0|Up to this many milliseconds added to each delay.\n"
		"angles|-90 0 0|Direction it plays in (the default is straight up).\n"
		"target||An entity to aim at (an info_notnull, say), instead of angles.\n"
		"target2||Fired each time the effect plays.\n"
		"targetname||Needed to use it at all.\n"
		"splashDamage|5|With DAMAGE: damage each time.\n"
		"splashRadius|16|With DAMAGE: radius, in units.\n"
		"soundset||An ambient sound set: start sound when switched on, a loop while on, stop sound when switched off.",
		"fx_runner fxFile env/small_fire",
		"common info_notnull" },
	{ "fx_snow", 0, NULL,
		"Snow over the whole map, with fog and a steady wind, for every player from the moment it spawns. It lasts until the map changes, even if the entity is removed. Refused once /admweather has taken over this map's weather: use /admweather then. Refused, and removed, when another weather entity already adds all of its weather (it would add nothing), or when the map's weather would hold more than 6 weather commands, 5 particle effects (rain, snow, sand, fog, dust) or 10 winds, the map's own included. A map's own follows the same rules: one left out is logged.",
		NULL,
		NULL,
		"fx_snow",
		"common fx_rain fx_wind" },
	{ "fx_spacedust", 0, NULL,
		"Floating space dust over the whole map for every player, from the moment it spawns. It lasts until the map changes, even if the entity is removed. Refused once /admweather has taken over this map's weather: use /admweather then. Refused, and removed, when another weather entity already adds all of its weather (it would add nothing), or when the map's weather would hold more than 6 weather commands, 5 particle effects (rain, snow, sand, fog, dust) or 10 winds, the map's own included. A map's own follows the same rules: one left out is logged.",
		NULL,
		"count|1000|Number of dust particles, 1 to 10000.",
		"fx_spacedust count 1000",
		"common fx_rain fx_snow" },
	{ "fx_wind", 0, NULL,
		"Wind over the whole map for every player from the moment it spawns, and, with the fog flags, drifting fog. With none of the flags 1, 2, 4, 32 and 64 it adds nothing and is refused. It lasts until the map changes, even if the entity is removed. Refused once /admweather has taken over this map's weather: use /admweather then. Refused, and removed, when another weather entity already adds all of its weather (it would add nothing), or when the map's weather would hold more than 6 weather commands, 5 particle effects (rain, snow, sand, fog, dust) or 10 winds, the map's own included. A map's own follows the same rules: one left out is logged.",
		"1|NORMAL|Light random wind.\n"
		"2|CONSTANT|Steady wind along its angles at speed.\n"
		"4|GUSTING|Random gusts.\n"
		"8|SWIRLING|Does nothing (switched off in the code).\n"
		"32|FOG|Misty fog.\n"
		"64|LIGHT_FOG|Light fog.",
		"speed|500|With CONSTANT: the wind speed (each part of the wind held to -10000 to 10000).\n"
		"angles||With CONSTANT: the direction the wind blows.",
		"fx_wind spawnflags 4",
		"common fx_rain fx_snow" },
	{ "gametype_item", RP_EH_GAMETYPE, "CTF and Capture the Ysalamiri only: in any other gametype the flag it becomes removes itself.",
		"A CTF flag placeholder. With a targetname containing red_flag or blue_flag it becomes that team's CTF flag item. Any other name, or none, leaves an inert point. Flags can only be picked up in CTF and Capture the Ysalamiri.",
		NULL,
		"targetname||red_flag or blue_flag anywhere in it picks the flag.",
		"gametype_item targetname red_flag",
		"common" },
	{ "info_camp", 0, NULL,
		"An invisible named point and nothing more: it does nothing by itself. Entities that use a target as a position read its origin, the same as info_notnull or target_position: a trigger_push aim, a trigger_teleport or target_teleporter destination, an fx_runner aim, a misc_portal_surface camera, the intermission view. Give it a targetname.",
		NULL,
		"targetname||The name another entity's target key finds this point by.",
		"info_camp targetname aimpoint1",
		"common info_notnull target_position" },
	{ "info_jedimaster_start", RP_EH_MAPONLY, "The Jedi Master saber cannot be added after map load: it could never be removed, and the map's own is the only one.",
		"Where the Jedi Master saber lies at map start in the Jedi Master gametype: the first player to touch it becomes the Jedi Master (all force powers at level 3, full force, 200 health). When its holder dies it is thrown towards the killer, and if nobody picks it up within 20 seconds it returns here. In any other gametype the map's own removes itself at load.",
		NULL,
		NULL,
		NULL,
		"common" },
	{ "info_notnull", 0, NULL,
		"An invisible named point: it does nothing by itself, but entities that use a target as a position read its origin: a trigger_push aim, a trigger_teleport or target_teleporter destination, an fx_runner aim, a misc_portal_surface camera, the intermission view (info_player_intermission target), an NPC script navgoal. Same as target_position.",
		NULL,
		"targetname||The name another entity's target key finds this point by.",
		"info_notnull targetname aimpoint1",
		"common target_position" },
	{ "info_null", 0, NULL,
		"An invisible aim point for other entities, like info_notnull. It is kept for the whole map only when it has a targetname, so the ref_tags, fx_runners, spotlights and cameras that name it can read its origin. Without a targetname it removes itself at once, and it is also freed when the entity table is nearly full.",
		NULL,
		"targetname||Required: the name other entities' target key uses. Without it the entity removes itself.",
		"info_null targetname aimpoint2",
		"common info_notnull" },
	{ "info_player_deathmatch", 0, NULL,
		"A player spawn point: used in FFA and as the fallback of every other gametype. The player appears 9 units above it facing its angle, picked at random among the free points furthest from where they died, and its target is fired with the player as activator. Ones you add can be removed, the map's own cannot.",
		"1|INITIAL|The first spawn of a player on the server's own machine (a listen-server host) goes here. Does nothing on a dedicated server.",
		"angle|0|The direction (yaw, degrees) players face when they spawn here.\n"
		"target||Fired each time a player spawns here, with the player as activator. On single-player maps the map's start script is fired by the first spawn only.\n"
		"nobots|0|1: bots never spawn here.\n"
		"nohumans|0|1: human players never spawn here.",
		"info_player_deathmatch angle 90",
		"common spawnpoint info_player_start" },
	{ "info_player_duel", 0, NULL,
		"A spawn point for the Duel gametype only: both duelists spawn on these, at a random free one among those furthest from where they were. With none free it falls back to info_player_deathmatch. In any other gametype it is never used.",
		NULL,
		"angle|0|The direction (yaw, degrees) players face when they spawn here.\n"
		"target||Fired each time a player spawns here, with the player as activator.\n"
		"nobots|0|1: bots never spawn here.\n"
		"nohumans|0|1: human players never spawn here.",
		"info_player_duel angle 180",
		"common spawnpoint info_player_duel1 info_player_duel2" },
	{ "info_player_duel1", 0, NULL,
		"A spawn point for the Power Duel gametype only: the lone duelist spawns on these, at a random free one among those furthest from where they were. With none free it falls back to info_player_deathmatch. In any other gametype it is never used.",
		NULL,
		"angle|0|The direction (yaw, degrees) players face when they spawn here.\n"
		"target||Fired each time a player spawns here, with the player as activator.\n"
		"nobots|0|1: bots never spawn here.\n"
		"nohumans|0|1: human players never spawn here.",
		"info_player_duel1 angle 0",
		"common spawnpoint info_player_duel2" },
	{ "info_player_duel2", 0, NULL,
		"A spawn point for the Power Duel gametype only: the two paired duelists spawn on these, at a random free one among those furthest from where they were. With none free it falls back to info_player_deathmatch. In any other gametype it is never used.",
		NULL,
		"angle|0|The direction (yaw, degrees) players face when they spawn here.\n"
		"target||Fired each time a player spawns here, with the player as activator.\n"
		"nobots|0|1: bots never spawn here.\n"
		"nohumans|0|1: human players never spawn here.",
		"info_player_duel2 angle 180",
		"common spawnpoint info_player_duel1" },
	{ "info_player_intermission", 0, NULL,
		"The camera point of the end-of-match scoreboard, and where spectators are put and look from. It faces its angle, or towards its target if it has one. Only the first one the game finds is used, and with none a deathmatch spawn point is used instead. One added inside a wall or outside the map is refused.",
		NULL,
		"angle|0|The direction (yaw, degrees) the view faces when there is no target.\n"
		"target||An entity (info_notnull, target_position...) the view turns to face.",
		"info_player_intermission angle 45",
		"common spawnpoint info_notnull" },
	{ "info_player_intermission_blue", 0, NULL,
		"Siege only: the scoreboard camera when team 2 (the defenders) wins a round, used instead of info_player_intermission, and its target2 is fired when it is chosen. In any other gametype it is never used.",
		NULL,
		"angle|0|The direction (yaw, degrees) the view faces when there is no target.\n"
		"target||An entity the view turns to face.\n"
		"target2||Fired when this point is chosen at the end of the round.",
		"info_player_intermission_blue angle 90",
		"common spawnpoint info_player_intermission" },
	{ "info_player_intermission_red", 0, NULL,
		"Siege only: the scoreboard camera when team 1 (the attackers) wins a round, used instead of info_player_intermission, and its target2 is fired when it is chosen. In any other gametype it is never used.",
		NULL,
		"angle|0|The direction (yaw, degrees) the view faces when there is no target.\n"
		"target||An entity the view turns to face.\n"
		"target2||Fired when this point is chosen at the end of the round.",
		"info_player_intermission_red angle 90",
		"common spawnpoint info_player_intermission" },
	{ "info_player_siegeteam1", 0, NULL,
		"Siege: a spawn point for team 1. Only enabled points are used, a random free one, preferring those whose idealclass is the player's class. Using it toggles it on and off, so a map can move a team's spawns as objectives fall. In any other gametype it becomes a plain info_player_deathmatch (and then reads nobots and nohumans).",
		NULL,
		"startoff|0|1: starts disabled until used (siege only).\n"
		"idealclass||Siege class name (as in the .scl file) that prefers this point.\n"
		"targetname||Using it switches it on or off (siege only).\n"
		"angle|0|The direction (yaw, degrees) players face when they spawn here.\n"
		"target||Fired each time a player spawns here, with the player as activator.",
		"info_player_siegeteam1 angle 0",
		"common spawnpoint info_player_siegeteam2" },
	{ "info_player_siegeteam2", 0, NULL,
		"Siege: a spawn point for team 2. Only enabled points are used, a random free one, preferring those whose idealclass is the player's class. Using it toggles it on and off, so a map can move a team's spawns as objectives fall. In any other gametype it becomes a plain info_player_deathmatch (and then reads nobots and nohumans).",
		NULL,
		"startoff|0|1: starts disabled until used (siege only).\n"
		"idealclass||Siege class name (as in the .scl file) that prefers this point.\n"
		"targetname||Using it switches it on or off (siege only).\n"
		"angle|0|The direction (yaw, degrees) players face when they spawn here.\n"
		"target||Fired each time a player spawns here, with the player as activator.",
		"info_player_siegeteam2 angle 180",
		"common spawnpoint info_player_siegeteam1" },
	{ "info_player_start", 0, NULL,
		"The same as info_player_deathmatch: it turns into one when it spawns (its classname becomes info_player_deathmatch), so it is a player spawn point in FFA and the fallback of every other gametype.",
		"1|INITIAL|The first spawn of a player on the server's own machine (a listen-server host) goes here. Does nothing on a dedicated server.",
		"angle|0|The direction (yaw, degrees) players face when they spawn here.\n"
		"target||Fired each time a player spawns here, with the player as activator.\n"
		"nobots|0|1: bots never spawn here.\n"
		"nohumans|0|1: human players never spawn here.",
		"info_player_start angle 270",
		"common spawnpoint info_player_deathmatch" },
	{ "info_player_start_blue", 0, NULL,
		"Team FFA only: blue team players spawn here, at a random free one among those furthest from where they died, falling back to info_player_deathmatch when none is free. Despite the editor text it does not become an info_player_deathmatch, so in FFA and every other gametype it is never used.",
		NULL,
		"angle|0|The direction (yaw, degrees) players face when they spawn here.\n"
		"target||Fired each time a player spawns here, with the player as activator.\n"
		"nobots|0|1: bots never spawn here.\n"
		"nohumans|0|1: human players never spawn here.",
		"info_player_start_blue angle 90",
		"common spawnpoint info_player_start_red" },
	{ "info_player_start_red", 0, NULL,
		"Team FFA only: red team players spawn here, at a random free one among those furthest from where they died, falling back to info_player_deathmatch when none is free. Despite the editor text it does not become an info_player_deathmatch, so in FFA and every other gametype it is never used.",
		NULL,
		"angle|0|The direction (yaw, degrees) players face when they spawn here.\n"
		"target||Fired each time a player spawns here, with the player as activator.\n"
		"nobots|0|1: bots never spawn here.\n"
		"nohumans|0|1: human players never spawn here.",
		"info_player_start_red angle 270",
		"common spawnpoint info_player_start_blue" },
	{ "info_siege_decomplete", RP_EH_GAMETYPE, "Siege only, on a map with a .siege file: it removes itself otherwise.",
		"Siege only: when used, an objective that was completed counts as not completed again and comes off its team's count. In any other gametype, on a map without a .siege file, or without objective and side, it removes itself.",
		NULL,
		"objective|0|Required: the objective number, as in the .siege file.\n"
		"side|0|Required: 1 for team 1, 2 for team 2.\n"
		"targetname||Using it undoes the objective.",
		"info_siege_decomplete targetname undo1 objective 1 side 1",
		"common info_siege_objective" },
	{ "info_siege_objective", RP_EH_GAMETYPE, "Siege only, on a map with a .siege file: it removes itself otherwise.",
		"Siege only: a siege objective. Using it completes objective N of its side: it fires the objective's target from the .siege file and its own target, and the round ends once the team has done enough. It shows on every player's radar with its icon. In any other gametype, on a map without a .siege file, or without objective and side, it removes itself.",
		"8|STARTOFFRADAR|Not on the radar at first: the first use only puts it on the radar, the next one completes it.",
		"objective|0|Required: the objective number, as in the .siege file.\n"
		"side|0|Required: 1 for team 1, 2 for team 2.\n"
		"icon||The icon shader shown on the radar.\n"
		"targetname||Using it completes the objective.\n"
		"target||Also fired when it is completed.",
		"info_siege_objective targetname obj1 objective 1 side 1",
		"common info_siege_decomplete info_siege_radaricon" },
	{ "info_siege_radaricon", RP_EH_GAMETYPE, "Siege only, on a map with a .siege file: it removes itself otherwise.",
		"Siege only: shows an icon on every player's radar at its position. Using it toggles the icon on and off. Without an icon, in any other gametype or on a map without a .siege file, it removes itself.",
		NULL,
		"icon||Required: the icon shader shown on the radar.\n"
		"startoff|0|1: starts hidden until used.\n"
		"targetname||Using it shows or hides the icon.",
		"info_siege_radaricon icon gfx/hud/i_icon_medkit targetname icon1",
		"common info_siege_objective" },
	{ "item_ammodisp", 0, NULL,
		"Ammo dispenser, a holdable that is kept. Using it does nothing in GalaxyRP (its toss is switched off). While carrying it, "
		"or the health dispenser, pressing Use on a player of your team refills the ammo of the weapon in their hands a shot at a "
		"time, and heals them if they are hurt.",
		NULL,
		NULL,
		"item_ammodisp",
		"items common item_healthdisp" },
	{ "item_binoculars", 0, NULL,
		"Electrobinoculars, a holdable that is kept: using it zooms the view in, using it again zooms back out. A downed player "
		"cannot raise them.",
		NULL,
		NULL,
		"item_binoculars",
		"items common" },
	{ "item_bluecube", RP_EH_MAPONLY, "No gametype lets anyone pick it up: it only shows an orb.",
		"A team item that no gametype uses: it shows a blue orb that nobody can pick up.",
		NULL,
		NULL,
		NULL,
		"items common" },
	{ "item_botroam", RP_EH_MAPONLY, "It does nothing in multiplayer: its spawn function is empty and no code reads it.",
		"A bot roaming hint from Quake 3 maps. In GalaxyRP nothing reads it: it spawns as an empty logical entity and does nothing.",
		NULL,
		NULL,
		NULL,
		"common" },
	{ "item_cloak", 0, NULL,
		"Cloaking device, a holdable that is kept: using it makes the player invisible, using it again ends it, at most once a "
		"second. It uses no fuel in GalaxyRP. A downed player cannot cloak.",
		NULL,
		NULL,
		"item_cloak",
		"items common" },
	{ "item_eweb_holdable", 0, NULL,
		"Portable E-Web, a holdable that is kept: using it sets up an E-Web gun for the player to man, and using it again packs it "
		"away. Not while busy, downed or already on another emplaced gun.",
		NULL,
		NULL,
		"item_eweb_holdable",
		"items common" },
	{ "item_force_boon", 0, NULL,
		"Force Boon powerup: for count seconds the player's Force comes back much faster and Force powers cost half. Taking a "
		"ysalamiri ends it. Not spawned in siege or Jedi Master, or when Force powers are disabled. It comes back after "
		"rp_powerup_respawn_time.",
		NULL,
		"count|25|Seconds it lasts.",
		"item_force_boon count 30",
		"items common item_ysalimari" },
	{ "item_force_enlighten_dark", 0, NULL,
		"Dark Force Enlightenment powerup: for count seconds every dark side and neutral Force power of the player is at level 3, "
		"even ones they do not know. Only logged-out players on the dark side can take it (logged-in characters never can), and "
		"it has no effect in duel tournament matches or melee battles. Not spawned in holocron, siege or Jedi Master, or when Force "
		"powers are disabled.",
		NULL,
		"count|25|Seconds it lasts.",
		"item_force_enlighten_dark count 30",
		"items common item_force_enlighten_light" },
	{ "item_force_enlighten_light", 0, NULL,
		"Light Force Enlightenment powerup: for count seconds every light side and neutral Force power of the player is at level "
		"3, even ones they do not know. Only logged-out players on the light side can take it (logged-in characters never can), "
		"and it has no effect in duel tournament matches or melee battles. Not spawned in holocron, siege or Jedi Master, or when "
		"Force powers are disabled.",
		NULL,
		"count|25|Seconds it lasts.",
		"item_force_enlighten_light count 30",
		"items common item_force_enlighten_dark" },
	{ "item_healthdisp", 0, NULL,
		"Health dispenser, a holdable that is kept. Using it does nothing in GalaxyRP (its toss is switched off). While carrying it, "
		"or the ammo dispenser, pressing Use on a hurt player of your team heals them 4 at a time, and refills the ammo of the "
		"weapon in their hands.",
		NULL,
		NULL,
		"item_healthdisp",
		"items common item_ammodisp" },
	{ "item_jetpack", 0, NULL,
		"Jetpack, a holdable that is kept: using it switches the jetpack on or off, at most once every 0.9 seconds. It burns "
		"jetpack fuel and will not start with too little. It cannot be switched on when downed, in a private duel or in a duel "
		"tournament match. On the floor it shows the sentry gun model, having none of its own.",
		NULL,
		NULL,
		"item_jetpack",
		"items common" },
	{ "item_medpac", 0, NULL,
		"Bacta canister, used up when used: heals 25 health (75 for a logged-in character with the Holdable Items Upgrade), never above "
		"the maximum. Not when downed. Not spawned in duel or power duel.",
		NULL,
		NULL,
		"item_medpac",
		"items common item_medpac_big" },
	{ "item_medpac_big", 0, NULL,
		"Big bacta canister, used up when used: heals 50 health (150 for a logged-in character with the Holdable Items Upgrade), never "
		"above the maximum. Not when downed. Not spawned in duel or power duel.",
		NULL,
		NULL,
		"item_medpac_big",
		"items common item_medpac" },
	{ "item_redcube", RP_EH_MAPONLY, "No gametype lets anyone pick it up: it only shows an orb.",
		"A team item that no gametype uses: it shows a red orb that nobody can pick up.",
		NULL,
		NULL,
		NULL,
		"items common" },
	{ "item_security_key", 0, NULL,
		"A security key lying on the ground. A live player walking over it who carries no security key takes it, fires its target and the key is removed. A player carries one key at a time, loses it on death or disconnect, and spends it on the func_security_panel or misc_security_panel whose message matches its name.",
		NULL,
		"message||The key's name, which the panel's message must match. Required: it is not spawned without one.\n"
		"target||Fired when a player takes it, with that player as activator.",
		"item_security_key message bluekey",
		"common func_security_panel misc_security_panel" },
	{ "item_seeker", 0, NULL,
		"Seeker drone, used up when used: a drone floats by the player and shoots at enemies for 60 seconds. One at a time. Not "
		"spawned on saber-only servers.",
		NULL,
		NULL,
		"item_seeker",
		"items common" },
	{ "item_sentry_gun", 0, NULL,
		"Assault sentry, used up when used: sets a sentry gun down 64 units in front of the player, which shoots enemies but not "
		"allies for 10 minutes or 500 shots (50 health). One per player at a time, and only with room in front. Not spawned on "
		"saber-only servers.",
		NULL,
		NULL,
		"item_sentry_gun",
		"items common misc_sentry_turret" },
	{ "item_shield", 0, NULL,
		"Portable forcefield, used up when used: puts up an energy wall 64 units ahead, across the way the player faces, up to 254 "
		"units high and 510 wide. It has 3000 health (2000 in siege and CTF) and loses 1 a second, and its owner and allies "
		"(teammates in team games) pass through it. Not spawned on saber-only servers.",
		NULL,
		NULL,
		"item_shield",
		"items common" },
	{ "item_ysalimari", 0, NULL,
		"Ysalamiri powerup: for count seconds the player cannot use the Force and no Force power works on them. Taking it ends "
		"Force Boon and Enlightenment, and while it lasts no other powerup can be picked up. Not spawned in siege or Jedi Master.",
		NULL,
		"count|25|Seconds it lasts.",
		"item_ysalimari count 15",
		"items common item_force_boon" },
	{ "items", RP_EH_TOPIC, NULL,
		"An item falls to the floor below its origin (one starting inside something solid is removed) and comes back after being "
		"taken. Gametype and server settings remove some: powerups in siege and Jedi Master, ammo and some holdables on "
		"saber-only servers, health, shields and bacta in duels, weapons in g_weaponDisable, any item whose disable_<classname> "
		"cvar is 1. A class the map had not precached shows as a placeholder to players with an older client until the map "
		"restarts. The map's own pickups that nothing links to are tagged E in /entlist and can be edited like placed ones.",
		"1|SUSPENDED|Stays where it is placed instead of falling to the floor.\n"
		"4|ALLOWNPC|NPCs can pick it up too.\n"
		"131072|NO_DISABLE|A weapon is not removed by g_weaponDisable.",
		"wait|0|Seconds before it comes back once taken. 0: the server's time for its kind (weapons g_weaponRespawn, ammo and "
		"the thermal, trip mine and det pack weapons rp_ammo_respawn_time, shields rp_shield_respawn_time, health "
		"rp_health_respawn_time, holdables rp_holdable_item_respawn_time, powerups rp_powerup_respawn_time). -1: it never comes "
		"back on its own.\n"
		"random|0|Seconds added or taken at random from the respawn time (never under 1).\n"
		"count|0|Overrides what it gives: ammo for a weapon (-1 none) or an ammo pack, health for a medpack, seconds for a "
		"powerup. Ammo is scaled by rp_add_ammo_scale and capped by the rp_max_ cvars. Shields and holdables ignore it.\n"
		"targetname||Hidden at first: using it makes it appear (and brings it back after a pickup when wait is -1).\n"
		"target||Fired when someone picks it up, with that player as activator.\n"
		"team||Items with the same team show one at a time: each pickup brings back a random one of them.\n"
		"noglobalsound|0|Powerups: 1 plays the respawn sound only near the item, not to everyone.",
		NULL,
		"common" },
	{ "light", 0, NULL,
		"A light switch, not a light source: it sets one of the map's switchable lightstyles on or off for every player, so only surfaces the map was compiled with that style change. Using it toggles it. It needs a targetname, and one added with the Entity System a style from 32 to 63. For a light you can place anywhere, use rp_light.",
		"4|START_OFF|Starts off.",
		"targetname||Required: using it switches it on and off.\n"
		"style|0|The lightstyle it switches: 32 to 63 for one you add (a map's own may use 0 to 63).\n"
		"switch_style|0|A style whose pattern it shows when on, instead of full bright (0 or 32 to 63).\n"
		"style_off|0|A style whose pattern it shows when off, instead of dark (0 or 32 to 63).",
		"light targetname lamp1 style 32",
		"common rp_light" },
	{ "misc_ammo_floor_unit", 0, NULL,
		"A floor ammo station, models/items/a_pwr_converter.md3, 32 units across and 40 high, dropped onto the floor below it (not spawned if it starts inside solid). "
		"Held with Use, every 100 ms it adds 0.8 percent of the cap (at least 1) to each ammo type below the server's cap, and costs 1 charge when it gave anything. "
		"Explosive ammo (thermals, trip mines, det packs) only for a weapon the player already has: it never gives weapons. Sounds ammocon_run, ammocon_done, ammocon_empty.",
		NULL,
		"count|200|Full charge.\n"
		"chargerate|100|Ms per point of charge regained.\n"
		"nodrain|0|1: never drains.\n"
		"model|models/items/a_pwr_converter.md3|Another .md3 to show (see dispenser).",
		"misc_ammo_floor_unit count 100 chargerate 500",
		"common dispenser misc_model_ammo_power_converter" },
	{ "misc_bsp", 0, NULL,
		"Places another BSP (a sub-BSP) as solid geometry, turned by its angle (yaw only), with that BSP's own entities, drawn for every client. "
		"After map load only a sub-BSP this map already loaded is accepted (others are refused and logged), so in practice it copies or moves the map's own misc_bsp. "
		"Its sub-BSP entities are rebuilt with it, go with it on /entremove and /entedit, and are not saved by /entsave.",
		NULL,
		"bspmodel||The sub-BSP, maps/NAME.bsp given as NAME: after map load the bspmodel of one of the map's own misc_bsp.\n"
		"angle|0|Yaw. Other rotations are not supported.",
		"misc_bsp bspmodel NAME angle 90",
		"common" },
	{ "misc_cubemap", RP_EH_MAPONLY | RP_EH_REMOVED, "Only the rend2 renderer reads it, from the map file: nothing for the server.",
		"A point the rend2 renderer reads from the map file to build its reflection cube maps at. The standard renderer does not use it, "
		"and the server removes it at once.",
		NULL,
		NULL,
		NULL,
		"common" },
	{ "misc_exploding_crate", 0, NULL,
		"An exploding crate, nar_shaddar/crate_xplode.md3 (fixed), solid, 48 units across and 64 high. Any damage hurts it: destroyed, it throws crate chunks, "
		"explodes (splashdamage within splashradius), fires its target and is removed. Using it (targetname) blows it up at once.",
		"2048|NO_EXPLOSION|Breaks without the explosion and its damage.",
		"health|40|Damage it takes. 0 does not make it unbreakable: it goes at the first hit.\n"
		"splashdamage|50|Explosion damage (0 or absent: the default).\n"
		"splashradius|128|Explosion radius (0 or absent: the default).\n"
		"target||Fired when it is destroyed.\n"
		"targetname||Using it blows it up.",
		"misc_exploding_crate health 60 splashdamage 80",
		"common misc_gas_tank misc_model_breakable" },
	{ "misc_faller", 0, NULL,
		"Drops a falling stormtrooper ragdoll (randomly tinted) from its origin: it screams on the way down, lands with a splat and disappears after 15 seconds. "
		"Without a targetname it makes one every interval plus up to fudgefactor ms, with one only each time it is used. One misc_faller keeps at most 32 alive.",
		NULL,
		"interval|500|Milliseconds between fallers, at least 500.\n"
		"fudgefactor|0|Up to this many ms (0 to 60000) added at random to each interval.\n"
		"targetname||If set, it only makes a faller each time it is used.",
		"misc_faller interval 4000 fudgefactor 2000",
		"common" },
	{ "misc_G2model", RP_EH_MAPONLY | RP_EH_REMOVED, "Does nothing in multiplayer: use misc_model_ghoul for a .glm model, misc_model_breakable for an .md3.",
		"The editor's Ghoul2 model placeholder. In multiplayer it removes itself at once.",
		NULL,
		NULL,
		NULL,
		"common misc_model_ghoul misc_model_breakable" },
	{ "misc_gas_tank", 0, NULL,
		"An oxygen tank, imp_mine/tank.md3 (fixed), solid, 8 units across and 40 high. Every 12 to 28 seconds it puffs a small gas jet. "
		"The first hit sets off a flame jet on its top that burns anything right above it (32 damage), and when destroyed it throws metal chunks, "
		"explodes (splashdamage within splashradius), fires its target and is removed. Using it (targetname) blows it up.",
		"2048|NO_EXPLOSION|Breaks without the explosion and its damage.",
		"health|20|Damage it takes. 0 does not make it unbreakable: it goes at the first hit.\n"
		"splashdamage|32|Explosion damage (0 or absent: the default).\n"
		"splashradius|48|Explosion radius (0 or absent: the default).\n"
		"target||Fired when it is destroyed.\n"
		"targetname||Using it blows it up.",
		"misc_gas_tank health 30",
		"common misc_exploding_crate misc_model_breakable" },
	{ "misc_holocron", RP_EH_GAMETYPE, "Holocron gametype only: in any other it removes itself.",
		"A holocron of the Holocron gametype: touched, it gives that force power until the carrier dies, then pops out again, "
		"and goes back to its spot after 30 seconds away. In any other gametype it removes itself at once.",
		NULL,
		"count|0|The force power: 0 heal, 1 jump, 2 speed, 3 push, 4 pull, 5 mind trick, 6 grip, 7 lightning, 8 rage, 9 protect, 10 absorb, "
		"11 team heal, 12 team force, 13 drain, 14 see, 15 saber attack, 16 saber defense, 17 saber throw.",
		"misc_holocron count 3",
		"common" },
	{ "misc_maglock", 0, NULL,
		"A magnetic door lock, imp_detention/door_lock.md3. Placed facing a func_door (it looks up to 128 units along its angle), it sticks to the door's surface "
		"and locks it: the door and its trigger cannot be used. Only lightsaber hits hurt it and it always has 10 health (a health key is ignored): destroyed, "
		"it unlocks the door when it was its last lock and fires its target. With no door found within 5 seconds, or placed in solid, it removes itself.",
		NULL,
		"angle||Points at the door it locks.\n"
		"target||Fired when it is destroyed.",
		"misc_maglock angle 90",
		"common func_door" },
	{ "misc_model", RP_EH_MAPONLY | RP_EH_REMOVED, "A map-compile hint, baked into the map: use misc_model_breakable for a model.",
		"The map compiler's static model: it becomes part of the map itself, so in the game it removes itself at once.",
		NULL,
		NULL,
		NULL,
		"common misc_model_breakable" },
	{ "misc_model_ammo_power_converter", 0, NULL,
		"An ammo power converter, models/items/power_converter.md3, a 32-unit cube centred where it is placed (it does not drop to the floor). "
		"Held with Use, every 100 ms it adds 10 percent of the cap (at least 1) to each ammo type below the server's cap, and costs 3 charge when it gave anything. "
		"Explosive ammo only for a weapon the player already has. It uses the ammo floor unit's sounds.",
		NULL,
		"count|200|Full charge.\n"
		"chargerate|100|Ms per point of charge regained.\n"
		"nodrain|0|1: never drains.\n"
		"model|models/items/power_converter.md3|Another .md3 to show (see dispenser).",
		"misc_model_ammo_power_converter count 300",
		"common dispenser misc_ammo_floor_unit" },
	{ "misc_model_ammo_rack", 0, NULL,
		"An ammo rack, kejim/weaponsrung.md3 (a model key is not used, as in single player), stocked with pickup items 0.1 s after it spawns: "
		"one ammo pack per chosen type (blaster packs when none is chosen), the first repeated to fill three places unless NO_FILL. "
		"WEAPON adds the weapon of the first chosen of blaster, metal bolts and rockets, HEALTH a medpak. Taken items are not put back, and ammo amounts follow g_npcspskill. "
		"Removing or editing the rack takes its items with it.",
		"1|BLASTER|Blaster ammo packs (also when no ammo type is chosen).\n"
		"2|METAL_BOLTS|Metal bolt packs.\n"
		"4|ROCKETS|Rocket packs.\n"
		"8|WEAPON|Adds a matching weapon (blaster, repeater or rocket launcher).\n"
		"16|HEALTH|Adds a medpak (item_medpak_instant) on the top shelf.\n"
		"32|PWR_CELL|Power cell packs.\n"
		"64|NO_FILL|Only the chosen packs: the shelf is not filled up to three.",
		NULL,
		"misc_model_ammo_rack spawnflags 26 angle 90",
		"common misc_model_gun_rack" },
	{ "misc_model_breakable", 0, NULL,
		"A prop showing any .md3 model, optionally breakable. With health it can be shot: at 0 it throws chunks of its material, explodes when splashdamage and splashradius are set, "
		"fires its target and switches to its damage model (model name + _d1.md3) or, without one, is removed. Using it (targetname, or Use with PLAYER_USE) breaks it too, unless USE_NOT_BREAK. "
		"Placed with the Entity System and no mins/maxs, its box is the model's own bounds.",
		"1|SOLID|Blocks movement. Without it, one with health can still be shot.\n"
		"2|AUTOANIMATE|Plays the model's frames over and over (stops when destroyed).\n"
		"4|DEADSOLID|Stays solid once destroyed (when it has a damage model).\n"
		"8|NO_DMODEL|No damage model: removed when destroyed.\n"
		"32|USE_MODEL|With USE_NOT_BREAK, using it switches between its model and model name + _u1.md3.\n"
		"64|USE_NOT_BREAK|Using it does not break it.\n"
		"128|PLAYER_USE|Players can use it with the Use key.\n"
		"2048|NO_EXPLOSION|No explosion when destroyed.\n"
		"4096|START_OFF|Starts hidden and not solid. Using it makes it visible (still not solid), and without USE_NOT_BREAK that use also breaks it.\n"
		"65536|NO_DEFAULT_BOX|No default box: only mins/maxs set it.",
		"model||The .md3 to show (required). The Entity System refuses one not on the server, not an .md3 or with no model slot.\n"
		"health|0|Damage it takes before breaking. 0: it cannot be shot (using it still breaks it).\n"
		"material|8|Chunks: 0 metal, 1 glass, 2 sparks, 3 metal and sparks, 4 brown stone, 5 tan stone, 6 glass and metal, 7 blue metal, 8 none, 9 grey stone, 10 mixed metal, 11 yellow crate, 12 grate, 13 rope bits, 14 red crate, 15 white metal.\n"
		"radius|1|Scales how many chunks it throws.\n"
		"splashdamage|0|Explosion damage when it breaks (needs splashradius too).\n"
		"splashradius|0|Radius of that explosion.\n"
		"target||Fired when it breaks.\n"
		"target3||Fired when it is used while broken (only one that had health and keeps a damage model).\n"
		"paintarget||Fired each time it is hurt but not broken.\n"
		"targetname||Using it breaks it, or does what USE_NOT_BREAK, USE_MODEL and START_OFF say.\n"
		"mins|-16 -16 -16|Box corner (Entity System default: the model's own bounds).\n"
		"maxs|16 16 16|Box corner (Entity System default: the model's own bounds).\n"
		"modelscale||Scales the drawn model and its box, 0.01 to 10.23. The origin is raised to keep the box bottom in place.\n"
		"modelscale_vec||Scales the box per axis (x y z). The GalaxyRP client draws the model so too, other clients unscaled.\n"
		"zykmodelscale||Drawn size in percent (1 to 1023), overrides the drawn size modelscale gives.\n"
		"light||Radius of a light on the model (100 when only color is given). It goes out when the model breaks.\n"
		"color||Light colour, red green blue from 0 to 1 (default white).\n"
		"gravity|0|Non-zero: set down on the floor below it when it spawns.\n"
		"throwtarget||With gravity: using it throws it toward the entity with this targetname instead of breaking it.",
		"misc_model_breakable model models/map_objects/kejim/cargo_small.md3 spawnflags 1 health 50 material 11",
		"common misc_model_ghoul misc_model_cargo_small" },
	{ "misc_model_cargo_small", 0, NULL,
		"Single player's small cargo crate, kejim/cargo_small.md3: always a solid breakable with no damage model (the breakable spawnflags are not used), hurt only by heavy weapons "
		"(rockets, explosives, mines, repeater alt fire, vehicles). Destroyed, it throws crate debris, makes a small blast and drops the items its spawnflags choose. "
		"Using it (targetname) breaks it without dropping anything. Other misc_model_breakable keys (light, color, modelscale, gravity) work as there.",
		"1|MEDPACK|Drops an item_medpak_instant.\n"
		"2|SHIELDS|Drops an item_shield_sm_instant.\n"
		"4|BACTA|Drops an item_medpac (bacta canister).\n"
		"8|BATTERIES|Nothing: multiplayer has no battery item.",
		"model|models/map_objects/kejim/cargo_small.md3|Another .md3 to show (see dispenser for the rules).\n"
		"health|25|Damage it takes. 0: unbreakable.\n"
		"material|11|Debris, as misc_model_breakable's (11 yellow crate chunks).\n"
		"radius|1.5|Scales how many chunks it throws.\n"
		"splashdamage|1|Damage of the blast when it breaks.\n"
		"splashradius|96|Radius of that blast.\n"
		"target||Fired when it breaks.",
		"misc_model_cargo_small spawnflags 3",
		"common misc_model_breakable dispenser" },
	{ "misc_model_ghoul", 0, NULL,
		"A Ghoul2 (.glm) model prop: any .glm with an optional skin, which can play a range of its animation frames once or looped and carry a light. "
		"Using it (targetname) shows or hides it, and it takes no damage. The Entity System refuses a model not on the server or not a .glm, bad frames or radius, or no model slot. "
		"Players need the GalaxyRP client to see a skin of its own.",
		"1|SOLID|Blocks movement and shots with its box.\n"
		"2|LOOP|Plays the frames over and over (needs more than one frame).\n"
		"4096|START_OFF|Starts hidden and not solid.",
		"model||Any .glm: as given or under models/, with .glm added when there is no extension (required).\n"
		"skin||Skin name: test shows model_test.skin from the model's folder. Empty or default: the model's own. A missing file shows the default skin.\n"
		"mins|-16 -16 -16|Box corner: what SOLID blocks and the entity commands aim at.\n"
		"maxs|16 16 16|Box corner.\n"
		"radius||1 to 255: the client stops drawing it when a sphere this big around its origin is out of view. Default: the box's farthest corner, at least 50.\n"
		"modelscale||Drawn size and box, 0.01 to 10.23. A SOLID one is raised to keep its box bottom in place.\n"
		"modelscale_vec||Scales the box only, per axis.\n"
		"zykmodelscale||Drawn size in percent, overrides the drawn size modelscale gives.\n"
		"startframe||First frame played, 0 to 65534. Alone: a still pose on that frame.\n"
		"endframe||Last frame played (from frame 0 without startframe). Frames play once at 20 a second and stay on the last. No frame keys: the base pose.\n"
		"light||Radius of a light on the model (100 when only color is given).\n"
		"color||Light colour, red green blue from 0 to 1 (default white).\n"
		"targetname||Using it shows or hides it.",
		"misc_model_ghoul model models/players/stormtrooper/model.glm startframe 0 endframe 40 spawnflags 3",
		"common misc_model_breakable" },
	{ "misc_model_gun_rack", 0, NULL,
		"A weapon rack, kejim/weaponsrack.md3 (a model key is not used, as in single player), with three weapons standing on it as pickup items: "
		"blasters, or the types its spawnflags choose, the first chosen filling the remaining places. Each carries fixed ammo (blaster 15, repeater 100, rocket launcher 4) "
		"and is not put back once taken. Removing or editing the rack takes its weapons with it, and an edit puts fresh ones up.",
		"1|BLASTER|Blasters (also when nothing is chosen).\n"
		"2|REPEATER|Repeaters.\n"
		"4|ROCKET|Rocket launchers.",
		NULL,
		"misc_model_gun_rack spawnflags 3 angle 90",
		"common misc_model_ammo_rack" },
	{ "misc_model_health_power_converter", 0, NULL,
		"A health power converter, models/items/power_converter.md3, a 32-unit cube centred where it is placed. Held with Use, every 100 ms it adds 5 health, "
		"up to the player's max health, and takes what it gave from its charge. It loops sound/player/pickuphealth.wav while healing and uses the shield units' done and empty sounds.",
		NULL,
		"count|200|Full charge.\n"
		"chargerate|100|Ms per point of charge regained.\n"
		"nodrain|0|1: never drains.\n"
		"model|models/items/power_converter.md3|Another .md3 to show (see dispenser).",
		"misc_model_health_power_converter count 100 chargerate 200",
		"common dispenser" },
	{ "misc_model_shield_power_converter", 0, NULL,
		"A shield power converter, models/items/power_converter.md3, a 32-unit cube centred where it is placed. Held with Use, every 100 ms it adds 2 shield, "
		"up to a logged-in character's max shield or else the player's max health, and takes what it gave from its charge. It uses the shield floor unit's sounds.",
		NULL,
		"count|200|Full charge.\n"
		"chargerate|100|Ms per point of charge regained.\n"
		"nodrain|0|1: never drains.\n"
		"model|models/items/power_converter.md3|Another .md3 to show (see dispenser).",
		"misc_model_shield_power_converter count 100",
		"common dispenser misc_shield_floor_unit" },
	{ "misc_model_static", RP_EH_MAPONLY | RP_EH_REMOVED, "Each client reads it from its own copy of the map file, so one added on the server is never seen: use misc_model_breakable for a model.",
		"A model drawn by each client, not by the server: a client loading the map reads every misc_model_static from its own copy of the map file "
		"and draws it as a model nothing collides with (scaled by modelscale or modelscale_vec, its culling point moved by zoffset). "
		"The server has no part in it and removes it at once, so one added with /entadd or a server entity file is never seen by anyone. "
		"It notes the map's own, though: Entity Bounds (/settings 6) draws them in red, and /entcopystatic picks up a misc_model_breakable copy of one.",
		NULL,
		NULL,
		NULL,
		"common misc_model_breakable" },
	{ "misc_portal_camera", 0, NULL,
		"The camera a misc_portal_surface shows: the portal surface whose target is this camera's targetname draws the view from here. "
		"It looks along its angles, or toward the entity its target names, and can turn.",
		"1|SLOWROTATE|Turns slowly, swinging back and forth unless NOSWING.\n"
		"2|FASTROTATE|Turns faster.\n"
		"4|NOSWING|With a rotate flag, turns all the way round instead of swinging.",
		"targetname||The name a misc_portal_surface targets.\n"
		"target||An entity to look at (otherwise it looks along its angles).\n"
		"roll|0|Roll of the view around its direction, in degrees.",
		"misc_portal_camera targetname cam1 angle 90",
		"common misc_portal_surface" },
	{ "misc_portal_surface", 0, NULL,
		"Turns the nearest portal surface of the map (a surface with a portal shader, within 64 units) into a view: of the misc_portal_camera its target names, "
		"or a mirror without a target. It only works where the map has such a surface. With a target that names nothing, it removes itself.",
		NULL,
		"target||The targetname of the misc_portal_camera to show. Empty: a mirror.",
		"misc_portal_surface target cam1",
		"common misc_portal_camera" },
	{ "misc_security_panel", 0, NULL,
		"A security panel, kejim/sec_panel.md3, solid and 16 units across. A player who uses it while carrying the security key its message names "
		"(from an item_security_key or a key officer) gives up the key, and the panel plays its pass sound and fires its target, once only. "
		"Without the key it plays a fail sound and fires target2. With no message, any player opens it.",
		"128|INACTIVE|Cannot be used until a target_activate turns it on.",
		"message||Name of the key it needs.\n"
		"target||Fired when it is opened (once).\n"
		"target2||Fired when a player without the key uses it.",
		"misc_security_panel message bluekey target door1",
		"common item_security_key target_activate" },
	{ "misc_sentry_turret", 0, NULL,
		"Single player's portable sentry gun (models/items/psgun.glm) placed in the map and owned by nobody: it settles on the floor, never expires, "
		"and shoots every client (players and NPCs) in sight except those of the team its team key names, which also cannot damage it. "
		"After count shots it shuts down and explodes. Destroyed, it explodes and fires its target.",
		NULL,
		"team|enemy|NPC team it leaves alone and takes no damage from: player, enemy, neutral or free (any other name: nobody). Players are on team player, so by default it shoots them.\n"
		"count|500|Shots before it runs out.\n"
		"health|50|Damage it takes before exploding.\n"
		"target||Fired when it is destroyed.",
		"misc_sentry_turret team player",
		"common misc_turretG2" },
	{ "misc_shield_floor_unit", 0, NULL,
		"A floor shield station, models/items/a_shield_converter.md3, 32 units across and 40 high, dropped onto the floor below it (not spawned if it starts inside solid). "
		"Held with Use, every 100 ms it adds 2 shield, up to a logged-in character's max shield or else the player's max health, and takes what it gave from its charge. "
		"Sounds shieldcon_run, shieldcon_done, shieldcon_empty.",
		NULL,
		"count|200|Full charge.\n"
		"chargerate|100|Ms per point of charge regained.\n"
		"nodrain|0|1: never drains.\n"
		"model|models/items/a_shield_converter.md3|Another .md3 to show (see dispenser).",
		"misc_shield_floor_unit count 100 chargerate 300",
		"common dispenser misc_model_shield_power_converter" },
	{ "misc_siege_item", RP_EH_GAMETYPE, "Siege only: in any other gametype it removes itself.",
		"A Siege objective item that players carry to a goal trigger (goaltarget). It shows on every player's radar. With a targetname it is hidden until used. "
		"With health it can be destroyed. Outside the Siege gametype, or without a model, it removes itself at once.",
		"8|STARTOFFRADAR|Not on the radar until it is used.",
		"model||Required: its model, an .md3 or a .glm.\n"
		"goaltarget||The targetname of the trigger_multiple or trigger_once the carrier must bring it into: that trigger may then fire.\n"
		"target2||Fired when it is picked up (only the first time with pickuponlyonce 1).\n"
		"target3||Fired when it is delivered to the goal.\n"
		"target4||Fired when it is destroyed.\n"
		"target5||Fired when it goes back to its spot.\n"
		"target6||Fired when its carrier drops it.\n"
		"canpickup|1|0: it cannot be carried, it just sits there.\n"
		"pickuponlyonce|1|target2 fires on the first pickup only.\n"
		"health|0|Above 0 it can be destroyed. showhealth 1 shows its health bar.\n"
		"teamnotouch|0|1 or 2: that team cannot pick it up.\n"
		"teamnocomplete|0|1 or 2: that team cannot deliver it.\n"
		"noradar|0|1: never on the radar.\n"
		"icon||Its radar icon.\n"
		"forcelimit|0|1: its carrier's force powers are crippled.\n"
		"usephysics|1|It falls and bounces when dropped (mass, gravity, bounce tune it).\n"
		"pickupsound||Sound played when it is picked up.\n"
		"deathfx||Effect played when it is destroyed. One starting with * is a weather command and is refused (a map's own plays no effect instead, logged).\n"
		"respawnfx||Effect played when it goes back to its spot. One starting with * is a weather command and is refused (a map's own plays no effect instead, logged).",
		"misc_siege_item model models/map_objects/kejim/cargo_small.md3 goaltarget goal1",
		"common info_siege_objective" },
	{ "misc_skyportal", 0, NULL,
		"Makes the map's sky show the view from this point, for every client, with the field of view and fog its keys give. Entities it can see are sent to every client while it is there. "
		"The Entity System refuses one on a map with a sky portal of its own, or while another added one is the sky portal. Removing it gives the map its own sky back.",
		NULL,
		"fov|80|Field of view of the sky view, degrees.\n"
		"fogcolor|0 0 0|Fog colour, red green blue from 0 to 1. Any of the fog keys turns fog on.\n"
		"fognear|0|Where the fog starts.\n"
		"fogfar|300|Where the fog is complete.\n"
		"onlyfoghere|0|1: the map's global fog shows only in the sky view. Each client reads it from its own copy of the map file, so it works only on a map's own sky portal.",
		"misc_skyportal fov 90",
		"common" },
	{ "misc_skyportal_orient", RP_EH_MAPONLY | RP_EH_REMOVED, "Each client reads it from its own copy of the map file, so one added on the server does nothing.",
		"Moves the sky portal's view with the player: each client reads it from its own copy of the map file, and the sky view is then "
		"shifted by the player's offset from this point, scaled by modelscale (0, the default: not moved). One per map. "
		"The server removes it at once, so one added on the server does nothing.",
		NULL,
		NULL,
		NULL,
		"common misc_skyportal" },
	{ "misc_spotlight", 0, NULL,
		"A search spotlight, imp_mine/spotlight.md3, that keeps pointing at the entity its target names (a func_train, say), or along its angles without one. "
		"When a living player is within 15 degrees of where it points with a clear line of sight, it fires target2 with that player as activator, "
		"again every wait seconds while they stay in it. Using it (targetname) switches it off and on.",
		"1|START_OFF|Starts switched off (no turning, no detecting) until used.",
		"target||Entity it points at, followed every frame.\n"
		"target2||Fired when it sees a player.\n"
		"wait|0.5|Seconds between target2 firings while a player stays in sight.\n"
		"targetname||Using it switches it off or on.",
		"misc_spotlight target train1 target2 alarm1",
		"common func_train" },
	{ "misc_teleporter_dest", 0, NULL,
		"A teleport destination: a trigger_teleport or target_teleporter whose target is its targetname sends the player here, facing its angle and pushed forward out of it. "
		"It does nothing by itself (a target_position works the same).",
		NULL,
		"targetname||The name teleporters target.\n"
		"angle||The direction the player faces on arrival.",
		"misc_teleporter_dest targetname dest1 angle 180",
		"common trigger_teleport target_teleporter" },
	{ "misc_trip_mine", 0, NULL,
		"A trip mine laid by the map and owned by nobody: it hurts whoever trips its beam. It sits at its origin and fires its beam opposite its angles, "
		"so place it on a wall with its angle pointing into the wall (/entaddaim). It is always armed and, once set off, gone. "
		"The misc_trip_mine itself stays as an invisible placeholder that /entsave saves, and removing or editing it takes its mine along.",
		"1|START_ON|Not used: the mine is always armed.\n"
		"2|BROADCAST|Its beam and sound reach clients through area portals.\n"
		"4|START_OFF|Not used: the mine is always armed.",
		"angle||Point it into the surface it sits on: the beam fires the other way.",
		"misc_trip_mine angle 90",
		"common" },
	{ "misc_turret", 0, NULL,
		"On ordinary maps, the large two-piece Hoth turbolaser turret (144 units high): it looks for clients (players and NPCs) within radius, turns and fires bolts at them, "
		"and explodes when its health is gone, firing its target. Using it (targetname) switches it off for good, and a START_OFF one never comes on. "
		"On a single-player map it is single player's small ceiling turret instead: a misc_turretG2 with single player's team rule.",
		"1|START_OFF|Starts off (on ordinary maps it never comes on).",
		"radius|1024|How far away it picks up a target.\n"
		"wait|300|Ms between shots (300 to 355 when 0 or under 100).\n"
		"dmg|100|Damage per shot.\n"
		"health|3000|Damage it takes before exploding.\n"
		"speed|20|How fast it turns.\n"
		"splashdamage|300|Damage of its explosion when destroyed.\n"
		"splashradius|128|Radius of that explosion.\n"
		"shotspeed|1100|Speed of its bolts (at least 100).\n"
		"target||Fired when it is destroyed.\n"
		"target2||Fired when it picks up a target.\n"
		"showhealth|0|1: shows its health bar under the crosshair.\n"
		"alliedteam|0|Team it does not shoot: 1 red, 2 blue (team games).\n"
		"teamnodmg|0|Team it takes no damage from: 1 red, 2 blue.\n"
		"teamowner|0|Team its crosshair shows as friendly: 1 red, 2 blue.\n"
		"team||On a single-player map: the team it leaves alone and takes no damage from (player, enemy by default, neutral, free). Elsewhere: as teamnodmg.",
		"misc_turret radius 800 dmg 20 health 500",
		"common misc_turretG2" },
	{ "misc_turretG2", 0, NULL,
		"A small turret hanging from the ceiling (standing on the floor with UPSIDE_DOWN): it looks for clients (players and NPCs) and breakable brushes within radius, "
		"winds up, turns and fires blaster bolts at them. Destroyed, it explodes, fires its target and shows its damaged model. "
		"TURBO makes it the big Death Star turbolaser with heavier defaults. Using it (targetname) switches it off and on.",
		"1|START_OFF|Starts off: using it switches it on.\n"
		"2|UPSIDE_DOWN|Stands on a floor instead of hanging (placed 22 units lower).\n"
		"4|CANRESPAWN|Comes back count ms after it is destroyed.\n"
		"8|TURBO|The big turbolaser: defaults radius 32768, wait 1000, dmg 500, health 2000, splash 200 in 500, shotspeed 20000.\n"
		"16|LEAD|Aims ahead of moving targets.\n"
		"32|SHOWRADAR|Shows on the radar.\n"
		"32768|NO_DROP|With UPSIDE_DOWN: not placed 22 units lower.",
		"radius|512|How far away it picks up a target.\n"
		"wait|150|Ms between shots (under 100: the default).\n"
		"dmg|5|Damage per shot.\n"
		"health|100|Damage it takes before exploding.\n"
		"count|20000|With CANRESPAWN: ms before it comes back.\n"
		"random|2|Aim error, in degrees.\n"
		"shotspeed|1100|Speed of its bolts (under 100: the default).\n"
		"splashdamage|10|Splash damage of each bolt and of its explosion when destroyed.\n"
		"splashradius|25|Radius of its explosion when destroyed.\n"
		"target||Fired when it is destroyed.\n"
		"target2||Fired when it picks up a target.\n"
		"paintarget||Fired when it is hurt.\n"
		"painwait|0|Ms between paintarget firings.\n"
		"showhealth|0|1: shows its health bar under the crosshair.\n"
		"customscale|0|Size in percent (100 normal, at most 1023), box too.\n"
		"alliedteam|0|Team it does not shoot: 1 red, 2 blue (team games).\n"
		"teamnodmg|0|Team it takes no damage from: 1 red, 2 blue. team does the same.\n"
		"teamowner|0|Team its crosshair shows as friendly: 1 red, 2 blue.",
		"misc_turretG2 spawnflags 2 radius 600 dmg 8",
		"common misc_turret misc_sentry_turret" },
	{ "misc_weapon_shooter", 0, NULL,
		"Fires a weapon from its origin each time it is used (targetname), at the entity its target names (aim kept up to date) or along its angles. "
		"ALTFIRE uses the alternate fire. With TOGGLE a use starts firing every wait ms, aiming each shot at where its target is, and the next use stops it. "
		"At most 16 per map, the 17th is refused.",
		"1|ALTFIRE|Fires the weapon's alternate fire.\n"
		"2|TOGGLE|Keeps firing every wait ms until used again.",
		"weapon|WP_BLASTER|Weapon by its WP_ name: WP_BRYAR_PISTOL, WP_BLASTER, WP_DISRUPTOR, WP_BOWCASTER, WP_REPEATER, WP_DEMP2, WP_FLECHETTE, WP_ROCKET_LAUNCHER, WP_THERMAL, WP_CONCUSSION... An unknown name gives WP_BLASTER.\n"
		"wait|500|With TOGGLE: ms between shots, at least 100.\n"
		"random|0|With TOGGLE: up to this many ms added at random to each wait.\n"
		"target||Entity to aim at.\n"
		"targetname||Using it fires.",
		"misc_weapon_shooter targetname gun1 weapon WP_REPEATER angle 90",
		"common" },
	{ "misc_weather_zone", RP_EH_MAPONLY | RP_EH_REMOVED, "Each client reads it from its own copy of the map file, so one added on the server does nothing. Weather itself is set with /admweather.",
		"A box (its brush model) each client reads from its own copy of the map file: inside the map's weather zones the renderer works out, "
		"cell by cell, which parts of the map are outdoors, and that is where weather shows -- the map's own and /admweather's alike. "
		"A map without one gets a single zone around the whole map. The server removes it at once, so one added on the server does nothing.",
		NULL,
		NULL,
		NULL,
		"common" },
	{ "model", RP_EH_TOPIC, NULL,
		"Model files for props: an .md3 for misc_model_breakable and for a func_ class whose model is not a brush model (*N), a "
		".glm for misc_model_ghoul. Every model takes one of the map's 511 model slots for the rest of the map, and the Entity "
		"System refuses a prop whose file the server does not have, or for which no slot is left. A slot that only removed Entity "
		"System props used is given to a new model when the table or the gamestate is full (at most 128 times a map). /entslots "
		"shows how full the slots are and how many can be reused.",
		NULL,
		"model||As typed, else under models/ with .md3 added (.glm for misc_model_ghoul) when it has no extension: "
		"map_objects/kejim/cargo_small works, and the record keeps what you typed. /list models <folder> lists the .md3 files.\n"
		"modelscale|0|misc_model_breakable and misc_model_ghoul: one scale for the drawn model and its box, drawn from 0.01 to "
		"10.23 (1 is normal). A breakable is raised so its box keeps its bottom where it was.\n"
		"zykmodelscale||The drawn size as a percentage (100 is normal, up to 1023), taking over from modelscale. The box is not "
		"changed.\n"
		"modelscale_vec||Three scales x y z, typed in quotes, for the box: a misc_model_breakable is drawn so too by the GalaxyRP "
		"client (other clients draw it unscaled), a misc_model_ghoul unscaled. zykmodelscale, if given, decides the drawn size. "
		"modelscale is then not read.",
		NULL,
		"common misc_model_breakable misc_model_ghoul" },
	{ "mover", RP_EH_TOPIC, NULL,
		"What the func_ mover classes share. Each takes a model: *N, one of the current map's inline brush models from *1 (*0, the map itself, and a number this map does not have are refused), or an .md3 file, refused if the server lacks it. A *N model is drawn shifted by the origin from where the map built it unless it was built around an origin brush, so origin 0 0 0 puts a plain one back in place. An .md3 one has no size or collision of its own: give mins and maxs, and spawnflag 1024.",
		"32|CRUSH_THROUGH|With dmg set, a player in its way takes dmg and it keeps moving (on func_breakable 32 is HEAVY_WEAP).\n"
		"64|PLAYER_USE|Players can use it with the use key (an .md3 one must be SOLID to be aimed at).\n"
		"128|INACTIVE|Ignores every use until a target_activate activates it.\n"
		"1024|SOLID|With an .md3 model: it is solid, by its mins and maxs. A *N model collides as its brushes were built, without it.",
		"model||*N: an inline brush model of the map, #name: a sub-BSP the map loaded, anything else: an .md3 path (tried as typed, then under models/ with .md3 added).\n"
		"model2||An extra .md3 model drawn with it (.glm is ignored). It collides by model, not model2.\n"
		"mins||With an .md3 model: the lower corner of its box (x y z), which sets what it blocks and how far a door, button or plat moves.\n"
		"maxs||With an .md3 model: the upper corner of its box.\n"
		"angles2||The facing of an .md3 model (pitch yaw roll). Not on func_static and func_pendulum: they face by angles.\n"
		"angle||On func_door and func_button the direction they move (-1 up, -2 down), not a facing. A *N model only turns on func_static and func_pendulum.\n"
		"light|100|Radius of a constant dynamic light around its origin, up to 1020. 100 when only color is given.\n"
		"color|1 1 1|Colour of that light, red green blue from 0 to 1. Giving light or color turns the light on.\n"
		"soundSet||An ambient sound set (bmodelSet in sound/sound.txt): doors, plats, buttons and trains play its start, loop and stop sounds as they move, and func_rotating as it is started and stopped (its loop also while it spins from spawn). Refused if the server does not know the set.\n"
		"team||Movers with the same team move as one: the first is the leader, using any member uses the leader, and a member's targetname moves to the leader. Rotators, bobbers and pendulums of a team start and stop together, and start as the leader spawned (its name and START_ON or START_OFF decide). Not on func_breakable.\n"
		"linear|0|1: doors, plats, buttons and trains move at a steady speed instead of easing in and out.\n"
		"target||Doors, plats and buttons fire it as they start to open.\n"
		"opentarget||Doors, plats and buttons fire it when fully open.\n"
		"target2||Doors, plats and buttons fire it when used while fully open.\n"
		"closetarget||Doors, plats and buttons fire it when closed again.",
		NULL,
		"common" },
	{ "npc", RP_EH_TOPIC, NULL,
		"Shared behaviour of every npc_* spawner (npc_spawner, npc_vehicle and the named classes such as npc_stormtrooper). /entadd needs a targetname: firing that name spawns one NPC, after delay. Each spawn uses one of count, and when the last is used the spawner fires its target and removes itself. spawnnow and respawn work only on a named spawner. A spawn is skipped (count kept) when 256 NPCs are alive or entity slots run short. Its NPCs get npc_targetname, npc_target, health, scripts and spawnflags from it.",
		"16|DROPTOFLOOR|Traces the spawner down to the floor, but the NPC is still made at the spawner's own origin, so it has no visible effect (single player is the same). For Jedi, Luke or Kyle allies and Tavion, Reborn, Desann or Shadowtrooper enemies it is CEILING: the NPC clings where placed and drops on seeing an enemy or being hurt.\n"
		"32|CINEMATIC|The NPC starts with no AI (cinematic behaviour state) and stands until a script moves it.\n"
		"64|NOTSOLID|The NPC is not solid at all and is not checked for a blocked spawn spot. Wins over npceffect.\n"
		"128|STARTINSOLID|Does not try to free an NPC that spawns inside something solid.\n"
		"2048|SHY|When fired, waits until no player is within 128 units or looking at the spot (checked every second). The editor's SHY box (256) does nothing (single player is the same).\n"
		"65536|MILLISECONDS|wait and delay are given in milliseconds instead of seconds. The spawner also sets it on itself.",
		"npc_type||The NPC type (see /list npcs). Required on npc_spawner, a vehicle name on npc_vehicle. The named classes pick their own.\n"
		"targetname||Required by /entadd and /entedit: using this name (trigger, button, /entuse) spawns an NPC. Without one (maps, /entsave files) it spawns one at map start and is gone.\n"
		"count|1|NPCs it spawns before it removes itself, -1 for no limit (at most 1000).\n"
		"delay|0|Seconds between being fired and the spawn (at most 3600).\n"
		"wait|0.5|Seconds before the NPC tries again when its spawn spot is blocked. Below 0 the NPC fires target3 and is removed instead.\n"
		"target||Fired when the spawner spawns its last NPC (count used up).\n"
		"spawnnow|0|1: also spawns one at once when placed or loaded, not counted and without delay or shyness. Spawned again in place (/entedit) it does so only if none it made is alive.\n"
		"respawn|0|1: when an NPC it made dies the spawner is fired again: its delay but at least 2 seconds, and one of its count. Use count -1 to keep it going.\n"
		"health|0|Health of each NPC, 0 for the NPC type's own.\n"
		"npc_targetname||targetname and script_targetname given to each NPC: using it uses the NPC.\n"
		"npc_target||Fired by each NPC when it dies.\n"
		"npcteam||player, enemy, neutral or free: the side of each NPC, as /npc team sets it.\n"
		"npceffect||holo, ghost or walkthrough: each NPC starts in that /npc effect mode (walkthrough: walked through but still hit). Not on npc_vehicle.\n"
		"npcfreeze|0|1: each NPC starts frozen, as /npc freeze leaves it, and its spawnscript waits until it is unfrozen. Not on npc_vehicle.\n"
		"npccredits|0|Credits paid to the logged-in player who kills one of its NPCs, 0 to 100000. Not on npc_vehicle.\n"
		"npcxp|0|XP paid to that player, 0 to 100. Not on npc_vehicle.\n"
		"showhealth|0|1: a health bar shows when a crosshair is on the NPC.\n"
		"message||Each NPC carries a security key of that name (goodie for a goodie key): a player standing over its body takes it.\n"
		"noBasicSounds||Any value, even 0: the NPCs load and play no basic sounds (pain, death). Not on npc_vehicle.\n"
		"noCombatSounds||Any value, even 0: no combat sounds (anger, victory). Not on npc_vehicle.\n"
		"noExtraSounds||Any value, even 0: no extra sounds (chase, detect, jedi combat). Not on npc_vehicle.\n"
		"spawnscript||Script or behaviour each NPC runs when it spawns. usescript, painscript, deathscript and the other *script keys and parm1-16 are handed on the same way (see common).",
		NULL,
		"common npc_spawner npc_vehicle" },
	{ "npc_alora", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Alora (NPC type alora, or alora_dual with DUAL). An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		"1|DUAL|Spawns alora_dual (two sabers) instead.\n"
		"16|CEILING|Ceiling ambush as for Reborn (see npc), when its NPC type has an NPC class that uses it.",
		NULL,
		"npc_alora targetname alora1 spawnnow 1",
		"npc common" },
	{ "npc_bartender", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a bartender (NPC type bartender). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_bartender targetname bartender1 spawnnow 1",
		"npc common" },
	{ "npc_bespincop", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a Bespin cop (bespincop or bespincop2, chosen at random). An npc_type key replaces this choice.",
		NULL,
		NULL,
		"npc_bespincop targetname bespincop1 spawnnow 1",
		"npc common" },
	{ "npc_bobafett", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Boba Fett (NPC type boba_fett). An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		NULL,
		NULL,
		"npc_bobafett targetname boba1 spawnnow 1",
		"npc common" },
	{ "npc_chewbacca", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Chewbacca (NPC type chewie). An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		NULL,
		NULL,
		"npc_chewbacca targetname chewie1 spawnnow 1",
		"npc common" },
	{ "npc_colombian_emplacedgunner", 0, NULL,
		"Old class name that works exactly like npc_shadowtrooper: spawns a shadowtrooper or shadowtrooper2 (chosen at random) each time it is fired by its targetname. As an enemy it starts cloaked. An npc_type key replaces this choice.",
		"16|CEILING|Starts clinging where placed (ceiling ambush, ignoring alerts) and drops when it sees an enemy or is hurt.",
		NULL,
		"npc_colombian_emplacedgunner targetname gunner1 spawnnow 1",
		"npc common" },
	{ "npc_colombian_rebel", 0, NULL,
		"Old class name that works exactly like npc_reborn: spawns a Reborn (type chosen by spawnflags, see below) each time it is fired by its targetname. An npc_type key replaces this choice.",
		"1|FORCE|rebornforceuser: uses force powers.\n"
		"2|FENCER|rebornfencer: a better saber fighter.\n"
		"4|ACROBAT|rebornacrobat: very acrobatic.\n"
		"8|BOSS|rebornboss: acrobatic, good fighter, force powers.\n"
		"16|CEILING|Starts clinging where placed (ceiling ambush, ignoring alerts) and drops when it sees an enemy or is hurt.",
		NULL,
		"npc_colombian_rebel targetname rebel1 spawnnow 1",
		"npc common" },
	{ "npc_colombian_soldier", 0, NULL,
		"Old class name that works exactly like npc_reborn: spawns a Reborn (type chosen by spawnflags, see below) each time it is fired by its targetname. An npc_type key replaces this choice.",
		"1|FORCE|rebornforceuser: uses force powers.\n"
		"2|FENCER|rebornfencer: a better saber fighter.\n"
		"4|ACROBAT|rebornacrobat: very acrobatic.\n"
		"8|BOSS|rebornboss: acrobatic, good fighter, force powers.\n"
		"16|CEILING|Starts clinging where placed (ceiling ambush, ignoring alerts) and drops when it sees an enemy or is hurt.",
		NULL,
		"npc_colombian_soldier targetname soldier1 spawnnow 1",
		"npc common" },
	{ "npc_cultist", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a cultist (NPC type cultist, a blaster and force powers, or the type the spawnflags pick). The grip, lightning and drain cultists only use the force. An npc_type key replaces this choice.",
		"1|SABER|A saber cultist of random style, as npc_cultist_saber. This replaces all its other spawnflags (CINEMATIC, SHY...) with random style ones.\n"
		"2|GRIP|cultist_grip: no weapon, grip, push and pull.\n"
		"4|LIGHTNING|cultist_lightning: no weapon, lightning and push.\n"
		"8|DRAIN|cultist_drain: no weapon, drain and push.",
		NULL,
		"npc_cultist targetname cultist1 spawnnow 1",
		"npc common" },
	{ "npc_cultist_commando", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a cultist commando (NPC type cultistcommando). An npc_type key replaces this choice.",
		NULL,
		NULL,
		"npc_cultist_commando targetname commando1 spawnnow 1",
		"npc common" },
	{ "npc_cultist_destroyer", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a plain cultist (NPC type cultist). The exploding destroyer the editor describes is not used in this code. An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_cultist_destroyer targetname destroyer1 spawnnow 1",
		"npc common" },
	{ "npc_cultist_saber", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a saber cultist with no force powers: cultist_saber (fast style), or the style the spawnflags pick. THROW adds _throw to the type, e.g. cultist_saber_med_throw. An npc_type key replaces this choice.",
		"1|MED|cultist_saber_med: medium style.\n"
		"2|STRONG|cultist_saber_strong: strong style.\n"
		"4|ALL|cultist_saber_all: all three styles.\n"
		"8|THROW|Can throw its saber, alone or with one of the above.\n"
		"16|CEILING|Ceiling ambush as for Reborn (see npc), when its NPC type has an NPC class that uses it.",
		NULL,
		"npc_cultist_saber targetname cultist1 spawnnow 1",
		"npc common" },
	{ "npc_cultist_saber_powers", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a saber cultist with some force powers: cultist_saber2 (fast style), or the style the spawnflags pick. With a style THROW adds _throw, e.g. cultist_saber_med_throw2. THROW alone gives cultist_saber_throw, the one without powers. An npc_type key replaces this choice.",
		"1|MED|cultist_saber_med2: medium style.\n"
		"2|STRONG|cultist_saber_strong2: strong style.\n"
		"4|ALL|cultist_saber_all2: all three styles.\n"
		"8|THROW|Can throw its saber, alone or with one of the above.\n"
		"16|CEILING|Ceiling ambush as for Reborn (see npc), when its NPC type has an NPC class that uses it.",
		NULL,
		"npc_cultist_saber_powers targetname cultist1 spawnnow 1",
		"npc common" },
	{ "npc_desann", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Desann (NPC type desann). An npc_type key is ignored.",
		"16|CEILING|Starts clinging where placed (ceiling ambush, ignoring alerts) and drops when it sees an enemy or is hurt.",
		NULL,
		"npc_desann targetname desann1 spawnnow 1",
		"npc common" },
	{ "npc_droid_assassin", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns an assassin droid (NPC type assassin_droid). An npc_type key replaces this choice.",
		NULL,
		NULL,
		"npc_droid_assassin targetname assassin1 spawnnow 1",
		"npc common" },
	{ "npc_droid_atst", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns an AT-ST walker NPC (NPC type atst, or atst_vehicle with spawnflag 1). It is shielded and not knocked back. An npc_type key is ignored.",
		"1|VEHICLE|Spawns NPC type atst_vehicle instead. For a rideable AT-ST use npc_vehicle.",
		NULL,
		"npc_droid_atst targetname atst1 spawnnow 1",
		"npc common" },
	{ "npc_droid_gonk", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a Gonk power droid (NPC type gonk). Enemies ignore it. An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		NULL,
		NULL,
		"npc_droid_gonk targetname gonk1 spawnnow 1",
		"npc common" },
	{ "npc_droid_interrogator", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a floating interrogator droid (NPC type interrogator). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_droid_interrogator targetname interrogator1 spawnnow 1",
		"npc common" },
	{ "npc_droid_mark1", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a Mark I walking droid (NPC type mark1). It is shielded and not knocked back. An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_droid_mark1 targetname mark1 spawnnow 1",
		"npc common" },
	{ "npc_droid_mark2", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a Mark II rolling droid (NPC type mark2). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_droid_mark2 targetname mark2 spawnnow 1",
		"npc common" },
	{ "npc_droid_mouse", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a mouse droid (NPC type mouse). Enemies ignore it. An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_droid_mouse targetname mouse1 spawnnow 1",
		"npc common" },
	{ "npc_droid_probe", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a floating Imperial probe droid (NPC type probe). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_droid_probe targetname probe1 spawnnow 1",
		"npc common" },
	{ "npc_droid_protocol", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a protocol droid (NPC type protocol, or protocol_imp with IMPERIAL). Enemies ignore it. An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		"1|IMPERIAL|protocol_imp: Imperial skin.",
		NULL,
		"npc_droid_protocol targetname protocol1 spawnnow 1",
		"npc common" },
	{ "npc_droid_r2d2", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns R2-D2 (NPC type r2d2, or r2d2_imp with IMPERIAL). Enemies ignore it. An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		"1|IMPERIAL|r2d2_imp: Imperial skin.\n"
		"2|ALWAYSDIE|Never loses its head and spins when badly hurt or hit by DEMP2.",
		NULL,
		"npc_droid_r2d2 targetname r2d21 spawnnow 1",
		"npc common" },
	{ "npc_droid_r5d2", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns R5-D2 (NPC type r5d2, or r5d2_imp with IMPERIAL). Enemies ignore it. An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		"1|IMPERIAL|r5d2_imp: Imperial skin.\n"
		"2|ALWAYSDIE|Never loses its head and spins when badly hurt or hit by DEMP2.",
		NULL,
		"npc_droid_r5d2 targetname r5d21 spawnnow 1",
		"npc common" },
	{ "npc_droid_remote", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a floating training remote (NPC type remote). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_droid_remote targetname remote1 spawnnow 1",
		"npc common" },
	{ "npc_droid_saber", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a saber droid (NPC type saber_droid, or saber_droid_training with TRAINING). An npc_type key replaces this choice. The training one is removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		"1|TRAINING|saber_droid_training: the training version.",
		NULL,
		"npc_droid_saber targetname saberdroid1 spawnnow 1",
		"npc common" },
	{ "npc_droid_seeker", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a floating seeker droid (NPC type seeker). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_droid_seeker targetname seeker1 spawnnow 1",
		"npc common" },
	{ "npc_droid_sentry", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a floating, armored Imperial sentry droid (NPC type sentry). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_droid_sentry targetname sentry1 spawnnow 1",
		"npc common" },
	{ "npc_galak", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Galak (NPC type galak, or the armored galak_mech with MECH). An npc_type key is ignored.",
		"1|MECH|Spawns galak_mech, the armored Galak.",
		NULL,
		"npc_galak targetname galak1 spawnnow 1",
		"npc common" },
	{ "npc_gran", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a Gran: gran or gran2 (chosen at random), or the type the spawnflags pick. An npc_type key replaces this choice.",
		"1|SHOOTER|granshooter: uses a blaster.\n"
		"2|BOXER|granboxer: fists only.",
		NULL,
		"npc_gran targetname gran1 spawnnow 1",
		"npc common" },
	{ "npc_hazardtrooper", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a hazard trooper (NPC type hazardtrooper, or the type the spawnflags pick). An npc_type key replaces this choice.",
		"1|OFFICER|hazardtrooperofficer.\n"
		"2|CONCUSSION|hazardtrooperconcussion: concussion rifle.",
		NULL,
		"npc_hazardtrooper targetname hazard1 spawnnow 1",
		"npc common" },
	{ "npc_human_merc", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a human mercenary (NPC type human_merc, or the type the spawnflags pick). With a message key it spawns human_merc_key, a merc carrying that key, whatever the spawnflags. An npc_type key replaces this choice.",
		"1|BOWCASTER|human_merc_bow: bowcaster.\n"
		"2|REPEATER|human_merc_rep: repeater.\n"
		"4|FLECHETTE|human_merc_flc: flechette.\n"
		"8|CONCUSSION|human_merc_cnc: concussion rifle.",
		"message||Spawns human_merc_key instead, carrying a security key of that name (see npc).",
		"npc_human_merc targetname merc1 spawnnow 1",
		"npc common" },
	{ "npc_imperial", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns an Imperial officer: imperial (greyshirt), or the type the spawnflags pick. The key on its arm is hidden unless a message key gives it one. An npc_type key replaces this choice.",
		"1|OFFICER|impofficer: brownshirt officer.\n"
		"2|COMMANDER|impcommander: blackshirt commander.",
		"message||The NPC carries a security key of that name and shows it on its arm (see npc).",
		"npc_imperial targetname imperial1 spawnnow 1",
		"npc common" },
	{ "npc_impworker", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns an Imperial worker (impworker, impworker2 or impworker3, chosen at random). An npc_type key replaces this choice.",
		NULL,
		NULL,
		"npc_impworker targetname impworker1 spawnnow 1",
		"npc common" },
	{ "npc_jan", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Jan Ors (NPC type jan). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_jan targetname jan1 spawnnow 1",
		"npc common" },
	{ "npc_jawa", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a Jawa (NPC type jawa, or jawa_armed with ARMED). An npc_type key replaces this choice.",
		"1|ARMED|jawa_armed: starts with the Jawa gun in hand.",
		NULL,
		"npc_jawa targetname jawa1 spawnnow 1",
		"npc common" },
	{ "npc_jedi", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns an allied Jedi: jedi or jedi2 (chosen at random), or the type the spawnflags pick. An npc_type key replaces this choice.",
		"1|TRAINER|jeditrainer.\n"
		"2|MASTER|jedimaster.\n"
		"4|RANDOM|A random Jedi student: one of jedi_hf1, jedi_hf2, jedi_hm1, jedi_hm2, jedi_kdm1, jedi_kdm2, jedi_rm1, jedi_rm2, jedi_tf1, jedi_tf2, jedi_zf1, jedi_zf2. Wins over the other two.\n"
		"16|CEILING|Starts clinging where placed (ceiling ambush, ignoring alerts) and drops when it sees an enemy or is hurt.",
		NULL,
		"npc_jedi targetname jedi1 spawnnow 1",
		"npc common" },
	{ "npc_kothos", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns one of the Kothos twins (NPC type dkothos, or vkothos with VIL). An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		"1|VIL|Spawns Vil Kothos (vkothos) instead of Dasariah.",
		NULL,
		"npc_kothos targetname kothos1 spawnnow 1",
		"npc common" },
	{ "npc_kyle", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Kyle Katarn (NPC type kyle). An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		"16|CEILING|Starts clinging where placed (ceiling ambush, ignoring alerts) and drops when it sees an enemy or is hurt.",
		NULL,
		"npc_kyle targetname kyle1 spawnnow 1",
		"npc common" },
	{ "npc_lando", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Lando Calrissian (NPC type lando). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_lando targetname lando1 spawnnow 1",
		"npc common" },
	{ "npc_lannik_racto", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Lannik Racto (NPC type lannik_racto). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_lannik_racto targetname racto1 spawnnow 1",
		"npc common" },
	{ "npc_luke", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Luke Skywalker (NPC type luke). An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		"16|CEILING|Starts clinging where placed (ceiling ambush, ignoring alerts) and drops when it sees an enemy or is hurt.",
		NULL,
		"npc_luke targetname luke1 spawnnow 1",
		"npc common" },
	{ "npc_manuel_vergara_rmg", 0, NULL,
		"Old class name that works exactly like npc_desann: spawns Desann (NPC type desann) each time it is fired by its targetname. An npc_type key is ignored.",
		"16|CEILING|Starts clinging where placed (ceiling ambush, ignoring alerts) and drops when it sees an enemy or is hurt.",
		NULL,
		"npc_manuel_vergara_rmg targetname desann1 spawnnow 1",
		"npc common" },
	{ "npc_merchant", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a merchant (NPC type merchant). An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		NULL,
		NULL,
		"npc_merchant targetname merchant1 spawnnow 1",
		"npc common" },
	{ "npc_minemonster", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a mine monster (NPC type minemonster). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_minemonster targetname minemonster1 spawnnow 1",
		"npc common" },
	{ "npc_monmothma", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Mon Mothma (NPC type monmothma). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_monmothma targetname monmothma1 spawnnow 1",
		"npc common" },
	{ "npc_monster_claw", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a claw monster (NPC type claw). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_monster_claw targetname claw1 spawnnow 1",
		"npc common" },
	{ "npc_monster_fish", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a fish monster (NPC type fish). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_monster_fish targetname fish1 spawnnow 1",
		"npc common" },
	{ "npc_monster_flier2", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a flier monster (NPC type flier2). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_monster_flier2 targetname flier1 spawnnow 1",
		"npc common" },
	{ "npc_monster_glider", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a glider monster (NPC type glider). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_monster_glider targetname glider1 spawnnow 1",
		"npc common" },
	{ "npc_monster_howler", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a howler (NPC type howler). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_monster_howler targetname howler1 spawnnow 1",
		"npc common" },
	{ "npc_monster_lizard", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a lizard monster (NPC type lizard). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_monster_lizard targetname lizard1 spawnnow 1",
		"npc common" },
	{ "npc_monster_murjj", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a murjj monster (NPC type murjj). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_monster_murjj targetname murjj1 spawnnow 1",
		"npc common" },
	{ "npc_monster_mutant_rancor", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a mutant rancor (NPC type mutant_rancor). The editor's FASTKILL spawnflag does nothing. An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		NULL,
		NULL,
		"npc_monster_mutant_rancor targetname mutant1 spawnnow 1",
		"npc common" },
	{ "npc_monster_rancor", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a rancor (NPC type rancor). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_monster_rancor targetname rancor1 spawnnow 1",
		"npc common" },
	{ "npc_monster_sand_creature", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a sand creature (NPC type sand_creature, or sand_creature_fast with FAST). An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		"1|FAST|sand_creature_fast.",
		NULL,
		"npc_monster_sand_creature targetname sand1 spawnnow 1",
		"npc common" },
	{ "npc_monster_swamp", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a swamp monster (NPC type swamp). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_monster_swamp targetname swamp1 spawnnow 1",
		"npc common" },
	{ "npc_monster_wampa", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a wampa (NPC type wampa). An npc_type key is ignored.",
		"1|WANDER|With no enemy it wanders the waypoint network.\n"
		"2|SEARCH|With no enemy it searches between nearby waypoints for one. Wins over WANDER.",
		NULL,
		"npc_monster_wampa targetname wampa1 spawnnow 1",
		"npc common" },
	{ "npc_morgankatarn", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Morgan Katarn (NPC type morgankatarn). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_morgankatarn targetname morgan1 spawnnow 1",
		"npc common" },
	{ "npc_noghri", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a Noghri (NPC type noghri). An npc_type key replaces this choice.",
		NULL,
		NULL,
		"npc_noghri targetname noghri1 spawnnow 1",
		"npc common" },
	{ "npc_player", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns an NPC of type player. An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		NULL,
		NULL,
		"npc_player targetname player1 spawnnow 1",
		"npc common" },
	{ "npc_prisoner", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a prisoner (prisoner or prisoner2, chosen at random). An npc_type key replaces this choice.",
		NULL,
		NULL,
		"npc_prisoner targetname prisoner1 spawnnow 1",
		"npc common" },
	{ "npc_ragnos", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Ragnos (NPC type ragnos). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_ragnos targetname ragnos1 spawnnow 1",
		"npc common" },
	{ "npc_rax", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Rax (NPC type rax). The editor's FUN spawnflag does nothing. An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_rax targetname rax1 spawnnow 1",
		"npc common" },
	{ "npc_rebel", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a Rebel soldier (rebel or rebel2, chosen at random). An npc_type key replaces this choice.",
		NULL,
		NULL,
		"npc_rebel targetname rebel1 spawnnow 1",
		"npc common" },
	{ "npc_reborn", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a Reborn: reborn (weak saber fighter), or the type the spawnflags pick. An npc_type key replaces this choice.",
		"1|FORCE|rebornforceuser: uses force powers.\n"
		"2|FENCER|rebornfencer: a better saber fighter.\n"
		"4|ACROBAT|rebornacrobat: very acrobatic.\n"
		"8|BOSS|rebornboss: acrobatic, good fighter, force powers.\n"
		"16|CEILING|Starts clinging where placed (ceiling ambush, ignoring alerts) and drops when it sees an enemy or is hurt.",
		NULL,
		"npc_reborn targetname reborn1 spawnnow 1",
		"npc common" },
	{ "npc_reborn_new", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a new-style Reborn: reborn_new, or the type the spawnflags pick. An npc_type key replaces this choice.",
		"1|DUAL|reborn_dual (reborn_dual2 with WEAK): two sabers.\n"
		"2|STAFF|reborn_staff (reborn_staff2 with WEAK): saber staff.\n"
		"4|WEAK|A weaker one: reborn_new2 alone.\n"
		"16|CEILING|Starts clinging where placed (ceiling ambush, ignoring alerts) and drops when it sees an enemy or is hurt.",
		NULL,
		"npc_reborn_new targetname reborn1 spawnnow 1",
		"npc common" },
	{ "npc_reelo", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Reelo (NPC type reelo). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_reelo targetname reelo1 spawnnow 1",
		"npc common" },
	{ "npc_rockettrooper", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a flying rocket trooper (NPC type rockettrooper2, or rockettrooper2officer with OFFICER). The editor's SPOTLIGHT spawnflag (2) does nothing. An npc_type key replaces this choice.",
		"1|OFFICER|rockettrooper2officer.",
		NULL,
		"npc_rockettrooper targetname rocket1 spawnnow 1",
		"npc common" },
	{ "npc_rodian", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a Rodian: rodian (sniper), or rodian2 (blaster) with BLASTER. An npc_type key replaces this choice.",
		"1|BLASTER|rodian2: uses a blaster instead of a sniper rifle.\n"
		"2|NO_HIDE|A sniper that does not duck and hide between shots.",
		NULL,
		"npc_rodian targetname rodian1 spawnnow 1",
		"npc common" },
	{ "npc_rosh_penin", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Rosh Penin: rosh_penin, or the type the spawnflags pick. An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		"1|DARKSIDE|rosh_dark: the dark side Rosh.\n"
		"2|NOFORCE|rosh_penin_noforce: no force jump and no saber.",
		NULL,
		"npc_rosh_penin targetname rosh1 spawnnow 1",
		"npc common" },
	{ "npc_saboteur", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a saboteur: saboteur (blaster rifle), or the type the spawnflags pick. Every saboteur starts cloaked, so the editor's CLOAKED box (16) is only DROPTOFLOOR. An npc_type key replaces this choice.",
		"1|SNIPER|saboteursniper: sniper rifle.\n"
		"2|PISTOL|saboteurpistol: a pistol.\n"
		"4|COMMANDO|saboteurcommando: two pistols.",
		NULL,
		"npc_saboteur targetname saboteur1 spawnnow 1",
		"npc common" },
	{ "npc_shadowtrooper", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a shadowtrooper (shadowtrooper or shadowtrooper2, chosen at random). As an enemy it starts cloaked. An npc_type key replaces this choice.",
		"16|CEILING|Starts clinging where placed (ceiling ambush, ignoring alerts) and drops when it sees an enemy or is hurt.",
		NULL,
		"npc_shadowtrooper targetname shadow1 spawnnow 1",
		"npc common" },
	{ "npc_snowtrooper", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a snowtrooper (NPC type snowtrooper). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_snowtrooper targetname snow1 spawnnow 1",
		"npc common" },
	{ "npc_spawner", 0, NULL,
		"The general NPC spawner: spawns the NPC type named in npc_type (see /list npcs) each time it is fired by its targetname. /entadd and /entedit refuse it without a targetname or an npc_type, with a type the server does not have, or with a vehicle type (use npc_vehicle). Refused too when the server has g_allowNPC 0. All the shared keys and spawnflags are under npc.",
		NULL,
		"npc_type||Required. The NPC type to spawn, e.g. stormtrooper, reborn, jawa.",
		"npc_spawner targetname guard1 npc_type stormtrooper spawnnow 1",
		"npc common" },
	{ "npc_stormtrooper", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a stormtrooper: stormtrooper or stormtrooper2 (chosen at random), or the type the spawnflags pick. Of several spawnflags the highest wins. An npc_type key is ignored.",
		"1|OFFICER|stofficer.\n"
		"2|COMMANDER|stcommander.\n"
		"4|ALTOFFICER|stofficeralt.\n"
		"8|ROCKET|rockettrooper: rocket launcher.",
		NULL,
		"npc_stormtrooper targetname trooper1 spawnnow 1",
		"npc common" },
	{ "npc_stormtrooperofficer", 0, NULL,
		"npc_stormtrooper with its OFFICER spawnflag always set: spawns a stormtrooper officer (NPC type stofficer) each time it is fired by its targetname. Its COMMANDER (2), ALTOFFICER (4) and ROCKET (8) spawnflags still win, as on npc_stormtrooper. An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_stormtrooperofficer targetname officer1 spawnnow 1",
		"npc common npc_stormtrooper" },
	{ "npc_swamptrooper", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a swamp trooper (NPC type swamptrooper, or swamptrooper2 with REPEATER). An npc_type key replaces this choice.",
		"1|REPEATER|swamptrooper2: uses a repeater.",
		NULL,
		"npc_swamptrooper targetname swamp1 spawnnow 1",
		"npc common" },
	{ "npc_tavion", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns Tavion (NPC type tavion). An npc_type key is ignored.",
		"16|CEILING|Starts clinging where placed (ceiling ambush, ignoring alerts) and drops when it sees an enemy or is hurt.",
		NULL,
		"npc_tavion targetname tavion1 spawnnow 1",
		"npc common" },
	{ "npc_tavion_new", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns the new Tavion: tavion_new, or the type the spawnflags pick. An npc_type key is ignored. Removed at once on single-player maps when the server has rp_sp_npc_fix 1.",
		"1|SCEPTER|tavion_scepter: saber and Ragnos' scepter.\n"
		"2|SITH_SWORD|tavion_sith_sword: Ragnos' Sith sword.\n"
		"16|CEILING|Starts clinging where placed (ceiling ambush, ignoring alerts) and drops when it sees an enemy or is hurt.",
		NULL,
		"npc_tavion_new targetname tavion1 spawnnow 1",
		"npc common" },
	{ "npc_tie_pilot", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a TIE pilot (NPC type stormpilot). An npc_type key is ignored.",
		NULL,
		NULL,
		"npc_tie_pilot targetname pilot1 spawnnow 1",
		"npc common" },
	{ "npc_trandoshan", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a Trandoshan (NPC type trandoshan). An npc_type key replaces this choice.",
		NULL,
		NULL,
		"npc_trandoshan targetname trandoshan1 spawnnow 1",
		"npc common" },
	{ "npc_tusken", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a Tusken raider (NPC type tusken, or tuskensniper with SNIPER). An npc_type key replaces this choice.",
		"1|SNIPER|tuskensniper.",
		NULL,
		"npc_tusken targetname tusken1 spawnnow 1",
		"npc common" },
	{ "npc_ugnaught", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns an Ugnaught (ugnaught or ugnaught2, chosen at random). An npc_type key replaces this choice.",
		NULL,
		NULL,
		"npc_ugnaught targetname ugnaught1 spawnnow 1",
		"npc common" },
	{ "npc_vehicle", 0, NULL,
		"Vehicle spawner: spawns the vehicle named in npc_type (see /list vehicles, default swoop) each time it is fired by its targetname. Fighters, speeders and walkers start empty and idle until ridden. Takes count, delay, wait, health, spawnnow, respawn, npcteam, showhealth and the npc_target keys like any spawner, but not npceffect, npcfreeze, npccredits, npcxp, the no*Sounds keys or SHY.",
		"1|NO_PILOT_DIE|Once ridden, it explodes when its pilot has been out of it and farther than speed for dmg milliseconds.\n"
		"2|SUSPENDED|Fighters: hang in the air until someone gets in. With dropTime they first drop for that long.",
		"npc_type|swoop|The vehicle to spawn. /entadd refuses one the server does not know.\n"
		"dmg|10000|With NO_PILOT_DIE: milliseconds without a pilot before it explodes.\n"
		"speed|512|With NO_PILOT_DIE: how far (units) the pilot must be before the countdown runs.\n"
		"dropTime|0|With SUSPENDED: seconds it drops straight down when first boarded.\n"
		"model2||NPC type of the droid put in its droid slot, if it has one (default: the vehicle's own, or r2d2 or r5d2).",
		"npc_vehicle targetname bike1 npc_type swoop spawnnow 1",
		"npc common" },
	{ "npc_weequay", 0, NULL,
		"Each time it is fired by its targetname (trigger, button, /entuse) it spawns a Weequay (weequay, weequay2, weequay3 or weequay4, chosen at random). An npc_type key replaces this choice.",
		NULL,
		NULL,
		"npc_weequay targetname weequay1 spawnnow 1",
		"npc common" },
	{ "path_corner", 0, NULL,
		"A point on a func_train's path. A train reaching it fires its other targets (with the train as activator), waits there wait seconds if set, then heads for the next path_corner it targets, at this corner's speed or its own.",
		NULL,
		"targetname||Required (it is not spawned without one): the train or the previous corner names it.\n"
		"target||The next path_corner, and anything else to fire when a train arrives. None: the train stops here.\n"
		"speed|0|Speed of the leg to the next corner. 0: the train's own speed.\n"
		"wait|0|Seconds the train waits here.",
		"path_corner targetname p1 target p2",
		"common func_train" },
	{ "point_combat", RP_EH_MAPONLY | RP_EH_REMOVED, "Combat points are indexed at map start: one added later is never used and cannot be removed.",
		"A spot NPCs in combat-point mode run to for cover. Combat points are matched to waypoints once, at map start, so one added later is refused.",
		NULL,
		NULL,
		NULL,
		"common" },
	{ "ref_tag", RP_EH_MAPONLY | RP_EH_REMOVED, "ref_tags are only read by ICARUS scripts and cannot be placed by the entity commands.",
		"A named position and facing for ICARUS scripts, aimed at its target if it has one. It hands its name to the script system and frees itself at once.",
		NULL,
		NULL,
		NULL,
		"common ref_tag_huge" },
	{ "ref_tag_huge", RP_EH_MAPONLY | RP_EH_REMOVED, "ref_tags are only read by ICARUS scripts and cannot be placed by the entity commands.",
		"The same as ref_tag: a named position for ICARUS scripts that frees itself at once.",
		NULL,
		NULL,
		NULL,
		"common ref_tag" },
	{ "rp_light", 0, NULL,
		"A GalaxyRP light and nothing else: a dynamic light at its origin, with no model and nothing to collide with. Unlike the map compiler's light it exists at run time, so the Entity System can place it anywhere. Using it (by its targetname) switches it between its on and its off light.",
		"1|START_OFF|Starts switched off, showing its off light (dark by default).",
		"light|300|Radius when on, in units (up to 1020).\n"
		"color|1 1 1|Red green blue, each 0 to 1, when on.\n"
		"offlight|0|Radius when off. 0: dark.\n"
		"offcolor|1 1 1|Red green blue, each 0 to 1, when off.\n"
		"targetname||Using it switches the light on or off.",
		"rp_light light 400 targetname lamp1",
		"common" },
	{ "shooter_blaster", 0, NULL,
		"An invisible gun that fires one blaster bolt each time it is used, at its target or along its angle, with a little random spread. The bolt's damage and speed are the server's E-11 rifle settings.",
		NULL,
		"targetname||Using it fires once.\n"
		"target||An entity to shoot at (picked half a second after spawn), instead of the angle.\n"
		"angle||Direction to fire in without a target (-1 up, -2 down).\n"
		"random|1|Spread, in degrees.",
		"shooter_blaster targetname gun1 angle 90",
		"common" },
	{ "spawnpoint", RP_EH_TOPIC, NULL,
		"Where players appear: info_player_deathmatch (FFA and every mode's fallback, info_player_start becomes one), info_player_start_red/blue (Team FFA), info_player_duel/duel1/duel2 (Duel, Power Duel), team_CTF_* (CTF), info_player_siegeteam1/2 (Siege, else deathmatch) and info_player_intermission (spectators). A player appears 9 units above a random free point (in FFA and duels, among the furthest from where they died) facing its angle, and its target fires. A point with someone on it or a wall in the player's box is skipped. One added inside a wall is refused, the map's own and its fixes' points (info_player_* and the team_CTF_* spawns) cannot be removed (added ones can), and with none left players spawn where the map's first one was.",
		"1|INITIAL|info_player_deathmatch and info_player_start only: the first spawn of a listen-server host goes there. Nothing on a dedicated server.",
		"angle|0|The direction (yaw, degrees) players face on spawning.\n"
		"target||Fired each time a player spawns on the point, the player as activator.\n"
		"nobots|0|1: bots never use it (deathmatch, start_red/blue, duel points).\n"
		"nohumans|0|1: humans never use it (same classes).\n"
		"startoff|0|Siege points: 1 starts disabled until used.\n"
		"idealclass||Siege points: the siege class that prefers it.",
		NULL,
		"common info_player_deathmatch info_player_intermission team_CTF_redspawn info_player_siegeteam1" },
	{ "target", RP_EH_TOPIC, NULL,
		"target_ entities are invisible helpers that do nothing until something uses them. An entity is used when another one fires its target and the name matches its targetname (a trigger touched, a button pressed, a door moving, another target_ entity), or when an admin types /entuse <name> (NPCs and vehicles are left alone). Every entity with that name is used, at once. The use carries the activator, the player or NPC who started the chain: that is who target_kill, target_give, target_push or target_teleporter act on. With /entuse the activator is you.",
		"128|INACTIVE|Several classes have it (see each). An inactive entity is skipped when another entity fires it, until a target_activate names it. /entuse calls it directly, so then only classes that check it themselves (e.g. target_relay) stay off.",
		"targetname||The name it is used by. Names compare ignoring case, and several entities may share one: all of them are used.\n"
		"target||Fired on the entity's own event (see each class): every entity whose targetname matches is used, immediately, in entity order. One naming itself is skipped. A chain that loops back on itself is cut after 128 nested uses or 2048 uses in all (logged once per map).\n"
		"target2||target2 to target6 are extra lists only some classes fire, each on an event of its own (e.g. each step of a target_counter, an NPC's death). Plain firing only uses target.\n"
		"targetshadername||With targetshadernewname: each time the entity fires its targets (even with no target set), that map shader is swapped for the new one, for everyone. Names that are empty, too long or contain spaces, = : or @ are refused.\n"
		"delay||Firing has no built-in delay or randomness. Triggers have a delay key of their own, otherwise put a target_delay in the chain. target_random, or a target_relay with RANDOM, fires one target at random.",
		NULL,
		"common trigger target_relay target_delay" },
	{ "target_activate", 0, NULL,
		"When used, makes every entity named by its target active again: it clears the inactive state left by an INACTIVE spawnflag or a target_deactivate, so other entities can use it (and triggers that check it fire) again. It fires nothing itself.",
		NULL,
		"target||The targetname of the entities to make active (all with that name).\n"
		"targetname||Using it activates its target.",
		"target_activate targetname unlock target door1",
		"common target target_deactivate" },
	{ "target_autosave", RP_EH_MAPONLY | RP_EH_REMOVED, "Single player only: multiplayer has nothing to save, so it removes itself at spawn.",
		"Single player saved the game when this was used. GalaxyRP accepts it so single-player maps load without complaint, and it removes itself at once.",
		NULL,
		NULL,
		NULL,
		"common target" },
	{ "target_counter", 0, NULL,
		"Counts uses: once it has been used count times it fires its target, with the last user as activator. Each use short of that fires target2 instead. After firing it ignores further uses, unless bouncecount makes it start over. Good for 'press all three switches' puzzles.",
		"128|INACTIVE|Becomes inactive each time it fires its target: other entities cannot use it again until a target_activate names it. It does not start inactive, whatever the editor text says.",
		"count|2|Uses needed before it fires its target.\n"
		"target||Fired when the count is reached.\n"
		"target2||Fired on each use that does not reach the count.\n"
		"bouncecount|0|How many times it resets to the full count after firing. -1 resets forever, 0 means it fires once and then ignores uses.\n"
		"targetname||Each use counts one.",
		"target_counter targetname switches count 3 target door1",
		"common target target_activate" },
	{ "target_deactivate", 0, NULL,
		"When used, makes every entity named by its target inactive: other entities firing it are ignored (and triggers that check it stop firing) until a target_activate names it. It fires nothing itself. An admin's /entuse still reaches an inactive entity unless its class checks the state itself.",
		NULL,
		"target||The targetname of the entities to make inactive (all with that name).\n"
		"targetname||Using it deactivates its target.",
		"target_deactivate targetname lockdown target door1",
		"common target target_activate" },
	{ "target_delay", 0, NULL,
		"Waits, then fires its target. Each use starts the countdown again and remembers who used it, and that one is the activator when it fires. With NO_RETRIGGER a use during the countdown is ignored instead.",
		"1|NO_RETRIGGER|A use while it is counting down is ignored, instead of restarting the countdown.",
		"wait|1|Seconds to wait before firing. 0 is taken as 1.\n"
		"delay||Same as wait, and read instead of it when present (older maps).\n"
		"random|0|Seconds of variance: the wait is wait plus or minus up to random.\n"
		"target||Fired when the countdown ends.\n"
		"targetname||Using it starts the countdown.",
		"target_delay targetname later wait 3 target lights1",
		"common target" },
	{ "target_escapetrig", RP_EH_MAPONLY | RP_EH_REMOVED, "Only works in the single-player gametype, which GalaxyRP does not run: it removes itself at spawn.",
		"Starts, or with escapegoal ends, a timed escape in the single-player gametype (survivors get points and the round ends). In every gametype GalaxyRP runs it removes itself at spawn.",
		NULL,
		"escapetime|60000|Milliseconds given for the escape.\n"
		"escapegoal|0|Non-zero: using it ends an escape in progress instead of starting one.",
		NULL,
		"common target" },
	{ "target_give", 0, NULL,
		"Gives the activator the items named by its target, as if they had walked over them (the usual pickup rules apply: a player at full health gets no medpak), then keeps those items off the map. Each use gives them again, so the items work as its stock: place them anywhere, they spawn hidden because they have a targetname. Using an item's own name still shows it on the map, and the next give takes it off again. An item someone picked up off the map is not given until it respawns. A player who cannot take one (full health for a medpak) leaves it where it is.",
		NULL,
		"target||The targetname of the item entities to give (all with that name). Entities that are not items are skipped.\n"
		"targetname||Using it gives the items to the activator.",
		"target_give targetname gift target giftitem",
		"common target" },
	{ "target_interest", RP_EH_MAPONLY | RP_EH_REMOVED, "Nothing in MP looks at interest points: it stores one and removes itself.",
		"Single player's look-at point for squadmates. In MP it records the point and removes itself at spawn, and no MP code ever uses those points, so it has no effect.",
		NULL,
		"target||Single player fired this when someone looked at the point. Never fired in MP.",
		NULL,
		"common target" },
	{ "target_kill", 0, NULL,
		"Kills the activator when used: 100000 damage that ignores god mode and armor (a player with chat protection on is still spared). Nobody deals it, so the Death System does not knock the victim down first: it is an outright death. Used through /entuse, the activator is the admin.",
		NULL,
		"targetname||Using it kills whoever started the chain.",
		"target_kill targetname pit",
		"common target" },
	{ "target_laser", 0, NULL,
		"A damaging beam reaching up to 2048 units, toward its target or along its angle, that hurts the first thing it hits 10 times a second. Using it switches it on or off, and the one who switched it on is credited with the damage.",
		"1|START_ON|On from the start, without being used.",
		"target||Name of an entity to aim at: the beam points at its middle and follows it as it moves. Without one, the angle sets the direction.\n"
		"angle||Direction of the beam when there is no target (-1 up, -2 down).\n"
		"dmg|1|Damage per hit, 10 hits a second.\n"
		"targetname||Using it toggles the beam.",
		"target_laser targetname laser1 angle 90 dmg 5",
		"common target" },
	{ "target_level_change", 0, NULL,
		"Changes the server to another map, for everyone, the moment it is used (devmap when sv_cheats is on). Who triggered it is logged. Any player who trips a trigger firing it ends the current map, so place it with care.",
		NULL,
		"mapname||Required: the map to change to, e.g. mp/ffa1. Placed with the entity commands it must be a plain map name the server has (maps/<name>.bsp), or it is refused. A name containing a command separator is refused when used.\n"
		"targetname||Using it changes the map.",
		"target_level_change targetname exit mapname mp/ffa1",
		"common target" },
	{ "target_location", RP_EH_MAPONLY, "It only registers its name and removes itself at once, so there is nothing for the Entity System to keep, edit or save: locations belong in the map.",
		"Names the area around it: team chat (say_team, and tells between teammates) and scripts asking for a location get the name of the nearest target_location in view (PVS). It registers the name and removes itself, so /entadd says it did not survive spawning and there is no entity left to edit or remove: the name stays until the map changes. At most 64 per map. With a targetname it is only a position marker instead.",
		NULL,
		"message||Required without a targetname: the location's name. Missing, the entity is refused.\n"
		"count|0|Colour of the name: 1 red, 2 green, 3 yellow, 4 blue, 5 cyan, 6 magenta, 7 white, 0 none.\n"
		"targetname||If set, it names no location: it stays as a plain position others can target, like target_position.",
		"target_location message Cantina count 3",
		"common target target_position" },
	{ "target_play_music", 0, NULL,
		"Changes the background music for everyone on the server when used. With one file that file loops, with two the first plays as an intro and the second then loops. A change that would overflow the gamestate is refused and logged.",
		NULL,
		"music||Required: one or two music file paths (intro, then loop), each under 64 characters, e.g. music/hoth2/hoth2_explore.mp3. Missing or invalid, the entity is refused.\n"
		"targetname||Using it starts the music.",
		"target_play_music targetname song music music/hoth2/hoth2_explore.mp3",
		"common target" },
	{ "target_position", 0, NULL,
		"An invisible point that other entities aim at by name: the top of a target_push or trigger_push throw, a target_teleporter destination, a target_laser aim point and the like. It does nothing by itself.",
		NULL,
		"targetname||The name other entities use to point at it.\n"
		"angle||Facing given to whoever a target_teleporter sends to it.",
		"target_position targetname apex1",
		"common target target_push target_teleporter" },
	{ "target_print", 0, NULL,
		"Prints its message in the middle of the screen when used: to everyone, to one team, or only to the activator. A message starting with @ is looked up in the game's string files. Text placed with the entity commands has any run of three @ signs broken up for safety.",
		"1|REDTEAM|Only players on the red team see it.\n"
		"2|BLUETEAM|Only players on the blue team see it (with 1, both teams do).\n"
		"4|PRIVATE|Only the activator sees it (overrides 1 and 2).",
		"message||The text to print. Without one, using it prints nothing.\n"
		"wait|0|Milliseconds (not seconds) during which further uses are ignored.\n"
		"targetname||Using it prints the message.",
		"target_print targetname hello message Welcome spawnflags 4",
		"common target" },
	{ "target_push", 0, NULL,
		"Throws the activator (a player or NPC, on foot or floating) when used: along its angle at speed, or, with a target, in an arc whose top is that entity. It does not push on touch: fire it from a trigger.",
		"1|BOUNCEPAD|Plays the force jump sound on the one pushed (at most every 1.5 seconds).\n"
		"2|CONSTANT|With a target: a straight push toward it at speed instead of an arc.",
		"speed|1000|Push speed in units per second (without a target, or with CONSTANT).\n"
		"angle||Push direction without a target (-1 up, -2 down).\n"
		"target||A target_position to throw toward: the top of the arc, which must be above the target_push or it removes itself (logged). It is also removed if nothing has that name.\n"
		"targetname||Using it pushes the activator.",
		"target_push targetname launch angle -1 speed 800",
		"common target target_position trigger_push" },
	{ "target_random", 0, NULL,
		"Each use fires just one of the entities named by its target, picked at random with equal chances. If the pick is an entity that cannot be used, nothing fires that time.",
		"1|USEONCE|Works only once.",
		"target||The targetname shared by the candidates: give several entities the same name to choose between them.\n"
		"targetname||Using it fires one target.",
		"target_random targetname dice target prize",
		"common target target_relay" },
	{ "target_relay", 0, NULL,
		"Passes a use on: when used it fires its target, keeping the original activator. Use it to fire several things from one trigger, to filter by team, to fire one target at random, or to fire only once.",
		"1|RED_ONLY|Ignores uses by players not on the red team (non-players pass).\n"
		"2|BLUE_ONLY|Ignores uses by players not on the blue team (non-players pass).\n"
		"4|RANDOM|Fires only one entity, picked at random among those with the target name (the first 32).\n"
		"128|INACTIVE|Starts inactive: it ignores every use, /entuse included, until a target_activate names it.",
		"target||Fired when it is used.\n"
		"wait||-1: fire only once, then the relay removes itself (with a usescript it stays but can never be used again). Other values do nothing.\n"
		"targetname||Using it fires its target.",
		"target_relay targetname relay1 target door1 wait -1",
		"common target target_random" },
	{ "target_remove_powerups", 0, NULL,
		"Takes every powerup from the activator when used: a carried CTF flag goes back to its base, cloak is switched off properly, and all other powerups (speed, enlightenment, ysalamiri and the rest) are cleared.",
		NULL,
		"targetname||Using it strips the activator's powerups.",
		"target_remove_powerups targetname strip",
		"common target" },
	{ "target_score", 0, NULL,
		"Adds count points to the activator's score when used. Nothing happens when the activator is not a player or NPC, or during warmup.",
		NULL,
		"count|1|Points to add (negative takes points away). 0 is taken as 1.\n"
		"targetname||Using it scores the activator.",
		"target_score targetname bonus count 5",
		"common target" },
	{ "target_screenshake", 0, NULL,
		"Shakes the screen when used: for everyone on the server, or with globalshake 0 only for players whose view could include the spot (PVS). The shake is the same strength at any distance.",
		NULL,
		"intensity|10|Strength of the shake.\n"
		"duration|800|Length in milliseconds.\n"
		"globalshake|1|1: every client shakes. 0: only clients that can potentially see the entity.\n"
		"targetname||Using it starts the shake.",
		"target_screenshake targetname quake intensity 20 duration 1500",
		"common target" },
	{ "target_scriptrunner", 0, NULL,
		"Runs its usescript (an ICARUS script) when used, on itself or, with RUNONACTIVATOR, on whoever used it. It runs count times at most, can wait delay seconds before running, and ignores uses during its wait.",
		"1|RUNONACTIVATOR|Runs the script on the activator (player, NPC or entity) instead of on the scriptrunner. The activator is given a script name if it has none.\n"
		"128|INACTIVE|Starts inactive: other entities cannot use it until a target_activate names it.",
		"usescript||The script to run, given without the scripts/ folder (it is added).\n"
		"count|1|How many times it can run. -1 means no limit.\n"
		"delay|0|Seconds (fractions allowed) between the use and the run.\n"
		"wait|0|Seconds after a run during which uses are ignored. With a delay set too, the script runs again by itself every wait seconds until count is used up.\n"
		"targetname||Using it runs the script.",
		"target_scriptrunner targetname runit usescript hoth3/elevator",
		"common target" },
	{ "target_secret", 0, NULL,
		"Marks a secret area: when a player uses it (usually through a trigger), everyone is told who found a secret area and that player hears the secret sound. It works once, then ignores uses. Only players count, not NPCs.",
		NULL,
		"count|0|Total secrets on the map, shown as (found / total). Each target_secret counts only itself, so it always reads 1 / count. 0 shows no numbers.\n"
		"targetname||Using it reveals the secret.",
		"target_secret targetname secret1",
		"common target" },
	{ "target_siege_end", RP_EH_GAMETYPE, "Siege only: in any other gametype it removes itself.",
		"Ends the round (Round ended) when used. Siege only: in any other gametype, or on a map without a valid siege file, it removes itself at spawn.",
		NULL,
		"targetname||Using it ends the round.",
		"target_siege_end targetname endround",
		"common target" },
	{ "target_speaker", 0, NULL,
		"Plays a sound. Normally each use plays noise once at the speaker. The LOOPED flags make it a looping sound that each use switches on or off. GLOBAL plays it at full volume everywhere, ACTIVATOR on the one who used it. Given a soundSet instead, it plays that ambient sound set by itself and ignores uses.",
		"1|LOOPED_ON|Looping sound, on from the start: each use toggles it.\n"
		"2|LOOPED_OFF|Looping sound, off at the start: each use toggles it.\n"
		"4|GLOBAL|Each use plays it at full volume for everyone (not with looping).\n"
		"8|ACTIVATOR|Each use plays it on the activator (not with looping). Forced on when noise starts with *.",
		"noise||Required unless soundSet is given: the sound file to play. Missing, the entity is not spawned.\n"
		"soundSet||An ambient sound set from sound/sound.txt, played instead of noise. It is sent only to players within hearing range of the set (its radius in sound.txt, plus 512). Placed with the entity commands, an unknown set is refused.\n"
		"wait|0|Seconds between automatic replays, played by the clients without any use. Only works when random is non-zero too (a stock quirk).\n"
		"random|0|Seconds of variance on wait. Must be non-zero for wait to repeat at all.\n"
		"targetname||Using it plays or toggles the sound.",
		"target_speaker targetname bell noise sound/movers/doors/door1start.wav",
		"common target" },
	{ "target_teleporter", 0, NULL,
		"Teleports the activator (a player or NPC) to the entity named by its target when used, facing that entity's angle and moving forward a little. A player or NPC standing at the destination is telefragged (an arriving NPC spares players). If several entities share the name, one is picked at random.",
		NULL,
		"target||Required: the destination, usually a target_position or misc_teleporter_dest. Missing, using it does nothing.\n"
		"targetname||Using it teleports the activator.",
		"target_teleporter targetname tele1 target apex1",
		"common target target_position" },
	{ "team_CTF_blueflag", RP_EH_GAMETYPE, "CTF and Capture the Ysalamiri only: in any other gametype it removes itself.",
		"The blue team's flag, for CTF and CTY. In any other gametype it removes itself a moment after spawning (the add still "
		"reports it). Red players take it and score by bringing it to their own flag at base, and a blue player touching it where "
		"it was dropped sends it home. In CTY whoever carries a flag is under a ysalamiri's effect.",
		NULL,
		NULL,
		"team_CTF_blueflag",
		"items common team_CTF_redflag" },
	{ "team_CTF_blueplayer", 0, NULL,
		"CTF and Capture the Ysalamiri only: blue players spawn here on their first spawn after joining the game or the team, later spawns use team_CTF_bluespawn. A random free one is used (all taken: the first one), and with none on the map an info_player_deathmatch. /entaddaim stands it on the surface aimed at, as a player would stand there. The map's own and the per-map fixes' ones cannot be removed (added ones can).",
		NULL,
		"angle|0|The direction (yaw, degrees) players face when they spawn here.\n"
		"target||Fired each time a player spawns here, with the player as activator.",
		"team_CTF_blueplayer angle 180",
		"common spawnpoint team_CTF_bluespawn" },
	{ "team_CTF_bluespawn", 0, NULL,
		"CTF and Capture the Ysalamiri only: blue players respawn here after their first spawn (that one uses team_CTF_blueplayer). A random free one is used (all taken: the first one), and with none on the map an info_player_deathmatch. /entaddaim stands it on the surface aimed at, as a player would stand there. The map's own and the per-map fixes' ones cannot be removed (added ones can).",
		NULL,
		"angle|0|The direction (yaw, degrees) players face when they spawn here.\n"
		"target||Fired each time a player spawns here, with the player as activator.",
		"team_CTF_bluespawn angle 180",
		"common spawnpoint team_CTF_blueplayer" },
	{ "team_CTF_neutralflag", RP_EH_MAPONLY, "No gametype uses it: removed outside CTF and CTY, and nobody can take it in them.",
		"The neutral flag of one-flag CTF, a gametype Jedi Academy does not have. It removes itself outside CTF and CTY, and in "
		"them nobody can pick it up.",
		NULL,
		NULL,
		NULL,
		"items common" },
	{ "team_CTF_redflag", RP_EH_GAMETYPE, "CTF and Capture the Ysalamiri only: in any other gametype it removes itself.",
		"The red team's flag, for CTF and CTY. In any other gametype it removes itself a moment after spawning (the add still "
		"reports it). Blue players take it and score by bringing it to their own flag at base, and a red player touching it where "
		"it was dropped sends it home. In CTY whoever carries a flag is under a ysalamiri's effect.",
		NULL,
		NULL,
		"team_CTF_redflag",
		"items common team_CTF_blueflag" },
	{ "team_CTF_redplayer", 0, NULL,
		"CTF and Capture the Ysalamiri only: red players spawn here on their first spawn after joining the game or the team, later spawns use team_CTF_redspawn. A random free one is used (all taken: the first one), and with none on the map an info_player_deathmatch. /entaddaim stands it on the surface aimed at, as a player would stand there. The map's own and the per-map fixes' ones cannot be removed (added ones can).",
		NULL,
		"angle|0|The direction (yaw, degrees) players face when they spawn here.\n"
		"target||Fired each time a player spawns here, with the player as activator.",
		"team_CTF_redplayer angle 0",
		"common spawnpoint team_CTF_redspawn" },
	{ "team_CTF_redspawn", 0, NULL,
		"CTF and Capture the Ysalamiri only: red players respawn here after their first spawn (that one uses team_CTF_redplayer). A random free one is used (all taken: the first one), and with none on the map an info_player_deathmatch. /entaddaim stands it on the surface aimed at, as a player would stand there. The map's own and the per-map fixes' ones cannot be removed (added ones can).",
		NULL,
		"angle|0|The direction (yaw, degrees) players face when they spawn here.\n"
		"target||Fired each time a player spawns here, with the player as activator.",
		"team_CTF_redspawn angle 0",
		"common spawnpoint team_CTF_redplayer" },
	{ "terrain", RP_EH_MAPONLY | RP_EH_REMOVED, "Random-map (RMG) terrain, which this engine does not have: it removes itself.",
		"Jedi Academy's random-map (RMG) terrain: a heightmap landscape the original game built at load. The OpenJK base removed RMG, "
		"so the entity does nothing and removes itself at once.",
		NULL,
		NULL,
		NULL,
		"common" },
	{ "trigger", RP_EH_TOPIC, NULL,
		"Triggers are invisible volumes that react to what is inside them. The volume is a brush model (model *N) or, with no model, the box mins/maxs around the origin. Living players and NPCs touch triggers (not while in noclip), spectators only teleporters. The activator handed to the targets is whoever touched it, or whoever used it. target_deactivate switches most triggers off and target_activate on, and an off trigger also ignores being used. /entedit switches a deactivated trigger back on unless it has INACTIVE.",
		"128|INACTIVE|Starts off until target_activate (or a script) switches it on. Honoured by trigger_once, trigger_multiple, trigger_hurt, trigger_push, trigger_teleport and trigger_visible.\n"
		"2048|MULTIPLE|trigger_once, trigger_multiple and a LINEAR trigger_push: several entities can set it off in the same frame.",
		"model||*N, one of this map's inline models from *1 (*0, the map itself, and a number this map does not have are refused). Its shape is the model's own moved by the origin, so with /entadd it is offset by where you stand. /entaddaim sets it on the surface aimed at.\n"
		"mins|0 0 0|With no model: lower corner of the box relative to the origin, e.g. -64 -64 0. The console logs a missing brush model but the box works.\n"
		"maxs|0 0 0|With no model: upper corner of the box relative to the origin, e.g. 64 64 128.\n"
		"wait|0|trigger_multiple: seconds before it can fire again. -1 fires once only, 0 fires again every frame while touched.\n"
		"random|0|trigger_multiple: seconds added to or taken from wait at random. Kept below wait.\n"
		"delay|0|trigger_once and trigger_multiple: whole seconds between being set off and firing the targets.\n"
		"soundSet||trigger_once and trigger_multiple: an ambient sound set from sound/sound.txt that becomes the whole map's ambience, for everyone, when it fires. GalaxyRP refuses a name that file lacks, and the set plays at once without a reconnect.\n"
		"team||trigger_once and trigger_multiple: 1 or 2, only clients on that team (red or blue) can touch it, so nobody in a game without teams. trigger_hurt: Siege only.\n"
		"targetname||Being used: trigger_once and trigger_multiple fire, trigger_hurt and trigger_lightningstrike switch on or off.",
		NULL,
		"common target" },
	{ "trigger_always", 0, NULL,
		"Fires its targets once, 0.3 seconds after it spawns, with itself as the activator, then removes itself. Placed with /entadd it is a one-shot 'fire now'. It has no volume and nothing touches it.",
		"1024|COUNT_DELAY|Waits count milliseconds instead of 0.3 seconds (only values over 300 count), e.g. to keep it around long enough for /entsave.",
		"target||What it fires.\n"
		"count||With spawnflag 1024: milliseconds to wait before firing (300 at least).",
		"trigger_always target mydoor",
		"common trigger trigger_once" },
	{ "trigger_asteroid_field", 0, NULL,
		"Keeps moving asteroids in its volume: copies of a random entity named by target (usually func_rotating templates: model, health, scale, material...), each flying from one face of the volume to the opposite one, spinning, and removed on arrival. It needs a brush model (mins/maxs alone are refused) and the asteroids fly through that inline model's own place in the map: the origin is not added. Not solid or touchable itself.",
		NULL,
		"model||Required: *N, the inline model whose bounds are the field.\n"
		"target||Targetname of the asteroid templates to copy (one is enough, with several each asteroid copies one at random). With none found it logs once and looks again every 10 seconds.\n"
		"count|1|Most asteroids at one time, 1 to 64 (none, 0 or less: 1).\n"
		"speed|10000|Average speed in units per second: each asteroid moves at 0.25 to 2 times this.",
		"trigger_asteroid_field model *1 target asteroid count 5 speed 400",
		"common trigger func_rotating" },
	{ "trigger_hurt", 0, NULL,
		"Damages players, NPCs and vehicles touching it: dmg every 0.1 seconds, or once a second with SLOW. The timer is the trigger's, so with several inside one is hurt per tick. With CAN_TARGET, using it switches it on and off and the player who used it gets the kill credit. Armor, shields, god mode and spawn protection count unless NO_PROTECTION is set.",
		"1|START_OFF|Starts off (removed from the world). Only CAN_TARGET can switch it on.\n"
		"2|CAN_TARGET|Using it switches it on and off.\n"
		"4|SILENT|No effect: no hurt sound is played anyway.\n"
		"8|NO_PROTECTION|The damage ignores armor, shields, god mode and spawn protection.\n"
		"16|SLOW|Hurts once a second instead of every 0.1 seconds.\n"
		"128|INACTIVE|Starts off: target_activate switches it on.",
		"dmg|5|Damage per tick. -1 is a fall to death: a player ragdolls, screams and dies 3 seconds later (credited to whoever pushed them in), an NPC dies at once.\n"
		"targetname||With CAN_TARGET: using it switches it on or off.\n"
		"team||Siege only: 1 or 2, only that team is hurt.",
		"trigger_hurt model *1 dmg 10 spawnflags 16",
		"common trigger" },
	{ "trigger_hyperspace", 0, NULL,
		"Hyperspace gate for vehicles. A piloted vehicle entering it starts a 4 second jump: it turns to the angles of target and plays the effect, and 3 seconds in (it must still be in the volume) it is moved to the same offset around target2 as it had around target, facing target2's angles. The pilot goes with it, passengers do not. An empty or damaged vehicle is destroyed. Players and NPCs on foot are ignored.",
		NULL,
		"target||Required (not spawned without it): the reference point, e.g. a target_position. Its angles are the way the ship turns. Missing at jump time: the jump fails, logged.\n"
		"target2||Required (not spawned without it): the arrival point, e.g. a target_position. Missing at jump time: the jump fails, logged.",
		"trigger_hyperspace model *1 target hyper_from target2 hyper_to",
		"common trigger target_position trigger_shipboundary" },
	{ "trigger_lightningstrike", 0, NULL,
		"Lightning strikes a random point of its volume every wait plus up to random milliseconds. The effect plays from just under the top of the volume and the strike runs down to its bottom: the first thing it hits takes dmg, or, with radius, everything near the impact point. Using it switches it on and off. Without lightningfx it is not spawned (logged).",
		"1|START_OFF|Starts off: using it switches it on.",
		"lightningfx||Required: the effect, e.g. env/huge_lightning. One starting with * is a weather command and is refused (a map's own trigger is not spawned, logged).\n"
		"wait|1000|Milliseconds between strikes, at least 100.\n"
		"random|2000|Up to this many milliseconds added at random each time.\n"
		"dmg|50|Damage per strike.\n"
		"radius|0|If set (at most 4096): radius damage around the impact point instead of only what the strike hits.\n"
		"targetname||Using it switches it on or off.",
		"trigger_lightningstrike model *1 lightningfx env/huge_lightning dmg 30",
		"common trigger" },
	{ "trigger_location", 0, NULL,
		"Names an area. Team chat (and private messages to a teammate in team games) shows the message of the trigger_location the speaker is in as their location, and a script's get SET_LOCATION returns it, before any target_location is looked at. Nothing touches or fires it.",
		NULL,
		"message||The location name. Without one it is ignored.",
		"trigger_location model *1 message Cantina",
		"common trigger target_location" },
	{ "trigger_multiple", 0, NULL,
		"A repeatable trigger: fires its targets when a player or NPC touching it meets its conditions, or when it is used, then waits wait seconds. Its usescript runs and soundSet (if any) becomes the map ambience each time. With target2, target fires only once until the trigger has been clear for speed seconds, then target2 fires. Unlike the original game, random does not break wait.",
		"1|CLIENTONLY|NPCs cannot set it off by touch.\n"
		"2|FACING|Only fires while the toucher looks within 45 degrees of its angle.\n"
		"4|USE_BUTTON|Only fires while a player in it presses Use (not attacking, not following). Plays the button animation and shows the use hint.\n"
		"8|FIRE_BUTTON|Only fires while the toucher presses attack or alt attack.\n"
		"16|NPCONLY|Only NPCs can set it off by touch.\n"
		"128|INACTIVE|Starts off: target_activate switches it on.\n"
		"2048|MULTIPLE|Several entities can set it off in the same frame.",
		"target||Fired each time, with the toucher as activator.\n"
		"targetname||Using it sets it off too: same wait, delay and INACTIVE rules, none of the touch conditions.\n"
		"wait|0|Seconds before it can fire again. -1: fires once, then does nothing. 0: fires again every frame while touched.\n"
		"random|0|Seconds added to or taken from wait at random. Cut to wait minus 0.1 if not below wait.\n"
		"delay|0|Whole seconds between being set off and firing.\n"
		"target2||Fired once the trigger has been clear (nobody meeting its conditions) for speed seconds after firing. Not with wait -1.\n"
		"speed|1|With target2: seconds it must stay clear before target2 fires.\n"
		"angle||With FACING: the direction to look. 0 does not work, use 360 for east.\n"
		"noise||Sound file played on the activator each time it fires.\n"
		"soundSet||Ambient sound set from sound/sound.txt that becomes the map ambience for everyone when it fires.\n"
		"team||1 or 2: only clients on that team can touch it (nobody in a game without teams). teamuser does the same.\n"
		"NPC_targetname||Only the NPC with this script_targetname can touch it, players cannot. Ignored with CLIENTONLY.\n"
		"usetime|0|With USE_BUTTON: milliseconds a player must hold Use, staying inside and facing the same way (at most 60000).\n"
		"siegetrig|0|Siege only. Any other value outside Siege means it never fires. The same for teambalance.",
		"trigger_multiple model *1 target mylight wait 2 spawnflags 4",
		"common trigger trigger_once target_activate" },
	{ "trigger_once", 0, NULL,
		"Like trigger_multiple but fires only once: when a player or NPC touching it meets its conditions, or when it is used, it fires its targets (its usescript runs and soundSet becomes the map ambience), then it stays in the map doing nothing more. wait, random, target2 and speed are not used.",
		"1|CLIENTONLY|NPCs cannot set it off by touch.\n"
		"2|FACING|Only fires while the toucher looks within 45 degrees of its angle.\n"
		"4|USE_BUTTON|Only fires while a player in it presses Use. Plays the button animation and shows the use hint.\n"
		"8|FIRE_BUTTON|Only fires while the toucher presses attack or alt attack.\n"
		"16|NPCONLY|Only NPCs can set it off by touch.\n"
		"128|INACTIVE|Starts off: target_activate switches it on.\n"
		"2048|MULTIPLE|Several entities can set it off in the same frame.",
		"target||Fired once, with the toucher as activator.\n"
		"targetname||Using it sets it off too, without the touch conditions.\n"
		"delay|0|Whole seconds between being set off and firing.\n"
		"angle||With FACING: the direction to look. 0 does not work, use 360 for east.\n"
		"noise||Sound file played on the activator when it fires.\n"
		"soundSet||Ambient sound set from sound/sound.txt that becomes the map ambience for everyone when it fires.\n"
		"team||1 or 2: only clients on that team can touch it (nobody in a game without teams). teamuser does the same.\n"
		"NPC_targetname||Only the NPC with this script_targetname can touch it, players cannot. Ignored with CLIENTONLY.\n"
		"usetime|0|With USE_BUTTON: milliseconds a player must hold Use, staying inside and facing the same way (at most 60000).\n"
		"siegetrig|0|Siege only. Any other value outside Siege means it never fires.",
		"trigger_once model *1 target mydoor",
		"common trigger trigger_multiple" },
	{ "trigger_push", 0, NULL,
		"A jump pad. By default it throws the players and NPCs touching it in an arc whose top is its target, worked out from g_gravity when it spawns. LINEAR and RELATIVE push in a straight line instead (a conveyor belt with CONVEYOR). Those are worked out by the server alone, so on a lagging player's screen they act a little later than an arc throw. If the target is missing, or an arc target is not above the trigger's centre, the trigger removes itself (logged).",
		"1|PLAYERONLY|Pushes players only, not NPCs or objects.\n"
		"2|NO_TOUCH|Never pushes: nothing switches it on (single player is the same). Do not set.\n"
		"4|LINEAR|Pushes in a straight line toward the target, in the direction from the trigger's centre, always at 1000 units per second (the speed key is overwritten, as in single player).\n"
		"8|NPCONLY|Pushes NPCs only (PLAYERONLY wins when both are set).\n"
		"16|RELATIVE|Pushes each toucher from where it is toward the target, at speed.\n"
		"32|CONVEYOR|Pushes only what stands on it, nothing in the air.\n"
		"128|INACTIVE|Starts off: target_activate switches it on.\n"
		"2048|MULTIPLE|LINEAR or RELATIVE: several entities can be pushed in the same frame.",
		"target||Required: the entity at the top of the arc (a target_position), or the one a straight push goes toward.\n"
		"speed|0|RELATIVE: units per second toward the target. 0: as fast as the toucher is far from it. LINEAR sets it to 1000.\n"
		"wait|0|LINEAR or RELATIVE, in milliseconds here: time between pushes. -1 pushes once only.",
		"trigger_push model *1 target jumptop",
		"common trigger target_position target_push" },
	{ "trigger_shipboundary", 0, NULL,
		"Map edge for vehicles. A piloted vehicle touching it is turned toward the target and flown that way for twice traveltime milliseconds. An empty or damaged vehicle is destroyed, and so is any vehicle when the target is missing or not networked (logged). Players and NPCs on foot are ignored. Without target or traveltime it is not spawned (logged).",
		NULL,
		"target||Required: the entity to turn toward. An info_notnull, info_null or target_position is made networked only when the map or an entity file loads, so one added during play makes ships explode instead.\n"
		"traveltime||Required, milliseconds: the ship is steered toward the target for twice this.",
		"trigger_shipboundary model *1 target turnpoint traveltime 2000",
		"common trigger trigger_hyperspace info_notnull" },
	{ "trigger_space", 0, NULL,
		"Space. Players and NPCs whose origin is inside have almost no gravity and, after 0.5 seconds, suffocate: 50 to 70 damage every 0.1 to 0.2 seconds, armor ignored. Vehicles and droids do not suffocate, and a rider inside an enclosed cockpit is protected. NPCs suffocate too, and lose the space state when they leave. It cannot be switched off (INACTIVE does nothing).",
		NULL,
		NULL,
		"trigger_space model *1",
		"common trigger" },
	{ "trigger_teleport", 0, NULL,
		"Teleports the living players, spectators and NPCs touching it to its target: they arrive at its origin facing its angles, thrown forward, and kill whoever stands there (telefrag). If the target is missing it prints an error and does nothing. Clients predict the teleport only with a brush model.",
		"1|SPECTATOR|Only spectators are teleported.\n"
		"128|INACTIVE|Starts off: target_activate switches it on.",
		"target||Required: the destination, e.g. a target_position or misc_teleporter_dest. One is picked at random if several share the name.",
		"trigger_teleport model *1 target tele_dest",
		"common trigger target_position misc_teleporter_dest" },
	{ "trigger_visible", 0, NULL,
		"Fires its targets once, with the player as activator, when a living player looks at its origin, then removes itself. Checked every 0.5 seconds. Needs a clear line of sight to the origin unless NOTRACE, so keep the origin out of walls. It has no volume. Using it switches it off.",
		"1|NOTRACE|Seen through walls and anything else.\n"
		"2|FORCESIGHT|Only players with Force Sight active see it.\n"
		"128|INACTIVE|Starts off: target_activate switches it on (being used does not).",
		"target||Fired when it is seen.\n"
		"radius|0|Most distance from the player's eyes, 0 for any.\n"
		"FOV|0|Most degrees off the centre of view, up or sideways. 0: anywhere in front.\n"
		"targetname||Using it switches it off.",
		"trigger_visible target mylight radius 512",
		"common trigger" },
	{ "waypoint", RP_EH_MAPONLY | RP_EH_REMOVED, "Waypoints cannot be added after map load: the navigation paths were computed at map start, and an NPC pathing to a later node would crash the server.",
		"A node of the NPC navigation graph. At map load each one is added to the graph, with a radius measured from the walls around it, and the entity is then removed. NPCs walking to a goal path along these nodes. One stuck in a wall (even at crouch height) is dropped with an error unless SOLID_OK is set.",
		"1|SOLID_OK|Added even when inside solid, for a spot that is clear in game (under a lift that starts at the top).",
		"targetname||The name other waypoints' target keys link to.\n"
		"target||A waypoint (by targetname) this one is always linked to, on top of the links found automatically. target2, target3 and target4 add more.",
		NULL,
		"common waypoints waypoint_small" },
	{ "waypoint_navgoal", RP_EH_MAPONLY | RP_EH_REMOVED, "Navgoal waypoints are only read by ICARUS scripts and cannot be placed by the entity commands.",
		"A named destination for ICARUS scripts: at map load it records its targetname, position and angles as a navgoal, then removes itself. A script's set navgoal with that name sends an NPC walking there. It is not part of the navigation graph.",
		"1|SOLID_OK|No in-solid error at load.",
		"targetname||The navgoal name scripts use.\n"
		"radius|0|How close, in units, an NPC must get. 0: its box must touch a 12-unit box here.",
		NULL,
		"common waypoints" },
	{ "waypoint_navgoal_1", RP_EH_MAPONLY | RP_EH_REMOVED, "Navgoal waypoints are only read by ICARUS scripts and cannot be placed by the entity commands.",
		"A named destination for ICARUS scripts, like waypoint_navgoal, reached only when the NPC's box touches a 1-unit box here (no radius). At map load it records its name and position, then removes itself.",
		"1|SOLID_OK|No in-solid error at load.",
		"targetname||The navgoal name scripts use.",
		NULL,
		"common waypoints waypoint_navgoal" },
	{ "waypoint_navgoal_2", RP_EH_MAPONLY | RP_EH_REMOVED, "Navgoal waypoints are only read by ICARUS scripts and cannot be placed by the entity commands.",
		"A named destination for ICARUS scripts, like waypoint_navgoal, reached only when the NPC's box touches a 2-unit box here (no radius). At map load it records its name and position, then removes itself.",
		"1|SOLID_OK|No in-solid error at load.",
		"targetname||The navgoal name scripts use.",
		NULL,
		"common waypoints waypoint_navgoal" },
	{ "waypoint_navgoal_4", RP_EH_MAPONLY | RP_EH_REMOVED, "Navgoal waypoints are only read by ICARUS scripts and cannot be placed by the entity commands.",
		"A named destination for ICARUS scripts, like waypoint_navgoal, reached only when the NPC's box touches a 4-unit box here (no radius). At map load it records its name and position, then removes itself.",
		"1|SOLID_OK|No in-solid error at load.",
		"targetname||The navgoal name scripts use.",
		NULL,
		"common waypoints waypoint_navgoal" },
	{ "waypoint_navgoal_8", RP_EH_MAPONLY | RP_EH_REMOVED, "Navgoal waypoints are only read by ICARUS scripts and cannot be placed by the entity commands.",
		"A named destination for ICARUS scripts, like waypoint_navgoal, reached only when the NPC's box touches an 8-unit box here (no radius). At map load it records its name and position, then removes itself.",
		"1|SOLID_OK|No in-solid error at load.",
		"targetname||The navgoal name scripts use.",
		NULL,
		"common waypoints waypoint_navgoal" },
	{ "waypoint_small", RP_EH_MAPONLY | RP_EH_REMOVED, "Waypoints cannot be added after map load: the navigation paths were computed at map start, and an NPC pathing to a later node would crash the server.",
		"A node of the NPC navigation graph for tight spots: like waypoint, but tested with a 4-unit-wide box and given a fixed radius of 2. At map load it is added to the graph and the entity is removed.",
		"1|SOLID_OK|Added even when inside solid, for a spot that is clear in game.",
		"targetname||The name other waypoints' target keys link to.\n"
		"target||A waypoint (by targetname) this one is always linked to. target2, target3 and target4 add more.",
		NULL,
		"common waypoints waypoint" },
	{ "waypoints", RP_EH_TOPIC, NULL,
		"NPC navigation: the map's waypoint and waypoint_small entities are built into a navigation graph at map load (linked automatically, plus forced links by target to target4) and NPCs path along it, point_combat marks spots NPCs pick in a fight, and waypoint_navgoal* are named destinations for ICARUS scripts. All of these are map only: the Entity System cannot add them after load. A script's navgoal can also name any entity's targetname, so an info_notnull or target_position you add can serve as a destination. path_corner is only for func_train, and bots use their own route files (botroutes/mapname.wnt), not these entities.",
		NULL,
		NULL,
		NULL,
		"waypoint waypoint_small waypoint_navgoal point_combat target_position path_corner common" },
	{ "weapon_bryar_pistol", 0, NULL,
		"Gives the old Bryar pistol (a weapon of its own, briar_pistol model, using blaster ammo), not the blaster pistol, whose "
		"item is weapon_blaster_pistol.",
		NULL,
		NULL,
		"weapon_bryar_pistol",
		"items common" },
	{ "weapon_det_pack", 0, NULL,
		"Det pack weapon with 3 det packs (or count). It comes back on the ammo time, rp_ammo_respawn_time, not g_weaponRespawn, "
		"and det packs are capped by rp_max_detpack_ammo. ammo_detpack also gives the weapon.",
		NULL,
		NULL,
		"weapon_det_pack",
		"items common ammo_detpack" },
	{ "weapon_emplaced", RP_EH_MAPONLY, "Not a pickup: the weapon of an emplaced_gun, which is the class to place.",
		"The weapon an emplaced_gun gives whoever mans it, in the item table so the gun's assets are precached. As a pickup it "
		"hands players that weapon with no gun to fire it from: place an emplaced_gun instead.",
		NULL,
		NULL,
		NULL,
		"items common emplaced_gun" },
	{ "weapon_melee", 0, NULL,
		"A placed weapon_melee spawns as a weapon_stun_baton: every player has melee already, so a melee pickup could never be "
		"taken. It looks the same, and a saved preset keeps weapon_melee and is swapped again each time it loads.",
		NULL,
		NULL,
		"weapon_melee",
		"items common" },
	{ "weapon_thermal", 0, NULL,
		"Thermal detonator weapon with 4 detonators (or count). It comes back on the ammo time, rp_ammo_respawn_time, not "
		"g_weaponRespawn, and detonators are capped by rp_max_thermal_ammo. ammo_thermal also gives the weapon.",
		NULL,
		NULL,
		"weapon_thermal count 2",
		"items common ammo_thermal" },
	{ "weapon_trip_mine", 0, NULL,
		"Trip mine weapon with 3 mines (or count). It comes back on the ammo time, rp_ammo_respawn_time, not g_weaponRespawn, and "
		"mines are capped by rp_max_tripmine_ammo. ammo_tripmine also gives the weapon.",
		NULL,
		NULL,
		"weapon_trip_mine",
		"items common ammo_tripmine" },
	{ "weapon_turretwp", RP_EH_MAPONLY, "Not a pickup: an item entry kept for the turrets' weapon type.",
		"An item entry that only exists because the turret weapon type needs one: it is not a real weapon. For a turret, place "
		"misc_turret, misc_turretG2 or misc_sentry_turret.",
		NULL,
		NULL,
		NULL,
		"items common misc_turret misc_sentry_turret" },
	{ "zyk_mini_game_joiner", 0, NULL,
		"A zone that signs players up for a mini-game: a living player in its box is run through /meleemode (Melee Battle) or /duelmode (Duel Tournament). Those are toggles, so a second pass signs the player out again: without spawnflag 64 this repeats every wait ms while they stand there, so use 64 or a long wait. The mode must be allowed (rp_allow_melee_battle, rp_allow_duel_tournament) and its arena set on this map (/meleearena, /duelarena), and logged-in players are refused.",
		"4|MELEE|Melee Battle (/meleemode).\n"
		"8|DUEL|Duel Tournament (/duelmode).\n"
		"64|USE_KEY|The player must press Use in the box: checked every 100 ms, and after a press not again for 1 second.",
		"wait|100|Ms between checks of the box (at least 100). With spawnflag 64 it is always 100.\n"
		"mins|0 0 0|Box corner relative to the origin. The default box is a single point: set one, e.g. -32 -32 -24.\n"
		"maxs|0 0 0|Opposite box corner, e.g. 32 32 40.",
		"zyk_mini_game_joiner spawnflags 68",
		"common" },
	{ "zyk_regen_unit", 0, NULL,
		"An invisible zone that restores every living player in its box by count points every wait ms, up to their maximum: health, shield and/or force, chosen by spawnflags. Nothing shows where it is and it blocks nothing.",
		"1|HEALTH|Restores health, up to max health.\n"
		"2|SHIELD|Restores shield, up to max health (a logged-in character's own shield maximum).\n"
		"4|FORCE|Restores force power, up to its maximum.\n"
		"8|MAGIC|Does nothing (the magic system is gone).",
		"count|0|Points restored each time. A negative count counts as 0.\n"
		"wait|100|Ms between restores (at least 100).\n"
		"mins|-15 -15 -24|Box corner relative to the origin (default: a player's box).\n"
		"maxs|15 15 40|Opposite box corner.",
		"zyk_regen_unit spawnflags 3 count 5 wait 1000",
		"common" },
	{ "zyk_training_pole", 0, NULL,
		"A solid saber training dummy: a model (the Rift statue by default) that can be hit but never destroyed. With spawnflag 1 it shows the damage it took as a number above it, wait ms after the last hit of a series. /spawndummy places one with spawnflag 1 on the floor in front of you, facing you (where you stand when there is no room).",
		"1|SHOW_DAMAGE|Shows the total damage of each series of hits as a number above it, to every player who can see it.",
		"model|models/map_objects/rift/statue.md3|The .md3 model shown: one the server has. The Entity System refuses any other, and a map's own shows the statue instead.\n"
		"angle|0|The direction (yaw, degrees) the model faces.\n"
		"wait|100|Ms after the last hit before the damage is shown (at least 100).\n"
		"mins|-15 -15 -24|Box corner relative to the origin.\n"
		"maxs|15 15 40|Opposite box corner.",
		"zyk_training_pole spawnflags 1 wait 1000 angle 90",
		"common" },
	{ "zyk_weather", 0, NULL,
		"The map's saved weather: adds the weather its message names to the whole map, for every player, and /entsave keeps it. /admweather is for changes on the go: it can clear the weather, layer more on top, and /admweather default brings this weather back. "
		"Once /admweather has been used on this map a new one is refused (use /admweather add instead). Refused, and removed: a message that is not one of the weather effects below, weather another weather entity already adds (it would add nothing), and weather past the limits, the map's own included: 6 weather commands, 5 particle effects (rain, snow, sand, fog, dust) and 10 winds. A map's own follows the same rules: one left out is logged. "
		"The weather stays until the map changes, even after the entity is removed, and an /entedit to it adds the new weather without taking the old away.",
		NULL,
		"message||Required: the weather, one of these. One with numbers is typed in quotes: message rain, or message, then constantwind 0 300 0 in quotes.\n"
		"rain, lightrain, heavyrain, acidrain||Rain: ordinary, a drizzle, a downpour, acid rain.\n"
		"snow, sand||Snow, or a sand storm.\n"
		"fog, heavyrainfog, light_fog||Drifting fog: misty, the heavy rain's, a light blue-grey one.\n"
		"spacedust [density]||Floating dust, density 1 to 10000 (1000 without one).\n"
		"wind, gustingwind||A light random wind, or random gusts.\n"
		"constantwind <x> <y> <z>||A steady wind of that velocity, each value -10000 to 10000.\n"
		"refused||clear (use /admweather clear), freeze (players would see different things, and nothing could switch it back), die (it deletes the map's indoor and outdoor zones), zone, outsidepain and outsideshake (they do nothing in multiplayer).",
		"zyk_weather message snow",
		"common" },
};
