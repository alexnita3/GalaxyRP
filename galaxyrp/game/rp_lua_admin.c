/*
===========================================================================
GLX_U: [Lua] /scripts (in game, needs the Scripts admin power) and the rcon commands lua_list,
lua_reload, lua_enable, lua_disable, lua_errors, lua_stats and lua_exec. Both share the code below;
in game the output goes to the admin's console in prints under the 1022-character limit, from rcon
it goes to the server console.

GLX_U: every folder in lua/plugins runs, and the map's own lua/maps/<mapname> scripts; enable and disable
last until the next map (or reload all). A plugin is kept off for good by moving its folder out. lua_exec runs a line of Lua in its own
"@console" environment, from rcon and the server console only; it does not exist in game. GLX_U: it needs no
switch any more -- rp_lua_allowexec was an ordinary cvar that rcon could turn on, so it protected nothing, and
the sandbox gives a line of Lua no more than rcon has. Every line is written to the server log.
===========================================================================
*/

#include "rp_lua_local.h"
#include <time.h>

#define RP_LUA_OUT_LIMIT	960		// one print command, well under the engine's 1022

static char rpOut[RP_LUA_OUT_LIMIT + RP_LUA_TEXT_MAX + 4];

void RP_LuaAdmin_Flush( gentity_t *ent ) {
	if ( ent && rpOut[0] ) {
		trap->SendServerCommand( ent->s.number, va( "print \"%s\"", rpOut ) );
	}
	rpOut[0] = '\0';
}

void RP_LuaAdmin_Out( gentity_t *ent, const char *line ) {
	char clean[RP_LUA_TEXT_MAX + 1];

	if ( !ent ) {
		trap->Print( "%s\n", line );
		return;
	}
	RP_Lua_CleanText( clean, sizeof( clean ), line, qfalse );
	if ( strlen( rpOut ) + strlen( clean ) + 1 > RP_LUA_OUT_LIMIT ) {
		RP_LuaAdmin_Flush( ent );
	}
	Q_strcat( rpOut, sizeof( rpOut ), clean );
	Q_strcat( rpOut, sizeof( rpOut ), "\n" );
}

static qboolean RP_LuaAdmin_Running( gentity_t *ent ) {
	if ( !rp_lua.integer && !rl.L ) {
		RP_LuaAdmin_Out( ent, "Lua scripting is off (rp_lua 0)." );
		return qfalse;
	}
	if ( !rl.L ) {
		RP_LuaAdmin_Out( ent, "Lua scripting is not running on this map: it failed to start (see the server log), or rp_lua was set to 1 after the map started -- use /scripts reload all." );
		return qfalse;
	}
	return qtrue;
}

