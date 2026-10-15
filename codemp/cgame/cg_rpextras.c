/*
===========================================================================
GalaxyRP: [Extras] the cgame half of the Extras menus of the Galaxy RP menu (Props, Effects, NPCs &
Vehicles, Music, Sounds, Lights, Spawners). The menus themselves are the ui's (ui_main.c); ui/rp_extras.h says how the
two work together. Here:

- "rpx list <kind>": gets the listing of the kind's current folder (ui_rpx_dir_<kind>) from the server
  (/rpxlist, g_rplist.c), keeps it until the map changes, and hands it to the ui through a list file
  (rpxListFiles in rp_extras.h). The server sends a listing in batches and the next batch is asked for only when one
  has arrived, spaced by the server's sv_floodProtect: a request that comes sooner is ignored by the
  server without a word. A request with no answer is sent again; "busy" (the server is building
  another listing) is asked again a moment later.
- "rpx do <action>": the command a menu button stands for, built here from the menu's cvars and
  sent with SendClientCommand -- never through the console, so a name cannot add a command of its
  own -- and only with a name from a listing the server sent. Action commands wait for the same
  flood-protection gap as the list requests.
- The effect preview: "rpx do fxpreview" plays the selected effect as a sky-portal effect at
  RPX_FX_ORIGIN, and CG_RpxFxPass() adds the sky-portal effects to the scene at the end of the
  frame, after everything else cgame draws, seen from the preview camera. The ui draws its menu
  after cgame and renders that scene in its preview box without clearing it first. Only sky-portal
  effects are updated in a sky-portal pass, and a map with a sky portal updates them in its own
  pass; so while the preview is being drawn, that pass leaves them to this one
  (CG_RpxFxPassActive()) -- each is still updated once a frame, and the sky's own effects are only
  not drawn in the sky for those few seconds.

  What the preview does not show as the spawned effect is, the effects system being what it is
  (FxScheduler.cpp): a part that needs a surface (a mark, a bounce); a sub-effect played after a
  delay -- a sub-effect is always played as a normal effect, so one that waits is made in the main
  view, at RPX_FX_ORIGIN, where nobody sees it; and a sound: in a sky-portal pass the effects system
  plays an effect's sounds as local sounds, so the player hears them, at full volume, as if there.
===========================================================================
*/

#include "cg_local.h"
#include "ui/rp_extras.h"

#define RPX_ARENA_SIZE		0x100000
#define RPX_CACHE_MAX		64
#define RPX_LISTING_MAX		( RPX_MAX_ENTRIES * RPX_NAME_LEN )	// the most one listing can take
#define RPX_REPLY_TIMEOUT	1500		// on top of the flood gap: an unanswered request is sent again then
#define RPX_QUEUE			4
#define RPX_MAX_TRIES		4
#define RPX_BUSY_RETRY		400
#define RPX_MAX_BUSY		40

typedef struct {
	int			kind;
	char		folder[RPX_NAME_LEN];
	int			num;
	qboolean	truncated;
	int			start;		// where its names start in rpx_arena, one after another, each ending in '\0'
	int			size;
} rpxListing_t;

static char			rpx_arena[RPX_ARENA_SIZE];
static int			rpx_arenaUsed;
static rpxListing_t	rpx_cache[RPX_CACHE_MAX];
static int			rpx_numCached;

static struct {
	qboolean	active;
	int			kind;
	char		folder[RPX_NAME_LEN];
	int			reqId;
	int			total;		// -1 until the first answer
	int			consumed;	// names the server has sent: where the next request starts
	int			num;		// names kept
	qboolean	truncated;
	int			start;		// where in rpx_arena this listing is being written
	int			used;
	qboolean	needSend;
	int			waitUntil;	// not before (after "busy")
	int			sentAt;		// when the request now unanswered went; 0 for none
	int			tries;
	int			busy;
} rpx_fetch;

static struct {
	int			windowEnd;	// Milliseconds() until which the preview is drawn; 0 for none
	int			pendingId;	// an effect to play at the next pass
	int			dirMode;
	float		pitch;
	char		previewed[RPX_PREVIEW_EFFECTS][RPX_NAME_LEN * 2];
	int			numPreviewed;
} rpx_fx;

static int			rpx_reqCounter;
static int			rpx_lastSend;
static char			rpx_queue[RPX_QUEUE][MAX_STRING_CHARS];	// action commands waiting for the flood gap
static int			rpx_queued;
static int			rpx_serial;
static qboolean		rpx_fxPass;
static int			rpx_viewYaw = -1;	// the view yaw last told the ui (ui_rpx_viewyaw), -1 for none yet

/*
===========================================================================
Helpers
===========================================================================
*/

static void RPX_CvarString( const char *name, char *buf, int size ) {
	trap->Cvar_VariableStringBuffer( name, buf, size );
}

static float RPX_CvarFloat( const char *name ) {
	char buf[64];

	trap->Cvar_VariableStringBuffer( name, buf, sizeof( buf ) );
	return atof( buf );
}

static int RPX_CvarInt( const char *name ) {
	char buf[64];

	trap->Cvar_VariableStringBuffer( name, buf, sizeof( buf ) );
	return atoi( buf );
}

static void RPX_Status( const char *text ) {
	trap->Cvar_Set( "ui_rpx_status", text );
}

static void RPX_Msg( const char *text ) {
	trap->Cvar_Set( "ui_rpx_msg", text );
}

// a refusal after the menu has closed: on screen, as a console line, and in the menu for next time
static void RPX_Refuse( const char *text ) {
	RPX_Msg( text );
	trap->Print( S_COLOR_YELLOW "%s\n", text );
}

static int RPX_KindByName( const char *name ) {
	int i;

	for ( i = 0; i < RPX_NUM_KINDS; i++ ) {
		if ( !Q_stricmp( name, rpxKinds[i].name ) ) {
			return i;
		}
	}
	return -1;
}

// the gap the server's sv_floodProtect wants between two commands from one client, with room to spare
static int RPX_FloodGap( void ) {
	const char *info = CG_ConfigString( CS_SERVERINFO );
	int v = atoi( Info_ValueForKey( info, "sv_floodProtect" ) );

	if ( v <= 0 ) {
		return 50;
	}
	if ( v == 1 ) {
		v = 1000;
	}
	if ( v > 20000 ) {
		v = 20000;
	}
	return v + 60;
}

// the gap is from the last command to the server this client sent, and not only the ones sent from
// here count: the ui stamps ui_rpx_sent when a menu of its sends one (the Galaxy RP menu's "zykmod"
// as it opens). With sv_floodProtectSlow a command that comes too soon is not only ignored, it
// starts the wait again.
static qboolean RPX_CanSend( int now ) {
	int gap = RPX_FloodGap();
	int ui = RPX_CvarInt( "ui_rpx_sent" );

	if ( rpx_lastSend && now - rpx_lastSend < gap ) {
		return qfalse;
	}
	if ( ui && now - ui >= 0 && now - ui < gap ) {
		return qfalse;
	}
	return qtrue;
}

