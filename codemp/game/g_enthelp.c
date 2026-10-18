/*
===========================================================================
DAJ_RP: [Entity Help] /enthelp -- what an entity class does, its keys and spawnflags, with an example.

  /enthelp                        the class of the entity aimed at (picked as /entedit picks it)
  /enthelp <entity id>            the class of that entity, either region
  /enthelp <classname>            that class (case does not matter); also an item classname or a topic
  /enthelp <part of a name>       the classes and topics whose name holds it; one alone is shown
  /enthelp <classname> <key>      that key's line, or a spawnflag's (by value or name); "spawnflags"
                                  shows all of them
  /enthelp list [page]            every class, grouped, in two pages

The text is compiled in (g_enthelp_data.h): one entry per spawn-table class, per topic (common, model,
npc, items, ...), and, where an item has something of its own to say, per item classname -- any other
item gets the item table's facts and the "items" topic. The heading of every class says what the code
knows by itself: networked or logical (the spawn table), whether the Entity System can place it, and
what a map's own is tagged in /entlist -- or, for a class flagged RP_EH_REMOVED, that it removes itself as it
spawns, so it holds no slot and /entlist never shows it (those are listed in a group of their own, "Map
file only"). For the Entity System's admins only, as the other /ent commands.

Every string of the data goes out inside a print command, so none of it may hold a double quote; the
lines are packed into prints of at most RP_EH_PRINT_MAX characters, a long one split at a space.
===========================================================================
*/

#include "g_local.h"

extern qboolean check_admin_command( gentity_t *ent, int admin_command, qboolean with_message );

#define RP_EH_TOPIC		1	// not a class: shared text the classes point to ("Also")
#define RP_EH_MAPONLY	2	// not for the Entity System: it cannot place it, or it does nothing placed (note says why)
#define RP_EH_GAMETYPE	4	// it only stays in some gametypes (note says which): it removes itself in the others
#define RP_EH_REMOVED	8	// it removes itself as it spawns, in every gametype DAJ_RP runs and whatever its keys: it
							// is only ever in a map file (the compiler's, the clients', or data read at load)

typedef struct {
	const char	*name;			// the classname, as the spawn table or the item table spells it, or a topic
	int			flags;			// RP_EH_*
	const char	*note;			// RP_EH_MAPONLY: why it cannot be placed; RP_EH_GAMETYPE: which gametypes
	const char	*text;			// what it is and does
	const char	*spawnflags;	// lines "value|NAME|meaning", '\n' between them; NULL for none
	const char	*keys;			// lines "key|default|meaning" ("" default: none); NULL for none
	const char	*example;		// what follows /entadd; NULL for none
	const char	*also;			// classes and topics to read too, space-separated; NULL for none
} rpEntHelp_t;

#include "g_enthelp_data.h"

#define RP_EH_PRINT_MAX		1000	// the text of one print: with "print \"" and the closing quote, 1008
#define RP_EH_MATCHES_SHOWN	60		// names listed for a part of a name before "... and N more"
#define RP_EH_LIST_PAGES	2

/*
==================
Output: lines packed into prints
==================
*/
typedef struct {
	int		client;
	char	text[RP_EH_PRINT_MAX + 1];
	int		prints;
} rpEhOut_t;

static void RP_EhFlush( rpEhOut_t *o )
{
	if ( !o->text[0] )
		return;

	trap->SendServerCommand( o->client, va( "print \"%s\"", o->text ) );
	o->text[0] = '\0';
	o->prints++;
}

