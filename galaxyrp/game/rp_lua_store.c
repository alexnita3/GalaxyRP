/*
===========================================================================
GLX_U: [Lua] Storage: what scripts keep across maps and restarts, in GalaxyRP/database/lua.db.

One table, LuaData(plugin, scope, key, value, updated): scope is "global" or "char:<CharID>". The
accounts database is never written here -- only read, for the orphan sweep below. lua.db is opened
when a map starts with any plugin or map script set to run, or by /scripts enable and reload --
outside any script's time budget -- so a server with no scripts never gets the file. It runs in
WAL mode with synchronous=NORMAL: it cannot be corrupted by a crash, and a power cut loses at most
the last batch of writes.

Writes do not touch the disk at once: Storage.set puts the value in a pending table that is
written in one transaction every 30 s, at shutdown and before /scripts reload, so no frame waits on
a disk sync. Storage.get reads the pending table first, then the database.

Values are kept in a small binary format of our own (RP_LuaStore_Write / RP_LuaStore_Read), read
back with bounds checks and never through the Lua parser. Limits: rp_lua_maxvalue bytes per value,
rp_lua_maxstore bytes per plugin (key + value + 32 bytes per entry), keys of 1-64 characters,
tables nested at most 16 deep, no cycles, no functions or userdata.

CharID is reused by SQLite after a character is deleted, so remove_character() drops that
character's rows (RP_LuaStore_CharacterRemoved) and each map's open sweeps rows of characters
that no longer exist.
===========================================================================
*/

#include "rp_lua_local.h"
#include "../../shared/sqlite/sqlite3.h"
#include <stdlib.h>
#include <math.h>

#define RP_LUA_DB_PATH			"GalaxyRP/database/lua.db"
#define RP_LUA_STORE_HARDMAX	( 1024 * 1024 )		// rp_lua_maxvalue can never go above this
#define RP_LUA_STORE_DEPTH		16
#define RP_LUA_STORE_KEYMAX		64
#define RP_LUA_STORE_OVERHEAD	32
#define RP_LUA_FLUSH_MS			30000
#define RP_LUA_PEND_BUCKETS		4096

#define RPS_MAGIC		'G'
#define RPS_VERSION		1
enum { RPS_FALSE = 1, RPS_TRUE, RPS_INT, RPS_FLOAT, RPS_STRING, RPS_TABLE };

typedef struct rpLuaPend_s {
	struct rpLuaPend_s	*next;
	char				*plugin;
	char				*scope;
	char				*key;
	unsigned char		*blob;		// NULL: delete
	int					len;
} rpLuaPend_t;

static sqlite3			*rpDb;
static qboolean			rpDbTried;
static sqlite3_stmt		*stGet, *stLen, *stPut, *stDel, *stSum;
static int				rpLastFlush;
static rpLuaPend_t		*rpPend[RP_LUA_PEND_BUCKETS];
static int				rpPendCount;
static unsigned char	rpWriteBuf[RP_LUA_STORE_HARDMAX];
static unsigned char	rpReadBuf[RP_LUA_STORE_HARDMAX];

static int RP_LuaStore_MaxValue( void ) {
	int v = rp_lua_maxvalue.integer;
	return v < 1024 ? 1024 : ( v > RP_LUA_STORE_HARDMAX ? RP_LUA_STORE_HARDMAX : v );
}

static int RP_LuaStore_MaxStore( void ) {
	int v = rp_lua_maxstore.integer;
	return v < 65536 ? 65536 : ( v > 256 * 1024 * 1024 ? 256 * 1024 * 1024 : v );
}

static void RP_LuaStore_Path( char *out, int outSize ) {
	char home[MAX_OSPATH] = { 0 };
	trap->Cvar_VariableStringBuffer( "fs_homepath", home, sizeof( home ) );
	if ( home[0] ) {
		Com_sprintf( out, outSize, "%s/%s", home, RP_LUA_DB_PATH );
	} else {
		Q_strncpyz( out, RP_LUA_DB_PATH, outSize );
	}
}

/*
================
Pending writes
================
*/
static unsigned RP_LuaStore_Hash( const char *plugin, const char *scope, const char *key ) {
	unsigned h = 2166136261u;
	const char *parts[3];
	int i;

	parts[0] = plugin;
	parts[1] = scope;
	parts[2] = key;
	for ( i = 0; i < 3; i++ ) {
		const char *s;
		for ( s = parts[i]; *s; s++ ) {
			h = ( h ^ (unsigned char)*s ) * 16777619u;
		}
		h = ( h ^ 0xFF ) * 16777619u;
	}
	return h % RP_LUA_PEND_BUCKETS;
}

