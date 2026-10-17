/*
===========================================================================
GLX_U: [Lua] API version 6: NPC orders. A plugin's own NPCs (World.npc) can be told to follow, cover or
guard a player, to walk or run to a spot, to attack a player or an NPC, and to stop.

Follow, cover and guard are the game's own follower orders -- what a player's Use key and /order follow,
/order cover and /order guard set (leader, the two order bits, the behaviour) -- with the script choosing the
player. The game's follower code then does the rest, releases included: a leader who disconnects, goes
spectator or changes team loses their followers as before (g_main.c), and the script hears "leader_left".

go_to puts the NPC's own move goal (tempGoal, as ICARUS set_navgoal does) on the spot and runs it in the
cutscene behaviour, BS_CINEMATIC, which every class's AI hands over to before its own (NPC_RunBehavior): it
goes straight there, does not stop to fight, and goes back to what it was doing when the order ends. On a map
without NPC waypoints it walks in a straight line, so a wall stops it: "stuck" after RP_ORD_STUCK_MS without
moving RP_ORD_STUCK_DIST units.

attack makes the target the NPC's enemy (G_SetEnemy) and keeps it so while the NPC has no other.

One order per NPC. The script's function hears once how it ended -- from the frame (RP_LuaOrders_Frame), so
everything is allowed in it. A new order from the script, or stop(), undoes what the last one set before it
takes over; a player's Use key or /order taking the NPC over (RP_Lua_NpcOrderTaken, called from game code)
leaves what they set alone. "replaced" and "stopped" are heard next frame (GLX_U: also when the script itself
replaced or stopped the order -- called inside follow(), go_to(), attack() or stop(), the function could free
the NPC or give it another order while that method was still setting it up). When the plugin is switched off
or the NPC is freed, nothing is called.
===========================================================================
*/

#include "rp_lua_local.h"

extern void G_ClearEnemy( gentity_t *self );
extern void G_SetEnemy( gentity_t *self, gentity_t *enemy );
extern void NPC_SetMoveGoal( gentity_t *ent, vec3_t point, int radius, qboolean isNavGoal, int combatPoint, gentity_t *targetEnt );
extern void NPC_Begin( gentity_t *ent );
extern void zyk_release_npc_from_leader( gentity_t *npc_ent );

#define RP_ORD_STUCK_MS			5000
#define RP_ORD_STUCK_DIST		16
#define RP_ORD_RADIUS_DEFAULT	48
#define RP_ORD_RADIUS_MIN		16
#define RP_ORD_RADIUS_MAX		512
#define RP_ORD_TIMEOUT_DEFAULT	60
#define RP_ORD_TIMEOUT_MIN		5
#define RP_ORD_TIMEOUT_MAX		600
#define RP_ORD_ARRIVE_SLACK		16		// on top of the radius: the NPC's own box
#define RP_ORD_ARRIVE_HEIGHT	64		// how far above or below the spot still counts
#define RP_ORD_MAX_NOTES		1024	// order ends heard next frame, per frame; past that they are dropped (a warning)

typedef enum { ORD_NONE, ORD_FOLLOW, ORD_COVER, ORD_GUARD, ORD_GOTO, ORD_ATTACK } rpLuaOrderKind_t;

static const char *rpOrderNames[] = { NULL, "follow", "cover", "guard", "go_to", "attack" };

typedef struct {
	int			kind;			// rpLuaOrderKind_t; ORD_NONE: no order
	int			gen;			// the NPC's g_entityGen[]
	int			plugin;
	int			ref;			// the script's function, 0: none
	int			target;			// the leader or the one to attack
	int			targetGen;
	vec3_t		spot;			// go_to
	int			radius;
	int			ends;
	vec3_t		lastPos;		// where it was when it last moved RP_ORD_STUCK_DIST
	int			lastMove;
	int			prevTempBehavior;
	int			prevScriptFlags;	// its SCF_RUNNING and SCF_WALKING before go_to
} rpLuaOrder_t;

// an order taken over inside game code: its function is heard in the next frame
typedef struct {
	int			plugin;
	int			ref;
	int			num;
	int			gen;
	const char	*why;
} rpLuaOrderNote_t;

