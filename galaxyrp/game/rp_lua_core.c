/*
===========================================================================
GLX_U: [Lua] the Lua state, the sandbox, the limits, the plugin loader and the protected call
every piece of game code goes through to run a script. See rp_lua.h for the overview.

Sandbox. Only the base, table, string, math, utf8 and coroutine libraries are opened; io, os,
package and debug are not even compiled (lib/lua/README.md). From base, dofile, loadfile, load,
collectgarbage, rawequal, rawget, rawset and rawlen are removed, and so is _G (each plugin gets its
own); string.dump goes too. Every chunk is loaded as text only, so precompiled bytecode is refused.
Each plugin runs in its own environment table whose __index is a read-only proxy of the shared
globals, and every library table is itself a read-only proxy, so no plugin can change what another
one sees. The string metatable is locked with __metatable. setmetatable refuses a metatable with
__gc: Lua runs finalizers with its hooks off, outside any budget (RP_Lua_SetMetatable).

Limits. A count hook fires every RP_LUA_HOOK_STEP instructions and checks the budget of the call
in progress: rp_lua_budget instructions and rp_lua_budgetms milliseconds. A call made while another
runs (an event a script raised) has instructions of its own and what is left of the outer call's time,
at least RP_LUA_NESTED_GRACE_MS (GLX_U: before, it shared the outer call's budget). Once over,
the call is "over budget" until it returns to C: the hook raises the error again every time it
fires, and pcall, xpcall, coroutine.resume and coroutine.close raise it again as they return
(RP_Lua_Guarded), so no script can catch it and carry on; xpcall's handler is skipped once over
(RP_Lua_MsgHandler), since the error was raised inside the hook and the handler would run with hooks
off. Pattern matching and the library's other loops run in C where the hook never fires: lib/lua
calls RP_Lua_PatternCheck every so many steps there, and no string is longer than 4 MB, which bounds
what one instruction can do between two hook calls (lib/lua/glx_lua.h). Memory is capped by the
allocator (rp_lua_memmb). Timers, tasks and the frame event
also share rp_lua_framems per server frame (RP_Lua_FrameBudgetLeft); events the game raises itself
are never deferred.

Errors. Every entry from C is a lua_pcall with a traceback handler. An error is printed and logged
with the plugin name, kept for /scripts errors, and shown to online admins with the Scripts power
at most once per plugin every 30 s. rp_lua_maxerrors errors within a minute switch the plugin off
until it is reloaded or enabled again.
===========================================================================
*/

#include "rp_lua_local.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>

rpLua_t rl;
int rpLuaClientGen[MAX_CLIENTS];

// lib/lua/lstrlib.c (the GLX_U patch)
extern void ( *glx_lstrlib_check )( lua_State *L, unsigned int steps );

static char rpLuaWarnBuf[512];

static int RP_Lua_Clamp( int v, int lo, int hi ) {
	return v < lo ? lo : ( v > hi ? hi : v );
}

/*
================
Allocator: the whole state shares rl.memCap. Frees and shrinks always succeed (Lua requires it).
================
*/
static void *RP_Lua_Alloc( void *ud, void *ptr, size_t osize, size_t nsize ) {
	size_t old = ptr ? osize : 0;
	void *n;

	(void)ud;
	if ( nsize == 0 ) {
		if ( ptr ) {
			rl.memUsed -= ( old <= rl.memUsed ) ? old : rl.memUsed;
			free( ptr );
		}
		return NULL;
	}
	if ( nsize > old && rl.memUsed - old + nsize > rl.memCap ) {
		return NULL;	// Lua raises "not enough memory" in the script
	}
	n = realloc( ptr, nsize );
	if ( !n ) {
		if ( nsize <= old ) {
			return ptr;	// a failed shrink: keep the old block, Lua treats it as the new size
		}
		return NULL;
	}
	rl.memUsed = rl.memUsed - old + nsize;
	return n;
}

/*
================
Budget
================
*/
// rp_lua_budgetms as it applies: what the "ran too long" message quotes (a nested call's own time limit can be
// shorter, see RP_Lua_Call)
static int RP_Lua_BudgetMs( void ) {
	return RP_Lua_Clamp( rp_lua_budgetms.integer, 5, 1000 );
}

static void RP_Lua_CheckBudget( lua_State *L ) {
	if ( rl.depth <= 0 ) {
		return;
	}
	if ( !rl.overBudget ) {
		if ( rl.instr >= rl.instrLimit || trap->Milliseconds() - rl.callStart >= rl.msLimit ) {
			rl.overBudget = qtrue;
		}
	}
	if ( rl.overBudget ) {
		luaL_error( L, "script ran too long (limit %d instructions or %d ms per call)", rl.instrLimit, RP_Lua_BudgetMs() );
	}
}

static void RP_Lua_AddInstr( int n ) {
	rl.instr = ( rl.instr > INT_MAX - n ) ? INT_MAX : rl.instr + n;
}

static void RP_Lua_Hook( lua_State *L, lua_Debug *ar ) {
	(void)ar;
	RP_Lua_AddInstr( RP_LUA_HOOK_STEP );
	RP_Lua_CheckBudget( L );
}

static void RP_Lua_PatternCheck( lua_State *L, unsigned int steps ) {
	RP_Lua_AddInstr( (int)steps );
	RP_Lua_CheckBudget( L );
}

/*
================
Guarded pcall, xpcall, coroutine.resume and coroutine.close. The count hook fires every
RP_LUA_HOOK_STEP instructions wherever the script happens to be, and a loop like
"while true do pcall(f) end" spends nearly all of its time inside f: the hook's error would land
in f every time, be caught, and the loop would go on for ever. These wrappers call the real
function and, when the call is over budget, raise the error again on their way out, so it reaches
our own protected call whatever the script wraps it in. lua_callk keeps them yieldable inside
coroutines, like the originals.
================
*/
static int RP_Lua_GuardCont( lua_State *L, int status, lua_KContext ctx ) {
	(void)status;
	(void)ctx;
	if ( rl.overBudget ) {
		return luaL_error( L, "script ran too long (limit %d instructions or %d ms per call)", rl.instrLimit, RP_Lua_BudgetMs() );
	}
	return lua_gettop( L );
}

static int RP_Lua_Guarded( lua_State *L ) {
	lua_pushvalue( L, lua_upvalueindex( 1 ) );
	lua_insert( L, 1 );
	lua_callk( L, lua_gettop( L ) - 1, LUA_MULTRET, 0, RP_Lua_GuardCont );
	return RP_Lua_GuardCont( L, LUA_OK, 0 );
}

