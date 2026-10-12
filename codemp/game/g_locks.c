/*
===========================================================================
DAJ_RP: [Locks] account-locked entities.

A door, a lift, a button, a trigger -- any entity -- can carry a lock: the name of a list of accounts
kept for the map in GalaxyRP/locks/<map>/locks.txt. Players whose account is not on that list cannot
use it, however they reach it:

  Use_BinaryMover()   doors, lifts and buttons: their own triggers, a touch, the use key, a chain
                      from another entity, a door shot open, a script (g_mover.c)
  GlobalUse()         everything used through a chain or with the use key (g_utils.c)
  G_TouchTriggers()   triggers walked into: teleporters, jump pads, trigger_multiple (g_active.c)
  Touch_Multi()       a trigger_multiple or trigger_once fired with the Use or fire button: refused when
                      the button is pressed, not when walked into (g_trigger.c)

Who gets through (RP_LockAllows):
  - a player: logged in, with the account on the lock's list;
  - a vehicle a player drives: the pilot's account decides;
  - no activator, or one that is not a character (timers, the world, map logic): passes;
  - an NPC, an empty vehicle, or one an NPC drives: never;
  - what /entuse, /enttrigger or a level 5 Stun Baton uses: always, and so does everything it sets off,
    at once (RP_LockBypass) or later, after a delay or when a door arrives (RP_LockMarkPass).
A lock whose name has no list lets nobody through but those. The classes that hurt or kill whoever
touches or uses them never carry a lock (RP_LockHarmful): it would only spare the players not on it.

Where the lock comes from (RP_LockNameOf):
  - the "lock" key of an entity the Entity System made -- kept in its record, so /entsave keeps it;
  - for the map's own entities, a "model *N <lock>" line of the map's lock file, matched on the brush
    model, which is unique to the entity and the same on every load: the map's linked doors cannot be
    edited (M in /entlist), so they cannot be given a key;
  - in a door team, the lock of any member locks the whole team.

The file, plain text, one line each:
  lock <name> <account> <account> ...
  model *<N> <name>
read at map start (RP_LocksLoad, G_InitGame) and written back whole after every change, with what it
could not use kept as it was written. "none" is never a lock name: it means no lock.
===========================================================================
*/

#include "g_local.h"
#include "sqlite/sqlite3.h"

extern int RP_DB_Open( sqlite3 **db );
extern void zyk_create_dir( char *file_path );
extern qboolean check_admin_command( gentity_t *ent, int admin_command, qboolean with_message );
extern void zyk_main_set_entity_field( gentity_t *ent, char *key, char *value );
extern qboolean StringIsInteger( const char *s );

#define RP_LOCK_MAX				128		// locks one map may have
#define RP_LOCK_MAX_ACCOUNTS	64		// accounts on one lock
#define RP_LOCK_MAX_MODELS		256		// "model *N" lines one map may have
#define RP_LOCK_NAME_LENGTH		32		// 31 characters and the terminator
#define RP_LOCK_ACCOUNT_LENGTH	32		// an account name: as clientSession_t::filename holds it
#define RP_LOCK_LINE_LENGTH		8192	// one "lock" line with 64 accounts fits with room to spare; longer is skipped
#define RP_LOCK_KEPT_SIZE		16384	// lines of the file the loader could not use, written back as they were
#define RP_LOCK_PRINT_MAX		900		// one print of a list: the engine drops a server command over 1022
#define RP_LOCK_LIST_SIZE		( RP_LOCK_MAX_ACCOUNTS * 2 * RP_LOCK_ACCOUNT_LENGTH )	// the names one command lists back
#define RP_LOCK_MESSAGE_MSEC	15000	// "Locked." at most this often for one player, whatever it was refused: one
										// standing in a locked trigger is not told every moment, and other centre
										// prints (/clientprint) are not covered over
#define RP_LOCK_INVALID_NAME	"(invalid)"	// a "lock" key that is no lock name (RP_LockNameOf)
#define RP_LOCK_NONE			"none"		// no lock: never a lock name

typedef struct {
	char	name[RP_LOCK_NAME_LENGTH];
	int		numAccounts;
	char	accounts[RP_LOCK_MAX_ACCOUNTS][RP_LOCK_ACCOUNT_LENGTH];
} rpLock_t;

typedef struct {
	int		model;							// the N of "*N"
	char	lock[RP_LOCK_NAME_LENGTH];
} rpLockModel_t;

static rpLock_t			rp_locks[RP_LOCK_MAX];
static int				rp_numLocks;
static rpLockModel_t	rp_lockModels[RP_LOCK_MAX_MODELS];
static int				rp_numLockModels;
static char				rp_lockMap[MAX_QPATH];
static int				rp_lockBypass;
static int				rp_lockMessageTime[MAX_CLIENTS];

/*
=============================================================================

NAMES, THE LISTS, THE FILE

=============================================================================
*/

// DAJ_RP: [Locks] a lock name: 1 to 31 letters, digits, '_' or '-', and not "none", which means no lock
// (/entlockset <id> none, a "lock" key of none)
static qboolean RP_LockNameValid( const char *name )
{
	int i;

	if ( !name || !name[0] || strlen( name ) >= RP_LOCK_NAME_LENGTH || !Q_stricmp( name, RP_LOCK_NONE ) )
		return qfalse;

	for ( i = 0; name[i]; i++ )
	{
		if ( !isalnum( (unsigned char)name[i] ) && name[i] != '_' && name[i] != '-' )
			return qfalse;
	}

	return qtrue;
}

// DAJ_RP: [Locks] an account name as typed: 1 to 31 printable characters that cannot end a print command
// or a line of the file. Whether the account exists is the database's to say (RP_LockAccountLookup).
static qboolean RP_LockAccountNameValid( const char *name )
{
	int i;

	if ( !name || !name[0] || strlen( name ) >= RP_LOCK_ACCOUNT_LENGTH )
		return qfalse;

	for ( i = 0; name[i]; i++ )
	{
		if ( (unsigned char)name[i] <= ' ' || (unsigned char)name[i] > '~' || name[i] == '"' || name[i] == ';' || name[i] == '\\' )
			return qfalse;
	}

	return qtrue;
}

static rpLock_t *RP_LockFind( const char *name )
{
	int i;

	if ( !name )
		return NULL;

	for ( i = 0; i < rp_numLocks; i++ )
	{
		if ( !Q_stricmp( rp_locks[i].name, name ) )
			return &rp_locks[i];
	}

	return NULL;
}

static int RP_LockAccountIndex( const rpLock_t *lock, const char *account )
{
	int i;

	for ( i = 0; lock && i < lock->numAccounts; i++ )
	{
		if ( !Q_stricmp( lock->accounts[i], account ) )
			return i;
	}

	return -1;
}

static void RP_LockDelete( rpLock_t *lock )
{
	int index = lock - rp_locks;

	if ( index < 0 || index >= rp_numLocks )
		return;

	if ( index < rp_numLocks - 1 )
		memmove( &rp_locks[index], &rp_locks[index + 1], sizeof( rp_locks[0] ) * ( rp_numLocks - 1 - index ) );
	rp_numLocks--;
}

static rpLockModel_t *RP_LockModelFind( int model )
{
	int i;

	for ( i = 0; i < rp_numLockModels; i++ )
	{
		if ( rp_lockModels[i].model == model )
			return &rp_lockModels[i];
	}

	return NULL;
}