// a piece of text, split at a space (or anywhere, failing one) when it would not fit a print by itself
static void RP_EhAdd( rpEhOut_t *o, const char *s )
{
	while ( s && *s )
	{
		int room = RP_EH_PRINT_MAX - (int)strlen( o->text );
		int len = (int)strlen( s );
		int take, k;

		if ( len <= room )
		{
			Q_strcat( o->text, sizeof( o->text ), s );
			return;
		}

		if ( o->text[0] && len <= RP_EH_PRINT_MAX )
		{
			RP_EhFlush( o );
			continue;
		}

		// longer than a print: as much as fits, ended at a space, and the rest goes on in the next
		take = room;
		for ( k = take; k > room / 2; k-- )
		{
			if ( s[k] == ' ' )
			{
				take = k + 1;
				break;
			}
		}
		if ( take <= 0 )
		{
			RP_EhFlush( o );
			continue;
		}
		{
			int used = (int)strlen( o->text );

			memcpy( o->text + used, s, take );
			o->text[used + take] = '\0';
		}
		RP_EhFlush( o );
		s += take;
	}
}

// GalaxyRP fix: [Entity Help] a line starts white unless it sets a colour of its own: the console resets the
// colour only at the start of a print, and the lines are packed several to a print, so a line with no colour
// code went on in the colour the line before it ended in -- a list's names in its cyan heading's colour.
// Leading line breaks are kept in front of it.
static void RP_EhLine( rpEhOut_t *o, const char *s )
{
	char line[RP_EH_PRINT_MAX * 4];
	int breaks = 0;

	while ( s[breaks] == '\n' )
		breaks++;

	Com_sprintf( line, sizeof( line ), "%.*s%s%s\n", breaks, s, Q_IsColorString( s + breaks ) ? "" : "^7", s + breaks );
	RP_EhAdd( o, line );
}

// text a player typed, or a map wrote, for a print: no character that would end the print's quotes or
// the command, and StringEd references broken up (RP_ShownText)
static const char *RP_EhSafe( const char *text )
{
	static char copies[4][MAX_QPATH * 2];
	static int next = 0;
	char *copy = copies[next++ & 3];
	int i;

	Q_strncpyz( copy, RP_ShownText( text ? text : "" ), sizeof( copies[0] ) );
	for ( i = 0; copy[i]; i++ )
	{
		if ( copy[i] == '"' || copy[i] == ';' || copy[i] == '\n' || copy[i] == '\r' )
			copy[i] = '?';
	}
	return copy;
}

/*
==================
The data
==================
*/
static const rpEntHelp_t *RP_EhEntry( const char *name )
{
	int i;

	if ( !name || !name[0] )
		return NULL;

	for ( i = 0; i < (int)ARRAY_LEN( rp_ent_help ); i++ )
	{
		if ( !Q_stricmp( rp_ent_help[i].name, name ) )
			return &rp_ent_help[i];
	}

	return NULL;
}

static const gitem_t *RP_EhItem( const char *name )
{
	const gitem_t *item;

	if ( !name || !name[0] )
		return NULL;

	for ( item = bg_itemlist + 1; item->classname; item++ )
	{
		if ( !Q_stricmp( item->classname, name ) )
			return item;
	}

	return NULL;
}

// a name a class, item or topic goes by: what the tables spell, or NULL
static const char *RP_EhKnownName( const char *name, qboolean *isClass, qboolean *isItem )
{
	const rpEntHelp_t *h;
	const gitem_t *item;
	int i, count = RP_SpawnClassCount();

	*isClass = *isItem = qfalse;

	for ( i = 0; i < count; i++ )
	{
		const char *cn = RP_SpawnClassName( i, NULL );

		if ( !Q_stricmp( cn, name ) )
		{
			*isClass = qtrue;
			return cn;
		}
	}

	if ( ( item = RP_EhItem( name ) ) != NULL )
	{
		*isItem = qtrue;
		return item->classname;
	}

	if ( ( h = RP_EhEntry( name ) ) != NULL && ( h->flags & RP_EH_TOPIC ) )
		return h->name;

	return NULL;
}