// GLX_U: xpcall's message handler, wrapped. Once the call is over budget it returns the error as it is: that
// error was raised inside the count hook, where Lua has hooks off, and the script's handler would run with them
// still off -- a handler that loops would never be stopped. Otherwise the script's handler runs as usual.
static int RP_Lua_MsgHandler( lua_State *L ) {
	if ( rl.overBudget ) {
		return 1;
	}
	lua_pushvalue( L, lua_upvalueindex( 1 ) );
	lua_insert( L, 1 );
	lua_call( L, lua_gettop( L ) - 1, 1 );
	return 1;
}

// xpcall(f, msgh, ...): RP_Lua_Guarded, with msgh wrapped in RP_Lua_MsgHandler
static int RP_Lua_GuardedXpcall( lua_State *L ) {
	luaL_checktype( L, 2, LUA_TFUNCTION );
	lua_pushvalue( L, 2 );
	lua_pushcclosure( L, RP_Lua_MsgHandler, 1 );
	lua_replace( L, 2 );
	return RP_Lua_Guarded( L );
}

// replaces t[name] (t at index idx) with the guarded version of itself
static void RP_Lua_Guard( lua_State *L, int idx, const char *name, lua_CFunction guarded ) {
	idx = lua_absindex( L, idx );
	lua_getfield( L, idx, name );
	lua_pushcclosure( L, guarded, 1 );
	lua_setfield( L, idx, name );
}

// GLX_U: setmetatable as the base library's, but a metatable with a __gc field is refused. Lua runs finalizers
// with its hooks off (lgc.c), so no budget reaches them: one that loops hangs the server at the next garbage
// collection or at the map change, and one can run in the middle of any API call that allocates. Lua 5.4 only
// marks an object for finalization when its metatable is set, so a __gc added afterwards does nothing.
static int RP_Lua_SetMetatable( lua_State *L ) {
	int t = lua_type( L, 2 );

	luaL_checktype( L, 1, LUA_TTABLE );
	luaL_argexpected( L, t == LUA_TNIL || t == LUA_TTABLE, 2, "nil or table" );
	if ( luaL_getmetafield( L, 1, "__metatable" ) != LUA_TNIL ) {
		return luaL_error( L, "cannot change a protected metatable" );
	}
	if ( t == LUA_TTABLE ) {
		lua_pushliteral( L, "__gc" );
		if ( lua_rawget( L, 2 ) != LUA_TNIL ) {
			return luaL_error( L, "setmetatable: __gc is not available to scripts" );
		}
		lua_pop( L, 1 );
	}
	lua_settop( L, 2 );
	lua_setmetatable( L, 1 );
	return 1;
}

static void RP_Lua_StartBudget( void ) {
	rl.instr = 0;
	rl.instrLimit = RP_Lua_Clamp( rp_lua_budget.integer, 10000, 100000000 );
	rl.msLimit = RP_Lua_BudgetMs();
	rl.callStart = trap->Milliseconds();
	rl.overBudget = qfalse;
}

qboolean RP_Lua_FrameBudgetLeft( void ) {
	if ( rl.frameExhausted ) {
		return qfalse;
	}
	if ( trap->Milliseconds() - rl.frameStart >= RP_Lua_Clamp( rp_lua_framems.integer, 1, 1000 ) ) {
		rl.frameExhausted = qtrue;
		return qfalse;
	}
	return qtrue;
}

/*
================
Panic and warnings
================
*/
static int RP_Lua_Panic( lua_State *L ) {
	const char *msg = lua_tostring( L, -1 );

	// only reachable through a bug in our C code (every call is protected); the state is unusable,
	// so it is abandoned rather than closed, and the map is dropped instead of the process aborted
	rl.panicked = qtrue;
	trap->Error( ERR_DROP, "Lua: unprotected error: %s", msg ? msg : "(no message)" );
	return 0;
}

static void RP_Lua_Warn( void *ud, const char *msg, int tocont ) {
	(void)ud;
	if ( !rpLuaWarnBuf[0] && !tocont && msg[0] == '@' ) {
		return;		// "@on" / "@off" control messages: warnings are always logged here
	}
	Q_strcat( rpLuaWarnBuf, sizeof( rpLuaWarnBuf ), msg );
	if ( !tocont ) {
		const char *name = ( rl.current >= 0 ) ? rl.plugins[rl.current].name : "core";
		trap->Print( S_COLOR_YELLOW "Lua warning [%s]" S_COLOR_WHITE " %s\n", name, rpLuaWarnBuf );
		G_LogPrintf( "Lua warning [%s] %s\n", name, rpLuaWarnBuf );
		rpLuaWarnBuf[0] = '\0';
	}
}

/*
================
Text helpers
================
*/
void RP_Lua_CleanText( char *out, int outSize, const char *in, qboolean keepNewlines ) {
	int o = 0, lines = 0;
	int limit = ( outSize - 1 < RP_LUA_TEXT_MAX ) ? outSize - 1 : RP_LUA_TEXT_MAX;

	for ( ; in && *in && o < limit; in++ ) {
		char c = *in;
		if ( c == '"' ) {
			c = '\'';
		} else if ( c == '\r' ) {
			continue;
		} else if ( c == '\n' ) {
			if ( !keepNewlines || ++lines > 16 ) {
				c = ' ';
			}
		} else if ( (unsigned char)c < ' ' && c != '\t' ) {
			continue;
		}
		out[o++] = c;
	}
	out[o] = '\0';
	RP_BreakStringEdRefs( out );	// GLX_U: never three '@' in a row -- see RP_BreakStringEdRefs() (g_utils.c)
}

void RP_Lua_AdminNotice( const char *text ) {
	char clean[RP_LUA_TEXT_MAX + 1];
	int i;

	RP_Lua_CleanText( clean, sizeof( clean ), text, qtrue );
	for ( i = 0; i < level.maxclients; i++ ) {
		gclient_t *cl = &level.clients[i];
		if ( cl->pers.connected != CON_CONNECTED || cl->sess.amrpgmode == 0 ) {
			continue;
		}
		if ( !( cl->pers.bitvalue & ( 1 << ADM_SCRIPTS ) ) ) {
			continue;
		}
		trap->SendServerCommand( i, va( "print \"%s\n\"", clean ) );
	}
}