static void RPX_Send( const char *cmd, int now ) {
	trap->SendClientCommand( cmd );
	rpx_lastSend = now ? now : 1;
}

// the folder of a kind's list now, cleaned; a bad one is put back to the kind's default
static void RPX_CurrentFolder( int kind, char *folder, int size ) {
	if ( !rpxKinds[kind].root ) {
		folder[0] = '\0';
		return;
	}
	RPX_CvarString( va( "ui_rpx_dir_%s", rpxKinds[kind].name ), folder, size );
	if ( !RPX_FolderOk( folder ) ) {
		Q_strncpyz( folder, rpxKinds[kind].defaultFolder, size );
		trap->Cvar_Set( va( "ui_rpx_dir_%s", rpxKinds[kind].name ), folder );
	}
}

static rpxListing_t *RPX_FindListing( int kind, const char *folder ) {
	int i;

	for ( i = 0; i < rpx_numCached; i++ ) {
		if ( rpx_cache[i].kind == kind && !Q_stricmp( rpx_cache[i].folder, folder ) ) {
			return &rpx_cache[i];
		}
	}
	return NULL;
}

// whether name is a file (not a folder) of that listing
static qboolean RPX_ListingHasFile( const rpxListing_t *l, const char *name ) {
	const char *p = rpx_arena + l->start;
	int i;

	for ( i = 0; i < l->num; i++ ) {
		if ( !strcmp( p, name ) ) {
			return qtrue;
		}
		p += strlen( p ) + 1;
	}
	return qfalse;
}

/*
===========================================================================
The list file
===========================================================================
*/

// the list into one file; qfalse when it could not be opened
static qboolean RPX_WriteListFile( const char *file, int kind, const char *folder, const char *names, int num, qboolean truncated, int serial ) {
	fileHandle_t f = 0;
	char buf[0x4000];
	int used = 0, i;
	const char *p = names;

	trap->FS_Open( file, &f, FS_WRITE );
	if ( !f ) {
		return qfalse;
	}

	Com_sprintf( buf, sizeof( buf ), "%s\t%d\t%s\t%s\t%d\t%d\n", RPX_LIST_MAGIC, serial, rpxKinds[kind].name, folder, num, truncated ? 1 : 0 );
	used = strlen( buf );

	for ( i = 0; i < num; i++ ) {
		int len = strlen( p );

		if ( used + len + 1 > (int)sizeof( buf ) ) {
			trap->FS_Write( buf, used, f );
			used = 0;
		}
		memcpy( buf + used, p, len );
		used += len;
		buf[used++] = '\n';
		p += len + 1;
	}
	if ( used ) {
		trap->FS_Write( buf, used, f );
	}
	trap->FS_Close( f );
	return qtrue;
}

// whether reading the file finds the one just written (see rpxListFiles in rp_extras.h)
static qboolean RPX_ListFileReadsBack( const char *file, int serial ) {
	fileHandle_t f = 0;
	char head[64], want[64];
	int len = trap->FS_Open( file, &f, FS_READ );
	int n;

	if ( !f ) {
		return qfalse;
	}
	n = ( len < (int)sizeof( head ) - 1 ) ? len : (int)sizeof( head ) - 1;
	if ( n < 0 ) {
		n = 0;
	}
	trap->FS_Read( head, n, f );
	trap->FS_Close( f );
	head[n] = '\0';

	Com_sprintf( want, sizeof( want ), "%s\t%d\t", RPX_LIST_MAGIC, serial );
	return strncmp( head, want, strlen( want ) ) ? qfalse : qtrue;
}

static int rpx_fileIndex;

static void RPX_WriteList( int kind, const char *folder, const char *names, int num, qboolean truncated ) {
	int serial = rpx_serial + 1;
	int tries;

	for ( tries = 0; tries < RPX_NUM_LIST_FILES; tries++ ) {
		const char *file = rpxListFiles[rpx_fileIndex];

		if ( RPX_WriteListFile( file, kind, folder, names, num, truncated, serial ) && RPX_ListFileReadsBack( file, serial ) ) {
			rpx_serial = serial;
			trap->Cvar_Set( "ui_rpx_file", file );
			trap->Cvar_Set( "ui_rpx_serial", va( "%d", rpx_serial ) );
			return;
		}
		rpx_fileIndex = ( rpx_fileIndex + 1 ) % RPX_NUM_LIST_FILES;
	}
	RPX_Status( "The list could not be written to your game folder." );
}

/*
===========================================================================
Listings
===========================================================================
*/

static void RPX_FetchFail( const char *why ) {
	rpx_fetch.active = qfalse;
	RPX_Status( why );
	RPX_WriteList( rpx_fetch.kind, rpx_fetch.folder, "", 0, qfalse );
}

static void RPX_FetchDone( void ) {
	rpxListing_t *l = &rpx_cache[rpx_numCached++];	// RPX_List() made sure there is room

	l->kind = rpx_fetch.kind;
	Q_strncpyz( l->folder, rpx_fetch.folder, sizeof( l->folder ) );
	l->num = rpx_fetch.num;
	l->truncated = rpx_fetch.truncated;
	l->start = rpx_fetch.start;
	l->size = rpx_fetch.used;
	rpx_arenaUsed = rpx_fetch.start + rpx_fetch.used;

	rpx_fetch.active = qfalse;
	RPX_Status( l->truncated ? "^3There are more here than one listing can hold, so some are missing." : "" );
	RPX_WriteList( l->kind, l->folder, rpx_arena + l->start, l->num, l->truncated );
}

static void RPX_FetchRestart( void ) {
	rpx_fetch.reqId = ++rpx_reqCounter;
	if ( rpx_reqCounter >= 999999999 ) {
		rpx_reqCounter = 0;
	}
	rpx_fetch.total = -1;
	rpx_fetch.consumed = 0;
	rpx_fetch.num = 0;
	rpx_fetch.used = 0;
	rpx_fetch.truncated = qfalse;
	rpx_fetch.needSend = qtrue;
	rpx_fetch.waitUntil = 0;
	rpx_fetch.sentAt = 0;
	rpx_fetch.tries = 0;
	rpx_fetch.busy = 0;
}

// "rpx list <kind>"
static void RPX_List( int kind ) {
	char folder[RPX_NAME_LEN];
	rpxListing_t *l;

	RPX_CurrentFolder( kind, folder, sizeof( folder ) );

	l = RPX_FindListing( kind, folder );
	if ( l ) {
		rpx_fetch.active = qfalse;	// whatever was on its way is not what the menu shows now
		RPX_Status( l->truncated ? "^3There are more here than one listing can hold, so some are missing." : "" );
		RPX_WriteList( kind, folder, rpx_arena + l->start, l->num, l->truncated );
		return;
	}

	if ( rpx_fetch.active && rpx_fetch.kind == kind && !Q_stricmp( rpx_fetch.folder, folder ) ) {
		return;	// already on its way
	}

	// room for the largest listing there can be, and for keeping it: otherwise start again empty
	if ( rpx_numCached >= RPX_CACHE_MAX || rpx_arenaUsed + RPX_LISTING_MAX > RPX_ARENA_SIZE ) {
		rpx_numCached = 0;
		rpx_arenaUsed = 0;
	}

	rpx_fetch.active = qtrue;
	rpx_fetch.kind = kind;
	Q_strncpyz( rpx_fetch.folder, folder, sizeof( rpx_fetch.folder ) );
	rpx_fetch.start = rpx_arenaUsed;
	RPX_FetchRestart();
	RPX_Status( "Loading..." );
}