// the next "a|b|c" line of a block: fields copied into the three buffers; the pointer after it, or NULL
static const char *RP_EhNextLine( const char *p, char *a, int as, char *b, int bs, char *c, int cs )
{
	const char *end, *bar1, *bar2;
	int len;

	if ( !p || !*p )
		return NULL;

	end = strchr( p, '\n' );
	if ( !end )
		end = p + strlen( p );

	a[0] = b[0] = c[0] = '\0';
	bar1 = memchr( p, '|', end - p );
	bar2 = bar1 ? memchr( bar1 + 1, '|', end - bar1 - 1 ) : NULL;

	if ( !bar1 )
	{
		len = (int)( end - p );
		Q_strncpyz( c, p, ( len + 1 < cs ) ? len + 1 : cs );
	}
	else if ( !bar2 )
	{
		len = (int)( bar1 - p );
		Q_strncpyz( a, p, ( len + 1 < as ) ? len + 1 : as );
		len = (int)( end - bar1 - 1 );
		Q_strncpyz( c, bar1 + 1, ( len + 1 < cs ) ? len + 1 : cs );
	}
	else
	{
		len = (int)( bar1 - p );
		Q_strncpyz( a, p, ( len + 1 < as ) ? len + 1 : as );
		len = (int)( bar2 - bar1 - 1 );
		Q_strncpyz( b, bar1 + 1, ( len + 1 < bs ) ? len + 1 : bs );
		len = (int)( end - bar2 - 1 );
		Q_strncpyz( c, bar2 + 1, ( len + 1 < cs ) ? len + 1 : cs );
	}

	return *end ? end + 1 : end;
}

static void RP_EhFlagLine( rpEhOut_t *o, const char *value, const char *name, const char *text )
{
	RP_EhLine( o, va( "^3%s %s^7 - %s", value, name, text ) );
}

static void RP_EhKeyLine( rpEhOut_t *o, const char *key, const char *def, const char *text )
{
	if ( def[0] )
		RP_EhLine( o, va( "^3%s^7 (default %s) - %s", key, def, text ) );
	else
		RP_EhLine( o, va( "^3%s^7 - %s", key, text ) );
}

/*
==================
A class's help
==================
*/
static const char *RP_EhItemGives( const gitem_t *item )
{
	switch ( item->giType )
	{
	case IT_WEAPON:
		return va( "A weapon pickup (%s), with %d ammo.", item->classname, item->quantity );
	case IT_AMMO:
		return va( "An ammo pickup: %d ammo.", item->quantity );
	case IT_ARMOR:
		return va( "A shield pickup: %d shield.", item->quantity );
	case IT_HEALTH:
		return va( "A health pickup: %d health.", item->quantity );
	case IT_POWERUP:
		return "A powerup pickup.";
	case IT_HOLDABLE:
		return "A holdable item: it goes in the inventory, used from it.";
	case IT_TEAM:
		return "A team item (flag): for the team gametypes.";
	default:
		return "A pickup.";
	}
}

static void RP_EhShowAlso( rpEhOut_t *o, const char *also )
{
	char line[512];
	char word[MAX_QPATH];
	const char *p = also;
	int n = 0;

	Q_strncpyz( line, "^5Also: ", sizeof( line ) );
	while ( p && *p )
	{
		int len = 0;

		while ( *p == ' ' )
			p++;
		while ( p[len] && p[len] != ' ' )
			len++;
		if ( !len )
			break;
		Q_strncpyz( word, p, ( len + 1 < (int)sizeof( word ) ) ? len + 1 : (int)sizeof( word ) );
		Q_strcat( line, sizeof( line ), va( "%s^3%s^7", n ? ", " : "", word ) );
		n++;
		p += len;
	}
	if ( n )
	{
		Q_strcat( line, sizeof( line ), " (/enthelp <name>)" );
		RP_EhLine( o, line );
	}
}