static rpLuaOrder_t		rpOrders[MAX_GENTITIES];
static rpLuaOrderNote_t	rpOrderNotes[RP_ORD_MAX_NOTES];
static int				rpNumOrderNotes;
static rpLuaOrderNote_t	rpOrderNotesRun[RP_ORD_MAX_NOTES];	// the ones being called (RP_LuaOrders_Frame)
static int				rpOrderNotesDropped;

/*
================
Ending an order
================
*/
typedef struct {
	int			ref;
	int			num;
	qboolean	done;
	const char	*why;
} rpLuaOrderCall_t;

static int RP_LuaOrders_Worker( lua_State *L, void *data ) {
	rpLuaOrderCall_t *c = (rpLuaOrderCall_t *)data;

	lua_rawgeti( L, LUA_REGISTRYINDEX, c->ref );
	RP_LuaWorld_PushEntity( L, &g_entities[c->num] );
	lua_pushboolean( L, c->done );
	lua_pushstring( L, c->why );
	lua_call( L, 3, 0 );
	return 0;
}

static void RP_LuaOrders_Unref( int ref ) {
	if ( ref > 0 && rl.L ) {
		luaL_unref( rl.L, LUA_REGISTRYINDEX, ref );
	}
}

// the function hears it, unless the plugin no longer runs or the NPC has gone
static void RP_LuaOrders_Notify( int plugin, int ref, int num, int gen, const char *why ) {
	if ( ref <= 0 ) {
		return;
	}
	if ( rl.L && RP_Lua_PluginRunning( plugin ) && g_entities[num].inuse && g_entityGen[num] == gen ) {
		rpLuaOrderCall_t c;
		c.ref = ref;
		c.num = num;
		c.done = !strcmp( why, "arrived" ) ? qtrue : qfalse;
		c.why = why;
		RP_Lua_Call( plugin, RP_LuaOrders_Worker, &c );
	}
	RP_LuaOrders_Unref( ref );
}

// what the order set on the NPC goes. go_to's walk always ends; a leader or an enemy is only taken off when the
// script itself ends the order (a new order, stop()), never when a player or the game has just set another
static void RP_LuaOrders_Undo( gentity_t *npc, const rpLuaOrder_t *o, qboolean all ) {
	gentity_t *target = &g_entities[o->target];

	switch ( o->kind ) {
	case ORD_GOTO:
		if ( npc->NPC->tempBehavior == BS_CINEMATIC ) {
			npc->NPC->tempBehavior = o->prevTempBehavior;
		}
		npc->NPC->scriptFlags = ( npc->NPC->scriptFlags & ~( SCF_RUNNING | SCF_WALKING ) ) | o->prevScriptFlags;
		if ( npc->NPC->tempGoal && npc->NPC->goalEntity == npc->NPC->tempGoal ) {
			npc->NPC->goalEntity = NULL;
		}
		break;
	case ORD_ATTACK:
		if ( all && npc->enemy == target ) {
			G_ClearEnemy( npc );
		}
		break;
	case ORD_FOLLOW:
	case ORD_COVER:
	case ORD_GUARD:
		if ( all && npc->client->leader == target ) {
			zyk_release_npc_from_leader( npc );
		}
		break;
	}
}

// an order's function to call at the start of the next frame (RP_LuaOrders_Frame)
static void RP_LuaOrders_Later( int plugin, int ref, int num, int gen, const char *why ) {
	rpLuaOrderNote_t *n;

	if ( ref <= 0 ) {
		return;
	}
	if ( rpNumOrderNotes >= RP_ORD_MAX_NOTES ) {
		RP_LuaOrders_Unref( ref );	// more than RP_ORD_MAX_NOTES in one frame: not heard (a warning next frame)
		rpOrderNotesDropped++;
		return;
	}
	n = &rpOrderNotes[rpNumOrderNotes++];
	n->plugin = plugin;
	n->ref = ref;
	n->num = num;
	n->gen = gen;
	n->why = why;
}

