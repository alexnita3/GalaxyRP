/*
===========================================================================
GalaxyRP: [Listings] the server-side listings: /list models, effects, sounds, music, maps, npcs and
vehicles, and /maplist. Also the file checks /playsound and /playmusic make before they touch the
sound table or the music configstring.

Now that the Entity System refuses a prop whose model or effect file the server does not have, an
admin needs a way to see what it does have; the sounds and music a player can play, the maps they
can vote for and the NPC and vehicle types an admin can spawn are the same kind of question.

Everything is read on the server, and every listing is protected against flooding the player:

- About RP_LIST_PAGE_SIZE entries a page. A page is built as one text and sent in pieces of at most
  RP_LIST_MSG_CHARS characters, never more than RP_LIST_MAX_MSGS of them. SV_SendServerCommand
  drops a formatted command over 1022 characters whole and without a word, and a client that
  falls 64 reliable commands behind is dropped with "server command overflow" -- the old
  /maplist bsp could send some forty messages at once. A piece ends at a space or a line break
  when the rest still fits in the pieces left (otherwise anywhere but inside a colour code), and
  the next one starts with the colour the last one ended in: the console runs consecutive
  messages together, but starts each one white.
- RP_LIST_COOLDOWN between two listings from one player, and RP_LIST_FS_GAP between any two file
  listings on the server: a file listing makes the engine walk every name in every pk3, several
  times.
- A note when the engine's cap on one file listing (MAX_FOUND_FILES in files.cpp, 4096 names, or
  the buffer it packs them into) may have cut the list short, since FS_GetFileList() says nothing
  when it does.

What FS_GetFileList() returns, and why the folder code below looks the way it does (files.cpp,
FS_ListFilteredFiles()):

- In a pk3 it returns the matching files of the folder AND of its subfolders one level down, as
  "sub/name.ext"; loose (unpacked) folders only give their own files. So the subfolders of a
  folder come from three places: the "sub/" part of those names; the same listing asked for
  "folder/", which reaches one level further (see RP_ListFiles()), for subfolders that hold only
  subfolders; and a listing with the extension "/", which returns folders -- every loose one, and a
  pk3's only when the pk3 stores folder entries, which not all do.
- In a pk3 it matches the folder name as a bare prefix, with no check for the "/" after it: listed
  for "sound", a pk3 file "soundfx/a/b.wav" comes back as "x/a/b.wav" -- "fx" minus the one
  character it skips for the "/". A name that starts with "/" is always one of these; a "sub/"
  part can be one too, so a subfolder only a pk3 name suggests is kept once one of its files
  opens under the folder being listed (RP_LIST_VERIFY_TRIES tries). A pk3's folder entries come
  through the same match ("soundtrack/" listed for "sound" is a folder "rack"), so a folder the
  "/" listing names is kept once one of its files opens the same way, or, if none of its files was
  named, once a listing of the folder itself finds something in it. A plain file name cannot be
  one: a file of a sibling folder always has a "/" left in it.
===========================================================================
*/

#include "g_local.h"

#define RP_LIST_PAGE_SIZE		50
#define RP_LIST_MAX_ENTRIES		4096
#define RP_LIST_RAW_SIZE		0x40000		// what FS_GetFileList() packs its names into
#define RP_LIST_STORE_SIZE		0x50000		// the names kept for one listing
#define RP_LIST_MSG_CHARS		1000		// text in one "print" command, well inside its 1022
#define RP_LIST_MAX_MSGS		4
#define RP_LIST_COOLDOWN		1000
#define RP_LIST_FS_GAP			250			// between any two file listings on the server
#define RP_LIST_ENGINE_CAP		( 0x1000 - 1 )	// MAX_FOUND_FILES - 1: the most names one listing returns
#define RP_LIST_LONGEST_NAME	256			// MAX_ZPATH: the longest name a pk3 entry can have
#define RP_LIST_VERIFY_TRIES	3
#define RP_LIST_MAX_FOLDERS		1024		// subfolder candidates of one folder
#define RP_LIST_MAX_MAP_DIRS	16			// subfolders of maps/ listed on their own (/list maps)
#define RP_LIST_MAX_DIR_CHECKS	32			// folders only the "/" listing names, looked into to see they are real

typedef struct {
	const char	*name;		// what is printed
	qboolean	folder;
	int			order;		// when it was added: of two names that differ only in case, the first is kept
	qboolean	vehicle;	// an NPC type that is a vehicle's (RP_ListBuildTypes())
} rpListEntry_t;

typedef struct {
	char			*store;
	int				storeSize;
	int				storeUsed;
	rpListEntry_t	*entries;
	int				maxEntries;
	int				num;
	qboolean		truncated;	// something did not fit, here or in the engine's listing
	int				tooLong;	// left out: a path too long for the game to load
} rpListSet_t;

static char				rp_listRaw[RP_LIST_RAW_SIZE];

static char				rp_fileStore[RP_LIST_STORE_SIZE];
static rpListEntry_t	rp_fileEntries[RP_LIST_MAX_ENTRIES];
static rpListSet_t		rp_files = { rp_fileStore, sizeof( rp_fileStore ), 0, rp_fileEntries, RP_LIST_MAX_ENTRIES, 0, qfalse, 0 };

// NPC and vehicle types: read once per map (level.rp_list_*_ready), the text never changes after
// the game starts (NPC_LoadParms(), BG_VehicleLoadParms())
static char				rp_npcStore[0x20000];
static rpListEntry_t	rp_npcEntries[RP_LIST_MAX_ENTRIES];
static rpListSet_t		rp_npcs = { rp_npcStore, sizeof( rp_npcStore ), 0, rp_npcEntries, RP_LIST_MAX_ENTRIES, 0, qfalse, 0 };

static char				rp_vehStore[0x10000];
static rpListEntry_t	rp_vehEntries[RP_LIST_MAX_ENTRIES];
static rpListSet_t		rp_vehicles = { rp_vehStore, sizeof( rp_vehStore ), 0, rp_vehEntries, RP_LIST_MAX_ENTRIES, 0, qfalse, 0 };

// the entries of a type listing that match its text
static rpListEntry_t	rp_viewEntries[RP_LIST_MAX_ENTRIES];

typedef struct {
	char		name[MAX_QPATH];
	int			state;		// 1 kept, 0 not checked yet, -1 dropped
	int			tries;
	int			files;		// names of the pk3 listings under it
	qboolean	dirListed;	// named by the "/" listing
} rpListFolder_t;

static rpListFolder_t	rp_folders[RP_LIST_MAX_FOLDERS];
static int				rp_numFolders;

static char				rp_listPage[RP_LIST_MSG_CHARS * RP_LIST_MAX_MSGS + 1];

/*
===========================================================================
Names
===========================================================================
*/

static void RP_ListReset( rpListSet_t *set ) {
	set->storeUsed = 0;
	set->num = 0;
	set->truncated = qfalse;
	set->tooLong = 0;
}

// a name that can be printed inside "print \"...\"" and typed back: no control characters, no quotes
// (which would end the print command early), no backslashes (FS names use "/")
static qboolean RP_ListNameOk( const char *name ) {
	const unsigned char *s = (const unsigned char *)name;

	if ( !*s ) {
		return qfalse;
	}
	for ( ; *s; s++ ) {
		if ( *s < 32 || *s == 127 || *s == '"' || *s == '\\' ) {
			return qfalse;
		}
	}
	return qtrue;
}

static void RP_ListAdd( rpListSet_t *set, const char *name, qboolean folder ) {
	int len;

	if ( !RP_ListNameOk( name ) ) {
		return;
	}

	len = strlen( name ) + 1;
	if ( set->num >= set->maxEntries || set->storeUsed + len > set->storeSize ) {
		set->truncated = qtrue;
		return;
	}

	memcpy( set->store + set->storeUsed, name, len );
	set->entries[set->num].name = set->store + set->storeUsed;
	set->entries[set->num].folder = folder;
	set->entries[set->num].order = set->num;
	set->entries[set->num].vehicle = qfalse;
	set->storeUsed += len;
	set->num++;
}