// DAJ_RP: [Locks] the N of a brush model "*N", -1 for anything else
static int RP_LockBrushModel( const char *model )
{
	int i;

	if ( !model || model[0] != '*' || !model[1] )
		return -1;

	for ( i = 1; model[i]; i++ )
	{
		if ( !isdigit( (unsigned char)model[i] ) )
			return -1;
	}

	return atoi( model + 1 );
}

static void RP_LockMapName( char *out, int size )
{
	char serverinfo[MAX_INFO_STRING];

	serverinfo[0] = '\0';
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz( out, Info_ValueForKey( serverinfo, "mapname" ), size );
}

/*
==================
RP_LocksLoad / RP_LocksSave

DAJ_RP: [Locks] the map's lock file, read at map start and written back whole after every change.

Nothing in it is checked against the account database: the file is the server owner's, and /entlockadd
checked what it added. What the loader cannot use is kept, word for word, and written back after the
locks and model lines under RP_LOCK_KEPT_MARK, so a hand edit is never lost to the next /entlock command:
the owner's comments, a line it does not understand, a "lock" or "model" line past the limits, and the
account names of a "lock" line it could not take (past the limit, or not a name an account can have):
those as a commented "lock" line, and a "lock" or "model" line past the limits commented out whole, so
they do not come back on their own once there is room. The file's
own header (RP_LOCK_HEADER) is not kept: it is written again. A UTF-8 byte order mark is skipped. A line
too long to read whole (over RP_LOCK_LINE_LENGTH) is skipped and logged rather than read in pieces,
which could have cut an account name short and let a different account in.
==================
*/
#define RP_LOCK_KEPT_MARK	"# Lines the server did not use (kept as written):"

static const char *rp_lockHeader[] = {
	"# Account locks for this map: \"lock <name> <account> ...\" lists who may use what carries the lock,",
	"# \"model *<N> <name>\" puts a lock on the map's own entity with that brush model. /entlockadd and the",
	"# other /entlock commands write this file.",
	RP_LOCK_KEPT_MARK,
	NULL
};

static char	rp_lockKept[RP_LOCK_KEPT_SIZE];
static int	rp_lockKeptLen;

// DAJ_RP: [Locks] one line for the kept part of the file; logged when there is no room left for it
static void RP_LockKeep( const char *text, int lineNumber )
{
	int len = (int)strlen( text );

	if ( rp_lockKeptLen + len + 2 > (int)sizeof( rp_lockKept ) )
	{
		G_LogPrintf( "locks.txt line %d: no room left to keep it, so it will not be written back\n", lineNumber );
		return;
	}

	memcpy( rp_lockKept + rp_lockKeptLen, text, len );
	rp_lockKeptLen += len;
	rp_lockKept[rp_lockKeptLen++] = '\n';
	rp_lockKept[rp_lockKeptLen] = '\0';
}

static qboolean RP_LockHeaderLine( const char *text )
{
	int i;

	if ( !Q_strncmp( text, "# Account locks for ", 20 ) )
		return qtrue;	// the first header line names the map; an older file's copy too

	for ( i = 1; rp_lockHeader[i]; i++ )
	{
		if ( !strcmp( text, rp_lockHeader[i] ) )
			return qtrue;
	}

	return qfalse;
}

void RP_LocksLoad( void )
{
	static char line[RP_LOCK_LINE_LENGTH];
	char raw[RP_LOCK_LINE_LENGTH];
	FILE *f;
	int lineNumber = 0;

	rp_numLocks = 0;
	rp_numLockModels = 0;
	rp_lockBypass = 0;
	rp_lockKept[0] = '\0';
	rp_lockKeptLen = 0;
	memset( rp_lockMessageTime, 0, sizeof( rp_lockMessageTime ) );
	RP_LockMapName( rp_lockMap, sizeof( rp_lockMap ) );

	f = fopen( va( "GalaxyRP/locks/%s/locks.txt", rp_lockMap ), "r" );
	if ( !f )
		return;

	while ( fgets( line, sizeof( line ), f ) )
	{
		char *words[2 + RP_LOCK_MAX_ACCOUNTS * 2];
		char *p = line, *rest;
		int n = 0, len;

		lineNumber++;

		len = (int)strlen( line );
		if ( len == (int)sizeof( line ) - 1 && line[len - 1] != '\n' && !feof( f ) )
		{	// too long to read whole: skip the rest of it too
			int c;

			while ( ( c = fgetc( f ) ) != EOF && c != '\n' )
				;
			G_LogPrintf( "locks.txt line %d: longer than %d characters, ignored, and it will not be written back\n", lineNumber, RP_LOCK_LINE_LENGTH - 1 );
			continue;
		}

		if ( lineNumber == 1 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF )
			p += 3;	// a UTF-8 byte order mark (Notepad)

		// the line as written, without its line break, for keeping
		Q_strncpyz( raw, p, sizeof( raw ) );
		len = (int)strlen( raw );
		while ( len > 0 && ( raw[len - 1] == '\n' || raw[len - 1] == '\r' ) )
			raw[--len] = '\0';

		while ( *p && n < (int)ARRAY_LEN( words ) )
		{
			while ( *p && (unsigned char)*p <= ' ' )
				*p++ = '\0';
			if ( !*p )
				break;
			words[n++] = p;
			while ( *p && (unsigned char)*p > ' ' )
				p++;
		}

		// more words than fit: the rest of the line, as it is (a "lock" line with that many accounts). The
		// last word taken ends where the rest begins: the loop stopped before it cut it off.
		if ( *p && (unsigned char)*p <= ' ' )
			*p++ = '\0';
		rest = p;
		while ( *rest && (unsigned char)*rest <= ' ' )
			rest++;
		len = (int)strlen( rest );
		while ( len > 0 && (unsigned char)rest[len - 1] <= ' ' )
			rest[--len] = '\0';

		if ( n == 0 )
			continue;

		if ( words[0][0] == '#' )
		{	// a comment: the owner's is kept, the file's own header is written again
			if ( !RP_LockHeaderLine( raw ) )
				RP_LockKeep( raw, lineNumber );
			continue;
		}

		if ( !Q_stricmp( words[0], "lock" ) && n >= 2 && RP_LockNameValid( words[1] ) )
		{
			char left[RP_LOCK_LINE_LENGTH];
			rpLock_t *lock = RP_LockFind( words[1] );
			int i;

			if ( !lock )
			{
				if ( rp_numLocks >= RP_LOCK_MAX )
				{
					G_LogPrintf( "locks.txt line %d: more than %d locks, lock %s not loaded (kept in the file, commented out)\n", lineNumber, RP_LOCK_MAX, words[1] );
					RP_LockKeep( va( "# not loaded (more than %d locks): %s", RP_LOCK_MAX, raw ), lineNumber );
					continue;
				}
				lock = &rp_locks[rp_numLocks++];
				memset( lock, 0, sizeof( *lock ) );
				Q_strncpyz( lock->name, words[1], sizeof( lock->name ) );
			}

			left[0] = '\0';
			for ( i = 2; i < n; i++ )
			{
				if ( RP_LockAccountNameValid( words[i] ) && RP_LockAccountIndex( lock, words[i] ) >= 0 )
					continue;	// listed twice
				if ( !RP_LockAccountNameValid( words[i] ) || lock->numAccounts >= RP_LOCK_MAX_ACCOUNTS )
				{
					Q_strcat( left, sizeof( left ), va( " %s", words[i] ) );
					continue;
				}
				Q_strncpyz( lock->accounts[lock->numAccounts++], words[i], RP_LOCK_ACCOUNT_LENGTH );
			}
			if ( rest[0] )
				Q_strcat( left, sizeof( left ), va( " %s", rest ) );

			if ( left[0] )
			{
				G_LogPrintf( "locks.txt line %d: lock %s, accounts not loaded (past %d, or not an account name; kept in the file):%s\n", lineNumber, lock->name, RP_LOCK_MAX_ACCOUNTS, left );
				RP_LockKeep( va( "# not loaded: lock %s%s", lock->name, left ), lineNumber );
			}
		}
		else if ( !Q_stricmp( words[0], "model" ) && n == 3 && RP_LockBrushModel( words[1] ) >= 0 && RP_LockNameValid( words[2] ) )
		{
			int model = RP_LockBrushModel( words[1] );
			rpLockModel_t *line_ = RP_LockModelFind( model );

			if ( !line_ )
			{
				if ( rp_numLockModels >= RP_LOCK_MAX_MODELS )
				{
					G_LogPrintf( "locks.txt line %d: more than %d model lines, *%d not loaded (kept in the file, commented out)\n", lineNumber, RP_LOCK_MAX_MODELS, model );
					RP_LockKeep( va( "# not loaded (more than %d model lines): %s", RP_LOCK_MAX_MODELS, raw ), lineNumber );
					continue;
				}
				line_ = &rp_lockModels[rp_numLockModels++];
				line_->model = model;
			}
			Q_strncpyz( line_->lock, words[2], sizeof( line_->lock ) );
		}
		else
		{
			G_LogPrintf( "locks.txt line %d: not understood, kept as written (lines are \"lock <name> <account> ...\" and \"model *<N> <name>\")\n", lineNumber );
			RP_LockKeep( raw, lineNumber );
		}
	}

	fclose( f );
	G_LogPrintf( "locks: %d lock(s) and %d map model line(s) for %s\n", rp_numLocks, rp_numLockModels, rp_lockMap );
}

