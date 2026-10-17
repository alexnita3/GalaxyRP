/*
===========================================================================
GLX_U: [Lua] API version 2: the world. Entity handles, World.spawn / zone / find / by_number, the
effect, sound, music and shake helpers, and the callbacks the game makes into scripts when an
entity is used, damaged or destroyed, or when a player enters or leaves a zone.

Handles. An entity handle (userdata GLX.Entity) holds an entity number and a generation:
g_entityGen[] (g_utils.c) changes whenever a slot is taken or freed, so a handle to an entity that
has gone -- and to a slot something else has taken since -- is a script error, never a crash. One
handle per (number, generation) is cached, so the same entity is always the same value.

Ownership. rpEnts[] holds, per slot, the plugin that spawned the entity (owner, plugin + 1) and its
callbacks. Scripts may read and use() any entity, but free, move, rotate and give callbacks only to
their own. G_FreeEntity() calls RP_Lua_EntityFreed() for every free, whoever frees it, and that only
updates this bookkeeping -- it never runs Lua. A plugin that is switched off or reloaded loses its
entities (RP_LuaWorld_RemovePlugin).

Game callbacks. on_use, on_pain, on_die, on_enter, on_leave and the use and map_event events run in
the middle of game code (GlobalUse, G_Damage, G_TouchTriggers, ClientDisconnect). Inside any of them
free() is queued and done at the start of the next server frame, so the game code that called the
script never sees its entity vanish under it (rpCallbackDepth). An admin's /entuse, /enttrigger and
/targetuse run the callbacks but cannot be stopped by them, nor can what those uses set off
(RP_Lua_ForceUses), just as locks cannot stop them.

NPCs (API version 3). World.npc spawns one through /npc spawn's own path (RP_NpcSpawnAt, NPC_spawn.c), at a point
the script gives; it is the script's own entity like any other, counted against rp_lua_maxnpcs as well, until it is
freed -- its corpse included. Any NPC's handle tells what it is; a script's own NPCs can also be killed, teleported,
frozen and given an /npc effect, and given an on_death. A kill() inside a game callback waits for the next frame as
free() does, and so does one asked for before the NPC has finished spawning (NPC_Begin). Zones made with npcs = true
see NPCs come and go as well as players.

Spawning goes through exactly the steps /entadd takes (zyk_entadd in g_cmds.c), so every Entity
System safeguard applies, plus a per-map budget for the game's memory pool, which is never given back
within a map (rp_lua_spawnkb), and a fixed list of classes scripts may not make (RP_LuaWorld_Denied).
Effects, sounds and shakes are temp entities: at most RP_LUA_EVENTS_PER_FRAME a frame, and never
into the reserve of free slots ordinary play depends on.
===========================================================================
*/

#include "rp_lua_local.h"

#define RP_LUA_ENTITY_MT			"GLX.Entity"
#define RP_LUA_FIND_MAX				64
#define RP_LUA_MAX_ZONES			256
#define RP_LUA_ZONE_MAX_SIZE		8192.0f
#define RP_LUA_ZONE_LEAVE_MS		500		// a player not touching a zone this long has left it
#define RP_LUA_ZONE_NPCS			32		// NPCs one zone made with npcs = true follows at once
#define RP_LUA_EVENTS_PER_FRAME		64		// effects, sounds and shakes, all scripts together
#define RP_LUA_MOVE_MAX_MS			600000
#define RP_LUA_COORD_MAX			262144.0f
#define RP_LUA_KEY_MAX				64
#define RP_LUA_VALUE_MAX			1024
// the /shakescreen bounds (RP_SHAKE_* in g_cmds.c)
#define RP_LUA_SHAKE_MAX_RADIUS		1500
#define RP_LUA_SHAKE_MIN_INTENSITY	1
#define RP_LUA_SHAKE_MAX_INTENSITY	10
#define RP_LUA_SHAKE_MIN_MS			1000
#define RP_LUA_SHAKE_MAX_MS			5000

// g_spawn.c, g_mover.c, g_ICARUScb.c
extern void zyk_main_set_entity_field( gentity_t *ent, char *key, char *value );
extern void zyk_main_spawn_entity( gentity_t *ent );
extern qboolean RP_SpawnRouteIsLogical( const rpSpawnRoute_t *route );
extern void InitMoverTrData( gentity_t *ent );
extern void MatchTeam( gentity_t *teamLeader, int moverState, int time );
extern void Blocked_Mover( gentity_t *ent, gentity_t *other );
extern void GlobalUse( gentity_t *self, gentity_t *other, gentity_t *activator );
extern void NPC_Begin( gentity_t *ent );

typedef void ( *rpPainFn_t )( gentity_t *self, gentity_t *attacker, int damage );
typedef void ( *rpDieFn_t )( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod );
typedef void ( *rpThinkFn_t )( gentity_t *self );

typedef struct {
	int			owner;			// plugin + 1 that spawned it, 0: not a script's
	int			gen;			// g_entityGen[] when it was spawned
	int			useRef, painRef, dieRef, enterRef, leaveRef, deathRef;	// 0: none
	qboolean	addedUsable;	// on_use made it usable (SVF_PLAYER_USABLE)
	qboolean	addedUseStub;	// on_use gave it a use function it did not have
	rpPainFn_t	origPain;		// the class's own, while ent->pain is RP_LuaWorld_Pain
	rpDieFn_t	origDie;
	rpThinkFn_t	origThink;		// saved by rotate() while it turns
	int			origNextthink;
	qboolean	thinkSaved;
	int			zone;			// rpZones index + 1, 0: not a zone
	qboolean	pendingFree;	// free() inside a game callback: done next frame
	qboolean	isNpc;			// World.npc made it: counted in numNpcs too
	qboolean	pendingKill;	// kill() inside a game callback, or before NPC_Begin: done in a later frame
	int			handleRef;		// cached handle (any entity), 0: none
	int			handleGen;
} rpLuaEnt_t;

typedef struct {
	qboolean	active;
	int			num;
	qboolean	inside[MAX_CLIENTS];
	int			lastTouch[MAX_CLIENTS];
	qboolean	npcs;							// World.zone{ npcs = true }: NPCs too
	int			npcNum[RP_LUA_ZONE_NPCS];		// 0: a free place (NPCs are never below MAX_CLIENTS)
	int			npcGen[RP_LUA_ZONE_NPCS];
	int			npcTouch[RP_LUA_ZONE_NPCS];
} rpLuaZone_t;

typedef struct {
	int		num;
	int		gen;
} rpLuaEntHandle_t;

static rpLuaEnt_t	rpEnts[MAX_ENTITIESTOTAL];
static rpLuaZone_t	rpZones[RP_LUA_MAX_ZONES];
static int			rpCallbackDepth;
static int			rpPendingFrees;
static int			rpPendingKills;
static int			rpEvTime = -1, rpEvCount;
static int			rpForceUses;		// RP_Lua_ForceUses: an admin's use and what it sets off

static void RP_LuaWorld_Unref( int *ref ) {
	if ( *ref > 0 && rl.L ) {
		luaL_unref( rl.L, LUA_REGISTRYINDEX, *ref );
	}
	*ref = 0;
}

static int RP_LuaWorld_Clamp( int v, int lo, int hi ) {
	return v < lo ? lo : ( v > hi ? hi : v );
}

/*
================
Handles
================
*/
static qboolean RP_LuaWorld_Alive( int num, int gen ) {
	return ( num >= 0 && num < MAX_ENTITIESTOTAL && g_entities[num].inuse && g_entityGen[num] == gen ) ? qtrue : qfalse;
}

static int RP_LuaWorld_Owner( int num ) {
	rpLuaEnt_t *e = &rpEnts[num];
	return ( e->owner && e->gen == g_entityGen[num] && g_entities[num].inuse ) ? e->owner - 1 : -1;
}

void RP_LuaWorld_PushEntity( lua_State *L, gentity_t *ent ) {
	rpLuaEntHandle_t *h;
	rpLuaEnt_t *e;
	int num;

	if ( !ent || !ent->inuse ) {
		lua_pushnil( L );
		return;
	}
	num = (int)( ent - g_entities );
	if ( num < 0 || num >= MAX_ENTITIESTOTAL ) {
		lua_pushnil( L );
		return;
	}
	e = &rpEnts[num];
	if ( e->handleRef > 0 ) {
		if ( e->handleGen == g_entityGen[num] ) {
			lua_rawgeti( L, LUA_REGISTRYINDEX, e->handleRef );
			return;
		}
		RP_LuaWorld_Unref( &e->handleRef );
	}
	h = (rpLuaEntHandle_t *)lua_newuserdatauv( L, sizeof( *h ), 0 );
	h->num = num;
	h->gen = g_entityGen[num];
	luaL_setmetatable( L, RP_LUA_ENTITY_MT );
	lua_pushvalue( L, -1 );
	e->handleRef = luaL_ref( L, LUA_REGISTRYINDEX );
	e->handleGen = h->gen;
}

// a player as a player handle, anything else as an entity handle, nothing as nil
void RP_LuaWorld_PushActor( lua_State *L, gentity_t *ent ) {
	int num;

	if ( !ent || !ent->inuse ) {
		lua_pushnil( L );
		return;
	}
	num = (int)( ent - g_entities );
	if ( num >= 0 && num < MAX_CLIENTS ) {
		RP_LuaApi_PushPlayer( L, num );
		return;
	}
	RP_LuaWorld_PushEntity( L, ent );
}

static gentity_t *RP_LuaWorld_CheckEntity( lua_State *L, int idx ) {
	rpLuaEntHandle_t *h = (rpLuaEntHandle_t *)luaL_checkudata( L, idx, RP_LUA_ENTITY_MT );
	if ( !RP_LuaWorld_Alive( h->num, h->gen ) ) {
		luaL_error( L, "that entity no longer exists" );
		return NULL;
	}
	return &g_entities[h->num];
}

// the entity, which must be the running plugin's own
static gentity_t *RP_LuaWorld_CheckOwn( lua_State *L, int idx, const char *what ) {
	gentity_t *ent = RP_LuaWorld_CheckEntity( L, idx );
	int plugin = RP_Lua_CurrentPlugin( L );

	if ( RP_LuaWorld_Owner( ent->s.number ) != plugin ) {
		luaL_error( L, "entity %d (%s) is not this plugin's: scripts can only %s their own entities",
			ent->s.number, ent->classname ? ent->classname : "noclass", what );
		return NULL;
	}
	return ent;
}

// a player or entity handle as the entity it stands for; nil gives NULL
static gentity_t *RP_LuaWorld_OptActor( lua_State *L, int idx ) {
	if ( lua_isnoneornil( L, idx ) ) {
		return NULL;
	}
	if ( luaL_testudata( L, idx, RP_LUA_ENTITY_MT ) ) {
		return RP_LuaWorld_CheckEntity( L, idx );
	}
	return &g_entities[RP_LuaApi_CheckPlayer( L, idx )];
}

/*
================
Vectors: { x =, y =, z = } or { 1, 2, 3 }
================
*/
static qboolean RP_LuaWorld_Number( lua_State *L, int idx, float *out ) {
	lua_Number n;
	int isnum;

	n = lua_tonumberx( L, idx, &isnum );
	if ( !isnum || n != n || n > RP_LUA_COORD_MAX || n < -RP_LUA_COORD_MAX ) {
		return qfalse;
	}
	*out = (float)n;
	return qtrue;
}