static int QDECL RP_ListCompare( const void *a, const void *b ) {
	const rpListEntry_t *ea = (const rpListEntry_t *)a;
	const rpListEntry_t *eb = (const rpListEntry_t *)b;
	int c;

	if ( ea->folder != eb->folder ) {
		return ea->folder ? -1 : 1;	// folders first
	}
	c = Q_stricmp( ea->name, eb->name );
	if ( c ) {
		return c;
	}
	// the same name in another case: the one found first comes first and is the one kept -- the
	// first .npc or .veh block of that name is the one the game uses, and the file system returns the
	// pk3 it would open first before the others
	return ea->order - eb->order;
}

// sorted, folders first, each name once (without regard to case, as the file system and the NPC and
// vehicle loaders look them up)
static void RP_ListSortUnique( rpListSet_t *set ) {
	int i, kept = 0;

	if ( set->num <= 0 ) {
		return;
	}

	qsort( set->entries, set->num, sizeof( set->entries[0] ), RP_ListCompare );

	for ( i = 0; i < set->num; i++ ) {
		if ( kept > 0 && set->entries[kept - 1].folder == set->entries[i].folder
			&& !Q_stricmp( set->entries[kept - 1].name, set->entries[i].name ) ) {
			continue;
		}
		set->entries[kept++] = set->entries[i];
	}
	set->num = kept;
}

