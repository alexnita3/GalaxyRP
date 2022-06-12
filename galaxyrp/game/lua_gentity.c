// lua_gentity.cpp -- entity library for Lua

#include "g_local.h"
#include "g_lua.h"

static int lua_GEntity_GC(lua_State * L)
{
//  G_Printf("Lua says bye to entity = %p\n", lua_getentity(L));
	return 0;
}

//
// GEntity.Find( field:String, value:String )
// GEntity.Find( from:Integer, field:String, value:String )
//
extern BG_field_t fields;
static int lua_GEntity_Find(lua_State *L)
{
	int n = lua_gettop(L), from = 0;
	char *field, *value;
	BG_field_t *f;

	if (n < 2)
		return luaL_error(L, "syntax: Find( field:String, value:String )");

	if (n > 2)
	{
		from = luaL_checkinteger(L, 1);
		field = (char*)luaL_checkstring(L, 2);
		value = (char*)luaL_checkstring(L, 3);
	}
	else
	{
		field = (char*)luaL_checkstring(L, 1);
		value = (char*)luaL_checkstring(L, 2);
	}

	if (from < 0 || from > MAX_GENTITIES)
		return luaL_error(L, "from is out of bounds");

	for (f = &fields; f->name; f++)
	{
		if ( !Q_stricmp(f->name, field) )
		{
			gentity_t *result = G_Find(&g_entities[from], f->ofs, value);

			if (!result)
				lua_pushnil(L);
			else
				lua_pushgentity(L, result);

			return 1;
		}
	}

	return 0;
}

//
// GEntity.FromNumber( id:Integer )
//
static int lua_GEntity_FromNumber(lua_State *L)
{
	int n = lua_gettop(L), num;

	if (n < 1)
		return luaL_error(L, "syntax: GEntity.FromNumber( id:Integer )");

	num = luaL_checkinteger(L, 1);

	if (num > MAX_GENTITIES)
		return luaL_error(L, "number can't be more than %i", MAX_GENTITIES);

	lua_pushgentity(L, &g_entities[num]);
	return 1;
}

//
// GEntity.Pick( targetName:String )
//
static int lua_GEntity_Pick(lua_State *L)
{
	int n = lua_gettop(L);
	char *targetName;
	gentity_t *result;

	if (n < 1)
		return luaL_error(L, "syntax: Pick( targetName:String )");

	targetName = (char*)luaL_checkstring(L, 1);
	result = G_Find( NULL, FOFS(targetname), targetName );

	if (result)
		lua_pushgentity(L, result);
	else
		lua_pushnil(L);

	return 1;
}

//
// GEntity.Spawn( )
//
static int lua_GEntity_Spawn(lua_State *L)
{
	gentity_t *ent;
	
	ent = G_Spawn();

	if (ent)
		lua_pushgentity(L, ent);
	else
		lua_pushnil(L);

	return 1;
}

//
// GEntity:AbsMaxs( )
// GEntity:AbsMaxs( newVal:QVector )
//
static int lua_GEntity_AbsMaxs(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		vec_t *newVal = lua_getvector(L, 2);
		VectorCopy(newVal, lent->e->r.absmax);
		return 0;
	}

	lua_pushvector(L, lent->e->r.absmax);
	return 1;
}

//
// GEntity:AbsMins( )
// GEntity:AbsMins( newVal:QVector )
//
static int lua_GEntity_AbsMins(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		vec_t *newVal = lua_getvector(L, 2);
		VectorCopy(newVal, lent->e->r.absmin);
		return 0;
	}

	lua_pushvector(L, lent->e->r.absmin);
	return 1;
}

//
// GEntity:Activated( )
// GEntity:Activated( newVal:Boolean )
//
static int lua_GEntity_Activated(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);

		if (newVal == 0)
			lent->e->flags |= FL_INACTIVE;
		else
			lent->e->flags &= ~FL_INACTIVE;
	}

	if (lent->e->flags & FL_INACTIVE)
		lua_pushboolean(L, 0);
	else
		lua_pushboolean(L, 1);

	return 1;
}

//
// GEntity:Angles( )
// GEntity:Angles( newVal:QVector )
//
static int lua_GEntity_Angles(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		vec_t *newVal = lua_getvector(L, 2);
		VectorCopy(newVal, lent->e->s.angles);
		return 0;
	}

	lua_pushvector(L, lent->e->s.angles);
	return 1;
}

/*
static int lua_GEntity_AutoBox(lua_State *L)
{
	return 0;
}
*/

/*
	int			lua_think;
	int			lua_reached;
	int			lua_blocked;
	int			lua_touch;
	int			lua_use;
	int			lua_pain;
	int			lua_die;
*/