static void RPX_SendRequest( int now ) {
	const char *folder = rpx_fetch.folder[0] ? rpx_fetch.folder : ".";

	RPX_Send( va( "rpxlist %s %d %d \"%s\"", rpxKinds[rpx_fetch.kind].name, rpx_fetch.reqId, rpx_fetch.consumed, folder ), now );
	rpx_fetch.needSend = qfalse;
	rpx_fetch.sentAt = now ? now : 1;
}

static qboolean RPX_IsDigits( const char *s, int maxLen ) {
	int i;

	if ( !s[0] || (int)strlen( s ) > maxLen ) {
		return qfalse;
	}
	for ( i = 0; s[i]; i++ ) {
		if ( s[i] < '0' || s[i] > '9' ) {
			return qfalse;
		}
	}
	return qtrue;
}

/*
==================
CG_RpxReply

"rpxl <kind> <request id> ..." from the server -- see RP_ListDataCommand() in g_rplist.c.
==================
*/
void CG_RpxReply( void ) {
	char kind[32], req[32], word[32], flags[16];
	char names[MAX_STRING_CHARS];
	int now = trap->Milliseconds();
	int total, offset;
	qboolean files;
	char *p;

	Q_strncpyz( kind, CG_Argv( 1 ), sizeof( kind ) );
	Q_strncpyz( req, CG_Argv( 2 ), sizeof( req ) );
	Q_strncpyz( word, CG_Argv( 3 ), sizeof( word ) );

	if ( !rpx_fetch.active || Q_stricmp( kind, rpxKinds[rpx_fetch.kind].name ) || !RPX_IsDigits( req, 9 ) || atoi( req ) != rpx_fetch.reqId ) {
		return;	// an answer to a request given up on
	}
	rpx_fetch.tries = 0;

	if ( !Q_stricmp( word, "busy" ) ) {
		rpx_fetch.sentAt = 0;
		if ( ++rpx_fetch.busy > RPX_MAX_BUSY ) {
			RPX_FetchFail( "The server is busy. Try again in a moment." );
			return;
		}
		rpx_fetch.needSend = qtrue;
		rpx_fetch.waitUntil = now + RPX_BUSY_RETRY;
		RPX_Status( "Waiting for the server..." );
		return;
	}
	if ( !Q_stricmp( word, "err" ) ) {
		char why[256];
		int i;

		Q_strncpyz( why, CG_Argv( 4 ), sizeof( why ) );
		for ( i = 0; why[i]; i++ ) {
			if ( (unsigned char)why[i] < 32 ) {
				why[i] = ' ';
			}
		}
		RPX_FetchFail( why[0] ? why : "The server refused the listing." );
		return;
	}

	if ( !RPX_IsDigits( word, 6 ) || !RPX_IsDigits( CG_Argv( 4 ), 6 ) ) {
		return;
	}
	total = atoi( word );
	offset = atoi( CG_Argv( 4 ) );
	Q_strncpyz( flags, CG_Argv( 5 ), sizeof( flags ) );
	Q_strncpyz( names, CG_Argv( 6 ), sizeof( names ) );

	if ( offset != rpx_fetch.consumed ) {
		return;	// a repeat of a part already here: its request was sent again
	}
	if ( rpx_fetch.total >= 0 && total != rpx_fetch.total ) {
		RPX_FetchRestart();	// the listing changed on the server between two parts
		return;
	}
	rpx_fetch.total = total;
	if ( strchr( flags, 't' ) ) {
		rpx_fetch.truncated = qtrue;
	}

	files = rpxKinds[rpx_fetch.kind].root ? qtrue : qfalse;
	p = names;
	while ( *p ) {
		char *bar = strchr( p, '|' );
		int len;

		if ( bar ) {
			*bar = '\0';
		}
		len = strlen( p );
		rpx_fetch.consumed++;
		if ( RPX_NameOk( p, files ) ) {
			if ( rpx_fetch.num >= RPX_MAX_ENTRIES || rpx_fetch.used + len + 1 > RPX_LISTING_MAX ) {
				rpx_fetch.truncated = qtrue;
			} else {
				memcpy( rpx_arena + rpx_fetch.start + rpx_fetch.used, p, len + 1 );
				rpx_fetch.used += len + 1;
				rpx_fetch.num++;
			}
		}
		if ( !bar ) {
			break;
		}
		p = bar + 1;
	}

	if ( flags[0] == 'e' || rpx_fetch.consumed >= rpx_fetch.total ) {
		RPX_FetchDone();
		return;
	}
	if ( flags[0] == 'm' ) {
		rpx_fetch.needSend = qtrue;
		rpx_fetch.sentAt = 0;
	} else {
		rpx_fetch.sentAt = now ? now : 1;	// more of this answer is on its way
	}
	RPX_Status( va( "Loading... %d of %d", rpx_fetch.consumed, rpx_fetch.total ) );
}

/*
===========================================================================
Actions
===========================================================================
*/

// the selected file of a kind's list, checked against the listing the server sent; *folder is its folder
static qboolean RPX_Selected( int kind, char *folder, int folderSize, char *name, int nameSize ) {
	rpxListing_t *l;

	RPX_CurrentFolder( kind, folder, folderSize );
	RPX_CvarString( va( "ui_rpx_sel_%s", rpxKinds[kind].name ), name, nameSize );
	if ( !name[0] ) {
		RPX_Refuse( "Select one from the list first." );
		return qfalse;
	}
	l = RPX_FindListing( kind, folder );
	if ( !RPX_NameOk( name, qfalse ) || !l || !RPX_ListingHasFile( l, name ) ) {
		RPX_Refuse( "That is not in the list the server sent. Open the list again." );
		return qfalse;
	}
	return qtrue;
}

// folder + "/" + name, or name alone in the root, with a prefix and a suffix
static void RPX_Path( char *out, int size, const char *prefix, const char *folder, const char *name, const char *suffix ) {
	Com_sprintf( out, size, "%s%s%s%s%s", prefix, folder, folder[0] ? "/" : "", name, suffix );
}

// an action command, sent once the flood gap allows (CG_RpxFrame()); with the queue full, the oldest
// goes -- it takes four buttons pressed within one gap
static void RPX_Queue( const char *cmd ) {
	if ( rpx_queued >= RPX_QUEUE ) {
		memmove( rpx_queue[0], rpx_queue[1], sizeof( rpx_queue[0] ) * ( RPX_QUEUE - 1 ) );
		rpx_queued = RPX_QUEUE - 1;
	}
	Q_strncpyz( rpx_queue[rpx_queued++], cmd, sizeof( rpx_queue[0] ) );
}

