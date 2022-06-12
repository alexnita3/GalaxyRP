#include "g_local.h"
#include "g_lua.h"

//
// Game.AngleVectors(angles:QVector)
//
static int lua_Game_AngleVectors(lua_State * L)
{
	vec_t *angles;
	vec3_t forward, right, up;

	angles = lua_getvector(L, 1);

	luaL_argcheck(L, angles != NULL, 1, "`QVector' expected");

	AngleVectors(angles, forward, right, up);

	// create a new table
	lua_newtable(L);

	// forward array
	lua_pushstring(L, "forward");
	lua_pushvector(L, forward);
	lua_settable(L, -3);

	// right array
	lua_pushstring(L, "right");
	lua_pushvector(L, right);
	lua_settable(L, -3);

	// up array
	lua_pushstring(L, "up");
	lua_pushvector(L, up);
	lua_settable(L, -3);
	return 1;
}

//
// Game.Argument(index:Number)
//
static int lua_Game_Argument(lua_State * L)
{
	int arg;
	char result[256];

	arg = luaL_optinteger(L, 1, 0);
	trap_Argv(arg, result, sizeof(result));

	lua_pushstring(L, result);
	return 1;
}

//
// Game.BroadcastChat(message:String)
//
static int lua_Game_BroadcastChat(lua_State * L)
{
	int             i;
	char            buf[1000];
	int             n = lua_gettop(L);

	memset(buf, 0, sizeof(buf));

	lua_rawgeti(L, LUA_REGISTRYINDEX, lua_toString);
	for(i = 1; i <= n; i++)
	{
		const char     *s;

		lua_pushvalue(L, -1);	// function to be called
		lua_pushvalue(L, i);	// value to print
		lua_call(L, 1, 1);
		s = lua_tostring(L, -1);	// get result

		if(s == NULL)
			return luaL_error(L, "`tostring' must return a string to `print'");

		Q_strcat(buf, sizeof(buf), s);

		lua_pop(L, 1);			// pop result
	}

	trap_SendServerCommand(-1, va("chat \"%s\"", buf));
	return 0;
}

//
// Game.BroadcastConsole(message:String)
//
static int lua_Game_BroadcastConsole(lua_State * L)
{
	int             i;
	char            buf[1000];
	int             n = lua_gettop(L);

	memset(buf, 0, sizeof(buf));

	lua_rawgeti(L, LUA_REGISTRYINDEX, lua_toString);
	for(i = 1; i <= n; i++)
	{
		const char     *s;

		lua_pushvalue(L, -1);	// function to be called
		lua_pushvalue(L, i);	// value to print
		lua_call(L, 1, 1);
		s = lua_tostring(L, -1);	// get result

		if(s == NULL)
			return luaL_error(L, "`tostring' must return a string to `print'");

		Q_strcat(buf, sizeof(buf), s);

		lua_pop(L, 1);			// pop result
	}

	trap_SendServerCommand(-1, va("print \"%s\"", buf));
	return 0;
}

//
// Game.BroadcastScreen(message:String)
//
static int lua_Game_BroadcastScreen(lua_State * L)
{
	int             i;
	char            buf[1000];
	int             n = lua_gettop(L);

	memset(buf, 0, sizeof(buf));

	lua_rawgeti(L, LUA_REGISTRYINDEX, lua_toString);
	for(i = 1; i <= n; i++)
	{
		const char     *s;

		lua_pushvalue(L, -1);	// function to be called
		lua_pushvalue(L, i);	// value to print
		lua_call(L, 1, 1);
		s = lua_tostring(L, -1);	// get result

		if(s == NULL)
			return luaL_error(L, "`tostring' must return a string to `print'");

		Q_strcat(buf, sizeof(buf), s);

		lua_pop(L, 1);			// pop result
	}

	trap_SendServerCommand(-1, va("cp \"%s\"", buf));
	return 0;
}