//
// GEntity:BindBlocked( callback:Function )
//
static int lua_GEntity_BindBlocked(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: BindBlocked( callback:Function )");

	lent = lua_getgentity(L, 1);

	if (lua_isnil(L, 2))
	{
		if (lent->e->lua_blocked > 0)
			luaL_unref(L, LUA_REGISTRYINDEX, lent->e->lua_blocked);
		lent->e->lua_blocked = 0;
	}
	else
	{
		lua_pushvalue(L, 2);
		lent->e->lua_blocked = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	return 0;
}

//
// GEntity:BindDie( callback:Function )
//
static int lua_GEntity_BindDie(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: BindDie( callback:Function )");

	lent = lua_getgentity(L, 1);

	if (lua_isnil(L, 2))
	{
		if (lent->e->lua_die > 0)
			luaL_unref(L, LUA_REGISTRYINDEX, lent->e->lua_die);
		lent->e->lua_die = 0;
	}
	else
	{
		lua_pushvalue(L, 2);
		lent->e->lua_die = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	return 0;
}

//
// GEntity:BindPain( callback:Function )
//
static int lua_GEntity_BindPain(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: BindPain( callback:Function )");

	lent = lua_getgentity(L, 1);

	if (lua_isnil(L, 2))
	{
		if (lent->e->lua_pain > 0)
			luaL_unref(L, LUA_REGISTRYINDEX, lent->e->lua_pain);
		lent->e->lua_pain = 0;
	}
	else
	{
		lua_pushvalue(L, 2);
		lent->e->lua_pain = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	return 0;
}

//
// GEntity:BindReached( callback:Function )
//
static int lua_GEntity_BindReached(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: BindReached( callback:Function )");

	lent = lua_getgentity(L, 1);

	if (lua_isnil(L, 2))
	{
		if (lent->e->lua_reached > 0)
			luaL_unref(L, LUA_REGISTRYINDEX, lent->e->lua_reached);
		lent->e->lua_reached = 0;
	}
	else
	{
		lua_pushvalue(L, 2);
		lent->e->lua_reached = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	return 0;
}

//
// GEntity:BindThink( callback:Function )
//
static int lua_GEntity_BindThink(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: BindThink( callback:Function )");

	lent = lua_getgentity(L, 1);

	if (lua_isnil(L, 2))
	{
		if (lent->e->lua_think > 0)
			luaL_unref(L, LUA_REGISTRYINDEX, lent->e->lua_think);
		lent->e->lua_think = 0;
	}
	else
	{
		lua_pushvalue(L, 2);
		lent->e->lua_think = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	return 0;
}

//
// GEntity:BindTouch( callback:Function )
//
static int lua_GEntity_BindTouch(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: BindTouch( callback:Function )");

	lent = lua_getgentity(L, 1);

	if (lua_isnil(L, 2))
	{
		if (lent->e->lua_touch > 0)
			luaL_unref(L, LUA_REGISTRYINDEX, lent->e->lua_touch);
		lent->e->lua_touch = 0;
	}
	else
	{
		lua_pushvalue(L, 2);
		lent->e->lua_touch = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	return 0;
}

//
// GEntity:BindUse( callback:Function )
//
static int lua_GEntity_BindUse(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: BindUse( callback:Function )");

	lent = lua_getgentity(L, 1);

	if (lua_isnil(L, 2))
	{
		if (lent->e->lua_use > 0)
			luaL_unref(L, LUA_REGISTRYINDEX, lent->e->lua_use);
		lent->e->lua_use = 0;
	}
	else
	{
		lua_pushvalue(L, 2);
		lent->e->lua_use = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	return 0;
}

//
// GEntity:CallSpawn()
//
qboolean G_CallSpawn( gentity_t *ent );
static int lua_GEntity_CallSpawn(lua_State *L)
{
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);
	G_CallSpawn(lent->e);
	return 0;
}

//
// GEntity:AddClipmask( clipmask:Integer )
//
static int lua_GEntity_AddClipmask(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: AddClipmask( clipmask:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (!(lent->e->clipmask & val))
		lent->e->clipmask |= val;

	return 0;
}

//
// GEntity:AddContent( content:Integer )
//
static int lua_GEntity_AddContent(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: AddContent( content:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (!(lent->e->r.contents & val))
		lent->e->r.contents |= val;

	return 0;
}

//
// GEntity:AddEflag( content:Integer )
//
static int lua_GEntity_AddEflag(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: AddEflag( eFlag:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (!(lent->e->s.eFlags & val))
		lent->e->s.eFlags |= val;

	return 0;
}

//
// GEntity:AddFlag( content:Integer )
//
static int lua_GEntity_AddFlag(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: AddFlag( Flag:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (!(lent->e->flags & val))
		lent->e->flags |= val;

	return 0;
}

//
// GEntity:AddSvFlag( content:Integer )
//
static int lua_GEntity_AddSvFlag(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: AddSvFlag( Flag:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (!(lent->e->r.svFlags & val))
		lent->e->r.svFlags |= val;

	return 0;
}

//
// GEntity:DelClipmask( clipmask:Integer )
//
static int lua_GEntity_DelClipmask(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: DelClipmask( clipmask:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (lent->e->clipmask & val)
		lent->e->clipmask &= ~val;

	return 0;
}

//
// GEntity:DelContent( content:Integer )
//
static int lua_GEntity_DelContent(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: DelContent( content:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (lent->e->r.contents & val)
		lent->e->r.contents &= ~val;

	return 0;
}

//
// GEntity:DelEflag( content:Integer )
//
static int lua_GEntity_DelEflag(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: DelEflag( eFlag:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (lent->e->s.eFlags & val)
		lent->e->s.eFlags &= ~val;

	return 0;
}

//
// GEntity:DelFlag( content:Integer )
//
static int lua_GEntity_DelFlag(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: DelFlag( Flag:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (lent->e->flags & val)
		lent->e->flags &= ~val;

	return 0;
}

//
// GEntity:DelSvFlag( content:Integer )
//
static int lua_GEntity_DelSvFlag(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: DelSvFlag( Flag:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (lent->e->r.svFlags & val)
		lent->e->r.svFlags &= ~val;

	return 0;
}
//
// GEntity:Free()
//
static int lua_GEntity_Free(lua_State *L)
{
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);
	G_FreeEntity(lent->e);
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_Hand(lua_State *L)
{
	return 0;
}

//
// GEntity:HasClipmask( clipmask:Integer )
//
static int lua_GEntity_HasClipmask(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: HasClipmask( clipmask:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	lua_pushboolean(L, lent->e->clipmask & val);
	return 1;
}

//
// GEntity:HasContent( content:Integer )
//
static int lua_GEntity_HasContent(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: HasContent( content:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	lua_pushboolean(L, lent->e->r.contents & val);
	return 1;
}

//
// GEntity:HasEflag( eFlag:Integer )
//
static int lua_GEntity_HasEflag(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: HasEflag( eFlag:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	lua_pushboolean(L, lent->e->s.eFlags & val);
	return 1;
}

//
// GEntity:HasFlag( Flag:Integer )
//
static int lua_GEntity_HasFlag(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: HasFlag( Flag:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	lua_pushboolean(L, lent->e->flags & val);
	return 1;
}

//
// GEntity:HasSvFlag( svFlag:Integer )
//
static int lua_GEntity_HasSvFlag(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: HasSvFlag( svFlag:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	lua_pushboolean(L, lent->e->r.svFlags & val);
	return 1;
}

//
// GEntity:HealthMax( )
// GEntity:HealthMax( newVal:Integer )
//
static int lua_GEntity_HealthMax(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->maxHealth = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->maxHealth);
	return 1;
}

//
// GEntity:Health( )
// GEntity:Health( newVal:Integer )
//
static int lua_GEntity_Health(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->health = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->health);
	return 1;
}

//
// GEntity:TODO
//
static int lua_GEntity_Inside(lua_State *L)
{
	return 0;
}

//
// GEntity:Key( field:String )
// GEntity:Key( field:String, value:Mixed )
//
static int lua_GEntity_Key(lua_State *L)
{
	int n = lua_gettop(L);
	BG_field_t	*f;
	char *field;
	lua_GEntity *lent;
	byte *b;

	if (n < 2)
		return luaL_error(L, "syntax: Key( field:String )");

	lent = lua_getgentity(L, 1);
	field = (char*)luaL_checkstring(L, 2);

	for ( f=&fields ; f->name ; f++ )
	{
		if ( !Q_stricmp(f->name, field) )
		{
			b = (byte *)lent->e;
			switch( f->type ) {
				case F_LSTRING:
				{
					if (n > 2)
					{
						char *value = (char*)luaL_checkstring(L, 3);
						*(char **)(b+f->ofs) = G_NewString(value);
						return 0;
					}
					else
					{
						lua_pushstring(L, *(char **)(b+f->ofs));
						return 1;
					}
				}
				case F_VECTOR:
				{
					if (n > 2)
					{
						vec_t *value = lua_getvector(L, 3);
						((float *)(b+f->ofs))[0] = value[0];
						((float *)(b+f->ofs))[1] = value[1];
						((float *)(b+f->ofs))[2] = value[2];
						return 0;
					}
					else
					{
						vec3_t result;
						VectorSet(result, ((float *)(b+f->ofs))[0], ((float *)(b+f->ofs))[1], ((float *)(b+f->ofs))[2]);
						lua_pushvector(L, result);
						return 1;
					}
				}
				case F_INT:
				{
					if (n > 2)
					{
						int value = luaL_checkinteger(L, 3);
						*(int *)(b+f->ofs) = value;
						return 0;
					}
					else
					{
						lua_pushinteger(L, *(int *)(b+f->ofs));
						return 1;
					}
				}
				case F_FLOAT:
				{
					if (n > 2)
					{
						float value = luaL_checknumber(L, 3);
						*(float *)(b+f->ofs) = value;
						return 0;
					}
					else
					{
						lua_pushnumber(L, *(float *)(b+f->ofs));
						return 1;
					}
				}
				case F_ANGLEHACK:
				{
					if (n > 2)
					{
						float value = luaL_checknumber(L, 3);
						((float *)(b+f->ofs))[0] = 0;
						((float *)(b+f->ofs))[1] = value;
						((float *)(b+f->ofs))[2] = 0;
						return 0;
					}
					else
					{
						lua_pushnumber(L, ((float *)(b+f->ofs))[1]);
						return 1;
					}
				}

				case F_PARM1:
				case F_PARM2:
				case F_PARM3:
				case F_PARM4:
				case F_PARM5:
				case F_PARM6:
				case F_PARM7:
				case F_PARM8:
				case F_PARM9:
				case F_PARM10:
				case F_PARM11:
				case F_PARM12:
				case F_PARM13:
				case F_PARM14:
				case F_PARM15:
				case F_PARM16:
					//Q3_SetParm( ((gentity_t *)(ent))->s.number, (f->type - F_PARM1), (char *) value );
					//break;
				default:
				case F_IGNORE:
					break;
			}
		}
	}

	return 0;
}

//
// GEntity:Link( )
// GEntity:Link( newVal:Boolean )
//
static int lua_GEntity_Link(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		if (newVal)
			trap_LinkEntity(lent->e);
		else
			trap_UnlinkEntity(lent->e);
		return 0;
	}

	lua_pushinteger(L, lent->e->r.linked);
	return 1;
}

//
// GEntity:Maxs( )
// GEntity:Maxs( newVal:QVector )
//
static int lua_GEntity_Maxs(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		vec_t *newVal = lua_getvector(L, 2);
		VectorCopy(newVal, lent->e->r.maxs);
		return 0;
	}

	lua_pushvector(L, lent->e->r.maxs);
	return 1;
}

//
// GEntity:Mins( )
// GEntity:Mins( newVal:QVector )
//
static int lua_GEntity_Mins(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		vec_t *newVal = lua_getvector(L, 2);
		VectorCopy(newVal, lent->e->r.mins);
		return 0;
	}

	lua_pushvector(L, lent->e->r.mins);
	return 1;
}

//
// GEntity:Model( )
// GEntity:Model( newVal:String )
//
static int lua_GEntity_Model(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		char *newVal = (char*)luaL_checkstring(L, 2);
		lent->e->model = G_NewString(newVal);
		return 0;
	}

	lua_pushstring(L, lent->e->model);
	return 1;
}

//
// GEntity:TODO
//
static int lua_GEntity_Move(lua_State *L)
{
	return 0;
}

//
// GEntity:MoverState( )
// GEntity:MoverState( newVal:Integer )
//
static int lua_GEntity_MoverState(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		moverState_t newVal = (moverState_t)luaL_checkinteger(L, 2);
		lent->e->moverState = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->moverState);
	return 1;
}

//
// GEntity:NextThink( )
// GEntity:NextThink( newVal:Integer )
//
static int lua_GEntity_NextThink(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->nextthink = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->nextthink);
	return 1;
}

//
// GEntity:Number( )
//
static int lua_GEntity_Number(lua_State *L)
{
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	lua_pushinteger(L, lent->e->s.number);
	return 1;
}

//
// GEntity:Origin( )
// GEntity:Origin( newVal:QVector )
//
static int lua_GEntity_Origin(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		vec_t *newVal = lua_getvector(L, 2);
		VectorCopy(newVal, lent->e->s.origin);
		return 0;
	}

	lua_pushvector(L, lent->e->s.origin);
	return 1;
}

//
// GEntity:Parent( )
// GEntity:Parent( newVal:GEntity )
//
static int lua_GEntity_Parent(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		lua_GEntity *newVal = lua_getgentity(L, 2);
		lent->e->parent = newVal->e;
		return 0;
	}

	lua_pushgentity(L, lent->e->parent);
	return 1;
}

//
// GEntity:Position( )
// GEntity:Position( newVal:QVector )
//
static int lua_GEntity_Position(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		vec_t *newVal = lua_getvector(L, 2);
		VectorCopy(newVal, lent->e->r.currentOrigin);
		return 0;
	}

	lua_pushvector(L, lent->e->r.currentOrigin);
	return 1;
}

//
// GEntity:TODO
//
static int lua_GEntity_Rotate(lua_State *L)
{
	return 0;
}

//
// GEntity:Rotation( )
// GEntity:Rotation( newVal:QVector )
//
static int lua_GEntity_Rotation(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		vec_t *newVal = lua_getvector(L, 2);
		VectorCopy(newVal, lent->e->r.currentAngles);
		return 0;
	}

	lua_pushvector(L, lent->e->r.currentAngles);
	return 1;
}

//
// GEntity:Scale( )
// GEntity:Scale( newVal:Number )
//
static int lua_GEntity_Scale(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		float newVal = luaL_checknumber(L, 1);
		VectorSet(lent->e->modelScale, newVal, newVal, newVal);
		return 0;
	}

	lua_pushvector(L, lent->e->modelScale);
	return 1;
}

//
// GEntity:Sound( sound:String )
// GEntity:Sound( sound:String, channel:Integer )
//
static int lua_GEntity_Sound(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;
	char *sound;
	int channel = CHAN_AUTO;

	lent = lua_getgentity(L, 1);

	if (n < 2)
		return luaL_error(L, "syntax: Sound( sound:String )");

	sound = (char*)luaL_checkstring(L, 2);

	if (n > 2)
		channel = luaL_checkinteger(L, 3);

	G_Sound(lent->e, channel, G_SoundIndex(sound));
	return 0;
}

//
// GEntity:Speed( )
// GEntity:Speed( newVal:Number )
//
static int lua_GEntity_Speed(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		float newVal = luaL_checknumber(L, 2);
		lent->e->speed = newVal;
		return 0;
	}

	lua_pushnumber(L, lent->e->speed);
	return 1;
}

//
// GEntity:Use( )
// GEntity:Use( from:GEntity )
//
static int lua_GEntity_Use(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent, *lfrom;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		lfrom = lua_getgentity(L, 2);
		GlobalUse(lent->e, lfrom->e, lfrom->e);
	}
	else
		GlobalUse(lent->e, NULL, NULL);

	return 0;
}

//
// GEntity:Var( name:String )
// GEntity:Var( name:String, value:Mixed )
//
static int lua_GEntity_Var(lua_State *L)
{
	int n = lua_gettop(L), i;
	lua_GEntity *lent;
	char *name;

	if (n < 2)
		return luaL_error(L, "syntax: Var( name:String )");

	lent = lua_getgentity(L, 1);
	name = (char*)luaL_checkstring(L, 2);

	//for (it = lent->e->lua_vars.begin(); it != lent->e->lua_vars.end(); ++it)
	for (i = 0; i < MAX_LUA_VARS; i++)
	{
		if (lent->e->lua_vars[i].used && !Q_stricmp(lent->e->lua_vars[i].name, name))
		{
			if (n > 2 && lua_isnil(L, 3))
			{
				lent->e->lua_vars[i].used = qfalse;
				lent->e->lua_vars[i].type = 0;
				lent->e->lua_vars[i].name = 0;
				lent->e->lua_vars[i].string = 0;
				lent->e->lua_vars[i].value = 0;
				memset(&lent->e->lua_vars[i], 0, sizeof(st_lua_var_t));
				return 0;
			}

			if (n == 2)
			{
				if (lent->e->lua_vars[i].type == LUA_TNIL)
					lua_pushnil(L);
				else if (lent->e->lua_vars[i].type == LUA_TNUMBER || lent->e->lua_vars[i].type == LUA_TBOOLEAN)
					lua_pushnumber(L, lent->e->lua_vars[i].value);
				else if (lent->e->lua_vars[i].type == LUA_TSTRING)
					lua_pushstring(L, lent->e->lua_vars[i].string);

				return 1;
			}
			else if (n == 3)
			{
				if (lua_isboolean(L, 3) || lua_isnumber(L, 3))
				{
					lent->e->lua_vars[i].type = LUA_TNUMBER;
					lent->e->lua_vars[i].value = lua_tonumber(L, 3);
				}
				else if (lua_isstring(L, 3))
				{
					lent->e->lua_vars[i].type = LUA_TSTRING;
					lent->e->lua_vars[i].string = G_NewString(lua_tostring(L, 3));
				}
			}

			return 0;
		}
	}

	for (i = 0; i < MAX_LUA_VARS; i++)
	{
		if (!lent->e->lua_vars[i].used)
		{
			lent->e->lua_vars[i].name = G_NewString(name);

			if (n == 3)
			{
				if (lua_isboolean(L, 3) || lua_isnumber(L, 3))
				{
					lent->e->lua_vars[i].type = LUA_TNUMBER;
					lent->e->lua_vars[i].value = lua_tonumber(L, 3);
				}
				else if (lua_isstring(L, 3))
				{
					lent->e->lua_vars[i].type = LUA_TSTRING;
					lent->e->lua_vars[i].string = G_NewString(lua_tostring(L, 3));
				}
			}

			lent->e->lua_vars[i].used = qtrue;
			return 0;
		}
	}
	return 0;
}

//
// GEntity:Weapon( )
// GEntity:Weapon( newVal:Integer )
//
static int lua_GEntity_Weapon(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		float newVal = luaL_checkinteger(L, 2);
		lent->e->s.weapon = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->s.weapon);
	return 1;
}

//
// GEntity:TODO
//
static int lua_GEntity_cliAimEntity(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_cliAimOrigin(lua_State *L)
{
	return 0;
}

//
// GEntity:Ammo( type:Integer )
// GEntity:Ammo( type:Integer, newVal:Integer )
//
static int lua_GEntity_cliAmmo(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;
	int ammoType;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");
	
	if (n < 2)
		return luaL_error(L, "syntax: GEntity:Ammo( type:Integer )");

	ammoType = luaL_checkinteger(L, 2);

	if (ammoType < AMMO_NONE || ammoType > AMMO_MAX)
		return luaL_error(L, "ammoType out of bounds");

	if (n > 2)
	{
		float newVal = luaL_checkinteger(L, 3);
		lent->e->client->ps.ammo[ammoType] = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->ps.ammo[ammoType]);
	return 1;
}

//
// GEntity:cliAngles( )
// GEntity:cliAngles( newVal:QVector )
//
static int lua_GEntity_cliAngles(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		vec_t *newVal = lua_getvector(L, 2);
		VectorCopy(lent->e->client->ps.viewangles, newVal);
		return 0;
	}

	lua_pushvector(L, lent->e->client->ps.viewangles);
	return 1;
}

//
// GEntity:cliAnimLegsTimer( )
// GEntity:cliAnimLegsTimer( newVal:Integer )
//
void BG_SetLegsAnimTimer(playerState_t *ps, int time);
static int lua_GEntity_cliAnimLegsTimer(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		BG_SetLegsAnimTimer(&lent->e->client->ps, newVal);
		return 0;
	}

	lua_pushinteger(L, lent->e->client->ps.legsTimer);
	return 1;
}

//
// GEntity:cliAnimLegs( )
// GEntity:cliAnimLegs( newVal:Integer )
//
void BG_StartLegsAnim( playerState_t *ps, int anim );
static int lua_GEntity_cliAnimLegs(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		BG_StartLegsAnim(&lent->e->client->ps, newVal);
		return 0;
	}

	lua_pushinteger(L, lent->e->client->ps.legsAnim);
	return 1;
}

//
// GEntity:cliAnimTorsoTimer( )
// GEntity:cliAnimTorsoTimer( newVal:Integer )
//
void BG_SetTorsoAnimTimer(playerState_t *ps, int time);
static int lua_GEntity_cliAnimTorsoTimer(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		BG_SetTorsoAnimTimer(&lent->e->client->ps, newVal);
		return 0;
	}

	lua_pushinteger(L, lent->e->client->ps.torsoTimer);
	return 1;
}

//
// GEntity:cliAnimTorso( )
// GEntity:cliAnimTorso( newVal:Integer )
//
void BG_StartTorsoAnim( playerState_t *ps, int anim );
static int lua_GEntity_cliAnimTorso(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		BG_StartTorsoAnim(&lent->e->client->ps, newVal);
		return 0;
	}

	lua_pushinteger(L, lent->e->client->ps.torsoAnim);
	return 1;
}

//
// GEntity:cliArmor( )
// GEntity:cliArmor( newVal:Integer )
//
static int lua_GEntity_cliArmor(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->client->ps.stats[STAT_ARMOR] = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->ps.stats[STAT_ARMOR]);
	return 1;
}

