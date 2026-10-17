/*
===========================================================================
GLX_U: [Lua] server-side Lua scripting -- the calls the rest of the game module makes.

Admins write plugins (lua/plugins/<name>/plugin.lua: every such folder is loaded) and map scripts
(lua/maps/<mapname>/*.lua, loaded on that map). Scripts run in a sandbox
with no file, OS, native-library or debug access, under instruction, time, memory and per-frame
limits, and reach the game only through the API in rp_lua_api.c. One Lua state lives for one map:
RP_Lua_Init() at the end of G_InitGame, RP_Lua_Shutdown() in G_ShutdownGame. Every call below
returns at once when Lua is off (rp_lua 0) or the state is not open.

The pieces: rp_lua_core.c (state, sandbox, limits, loader, protected calls, errors),
rp_lua_api.c (what scripts can call), rp_lua_world.c (entities and the World table, API version 2), rp_lua_people.c (players, and the death and damage events, API version 3; credits, XP and the level events, API version 4;
NPC handles are in rp_lua_world.c), rp_lua_dialog.c (dialogues, API version 5), rp_lua_orders.c (NPC orders, API version 6),
rp_lua_store.c (lua.db), rp_lua_admin.c (/scripts and the rcon lua_* commands). lib/lua/README.md says what was left out of Lua itself and why.
===========================================================================
*/
#ifndef RP_LUA_H
#define RP_LUA_H

void		RP_Lua_Init( void );
void		RP_Lua_Shutdown( void );
void		RP_Lua_Frame( int levelTime );

// client events: notifications only -- nothing a script does here changes the connection
void		RP_Lua_ClientConnect( int clientNum, qboolean firstTime );
void		RP_Lua_ClientBegin( int clientNum );
void		RP_Lua_ClientSpawn( int clientNum );
void		RP_Lua_ClientDisconnect( int clientNum );

// chat about to be sent: qfalse when a script stopped it. ooc is G_Say's ooc_flag.
qboolean	RP_Lua_Chat( gentity_t *ent, gentity_t *target, int mode, qboolean ooc, const char *text );

// a client command no built-in command took: qtrue when a script command ran (or refused it)
qboolean	RP_Lua_ClientCommand( gentity_t *ent, const char *cmd );

// remove_character() deleted this character: drop its script data from lua.db
void		RP_Lua_CharacterRemoved( int charID );

// GLX_U: [Lua] entities (rp_lua_world.c). G_FreeEntity() reports every free: bookkeeping only, no
// script runs. An entity about to be used: the script's on_use and the "use" event for its
// targetname run, and qfalse means one of them stopped the use -- never when forced (the admin
// commands /entuse and /enttrigger, and the cheat command /t_use, which locks do not stop either). A target_lua was
// used: its event goes to the scripts. RP_Lua_EntityOwner() names the plugin that spawned the
// entity, NULL when no script did (the Entity System commands leave those alone).
void		RP_Lua_EntityFreed( gentity_t *ent );
qboolean	RP_Lua_EntityUsed( gentity_t *self, gentity_t *other, gentity_t *activator, qboolean forced );
void		RP_Lua_ForceUses( qboolean on );	// around an admin's use: every use until off is forced
void		RP_Lua_MapEvent( const char *name, gentity_t *activator, gentity_t *ent );
const char	*RP_Lua_EntityOwner( const gentity_t *ent );
qboolean	RP_Lua_ValidEventName( const char *name );

// GLX_U: [Lua] players and NPCs (rp_lua_people.c, API version 3), from g_combat.c. A real death in player_die()
// (player or NPC; not a knockdown), a hit G_Damage() has just applied to a player or NPC, and a player the Death
// System has just knocked down. The scripts cannot change what happens next: they are told, and a free() or
// kill() they ask for waits for the next frame.
void		RP_Lua_Death( gentity_t *self, gentity_t *attacker, int mod );
void		RP_Lua_Damage( gentity_t *targ, gentity_t *attacker, int amount, int mod );
void		RP_Lua_Downed( gentity_t *ent, gentity_t *attacker );

// GLX_U: [Lua] API version 4 (rp_lua_people.c), from g_cmds.c: increase_level() gave a player levels (> 0) or
// decrease_level() took some (< 0) -- the level_up and level_down events. Read-only for the scripts, like the above.
void		RP_Lua_LevelChanged( gentity_t *ent, int levels );

// GLX_U: [Lua] dialogues (rp_lua_dialog.c, API version 5): /choose answers the player's open dialogue (it works,
// and says there is none, with Lua off), and a chat line that is just one of its numbers answers it too: qtrue when
// it did, and the line goes no further.
void		Cmd_Choose_f( gentity_t *ent );
qboolean	RP_Lua_DialogChat( gentity_t *ent, const char *text );

// GLX_U: [Lua] NPC orders (rp_lua_orders.c, API version 6): a player's Use key or /order is taking the NPC over,
// so a script's order on it ends ("replaced"); and a script NPC with an on_use is not taken as a follower by the
// Use key at all -- its on_use decides what the Use key does.
void		RP_Lua_NpcOrderTaken( gentity_t *npc );
qboolean	RP_Lua_NpcKeepsUse( const gentity_t *npc );

// /scripts (needs the Scripts admin power) and the rcon commands
void		Cmd_Scripts_f( gentity_t *ent );
void		Svcmd_LuaList_f( void );
void		Svcmd_LuaReload_f( void );
void		Svcmd_LuaEnable_f( void );
void		Svcmd_LuaDisable_f( void );
void		Svcmd_LuaErrors_f( void );
void		Svcmd_LuaStats_f( void );
void		Svcmd_LuaExec_f( void );

#endif // RP_LUA_H