/*
================
list
================
*/
static void RP_LuaAdmin_List( gentity_t *ent ) {
	static char folders[RP_LUA_MAX_FOLDERS][RP_LUA_PLUGIN_NAME_MAX + 1];
	char name[RP_LUA_CMD_NAME_SIZE], help[RP_LUA_CMD_HELP_SIZE];
	int i, shown, plugin, numFolders;

	if ( !RP_LuaAdmin_Running( ent ) ) {
		return;
	}
	RP_LuaAdmin_Out( ent, va( "^3Lua 5.4^7: memory %d KB of %d MB%s", (int)( rl.memUsed / 1024 ), (int)( rl.memCap / ( 1024 * 1024 ) ),
		rp_lua.integer ? "" : " ^1(rp_lua is 0: scripts stop at the next map start or /scripts reload all)" ) );

	RP_LuaAdmin_Out( ent, "^3Plugins^7 (lua/plugins):" );
	shown = 0;
	for ( i = 0; i < RP_LUA_MAX_PLUGINS; i++ ) {
		rpLuaPlugin_t *p = &rl.plugins[i];
		if ( !p->inUse || p->isMap ) {
			continue;
		}
		shown++;
		if ( p->enabled ) {
			RP_LuaAdmin_Out( ent, va( "  ^2on^7  %s%s%s%s%s, %d error(s)", p->name, p->version[0] ? " " : "", p->version,
				p->title[0] ? " - " : "", p->title, p->errTotal ) );
		} else {
			RP_LuaAdmin_Out( ent, va( "  ^1off^7 %s: %s", p->name, p->failReason[0] ? p->failReason : "off" ) );
		}
	}
	// GLX_U: the plugin folders not running now: switched off this map, or put there since it started
	numFolders = RP_Lua_PluginFolders( folders, RP_LUA_MAX_FOLDERS );
	for ( i = 0; i < numFolders; i++ ) {
		if ( RP_Lua_FindPlugin( folders[i] ) < 0 ) {
			shown++;
			RP_LuaAdmin_Out( ent, va( "  ^1off^7 %s: not running until the next map (/scripts enable %s starts it now)", folders[i], folders[i] ) );
		}
	}
	if ( !shown ) {
		RP_LuaAdmin_Out( ent, "  none. Put a plugin in lua/plugins/<name>/plugin.lua: it starts with the next map, or now with /scripts enable <name>." );
	}

	{
		char dir[MAX_QPATH];

		trap->Cvar_VariableStringBuffer( "mapname", dir, sizeof( dir ) );
		RP_LuaAdmin_Out( ent, va( "^3Map scripts^7 (lua/maps/%s):", dir ) );
		shown = 0;
		for ( i = 0; i < RP_LUA_MAX_PLUGINS; i++ ) {
			rpLuaPlugin_t *p = &rl.plugins[i];
			if ( !p->inUse || !p->isMap ) {
				continue;
			}
			shown++;
			if ( p->enabled ) {
				RP_LuaAdmin_Out( ent, va( "  ^2on^7  %s, %d error(s)", p->name, p->errTotal ) );
			} else {
				RP_LuaAdmin_Out( ent, va( "  ^1off^7 %s: %s", p->name, p->failReason[0] ? p->failReason : "off" ) );
			}
		}
		if ( !shown ) {
			RP_LuaAdmin_Out( ent, "  none for this map." );
		}
	}

	RP_LuaAdmin_Out( ent, "^3Script commands^7:" );
	shown = 0;
	for ( i = 0; i < RP_LuaApi_CommandCount(); i++ ) {
		if ( !RP_LuaApi_CommandInfo( i, name, sizeof( name ), help, sizeof( help ), &plugin ) ) {
			continue;
		}
		shown++;
		RP_LuaAdmin_Out( ent, va( "  ^3/%s^7%s%s (%s)", name, help[0] ? " - " : "", help, rl.plugins[plugin].name ) );
	}
	if ( !shown ) {
		RP_LuaAdmin_Out( ent, "  none." );
	}
}

/*
================
reload, enable, disable
================
*/
static void RP_LuaAdmin_ReloadAll( gentity_t *ent ) {
	int i, on = 0, off = 0;

	if ( rl.L ) {
		// the scripts' shutdown handlers run first, then every script entity goes (a map change frees
		// them with the map; here the map stays). RP_Lua_Shutdown() itself is the map-end path.
		RP_LuaApi_FireEvent( RPLE_SHUTDOWN, -1, -1, 0, NULL, -1, NULL );
		RP_LuaWorld_FreeAll();
		RP_LuaStore_Close();
		RP_Lua_CloseState();
	}
	RP_Lua_Shutdown();
	RP_Lua_Init();
	if ( !rl.L ) {
		RP_LuaAdmin_Out( ent, rp_lua.integer ? "Lua could not start; see the server log." : "Scripts stopped (rp_lua 0)." );
		return;
	}
	for ( i = 0; i < RP_LUA_MAX_PLUGINS; i++ ) {
		if ( rl.plugins[i].inUse ) {
			if ( rl.plugins[i].enabled ) {
				on++;
			} else {
				off++;
			}
		}
	}
	RP_LuaAdmin_Out( ent, va( "Reloaded all scripts: %d running, %d failed.%s", on, off, off ? " See /scripts errors." : "" ) );
}

// loads (or loads again) one plugin slot and runs its init handlers; qtrue when it is running
static qboolean RP_LuaAdmin_Load( int i ) {
	RP_LuaStore_Open();		// outside the plugin's time budget (a no-op when already open)
	RP_LuaStore_Flush();
	rl.plugins[i].storeBytes = -1;
	rl.plugins[i].heldNotices = 0;
	if ( !RP_Lua_LoadPluginFile( i ) ) {
		return qfalse;
	}
	RP_LuaApi_FireEvent( RPLE_INIT, i, -1, 0, NULL, -1, NULL );
	return rl.plugins[i].enabled;
}

