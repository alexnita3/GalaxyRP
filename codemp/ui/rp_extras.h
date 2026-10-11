/*
===========================================================================
GalaxyRP: [Extras] what the Extras menus of the Galaxy RP menu share between the ui module (ui_main.c:
the lists, the prop preview, the menus' scripts) and cgame (cg_rpextras.c: asking the server for the
listings, the commands the menus send, the effect preview).

How the two work together:

- The menus keep their state in the ui_rpx_* cvars below: the folder and the selection of each list,
  the options of each menu. cgame puts them all back to these defaults when it starts (every map),
  so a menu remembers its folder, selection and options until the map changes.
- The ui asks cgame for a listing with "rpx list <kind>" (a cgame console command); cgame asks the
  server (/rpxlist, g_rplist.c), writes what it gets to one of rpxListFiles (named in ui_rpx_file)
  and raises ui_rpx_serial, and the ui reads the file then. ui_rpx_status says what is going on
  meanwhile. Several names, because the engine writes to one folder and reads from the first folder
  on its search path that has the file: on TaystJK the write folder can be taystjk/ for a player's
  first map and the mod folder after (RP_AdoptTaystJKWriteFolder() in bg_misc.c), and an old copy
  in a folder searched first would hide the new one. cgame reads each file back after writing it
  and moves on to the next name when what it reads is not what it wrote (the serial in its first
  line).
- The action buttons run "rpx do <action>": cgame builds the server command from the cvars and
  sends it itself (never through the console, so no name can add a command of its own), after
  checking the selected name is one the server listed.
- The effect preview: cgame plays the effect as a sky-portal effect at RPX_FX_ORIGIN, far outside
  any map, and adds it to the scene after everything else it draws; the ui's preview box renders
  that scene without clearing it first. See CG_RpxFxPass() in cg_rpextras.c for what it shows and
  what it does not.
===========================================================================
*/

#ifndef RP_EXTRAS_H
#define RP_EXTRAS_H

// .dat: readable as a loose file even on a pure server. The first line is
// RPX_LIST_MAGIC <serial> <kind> <folder> <count> <cut short 0/1>, tab-separated; then a name a line.
static const char * const rpxListFiles[] = { "rpx/list.dat", "rpx/list1.dat", "rpx/list2.dat", "rpx/list3.dat" };
#define RPX_NUM_LIST_FILES	( (int)( sizeof( rpxListFiles ) / sizeof( rpxListFiles[0] ) ) )
#define RPX_LIST_MAGIC		"RPX1"
#define RPX_MAX_ENTRIES		4096
#define RPX_NAME_LEN		64				// MAX_QPATH: nothing longer could be loaded anyway

typedef enum {
	RPX_MODELS,
	RPX_EFFECTS,
	RPX_NPCS,
	RPX_VEHICLES,
	RPX_MUSIC,
	RPX_SOUNDS,
	RPX_NUM_KINDS
} rpxKindNum_t;

// the kind names are also the /rpxlist words; root and default folder as in g_rplist.c's sources
static const struct {
	const char *name;
	const char *root;			// NULL: a list of types, no folders
	const char *defaultFolder;
	const char *ext;			// added to a file name to make its path; "" when it keeps its own
} rpxKinds[RPX_NUM_KINDS] = {
	{ "models",		"models",	"map_objects",	".md3" },
	{ "effects",	"effects",	"",				"" },
	{ "npcs",		NULL,		"",				"" },
	{ "vehicles",	NULL,		"",				"" },
	{ "music",		"music",	"",				"" },
	{ "sounds",		"sound",	"",				"" },
};

// the /playsound channels, in the order the Sounds menu lists them (Cmd_ZykSound_f in g_cmds.c)
static const char * const rpxSoundChannels[] = {
	"auto", "local", "weapon", "voice", "voice_attenuate", "item", "body", "ambient",
	"local_sound", "announcer", "less_attenuate", "menu1", "voice_global", "music"
};
#define RPX_NUM_SOUND_CHANNELS	( (int)( sizeof( rpxSoundChannels ) / sizeof( rpxSoundChannels[0] ) ) )