static rpLuaPend_t *RP_LuaStore_FindPend( const char *plugin, const char *scope, const char *key ) {
	rpLuaPend_t *p;
	for ( p = rpPend[RP_LuaStore_Hash( plugin, scope, key )]; p; p = p->next ) {
		if ( !strcmp( p->plugin, plugin ) && !strcmp( p->scope, scope ) && !strcmp( p->key, key ) ) {
			return p;
		}
	}
	return NULL;
}

static char *RP_LuaStore_Dup( const char *s ) {
	size_t n = strlen( s ) + 1;
	char *d = (char *)malloc( n );
	if ( d ) {
		memcpy( d, s, n );
	}
	return d;
}

static void RP_LuaStore_FreePend( rpLuaPend_t *p ) {
	free( p->plugin );
	free( p->scope );
	free( p->key );
	free( p->blob );
	free( p );
}

// GLX_U: forgets a pending write, as if it had never been made (Storage.set(key, nil) of a key that is not in the
// database: there is nothing to delete)
static void RP_LuaStore_DropPend( const char *plugin, const char *scope, const char *key ) {
	rpLuaPend_t **link = &rpPend[RP_LuaStore_Hash( plugin, scope, key )];

	for ( ; *link; link = &( *link )->next ) {
		rpLuaPend_t *p = *link;
		if ( !strcmp( p->plugin, plugin ) && !strcmp( p->scope, scope ) && !strcmp( p->key, key ) ) {
			*link = p->next;
			RP_LuaStore_FreePend( p );
			rpPendCount--;
			return;
		}
	}
}

static void RP_LuaStore_ClearPend( void ) {
	int i;
	for ( i = 0; i < RP_LUA_PEND_BUCKETS; i++ ) {
		while ( rpPend[i] ) {
			rpLuaPend_t *p = rpPend[i];
			rpPend[i] = p->next;
			RP_LuaStore_FreePend( p );
		}
	}
	rpPendCount = 0;
}

// qfalse: out of memory (the caller raises the script error)
static qboolean RP_LuaStore_SetPend( const char *plugin, const char *scope, const char *key, const unsigned char *blob, int len ) {
	rpLuaPend_t *p = RP_LuaStore_FindPend( plugin, scope, key );
	unsigned char *copy = NULL;

	if ( blob ) {
		copy = (unsigned char *)malloc( len > 0 ? len : 1 );
		if ( !copy ) {
			return qfalse;
		}
		memcpy( copy, blob, len );
	}
	if ( !p ) {
		unsigned h = RP_LuaStore_Hash( plugin, scope, key );
		p = (rpLuaPend_t *)calloc( 1, sizeof( *p ) );
		if ( !p ) {
			free( copy );
			return qfalse;
		}
		p->plugin = RP_LuaStore_Dup( plugin );
		p->scope = RP_LuaStore_Dup( scope );
		p->key = RP_LuaStore_Dup( key );
		if ( !p->plugin || !p->scope || !p->key ) {
			free( copy );
			RP_LuaStore_FreePend( p );
			return qfalse;
		}
		p->next = rpPend[h];
		rpPend[h] = p;
		rpPendCount++;
	}
	free( p->blob );
	p->blob = copy;
	p->len = blob ? len : 0;
	return qtrue;
}

int RP_LuaStore_PendingCount( void ) {
	return rpPendCount;
}

/*
================
The database
================
*/
static void RP_LuaStore_Finalize( void ) {
	sqlite3_finalize( stGet );
	sqlite3_finalize( stLen );
	sqlite3_finalize( stPut );
	sqlite3_finalize( stDel );
	sqlite3_finalize( stSum );
	stGet = stLen = stPut = stDel = stSum = NULL;
}