static void RP_EhShowClass( rpEhOut_t *o, const char *name )
{
	const rpEntHelp_t *h = RP_EhEntry( name );
	const gitem_t *item = RP_EhItem( name );
	qboolean isClass, isItem, logical = qfalse;
	const char *spelled = RP_EhKnownName( name, &isClass, &isItem );
	char a[MAX_QPATH], b[256], c[RP_EH_PRINT_MAX];
	const char *p;

	if ( isClass )
	{
		int i, count = RP_SpawnClassCount();

		for ( i = 0; i < count; i++ )
		{
			if ( !Q_stricmp( RP_SpawnClassName( i, NULL ), spelled ) )
			{
				RP_SpawnClassName( i, &logical );
				break;
			}
		}
	}

	RP_EhLine( o, va( "\n^2%s^7%s", spelled ? spelled : name, ( h && ( h->flags & RP_EH_TOPIC ) ) ? " (topic)" : "" ) );

	// what the code knows by itself
	if ( isClass || isItem )
	{
		// GalaxyRP fix: [Entity Help] a class that removes itself as it spawns holds no slot of either kind,
		// and is never in /entlist: the spawn table's region and the /entlist tags below do not apply to it
		if ( h && ( h->flags & RP_EH_REMOVED ) )
			RP_EhLine( o, "^1Removed at spawn:^7 it never stays in the world, so it holds no entity slot and /entlist never shows it." );
		else if ( logical )
			RP_EhLine( o, "Logical: not networked, it takes no entity slot -- unless it has a script_targetname or a script key, or nological 1 (/enthelp common)." );
		else
			RP_EhLine( o, "Networked: it takes one of the map's entity slots." );

		if ( h && ( h->flags & RP_EH_MAPONLY ) )
			RP_EhLine( o, va( "^1Not for the Entity System:^7 %s", h->note ? h->note : "map only." ) );
		if ( h && ( h->flags & RP_EH_GAMETYPE ) )
			RP_EhLine( o, va( "^3Only in some gametypes:^7 %s", h->note ? h->note : "it removes itself in the others." ) );

		if ( h && ( h->flags & RP_EH_REMOVED ) )
		{
			// (nothing for /entlist: see above)
		}
		else if ( RP_MapExemptClass( spelled ) )
			RP_EhLine( o, "A map's own is tagged E in /entlist, and can be edited, when nothing in the map links to it; M when something does." );
		else if ( !logical )
			RP_EhLine( o, "A map's own is tagged M in /entlist: left as the map made it (copy it with /entcopy)." );
	}
	if ( item )
		RP_EhLine( o, RP_EhItemGives( item ) );

	if ( !h )
	{
		if ( item )
		{
			RP_EhLine( o, "^5Example" );
			RP_EhLine( o, va( "^3/entadd %s", item->classname ) );
			RP_EhShowAlso( o, "items common" );
		}
		else
		{
			RP_EhLine( o, "No help has been written for this class yet." );
		}
		return;
	}

	if ( h->text && h->text[0] )
		RP_EhLine( o, h->text );

	if ( h->spawnflags && h->spawnflags[0] )
	{
		RP_EhLine( o, "^5Spawnflags" );
		for ( p = h->spawnflags; ( p = RP_EhNextLine( p, a, sizeof( a ), b, sizeof( b ), c, sizeof( c ) ) ) != NULL; )
			RP_EhFlagLine( o, a, b, c );
	}

	if ( h->keys && h->keys[0] )
	{
		RP_EhLine( o, "^5Keys" );
		for ( p = h->keys; ( p = RP_EhNextLine( p, a, sizeof( a ), b, sizeof( b ), c, sizeof( c ) ) ) != NULL; )
			RP_EhKeyLine( o, a, b, c );
	}

	if ( h->example && h->example[0] )
	{
		RP_EhLine( o, "^5Example" );
		RP_EhLine( o, va( "^3/entadd %s", h->example ) );
	}

	if ( h->also && h->also[0] )
		RP_EhShowAlso( o, h->also );
}