// the menus' cvars and their defaults. Folders and selections per list; the filter per menu.
#define RPX_CVAR_LIST( X ) \
	X( "ui_rpx_dir_models",		"map_objects" ) \
	X( "ui_rpx_dir_effects",	"" ) \
	X( "ui_rpx_dir_music",		"" ) \
	X( "ui_rpx_dir_sounds",		"" ) \
	X( "ui_rpx_sel_models",		"" ) \
	X( "ui_rpx_sel_effects",	"" ) \
	X( "ui_rpx_sel_npcs",		"" ) \
	X( "ui_rpx_sel_vehicles",	"" ) \
	X( "ui_rpx_sel_music",		"" ) \
	X( "ui_rpx_sel_sounds",		"" ) \
	X( "ui_rpx_filter_models",	"" ) \
	X( "ui_rpx_filter_effects",	"" ) \
	X( "ui_rpx_filter_npcs",	"" ) \
	X( "ui_rpx_filter_music",	"" ) \
	X( "ui_rpx_filter_sounds",	"" ) \
	X( "ui_rpx_npcmode",		"0" )		/* 0 NPCs, 1 vehicles */ \
	X( "ui_rpx_p_yaw",			"0" ) \
	X( "ui_rpx_p_pitch",		"0" ) \
	X( "ui_rpx_p_roll",			"0" ) \
	X( "ui_rpx_p_zoom",			"100" )		/* percent */ \
	X( "ui_rpx_p_scale",		"100" )		/* percent: 1 to 1023 (modelscale 0.01 to 10.23) */ \
	X( "ui_rpx_p_solid",		"1" ) \
	X( "ui_rpx_p_anim",			"0" ) \
	X( "ui_rpx_p_break",		"0" ) \
	X( "ui_rpx_p_health",		"100" ) \
	X( "ui_rpx_p_light",		"0" ) \
	X( "ui_rpx_p_lrad",			"200" ) \
	X( "ui_rpx_p_lr",			"255" )		/* the light's colour, 0 to 255 like the saber colour sliders */ \
	X( "ui_rpx_p_lg",			"255" ) \
	X( "ui_rpx_p_lb",			"255" ) \
	X( "ui_rpx_p_faceme",		"1" ) \
	X( "ui_rpx_p_lift",			"0" )		/* Props: units out from the surface aimed at (/entaddaim aimoffset) */ \
	X( "ui_rpx_e_dir",			"0" )		/* 0 up, 1 facing me, 2 facing me, tilted by ui_rpx_e_pitch */ \
	X( "ui_rpx_e_pitch",		"0" ) \
	X( "ui_rpx_e_delay",		"200" ) \
	X( "ui_rpx_e_random",		"0" ) \
	X( "ui_rpx_e_off",			"0" ) \
	X( "ui_rpx_e_once",			"0" ) \
	X( "ui_rpx_e_lift",			"0" )		/* Effects: units out from the surface aimed at (/entaddaim aimoffset) */ \
	X( "ui_rpx_e_cdist",		"160" ) \
	X( "ui_rpx_e_cang",			"0" ) \
	X( "ui_rpx_n_name",			"" ) \
	X( "ui_rpx_l_rad",			"300" )		/* Lights: the radius, on */ \
	X( "ui_rpx_l_r",			"255" )		/* the colour on, 0 to 255 */ \
	X( "ui_rpx_l_g",			"255" ) \
	X( "ui_rpx_l_b",			"255" ) \
	X( "ui_rpx_l_off",			"0" )		/* starts switched off */ \
	X( "ui_rpx_l_offrad",		"0" )		/* the radius when off: 0 is dark */ \
	X( "ui_rpx_l_offr",			"255" )		/* the colour when off */ \
	X( "ui_rpx_l_offg",			"255" ) \
	X( "ui_rpx_l_offb",			"255" ) \
	X( "ui_rpx_l_name",			"" )		/* its targetname, to switch it with */ \
	X( "ui_rpx_l_lift",			"32" )		/* units out from the surface aimed at (/entaddaim aimoffset) */ \
	X( "ui_rpx_sp_name",		"" )		/* Spawners: the spawner's targetname, required */ \
	X( "ui_rpx_sp_now",			"1" )		/* spawnnow: one at once as well, not counted */ \
	X( "ui_rpx_sp_count",		"1" )		/* how many times it can be fired, 1 to 999 */ \
	X( "ui_rpx_sp_unlim",		"0" )		/* no limit: count -1 */ \
	X( "ui_rpx_sp_delay",		"0" )		/* seconds after being fired, 0 to 3600 */ \
	X( "ui_rpx_sp_shy",			"0" )		/* spawnflags 2048 */ \
	X( "ui_rpx_sp_respawn",		"0" )		/* respawn: fired again when one it made dies */ \
	X( "ui_rpx_sp_face",		"0" )		/* 0 towards me, 1 the way I look */ \
	X( "ui_rpx_sp_npcname",		"" )		/* NPC_targetname: the NPC's or vehicle's own name */ \
	X( "ui_rpx_sp_health",		"0" )		/* NPCs: 0 is the type's own */ \
	X( "ui_rpx_sp_hbar",		"0" )		/* NPCs: showhealth */ \
	X( "ui_rpx_sp_noai",		"0" )		/* NPCs: spawnflags 32 (cinematic) */ \
	X( "ui_rpx_sp_frozen",		"0" )		/* NPCs: npcfreeze, spawned frozen as /npc freeze leaves them */ \
	X( "ui_rpx_sp_solid",		"1" )		/* NPCs: 0 is spawnflags 64 (not solid) */ \
	X( "ui_rpx_sp_effect",		"0" )		/* NPCs: npceffect, 1 holo, 2 ghost, 3 walkthrough */ \
	X( "ui_rpx_sp_team",		"0" )		/* both: npcteam, 1 player, 2 enemy, 3 neutral, 4 free; 0 its type's own */ \
	X( "ui_rpx_sp_credits",		"0" )		/* NPCs: npccredits, paid to the logged-in player who kills one, 0 to 100000 */ \
	X( "ui_rpx_sp_xp",			"0" )		/* NPCs: npcxp, likewise, 0 to 100 */ \
	X( "ui_rpx_sp_ondeath",		"" )		/* NPCs: NPC_target, fired when it dies */ \
	X( "ui_rpx_sp_vdie",		"0" )		/* vehicles: spawnflags 1, explodes once left by its rider */ \
	X( "ui_rpx_sp_vtime",		"10" )		/* vehicles: seconds the rider may stay away (dmg, in ms) */ \
	X( "ui_rpx_sp_vdist",		"512" )		/* vehicles: how far the rider may go first (speed) */ \
	X( "ui_rpx_sp_vdock",		"0" )		/* vehicles: spawnflags 2, fighters hang until boarded */ \
	X( "ui_rpx_sp_lift",		"0" )		/* units out from the surface aimed at (/entaddaim aimoffset) */ \
	X( "ui_rpx_s_chan",			"0" ) \
	X( "ui_rpx_status",			"" )		/* cgame: loading, or why a listing failed */ \
	X( "ui_rpx_serial",			"0" )		/* cgame: raised each time it writes the list file */ \
	X( "ui_rpx_file",			"rpx/list.dat" )	/* cgame: which of rpxListFiles it wrote */ \
	X( "ui_rpx_msg",			"" )		/* either: the result of the last button */ \
	X( "ui_rpx_info",			"" )		/* ui: the folder and how many entries */ \
	X( "ui_rpx_selname",		"" )		/* ui: the selected name in full, which the list may cut short */ \
	X( "ui_rpx_fxbox",			"0" )		/* ui: when it last drew the effect preview box (Milliseconds) */ \
	X( "ui_rpx_back",			"0" )		/* ui: the Back button asks the Galaxy RP menu for its Extras tab */ \
	X( "ui_rpx_sent",			"0" )		/* ui: when a menu last sent the server a command (Milliseconds) */ \
	X( "ui_rpx_fxtime",			"0" )		/* cgame: its time, for the effect preview's render */ \
	X( "ui_rpx_mapid",			"0" )		/* cgame: new at each cgame start; lists and prop previews from before are old */ \
	X( "ui_rpx_viewyaw",		"0" )		/* cgame: the player's view yaw (0-359) while a menu is open, for the prop preview */