static void RP_LuaStore_SweepOrphans( void ) {
	sqlite3 *acc = NULL;
	sqlite3_stmt *sel = NULL, *ins = NULL;
	int rc, removed;

	if ( RP_DB_Open( &acc ) != SQLITE_OK ) {
		trap->Print( S_COLOR_YELLOW "Lua: could not read the accounts database, character data sweep skipped\n" );
		sqlite3_close( acc );
		return;
	}
	if ( sqlite3_prepare_v2( acc, "SELECT CharID FROM Characters", -1, &sel, NULL ) != SQLITE_OK
		|| sqlite3_exec( rpDb, "CREATE TEMP TABLE IF NOT EXISTS LuaLiveChars(id INTEGER PRIMARY KEY); DELETE FROM LuaLiveChars;", 0, 0, NULL ) != SQLITE_OK
		|| sqlite3_prepare_v2( rpDb, "INSERT OR IGNORE INTO LuaLiveChars(id) VALUES(?)", -1, &ins, NULL ) != SQLITE_OK
		|| sqlite3_exec( rpDb, "BEGIN", 0, 0, NULL ) != SQLITE_OK ) {
		trap->Print( S_COLOR_YELLOW "Lua: character data sweep skipped (%s)\n", sqlite3_errmsg( rpDb ) );
		sqlite3_finalize( sel );
		sqlite3_finalize( ins );
		sqlite3_close( acc );
		return;
	}
	while ( ( rc = sqlite3_step( sel ) ) == SQLITE_ROW ) {
		sqlite3_bind_int( ins, 1, sqlite3_column_int( sel, 0 ) );
		if ( sqlite3_step( ins ) != SQLITE_DONE ) {
			rc = SQLITE_ERROR;
			break;
		}
		sqlite3_reset( ins );
	}
	sqlite3_finalize( sel );
	sqlite3_finalize( ins );
	sqlite3_close( acc );
	// delete only when the whole character list was read: a failed read must never look like
	// "no characters exist"
	if ( rc != SQLITE_DONE
		|| sqlite3_exec( rpDb, "DELETE FROM LuaData WHERE scope LIKE 'char:%' AND CAST(substr(scope, 6) AS INTEGER) NOT IN (SELECT id FROM LuaLiveChars)", 0, 0, NULL ) != SQLITE_OK ) {
		sqlite3_exec( rpDb, "ROLLBACK", 0, 0, NULL );
		trap->Print( S_COLOR_YELLOW "Lua: character data sweep skipped (%s)\n", sqlite3_errmsg( rpDb ) );
		return;
	}
	removed = sqlite3_changes( rpDb );
	sqlite3_exec( rpDb, "COMMIT", 0, 0, NULL );
	if ( removed > 0 ) {
		trap->Print( "Lua: removed %d stored value(s) of characters that no longer exist\n", removed );
	}
}

static qboolean RP_LuaStore_Ensure( void ) {
	char path[MAX_OSPATH * 2];

	if ( rpDb ) {
		return qtrue;
	}
	if ( rpDbTried ) {
		return qfalse;
	}
	rpDbTried = qtrue;
	RP_LuaStore_Path( path, sizeof( path ) );
	if ( sqlite3_open_v2( path, &rpDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL ) != SQLITE_OK ) {
		trap->Print( S_COLOR_RED "Lua: could not open %s: %s\n", path, rpDb ? sqlite3_errmsg( rpDb ) : "out of memory" );
		sqlite3_close( rpDb );
		rpDb = NULL;
		return qfalse;
	}
	sqlite3_busy_timeout( rpDb, 2000 );
	sqlite3_exec( rpDb, "PRAGMA journal_mode=WAL;", 0, 0, NULL );
	sqlite3_exec( rpDb, "PRAGMA synchronous=NORMAL;", 0, 0, NULL );
	if ( sqlite3_exec( rpDb, "CREATE TABLE IF NOT EXISTS LuaData(plugin TEXT NOT NULL, scope TEXT NOT NULL, key TEXT NOT NULL, value BLOB NOT NULL, updated INTEGER NOT NULL, PRIMARY KEY(plugin, scope, key))", 0, 0, NULL ) != SQLITE_OK
		|| sqlite3_prepare_v2( rpDb, "SELECT value FROM LuaData WHERE plugin=? AND scope=? AND key=?", -1, &stGet, NULL ) != SQLITE_OK
		|| sqlite3_prepare_v2( rpDb, "SELECT length(key) + length(value) FROM LuaData WHERE plugin=? AND scope=? AND key=?", -1, &stLen, NULL ) != SQLITE_OK
		|| sqlite3_prepare_v2( rpDb, "INSERT OR REPLACE INTO LuaData(plugin, scope, key, value, updated) VALUES(?, ?, ?, ?, strftime('%s','now'))", -1, &stPut, NULL ) != SQLITE_OK
		|| sqlite3_prepare_v2( rpDb, "DELETE FROM LuaData WHERE plugin=? AND scope=? AND key=?", -1, &stDel, NULL ) != SQLITE_OK
		|| sqlite3_prepare_v2( rpDb, "SELECT COUNT(*), COALESCE(SUM(length(key) + length(value)), 0) FROM LuaData WHERE plugin=?", -1, &stSum, NULL ) != SQLITE_OK ) {
		trap->Print( S_COLOR_RED "Lua: could not set up %s: %s\n", path, sqlite3_errmsg( rpDb ) );
		RP_LuaStore_Finalize();
		sqlite3_close( rpDb );
		rpDb = NULL;
		return qfalse;
	}
	rpLastFlush = trap->Milliseconds();
	RP_LuaStore_SweepOrphans();
	if ( rl.depth > 0 ) {
		rl.callStart = trap->Milliseconds();	// opening the file is not the script's time
	}
	return qtrue;
}

qboolean RP_LuaStore_Open( void ) {
	return RP_LuaStore_Ensure();
}