static qboolean RP_LocksSave( void )
{
	FILE *f;
	int i, j;

	zyk_create_dir( va( "locks/%s", rp_lockMap ) );

	f = fopen( va( "GalaxyRP/locks/%s/locks.txt", rp_lockMap ), "w" );
	if ( !f )
		return qfalse;

	fprintf( f, "# Account locks for %s: \"lock <name> <account> ...\" lists who may use what carries the lock,\n", rp_lockMap );
	fprintf( f, "%s\n%s\n", rp_lockHeader[1], rp_lockHeader[2] );

	for ( i = 0; i < rp_numLocks; i++ )
	{
		fprintf( f, "lock %s", rp_locks[i].name );
		for ( j = 0; j < rp_locks[i].numAccounts; j++ )
			fprintf( f, " %s", rp_locks[i].accounts[j] );
		fprintf( f, "\n" );
	}

	for ( i = 0; i < rp_numLockModels; i++ )
		fprintf( f, "model *%d %s\n", rp_lockModels[i].model, rp_lockModels[i].lock );

	if ( rp_lockKeptLen > 0 )
		fprintf( f, "%s\n%s", RP_LOCK_KEPT_MARK, rp_lockKept );

	fclose( f );
	return qtrue;
}

/*
==================
RP_LockAccountLookup

DAJ_RP: [Locks] whether an account of that name exists, compared as /login compares it (without case): 1,
with the account's own spelling in canonical; 0, none; -1, the database could not be read.
==================
*/
static int RP_LockAccountLookup( const char *name, char *canonical, int size )
{
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;
	int rc, found = 0;

	if ( RP_DB_Open( &db ) != SQLITE_OK )
	{
		if ( db )
			sqlite3_close( db );
		return -1;
	}

	rc = sqlite3_prepare_v2( db, "SELECT Username FROM Accounts WHERE Username = ? COLLATE NOCASE", -1, &stmt, NULL );
	if ( rc != SQLITE_OK )
	{
		sqlite3_finalize( stmt );
		sqlite3_close( db );
		return -1;
	}

	sqlite3_bind_text( stmt, 1, name, -1, SQLITE_TRANSIENT );
	rc = sqlite3_step( stmt );
	if ( rc == SQLITE_ROW )
	{
		const unsigned char *spelling = sqlite3_column_text( stmt, 0 );

		Q_strncpyz( canonical, ( spelling && RP_LockAccountNameValid( (const char *)spelling ) ) ? (const char *)spelling : name, size );
		found = 1;
	}
	else if ( rc != SQLITE_DONE )
	{
		found = -1;
	}

	sqlite3_finalize( stmt );
	sqlite3_close( db );
	return found;
}

/*
=============================================================================

THE CHECK

=============================================================================
*/

// DAJ_RP: [Locks] the classes that hurt or kill whoever touches them (the triggers) or uses them
// (target_kill): a lock would only spare the players not on its list, so they never carry one. /entlockset
// refuses them, and a "lock" key or a model line on one is ignored (RP_LockSpawnNote says so).
static const char *rp_lockHarmfulClasses[] = {
	"trigger_hurt", "trigger_space", "trigger_shipboundary", "trigger_hyperspace", "target_kill", NULL
};

static qboolean RP_LockHarmful( const gentity_t *ent )
{
	int i;

	if ( !ent || !ent->classname )
		return qfalse;

	for ( i = 0; rp_lockHarmfulClasses[i]; i++ )
	{
		if ( !Q_stricmp( ent->classname, rp_lockHarmfulClasses[i] ) )
			return qtrue;
	}

	return qfalse;
}

// DAJ_RP: [Locks] the entity's own lock: its key, or for the map's own a model line of the lock file. The
// key counts only on an entity that is not the map's own (a map that happened to use a "lock" key of its
// own must not lock its doors). A key of none is no lock. A key that is no lock name -- set by hand in an
// entity file, say -- still locks, as a lock with no list does, and goes by RP_LOCK_INVALID_NAME, which no
// list can have, so it is never shown as it was typed. A harmful class has none (RP_LockHarmful).
const char *RP_LockNameOf( const gentity_t *ent )
{
	if ( !ent || !ent->inuse || RP_LockHarmful( ent ) )
		return NULL;

	if ( ent->rpLock && ent->rpLock[0] && !ent->rpMapEntity )
	{
		if ( !Q_stricmp( ent->rpLock, RP_LOCK_NONE ) )
			return NULL;
		return RP_LockNameValid( ent->rpLock ) ? ent->rpLock : RP_LOCK_INVALID_NAME;
	}

	if ( ent->rpMapEntity && ent->rpSubBSPOf <= 0 )
	{	// not one from a misc_bsp: its "*N" counts in its own BSP's models, not the map's
		int model = RP_LockBrushModel( ent->model );
		const rpLockModel_t *line = ( model >= 0 ) ? RP_LockModelFind( model ) : NULL;

		if ( line )
			return line->lock;
	}

	return NULL;
}

// DAJ_RP: [Locks] its own lock, or in a door team any member's. Team links are raw pointers, so only live
// members are followed, and not forever.
static const char *RP_LockNameOfTeam( gentity_t *ent )
{
	const char *name = RP_LockNameOf( ent );
	gentity_t *member;
	int guard = 0;

	if ( name )
		return name;

	if ( !ent->teammaster || !ent->teammaster->inuse )
		return NULL;

	for ( member = ent->teammaster; member && guard < 64; member = member->teamchain, guard++ )
	{
		if ( !member->inuse )
			break;
		if ( member != ent && ( name = RP_LockNameOf( member ) ) != NULL )
			return name;
	}

	return NULL;
}