// one key's line (or spawnflag's, by value or name), or every spawnflag for "spawnflags"
static void RP_EhShowKey( rpEhOut_t *o, const char *name, const char *key )
{
	const rpEntHelp_t *h = RP_EhEntry( name );
	char a[MAX_QPATH], b[256], c[RP_EH_PRINT_MAX];
	char keys[RP_EH_PRINT_MAX];
	const char *p;
	qboolean found = qfalse;

	if ( !h )
	{
		RP_EhLine( o, va( "No help has been written for %s yet: ^3/enthelp %s^7 shows what is known.", name, name ) );
		return;
	}

	if ( !Q_stricmp( key, "spawnflags" ) )
	{
		if ( !h->spawnflags || !h->spawnflags[0] )
		{
			RP_EhLine( o, va( "%s has no spawnflags of its own.", h->name ) );
			return;
		}
		RP_EhLine( o, va( "^2%s^7 spawnflags", h->name ) );
		for ( p = h->spawnflags; ( p = RP_EhNextLine( p, a, sizeof( a ), b, sizeof( b ), c, sizeof( c ) ) ) != NULL; )
			RP_EhFlagLine( o, a, b, c );
		return;
	}

	for ( p = h->keys; p && ( p = RP_EhNextLine( p, a, sizeof( a ), b, sizeof( b ), c, sizeof( c ) ) ) != NULL; )
	{
		if ( !Q_stricmp( a, key ) )
		{
			RP_EhLine( o, va( "^2%s^7:", h->name ) );
			RP_EhKeyLine( o, a, b, c );
			found = qtrue;
		}
	}
	for ( p = h->spawnflags; p && ( p = RP_EhNextLine( p, a, sizeof( a ), b, sizeof( b ), c, sizeof( c ) ) ) != NULL; )
	{
		if ( !Q_stricmp( a, key ) || !Q_stricmp( b, key ) )
		{
			RP_EhLine( o, va( "^2%s^7 spawnflag:", h->name ) );
			RP_EhFlagLine( o, a, b, c );
			found = qtrue;
		}
	}
	if ( found )
		return;

	// not there: what there is
	keys[0] = '\0';
	for ( p = h->keys; p && ( p = RP_EhNextLine( p, a, sizeof( a ), b, sizeof( b ), c, sizeof( c ) ) ) != NULL; )
		Q_strcat( keys, sizeof( keys ), va( "%s%s", keys[0] ? ", " : "", a ) );
	RP_EhLine( o, va( "%s has no key or spawnflag %s in its help.%s%s", h->name, RP_EhSafe( key ),
		keys[0] ? " Its keys: " : "", keys ) );
	if ( h->also && h->also[0] )
		RP_EhShowAlso( o, h->also );
}

/*
==================
/enthelp list
==================
*/
typedef struct {
	const char	*title;
	const char	*prefixes;	// space-separated; "" is the group of what no other group takes
	int			page;
	int			flag;		// RP_EH_*: the group of the classes with this flag, before any prefix group
} rpEhGroup_t;

static const rpEhGroup_t rp_eh_groups[] = {
	{ "Brush and moving entities",		"func_",						1 },
	{ "Effects and weather",			"fx_",							1 },
	{ "Points: spawns, camp, siege",	"info_",						1 },
	{ "Pickups",						"item_ ammo_ weapon_",			1 },
	{ "Models, props and machines",		"misc_",						1 },
	{ "NPCs",							"npc_",							2 },
	{ "Targets: things a trigger fires",	"target_",					2 },
	{ "Triggers",						"trigger_",						2 },
	{ "Team gametypes",					"team_",						2 },
	{ "Bot and NPC navigation",			"waypoint path_ point_",		2 },
	{ "Map file only: removed at spawn",	"",							2,	RP_EH_REMOVED },
	{ "Other",							"",								2 },
};

static qboolean RP_EhPrefixed( const char *name, const char *prefixes )
{
	char word[32];
	const char *p = prefixes;

	while ( *p )
	{
		int len = 0;

		while ( *p == ' ' )
			p++;
		while ( p[len] && p[len] != ' ' )
			len++;
		if ( !len )
			break;
		Q_strncpyz( word, p, ( len + 1 < (int)sizeof( word ) ) ? len + 1 : (int)sizeof( word ) );
		if ( !Q_stricmpn( name, word, len ) )
			return qtrue;
		p += len;
	}

	return qfalse;
}