// GalaxyRP: [Extras] the Props, Effects and Spawners menus' Offset: /entaddaim's aimoffset, 0 to 512, sent
// only when it is not 0 -- the command is then the one these menus always sent
static void RPX_AddOffset( char *cmd, int size, const char *cvar ) {
	int lift = RPX_CvarInt( cvar );

	if ( lift > 512 ) lift = 512;
	if ( lift > 0 ) {
		Q_strcat( cmd, size, va( " aimoffset \"%d\"", lift ) );
	}
}

// DAJ_RP: [Extras] the command a Spawn button sends, or its Grab button: /entaddgrab takes the same arguments
// as /entaddaim, aimoffset too, but picks the new entity up, to be placed with /entaddgrab again (g_entgrab.c)
static const char *RPX_AddVerb( qboolean grab ) {
	return grab ? "entaddgrab" : "entaddaim";
}

static int RPX_FacingYaw( float extra ) {
	float yaw = AngleNormalize360( cg.predictedPlayerState.viewangles[YAW] + 180.0f + extra );

	return (int)( yaw + 0.5f ) % 360;
}

static void RPX_SpawnProp( qboolean grab ) {
	char folder[RPX_NAME_LEN], name[RPX_NAME_LEN], path[MAX_QPATH * 2], cmd[MAX_STRING_CHARS];
	int pitch, yaw, roll, flags;
	float scale;

	if ( !RPX_Selected( RPX_MODELS, folder, sizeof( folder ), name, sizeof( name ) ) ) {
		return;
	}
	RPX_Path( path, sizeof( path ), "models/", folder, name, ".md3" );
	if ( strlen( path ) >= MAX_QPATH ) {
		RPX_Refuse( "That model's path is too long for the game." );
		return;
	}

	pitch = (int)RPX_CvarFloat( "ui_rpx_p_pitch" );
	roll = (int)RPX_CvarFloat( "ui_rpx_p_roll" );
	yaw = (int)RPX_CvarFloat( "ui_rpx_p_yaw" );
	if ( RPX_CvarInt( "ui_rpx_p_faceme" ) ) {
		yaw = RPX_FacingYaw( yaw );
	}
	Com_sprintf( cmd, sizeof( cmd ), "%s misc_model_breakable model \"%s\" angles \"%d %d %d\"", RPX_AddVerb( grab ), path, pitch, yaw, roll );

	scale = RPX_CvarFloat( "ui_rpx_p_scale" ) / 100.0f;	// a percent
	if ( scale < 0.01f ) scale = 0.01f;
	if ( scale > 10.23f ) scale = 10.23f;
	if ( fabs( scale - 1.0f ) > 0.001f ) {
		Q_strcat( cmd, sizeof( cmd ), va( " modelscale \"%.2f\"", scale ) );
	}

	flags = ( RPX_CvarInt( "ui_rpx_p_solid" ) ? 1 : 0 ) | ( RPX_CvarInt( "ui_rpx_p_anim" ) ? 2 : 0 );
	if ( flags ) {
		Q_strcat( cmd, sizeof( cmd ), va( " spawnflags \"%d\"", flags ) );
	}

	if ( RPX_CvarInt( "ui_rpx_p_break" ) ) {
		int health = RPX_CvarInt( "ui_rpx_p_health" );

		if ( health < 1 ) health = 1;
		if ( health > 100000 ) health = 100000;
		Q_strcat( cmd, sizeof( cmd ), va( " health \"%d\"", health ) );
	}

	if ( RPX_CvarInt( "ui_rpx_p_light" ) ) {
		int radius = RPX_CvarInt( "ui_rpx_p_lrad" );
		// 0 to 255 in the menu, 0 to 1 for the entity
		float r = RPX_CvarFloat( "ui_rpx_p_lr" ) / 255.0f, g = RPX_CvarFloat( "ui_rpx_p_lg" ) / 255.0f, b = RPX_CvarFloat( "ui_rpx_p_lb" ) / 255.0f;

		if ( radius < 1 ) radius = 1;
		if ( radius > 1000 ) radius = 1000;
		r = Com_Clamp( 0.0f, 1.0f, r );
		g = Com_Clamp( 0.0f, 1.0f, g );
		b = Com_Clamp( 0.0f, 1.0f, b );
		Q_strcat( cmd, sizeof( cmd ), va( " light \"%d\" color \"%.2f %.2f %.2f\"", radius, r, g, b ) );
	}

	RPX_AddOffset( cmd, sizeof( cmd ), "ui_rpx_p_lift" );
	RPX_Queue( cmd );
}

static void RPX_SpawnEffect( qboolean grab ) {
	char folder[RPX_NAME_LEN], name[RPX_NAME_LEN], file[MAX_QPATH * 2], cmd[MAX_STRING_CHARS];
	int dir, delay, rnd, flags;

	if ( !RPX_Selected( RPX_EFFECTS, folder, sizeof( folder ), name, sizeof( name ) ) ) {
		return;
	}
	RPX_Path( file, sizeof( file ), "", folder, name, "" );
	if ( strlen( file ) + 12 >= MAX_QPATH ) {	// "effects/" and ".efx" are added to it
		RPX_Refuse( "That effect's path is too long for the game." );
		return;
	}

	dir = RPX_CvarInt( "ui_rpx_e_dir" );
	if ( dir == 1 ) {
		Com_sprintf( cmd, sizeof( cmd ), "%s fx_runner fxFile \"%s\" angles \"0 %d 0\"", RPX_AddVerb( grab ), file, RPX_FacingYaw( 0 ) );
	} else if ( dir == 2 ) {
		int pitch = (int)Com_Clamp( -90.0f, 90.0f, RPX_CvarFloat( "ui_rpx_e_pitch" ) );

		Com_sprintf( cmd, sizeof( cmd ), "%s fx_runner fxFile \"%s\" angles \"%d %d 0\"", RPX_AddVerb( grab ), file, pitch, RPX_FacingYaw( 0 ) );
	} else {
		Com_sprintf( cmd, sizeof( cmd ), "%s fx_runner fxFile \"%s\" angles \"-90 0 0\"", RPX_AddVerb( grab ), file );
	}

	delay = RPX_CvarInt( "ui_rpx_e_delay" );
	if ( delay < 50 ) delay = 50;
	if ( delay > 60000 ) delay = 60000;
	rnd = RPX_CvarInt( "ui_rpx_e_random" );
	if ( rnd < 0 ) rnd = 0;
	if ( rnd > 60000 ) rnd = 60000;
	Q_strcat( cmd, sizeof( cmd ), va( " delay \"%d\" random \"%d\"", delay, rnd ) );

	flags = ( RPX_CvarInt( "ui_rpx_e_off" ) ? 1 : 0 ) | ( RPX_CvarInt( "ui_rpx_e_once" ) ? 2 : 0 );
	if ( flags ) {
		Q_strcat( cmd, sizeof( cmd ), va( " spawnflags \"%d\"", flags ) );
	}

	RPX_AddOffset( cmd, sizeof( cmd ), "ui_rpx_e_lift" );
	RPX_Queue( cmd );
}

