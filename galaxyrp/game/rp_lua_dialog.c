/*
===========================================================================
GLX_U: [Lua] API version 5: dialogues. A script shows a player a text with up to six numbered options
(Dialog.open) and its function hears the answer, or why the dialogue closed without one.

One dialogue per player at a time. It closes when the player answers, cancels (/choose 0), runs out of
time, walks away from the entity it belongs to, dies, is knocked down or goes spectator, disconnects, or
when the same plugin opens another one for them; the script's function hears which. When the plugin is
switched off, its dialogues close and nothing is called.

The player sees it in the chat area, and answers with /choose <number> or by saying just the number
(RP_Lua_DialogChat, called from G_Say before any chat goes out). Every answer is checked against the
open dialogue: a late or repeated one does nothing. The menu our client will show (claude/
Lua_dialogues_scope_2026-10-17.md, D2) reaches the same /choose with the dialogue's id.

The text is cleaned on the way in so it can later go into one menu cvar as it is: quotes become ', line
breaks spaces, a run of more than 40 characters with no space is broken, and a leading @ (a string-table
name to the menus) gets ^7 in front. The text may then be at most 255 characters, an option 64.
===========================================================================
*/

#include "rp_lua_local.h"

#define RP_DLG_MAX_OPTIONS		6
#define RP_DLG_TEXT_MAX			255
#define RP_DLG_TITLE_MAX		48
#define RP_DLG_OPTION_MAX		64
#define RP_DLG_WORD_MAX			40		// a run of more characters with no space is broken
#define RP_DLG_RANGE_DEFAULT	256
#define RP_DLG_RANGE_MAX		8192
#define RP_DLG_TIMEOUT_DEFAULT	60
#define RP_DLG_TIMEOUT_MIN		5
#define RP_DLG_TIMEOUT_MAX		600

typedef struct {
	qboolean	open;
	int			plugin;
	int			ref;			// the script's function
	int			id;
	int			ends;			// level.time
	int			speaker;		// entity number, -1 for none
	int			speakerGen;
	float		range;			// 0: never too far
	int			count;
	char		title[RP_DLG_TITLE_MAX + 1];
	char		text[RP_DLG_TEXT_MAX + 1];
	char		options[RP_DLG_MAX_OPTIONS][RP_DLG_OPTION_MAX + 1];
} rpLuaDialog_t;

static rpLuaDialog_t	rpDialogs[MAX_CLIENTS];
static int				rpDialogNextId = 1;

/*
================
Text
================
*/
// cleans a script's text as §1.4 of the scope says; qfalse when the result is longer than max
static qboolean RP_LuaDialog_Clean( char *out, int max, const char *in, qboolean breakWords ) {
	char clean[RP_LUA_TEXT_MAX + 1];
	int i, o = 0, run = 0;

	RP_Lua_CleanText( clean, sizeof( clean ), in, qfalse );
	if ( clean[0] == '@' ) {
		if ( o + 2 > max ) {
			return qfalse;
		}
		out[o++] = '^';
		out[o++] = '7';
	}
	for ( i = 0; clean[i]; i++ ) {
		char c = clean[i];
		if ( c == '\t' ) {
			c = ' ';
		}
		if ( c == ' ' ) {
			run = 0;
		} else if ( breakWords && ++run > RP_DLG_WORD_MAX ) {
			if ( o >= max ) {
				return qfalse;
			}
			out[o++] = ' ';
			run = 1;
		}
		if ( o >= max ) {
			return qfalse;
		}
		out[o++] = c;
	}
	out[o] = '\0';
	return qtrue;
}

static void RP_LuaDialog_Show( int client ) {
	rpLuaDialog_t *d = &rpDialogs[client];
	int i;

	if ( d->title[0] ) {
		trap->SendServerCommand( client, va( "chat \"^3%s^7: %s\"", d->title, d->text ) );
	} else {
		trap->SendServerCommand( client, va( "chat \"%s\"", d->text ) );
	}
	for ( i = 0; i < d->count; i++ ) {
		trap->SendServerCommand( client, va( "chat \"  ^3%d.^7 %s\"", i + 1, d->options[i] ) );
	}
	trap->SendServerCommand( client, "chat \"^7(type ^3/choose <number>^7 or say the number)\"" );
}

/*
================
Closing
================
*/
typedef struct {
	int			ref;
	int			client;
	int			choice;			// 0: none
	const char	*why;
} rpLuaDialogCall_t;

static int RP_LuaDialog_Worker( lua_State *L, void *data ) {
	rpLuaDialogCall_t *c = (rpLuaDialogCall_t *)data;

	lua_rawgeti( L, LUA_REGISTRYINDEX, c->ref );
	RP_LuaApi_PushPlayer( L, c->client );
	if ( c->choice > 0 ) {
		lua_pushinteger( L, c->choice );
		lua_pushnil( L );
	} else {
		lua_pushnil( L );
		lua_pushstring( L, c->why );
	}
	lua_call( L, 3, 0 );
	return 0;
}