/*
================
Errors
================
*/
// GLX_U: [Lua] API 4: something a script should know about but that is not its error (an amount cut to a cap):
// the console, the server log and /scripts errors, but no admin notice and nothing counted towards rp_lua_maxerrors
void RP_Lua_ReportWarning( int plugin, const char *message ) {
	const char *name = ( plugin >= 0 && plugin <= RP_LUA_CONSOLE ) ? rl.plugins[plugin].name : "core";

	trap->Print( S_COLOR_YELLOW "Lua warning [%s]" S_COLOR_WHITE " %s\n", name, message );
	G_LogPrintf( "Lua warning [%s] %s\n", name, message );
	rl.errors[rl.errorNext].time = (int)time( NULL );
	Q_strncpyz( rl.errors[rl.errorNext].plugin, name, sizeof( rl.errors[0].plugin ) );
	Q_strncpyz( rl.errors[rl.errorNext].line, va( "warning: %s", message ), sizeof( rl.errors[0].line ) );
	rl.errorNext = ( rl.errorNext + 1 ) % RP_LUA_ERROR_RING;
	if ( rl.errorCount < RP_LUA_ERROR_RING ) {
		rl.errorCount++;
	}
}

void RP_Lua_ReportError( int plugin, const char *message ) {
	rpLuaPlugin_t *p = ( plugin >= 0 && plugin <= RP_LUA_CONSOLE ) ? &rl.plugins[plugin] : NULL;
	const char *name = p ? p->name : "core";
	char line[RP_LUA_ERROR_LINE];
	int now = trap->Milliseconds();
	int i;

	trap->Print( S_COLOR_RED "Lua error [%s]" S_COLOR_WHITE " %s\n", name, message );
	G_LogPrintf( "Lua error [%s] %s\n", name, message );

	// first line only, for /scripts errors and the admin notice
	Q_strncpyz( line, message, sizeof( line ) );
	for ( i = 0; line[i]; i++ ) {
		if ( line[i] == '\n' || line[i] == '\r' ) {
			line[i] = '\0';
			break;
		}
	}
	rl.errors[rl.errorNext].time = (int)time( NULL );
	Q_strncpyz( rl.errors[rl.errorNext].plugin, name, sizeof( rl.errors[0].plugin ) );
	Q_strncpyz( rl.errors[rl.errorNext].line, line, sizeof( rl.errors[0].line ) );
	rl.errorNext = ( rl.errorNext + 1 ) % RP_LUA_ERROR_RING;
	if ( rl.errorCount < RP_LUA_ERROR_RING ) {
		rl.errorCount++;
	}

	if ( !p ) {
		RP_Lua_AdminNotice( va( "^1Lua error^7: %s", line ) );
		return;
	}
	// GLX_U: a plugin already switched off (its code still running when it was, see RP_Lua_CurrentPlugin): the
	// error is in the log and /scripts errors, but not counted again or shown to the admins a second time
	if ( plugin != RP_LUA_CONSOLE && !p->enabled ) {
		return;
	}

	p->errTotal++;
	if ( plugin == RP_LUA_CONSOLE ) {
		return;		// lua_exec: the rcon user sees the error; nothing to switch off
	}

	if ( !p->lastNotice || now - p->lastNotice >= RP_LUA_NOTICE_GAP_MS ) {
		if ( p->heldNotices > 0 ) {
			RP_Lua_AdminNotice( va( "^1Lua error^7 in ^3%s^7: %s ^3(%d more since the last notice)", name, line, p->heldNotices ) );
		} else {
			RP_Lua_AdminNotice( va( "^1Lua error^7 in ^3%s^7: %s", name, line ) );
		}
		p->lastNotice = now ? now : 1;
		p->heldNotices = 0;
	} else {
		p->heldNotices++;
	}

	if ( !p->errWindowStart || now - p->errWindowStart > RP_LUA_ERROR_WINDOW_MS ) {
		p->errWindowStart = now ? now : 1;
		p->errWindowCount = 0;
	}
	p->errWindowCount++;
	if ( p->enabled && p->errWindowCount >= RP_Lua_Clamp( rp_lua_maxerrors.integer, 1, 1000 ) ) {
		char reason[96];
		Com_sprintf( reason, sizeof( reason ), "switched off after %d errors in a minute", p->errWindowCount );
		RP_Lua_DisablePlugin( plugin, reason );
		trap->Print( S_COLOR_RED "Lua: plugin %s %s\n", name, reason );
		G_LogPrintf( "Lua: plugin %s %s\n", name, reason );
		RP_Lua_AdminNotice( va( "^1Lua^7: plugin ^3%s^7 %s. Fix it, then use ^3/scripts reload %s^7.", name, reason, name ) );
	}
}

/*
================
Protected calls
================
*/
typedef struct {
	rpLuaWorker_t	worker;
	void			*data;
} rpLuaCallArgs_t;

static int RP_Lua_Trampoline( lua_State *L ) {
	rpLuaCallArgs_t *a = (rpLuaCallArgs_t *)lua_touserdata( L, 1 );
	lua_settop( L, 0 );
	return a->worker( L, a->data );
}

static int RP_Lua_Traceback( lua_State *L ) {
	const char *msg = lua_tostring( L, 1 );
	if ( !msg ) {
		msg = lua_pushfstring( L, "(error object is a %s value)", luaL_typename( L, 1 ) );
	}
	luaL_traceback( L, L, msg, 1 );
	return 1;
}