//
// GEntity:cliDrop( )
// GEntity:cliDrop( newVal:Integer, reason:String )
//
static int lua_GEntity_cliDrop(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;
	char *reason;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
		trap_DropClient(lent->e->client->ps.clientNum, "");
	else
	{
		reason = (char*)luaL_checkstring(L, 2);
		trap_DropClient(lent->e->client->ps.clientNum, reason);
	}

	return 0;
}

//
// GEntity:cliForce( )
// GEntity:cliForce( newVal:Integer )
//
static int lua_GEntity_cliForce(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->client->ps.fd.forcePower = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->ps.fd.forcePower);
	return 1;
}

//
// GEntity:cliForceMax( )
// GEntity:cliForceMax( newVal:Integer )
//
static int lua_GEntity_cliForceMax(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->client->ps.fd.forcePowerMax = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->ps.fd.forcePowerMax);
	return 1;
}

//
// GEntity:cliForcePowerLevel( forcePower:Integer )
// GEntity:cliForcePowerLevel( forcePower:Integer, newVal:Integer )
//
static int lua_GEntity_cliForcePowerLevel(lua_State *L)
{
	int n = lua_gettop(L), forcePower;
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n < 2)
		return luaL_error(L, "syntax: cliForcePowerLevel( forcePower:Integer )");

	forcePower = luaL_checkinteger(L, 2);

	if (forcePower < FP_FIRST || forcePower >= NUM_FORCE_POWERS)
		return luaL_error(L, "forcePower out of bounds");

	if (n > 2)
	{
		int newVal = luaL_checkinteger(L, 3);
		lent->e->client->ps.fd.forcePowerLevel[forcePower] = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->ps.fd.forcePowerLevel[forcePower]);
	return 1;
}