// takes num's order off: undone (all: everything it set), then its function hears why -- now, or in the next
// frame (later): when this runs inside game code, or inside a script's own order method
static void RP_LuaOrders_End( int num, const char *why, qboolean all, qboolean later ) {
	rpLuaOrder_t o = rpOrders[num];
	gentity_t *npc = &g_entities[num];

	if ( o.kind == ORD_NONE ) {
		return;
	}
	memset( &rpOrders[num], 0, sizeof( rpOrders[num] ) );
	if ( npc->inuse && g_entityGen[num] == o.gen && npc->client && npc->NPC ) {
		RP_LuaOrders_Undo( npc, &o, all );
	}
	if ( !later ) {
		RP_LuaOrders_Notify( o.plugin, o.ref, num, o.gen, why );
	} else {
		RP_LuaOrders_Later( o.plugin, o.ref, num, o.gen, why );
	}
}

// game code (TryUse, Cmd_Order_f): a player is taking the NPC over -- the script's order ends, what the player
// sets stays
void RP_Lua_NpcOrderTaken( gentity_t *npc ) {
	int num;

	if ( !npc ) {
		return;
	}
	num = npc->s.number;
	if ( num < MAX_CLIENTS || num >= MAX_GENTITIES || rpOrders[num].kind == ORD_NONE ) {
		return;
	}
	RP_LuaOrders_End( num, "replaced", qfalse, qtrue );
}

/*
================
Each frame
================
*/
static qboolean RP_LuaOrders_PlayerGone( const gentity_t *ent ) {
	return ( !ent->inuse || !ent->client || ent->client->pers.connected != CON_CONNECTED
		|| ent->client->sess.sessionTeam == TEAM_SPECTATOR || ent->client->tempSpectate >= level.time ) ? qtrue : qfalse;
}

// a player or NPC that has gone: freed, slot reused, disconnected, spectating
static qboolean RP_LuaOrders_TargetGone( const rpLuaOrder_t *o ) {
	const gentity_t *t = &g_entities[o->target];

	if ( !t->inuse || g_entityGen[o->target] != o->targetGen || !t->client ) {
		return qtrue;
	}
	return ( o->target < MAX_CLIENTS ) ? RP_LuaOrders_PlayerGone( t ) : qfalse;
}

// dead, or down: a player the Death System knocked down counts
static qboolean RP_LuaOrders_TargetDown( gentity_t *t ) {
	if ( t->s.number < MAX_CLIENTS ) {
		return ( t->health <= 0 || ( t->client->ps.eFlags & EF_DEAD ) || t->client->ps.pm_type == PM_DEAD || G_PlayerIsDowned( t ) ) ? qtrue : qfalse;
	}
	return !RP_LuaWorld_NpcIsAlive( t );
}

static void RP_LuaOrders_SetGoal( gentity_t *npc, rpLuaOrder_t *o ) {
	NPC_SetMoveGoal( npc, o->spot, o->radius, qfalse, -1, NULL );
	npc->NPC->goalEntity = npc->NPC->tempGoal;
	npc->NPC->goalRadius = o->radius;
	npc->NPC->aiFlags &= ~NPCAI_TOUCHED_GOAL;
}

// go_to: NULL while on its way
static const char *RP_LuaOrders_GoToCheck( gentity_t *npc, rpLuaOrder_t *o ) {
	const float *at = npc->r.currentOrigin;
	float dx = at[0] - o->spot[0], dy = at[1] - o->spot[1], flat = sqrtf( dx * dx + dy * dy );
	qboolean goalCleared = ( npc->NPC->goalEntity != npc->NPC->tempGoal ) ? qtrue : qfalse;

	if ( fabsf( at[2] - o->spot[2] ) <= RP_ORD_ARRIVE_HEIGHT
		&& ( flat <= o->radius + RP_ORD_ARRIVE_SLACK || ( goalCleared && flat <= o->radius + 3 * RP_ORD_ARRIVE_SLACK ) ) ) {
		return "arrived";	// close enough, or the game's own ReachedGoal() said so a little further off
	}
	if ( level.time >= o->ends ) {
		return "timeout";
	}
	if ( Distance( at, o->lastPos ) >= RP_ORD_STUCK_DIST ) {
		VectorCopy( at, o->lastPos );
		o->lastMove = level.time;
	} else if ( level.time - o->lastMove >= RP_ORD_STUCK_MS ) {
		return "stuck";
	}
	if ( goalCleared ) {
		RP_LuaOrders_SetGoal( npc, o );		// something took its goal away: it carries on
	}
	return NULL;
}