static void RPX_SpawnNpc( void ) {
	int kind = RPX_CvarInt( "ui_rpx_npcmode" ) ? RPX_VEHICLES : RPX_NPCS;
	char folder[RPX_NAME_LEN], name[RPX_NAME_LEN], label[64], cmd[MAX_STRING_CHARS];
	int i;

	if ( !RPX_Selected( kind, folder, sizeof( folder ), name, sizeof( name ) ) ) {
		return;
	}
	if ( strchr( name, ' ' ) ) {
		RPX_Refuse( "That type's name has a space in it, so it cannot be spawned." );
		return;
	}

	// the optional name (targetname): letters, digits, _ and -
	RPX_CvarString( "ui_rpx_n_name", label, sizeof( label ) );
	for ( i = 0; label[i]; i++ ) {
		char c = label[i];

		if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '-' ) ) {
			RPX_Refuse( "The name may only have letters, digits, _ and -." );
			return;
		}
	}
	if ( strlen( label ) > 32 ) {
		RPX_Refuse( "The name may be at most 32 characters long." );
		return;
	}

	Com_sprintf( cmd, sizeof( cmd ), "npc spawn %s%s%s%s", kind == RPX_VEHICLES ? "vehicle " : "", name,
		label[0] ? " " : "", label );
	RPX_Queue( cmd );
}

static qboolean RPX_LocalFileExists( const char *path ) {
	fileHandle_t f = 0;
	int len = trap->FS_Open( path, &f, FS_READ );

	if ( f ) {
		trap->FS_Close( f );
	}
	return ( f && len > 0 ) ? qtrue : qfalse;
}

/*
==================
RPX_SpawnLight

GalaxyRP: [Extras] the Lights menu's Spawn: an rp_light (g_misc.c) on the surface aimed at, lifted
ui_rpx_l_lift units out from it (/entaddaim's aimoffset). Its radius and colour on; its radius and colour
off, sent only when it gives light off (offlight 0 is dark, rp_light's own default); START_OFF; and a
name to switch it with. No list, so nothing to check against a listing -- only the name, as the NPCs
menu does. DAJ_RP: its Grab button sends the same with /entaddgrab (RPX_AddVerb()).
==================
*/
static void RPX_SpawnLight( qboolean grab ) {
	char cmd[MAX_STRING_CHARS], label[64];
	const char *problem;
	int radius, offRadius, lift;
	float r, g, b;

	RPX_CvarString( "ui_rpx_l_name", label, sizeof( label ) );
	problem = RPX_LabelProblem( label );
	if ( problem ) {
		RPX_Refuse( problem );
		return;
	}

	radius = RPX_CvarInt( "ui_rpx_l_rad" );
	if ( radius < 1 ) radius = 1;
	if ( radius > 1000 ) radius = 1000;
	r = Com_Clamp( 0.0f, 1.0f, RPX_CvarFloat( "ui_rpx_l_r" ) / 255.0f );
	g = Com_Clamp( 0.0f, 1.0f, RPX_CvarFloat( "ui_rpx_l_g" ) / 255.0f );
	b = Com_Clamp( 0.0f, 1.0f, RPX_CvarFloat( "ui_rpx_l_b" ) / 255.0f );
	Com_sprintf( cmd, sizeof( cmd ), "%s rp_light light \"%d\" color \"%.2f %.2f %.2f\"", RPX_AddVerb( grab ), radius, r, g, b );

	offRadius = RPX_CvarInt( "ui_rpx_l_offrad" );
	if ( offRadius < 0 ) offRadius = 0;
	if ( offRadius > 1000 ) offRadius = 1000;
	if ( offRadius > 0 ) {
		r = Com_Clamp( 0.0f, 1.0f, RPX_CvarFloat( "ui_rpx_l_offr" ) / 255.0f );
		g = Com_Clamp( 0.0f, 1.0f, RPX_CvarFloat( "ui_rpx_l_offg" ) / 255.0f );
		b = Com_Clamp( 0.0f, 1.0f, RPX_CvarFloat( "ui_rpx_l_offb" ) / 255.0f );
		Q_strcat( cmd, sizeof( cmd ), va( " offlight \"%d\" offcolor \"%.2f %.2f %.2f\"", offRadius, r, g, b ) );
	}

	if ( RPX_CvarInt( "ui_rpx_l_off" ) ) {
		Q_strcat( cmd, sizeof( cmd ), " spawnflags \"1\"" );
	}
	if ( label[0] ) {
		Q_strcat( cmd, sizeof( cmd ), va( " targetname \"%s\"", label ) );
	}

	lift = RPX_CvarInt( "ui_rpx_l_lift" );
	if ( lift < 0 ) lift = 0;
	if ( lift > 512 ) lift = 512;
	Q_strcat( cmd, sizeof( cmd ), va( " aimoffset \"%d\"", lift ) );

	RPX_Queue( cmd );
}

static int RPX_ClampInt( int v, int lo, int hi ) {
	return v < lo ? lo : ( v > hi ? hi : v );
}