//
// GEntity:cliGiveForce( forcePower:Integer )
//
static int lua_GEntity_cliGiveForce(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: cliGiveForce( forcePower:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (val < FP_FIRST || val > NUM_FORCE_POWERS)
		return luaL_error(L, "forcePower out of bounds");

	if (!(lent->e->client->ps.fd.forcePowersKnown & val))
		lent->e->client->ps.fd.forcePowersKnown |= (1 << val);

	return 0;
}

//
// GEntity:cliGiveHoldable( holdable:Integer )
//
static int lua_GEntity_cliGiveHoldable(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: cliGiveHoldable( holdable:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (val < HI_NONE || val > HI_NUM_HOLDABLE)
		return luaL_error(L, "holdable out of bounds");

	if (!(lent->e->client->ps.stats[STAT_HOLDABLE_ITEMS] & val))
		lent->e->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << val);

	return 0;
}

//
// GEntity:cliGiveWeapon( weapon:Integer )
//
static int lua_GEntity_cliGiveWeapon(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: cliGiveWeapon( weapon:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (val < WP_NONE || val > WP_NUM_WEAPONS)
		return luaL_error(L, "weapon out of bounds");

	if (!(lent->e->client->ps.stats[STAT_WEAPONS] & val))
		lent->e->client->ps.stats[STAT_WEAPONS] |= (1 << val);

	return 0;
}

//
// GEntity:cliGravity( )
// GEntity:cliGravity( newVal:Integer )
//
static int lua_GEntity_cliGravity(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->client->ps.gravity = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->ps.gravity);
	return 1;
}

//
// GEntity:cliHasForce( forcePower:Integer )
//
static int lua_GEntity_cliHasForce(lua_State *L)
{
	int n = lua_gettop(L), fp;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: cliHasForce( forcePower:Integer )");

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	fp = luaL_checkinteger(L, 2);
	if (fp < FP_FIRST || fp > NUM_FORCE_POWERS)
		return luaL_error(L, "forcePower is out of bounds");

	lua_pushboolean(L, lent->e->client->ps.fd.forcePowerLevel[fp] > 0);
	return 1;
}

//
// GEntity:cliHasHoldable( holdable:Integer )
//
static int lua_GEntity_cliHasHoldable(lua_State *L)
{
	int n = lua_gettop(L), hi;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: cliHasHoldable( holdable:Integer )");

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	hi = luaL_checkinteger(L, 2);
	if (hi < HI_NONE || hi > HI_NUM_HOLDABLE)
		return luaL_error(L, "holdable is out of bounds");

	lua_pushboolean(L, lent->e->client->ps.stats[STAT_HOLDABLE_ITEMS] & hi);
	return 1;
}

//
// GEntity:cliHasWeapon( weapon:Integer )
//
static int lua_GEntity_cliHasWeapon(lua_State *L)
{
	int n = lua_gettop(L), wp;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: cliHasWeapon( holdable:Integer )");

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	wp = luaL_checkinteger(L, 2);
	if (wp < HI_NONE || wp > HI_NUM_HOLDABLE)
		return luaL_error(L, "weapon is out of bounds");

	lua_pushboolean(L, lent->e->client->ps.stats[STAT_WEAPONS] & wp);
	return 1;
}

//
// GEntity:cliHealthMax( )
// GEntity:cliHealthMax( newVal:Integer )
//
static int lua_GEntity_cliHealthMax(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->client->ps.stats[STAT_MAX_HEALTH] = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->ps.stats[STAT_MAX_HEALTH]);
	return 1;
}

