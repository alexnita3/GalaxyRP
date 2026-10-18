/*
===========================================================================
GalaxyRP: [Extras] the ui half of the Extras menus of the Galaxy RP menu: ingame_rpx_props,
ingame_rpx_effects, ingame_rpx_npcs, ingame_rpx_music, ingame_rpx_sounds, ingame_rpx_lights and
ingame_rpx_spawners. cgame's half is cg_rpextras.c; ui/rp_extras.h says how the two work together. Here:

- The list the open menu shows (FEEDER_RPX_LIST): read from the list file cgame writes, when
  ui_rpx_serial changes; filtered by the menu's filter field; folders open with a double click.
  The file is only taken when it is the one for this menu's kind and folder, from this map.
- The menus' scripts (uiScript rpx...): opening a menu, the NPCs/vehicles tabs, the buttons.
  A button that needs a selection only goes ahead with a file selected in the list; the ones that
  spawn or play something close the menus, as the Extras menus do after an action.
- The prop preview (UI_RPX_PREVIEW), drawn here like an ITEM_TYPE_MODEL: the model is only loaded
  when Preview is pressed -- each different model keeps a renderer slot until the map changes, so
  RPX_PREVIEW_MODELS different ones a map at most, and a model the player does not have is not
  asked for at all (a failed load keeps a slot too). The sliders then turn and zoom the loaded
  model at no cost.
- The effect preview box (UI_RPX_FXBOX): renders the scene cgame left the effect in (without
  clearing it first, see CG_RpxFxPass()), and tells cgame it is on screen (ui_rpx_fxbox).
- The light colour swatch of the Props menu (UI_RPX_SWATCH), and the Lights menu's two (UI_RPX_LSWATCH,
  UI_RPX_LOFFSWATCH). The Lights menu has no list: rpxOpen lights opens it without one (RPX_OpenNoList).
- The Spawners menu shows the NPCs & Vehicles menu's lists, with its tabs, filter and selection: rpxOpen
  spawners is rpxOpen npcs.
===========================================================================
*/

#include "ui_local.h"
#include "ui/rp_extras.h"

extern void Text_Paint( float x, float y, float scale, vec4_t color, const char *text, float adjust, int limit, int style, int iMenuFont );
extern int Text_Width( const char *text, float scale, int iMenuFont );
extern void Item_RunScript( itemDef_t *item, const char *s );
extern void Menu_ShowItemByName( menuDef_t *menu, const char *p, qboolean bShow );
extern int Item_ListBox_MaxScroll( itemDef_t *item );
extern void UI_DrawRect( float x, float y, float width, float height, const float *color );

typedef struct {
	char		name[RPX_NAME_LEN];
	qboolean	folder;
} uiRpxEntry_t;

static uiRpxEntry_t	rpxEntries[RPX_MAX_ENTRIES];
static int			rpxNum;
static qboolean		rpxTruncated;
static int			rpxView[RPX_MAX_ENTRIES + 1];	// index into rpxEntries; -1 for ".."
static int			rpxViewNum;
static char			rpxFile[RPX_MAX_ENTRIES * ( RPX_NAME_LEN + 1 ) + 256];

static int			rpxKind = -1;			// the list the open Extras menu shows
static char			rpxMenu[64];			// that menu's name
static qboolean		rpxLoaded;				// rpxEntries is rpxLoadedKind's rpxLoadedFolder, this map
static int			rpxLoadedKind = -1;
static char			rpxLoadedFolder[RPX_NAME_LEN];
static int			rpxSerialSeen;
static int			rpxMapId;
static char			rpxFilterSeen[64];
static int			rpxSyncTime = -1;
static int			rpxPendingNav = -2;		// a row to open at the next sync (-1 is ".."); -2 for none

static struct {
	char		path[MAX_QPATH];
	qhandle_t	model;
	vec3_t		mins, maxs;
	char		previewed[RPX_PREVIEW_MODELS][MAX_QPATH];
	int			numPreviewed;
} rpxProp;

static int			rpxFxWindowEnd;
static int			rpxFxDir;
static int			rpxFxPitch;			// the Tilt the effect showing was played with (for Direction Tilted)
static int			rpxFxChangeAt;		// Direction or Tilt differ from the playing effect's since then; 0 if they do not
static int			rpxFxSeenDir, rpxFxSeenPitch;
static int			rpxFxBoxSet;

/*
===========================================================================
Helpers
===========================================================================
*/

static void RPX_Cvar( const char *name, char *buf, int size ) {
	trap->Cvar_VariableStringBuffer( name, buf, size );
}

static const char *RPX_CvarStr( const char *name ) {
	static char buf[4][256];
	static int n;

	n = ( n + 1 ) & 3;
	trap->Cvar_VariableStringBuffer( name, buf[n], sizeof( buf[n] ) );
	return buf[n];
}

static void RPX_Msg( const char *text ) {
	trap->Cvar_Set( "ui_rpx_msg", text );
}

// the menus' filter field of a kind: the NPC and vehicle tabs share theirs
static const char *RPX_FilterCvar( int kind ) {
	if ( kind == RPX_VEHICLES ) {
		kind = RPX_NPCS;
	}
	return va( "ui_rpx_filter_%s", rpxKinds[kind].name );
}

static void RPX_Folder( int kind, char *folder, int size ) {
	folder[0] = '\0';
	if ( kind >= 0 && rpxKinds[kind].root ) {
		RPX_Cvar( va( "ui_rpx_dir_%s", rpxKinds[kind].name ), folder, size );
	}
}