static void RP_LuaStore_Bind3( sqlite3_stmt *st, const char *plugin, const char *scope, const char *key ) {
	sqlite3_bind_text( st, 1, plugin, -1, SQLITE_STATIC );
	sqlite3_bind_text( st, 2, scope, -1, SQLITE_STATIC );
	sqlite3_bind_text( st, 3, key, -1, SQLITE_STATIC );
}

qboolean RP_LuaStore_Flush( void ) {
	int i, written = 0;

	if ( !rpDb || !rpPendCount ) {
		return qtrue;
	}
	if ( sqlite3_exec( rpDb, "BEGIN IMMEDIATE", 0, 0, NULL ) != SQLITE_OK ) {
		trap->Print( S_COLOR_YELLOW "Lua: could not save script data now, will retry (%s)\n", sqlite3_errmsg( rpDb ) );
		return qfalse;
	}
	for ( i = 0; i < RP_LUA_PEND_BUCKETS; i++ ) {
		rpLuaPend_t *p;
		for ( p = rpPend[i]; p; p = p->next ) {
			sqlite3_stmt *st = p->blob ? stPut : stDel;
			int rc;
			RP_LuaStore_Bind3( st, p->plugin, p->scope, p->key );
			if ( p->blob ) {
				sqlite3_bind_blob( st, 4, p->blob, p->len, SQLITE_STATIC );
			}
			rc = sqlite3_step( st );
			sqlite3_reset( st );
			sqlite3_clear_bindings( st );
			if ( rc != SQLITE_DONE ) {
				trap->Print( S_COLOR_YELLOW "Lua: could not save script data now, will retry (%s)\n", sqlite3_errmsg( rpDb ) );
				sqlite3_exec( rpDb, "ROLLBACK", 0, 0, NULL );
				return qfalse;
			}
			written++;
		}
	}
	if ( sqlite3_exec( rpDb, "COMMIT", 0, 0, NULL ) != SQLITE_OK ) {
		trap->Print( S_COLOR_YELLOW "Lua: could not save script data now, will retry (%s)\n", sqlite3_errmsg( rpDb ) );
		sqlite3_exec( rpDb, "ROLLBACK", 0, 0, NULL );
		return qfalse;
	}
	RP_LuaStore_ClearPend();
	(void)written;
	return qtrue;
}

void RP_LuaStore_Frame( void ) {
	if ( rpDb && trap->Milliseconds() - rpLastFlush >= RP_LUA_FLUSH_MS ) {
		rpLastFlush = trap->Milliseconds();
		RP_LuaStore_Flush();
	}
}

void RP_LuaStore_Close( void ) {
	if ( rpDb ) {
		if ( !RP_LuaStore_Flush() ) {
			trap->Print( S_COLOR_RED "Lua: %d unsaved script value(s) were lost at map end\n", rpPendCount );
			G_LogPrintf( "Lua: %d unsaved script value(s) were lost at map end\n", rpPendCount );
		}
		RP_LuaStore_Finalize();
		sqlite3_close( rpDb );
		rpDb = NULL;
	}
	RP_LuaStore_ClearPend();
	rpDbTried = qfalse;
}

void RP_LuaStore_CharacterRemoved( int charID ) {
	char scope[32], path[MAX_OSPATH * 2];
	sqlite3 *db = rpDb;
	int i;

	Com_sprintf( scope, sizeof( scope ), "char:%d", charID );
	for ( i = 0; i < RP_LUA_PEND_BUCKETS; i++ ) {
		rpLuaPend_t **pp = &rpPend[i];
		while ( *pp ) {
			if ( !strcmp( ( *pp )->scope, scope ) ) {
				rpLuaPend_t *dead = *pp;
				*pp = dead->next;
				RP_LuaStore_FreePend( dead );
				rpPendCount--;
			} else {
				pp = &( *pp )->next;
			}
		}
	}
	for ( i = 0; i <= RP_LUA_CONSOLE; i++ ) {
		rl.plugins[i].storeBytes = -1;	// recounted on next use
	}

	if ( !db ) {
		// no script used storage this map: open the file only if it exists, never create it
		RP_LuaStore_Path( path, sizeof( path ) );
		if ( sqlite3_open_v2( path, &db, SQLITE_OPEN_READWRITE, NULL ) != SQLITE_OK ) {
			sqlite3_close( db );
			return;
		}
		sqlite3_busy_timeout( db, 2000 );
	}
	{
		sqlite3_stmt *st = NULL;
		if ( sqlite3_prepare_v2( db, "DELETE FROM LuaData WHERE scope=?", -1, &st, NULL ) == SQLITE_OK ) {
			sqlite3_bind_text( st, 1, scope, -1, SQLITE_STATIC );
			if ( sqlite3_step( st ) == SQLITE_DONE && sqlite3_changes( db ) > 0 ) {
				trap->Print( "Lua: removed %d stored value(s) of deleted character %d\n", sqlite3_changes( db ), charID );
			}
		}
		sqlite3_finalize( st );
	}
	if ( db != rpDb ) {
		sqlite3_close( db );
	}
}