qboolean RP_Lua_Call( int plugin, rpLuaWorker_t worker, void *data ) {
	lua_State *L = rl.L;
	rpLuaCallArgs_t args;
	int top, status, prevPlugin, start, instrBefore;
	int outerInstr = 0, outerInstrLimit = 0, outerStart = 0, outerMsLimit = 0;
	qboolean outer;
	static char errBuf[2048];

	if ( !L || rl.panicked ) {
		return qfalse;
	}
	if ( rl.depth >= RP_LUA_MAX_DEPTH ) {
		const char *name = ( plugin >= 0 ) ? rl.plugins[plugin].name : "core";
		trap->Print( S_COLOR_YELLOW "Lua: a call to %s was skipped: scripts nested more than %d deep\n", name, RP_LUA_MAX_DEPTH );
		return qfalse;
	}

	outer = ( rl.depth == 0 ) ? qtrue : qfalse;
	if ( outer ) {
		RP_Lua_StartBudget();
	} else {
		// GLX_U: a call made while another is running (an event a script raised): its own instruction budget,
		// and what is left of the outer call's time, but never less than RP_LUA_NESTED_GRACE_MS. Before, the
		// budget was shared, so the plugin whose handler happened to run when the outer call had used it up
		// got the error -- and enough of them switched it off for another plugin's loop. The outer call's
		// budget comes back when this one returns; its time went on running meanwhile.
		int now = trap->Milliseconds(), left;

		if ( rl.overBudget ) {
			return qfalse;	// the outer call is already being stopped: nothing more runs, nobody is blamed
		}
		outerInstr = rl.instr;
		outerInstrLimit = rl.instrLimit;
		outerStart = rl.callStart;
		outerMsLimit = rl.msLimit;
		left = rl.msLimit - ( now - rl.callStart );
		rl.instr = 0;
		rl.instrLimit = RP_Lua_Clamp( rp_lua_budget.integer, 10000, 100000000 );
		rl.callStart = now;
		rl.msLimit = ( left > RP_LUA_NESTED_GRACE_MS ) ? left : RP_LUA_NESTED_GRACE_MS;
	}
	start = trap->Milliseconds();
	instrBefore = rl.instr;
	prevPlugin = rl.current;
	top = lua_gettop( L );

	args.worker = worker;
	args.data = data;
	rl.depth++;
	rl.current = plugin;
	lua_pushcfunction( L, RP_Lua_Traceback );
	lua_pushcfunction( L, RP_Lua_Trampoline );
	lua_pushlightuserdata( L, &args );
	status = lua_pcall( L, 1, 0, top + 1 );
	rl.depth--;

	errBuf[0] = '\0';
	if ( status != LUA_OK ) {
		const char *msg = lua_tostring( L, -1 );
		Q_strncpyz( errBuf, msg ? msg : "(error with no message)", sizeof( errBuf ) );
	}
	lua_settop( L, top );
	rl.current = prevPlugin;

	if ( plugin >= 0 && plugin <= RP_LUA_CONSOLE ) {
		rpLuaPlugin_t *p = &rl.plugins[plugin];
		p->calls++;
		p->instructions += (double)( rl.instr - instrBefore );
		p->ms += trap->Milliseconds() - start;
	}
	if ( outer ) {
		rl.overBudget = qfalse;
	} else {
		// the outer call's budget again: it was not over when this one began
		rl.instr = outerInstr;
		rl.instrLimit = outerInstrLimit;
		rl.callStart = outerStart;
		rl.msLimit = outerMsLimit;
		rl.overBudget = qfalse;
	}
	if ( status != LUA_OK ) {
		RP_Lua_ReportError( plugin, errBuf );
		return qfalse;
	}
	return qtrue;
}

int RP_Lua_CurrentPlugin( lua_State *L ) {
	if ( rl.current < 0 || rl.current > RP_LUA_CONSOLE || !rl.plugins[rl.current].inUse ) {
		return luaL_error( L, "no plugin is running" );
	}
	// GLX_U: a plugin switched off while its code runs (too many errors in a nested event, or while loading)
	// stops at its next API call: before, it carried on registering timers, handlers, commands and entities
	// after RP_LuaApi_RemovePlugin had cleaned it up, and they came back on the next reload. Its unload
	// handlers still run: RP_Lua_DisablePlugin fires them before it clears enabled.
	if ( !rl.plugins[rl.current].enabled ) {
		return luaL_error( L, "this plugin has been switched off" );
	}
	return rl.current;
}

qboolean RP_Lua_PluginRunning( int plugin ) {
	return ( plugin >= 0 && plugin <= RP_LUA_CONSOLE && rl.plugins[plugin].inUse && rl.plugins[plugin].enabled ) ? qtrue : qfalse;
}