void RP_LockBypass( qboolean on )
{
	if ( on )
		rp_lockBypass++;
	else if ( rp_lockBypass > 0 )
		rp_lockBypass--;
}

static void RP_LockTell( gentity_t *player, const char *name )
{
	int num = player->s.number;

	if ( num < 0 || num >= MAX_CLIENTS || rp_lockMessageTime[num] > level.time )
		return;

	rp_lockMessageTime[num] = level.time + RP_LOCK_MESSAGE_MSEC;

	if ( player->client->pers.bitvalue & ( 1 << ADM_ENTITYSYSTEM ) )
		trap->SendServerCommand( num, va( "cp \"Locked.\n^7(lock ^3%s^7)\"", name ) );
	else
		trap->SendServerCommand( num, "cp \"Locked.\"" );
}

/*
==================
RP_LockMarkPass / RP_LockPassMatches

DAJ_RP: [Locks] /entuse, /enttrigger and a level 5 Stun Baton pass every lock, and so does what they
set off. RP_LockBypass covers what happens during the use itself; what happens later -- a door or button
with a delay, a door's opentarget, closetarget and target2 when it arrives, a trigger_multiple with a
delay, a target_delay -- runs from a think, after the bypass is over. Every one of those keeps the
activator it was used with (ent->activator) and fires with it. So an entity used during the bypass is
marked with who that activator was, and a later use by a marked entity with that same activator passes,
and marks what it uses in turn, however many delays the chain goes through.

The mark is the activator's number and, for a client, its pers.enterTime: a player who later gets the
same client slot is someone else. A mark only ever lets its own activator through, and only for what the
marked entity sets off: another player using the same button later is judged as always. And it lasts
only until its holder sets the entity off again the ordinary way (RP_LockAllows clears it then, before
the entity fires): what /enttrigger once used must not be a lasting key for the admin who used it --
logged out, or as anyone -- nor for a baton user who lost the baton. That use is then judged as any other;
a delayed use of the earlier one still pending loses its pass too, which only the holder can cause.
Another player's use never clears it. A door team is marked and cleared whole, since any member may be
the one that fires its targets.
==================
*/
static int RP_LockStampOf( const gentity_t *activator )
{
	return ( activator && activator->client ) ? activator->client->pers.enterTime : 0;
}

static qboolean RP_LockPassMatches( const gentity_t *marked, const gentity_t *activator )
{
	return ( marked && activator && marked->rpLockPassBy && marked->rpLockPassBy == activator->s.number + 1 &&
		marked->rpLockPassStamp == RP_LockStampOf( activator ) ) ? qtrue : qfalse;
}

static void RP_LockMarkOne( gentity_t *ent, gentity_t *activator )
{
	ent->rpLockPassBy = activator->s.number + 1;
	ent->rpLockPassStamp = RP_LockStampOf( activator );
}

// DAJ_RP: [Locks] the mark taken off again, from the entity and its door team, as RP_LockMarkPass put it on
static void RP_LockClearPass( gentity_t *ent )
{
	gentity_t *member;
	int guard = 0;

	ent->rpLockPassBy = ent->rpLockPassStamp = 0;

	if ( ent->s.eType != ET_MOVER || !ent->teammaster || !ent->teammaster->inuse )
		return;

	for ( member = ent->teammaster; member && guard < 64; member = member->teamchain, guard++ )
	{
		if ( !member->inuse )
			break;
		member->rpLockPassBy = member->rpLockPassStamp = 0;
	}
}

void RP_LockMarkPass( gentity_t *ent, gentity_t *activator )
{
	gentity_t *member;
	int guard = 0;

	if ( !ent || !ent->inuse || !activator || !activator->inuse )
		return;

	RP_LockMarkOne( ent, activator );

	if ( ent->s.eType != ET_MOVER || !ent->teammaster || !ent->teammaster->inuse )
		return;

	for ( member = ent->teammaster; member && guard < 64; member = member->teamchain, guard++ )
	{
		if ( !member->inuse )
			break;
		RP_LockMarkOne( member, activator );
	}
}

/*
==================
RP_LockAllows

DAJ_RP: [Locks] whether activator may use ent, which other (the entity doing the using: a trigger, a
button, a door's own trigger, the toucher itself) set off. Who passes:
  - anything, during /entuse, /enttrigger or a level 5 Stun Baton (RP_LockBypass), or set off later by
    an entity they set off, with the same activator (RP_LockMarkPass) -- ent is marked in turn;
  - with no lock on ent or its door team: anything;
  - nothing at all as the activator, or one that is not a character -- a timer, the world, map logic;
  - a player: logged in, with the account on the lock's list; a vehicle a player drives: by the pilot.
An NPC does not -- one following a player into a locked door's trigger would open it for them -- nor a
vehicle that is empty or that an NPC drives. A player who may not is told (RP_LockTell).
==================
*/
static qboolean RP_LockCheck( gentity_t *ent, gentity_t *activator );

// DAJ_RP: [Locks] a door team's member other than its master stands for the team's activator. Every member
// fires its own opentarget and closetarget when it arrives, with its own activator -- and only the master's
// is ever set (Use_BinaryMover), so a member's is none, or the door itself, and its targets passed as map
// logic: an unlisted player opening an unlocked team opened the locked door its second half targets. Only
// the lock judges by the master's: the targets are still used with the activator they always were.
static gentity_t *RP_LockActivatorOf( gentity_t *activator )
{
	gentity_t *master;

	if ( !activator || !activator->inuse || activator->s.eType != ET_MOVER || !( activator->flags & FL_TEAMSLAVE ) )
		return activator;

	master = activator->teammaster;
	if ( !master || master == activator || !master->inuse || ( master->flags & FL_TEAMSLAVE ) || master->s.eType != ET_MOVER )
		return activator;

	return master->activator ? master->activator : master;
}

qboolean RP_LockAllows( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	qboolean allowed;

	if ( !ent || !ent->inuse )
		return qtrue;

	activator = RP_LockActivatorOf( activator );

	if ( rp_lockBypass > 0 || ( other && other != ent && RP_LockPassMatches( other, activator ) ) )
	{
		RP_LockMarkPass( ent, activator );
		return qtrue;
	}

	allowed = RP_LockCheck( ent, activator );

	// an ordinary use by the one who holds this entity's mark, which goes ahead: the mark has had its turn
	if ( allowed && RP_LockPassMatches( ent, activator ) )
		RP_LockClearPass( ent );

	return allowed;
}