/*
================
The value format
================
*/
typedef struct {
	unsigned char	*b;
	int				len;
	int				cap;
	const void		*open[RP_LUA_STORE_DEPTH + 1];	// tables being written, for the cycle check
	int				depth;
} rpLuaWriter_t;

static void RP_LuaStore_Put( lua_State *L, rpLuaWriter_t *w, const void *data, int n ) {
	if ( w->len + n > w->cap ) {
		luaL_error( L, "the value is too big to store (the limit is %d bytes)", w->cap );
	}
	memcpy( w->b + w->len, data, n );
	w->len += n;
}

static void RP_LuaStore_PutByte( lua_State *L, rpLuaWriter_t *w, int v ) {
	unsigned char c = (unsigned char)v;
	RP_LuaStore_Put( L, w, &c, 1 );
}

static void RP_LuaStore_PutU32( lua_State *L, rpLuaWriter_t *w, unsigned v ) {
	unsigned char c[4];
	c[0] = v & 0xFF; c[1] = ( v >> 8 ) & 0xFF; c[2] = ( v >> 16 ) & 0xFF; c[3] = ( v >> 24 ) & 0xFF;
	RP_LuaStore_Put( L, w, c, 4 );
}

static void RP_LuaStore_PutU64( lua_State *L, rpLuaWriter_t *w, unsigned long long v ) {
	unsigned char c[8];
	int i;
	for ( i = 0; i < 8; i++ ) {
		c[i] = (unsigned char)( ( v >> ( 8 * i ) ) & 0xFF );
	}
	RP_LuaStore_Put( L, w, c, 8 );
}

static void RP_LuaStore_WriteValue( lua_State *L, rpLuaWriter_t *w, int idx, qboolean isKey ) {
	idx = lua_absindex( L, idx );
	switch ( lua_type( L, idx ) ) {
	case LUA_TBOOLEAN:
		RP_LuaStore_PutByte( L, w, lua_toboolean( L, idx ) ? RPS_TRUE : RPS_FALSE );
		break;
	case LUA_TNUMBER:
		if ( lua_isinteger( L, idx ) ) {
			RP_LuaStore_PutByte( L, w, RPS_INT );
			RP_LuaStore_PutU64( L, w, (unsigned long long)lua_tointeger( L, idx ) );
		} else {
			double d = (double)lua_tonumber( L, idx );
			unsigned long long u;
			memcpy( &u, &d, sizeof( u ) );
			RP_LuaStore_PutByte( L, w, RPS_FLOAT );
			RP_LuaStore_PutU64( L, w, u );
		}
		break;
	case LUA_TSTRING: {
		size_t n;
		const char *s = lua_tolstring( L, idx, &n );
		if ( n > (size_t)w->cap ) {
			luaL_error( L, "the value is too big to store (the limit is %d bytes)", w->cap );
		}
		RP_LuaStore_PutByte( L, w, RPS_STRING );
		RP_LuaStore_PutU32( L, w, (unsigned)n );
		RP_LuaStore_Put( L, w, s, (int)n );
		break;
	}
	case LUA_TTABLE: {
		const void *self = lua_topointer( L, idx );
		int countAt, count = 0, i;
		if ( isKey ) {
			luaL_error( L, "tables cannot be used as keys in a stored value" );
		}
		if ( w->depth >= RP_LUA_STORE_DEPTH ) {
			luaL_error( L, "a stored value can be nested at most %d tables deep", RP_LUA_STORE_DEPTH );
		}
		for ( i = 0; i < w->depth; i++ ) {
			if ( w->open[i] == self ) {
				luaL_error( L, "a table that contains itself cannot be stored" );
			}
		}
		luaL_checkstack( L, 4, "stored value too deep" );
		w->open[w->depth++] = self;
		RP_LuaStore_PutByte( L, w, RPS_TABLE );
		countAt = w->len;
		RP_LuaStore_PutU32( L, w, 0 );
		lua_pushnil( L );
		while ( lua_next( L, idx ) ) {
			RP_LuaStore_WriteValue( L, w, -2, qtrue );
			RP_LuaStore_WriteValue( L, w, -1, qfalse );
			lua_pop( L, 1 );
			count++;
		}
		w->b[countAt] = count & 0xFF;
		w->b[countAt + 1] = ( count >> 8 ) & 0xFF;
		w->b[countAt + 2] = ( count >> 16 ) & 0xFF;
		w->b[countAt + 3] = ( count >> 24 ) & 0xFF;
		w->depth--;
		break;
	}
	default:
		luaL_error( L, "a %s cannot be stored (only nil, booleans, numbers, strings and tables of them)", luaL_typename( L, idx ) );
	}
}