static void RP_LuaAdmin_Reload( gentity_t *ent, const char *arg ) {
	int i;

	if ( !arg[0] || !Q_stricmp( arg, "all" ) ) {
		if ( !rp_lua.integer && !rl.L ) {
			RP_LuaAdmin_Out( ent, "Lua scripting is off (rp_lua 0)." );
			return;
		}
		RP_LuaAdmin_ReloadAll( ent );
		return;
	}
	if ( !RP_LuaAdmin_Running( ent ) ) {
		return;
	}
	i = RP_Lua_FindPlugin( arg );
	if ( i < 0 ) {
		RP_LuaAdmin_Out( ent, va( "No plugin or map script called %s is loaded. Use /scripts enable %s to start a plugin.", arg, arg ) );
		return;
	}
	if ( rl.plugins[i].enabled ) {
		RP_Lua_DisablePlugin( i, "reloading" );
	}
	if ( RP_LuaAdmin_Load( i ) ) {
		RP_LuaAdmin_Out( ent, va( "Reloaded %s.", rl.plugins[i].name ) );
	} else {
		RP_LuaAdmin_Out( ent, va( "%s failed to load: %s. See /scripts errors.", rl.plugins[i].name, rl.plugins[i].failReason ) );
	}
}

// GLX_U: "map:<file>", a script of this map's lua/maps/<mapname> folder (RP_Lua_LoadMapScripts names them so)
static qboolean RP_LuaAdmin_MapScriptName( const char *name ) {
	const char *file = name + 4;
	int len = (int)strlen( name );

	return ( !Q_stricmpn( name, "map:", 4 ) && file[0] && !strchr( file, '/' ) && !strchr( file, '\\' ) && len < RP_LUA_NAME_SIZE
		&& len > 8 && !Q_stricmp( name + len - 4, ".lua" ) ) ? qtrue : qfalse;
}

// enable: a plugin (its folder in lua/plugins) or a map script, started now; at the next map every folder and this
// map's scripts start anyway, so this is for one switched off this map, or added since it started
static void RP_LuaAdmin_Enable( gentity_t *ent, const char *name ) {
	char dir[MAX_QPATH];
	qboolean isMap = RP_LuaAdmin_MapScriptName( name );
	int i;

	if ( !isMap && !RP_Lua_ValidPluginName( name ) ) {
		RP_LuaAdmin_Out( ent, va( "Usage: enable <plugin> or enable map:<file>. A plugin name is the name of its folder in lua/plugins: lower-case letters, digits and _, up to %d.", RP_LUA_PLUGIN_NAME_MAX ) );
		return;
	}
	if ( !RP_LuaAdmin_Running( ent ) ) {
		return;
	}
	i = RP_Lua_FindPlugin( name );
	if ( i >= 0 && rl.plugins[i].enabled ) {
		RP_LuaAdmin_Out( ent, va( "%s is already on.", name ) );
		return;
	}
	if ( i < 0 ) {
		if ( isMap ) {
			char mapname[MAX_QPATH];
			trap->Cvar_VariableStringBuffer( "mapname", mapname, sizeof( mapname ) );
			Com_sprintf( dir, sizeof( dir ), "lua/maps/%s", mapname );
			i = RP_Lua_AddPlugin( name, qtrue, dir, name + 4 );
		} else {
			Com_sprintf( dir, sizeof( dir ), "lua/plugins/%s", name );
			i = RP_Lua_AddPlugin( name, qfalse, dir, "plugin.lua" );
		}
		if ( i < 0 ) {
			RP_LuaAdmin_Out( ent, va( "No room for another plugin (at most %d plugins and map scripts).", RP_LUA_MAX_PLUGINS ) );
			return;
		}
	}
	if ( !RP_LuaAdmin_Load( i ) ) {
		RP_LuaAdmin_Out( ent, va( "%s failed to load: %s. See /scripts errors.", name, rl.plugins[i].failReason ) );
		return;
	}
	RP_LuaAdmin_Out( ent, isMap ? va( "%s is on. It starts with this map every time while the file is in %s.", name, rl.plugins[i].dir )
		: va( "%s is on. It starts with every map while its folder is in lua/plugins.", name ) );
}