static int RP_EhGroupOf( const char *name )
{
	const rpEntHelp_t *h = RP_EhEntry( name );
	int g;

	// GalaxyRP: [Entity Help] a flag's group first: the "Map file only" classes leave their prefix's group
	for ( g = 0; g < (int)ARRAY_LEN( rp_eh_groups ); g++ )
	{
		if ( rp_eh_groups[g].flag && h && ( h->flags & rp_eh_groups[g].flag ) )
			return g;
	}

	for ( g = 0; g < (int)ARRAY_LEN( rp_eh_groups ); g++ )
	{
		if ( !rp_eh_groups[g].flag && rp_eh_groups[g].prefixes[0] && RP_EhPrefixed( name, rp_eh_groups[g].prefixes ) )
			return g;
	}

	return (int)ARRAY_LEN( rp_eh_groups ) - 1;
}

// GalaxyRP fix: [Entity Help] a space before each tag, so it reads apart from the name: "item_botroam L x"
static const char *RP_EhListName( const char *name, qboolean logical )
{
	const rpEntHelp_t *h = RP_EhEntry( name );
	const qboolean mapOnly = ( h && ( h->flags & RP_EH_MAPONLY ) ) ? qtrue : qfalse;
	const qboolean gametype = ( h && ( h->flags & RP_EH_GAMETYPE ) ) ? qtrue : qfalse;

	return va( "%s%s%s%s%s", name, logical ? " ^5L" : "", mapOnly ? " ^1x" : "", gametype ? " ^3g" : "",
		( logical || mapOnly || gametype ) ? "^7" : "" );
}

static void RP_EhList( rpEhOut_t *o, int page )
{
	int g, i, count = RP_SpawnClassCount();
	char line[RP_EH_PRINT_MAX];

	RP_EhLine( o, va( "\n^2Entity classes^7, page %d of %d -- ^3/enthelp <name>^7 for one. ^5L^7: logical (not networked), ^1x^7: not for the Entity System, ^3g^7: only in some gametypes.", page, RP_EH_LIST_PAGES ) );

	for ( g = 0; g < (int)ARRAY_LEN( rp_eh_groups ); g++ )
	{
		int n = 0;
		const gitem_t *item;

		if ( rp_eh_groups[g].page != page )
			continue;

		// DAJ_RP fix: [Entity Help] the heading first: a long group (the NPCs) is printed in chunks as it
		// fills, and with the heading after the loops its first chunk came out under the previous group's
		RP_EhLine( o, va( "^5%s", rp_eh_groups[g].title ) );

		line[0] = '\0';
		for ( i = 0; i < count; i++ )
		{
			qboolean logical;
			const char *cn = RP_SpawnClassName( i, &logical );

			if ( RP_EhGroupOf( cn ) != g )
				continue;
			Q_strcat( line, sizeof( line ), va( "%s%s", n ? ", " : "", RP_EhListName( cn, logical ) ) );
			n++;
			if ( strlen( line ) > RP_EH_PRINT_MAX - 100 )
			{
				RP_EhLine( o, line );
				line[0] = '\0';
				n = 0;
			}
		}
		for ( item = bg_itemlist + 1; item->classname; item++ )
		{
			if ( RP_EhGroupOf( item->classname ) != g )
				continue;
			Q_strcat( line, sizeof( line ), va( "%s%s", n ? ", " : "", RP_EhListName( item->classname, qfalse ) ) );
			n++;
			if ( strlen( line ) > RP_EH_PRINT_MAX - 100 )
			{
				RP_EhLine( o, line );
				line[0] = '\0';
				n = 0;
			}
		}

		if ( line[0] )
			RP_EhLine( o, line );
	}

	if ( page == RP_EH_LIST_PAGES )
	{
		int n = 0;

		line[0] = '\0';
		for ( i = 0; i < (int)ARRAY_LEN( rp_ent_help ); i++ )
		{
			if ( !( rp_ent_help[i].flags & RP_EH_TOPIC ) )
				continue;
			Q_strcat( line, sizeof( line ), va( "%s%s", n++ ? ", " : "", rp_ent_help[i].name ) );
		}
		RP_EhLine( o, "^5Topics: keys and rules many classes share" );
		RP_EhLine( o, line );
	}
	else
	{
		RP_EhLine( o, va( "^3/enthelp list %d^7 for the rest.", page + 1 ) );
	}
}