// DAJ_RP: [Locks] the check itself, with no bypass or mark involved (RP_LockAllows)
static qboolean RP_LockCheck( gentity_t *ent, gentity_t *activator )
{
	const char *name;
	rpLock_t *lock;
	gentity_t *player = activator;

	name = RP_LockNameOfTeam( ent );
	if ( !name )
		return qtrue;

	// DAJ_RP: [Entity System] a misc_weapon_shooter is map logic too -- its client is only its pool's
	// (RP_EntityIsCharacter, g_misc.c); its bolt opening a locked door was refused as an NPC's
	if ( !player || !player->inuse || !RP_EntityIsCharacter( player ) )
		return qtrue;	// map logic, a timer, a script

	if ( player->s.number >= MAX_CLIENTS )
	{	// an NPC or a vehicle: only a vehicle a player drives, and then by the pilot's account
		gentity_t *pilot = NULL;

		if ( player->s.eType == ET_NPC && player->m_pVehicle && player->m_pVehicle->m_pPilot )
			pilot = (gentity_t *)player->m_pVehicle->m_pPilot;

		if ( !pilot || pilot < g_entities || pilot >= &g_entities[MAX_CLIENTS] || !pilot->inuse || !pilot->client )
			return qfalse;

		player = pilot;
	}

	lock = RP_LockFind( name );
	if ( lock && player->client->pers.connected == CON_CONNECTED && player->client->sess.amrpgmode > 0 &&
		player->client->sess.filename[0] && RP_LockAccountIndex( lock, player->client->sess.filename ) >= 0 )
	{
		return qtrue;
	}

	RP_LockTell( player, name );
	return qfalse;
}

/*
==================
RP_LockSpawnNote

DAJ_RP: [Locks] after the Entity System spawns an entity (zyk_main_spawn_entity): a "lock" key on a class
that cannot carry one is kept in its record but does nothing -- say so to the admin (/entadd, /entedit).
==================
*/
void RP_LockSpawnNote( gentity_t *ent )
{
	if ( !ent || !ent->inuse || !ent->rpLock || !ent->rpLock[0] || !Q_stricmp( ent->rpLock, RP_LOCK_NONE ) ||
		!RP_LockHarmful( ent ) || level.rp_spawn_note[0] )
	{
		return;
	}

	Q_strncpyz( level.rp_spawn_note, va( "A %s hurts whoever touches or uses it: a lock would only spare the players not on its list, so its lock key does nothing.", ent->classname ),
		sizeof( level.rp_spawn_note ) );
}

/*
=============================================================================

COMMANDS

=============================================================================
*/

/*
==================
RP_LockPrintList

DAJ_RP: [Locks] "<head><word><sep><word>...<tail>" for a list of space-separated words, in as many prints
as it takes: the engine drops a server command over 1022 characters whole (SV_SendServerCommand), so a
lock's 64 long account names would otherwise not show at all. Each further print starts indented.
==================
*/
static void RP_LockPrintList( gentity_t *ent, const char *head, const char *words, const char *sep, const char *tail )
{
	char msg[RP_LOCK_PRINT_MAX + 128];
	const char *p = words;
	qboolean first = qtrue;

	Q_strncpyz( msg, head, sizeof( msg ) );

	while ( p && *p )
	{
		char word[RP_LOCK_LINE_LENGTH];
		int n = 0;

		while ( *p == ' ' )
			p++;
		if ( !*p )
			break;
		while ( *p && *p != ' ' )
		{
			if ( n < (int)sizeof( word ) - 1 )
				word[n++] = *p;
			p++;
		}
		word[n] = '\0';
		if ( n > 64 )
			Q_strncpyz( word + 60, "...", sizeof( word ) - 60 );	// no list word is this long; never let one fill a print

		if ( !first && (int)( strlen( msg ) + strlen( sep ) + strlen( word ) + strlen( tail ) ) > RP_LOCK_PRINT_MAX )
		{
			trap->SendServerCommand( ent - g_entities, va( "print \"%s\n\"", msg ) );
			Q_strncpyz( msg, "  ", sizeof( msg ) );
			first = qtrue;
		}

		Q_strcat( msg, sizeof( msg ), first ? word : va( "%s%s", sep, word ) );
		first = qfalse;
	}

	Q_strcat( msg, sizeof( msg ), tail );
	trap->SendServerCommand( ent - g_entities, va( "print \"%s\n\"", msg ) );
}

static int RP_LockEntityCount( const char *name, char *ids, int idsSize )
{
	gentity_t *e;
	int count = 0;

	if ( ids && idsSize > 0 )
		ids[0] = '\0';

	RP_FOR_EACH_ENTITY( e )
	{
		const char *own = RP_LockNameOf( e );

		if ( !own || Q_stricmp( own, name ) )
			continue;

		count++;
		if ( ids && count <= 24 )
			Q_strcat( ids, idsSize, va( "%s%d", count > 1 ? ", " : "", e->s.number ) );
		else if ( ids && count == 25 )
			Q_strcat( ids, idsSize, ", ..." );
	}

	return count;
}

/*
==================
Cmd_EntLockAdd_f

DAJ_RP: [Locks] /entlockadd <lock> <account> [account...]: puts accounts on a lock's list, making the list
when it is new. Each must be an account that exists; it is stored as the account spells it.
==================
*/
void Cmd_EntLockAdd_f( gentity_t *ent )
{
	char name[MAX_STRING_CHARS], account[MAX_STRING_CHARS], canonical[RP_LOCK_ACCOUNT_LENGTH];
	char added[RP_LOCK_LIST_SIZE], already[RP_LOCK_LIST_SIZE], missing[RP_LOCK_LIST_SIZE], refused[RP_LOCK_LIST_SIZE];
	rpLock_t *lock;
	qboolean created = qfalse, changed = qfalse;
	int i, before;

	if ( !check_admin_command( ent, ADM_ENTITYSYSTEM, qtrue ) )
		return;

	if ( trap->Argc() < 3 )
	{
		trap->SendServerCommand( ent - g_entities, "print \"Usage: ^3/entlockadd <lock> <account> [more accounts]^7. Lets those accounts use whatever carries that lock on this map.\n\"" );
		return;
	}

	trap->Argv( 1, name, sizeof( name ) );
	if ( !RP_LockNameValid( name ) )
	{
		trap->SendServerCommand( ent - g_entities, "print \"A lock name is 1 to 31 letters, digits, _ or -, and not none (that means no lock).\n\"" );
		return;
	}

	lock = RP_LockFind( name );
	if ( !lock )
	{
		if ( rp_numLocks >= RP_LOCK_MAX )
		{
			trap->SendServerCommand( ent - g_entities, va( "print \"This map has %d locks already, the most it can have. Remove one with ^3/entlockremove <lock> all^7.\n\"", RP_LOCK_MAX ) );
			return;
		}
		lock = &rp_locks[rp_numLocks++];
		memset( lock, 0, sizeof( *lock ) );
		Q_strncpyz( lock->name, name, sizeof( lock->name ) );
		created = qtrue;
	}

	added[0] = already[0] = missing[0] = refused[0] = '\0';
	before = lock->numAccounts;

	for ( i = 2; i < trap->Argc(); i++ )
	{
		int found;

		trap->Argv( i, account, sizeof( account ) );
		if ( !RP_LockAccountNameValid( account ) )
		{
			Q_strcat( missing, sizeof( missing ), " ?" );
			continue;
		}

		found = RP_LockAccountLookup( account, canonical, sizeof( canonical ) );
		if ( found < 0 )
		{
			trap->SendServerCommand( ent - g_entities, "print \"^1The account database could not be read; nothing was added.\n\"" );
			if ( created )
				RP_LockDelete( lock );
			else
				lock->numAccounts = before;	// the ones this command added, at the end of the list
			return;
		}
		if ( found == 0 )
		{
			Q_strcat( missing, sizeof( missing ), va( " %s", account ) );
			continue;
		}
		if ( RP_LockAccountIndex( lock, canonical ) >= 0 )
		{
			Q_strcat( already, sizeof( already ), va( " %s", canonical ) );
			continue;
		}
		if ( lock->numAccounts >= RP_LOCK_MAX_ACCOUNTS )
		{
			Q_strcat( refused, sizeof( refused ), va( " %s", canonical ) );
			continue;
		}

		Q_strncpyz( lock->accounts[lock->numAccounts++], canonical, RP_LOCK_ACCOUNT_LENGTH );
		Q_strcat( added, sizeof( added ), va( " %s", canonical ) );
		changed = qtrue;
	}

	if ( created && !changed )
	{	// nothing to list: no empty list is made
		RP_LockDelete( lock );
		lock = NULL;
	}

	if ( changed && !RP_LocksSave() )
		trap->SendServerCommand( ent - g_entities, "print \"^1The lock file could not be written: the change lasts until the map changes.\n\"" );

	if ( added[0] )
		RP_LockPrintList( ent, va( "Lock ^3%s^7%s: added ", name, created ? " (new)" : "" ), added, " ", "." );
	if ( already[0] )
		RP_LockPrintList( ent, va( "Already on lock ^3%s^7: ", name ), already, " ", "." );
	if ( missing[0] )
		RP_LockPrintList( ent, "^3No such account:^7 ", missing, " ", "." );
	if ( refused[0] )
		RP_LockPrintList( ent, va( "^3Lock %s has %d accounts, the most it can have; not added:^7 ", name, RP_LOCK_MAX_ACCOUNTS ), refused, " ", "." );

	if ( changed )
		G_LogPrintf( "/entlockadd %s by %s:%s\n", name, ent->client->pers.netname, added );
}

