#include "g_local.h"
#include "g_lua.h"

lua_State *g_lua;
int lua_toString;

st_lua_cmd_t st_lua_cmds[MAX_LUA_CMDS];
st_lua_cmd_t st_lua_evts[MAX_LUA_EVTS];
st_lua_cmd_t st_lua_svcmds[MAX_LUA_SVCMDS];

/*
 * Lua Initialization
 */
void G_Lua_Init()
{
	int numMapScripts, numGlobalScripts, i, j, mapScriptlen, globalScriptlen;
	char lstMapScripts[2048], lstGlobalScripts[2048], *mapScriptPtr, *globalScriptPtr;
	vmCvar_t mapname;

	Com_Printf("-------- Lua Initialization ---------\n");
	Com_Printf("- creating lua instance...\n");

	for (i = 0; i < MAX_LUA_CMDS; i++)
	{
		memset(&st_lua_cmds[i], 0, sizeof(st_lua_cmd_t));
		st_lua_cmds[i].name = 0;
		st_lua_cmds[i].function = 0;
	}

	for (i = 0; i < MAX_LUA_EVTS; i++)
	{
		memset(&st_lua_evts[i], 0, sizeof(st_lua_cmd_t));
		st_lua_evts[i].name = 0;
		st_lua_evts[i].function = 0;
	}

	for (i = 0; i < MAX_LUA_SVCMDS; i++)
	{
		memset(&st_lua_svcmds[i], 0, sizeof(st_lua_cmd_t));
		st_lua_svcmds[i].name = 0;
		st_lua_svcmds[i].function = 0;
	}

/*	for (i = 0; i < MAX_GENTITIES; i++)
	{
		for (j = 0; j < MAX_LUA_VARS; j++)
		{
			memset(&g_entities[i].lua_vars[j], 0, sizeof(st_lua_var_t));

			g_entities[i].lua_vars[j].used = qfalse;
			g_entities[i].lua_vars[j].name = 0;
			g_entities[i].lua_vars[j].type = 0;
			g_entities[i].lua_vars[j].string = 0;
			g_entities[i].lua_vars[j].value = 0;
		}
	}*/

	// Init lua
	g_lua = lua_open();

	// Init libs
	luaL_openlibs(g_lua);
	//luaopen_sqlite3(g_lua);

	// Init JA libs
	luaopen_game(g_lua);
	luaopen_gentity(g_lua);
	luaopen_qmath(g_lua);
	luaopen_vector(g_lua);

	lua_getglobal(g_lua, "tostring");
	lua_toString = luaL_ref(g_lua, LUA_REGISTRYINDEX);

	Com_Printf("- looking for map scripts...\n");
	// Load map scripts
	trap_Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
	numMapScripts = trap_FS_GetFileList(va("lua/%s", mapname.string), ".lua", lstMapScripts, sizeof(lstMapScripts) );
	mapScriptPtr = lstMapScripts;
	for (i = 0; i < numMapScripts; i++, mapScriptPtr += mapScriptlen+1)
	{
		char filename[MAX_QPATH];
		mapScriptlen = strlen(mapScriptPtr);
		strcpy(filename, va("lua/%s/", mapname.string));
		strcat(filename, mapScriptPtr);
		G_Lua_LoadScript(filename);
	}

	Com_Printf("- looking for global scripts...\n");
	// Load global scripts
	numGlobalScripts = trap_FS_GetFileList("lua", ".lua", lstGlobalScripts, sizeof(lstGlobalScripts) );
	globalScriptPtr = lstGlobalScripts;
	for (i = 0; i < numGlobalScripts; i++, globalScriptPtr += globalScriptlen+1)
	{
		char filename[MAX_QPATH];
		globalScriptlen = strlen(globalScriptPtr);
		strcpy(filename, "lua/");
		strcat(filename, globalScriptPtr);
		G_Lua_LoadScript(filename);
	}

	Com_Printf("- done loading scripts, lua is ready.\n");
	Com_Printf("-------------------------------------\n");
}

/*
 * Lua script loading
 */
int G_Lua_LoadScript(char *fileName)
{
	fileHandle_t f;
	char fileBuffer[32000];
	int len;

	Com_Printf(" > loading %s...\n", fileName);

	len = trap_FS_FOpenFile(fileName, &f, FS_READ);
	if (!f || len >= 32000)
	{
		return 0;
	}

	trap_FS_Read(fileBuffer, len, f);
	fileBuffer[len] = 0;
	trap_FS_FCloseFile(f);

	if (luaL_loadbuffer(g_lua, fileBuffer, strlen(fileBuffer), fileName))
	{
		G_Lua_ReportError();
		return 0;
	}

	if (lua_pcall(g_lua, 0, 0, 0))
	{
		G_Lua_ReportError();
		return 0;
	}

	return 1;
}

void G_Lua_DoCommand(const char *cmd)
{
	luaL_loadstring(g_lua, cmd);
	if (lua_pcall(g_lua, 0, 0, 0))
		G_Lua_ReportError();
}

int G_Lua_CallClCommand(gentity_t *ent, const char *cmd)
{
	int i;
	for (i = 0; i < MAX_LUA_CMDS; i++)
	{
		if (st_lua_cmds[i].function && !Q_stricmp(st_lua_cmds[i].name, cmd))
		{
			lua_rawgeti(g_lua, LUA_REGISTRYINDEX, st_lua_cmds[i].function);
			lua_pushgentity(g_lua, ent);
			lua_pushinteger(g_lua, trap_Argc());

			if (lua_pcall(g_lua, 2, 1, 0))
			{
				G_Lua_ReportError();
				return 0;
			}
			else
			{
				int res = (int)lua_tonumber(g_lua, -1);
				return res;
			}
		}
	}
	
	return 0;
}