/*
==================
A part of a name
==================
*/
static int RP_EhMatches( rpEhOut_t *o, const char *part, const char **only, qboolean print )
{
	char line[RP_EH_PRINT_MAX * 2];
	int i, n = 0, count = RP_SpawnClassCount();
	const gitem_t *item;

	line[0] = '\0';

#define RP_EH_MATCH( nm ) do { \
		if ( Q_stristr( (nm), part ) ) { \
			if ( !n ) *only = (nm); \
			if ( print && n < RP_EH_MATCHES_SHOWN ) Q_strcat( line, sizeof( line ), va( "%s^3%s^7", n ? ", " : "", (nm) ) ); \
			n++; \
		} \
	} while ( 0 )

	for ( i = 0; i < count; i++ )
		RP_EH_MATCH( RP_SpawnClassName( i, NULL ) );
	for ( item = bg_itemlist + 1; item->classname; item++ )
		RP_EH_MATCH( item->classname );
	for ( i = 0; i < (int)ARRAY_LEN( rp_ent_help ); i++ )
	{
		if ( rp_ent_help[i].flags & RP_EH_TOPIC )
			RP_EH_MATCH( rp_ent_help[i].name );
	}

#undef RP_EH_MATCH

	if ( print && n > 1 )
	{
		RP_EhLine( o, va( "%d entity classes and topics match %s:", n, RP_EhSafe( part ) ) );
		if ( n > RP_EH_MATCHES_SHOWN )
			Q_strcat( line, sizeof( line ), va( " ... and %d more", n - RP_EH_MATCHES_SHOWN ) );
		RP_EhLine( o, line );
		RP_EhLine( o, "^3/enthelp <name>^7 for one of them." );
	}

	return n;
}

/*
==================
The entity aimed at, or an id
==================
*/
static void RP_EhShowEntity( rpEhOut_t *o, gentity_t *target )
{
	qboolean isClass, isItem;
	const char *known;

	if ( !target || !target->inuse )
	{
		RP_EhLine( o, "There is no entity there." );
		return;
	}

	// DAJ_RP: [Entity System] a character -- not a misc_weapon_shooter, whose client is only its pool's
	// (RP_EntityIsCharacter, g_misc.c); it is shown as the entity it is, below
	if ( RP_EntityIsCharacter( target ) )
	{
		if ( target->s.eType == ET_NPC )
			RP_EhLine( o, va( "Entity %d is an NPC: npc_ classes and npc_spawner place them -- ^3/enthelp npc^7.", target->s.number ) );
		else
			RP_EhLine( o, va( "Entity %d is a player.", target->s.number ) );
		return;
	}

	known = RP_EhKnownName( target->classname, &isClass, &isItem );

	if ( !RP_EntityHasSpawnKeys( target ) )
	{
		gentity_t *maker = target->rpMadeBy ? target->rpMadeBy : target->parent;

		RP_EhLine( o, va( "Entity %d (%s) was made by the game, not placed, so it has no keys to set.", target->s.number,
			target->classname ? RP_EhSafe( target->classname ) : "no class" ) );
		// DAJ_RP: [Entity System] a shooter's bolt points back to the shooter, as other made things do to their
		// maker; a player's or an NPC's things still do not
		if ( maker && maker != target && maker->inuse && maker->classname && !RP_EntityIsCharacter( maker ) )
			RP_EhLine( o, va( "It belongs to entity %d (%s): ^3/enthelp %d^7.", maker->s.number, RP_EhSafe( maker->classname ), maker->s.number ) );
		else if ( known && ( isClass || isItem ) )
			RP_EhShowClass( o, known );
		return;
	}

	if ( !known || !( isClass || isItem ) )
	{
		RP_EhLine( o, va( "Entity %d is a %s, which is not an entity class the Entity System knows.", target->s.number,
			target->classname ? RP_EhSafe( target->classname ) : "nameless entity" ) );
		return;
	}

	RP_EhLine( o, va( "Entity %d:", target->s.number ) );
	RP_EhShowClass( o, known );
}