//
// Game.BindCommand(name:String, command:Function)
//
static int lua_Game_BindCommand(lua_State * L)
{
	int n = lua_gettop(L), cmdref = LUA_REFNIL;
	int i;
	char *name;

	if (n < 2)
		return luaL_error(L, "syntax: BindCommand(name:String, command:Function)");

	// get the first argument, must be a string
	name = (char*)luaL_checkstring(L, 1);

	// check if the first argument is a string
	luaL_argcheck(L, name != NULL, 1, "`string' expected");

	// check if the second argument is a function or a nil value
	if (!lua_isfunction(L, 2) && !lua_isnil(L, 2))
		return luaL_error(L, "second argument is not a function or a nil value");

	for (i = 0; i < MAX_LUA_CMDS; i++)
	{
		if (st_lua_cmds[i].function && !Q_stricmp(st_lua_cmds[i].name, name) && lua_isnil(L, 2))
		{
			luaL_unref(L, LUA_REGISTRYINDEX, st_lua_cmds[i].function);
			memset(&st_lua_cmds[i], 0, sizeof(st_lua_cmd_t));
			st_lua_cmds[i].name = 0;
			st_lua_cmds[i].function = 0;
			return 0;
		}
	}

	if (!lua_isfunction(L, 2))
		return luaL_error(L, "second argument must be a function");

	// push the value of the second argument and register it
	// inside the registry
	lua_pushvalue(L, 2);
	cmdref = luaL_ref(L, LUA_REGISTRYINDEX);

	for (i = 0; i < MAX_LUA_CMDS; i++)
	{
		if (!st_lua_cmds[i].function)
		{
			st_lua_cmds[i].name = G_NewString(name);
			st_lua_cmds[i].function = cmdref;
			return 0;
		}
	}
	return 0;
}

//
// Game.BindSvCommand(name:String, command:Function)
//
static int lua_Game_BindSvCommand(lua_State * L)
{
	int n = lua_gettop(L), cmdref = LUA_REFNIL;
	int i;
	char *name;

	if (n < 2)
		return luaL_error(L, "syntax: BindSvCommand(name:String, command:Function)");

	// get the first argument, must be a string
	name = (char*)luaL_checkstring(L, 1);

	// check if the first argument is a string
	luaL_argcheck(L, name != NULL, 1, "`string' expected");

	// check if the second argument is a function or a nil value
	if (!lua_isfunction(L, 2) && !lua_isnil(L, 2))
		return luaL_error(L, "second argument is not a function or a nil value");

	for (i = 0; i < MAX_LUA_SVCMDS; i++)
	{
		if (st_lua_svcmds[i].function && !Q_stricmp(st_lua_svcmds[i].name, name) && lua_isnil(L, 2))
		{
			luaL_unref(L, LUA_REGISTRYINDEX, st_lua_svcmds[i].function);
			memset(&st_lua_svcmds[i], 0, sizeof(st_lua_cmd_t));
			st_lua_svcmds[i].name = 0;
			st_lua_svcmds[i].function = 0;
			return 0;
		}
	}

	if (!lua_isfunction(L, 2))
		return luaL_error(L, "second argument must be a function");

	// push the value of the second argument and register it
	// inside the registry
	lua_pushvalue(L, 2);
	cmdref = luaL_ref(L, LUA_REGISTRYINDEX);

	for (i = 0; i < MAX_LUA_SVCMDS; i++)
	{
		if (!st_lua_svcmds[i].function)
		{
			st_lua_svcmds[i].name = G_NewString(name);
			st_lua_svcmds[i].function = cmdref;
			return 0;
		}
	}

	return 0;
}

//
// Game.BindEvent(name:String, command:Function)
//
static int lua_Game_BindEvent(lua_State * L)
{
	int n = lua_gettop(L), evtref = LUA_REFNIL;
	char *name;
	int i;

	if (n < 2)
		return luaL_error(L, "syntax: BindEvent(name:String, command:Function)");
	
	// get the first argument, must be a string
	name = (char*)luaL_checkstring(L, 1);

	// check if the first argument is a string
	luaL_argcheck(L, name != NULL, 1, "`string' expected");

	// check if the second argument is a function or a nil value
	if (!lua_isfunction(L, 2))
		return luaL_error(L, "second argument must be a function");

	// push the value of the second argument and register it
	// inside the registry
	lua_pushvalue(L, 2);
	evtref = luaL_ref(L, LUA_REGISTRYINDEX);

	for (i = 0; i < MAX_LUA_EVTS; i++)
	{
		// find if the function has already been registered
		if (st_lua_evts[i].function && st_lua_evts[i].function == evtref)
		{
			lua_pushinteger(L, evtref);
			return 1;
		}
	}

	for (i = 0; i < MAX_LUA_EVTS; i++)
	{
		// find if the function has already been registered
		if (!st_lua_evts[i].function)
		{
			st_lua_evts[i].name = G_NewString(name);
			st_lua_evts[i].function = evtref;
			lua_pushinteger(L, evtref);
			return 1;
		}
	}

	lua_pushinteger(L, evtref);
	return 1;
}