//
// GEntity:cliHealth( )
// GEntity:cliHealth( newVal:Integer )
//
static int lua_GEntity_cliHealth(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->client->ps.stats[STAT_HEALTH] = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->ps.stats[STAT_HEALTH]);
	return 1;
}

//
// GEntity:cliIP( )
//
static int lua_GEntity_cliIP(lua_State *L)
{
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	lua_pushstring(L, lent->e->client->sess.IPstring);
	return 1;
}

//
// GEntity:TODO
//
static int lua_GEntity_cliLeaveVehicle(lua_State *L)
{
	return 0;
}

//
// GEntity:cliModel( )
// GEntity:cliModel( newVal:String )
//
static int lua_GEntity_cliModel(lua_State *L)
{
	return 0;
}

//
// GEntity:cliNameTime( )
// GEntity:cliNameTime( newVal:Integer )
//
static int lua_GEntity_cliNameTime(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->client->pers.netnameTime = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->pers.netnameTime);
	return 1;
}

//
// GEntity:cliName( )
// GEntity:cliName( newVal:String )
//
static int lua_GEntity_cliName(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		char *newVal = (char*)luaL_checkstring(L, 2);
		Q_strncpyz(lent->e->client->pers.netname, newVal, sizeof(lent->e->client->pers.netname));
		lent->e->client->pers.netnameTime = 0;
		ClientUserinfoChanged(lent->e->client->ps.clientNum);
		lent->e->client->pers.netnameTime = 0;
		return 0;
	}

	lua_pushstring(L, lent->e->client->pers.netname);
	return 1;
}

//
// GEntity:cliOrigin( )
// GEntity:cliOrigin( newVal:QVector )
//
static int lua_GEntity_cliOrigin(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		vec_t *newVal = lua_getvector(L, 2);
		VectorCopy(newVal, lent->e->client->ps.origin);
		return 0;
	}

	lua_pushvector(L, lent->e->client->ps.origin);
	return 1;
}

//
// GEntity:cliPing( )
//
static int lua_GEntity_cliPing(lua_State *L)
{
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	lua_pushinteger(L, lent->e->client->ps.ping);
	return 1;
}

//
// GEntity:cliPrintChat( message:String )
//
static int lua_GEntity_cliPrintChat(lua_State *L)
{
	int             i;
	char            buf[1000];
	int             n = lua_gettop(L);
	lua_GEntity		*lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	memset(buf, 0, sizeof(buf));

	lua_rawgeti(L, LUA_REGISTRYINDEX, lua_toString);
	for(i = 2; i <= n; i++)
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

	trap_SendServerCommand(lent->e->client->ps.clientNum, va("chat \"%s\"", buf));
	return 0;
}

//
// GEntity:cliPrintConsole( message:String )
//
static int lua_GEntity_cliPrintConsole(lua_State *L)
{
	int             i;
	char            buf[1000];
	int             n = lua_gettop(L);
	lua_GEntity		*lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	memset(buf, 0, sizeof(buf));

	lua_rawgeti(L, LUA_REGISTRYINDEX, lua_toString);
	for(i = 2; i <= n; i++)
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

	trap_SendServerCommand(lent->e->client->ps.clientNum, va("print \"%s\"", buf));
	return 0;
}

//
// GEntity:cliPrintScreen( message:String )
//
static int lua_GEntity_cliPrintScreen(lua_State *L)
{
	int             i;
	char            buf[1000];
	int             n = lua_gettop(L);
	lua_GEntity		*lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	memset(buf, 0, sizeof(buf));

	lua_rawgeti(L, LUA_REGISTRYINDEX, lua_toString);
	for(i = 2; i <= n; i++)
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

	trap_SendServerCommand(lent->e->client->ps.clientNum, va("cp \"%s\"", buf));
	return 0;
}