/*
==================
RPX_SpawnSpawner

GalaxyRP: [Extras] the Spawners menu's Spawn: an npc_spawner (NPCs tab) or NPC_Vehicle (Vehicles tab) of the
selected type, on the surface aimed at (/entaddaim), with the targetname the server requires and the
menu's settings -- each sent only when it changes something. The type is checked against the listing the
server sent and the names as the ui checked them (RPX_SpawnerNamesProblem()), in case they changed since.
DAJ_RP: its Grab button sends the same with /entaddgrab (RPX_AddVerb()).
Shy is an NPC spawner's only: a vehicle spawner spawns when fired, never waiting to be unseen. The
health bar (showhealth) is both kinds'. The effect (npceffect) is an NPC spawner's only: vehicles cannot
have one. The team (npcteam) is both kinds', as /npc team is. The kill rewards (npccredits, npcxp) are an NPC
spawner's only: a vehicle pays nothing, and the server refuses them on one. So is Frozen (npcfreeze): /npc freeze
refuses vehicles, and the server refuses the key on a vehicle spawner. With No AI as well both are sent: the NPC
stays frozen until unfrozen, then stands as a no-AI one.
DAJ_RP: the scale override (npcscale) is both kinds', sent from 1 to 1023 percent: more is sent as 1023, 0 or
less is not sent (its type's own size).
==================
*/
static void RPX_SpawnSpawner( qboolean grab ) {
	int kind = RPX_CvarInt( "ui_rpx_npcmode" ) ? RPX_VEHICLES : RPX_NPCS;
	char folder[RPX_NAME_LEN], type[RPX_NAME_LEN], cmd[MAX_STRING_CHARS];
	char spname[64], npcname[64], ondeath[64];
	const char *problem;
	int yaw, count, delay, flags = 0;

	if ( !RPX_Selected( kind, folder, sizeof( folder ), type, sizeof( type ) ) ) {
		return;
	}
	if ( strchr( type, ' ' ) ) {
		RPX_Refuse( "That type's name has a space in it, so it cannot be spawned." );
		return;
	}

	RPX_CvarString( "ui_rpx_sp_name", spname, sizeof( spname ) );
	RPX_CvarString( "ui_rpx_sp_npcname", npcname, sizeof( npcname ) );
	RPX_CvarString( "ui_rpx_sp_ondeath", ondeath, sizeof( ondeath ) );
	problem = RPX_SpawnerNamesProblem( spname, npcname, ondeath, kind == RPX_NPCS );
	if ( problem ) {
		RPX_Refuse( problem );
		return;
	}

	// towards me, or the way I look
	if ( RPX_CvarInt( "ui_rpx_sp_face" ) ) {
		yaw = (int)( AngleNormalize360( cg.predictedPlayerState.viewangles[YAW] ) + 0.5f ) % 360;
	} else {
		yaw = RPX_FacingYaw( 0 );
	}
	Com_sprintf( cmd, sizeof( cmd ), "%s %s npc_type \"%s\" angles \"0 %d 0\" targetname \"%s\"",
		RPX_AddVerb( grab ), kind == RPX_VEHICLES ? "NPC_Vehicle" : "npc_spawner", type, yaw, spname );

	if ( RPX_CvarInt( "ui_rpx_sp_now" ) ) {
		Q_strcat( cmd, sizeof( cmd ), " spawnnow \"1\"" );
	}
	count = RPX_CvarInt( "ui_rpx_sp_unlim" ) ? -1 : RPX_ClampInt( RPX_CvarInt( "ui_rpx_sp_count" ), 1, 999 );
	if ( count != 1 ) {	// 1 is what a spawner takes no count for
		Q_strcat( cmd, sizeof( cmd ), va( " count \"%d\"", count ) );
	}
	delay = RPX_ClampInt( RPX_CvarInt( "ui_rpx_sp_delay" ), 0, 3600 );
	if ( delay > 0 ) {
		Q_strcat( cmd, sizeof( cmd ), va( " delay \"%d\"", delay ) );
	}
	if ( RPX_CvarInt( "ui_rpx_sp_respawn" ) ) {
		Q_strcat( cmd, sizeof( cmd ), " respawn \"1\"" );
	}
	if ( npcname[0] ) {
		Q_strcat( cmd, sizeof( cmd ), va( " NPC_targetname \"%s\"", npcname ) );
	}
	if ( RPX_CvarInt( "ui_rpx_sp_hbar" ) ) {
		Q_strcat( cmd, sizeof( cmd ), " showhealth \"1\"" );	// both kinds: SP_NPC_spawner(), SP_NPC_Vehicle()
	}
	// the /npc team it is given (npcteam), both kinds: /npc team takes vehicles too. 0 is its type's own.
	switch ( RPX_CvarInt( "ui_rpx_sp_team" ) ) {
	case 1:
		Q_strcat( cmd, sizeof( cmd ), " npcteam \"player\"" );
		break;
	case 2:
		Q_strcat( cmd, sizeof( cmd ), " npcteam \"enemy\"" );
		break;
	case 3:
		Q_strcat( cmd, sizeof( cmd ), " npcteam \"neutral\"" );
		break;
	case 4:
		Q_strcat( cmd, sizeof( cmd ), " npcteam \"free\"" );
		break;
	default:
		break;
	}
	// DAJ_RP: [NPC Scale] the size of each one it makes in percent (npcscale), both kinds; 0 is its type's own, so
	// nothing is sent. More than the server takes (RP_NPC_SCALE_MAX) is brought down to it, as the rewards are.
	{
		const int scale = RPX_ClampInt( RPX_CvarInt( "ui_rpx_sp_scale" ), 0, 1023 );

		if ( scale > 0 ) {
			Q_strcat( cmd, sizeof( cmd ), va( " npcscale \"%d\"", scale ) );
		}
	}

	if ( kind == RPX_NPCS ) {
		int health = RPX_ClampInt( RPX_CvarInt( "ui_rpx_sp_health" ), 0, 100000 );

		if ( health > 0 ) {
			Q_strcat( cmd, sizeof( cmd ), va( " health \"%d\"", health ) );
		}
		// DAJ_RP: [NPC Rewards] what the logged-in player who kills one is paid; the server's limits
		// (RP_REWARD_MAX_CREDITS, RP_REWARD_MAX_XP), and 0 is none, so nothing is sent for it
		{
			const int credits = RPX_ClampInt( RPX_CvarInt( "ui_rpx_sp_credits" ), 0, 100000 );
			const int xp = RPX_ClampInt( RPX_CvarInt( "ui_rpx_sp_xp" ), 0, 100 );

			if ( credits > 0 ) {
				Q_strcat( cmd, sizeof( cmd ), va( " npccredits \"%d\"", credits ) );
			}
			if ( xp > 0 ) {
				Q_strcat( cmd, sizeof( cmd ), va( " npcxp \"%d\"", xp ) );
			}
		}
		if ( ondeath[0] ) {
			Q_strcat( cmd, sizeof( cmd ), va( " NPC_target \"%s\"", ondeath ) );
		}
		if ( RPX_CvarInt( "ui_rpx_sp_shy" ) ) {
			flags |= 2048;	// SHY
		}
		if ( RPX_CvarInt( "ui_rpx_sp_noai" ) ) {
			flags |= 32;	// CINEMATIC
		}
		// DAJ_RP: [NPC System] spawned frozen, as /npc freeze leaves an NPC (SP_NPC_spawner()'s npcfreeze)
		if ( RPX_CvarInt( "ui_rpx_sp_frozen" ) ) {
			Q_strcat( cmd, sizeof( cmd ), " npcfreeze \"1\"" );
		}
		if ( !RPX_CvarInt( "ui_rpx_sp_solid" ) ) {
			flags |= 64;	// NOTSOLID
		}
		// the /npc effect it starts with (SP_NPC_spawner()'s npceffect). Any of them goes with NOTSOLID
		// above: that one wins, the NPC staying untouchable with holo or ghost still showing their look.
		switch ( RPX_CvarInt( "ui_rpx_sp_effect" ) ) {
		case 1:
			Q_strcat( cmd, sizeof( cmd ), " npceffect \"holo\"" );
			break;
		case 2:
			Q_strcat( cmd, sizeof( cmd ), " npceffect \"ghost\"" );
			break;
		case 3:
			Q_strcat( cmd, sizeof( cmd ), " npceffect \"walkthrough\"" );	// DAJ_RP: [NPC System] was "nonsolid"
			break;
		default:
			break;
		}
	} else {
		if ( RPX_CvarInt( "ui_rpx_sp_vdie" ) ) {
			flags |= 1;		// NO_PILOT_DIE: dmg is in milliseconds, speed the distance
			Q_strcat( cmd, sizeof( cmd ), va( " dmg \"%d\" speed \"%d\"",
				RPX_ClampInt( RPX_CvarInt( "ui_rpx_sp_vtime" ), 1, 3600 ) * 1000, RPX_ClampInt( RPX_CvarInt( "ui_rpx_sp_vdist" ), 16, 8192 ) ) );
		}
		if ( RPX_CvarInt( "ui_rpx_sp_vdock" ) ) {
			flags |= 2;		// SUSPENDED
		}
	}
	if ( flags ) {
		Q_strcat( cmd, sizeof( cmd ), va( " spawnflags \"%d\"", flags ) );
	}

	RPX_AddOffset( cmd, sizeof( cmd ), "ui_rpx_sp_lift" );
	RPX_Queue( cmd );
}