void RP_LuaWorld_CheckVec( lua_State *L, int idx, vec3_t out, const char *what ) {
	static const char *names[3] = { "x", "y", "z" };
	int i;

	idx = lua_absindex( L, idx );
	if ( !lua_istable( L, idx ) ) {
		luaL_error( L, "%s must be a vector: { x = , y = , z = }", what );
		return;
	}
	for ( i = 0; i < 3; i++ ) {
		qboolean ok;
		if ( lua_getfield( L, idx, names[i] ) == LUA_TNIL ) {
			lua_pop( L, 1 );
			lua_rawgeti( L, idx, i + 1 );
		}
		ok = RP_LuaWorld_Number( L, -1, &out[i] );
		lua_pop( L, 1 );
		if ( !ok ) {
			luaL_error( L, "%s must be a vector of three numbers between -%d and %d", what, (int)RP_LUA_COORD_MAX, (int)RP_LUA_COORD_MAX );
			return;
		}
	}
}

void RP_LuaWorld_PushVec( lua_State *L, const vec3_t v ) {
	lua_createtable( L, 0, 3 );
	lua_pushnumber( L, v[0] );
	lua_setfield( L, -2, "x" );
	lua_pushnumber( L, v[1] );
	lua_setfield( L, -2, "y" );
	lua_pushnumber( L, v[2] );
	lua_setfield( L, -2, "z" );
}

// where an entity is: a brush entity with no origin brush sits at 0 0 0, so its box's middle
static void RP_LuaWorld_Center( const gentity_t *ent, vec3_t out ) {
	if ( ent->r.bmodel && ent->r.linked ) {
		VectorAdd( ent->r.absmin, ent->r.absmax, out );
		VectorScale( out, 0.5f, out );
	} else {
		VectorCopy( ent->r.currentOrigin, out );
	}
}

/*
================
Game callbacks
================
*/
typedef enum { RPCB_USE, RPCB_PAIN, RPCB_DIE, RPCB_ENTER, RPCB_LEAVE, RPCB_DEATH } rpLuaCbKind_t;

typedef struct {
	rpLuaCbKind_t	kind;
	int				ref;
	gentity_t		*self;
	gentity_t		*a;			// activator / attacker
	gentity_t		*b;			// other
	int				n;			// damage, or the client entering or leaving (-1: the NPC in a, NULL once freed)
	int				result;		// 0 when the function returned false
} rpLuaCb_t;

static int RP_LuaWorld_CbWorker( lua_State *L, void *data ) {
	rpLuaCb_t *c = (rpLuaCb_t *)data;
	int nargs;

	lua_rawgeti( L, LUA_REGISTRYINDEX, c->ref );
	RP_LuaWorld_PushEntity( L, c->self );
	switch ( c->kind ) {
	case RPCB_USE:
		RP_LuaWorld_PushActor( L, c->a );
		RP_LuaWorld_PushActor( L, c->b );
		nargs = 3;
		break;
	case RPCB_PAIN:
		RP_LuaWorld_PushActor( L, c->a );
		lua_pushinteger( L, c->n );
		nargs = 3;
		break;
	case RPCB_DIE:
	case RPCB_DEATH:
		RP_LuaWorld_PushActor( L, c->a );
		nargs = 2;
		break;
	default:
		if ( c->n >= 0 ) {
			RP_LuaApi_PushPlayer( L, c->n );
		} else {
			RP_LuaWorld_PushActor( L, c->a );	// an NPC entering or leaving; nil once it is gone
		}
		nargs = 2;
		break;
	}
	lua_call( L, nargs, 1 );
	if ( lua_isboolean( L, -1 ) && !lua_toboolean( L, -1 ) ) {
		c->result = 0;
	}
	return 0;
}

// runs the callback ref of the script that owns ent; 0 when it returned false
static int RP_LuaWorld_CallOwn( gentity_t *ent, int ref, rpLuaCbKind_t kind, gentity_t *a, gentity_t *b, int n ) {
	rpLuaCb_t c;
	int plugin;

	if ( !rl.L || ref <= 0 ) {
		return 1;
	}
	plugin = RP_LuaWorld_Owner( ent->s.number );
	if ( !RP_Lua_PluginRunning( plugin ) ) {
		return 1;
	}
	c.kind = kind;
	c.ref = ref;
	c.self = ent;
	c.a = a;
	c.b = b;
	c.n = n;
	c.result = 1;
	rpCallbackDepth++;		// every script callback runs inside game code: free() waits for the next frame
	RP_Lua_Call( plugin, RP_LuaWorld_CbWorker, &c );
	rpCallbackDepth--;
	return c.result;
}

void RP_LuaWorld_EnterCallback( void ) {
	rpCallbackDepth++;
}

void RP_LuaWorld_LeaveCallback( void ) {
	if ( rpCallbackDepth > 0 ) {
		rpCallbackDepth--;
	}
}

// an NPC is dying (player_die, through RP_Lua_Death): the script that owns it hears it in its on_death
void RP_LuaWorld_NpcDied( gentity_t *npc, gentity_t *killer ) {
	int num;

	if ( !rl.L || !npc ) {
		return;
	}
	num = npc->s.number;
	if ( num < 0 || num >= MAX_ENTITIESTOTAL || RP_LuaWorld_Owner( num ) < 0 || rpEnts[num].deathRef <= 0 ) {
		return;
	}
	RP_LuaWorld_CallOwn( npc, rpEnts[num].deathRef, RPCB_DEATH, killer, NULL, 0 );
}

void RP_Lua_ForceUses( qboolean on ) {
	rpForceUses += on ? 1 : -1;
	if ( rpForceUses < 0 ) {
		rpForceUses = 0;
	}
}

qboolean RP_Lua_EntityUsed( gentity_t *self, gentity_t *other, gentity_t *activator, qboolean forced ) {
	qboolean allowed = qtrue;
	int num;

	if ( rpForceUses > 0 ) {
		forced = qtrue;
	}
	if ( !rl.L || !self || !self->inuse ) {
		return qtrue;
	}
	num = (int)( self - g_entities );
	if ( num < 0 || num >= MAX_ENTITIESTOTAL ) {
		return qtrue;
	}
	if ( RP_LuaWorld_Owner( num ) >= 0 && rpEnts[num].useRef > 0 ) {
		if ( !RP_LuaWorld_CallOwn( self, rpEnts[num].useRef, RPCB_USE, activator, other, 0 ) ) {
			allowed = qfalse;
		}
	}
	if ( ( allowed || forced ) && self->inuse && self->targetname && self->targetname[0] ) {
		rpCallbackDepth++;
		if ( !RP_LuaApi_FireUse( self, other, activator, forced ) ) {
			allowed = qfalse;
		}
		rpCallbackDepth--;
	}
	return ( forced || allowed ) ? qtrue : qfalse;
}

// a target_lua's event, and the name a map_event handler filters on: [a-z0-9_]{1,32}. Works with
// Lua off too: a target_lua checks its key when it spawns.
qboolean RP_Lua_ValidEventName( const char *name ) {
	int i;

	if ( !name ) {
		return qfalse;
	}
	for ( i = 0; name[i]; i++ ) {
		char c = name[i];
		if ( i >= RP_LUA_EVENT_NAME_MAX || !( ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == '_' ) ) {
			return qfalse;
		}
	}
	return ( i > 0 ) ? qtrue : qfalse;
}

void RP_Lua_MapEvent( const char *name, gentity_t *activator, gentity_t *ent ) {
	if ( !rl.L || !name || !name[0] ) {
		return;
	}
	rpCallbackDepth++;
	RP_LuaApi_FireMapEvent( name, activator, ent );
	rpCallbackDepth--;
}

static void RP_LuaWorld_Pain( gentity_t *self, gentity_t *attacker, int damage ) {
	int num = self->s.number, gen = g_entityGen[num];
	rpPainFn_t orig = rpEnts[num].origPain;

	if ( RP_LuaWorld_Owner( num ) >= 0 && rpEnts[num].painRef > 0 ) {
		RP_LuaWorld_CallOwn( self, rpEnts[num].painRef, RPCB_PAIN, attacker, NULL, damage );
	}
	if ( orig && self->inuse && g_entityGen[num] == gen ) {
		orig( self, attacker, damage );
	}
}

static void RP_LuaWorld_Die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	int num = self->s.number, gen = g_entityGen[num];
	rpDieFn_t orig = rpEnts[num].origDie;

	if ( RP_LuaWorld_Owner( num ) >= 0 && rpEnts[num].dieRef > 0 ) {
		RP_LuaWorld_CallOwn( self, rpEnts[num].dieRef, RPCB_DIE, attacker, NULL, damage );
	}
	if ( orig && self->inuse && g_entityGen[num] == gen ) {
		orig( self, inflictor, attacker, damage, mod );
	}
}

// what on_use gives an entity that had no use function of its own
static void RP_LuaWorld_UseStub( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	(void)self;
	(void)other;
	(void)activator;
}

/*
================
Freeing
================
*/
static void RP_LuaWorld_DoFree( gentity_t *ent ) {
	if ( !ent->inuse ) {
		return;
	}
	RP_FreeEntityChildren( ent );	// the trigger a door or platform made, a turret's top...
	G_FreeEntity( ent );
}

// every free goes through here (G_FreeEntity), whoever frees it: bookkeeping only, never Lua code
void RP_Lua_EntityFreed( gentity_t *ent ) {
	rpLuaEnt_t *e;
	int num;

	if ( !ent ) {
		return;
	}
	num = (int)( ent - g_entities );
	if ( num < 0 || num >= MAX_ENTITIESTOTAL ) {
		return;
	}
	e = &rpEnts[num];
	if ( e->owner ) {
		rpLuaPlugin_t *p = &rl.plugins[e->owner - 1];
		if ( p->numEnts > 0 ) {
			p->numEnts--;
		}
		if ( e->isNpc && p->numNpcs > 0 ) {
			p->numNpcs--;
		}
	}
	if ( e->zone ) {
		rpZones[e->zone - 1].active = qfalse;
	}
	if ( e->pendingFree && rpPendingFrees > 0 ) {
		rpPendingFrees--;
	}
	if ( e->pendingKill && rpPendingKills > 0 ) {
		rpPendingKills--;
	}
	RP_LuaWorld_Unref( &e->useRef );
	RP_LuaWorld_Unref( &e->painRef );
	RP_LuaWorld_Unref( &e->dieRef );
	RP_LuaWorld_Unref( &e->enterRef );
	RP_LuaWorld_Unref( &e->leaveRef );
	RP_LuaWorld_Unref( &e->deathRef );
	RP_LuaWorld_Unref( &e->handleRef );
	memset( e, 0, sizeof( *e ) );
}

const char *RP_Lua_EntityOwner( const gentity_t *ent ) {
	int num, plugin;

	if ( !ent ) {
		return NULL;
	}
	num = (int)( ent - g_entities );
	if ( num < 0 || num >= MAX_ENTITIESTOTAL ) {
		return NULL;
	}
	plugin = RP_LuaWorld_Owner( num );
	return ( plugin >= 0 && rl.plugins[plugin].inUse ) ? rl.plugins[plugin].name : NULL;
}