// the effect preview: where the effect plays (far outside any map, so no map geometry or sky effect
// is near it), how long after a Preview click it is drawn, and the camera
#define RPX_FX_ORIGIN_X		0.0f
#define RPX_FX_ORIGIN_Y		0.0f
#define RPX_FX_ORIGIN_Z		60000.0f
#define RPX_FX_WINDOW		15000
#define RPX_FX_HEARTBEAT	300		// the box counts as on screen this long after the ui last drew it
#define RPX_FX_FOV			60.0f
#define RPX_FX_ELEVATION	20.0f
#define RPX_FX_SETTLE		250		// Direction or Tilt changed while the preview shows: replayed once they rest this long

// the most different models and effects one map's previews may load (each keeps a slot until then)
#define RPX_PREVIEW_MODELS	150
#define RPX_PREVIEW_EFFECTS	32

// the camera of the effect preview, the same in both modules: it circles the look-at point at "angle"
// degrees round from +x, RPX_FX_ELEVATION up, "dist" away; for an effect going up it looks a little
// above where it starts
static QINLINE void RPX_FxCamera( float dist, float angle, int dirMode, vec3_t origin, vec3_t camOrg, vec3_t camAngles ) {
	vec3_t lookAt, dir;
	float yaw, pitch;

	if ( dist < 32.0f ) dist = 32.0f;
	if ( dist > 1024.0f ) dist = 1024.0f;

	VectorSet( origin, RPX_FX_ORIGIN_X, RPX_FX_ORIGIN_Y, RPX_FX_ORIGIN_Z );
	VectorCopy( origin, lookAt );
	if ( dirMode == 0 ) {
		lookAt[2] += dist * 0.3f;
	}

	yaw = DEG2RAD( angle );
	pitch = DEG2RAD( RPX_FX_ELEVATION );
	VectorSet( dir, cos( pitch ) * cos( yaw ), cos( pitch ) * sin( yaw ), sin( pitch ) );
	VectorMA( lookAt, dist, dir, camOrg );

	VectorSubtract( lookAt, camOrg, dir );
	vectoangles( dir, camAngles );
}