// disable: off until the next map (or reload all); to keep it off, its folder (or file) has to go
static void RP_LuaAdmin_Disable( gentity_t *ent, const char *name ) {
	char where[MAX_QPATH];
	qboolean isMap = RP_LuaAdmin_MapScriptName( name );
	int i;

	if ( !isMap && !RP_Lua_ValidPluginName( name ) ) {
		RP_LuaAdmin_Out( ent, "Usage: disable <plugin> or disable map:<file>" );
		return;
	}
	i = RP_Lua_FindPlugin( name );
	if ( i < 0 ) {
		RP_LuaAdmin_Out( ent, va( "No plugin or map script called %s is loaded.", name ) );
		return;
	}
	Q_strncpyz( where, rl.plugins[i].dir, sizeof( where ) );
	RP_Lua_DisablePlugin( i, "switched off with /scripts disable" );
	RP_LuaStore_Flush();
	rl.plugins[i].inUse = qfalse;
	RP_LuaAdmin_Out( ent, isMap ? va( "%s is off until the next map. To keep it off, move the file out of %s.", name, where )
		: va( "%s is off until the next map. To keep it off, move its folder out of lua/plugins.", name ) );
}

/*
================
errors, stats
================
*/
static void RP_LuaAdmin_Errors( gentity_t *ent ) {
	int i, n;

	if ( !rl.errorCount ) {
		RP_LuaAdmin_Out( ent, "No Lua errors or warnings on this map." );
		return;
	}
	RP_LuaAdmin_Out( ent, va( "^3Last %d Lua error(s) and warning(s)^7, oldest first (full messages are in the server log):", rl.errorCount ) );
	for ( n = 0; n < rl.errorCount; n++ ) {
		time_t t;
		struct tm *tm;
		char when[16] = "--:--:--";

		i = ( rl.errorNext - rl.errorCount + n + RP_LUA_ERROR_RING ) % RP_LUA_ERROR_RING;
		t = (time_t)rl.errors[i].time;
		tm = localtime( &t );
		if ( tm ) {
			strftime( when, sizeof( when ), "%H:%M:%S", tm );
		}
		RP_LuaAdmin_Out( ent, va( "  %s ^3%s^7: %s", when, rl.errors[i].plugin, rl.errors[i].line ) );
	}
}

static void RP_LuaAdmin_Stats( gentity_t *ent ) {
	int i;

	if ( !RP_LuaAdmin_Running( ent ) ) {
		return;
	}
	RP_LuaAdmin_Out( ent, va( "^3Lua^7: memory %d KB of %d MB, frame budget %d ms, %d value(s) waiting to be saved",
		(int)( rl.memUsed / 1024 ), (int)( rl.memCap / ( 1024 * 1024 ) ), rp_lua_framems.integer, RP_LuaStore_PendingCount() ) );
	for ( i = 0; i <= RP_LUA_CONSOLE; i++ ) {
		rpLuaPlugin_t *p = &rl.plugins[i];
		if ( !p->inUse ) {
			continue;
		}
		RP_LuaAdmin_Out( ent, va( "  ^3%s^7 (%s): %u call(s), %dk instructions, %d ms, %d timer(s), %d task(s), %d entit%s, %d deferred, %d error(s)%s",
			p->name, p->enabled ? "on" : "off", p->calls, (int)( p->instructions / 1000.0 ), p->ms, p->numTimers, p->numTasks,
			p->numEnts, p->numEnts == 1 ? "y" : "ies", p->deferred, p->errTotal,
			p->storeBytes >= 0 ? va( ", %d bytes stored", p->storeBytes ) : "" ) );
		if ( p->creditsGiven || p->creditsTaken || p->xpGiven ) {	// API 4
			RP_LuaAdmin_Out( ent, va( "    this map: %d credit(s) given, %d taken, %d XP given", p->creditsGiven, p->creditsTaken, p->xpGiven ) );
		}
	}
}