int G_Lua_CallSvCommand(const char *cmd)
{
	int i;
	for (i = 0; i < MAX_LUA_SVCMDS; i++)
	{
		if (st_lua_svcmds[i].function && !Q_stricmp(st_lua_svcmds[i].name, cmd))
		{
			lua_rawgeti(g_lua, LUA_REGISTRYINDEX, st_lua_svcmds[i].function);
			lua_pushinteger(g_lua, trap_Argc());

			if (lua_pcall(g_lua, 1, 1, 0))
			{
				G_Lua_ReportError();
				return 0;
			}
			else
			{
				int res = lua_toboolean(g_lua, -1);
				return res;
			}
		}
	}
	
	return 0;
}

void G_LuaCallFrameEvent(int time)
{
	int i;
	for (i = 0; i < MAX_LUA_EVTS; i++)
	{
		if (!Q_stricmp(st_lua_evts[i].name, "frame"))
		{
			lua_rawgeti(g_lua, LUA_REGISTRYINDEX, st_lua_evts[i].function);
			lua_pushinteger(g_lua, time);

			if (lua_pcall(g_lua, 1, 1, 0))
				G_Lua_ReportError();

			break;
		}
	}
}

void G_LuaCallInitEvent(int levelTime, int randomSeed, int restart)
{
	int i;
	for (i = 0; i < MAX_LUA_EVTS; i++)
	{
		if (!Q_stricmp(st_lua_evts[i].name, "init"))
		{
			lua_rawgeti(g_lua, LUA_REGISTRYINDEX, st_lua_evts[i].function);
			lua_pushinteger(g_lua, levelTime);
			lua_pushinteger(g_lua, randomSeed);
			lua_pushinteger(g_lua, restart);

			if (lua_pcall(g_lua, 3, 0, 0))
				G_Lua_ReportError();

			break;
		}
	}
}

void G_LuaCallShutdownEvent(int restart)
{
	int i;
	for (i = 0; i < MAX_LUA_EVTS; i++)
	{
		if (!Q_stricmp(st_lua_evts[i].name, "shutdown"))
		{
			lua_rawgeti(g_lua, LUA_REGISTRYINDEX, st_lua_evts[i].function);
			lua_pushinteger(g_lua, restart);

			if (lua_pcall(g_lua, 1, 0, 0))
				G_Lua_ReportError();

			break;
		}
	}
}

char *G_LuaCallClientConnectEvent(int clientNum, qboolean firstTime, qboolean isBot, char *userInfo)
{
	int i;
	for (i = 0; i < MAX_LUA_EVTS; i++)
	{
		if (!Q_stricmp(st_lua_evts[i].name, "clientconnect"))
		{
			lua_rawgeti(g_lua, LUA_REGISTRYINDEX, st_lua_evts[i].function);
			lua_pushinteger(g_lua, clientNum);
			lua_pushboolean(g_lua, firstTime);
			lua_pushboolean(g_lua, isBot);
			lua_pushstring(g_lua, userInfo);

			if (lua_pcall(g_lua, 4, 1, 0))
				G_Lua_ReportError();
			else
				return (char*)lua_tostring(g_lua, -1);

			break;
		}
	}

	return NULL;
}

void G_LuaCallClientDisconnectEvent(int clientNum)
{
	int i;
	for (i = 0; i < MAX_LUA_EVTS; i++)
	{
		if (!Q_stricmp(st_lua_evts[i].name, "clientdisconnect"))
		{
			lua_rawgeti(g_lua, LUA_REGISTRYINDEX, st_lua_evts[i].function);
			lua_pushinteger(g_lua, clientNum);

			if (lua_pcall(g_lua, 1, 0, 0))
				G_Lua_ReportError();

			break;
		}
	}
}

void G_LuaCallClientBeginEvent(int clientNum)
{
	int i;
	for (i = 0; i < MAX_LUA_EVTS; i++)
	{
		if (!Q_stricmp(st_lua_evts[i].name, "clientbegin"))
		{
			lua_rawgeti(g_lua, LUA_REGISTRYINDEX, st_lua_evts[i].function);
			lua_pushinteger(g_lua, clientNum);

			if (lua_pcall(g_lua, 1, 0, 0))
				G_Lua_ReportError();

			break;
		}
	}
}

int G_LuaCallSayEvent( gentity_t *ent, gentity_t *target, int mode, const char *chatText )
{
	int i;
	for (i = 0; i < MAX_LUA_EVTS; i++)
	{
		if (!Q_stricmp(st_lua_evts[i].name, "chat"))
		{
			lua_rawgeti(g_lua, LUA_REGISTRYINDEX, st_lua_evts[i].function);
			lua_pushgentity(g_lua, ent);
			lua_pushgentity(g_lua, target);
			lua_pushinteger(g_lua, mode);
			lua_pushstring(g_lua, chatText);

			if (lua_pcall(g_lua, 4, 1, 0))
				G_Lua_ReportError();
			else
			{
				return lua_toboolean(g_lua, -1);
			}

			break;
		}
	}
	
	return 0;
}

/*
 * Lua error reporting
 */
void G_Lua_ReportError()
{
	char errorMsg[512];

	Q_strncpyz(errorMsg, lua_tostring(g_lua, -1), sizeof(errorMsg));
	luaL_where(g_lua, 1);
	Com_Printf("lua error: %s (%s)\n", errorMsg, lua_tostring(g_lua, -1));
	lua_pop(g_lua, 2);
}

/*
 * Lua Destruction
 */
void G_Lua_Shutdown()
{
	lua_close(g_lua);
}