void RP_LuaOrders_Frame( void ) {
	int num, i, n = rpNumOrderNotes;

	// GLX_U: first what ended last frame. Copied out before anything is called: a function that gives an order
	// queues new notes (heard next frame), which must not overwrite the ones still to be called.
	if ( rpOrderNotesDropped ) {
		G_LogPrintf( "Lua: more than %d NPC order ends in one frame; %d were not called\n", RP_ORD_MAX_NOTES, rpOrderNotesDropped );
		RP_Lua_AdminNotice( va( "^3Lua:^7 more than %d NPC order ends in one frame; %d were not called", RP_ORD_MAX_NOTES, rpOrderNotesDropped ) );
		rpOrderNotesDropped = 0;
	}
	memcpy( rpOrderNotesRun, rpOrderNotes, n * sizeof( rpOrderNotes[0] ) );
	rpNumOrderNotes = 0;
	for ( i = 0; i < n; i++ ) {
		RP_LuaOrders_Notify( rpOrderNotesRun[i].plugin, rpOrderNotesRun[i].ref, rpOrderNotesRun[i].num, rpOrderNotesRun[i].gen, rpOrderNotesRun[i].why );
	}

	for ( num = MAX_CLIENTS; num < MAX_GENTITIES; num++ ) {
		rpLuaOrder_t *o = &rpOrders[num];
		gentity_t *npc = &g_entities[num];
		const char *why = NULL;
		qboolean all = qtrue;

		if ( o->kind == ORD_NONE ) {
			continue;
		}
		if ( !npc->inuse || g_entityGen[num] != o->gen || !npc->client || !npc->NPC || !RP_Lua_PluginRunning( o->plugin ) ) {
			RP_LuaOrders_Unref( o->ref );	// freed, or its plugin gone: nothing is called
			memset( o, 0, sizeof( *o ) );
			continue;
		}
		if ( !RP_LuaWorld_NpcIsAlive( npc ) ) {
			why = "died";
			all = qfalse;
		} else if ( RP_NpcIsFrozen( npc ) && ( o->kind == ORD_GOTO || o->kind == ORD_ATTACK ) ) {
			why = "frozen";
		} else if ( o->kind == ORD_GOTO ) {
			why = RP_LuaOrders_GoToCheck( npc, o );
		} else if ( o->kind == ORD_ATTACK ) {
			if ( RP_LuaOrders_TargetGone( o ) ) {
				why = "target_left";
			} else if ( RP_LuaOrders_TargetDown( &g_entities[o->target] ) ) {
				why = "killed";
			} else if ( !npc->enemy && !RP_NpcIsFrozen( npc ) ) {
				G_SetEnemy( npc, &g_entities[o->target] );	// lost sight of it: back after it
			}
		} else if ( npc->client->leader != &g_entities[o->target] ) {
			// the game released it (its leader went), or someone else took it (a mind trick, ICARUS)
			why = ( !npc->client->leader || RP_LuaOrders_TargetGone( o ) ) ? "leader_left" : "replaced";
			all = qfalse;
		} else if ( RP_LuaOrders_TargetGone( o ) ) {
			why = "leader_left";
		}
		if ( !why ) {
			continue;
		}
		if ( !RP_Lua_FrameBudgetLeft() ) {
			RP_Lua_NoteDeferred( o->plugin, 1 );
			return;		// the next frame ends it
		}
		RP_LuaOrders_End( num, why, all, qfalse );
	}
}