//
// Game.UnbindEvent(ref:Integer)
//
static int lua_Game_UnbindEvent(lua_State * L)
{
	int n = lua_gettop(L);
	int ref, i;

	if (n < 1)
		return luaL_error(L, "UnbindEvent(ref:Integer)");

	ref = luaL_checkinteger(L, 1);

	for (i = 0; i < MAX_LUA_EVTS; i++)
	{
		// find if the function has already been registered
		if (st_lua_evts[i].function && st_lua_evts[i].function == ref)
		{
			luaL_unref(L, LUA_REGISTRYINDEX, ref);
			memset(&st_lua_evts[i], 0, sizeof(st_lua_cmd_t));
			st_lua_evts[i].name = 0;
			st_lua_evts[i].function = 0;
			break;
		}
	}

	return 0;
}

//
// Game.Clients()
//
static int lua_Game_Clients(lua_State * L)
{
	int i, in = 0;

	// create a new table
	lua_newtable(L);

	for (i = 0; i < level.maxclients; i++)
	{
		gentity_t *ent = &level.gentities[i];
		if (ent && ent->client && ent->client->pers.connected == CON_CONNECTED)
		{
			in++;
			lua_pushinteger(L, in);
			lua_pushgentity(L, ent);
			lua_settable(L, -3);
		}
	}
	return 1;
}


//
// Game.ConcatArgs(index:Integer)
//
extern char	*ConcatArgs( int start );
static int lua_Game_ConcatArgs(lua_State * L)
{
	int n;

	n = luaL_optinteger(L, 1, 1);
	lua_pushstring(L, ConcatArgs(n));
	return 1;
}

//
// Game.Cvar(name:String)
// Game.Cvar(name:String, value:Mixed)
//
static int lua_Game_Cvar(lua_State * L)
{
	int n = lua_gettop(L);
	vmCvar_t cvar;
	char *name, *value;

	if (n < 1)
		return luaL_error(L, "must have at least one argument");

	name = (char*)luaL_checkstring(L, 1);
	luaL_argcheck(L, name != NULL, 1, "`string' expected");

	if (n == 1)
	{
		trap_Cvar_Register( &cvar, name, "", CVAR_ARCHIVE );
		
		if (cvar.integer)
			lua_pushinteger(L, cvar.integer);
		else if (cvar.string)
			lua_pushstring(L, cvar.string);
		else if (cvar.value)
			lua_pushnumber(L, cvar.value);
		else
			lua_pushnil(L);

		return 1;
	}
	
	if (n == 2)
	{
		value = (char*)luaL_checkstring(L, 2);
		luaL_argcheck(L, value != NULL, 2, "`string' expected");

		trap_Cvar_Set(name, value);
		return 0;
	}

	return 0;
}

//
// Game.Damage( to:Entity, damage:Number, from:Entity, mod:MethodOfDeath )
// Game.Damage( to:Entity, damage:Number, from:Entity, direction:Vector, point:Vector, mod:MethodOfDeath )
//
static int lua_Game_Damage(lua_State * L)
{
	int n = lua_gettop(L);
	lua_GEntity *to, *from;
	float damage;
	vec_t *direction, *point;
	int mod;

	if (n < 4)
		return luaL_error(L, "syntax: Game.Damage( to:Entity, damage:Number, from:Entity, mod:MethodOfDeath )");

	to = lua_getgentity(L, 1);
	luaL_argcheck(L, to != NULL, 1, "`GEntity' expected");

	damage = luaL_checknumber(L, 2);

	from = lua_getgentity(L, 3);
	luaL_argcheck(L, from != NULL, 3, "`GEntity' expected");

	mod = luaL_checkinteger(L, 4);

	direction = NULL;
	point = NULL;

	if (n > 4)
	{
		direction = lua_getvector(L, 5);
		point = lua_getvector(L, 6);
	}

	G_Damage(to->e, from->e, from->e, direction, point, damage, 0, mod);
	return 0;
}