typedef struct {
	const unsigned char	*b;
	int					len;
	int					pos;
} rpLuaReader_t;

static qboolean RP_LuaStore_Get( rpLuaReader_t *r, void *out, int n ) {
	if ( n < 0 || r->pos + n > r->len ) {
		return qfalse;
	}
	memcpy( out, r->b + r->pos, n );
	r->pos += n;
	return qtrue;
}

static qboolean RP_LuaStore_GetU32( rpLuaReader_t *r, unsigned *v ) {
	unsigned char c[4];
	if ( !RP_LuaStore_Get( r, c, 4 ) ) {
		return qfalse;
	}
	*v = c[0] | ( c[1] << 8 ) | ( c[2] << 16 ) | ( (unsigned)c[3] << 24 );
	return qtrue;
}

static qboolean RP_LuaStore_GetU64( rpLuaReader_t *r, unsigned long long *v ) {
	unsigned char c[8];
	int i;
	if ( !RP_LuaStore_Get( r, c, 8 ) ) {
		return qfalse;
	}
	*v = 0;
	for ( i = 7; i >= 0; i-- ) {
		*v = ( *v << 8 ) | c[i];
	}
	return qtrue;
}

// pushes one value; qfalse when the data is damaged (whatever it pushed is left for the caller to drop)
static qboolean RP_LuaStore_ReadValue( lua_State *L, rpLuaReader_t *r, int depth, qboolean isKey ) {
	unsigned char tag;
	unsigned long long u;
	unsigned n, i;

	if ( !RP_LuaStore_Get( r, &tag, 1 ) ) {
		return qfalse;
	}
	luaL_checkstack( L, 3, "stored value too deep" );
	switch ( tag ) {
	case RPS_FALSE:
	case RPS_TRUE:
		lua_pushboolean( L, tag == RPS_TRUE );
		return qtrue;
	case RPS_INT:
		if ( !RP_LuaStore_GetU64( r, &u ) ) {
			return qfalse;
		}
		lua_pushinteger( L, (lua_Integer)u );
		return qtrue;
	case RPS_FLOAT: {
		double d;
		if ( !RP_LuaStore_GetU64( r, &u ) ) {
			return qfalse;
		}
		memcpy( &d, &u, sizeof( d ) );
		if ( isKey && d != d ) {
			return qfalse;	// NaN cannot be a key
		}
		lua_pushnumber( L, (lua_Number)d );
		return qtrue;
	}
	case RPS_STRING:
		if ( !RP_LuaStore_GetU32( r, &n ) || n > (unsigned)( r->len - r->pos ) ) {
			return qfalse;
		}
		lua_pushlstring( L, (const char *)r->b + r->pos, n );
		r->pos += n;
		return qtrue;
	case RPS_TABLE:
		if ( isKey || depth >= RP_LUA_STORE_DEPTH || !RP_LuaStore_GetU32( r, &n ) || n > (unsigned)( r->len - r->pos ) / 2 ) {
			return qfalse;
		}
		lua_createtable( L, 0, n > 64 ? 64 : (int)n );
		for ( i = 0; i < n; i++ ) {
			if ( !RP_LuaStore_ReadValue( L, r, depth + 1, qtrue ) || !RP_LuaStore_ReadValue( L, r, depth + 1, qfalse ) ) {
				return qfalse;
			}
			lua_rawset( L, -3 );
		}
		return qtrue;
	default:
		return qfalse;
	}
}

static void RP_LuaStore_PushBlob( lua_State *L, const unsigned char *b, int len, const char *plugin, const char *key ) {
	rpLuaReader_t r;
	int top = lua_gettop( L );

	r.b = b;
	r.len = len;
	r.pos = 2;
	if ( len >= 2 && b[0] == RPS_MAGIC && b[1] == RPS_VERSION && RP_LuaStore_ReadValue( L, &r, 0, qfalse ) && r.pos == r.len ) {
		return;
	}
	lua_settop( L, top );
	trap->Print( S_COLOR_YELLOW "Lua: stored value \"%s\" of %s is damaged and was read as nil\n", key, plugin );
	lua_pushnil( L );
}

/*
================
Get and set
================
*/
static int RP_LuaStore_DbSize( const char *plugin, const char *scope, const char *key ) {
	int size = 0;
	if ( !rpDb ) {
		return 0;
	}
	RP_LuaStore_Bind3( stLen, plugin, scope, key );
	if ( sqlite3_step( stLen ) == SQLITE_ROW ) {
		size = sqlite3_column_int( stLen, 0 ) + RP_LUA_STORE_OVERHEAD;
	}
	sqlite3_reset( stLen );
	sqlite3_clear_bindings( stLen );
	return size;
}