/*
==================
Cmd_EntLockRemove_f

DAJ_RP: [Locks] /entlockremove <lock> <account> [more], or /entlockremove <lock> all: takes accounts off a
lock's list, or the whole list. A list left empty goes; what carries the lock then lets nobody through.
==================
*/
void Cmd_EntLockRemove_f( gentity_t *ent )
{
	char name[MAX_STRING_CHARS], account[MAX_STRING_CHARS], removed[RP_LOCK_LIST_SIZE], absent[RP_LOCK_LIST_SIZE];
	rpLock_t *lock;
	int i, entities;

	if ( !check_admin_command( ent, ADM_ENTITYSYSTEM, qtrue ) )
		return;

	if ( trap->Argc() < 3 )
	{
		trap->SendServerCommand( ent - g_entities, "print \"Usage: ^3/entlockremove <lock> <account> [more accounts]^7, or ^3/entlockremove <lock> all^7 for the whole list.\n\"" );
		return;
	}

	trap->Argv( 1, name, sizeof( name ) );
	lock = RP_LockNameValid( name ) ? RP_LockFind( name ) : NULL;
	if ( !lock )
	{
		trap->SendServerCommand( ent - g_entities, va( "print \"This map has no lock named ^3%s^7. ^3/entlocklist^7 lists them.\n\"", RP_ShownText( name ) ) );
		return;
	}
	Q_strncpyz( name, lock->name, sizeof( name ) );

	trap->Argv( 2, account, sizeof( account ) );
	removed[0] = absent[0] = '\0';

	if ( trap->Argc() == 3 && !Q_stricmp( account, "all" ) && RP_LockAccountIndex( lock, "all" ) < 0 )
	{
		Q_strcat( removed, sizeof( removed ), va( " all %d", lock->numAccounts ) );
		lock->numAccounts = 0;
	}
	else
	{
		for ( i = 2; i < trap->Argc(); i++ )
		{
			int index;

			trap->Argv( i, account, sizeof( account ) );
			index = RP_LockAccountIndex( lock, account );
			if ( index < 0 )
			{
				Q_strcat( absent, sizeof( absent ), va( " %s", RP_ShownText( account ) ) );
				continue;
			}

			Q_strcat( removed, sizeof( removed ), va( " %s", lock->accounts[index] ) );
			if ( index < lock->numAccounts - 1 )
				memmove( lock->accounts[index], lock->accounts[index + 1], RP_LOCK_ACCOUNT_LENGTH * ( lock->numAccounts - 1 - index ) );
			lock->numAccounts--;
		}
	}

	if ( absent[0] )
		RP_LockPrintList( ent, va( "Not on lock ^3%s^7: ", name ), absent, " ", "." );

	if ( !removed[0] )
		return;

	if ( lock->numAccounts == 0 )
		RP_LockDelete( lock );

	if ( !RP_LocksSave() )
		trap->SendServerCommand( ent - g_entities, "print \"^1The lock file could not be written: the change lasts until the map changes.\n\"" );

	G_LogPrintf( "/entlockremove %s by %s:%s\n", name, ent->client->pers.netname, removed );

	entities = RP_LockEntityCount( name, NULL, 0 );
	if ( RP_LockFind( name ) )
		RP_LockPrintList( ent, va( "Lock ^3%s^7: removed ", name ), removed, " ", "." );
	else
	{
		char tail[256];

		Com_sprintf( tail, sizeof( tail ), "; its list is gone.%s", entities ? va( " ^3%d entit%s still carr%s it and now let%s nobody through^7 (only /entuse and /enttrigger).", entities,
			entities == 1 ? "y" : "ies", entities == 1 ? "ies" : "y", entities == 1 ? "s" : "" ) : "" );
		RP_LockPrintList( ent, va( "Lock ^3%s^7: removed ", name ), removed, " ", tail );
	}
}

/*
==================
Cmd_EntLockList_f

DAJ_RP: [Locks] /entlocklist: this map's locks, with how many accounts and entities each has, and the locks
entities carry that have no list. /entlocklist <lock>: its accounts and the entities carrying it.
==================
*/
void Cmd_EntLockList_f( gentity_t *ent )
{
	char name[MAX_STRING_CHARS], ids[512];
	gentity_t *e;
	int i;

	if ( !check_admin_command( ent, ADM_ENTITYSYSTEM, qtrue ) )
		return;

	if ( trap->Argc() >= 2 )
	{
		rpLock_t *lock;
		int count;

		trap->Argv( 1, name, sizeof( name ) );
		if ( !RP_LockNameValid( name ) )
		{
			trap->SendServerCommand( ent - g_entities, "print \"A lock name is 1 to 31 letters, digits, _ or -, and not none (that means no lock).\n\"" );
			return;
		}

		lock = RP_LockFind( name );
		count = RP_LockEntityCount( name, ids, sizeof( ids ) );

		if ( !lock && !count )
		{
			trap->SendServerCommand( ent - g_entities, va( "print \"This map has no lock named ^3%s^7, and nothing carries one.\n\"", name ) );
			return;
		}

		if ( lock )
		{
			char accounts[RP_LOCK_MAX_ACCOUNTS * ( RP_LOCK_ACCOUNT_LENGTH + 1 ) + 1];

			accounts[0] = '\0';
			for ( i = 0; i < lock->numAccounts; i++ )
				Q_strcat( accounts, sizeof( accounts ), va( "%s%s", i ? " " : "", lock->accounts[i] ) );
			RP_LockPrintList( ent, va( "\n^3Lock %s^7: %d account(s): ", lock->name, lock->numAccounts ), accounts, ", ", "" );
		}
		else
		{
			trap->SendServerCommand( ent - g_entities, va( "print \"\n^3Lock %s^7: ^1no list^7 -- what carries it lets nobody through (only /entuse and /enttrigger).\n\"", name ) );
		}

		trap->SendServerCommand( ent - g_entities, va( "print \"Carried by %d entit%s%s%s\n\n\"", count, count == 1 ? "y" : "ies", count ? ": " : ".", ids ) );
		return;
	}

	trap->SendServerCommand( ent - g_entities, va( "print \"\n^3Locks on this map^7 (%d of %d)\n\"", rp_numLocks, RP_LOCK_MAX ) );

	if ( !rp_numLocks )
		trap->SendServerCommand( ent - g_entities, "print \"  none. ^3/entlockadd <lock> <account>^7 makes one.\n\"" );

	for ( i = 0; i < rp_numLocks; i++ )
	{
		int count = RP_LockEntityCount( rp_locks[i].name, NULL, 0 );

		trap->SendServerCommand( ent - g_entities, va( "print \"  ^3%s^7 - %d account(s), carried by %d entit%s\n\"", rp_locks[i].name,
			rp_locks[i].numAccounts, count, count == 1 ? "y" : "ies" ) );
	}

	// zyk: locks something carries but no list has -- each once (a name is at most 31 characters, and a
	// map cannot carry more different ones than the buffer holds before entities run out)
	{
		static char carried[RP_LOCK_KEPT_SIZE];

		carried[0] = '\0';
		RP_FOR_EACH_ENTITY( e )
		{
			const char *own = RP_LockNameOf( e );

			if ( !own || RP_LockFind( own ) || strstr( va( " %s ", carried ), va( " %s ", own ) ) )
				continue;
			if ( strlen( carried ) + strlen( own ) + 2 < sizeof( carried ) )
				Q_strcat( carried, sizeof( carried ), va( "%s%s", carried[0] ? " " : "", own ) );
		}
		if ( carried[0] )
			RP_LockPrintList( ent, "^1Carried, with no list^7 (they let nobody through): ", carried, ", ", "" );
	}

	trap->SendServerCommand( ent - g_entities, "print \"^3/entlocklist <lock>^7 shows one lock's accounts and entities.\n\n\"" );
}