//
// GEntity:cliScore( )
// GEntity:cliScore( newVal:Integer )
//
static int lua_GEntity_cliScore(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->client->pers.netnameTime = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->pers.netnameTime);
	return 1;
}

//
// GEntity:cliSpeed( )
// GEntity:cliSpeed( newVal:Number )
//
static int lua_GEntity_cliSpeed(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		float newVal = luaL_checknumber(L, 2);
		lent->e->client->ps.speed = newVal;
		return 0;
	}

	lua_pushnumber(L, lent->e->client->ps.speed);
	return 1;
}

//
// GEntity:cliStripForces( )
//
static int lua_GEntity_cliStripForces(lua_State *L)
{
	int fp;
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	for (fp = HI_NONE; fp < HI_NUM_HOLDABLE; fp++)
		lent->e->client->ps.fd.forcePowersKnown &= ~(1 << fp);

	return 0;
}

//
// GEntity:cliStripHoldables( )
//
static int lua_GEntity_cliStripHoldables(lua_State *L)
{
	int hi;
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	for (hi = HI_NONE; hi < HI_NUM_HOLDABLE; hi++)
		lent->e->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~ (1 << hi);

	return 0;
}

//
// GEntity:cliStripWeapons( )
//
static int lua_GEntity_cliStripWeapons(lua_State *L)
{
	int wp;
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	for (wp = WP_NONE; wp < WP_NUM_WEAPONS; wp++)
		lent->e->client->ps.stats[STAT_WEAPONS] &= ~ (1 << wp);

	return 0;
}

//
// GEntity:cliTakeForce( forcePower:Integer )
//
static int lua_GEntity_cliTakeForce(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: cliTakeForce( forcePower:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (val < FP_FIRST || val > NUM_FORCE_POWERS)
		return luaL_error(L, "forcePower out of bounds");

	if ((lent->e->client->ps.fd.forcePowersKnown & val))
		lent->e->client->ps.fd.forcePowersKnown &= ~ (1 << val);

	return 0;
}

//
// GEntity:cliTakeHoldable( weapon:Integer )
//
static int lua_GEntity_cliTakeHoldable(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: cliTakeHoldable( holdable:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (val < HI_NONE || val > HI_NUM_HOLDABLE)
		return luaL_error(L, "holdable out of bounds");

	if ((lent->e->client->ps.stats[STAT_HOLDABLE_ITEMS] & val))
		lent->e->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~ (1 << val);

	return 0;
}

//
// GEntity:cliTakeWeapon( weapon:Integer )
//
static int lua_GEntity_cliTakeWeapon(lua_State *L)
{
	int n = lua_gettop(L), val;
	lua_GEntity *lent;

	if (n < 2)
		return luaL_error(L, "syntax: cliTakeWeapon( weapon:Integer )");

	lent = lua_getgentity(L, 1);
	val = luaL_checkinteger(L, 2);

	if (val < WP_NONE || val > WP_NUM_WEAPONS)
		return luaL_error(L, "weapon out of bounds");

	if ((lent->e->client->ps.stats[STAT_WEAPONS] & val))
		lent->e->client->ps.stats[STAT_WEAPONS] &= ~ (1 << val);

	return 0;
}

//
// GEntity:cliTeam( )
// GEntity:cliTeam( newVal:Integer )
//
void SetTeamQuick(gentity_t *ent, int team, qboolean doBegin);
static int lua_GEntity_cliTeam(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checknumber(L, 2);

		if (newVal < TEAM_FREE || newVal > TEAM_NUM_TEAMS)
			return luaL_error(L, "team is out of bounds");

		SetTeamQuick(lent->e, newVal, qtrue);
		return 0;
	}

	lua_pushinteger(L, lent->e->client->sess.sessionTeam);
	return 1;
}

//
// GEntity:cliTeleport( target:GEntity )
// GEntity:cliTeleport( origin:QVector, angles:QVector )
//
static int lua_GEntity_cliTeleport(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent, *ltarg;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n < 2)
		return luaL_error(L, "syntax: cliTeleport( target:GEntity )");

	if (n > 2)
	{
		vec_t *org, *ang;
		org = lua_getvector(L, 2);
		ang = lua_getvector(L, 3);

		TeleportPlayer(lent->e, org, ang);
		return 0;
	}

	ltarg = lua_getgentity(L, 2);
	TeleportPlayer(lent->e, ltarg->e->s.origin, ltarg->e->s.angles);
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_cliThrow(lua_State *L)
{
	return 0;
}

//
// GEntity:cliWeapon( )
// GEntity:cliWeapon( newVal:Integer )
//
static int lua_GEntity_cliWeapon(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);

		if (newVal < WP_NONE || newVal > WP_NUM_WEAPONS)
			return luaL_error(L, "weapon is out of bounds");

		lent->e->client->pers.cmd.weapon = newVal;
		lent->e->client->ps.weapon = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->ps.weapon);
	return 1;
}

//
// GEntity:TODO
//
static int lua_GEntity_usrButtons(lua_State *L)
{
	return 0;
}

//
// GEntity:usrForceSelect( )
// GEntity:usrForceSelect( newVal:Integer )
//
static int lua_GEntity_usrForceSelect(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);

		if (newVal < FP_FIRST || newVal > NUM_FORCE_POWERS)
			return luaL_error(L, "forcePower is out of bounds");

		lent->e->client->pers.cmd.forcesel = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->pers.cmd.forcesel);
	return 1;
}

//
// GEntity:usrForwardMove( )
// GEntity:usrForwardMove( newVal:Integer )
//
static int lua_GEntity_usrForwardMove(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->client->pers.cmd.forwardmove = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->pers.cmd.forwardmove);
	return 1;
}

//
// GEntity:usrGenericCmd( )
// GEntity:usrGenericCmd( newVal:Integer )
//
static int lua_GEntity_usrGenericCmd(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->client->pers.cmd.generic_cmd = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->pers.cmd.generic_cmd);
	return 1;
}

//
// GEntity:usrInventorySelect( )
// GEntity:usrInventorySelect( newVal:Integer )
//
static int lua_GEntity_usrInventorySelect(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->client->pers.cmd.invensel = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->pers.cmd.invensel);
	return 1;
}

//
// GEntity:usrRightMove( )
// GEntity:usrRightMove( newVal:Integer )
//
static int lua_GEntity_usrRightMove(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->client->pers.cmd.rightmove = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->pers.cmd.rightmove);
	return 1;
}

//
// GEntity:usrServerTime( )
// GEntity:usrServerTime( newVal:Integer )
//
static int lua_GEntity_usrServerTime(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->client->pers.cmd.serverTime = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->pers.cmd.serverTime);
	return 1;
}

//
// GEntity:usrUpMove( )
// GEntity:usrUpMove( newVal:Integer )
//
static int lua_GEntity_usrUpMove(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->client->pers.cmd.upmove = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->pers.cmd.upmove);
	return 1;
}

