#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#define MAX_LUA_CMDS 128
#define MAX_LUA_EVTS 128
#define MAX_LUA_SVCMDS 128


typedef struct st_lua_cmd {
	char *name;
	int function;
} st_lua_cmd_t;

extern st_lua_cmd_t st_lua_cmds[MAX_LUA_CMDS];
extern st_lua_cmd_t st_lua_evts[MAX_LUA_EVTS];
extern st_lua_cmd_t st_lua_svcmds[MAX_LUA_SVCMDS];

// g_lua.cpp
extern lua_State *g_lua;
extern int lua_toString;

//int luaopen_sqlite3(lua_State * L);

extern void G_Lua_Init();
extern int G_Lua_LoadScript(char *fileName);
extern void G_Lua_DoCommand(const char *cmd);
extern int G_Lua_CallClCommand(gentity_t *ent, const char *cmd);
extern int G_Lua_CallSvCommand(const char *cmd);
extern void G_Lua_ReportError();
extern void G_Lua_Shutdown();

extern char *G_LuaCallClientConnectEvent(int clientNum, qboolean firstTime, qboolean isBot, char *userInfo);
extern void G_LuaCallClientDisconnectEvent(int clientNum);
extern void G_LuaCallClientBeginEvent(int clientNum);
extern void G_LuaCallFrameEvent(int time);
extern void G_LuaCallInitEvent(int levelTime, int randomSeed, int restart);
extern int G_LuaCallSayEvent( gentity_t *ent, gentity_t *target, int mode, const char *chatText );
extern void G_LuaCallShutdownEvent(int restart);

// lua_game.cpp
extern int luaopen_game(lua_State * L);

// lua_qmath.cpp
extern int luaopen_qmath(lua_State * L);

// lua_vector.cpp
extern int luaopen_vector(lua_State * L);
extern void lua_pushvector(lua_State * L, vec3_t v);
extern vec_t *lua_getvector(lua_State * L, int argNum);

// lua_gentity.cpp
typedef struct
{
	gentity_t      *e;
} lua_GEntity;

extern int             luaopen_gentity(lua_State * L);
extern void            lua_pushgentity(lua_State * L, gentity_t * ent);
extern lua_GEntity    *lua_getgentity(lua_State * L, int argNum);