// the direction the preview effect is played in: up, or towards the camera (level, or tilted by
// pitch: -90 is straight up, as for an fx_runner's angles)
static QINLINE void RPX_FxDirection( int dirMode, float pitch, const vec3_t origin, const vec3_t camOrg, vec3_t fwd ) {
	vec3_t toCam, angles;

	if ( dirMode <= 0 || dirMode > 2 ) {
		VectorSet( fwd, 0, 0, 1 );
		return;
	}
	VectorSubtract( camOrg, origin, toCam );
	toCam[2] = 0;
	vectoangles( toCam, angles );
	angles[PITCH] = ( dirMode == 2 ) ? pitch : 0;
	angles[ROLL] = 0;
	AngleVectors( angles, fwd, NULL, NULL );
}

// what is wrong with a name (targetname) typed in the NPCs or Lights menu, or NULL: letters, digits, _ and -,
// at most 32 of them
static QINLINE const char *RPX_LabelProblem( const char *label ) {
	int i;

	for ( i = 0; label[i]; i++ ) {
		char c = label[i];

		if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '-' ) ) {
			return "The name may only have letters, digits, _ and -.";
		}
	}
	if ( i > 32 ) {
		return "The name may be at most 32 characters long.";
	}
	return NULL;
}

// GalaxyRP: [Extras] what is wrong with the names typed in the Spawners menu, or NULL. The spawner's name is
// required -- triggers, buttons and /entuse fire it by it; the server refuses a spawner without one -- and
// all three are names as RPX_LabelProblem() takes them. The NPC's own name may not be the spawner's (firing
// the spawner would use its NPCs too: a vehicle used makes whoever used it board it), and an NPC's
// "On death fires" (npcs only) may not be the spawner's either: Respawn when killed is that, done once.
static QINLINE const char *RPX_SpawnerNamesProblem( const char *spawner, const char *npc, const char *ondeath, qboolean npcs ) {
	static char problem[128];
	const char *names[3], *labels[3];
	int i, n = npcs ? 3 : 2;

	if ( !spawner[0] ) {
		return "Give the spawner a name: triggers, buttons and /entuse fire it by it.";
	}
	names[0] = spawner;	labels[0] = "The spawner's name";
	names[1] = npc;		labels[1] = "Its name";
	names[2] = ondeath;	labels[2] = "On death fires";
	for ( i = 0; i < n; i++ ) {
		const char *p = RPX_LabelProblem( names[i] );

		if ( p ) {
			// "The name may ..." with the field's own label
			if ( strncmp( p, "The name", 8 ) ) {
				return p;
			}
			Com_sprintf( problem, sizeof( problem ), "%s%s", labels[i], p + 8 );
			return problem;
		}
	}
	if ( npc[0] && !Q_stricmp( npc, spawner ) ) {
		return "The spawner and what it spawns need different names.";
	}
	if ( npcs && ondeath[0] && !Q_stricmp( ondeath, spawner ) ) {
		return "On death fires cannot be the spawner's own name: use Respawn when killed.";
	}
	return NULL;
}