// DAJ_RP: [Locks] the "lock" value in the entity's key/value record, NULL when it has none
static const char *RP_LockRecordValue( const gentity_t *ent )
{
	int num = ent->s.number, i;

	if ( num < 0 || num >= MAX_ENTITIESTOTAL )
		return NULL;

	for ( i = 0; i + 1 < level.zyk_spawn_strings_values_count[num] && i + 1 < ZYK_MAX_SPAWN_STRING_SLOTS; i += 2 )
	{
		if ( !Q_stricmp( level.zyk_spawn_strings[num][i], "lock" ) )
			return level.zyk_spawn_strings[num][i + 1];
	}

	return NULL;
}

/*
==================
Cmd_EntLockSet_f

DAJ_RP: [Locks] /entlockset [entity id] <lock | none>: puts a lock on an entity -- that id, or the one
aimed at -- or takes it off. One the
Entity System made gets the "lock" key, in its record too, without being spawned again. The map's own --
whose linked doors /entedit does not touch -- gets a "model *N" line of the lock file, so it needs a brush
model; one without is refused. Not an entity held with /entcopy or /entcut, and not while the admin holds
one (RP_EntRefuseWhileHolding).
==================
*/
void Cmd_EntLockSet_f( gentity_t *ent )
{
	char arg[MAX_STRING_CHARS], name[MAX_STRING_CHARS];
	gentity_t *target;
	qboolean none;
	int id;

	if ( !check_admin_command( ent, ADM_ENTITYSYSTEM, qtrue ) )
		return;

	if ( RP_EntRefuseWhileHolding( ent ) )
		return;

	if ( trap->Argc() != 2 && trap->Argc() != 3 )
	{
		trap->SendServerCommand( ent - g_entities, "print \"Usage: ^3/entlockset <lock>^7 puts a lock on the entity you aim at, ^3/entlockset <entity id> <lock>^7 on that entity. ^3none^7 instead of the lock takes it off.\n\"" );
		return;
	}

	if ( trap->Argc() == 2 )
	{	// DAJ_RP: [Locks] the entity aimed at, as /entedit and /entremove pick theirs (RP_EntAimTarget): a
		// door's own trigger means the door. The one argument is the lock, whatever it looks like.
		trap->Argv( 1, name, sizeof( name ) );

		if ( RP_EntAimFollowing( ent ) )
		{
			trap->SendServerCommand( ent - g_entities, "print \"You are following another player. Stop following first, or give the entity id.\n\"" );
			return;
		}

		target = RP_EntAimTarget( ent );
		if ( !target || !target->inuse )
		{
			trap->SendServerCommand( ent - g_entities, "print \"You are not aiming at an entity. Aim at one, or give its id: ^3/entlockset <entity id> <lock>^7.\n\"" );
			return;
		}
		id = target->s.number;
	}
	else
	{
		trap->Argv( 1, arg, sizeof( arg ) );
		trap->Argv( 2, name, sizeof( name ) );
		id = atoi( arg );

		if ( !StringIsInteger( arg ) || id < 0 || id >= MAX_ENTITIESTOTAL || !g_entities[id].inuse )
		{
			trap->SendServerCommand( ent - g_entities, va( "print \"There is no entity %s.\n\"", RP_ShownText( arg ) ) );
			return;
		}
		target = &g_entities[id];
	}

	none = !Q_stricmp( name, "none" ) ? qtrue : qfalse;
	if ( !none && !RP_LockNameValid( name ) )
	{
		trap->SendServerCommand( ent - g_entities, "print \"A lock name is 1 to 31 letters, digits, _ or -, and not none (that means no lock).\n\"" );
		return;
	}

	// DAJ_RP: [Entity System] a character; a misc_weapon_shooter, whose client is only its pool's, can carry a
	// lock as /entedit's "lock" key always let it (RP_EntityIsCharacter, g_misc.c)
	if ( RP_EntityIsCharacter( target ) )
	{
		trap->SendServerCommand( ent - g_entities, va( "print \"Entity %d is a player, an NPC or a vehicle: it cannot carry a lock.\n\"", id ) );
		return;
	}

	// DAJ_RP: [Locks] nor one an admin is holding with /entcopy or /entcut (g_entgrab.c), as /entedit refuses it
	if ( target->rpHeldBy )
	{
		// DAJ_RP: [Entity System] by another admin: who, and with what (g_entgrab.c)
		trap->SendServerCommand( ent - g_entities, va( "print \"Entity %d is %s: wait until they drop it or let go.\n\"", id, RP_EntHeldByText( target ) ) );
		return;
	}

	if ( RP_LockHarmful( target ) && !none )
	{
		trap->SendServerCommand( ent - g_entities, va( "print \"Entity %d (%s) hurts whoever touches or uses it: a lock would only spare the players not on its list, so it cannot carry one.\n\"", id, target->classname ) );
		return;
	}

	if ( target->rpSubBSPOf > 0 )
	{
		trap->SendServerCommand( ent - g_entities, va( "print \"Entity %d comes from a misc_bsp, which makes it again whenever it spawns: it cannot carry a lock.\n\"", id ) );
		return;
	}

	if ( target->rpMapEntity )
	{
		int model = RP_LockBrushModel( target->model );
		rpLockModel_t *line;

		if ( model < 0 )
		{
			if ( RP_MapEntityExempt( target ) )
				trap->SendServerCommand( ent - g_entities, va( "print \"Entity %d is the map's own and has no brush model to note the lock by. It may be edited (E in /entlist): ^3/entedit %d lock <lock>^7 gives it the key, and makes it an ordinary entity.\n\"", id, id ) );
			else
				trap->SendServerCommand( ent - g_entities, va( "print \"Entity %d is the map's own and has no brush model to note the lock by: it cannot carry one.\n\"", id ) );
			return;
		}

		line = RP_LockModelFind( model );
		if ( none )
		{
			if ( !line )
			{
				trap->SendServerCommand( ent - g_entities, va( "print \"Entity %d carries no lock.\n\"", id ) );
				return;
			}
			*line = rp_lockModels[--rp_numLockModels];
		}
		else
		{
			if ( !line )
			{
				if ( rp_numLockModels >= RP_LOCK_MAX_MODELS )
				{
					trap->SendServerCommand( ent - g_entities, va( "print \"This map has %d of its own entities locked already, the most it can have.\n\"", RP_LOCK_MAX_MODELS ) );
					return;
				}
				line = &rp_lockModels[rp_numLockModels++];
				line->model = model;
			}
			Q_strncpyz( line->lock, name, sizeof( line->lock ) );
		}

		if ( !RP_LocksSave() )
			trap->SendServerCommand( ent - g_entities, "print \"^1The lock file could not be written: the change lasts until the map changes.\n\"" );
	}
	else if ( RP_EntitySystemMade( target ) )
	{
		if ( none )
		{
			if ( !target->rpLock || !target->rpLock[0] )
			{
				trap->SendServerCommand( ent - g_entities, va( "print \"Entity %d carries no lock.\n\"", id ) );
				return;
			}
			target->rpLock = NULL;
			zyk_main_set_entity_field( target, "lock", "zykremovekey" );
		}
		else
		{
			const char *stored;

			zyk_main_set_entity_field( target, "lock", name );
			stored = RP_LockRecordValue( target );
			if ( !stored || strcmp( stored, name ) )
			{	// zyk_main_set_entity_field drops a new key on a full record
				trap->SendServerCommand( ent - g_entities, va( "print \"Entity %d holds the most keys an entity can: there is no room for the lock key.\n\"", id ) );
				return;
			}
			target->rpLock = (char *)stored;	// the record's own copy: no new one for every /entlockset
		}
	}
	else
	{
		trap->SendServerCommand( ent - g_entities, va( "print \"Entity %d was made by the game (G in /entlist): it cannot carry a lock.\n\"", id ) );
		return;
	}

	G_LogPrintf( "/entlockset %d %s by %s\n", id, none ? "none" : name, ent->client->pers.netname );

	if ( none )
	{
		trap->SendServerCommand( ent - g_entities, va( "print \"Entity %d (%s) carries no lock now.\n\"", id, RP_ShownText( target->classname ? target->classname : "" ) ) );
		return;
	}

	trap->SendServerCommand( ent - g_entities, va( "print \"Entity %d (%s) carries lock ^3%s^7%s.\n\"", id, RP_ShownText( target->classname ? target->classname : "" ), name,
		RP_LockFind( name ) ? "" : " -- which has no list yet, so nobody gets through: ^3/entlockadd^7 the accounts" ) );
}