void RP_LuaOrders_RemovePlugin( int plugin ) {
	int num;

	for ( num = MAX_CLIENTS; num < MAX_GENTITIES; num++ ) {
		if ( rpOrders[num].kind != ORD_NONE && rpOrders[num].plugin == plugin ) {
			RP_LuaOrders_Unref( rpOrders[num].ref );
			memset( &rpOrders[num], 0, sizeof( rpOrders[num] ) );
		}
	}
	for ( num = 0; num < rpNumOrderNotes; num++ ) {
		if ( rpOrderNotes[num].plugin == plugin ) {
			RP_LuaOrders_Unref( rpOrderNotes[num].ref );
			rpOrderNotes[num].ref = 0;
		}
	}
}

void RP_LuaOrders_Reset( void ) {
	memset( rpOrders, 0, sizeof( rpOrders ) );
	rpNumOrderNotes = 0;
	rpOrderNotesDropped = 0;
}

/*
================
Giving orders
================
*/
// the refusals every order shares; NULL when the NPC can take one
static const char *RP_LuaOrders_NpcRefusal( gentity_t *npc ) {
	if ( npc->think == NPC_Begin ) {
		return "it has not finished spawning (try again next frame)";
	}
	if ( !RP_LuaWorld_NpcIsAlive( npc ) ) {
		return "it is dead";
	}
	if ( RP_NpcIsFrozen( npc ) ) {
		return "it is frozen";
	}
	// zyk_npc_can_take_orders()'s classes: their AI never reads an order
	if ( npc->client->NPC_class == CLASS_VEHICLE || npc->s.NPC_class == CLASS_VEHICLE || npc->client->NPC_class == CLASS_WAMPA
		|| npc->client->NPC_class == CLASS_REMOTE ) {
		return "it cannot take orders (vehicles, wampas and remotes)";
	}
	if ( npc->client->ps.m_iVehicleNum ) {
		return "it is riding a vehicle";
	}
	if ( npc->NPC->charmedTime > level.time ) {
		return "it is under a mind trick";
	}
	return NULL;
}

static int RP_LuaOrders_Fail( lua_State *L, const char *why ) {
	lua_pushboolean( L, 0 );
	lua_pushstring( L, why );
	return 2;
}

// the optional function at idx
static int RP_LuaOrders_TakeFn( lua_State *L, int idx ) {
	if ( lua_isnoneornil( L, idx ) ) {
		return 0;
	}
	luaL_checktype( L, idx, LUA_TFUNCTION );
	lua_pushvalue( L, idx );
	return luaL_ref( L, LUA_REGISTRYINDEX );
}

static void RP_LuaOrders_Log( lua_State *L, gentity_t *npc, const char *what, gentity_t *target ) {
	G_LogPrintf( "Lua: %s ordered NPC %d (%s) to %s%s%s\n", rl.plugins[RP_Lua_CurrentPlugin( L )].name, npc->s.number,
		npc->NPC_type ? npc->NPC_type : "?", what, target ? " " : "", !target ? "" : ( target->client && target->s.number < MAX_CLIENTS )
		? target->client->pers.netname : va( "NPC %d (%s)", target->s.number, target->NPC_type ? target->NPC_type : "?" ) );
}

// installs the new order; the last one is undone first and its function hears "replaced" next frame. GLX_U:
// not now -- called here, inside the method giving the new order, it could free the NPC or the target, stop
// the NPC or give it yet another order while the method is still setting it up (a freed NPC crashed the server).
// Undone only when it is this NPC's own order: a slot freed and reused before the frame cleared its record
// keeps the old NPC's order, which is not the new NPC's to undo.
static void RP_LuaOrders_Install( gentity_t *npc, const rpLuaOrder_t *o ) {
	int num = npc->s.number;
	rpLuaOrder_t old = rpOrders[num];

	if ( old.kind != ORD_NONE && old.gen == g_entityGen[num] ) {
		RP_LuaOrders_Undo( npc, &old, qtrue );
	}
	rpOrders[num] = *o;
	if ( old.kind != ORD_NONE ) {
		RP_LuaOrders_Later( old.plugin, old.ref, num, old.gen, "replaced" );
	}
}