static qboolean RPX_Contains( const char *hay, const char *needle ) {
	int hl = strlen( hay ), nl = strlen( needle ), i;

	if ( !nl ) {
		return qtrue;
	}
	for ( i = 0; i + nl <= hl; i++ ) {
		if ( !Q_stricmpn( hay + i, needle, nl ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

// the list of the open Extras menu
static itemDef_t *RPX_ListItem( void ) {
	menuDef_t *menu = rpxMenu[0] ? Menus_FindByName( rpxMenu ) : NULL;
	int i;

	if ( !menu ) {
		return NULL;
	}
	for ( i = 0; i < menu->itemCount; i++ ) {
		if ( menu->items[i]->type == ITEM_TYPE_LISTBOX && menu->items[i]->special == FEEDER_RPX_LIST ) {
			return menu->items[i];
		}
	}
	return NULL;
}

// the selected row, as the list shows it
static int RPX_SelectedRow( void ) {
	itemDef_t *item = RPX_ListItem();

	if ( !item || item->cursorPos < 0 || item->cursorPos >= rpxViewNum ) {
		return -3;
	}
	return rpxView[item->cursorPos];
}

static void RPX_SetListCursor( int index ) {
	itemDef_t *item = RPX_ListItem();
	listBoxDef_t *listPtr;

	if ( !item || !item->typeData.listbox ) {
		return;
	}
	listPtr = item->typeData.listbox;
	item->cursorPos = index;
	listPtr->cursorPos = ( index >= 0 ) ? index : 0;
	if ( index < 0 ) {
		listPtr->startPos = 0;
	} else if ( index < listPtr->startPos || listPtr->endPos <= 0 || index > listPtr->endPos ) {
		int max = Item_ListBox_MaxScroll( item );

		listPtr->startPos = ( index > 3 ) ? index - 3 : 0;
		if ( listPtr->startPos > max ) {
			listPtr->startPos = max;
		}
	}
}

// the selected name in full under the list: the list draws a name only as wide as its column
static void RPX_ShowSelected( const char *name ) {
	trap->Cvar_Set( "ui_rpx_selname", name[0] ? va( "Selected: ^7%s", name ) : "" );
}

static void RPX_UpdateInfo( void ) {
	char folder[RPX_NAME_LEN];
	const char *where;
	int files = 0, i;

	if ( rpxKind < 0 ) {
		return;
	}
	for ( i = 0; i < rpxNum; i++ ) {
		if ( !rpxEntries[i].folder ) {
			files++;
		}
	}
	RPX_Folder( rpxKind, folder, sizeof( folder ) );
	if ( !rpxKinds[rpxKind].root ) {
		where = ( rpxKind == RPX_VEHICLES ) ? "Vehicle types" : "NPC types";
	} else {
		where = va( "%s%s%s", rpxKinds[rpxKind].root, folder[0] ? "/" : "", folder );
	}

	if ( !rpxLoaded ) {
		trap->Cvar_Set( "ui_rpx_info", where );
	} else if ( rpxFilterSeen[0] ) {
		trap->Cvar_Set( "ui_rpx_info", va( "%s: %d of %d shown", where, rpxViewNum - ( ( rpxViewNum && rpxView[0] == -1 ) ? 1 : 0 ), rpxNum ) );
	} else {
		trap->Cvar_Set( "ui_rpx_info", va( "%s: %d %s", where, files, files == 1 ? "file" : "files" ) );
	}
}

// the rows the list shows: ".." first in a subfolder, then what the filter lets through; the
// selection kept on the selected name
static void RPX_BuildView( void ) {
	char folder[RPX_NAME_LEN], sel[RPX_NAME_LEN];
	int i, cursor = -1;

	rpxViewNum = 0;
	if ( rpxKind < 0 ) {
		return;
	}
	RPX_Folder( rpxKind, folder, sizeof( folder ) );
	if ( rpxLoaded && rpxKinds[rpxKind].root && folder[0] ) {
		rpxView[rpxViewNum++] = -1;
	}
	if ( rpxLoaded ) {
		for ( i = 0; i < rpxNum; i++ ) {
			if ( RPX_Contains( rpxEntries[i].name, rpxFilterSeen ) ) {
				rpxView[rpxViewNum++] = i;
			}
		}
	}

	RPX_Cvar( va( "ui_rpx_sel_%s", rpxKinds[rpxKind].name ), sel, sizeof( sel ) );
	if ( sel[0] ) {
		for ( i = 0; i < rpxViewNum; i++ ) {
			if ( rpxView[i] >= 0 && !rpxEntries[rpxView[i]].folder && !strcmp( rpxEntries[rpxView[i]].name, sel ) ) {
				cursor = i;
				break;
			}
		}
	}
	RPX_SetListCursor( cursor );
	RPX_ShowSelected( cursor >= 0 ? sel : "" );
	RPX_UpdateInfo();
}

// the list file, when it is the one for this menu (its kind and folder, and the serial cgame wrote)
static void RPX_LoadFile( int serial ) {
	char file[64], folder[RPX_NAME_LEN];
	fileHandle_t f = 0;
	char *p, *line, *fields[6];
	int len, i, nf;

	RPX_Cvar( "ui_rpx_file", file, sizeof( file ) );
	for ( i = 0; i < RPX_NUM_LIST_FILES; i++ ) {
		if ( !strcmp( file, rpxListFiles[i] ) ) {
			break;
		}
	}
	if ( i == RPX_NUM_LIST_FILES || rpxKind < 0 ) {
		return;
	}

	len = trap->FS_Open( file, &f, FS_READ );
	if ( !f ) {
		return;
	}
	if ( len <= 0 || len >= (int)sizeof( rpxFile ) ) {
		trap->FS_Close( f );
		return;
	}
	trap->FS_Read( rpxFile, len, f );
	trap->FS_Close( f );
	rpxFile[len] = '\0';

	// the header: magic, serial, kind, folder, count, cut short
	line = rpxFile;
	p = strchr( line, '\n' );
	if ( !p ) {
		return;
	}
	*p++ = '\0';
	nf = 0;
	fields[nf++] = line;
	while ( nf < 6 && ( line = strchr( line, '\t' ) ) ) {
		*line++ = '\0';
		fields[nf++] = line;
	}
	if ( nf != 6 || strcmp( fields[0], RPX_LIST_MAGIC ) || atoi( fields[1] ) != serial ) {
		return;
	}

	RPX_Folder( rpxKind, folder, sizeof( folder ) );
	if ( Q_stricmp( fields[2], rpxKinds[rpxKind].name ) || Q_stricmp( fields[3], folder ) ) {
		return;	// not what this menu shows now: the file for a list it has left
	}

	rpxNum = 0;
	rpxTruncated = atoi( fields[5] ) ? qtrue : qfalse;
	while ( *p ) {
		char *end = strchr( p, '\n' );

		if ( end ) {
			*end = '\0';
		}
		if ( rpxNum < RPX_MAX_ENTRIES && RPX_NameOk( p, rpxKinds[rpxKind].root ? qtrue : qfalse ) ) {
			int l = strlen( p );

			rpxEntries[rpxNum].folder = ( p[l - 1] == '/' ) ? qtrue : qfalse;
			Q_strncpyz( rpxEntries[rpxNum].name, p, sizeof( rpxEntries[0].name ) );
			rpxNum++;
		}
		if ( !end ) {
			break;
		}
		p = end + 1;
	}

	rpxLoaded = qtrue;
	rpxLoadedKind = rpxKind;
	Q_strncpyz( rpxLoadedFolder, folder, sizeof( rpxLoadedFolder ) );
	RPX_BuildView();
}

static void RPX_Request( void ) {
	rpxLoaded = qfalse;
	rpxNum = 0;
	RPX_BuildView();
	trap->Cmd_ExecuteText( EXEC_APPEND, va( "rpx list %s\n", rpxKinds[rpxKind].name ) );
}

static void RPX_StopFxPreview( void ) {
	rpxFxChangeAt = 0;
	if ( rpxFxWindowEnd ) {
		rpxFxWindowEnd = 0;
		trap->Cmd_ExecuteText( EXEC_APPEND, "rpx do fxstop\n" );
	}
}

// open a row: a folder, or ".." (-1)
static void RPX_Navigate( int row ) {
	char folder[RPX_NAME_LEN], next[RPX_NAME_LEN * 2];

	if ( rpxKind < 0 || !rpxKinds[rpxKind].root ) {
		return;
	}
	RPX_Folder( rpxKind, folder, sizeof( folder ) );

	if ( row == -1 ) {
		char *slash = strrchr( folder, '/' );

		if ( slash ) {
			*slash = '\0';
		} else {
			folder[0] = '\0';
		}
		Q_strncpyz( next, folder, sizeof( next ) );
	} else {
		char name[RPX_NAME_LEN];
		int l;

		if ( row < 0 || row >= rpxNum || !rpxEntries[row].folder ) {
			return;
		}
		Q_strncpyz( name, rpxEntries[row].name, sizeof( name ) );
		l = strlen( name );
		name[l - 1] = '\0';	// its "/"
		Com_sprintf( next, sizeof( next ), "%s%s%s", folder, folder[0] ? "/" : "", name );
	}

	if ( !RPX_FolderOk( next ) ) {
		RPX_Msg( "^3That folder's path is too long to open." );
		return;
	}

	trap->Cvar_Set( va( "ui_rpx_dir_%s", rpxKinds[rpxKind].name ), next );
	trap->Cvar_Set( va( "ui_rpx_sel_%s", rpxKinds[rpxKind].name ), "" );
	RPX_Msg( "" );
	rpxProp.model = 0;
	RPX_StopFxPreview();
	RPX_Request();
}

// the map changed (cgame started again): nothing from before is ours any more
static void RPX_CheckMap( void ) {
	int mapId = atoi( RPX_CvarStr( "ui_rpx_mapid" ) );

	if ( mapId == rpxMapId ) {
		return;
	}
	rpxMapId = mapId;
	rpxLoaded = qfalse;
	rpxNum = 0;
	rpxViewNum = 0;
	rpxSerialSeen = -1;
	rpxProp.model = 0;
	rpxProp.numPreviewed = 0;
	rpxFxWindowEnd = 0;
}

// once a frame, from whatever asks first: a folder to open, a new list file, a new filter
static void RPX_Sync( void ) {
	char filter[64];
	int serial;

	if ( rpxSyncTime == uiInfo.uiDC.realTime ) {
		return;
	}
	rpxSyncTime = uiInfo.uiDC.realTime;
	if ( rpxKind < 0 ) {
		return;
	}

	RPX_CheckMap();

	if ( rpxPendingNav != -2 ) {
		int row = rpxPendingNav;

		rpxPendingNav = -2;
		RPX_Navigate( row );
	}

	serial = atoi( RPX_CvarStr( "ui_rpx_serial" ) );
	if ( serial != rpxSerialSeen ) {
		rpxSerialSeen = serial;
		RPX_LoadFile( serial );
	}

	RPX_Cvar( RPX_FilterCvar( rpxKind ), filter, sizeof( filter ) );
	if ( strcmp( filter, rpxFilterSeen ) ) {
		Q_strncpyz( rpxFilterSeen, filter, sizeof( rpxFilterSeen ) );
		RPX_BuildView();
	}
}

/*
===========================================================================
The list
===========================================================================
*/

int UI_RpxFeederCount( void ) {
	RPX_Sync();
	return rpxViewNum;
}

const char *UI_RpxFeederItemText( int index ) {
	int row;

	if ( index < 0 || index >= rpxViewNum ) {
		return "";
	}
	row = rpxView[index];
	if ( row == -1 ) {
		return "^5..  (up one folder)";
	}
	if ( rpxEntries[row].folder ) {
		return va( "^5%s", rpxEntries[row].name );
	}
	return rpxEntries[row].name;
}

// a row selected: a file becomes the selection; the previews show the old one no longer
qboolean UI_RpxFeederSelection( int index ) {
	const char *name = "";
	int row;

	if ( rpxKind < 0 || index < 0 || index >= rpxViewNum ) {
		return qtrue;
	}
	row = rpxView[index];
	if ( row >= 0 && !rpxEntries[row].folder ) {
		name = rpxEntries[row].name;
	}
	RPX_ShowSelected( name );
	if ( strcmp( name, RPX_CvarStr( va( "ui_rpx_sel_%s", rpxKinds[rpxKind].name ) ) ) ) {
		trap->Cvar_Set( va( "ui_rpx_sel_%s", rpxKinds[rpxKind].name ), name );
		RPX_Msg( "" );
		if ( rpxKind == RPX_MODELS ) {
			rpxProp.model = 0;
		} else if ( rpxKind == RPX_EFFECTS ) {
			RPX_StopFxPreview();
		}
	}
	return qtrue;
}

/*
===========================================================================
Scripts
===========================================================================
*/

static int RPX_KindForMenu( const char *word ) {
	if ( !Q_stricmp( word, "props" ) ) return RPX_MODELS;
	if ( !Q_stricmp( word, "effects" ) ) return RPX_EFFECTS;
	// GalaxyRP: [Extras] the Spawners menu shows the same lists, with the same tab and selection
	if ( !Q_stricmp( word, "npcs" ) || !Q_stricmp( word, "spawners" ) ) return atoi( RPX_CvarStr( "ui_rpx_npcmode" ) ) ? RPX_VEHICLES : RPX_NPCS;
	if ( !Q_stricmp( word, "music" ) ) return RPX_MUSIC;
	if ( !Q_stricmp( word, "sounds" ) ) return RPX_SOUNDS;
	return -1;
}

// GalaxyRP: [Extras] a menu with no list (Lights): nothing to ask the server for; no list kind is open
static void RPX_OpenNoList( const char *menu ) {
	rpxKind = -1;
	rpxViewNum = 0;
	Q_strncpyz( rpxMenu, menu, sizeof( rpxMenu ) );
	rpxSyncTime = -1;
	RPX_CheckMap();
	RPX_Msg( "" );
	RPX_StopFxPreview();
}

static void RPX_Open( int kind, const char *menu ) {
	char folder[RPX_NAME_LEN];

	rpxKind = kind;
	Q_strncpyz( rpxMenu, menu, sizeof( rpxMenu ) );
	rpxSyncTime = -1;
	RPX_CheckMap();
	RPX_Cvar( RPX_FilterCvar( kind ), rpxFilterSeen, sizeof( rpxFilterSeen ) );
	RPX_Msg( "" );
	RPX_StopFxPreview();

	RPX_Folder( kind, folder, sizeof( folder ) );
	if ( rpxLoaded && rpxLoadedKind == kind && !Q_stricmp( rpxLoadedFolder, folder ) ) {
		// shown as it was, and asked for again all the same: cgame answers at once from what it keeps,
		// and a listing that failed last time (the server busy, or no answer) is tried again
		RPX_BuildView();
		trap->Cmd_ExecuteText( EXEC_APPEND, va( "rpx list %s\n", rpxKinds[kind].name ) );
		return;
	}
	RPX_Request();
}

// the selected row is a file of the list (what cgame will check again against the server's list)
static qboolean RPX_HaveSelection( void ) {
	int row = RPX_SelectedRow();

	if ( row < 0 || row >= rpxNum || rpxEntries[row].folder ) {
		RPX_Msg( "^3Select one from the list first." );
		return qfalse;
	}
	return qtrue;
}

static void RPX_CloseMenus( void ) {
	trap->Key_SetCatcher( trap->Key_GetCatcher() & ~KEYCATCH_UI );
	trap->Key_ClearStates();
	trap->Cvar_Set( "cl_paused", "0" );
	Menus_CloseAll();
}

static qboolean RPX_LocalFileExists( const char *path ) {
	fileHandle_t f = 0;
	int len = trap->FS_Open( path, &f, FS_READ );

	if ( f ) {
		trap->FS_Close( f );
	}
	return ( f && len > 0 ) ? qtrue : qfalse;
}

static void RPX_PreviewProp( void ) {
	char folder[RPX_NAME_LEN], path[MAX_QPATH * 2];
	int row = RPX_SelectedRow(), i;
	qhandle_t model;

	if ( rpxKind != RPX_MODELS || !RPX_HaveSelection() ) {
		return;
	}
	RPX_Folder( RPX_MODELS, folder, sizeof( folder ) );
	Com_sprintf( path, sizeof( path ), "models/%s%s%s.md3", folder, folder[0] ? "/" : "", rpxEntries[row].name );
	if ( strlen( path ) >= MAX_QPATH ) {
		RPX_Msg( "^3That model's path is too long for the game." );
		return;
	}

	for ( i = 0; i < rpxProp.numPreviewed; i++ ) {
		if ( !Q_stricmp( rpxProp.previewed[i], path ) ) {
			break;
		}
	}
	if ( i == rpxProp.numPreviewed ) {
		if ( rpxProp.numPreviewed >= RPX_PREVIEW_MODELS ) {
			RPX_Msg( "^3Preview limit reached until the next map." );
			return;
		}
		if ( !RPX_LocalFileExists( path ) ) {
			rpxProp.model = 0;
			RPX_Msg( "^3That model is not installed on your game (players without it will not see it either)." );
			return;
		}
	}

	model = trap->R_RegisterModel( path );
	if ( i == rpxProp.numPreviewed ) {
		Q_strncpyz( rpxProp.previewed[rpxProp.numPreviewed++], path, sizeof( rpxProp.previewed[0] ) );
	}
	if ( !model ) {
		rpxProp.model = 0;
		RPX_Msg( "^3That model could not be loaded." );
		return;
	}
	Q_strncpyz( rpxProp.path, path, sizeof( rpxProp.path ) );
	rpxProp.model = model;
	trap->R_ModelBounds( model, rpxProp.mins, rpxProp.maxs );
	RPX_Msg( "" );
}

// the Tilt as cgame plays it (RPX_FxPreview: clamped to -90..90), in whole degrees
static int RPX_FxTilt( void ) {
	return (int)Com_Clamp( -90.0f, 90.0f, (float)(int)atof( RPX_CvarStr( "ui_rpx_e_pitch" ) ) );
}

static void RPX_PreviewEffect( void ) {
	if ( rpxKind != RPX_EFFECTS || !RPX_HaveSelection() ) {
		return;
	}
	rpxFxDir = atoi( RPX_CvarStr( "ui_rpx_e_dir" ) );
	rpxFxPitch = RPX_FxTilt();
	rpxFxChangeAt = 0;
	rpxFxWindowEnd = trap->Milliseconds() + RPX_FX_WINDOW;
	trap->Cmd_ExecuteText( EXEC_APPEND, "rpx do fxpreview\n" );
}

// the Galaxy RP menu opening (its onOpen): opened from an Extras menu's Back button, its Extras tab,
// as the Extras button leaves it (the same lines as that button's action)
static void RPX_BackTab( void ) {
	menuDef_t *menu = Menus_FindByName( "ingame_galaxyrp" );

	// the Galaxy RP menu sends "zykmod" as it opens: cgame's own commands keep the flood gap from it
	trap->Cvar_Set( "ui_rpx_sent", va( "%d", trap->Milliseconds() ) );

	if ( !menu || menu->itemCount <= 0 || atoi( RPX_CvarStr( "ui_rpx_back" ) ) != 1 ) {
		return;
	}
	trap->Cvar_Set( "ui_rpx_back", "0" );
	Menu_ShowItemByName( menu, "listControls", qfalse );
	Menu_ShowItemByName( menu, "extrasControls", qtrue );
	Item_RunScript( menu->items[0], "setitemcolor mainButtons forecolor 1 .682 0 1 ; setitemcolor extrasButton forecolor 1 1 1 1 ; setitemcolor extrasControls forecolor 1 .682 0 1" );
}

/*
==================
UI_RpxScript

The Extras menus' uiScripts; qfalse when name is not one of them.

	rpxOpen <props|effects|npcs|music|sounds|lights|spawners> <menu>	in each menu's onOpen
	rpxNpcMode <0|1>									the NPCs and Vehicles tabs (NPCs & Vehicles, Spawners)
	rpxEnter											the list's double click: open a folder
	rpxPreview											Props and Effects
	rpxDo <action>										a button: see CG_Rpx_f() in cg_rpextras.c
	rpxReset <prop|angles|cam>							the preview's sliders back, or the prop's angles
	rpxLightSame										Lights: the colour when off is the colour when on
	rpxClose											in each menu's onClose
	rpxBackTab											in the Galaxy RP menu's onOpen
==================
*/
qboolean UI_RpxScript( const char *name, char **args ) {
	const char *arg = NULL, *arg2 = NULL;

	if ( Q_stricmpn( name, "rpx", 3 ) ) {
		return qfalse;
	}

	if ( !Q_stricmp( name, "rpxOpen" ) ) {
		if ( String_Parse( args, &arg ) && String_Parse( args, &arg2 ) ) {
			int kind = RPX_KindForMenu( arg );

			if ( kind >= 0 ) {
				RPX_Open( kind, arg2 );
			} else if ( !Q_stricmp( arg, "lights" ) ) {
				RPX_OpenNoList( arg2 );
			}
		}
	} else if ( !Q_stricmp( name, "rpxNpcMode" ) ) {
		if ( String_Parse( args, &arg ) && ( rpxKind == RPX_NPCS || rpxKind == RPX_VEHICLES ) ) {
			int kind = atoi( arg ) ? RPX_VEHICLES : RPX_NPCS;

			trap->Cvar_Set( "ui_rpx_npcmode", ( kind == RPX_VEHICLES ) ? "1" : "0" );
			if ( kind != rpxKind ) {
				RPX_Open( kind, rpxMenu );
			}
		}
	} else if ( !Q_stricmp( name, "rpxEnter" ) ) {
		itemDef_t *item = RPX_ListItem();
		int row = RPX_SelectedRow();

		// the list runs this before it selects the row just clicked (Item_ListBox_HandleKey()), so it is a
		// double click on one row only when the row under the mouse is the one already selected -- not a
		// click on one row and then quickly on another. And after it the list goes on to select the row
		// under the mouse in the list as it is now, so the folder is opened at the next frame.
		if ( !item || !item->typeData.listbox || item->typeData.listbox->cursorPos != item->cursorPos ) {
			return qtrue;
		}
		if ( row == -1 || ( row >= 0 && row < rpxNum && rpxEntries[row].folder ) ) {
			rpxPendingNav = row;
		}
	} else if ( !Q_stricmp( name, "rpxPreview" ) ) {
		if ( rpxKind == RPX_MODELS ) {
			RPX_PreviewProp();
		} else if ( rpxKind == RPX_EFFECTS ) {
			RPX_PreviewEffect();
		}
	} else if ( !Q_stricmp( name, "rpxDo" ) ) {
		if ( String_Parse( args, &arg ) ) {
			// DAJ_RP: [Extras] grabprop and grabfx: the Grab buttons under Spawn, as Spawn
			static const char *closing[] = { "spawnprop", "spawnfx", "grabprop", "grabfx", "musicme", "musicall", "soundall" };
			int i;

			if ( !Q_stricmp( arg, "musicstop" ) || !Q_stricmp( arg, "musicmap" ) || !Q_stricmp( arg, "undo" ) ) {
				trap->Cmd_ExecuteText( EXEC_APPEND, va( "rpx do %s\n", arg ) );
				return qtrue;
			}
			// GalaxyRP: [Extras] the Lights menu's Spawn: nothing to select, only its name to check first,
			// so a name it cannot use is said in the menu, which stays open
			// DAJ_RP: [Extras] and its Grab (grablight), the same
			if ( !Q_stricmp( arg, "spawnlight" ) || !Q_stricmp( arg, "grablight" ) ) {
				const char *problem = RPX_LabelProblem( RPX_CvarStr( "ui_rpx_l_name" ) );

				if ( problem ) {
					RPX_Msg( va( "^3%s", problem ) );
					return qtrue;
				}
				trap->Cmd_ExecuteText( EXEC_APPEND, !Q_stricmp( arg, "grablight" ) ? "rpx do grablight\n" : "rpx do spawnlight\n" );
				RPX_CloseMenus();
				return qtrue;
			}
			// GalaxyRP: [Extras] the NPCs & Vehicles menu's Spawn: a selection, and its optional name as the
			// Lights menu checks one -- a problem is said in the menu, which stays open (cgame checks it again)
			if ( !Q_stricmp( arg, "spawnnpc" ) ) {
				const char *problem;

				if ( !RPX_HaveSelection() ) {
					return qtrue;
				}
				problem = RPX_LabelProblem( RPX_CvarStr( "ui_rpx_n_name" ) );
				if ( problem ) {
					RPX_Msg( va( "^3%s", problem ) );
					return qtrue;
				}
				trap->Cmd_ExecuteText( EXEC_APPEND, "rpx do spawnnpc\n" );
				RPX_CloseMenus();
				return qtrue;
			}
			// GalaxyRP: [Extras] the Spawners menu's Spawn: a selection, and the names it needs and may have
			// (RPX_SpawnerNamesProblem()) -- a problem is said in the menu, which stays open
			// DAJ_RP: [Extras] and its Grab (grabspawner), the same
			if ( !Q_stricmp( arg, "spawnspawner" ) || !Q_stricmp( arg, "grabspawner" ) ) {
				const char *problem;

				if ( !RPX_HaveSelection() ) {
					return qtrue;
				}
				problem = RPX_SpawnerNamesProblem( RPX_CvarStr( "ui_rpx_sp_name" ), RPX_CvarStr( "ui_rpx_sp_npcname" ),
					RPX_CvarStr( "ui_rpx_sp_ondeath" ), rpxKind != RPX_VEHICLES );
				if ( problem ) {
					RPX_Msg( va( "^3%s", problem ) );
					return qtrue;
				}
				trap->Cmd_ExecuteText( EXEC_APPEND, !Q_stricmp( arg, "grabspawner" ) ? "rpx do grabspawner\n" : "rpx do spawnspawner\n" );
				RPX_CloseMenus();
				return qtrue;
			}
			for ( i = 0; i < (int)ARRAY_LEN( closing ); i++ ) {
				if ( !Q_stricmp( arg, closing[i] ) ) {
					if ( RPX_HaveSelection() ) {
						trap->Cmd_ExecuteText( EXEC_APPEND, va( "rpx do %s\n", closing[i] ) );
						RPX_StopFxPreview();
						RPX_CloseMenus();
					}
					break;
				}
			}
		}
	} else if ( !Q_stricmp( name, "rpxReset" ) ) {
		if ( String_Parse( args, &arg ) ) {
			// GalaxyRP: [Extras] "prop" is the Props preview's Reset view: only what the preview alone
			// uses. The prop's own angles, which it is spawned with, have their own button ("angles").
			if ( !Q_stricmp( arg, "prop" ) ) {
				trap->Cvar_Set( "ui_rpx_p_zoom", "100" );
			} else if ( !Q_stricmp( arg, "angles" ) ) {
				trap->Cvar_Set( "ui_rpx_p_yaw", "0" );
				trap->Cvar_Set( "ui_rpx_p_pitch", "0" );
				trap->Cvar_Set( "ui_rpx_p_roll", "0" );
			} else if ( !Q_stricmp( arg, "cam" ) ) {
				trap->Cvar_Set( "ui_rpx_e_cdist", "160" );
				trap->Cvar_Set( "ui_rpx_e_cang", "0" );
			}
		}
	} else if ( !Q_stricmp( name, "rpxLightSame" ) ) {
		// GalaxyRP: [Extras] the Lights menu's "Same as on": its colour when off is the colour when on
		trap->Cvar_Set( "ui_rpx_l_offr", RPX_CvarStr( "ui_rpx_l_r" ) );
		trap->Cvar_Set( "ui_rpx_l_offg", RPX_CvarStr( "ui_rpx_l_g" ) );
		trap->Cvar_Set( "ui_rpx_l_offb", RPX_CvarStr( "ui_rpx_l_b" ) );
	} else if ( !Q_stricmp( name, "rpxClose" ) ) {
		RPX_StopFxPreview();
		trap->Cmd_ExecuteText( EXEC_APPEND, "rpx do stop\n" );
	} else if ( !Q_stricmp( name, "rpxBackTab" ) ) {
		RPX_BackTab();
	} else {
		return qfalse;
	}
	return qtrue;
}

/*
===========================================================================
Owner draws
===========================================================================
*/

static void RPX_CenterText( rectDef_t *rect, float scale, vec4_t color, const char *text, int iMenuFont ) {
	int w = Text_Width( text, scale, iMenuFont );

	Text_Paint( rect->x + ( rect->w - w ) / 2, rect->y + rect->h / 2 - 8, scale, color, text, 0, 0, ITEM_TEXTSTYLE_NORMAL, iMenuFont );
}

static void RPX_SceneRect( rectDef_t *rect, refdef_t *refdef, float fovX ) {
	float x = rect->x + 1, y = rect->y + 1, w = rect->w - 2, h = rect->h - 2;

	memset( refdef, 0, sizeof( *refdef ) );
	refdef->rdflags = RDF_NOWORLDMODEL;
	AxisClear( refdef->viewaxis );
	refdef->x = x * uiInfo.uiDC.xscale;
	refdef->y = y * uiInfo.uiDC.yscale;
	refdef->width = w * uiInfo.uiDC.xscale;
	refdef->height = h * uiInfo.uiDC.yscale;
	refdef->fov_x = fovX;
	refdef->fov_y = atan2( refdef->height, refdef->width / tan( fovX / 360 * M_PI ) ) * ( 360 / M_PI );
	refdef->time = uiInfo.uiDC.realTime;
}

// the yaw the Props preview turns the model by, in the camera's frame (the camera looks along +x, as
// the player looks at what they spawn): the spawned prop's yaw less the player's view yaw
static float RPX_PropPreviewYaw( qboolean faceMe, int yaw, int viewYaw ) {
	if ( faceMe ) {
		return 180.0f + yaw;
	}
	return (float)( yaw - viewYaw );
}

static void RPX_DrawPropPreview( rectDef_t *rect, float scale, vec4_t color, int iMenuFont ) {
	static vec4_t back = { 0.02f, 0.03f, 0.06f, 1.0f };
	refdef_t refdef;
	refEntity_t ent;
	vec3_t center, size, angles, rc;
	float radius, dist, zoom, propScale, fov;
	char text[128];
	int k;

	RPX_CheckMap();
	UI_FillRect( rect->x, rect->y, rect->w, rect->h, back );

	if ( !rpxProp.model ) {
		RPX_CenterText( rect, scale, color, "Press Preview to see it", iMenuFont );
		return;
	}

	fov = 40.0f;
	RPX_SceneRect( rect, &refdef, fov );
	if ( refdef.fov_y < fov ) {
		fov = refdef.fov_y;
	}

	VectorAdd( rpxProp.mins, rpxProp.maxs, center );
	VectorScale( center, 0.5f, center );
	VectorSubtract( rpxProp.maxs, rpxProp.mins, size );
	radius = 0.5f * VectorLength( size );
	if ( radius < 1.0f ) {
		radius = 1.0f;
	}
	zoom = atof( RPX_CvarStr( "ui_rpx_p_zoom" ) );
	if ( zoom < 25.0f ) zoom = 25.0f;
	if ( zoom > 400.0f ) zoom = 400.0f;
	dist = radius / sin( DEG2RAD( fov * 0.5f ) ) * ( 100.0f / zoom );

	// GalaxyRP: [Extras] the prop as it will stand in front of the player, the camera standing for
	// them: the spawn's yaw (cg_rpextras.c, RPX_SpawnProp) less their view yaw. With Face me that is
	// 180 + Yaw whatever way they look (its front towards them, then Yaw); without it, Yaw is a map
	// direction, so how it turns to them depends on which way they look (ui_rpx_viewyaw, from cgame).
	angles[PITCH] = (int)atof( RPX_CvarStr( "ui_rpx_p_pitch" ) );
	angles[YAW] = RPX_PropPreviewYaw( atoi( RPX_CvarStr( "ui_rpx_p_faceme" ) ) ? qtrue : qfalse,
		(int)atof( RPX_CvarStr( "ui_rpx_p_yaw" ) ), atoi( RPX_CvarStr( "ui_rpx_viewyaw" ) ) );
	angles[ROLL] = (int)atof( RPX_CvarStr( "ui_rpx_p_roll" ) );

	memset( &ent, 0, sizeof( ent ) );
	AnglesToAxis( angles, ent.axis );
	// the middle of the model's box, turned with it, at the point the camera looks at
	for ( k = 0; k < 3; k++ ) {
		rc[k] = center[0] * ent.axis[0][k] + center[1] * ent.axis[1][k] + center[2] * ent.axis[2][k];
	}
	VectorSet( ent.origin, dist, 0, 0 );
	VectorSubtract( ent.origin, rc, ent.origin );
	VectorCopy( ent.origin, ent.oldorigin );
	VectorCopy( ent.origin, ent.lightingOrigin );
	ent.hModel = rpxProp.model;
	ent.renderfx = RF_LIGHTING_ORIGIN | RF_NOSHADOW;

	propScale = atof( RPX_CvarStr( "ui_rpx_p_scale" ) ) / 100.0f;	// a percent
	if ( propScale < 0.01f ) propScale = 0.01f;
	if ( propScale > 10.23f ) propScale = 10.23f;

	trap->R_ClearScene();
	trap->R_AddRefEntityToScene( &ent );
	if ( atoi( RPX_CvarStr( "ui_rpx_p_light" ) ) ) {
		// the prop's light, near the side of the model facing the camera, at the size it has next to
		// the model at this scale (kept big enough to be seen)
		vec3_t lightOrg;
		float intensity = atof( RPX_CvarStr( "ui_rpx_p_lrad" ) ) / propScale;

		if ( intensity < radius * 0.5f ) intensity = radius * 0.5f;
		if ( intensity > radius * 6.0f ) intensity = radius * 6.0f;
		VectorSet( lightOrg, dist - radius * 1.2f, radius * 0.4f, radius * 0.6f );
		trap->R_AddLightToScene( lightOrg, intensity,
			Com_Clamp( 0, 1, atof( RPX_CvarStr( "ui_rpx_p_lr" ) ) / 255.0f ),
			Com_Clamp( 0, 1, atof( RPX_CvarStr( "ui_rpx_p_lg" ) ) / 255.0f ),
			Com_Clamp( 0, 1, atof( RPX_CvarStr( "ui_rpx_p_lb" ) ) / 255.0f ) );
	}
	trap->R_RenderScene( &refdef );

	Com_sprintf( text, sizeof( text ), "%.0f x %.0f x %.0f units", size[0] * propScale, size[1] * propScale, size[2] * propScale );
	Text_Paint( rect->x + 4, rect->y + rect->h - 18, scale * 0.8f, color, text, 0, 0, ITEM_TEXTSTYLE_NORMAL, iMenuFont );
}

static void RPX_DrawFxBox( rectDef_t *rect, float scale, vec4_t color, int iMenuFont ) {
	static vec4_t black = { 0, 0, 0, 1 };
	int now = trap->Milliseconds();

	RPX_CheckMap();
	UI_FillRect( rect->x, rect->y, rect->w, rect->h, black );

	// tell cgame the box is on screen, so it adds the preview's effect to the scene
	if ( !rpxFxBoxSet || now - rpxFxBoxSet >= 100 || now - rpxFxBoxSet < 0 ) {
		rpxFxBoxSet = now ? now : 1;
		trap->Cvar_Set( "ui_rpx_fxbox", va( "%d", rpxFxBoxSet ) );
	}

	if ( rpxFxWindowEnd && now - rpxFxWindowEnd < 0 ) {
		refdef_t refdef;
		vec3_t origin, camOrg, camAngles;
		int dir = atoi( RPX_CvarStr( "ui_rpx_e_dir" ) ), tilt = RPX_FxTilt();

		// GalaxyRP: [Extras] Direction or Tilt changed while the effect shows: it is played again the
		// new way once they have rested RPX_FX_SETTLE (a dragged slider changes it every frame). Tilt
		// only counts for Direction Tilted. A replay of the same effect takes no new preview slot.
		if ( dir != rpxFxDir || ( dir == 2 && tilt != rpxFxPitch ) ) {
			if ( !rpxFxChangeAt || dir != rpxFxSeenDir || tilt != rpxFxSeenPitch ) {
				rpxFxChangeAt = now ? now : 1;
				rpxFxSeenDir = dir;
				rpxFxSeenPitch = tilt;
			} else if ( now - rpxFxChangeAt >= RPX_FX_SETTLE ) {
				RPX_PreviewEffect();
			}
		} else {
			rpxFxChangeAt = 0;
		}

		RPX_SceneRect( rect, &refdef, RPX_FX_FOV );
		refdef.time = atoi( RPX_CvarStr( "ui_rpx_fxtime" ) );	// the effects' shader times are on cgame's clock
		RPX_FxCamera( atof( RPX_CvarStr( "ui_rpx_e_cdist" ) ), atof( RPX_CvarStr( "ui_rpx_e_cang" ) ), rpxFxDir, origin, camOrg, camAngles );
		VectorCopy( camOrg, refdef.vieworg );
		VectorCopy( camAngles, refdef.viewangles );
		AnglesToAxis( camAngles, refdef.viewaxis );
		// no R_ClearScene: the scene holds what cgame's CG_RpxFxPass() added, the preview's effect
		trap->R_RenderScene( &refdef );
		return;
	}
	rpxFxWindowEnd = 0;
	rpxFxChangeAt = 0;
	RPX_CenterText( rect, scale, color, "Press Preview to play it", iMenuFont );
}

// a light's colour from three 0..255 cvars; dark (black) when "lit" says it gives no light
static void RPX_DrawSwatchOf( rectDef_t *rect, const char *r, const char *g, const char *b, qboolean lit ) {
	static vec4_t edge = { 1, 0.682f, 0, 1 };
	vec4_t c;

	c[0] = lit ? Com_Clamp( 0, 1, atof( RPX_CvarStr( r ) ) / 255.0f ) : 0;
	c[1] = lit ? Com_Clamp( 0, 1, atof( RPX_CvarStr( g ) ) / 255.0f ) : 0;
	c[2] = lit ? Com_Clamp( 0, 1, atof( RPX_CvarStr( b ) ) / 255.0f ) : 0;
	c[3] = 1;
	UI_FillRect( rect->x, rect->y, rect->w, rect->h, c );
	UI_DrawRect( rect->x, rect->y, rect->w, rect->h, edge );
}

static void RPX_DrawSwatch( rectDef_t *rect ) {
	RPX_DrawSwatchOf( rect, "ui_rpx_p_lr", "ui_rpx_p_lg", "ui_rpx_p_lb", qtrue );
}

qboolean UI_RpxOwnerDraw( int ownerDraw, rectDef_t *rect, float scale, vec4_t color, int iMenuFont ) {
	switch ( ownerDraw ) {
	case UI_RPX_PREVIEW:
		RPX_DrawPropPreview( rect, scale, color, iMenuFont );
		return qtrue;
	case UI_RPX_FXBOX:
		RPX_DrawFxBox( rect, scale, color, iMenuFont );
		return qtrue;
	case UI_RPX_LSWATCH:
		RPX_DrawSwatchOf( rect, "ui_rpx_l_r", "ui_rpx_l_g", "ui_rpx_l_b", qtrue );
		return qtrue;
	case UI_RPX_LOFFSWATCH:	// black while it is dark when off (Off radius 0)
		RPX_DrawSwatchOf( rect, "ui_rpx_l_offr", "ui_rpx_l_offg", "ui_rpx_l_offb", atoi( RPX_CvarStr( "ui_rpx_l_offrad" ) ) > 0 ? qtrue : qfalse );
		return qtrue;
	case UI_RPX_SWATCH:
		RPX_DrawSwatch( rect );
		return qtrue;
	default:
		return qfalse;
	}
}

// with the ui's own cvars, so they exist, with their defaults, before cgame first starts
void UI_RpxRegisterCvars( void ) {
#define RPX_REGISTER( name, def ) trap->Cvar_Register( NULL, name, def, CVAR_TEMP );
	RPX_CVAR_LIST( RPX_REGISTER )
#undef RPX_REGISTER
}