// a name from a listing that can be used safely: printable, none of " ; \ | and not "." or "..";
// a folder (folderOk) ends in its one "/"
static QINLINE qboolean RPX_NameOk( const char *name, qboolean folderOk ) {
	int i, len;

	if ( !name || !name[0] ) {
		return qfalse;
	}
	len = strlen( name );
	if ( len >= RPX_NAME_LEN ) {
		return qfalse;
	}
	for ( i = 0; i < len; i++ ) {
		unsigned char c = (unsigned char)name[i];

		// '%': a server command or a client command carries it as '.' (MSG_ReadString())
		if ( c < 32 || c == 127 || c == '"' || c == ';' || c == '\\' || c == '|' || c == '%' ) {
			return qfalse;
		}
		if ( c == '/' && !( folderOk && i == len - 1 && i > 0 ) ) {
			return qfalse;
		}
	}
	if ( !strcmp( name, "." ) || !strcmp( name, ".." ) || !strcmp( name, "./" ) || !strcmp( name, "../" ) ) {
		return qfalse;
	}
	return qtrue;
}

// a folder of a list, relative to its root ("" for the root): no "..", "//", or slash at either end,
// and only characters a name can have
static QINLINE qboolean RPX_FolderOk( const char *folder ) {
	int i, len = strlen( folder );

	if ( len >= RPX_NAME_LEN ) {
		return qfalse;
	}
	if ( !len ) {
		return qtrue;
	}
	if ( folder[0] == '/' || folder[len - 1] == '/' || strstr( folder, "//" ) || strstr( folder, ".." ) ) {
		return qfalse;
	}
	for ( i = 0; i < len; i++ ) {
		unsigned char c = (unsigned char)folder[i];

		if ( c < 32 || c == 127 || c == '"' || c == ';' || c == '\\' || c == '|' || c == ':' || c == '%' ) {
			return qfalse;
		}
	}
	return qtrue;
}

#endif // RP_EXTRAS_H