static int RP_LuaStore_PluginBytes( int plugin ) {
	rpLuaPlugin_t *p = &rl.plugins[plugin];
	int total = 0, i;

	if ( p->storeBytes >= 0 ) {
		return p->storeBytes;
	}
	sqlite3_bind_text( stSum, 1, p->name, -1, SQLITE_STATIC );
	if ( sqlite3_step( stSum ) == SQLITE_ROW ) {
		total = sqlite3_column_int( stSum, 1 ) + sqlite3_column_int( stSum, 0 ) * RP_LUA_STORE_OVERHEAD;
	}
	sqlite3_reset( stSum );
	sqlite3_clear_bindings( stSum );
	// what is waiting to be written replaces what is in the database
	for ( i = 0; i < RP_LUA_PEND_BUCKETS; i++ ) {
		rpLuaPend_t *e;
		for ( e = rpPend[i]; e; e = e->next ) {
			if ( strcmp( e->plugin, p->name ) ) {
				continue;
			}
			total -= RP_LuaStore_DbSize( e->plugin, e->scope, e->key );
			if ( e->blob ) {
				total += (int)strlen( e->key ) + e->len + RP_LUA_STORE_OVERHEAD;
			}
		}
	}
	p->storeBytes = total < 0 ? 0 : total;
	return p->storeBytes;
}

static const char *RP_LuaStore_CheckKey( lua_State *L, int idx ) {
	size_t n;
	const char *key = luaL_checklstring( L, idx, &n );
	if ( n < 1 || n > RP_LUA_STORE_KEYMAX || strlen( key ) != n ) {
		luaL_error( L, "a storage key is a string of 1 to %d characters", RP_LUA_STORE_KEYMAX );
	}
	return key;
}

static int RP_LuaStore_DoGet( lua_State *L, int plugin, const char *scope, const char *key ) {
	rpLuaPend_t *e;
	const char *name = rl.plugins[plugin].name;

	if ( !RP_LuaStore_Ensure() ) {
		return luaL_error( L, "script storage is not available (see the server log)" );
	}
	e = RP_LuaStore_FindPend( name, scope, key );
	if ( e ) {
		if ( !e->blob ) {
			lua_pushnil( L );
		} else {
			RP_LuaStore_PushBlob( L, e->blob, e->len, name, key );
		}
		return 1;
	}
	RP_LuaStore_Bind3( stGet, name, scope, key );
	if ( sqlite3_step( stGet ) == SQLITE_ROW ) {
		int len = sqlite3_column_bytes( stGet, 0 );
		const void *b = sqlite3_column_blob( stGet, 0 );
		if ( len > RP_LUA_STORE_HARDMAX || !b ) {
			len = -1;
		} else {
			memcpy( rpReadBuf, b, len );
		}
		sqlite3_reset( stGet );
		sqlite3_clear_bindings( stGet );
		if ( len < 0 ) {
			trap->Print( S_COLOR_YELLOW "Lua: stored value \"%s\" of %s is damaged and was read as nil\n", key, name );
			lua_pushnil( L );
		} else {
			RP_LuaStore_PushBlob( L, rpReadBuf, len, name, key );
		}
		return 1;
	}
	sqlite3_reset( stGet );
	sqlite3_clear_bindings( stGet );
	lua_pushnil( L );
	return 1;
}

static int RP_LuaStore_DoSet( lua_State *L, int plugin, const char *scope, const char *key, int valueIdx ) {
	rpLuaPlugin_t *p = &rl.plugins[plugin];
	rpLuaWriter_t w;
	rpLuaPend_t *e;
	int oldSize, newSize, total;
	qboolean del = lua_isnoneornil( L, valueIdx ) ? qtrue : qfalse;

	if ( !RP_LuaStore_Ensure() ) {
		return luaL_error( L, "script storage is not available (see the server log)" );
	}
	memset( &w, 0, sizeof( w ) );
	if ( !del ) {
		w.b = rpWriteBuf;
		w.cap = RP_LuaStore_MaxValue();
		RP_LuaStore_PutByte( L, &w, RPS_MAGIC );
		RP_LuaStore_PutByte( L, &w, RPS_VERSION );
		RP_LuaStore_WriteValue( L, &w, valueIdx, qfalse );
	}

	total = RP_LuaStore_PluginBytes( plugin );
	e = RP_LuaStore_FindPend( p->name, scope, key );
	if ( e ) {
		oldSize = e->blob ? (int)strlen( key ) + e->len + RP_LUA_STORE_OVERHEAD : 0;
	} else {
		oldSize = RP_LuaStore_DbSize( p->name, scope, key );
	}
	// GLX_U: a delete of a key the database does not have writes nothing: before, every one became a pending
	// entry (even for keys that never existed, and with no limit), all flushed as DELETEs in one frame. A write
	// still waiting is simply forgotten.
	if ( del && ( e ? RP_LuaStore_DbSize( p->name, scope, key ) : oldSize ) == 0 ) {
		if ( e ) {
			RP_LuaStore_DropPend( p->name, scope, key );
		}
		p->storeBytes = total - oldSize;
		return 0;
	}
	newSize = del ? 0 : (int)strlen( key ) + w.len + RP_LUA_STORE_OVERHEAD;
	if ( newSize > oldSize && total - oldSize + newSize > RP_LuaStore_MaxStore() ) {
		return luaL_error( L, "storage is full for %s (the limit is %d bytes per plugin)", p->name, RP_LuaStore_MaxStore() );
	}
	if ( !RP_LuaStore_SetPend( p->name, scope, key, del ? NULL : w.b, w.len ) ) {
		return luaL_error( L, "out of memory saving \"%s\"", key );
	}
	p->storeBytes = total - oldSize + newSize;
	return 0;
}