// the dialogue is over: the slot is free first (the script's function may open the next one), then the function
// hears it once, unless the plugin no longer runs
static void RP_LuaDialog_Finish( int client, int choice, const char *why ) {
	rpLuaDialog_t *d = &rpDialogs[client];
	rpLuaDialogCall_t c;
	int plugin = d->plugin;

	if ( !d->open ) {
		return;
	}
	c.ref = d->ref;
	c.client = client;
	c.choice = choice;
	c.why = why;
	d->open = qfalse;
	d->ref = 0;
	if ( rl.L && c.ref > 0 ) {
		if ( RP_Lua_PluginRunning( plugin ) ) {
			RP_Lua_Call( plugin, RP_LuaDialog_Worker, &c );
		}
		if ( rl.L ) {
			luaL_unref( rl.L, LUA_REGISTRYINDEX, c.ref );
		}
	}
}

// what closes a dialogue by itself, checked each frame and before an answer is taken: NULL while it may stay open
static const char *RP_LuaDialog_Over( int client ) {
	rpLuaDialog_t *d = &rpDialogs[client];
	gentity_t *ent = &g_entities[client];
	gclient_t *cl = &level.clients[client];

	if ( !ent->inuse || !ent->client || cl->pers.connected != CON_CONNECTED || cl->sess.sessionTeam == TEAM_SPECTATOR
		|| cl->tempSpectate >= level.time || ent->health <= 0 || cl->ps.pm_type == PM_DEAD || G_PlayerIsDowned( ent ) ) {
		return "died";
	}
	if ( level.time >= d->ends ) {
		return "timeout";
	}
	if ( d->speaker >= 0 ) {
		gentity_t *sp = &g_entities[d->speaker];
		vec3_t at;
		if ( !sp->inuse || g_entityGen[d->speaker] != d->speakerGen ) {
			return "away";
		}
		if ( d->range > 0 ) {
			RP_LuaWorld_Center( sp, at );
			if ( Distance( at, cl->ps.origin ) > d->range ) {
				return "away";
			}
		}
	}
	return NULL;
}

void RP_LuaDialog_Frame( void ) {
	int i;

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		const char *why;
		if ( !rpDialogs[i].open ) {
			continue;
		}
		why = RP_LuaDialog_Over( i );
		if ( !why ) {
			continue;
		}
		if ( !RP_Lua_FrameBudgetLeft() ) {
			RP_Lua_NoteDeferred( rpDialogs[i].plugin, 1 );
			return;		// still open: the next frame closes it
		}
		if ( strcmp( why, "died" ) ) {
			trap->SendServerCommand( i, "chat \"^7(the conversation is over)\"" );
		}
		RP_LuaDialog_Finish( i, 0, why );
	}
}

void RP_LuaDialog_ClientGone( int client ) {
	if ( client >= 0 && client < MAX_CLIENTS && rpDialogs[client].open ) {
		RP_LuaDialog_Finish( client, 0, "left" );
	}
}

void RP_LuaDialog_RemovePlugin( int plugin ) {
	int i;

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		rpLuaDialog_t *d = &rpDialogs[i];
		if ( d->open && d->plugin == plugin ) {
			if ( rl.L && d->ref > 0 ) {
				luaL_unref( rl.L, LUA_REGISTRYINDEX, d->ref );
			}
			d->ref = 0;
			d->open = qfalse;
		}
	}
}

void RP_LuaDialog_Reset( void ) {
	memset( rpDialogs, 0, sizeof( rpDialogs ) );
	rpDialogNextId = 1;
}

/*
================
Answers: /choose and chat
================
*/
// an answer for client's dialogue: 0 cancels; id 0 is "the one open"
static qboolean RP_LuaDialog_Answer( int client, int choice, int id ) {
	rpLuaDialog_t *d = &rpDialogs[client];
	const char *why;

	if ( !d->open || ( id && id != d->id ) || choice < 0 || choice > d->count ) {
		return qfalse;
	}
	why = RP_LuaDialog_Over( client );
	if ( why ) {	// it ended before this frame's check: closed for that reason, as RP_LuaDialog_Frame would
		if ( strcmp( why, "died" ) ) {
			trap->SendServerCommand( client, "chat \"^7(the conversation is over)\"" );
		}
		RP_LuaDialog_Finish( client, 0, why );
		return qfalse;
	}
	RP_LuaDialog_Finish( client, choice, "cancelled" );
	return qtrue;
}