// case-insensitive "is needle anywhere in hay"
static qboolean RP_ListContains( const char *hay, const char *needle ) {
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

/*
==================
RP_ListParsePage

A page number typed by a player: 1 to 999999, digits only. 0 for anything else.
==================
*/
int RP_ListParsePage( const char *s ) {
	int i;

	if ( !s || !s[0] || strlen( s ) > 6 ) {
		return 0;
	}
	for ( i = 0; s[i]; i++ ) {
		if ( s[i] < '0' || s[i] > '9' ) {
			return 0;
		}
	}
	return atoi( s );
}

static qboolean RP_ListIsNumber( const char *s ) {
	int i;

	if ( !s || !s[0] ) {
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
===========================================================================
Output
===========================================================================
*/

/*
==================
RP_ListCooldown

qtrue (and a message) while this player's last listing is less than RP_LIST_COOLDOWN old; otherwise
starts a new one. pers is cleared on connect and on every map, so level.time and the stored time
always come from the same map; the "too far ahead" reset is only a guard.
==================
*/
static qboolean RP_ListCooldown( gentity_t *ent ) {
	gclient_t *cl = ent->client;

	if ( cl->pers.rpListNextTime > level.time + RP_LIST_COOLDOWN ) {
		cl->pers.rpListNextTime = 0;
	}
	if ( level.time < cl->pers.rpListNextTime ) {
		trap->SendServerCommand( ent - g_entities, "print \"^7Wait a second before the next listing.\n\"" );
		return qtrue;
	}
	cl->pers.rpListNextTime = level.time + RP_LIST_COOLDOWN;
	return qfalse;
}

/*
==================
RP_ListSendPage

rp_listPage in pieces of at most RP_LIST_MSG_CHARS characters, never more than RP_LIST_MAX_MSGS.

The console runs the pieces together as one text, but starts each "print" in white
(CL_ConsolePrint()), so each piece after the first begins with the colour code the one before it
was in -- two characters, which is why a piece carries at most RP_LIST_MSG_CHARS - 2 of the page.
A piece ends at the last space or line break in its final RP_LIST_CUT_SEARCH characters, so a name
is not broken across two, as long as what is left still fits in the pieces left at that size;
otherwise it is cut at its full size, but never between a "^" and the character it colours. The
largest page RP_ListPrintPage() builds is about 3900 characters, inside 4 * 998.
==================
*/
#define RP_LIST_PIECE		( RP_LIST_MSG_CHARS - 2 )
#define RP_LIST_CUT_SEARCH	80

static void RP_ListSendPage( gentity_t *ent ) {
	const char *p = rp_listPage;
	char chunk[RP_LIST_MSG_CHARS + 1];
	char carry[3] = "";
	int msgs = 0;

	while ( *p && msgs < RP_LIST_MAX_MSGS ) {
		int n = strlen( p );
		int k, c;

		if ( n > RP_LIST_PIECE ) {
			int left = ( RP_LIST_MAX_MSGS - msgs - 1 ) * RP_LIST_PIECE;

			n = RP_LIST_PIECE;
			if ( p[n - 1] == Q_COLOR_ESCAPE ) {
				n--;	// never between a "^" and the character it colours
			}
			for ( k = n; k > n - RP_LIST_CUT_SEARCH && k > 1; k-- ) {
				// after the whole run of spaces, so the next piece starts at the name and its own colour
				if ( ( p[k - 1] == ' ' || p[k - 1] == '\n' ) && p[k] != ' ' ) {
					if ( (int)strlen( p + k ) <= left ) {
						n = k;
					}
					break;
				}
			}
		}

		// no carried colour when the piece starts with a colour code of its own
		Com_sprintf( chunk, sizeof( chunk ), "%s%.*s", Q_IsColorString( p ) ? "" : carry, n, p );
		trap->SendServerCommand( ent - g_entities, va( "print \"%s\"", chunk ) );

		// the colour the next piece starts in: the last code in this one
		for ( c = strlen( chunk ) - 2; c >= 0; c-- ) {
			if ( Q_IsColorString( chunk + c ) ) {
				carry[0] = chunk[c];
				carry[1] = chunk[c + 1];
				carry[2] = '\0';
				break;
			}
		}

		p += n;
		msgs++;
	}
}

/*
==================
RP_ListPrintPage

One page of entries[0..num): a header, the entries (folders in cyan, the rest alternating green and
yellow as the old map list did), then the notes. nextCmd is what a player types for the next page,
without the number; useHint and a folder hint (when there are folders) follow it.
==================
*/
static void RP_ListPrintPage( gentity_t *ent, const rpListEntry_t *entries, int num, int page,
	const char *title, const char *nextCmd, const char *useHint, const char *what,
	qboolean truncated, int tooLong ) {
	int pages = ( num + RP_LIST_PAGE_SIZE - 1 ) / RP_LIST_PAGE_SIZE;
	int first, last, i, toggle = 0;
	qboolean anyFolder = qfalse, labels = qfalse, filesLabel = qfalse;
	int sz = sizeof( rp_listPage );

	if ( pages < 1 ) {
		pages = 1;
	}
	if ( page > pages ) {
		trap->SendServerCommand( ent - g_entities, va( "print \"^7Page %d does not exist. %s has %d page%s.\n\"",
			page, title, pages, pages == 1 ? "" : "s" ) );
		return;
	}

	first = ( page - 1 ) * RP_LIST_PAGE_SIZE;
	last = first + RP_LIST_PAGE_SIZE;
	if ( last > num ) {
		last = num;
	}

	for ( i = 0; i < num; i++ ) {
		if ( entries[i].folder ) {
			anyFolder = qtrue;
			break;
		}
	}
	labels = anyFolder;

	rp_listPage[0] = '\0';
	Q_strcat( rp_listPage, sz, va( "^3%s ^7- page %d of %d, %d %s:\n", title, page, pages, num, num == 1 ? "entry" : "entries" ) );

	for ( i = first; i < last; i++ ) {
		if ( entries[i].folder ) {
			if ( i == first ) {
				Q_strcat( rp_listPage, sz, "^7Folders:" );
			}
			Q_strcat( rp_listPage, sz, va( "  ^5%s/", entries[i].name ) );
		} else {
			if ( labels && !filesLabel ) {
				Q_strcat( rp_listPage, sz, i == first ? "^7Files:" : "\n^7Files:" );
				filesLabel = qtrue;
			}
			Q_strcat( rp_listPage, sz, va( "%s^%c%s", ( i == first && !labels ) ? "" : "  ",
				( ++toggle & 1 ) ? COLOR_GREEN : COLOR_YELLOW, entries[i].name ) );
		}
	}
	Q_strcat( rp_listPage, sz, "\n" );

	if ( page < pages ) {
		Q_strcat( rp_listPage, sz, va( "^7Next page: ^3%s %d\n", nextCmd, page + 1 ) );
	}
	if ( anyFolder && what ) {
		Q_strcat( rp_listPage, sz, va( "^7Open a folder with ^3/list %s <folder>^7.\n", what ) );
	}
	if ( useHint ) {
		Q_strcat( rp_listPage, sz, useHint );
	}
	if ( tooLong > 0 ) {
		Q_strcat( rp_listPage, sz, va( "^7%d with a path too long for the game to load %s left out.\n", tooLong, tooLong == 1 ? "was" : "were" ) );
	}
	if ( truncated ) {
		Q_strcat( rp_listPage, sz, "^1There are more than one listing can hold here, so some are missing.\n" );
	}

	RP_ListSendPage( ent );
}

/*
===========================================================================
Files
===========================================================================
*/

typedef struct {
	const char	*what;			// the /list word
	const char	*single;		// the same, singular
	const char	*title;
	const char	*root;
	const char	*defaultFolder;	// where a listing with no folder starts
	const char	*exts[2];
	qboolean	fullPath;		// print root/folder/name.ext (what /playsound and /playmusic take)
	qboolean	keepExt;
	qboolean	flatten;		// maps: the files of the first level of subfolders as "sub/name", no folders
	const char	*useHint;
} rpListSource_t;

static const rpListSource_t rp_listSources[] = {
	{ "models", "model", "Models", "models", "map_objects", { ".md3", NULL }, qfalse, qfalse, qfalse,
		"^7Use a name as the ^3model ^7key of a misc_model_breakable.\n" },
	{ "effects", "effect", "Effects", "effects", "", { ".efx", NULL }, qfalse, qfalse, qfalse,
		"^7Use a name as the ^3fxFile ^7key of an fx_runner.\n" },
	{ "sounds", "sound", "Sounds", "sound", "", { ".wav", ".mp3" }, qtrue, qtrue, qfalse,
		"^7Play one with ^3/playsound <path>^7.\n" },
	{ "music", "music", "Music", "music", "", { ".mp3", NULL }, qtrue, qtrue, qfalse,
		"^7Play one with ^3/playmusic <path>^7.\n" },
	{ "maps", "map", "Maps", "maps", "", { ".bsp", NULL }, qfalse, qfalse, qtrue,
		"^7Vote for one with ^3/callvote map <name>^7.\n" },
};

/*
==================
RP_ListCleanFolder

The folder a player typed, relative to the source's root: "\" read as "/", slashes at either end
dropped, the root itself allowed in front ("models/map_objects" = "map_objects") when stripRoot, "."
or "/" for the root. Refused: anything with "..", "//", ":", ";", quotes or control characters, or
too long. The Extras menus always send the folder relative to the root, so for them a first part
named like the root is a real subfolder (sound/sound) and is kept.
==================
*/
static qboolean RP_ListCleanFolder( const char *root, const char *in, char *out, int outSize, qboolean stripRoot ) {
	char tmp[MAX_QPATH];
	char *s;
	int i, len, rl;

	if ( strlen( in ) >= sizeof( tmp ) ) {
		return qfalse;
	}
	Q_strncpyz( tmp, in, sizeof( tmp ) );

	for ( i = 0; tmp[i]; i++ ) {
		unsigned char c = (unsigned char)tmp[i];

		if ( c == '\\' ) {
			tmp[i] = '/';
		} else if ( c < 32 || c == 127 || c == '"' || c == ';' || c == ':' ) {
			return qfalse;
		}
	}

	s = tmp;
	while ( *s == '/' ) {
		s++;
	}
	len = strlen( s );
	while ( len > 0 && s[len - 1] == '/' ) {
		s[--len] = '\0';
	}
	if ( !strcmp( s, "." ) ) {
		s[0] = '\0';
	}

	rl = strlen( root );
	if ( stripRoot && rl && !Q_stricmpn( s, root, rl ) && ( s[rl] == '/' || s[rl] == '\0' ) ) {
		s += rl;
		while ( *s == '/' ) {
			s++;
		}
	}

	if ( strstr( s, ".." ) || strstr( s, "//" ) ) {
		return qfalse;
	}

	Q_strncpyz( out, s, outSize );
	return qtrue;
}

// FS_GetFileList() into rp_listRaw, noting in set (when given) that the engine may have cut it short
static int RP_ListGetFiles( const char *path, const char *ext, rpListSet_t *set ) {
	int n, i, used = 0;

	rp_listRaw[0] = '\0';
	n = trap->FS_GetFileList( path, ext, rp_listRaw, sizeof( rp_listRaw ) );
	if ( n <= 0 ) {
		return 0;
	}

	for ( i = 0; i < n; i++ ) {
		const char *end = (const char *)memchr( rp_listRaw + used, '\0', sizeof( rp_listRaw ) - used );

		if ( !end ) {	// cannot happen with the engine's own FS_GetFileList; do not read past the buffer
			n = i;
			if ( set ) {
				set->truncated = qtrue;
			}
			break;
		}
		used = ( end - rp_listRaw ) + 1;
		if ( used >= (int)sizeof( rp_listRaw ) ) {
			n = i + 1;
			break;
		}
	}

	if ( set && ( n >= RP_LIST_ENGINE_CAP || used + RP_LIST_LONGEST_NAME + 2 >= (int)sizeof( rp_listRaw ) ) ) {
		set->truncated = qtrue;
	}
	return n;
}

static rpListFolder_t *RP_ListFolder( const char *name, rpListSet_t *set ) {
	static int last = -1;	// a pk3 names a folder's files one after another: ask about that one first
	int i;

	if ( last >= 0 && last < rp_numFolders && !Q_stricmp( rp_folders[last].name, name ) ) {
		return &rp_folders[last];
	}
	for ( i = 0; i < rp_numFolders; i++ ) {
		if ( !Q_stricmp( rp_folders[i].name, name ) ) {
			last = i;
			return &rp_folders[i];
		}
	}
	if ( strlen( name ) >= MAX_QPATH ) {
		return NULL;	// nothing in it could be loaded anyway
	}
	if ( rp_numFolders >= RP_LIST_MAX_FOLDERS ) {
		set->truncated = qtrue;
		return NULL;
	}
	Q_strncpyz( rp_folders[rp_numFolders].name, name, sizeof( rp_folders[0].name ) );
	rp_folders[rp_numFolders].state = 0;
	rp_folders[rp_numFolders].tries = 0;
	rp_folders[rp_numFolders].files = 0;
	rp_folders[rp_numFolders].dirListed = qfalse;
	last = rp_numFolders;
	return &rp_folders[rp_numFolders++];
}

// base + "/" + a + "/" + b, leaving out the empty parts
static void RP_ListJoin( char *out, int outSize, const char *base, const char *a, const char *b ) {
	out[0] = '\0';
	if ( base && base[0] ) {
		Q_strcat( out, outSize, base );
	}
	if ( a && a[0] ) {
		if ( out[0] ) {
			Q_strcat( out, outSize, "/" );
		}
		Q_strcat( out, outSize, a );
	}
	if ( b && b[0] ) {
		if ( out[0] ) {
			Q_strcat( out, outSize, "/" );
		}
		Q_strcat( out, outSize, b );
	}
}

// a file of folder/sub, as the source prints it
static void RP_ListAddFile( rpListSet_t *set, const rpListSource_t *src, const char *folder, const char *sub, const char *file, qboolean countTooLong ) {
	char rel[RP_LIST_LONGEST_NAME * 2];
	char full[RP_LIST_LONGEST_NAME * 3];

	if ( !RP_ListNameOk( file ) ) {
		return;
	}

	RP_ListJoin( rel, sizeof( rel ), folder, sub, file );
	RP_ListJoin( full, sizeof( full ), src->root, rel, NULL );

	// the game cannot load it (R_RegisterModel, S_RegisterSound, the effect and music names are all
	// MAX_QPATH), so it is no use to list it
	if ( strlen( full ) >= MAX_QPATH ) {
		if ( countTooLong ) {
			set->tooLong++;
		}
		return;
	}

	if ( src->fullPath ) {
		RP_ListAdd( set, full, qfalse );
	} else {
		if ( !src->keepExt ) {
			COM_StripExtension( rel, rel, sizeof( rel ) );
		}
		RP_ListAdd( set, rel, qfalse );
	}
}

// file "sub/name" of a pk3: keep it (qtrue) once sub is known to be a real subfolder of path
static qboolean RP_ListCheckSub( rpListSet_t *set, const char *path, const char *sub, const char *file ) {
	rpListFolder_t *f = RP_ListFolder( sub, set );
	char check[RP_LIST_LONGEST_NAME * 3];

	if ( !f ) {
		return qfalse;
	}
	f->files++;
	if ( f->state == 0 ) {
		RP_ListJoin( check, sizeof( check ), path, sub, file );
		if ( strlen( check ) < RP_LIST_LONGEST_NAME && RP_FileExists( check ) ) {
			f->state = 1;
		} else if ( ++f->tries >= RP_LIST_VERIFY_TRIES ) {
			f->state = -1;
		}
		return ( f->state == 1 ) ? qtrue : qfalse;
	}
	return ( f->state == 1 ) ? qtrue : qfalse;
}

/*
==================
RP_ListCollectFiles

The files and subfolders of one folder of a source into rp_files, sorted, folders first, each once:
what /list prints a page of and what the Extras menus are sent (RP_ListDataCommand()). folderArg is
the folder as typed, NULL for the source's default one; folder gets it cleaned, relative to the
source's root, and path the root joined with it. qfalse, with *error saying why, when it is not a
folder name that can be listed.
==================
*/
static qboolean RP_ListCollectFiles( const rpListSource_t *src, const char *folderArg, qboolean stripRoot,
	char *folder, int folderSize, char *path, int pathSize, const char **error ) {
	rpListSet_t *set = &rp_files;
	char name[RP_LIST_LONGEST_NAME + 1];
	int e, n, i, d, numDirs, checks;
	const char *p;

	if ( folderArg ) {
		if ( !RP_ListCleanFolder( src->root, folderArg, folder, folderSize, stripRoot ) ) {
			*error = "That is not a folder name that can be listed.";
			return qfalse;
		}
	} else {
		Q_strncpyz( folder, src->defaultFolder, folderSize );
	}

	RP_ListJoin( path, pathSize, src->root, folder, NULL );
	if ( strlen( path ) >= MAX_QPATH ) {
		*error = "That folder name is too long.";
		return qfalse;
	}

	RP_ListReset( set );
	rp_numFolders = 0;

	// folders the engine reports as folders: every loose one, and a pk3's when it stores them -- which
	// the bare-prefix match applies to as well, so they are checked like the others (see the top)
	n = RP_ListGetFiles( path, "/", set );
	for ( i = 0, p = rp_listRaw; i < n; i++, p += strlen( p ) + 1 ) {
		rpListFolder_t *f;
		int len;

		Q_strncpyz( name, p, sizeof( name ) );
		for ( len = 0; name[len]; len++ ) {
			if ( name[len] == '\\' ) {
				name[len] = '/';
			}
		}
		while ( len > 0 && name[len - 1] == '/' ) {
			name[--len] = '\0';
		}
		if ( !name[0] || strchr( name, '/' ) || !strcmp( name, "." ) || !strcmp( name, ".." ) || !RP_ListNameOk( name ) ) {
			continue;
		}
		f = RP_ListFolder( name, set );
		if ( f ) {
			f->dirListed = qtrue;
		}
	}
	numDirs = rp_numFolders;	// rp_folders[0..numDirs) are the ones the "/" listing named

	for ( e = 0; e < 2 && src->exts[e]; e++ ) {
		n = RP_ListGetFiles( path, src->exts[e], set );
		for ( i = 0, p = rp_listRaw; i < n; i++, p += strlen( p ) + 1 ) {
			char *slash;
			int k;

			Q_strncpyz( name, p, sizeof( name ) );
			for ( k = 0; name[k]; k++ ) {
				if ( name[k] == '\\' ) {
					name[k] = '/';
				}
			}
			if ( !name[0] || name[0] == '/' ) {
				continue;	// from a pk3 folder whose name is this one plus a character
			}

			slash = strchr( name, '/' );
			if ( !slash ) {
				RP_ListAddFile( set, src, folder, NULL, name, qtrue );
				continue;
			}
			if ( strchr( slash + 1, '/' ) || !slash[1] ) {
				continue;
			}
			*slash = '\0';
			if ( RP_ListCheckSub( set, path, name, slash + 1 ) && src->flatten ) {
				RP_ListAddFile( set, src, folder, name, slash + 1, qtrue );
			}
		}
	}

	if ( !src->flatten ) {
		// subfolders that hold only subfolders (models/map_objects, sound/chars): a pk3 names nothing in
		// them one level down, so they are only seen from two levels down. FS_ListFilteredFiles() takes
		// its depth limit from the "/"s in the path but drops a trailing one before it matches the names,
		// so "path/" reaches one level further than "path" and returns the same relative names.
		char deepPath[MAX_QPATH * 2 + 2];

		Com_sprintf( deepPath, sizeof( deepPath ), "%s/", path );
		for ( e = 0; e < 2 && src->exts[e]; e++ ) {
			// not noted when cut short: only folders that hold nothing but folders could be missing, and a big
			// folder reaches the engine's cap this deep while every file shown is there
			n = RP_ListGetFiles( deepPath, src->exts[e], NULL );
			for ( i = 0, p = rp_listRaw; i < n; i++, p += strlen( p ) + 1 ) {
				char *slash;
				int k;

				Q_strncpyz( name, p, sizeof( name ) );
				for ( k = 0; name[k]; k++ ) {
					if ( name[k] == '\\' ) {
						name[k] = '/';
					}
				}
				if ( !name[0] || name[0] == '/' ) {
					continue;
				}
				slash = strchr( name, '/' );
				if ( !slash || !slash[1] ) {
					continue;	// the folder's own files: the listing above has them
				}
				*slash = '\0';
				RP_ListCheckSub( set, path, name, slash + 1 );
			}
		}
	}

	// the folders only the "/" listing named, with no file of this kind under them in any pk3 name:
	// a loose folder (its files and subfolders are never "sub/" names), a pk3 folder of other files,
	// or a sibling folder's entry seen through the bare-prefix match. The "/" listing of the folder
	// itself tells them apart: a loose folder always gives "." and "..", a pk3 folder its own
	// subfolders, and the made-up one nothing (it is not a prefix of anything). More than
	// RP_LIST_MAX_DIR_CHECKS of them are kept unchecked rather than dropped.
	for ( d = 0, checks = 0; d < numDirs; d++ ) {
		rpListFolder_t *f = &rp_folders[d];
		char subPath[MAX_QPATH * 3];

		if ( f->state != 0 || f->files > 0 ) {
			continue;
		}
		if ( checks >= RP_LIST_MAX_DIR_CHECKS ) {
			f->state = 1;
			continue;
		}
		checks++;
		f->state = -1;
		RP_ListJoin( subPath, sizeof( subPath ), path, f->name, NULL );
		if ( strlen( subPath ) >= MAX_QPATH ) {
			continue;
		}
		n = RP_ListGetFiles( subPath, "/", NULL );
		for ( i = 0, p = rp_listRaw; i < n; i++, p += strlen( p ) + 1 ) {
			if ( !strcmp( p, "." ) || !strcmp( p, ".." ) || ( p[0] && p[0] != '/' && p[0] != '\\' ) ) {
				f->state = 1;
				break;
			}
		}
	}

	if ( src->flatten ) {
		// loose subfolders only list at their own level, so each one the engine reported is listed on its own
		// (a pk3's again as well, which the de-duplication below takes care of)
		for ( d = 0; d < numDirs && d < RP_LIST_MAX_MAP_DIRS; d++ ) {
			char sub[MAX_QPATH];
			char subPath[MAX_QPATH * 3];
			// a too-long name of a folder the pk3 listing above already went through was counted there
			qboolean countTooLong = ( rp_folders[d].files == 0 ) ? qtrue : qfalse;

			if ( rp_folders[d].state != 1 ) {
				continue;
			}
			Q_strncpyz( sub, rp_folders[d].name, sizeof( sub ) );
			RP_ListJoin( subPath, sizeof( subPath ), path, sub, NULL );
			if ( strlen( subPath ) >= MAX_QPATH ) {
				continue;
			}
			for ( e = 0; e < 2 && src->exts[e]; e++ ) {
				n = RP_ListGetFiles( subPath, src->exts[e], set );
				for ( i = 0, p = rp_listRaw; i < n; i++, p += strlen( p ) + 1 ) {
					if ( !p[0] || strchr( p, '/' ) || strchr( p, '\\' ) ) {
						continue;
					}
					RP_ListAddFile( set, src, folder, sub, p, countTooLong );
				}
			}
		}
		if ( numDirs > RP_LIST_MAX_MAP_DIRS ) {
			set->truncated = qtrue;
		}
	} else {
		for ( d = 0; d < rp_numFolders; d++ ) {
			char rel[MAX_QPATH * 2];
			char full[MAX_QPATH * 3];

			if ( rp_folders[d].state != 1 ) {
				continue;
			}
			RP_ListJoin( rel, sizeof( rel ), folder, rp_folders[d].name, NULL );
			RP_ListJoin( full, sizeof( full ), src->root, rel, NULL );
			// room left for at least "/x" inside it, or nothing in it could be loaded
			if ( strlen( full ) + 2 >= MAX_QPATH ) {
				set->tooLong++;
				continue;
			}
			RP_ListAdd( set, rel, qtrue );
		}
	}

	RP_ListSortUnique( set );
	return qtrue;
}

static void RP_ListFiles( gentity_t *ent, const rpListSource_t *src, const char *folderArg, int page ) {
	rpListSet_t *set = &rp_files;
	char folder[MAX_QPATH];
	char path[MAX_QPATH * 2];
	char title[MAX_QPATH * 3];
	char nextCmd[MAX_QPATH * 3];
	const char *error = NULL;

	if ( !RP_ListCollectFiles( src, folderArg, qtrue, folder, sizeof( folder ), path, sizeof( path ), &error ) ) {
		trap->SendServerCommand( ent - g_entities, va( "print \"^7%s\n\"", error ) );
		return;
	}

	Com_sprintf( title, sizeof( title ), "%s in %s", src->title, path );
	if ( folder[0] ) {
		Com_sprintf( nextCmd, sizeof( nextCmd ), "/list %s %s", src->what, folder );
	} else if ( src->defaultFolder[0] ) {
		Com_sprintf( nextCmd, sizeof( nextCmd ), "/list %s /", src->what );
	} else {
		Com_sprintf( nextCmd, sizeof( nextCmd ), "/list %s", src->what );
	}

	if ( set->num == 0 ) {
		trap->SendServerCommand( ent - g_entities, va( "print \"^7No %s found in %s.\n%s%s\"",
			src->what, path,
			set->tooLong ? va( "^7%d with a path too long for the game to load %s left out.\n", set->tooLong, set->tooLong == 1 ? "was" : "were" ) : "",
			set->truncated ? "^1The listing was cut short by the engine's limits.\n" : "" ) );
		return;
	}

	RP_ListPrintPage( ent, set->entries, set->num, page, title, nextCmd, src->useHint,
		src->flatten ? NULL : src->what, set->truncated, set->tooLong );
}

/*
===========================================================================
NPC and vehicle types
===========================================================================
*/

extern char NPCParms[];
extern char VehicleParms[];

/*
==================
RP_ListBlockIsVehicle

Whether the block p starts at ("{ ... }", just after its name) says "class CLASS_VEHICLE" at its own
level, as NPC_ParseParms() reads it: the NPC entry of a vehicle, which /npc spawn refuses ("Tried to
spawn a vehicle NPC ... without using ... 'NPC spawn vehicle <vehiclename>'") -- every vehicle has
one, and /npc spawn vehicle takes the vehicle's own (.veh) name instead.
==================
*/
static qboolean RP_ListBlockIsVehicle( const char *p ) {
	int depth = 0;

	while ( p ) {
		const char *token = COM_ParseExt( &p, qtrue );

		if ( !token[0] ) {
			break;
		}
		if ( !strcmp( token, "{" ) ) {
			depth++;
			continue;
		}
		if ( !strcmp( token, "}" ) ) {
			if ( --depth <= 0 ) {
				break;
			}
			continue;
		}
		if ( depth == 1 && !Q_stricmp( token, "class" ) ) {
			token = COM_ParseExt( &p, qfalse );	// COM_ParseString(), as NPC_ParseParms() reads the value
			if ( !Q_stricmp( token, "CLASS_VEHICLE" ) ) {
				return qtrue;
			}
		}
	}
	return qfalse;
}

// the top-level "<name> { ... }" blocks of an NPC or vehicle text, as NPC_Precache() and
// VEH_LoadVehicle() look them up: a name, then SkipBracedSection() over what follows it. For the NPC
// types (skipVehicles), the entries of vehicles are left out: /npc spawn cannot spawn them (see
// RP_ListBlockIsVehicle()), and /list vehicles lists the vehicles. The first block of a name is the
// one the game uses, so it is the one that decides.
static void RP_ListBuildTypes( rpListSet_t *set, const char *text, qboolean skipVehicles ) {
	const char *p = text;
	char name[MAX_QPATH];
	int i, kept;

	RP_ListReset( set );
	COM_BeginParseSession( "RP_ListTypes" );

	while ( p ) {
		const char *peek;
		const char *token = COM_ParseExt( &p, qtrue );
		qboolean block;

		if ( !token[0] ) {
			break;
		}
		if ( strlen( token ) < sizeof( name ) ) {
			Q_strncpyz( name, token, sizeof( name ) );
		} else {
			name[0] = '\0';
		}

		peek = p;
		token = COM_ParseExt( &peek, qtrue );
		block = !strcmp( token, "{" ) ? qtrue : qfalse;

		if ( block && name[0] && strcmp( name, "{" ) && strcmp( name, "}" ) ) {
			int before = set->num;
			qboolean vehicle = skipVehicles ? RP_ListBlockIsVehicle( p ) : qfalse;

			RP_ListAdd( set, name, qfalse );
			if ( set->num > before ) {
				set->entries[set->num - 1].vehicle = vehicle;
			}
		}

		SkipBracedSection( &p, 0 );
	}

	RP_ListSortUnique( set );

	for ( i = 0, kept = 0; i < set->num; i++ ) {
		if ( !set->entries[i].vehicle ) {
			set->entries[kept++] = set->entries[i];
		}
	}
	set->num = kept;
}

/*
==================
RP_NpcTypeIsVehicle

Whether /npc spawn <type> would find a vehicle's NPC entry for this type, looking it up the way
NPC_ParseParms() does -- for /npc spawn to say so, instead of the refusal only the server console sees.
==================
*/
qboolean RP_NpcTypeIsVehicle( const char *type ) {
	const char *p = NPCParms;

	if ( !VALIDSTRING( type ) ) {
		return qfalse;
	}
	COM_BeginParseSession( "RP_NpcTypeIsVehicle" );
	while ( p ) {
		const char *token = COM_ParseExt( &p, qtrue );

		if ( !token[0] ) {
			break;
		}
		if ( !Q_stricmp( token, type ) ) {
			return RP_ListBlockIsVehicle( p );
		}
		SkipBracedSection( &p, 0 );
	}
	return qfalse;
}

/*
==================
RP_NpcTypeKnown / RP_VehicleTypeKnown

GalaxyRP: [Entity System] whether the NPC types (NPCParms) or the vehicle types (VehicleParms) have a
block of this name, looked up the way NPC_ParseParms() and VEH_LoadVehicle() look one up: a top-level
name followed by "{". Only the text is read -- nothing is loaded -- so /entadd can refuse a spawner
whose type would make it fail later, when nobody is told why.
==================
*/
static qboolean RP_TypeBlockKnown( const char *text, const char *type ) {
	const char *p = text;

	if ( !VALIDSTRING( type ) ) {
		return qfalse;
	}
	COM_BeginParseSession( "RP_TypeBlockKnown" );
	while ( p ) {
		const char *token = COM_ParseExt( &p, qtrue );

		if ( !token[0] ) {
			break;
		}
		if ( !Q_stricmp( token, type ) ) {
			const char *peek = p;

			token = COM_ParseExt( &peek, qtrue );
			return !strcmp( token, "{" ) ? qtrue : qfalse;
		}
		SkipBracedSection( &p, 0 );
	}
	return qfalse;
}

qboolean RP_NpcTypeKnown( const char *type ) {
	return RP_TypeBlockKnown( NPCParms, type );
}

qboolean RP_VehicleTypeKnown( const char *type ) {
	return RP_TypeBlockKnown( VehicleParms, type );
}


// the NPC or vehicle types, read the first time they are asked for on this map
static rpListSet_t *RP_ListTypeSet( qboolean vehicles ) {
	rpListSet_t *set = vehicles ? &rp_vehicles : &rp_npcs;

	if ( vehicles ) {
		if ( !level.rp_list_vehicles_ready ) {
			RP_ListBuildTypes( set, VehicleParms, qfalse );
			level.rp_list_vehicles_ready = qtrue;
		}
	} else if ( !level.rp_list_npcs_ready ) {
		RP_ListBuildTypes( set, NPCParms, qtrue );
		level.rp_list_npcs_ready = qtrue;
	}
	return set;
}

static void RP_ListTypes( gentity_t *ent, qboolean vehicles, const char *text, int page ) {
	rpListSet_t *set = RP_ListTypeSet( vehicles );
	char title[MAX_QPATH * 2];
	char nextCmd[MAX_QPATH * 2];
	const char *what = vehicles ? "vehicles" : "npcs";
	int i, num = 0;

	if ( text && ( strlen( text ) >= MAX_QPATH || !RP_ListNameOk( text ) ) ) {
		trap->SendServerCommand( ent - g_entities, "print \"^7That is not a name that can be searched for.\n\"" );
		return;
	}

	for ( i = 0; i < set->num; i++ ) {
		if ( !text || RP_ListContains( set->entries[i].name, text ) ) {
			rp_viewEntries[num++] = set->entries[i];
		}
	}

	if ( text ) {
		Com_sprintf( title, sizeof( title ), "%s types with '%s'", vehicles ? "Vehicle" : "NPC", text );
		Com_sprintf( nextCmd, sizeof( nextCmd ), "/list %s %s", what, text );
	} else {
		Com_sprintf( title, sizeof( title ), "%s types", vehicles ? "Vehicle" : "NPC" );
		Com_sprintf( nextCmd, sizeof( nextCmd ), "/list %s", what );
	}

	if ( num == 0 ) {
		trap->SendServerCommand( ent - g_entities, va( "print \"^7No %s found.\n\"", title ) );
		return;
	}

	RP_ListPrintPage( ent, rp_viewEntries, num, page, title, nextCmd,
		vehicles ? "^7Spawn one with ^3/npc spawn vehicle <type>^7.\n" : "^7Spawn one with ^3/npc spawn <type>^7.\n",
		NULL, set->truncated, 0 );
}

/*
===========================================================================
Commands
===========================================================================
*/

/*
==================
RP_ListCommand

/list models|effects|sounds|music|maps|npcs|vehicles [folder or text] [page], from Cmd_ListAccount_f
(logged-in players). qfalse when "what" is none of these, so the caller goes on to its own words.
A lone number after the word is the page.
==================
*/
qboolean RP_ListCommand( gentity_t *ent, const char *what ) {
	const rpListSource_t *src = NULL;
	qboolean types = qfalse, vehicles = qfalse;
	char a2[MAX_STRING_CHARS] = "", a3[MAX_STRING_CHARS] = "";
	const char *text = NULL;
	const char *usage;
	int argc = trap->Argc();
	int page = 1;
	int i;

	for ( i = 0; i < (int)ARRAY_LEN( rp_listSources ); i++ ) {
		if ( !Q_stricmp( what, rp_listSources[i].what ) || !Q_stricmp( what, rp_listSources[i].single ) ) {
			src = &rp_listSources[i];
			break;
		}
	}
	if ( !src ) {
		if ( !Q_stricmp( what, "npcs" ) || !Q_stricmp( what, "npc" ) ) {
			types = qtrue;
		} else if ( !Q_stricmp( what, "vehicles" ) || !Q_stricmp( what, "vehicle" ) ) {
			types = qtrue;
			vehicles = qtrue;
		} else {
			return qfalse;
		}
	}

	if ( types ) {
		usage = va( "print \"^7Usage: ^3/list %s <text (optional)> <page (optional)>\n\"", vehicles ? "vehicles" : "npcs" );
	} else if ( src->flatten ) {
		usage = va( "print \"^7Usage: ^3/list %s <page (optional)>\n\"", src->what );
	} else {
		usage = va( "print \"^7Usage: ^3/list %s <folder (optional)> <page (optional)>\n\"", src->what );
	}

	if ( argc > 4 ) {
		trap->SendServerCommand( ent - g_entities, usage );
		return qtrue;
	}
	if ( argc >= 3 ) {
		trap->Argv( 2, a2, sizeof( a2 ) );
	}
	if ( argc == 4 ) {
		trap->Argv( 3, a3, sizeof( a3 ) );
		page = RP_ListParsePage( a3 );
		if ( page <= 0 ) {
			trap->SendServerCommand( ent - g_entities, "print \"^7Invalid page number.\n\"" );
			return qtrue;
		}
		text = a2;
	} else if ( argc == 3 ) {
		if ( RP_ListIsNumber( a2 ) ) {
			page = RP_ListParsePage( a2 );
			if ( page <= 0 ) {
				trap->SendServerCommand( ent - g_entities, "print \"^7Invalid page number.\n\"" );
				return qtrue;
			}
		} else {
			text = a2;
		}
	}
	if ( text && !text[0] ) {
		text = NULL;
	}

	if ( text && src && src->flatten ) {
		trap->SendServerCommand( ent - g_entities, usage );
		return qtrue;
	}

	if ( !types ) {
		if ( level.rp_list_fs_next_time > level.time + RP_LIST_FS_GAP ) {
			level.rp_list_fs_next_time = 0;	// only a guard, as in RP_ListCooldown()
		}
		if ( level.time < level.rp_list_fs_next_time ) {
			trap->SendServerCommand( ent - g_entities, "print \"^7The server is busy with another listing. Try again in a moment.\n\"" );
			return qtrue;
		}
	}

	if ( RP_ListCooldown( ent ) ) {
		return qtrue;
	}

	if ( !types ) {
		level.rp_list_fs_next_time = level.time + RP_LIST_FS_GAP;
	}

	if ( types ) {
		RP_ListTypes( ent, vehicles, text, page );
	} else {
		RP_ListFiles( ent, src, text, page );
	}
	return qtrue;
}

/*
==================
RP_ListVotableMaps

/maplist, and /callvote map with no map named: the maps of the arena files that the current
gametype supports -- the same test G_VoteMap() makes -- sorted, each once (a map can be in more than
one .arena file), a page at a time. In FFA every bsp on the server can be voted for, including the
ones with no arena entry; /list maps shows those.
==================
*/
void RP_ListVotableMaps( gentity_t *ent, int page ) {
	rpListSet_t *set = &rp_files;
	char map[MAX_QPATH];
	int i;

	if ( RP_ListCooldown( ent ) ) {
		return;
	}

	RP_ListReset( set );
	for ( i = 0; i < level.arenas.num; i++ ) {
		Q_strncpyz( map, Info_ValueForKey( level.arenas.infos[i], "map" ), sizeof( map ) );
		Q_StripColor( map );
		if ( map[0] && G_DoesMapSupportGametype( map, level.gametype ) ) {
			RP_ListAdd( set, map, qfalse );
		}
	}
	RP_ListSortUnique( set );

	if ( set->num == 0 ) {
		trap->SendServerCommand( ent - g_entities, "print \"^7No map in the arena files supports the current gametype.\n\"" );
		return;
	}

	RP_ListPrintPage( ent, set->entries, set->num, page, "Maps you can vote for", "/maplist",
		"^7Vote for one with ^3/callvote map <name>^7.\n", NULL, set->truncated, 0 );
}

/*
===========================================================================
Extras menus

/rpxlist <kind> <request id> <offset> <folder>: the same listings as /list, for the Extras menus of
the Galaxy RP menu (cg_rpextras.c in cgame asks and collects, the UI shows them). Not typed by
players. kind is models, effects, sounds, music, npcs or vehicles; the folder is relative to the
kind's root ("." for the root itself) and ignored for npcs and vehicles.

The answer is one or more

	rpxl <kind> <request id> <total> <offset> <flags> "<name>|<name>|<folder>/|..."

-- the names of one folder, from offset on, only the last part of each (a folder ends in "/"), in
pieces of at most RPX_CHUNK_CHARS characters and never more than RPX_MAX_CHUNKS of them for one
request. The flags are "c" when another piece of this answer follows, "m" when this is the last
piece of this answer but not of the list (the client asks again from where it got to), "e" at the
end of the list, and a "t" after it when the listing was cut short by the engine's limits. Or

	rpxl <kind> <request id> busy			(ask again in a moment)
	rpxl <kind> <request id> err "<why>"

The client asks for the next part only once a piece flagged "m" arrives, so however long the list,
at most RPX_MAX_CHUNKS of these are waiting to go to one client at a time, and sv_floodProtect
spaces its requests. A listing is built once and kept (RPX_CACHE_SLOTS of them, least recently asked
for goes first) until the map changes, so asking for the rest of it, or for a folder another player
just opened, costs nothing; building one has the same limits as /list, RP_LIST_FS_GAP between any two
on the server, and RPX_BUILD_COOLDOWN between two from one player.

Names that could not be used safely are left out: besides what RP_ListNameOk() refuses, "|" (the
separator), ";" (ends a console command), "%" (the network turns it into ".") and, for NPC and
vehicle types, "/". The folder is always relative to the kind's root, so, unlike /list, a first part
named like the root is not dropped (a folder sound/sound).
===========================================================================
*/

#define RPX_CACHE_SLOTS		4
#define RPX_CACHE_DATA		0x40000		// 4096 names of up to 63 characters, and their ends
#define RPX_CHUNK_CHARS		900			// names in one answer, well inside SV_SendServerCommand's 1022
#define RPX_MAX_CHUNKS		4
#define RPX_BUILD_COOLDOWN	500

typedef struct {
	const char	*name;
	int			source;		// index into rp_listSources, -1 for NPC types, -2 for vehicle types
} rpxKind_t;

static const rpxKind_t rpx_kinds[] = {
	{ "models",		0 },
	{ "effects",	1 },
	{ "sounds",		2 },
	{ "music",		3 },
	{ "npcs",		-1 },
	{ "vehicles",	-2 },
};

typedef struct {
	qboolean	used;
	int			kind;
	char		folder[MAX_QPATH];	// cleaned, relative to the kind's root; "" for the root and for types
	int			lastUse;
	int			num;
	qboolean	truncated;
	int			dataUsed;
	int			offs[RP_LIST_MAX_ENTRIES];
	char		data[RPX_CACHE_DATA];
} rpxCacheSlot_t;

static rpxCacheSlot_t	rpx_cache[RPX_CACHE_SLOTS];
static int				rpx_useCounter;

static qboolean RPX_NameOk( const char *name ) {
	if ( !RP_ListNameOk( name ) || strlen( name ) >= MAX_QPATH ) {
		return qfalse;
	}
	// '%': a server command carries it as '.' (MSG_ReadString()), so the name would come back wrong
	return ( strchr( name, '|' ) || strchr( name, ';' ) || strchr( name, '%' ) ) ? qfalse : qtrue;
}

static void RPX_Reply( gentity_t *ent, const char *kind, const char *req, const char *rest ) {
	trap->SendServerCommand( ent - g_entities, va( "rpxl %s %s %s", kind, req, rest ) );
}

static rpxCacheSlot_t *RPX_FindSlot( int kind, const char *folder ) {
	int i;

	for ( i = 0; i < RPX_CACHE_SLOTS; i++ ) {
		if ( rpx_cache[i].used && rpx_cache[i].kind == kind && !Q_stricmp( rpx_cache[i].folder, folder ) ) {
			return &rpx_cache[i];
		}
	}
	return NULL;
}

// a free slot, or the one asked for least recently
static rpxCacheSlot_t *RPX_TakeSlot( void ) {
	rpxCacheSlot_t *best = &rpx_cache[0];
	int i;

	for ( i = 0; i < RPX_CACHE_SLOTS; i++ ) {
		if ( !rpx_cache[i].used ) {
			return &rpx_cache[i];
		}
		if ( rpx_cache[i].lastUse < best->lastUse ) {
			best = &rpx_cache[i];
		}
	}
	return best;
}

// the names of set, as the menus are sent them, into a slot
static rpxCacheSlot_t *RPX_Pack( int kind, const char *folder, const rpListSet_t *set, qboolean types ) {
	rpxCacheSlot_t *slot = RPX_TakeSlot();
	int i;

	slot->used = qtrue;
	slot->kind = kind;
	Q_strncpyz( slot->folder, folder, sizeof( slot->folder ) );
	slot->num = 0;
	slot->dataUsed = 0;
	slot->truncated = set->truncated;

	for ( i = 0; i < set->num; i++ ) {
		const char *name = set->entries[i].name;
		char leaf[MAX_QPATH + 1];
		int len;

		if ( types ) {
			if ( strchr( name, '/' ) || !RPX_NameOk( name ) ) {
				continue;
			}
			Q_strncpyz( leaf, name, sizeof( leaf ) );
		} else {
			const char *slash = strrchr( name, '/' );

			if ( slash ) {
				name = slash + 1;
			}
			if ( !RPX_NameOk( name ) ) {
				continue;
			}
			Com_sprintf( leaf, sizeof( leaf ), "%s%s", name, set->entries[i].folder ? "/" : "" );
		}

		len = strlen( leaf ) + 1;
		if ( slot->num >= RP_LIST_MAX_ENTRIES || slot->dataUsed + len > (int)sizeof( slot->data ) ) {
			slot->truncated = qtrue;
			break;
		}
		memcpy( slot->data + slot->dataUsed, leaf, len );
		slot->offs[slot->num++] = slot->dataUsed;
		slot->dataUsed += len;
	}
	return slot;
}

// up to RPX_MAX_CHUNKS pieces of the names from offset on (an offset past the end: the end, empty)
static void RPX_SendNames( gentity_t *ent, const char *kind, const char *req, const rpxCacheSlot_t *slot, int offset ) {
	char buf[RPX_CHUNK_CHARS + 1];
	int i = offset, chunks = 0;

	if ( i > slot->num ) {
		i = slot->num;
	}

	do {
		int len = 0, start = i;
		char flag;

		buf[0] = '\0';
		while ( i < slot->num ) {
			const char *name = slot->data + slot->offs[i];
			int nl = strlen( name );

			if ( len + ( len ? 1 : 0 ) + nl > RPX_CHUNK_CHARS ) {
				break;
			}
			if ( len ) {
				buf[len++] = '|';
			}
			memcpy( buf + len, name, nl );
			len += nl;
			buf[len] = '\0';
			i++;
		}
		chunks++;

		if ( i >= slot->num ) {
			flag = 'e';
		} else if ( chunks >= RPX_MAX_CHUNKS ) {
			flag = 'm';
		} else {
			flag = 'c';
		}
		RPX_Reply( ent, kind, req, va( "%d %d %c%s \"%s\"", slot->num, start, flag, slot->truncated ? "t" : "", buf ) );
	} while ( i < slot->num && chunks < RPX_MAX_CHUNKS );
}

static qboolean RPX_IsNumber( const char *s, int maxLen ) {
	return ( RP_ListIsNumber( s ) && (int)strlen( s ) <= maxLen ) ? qtrue : qfalse;
}

/*
==================
RP_ListDataCommand

/rpxlist, from the Extras menus -- see above. Logged-in players only, like /list.
==================
*/
void RP_ListDataCommand( gentity_t *ent ) {
	char kindArg[MAX_STRING_CHARS], req[MAX_STRING_CHARS], offArg[MAX_STRING_CHARS], folderArg[MAX_STRING_CHARS];
	char folder[MAX_QPATH];
	const rpxKind_t *kind = NULL;
	rpxCacheSlot_t *slot;
	int argc = trap->Argc();
	int k, offset;

	if ( argc < 4 || argc > 5 ) {
		trap->SendServerCommand( ent - g_entities, "print \"^7This command is used by the Extras menus of the Galaxy RP menu.\n\"" );
		return;
	}
	trap->Argv( 1, kindArg, sizeof( kindArg ) );
	trap->Argv( 2, req, sizeof( req ) );
	trap->Argv( 3, offArg, sizeof( offArg ) );
	folderArg[0] = '\0';
	if ( argc == 5 ) {
		trap->Argv( 4, folderArg, sizeof( folderArg ) );
	}

	for ( k = 0; k < (int)ARRAY_LEN( rpx_kinds ); k++ ) {
		if ( !Q_stricmp( kindArg, rpx_kinds[k].name ) ) {
			kind = &rpx_kinds[k];
			break;
		}
	}
	// nothing to answer to: the reply names the kind and the request, and neither can be trusted
	if ( !kind || !RPX_IsNumber( req, 9 ) || !RPX_IsNumber( offArg, 6 ) ) {
		trap->SendServerCommand( ent - g_entities, "print \"^7This command is used by the Extras menus of the Galaxy RP menu.\n\"" );
		return;
	}
	offset = atoi( offArg );

	if ( ent->client->sess.amrpgmode != 2 ) {
		RPX_Reply( ent, kind->name, req, "err \"Log in to use the Extras menus.\"" );
		return;
	}

	if ( !level.rp_rpx_cache_ready ) {
		// the files and the types are the same all game, but a map is when the server owner can have
		// changed a loose folder, and the NPC and vehicle types are read again then too
		memset( rpx_cache, 0, sizeof( rpx_cache ) );
		rpx_useCounter = 0;
		level.rp_rpx_cache_ready = qtrue;
	}

	folder[0] = '\0';
	if ( kind->source >= 0 ) {
		const rpListSource_t *src = &rp_listSources[kind->source];

		if ( !RP_ListCleanFolder( src->root, folderArg, folder, sizeof( folder ), qfalse ) ) {
			RPX_Reply( ent, kind->name, req, "err \"That is not a folder name that can be listed.\"" );
			return;
		}
	}

	slot = RPX_FindSlot( k, folder );
	if ( !slot ) {
		gclient_t *cl = ent->client;

		if ( kind->source >= 0 ) {
			if ( level.rp_list_fs_next_time > level.time + RP_LIST_FS_GAP ) {
				level.rp_list_fs_next_time = 0;	// only a guard, as in RP_ListCooldown()
			}
			if ( level.time < level.rp_list_fs_next_time ) {
				RPX_Reply( ent, kind->name, req, "busy" );
				return;
			}
		}
		if ( cl->pers.rpxListNextTime > level.time + RPX_BUILD_COOLDOWN ) {
			cl->pers.rpxListNextTime = 0;
		}
		if ( level.time < cl->pers.rpxListNextTime ) {
			RPX_Reply( ent, kind->name, req, "busy" );
			return;
		}
		cl->pers.rpxListNextTime = level.time + RPX_BUILD_COOLDOWN;

		if ( kind->source >= 0 ) {
			const rpListSource_t *src = &rp_listSources[kind->source];
			char cleaned[MAX_QPATH];
			char path[MAX_QPATH * 2];
			const char *error = NULL;

			level.rp_list_fs_next_time = level.time + RP_LIST_FS_GAP;
			if ( !RP_ListCollectFiles( src, folderArg, qfalse, cleaned, sizeof( cleaned ), path, sizeof( path ), &error ) ) {
				RPX_Reply( ent, kind->name, req, va( "err \"%s\"", error ) );
				return;
			}
			slot = RPX_Pack( k, folder, &rp_files, qfalse );
		} else {
			slot = RPX_Pack( k, "", RP_ListTypeSet( kind->source == -2 ? qtrue : qfalse ), qtrue );
		}
	}

	slot->lastUse = ++rpx_useCounter;
	RPX_SendNames( ent, kind->name, req, slot, offset );
}

/*
===========================================================================
File checks
===========================================================================
*/

// the file as typed, or in lower case: a pk3 finds either, a loose file on a Linux server only its
// own spelling, and the game client asks for sounds in lower case
static qboolean RP_ListFileExistsAnyCase( const char *path ) {
	char low[MAX_QPATH];

	if ( RP_FileExists( path ) ) {
		return qtrue;
	}
	Q_strncpyz( low, path, sizeof( low ) );
	Q_strlwr( low );
	return ( strcmp( low, path ) && RP_FileExists( low ) ) ? qtrue : qfalse;
}

/*
==================
RP_SoundFileExists

Whether a client can load this sound, looked for the way the client does (S_FindName() and
S_LoadSound() in the engine): the extension is dropped, then "name.wav" is tried and "name.mp3"
after it. A name of MAX_QPATH or more is refused, as the client refuses it.

Names starting with "*" are not files: they are the player model's own sounds ("*jump1.wav",
"*taunt"), which each client picks per model, so they are allowed when they look like one -- a
letter, then letters, digits, "_" and at most one "." for the extension. The client handles every
other "*" name as something else: "*$<name>" is an NPC sound set, which CG_PrecacheNPCSounds()
copies into a MAX_QPATH buffer without a length check for every client when the configstring
arrives -- registered from here, a long one crashed everyone on the server.
==================
*/
qboolean RP_SoundFileExists( const char *name ) {
	char base[MAX_QPATH];
	char path[MAX_QPATH];

	if ( !VALIDSTRING( name ) || strlen( name ) >= MAX_QPATH ) {
		return qfalse;
	}
	if ( name[0] == '*' ) {
		const char *c = name + 1;
		int dots = 0;

		if ( !( ( *c >= 'a' && *c <= 'z' ) || ( *c >= 'A' && *c <= 'Z' ) ) ) {
			return qfalse;
		}
		for ( ; *c; c++ ) {
			if ( *c == '.' ) {
				dots++;
			} else if ( !( ( *c >= 'a' && *c <= 'z' ) || ( *c >= 'A' && *c <= 'Z' ) || ( *c >= '0' && *c <= '9' ) || *c == '_' ) ) {
				return qfalse;
			}
		}
		return ( dots <= 1 ) ? qtrue : qfalse;
	}

	COM_StripExtension( name, base, sizeof( base ) );
	if ( !base[0] || strlen( base ) + 4 >= MAX_QPATH ) {
		return qfalse;
	}

	Com_sprintf( path, sizeof( path ), "%s.wav", base );
	if ( RP_ListFileExistsAnyCase( path ) ) {
		return qtrue;
	}
	Com_sprintf( path, sizeof( path ), "%s.mp3", base );
	return RP_ListFileExistsAnyCase( path );
}

/*
==================
RP_MusicFileExists

Whether a client will play this music, looked for the way S_StartBackgroundTrack() does: the first
word of the CS_MUSIC text is the track (a second one is the loop, which falls back to the first when
it is missing), ".mp3" is added when it has no extension, and it is only looked for as a file when it
has a "/" in it -- otherwise the client treats it as a dynamic music set, which multiplayer maps do
not use; those names are refused too.
==================
*/
qboolean RP_MusicFileExists( const char *music ) {
	char copy[MAX_STRING_CHARS];
	char path[MAX_QPATH];
	const char *p = copy;
	const char *token;

	if ( !VALIDSTRING( music ) ) {
		return qfalse;
	}

	Q_strncpyz( copy, music, sizeof( copy ) );
	COM_BeginParseSession( "RP_MusicFileExists" );
	token = COM_Parse( &p );
	if ( !token[0] || strlen( token ) >= MAX_QPATH || !strchr( token, '/' ) ) {
		return qfalse;
	}

	// the client's own MAX_QPATH copy, extension added the same way (not at all when it does not fit)
	Q_strncpyz( path, token, sizeof( path ) );
	COM_DefaultExtension( path, sizeof( path ), ".mp3" );
	return RP_ListFileExistsAnyCase( path );
}