/*
================
/scripts
================
*/
void Cmd_Scripts_f( gentity_t *ent ) {
	char sub[MAX_TOKEN_CHARS], arg[MAX_TOKEN_CHARS];

	if ( !check_admin_command( ent, ADM_SCRIPTS, qtrue ) ) {
		return;
	}
	trap->Argv( 1, sub, sizeof( sub ) );
	trap->Argv( 2, arg, sizeof( arg ) );
	rpOut[0] = '\0';

	if ( !sub[0] ) {
		RP_LuaAdmin_List( ent );
	} else if ( !Q_stricmp( sub, "reload" ) ) {
		RP_LuaAdmin_Reload( ent, arg );
		G_LogPrintf( "Lua: %s used /scripts reload %s\n", ent->client->pers.netname, arg[0] ? arg : "all" );
	} else if ( !Q_stricmp( sub, "enable" ) ) {
		RP_LuaAdmin_Enable( ent, arg );
		G_LogPrintf( "Lua: %s used /scripts enable %s\n", ent->client->pers.netname, arg );
	} else if ( !Q_stricmp( sub, "disable" ) ) {
		RP_LuaAdmin_Disable( ent, arg );
		G_LogPrintf( "Lua: %s used /scripts disable %s\n", ent->client->pers.netname, arg );
	} else if ( !Q_stricmp( sub, "errors" ) ) {
		RP_LuaAdmin_Errors( ent );
	} else if ( !Q_stricmp( sub, "stats" ) ) {
		RP_LuaAdmin_Stats( ent );
	} else {
		RP_LuaAdmin_Out( ent, "Usage: /scripts [reload [plugin|all] | enable <plugin> | disable <plugin> | errors | stats]" );
	}
	RP_LuaAdmin_Flush( ent );
}

/*
================
rcon
================
*/
static const char *RP_LuaAdmin_Arg1( void ) {
	static char arg[MAX_TOKEN_CHARS];
	trap->Argv( 1, arg, sizeof( arg ) );
	return arg;
}

void Svcmd_LuaList_f( void ) {
	RP_LuaAdmin_List( NULL );
}

void Svcmd_LuaReload_f( void ) {
	RP_LuaAdmin_Reload( NULL, RP_LuaAdmin_Arg1() );
}

void Svcmd_LuaEnable_f( void ) {
	RP_LuaAdmin_Enable( NULL, RP_LuaAdmin_Arg1() );
}

void Svcmd_LuaDisable_f( void ) {
	RP_LuaAdmin_Disable( NULL, RP_LuaAdmin_Arg1() );
}

void Svcmd_LuaErrors_f( void ) {
	RP_LuaAdmin_Errors( NULL );
}

void Svcmd_LuaStats_f( void ) {
	RP_LuaAdmin_Stats( NULL );
}

static int RP_LuaAdmin_ExecWorker( lua_State *L, void *data ) {
	const char *code = (const char *)data;
	rpLuaPlugin_t *p = &rl.plugins[RP_LUA_CONSOLE];
	int n, i;

	if ( p->envRef == LUA_NOREF ) {
		RP_Lua_NewEnv( L, p );
	}
	if ( luaL_loadbufferx( L, code, strlen( code ), "=lua_exec", "t" ) != LUA_OK ) {
		return lua_error( L );
	}
	lua_rawgeti( L, LUA_REGISTRYINDEX, p->envRef );
	if ( !lua_setupvalue( L, -2, 1 ) ) {
		lua_pop( L, 1 );
	}
	lua_call( L, 0, LUA_MULTRET );
	n = lua_gettop( L );
	for ( i = 1; i <= n; i++ ) {
		trap->Print( "%s\n", luaL_tolstring( L, i, NULL ) );
		lua_pop( L, 1 );
	}
	return 0;
}

void Svcmd_LuaExec_f( void ) {
	rpLuaPlugin_t *p = &rl.plugins[RP_LUA_CONSOLE];
	const char *code;

	if ( !RP_LuaAdmin_Running( NULL ) ) {
		return;
	}
	code = ConcatArgs( 1 );
	if ( !code[0] ) {
		trap->Print( "Usage: lua_exec <Lua code>. The console removes double quotes and splits at ';', so use 'single quotes' or [[...]].\n" );
		return;
	}
	if ( !p->inUse ) {
		memset( p, 0, sizeof( *p ) );
		p->inUse = qtrue;
		p->enabled = qtrue;
		p->envRef = LUA_NOREF;
		p->storeBytes = -1;
		Q_strncpyz( p->name, "@console", sizeof( p->name ) );
		Q_strncpyz( p->storeName, p->name, sizeof( p->storeName ) );	// GLX_U: Storage files its data under this
	}
	G_LogPrintf( "Lua: lua_exec %s\n", code );
	RP_Lua_Call( RP_LUA_CONSOLE, RP_LuaAdmin_ExecWorker, (void *)code );
}