// a whole number of at most 2 digits, nothing else (spaces around it allowed); -1 for anything else
static int RP_LuaDialog_Number( const char *s ) {
	int n = 0, digits = 0;

	while ( *s == ' ' ) {
		s++;
	}
	while ( *s >= '0' && *s <= '9' ) {
		n = n * 10 + ( *s - '0' );
		if ( ++digits > 2 ) {
			return -1;
		}
		s++;
	}
	while ( *s == ' ' ) {
		s++;
	}
	return ( digits && !*s ) ? n : -1;
}

void Cmd_Choose_f( gentity_t *ent ) {
	char arg[MAX_TOKEN_CHARS];
	int client = ent->s.number, choice, id = 0;
	rpLuaDialog_t *d = ( client >= 0 && client < MAX_CLIENTS ) ? &rpDialogs[client] : NULL;

	if ( !d || !d->open ) {
		trap->SendServerCommand( client, "print \"You have no dialogue open.\n\"" );
		return;
	}
	if ( trap->Argc() < 2 ) {
		RP_LuaDialog_Show( client );
		return;
	}
	trap->Argv( 1, arg, sizeof( arg ) );
	choice = RP_LuaDialog_Number( arg );
	if ( trap->Argc() > 2 ) {
		trap->Argv( 2, arg, sizeof( arg ) );
		id = atoi( arg );
		if ( id <= 0 ) {
			return;
		}
	}
	if ( choice < 0 || choice > d->count ) {
		trap->SendServerCommand( client, va( "print \"Answer with ^3/choose <1 to %d>^7 (0 to end the conversation).\n\"", d->count ) );
		return;
	}
	RP_LuaDialog_Answer( client, choice, id );	// a stale id (an old menu) is ignored
}

qboolean RP_Lua_DialogChat( gentity_t *ent, const char *text ) {
	char clean[MAX_SAY_TEXT];
	int client, n;

	if ( !ent || !ent->client || !text ) {
		return qfalse;
	}
	client = ent->s.number;
	if ( client < 0 || client >= MAX_CLIENTS || !rpDialogs[client].open ) {
		return qfalse;
	}
	Q_strncpyz( clean, text, sizeof( clean ) );
	Q_StripColor( clean );
	n = RP_LuaDialog_Number( clean );
	if ( n < 1 || n > rpDialogs[client].count ) {
		return qfalse;		// ordinary chat
	}
	RP_LuaDialog_Answer( client, n, 0 );
	return qtrue;
}

/*
================
The Dialog table
================
*/
static int RP_LuaDialog_IntField( lua_State *L, const char *field, int def, int lo, int hi ) {
	lua_Integer v;
	int isnum;

	if ( lua_getfield( L, 2, field ) == LUA_TNIL ) {
		lua_pop( L, 1 );
		return def;
	}
	v = lua_tointegerx( L, -1, &isnum );
	if ( !isnum || v < lo || v > hi ) {
		luaL_error( L, "Dialog.open: %s must be a whole number from %d to %d", field, lo, hi );
		return def;
	}
	lua_pop( L, 1 );
	return (int)v;
}

// a string field cleaned into out; qfalse when absent
static qboolean RP_LuaDialog_StrField( lua_State *L, int t, const char *field, char *out, int max, qboolean breakWords ) {
	if ( lua_getfield( L, t, field ) == LUA_TNIL ) {
		lua_pop( L, 1 );
		return qfalse;
	}
	if ( lua_type( L, -1 ) != LUA_TSTRING ) {
		luaL_error( L, "Dialog.open: %s must be a string", field );
		return qfalse;
	}
	if ( !RP_LuaDialog_Clean( out, max, lua_tostring( L, -1 ), breakWords ) ) {
		luaL_error( L, "Dialog.open: %s is too long (at most %d characters%s)", field, max,
			breakWords ? ", counting a space put into every word over 40" : "" );
		return qfalse;
	}
	lua_pop( L, 1 );
	return qtrue;
}

static int RP_LuaDialog_Fail( lua_State *L, const char *why ) {
	lua_pushnil( L );
	lua_pushstring( L, why );
	return 2;
}

