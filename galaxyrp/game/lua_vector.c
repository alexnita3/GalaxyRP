/*
===========================================================================
Copyright (C) 2007 Robert Beckebans <trebor_7@users.sourceforge.net>

This file is part of XreaL source code.

XreaL source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

XreaL source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with XreaL source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// lua_vector.c -- vector library for Lua

#include "g_local.h"
#include "g_lua.h"

static int Vector_New(lua_State * L)
{
	vec_t          *v;

	v = (vec_t*)lua_newuserdata(L, sizeof(vec3_t));

	luaL_getmetatable(L, "Vector");
	lua_setmetatable(L, -2);

	VectorClear(v);

	return 1;
}

static int Vector_Construct(lua_State * L)
{
	vec_t          *v;

	v = (vec_t*)lua_newuserdata(L, sizeof(vec3_t));

	luaL_getmetatable(L, "Vector");
	lua_setmetatable(L, -2);

	v[0] = luaL_optnumber(L, 1, 0);
	v[1] = luaL_optnumber(L, 2, 0);
	v[2] = luaL_optnumber(L, 3, 0);

	return 1;
}

static int Vector_Set(lua_State * L)
{
	vec_t          *v;

	v = (vec_t*)lua_getvector(L, 1);

	v[0] = luaL_optnumber(L, 2, 0);
	v[1] = luaL_optnumber(L, 3, 0);
	v[2] = luaL_optnumber(L, 4, 0);

	return 1;
}

static int Vector_Clear(lua_State * L)
{
	vec_t          *a;

	a = lua_getvector(L, 1);

	VectorClear(a);

	return 1;
}

static int Vector_Add(lua_State * L)
{
	vec_t          *a, *b, *c;

	a = lua_getvector(L, 1);
	b = lua_getvector(L, 2);
	c = lua_getvector(L, 3);

	VectorAdd(a, b, c);

	return 1;
}

static int Vector_Snap(lua_State * L)
{
	vec_t          *a;

	a = lua_getvector(L, 1);
	SnapVector(a);

	return 1;
}

static int Vector_Subtract(lua_State * L)
{
	vec_t          *a, *b, *c;

	a = lua_getvector(L, 1);
	b = lua_getvector(L, 2);
	c = lua_getvector(L, 3);

	VectorSubtract(a, b, c);

	return 1;
}

static int Vector_Scale(lua_State * L)
{
	vec_t          *a, b, *c;

	a = lua_getvector(L, 1);
	b = luaL_checknumber(L, 2);
	c = lua_getvector(L, 3);

	VectorScale(a, b, c);

	return 1;
}

static int Vector_Length(lua_State * L)
{
	vec_t          *a;
	vec_t           len;

	a = lua_getvector(L, 1);

	len = VectorLength(a);
	lua_pushnumber(L, len);

	return 1;
}

static int Vector_Normalize(lua_State * L)
{
	vec_t          *a;
	vec_t           len;

	a = lua_getvector(L, 1);

	len = VectorNormalize(a);
	lua_pushnumber(L, len);

	return 1;
}

static int Vector_NormalizeFast(lua_State * L)
{
	vec_t          *a;

	a = lua_getvector(L, 1);

	VectorNormalizeFast(a);

	return 1;
}

static int Vector_RotatePointAround(lua_State * L)
{
	vec_t          *dst;
	vec_t          *dir;
	vec_t          *point;
	vec_t           degrees;

	dst = lua_getvector(L, 1);
	dir = lua_getvector(L, 2);
	point = lua_getvector(L, 3);
	degrees = luaL_checknumber(L, 4);

	RotatePointAroundVector(dst, dir, point, degrees);

	return 1;
}

static int Vector_Perpendicular(lua_State * L)
{
	vec_t          *dst;
	vec_t          *src;

	dst = lua_getvector(L, 1);
	src = lua_getvector(L, 2);

	PerpendicularVector(dst, src);

	return 1;
}

// *INDENT-OFF*
static int Vector_Index(lua_State * L)
{
	vec_t          *v;
	const char     *i;
	
	v = lua_getvector(L, 1);
	i = luaL_checkstring(L, 2);
	
	switch (*i)
	{
		case '0': case 'x': case 'r': lua_pushnumber(L, v[0]); break;
		case '1': case 'y': case 'g': lua_pushnumber(L, v[1]); break;
		case '2': case 'z': case 'b': lua_pushnumber(L, v[2]); break;
		default: lua_pushnil(L); break;
		//default: lua_pushvector(L, v); break;
	}
	
	return 1;
}

static int Vector_NewIndex(lua_State * L)
{
	vec_t          *v;
	const char     *i;
	vec_t           t;
	
	v = lua_getvector(L, 1);
	i = luaL_checkstring(L, 2);
	t = luaL_checknumber(L, 3);
	
	switch (*i)
	{
		case '0': case 'x': case 'r': v[0] = t; break;
		case '1': case 'y': case 'g': v[1] = t; break;
		case '2': case 'z': case 'b': v[2] = t; break;
		default: break;
	}
	
	return 1;
}
// *INDENT-ON*

/*
static int Vector_GetElem(lua_State * L)
{
	vec_t          *v;
	int				index;
	
	v = lua_getvector(L, 1);
	index = (int) luaL_checkinteger(L, 2);

	luaL_argcheck(L, index >=0 && index <= 2, 2, "index out of range");

	lua_pushnumber(L, v[index]);
	
	return 1;
}

static int Vector_SetElem(lua_State * L)
{
	vec_t          *v;
	int				index;
	vec_t           t;
	
	v = lua_getvector(L, 1);
	index = (int) luaL_checkinteger(L, 2);
	t = luaL_checknumber(L, 3);
	
	luaL_argcheck(L, index >=0 && index <= 2, 2, "index out of range");

	v[index] = t;
	
	return 1;
}
*/