static int RP_LuaStore_LGet( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	return RP_LuaStore_DoGet( L, plugin, "global", RP_LuaStore_CheckKey( L, 1 ) );
}

static int RP_LuaStore_LSet( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	const char *key = RP_LuaStore_CheckKey( L, 1 );
	lua_settop( L, 2 );
	return RP_LuaStore_DoSet( L, plugin, "global", key, 2 );
}

#define RP_LUA_CHARSTORE_MT	"GLX.CharStore"

typedef struct {
	int		plugin;
	int		charID;
} rpLuaCharStore_t;

static rpLuaCharStore_t *RP_LuaStore_CheckCharStore( lua_State *L ) {
	rpLuaCharStore_t *cs = (rpLuaCharStore_t *)luaL_checkudata( L, 1, RP_LUA_CHARSTORE_MT );
	if ( cs->plugin != RP_Lua_CurrentPlugin( L ) ) {
		luaL_error( L, "this character store belongs to another plugin" );
	}
	return cs;
}

static int RP_LuaStore_CharGet( lua_State *L ) {
	rpLuaCharStore_t *cs = RP_LuaStore_CheckCharStore( L );
	const char *key = RP_LuaStore_CheckKey( L, 2 );
	char scope[32];
	Com_sprintf( scope, sizeof( scope ), "char:%d", cs->charID );
	return RP_LuaStore_DoGet( L, cs->plugin, scope, key );
}

static int RP_LuaStore_CharSet( lua_State *L ) {
	rpLuaCharStore_t *cs = RP_LuaStore_CheckCharStore( L );
	const char *key = RP_LuaStore_CheckKey( L, 2 );
	char scope[32];
	Com_sprintf( scope, sizeof( scope ), "char:%d", cs->charID );
	lua_settop( L, 3 );
	return RP_LuaStore_DoSet( L, cs->plugin, scope, key, 3 );
}

static int RP_LuaStore_CharId( lua_State *L ) {
	rpLuaCharStore_t *cs = RP_LuaStore_CheckCharStore( L );
	lua_pushinteger( L, cs->charID );
	return 1;
}

static int RP_LuaStore_LChar( lua_State *L ) {
	int plugin = RP_Lua_CurrentPlugin( L );
	int client = RP_LuaApi_CheckPlayer( L, 1 );
	gclient_t *cl = &level.clients[client];
	rpLuaCharStore_t *cs;

	if ( cl->sess.amrpgmode != 2 || cl->pers.CharID <= 0 ) {
		return luaL_error( L, "Storage.char: this player is not playing a character (check player.character_id first)" );
	}
	cs = (rpLuaCharStore_t *)lua_newuserdatauv( L, sizeof( *cs ), 0 );
	cs->plugin = plugin;
	cs->charID = cl->pers.CharID;
	luaL_setmetatable( L, RP_LUA_CHARSTORE_MT );
	return 1;
}

void RP_LuaStore_Register( lua_State *L ) {
	static const luaL_Reg storageFuncs[] = { { "get", RP_LuaStore_LGet }, { "set", RP_LuaStore_LSet }, { "char", RP_LuaStore_LChar }, { NULL, NULL } };
	static const luaL_Reg charMethods[] = { { "get", RP_LuaStore_CharGet }, { "set", RP_LuaStore_CharSet }, { "id", RP_LuaStore_CharId }, { NULL, NULL } };
	int g = lua_absindex( L, -1 );

	lua_newtable( L );
	luaL_setfuncs( L, storageFuncs, 0 );
	lua_setfield( L, g, "Storage" );

	luaL_newmetatable( L, RP_LUA_CHARSTORE_MT );
	lua_newtable( L );
	luaL_setfuncs( L, charMethods, 0 );
	lua_setfield( L, -2, "__index" );
	lua_pushboolean( L, 0 );
	lua_setfield( L, -2, "__metatable" );
	lua_pop( L, 1 );
}