//
// Game.Entities()
//
static int lua_Game_Entities(lua_State * L)
{
	int i, in = 0;

	// create a new table
	lua_newtable(L);

	for (i = 0; i < level.num_entities; i++)
	{
		gentity_t *ent = &level.gentities[i];
		if (ent && ent->inuse)
		{
			in++;
			lua_pushinteger(L, in);
			lua_pushgentity(L, ent);
			lua_settable(L, -3);
		}
	}
	return 1;
}

//
// Game.IsClient(cl:GEntity)
//
static int lua_Game_IsClient(lua_State * L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	if (n < 1)
		return luaL_error(L, "must have at least one argument");

	lent = lua_getgentity(L, 1);

	if (lent && lent->e && lent->e->client)
		lua_pushboolean(L, 1);
	else
		lua_pushboolean(L, 0);

	return 1;
}

//
// Game.IsEntity(cl:GEntity)
//
static int lua_Game_IsEntity(lua_State * L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	if (n < 1)
		return luaL_error(L, "must have at least one argument");

	lent = lua_getgentity(L, 1);

	if (lent && lent->e)
		lua_pushboolean(L, 1);
	else
		lua_pushboolean(L, 0);

	return 1;
}

//
// Game.IsNPC(cl:GEntity)
//
static int lua_Game_IsNPC(lua_State * L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	if (n < 1)
		return luaL_error(L, "must have at least one argument");

	lent = lua_getgentity(L, 1);

	if (lent && lent->e && lent->e->NPC)
		lua_pushboolean(L, 1);
	else
		lua_pushboolean(L, 0);

	return 1;
}

//
// Game.IsVehicle(cl:GEntity)
//
static int lua_Game_IsVehicle(lua_State * L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	if (n < 1)
		return luaL_error(L, "must have at least one argument");

	lent = lua_getgentity(L, 1);

	if (lent && lent->e && lent->e->m_pVehicle)
		lua_pushboolean(L, 1);
	else
		lua_pushboolean(L, 0);

	return 1;
}

//
// Game.PlayEffect( effect:String, origin:Vector, direction:Vector )
//
static int lua_Game_PlayEffect(lua_State * L)
{
	int n = lua_gettop(L);
	char *effect;
	vec_t *origin, *direction;

	if (n < 3)
		return luaL_error(L, "syntax: PlayEffect( effect:String, origin:Vector, direction:Vector )");

	effect = (char*)luaL_checkstring(L, 1);

	origin = lua_getvector(L, 2);
	luaL_argcheck(L, origin != NULL, 2, "`QVector' expected");

	direction = lua_getvector(L, 3);
	luaL_argcheck(L, direction != NULL, 3, "`QVector' expected");

	G_PlayEffect(G_EffectIndex(effect), origin, direction);
	return 0;
}

//
// Game.PointInBounds( origin:Vector, absMins:Vector, absMaxs:Vector )
//
qboolean G_PointInBounds( vec3_t point, vec3_t mins, vec3_t maxs );
static int lua_Game_PointInBounds(lua_State * L)
{
	int n = lua_gettop(L);
	vec_t *origin, *absMins, *absMaxs;

	if (n < 3)
		return luaL_error(L, "syntax: PointInBounds( origin:Vector, absMins:Vector, absMaxs:Vector )");

	origin = lua_getvector(L, 1);
	luaL_argcheck(L, origin != NULL, 1, "`QVector' expected");

	absMins = lua_getvector(L, 2);
	luaL_argcheck(L, absMins != NULL, 2, "`QVector' expected");

	absMaxs = lua_getvector(L, 3);
	luaL_argcheck(L, absMaxs != NULL, 3, "`QVector' expected");

	lua_pushboolean(L, G_PointInBounds(origin, absMins, absMaxs));
	return 1;
}