/*
==================
Cmd_EntTrigger_f

DAJ_RP: [Locks] /enttrigger [entity id]: activates that entity -- or, with no id, the one aimed at, as
/entedit and /entremove pick theirs (RP_EntAimTarget: a door's own trigger means the door) -- as a
level 5 Stun Baton does (RP_StunBatonUseMover): a door, a lift or a button is made active again, the map's
own locked ones are unlocked until the map reloads, a door that started open and closed for good is opened
again, and then the mover is used; anything else is used, as /entuse uses it. Account locks do not stop it,
nor what it sets off. Not a player, an NPC or a vehicle (using a vehicle boards it from anywhere), nor an
entity held with /entcopy or /entcut, nor one that does nothing when used; and not while the admin holds
one (RP_EntRefuseWhileHolding).
==================
*/
void Cmd_EntTrigger_f( gentity_t *ent )
{
	char arg[MAX_STRING_CHARS];
	gentity_t *target;
	int id;

	if ( !check_admin_command( ent, ADM_ENTITYSYSTEM, qtrue ) )
		return;

	if ( RP_EntRefuseWhileHolding( ent ) )
		return;

	if ( trap->Argc() > 2 )
	{
		trap->SendServerCommand( ent - g_entities, "print \"Usage: ^3/enttrigger^7 activates the entity you aim at, ^3/enttrigger <entity id>^7 that entity, as a level 5 Stun Baton would: doors, lifts and buttons open even when locked or inactive; anything else is used. Account locks do not stop it.\n\"" );
		return;
	}

	if ( trap->Argc() < 2 )
	{	// DAJ_RP: [Locks] the entity aimed at, as /entedit and /entremove pick theirs (RP_EntAimTarget): a
		// door's own trigger means the door
		if ( RP_EntAimFollowing( ent ) )
		{
			trap->SendServerCommand( ent - g_entities, "print \"You are following another player. Stop following first, or give the entity id.\n\"" );
			return;
		}

		target = RP_EntAimTarget( ent );
		if ( !target || !target->inuse )
		{
			trap->SendServerCommand( ent - g_entities, "print \"You are not aiming at an entity. Aim at one, or give its id: ^3/enttrigger <entity id>^7.\n\"" );
			return;
		}
		id = target->s.number;
	}
	else
	{
		trap->Argv( 1, arg, sizeof( arg ) );
		id = atoi( arg );

		if ( !StringIsInteger( arg ) || id < 0 || id >= MAX_ENTITIESTOTAL || !g_entities[id].inuse )
		{
			trap->SendServerCommand( ent - g_entities, va( "print \"There is no entity %s.\n\"", RP_ShownText( arg ) ) );
			return;
		}
		target = &g_entities[id];
	}

	// DAJ_RP: [Entity System] a character; a misc_weapon_shooter is used, and fires (RP_EntityIsCharacter)
	if ( RP_EntityIsCharacter( target ) )
	{
		trap->SendServerCommand( ent - g_entities, va( "print \"Entity %d is a player, an NPC or a vehicle: it is left alone.\n\"", id ) );
		return;
	}
	if ( target->rpHeldBy )
	{
		// DAJ_RP: [Entity System] by another admin: who, and with what (g_entgrab.c)
		trap->SendServerCommand( ent - g_entities, va( "print \"Entity %d is %s: it is left alone.\n\"", id, RP_EntHeldByText( target ) ) );
		return;
	}
	if ( target->s.eType != ET_MOVER && !target->use )
	{
		trap->SendServerCommand( ent - g_entities, va( "print \"Entity %d (%s) does nothing when used.\n\"", id, RP_ShownText( target->classname ? target->classname : "" ) ) );
		return;
	}

	RP_LockBypass( qtrue );
	if ( target->s.eType == ET_MOVER )
		RP_StunBatonUseMover( target, ent );
	else
	{
		RP_LockMarkPass( target, ent );	// what it sets off later passes too
		target->use( target, ent, ent );
	}
	RP_LockBypass( qfalse );

	G_LogPrintf( "/enttrigger %d by %s\n", id, ent->client->pers.netname );
	trap->SendServerCommand( ent - g_entities, va( "print \"Triggered entity %d (%s).\n\"", id, RP_ShownText( target->inuse && target->classname ? target->classname : "" ) ) );
}