static void RP_LuaOrders_Base( lua_State *L, gentity_t *npc, rpLuaOrder_t *o, int kind ) {
	memset( o, 0, sizeof( *o ) );
	o->kind = kind;
	o->gen = g_entityGen[npc->s.number];
	o->plugin = RP_Lua_CurrentPlugin( L );
}

// follow, cover and guard: the game's own follower order, with the script choosing the player
static int RP_LuaOrders_Follower( lua_State *L, int kind ) {
	gentity_t *npc = RP_LuaWorld_OwnNpcArg( L, rpOrderNames[kind] );
	int client = RP_LuaApi_CheckPlayer( L, 2 );
	gentity_t *player = &g_entities[client];
	const char *why = RP_LuaOrders_NpcRefusal( npc );
	rpLuaOrder_t o;

	if ( lua_gettop( L ) >= 3 && !lua_isnil( L, 3 ) ) {
		luaL_checktype( L, 3, LUA_TFUNCTION );
	}
	if ( why ) {
		return RP_LuaOrders_Fail( L, why );
	}
	if ( RP_LuaOrders_PlayerGone( player ) ) {
		return RP_LuaOrders_Fail( L, "they are spectating" );
	}
	if ( player->health <= 0 || ( player->client->ps.eFlags & EF_DEAD ) || player->client->ps.pm_type == PM_DEAD ) {
		return RP_LuaOrders_Fail( L, "they are not alive" );
	}
	RP_LuaOrders_Base( L, npc, &o, kind );
	o.target = client;
	o.targetGen = g_entityGen[client];
	o.ref = RP_LuaOrders_TakeFn( L, 3 );

	if ( npc->client->leader && npc->client->leader != player ) {
		zyk_release_npc_from_leader( npc );		// someone else's follower (the Use key): theirs no more
	}
	RP_LuaOrders_Install( npc, &o );
	npc->client->pers.player_statuses &= ~( ( 1 << PLAYER_STATUS_NPC_ORDER_GUARD ) | ( 1 << PLAYER_STATUS_NPC_ORDER_COVER ) );
	npc->client->leader = player;
	if ( kind == ORD_GUARD ) {
		npc->NPC->tempBehavior = BS_STAND_GUARD;
		npc->client->pers.player_statuses |= ( 1 << PLAYER_STATUS_NPC_ORDER_GUARD );
	} else {
		npc->NPC->tempBehavior = BS_FOLLOW_LEADER;
		if ( kind == ORD_COVER ) {
			npc->client->pers.player_statuses |= ( 1 << PLAYER_STATUS_NPC_ORDER_COVER );
		}
	}
	RP_LuaOrders_Log( L, npc, rpOrderNames[kind], player );
	lua_pushboolean( L, 1 );
	return 1;
}

static int RP_LuaOrders_Follow( lua_State *L ) {
	return RP_LuaOrders_Follower( L, ORD_FOLLOW );
}

static int RP_LuaOrders_Cover( lua_State *L ) {
	return RP_LuaOrders_Follower( L, ORD_COVER );
}

static int RP_LuaOrders_Guard( lua_State *L ) {
	return RP_LuaOrders_Follower( L, ORD_GUARD );
}

static int RP_LuaOrders_IntOpt( lua_State *L, int t, const char *field, int def, int lo, int hi ) {
	lua_Integer v;
	int isnum;

	if ( !t || lua_getfield( L, t, field ) == LUA_TNIL ) {
		if ( t ) {
			lua_pop( L, 1 );
		}
		return def;
	}
	v = lua_tointegerx( L, -1, &isnum );
	if ( !isnum || v < lo || v > hi ) {
		return luaL_error( L, "go_to: %s must be a whole number from %d to %d", field, lo, hi );
	}
	lua_pop( L, 1 );
	return (int)v;
}