static void RPX_Music( qboolean everyone ) {
	char folder[RPX_NAME_LEN], name[RPX_NAME_LEN], path[MAX_QPATH * 2];

	if ( !RPX_Selected( RPX_MUSIC, folder, sizeof( folder ), name, sizeof( name ) ) ) {
		return;
	}
	RPX_Path( path, sizeof( path ), "music/", folder, name, "" );
	if ( strlen( path ) >= MAX_QPATH ) {
		RPX_Refuse( "That track's path is too long for the game." );
		return;
	}

	if ( everyone ) {
		RPX_Queue( va( "playmusic \"%s\"", path ) );
		return;
	}
	if ( !RPX_LocalFileExists( path ) ) {
		RPX_Refuse( "That track is not installed on your game." );
		return;
	}
	trap->S_StartBackgroundTrack( path, path, qfalse );
}

static void RPX_Sound( void ) {
	char folder[RPX_NAME_LEN], name[RPX_NAME_LEN], path[MAX_QPATH * 2];
	int chan = RPX_CvarInt( "ui_rpx_s_chan" );

	if ( !RPX_Selected( RPX_SOUNDS, folder, sizeof( folder ), name, sizeof( name ) ) ) {
		return;
	}
	RPX_Path( path, sizeof( path ), "sound/", folder, name, "" );
	if ( strlen( path ) >= MAX_QPATH ) {
		RPX_Refuse( "That sound's path is too long for the game." );
		return;
	}
	if ( chan < 0 || chan >= RPX_NUM_SOUND_CHANNELS ) {
		chan = 0;
	}
	RPX_Queue( va( "playsound %s \"%s\"", rpxSoundChannels[chan], path ) );
}

static void RPX_FxPreview( void ) {
	char folder[RPX_NAME_LEN], name[RPX_NAME_LEN], file[MAX_QPATH * 2];
	int i, id;

	if ( !RPX_Selected( RPX_EFFECTS, folder, sizeof( folder ), name, sizeof( name ) ) ) {
		return;
	}
	RPX_Path( file, sizeof( file ), "", folder, name, "" );
	if ( strlen( file ) + 12 >= MAX_QPATH ) {
		RPX_Msg( "That effect's path is too long for the game." );
		return;
	}

	for ( i = 0; i < rpx_fx.numPreviewed; i++ ) {
		if ( !Q_stricmp( rpx_fx.previewed[i], file ) ) {
			break;
		}
	}
	if ( i == rpx_fx.numPreviewed ) {
		// a new one: an effect slot until the map changes (a missing file takes none, so check first)
		if ( rpx_fx.numPreviewed >= RPX_PREVIEW_EFFECTS ) {
			RPX_Msg( "^3Preview limit reached until the next map." );
			return;
		}
		if ( !RPX_LocalFileExists( va( "effects/%s.efx", file ) ) ) {
			RPX_Msg( "^3That effect is not installed on your game (players without it will not see it either)." );
			return;
		}
	}

	id = trap->FX_RegisterEffect( file );
	if ( !id ) {
		RPX_Msg( "^3That effect could not be loaded." );
		return;
	}
	if ( i == rpx_fx.numPreviewed ) {
		Q_strncpyz( rpx_fx.previewed[rpx_fx.numPreviewed++], file, sizeof( rpx_fx.previewed[0] ) );
	}

	rpx_fx.pendingId = id;
	rpx_fx.dirMode = RPX_CvarInt( "ui_rpx_e_dir" );
	rpx_fx.pitch = Com_Clamp( -90.0f, 90.0f, RPX_CvarFloat( "ui_rpx_e_pitch" ) );
	rpx_fx.windowEnd = trap->Milliseconds() + RPX_FX_WINDOW;
	RPX_Msg( "" );
}

/*
==================
CG_Rpx_f

"rpx list <kind>", "rpx do <action>" -- from the Extras menus' scripts (ui_main.c).
==================
*/
void CG_Rpx_f( void ) {
	char sub[32], arg[32];

	// copies: CG_Argv() returns one buffer, which the next call overwrites
	Q_strncpyz( sub, CG_Argv( 1 ), sizeof( sub ) );
	Q_strncpyz( arg, CG_Argv( 2 ), sizeof( arg ) );

	if ( !Q_stricmp( sub, "list" ) ) {
		int kind = RPX_KindByName( arg );

		if ( kind >= 0 ) {
			RPX_List( kind );
		}
		return;
	}

	if ( !Q_stricmp( sub, "do" ) ) {
		// DAJ_RP: [Extras] grab*: the Grab buttons under Spawn, the same command with /entaddgrab
		if ( !Q_stricmp( arg, "spawnprop" ) || !Q_stricmp( arg, "grabprop" ) ) {
			RPX_SpawnProp( !Q_stricmp( arg, "grabprop" ) );
		} else if ( !Q_stricmp( arg, "spawnfx" ) || !Q_stricmp( arg, "grabfx" ) ) {
			RPX_SpawnEffect( !Q_stricmp( arg, "grabfx" ) );
		} else if ( !Q_stricmp( arg, "spawnnpc" ) ) {
			RPX_SpawnNpc();
		} else if ( !Q_stricmp( arg, "spawnlight" ) || !Q_stricmp( arg, "grablight" ) ) {
			RPX_SpawnLight( !Q_stricmp( arg, "grablight" ) );
		} else if ( !Q_stricmp( arg, "spawnspawner" ) || !Q_stricmp( arg, "grabspawner" ) ) {
			RPX_SpawnSpawner( !Q_stricmp( arg, "grabspawner" ) );
		} else if ( !Q_stricmp( arg, "musicme" ) ) {
			RPX_Music( qfalse );
		} else if ( !Q_stricmp( arg, "musicall" ) ) {
			RPX_Music( qtrue );
		} else if ( !Q_stricmp( arg, "musicstop" ) ) {
			trap->S_StopBackgroundTrack();
		} else if ( !Q_stricmp( arg, "musicmap" ) ) {
			CG_StartMusic( qtrue );
		} else if ( !Q_stricmp( arg, "soundall" ) ) {
			RPX_Sound();
		} else if ( !Q_stricmp( arg, "fxpreview" ) ) {
			RPX_FxPreview();
		} else if ( !Q_stricmp( arg, "fxstop" ) ) {
			rpx_fx.windowEnd = 0;
			rpx_fx.pendingId = 0;
		} else if ( !Q_stricmp( arg, "undo" ) ) {
			RPX_Queue( "entundo" );		// through the queue, so it keeps to the flood gap too
		} else if ( !Q_stricmp( arg, "stop" ) ) {
			// the menu closed: a listing still on its way stops asking (it would take the player's own
			// commands' turns for as long as it lasts); the next menu asks again from the start
			if ( rpx_fetch.active ) {
				rpx_fetch.active = qfalse;
				RPX_Status( "" );
			}
		}
		return;
	}

	trap->Print( "rpx is used by the Extras menus of the Galaxy RP menu.\n" );
}