// a plugin is switched off or reloaded: its entities go -- at once, or next frame when this
// happens inside a game callback (a plugin switched off by an error in its on_die, say)
void RP_LuaWorld_RemovePlugin( int plugin ) {
	int num;

	for ( num = 0; num < MAX_ENTITIESTOTAL; num++ ) {
		rpLuaEnt_t *e = &rpEnts[num];
		if ( e->owner != plugin + 1 ) {
			continue;
		}
		if ( !g_entities[num].inuse || e->gen != g_entityGen[num] ) {
			e->owner = 0;
			continue;
		}
		RP_LuaWorld_Unref( &e->useRef );
		RP_LuaWorld_Unref( &e->painRef );
		RP_LuaWorld_Unref( &e->dieRef );
		RP_LuaWorld_Unref( &e->enterRef );
		RP_LuaWorld_Unref( &e->leaveRef );
		RP_LuaWorld_Unref( &e->deathRef );
		if ( e->pendingKill ) {
			e->pendingKill = qfalse;	// it goes instead
			if ( rpPendingKills > 0 ) {
				rpPendingKills--;
			}
		}
		e->isNpc = qfalse;
		if ( rpCallbackDepth > 0 ) {
			e->owner = 0;
			if ( !e->pendingFree ) {
				e->pendingFree = qtrue;
				rpPendingFrees++;
			}
		} else {
			RP_LuaWorld_DoFree( &g_entities[num] );
		}
	}
	rl.plugins[plugin].numEnts = 0;
	rl.plugins[plugin].numNpcs = 0;
}

static void RP_LuaWorld_RunPendingFrees( void ) {
	int num;

	if ( rpPendingFrees <= 0 || rpCallbackDepth > 0 ) {
		return;
	}
	for ( num = 0; num < MAX_ENTITIESTOTAL && rpPendingFrees > 0; num++ ) {
		if ( rpEnts[num].pendingFree ) {
			if ( g_entities[num].inuse && rpEnts[num].gen == g_entityGen[num] ) {
				RP_LuaWorld_DoFree( &g_entities[num] );
			} else {
				rpEnts[num].pendingFree = qfalse;
				rpPendingFrees--;
			}
		}
	}
	rpPendingFrees = 0;
}

// an NPC killed the way /npc kill kills one: its death and corpse, as if hit for its full health
static void RP_LuaWorld_DoKill( gentity_t *npc ) {
	npc->health = 0;
	npc->client->ps.stats[STAT_HEALTH] = 0;
	if ( npc->die ) {
		npc->die( npc, npc, npc, npc->client->pers.maxHealth, MOD_UNKNOWN );
	}
}

// alive as /entsave counts an NPC: not dead, not a vehicle counting down to its explosion; one that has not run
// NPC_Begin yet has no health but is alive
static qboolean RP_LuaWorld_NpcAlive( const gentity_t *npc ) {
	if ( !npc->inuse || !npc->client || !npc->NPC || ( npc->s.eFlags & EF_DEAD ) || npc->client->ps.pm_type == PM_DEAD
		|| ( npc->m_pVehicle && npc->m_pVehicle->m_iDieTime != 0 ) ) {
		return qfalse;
	}
	return ( npc->health > 0 || npc->think == NPC_Begin ) ? qtrue : qfalse;
}

// a kill's die() runs scripts (npc_death, on_death), which may ask for more kills: the count is taken again after
static void RP_LuaWorld_RunPendingKills( void ) {
	int num;

	if ( rpPendingKills <= 0 || rpCallbackDepth > 0 ) {
		return;
	}
	for ( num = MAX_CLIENTS; num < MAX_ENTITIESTOTAL; num++ ) {
		rpLuaEnt_t *e = &rpEnts[num];
		gentity_t *npc = &g_entities[num];
		if ( !e->pendingKill ) {
			continue;
		}
		if ( !npc->inuse || e->gen != g_entityGen[num] || e->pendingFree || !RP_LuaWorld_NpcAlive( npc ) ) {
			e->pendingKill = qfalse;	// gone, going, or dead already
			continue;
		}
		if ( npc->think == NPC_Begin ) {
			continue;					// not spawned yet: a later frame
		}
		e->pendingKill = qfalse;
		RP_LuaWorld_DoKill( npc );
	}
	rpPendingKills = 0;
	for ( num = MAX_CLIENTS; num < MAX_ENTITIESTOTAL; num++ ) {
		if ( rpEnts[num].pendingKill ) {
			rpPendingKills++;
		}
	}
}

// /scripts reload all: every script entity goes before the state does
void RP_LuaWorld_FreeAll( void ) {
	int num;

	for ( num = 0; num < MAX_ENTITIESTOTAL; num++ ) {
		rpLuaEnt_t *e = &rpEnts[num];
		if ( ( e->owner || e->pendingFree ) && g_entities[num].inuse && e->gen == g_entityGen[num] ) {
			RP_LuaWorld_DoFree( &g_entities[num] );
		}
	}
}

// the state is closing: forget every reference (the entities themselves are the map's business)
void RP_LuaWorld_Reset( void ) {
	int num;

	for ( num = 0; num < MAX_ENTITIESTOTAL; num++ ) {
		rpLuaEnt_t *e = &rpEnts[num];
		// a script entity still alive keeps working without its script: its wrapped pain and die call
		// the class's own, which stay here until the entity is freed
		rpPainFn_t pain = e->origPain;
		rpDieFn_t die = e->origDie;
		memset( e, 0, sizeof( *e ) );
		e->origPain = pain;
		e->origDie = die;
	}
	memset( rpZones, 0, sizeof( rpZones ) );
	rpCallbackDepth = 0;
	rpPendingFrees = 0;
	rpPendingKills = 0;
	rpForceUses = 0;
}

/*
================
Zones
================
*/
static void RP_LuaWorld_ZoneLeave( int z, int client ) {
	rpLuaZone_t *zone = &rpZones[z];

	zone->inside[client] = qfalse;
	if ( RP_LuaWorld_Owner( zone->num ) >= 0 && rpEnts[zone->num].leaveRef > 0 ) {
		RP_LuaWorld_CallOwn( &g_entities[zone->num], rpEnts[zone->num].leaveRef, RPCB_LEAVE, NULL, NULL, client );
	}
}

// an NPC leaves zone z (place i): the handle, or nil when it has been freed
static void RP_LuaWorld_ZoneLeaveNpc( int z, int i ) {
	rpLuaZone_t *zone = &rpZones[z];
	int num = zone->npcNum[i];
	gentity_t *npc = ( g_entities[num].inuse && g_entityGen[num] == zone->npcGen[i] ) ? &g_entities[num] : NULL;

	zone->npcNum[i] = 0;
	if ( RP_LuaWorld_Owner( zone->num ) >= 0 && rpEnts[zone->num].leaveRef > 0 ) {
		RP_LuaWorld_CallOwn( &g_entities[zone->num], rpEnts[zone->num].leaveRef, RPCB_LEAVE, npc, NULL, -1 );
	}
}

// an NPC touching a zone made with npcs = true. Up to RP_LUA_ZONE_NPCS at once; more are not seen until one leaves.
static void RP_LuaWorld_ZoneTouchNpc( gentity_t *self, int z, gentity_t *npc ) {
	rpLuaZone_t *zone = &rpZones[z];
	int i, num = npc->s.number, gen = g_entityGen[num], freeAt = -1;

	if ( !RP_LuaWorld_NpcAlive( npc ) ) {
		return;
	}
	for ( i = 0; i < RP_LUA_ZONE_NPCS; i++ ) {
		if ( zone->npcNum[i] == num && zone->npcGen[i] == gen ) {
			zone->npcTouch[i] = level.time;
			return;
		}
		if ( !zone->npcNum[i] && freeAt < 0 ) {
			freeAt = i;
		}
	}
	if ( freeAt < 0 ) {
		return;
	}
	zone->npcNum[freeAt] = num;
	zone->npcGen[freeAt] = gen;
	zone->npcTouch[freeAt] = level.time;
	if ( RP_LuaWorld_Owner( self->s.number ) >= 0 && rpEnts[self->s.number].enterRef > 0 ) {
		RP_LuaWorld_CallOwn( self, rpEnts[self->s.number].enterRef, RPCB_ENTER, npc, NULL, -1 );
	}
}

static void RP_LuaWorld_ZoneTouch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	rpLuaZone_t *zone;
	int z, c;

	(void)trace;
	if ( !rl.L || !other ) {
		return;
	}
	z = rpEnts[self->s.number].zone - 1;
	c = other->s.number;
	if ( z >= 0 && rpZones[z].active && rpZones[z].npcs && c >= MAX_CLIENTS && other->NPC && other->client ) {
		RP_LuaWorld_ZoneTouchNpc( self, z, other );
		return;
	}
	if ( z < 0 || !rpZones[z].active || c < 0 || c >= MAX_CLIENTS || !other->client
		|| other->client->pers.connected != CON_CONNECTED || other->client->sess.sessionTeam == TEAM_SPECTATOR || other->health <= 0 ) {
		return;
	}
	zone = &rpZones[z];
	zone->lastTouch[c] = level.time;
	if ( !zone->inside[c] ) {
		zone->inside[c] = qtrue;
		if ( RP_LuaWorld_Owner( self->s.number ) >= 0 && rpEnts[self->s.number].enterRef > 0 ) {
			RP_LuaWorld_CallOwn( self, rpEnts[self->s.number].enterRef, RPCB_ENTER, NULL, NULL, c );
		}
	}
}

static void RP_LuaWorld_ZonesFrame( void ) {
	int z, c;

	for ( z = 0; z < RP_LUA_MAX_ZONES; z++ ) {
		rpLuaZone_t *zone = &rpZones[z];
		if ( !zone->active ) {
			continue;
		}
		for ( c = 0; c < MAX_CLIENTS; c++ ) {
			gentity_t *pl;
			if ( !zone->inside[c] ) {
				continue;
			}
			pl = &g_entities[c];
			if ( !pl->inuse || !pl->client || pl->client->pers.connected != CON_CONNECTED
				|| pl->client->sess.sessionTeam == TEAM_SPECTATOR || pl->health <= 0
				|| level.time - zone->lastTouch[c] > RP_LUA_ZONE_LEAVE_MS ) {
				if ( !RP_Lua_FrameBudgetLeft() ) {
					RP_Lua_NoteDeferred( RP_LuaWorld_Owner( zone->num ), 1 );
					return;
				}
				RP_LuaWorld_ZoneLeave( z, c );
				if ( !zone->active ) {
					break;
				}
			}
		}
		for ( c = 0; zone->active && zone->npcs && c < RP_LUA_ZONE_NPCS; c++ ) {
			int num = zone->npcNum[c];
			if ( !num ) {
				continue;
			}
			if ( !g_entities[num].inuse || g_entityGen[num] != zone->npcGen[c] || !RP_LuaWorld_NpcAlive( &g_entities[num] )
				|| level.time - zone->npcTouch[c] > RP_LUA_ZONE_LEAVE_MS ) {
				if ( !RP_Lua_FrameBudgetLeft() ) {
					RP_Lua_NoteDeferred( RP_LuaWorld_Owner( zone->num ), 1 );
					return;
				}
				RP_LuaWorld_ZoneLeaveNpc( z, c );
			}
		}
	}
}

// a player is leaving the server: every zone they are in sees them leave, while the handle works
void RP_LuaWorld_ClientGone( int client ) {
	int z;

	if ( client < 0 || client >= MAX_CLIENTS ) {
		return;
	}
	for ( z = 0; z < RP_LUA_MAX_ZONES; z++ ) {
		if ( rpZones[z].active && rpZones[z].inside[client] ) {
			RP_LuaWorld_ZoneLeave( z, client );
		}
	}
}

void RP_LuaWorld_Frame( void ) {
	RP_LuaWorld_RunPendingKills();		// before the frees: a killed NPC that is also freed just goes
	RP_LuaWorld_RunPendingFrees();
	RP_LuaWorld_ZonesFrame();
}