// go_to(pos [, { run =, radius =, timeout = }] [, fn])
static int RP_LuaOrders_GoTo( lua_State *L ) {
	gentity_t *npc = RP_LuaWorld_OwnNpcArg( L, "go_to" );
	int t = 0, fnIdx = 3, run = 0;
	const char *why;
	rpLuaOrder_t o;
	vec3_t spot;

	RP_LuaWorld_CheckVec( L, 2, spot, "the spot to go to" );
	if ( lua_istable( L, 3 ) ) {
		t = 3;
		fnIdx = 4;
		if ( lua_getfield( L, 3, "run" ) != LUA_TNIL ) {
			if ( !lua_isboolean( L, -1 ) ) {
				return luaL_error( L, "go_to: run must be true or false" );
			}
			run = lua_toboolean( L, -1 );
		}
		lua_pop( L, 1 );
	} else if ( !lua_isnoneornil( L, 3 ) && !lua_isfunction( L, 3 ) ) {
		return luaL_error( L, "go_to: the third argument is the options table or the function" );
	}
	if ( !lua_isnoneornil( L, fnIdx ) ) {
		luaL_checktype( L, fnIdx, LUA_TFUNCTION );
	}
	RP_LuaOrders_Base( L, npc, &o, ORD_GOTO );
	o.radius = RP_LuaOrders_IntOpt( L, t, "radius", RP_ORD_RADIUS_DEFAULT, RP_ORD_RADIUS_MIN, RP_ORD_RADIUS_MAX );
	o.ends = level.time + RP_LuaOrders_IntOpt( L, t, "timeout", RP_ORD_TIMEOUT_DEFAULT, RP_ORD_TIMEOUT_MIN, RP_ORD_TIMEOUT_MAX ) * 1000;

	why = RP_LuaOrders_NpcRefusal( npc );
	if ( !why && !npc->NPC->tempGoal ) {
		why = "it has no move goal (it is still spawning)";
	}
	if ( why ) {
		return RP_LuaOrders_Fail( L, why );
	}
	VectorCopy( spot, o.spot );
	VectorCopy( npc->r.currentOrigin, o.lastPos );
	o.lastMove = level.time;
	o.ref = RP_LuaOrders_TakeFn( L, fnIdx );

	if ( npc->client->leader ) {
		zyk_release_npc_from_leader( npc );		// a follower going somewhere is no longer following
	}
	RP_LuaOrders_Install( npc, &o );
	// what it was doing before (after the last order was undone), to go back to
	rpOrders[npc->s.number].prevTempBehavior = npc->NPC->tempBehavior;
	rpOrders[npc->s.number].prevScriptFlags = npc->NPC->scriptFlags & ( SCF_RUNNING | SCF_WALKING );
	npc->NPC->scriptFlags = ( npc->NPC->scriptFlags & ~( SCF_RUNNING | SCF_WALKING ) ) | ( run ? SCF_RUNNING : SCF_WALKING );
	npc->NPC->tempBehavior = BS_CINEMATIC;
	RP_LuaOrders_SetGoal( npc, &rpOrders[npc->s.number] );
	G_LogPrintf( "Lua: %s ordered NPC %d (%s) to go to (%d %d %d)\n", rl.plugins[o.plugin].name, npc->s.number,
		npc->NPC_type ? npc->NPC_type : "?", (int)spot[0], (int)spot[1], (int)spot[2] );
	lua_pushboolean( L, 1 );
	return 1;
}