static int Vector_AddOperator(lua_State * L)
{
	vec_t          *a, *b;
	vec3_t          c;

	a = lua_getvector(L, 1);
	b = lua_getvector(L, 2);

	VectorAdd(a, b, c);

	lua_pushvector(L, c);

	return 1;
}

static int Vector_SubOperator(lua_State * L)
{
	vec_t          *a, *b;
	vec3_t          c;

	a = lua_getvector(L, 1);
	b = lua_getvector(L, 2);

	VectorSubtract(a, b, c);

	lua_pushvector(L, c);

	return 1;
}

static int Vector_DotOperator(lua_State * L)
{
	vec_t          *a, *b;

	a = lua_getvector(L, 1);
	b = lua_getvector(L, 2);

	lua_pushnumber(L, DotProduct(a, b));

	return 1;
}

static int Vector_NegateOperator(lua_State * L)
{
	vec_t          *a;
	vec3_t          b;

	a = lua_getvector(L, 1);

	VectorNegate(a, b);

	lua_pushvector(L, b);

	return 1;
}

static int Vector_GC(lua_State * L)
{
//  G_Printf("Lua says bye to vector = %p\n", lua_getvector(L));

	return 0;
}

static int Vector_ToString(lua_State * L)
{
	vec_t          *vec;

	vec = lua_getvector(L, 1);
	lua_pushstring(L, va("(%i %i %i)", (int)vec[0], (int)vec[1], (int)vec[2]));

	return 1;
}

static const luaL_reg Vector_ctor[] = {
	{"New", Vector_New},
	{"Construct", Vector_Construct},
	{"Set", Vector_Set},
	{"Clear", Vector_Clear},
	{"Add", Vector_Add},
	{"Subtract", Vector_Subtract},
	{"Scale", Vector_Scale},
	{"Length", Vector_Length},
	{"Normalize", Vector_Normalize},
	{"NormalizeFast", Vector_NormalizeFast},
	{"RotatePointAround", Vector_RotatePointAround},
	{"Perpendicular", Vector_Perpendicular},
	{"Snap", Vector_Snap},
	{NULL, NULL}
};

static const luaL_reg Vector_meta[] = {
	{"__index", Vector_Index},
	{"__newindex", Vector_NewIndex},
	{"__add", Vector_AddOperator},
	{"__sub", Vector_SubOperator},
	{"__mul", Vector_DotOperator},
	{"__unm", Vector_NegateOperator},
	{"__gc", Vector_GC},
	{"__tostring", Vector_ToString},
	{NULL, NULL}
};

int luaopen_vector(lua_State * L)
{
	luaL_newmetatable(L, "Vector");

	luaL_register(L, NULL, Vector_meta);
	luaL_register(L, "Vector", Vector_ctor);

#if 0
	// now the stack has the metatable at index 1 and `vector' at index 2
	lua_pushstring(L, "__index");
	lua_pushstring(L, "Get");
	lua_gettable(L, 2);			/* get array.get */
	lua_settable(L, 1);			/* metatable.__index = array.get */

	lua_pushstring(L, "__newindex");
	lua_pushstring(L, "Set");
	lua_gettable(L, 2);			/* get array.set */
	lua_settable(L, 1);			/* metatable.__newindex = array.set */
#endif

	return 1;
}

void lua_pushvector(lua_State * L, vec3_t v)
{
	vec_t          *vec;

	vec = (vec_t*)lua_newuserdata(L, sizeof(vec3_t));

	luaL_getmetatable(L, "Vector");
	lua_setmetatable(L, -2);

	VectorCopy(v, vec);
}

vec_t          *lua_getvector(lua_State * L, int argNum)
{
	void           *ud;

	ud = luaL_checkudata(L, argNum, "Vector");
	luaL_argcheck(L, ud != NULL, argNum, "`vector' expected");
	return (vec_t *) ud;
}