/*
================
World.spawn
================
*/
// classes scripts may not make: the map itself, NPCs and spawners (World.npc spawns NPCs), player spawn points, sub-BSPs,
// weather, and target_lua (a script raises its own events directly)
static const char *RP_LuaWorld_Denied( const char *classname ) {
	static const char *exact[] = {
		"worldspawn", "misc_bsp", "info_jedimaster_start", "fx_rain", "fx_snow", "fx_spacedust", "fx_wind",
		"zyk_weather", "misc_weather_zone", "target_lua", NULL
	};
	int i;

	if ( RP_IsNpcSpawnerClass( classname ) ) {
		return "NPCs and NPC or vehicle spawners (World.npc spawns an NPC or a vehicle)";
	}
	if ( !Q_stricmpn( classname, "info_player_", 12 ) || !Q_stricmpn( classname, "team_CTF_", 9 ) ) {
		return "player spawn points";
	}
	for ( i = 0; exact[i]; i++ ) {
		if ( !Q_stricmp( classname, exact[i] ) ) {
			return classname;
		}
	}
	return NULL;
}

// a class the game can spawn, found as G_CallSpawn() finds it (items by exact name, the spawn table
// in any case): an unknown one is a script error before a slot or any pool memory is taken
static qboolean RP_LuaWorld_KnownClass( const char *classname ) {
	gitem_t *item;
	int i, n = RP_SpawnClassCount();

	for ( item = bg_itemlist + 1; item->classname; item++ ) {
		if ( !strcmp( item->classname, classname ) ) {
			return qtrue;
		}
	}
	for ( i = 0; i < n; i++ ) {
		const char *name = RP_SpawnClassName( i, NULL );
		if ( name && !Q_stricmp( name, classname ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

static qboolean RP_LuaWorld_ValidName( const char *s, int maxLen ) {
	int i;
	for ( i = 0; s[i]; i++ ) {
		char c = s[i];
		if ( i >= maxLen || !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' ) ) {
			return qfalse;
		}
	}
	return ( i > 0 ) ? qtrue : qfalse;
}

// what G_Alloc() takes for one string: rounded as it rounds, twice (the record and the spawn), as
// /entadd counts it (zyk_args_bytes in g_cmds.c)
static int RP_LuaWorld_StrBytes( const char *s ) {
	return ( ( (int)strlen( s ) + 1 + 31 ) & ~31 ) * 2;
}

static int RP_LuaWorld_Refused( lua_State *L, const char *reason ) {
	lua_pushnil( L );
	lua_pushstring( L, reason );
	return 2;
}

static char rpSpawnKeys[MAX_SPAWN_VARS][RP_LUA_KEY_MAX];
static char rpSpawnValues[MAX_SPAWN_VARS][RP_LUA_VALUE_MAX];

static int RP_LuaWorld_Spawn( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	rpLuaPlugin_t *p = &rl.plugins[plugin];
	const char *cls = luaL_checkstring( L, 1 );
	char classname[RP_LUA_KEY_MAX];
	const char *denied;
	rpSpawnRoute_t route;
	gentity_t *ent;
	int pairs = 0, bytes, limit, before, used, i, num;

	if ( !RP_LuaWorld_ValidName( cls, RP_LUA_KEY_MAX - 1 ) ) {
		return luaL_error( L, "World.spawn: \"%s\" is not a class name", cls );
	}
	Q_strncpyz( classname, cls, sizeof( classname ) );
	denied = RP_LuaWorld_Denied( classname );
	if ( denied ) {
		return luaL_error( L, "World.spawn: scripts cannot make %s", denied );
	}
	if ( !RP_LuaWorld_KnownClass( classname ) ) {
		return luaL_error( L, "World.spawn: there is no entity class called %s (see /enthelp)", classname );
	}

	// the keys, checked before anything is taken
	if ( !lua_isnoneornil( L, 2 ) ) {
		luaL_checktype( L, 2, LUA_TTABLE );
		lua_pushnil( L );
		while ( lua_next( L, 2 ) ) {
			const char *key;
			if ( lua_type( L, -2 ) != LUA_TSTRING ) {
				return luaL_error( L, "World.spawn: key names must be strings" );
			}
			key = lua_tostring( L, -2 );
			if ( !RP_LuaWorld_ValidName( key, RP_LUA_KEY_MAX - 1 ) ) {
				return luaL_error( L, "World.spawn: \"%s\" is not a key name", key );
			}
			if ( !Q_stricmp( key, "classname" ) || !Q_stricmp( key, "aimoffset" ) ) {
				return luaL_error( L, "World.spawn: the class is the first argument, not a \"%s\" key", key );
			}
			if ( pairs >= MAX_SPAWN_VARS - 1 ) {
				return luaL_error( L, "World.spawn: at most %d keys", MAX_SPAWN_VARS - 1 );
			}
			Q_strncpyz( rpSpawnKeys[pairs], key, RP_LUA_KEY_MAX );
			switch ( lua_type( L, -1 ) ) {
			case LUA_TSTRING: {
				size_t len;
				const char *v = lua_tolstring( L, -1, &len );
				if ( len >= RP_LUA_VALUE_MAX || strlen( v ) != len ) {
					return luaL_error( L, "World.spawn: the value of \"%s\" is too long (at most %d characters)", key, RP_LUA_VALUE_MAX - 1 );
				}
				Q_strncpyz( rpSpawnValues[pairs], v, RP_LUA_VALUE_MAX );
				break;
			}
			case LUA_TNUMBER:
				if ( lua_isinteger( L, -1 ) ) {
					Com_sprintf( rpSpawnValues[pairs], RP_LUA_VALUE_MAX, "%lld", (long long)lua_tointeger( L, -1 ) );
				} else {
					Com_sprintf( rpSpawnValues[pairs], RP_LUA_VALUE_MAX, "%g", (double)lua_tonumber( L, -1 ) );
				}
				break;
			case LUA_TBOOLEAN:
				Q_strncpyz( rpSpawnValues[pairs], lua_toboolean( L, -1 ) ? "1" : "0", RP_LUA_VALUE_MAX );
				break;
			case LUA_TTABLE: {
				vec3_t v;
				RP_LuaWorld_CheckVec( L, -1, v, key );
				Com_sprintf( rpSpawnValues[pairs], RP_LUA_VALUE_MAX, "%g %g %g", v[0], v[1], v[2] );
				break;
			}
			default:
				return luaL_error( L, "World.spawn: the value of \"%s\" must be a string, number, boolean or vector", key );
			}
			pairs++;
			lua_pop( L, 1 );
		}
	}

	if ( p->numEnts >= RP_LuaWorld_Clamp( rp_lua_maxents.integer, 0, 1024 ) ) {
		return RP_LuaWorld_Refused( L, va( "this plugin already has %d entities (rp_lua_maxents)", p->numEnts ) );
	}

	RP_SpawnRouteInit( &route );
	RP_SpawnRouteNoteKey( &route, "classname", classname );
	for ( i = 0; i < pairs; i++ ) {
		RP_SpawnRouteNoteKey( &route, rpSpawnKeys[i], rpSpawnValues[i] );
	}
	if ( !RP_SpawnRouteHasRoom( &route ) ) {
		return RP_LuaWorld_Refused( L, RP_SpawnRouteIsLogical( &route )
			? "the server is near its logical entity limit" : "the server is near its entity limit" );
	}

	bytes = 64 + RP_LuaWorld_StrBytes( "classname" ) + RP_LuaWorld_StrBytes( classname );
	for ( i = 0; i < pairs; i++ ) {
		bytes += RP_LuaWorld_StrBytes( rpSpawnKeys[i] ) + RP_LuaWorld_StrBytes( rpSpawnValues[i] );
	}
	limit = RP_LuaWorld_Clamp( rp_lua_spawnkb.integer, 0, 2048 ) * 1024;
	if ( level.rp_lua_spawn_bytes + bytes > limit ) {
		return RP_LuaWorld_Refused( L, va( "scripts have used their %d KB of spawn memory for this map (rp_lua_spawnkb); it is freed when the map changes", limit / 1024 ) );
	}
	if ( !G_AllocRoomFor( bytes ) ) {
		return RP_LuaWorld_Refused( L, "the game's memory pool is nearly full; it is freed when the map changes" );
	}

	before = G_AllocRemaining();
	ent = RP_SpawnForRoute( &route );
	if ( !ent ) {
		return RP_LuaWorld_Refused( L, "no free entity slot" );
	}
	zyk_main_set_entity_field( ent, "classname", classname );
	for ( i = 0; i < pairs; i++ ) {
		zyk_main_set_entity_field( ent, rpSpawnKeys[i], rpSpawnValues[i] );
	}
	level.rp_spawn_refusal[0] = '\0';
	zyk_main_spawn_entity( ent );

	used = before - G_AllocRemaining();
	level.rp_lua_spawn_bytes += ( used > 0 ) ? used : 0;

	if ( !ent->inuse ) {
		return RP_LuaWorld_Refused( L, level.rp_spawn_refusal[0] ? level.rp_spawn_refusal
			: "it did not survive being spawned (a class that removes itself, or a spawn its spawn function refused)" );
	}

	num = ent->s.number;
	rpEnts[num].owner = plugin + 1;
	rpEnts[num].gen = g_entityGen[num];
	p->numEnts++;
	RP_LuaWorld_PushEntity( L, ent );
	return 1;
}

/*
================
World.zone
================
*/
static int RP_LuaWorld_Zone( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	rpLuaPlugin_t *p = &rl.plugins[plugin];
	vec3_t mins, maxs, center;
	gentity_t *ent;
	qboolean npcs;
	int i, z;

	luaL_checktype( L, 1, LUA_TTABLE );
	lua_getfield( L, 1, "mins" );
	RP_LuaWorld_CheckVec( L, -1, mins, "mins" );
	lua_getfield( L, 1, "maxs" );
	RP_LuaWorld_CheckVec( L, -1, maxs, "maxs" );
	lua_pop( L, 2 );
	if ( lua_getfield( L, 1, "npcs" ) != LUA_TNIL && !lua_isboolean( L, -1 ) ) {
		return luaL_error( L, "World.zone: npcs must be true or false" );
	}
	npcs = lua_toboolean( L, -1 ) ? qtrue : qfalse;
	lua_pop( L, 1 );
	for ( i = 0; i < 3; i++ ) {
		if ( maxs[i] <= mins[i] || maxs[i] - mins[i] > RP_LUA_ZONE_MAX_SIZE ) {
			return luaL_error( L, "World.zone: each side must be more than 0 and at most %d units, with maxs above mins", (int)RP_LUA_ZONE_MAX_SIZE );
		}
	}

	if ( p->numEnts >= RP_LuaWorld_Clamp( rp_lua_maxents.integer, 0, 1024 ) ) {
		return RP_LuaWorld_Refused( L, va( "this plugin already has %d entities (rp_lua_maxents)", p->numEnts ) );
	}
	for ( z = 0; z < RP_LUA_MAX_ZONES; z++ ) {
		if ( !rpZones[z].active ) {
			break;
		}
	}
	if ( z == RP_LUA_MAX_ZONES ) {
		return RP_LuaWorld_Refused( L, va( "too many zones (at most %d for all plugins)", RP_LUA_MAX_ZONES ) );
	}
	if ( !G_EntitySlotsAvailable( 1 ) ) {
		return RP_LuaWorld_Refused( L, "the server is near its entity limit" );
	}

	ent = G_Spawn();
	ent->classname = "lua_zone";
	VectorAdd( mins, maxs, center );
	VectorScale( center, 0.5f, center );
	G_SetOrigin( ent, center );
	VectorSubtract( mins, center, ent->r.mins );
	VectorSubtract( maxs, center, ent->r.maxs );
	ent->r.contents = CONTENTS_TRIGGER;
	ent->r.svFlags |= SVF_NOCLIENT;
	ent->touch = RP_LuaWorld_ZoneTouch;
	trap->LinkEntity( (sharedEntity_t *)ent );

	memset( &rpZones[z], 0, sizeof( rpZones[z] ) );
	rpZones[z].active = qtrue;
	rpZones[z].num = ent->s.number;
	rpZones[z].npcs = npcs;
	rpEnts[ent->s.number].owner = plugin + 1;
	rpEnts[ent->s.number].gen = g_entityGen[ent->s.number];
	rpEnts[ent->s.number].zone = z + 1;
	p->numEnts++;
	RP_LuaWorld_PushEntity( L, ent );
	return 1;
}

/*
================
World.find, World.by_number
================
*/
typedef struct {
	gentity_t	*ent;
	float		dist;
} rpLuaFound_t;

static int RP_LuaWorld_FoundCompare( const void *a, const void *b ) {
	const rpLuaFound_t *x = (const rpLuaFound_t *)a, *y = (const rpLuaFound_t *)b;
	if ( x->dist != y->dist ) {
		return x->dist < y->dist ? -1 : 1;
	}
	return ( x->ent < y->ent ) ? -1 : ( x->ent > y->ent ? 1 : 0 );
}

static const char *RP_LuaWorld_OptString( lua_State *L, int t, const char *field ) {
	const char *s = NULL;
	if ( lua_getfield( L, t, field ) != LUA_TNIL ) {
		if ( lua_type( L, -1 ) != LUA_TSTRING ) {
			luaL_error( L, "World.find: %s must be a string", field );
			return NULL;
		}
		s = lua_tostring( L, -1 );	// stays valid: the table holds it
	}
	lua_pop( L, 1 );
	return s;
}

static qboolean RP_LuaWorld_Listed( const gentity_t *e ) {
	int num = (int)( e - g_entities );
	return ( e->inuse && num >= MAX_CLIENTS && num != ENTITYNUM_WORLD && num != ENTITYNUM_NONE
		&& !e->freeAfterEvent && e->s.eType < ET_EVENTS ) ? qtrue : qfalse;
}

static int RP_LuaWorld_Find( lua_State *L ) {
	static rpLuaFound_t found[MAX_ENTITIESTOTAL];
	const char *classname = NULL, *targetname = NULL, *target = NULL;
	qboolean hasNear = qfalse;
	vec3_t nearPt = { 0, 0, 0 };
	float radius = 0;
	gentity_t *e;
	int n = 0, i, count;

	RP_Lua_CurrentPlugin( L );
	if ( !lua_isnoneornil( L, 1 ) ) {
		luaL_checktype( L, 1, LUA_TTABLE );
		classname = RP_LuaWorld_OptString( L, 1, "classname" );
		targetname = RP_LuaWorld_OptString( L, 1, "targetname" );
		target = RP_LuaWorld_OptString( L, 1, "target" );
		if ( lua_getfield( L, 1, "near" ) != LUA_TNIL ) {
			RP_LuaWorld_CheckVec( L, -1, nearPt, "near" );
			hasNear = qtrue;
		}
		lua_pop( L, 1 );
		if ( lua_getfield( L, 1, "radius" ) != LUA_TNIL ) {
			if ( !hasNear || !RP_LuaWorld_Number( L, -1, &radius ) || radius <= 0 ) {
				return luaL_error( L, "World.find: radius must be a positive number, with near" );
			}
		}
		lua_pop( L, 1 );
	}

	RP_FOR_EACH_ENTITY( e ) {
		vec3_t c;
		if ( !RP_LuaWorld_Listed( e ) ) {
			continue;
		}
		if ( classname && ( !e->classname || Q_stricmp( e->classname, classname ) ) ) {
			continue;
		}
		if ( targetname && ( !e->targetname || Q_stricmp( e->targetname, targetname ) ) ) {
			continue;
		}
		if ( target && ( !e->target || Q_stricmp( e->target, target ) ) ) {
			continue;
		}
		found[n].ent = e;
		found[n].dist = 0;
		if ( hasNear ) {
			RP_LuaWorld_Center( e, c );
			found[n].dist = Distance( c, nearPt );
			if ( radius > 0 && found[n].dist > radius ) {
				continue;
			}
		}
		if ( ++n >= MAX_ENTITIESTOTAL ) {
			break;
		}
		if ( !hasNear && n >= RP_LUA_FIND_MAX ) {
			break;
		}
	}
	if ( hasNear ) {
		qsort( found, n, sizeof( found[0] ), RP_LuaWorld_FoundCompare );
	}
	count = ( n < RP_LUA_FIND_MAX ) ? n : RP_LUA_FIND_MAX;
	lua_createtable( L, count, 0 );
	for ( i = 0; i < count; i++ ) {
		RP_LuaWorld_PushEntity( L, found[i].ent );
		lua_rawseti( L, -2, i + 1 );
	}
	return 1;
}

static int RP_LuaWorld_ByNumber( lua_State *L ) {
	lua_Integer n = luaL_checkinteger( L, 1 );

	if ( n < 0 || n >= MAX_ENTITIESTOTAL || !RP_LuaWorld_Listed( &g_entities[n] ) ) {
		lua_pushnil( L );
		return 1;
	}
	RP_LuaWorld_PushEntity( L, &g_entities[n] );
	return 1;
}

/*
================
Effects, sounds, music, shake
================
*/
// room for n more temp entities this frame, outside the reserve of free slots
qboolean RP_LuaWorld_EventRoom( int n ) {
	if ( rpEvTime != level.time ) {
		rpEvTime = level.time;
		rpEvCount = 0;
	}
	if ( rpEvCount + n > RP_LUA_EVENTS_PER_FRAME || !G_EntitySlotsAvailable( n ) ) {
		return qfalse;
	}
	rpEvCount += n;
	return qtrue;
}

static int RP_LuaWorld_Fail( lua_State *L, const char *reason ) {
	lua_pushboolean( L, 0 );
	lua_pushstring( L, reason );
	return 2;
}

static int RP_LuaWorld_FindEffect( const char *name ) {
	char s[MAX_STRING_CHARS];
	int i;

	for ( i = 1; i < MAX_FX; i++ ) {
		trap->GetConfigstring( CS_EFFECTS + i, s, sizeof( s ) );
		if ( !s[0] ) {
			break;
		}
		if ( !strcmp( s, name ) ) {
			return i;
		}
	}
	return 0;
}

qboolean RP_LuaWorld_PathName( const char *s ) {
	int i;
	if ( !s[0] || s[0] == '/' || s[0] == '*' || strstr( s, ".." ) || strlen( s ) >= MAX_QPATH ) {
		return qfalse;
	}
	for ( i = 0; s[i]; i++ ) {
		char c = s[i];
		if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '-' || c == '.' || c == '/' ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

static int RP_LuaWorld_Effect( lua_State *L ) {
	const char *name = luaL_checkstring( L, 1 );
	vec3_t org, dir = { 0, 0, 1 };
	int index;

	RP_Lua_CurrentPlugin( L );
	if ( !RP_LuaWorld_PathName( name ) ) {
		return luaL_error( L, "World.effect: \"%s\" is not an effect file name", name );
	}
	RP_LuaWorld_CheckVec( L, 2, org, "the effect's position" );
	if ( !lua_isnoneornil( L, 3 ) ) {
		RP_LuaWorld_CheckVec( L, 3, dir, "the effect's direction" );
	}

	index = RP_LuaWorld_FindEffect( name );
	if ( !index ) {
		char path[MAX_QPATH * 2];
		int cap = RP_LuaWorld_Clamp( rp_lua_newfx.integer, 0, MAX_FX );

		// the path the effect system opens (as RP_EntitySystemSpawnRefused checks an fx_runner's)
		Q_strncpyz( path, name, sizeof( path ) );
		if ( !strchr( path, '.' ) ) {
			Q_strcat( path, sizeof( path ), ".efx" );
		}
		if ( Q_strncmp( name, "effects", 7 ) ) {
			char tmp[MAX_QPATH * 2];
			Q_strncpyz( tmp, path, sizeof( tmp ) );
			Com_sprintf( path, sizeof( path ), "effects/%s", tmp );
		}
		if ( !RP_FileExists( path ) ) {
			return RP_LuaWorld_Fail( L, va( "effect %s is not on the server", name ) );
		}
		if ( level.rp_lua_new_fx >= cap ) {
			return RP_LuaWorld_Fail( L, va( "scripts have registered their %d new effects for this map (rp_lua_newfx); effects the map already uses still play", cap ) );
		}
		if ( !RP_LuaWorld_EventRoom( 1 ) ) {
			return RP_LuaWorld_Fail( L, "too many effects, sounds and shakes this frame" );
		}
		index = G_EffectIndex( name );
		if ( !index ) {
			rpEvCount--;
			return RP_LuaWorld_Fail( L, "the map has no effect slot left" );
		}
		level.rp_lua_new_fx++;
	} else if ( !RP_LuaWorld_EventRoom( 1 ) ) {
		return RP_LuaWorld_Fail( L, "too many effects, sounds and shakes this frame" );
	}
	G_PlayEffectID( index, org, dir );
	lua_pushboolean( L, 1 );
	return 1;
}

static int RP_LuaWorld_Sound( lua_State *L ) {
	const char *name = luaL_checkstring( L, 1 );
	gentity_t *at = NULL;
	vec3_t org;
	char reason[256];
	int index;

	RP_Lua_CurrentPlugin( L );
	if ( !RP_LuaWorld_PathName( name ) ) {
		return luaL_error( L, "World.sound: \"%s\" is not a sound file name", name );
	}
	if ( lua_istable( L, 2 ) ) {
		RP_LuaWorld_CheckVec( L, 2, org, "the sound's position" );
	} else {
		at = RP_LuaWorld_OptActor( L, 2 );
		if ( !at ) {
			return luaL_error( L, "World.sound: give a position, an entity or a player" );
		}
		RP_LuaWorld_Center( at, org );
		if ( at->isLogical ) {
			at = NULL;	// not networked: nothing to follow, so play at its position
		}
	}
	if ( !RP_SoundFileExists( name ) ) {
		return RP_LuaWorld_Fail( L, va( "sound %s is not on the server", name ) );
	}
	if ( !RP_LuaWorld_EventRoom( 1 ) ) {
		return RP_LuaWorld_Fail( L, "too many effects, sounds and shakes this frame" );
	}
	index = RP_PlaySoundIndex( name, reason, sizeof( reason ) );
	if ( !index ) {
		rpEvCount--;
		return RP_LuaWorld_Fail( L, reason[0] ? reason : "no sound slot is free" );
	}
	if ( at ) {
		G_EntitySound( at, CHAN_AUTO, index );
	} else {
		G_SoundAtLoc( org, CHAN_AUTO, index );
	}
	lua_pushboolean( L, 1 );
	return 1;
}

static int RP_LuaWorld_Music( lua_State *L ) {
	const char *track = luaL_checkstring( L, 1 );
	const char *reason = "";

	RP_Lua_CurrentPlugin( L );
	if ( !track[0] ) {
		trap->SetConfigstring( CS_MUSIC, "" );
		lua_pushboolean( L, 1 );
		return 1;
	}
	if ( !RP_MusicFileExists( track ) ) {
		return RP_LuaWorld_Fail( L, va( "music %s is not on the server", track ) );
	}
	if ( !G_SetMusic( track, &reason ) ) {
		return RP_LuaWorld_Fail( L, reason && reason[0] ? reason : "the music was refused" );
	}
	lua_pushboolean( L, 1 );
	return 1;
}

static int RP_LuaWorld_Shake( lua_State *L ) {
	vec3_t org;
	lua_Integer radius = luaL_checkinteger( L, 2 );
	lua_Integer intensity = luaL_checkinteger( L, 3 );
	lua_Integer ms = luaL_checkinteger( L, 4 );
	int i, n = 0, shaken = 0;

	RP_Lua_CurrentPlugin( L );
	RP_LuaWorld_CheckVec( L, 1, org, "the shake's position" );
	if ( radius < 0 || radius > RP_LUA_SHAKE_MAX_RADIUS ) {
		return luaL_error( L, "World.shake: radius must be 0 to %d", RP_LUA_SHAKE_MAX_RADIUS );
	}
	if ( intensity < RP_LUA_SHAKE_MIN_INTENSITY || intensity > RP_LUA_SHAKE_MAX_INTENSITY ) {
		return luaL_error( L, "World.shake: intensity must be %d to %d", RP_LUA_SHAKE_MIN_INTENSITY, RP_LUA_SHAKE_MAX_INTENSITY );
	}
	if ( ms < RP_LUA_SHAKE_MIN_MS || ms > RP_LUA_SHAKE_MAX_MS ) {
		return luaL_error( L, "World.shake: the length must be %d to %d ms", RP_LUA_SHAKE_MIN_MS, RP_LUA_SHAKE_MAX_MS );
	}
	for ( i = 0; i < level.maxclients && i < MAX_CLIENTS; i++ ) {
		gentity_t *pl = &g_entities[i];
		if ( pl->inuse && pl->client && pl->client->pers.connected == CON_CONNECTED && Distance( org, pl->client->ps.origin ) <= radius ) {
			n++;
		}
	}
	if ( n && !RP_LuaWorld_EventRoom( n ) ) {
		return RP_LuaWorld_Fail( L, "too many effects, sounds and shakes this frame" );
	}
	for ( i = 0; i < level.maxclients && i < MAX_CLIENTS && shaken < n; i++ ) {
		gentity_t *pl = &g_entities[i];
		if ( pl->inuse && pl->client && pl->client->pers.connected == CON_CONNECTED && Distance( org, pl->client->ps.origin ) <= radius ) {
			G_ScreenShake( pl->client->ps.origin, pl, (float)intensity, (int)ms, qtrue );
			shaken++;
		}
	}
	lua_pushinteger( L, shaken );
	return 1;
}

static int RP_LuaWorld_Distance( lua_State *L ) {
	vec3_t a, b;
	RP_LuaWorld_CheckVec( L, 1, a, "the first point" );
	RP_LuaWorld_CheckVec( L, 2, b, "the second point" );
	lua_pushnumber( L, Distance( a, b ) );
	return 1;
}

/*
================
Entity methods
================
*/
static int RP_LuaWorld_EntKey( lua_State *L ) {
	gentity_t *ent = RP_LuaWorld_CheckEntity( L, 1 );
	const char *key = luaL_checkstring( L, 2 );
	int num = ent->s.number, j, count = level.zyk_spawn_strings_values_count[num];

	for ( j = 0; j + 1 < count && j + 1 < ZYK_MAX_SPAWN_STRING_SLOTS; j += 2 ) {
		const char *k = level.zyk_spawn_strings[num][j];
		if ( k && !Q_stricmp( k, key ) ) {
			lua_pushstring( L, level.zyk_spawn_strings[num][j + 1] ? level.zyk_spawn_strings[num][j + 1] : "" );
			return 1;
		}
	}
	lua_pushnil( L );
	return 1;
}

static int RP_LuaWorld_EntUse( lua_State *L ) {
	gentity_t *ent = RP_LuaWorld_CheckEntity( L, 1 );
	gentity_t *activator = RP_LuaWorld_OptActor( L, 2 );

	RP_Lua_CurrentPlugin( L );
	if ( activator ) {
		GlobalUse( ent, activator, activator );		// a player's use: locks judge it
	} else {
		GlobalUse( ent, ent, ent );					// map logic, as ICARUS and the map's relays use
	}
	return 0;
}

static int RP_LuaWorld_EntFree( lua_State *L ) {
	gentity_t *ent = RP_LuaWorld_CheckOwn( L, 1, "free" );
	rpLuaEnt_t *e = &rpEnts[ent->s.number];

	if ( rpCallbackDepth > 0 ) {
		if ( !e->pendingFree ) {
			e->pendingFree = qtrue;
			rpPendingFrees++;
		}
		return 0;
	}
	RP_LuaWorld_DoFree( ent );
	return 0;
}

// move and rotate: brush entities and models that clients see, not zones or a mover team's followers
static gentity_t *RP_LuaWorld_CheckMovable( lua_State *L, const char *what ) {
	gentity_t *ent = RP_LuaWorld_CheckOwn( L, 1, what );

	if ( rpEnts[ent->s.number].zone || ent->isLogical || !ent->classname
		|| ( Q_stricmpn( ent->classname, "func_", 5 ) && Q_stricmpn( ent->classname, "misc_model", 10 ) ) ) {
		luaL_error( L, "entity %d (%s) cannot be %sd: only brush entities (func_*) and models (misc_model*) can", ent->s.number,
			ent->classname ? ent->classname : "noclass", what );
		return NULL;
	}
	if ( ent->flags & FL_TEAMSLAVE ) {
		luaL_error( L, "entity %d is part of a mover team: %s the team's first entity instead", ent->s.number, what );
		return NULL;
	}
	return ent;
}

static int RP_LuaWorld_CheckMs( lua_State *L, int idx ) {
	lua_Integer ms = luaL_optinteger( L, idx, 0 );
	if ( ms < 0 || ms > RP_LUA_MOVE_MAX_MS ) {
		return luaL_error( L, "the time must be 0 to %d ms", RP_LUA_MOVE_MAX_MS );
	}
	return ms > 0 ? (int)ms : 1;
}

// the end of a move: moverCallback (g_ICARUScb.c) without the ICARUS task, which only a script
// running under ICARUS has
static void RP_LuaWorld_MoveDone( gentity_t *ent ) {
	ent->s.loopSound = 0;
	ent->s.loopIsSoundset = qfalse;
	G_PlayDoorSound( ent, BMS_END );
	if ( ent->moverState == MOVER_1TO2 ) {
		MatchTeam( ent, MOVER_POS2, level.time );
	} else if ( ent->moverState == MOVER_2TO1 ) {
		MatchTeam( ent, MOVER_POS1, level.time );
	}
	if ( ent->blocked == Blocked_Mover ) {
		ent->blocked = 0;
	}
}

static int RP_LuaWorld_EntMove( lua_State *L ) {
	gentity_t *ent = RP_LuaWorld_CheckMovable( L, "move" );
	vec3_t to;
	int duration, moverState;

	RP_LuaWorld_CheckVec( L, 2, to, "the position to move to" );
	duration = RP_LuaWorld_CheckMs( L, 3 );

	// Q3_Lerp2Origin (g_ICARUScb.c) without the ICARUS task
	if ( ent->s.eType != ET_MOVER ) {
		ent->s.eType = ET_MOVER;
	}
	moverState = ent->moverState;
	if ( moverState == MOVER_POS1 || moverState == MOVER_2TO1 ) {
		VectorCopy( ent->r.currentOrigin, ent->pos1 );
		VectorCopy( to, ent->pos2 );
		moverState = MOVER_1TO2;
	} else {
		VectorCopy( ent->r.currentOrigin, ent->pos2 );
		VectorCopy( to, ent->pos1 );
		moverState = MOVER_2TO1;
	}
	InitMoverTrData( ent );
	ent->s.pos.trDuration = duration;
	MatchTeam( ent, moverState, level.time );
	ent->reached = RP_LuaWorld_MoveDone;
	if ( ent->damage ) {
		ent->blocked = Blocked_Mover;
	}
	G_PlayDoorLoopSound( ent );
	G_PlayDoorSound( ent, BMS_START );
	trap->LinkEntity( (sharedEntity_t *)ent );
	return 0;
}

// the end of a turn: anglerCallback (g_ICARUScb.c) without the ICARUS task, giving the entity back
// the think it had before
static void RP_LuaWorld_RotateDone( gentity_t *ent ) {
	rpLuaEnt_t *e = &rpEnts[ent->s.number];

	VectorMA( ent->s.apos.trBase, ( ent->s.apos.trDuration * 0.001f ), ent->s.apos.trDelta, ent->r.currentAngles );
	VectorCopy( ent->r.currentAngles, ent->s.apos.trBase );
	VectorClear( ent->s.apos.trDelta );
	ent->s.apos.trDuration = 1;
	ent->s.apos.trType = TR_STATIONARY;
	ent->s.apos.trTime = level.time;

	ent->think = NULL;
	ent->nextthink = 0;
	if ( e->thinkSaved ) {
		ent->think = e->origThink;
		if ( e->origThink && e->origNextthink ) {
			ent->nextthink = ( e->origNextthink > level.time ) ? e->origNextthink : level.time;
		}
		e->thinkSaved = qfalse;
		e->origThink = NULL;
		e->origNextthink = 0;
	}
	trap->LinkEntity( (sharedEntity_t *)ent );
}

static int RP_LuaWorld_EntRotate( lua_State *L ) {
	gentity_t *ent = RP_LuaWorld_CheckMovable( L, "rotate" );
	rpLuaEnt_t *e = &rpEnts[ent->s.number];
	vec3_t angles;
	int duration, i;

	RP_LuaWorld_CheckVec( L, 2, angles, "the angles to turn to" );
	duration = RP_LuaWorld_CheckMs( L, 3 );

	// Q3_Lerp2Angles (g_ICARUScb.c) without the ICARUS task
	ent->s.apos.trDuration = duration;
	for ( i = 0; i < 3; i++ ) {
		float ang = AngleSubtract( angles[i], ent->r.currentAngles[i] );
		ent->s.apos.trDelta[i] = ang / ( duration * 0.001f );
	}
	VectorCopy( ent->r.currentAngles, ent->s.apos.trBase );
	ent->s.apos.trType = ent->alt_fire ? TR_LINEAR_STOP : TR_NONLINEAR_STOP;
	ent->s.apos.trTime = level.time;

	if ( ent->think != RP_LuaWorld_RotateDone ) {
		e->origThink = ent->think;
		e->origNextthink = ent->nextthink;
		e->thinkSaved = qtrue;
	}
	ent->think = RP_LuaWorld_RotateDone;
	ent->nextthink = level.time + duration;
	trap->LinkEntity( (sharedEntity_t *)ent );
	return 0;
}

// on_use / on_pain / on_die / on_enter / on_leave: a function sets it, nil removes it
static int RP_LuaWorld_SetCallback( lua_State *L, int *ref ) {
	if ( lua_isnoneornil( L, 2 ) ) {
		RP_LuaWorld_Unref( ref );
		return 0;
	}
	luaL_checktype( L, 2, LUA_TFUNCTION );
	RP_LuaWorld_Unref( ref );
	lua_pushvalue( L, 2 );
	*ref = luaL_ref( L, LUA_REGISTRYINDEX );
	return 1;
}

static int RP_LuaWorld_EntOnUse( lua_State *L ) {
	gentity_t *ent = RP_LuaWorld_CheckOwn( L, 1, "give callbacks to" );
	rpLuaEnt_t *e = &rpEnts[ent->s.number];

	if ( e->zone ) {
		return luaL_error( L, "a zone has on_enter and on_leave, not on_use" );
	}
	if ( RP_LuaWorld_SetCallback( L, &e->useRef ) ) {
		if ( !( ent->r.svFlags & SVF_PLAYER_USABLE ) ) {
			ent->r.svFlags |= SVF_PLAYER_USABLE;
			e->addedUsable = qtrue;
		}
		if ( !ent->use ) {
			ent->use = RP_LuaWorld_UseStub;
			e->addedUseStub = qtrue;
		}
	} else {
		if ( e->addedUsable ) {
			ent->r.svFlags &= ~SVF_PLAYER_USABLE;
			e->addedUsable = qfalse;
		}
		if ( e->addedUseStub && ent->use == RP_LuaWorld_UseStub ) {
			ent->use = NULL;
		}
		e->addedUseStub = qfalse;
	}
	return 0;
}

static gentity_t *RP_LuaWorld_CheckDamageable( lua_State *L ) {
	gentity_t *ent = RP_LuaWorld_CheckOwn( L, 1, "give callbacks to" );
	if ( ent->NPC ) {
		// its pain and die are the AI's own and NPC_Begin sets them again when it finishes spawning
		luaL_error( L, "an NPC has on_death, and the damage event, instead of on_pain and on_die" );
		return NULL;
	}
	if ( !ent->takedamage ) {
		luaL_error( L, "entity %d (%s) cannot be damaged, so it has no on_pain or on_die", ent->s.number, ent->classname ? ent->classname : "noclass" );
		return NULL;
	}
	return ent;
}

static int RP_LuaWorld_EntOnPain( lua_State *L ) {
	gentity_t *ent = RP_LuaWorld_CheckDamageable( L );
	rpLuaEnt_t *e = &rpEnts[ent->s.number];

	if ( RP_LuaWorld_SetCallback( L, &e->painRef ) ) {
		if ( ent->pain != RP_LuaWorld_Pain ) {
			e->origPain = ent->pain;
			ent->pain = RP_LuaWorld_Pain;
		}
	} else if ( ent->pain == RP_LuaWorld_Pain ) {
		ent->pain = e->origPain;
		e->origPain = NULL;
	}
	return 0;
}

static int RP_LuaWorld_EntOnDie( lua_State *L ) {
	gentity_t *ent = RP_LuaWorld_CheckDamageable( L );
	rpLuaEnt_t *e = &rpEnts[ent->s.number];

	if ( RP_LuaWorld_SetCallback( L, &e->dieRef ) ) {
		if ( ent->die != RP_LuaWorld_Die ) {
			e->origDie = ent->die;
			ent->die = RP_LuaWorld_Die;
		}
	} else if ( ent->die == RP_LuaWorld_Die ) {
		ent->die = e->origDie;
		e->origDie = NULL;
	}
	return 0;
}

static int RP_LuaWorld_EntOnZone( lua_State *L, qboolean enter ) {
	gentity_t *ent = RP_LuaWorld_CheckOwn( L, 1, "give callbacks to" );
	rpLuaEnt_t *e = &rpEnts[ent->s.number];

	if ( !e->zone ) {
		return luaL_error( L, "on_enter and on_leave are for zones (World.zone)" );
	}
	RP_LuaWorld_SetCallback( L, enter ? &e->enterRef : &e->leaveRef );
	return 0;
}

static int RP_LuaWorld_EntOnEnter( lua_State *L ) {
	return RP_LuaWorld_EntOnZone( L, qtrue );
}

static int RP_LuaWorld_EntOnLeave( lua_State *L ) {
	return RP_LuaWorld_EntOnZone( L, qfalse );
}

static void RP_LuaWorld_PushOptString( lua_State *L, const char *s ) {
	if ( s && s[0] ) {
		lua_pushstring( L, s );
	} else {
		lua_pushnil( L );
	}
}

/*
================
NPCs (API version 3): World.npc, and what any NPC's handle tells and a script's own NPCs do
================
*/
static const char *RP_LuaWorld_NpcTeamName( int team ) {
	switch ( team ) {
	case NPCTEAM_ENEMY:		return "enemy";
	case NPCTEAM_PLAYER:	return "player";
	case NPCTEAM_NEUTRAL:	return "neutral";
	default:				return "free";
	}
}

static int RP_LuaWorld_NpcTeamFromName( const char *name ) {
	if ( !strcmp( name, "enemy" ) ) {
		return NPCTEAM_ENEMY;
	}
	if ( !strcmp( name, "player" ) ) {
		return NPCTEAM_PLAYER;
	}
	if ( !strcmp( name, "neutral" ) ) {
		return NPCTEAM_NEUTRAL;
	}
	if ( !strcmp( name, "free" ) ) {
		return NPCTEAM_FREE;
	}
	return -1;
}

static qboolean RP_LuaWorld_IsVehicle( const gentity_t *ent ) {
	return ( ent->client && ( ent->client->NPC_class == CLASS_VEHICLE || ent->s.NPC_class == CLASS_VEHICLE ) ) ? qtrue : qfalse;
}

// World.npc(type, pos [, { yaw =, team =, effect =, vehicle = }]): /npc spawn at a point
static int RP_LuaWorld_Npc( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	rpLuaPlugin_t *p = &rl.plugins[plugin];
	const char *type = luaL_checkstring( L, 1 );
	char typeBuf[RP_LUA_KEY_MAX];
	vec3_t org;
	float yaw = 0;
	int team = -1, effect = RP_PHASE_NONE, i, num;
	qboolean vehicle = qfalse;
	gentity_t *npc;

	for ( i = 0; type[i]; i++ ) {
		char c = type[i];
		if ( i >= RP_LUA_KEY_MAX - 1 || !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '-' ) ) {
			return luaL_error( L, "World.npc: \"%s\" is not an NPC type name", type );
		}
	}
	if ( !i ) {
		return luaL_error( L, "World.npc: the NPC type is empty" );
	}
	Q_strncpyz( typeBuf, type, sizeof( typeBuf ) );
	RP_LuaWorld_CheckVec( L, 2, org, "the NPC's position" );
	if ( !lua_isnoneornil( L, 3 ) ) {
		luaL_checktype( L, 3, LUA_TTABLE );
		lua_pushnil( L );
		while ( lua_next( L, 3 ) ) {
			const char *key;
			if ( lua_type( L, -2 ) != LUA_TSTRING ) {
				return luaL_error( L, "World.npc: option names must be strings" );
			}
			key = lua_tostring( L, -2 );
			if ( !strcmp( key, "yaw" ) ) {
				if ( !RP_LuaWorld_Number( L, -1, &yaw ) ) {
					return luaL_error( L, "World.npc: yaw must be a number of degrees" );
				}
			} else if ( !strcmp( key, "team" ) ) {
				if ( lua_type( L, -1 ) != LUA_TSTRING || ( team = RP_LuaWorld_NpcTeamFromName( lua_tostring( L, -1 ) ) ) < 0 ) {
					return luaL_error( L, "World.npc: team must be \"enemy\", \"player\", \"neutral\" or \"free\"" );
				}
			} else if ( !strcmp( key, "effect" ) ) {
				if ( lua_type( L, -1 ) != LUA_TSTRING || ( effect = RP_LuaPeople_EffectFromName( lua_tostring( L, -1 ) ) ) < 0
					|| effect == RP_PHASE_HIDDEN ) {
					return luaL_error( L, "World.npc: effect must be \"walkthrough\", \"ghost\" or \"holo\"" );
				}
			} else if ( !strcmp( key, "vehicle" ) ) {
				if ( !lua_isboolean( L, -1 ) ) {
					return luaL_error( L, "World.npc: vehicle must be true or false" );
				}
				vehicle = lua_toboolean( L, -1 ) ? qtrue : qfalse;
			} else {
				return luaL_error( L, "World.npc: there is no option \"%s\" (yaw, team, effect, vehicle)", key );
			}
			lua_pop( L, 1 );
		}
	}
	if ( vehicle && effect != RP_PHASE_NONE ) {
		return luaL_error( L, "World.npc: vehicles cannot be given an effect" );
	}

	// the checks /entadd makes for a spawner (zyk_spawner_problem, g_cmds.c), then the caps
	if ( vehicle ) {
		if ( !RP_VehicleTypeKnown( typeBuf ) ) {
			return RP_LuaWorld_Refused( L, va( "%s is not a vehicle type on this server (see /list vehicles)", typeBuf ) );
		}
		if ( !RP_NpcTypeKnown( typeBuf ) ) {
			return RP_LuaWorld_Refused( L, va( "the vehicle %s has no NPC entry of its name on this server, so it cannot be spawned", typeBuf ) );
		}
	} else {
		if ( !g_allowNPC.integer ) {
			return RP_LuaWorld_Refused( L, "NPCs are switched off on this server (g_allowNPC 0)" );
		}
		if ( RP_NpcTypeIsVehicle( typeBuf ) ) {
			return RP_LuaWorld_Refused( L, va( "%s is a vehicle's NPC entry: spawn it with vehicle = true", typeBuf ) );
		}
		if ( !RP_NpcTypeKnown( typeBuf ) ) {
			return RP_LuaWorld_Refused( L, va( "%s is not an NPC type on this server (see /list npcs)", typeBuf ) );
		}
	}
	if ( p->numNpcs >= RP_LuaWorld_Clamp( rp_lua_maxnpcs.integer, 0, 256 ) ) {
		return RP_LuaWorld_Refused( L, va( "this plugin already has %d NPCs (rp_lua_maxnpcs)", p->numNpcs ) );
	}
	if ( p->numEnts >= RP_LuaWorld_Clamp( rp_lua_maxents.integer, 0, 1024 ) ) {
		return RP_LuaWorld_Refused( L, va( "this plugin already has %d entities (rp_lua_maxents)", p->numEnts ) );
	}
	if ( !G_EntitySlotsAvailable( 3 ) ) {
		return RP_LuaWorld_Refused( L, "the server is near its entity limit" );
	}

	npc = RP_NpcSpawnAt( typeBuf, org, yaw, team, effect, vehicle );
	if ( !npc || !npc->inuse || !npc->client ) {
		return RP_LuaWorld_Refused( L, "the NPC could not be spawned (the server console says why)" );
	}
	num = npc->s.number;
	rpEnts[num].owner = plugin + 1;
	rpEnts[num].gen = g_entityGen[num];
	rpEnts[num].isNpc = qtrue;
	p->numEnts++;
	p->numNpcs++;
	RP_LuaWorld_PushEntity( L, npc );
	return 1;
}

// the running plugin's own NPC, or a Lua error
static gentity_t *RP_LuaWorld_CheckOwnNpc( lua_State *L, const char *what, const char *method ) {
	gentity_t *ent = RP_LuaWorld_CheckOwn( L, 1, what );

	if ( !ent->NPC || !ent->client ) {
		luaL_error( L, "%s: entity %d (%s) is not an NPC", method, ent->s.number, ent->classname ? ent->classname : "noclass" );
		return NULL;
	}
	return ent;
}

static int RP_LuaWorld_EntKill( lua_State *L ) {
	gentity_t *npc = RP_LuaWorld_CheckOwnNpc( L, "kill", "kill" );
	rpLuaEnt_t *e = &rpEnts[npc->s.number];

	if ( e->pendingKill ) {
		lua_pushboolean( L, 1 );	// already on its way
		return 1;
	}
	if ( e->pendingFree ) {
		return RP_LuaWorld_Fail( L, "it is being removed" );
	}
	if ( !RP_LuaWorld_NpcAlive( npc ) ) {
		return RP_LuaWorld_Fail( L, "it is already dead" );
	}
	if ( npc->m_pVehicle && npc->m_pVehicle->m_pPilot ) {
		return RP_LuaWorld_Fail( L, "someone is riding it" );	// as /npc kill leaves a ridden vehicle
	}
	if ( rpCallbackDepth > 0 || npc->think == NPC_Begin ) {
		e->pendingKill = qtrue;
		rpPendingKills++;
	} else {
		RP_LuaWorld_DoKill( npc );
	}
	lua_pushboolean( L, 1 );
	return 1;
}

static int RP_LuaWorld_EntTeleport( lua_State *L ) {
	gentity_t *npc = RP_LuaWorld_CheckOwnNpc( L, "teleport", "teleport" );
	const char *why;
	vec3_t dest;
	float yaw = npc->client->ps.viewangles[YAW];

	RP_LuaWorld_CheckVec( L, 2, dest, "the position to teleport to" );
	if ( !lua_isnoneornil( L, 3 ) && !RP_LuaWorld_Number( L, 3, &yaw ) ) {
		return luaL_error( L, "teleport: the yaw must be a number of degrees" );
	}
	if ( npc->think == NPC_Begin ) {
		return RP_LuaWorld_Fail( L, "it has not finished spawning (try again next frame)" );
	}
	if ( !RP_LuaWorld_NpcAlive( npc ) ) {
		return RP_LuaWorld_Fail( L, "it is dead" );
	}
	why = RP_LuaPeople_TeleportRefusal( npc, dest );
	if ( why ) {
		return RP_LuaWorld_Fail( L, why );
	}
	RP_LuaPeople_DoTeleport( npc, dest, yaw );
	G_LogPrintf( "Lua: %s teleported NPC %d (%s) to (%d %d %d)\n", rl.plugins[RP_Lua_CurrentPlugin( L )].name, npc->s.number,
		npc->NPC_type ? npc->NPC_type : "?", (int)dest[0], (int)dest[1], (int)dest[2] );
	lua_pushboolean( L, 1 );
	return 1;
}

// freeze and unfreeze: /npc freeze, which refuses vehicles and riders
static int RP_LuaWorld_EntFreezeSet( lua_State *L, qboolean on ) {
	gentity_t *npc = RP_LuaWorld_CheckOwnNpc( L, on ? "freeze" : "unfreeze", on ? "freeze" : "unfreeze" );

	if ( RP_LuaWorld_IsVehicle( npc ) ) {
		return RP_LuaWorld_Fail( L, "vehicles cannot be frozen" );
	}
	if ( npc->client->ps.m_iVehicleNum ) {
		return RP_LuaWorld_Fail( L, "it is riding a vehicle" );
	}
	if ( on ) {
		if ( !RP_LuaWorld_NpcAlive( npc ) ) {
			return RP_LuaWorld_Fail( L, "it is dead" );
		}
		RP_NpcFreeze( npc );
	} else {
		RP_NpcUnfreeze( npc );
	}
	lua_pushboolean( L, 1 );
	return 1;
}

static int RP_LuaWorld_EntFreeze( lua_State *L ) {
	return RP_LuaWorld_EntFreezeSet( L, qtrue );
}

static int RP_LuaWorld_EntUnfreeze( lua_State *L ) {
	return RP_LuaWorld_EntFreezeSet( L, qfalse );
}

// set_effect(name or nil): /npc effect's own change, with its refusals
static int RP_LuaWorld_EntSetEffect( lua_State *L ) {
	gentity_t *npc = RP_LuaWorld_CheckOwnNpc( L, "give effects to", "set_effect" );
	int mode = RP_PHASE_NONE;

	if ( !lua_isnoneornil( L, 2 ) ) {
		const char *name = luaL_checkstring( L, 2 );
		mode = RP_LuaPeople_EffectFromName( name );
		if ( mode < 0 || mode == RP_PHASE_HIDDEN ) {
			return luaL_error( L, "set_effect: \"%s\" is not an NPC effect (walkthrough, ghost, holo, or nil for none)", name );
		}
	}
	if ( RP_LuaWorld_IsVehicle( npc ) ) {
		return RP_LuaWorld_Fail( L, "vehicles cannot be given an effect" );
	}
	if ( npc->client->ps.m_iVehicleNum ) {
		return RP_LuaWorld_Fail( L, "it is riding a vehicle" );
	}
	RP_NpcApplyEffect( npc, mode );
	lua_pushboolean( L, 1 );
	return 1;
}

static int RP_LuaWorld_EntOnDeath( lua_State *L ) {
	gentity_t *npc = RP_LuaWorld_CheckOwnNpc( L, "give callbacks to", "on_death" );

	RP_LuaWorld_SetCallback( L, &rpEnts[npc->s.number].deathRef );
	return 0;
}

// the NPC fields of an entity handle: 1 when key was one of them (its value pushed)
static int RP_LuaWorld_NpcField( lua_State *L, gentity_t *ent, const char *key ) {
	qboolean isNpc = ( ent->NPC && ent->client ) ? qtrue : qfalse;

	if ( !strcmp( key, "is_npc" ) ) {
		lua_pushboolean( L, isNpc );
		return 1;
	}
	if ( !strcmp( key, "is_vehicle" ) ) {
		lua_pushboolean( L, isNpc && RP_LuaWorld_IsVehicle( ent ) );
		return 1;
	}
	if ( strcmp( key, "npc_type" ) && strcmp( key, "team" ) && strcmp( key, "max_health" ) && strcmp( key, "alive" )
		&& strcmp( key, "spawner" ) && strcmp( key, "effect" ) && strcmp( key, "frozen" ) ) {
		return 0;
	}
	if ( !isNpc ) {
		lua_pushnil( L );
		return 1;
	}
	if ( !strcmp( key, "npc_type" ) ) {
		RP_LuaWorld_PushOptString( L, ent->NPC_type );
	} else if ( !strcmp( key, "team" ) ) {
		lua_pushstring( L, RP_LuaWorld_NpcTeamName( ent->client->playerTeam ) );
	} else if ( !strcmp( key, "max_health" ) ) {
		lua_pushinteger( L, ent->client->ps.stats[STAT_MAX_HEALTH] );
	} else if ( !strcmp( key, "alive" ) ) {
		lua_pushboolean( L, RP_LuaWorld_NpcAlive( ent ) );
	} else if ( !strcmp( key, "spawner" ) ) {
		const gentity_t *sp = ent->zyk_npc_spawner;
		RP_LuaWorld_PushOptString( L, RP_SpawnerMadeIt( sp, ent ) ? sp->targetname : NULL );
	} else if ( !strcmp( key, "effect" ) ) {
		RP_LuaWorld_PushOptString( L, RP_LuaPeople_EffectName( ent->client->pers.phase_mode ) );
	} else {
		lua_pushboolean( L, RP_NpcIsFrozen( ent ) );
	}
	return 1;
}

static int RP_LuaWorld_EntIndex( lua_State *L ) {
	static const luaL_Reg methods[] = {
		{ "key", RP_LuaWorld_EntKey }, { "use", RP_LuaWorld_EntUse }, { "free", RP_LuaWorld_EntFree },
		{ "move", RP_LuaWorld_EntMove }, { "rotate", RP_LuaWorld_EntRotate }, { "on_use", RP_LuaWorld_EntOnUse },
		{ "on_pain", RP_LuaWorld_EntOnPain }, { "on_die", RP_LuaWorld_EntOnDie }, { "on_enter", RP_LuaWorld_EntOnEnter },
		{ "on_leave", RP_LuaWorld_EntOnLeave }, { "kill", RP_LuaWorld_EntKill }, { "teleport", RP_LuaWorld_EntTeleport },
		{ "freeze", RP_LuaWorld_EntFreeze }, { "unfreeze", RP_LuaWorld_EntUnfreeze }, { "set_effect", RP_LuaWorld_EntSetEffect },
		{ "on_death", RP_LuaWorld_EntOnDeath }, { NULL, NULL } };
	rpLuaEntHandle_t *h = (rpLuaEntHandle_t *)luaL_checkudata( L, 1, RP_LUA_ENTITY_MT );
	const char *key = luaL_checkstring( L, 2 );
	gentity_t *ent;
	int i;

	if ( !strcmp( key, "valid" ) ) {
		lua_pushboolean( L, RP_LuaWorld_Alive( h->num, h->gen ) );
		return 1;
	}
	for ( i = 0; methods[i].name; i++ ) {
		if ( !strcmp( key, methods[i].name ) ) {
			lua_pushcfunction( L, methods[i].func );
			return 1;
		}
	}
	if ( !RP_LuaWorld_Alive( h->num, h->gen ) ) {
		return luaL_error( L, "that entity no longer exists" );
	}
	ent = &g_entities[h->num];

	if ( !strcmp( key, "id" ) ) {
		lua_pushinteger( L, h->num );
	} else if ( !strcmp( key, "classname" ) ) {
		RP_LuaWorld_PushOptString( L, ent->classname );
	} else if ( !strcmp( key, "targetname" ) ) {
		RP_LuaWorld_PushOptString( L, ent->targetname );
	} else if ( !strcmp( key, "target" ) ) {
		RP_LuaWorld_PushOptString( L, ent->target );
	} else if ( !strcmp( key, "origin" ) ) {
		RP_LuaWorld_PushVec( L, ent->r.currentOrigin );
	} else if ( !strcmp( key, "angles" ) ) {
		RP_LuaWorld_PushVec( L, ent->r.currentAngles );
	} else if ( !strcmp( key, "is_logical" ) ) {
		lua_pushboolean( L, ent->isLogical );
	} else if ( !strcmp( key, "owner" ) ) {
		RP_LuaWorld_PushOptString( L, RP_Lua_EntityOwner( ent ) );
	} else if ( !strcmp( key, "health" ) ) {
		if ( ent->takedamage || ( ent->NPC && ent->client ) ) {
			lua_pushinteger( L, ent->health );
		} else {
			lua_pushnil( L );
		}
	} else if ( !RP_LuaWorld_NpcField( L, ent, key ) ) {	// API 3
		lua_pushnil( L );
	}
	return 1;
}

static int RP_LuaWorld_EntNewIndex( lua_State *L ) {
	return luaL_error( L, "entity handles are read-only: use their methods" );
}

static int RP_LuaWorld_EntToString( lua_State *L ) {
	rpLuaEntHandle_t *h = (rpLuaEntHandle_t *)luaL_checkudata( L, 1, RP_LUA_ENTITY_MT );
	if ( RP_LuaWorld_Alive( h->num, h->gen ) ) {
		const char *cls = g_entities[h->num].classname;
		lua_pushfstring( L, "Entity(%d: %s)", h->num, cls ? cls : "noclass" );
	} else {
		lua_pushfstring( L, "Entity(%d: gone)", h->num );
	}
	return 1;
}

/*
================
Registration
================
*/
void RP_LuaWorld_Register( lua_State *L ) {
	static const luaL_Reg worldFuncs[] = {
		{ "spawn", RP_LuaWorld_Spawn }, { "zone", RP_LuaWorld_Zone }, { "find", RP_LuaWorld_Find },
		{ "by_number", RP_LuaWorld_ByNumber }, { "effect", RP_LuaWorld_Effect }, { "sound", RP_LuaWorld_Sound },
		{ "music", RP_LuaWorld_Music }, { "shake", RP_LuaWorld_Shake }, { "distance", RP_LuaWorld_Distance },
		{ "npc", RP_LuaWorld_Npc }, { NULL, NULL } };
	static const luaL_Reg entityMeta[] = {
		{ "__index", RP_LuaWorld_EntIndex }, { "__newindex", RP_LuaWorld_EntNewIndex }, { "__tostring", RP_LuaWorld_EntToString }, { NULL, NULL } };
	int g = lua_absindex( L, -1 );

	lua_newtable( L );
	luaL_setfuncs( L, worldFuncs, 0 );
	lua_setfield( L, g, "World" );

	luaL_newmetatable( L, RP_LUA_ENTITY_MT );
	luaL_setfuncs( L, entityMeta, 0 );
	lua_pushboolean( L, 0 );
	lua_setfield( L, -2, "__metatable" );
	lua_pop( L, 1 );
}