static int RP_LuaDialog_Open( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	int client = RP_LuaApi_CheckPlayer( L, 1 );
	rpLuaDialog_t nd, *d = &rpDialogs[client];
	int oldRef = 0, oldPlugin = -1, i, n;
	const char *why;

	memset( &nd, 0, sizeof( nd ) );
	luaL_checktype( L, 2, LUA_TTABLE );
	luaL_checktype( L, 3, LUA_TFUNCTION );

	if ( !RP_LuaDialog_StrField( L, 2, "text", nd.text, RP_DLG_TEXT_MAX, qtrue ) || !nd.text[0] ) {
		return luaL_error( L, "Dialog.open: a text is required" );
	}
	RP_LuaDialog_StrField( L, 2, "title", nd.title, RP_DLG_TITLE_MAX, qtrue );
	if ( lua_getfield( L, 2, "options" ) != LUA_TTABLE ) {
		return luaL_error( L, "Dialog.open: options must be a list of 1 to %d strings", RP_DLG_MAX_OPTIONS );
	}
	n = (int)luaL_len( L, -1 );
	if ( n < 1 || n > RP_DLG_MAX_OPTIONS ) {
		return luaL_error( L, "Dialog.open: options must be a list of 1 to %d strings", RP_DLG_MAX_OPTIONS );
	}
	for ( i = 0; i < n; i++ ) {
		if ( lua_rawgeti( L, -1, i + 1 ) != LUA_TSTRING ) {
			return luaL_error( L, "Dialog.open: option %d must be a string", i + 1 );
		}
		if ( !RP_LuaDialog_Clean( nd.options[i], RP_DLG_OPTION_MAX, lua_tostring( L, -1 ), qfalse ) || !nd.options[i][0] ) {
			return luaL_error( L, "Dialog.open: option %d must be 1 to %d characters", i + 1, RP_DLG_OPTION_MAX );
		}
		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );
	nd.count = n;

	nd.speaker = -1;
	if ( lua_getfield( L, 2, "speaker" ) != LUA_TNIL ) {
		gentity_t *sp;
		if ( !RP_LuaWorld_IsEntityArg( L, -1 ) ) {
			return luaL_error( L, "Dialog.open: speaker must be an entity handle (the NPC or object the player talks to)" );
		}
		sp = RP_LuaWorld_CheckEntityArg( L, -1 );
		nd.speaker = sp->s.number;
		nd.speakerGen = g_entityGen[nd.speaker];
	}
	lua_pop( L, 1 );
	nd.range = (float)RP_LuaDialog_IntField( L, "range", RP_DLG_RANGE_DEFAULT, 0, RP_DLG_RANGE_MAX );
	nd.ends = level.time + RP_LuaDialog_IntField( L, "timeout", RP_DLG_TIMEOUT_DEFAULT, RP_DLG_TIMEOUT_MIN, RP_DLG_TIMEOUT_MAX ) * 1000;

	if ( d->open && d->plugin != plugin ) {
		return RP_LuaDialog_Fail( L, "they are in a dialogue of another plugin" );
	}
	nd.open = qtrue;
	nd.plugin = plugin;
	nd.id = rpDialogNextId++;
	if ( rpDialogNextId <= 0 ) {
		rpDialogNextId = 1;
	}
	{	// the player must be able to take part: the same test that closes one by itself (timeout and range pass here)
		rpLuaDialog_t keep = *d;
		*d = nd;
		why = RP_LuaDialog_Over( client );
		*d = keep;
	}
	if ( why ) {
		return RP_LuaDialog_Fail( L, !strcmp( why, "away" ) ? "they are too far from the speaker" : "they are not alive (spectating, dead or knocked down)" );
	}

	// the new one goes in first, then the one it replaces hears it: that function may open yet another, which then
	// replaces this one in turn
	if ( d->open ) {
		oldRef = d->ref;
		oldPlugin = d->plugin;
	}
	lua_pushvalue( L, 3 );
	nd.ref = luaL_ref( L, LUA_REGISTRYINDEX );
	*d = nd;
	RP_LuaDialog_Show( client );
	if ( oldRef > 0 ) {
		rpLuaDialogCall_t c;
		c.ref = oldRef;
		c.client = client;
		c.choice = 0;
		c.why = "replaced";
		if ( RP_Lua_PluginRunning( oldPlugin ) ) {
			RP_Lua_Call( oldPlugin, RP_LuaDialog_Worker, &c );
		}
		luaL_unref( L, LUA_REGISTRYINDEX, oldRef );
	}
	lua_pushinteger( L, nd.id );
	return 1;
}

static int RP_LuaDialog_Close( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	int client = RP_LuaApi_CheckPlayer( L, 1 );

	if ( !rpDialogs[client].open || rpDialogs[client].plugin != plugin ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	RP_LuaDialog_Finish( client, 0, "closed" );
	lua_pushboolean( L, 1 );
	return 1;
}

static int RP_LuaDialog_IsOpen( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	int client = RP_LuaApi_CheckPlayer( L, 1 );

	lua_pushboolean( L, rpDialogs[client].open && rpDialogs[client].plugin == plugin );
	return 1;
}

void RP_LuaDialog_Register( lua_State *L ) {
	static const luaL_Reg funcs[] = {
		{ "open", RP_LuaDialog_Open }, { "close", RP_LuaDialog_Close }, { "is_open", RP_LuaDialog_IsOpen }, { NULL, NULL } };
	int g = lua_absindex( L, -1 );

	lua_newtable( L );
	luaL_setfuncs( L, funcs, 0 );
	lua_setfield( L, g, "Dialog" );
}