//
// Game.Print( message:String )
//
static int lua_Game_Print(lua_State * L)
{
	int             i;
	char            buf[1000];
	int             n = lua_gettop(L);

	memset(buf, 0, sizeof(buf));

	lua_rawgeti(L, LUA_REGISTRYINDEX, lua_toString);
	for(i = 1; i <= n; i++)
	{
		const char     *s;

		lua_pushvalue(L, -1);	// function to be called
		lua_pushvalue(L, i);	// value to print
		lua_call(L, 1, 1);
		s = lua_tostring(L, -1);	// get result

		if(s == NULL)
			return luaL_error(L, "`tostring' must return a string to `print'");

		Q_strcat(buf, sizeof(buf), s);

		lua_pop(L, 1);			// pop result
	}

	Com_Printf("%s", buf);
	return 0;
}

//
// Game.ScreenShake( org:Vector, target:Entity, intensity:Number, duration:Integer )
// Game.ScreenShake( org:Vector, target:Entity, intensity:Number, duration:Integer, global:Boolean )
//
gentity_t *G_ScreenShake(vec3_t org, gentity_t *target, float intensity, int duration, qboolean global);
static int lua_Game_ScreenShake(lua_State * L)
{
	int n = lua_gettop(L);
	vec_t *org;
	lua_GEntity *target;
	float intensity;
	int duration, global;

	if (n < 4)
		return luaL_error(L, "syntax: ScreenShake( org:Vector, target:Entity, intensity:Number, duration:Number )");

	if (n == 4)
		global = 0;
	else
		global = 1;

	org = lua_getvector(L, 1);
	target = lua_getgentity(L, 1);
	intensity = luaL_checknumber(L, 1);
	duration = luaL_checkinteger(L, 1);

	G_ScreenShake(org, target->e, intensity, duration, global);
	return 0;
}

//
// Game.Team( team:Integer )
//
static int lua_Game_Team(lua_State * L)
{
	int n = lua_gettop(L), i, in = 0;
	int team;

	if (n < 1)
		return luaL_error(L, "syntax: Game.Team( team:Integer )");

	team = luaL_optinteger(L, 1, 0);
	lua_newtable(L);

	for (i=0; i<level.num_entities; i++) {
		gentity_t *ent = &g_entities[i];
		if (ent && ent->client && ent->client->sess.sessionTeam == team) {
			lua_pushnumber(L, in);
			lua_pushgentity(L, ent);
			lua_settable(L, -3);
			in++;
		}
	}

	return 1;
}

//
// Game.Time()
//
static int lua_Game_Time(lua_State * L)
{
	lua_pushinteger(L, level.time);
	return 1;
}

//
// Game.TimeLeft()
//
static int lua_Game_TimeLeft(lua_State * L)
{
	lua_pushnumber(L, (g_timelimit.integer*60000) - level.time);
	return 1;
}

static const luaL_reg GameRegistry[] = {
	{"AngleVectors", lua_Game_AngleVectors},
	{"Argument", lua_Game_Argument},
	{"BindCommand", lua_Game_BindCommand},
	{"BindSvCommand", lua_Game_BindSvCommand},
	{"BindEvent", lua_Game_BindEvent},
	{"BroadcastChat", lua_Game_BroadcastChat},
	{"BroadcastConsole", lua_Game_BroadcastConsole},
	{"BroadcastScreen", lua_Game_BroadcastScreen},
	{"Clients", lua_Game_Clients},
	{"ConcatArgs", lua_Game_ConcatArgs},
	{"Cvar", lua_Game_Cvar},
	{"Damage", lua_Game_Damage},
	{"Entities", lua_Game_Entities},
	{"IsClient", lua_Game_IsClient},
	{"IsEntity", lua_Game_IsEntity},
	{"IsNPC", lua_Game_IsNPC},
	{"IsVehicle", lua_Game_IsVehicle},

	{"PlayEffect", lua_Game_PlayEffect},
	{"PointInBounds", lua_Game_PointInBounds},
	{"Print", lua_Game_Print},
	{"ScreenShake", lua_Game_ScreenShake},
	{"Team", lua_Game_Team},
	{"Time", lua_Game_Time},
	{"TimeLeft", lua_Game_TimeLeft},
	{"UnbindEvent", lua_Game_UnbindEvent},

	{NULL, NULL}
};

int luaopen_game(lua_State * L)
{
	luaL_register(L, "Game", GameRegistry);

	lua_pushliteral(L, "GAMEVERSION");
	lua_pushliteral(L, GAMEVERSION);
	return 1;
}