/*
===========================================================================
Frame
===========================================================================
*/

static qboolean RPX_FxPassWanted( int now ) {
	char buf[32];
	int seen;

	if ( !rpx_fx.windowEnd || now - rpx_fx.windowEnd > 0 ) {
		rpx_fx.windowEnd = 0;
		return qfalse;
	}
	if ( !( trap->Key_GetCatcher() & KEYCATCH_UI ) ) {
		return qfalse;
	}
	trap->Cvar_VariableStringBuffer( "ui_rpx_fxbox", buf, sizeof( buf ) );
	seen = atoi( buf );
	if ( !seen || now - seen < 0 || now - seen > RPX_FX_HEARTBEAT ) {
		return qfalse;	// the Effects menu is not what is on screen
	}
	return qtrue;
}

/*
==================
CG_RpxFrame

Once a frame, once the snapshot's server commands have run: an action command or list request whose
turn has come, a request that has gone unanswered, and whether the effect preview pass runs this
frame (it has to be known before the sky portal is drawn).
==================
*/
void CG_RpxFrame( void ) {
	int now = trap->Milliseconds();

	if ( rpx_queued && RPX_CanSend( now ) ) {
		RPX_Send( rpx_queue[0], now );
		rpx_queued--;
		memmove( rpx_queue[0], rpx_queue[1], sizeof( rpx_queue[0] ) * rpx_queued );
	}

	if ( rpx_fetch.active ) {
		if ( rpx_fetch.sentAt && now - rpx_fetch.sentAt > RPX_REPLY_TIMEOUT + RPX_FloodGap() ) {
			rpx_fetch.sentAt = 0;
			if ( ++rpx_fetch.tries >= RPX_MAX_TRIES ) {
				RPX_FetchFail( "The server did not answer. It may not have the Extras menus." );
			} else {
				rpx_fetch.needSend = qtrue;
			}
		}
		if ( rpx_fetch.active && rpx_fetch.needSend && now - rpx_fetch.waitUntil >= 0 && !rpx_queued && RPX_CanSend( now ) ) {
			RPX_SendRequest( now );
		}
	}

	rpx_fxPass = RPX_FxPassWanted( now );

	// GalaxyRP: [Extras] the player's view yaw for the Props preview, which shows a prop spawned with
	// Face me off (Yaw a map direction) as it will stand in front of them -- while a menu is open, when
	// it changes by a whole degree; the player cannot turn then, so in practice once a menu
	if ( trap->Key_GetCatcher() & KEYCATCH_UI ) {
		int yaw = (int)( AngleNormalize360( cg.predictedPlayerState.viewangles[YAW] ) + 0.5f ) % 360;

		if ( yaw != rpx_viewYaw ) {
			rpx_viewYaw = yaw;
			trap->Cvar_Set( "ui_rpx_viewyaw", va( "%d", yaw ) );
		}
	}
}

qboolean CG_RpxFxPassActive( void ) {
	return rpx_fxPass;
}

/*
==================
CG_RpxFxPass

The effect preview's pass, at the very end of the frame. See the top of the file for what it cannot
show. The effects system culls against cg.refdef
(it holds a pointer to it) -- both when an effect is played (its cull range) and when it is drawn
(anything behind the viewer) -- so cg.refdef is the preview camera for the length of it, as
CG_DrawSkyBoxPortal() makes it the sky camera for the sky's.
==================
*/
void CG_RpxFxPass( void ) {
	refdef_t saved;
	vec3_t origin, camOrg, camAngles;

	if ( !rpx_fxPass || cg.hyperspace ) {
		return;
	}

	saved = cg.refdef;

	RPX_FxCamera( RPX_CvarFloat( "ui_rpx_e_cdist" ), RPX_CvarFloat( "ui_rpx_e_cang" ), rpx_fx.dirMode, origin, camOrg, camAngles );
	VectorCopy( camOrg, cg.refdef.vieworg );
	VectorCopy( camAngles, cg.refdef.viewangles );
	AnglesToAxis( camAngles, cg.refdef.viewaxis );
	cg.refdef.fov_x = RPX_FX_FOV;
	cg.refdef.fov_y = RPX_FX_FOV * 0.75f;

	if ( rpx_fx.pendingId ) {
		vec3_t fwd;

		RPX_FxDirection( rpx_fx.dirMode, rpx_fx.pitch, origin, camOrg, fwd );
		trap->FX_PlayEffectID( rpx_fx.pendingId, origin, fwd, -1, -1, qtrue );
		rpx_fx.pendingId = 0;
	}

	trap->FX_AddScheduledEffects( qtrue );

	cg.refdef = saved;

	// the clock the ui renders the preview with: the effects' shader times are on cgame's clock
	trap->Cvar_Set( "ui_rpx_fxtime", va( "%d", cg.time ) );
}

/*
==================
CG_RpxInit

At every cgame start (a map, a vid_restart): the menus' cvars back to their defaults, so what a menu
remembers lasts until the map changes, and a new ui_rpx_mapid, which tells the ui its lists and prop
previews are from before.
==================
*/
void CG_RpxInit( void ) {
	int now = trap->Milliseconds();

	memset( &rpx_fetch, 0, sizeof( rpx_fetch ) );
	memset( &rpx_fx, 0, sizeof( rpx_fx ) );
	rpx_numCached = 0;
	rpx_arenaUsed = 0;
	rpx_lastSend = 0;
	rpx_queued = 0;
	rpx_fxPass = qfalse;
	rpx_viewYaw = -1;
	rpx_reqCounter = ( now & 0xffff ) * 1000;
	rpx_serial = ( now & 0xffff ) * 1000;
	rpx_fileIndex = 0;

#define RPX_RESET( name, def ) trap->Cvar_Set( name, def );
	RPX_CVAR_LIST( RPX_RESET )
#undef RPX_RESET

	trap->Cvar_Set( "ui_rpx_serial", va( "%d", rpx_serial ) );
	trap->Cvar_Set( "ui_rpx_mapid", va( "%d", now ? now : 1 ) );
}