//
// GEntity:usrWeapon( )
// GEntity:usrWeapon( newVal:Integer )
//
static int lua_GEntity_usrWeapon(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (!lent->e->client)
		return luaL_error(L, "entity is not a client");

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);

		if (newVal < WP_NONE || newVal > WP_NUM_WEAPONS)
			return luaL_error(L, "weapon is out of bounds");

		lent->e->client->pers.cmd.weapon = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->client->pers.cmd.weapon);
	return 1;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcArmor(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcCheckLookTarget(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcClearEnemy(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcClearLookTarget(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcEjectAll(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcEject(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcEnemy(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcFaceEnemy(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcFaceEntity(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcFacePosition(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcFindEnemy(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcFindNearestEnemy(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcGoal(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcHealth(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcLookTarget(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcMoveToGoal(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcOldPilot(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcPassengers(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcPickEnemyExt(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcPilot(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcTargetVisible(lua_State *L)
{
	return 0;
}

//
// GEntity:TODO
//
static int lua_GEntity_npcValidEnemy(lua_State *L)
{
	return 0;
}

//
// GEntity:trAngBase( )
// GEntity:trAngBase( newVal:QVector )
//
static int lua_GEntity_trAngBase(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		vec_t *newVal = lua_getvector(L, 2);
		VectorCopy(newVal, lent->e->s.apos.trBase);
		return 0;
	}

	lua_pushvector(L, lent->e->s.apos.trBase);
	return 1;
}

//
// GEntity:trAngDelta( )
// GEntity:trAngDelta( newVal:QVector )
//
static int lua_GEntity_trAngDelta(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		vec_t *newVal = lua_getvector(L, 2);
		VectorCopy(newVal, lent->e->s.apos.trDelta);
		return 0;
	}

	lua_pushvector(L, lent->e->s.apos.trDelta);
	return 1;
}

//
// GEntity:trAngDuration( )
// GEntity:trAngDuration( newVal:Integer )
//
static int lua_GEntity_trAngDuration(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->s.apos.trDuration = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->s.apos.trDuration);
	return 1;
}

//
// GEntity:trAngTime( )
// GEntity:trAngTime( newVal:Integer )
//
static int lua_GEntity_trAngTime(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->s.apos.trTime = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->s.apos.trTime);
	return 1;
}

//
// GEntity:trAngType( )
// GEntity:trAngType( newVal:Integer )
//
static int lua_GEntity_trAngType(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);

		if (newVal < TR_STATIONARY || newVal > TR_GRAVITY)
			return luaL_error(L, "trajectory type is out of bounds");
		lent->e->s.apos.trTime = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->s.apos.trType);
	return 1;
}

//
// GEntity:trOrgBase( )
// GEntity:trOrgBase( newVal:QVector )
//
static int lua_GEntity_trOrgBase(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		vec_t *newVal = lua_getvector(L, 2);
		VectorCopy(newVal, lent->e->s.pos.trBase);
		return 0;
	}

	lua_pushvector(L, lent->e->s.pos.trBase);
	return 1;
}

//
// GEntity:trOrgDelta( )
// GEntity:trOrgDelta( newVal:QVector )
//
static int lua_GEntity_trOrgDelta(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		vec_t *newVal = lua_getvector(L, 2);
		VectorCopy(newVal, lent->e->s.pos.trDelta);
		return 0;
	}

	lua_pushvector(L, lent->e->s.pos.trDelta);
	return 1;
}

//
// GEntity:trOrgDuration( )
// GEntity:trOrgDuration( newVal:Integer )
//
static int lua_GEntity_trOrgDuration(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->s.pos.trDuration = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->s.pos.trDuration);
	return 1;
}

//
// GEntity:trOrgTime( )
// GEntity:trOrgTime( newVal:Integer )
//
static int lua_GEntity_trOrgTime(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);
		lent->e->s.pos.trTime = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->s.pos.trTime);
	return 1;
}

//
// GEntity:trOrgType( )
// GEntity:trOrgType( newVal:Integer )
//
static int lua_GEntity_trOrgType(lua_State *L)
{
	int n = lua_gettop(L);
	lua_GEntity *lent;

	lent = lua_getgentity(L, 1);

	if (n > 1)
	{
		int newVal = luaL_checkinteger(L, 2);

		if (newVal < TR_STATIONARY || newVal > TR_GRAVITY)
			return luaL_error(L, "trajectory type is out of bounds");
		lent->e->s.pos.trTime = newVal;
		return 0;
	}

	lua_pushinteger(L, lent->e->s.pos.trType);

	return 1;
}

static const luaL_reg gentity_ctor[] = {
	{"Find", lua_GEntity_Find},
	{"FromNumber", lua_GEntity_FromNumber},
	{"Pick", lua_GEntity_Pick},
	{"Spawn", lua_GEntity_Spawn},

	{NULL, NULL}
};

static const luaL_reg gentity_meta[] = {
	{"__gc", lua_GEntity_GC},

	{"AbsMaxs", lua_GEntity_AbsMaxs},
	{"AbsMins", lua_GEntity_AbsMins},
	{"Activated", lua_GEntity_Activated},
	{"Angles", lua_GEntity_Angles},

	{"BindBlocked", lua_GEntity_BindBlocked},
	{"BindDie", lua_GEntity_BindDie},
	{"BindPain", lua_GEntity_BindPain},
	{"BindReached", lua_GEntity_BindReached},
	{"BindThink", lua_GEntity_BindThink},
	{"BindTouch", lua_GEntity_BindTouch},
	{"BindUse", lua_GEntity_BindUse},

	{"CallSpawn", lua_GEntity_CallSpawn},

	{"AddClipmask", lua_GEntity_AddClipmask},
	{"HasClipmask", lua_GEntity_HasClipmask},
	{"DelClipmask", lua_GEntity_DelClipmask},

	{"AddContent", lua_GEntity_AddContent},
	{"HasContent", lua_GEntity_HasContent},
	{"DelContent", lua_GEntity_DelContent},

	{"AddEflag", lua_GEntity_AddEflag},
	{"HasEflag", lua_GEntity_HasEflag},
	{"DelEflag", lua_GEntity_DelEflag},

	{"AddFlag", lua_GEntity_AddFlag},
	{"HasFlag", lua_GEntity_HasFlag},
	{"DelFlag", lua_GEntity_DelFlag},

	{"AddSvflag", lua_GEntity_AddSvFlag},
	{"HasSvflag", lua_GEntity_HasSvFlag},
	{"DelSvflag", lua_GEntity_DelSvFlag},

	{"Free", lua_GEntity_Free},
	{"Hand", lua_GEntity_Hand},
	{"HealthMax", lua_GEntity_HealthMax},
	{"Health", lua_GEntity_Health},
	{"Inside", lua_GEntity_Inside},
	{"Key", lua_GEntity_Key},
	{"Link", lua_GEntity_Link},
	{"Maxs", lua_GEntity_Maxs},
	{"Mins", lua_GEntity_Mins},
	{"Model", lua_GEntity_Model},
	{"Move", lua_GEntity_Move},
	{"MoverState", lua_GEntity_MoverState},
	{"NextThink", lua_GEntity_NextThink},
	{"Number", lua_GEntity_Number},
	{"Origin", lua_GEntity_Origin},
	{"Parent", lua_GEntity_Parent},
	{"Position", lua_GEntity_Position},
	{"Rotate", lua_GEntity_Rotate},
	{"Rotation", lua_GEntity_Rotation},
	{"Scale", lua_GEntity_Scale},
	{"Sound", lua_GEntity_Sound},
	{"Speed", lua_GEntity_Speed},
	{"Use", lua_GEntity_Use},
	{"Var", lua_GEntity_Var},
	{"Weapon", lua_GEntity_Weapon},
	
	{"cliAimEntity", lua_GEntity_cliAimEntity},
	{"cliAimOrigin", lua_GEntity_cliAimOrigin},
	{"cliAmmo", lua_GEntity_cliAmmo},
	{"cliAngles", lua_GEntity_cliAngles},
	{"cliAnimLegsTimer", lua_GEntity_cliAnimLegsTimer},
	{"cliAnimLegs", lua_GEntity_cliAnimLegs},
	{"cliAnimTorsoTimer", lua_GEntity_cliAnimTorsoTimer},
	{"cliAnimTorso", lua_GEntity_cliAnimTorso},
	{"cliArmor", lua_GEntity_cliArmor},
	{"cliDrop", lua_GEntity_cliDrop},
	{"cliForce", lua_GEntity_cliForce},
	{"cliForceMax", lua_GEntity_cliForceMax},
	{"cliForcePowerLevel", lua_GEntity_cliForcePowerLevel},
	{"cliGiveForce", lua_GEntity_cliGiveForce},
	{"cliGiveHoldable", lua_GEntity_cliGiveHoldable},
	{"cliGiveWeapon", lua_GEntity_cliGiveWeapon},
	{"cliGravity", lua_GEntity_cliGravity},
	{"cliHasForce", lua_GEntity_cliHasForce},
	{"cliHasHoldable", lua_GEntity_cliHasHoldable},
	{"cliHasWeapon", lua_GEntity_cliHasWeapon},
	{"cliHealthMax", lua_GEntity_cliHealthMax},
	{"cliHealth", lua_GEntity_cliHealth},
	{"cliIP", lua_GEntity_cliIP},
	{"cliLeaveVehicle", lua_GEntity_cliLeaveVehicle},
	{"cliModel", lua_GEntity_cliModel},
	{"cliNameTime", lua_GEntity_cliNameTime},
	{"cliName", lua_GEntity_cliName},
	{"cliOrigin", lua_GEntity_cliOrigin},
	{"cliPing", lua_GEntity_cliPing},
	{"cliPrintChat", lua_GEntity_cliPrintChat},
	{"cliPrintConsole", lua_GEntity_cliPrintConsole},
	{"cliPrintScreen", lua_GEntity_cliPrintScreen},
	{"cliScore", lua_GEntity_cliScore},
	{"cliSpeed", lua_GEntity_cliSpeed},
	{"cliStripForces", lua_GEntity_cliStripForces},
	{"cliStripHoldables", lua_GEntity_cliStripHoldables},
	{"cliStripWeapons", lua_GEntity_cliStripWeapons},
	{"cliTakeForce", lua_GEntity_cliTakeForce},
	{"cliTakeHoldable", lua_GEntity_cliTakeHoldable},
	{"cliTakeWeapon", lua_GEntity_cliTakeWeapon},
	{"cliTeam", lua_GEntity_cliTeam},
	{"cliTeleport", lua_GEntity_cliTeleport},
	{"cliThrow", lua_GEntity_cliThrow},
	{"cliWeapon", lua_GEntity_cliWeapon},
	
	{"usrButtons", lua_GEntity_usrButtons},
	{"usrForceSelect", lua_GEntity_usrForceSelect},
	{"usrForwardMove", lua_GEntity_usrForwardMove},
	{"usrGenericCmd", lua_GEntity_usrGenericCmd},
	{"usrInventorySelect", lua_GEntity_usrInventorySelect},
	{"usrRightMove", lua_GEntity_usrRightMove},
	{"usrServerTime", lua_GEntity_usrServerTime},
	{"usrUpMove", lua_GEntity_usrUpMove},
	{"usrWeapon", lua_GEntity_usrWeapon},
	
	{"npcArmor", lua_GEntity_npcArmor},
	{"npcCheckLookTarget", lua_GEntity_npcCheckLookTarget},
	{"npcClearEnemy", lua_GEntity_npcClearEnemy},
	{"npcClearLookTarget", lua_GEntity_npcClearLookTarget},
	{"npcEjectAll", lua_GEntity_npcEjectAll},
	{"npcEject", lua_GEntity_npcEject},
	{"npcEnemy", lua_GEntity_npcEnemy},
	{"npcFaceEnemy", lua_GEntity_npcFaceEnemy},
	{"npcFaceEntity", lua_GEntity_npcFaceEntity},
	{"npcFacePosition", lua_GEntity_npcFacePosition},
	{"npcFindEnemy", lua_GEntity_npcFindEnemy},
	{"npcFindNearestEnemy", lua_GEntity_npcFindNearestEnemy},
	{"npcGoal", lua_GEntity_npcGoal},
	{"npcHealth", lua_GEntity_npcHealth},
	{"npcLookTarget", lua_GEntity_npcLookTarget},
	{"npcMoveToGoal", lua_GEntity_npcMoveToGoal},
	{"npcOldPilot", lua_GEntity_npcOldPilot},
	{"npcPassengers", lua_GEntity_npcPassengers},
	{"npcPickEnemyExt", lua_GEntity_npcPickEnemyExt},
	{"npcPilot", lua_GEntity_npcPilot},
	{"npcTargetVisible", lua_GEntity_npcTargetVisible},
	{"npcValidEnemy", lua_GEntity_npcValidEnemy},

	{"trAngBase", lua_GEntity_trAngBase},
	{"trAngDelta", lua_GEntity_trAngDelta},
	{"trAngDuration", lua_GEntity_trAngDuration},
	{"trAngTime", lua_GEntity_trAngTime},
	{"trAngType", lua_GEntity_trAngType},
	{"trOrgBase", lua_GEntity_trOrgBase},
	{"trOrgDelta", lua_GEntity_trOrgDelta},
	{"trOrgDuration", lua_GEntity_trOrgDuration},
	{"trOrgTime", lua_GEntity_trOrgTime},
	{"trOrgType", lua_GEntity_trOrgType},

	{NULL, NULL}
};

int luaopen_gentity(lua_State * L)
{
	luaL_newmetatable(L, "Game.GEntity");

	lua_pushstring(L, "__index");
	lua_pushvalue(L, -2);		// pushes the metatable
	lua_settable(L, -3);		// metatable.__index = metatable

	luaL_register(L, NULL, gentity_meta);
	luaL_register(L, "GEntity", gentity_ctor);

	return 1;
}

void lua_pushgentity(lua_State * L, gentity_t * ent)
{
	lua_GEntity     *lent;

	lent = (lua_GEntity*)lua_newuserdata(L, sizeof(lua_GEntity));

	luaL_getmetatable(L, "Game.GEntity");
	lua_setmetatable(L, -2);

	lent->e = ent;
}

lua_GEntity	*lua_getgentity(lua_State * L, int argNum)
{
	void           *ud;
	lua_GEntity		*lent;

	ud = luaL_checkudata(L, argNum, "Game.GEntity");
	luaL_argcheck(L, ud != NULL, argNum, "`entity' expected");

	lent = (lua_GEntity *) ud;
	return lent;
}