/*
==================
Cmd_EntHelp_f
==================
*/
void Cmd_EntHelp_f( gentity_t *ent )
{
	rpEhOut_t o;
	char arg1[MAX_STRING_CHARS], arg2[MAX_STRING_CHARS];
	qboolean isClass, isItem;
	const char *known;

	if ( !check_admin_command( ent, ADM_ENTITYSYSTEM, qtrue ) )
		return;

	memset( &o, 0, sizeof( o ) );
	o.client = ent - g_entities;

	if ( trap->Argc() < 2 )
	{
		gentity_t *aimed = RP_EntAimTarget( ent );

		if ( !aimed )
		{
			RP_EhLine( &o, "Aim at an entity, or give a class name or an entity id: ^3/enthelp <classname>^7. ^3/enthelp list^7 lists every class." );
		}
		else
		{
			RP_EhShowEntity( &o, aimed );
		}
		RP_EhFlush( &o );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	arg2[0] = '\0';
	if ( trap->Argc() >= 3 )
		trap->Argv( 2, arg2, sizeof( arg2 ) );

	if ( !Q_stricmp( arg1, "list" ) )
	{
		int page = arg2[0] ? atoi( arg2 ) : 1;

		if ( page < 1 || page > RP_EH_LIST_PAGES )
			RP_EhLine( &o, va( "There are %d pages: ^3/enthelp list 1^7 and ^3/enthelp list 2^7.", RP_EH_LIST_PAGES ) );
		else
			RP_EhList( &o, page );
		RP_EhFlush( &o );
		return;
	}

	// an entity id, either region -- the numbers /entlist prints
	if ( arg1[0] >= '0' && arg1[0] <= '9' )
	{
		int id = atoi( arg1 );
		int k;

		for ( k = 0; arg1[k]; k++ )
		{
			if ( arg1[k] < '0' || arg1[k] > '9' )
				break;
		}
		if ( arg1[k] || id < 0 || id >= MAX_GENTITIES + level.num_logicalents || ( id >= level.num_entities && id < MAX_GENTITIES ) )
			RP_EhLine( &o, "Invalid entity id." );
		else
			RP_EhShowEntity( &o, &g_entities[id] );
		RP_EhFlush( &o );
		return;
	}

	known = RP_EhKnownName( arg1, &isClass, &isItem );
	if ( !known )
	{
		const char *only = NULL;
		int n = RP_EhMatches( &o, arg1, &only, qtrue );

		if ( n == 0 )
			RP_EhLine( &o, va( "No entity class or topic matches %s. ^3/enthelp list^7 lists every class.", RP_EhSafe( arg1 ) ) );
		else if ( n == 1 )
			known = only;
	}

	if ( known )
	{
		if ( arg2[0] )
			RP_EhShowKey( &o, known, arg2 );
		else
			RP_EhShowClass( &o, known );
	}

	RP_EhFlush( &o );
}

/*
==================
For the tests: the table, read-only
==================
*/
int RP_EntHelpCount( void )
{
	return (int)ARRAY_LEN( rp_ent_help );
}

const char *RP_EntHelpField( int i, int field, int *flags )
{
	const rpEntHelp_t *h;

	if ( i < 0 || i >= (int)ARRAY_LEN( rp_ent_help ) )
		return NULL;

	h = &rp_ent_help[i];
	if ( flags )
		*flags = h->flags;

	switch ( field )
	{
	case 0: return h->name;
	case 1: return h->note;
	case 2: return h->text;
	case 3: return h->spawnflags;
	case 4: return h->keys;
	case 5: return h->example;
	case 6: return h->also;
	default: return NULL;
	}
}