// attack(target [, fn]): a player's or an NPC's handle
static int RP_LuaOrders_Attack( lua_State *L ) {
	gentity_t *npc = RP_LuaWorld_OwnNpcArg( L, "attack" );
	gentity_t *target;
	const char *why;
	rpLuaOrder_t o;

	if ( lua_isnoneornil( L, 2 ) ) {
		return luaL_error( L, "attack: the target must be a player or an NPC" );
	}
	target = RP_LuaWorld_OptActorArg( L, 2 );
	if ( !target->client ) {
		return luaL_error( L, "attack: the target must be a player or an NPC" );
	}
	if ( lua_gettop( L ) >= 3 && !lua_isnil( L, 3 ) ) {
		luaL_checktype( L, 3, LUA_TFUNCTION );
	}
	why = RP_LuaOrders_NpcRefusal( npc );
	if ( why ) {
		return RP_LuaOrders_Fail( L, why );
	}
	if ( target == npc ) {
		return RP_LuaOrders_Fail( L, "it cannot attack itself" );
	}
	if ( target->s.number < MAX_CLIENTS ) {
		if ( RP_LuaOrders_PlayerGone( target ) ) {
			return RP_LuaOrders_Fail( L, "the target is spectating" );
		}
		if ( RP_LuaPeople_IsProtected( target ) ) {
			return RP_LuaOrders_Fail( L, "the target is admin-protected" );
		}
	} else if ( !target->NPC ) {
		return luaL_error( L, "attack: the target must be a player or an NPC" );
	}
	if ( RP_LuaOrders_TargetDown( target ) ) {
		return RP_LuaOrders_Fail( L, "the target is not alive" );
	}
	if ( npc->client->playerTeam == target->client->playerTeam && npc->client->playerTeam != NPCTEAM_FREE ) {
		return RP_LuaOrders_Fail( L, "it is on the target's side (World.npc's team option puts it on another)" );
	}
	RP_LuaOrders_Base( L, npc, &o, ORD_ATTACK );
	o.target = target->s.number;
	o.targetGen = g_entityGen[target->s.number];
	o.ref = RP_LuaOrders_TakeFn( L, 3 );

	if ( npc->client->leader ) {
		zyk_release_npc_from_leader( npc );
	}
	RP_LuaOrders_Install( npc, &o );
	G_SetEnemy( npc, target );
	RP_LuaOrders_Log( L, npc, "attack", target );
	lua_pushboolean( L, 1 );
	return 1;
}

// stop(): no order, no leader, standing where it is
static int RP_LuaOrders_Stop( lua_State *L ) {
	gentity_t *npc = RP_LuaWorld_OwnNpcArg( L, "stop" );
	int num = npc->s.number;

	if ( rpOrders[num].kind != ORD_NONE ) {
		RP_LuaOrders_End( num, "stopped", qtrue, qtrue );	// GLX_U: heard next frame, as "replaced"
	}
	if ( npc->inuse && npc->client && npc->client->leader && npc->NPC ) {
		zyk_release_npc_from_leader( npc );		// a follower the Use key or the game gave it
	}
	lua_pushboolean( L, 1 );
	return 1;
}

/*
================
Handle fields and methods
================
*/
lua_CFunction RP_LuaOrders_Method( const char *key ) {
	static const luaL_Reg methods[] = {
		{ "follow", RP_LuaOrders_Follow }, { "cover", RP_LuaOrders_Cover }, { "guard", RP_LuaOrders_Guard },
		{ "go_to", RP_LuaOrders_GoTo }, { "attack", RP_LuaOrders_Attack }, { "stop", RP_LuaOrders_Stop }, { NULL, NULL } };
	int i;

	for ( i = 0; methods[i].name; i++ ) {
		if ( !strcmp( key, methods[i].name ) ) {
			return methods[i].func;
		}
	}
	return NULL;
}

// order and leader, on any NPC: 1 when key was one of them (its value pushed)
int RP_LuaOrders_Field( lua_State *L, gentity_t *ent, const char *key ) {
	qboolean isNpc = ( ent->NPC && ent->client ) ? qtrue : qfalse;
	int num = ent->s.number;

	if ( !strcmp( key, "order" ) ) {
		if ( !isNpc ) {
			lua_pushnil( L );
		} else if ( num >= MAX_CLIENTS && num < MAX_GENTITIES && rpOrders[num].kind != ORD_NONE && rpOrders[num].gen == g_entityGen[num] ) {
			lua_pushstring( L, rpOrderNames[rpOrders[num].kind] );
		} else if ( ent->client->leader ) {	// the Use key's and /order's
			int st = ent->client->pers.player_statuses;
			lua_pushstring( L, ( st & ( 1 << PLAYER_STATUS_NPC_ORDER_GUARD ) ) ? "guard" : ( st & ( 1 << PLAYER_STATUS_NPC_ORDER_COVER ) ) ? "cover" : "follow" );
		} else {
			lua_pushnil( L );
		}
		return 1;
	}
	if ( !strcmp( key, "leader" ) ) {
		RP_LuaWorld_PushActor( L, isNpc ? ent->client->leader : NULL );
		return 1;
	}
	return 0;
}