/*
================
Files: loose files only, read straight from the OS. A copy that is only inside a pk3 is never read,
so no script can end up in a pk3 the server offers clients for download.
================
*/
static qboolean RP_Lua_SafeRelPath( const char *rel ) {
	const char *c;
	if ( !rel[0] || rel[0] == '/' || strstr( rel, ".." ) || strstr( rel, "//" ) ) {
		return qfalse;
	}
	for ( c = rel; *c; c++ ) {
		if ( !( ( *c >= 'a' && *c <= 'z' ) || ( *c >= 'A' && *c <= 'Z' ) || ( *c >= '0' && *c <= '9' ) || *c == '_' || *c == '-' || *c == '.' || *c == '/' ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

// a loose file of the mod folder, opened for reading: fs_homepath first, then fs_basepath (NULL when there is none;
// a copy only inside a pk3 is never found). game gets the mod folder's name.
static FILE *RP_Lua_OpenOSFile( const char *relPath, char *game, int gameSize ) {
	char roots[2][MAX_OSPATH], path[MAX_OSPATH * 2];
	FILE *f = NULL;
	int i;

	trap->Cvar_VariableStringBuffer( "fs_homepath", roots[0], sizeof( roots[0] ) );
	trap->Cvar_VariableStringBuffer( "fs_basepath", roots[1], sizeof( roots[1] ) );
	trap->Cvar_VariableStringBuffer( "fs_game", game, gameSize );
	if ( !game[0] ) {
		Q_strncpyz( game, BASEGAME, gameSize );
	}

	for ( i = 0; i < 2 && !f; i++ ) {
		if ( !roots[i][0] ) {
			continue;
		}
		Com_sprintf( path, sizeof( path ), "%s/%s/%s", roots[i], game, relPath );
		f = fopen( path, "rb" );
	}
	if ( !f && !roots[0][0] && !roots[1][0] ) {
		Com_sprintf( path, sizeof( path ), "%s/%s", game, relPath );
		f = fopen( path, "rb" );
	}
	return f;
}

// GLX_U: whether relPath is a loose file a script could be read from (RP_Lua_ReadOSFile)
qboolean RP_Lua_LooseFileExists( const char *relPath ) {
	char game[MAX_QPATH];
	FILE *f;

	if ( !RP_Lua_SafeRelPath( relPath ) ) {
		return qfalse;
	}
	f = RP_Lua_OpenOSFile( relPath, game, sizeof( game ) );
	if ( !f ) {
		return qfalse;
	}
	fclose( f );
	return qtrue;
}

qboolean RP_Lua_ReadOSFile( const char *relPath, char **buf, int *len, char *err, int errSize ) {
	char game[MAX_QPATH];
	FILE *f = NULL;
	long size;
	int got;

	*buf = NULL;
	*len = 0;
	if ( !RP_Lua_SafeRelPath( relPath ) ) {
		Com_sprintf( err, errSize, "bad file name: %s", relPath );
		return qfalse;
	}

	f = RP_Lua_OpenOSFile( relPath, game, sizeof( game ) );
	if ( !f ) {
		Com_sprintf( err, errSize, "%s not found (scripts are read only from loose files in the %s folder, never from pk3s)", relPath, game );
		return qfalse;
	}

	fseek( f, 0, SEEK_END );
	size = ftell( f );
	fseek( f, 0, SEEK_SET );
	if ( size < 0 || size > RP_LUA_MAX_FILE ) {
		fclose( f );
		Com_sprintf( err, errSize, "%s is too big (the limit is %d KB)", relPath, RP_LUA_MAX_FILE / 1024 );
		return qfalse;
	}
	*buf = (char *)malloc( size + 1 );
	if ( !*buf ) {
		fclose( f );
		Com_sprintf( err, errSize, "out of memory reading %s", relPath );
		return qfalse;
	}
	got = (int)fread( *buf, 1, size, f );
	fclose( f );
	( *buf )[got] = '\0';
	*len = got;
	return qtrue;
}

typedef struct {
	const char	*buf;
	int			len;
	const char	*chunkname;
} rpLuaChunk_t;

static int RP_Lua_LoadChunkWorker( lua_State *L ) {
	rpLuaChunk_t *c = (rpLuaChunk_t *)lua_touserdata( L, 1 );
	if ( luaL_loadbufferx( L, c->buf, c->len, c->chunkname, "t" ) != LUA_OK ) {
		return lua_error( L );
	}
	return 1;
}

// pushes the chunk dir/file of this plugin as a function running in the plugin's environment, or
// raises a Lua error. The file buffer is freed before any error can unwind past it.
void RP_Lua_PushChunk( lua_State *L, int plugin, const char *dir, const char *file ) {
	char rel[MAX_QPATH * 2], chunkname[MAX_QPATH * 2 + 2], err[256];
	rpLuaChunk_t c;
	char *buf;
	int len, status;

	Com_sprintf( rel, sizeof( rel ), "%s/%s", dir, file );
	if ( !RP_Lua_ReadOSFile( rel, &buf, &len, err, sizeof( err ) ) ) {
		luaL_error( L, "%s", err );
		return;
	}
	Com_sprintf( chunkname, sizeof( chunkname ), "@%s", rel );
	c.buf = buf;
	c.len = len;
	c.chunkname = chunkname;
	lua_pushcfunction( L, RP_Lua_LoadChunkWorker );
	lua_pushlightuserdata( L, &c );
	status = lua_pcall( L, 1, 1, 0 );
	free( buf );
	if ( status != LUA_OK ) {
		lua_error( L );
		return;
	}
	lua_rawgeti( L, LUA_REGISTRYINDEX, rl.plugins[plugin].envRef );
	if ( !lua_setupvalue( L, -2, 1 ) ) {
		lua_pop( L, 1 );	// no upvalues: a chunk that reads no globals at all
	}
}

/*
================
Plugins
================
*/
qboolean RP_Lua_ValidPluginName( const char *name ) {
	int i;
	for ( i = 0; name[i]; i++ ) {
		if ( i >= RP_LUA_PLUGIN_NAME_MAX ) {
			return qfalse;
		}
		if ( !( ( name[i] >= 'a' && name[i] <= 'z' ) || ( name[i] >= '0' && name[i] <= '9' ) || name[i] == '_' ) ) {
			return qfalse;
		}
	}
	return ( i > 0 ) ? qtrue : qfalse;
}

int RP_Lua_FindPlugin( const char *name ) {
	int i;
	for ( i = 0; i < RP_LUA_MAX_PLUGINS; i++ ) {
		if ( rl.plugins[i].inUse && !Q_stricmp( rl.plugins[i].name, name ) ) {
			return i;
		}
	}
	return -1;
}

int RP_Lua_AddPlugin( const char *name, qboolean isMap, const char *dir, const char *file ) {
	int i;
	for ( i = 0; i < RP_LUA_MAX_PLUGINS; i++ ) {
		rpLuaPlugin_t *p = &rl.plugins[i];
		if ( p->inUse ) {
			continue;
		}
		memset( p, 0, sizeof( *p ) );
		p->inUse = qtrue;
		p->isMap = isMap;
		p->envRef = LUA_NOREF;
		p->storeBytes = -1;
		Q_strncpyz( p->name, name, sizeof( p->name ) );
		Q_strncpyz( p->dir, dir, sizeof( p->dir ) );
		Q_strncpyz( p->mainFile, file, sizeof( p->mainFile ) );
		return i;
	}
	return -1;
}

void RP_Lua_NewEnv( lua_State *L, rpLuaPlugin_t *p ) {
	lua_newtable( L );								// the environment
	lua_pushvalue( L, -1 );
	lua_setfield( L, -2, "_G" );					// _G is the plugin's own table
	lua_newtable( L );								// its metatable
	lua_rawgeti( L, LUA_REGISTRYINDEX, rl.rootRef );
	lua_setfield( L, -2, "__index" );
	lua_pushboolean( L, 0 );
	lua_setfield( L, -2, "__metatable" );
	lua_setmetatable( L, -2 );
	p->envRef = luaL_ref( L, LUA_REGISTRYINDEX );
}

static int RP_Lua_LoadWorker( lua_State *L, void *data ) {
	int plugin = *(int *)data;
	rpLuaPlugin_t *p = &rl.plugins[plugin];

	RP_Lua_NewEnv( L, p );
	RP_Lua_PushChunk( L, plugin, p->dir, p->mainFile );
	lua_call( L, 0, 0 );
	return 0;
}

static void RP_Lua_DropEnv( rpLuaPlugin_t *p ) {
	if ( rl.L && p->envRef != LUA_NOREF && p->envRef != LUA_REFNIL ) {
		luaL_unref( rl.L, LUA_REGISTRYINDEX, p->envRef );
	}
	p->envRef = LUA_NOREF;
}

// returns qtrue when the plugin's file ran without an error; the plugin is then enabled
int RP_Lua_LoadPluginFile( int plugin ) {
	rpLuaPlugin_t *p = &rl.plugins[plugin];

	RP_LuaApi_RemovePlugin( plugin );	// GLX_U: nothing left from before survives into this load
	p->enabled = qtrue;		// so what it registers while loading is accepted
	p->numIncludes = 0;
	p->title[0] = p->version[0] = p->failReason[0] = '\0';
	p->errWindowStart = p->errWindowCount = 0;
	if ( !RP_Lua_Call( plugin, RP_Lua_LoadWorker, &plugin ) ) {
		p->enabled = qfalse;
		RP_LuaApi_RemovePlugin( plugin );
		RP_Lua_DropEnv( p );
		if ( !p->failReason[0] ) {
			Q_strncpyz( p->failReason, "failed to load, see /scripts errors", sizeof( p->failReason ) );
		}
		return qfalse;
	}
	if ( !p->enabled ) {
		return qfalse;	// switched itself off while loading (too many errors)
	}
	trap->Print( "Lua: loaded %s%s%s\n", p->name, p->version[0] ? " " : "", p->version );
	return qtrue;
}

void RP_Lua_DisablePlugin( int plugin, const char *reason ) {
	rpLuaPlugin_t *p;

	if ( plugin < 0 || plugin > RP_LUA_CONSOLE ) {
		return;
	}
	p = &rl.plugins[plugin];
	if ( !p->inUse || !p->enabled || p->disabling ) {
		return;
	}
	p->disabling = qtrue;
	RP_LuaApi_FireEvent( RPLE_UNLOAD, plugin, -1, 0, NULL, -1, NULL );
	p->enabled = qfalse;
	RP_LuaApi_RemovePlugin( plugin );
	RP_Lua_DropEnv( p );
	Q_strncpyz( p->failReason, reason ? reason : "off", sizeof( p->failReason ) );
	p->disabling = qfalse;
}

/*
================
The state
================
*/
static int RP_Lua_ProxyNewIndex( lua_State *L ) {
	return luaL_error( L, "this table is read-only" );
}

static int RP_Lua_ProxyNext( lua_State *L ) {
	lua_settop( L, 2 );
	lua_pushvalue( L, lua_upvalueindex( 1 ) );
	lua_pushvalue( L, 2 );
	if ( lua_next( L, 3 ) ) {
		return 2;
	}
	lua_pushnil( L );
	return 1;
}

static int RP_Lua_ProxyPairs( lua_State *L ) {
	lua_pushvalue( L, lua_upvalueindex( 1 ) );
	lua_pushcclosure( L, RP_Lua_ProxyNext, 1 );
	lua_pushvalue( L, 1 );
	lua_pushnil( L );
	return 3;
}

// replaces the table at idx with a read-only proxy of it. The real table is reachable only through
// the proxy's __index and its pairs() iterator closure -- never handed to a script.
static void RP_Lua_MakeProxy( lua_State *L, int idx ) {
	idx = lua_absindex( L, idx );
	lua_newtable( L );							// proxy
	lua_newtable( L );							// its metatable
	lua_pushvalue( L, idx );
	lua_setfield( L, -2, "__index" );
	lua_pushcfunction( L, RP_Lua_ProxyNewIndex );
	lua_setfield( L, -2, "__newindex" );
	lua_pushvalue( L, idx );
	lua_pushcclosure( L, RP_Lua_ProxyPairs, 1 );
	lua_setfield( L, -2, "__pairs" );
	lua_pushboolean( L, 0 );
	lua_setfield( L, -2, "__metatable" );
	lua_setmetatable( L, -2 );
	lua_replace( L, idx );
}

static const char *rpLuaRemovedGlobals[] = {
	"dofile", "loadfile", "load", "collectgarbage", "rawequal", "rawget", "rawset", "rawlen", "require", "_G", NULL
};

// the library tables replaced by read-only proxies; the API tables are added by RP_LuaApi_Register
// and RP_LuaStore_Register
static const char *rpLuaProxiedGlobals[] = {
	"string", "table", "math", "utf8", "coroutine",
	"Plugin", "Events", "Timer", "Task", "Commands", "Msg", "Server", "Storage", "World", "Dialog", NULL
};

static int RP_Lua_SandboxWorker( lua_State *L, void *data ) {
	int g, i;

	(void)data;
	luaL_requiref( L, LUA_GNAME, luaopen_base, 1 );
	luaL_requiref( L, LUA_TABLIBNAME, luaopen_table, 1 );
	luaL_requiref( L, LUA_STRLIBNAME, luaopen_string, 1 );
	luaL_requiref( L, LUA_MATHLIBNAME, luaopen_math, 1 );
	luaL_requiref( L, LUA_UTF8LIBNAME, luaopen_utf8, 1 );
	luaL_requiref( L, LUA_COLIBNAME, luaopen_coroutine, 1 );
	lua_settop( L, 0 );

	lua_pushglobaltable( L );
	g = lua_gettop( L );
	for ( i = 0; rpLuaRemovedGlobals[i]; i++ ) {
		lua_pushnil( L );
		lua_setfield( L, g, rpLuaRemovedGlobals[i] );
	}
	lua_getfield( L, g, "string" );
	lua_pushnil( L );
	lua_setfield( L, -2, "dump" );
	lua_pop( L, 1 );

	// the functions that catch errors re-raise "ran too long" (see RP_Lua_Guarded)
	RP_Lua_Guard( L, g, "pcall", RP_Lua_Guarded );
	RP_Lua_Guard( L, g, "xpcall", RP_Lua_GuardedXpcall );
	lua_getfield( L, g, "coroutine" );
	RP_Lua_Guard( L, -1, "resume", RP_Lua_Guarded );
	RP_Lua_Guard( L, -1, "close", RP_Lua_Guarded );
	lua_pop( L, 1 );
	lua_pushcfunction( L, RP_Lua_SetMetatable );	// GLX_U: no __gc
	lua_setfield( L, g, "setmetatable" );

	// lock the string metatable: getmetatable("") returns false and nothing can change it
	lua_pushliteral( L, "" );
	if ( lua_getmetatable( L, -1 ) ) {
		lua_pushboolean( L, 0 );
		lua_setfield( L, -2, "__metatable" );
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );

	RP_LuaApi_Register( L );
	RP_LuaStore_Register( L );
	RP_LuaWorld_Register( L );
	RP_LuaDialog_Register( L );		// API 5

	for ( i = 0; rpLuaProxiedGlobals[i]; i++ ) {
		lua_getfield( L, g, rpLuaProxiedGlobals[i] );
		if ( lua_istable( L, -1 ) ) {
			RP_Lua_MakeProxy( L, -1 );
			lua_setfield( L, g, rpLuaProxiedGlobals[i] );
		} else {
			lua_pop( L, 1 );
		}
	}

	lua_pushvalue( L, g );
	RP_Lua_MakeProxy( L, -1 );
	rl.rootRef = luaL_ref( L, LUA_REGISTRYINDEX );
	return 0;
}

void RP_Lua_InitState( void ) {
	int i;

	rl.memCap = (size_t)RP_Lua_Clamp( rp_lua_memmb.integer, 4, 1024 ) * 1024 * 1024;
	rl.memUsed = 0;
	rl.L = lua_newstate( RP_Lua_Alloc, NULL );
	if ( !rl.L ) {
		trap->Print( S_COLOR_RED "Lua: could not create the Lua state\n" );
		return;
	}
	lua_atpanic( rl.L, RP_Lua_Panic );
	lua_setwarnf( rl.L, RP_Lua_Warn, NULL );
	lua_sethook( rl.L, RP_Lua_Hook, LUA_MASKCOUNT, RP_LUA_HOOK_STEP );
	glx_lstrlib_check = RP_Lua_PatternCheck;
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		rl.playerRef[i] = LUA_NOREF;
	}
	rl.rootRef = LUA_NOREF;
	if ( !RP_Lua_Call( -1, RP_Lua_SandboxWorker, NULL ) ) {
		trap->Print( S_COLOR_RED "Lua: could not set up the sandbox, scripts are off for this map\n" );
		RP_Lua_CloseState();
	}
}

void RP_Lua_CloseState( void ) {
	RP_LuaApi_Reset();
	RP_LuaWorld_Reset();
	RP_LuaPeople_Reset();	// API 3: no script is left to own a player effect
	RP_LuaDialog_Reset();	// API 5: nor a dialogue
	RP_LuaOrders_Reset();	// API 6: nor an NPC order
	if ( rl.L && !rl.panicked ) {
		// finalizers (__gc) would run here; setmetatable refuses them (RP_Lua_SetMetatable), but the
		// close still gets one budget, which stays exhausted once used up
		rl.depth = 1;
		RP_Lua_StartBudget();
		lua_close( rl.L );
		rl.depth = 0;
		rl.overBudget = qfalse;
		if ( rl.memUsed ) {
			trap->Print( S_COLOR_YELLOW "Lua: %d bytes still counted after closing the state\n", (int)rl.memUsed );
		}
	}
	rl.L = NULL;
	memset( rl.plugins, 0, sizeof( rl.plugins ) );
}

/*
================
Startup and shutdown
================
*/
static int RP_Lua_NameCompare( const void *a, const void *b ) {
	return strcmp( (const char *)a, (const char *)b );
}

// GLX_U: the plugins to load: every folder in lua/plugins with a loose plugin.lua, its name a plugin name, sorted
// (the engine's list is in no set order). How many, at most max; the others are skipped with a line in the log.
int RP_Lua_PluginFolders( char names[][RP_LUA_PLUGIN_NAME_MAX + 1], int max ) {
	static char list[16384];
	char *name;
	int count, i, j, len, n = 0;

	count = trap->FS_GetFileList( "lua/plugins", "/", list, sizeof( list ) );
	for ( i = 0, name = list; i < count; i++, name += len + 1 ) {
		char rel[MAX_QPATH];

		len = strlen( name );
		if ( len && name[len - 1] == '/' ) {
			name[len - 1] = '\0';	// a folder inside a pk3 comes with its slash
		}
		if ( !name[0] || name[0] == '.' ) {
			continue;
		}
		if ( !RP_Lua_ValidPluginName( name ) ) {
			trap->Print( S_COLOR_YELLOW "Lua: lua/plugins/%s skipped: a plugin folder's name is lower-case letters, digits and _ only, up to %d\n", name, RP_LUA_PLUGIN_NAME_MAX );
			continue;
		}
		for ( j = 0; j < n && strcmp( names[j], name ); j++ ) {
		}
		if ( j < n ) {
			continue;	// the same folder loose and in a pk3
		}
		Com_sprintf( rel, sizeof( rel ), "lua/plugins/%s/plugin.lua", name );
		if ( !RP_Lua_LooseFileExists( rel ) ) {
			trap->Print( S_COLOR_YELLOW "Lua: lua/plugins/%s skipped: it has no plugin.lua (a loose file: one inside a pk3 is never read)\n", name );
			continue;
		}
		if ( n >= max ) {
			trap->Print( S_COLOR_YELLOW "Lua: more than %d plugin folders, %s skipped\n", max, name );
			continue;
		}
		Q_strncpyz( names[n++], name, sizeof( names[0] ) );
	}
	qsort( names, n, sizeof( names[0] ), RP_Lua_NameCompare );
	return n;
}

// GLX_U: this map's script folder (lua/maps/<mapname>) and the .lua files the engine lists in it; returns how many
// (0 with no map or a map name no path can hold). list may be NULL to only count them.
static int RP_Lua_MapScriptFiles( char *list, int listSize, char *dir ) {
	static char buf[8192];
	char mapname[MAX_QPATH], path[MAX_QPATH];

	trap->Cvar_VariableStringBuffer( "mapname", mapname, sizeof( mapname ) );
	Com_sprintf( path, sizeof( path ), "lua/maps/%s", mapname );
	if ( dir ) {
		Q_strncpyz( dir, path, MAX_QPATH );
	}
	if ( !mapname[0] || !RP_Lua_SafeRelPath( path ) ) {
		return 0;
	}
	if ( !list ) {
		list = buf;
		listSize = sizeof( buf );
	}
	return trap->FS_GetFileList( path, ".lua", list, listSize );
}

static void RP_Lua_LoadMapScripts( int *loaded, int *failed ) {
	char dir[MAX_QPATH], list[8192];
	char *name;
	int count, i, len;

	count = RP_Lua_MapScriptFiles( list, sizeof( list ), dir );
	name = list;
	for ( i = 0; i < count; i++, name += len + 1 ) {
		char pname[RP_LUA_NAME_SIZE];
		int plugin;

		len = strlen( name );
		if ( strchr( name, '/' ) || strchr( name, '\\' ) || !RP_Lua_SafeRelPath( name ) || len >= RP_LUA_NAME_SIZE - 5 ) {
			trap->Print( S_COLOR_YELLOW "Lua: map script %s/%s skipped: bad file name\n", dir, name );
			continue;
		}
		if ( !RP_Lua_LooseFileExists( va( "%s/%s", dir, name ) ) ) {
			trap->Print( S_COLOR_YELLOW "Lua: map script %s/%s skipped: only inside a pk3, which is never read\n", dir, name );
			continue;
		}
		Com_sprintf( pname, sizeof( pname ), "map:%s", name );
		plugin = RP_Lua_AddPlugin( pname, qtrue, dir, name );
		if ( plugin < 0 ) {
			trap->Print( S_COLOR_YELLOW "Lua: more than %d plugins and map scripts, %s skipped\n", RP_LUA_MAX_PLUGINS, pname );
			break;
		}
		if ( RP_Lua_LoadPluginFile( plugin ) ) {
			( *loaded )++;
		} else {
			( *failed )++;
		}
	}
}

void RP_Lua_Init( void ) {
	static char names[RP_LUA_MAX_FOLDERS][RP_LUA_PLUGIN_NAME_MAX + 1];
	int count, i, loaded = 0, failed = 0, mapLoaded = 0;

	if ( rl.L ) {
		RP_Lua_Shutdown();
	}
	memset( &rl, 0, sizeof( rl ) );
	rl.current = -1;
	rl.rootRef = LUA_NOREF;

	if ( !rp_lua.integer ) {
		return;
	}
	RP_Lua_InitState();
	if ( !rl.L ) {
		return;
	}

	// GLX_U: every plugin folder in lua/plugins (rp_lua_plugins is gone), then this map's scripts (rp_lua_mapscripts
	// is gone too). lua.db is opened (and swept) now, outside any script's time budget, when anything will run:
	// a server with no plugins and no map scripts never gets the file.
	count = RP_Lua_PluginFolders( names, RP_LUA_MAX_FOLDERS );
	if ( count || RP_Lua_MapScriptFiles( NULL, 0, NULL ) ) {
		RP_LuaStore_Open();
	}

	for ( i = 0; i < count; i++ ) {
		char dir[MAX_QPATH];
		int plugin;

		Com_sprintf( dir, sizeof( dir ), "lua/plugins/%s", names[i] );
		plugin = RP_Lua_AddPlugin( names[i], qfalse, dir, "plugin.lua" );
		if ( plugin < 0 ) {
			trap->Print( S_COLOR_YELLOW "Lua: more than %d plugins, %s and the %d after it skipped\n", RP_LUA_MAX_PLUGINS, names[i], count - i - 1 );
			break;
		}
		if ( RP_Lua_LoadPluginFile( plugin ) ) {
			loaded++;
		} else {
			failed++;
		}
	}

	RP_Lua_LoadMapScripts( &mapLoaded, &failed );

	RP_LuaApi_FireEvent( RPLE_INIT, -1, -1, 0, NULL, -1, NULL );
	if ( loaded || mapLoaded || failed ) {
		trap->Print( "Lua: %d plugin(s) and %d map script(s) running, %d failed to load\n", loaded, mapLoaded, failed );
	}
}

void RP_Lua_Shutdown( void ) {
	if ( !rl.L ) {
		RP_LuaStore_Close();
		return;
	}
	RP_LuaApi_FireEvent( RPLE_SHUTDOWN, -1, -1, 0, NULL, -1, NULL );
	RP_LuaStore_Close();
	RP_Lua_CloseState();
}

void RP_Lua_Frame( int levelTime ) {
	if ( !rl.L ) {
		return;
	}
	rl.frameStart = trap->Milliseconds();
	rl.frameExhausted = qfalse;
	RP_LuaWorld_Frame();	// frees queued inside game callbacks, zones' leave checks
	RP_LuaDialog_Frame();	// API 5: dialogues that time out or that the player walked away from
	RP_LuaOrders_Frame();	// API 6: NPC orders that have ended
	RP_LuaApi_RunTimersAndTasks( levelTime );
	RP_LuaApi_FireFrame( levelTime );
	RP_LuaStore_Frame();
}

void RP_Lua_NoteDeferred( int plugin, int count ) {
	int now = trap->Milliseconds();
	if ( plugin >= 0 && plugin <= RP_LUA_CONSOLE ) {
		rl.plugins[plugin].deferred += count;
	}
	if ( !rl.lastDeferWarn || now - rl.lastDeferWarn >= RP_LUA_DEFER_WARN_MS ) {
		rl.lastDeferWarn = now ? now : 1;
		trap->Print( S_COLOR_YELLOW "Lua: scripts used this frame's %d ms; the rest of their timers and tasks run next frame\n", RP_Lua_Clamp( rp_lua_framems.integer, 1, 1000 ) );
	}
}

/*
================
Game hooks
================
*/
void RP_Lua_ClientConnect( int clientNum, qboolean firstTime ) {
	if ( !rl.L || clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return;
	}
	RP_LuaApi_FireEvent( RPLE_CLIENT_CONNECT, -1, clientNum, firstTime ? 1 : 0, NULL, -1, NULL );
}

void RP_Lua_ClientBegin( int clientNum ) {
	if ( !rl.L || clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return;
	}
	RP_LuaApi_FireEvent( RPLE_CLIENT_BEGIN, -1, clientNum, 0, NULL, -1, NULL );
}

void RP_Lua_ClientSpawn( int clientNum ) {
	if ( !rl.L || clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return;
	}
	RP_LuaApi_FireEvent( RPLE_CLIENT_SPAWN, -1, clientNum, 0, NULL, -1, NULL );
}

void RP_Lua_ClientDisconnect( int clientNum ) {
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		return;
	}
	if ( rl.L ) {
		RP_LuaWorld_ClientGone( clientNum );	// zones see them leave first, while the handle still works
		RP_LuaDialog_ClientGone( clientNum );	// and their dialogue closes ("left")
		RP_LuaApi_FireEvent( RPLE_CLIENT_DISCONNECT, -1, clientNum, 0, NULL, -1, NULL );
		RP_LuaApi_PlayerGone( clientNum );
	}
	rpLuaClientGen[clientNum]++;	// every handle to this connection is stale from now on
}

qboolean RP_Lua_Chat( gentity_t *ent, gentity_t *target, int mode, qboolean ooc, const char *text ) {
	int result = 1;

	if ( !rl.L || !ent || !ent->client ) {
		return qtrue;
	}
	RP_LuaApi_FireEvent( RPLE_CHAT, -1, ent->s.number, ooc ? SAY_OOC : mode, text,
		( target && target->client ) ? target->s.number : -1, &result );
	return result ? qtrue : qfalse;
}

qboolean RP_Lua_ClientCommand( gentity_t *ent, const char *cmd ) {
	if ( !rl.L || !ent || !ent->client ) {
		return qfalse;
	}
	return RP_LuaApi_RunCommand( ent, cmd );
}

void RP_Lua_CharacterRemoved( int charID ) {
	RP_LuaStore_CharacterRemoved( charID );
}
