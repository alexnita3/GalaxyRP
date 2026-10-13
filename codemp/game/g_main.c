/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/


#include "g_local.h"
#include "g_ICARUScb.h"
#include "g_nav.h"
#include "bg_saga.h"
#include "b_local.h"
#include "qcommon/q_version.h"
#include "sqlite/sqlite3.h"

NORETURN_PTR void (*Com_Error)( int level, const char *error, ... );
void (*Com_Printf)( const char *msg, ... );

level_locals_t	level;

int		eventClearTime = 0;
static int navCalcPathTime = 0;
extern int fatalErrors;

int killPlayerTimer = 0;

// GalaxyRP: [Logical Entities] the array holds both regions; see g_local.h. g_logicalents is the
// first slot of the upper, engine-invisible region.
gentity_t		g_entities[MAX_ENTITIESTOTAL];
gentity_t		*g_logicalents = &g_entities[MAX_GENTITIES];
gclient_t		g_clients[MAX_CLIENTS];

qboolean gDuelExit = qfalse;

void G_InitGame					( int levelTime, int randomSeed, int restart );
void G_RunFrame					( int levelTime );
void G_ShutdownGame				( int restart );
void CheckExitRules				( void );
void G_ROFF_NotetrackCallback	( gentity_t *cent, const char *notetrack);

extern stringID_table_t setTable[];

qboolean G_ParseSpawnVars( qboolean inSubBSP );
void G_SpawnGEntityFromSpawnVars( qboolean inSubBSP );


qboolean NAV_ClearPathToPoint( gentity_t *self, vec3_t pmins, vec3_t pmaxs, vec3_t point, int clipmask, int okToHitEntNum );
qboolean NPC_ClearLOS2( gentity_t *ent, const vec3_t end );
int NAVNEW_ClearPathBetweenPoints(vec3_t start, vec3_t end, vec3_t mins, vec3_t maxs, int ignore, int clipmask);
qboolean NAV_CheckNodeFailedForEnt( gentity_t *ent, int nodeNum );
qboolean G_EntIsUnlockedDoor( int entityNum );
qboolean G_EntIsDoor( int entityNum );
qboolean G_EntIsBreakable( int entityNum );
qboolean G_EntIsRemovableUsable( int entNum );
void CP_FindCombatPointWaypoints( void );

/*
================
G_FindTeams

Chain together all entities with a matching team field.
Entity teams are used for item groups and multi-entity mover groups.

All but the first will have the FL_TEAMSLAVE flag set and teammaster field set
All but the last will have the teamchain field set to the next one
================
*/
void G_FindTeams( void ) {
	gentity_t	*e, *e2;
	int		i, j;
	int		c, c2;

	c = 0;
	c2 = 0;
	for ( i=MAX_CLIENTS, e=g_entities+i ; i < level.num_entities ; i++,e++ ) {
		if (!e->inuse)
			continue;
		if (!e->team)
			continue;
		if (e->flags & FL_TEAMSLAVE)
			continue;
		if (e->r.contents==CONTENTS_TRIGGER)
			continue;//triggers NEVER link up in teams!
		e->teammaster = e;
		c++;
		c2++;
		for (j=i+1, e2=e+1 ; j < level.num_entities ; j++,e2++)
		{
			if (!e2->inuse)
				continue;
			if (!e2->team)
				continue;
			if (e2->flags & FL_TEAMSLAVE)
				continue;
			if (!strcmp(e->team, e2->team))
			{
				c2++;
				e2->teamchain = e->teamchain;
				e->teamchain = e2;
				e2->teammaster = e;
				e2->flags |= FL_TEAMSLAVE;

				// make sure that targets only point at the master
				if ( e2->targetname ) {
					e->targetname = e2->targetname;
					e2->targetname = NULL;
				}
			}
		}
	}

//	trap->Print ("%i teams with %i entities\n", c, c2);
}

/*
=================
RP_TeamLinkEntity

GalaxyRP: [Entity System] one entity into its team, the way G_FindTeams() would have linked it had it been
there at map load: as a slave of the live master of the team its "team" key names (its targetname moves
onto the master, as there), or as a master on its own when there is none. For entities spawned after map
load (/entadd, /entedit, a copy, a preset) -- G_FindTeams() was never run again for them, so a door added
to a team stood alone, and one spawned again in place after RP_TeamDetach() has to be put back. Triggers
never link, as there. An entity linked already is left as it is.

A trigger is told by its contents, as G_FindTeams() tells it -- but G_FindTeams() runs before any item
has its contents (FinishSpawningItem sets CONTENTS_TRIGGER two frames later), while here a live item has
them, so an item is not a trigger for this: "team" on items is the one-of-a-team respawn (RespawnItem),
and an item added to such a team joins it.
=================
*/
static qboolean RP_TeamIsTrigger( const gentity_t *e )
{
	return ( e->r.contents == CONTENTS_TRIGGER && e->s.eType != ET_ITEM ) ? qtrue : qfalse;
}

void RP_TeamLinkEntity( gentity_t *e )
{
	gentity_t *master = NULL;
	int i;

	if ( !e || !e->inuse || !e->team || !e->team[0] || e->isLogical )
		return;
	if ( RP_TeamIsTrigger( e ) )
		return;
	if ( e->teammaster && e->teammaster->inuse )
		return;

	for ( i = MAX_CLIENTS; i < level.num_entities; i++ )
	{
		gentity_t *m = &g_entities[i];

		if ( m == e || !m->inuse || !m->team || ( m->flags & FL_TEAMSLAVE ) || m->teammaster != m )
			continue;
		if ( RP_TeamIsTrigger( m ) )
			continue;
		if ( !strcmp( m->team, e->team ) )
		{
			master = m;
			break;
		}
	}

	if ( !master )
	{
		e->teammaster = e;
		e->teamchain = NULL;
		e->flags &= ~FL_TEAMSLAVE;
		return;
	}

	e->teamchain = master->teamchain;
	master->teamchain = e;
	e->teammaster = master;
	e->flags |= FL_TEAMSLAVE;

	// make sure that targets only point at the master
	if ( e->targetname )
	{
		master->targetname = e->targetname;
		e->targetname = NULL;
	}
}

sharedBuffer_t gSharedBuffer;

void WP_SaberLoadParms( void );
void BG_VehicleLoadParms( void );

void G_CacheGametype( void )
{
	// check some things
	if ( g_gametype.string[0] && isalpha( g_gametype.string[0] ) )
	{
		int gt = BG_GetGametypeForString( g_gametype.string );
		if ( gt == -1 )
		{
			trap->Print( "Gametype '%s' unrecognised, defaulting to FFA/Deathmatch\n", g_gametype.string );
			level.gametype = GT_FFA;
		}
		else
			level.gametype = gt;
	}
	else if ( g_gametype.integer < 0 || g_gametype.integer >= GT_MAX_GAME_TYPE )
	{
		trap->Print( "g_gametype %i is out of range, defaulting to 0 (FFA/Deathmatch)\n", g_gametype.integer );
		level.gametype = GT_FFA;
	}
	else
		level.gametype = atoi( g_gametype.string );

	trap->Cvar_Set( "g_gametype", va( "%i", level.gametype ) );
	trap->Cvar_Update( &g_gametype );
}

void G_CacheMapname( const vmCvar_t *mapname )
{
	Com_sprintf( level.mapname, sizeof( level.mapname ), "maps/%s.bsp", mapname->string );
	Com_sprintf( level.rawmapname, sizeof( level.rawmapname ), "maps/%s", mapname->string );
}

void RP_CVU_pluginRequired(void)
{
	if (rp_pluginRequired.integer == 2)
	{
		gentity_t *ent;
		int i;

		for (i = 0, ent = g_entities; i < MAX_CLIENTS; ++i, ++ent)
		{
			if (ent && ent->client && ent->client->pers.connected != CON_DISCONNECTED)
			{
				if (!ent->client->pers.clientPlugin) ClientBegin(ent->s.number, qfalse);
			}
		}
	}
}

// GalaxyRP fix: [Database] every prepare in the game module is sqlite3_prepare_v2(), not the
// legacy sqlite3_prepare(). They take identical arguments; what differs is how sqlite3_step()
// behaves afterwards, and both differences cost us something real:
//
//   1. Error detail. With the legacy interface a failing step() returns a bare SQLITE_ERROR and
//      sqlite3_errmsg() says "SQL logic error" -- the actual reason is only available from
//      sqlite3_finalize()'s return value, which nothing in this codebase reads. So every one of
//      the ~107 trap->Print("SQL error: %s", sqlite3_errmsg(db)) lines in the database layer
//      reported "SQL logic error" for any constraint failure. Measured against our own bundled
//      amalgamation, the same duplicate-username INSERT reports:
//
//          legacy : rc=1  "SQL logic error"
//          _v2    : rc=19 "UNIQUE constraint failed: Accounts.Username"
//
//      That matters here specifically because Accounts.Username now HAS a UNIQUE index (see
//      statement_username_unique_index in InitializeGalaxyRpTables below), so a duplicate
//      registration is a reachable failure that insert_accounts_table_row() was fixed to report --
//      and it was reporting it uselessly.
//
//   2. Schema changes. If the schema changes between prepare() and step(), the legacy interface
//      fails the query outright; _v2 recompiles the statement and runs it. This is reachable for
//      us: DB_PATH resolves against fs_homepath, so two server instances on one machine share the
//      database file, and InitializeGalaxyRpTables() runs its ALTER TABLE batch on every map load.
//      One instance migrating while the other has a statement prepared silently loses that other
//      query -- a character load, typically. WAL (which RP_DB_Open sets) widens the window rather
//      than narrowing it, because a writer is no longer blocked by a reader.
//
// The third documented _v2 difference -- recompiling when a bound value could change the query
// plan -- needs LIKE/GLOB or SQLITE_ENABLE_STAT4, and we have neither. It does not apply.
//
// Nothing else changes. No call site in this codebase tests a specific failure code (every one
// checks != SQLITE_OK, != SQLITE_DONE, or != SQLITE_ROW && != SQLITE_DONE), so a step that now
// returns SQLITE_CONSTRAINT instead of SQLITE_ERROR takes the identical branch; only the logged
// text differs. SQLITE_BUSY was already passed through unchanged by both interfaces, so none of
// the "database is locked" handling in RP_DB_Open() is affected. The sqlite3_exec() sites needed
// nothing -- exec has always used prepare_v2 internally, which is why exec failures in this file
// already reported properly while prepare failures did not.
//
// Note this is not something the 3.53.4 bump forced: _v2 has existed since 2007, the old 3.8.8.3
// amalgamation behaved identically, and sqlite3_prepare() carries no SQLITE_DEPRECATED attribute
// and is not behind SQLITE_OMIT_DEPRECATED. It is not going away. This is a diagnostics fix.
//alex: checks if an admin account exists
qboolean admin_account_exists(sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {

	int count = 0;

	rc = sqlite3_prepare_v2(db, "SELECT count(AccountID) FROM Accounts WHERE Username = 'admin' COLLATE NOCASE", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return qfalse;
	}
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return qfalse;
	}
	if (rc == SQLITE_ROW)
	{
		count = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	else
	{
		// GalaxyRP fix: [stability] this fallthrough (rc == SQLITE_DONE, no row at all) used to return
		// without ever finalizing stmt -- effectively dead in practice for a bare COUNT(*) query (which
		// always yields exactly one row), but the same missing-finalize class of bug fixed in several
		// other functions in g_cmds.c. Finalized here too so sqlite3_close() on this connection can't be
		// silently left BUSY by it.
		sqlite3_finalize(stmt);
	}

	if (count == 0) {
		return qfalse;
	}

	return qtrue;
}

//alex: creates an admin account with default values and highest admin authority possible
// GalaxyRP fix: [gameplay] AdminLevel is a bitmask of the zyk_admin_t enum in rp_local.h
// (ADM_NPC=0 .. ADM_GETUP=26, ADM_NUM_CMDS=27 total admin commands today). It used to be set to
// (1 << 27) - 1 = 134217727 -- literally every bit for the commands that exist right now -- but
// every admin check in the codebase (e.g. check_admin_command() and the ADM_* bit tests in
// g_cmds.c) is a plain "bitvalue & (1 << admin_command)", so that value silently stopped covering
// any admin command added above bit 26 in the future (it would need to be bumped by hand every
// time). Using -1 (all 32 bits set, via two's complement) instead means every bit position is
// already set, so the default admin account automatically has every admin command that exists now
// AND every one added later, with nothing left to remember to update here again.
void create_admin_account(sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt)
{
	int accountID = 1;
	int charID = 0;
	char comparisonName[256] = { 0 };
	char char_name[] = "admin";

	// GalaxyRP fix: [Database] PlayerSettings='0' here is intentionally left as a literal, unlike the
	// same column in g_cmds.c's UPDATE sites -- this is a one-time bootstrap of a brand new admin
	// account with no gentity_t/pers.player_settings to bind at all, so 0 (no custom settings) is
	// simply the correct starting value, not an instance of the same bug.
	char statement_account_entry_creation[] = "INSERT INTO Accounts(Username, Password, AdminLevel, PlayerSettings, DefaultChar) VALUES('admin','admin','-1','0','admin')";
	char statement_account_id_select[] = "SELECT AccountID FROM Accounts WHERE Username = 'admin' COLLATE NOCASE";
	char statement_character_entry_creation[] = "INSERT INTO Characters(AccountID, Credits, Level, ModelScale, Name, SkillPoints, Description, NetName, ModelName, xp) VALUES('%i', '100', '1', '100', '%s', '1', 'Nothing to show.', 'DefaultName', 'kyle', 0)";
	char statement_skill_entry_creation[] = "INSERT INTO Skills(CharID, Jump, Push, Pull, Speed, Sense, SaberAttack, SaberDefense, SaberThrow, Absorb, Heal, Protect, MindTrick, TeamHeal, Lightning, Grip, Drain, Rage, TeamEnergize, StunBaton, BlasterPistol, BlasterRifle, Disruptor, Bowcaster, Repeater, DEMP2, Flechette, RocketLauncher, ConcussionRifle, BryarPistol, Melee, MaxShield, ShieldStrength, HealthStrength, DrainShield, Jetpack, SenseHealth, ShieldHeal, TeamShieldHeal, UniqueSkill, BlasterPack, PowerCell, MetalBolts, Rockets, Thermals, TripMines, Detpacks, Binoculars, BactaCanister, SentryGun, SeekerDrone, Eweb, BigBacta, ForceField, CloakItem, ForcePower, Improvements, Armor, Flamethrower, ShieldRegen, HealthRegen) VALUES('%i', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0')";
	char statement_weapon_entry_creation[] = "INSERT INTO Weapons(CharID, AmmoBlaster, AmmoPowercell, AmmoMetalBolts, AmmoRockets, AmmoThermal, AmmoTripmine, AmmoDetpack) VALUES('%i', '0', '0', '0', '0', '0', '0', '0')";

	//alex: Create account record
	// GalaxyRP fix: [stability] this used to call sqlite3_close(db) before returning on this specific
	// failure path -- unlike every other early return in this function, none of which close db
	// themselves. This function's only caller, InitializeGalaxyRpTables(), already calls
	// sqlite3_close(db) unconditionally right after the admin-account block finishes, regardless of
	// which path was taken in here. That meant this one path closed db twice: once here, and once again
	// in the caller right after -- calling sqlite3_close() on an already-closed handle is undefined
	// behavior (a use-after-free of the connection object). This INSERT can fail on any genuine DB
	// error, and, now that Accounts.Username has a real UNIQUE index (see InitializeGalaxyRpTables()),
	// also on a UNIQUE constraint violation if this ever races another process inserting the same
	// 'admin' username against a shared database file. Removed the redundant close here so the caller's
	// single close is the only one, matching every other early-return path in this function.
	rc = sqlite3_exec(db, statement_account_entry_creation, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return;
	}

	//alex: Get AccountID for later so we know which account the char is tied to
	rc = sqlite3_prepare_v2(db, statement_account_id_select, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	if (rc == SQLITE_ROW)
	{
		accountID = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}


	//alex: Create character record
	rc = sqlite3_exec(db, va(statement_character_entry_creation, accountID, char_name), 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return;
	}

	// GalaxyRP fix: [Database] the Skills and Weapons rows below are bound to the Characters row by
	// its actual CharID now. Both INSERTs used to leave CharID out of the column list entirely and
	// rely on SQLite handing all three tables the same auto-assigned rowid -- which it does only for
	// as long as the three stay in perfect lockstep. On a brand new database they do (every table is
	// empty, so all three rows come out as 1), which is why this has always appeared to work, and
	// the one reachable delete path (remove_character, g_cmds.c) removes all three rows together, so
	// nothing routine pulls them apart either.
	//
	// What pulls them apart is a failure partway through this very function. If the INSERT above
	// succeeds and the Skills INSERT below then fails, this returns with Characters at rowid N and
	// Skills still at N-1 -- and from then on EVERY later CharID-less insert pair on this database
	// binds mismatched ids, silently giving new characters somebody else's skills row or none at
	// all. The failure is rare; the corruption it leaves behind is permanent and invisible.
	//
	// sqlite3_last_insert_rowid() is read here rather than re-selected, and read immediately, while
	// the INSERT above is still the last statement executed on this connection. This is the same
	// thing create_new_character() (g_cmds.c) already does -- that path was written later and got it
	// right; this one and Cmd_Register_F's are the two that predate it.
	charID = (int)sqlite3_last_insert_rowid(db);

	//alex: Create skill record
	rc = sqlite3_exec(db, va(statement_skill_entry_creation, charID), 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return;
	}

	//alex: Create ammo record
	rc = sqlite3_exec(db, va(statement_weapon_entry_creation, charID), 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return;
	}

	return;
}

// GalaxyRP fix: [Database] "database is locked" (SQLITE_BUSY) fix. Every database access in the
// game module opens its own short-lived sqlite3 connection to the same on-disk file via plain
// sqlite3_open(), with no shared connection and no busy handler. SQLite's rollback-journal default
// is to fail a write *immediately* with SQLITE_BUSY/SQLITE_LOCKED if another connection already
// holds the write lock, instead of waiting. InitializeGalaxyRpTables() below runs a batch of
// ALTER TABLE statements on every single map load (it's called unconditionally from G_InitGame),
// at exactly the moment reconnecting clients are opening their own connections to read/save their
// character row -- so map change routinely produced "database is locked" errors on whichever side
// lost the race, which is why players could come back from a map change with no weapons/inventory
// (their load query failed) until a /kill forced a retry once the lock had cleared.
//
// RP_DB_Open() is a drop-in replacement for sqlite3_open(DB_PATH, &db) that (1) sets a busy
// timeout so a connection that does collide with another writer blocks and retries for up to
// RP_DB_BUSY_TIMEOUT_MS instead of failing outright, and (2) switches the database to WAL
// journal mode, where readers no longer block a writer and a writer no longer blocks readers --
// which removes the vast majority of the contention in the first place. WAL mode is a persistent
// property of the database file (only needs to succeed once), but PRAGMA journal_mode is cheap
// and idempotent, so it's simplest to just set it on every open.
//
// GalaxyRP fix: [Database] DB_PATH ("GalaxyRP/database/accounts.db") is a relative filesystem
// path, and sqlite3_open() resolves it against the server process's actual OS working directory
// -- NOT against fs_homepath/fs_basepath/fs_game, which the rest of the engine's virtual
// filesystem uses. That only happened to work as long as the server was launched with its
// working directory set to exactly the folder containing GalaxyRP (e.g. via galaxyrp_host.bat's
// `cd..` before launching), and breaks silently with "unable to open database file" for any
// other launch method (double-clicking the engine exe directly, a shortcut with a different
// "Start in" folder, launching from an IDE, etc.) -- the working directory just isn't guaranteed
// to be right. Resolve it against fs_homepath instead, which the engine already knows
// authoritatively regardless of how it was launched, so the DB path no longer depends on the
// process's working directory at all. Falls back to the old relative path if fs_homepath is
// somehow empty, rather than failing outright.
int RP_DB_Open(sqlite3 **db)
{
	char fsHomepath[MAX_OSPATH] = { 0 };
	char dbPath[MAX_OSPATH * 2];

	trap->Cvar_VariableStringBuffer("fs_homepath", fsHomepath, sizeof(fsHomepath));
	if (fsHomepath[0])
	{
		Com_sprintf(dbPath, sizeof(dbPath), "%s/%s", fsHomepath, DB_PATH);
	}
	else
	{
		Q_strncpyz(dbPath, DB_PATH, sizeof(dbPath));
	}

	int rc = sqlite3_open(dbPath, db);
	if (rc != SQLITE_OK)
	{
		return rc;
	}

	sqlite3_busy_timeout(*db, RP_DB_BUSY_TIMEOUT_MS);

	char *walErrMsg = 0;
	int walRc = sqlite3_exec(*db, "PRAGMA journal_mode=WAL;", 0, 0, &walErrMsg);
	if (walRc != SQLITE_OK)
	{
		trap->Print("Warning: could not enable WAL journal mode on database: %s\n", walErrMsg ? walErrMsg : sqlite3_errmsg(*db));
		if (walErrMsg)
		{
			sqlite3_free(walErrMsg);
		}
	}

	return rc;
}

//alex: creates tables is they didn't exist, and admin account if it doesn't exist. This is the place to add things whenever the database structure changes (use UPDATE TABLE for further changes)
void InitializeGalaxyRpTables(qboolean with_admin_account)
{
	sqlite3* db;
	char* zErrMsg = 0;
	int rc;
	sqlite3_stmt* stmt = 0;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	char statement_account_table_creation[] = "CREATE TABLE IF NOT EXISTS 'Accounts' ('AccountID' INTEGER, 'PlayerSettings' INTEGER, 'AdminLevel' INTEGER, 'Password' TEXT, 'Username' TEXT, 'DefaultChar' TEXT, PRIMARY KEY(AccountID))";
	char statement_character_table_creation[] = "CREATE TABLE IF NOT EXISTS 'Characters' ('AccountID' INTEGER, 'CharID' INTEGER, 'Credits' INTEGER, 'Level' INTEGER, 'ModelScale' INTEGER, 'Name' TEXT, 'SkillPoints' INTEGER, 'Description' TEXT, 'NetName' TEXT, 'ModelName' TEXT, PRIMARY KEY(CharID))";
	char statement_weapon_table_creation[] = "CREATE TABLE IF NOT EXISTS 'Weapons' ('CharID' INTEGER, 'AmmoBlaster' INTEGER, 'AmmoPowercell' INTEGER, 'AmmoMetalBolts' INTEGER, 'AmmoRockets' INTEGER, 'AmmoThermal' INTEGER, 'AmmoTripmine' INTEGER, 'AmmoDetpack' INTEGER, PRIMARY KEY(CharID))";
	char statement_skill_table_creation[] = "CREATE TABLE IF NOT EXISTS 'Skills' ('CharID' INTEGER, 'Jump' INTEGER, 'Push' INTEGER, 'Pull' INTEGER, 'Speed' INTEGER, 'Sense' INTEGER, 'SaberAttack' INTEGER, 'SaberDefense' INTEGER, 'SaberThrow' INTEGER, 'Absorb' INTEGER, 'Heal' INTEGER, 'Protect' INTEGER, 'MindTrick' INTEGER, 'TeamHeal' INTEGER, 'Lightning' INTEGER, 'Grip' INTEGER, 'Drain' INTEGER, 'Rage' INTEGER, 'TeamEnergize' INTEGER, 'StunBaton' INTEGER, 'BlasterPistol' INTEGER, 'BlasterRifle' INTEGER, 'Disruptor' INTEGER, 'Bowcaster' INTEGER, 'Repeater' INTEGER, 'DEMP2' INTEGER, 'Flechette' INTEGER, 'RocketLauncher' INTEGER, 'ConcussionRifle' INTEGER, 'BryarPistol' INTEGER, 'Melee' INTEGER, 'MaxShield' INTEGER, 'ShieldStrength' INTEGER, 'HealthStrength' INTEGER, 'DrainShield' INTEGER, 'Jetpack' INTEGER, 'SenseHealth' INTEGER, 'ShieldHeal' INTEGER, 'TeamShieldHeal' INTEGER, 'UniqueSkill' INTEGER, 'BlasterPack' INTEGER, 'PowerCell' INTEGER, 'MetalBolts' INTEGER, 'Rockets' INTEGER, 'Thermals' INTEGER, 'TripMines' INTEGER, 'Detpacks' INTEGER, 'Binoculars' INTEGER, 'BactaCanister' INTEGER, 'SentryGun' INTEGER, 'SeekerDrone' INTEGER, 'Eweb' INTEGER, 'BigBacta' INTEGER, 'ForceField' INTEGER, 'CloakItem' INTEGER, 'ForcePower' INTEGER, 'Improvements' INTEGER, PRIMARY KEY(CharID))";
	char statement_item_table_creation[] = "CREATE TABLE IF NOT EXISTS 'Items' ('ItemID' INTEGER, 'CharID' INTEGER, 'ItemName' TEXT, PRIMARY KEY(ItemID))";
	char statement_news_table_creation[] = "CREATE TABLE IF NOT EXISTS 'News' ('newsID' INTEGER, 'channel' TEXT, 'date' TEXT DEFAULT (strftime('%d-%m-%Y','now')), 'text' TEXT, PRIMARY KEY('newsID'))";

	// GalaxyRP (Alex): [Database] New columns that are added as part of updates. If they were included in the previous columns, upgrading servers would have to redo their database from scratch.
	char statement_xp_column_alter[] = "ALTER TABLE Characters ADD COLUMN xp INTEGER DEFAULT 0";
	char statement_armor_column_alter[] = "ALTER TABLE Skills ADD COLUMN Armor INTEGER DEFAULT 0";
	char statement_flamethrower_column_alter[] = "ALTER TABLE Skills ADD COLUMN Flamethrower INTEGER DEFAULT 0";
	char statement_shieldregen_columns_alter[] = "ALTER TABLE Skills ADD COLUMN ShieldRegen INTEGER DEFAULT 0";
	char statement_heathregen_columns_alter[] = "ALTER TABLE Skills ADD COLUMN HealthRegen INTEGER DEFAULT 0";
	// GalaxyRP fix: [Database] wrapped in an explicit BEGIN/COMMIT so the four ADD COLUMNs apply
	// atomically -- previously they ran as four separate auto-committed statements within one
	// sqlite3_exec() call, so a server crash at the exact instant between two of them could in
	// principle leave the migration half-applied (only some of the four columns added), and the
	// guard below -- which only re-checks whether the *first* column already exists -- would then
	// treat that partial state as "already fully migrated" and never add the rest.
	// GalaxyRP fix: [Database] buffer bumped 310 -> 400 -- the added "BEGIN;"/"COMMIT;" lines are two
	// more backslash-newline-spliced continuation lines, and (like every other line here) the tab
	// indentation on each continuation line is itself part of the literal string content, not just
	// source formatting -- true content is 347 bytes plus the NUL terminator, so 310 would silently
	// truncate this string with no space left for the terminator (see create_new_character_query's
	// near-identical bug, fixed earlier this engagement, for what that failure mode looks like).
	// GalaxyRP: [Saber RGB] no schema change for the RGB/blade-style redesign -- saberOneColor/
	// saberTwoColor (INTEGER DEFAULT 1) are reused as-is, now packing both the selected
	// saber_colors_t mode and the custom RGB payload via SABER_STORED_PACK (g_local.h) instead of
	// just the RGB payload. The DEFAULT 1 a pre-existing row still holds decodes to mode 0
	// (SABER_RED) with no RGB payload -- a reasonable default for a row this feature predates.
	char statement_saber_columns_alter[] = "BEGIN;\
												ALTER TABLE Characters ADD COLUMN saberOneModel TEXT DEFAULT 'saber_1';\
												ALTER TABLE Characters ADD COLUMN saberOneColor INTEGER DEFAULT 1;\
												ALTER TABLE Characters ADD COLUMN saberTwoModel TEXT DEFAULT 'saber_1';\
												ALTER TABLE Characters ADD COLUMN saberTwoColor INTEGER DEFAULT 1;\
												COMMIT;";

	// GalaxyRP fix: [security/Account] Accounts.Username was never actually constrained to be unique at
	// the database level -- uniqueness was enforced only in application code, as a check-then-insert in
	// Cmd_Register_F (g_cmds.c): select_number_of_accounts_with_username() followed by
	// insert_accounts_table_row(). Two players submitting /new for the same username within the same
	// narrow window could both pass that check before either INSERT committed, producing two Accounts
	// rows with an identical Username -- and every username-keyed query in this codebase (/login,
	// is_password_correct(), select_account_and_default_character_data(), etc.) assumes exactly one
	// matching row, with no ORDER BY, so which of the two duplicates a given query resolves to is
	// undefined once that happens. CREATE UNIQUE INDEX (rather than a UNIQUE column constraint, which
	// SQLite has no ALTER TABLE support for adding to an existing table) closes this at the storage layer
	// itself: insert_accounts_table_row() now checks its own INSERT's result and reports failure to
	// Cmd_Register_F instead of silently ignoring a constraint violation this index can now raise.
	//
	// GalaxyRP fix: [Account] the index is case-insensitive now, to match every Username comparison in
	// g_cmds.c (all of them carry COLLATE NOCASE -- see the note above
	// select_number_of_characters_with_name() there for why). A binary index would keep letting "Bob"
	// and "bob" coexist at the storage layer while the application refused them, which is the wrong
	// way round for a backstop. Since CREATE INDEX IF NOT EXISTS never touches an index that already
	// exists, the collated one is created under a NEW name, and the old binary one is dropped only
	// once that has succeeded -- so a database that already holds a case-variant pair (the one thing
	// that makes the NOCASE build fail) keeps the binary index it had, logs the same warning as before,
	// and is never left with no uniqueness guarantee at all.
	char statement_username_unique_index[] = "CREATE UNIQUE INDEX IF NOT EXISTS 'idx_accounts_username_nocase' ON 'Accounts' ('Username' COLLATE NOCASE)";
	char statement_username_old_index_drop[] = "DROP INDEX IF EXISTS 'idx_accounts_username_unique'";

	// GalaxyRP fix: [Account] and the same backstop for character names, which never had one. The
	// duplicate check in create_new_character() is the same check-then-insert the account index exists
	// to close, with the same window between the two. Keyed on (AccountID, Name) because uniqueness is
	// per account: two players may each have a character called Jedi. Built on the collation the
	// lookups use, so the database rather than the C code is what says two characters cannot share a
	// name. Same warn-and-continue handling as the account index -- a database that already holds an
	// "Admin"/"admin" pair logs it and keeps running.
	char statement_character_unique_index[] = "CREATE UNIQUE INDEX IF NOT EXISTS 'idx_characters_account_name_nocase' ON 'Characters' ('AccountID', 'Name' COLLATE NOCASE)";

	//Alex: Create Account Table
	trap->Print("Initializing Account table.\n");

	rc = sqlite3_exec(db, statement_account_table_creation, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_close(db);
		return;
	}
	trap->Print("Done with Account table.\n");

	//Alex: Create Character Table
	trap->Print("Initializing Character Table.\n");

	rc = sqlite3_exec(db, statement_character_table_creation, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_close(db);
		return;
	}
	trap->Print("Done with Character table.\n");

	//Alex: Create Weapons Table
	trap->Print("Initializing Weapons Table.\n");

	rc = sqlite3_exec(db, statement_weapon_table_creation, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_close(db);
		return;
	}
	trap->Print("Done with Weapons table.\n");

	//Alex: Create Skills Table
	trap->Print("Initializing Skills Table.\n");

	rc = sqlite3_exec(db, statement_skill_table_creation, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_close(db);
		return;
	}
	trap->Print("Done with Skills table.\n");

	//Alex: Create Items Table
	trap->Print("Initializing Items Table.\n");

	rc = sqlite3_exec(db, statement_item_table_creation, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_close(db);
		return;
	}
	trap->Print("Done with Items table.\n");

	// GalaxyRP fix: [Items] sweep up items whose character no longer exists. remove_character()
	// (g_cmds.c) never deleted a character's Items rows until it was fixed, and Characters.CharID is
	// reused: it is a plain INTEGER PRIMARY KEY, so a new character gets max(CharID) + 1, and removing
	// the character with the highest CharID handed its items to whoever created a character next, on
	// any account. The fix stops new leftovers; this removes the ones already in the database, on the
	// first map load of the fixed build and harmlessly (it finds nothing) on every one after.
	//
	// It also takes the items the old /giveitem sent to CharID 0 (a player who was not logged in yet)
	// -- no character has that id, so nobody could ever see them. CharID in Characters is the primary
	// key and never NULL, so the NOT IN cannot be tripped up by a NULL in the subquery; the IS NULL arm
	// catches an Items row with no owner at all. A failure is reported and the start-up carries on:
	// nothing after this depends on it.
	rc = sqlite3_exec(db, "DELETE FROM Items WHERE CharID IS NULL OR CharID NOT IN (SELECT CharID FROM Characters)", 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		zErrMsg = 0;
	}
	else if (sqlite3_changes(db) > 0)
	{
		trap->Print("Removed %d orphaned item(s) belonging to characters that no longer exist.\n", sqlite3_changes(db));
	}

	//Alex: Create News Table

	trap->Print("Initializing News Table.\n");

	rc = sqlite3_exec(db, statement_news_table_creation, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		sqlite3_close(db);
		return;
	}
	trap->Print("Done with News table.\n");

	trap->Print("All tables have been initialized.\n");

	// GalaxyRP (Alex): [XP System] Add the XP column to the tables (done this way so that servers upgrading don't have to redo their database).
	trap->Print("Initializing XP column.\n");

	rc = sqlite3_exec(db, statement_xp_column_alter, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		if (strcmp(zErrMsg, "duplicate column name: xp") != 0) {
			trap->Print("SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
			sqlite3_close(db);
			return;
		}
		else {
			trap->Print("XP column already exists, nothing to do here.\n");
		}
	}
	trap->Print("Done with XP column.\n");

	// GalaxyRP (Alex): [Armor Skill] Add the Armor Skill column to the tables (done this way so that servers upgrading don't have to redo their database).
	trap->Print("Initializing Armor Skill column.\n");

	rc = sqlite3_exec(db, statement_armor_column_alter, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		if (strcmp(zErrMsg, "duplicate column name: Armor") != 0) {
			trap->Print("SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
			sqlite3_close(db);
			return;
		}
		else {
			trap->Print("Armor skill column already exists, nothing to do here.\n");
		}
	}
	trap->Print("Done with Armor Skill column.\n");

	// GalaxyRP (Alex): [Armor Skill] Add the Armor Skill column to the tables (done this way so that servers upgrading don't have to redo their database).
	trap->Print("Initializing Flamethrower Skill column.\n");

	rc = sqlite3_exec(db, statement_flamethrower_column_alter, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		if (strcmp(zErrMsg, "duplicate column name: Flamethrower") != 0) {
			trap->Print("SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
			sqlite3_close(db);
			return;
		}
		else {
			trap->Print("Flamethrower skill column already exists, nothing to do here.\n");
		}
	}
	trap->Print("Done with Flamethrower Skill column.\n");

	rc = sqlite3_exec(db, statement_shieldregen_columns_alter, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		if (strcmp(zErrMsg, "duplicate column name: ShieldRegen") != 0) {
			trap->Print("SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
			sqlite3_close(db);
			return;
		}
		else {
			trap->Print("ShieldRegen skill column already exists, nothing to do here.\n");
		}
	}
	trap->Print("Done with HealthRegen Skill column.\n");

	rc = sqlite3_exec(db, statement_heathregen_columns_alter, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		if (strcmp(zErrMsg, "duplicate column name: HealthRegen") != 0) {
			trap->Print("SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
			sqlite3_close(db);
			return;
		}
		else {
			trap->Print("HealthRegen skill column already exists, nothing to do here.\n");
		}
	}
	trap->Print("Done with HealthRegen Skill column.\n");

	rc = sqlite3_exec(db, statement_saber_columns_alter, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		// GalaxyRP fix: this was comparing against "duplicate column name: HealthRegen" (copy-pasted
		// from the block above) instead of the column this statement actually adds first
		// ("saberOneModel"). Because the comparison never matched, every server (re)start after the
		// saber columns had already been created printed a spurious "SQL error: duplicate column
		// name: saberOneModel" and returned out of this function early, skipping the rest of
		// InitializeGalaxyRpTables (including admin account setup) on every subsequent run.
		if (strcmp(zErrMsg, "duplicate column name: saberOneModel") != 0) {
			trap->Print("SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
			sqlite3_close(db);
			return;
		}
		else {
			// GalaxyRP fix: [Database] the BEGIN above already opened a transaction on db before the
			// first ALTER failed, and sqlite3_exec() stops at the first failing statement without
			// ever reaching the COMMIT -- so db is left mid-transaction here. Everything else in this
			// function (admin_account_exists()/create_admin_account() below, and every other DB call
			// on this same connection for the rest of the process) would silently run inside that
			// same never-committed transaction and vanish when db is eventually closed. Roll it back
			// explicitly so the connection is back in its normal autocommit state; there is nothing
			// to lose here since this failed attempt never got past its first (rejected) statement.
			sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
			trap->Print("Saber columns already exists, nothing to do here.\n");
		}
	}
	trap->Print("Done with Saber columns.\n");

	// GalaxyRP fix: [security/Account] see the doc comment on statement_username_unique_index above --
	// this is the DB-level enforcement half of that fix. "IF NOT EXISTS" makes this safe to (re-)run on
	// every server start once it has succeeded once. If it fails, that is not "already applied" the way
	// duplicate-column-name is for the ALTER TABLE migrations above (IF NOT EXISTS already handles that
	// case for an index) -- the realistic failure here is that this database already has two or more
	// Accounts rows sharing a Username from before this fix existed, which SQLite refuses to build a
	// UNIQUE index over. Don't block server startup over it: log a clear, actionable warning so an admin
	// can find and merge/rename the duplicate(s), and continue -- the application-level check in
	// Cmd_Register_F still guards *new* registrations either way, this index just can't be created until
	// the existing duplicate is resolved.
	trap->Print("Initializing unique index on Accounts.Username.\n");

	rc = sqlite3_exec(db, statement_username_unique_index, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("WARNING: could not create a UNIQUE index on Accounts.Username: %s\n", zErrMsg);
		trap->Print("WARNING: this usually means two or more existing accounts already share a Username "
			"(the comparison is case-insensitive, so \"Bob\" and \"bob\" count as the same name). "
			"Find and rename/merge the duplicate account(s) in the database, then restart the server to "
			"finish enabling this protection. Until then, duplicate usernames can still be created.\n");
		sqlite3_free(zErrMsg);
	}
	else {
		// GalaxyRP fix: [Account] only now, with the collated index in place, retire the binary one it
		// replaces. On a database that never had it this is a no-op.
		rc = sqlite3_exec(db, statement_username_old_index_drop, 0, 0, &zErrMsg);
		if (rc != SQLITE_OK)
		{
			trap->Print("WARNING: could not drop the superseded index idx_accounts_username_unique: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		trap->Print("Done with unique index on Accounts.Username.\n");
	}

	trap->Print("Initializing unique index on Characters.(AccountID, Name).\n");

	rc = sqlite3_exec(db, statement_character_unique_index, 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("WARNING: could not create a UNIQUE index on Characters.(AccountID, Name): %s\n", zErrMsg);
		trap->Print("WARNING: this usually means an account already has two characters whose names differ "
			"only in case. Rename or remove one of them in the database, then restart the server to "
			"finish enabling this protection. Until then, the application-level check in "
			"create_new_character() is the only thing refusing a duplicate name.\n");
		sqlite3_free(zErrMsg);
	}
	else {
		trap->Print("Done with unique index on Characters.(AccountID, Name).\n");
	}

	if (with_admin_account == qtrue) {
		trap->Print("Initializing admin account.\n");
		if (admin_account_exists(db, zErrMsg, rc, stmt) == qfalse) {
			create_admin_account(db, zErrMsg, rc, stmt);
		}
		else {
			trap->Print("Admin account already exists, nothing to do here.\n");
		}
		trap->Print("Done with admin account.\n");
	}

	sqlite3_close(db);
}

// zyk: this function spawns an info_player_deathmatch entity in the map
extern void zyk_set_entity_field(gentity_t *ent, char *key, char *value);
extern void zyk_spawn_entity(gentity_t *ent);
extern void zyk_main_set_entity_field(gentity_t *ent, char *key, char *value);
extern void zyk_main_spawn_entity(gentity_t *ent);
void zyk_create_info_player_deathmatch(int x, int y, int z, int yaw)
{
	gentity_t *spawn_ent = NULL;

	// GalaxyRP: [Logical Entities] a spawn point is a logical class; allocate it where the map
	// loader would, so an SP map's added spawn points do not take networked slots either.
	spawn_ent = RP_SpawnForClassname("info_player_deathmatch", qfalse, qfalse);
	if (spawn_ent)
	{
		gentity_t *spawn_point_ent = NULL;

		// GalaxyRP: [Logical Entities] G_Find covers both regions; the plain index loop this used
		// to be only saw the networked one, and the map's own spawn points are logical now.
		spawn_point_ent = G_Find(NULL, FOFS(classname), "info_player_deathmatch");

		zyk_set_entity_field(spawn_ent,"classname","info_player_deathmatch");
		zyk_set_entity_field(spawn_ent,"origin",va("%d %d %d",x,y,z));
		zyk_set_entity_field(spawn_ent,"angles",va("0 %d 0",yaw));
		if (spawn_point_ent && spawn_point_ent->target)
		{ // zyk: setting the target for SP map spawn points so they will work properly
			zyk_set_entity_field(spawn_ent,"target",spawn_point_ent->target);
		}

		zyk_spawn_entity(spawn_ent);
	}
}

// zyk: creates a ctf flag spawn point
void zyk_create_ctf_flag_spawn(int x, int y, int z, qboolean redteam)
{
	gentity_t *spawn_ent = NULL;

	// GalaxyRP: [Logical Entities] a flag is an item and so always networked, but every code
	// spawn goes through the one allocator so the rule lives in one place.
	spawn_ent = RP_SpawnForClassname(redteam ? "team_CTF_redflag" : "team_CTF_blueflag", qfalse, qfalse);
	if (spawn_ent)
	{
		if (redteam == qtrue)
			zyk_set_entity_field(spawn_ent,"classname","team_CTF_redflag");
		else
			zyk_set_entity_field(spawn_ent,"classname","team_CTF_blueflag");

		zyk_set_entity_field(spawn_ent,"origin",va("%d %d %d",x,y,z));
		zyk_spawn_entity(spawn_ent);
	}
}

// zyk: creates a ctf player spawn point
void zyk_create_ctf_player_spawn(int x, int y, int z, int yaw, qboolean redteam, qboolean team_begin_spawn_point)
{
	gentity_t *spawn_ent = NULL;
	const char *classname;

	if (redteam == qtrue)
		classname = team_begin_spawn_point ? "team_CTF_redplayer" : "team_CTF_redspawn";
	else
		classname = team_begin_spawn_point ? "team_CTF_blueplayer" : "team_CTF_bluespawn";

	// GalaxyRP: [Logical Entities] the four CTF spawn-point classes are logical; see above.
	spawn_ent = RP_SpawnForClassname(classname, qfalse, qfalse);
	if (spawn_ent)
	{
		if (redteam == qtrue)
		{
			if (team_begin_spawn_point == qtrue)
				zyk_set_entity_field(spawn_ent,"classname","team_CTF_redplayer");
			else
				zyk_set_entity_field(spawn_ent,"classname","team_CTF_redspawn");
		}
		else
		{
			if (team_begin_spawn_point == qtrue)
				zyk_set_entity_field(spawn_ent,"classname","team_CTF_blueplayer");
			else
				zyk_set_entity_field(spawn_ent,"classname","team_CTF_bluespawn");
		}

		zyk_set_entity_field(spawn_ent,"origin",va("%d %d %d",x,y,z));
		zyk_set_entity_field(spawn_ent,"angles",va("0 %d 0",yaw));

		zyk_spawn_entity(spawn_ent);
	}
}

// GalaxyRP: [SP Maps] the per-map fix-ups in G_InitGame name the entities they change by what they
// are -- classname and brush model -- rather than by entity number. New Zyk mod picks some of them
// by slot; those numbers only hold for one spawn order and one server config (g_weaponDisable frees
// rack weapons at load and shifts every later number), so each was matched against the real map and
// replaced by the classname and "*N" model of the entity it hit. An inline model belongs to exactly
// one entity in a map, and a freed entity (classname "freed", model NULL) never matches.
static qboolean RP_IsBrushEntity( const gentity_t *ent, const char *classname, const char *model )
{
	return ( ent->inuse && ent->classname && ent->model &&
		!Q_stricmp( ent->classname, classname ) && !Q_stricmp( ent->model, model ) ) ? qtrue : qfalse;
}

// zyk: used to fix func_door entities in SP maps that wont work and must be removed without causing the door glitch
void fix_sp_func_door(gentity_t *ent)
{
	ent->spawnflags = 0;
	ent->flags = 0;
	GlobalUse(ent,ent,ent);
	G_FreeEntity( ent );
}

/*
==================================================================================================
GalaxyRP: [SP Maps] Jedi Outcast single-player maps

The Jedi Outcast SP maps load in Jedi Academy multiplayer but were never part of Zyk's SP map list
(rp_ja_sp_maps, below): each has exactly one spawn point (its info_player_start), so everyone who
spawns together telefrags. This adds 3-4 spawn points to every one of the 28, and on bespin_streets
makes the doors, lifts and start lift that only single-player scripts moved usable by players.

Spawn points. For 22 maps they sit 40-80 units from the original start, facing the same way, and
were picked by testing each candidate against the map's own geometry (read from the .bsp): a clear
player box and a clear path from the start, level solid floor within 24 units of the start's, no
water/slime/lava, no hurt/teleport/push trigger down to the floor, nothing within 16 units of a
door or lift, and 40+ units from every other spawn point (the player box is 30 wide, so closer
would telefrag). On five maps where the start is on a moving lift, inside a moving bin, in a
shuttle over a hurt zone or on a slope, the points were placed in game and checked the same way,
and so were cairn_assembly's (see below).

The original start is kept, except where it stands on or in something that moves, or where it
hurts:
  - ns_starpad: at the bottom of a script-driven elevator ride of some 6,700 units;
  - ns_hideout: inside a moving bin under a low ceiling;
  - bespin_streets: on the start lift's deck, which RP_FixBespinStreets() below makes rest at the
    top -- its column then fills the bottom of the shaft, so a spawn there would be inside it;
  - cairn_assembly: in a closet behind ass_door, a door that starts open, wait -1. A one-shot
    trigger across the hall beyond it closes it for good the first time anyone walks through, and
    everyone who spawned after that was shut in. The four points are in that hall, past the door
    and short of the trigger, out of sight of the two entry turrets;
  - artus_detention: on the tram rail, where a player lands inside the rail's own trigger_hurt.
    Three points are at the east end of the landing past the bridge the intro script collapses,
    the fourth on the rock ledge below the landing's west end. See RP_FixArtusDetention() for the
    falling beam the eastern three would otherwise never set off.
Its target is read before it is removed, and every added point carries it, as Zyk's helper copies
it: it is the map's single-player start script. level.rp_spawn_target_once makes it fire for the
first spawn only (ClientSpawn(), g_client.c).
==================================================================================================
*/
#define RP_JO_MAX_SPAWNS	4

typedef struct rp_jo_spawns_s {
	const char	*map;
	qboolean	removeOriginal;
	int			count;
	int			points[RP_JO_MAX_SPAWNS][4];	// x, y, z, yaw
} rp_jo_spawns_t;

static const rp_jo_spawns_t rp_jo_spawns[] = {
	{ "artus_detention", qtrue , 4, { { -241, 4222, 660, 167 }, { -215, 4360, 660, 178 }, { -182, 4509, 660, -158 }, { -815, 4545, 496, -4 } } },
	{ "artus_mine", qfalse, 3, { { 3904, -2928, 1192, 180 }, { 3904, -2848, 1192, 180 }, { 3944, -2888, 1194, 180 } } },
	{ "artus_topside", qfalse, 3, { { 2204, 1892, 160, 0 }, { 2204, 1812, 156, 0 }, { 2244, 1852, 156, 0 } } },
	{ "bespin_platform", qfalse, 3, { { -1280, 40, 24, 0 }, { -1280, -40, 24, 0 }, { -1320, 0, 24, 0 } } },
	{ "bespin_streets", qtrue , 4, { { -3782, -4094, -1703, 91 }, { -3590, -4069, -1703, 95 }, { -4044, -4092, -1703, 58 }, { -4013, -3903, -1703, -1 } } },
	{ "bespin_undercity", qfalse, 3, { { -40, -496, -3432, 90 }, { 40, -496, -3432, 90 }, { 0, -456, -3432, 90 } } },
	{ "cairn_assembly", qtrue , 4, { { -2326, 815, 472, 62 }, { -2324, 1118, 472, -54 }, { -2339, 970, 472, 2 }, { -2232, 974, 472, 2 } } },
	{ "cairn_bay", qfalse, 3, { { -3128, 176, 600, 135 }, { -3088, 216, 600, 135 }, { -3168, 216, 600, 135 } } },
	{ "cairn_dock1", qfalse, 3, { { 6584, -5928, 464, 90 }, { 6664, -5928, 464, 90 }, { 6624, -5968, 468, 90 } } },
	{ "cairn_reactor", qfalse, 3, { { 1464, -8912, -584, 270 }, { 1384, -8912, -584, 270 }, { 1424, -8872, -584, 270 } } },
	{ "demo", qfalse, 3, { { 464, 2848, -28, 315 }, { 424, 2808, -28, 315 }, { 504, 2808, -28, 315 } } },
	{ "doom_comm", qfalse, 3, { { 40, -832, 24, 270 }, { -40, -832, 24, 270 }, { 0, -872, 24, 270 } } },
	{ "doom_detention", qfalse, 3, { { -1024, -376, 216, 0 }, { -1024, -456, 216, 0 }, { -1064, -416, 216, 0 } } },
	{ "doom_shields", qfalse, 3, { { 2144, -1576, 24, 135 }, { 2184, -1536, 24, 135 }, { 2104, -1536, 24, 135 } } },
	{ "jodemo", qfalse, 3, { { 464, 2848, -28, 315 }, { 424, 2808, -28, 315 }, { 504, 2808, -28, 315 } } },
	{ "kejim_base", qfalse, 3, { { 416, 752, 24, 180 }, { 416, 832, 24, 180 }, { 456, 792, 24, 180 } } },
	{ "kejim_post", qfalse, 3, { { 1692, -1732, -40, 180 }, { 1692, -1652, -40, 180 }, { 1732, -1692, -40, 180 } } },
	{ "ns_hideout", qtrue , 4, { { -177, 2486, 8, -1 }, { -128, 2332, 8, 32 }, { -289, 2420, 8, 175 }, { 54, 2462, -247, 84 } } },
	{ "ns_starpad", qtrue , 3, { { -1647, -2984, -1063, 93 }, { -1643, -2826, -1063, -163 }, { -1835, -2955, -1063, 4 } } },
	{ "ns_streets", qfalse, 3, { { 2292, 388, -224, 135 }, { 2332, 428, -224, 135 }, { 2252, 428, -224, 135 } } },
	{ "pit", qfalse, 3, { { 312, -1040, 216, 0 }, { 312, -1120, 216, 0 }, { 272, -1080, 216, 0 } } },
	{ "valley", qfalse, 3, { { -1312, -1080, -104, 135 }, { -1272, -1040, -104, 135 }, { -1352, -1040, -104, 135 } } },
	{ "yavin_canyon", qfalse, 3, { { -4322, -4798, 2241, 45 }, { -4282, -4838, 2238, 45 }, { -4282, -4758, 2238, 45 } } },
	{ "yavin_courtyard", qfalse, 3, { { 2464, -2424, -536, 45 }, { 2504, -2464, -533, 45 }, { 2504, -2384, -538, 45 } } },
	{ "yavin_final", qfalse, 3, { { -8608, 3944, -784, 90 }, { -8528, 3944, -784, 90 }, { -8568, 3904, -784, 90 } } },
	{ "yavin_swamp", qfalse, 3, { { -5954, -4174, 1751, 315 }, { -5914, -4214, 1757, 315 }, { -5954, -4214, 1758, 315 } } },
	{ "yavin_temple", qfalse, 3, { { 896, -1336, -488, 90 }, { 976, -1336, -488, 90 }, { 936, -1296, -488, 90 } } },
	{ "yavin_trial", qfalse, 3, { { 135, 383, 24, -92 }, { 356, -105, 56, 164 }, { 505, 199, 88, 8 } } },
};

/*
------------------
Single-player map lists

Which campaign a map comes from, by its name -- the only reliable test there is. DAJ runs these
maps in FFA, so the gametype says nothing, and the folder does not either: custom multiplayer maps
sit in maps/ just as the SP maps do, and must keep the multiplayer meaning of every classname.
The two campaigns have to be told apart as well, not just "SP or not" -- misc_turret's spawnflag
4 is TURBO in Jedi Academy and means nothing in Jedi Outcast (SP_misc_turret(), g_turret.c).
No name is on both lists.

  - Jedi Academy: Zyk's SP map list, which sets level.sp_map (and so rp_sp_npc_fix) as it always
    did -- the 28 campaign maps and the six academy hubs;
  - Jedi Outcast: the 28 maps of the spawn-point table above.
------------------
*/
static const char *rp_ja_sp_maps[] = {
	"yavin1", "yavin1b", "yavin2",
	"t1_danger", "t1_fatal", "t1_inter", "t1_rail", "t1_sour", "t1_surprise",
	"hoth2", "hoth3",
	"t2_dpred", "t2_rancor", "t2_rogue", "t2_trip", "t2_wedge",
	"vjun1", "vjun2", "vjun3",
	"t3_bounty", "t3_byss", "t3_hevil", "t3_rift", "t3_stamp",
	"taspir1", "taspir2", "kor1", "kor2",
	// GalaxyRP fix: [NPC] the six academy hub maps were missing, so rp_sp_npc_fix never applied
	// to the SP maps most likely to host RP -- the ones with Kyle, Luke, Rosh, the students and
	// the protocol droids in them.
	"academy1", "academy2", "academy3", "academy4", "academy5", "academy6",
};

static qboolean RP_IsJediAcademySPMap( const char *mapname )
{
	int i;

	for ( i = 0; i < (int)ARRAY_LEN( rp_ja_sp_maps ); i++ )
	{
		if ( !Q_stricmp( mapname, rp_ja_sp_maps[i] ) )
		{
			return qtrue;
		}
	}

	return qfalse;
}

static qboolean RP_IsJediOutcastMap( const char *mapname )
{
	int i;

	for ( i = 0; i < (int)ARRAY_LEN( rp_jo_spawns ); i++ )
	{
		if ( !Q_stricmp( mapname, rp_jo_spawns[i].map ) )
		{
			return qtrue;
		}
	}

	return qfalse;
}

// zyk_create_info_player_deathmatch() with the target passed in: it copies the target of the
// first spawn point it finds, which is gone on the maps that remove their original.
static void RP_CreateSpawnPoint( const int *point, const char *target )
{
	gentity_t *spawn_ent = RP_SpawnForClassname( "info_player_deathmatch", qfalse, qfalse );

	if ( !spawn_ent )
	{
		return;
	}

	zyk_set_entity_field( spawn_ent, "classname", "info_player_deathmatch" );
	zyk_set_entity_field( spawn_ent, "origin", va( "%d %d %d", point[0], point[1], point[2] ) );
	zyk_set_entity_field( spawn_ent, "angles", va( "0 %d 0", point[3] ) );
	if ( target && target[0] )
	{
		zyk_set_entity_field( spawn_ent, "target", (char *)target );
	}

	zyk_spawn_entity( spawn_ent );
}

/*
------------------
Lift calls

A use-panel that sends a lift to its other end. A func_door that is not TOGGLE, used while it is
waiting at its far end (pos2), only restarts its wait -- so pressing the panel at the lift's
resting landing to call it back would keep it away longer instead, and on bespin_streets' start
lift that means holding a 1,728-unit shaft open in the street. So every panel goes through one of
these: resting at pos1 -> a normal use, and it travels to pos2; waiting at pos2 -> it heads back
now instead of after its wait; moving -> ignored, so nobody reverses it under a rider. Every press
therefore means "go to the other end" -- once the lift has stood at that landing for
RP_LIFT_CALL_COOLDOWN: a press sooner than that after it arrives is ignored too, so the people
it brought can step off before anyone sends it away again (and a rider still holding the use key
on the deck does not bounce it straight back). SetMoverState() stamps s.pos.trTime with the
moment the lift reached pos1 or pos2, so that is what the cooldown counts from. It is 0 on a lift
that has not moved since the map loaded, and that never blocks -- level.time itself can be under
the cooldown when the server has only just started.
------------------
*/
#define RP_LIFT_CALL_COOLDOWN	3000	// ms after a lift reaches either end before a call moves it

extern void ReturnToPos1( gentity_t *ent );
extern void InitMoverTrData( gentity_t *ent );
extern void Touch_Multi( gentity_t *self, gentity_t *other, trace_t *trace );
extern void Use_Multi( gentity_t *ent, gentity_t *other, gentity_t *activator );

static void RP_LiftCallUse( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	gentity_t *lift = self->target_ent;

	if ( !lift || !lift->inuse || lift->s.eType != ET_MOVER || !lift->use )
	{
		return;
	}

	if ( ( lift->moverState == MOVER_POS1 || lift->moverState == MOVER_POS2 ) &&
		lift->s.pos.trTime && level.time < lift->s.pos.trTime + RP_LIFT_CALL_COOLDOWN )
	{ // arrived less than the cooldown ago
		return;
	}

	if ( lift->moverState == MOVER_POS1 )
	{
		lift->use( lift, self, activator );
	}
	else if ( lift->moverState == MOVER_POS2 && lift->think == ReturnToPos1 && lift->nextthink > level.time )
	{
		// DAJ_RP: [Locks] sending it back early is a use of the lift too: only for whom its lock lets through,
		// and the one who sent it is who its closetarget answers to (the way up goes through Use_BinaryMover)
		if ( !RP_LockAllows( lift, self, activator ) )
		{
			return;
		}
		if ( activator )
		{
			lift->activator = activator;
		}
		lift->nextthink = level.time;
	}
}

// The logic entity panels target; target_ent is the lift. Never linked, networked or thinking, and
// only ever reached by name through G_UseTargets() -> G_Find(), which walks both regions -- so it
// goes in the logical region like a target_relay, and falls back to a networked slot only when
// rp_logical_entities was 0 at map start.
static void RP_SpawnLiftCall( gentity_t *lift, const char *name )
{
	gentity_t *call = level.logical_entities_enabled ? G_SpawnLogical() : G_Spawn();

	call->classname = "rp_lift_call";
	call->targetname = G_NewString( name );
	call->target_ent = lift;
	call->use = RP_LiftCallUse;
}

// A trigger volume (a trigger_multiple) built in place: one spawned without a brush model works,
// but InitTrigger() complains about it on the console at every map load. spawnflags as
// trigger_multiple's (1 CLIENTONLY, 4 USE_BUTTON); wait in seconds, and -1 fires once, as
// trigger_once does.
static void RP_SpawnTrigger( const vec3_t absmin, const vec3_t absmax, const char *target, int spawnflags, float wait )
{
	gentity_t *trig = G_Spawn();
	vec3_t center;
	int k;

	for ( k = 0; k < 3; k++ )
	{
		center[k] = ( absmin[k] + absmax[k] ) * 0.5f;
		trig->r.mins[k] = absmin[k] - center[k];
		trig->r.maxs[k] = absmax[k] - center[k];
	}

	trig->classname = "trigger_multiple";
	trig->spawnflags = spawnflags;
	trig->target = G_NewString( target );
	trig->wait = wait;
	trig->touch = Touch_Multi;
	trig->use = Use_Multi;
	trig->r.contents = CONTENTS_TRIGGER;
	trig->r.svFlags = SVF_NOCLIENT;
	G_SetOrigin( trig, center );
	trap->LinkEntity( (sharedEntity_t *)trig );
}

extern void Think_SpawnNewDoorTrigger( gentity_t *ent );

/*
------------------
RP_OpenUpDoor

A door that only a single-player script opens -- one that runs inside an affect("kyle") block,
waits on a signal sent from one, or needs a cutscene or a companion's walk -- stays shut for good
in multiplayer. This makes every func_door with that name an ordinary door, before the first frame:
  - a locked one is unlocked (every member of its team, the lock shader stepped to its unlocked
    frame, as UnLockDoors() does). SP_func_door gave it the think that builds its walk-up trigger
    one frame after spawn, and that still runs;
  - a toggle one stops toggling, and gets the same walk-up-trigger think in place of
    Think_MatchTeam (Think_SpawnNewDoorTrigger() ends with the same MatchTeam() call), so it opens
    as anyone walks up and closes after its wait;
  - an ordinary named door (named doors get no walk-up trigger) gets that think too.
The name stays: a script that still reaches the door opens it as any use would. G_FindTeams() has
already moved a team's name onto its master, so the name finds the master and the team is walked
from there.
------------------
*/
static void RP_OpenUpDoor( const char *name )
{
	gentity_t *ent, *member;
	int count = 0;

	RP_FOR_EACH_ENTITY( ent )
	{
		if ( !ent->inuse || !ent->classname || Q_stricmp( ent->classname, "func_door" ) ||
			!ent->targetname || Q_stricmp( ent->targetname, name ) )
		{
			continue;
		}

		for ( member = ent; member; member = member->teamchain )
		{
			if ( member->spawnflags & 16 )	// MOVER_LOCKED
			{
				member->spawnflags &= ~16;
				member->s.frame = 1;	// second stage of the locked-door shader anim, as UnLockDoors()
			}
			member->spawnflags &= ~(8 | 128);	// MOVER_TOGGLE, MOVER_INACTIVE
			member->flags &= ~FL_INACTIVE;
		}

		if ( ent->think != Think_SpawnNewDoorTrigger )
		{
			ent->think = Think_SpawnNewDoorTrigger;
			ent->nextthink = level.time + FRAMETIME;
		}
		count++;
	}

	if ( !count )
	{
		G_Printf( "RP_OpenUpDoor: no func_door named %s on this map\n", name );
	}
}

/*
------------------
RP_DisableLockedTrigger

The use-button trigger by a locked door that plays the "locked" sound or a Kyle line when someone
presses use at it. Single player switches it off (a target_deactivate or a script) when the door
opens; a door opened by RP_OpenUpDoor() needs it off from the start. Only trigger_multiples with
that name, and only the one with that brush model when model is given -- a name some maps share
between several doors.
------------------
*/
static void RP_DisableLockedTrigger( const char *name, const char *model )
{
	gentity_t *ent;
	int count = 0;

	RP_FOR_EACH_ENTITY( ent )
	{
		if ( !ent->inuse || !ent->classname || Q_stricmp( ent->classname, "trigger_multiple" ) ||
			!ent->targetname || Q_stricmp( ent->targetname, name ) )
		{
			continue;
		}
		if ( model && ( !ent->model || Q_stricmp( ent->model, model ) ) )
		{
			continue;
		}
		ent->flags |= FL_INACTIVE;
		count++;
	}

	if ( !count )
	{
		G_Printf( "RP_DisableLockedTrigger: no trigger_multiple named %s on this map\n", name );
	}
}

/*
------------------
RP_FixBespinStreets

In single player an R5 droid's scripts unlock three doors and the player's scripts run the lifts;
without those scripts nothing here moves. Fixed as follows:
  - locked_door1/2/3 (the droid's doors), copdoors (a locked double door) and bigdoor (the hangar
    door, a 1,000-damage crusher): opened and removed with Zyk's fix_sp_func_door(), matched on
    func_door as well as the name -- "copdoors" is also a target_deactivate's name. A door's team
    partners go with it (copdoors is a two-door team, and G_FindTeams() has moved the partner's
    name onto the master), and team links are cut first: moving a door team relinks every member,
    and a partner already freed must not be. Their target and target2 are cleared first too:
    opening a door fires its target, which would have spawned locked_door1's eight ledge_enemies
    (Rodians, Grans, a Weequay; MP ignores the difficulty flags, so all eight) at map load. They
    now spawn when a player first walks through that doorway, from a one-shot trigger put there;
  - lift1-lift5: they already have a use-panel (trigger_multiple, USE_BUTTON) at each landing
    that fired a single-player script; the panels now target a lift call for their lift, and the
    lifts wait 5 seconds at the far end instead of 3;
  - uplift, the start lift -- a deck on a 1,728-unit hydraulic column, START_OPEN|TOGGLE, resting
    at the bottom until a script raised it: it now rests at the top (START_OPEN's position swap
    undone, not TOGGLE, 5-second wait), since at street level the shaft is open with floor right
    up to its edge and a lift resting at the bottom would leave that drop open; at the top the
    deck closes the street and the column fills the shaft. It travels at the map's own speed (200,
    8.6 seconds end to end). It gets a use-trigger over each landing, 48 units wider than the
    shaft and 88 high, built from its own bounds, that fires at most every 3 seconds. Renamed, so a
    leftover script cannot drive it;
  - every lift, the start lift too, ignores calls for RP_LIFT_CALL_COOLDOWN after it arrives;
  - the R5 droid stood on the lift deck, inside the column now: its spawner (which spawns it after
    this has run) moves beside the new spawn points, clear of the lift trigger, facing the lift.
------------------
*/
static void RP_FixBespinStreets( void )
{
	static const char *removeDoors[] = { "locked_door1", "locked_door2", "locked_door3", "copdoors", "bigdoor" };
	static const char *liftNames[] = { "lift1", "lift2", "lift3", "lift4", "lift5" };
	static const char *panelNames[] = { "t296", "t297", "t299", "t302", "t304" };	// same order as liftNames
	gentity_t *doors[16];
	int numDoors = 0;
	gentity_t *ent;
	gentity_t *uplift = NULL;
	vec3_t ledgeMins, ledgeMaxs;
	qboolean haveLedgeDoor = qfalse;
	int i;

	RP_FOR_EACH_ENTITY( ent )
	{
		if ( !ent->inuse || !ent->classname )
		{
			continue;
		}

		if ( !Q_stricmp( ent->classname, "func_door" ) && ent->targetname )
		{
			for ( i = 0; i < (int)ARRAY_LEN( removeDoors ); i++ )
			{
				if ( !Q_stricmp( ent->targetname, removeDoors[i] ) )
				{ // and its team partners: G_FindTeams() has already moved their targetname onto this
				  // one, the team master, so they cannot be found by name any more
					gentity_t *member;

					for ( member = ent; member && numDoors < (int)ARRAY_LEN( doors ); member = member->teamchain )
					{
						int k;

						for ( k = 0; k < numDoors && doors[k] != member; k++ )
							;
						if ( k == numDoors )
						{ // not collected yet
							doors[numDoors++] = member;
						}
					}
				}
			}

			for ( i = 0; i < (int)ARRAY_LEN( liftNames ); i++ )
			{
				if ( !Q_stricmp( ent->targetname, liftNames[i] ) )
				{
					ent->wait = 5000;
					RP_SpawnLiftCall( ent, va( "rp_liftcall_%s", liftNames[i] ) );
				}
			}

			if ( !Q_stricmp( ent->targetname, "uplift" ) )
			{
				uplift = ent;
			}
		}
		else if ( !Q_stricmp( ent->classname, "trigger_multiple" ) && ent->targetname )
		{
			for ( i = 0; i < (int)ARRAY_LEN( panelNames ); i++ )
			{
				if ( !Q_stricmp( ent->targetname, panelNames[i] ) )
				{
					ent->target = G_NewString( va( "rp_liftcall_%s", liftNames[i] ) );
				}
			}
		}
		else if ( !Q_stricmp( ent->classname, "NPC_Droid_R5D2" ) )
		{
			vec3_t droidOrigin = { -3900, -4010, -1704 };

			VectorCopy( droidOrigin, ent->s.origin );
			G_SetOrigin( ent, droidOrigin );
			ent->s.angles[YAW] = 90;
		}
	}

	for ( i = 0; i < numDoors; i++ )
	{
		gentity_t *door = doors[i];

		// the doorway locked_door1 closed: both halves fire ledge_enemies when they start to open.
		// Kept, before the targets are cleared below, to put a trigger in their place.
		if ( door->target && !Q_stricmp( door->target, "ledge_enemies" ) )
		{
			vec3_t dmins, dmaxs;
			int k;

			VectorAdd( door->r.currentOrigin, door->r.mins, dmins );
			VectorAdd( door->r.currentOrigin, door->r.maxs, dmaxs );
			for ( k = 0; k < 3; k++ )
			{
				if ( !haveLedgeDoor || dmins[k] < ledgeMins[k] ) ledgeMins[k] = dmins[k];
				if ( !haveLedgeDoor || dmaxs[k] > ledgeMaxs[k] ) ledgeMaxs[k] = dmaxs[k];
			}
			haveLedgeDoor = qtrue;
		}

		// fix_sp_func_door() opens a door before it frees it, and a door that starts to open fires
		// its target (Use_BinaryMover_Go) -- at map load, for nobody. target2 is cleared with it.
		door->target = NULL;
		door->target2 = NULL;

		door->teamchain = NULL;
		door->teammaster = door;
	}
	for ( i = 0; i < numDoors; i++ )
	{
		fix_sp_func_door( doors[i] );
	}

	// the ledge enemies now appear when a player first walks through that doorway, as they did in
	// single player when the door opened: a one-shot, players-only trigger filling the opening,
	// 32 units deep either side of the doors' plane (their thin horizontal axis)
	if ( haveLedgeDoor )
	{
		int thin = ( ledgeMaxs[0] - ledgeMins[0] ) <= ( ledgeMaxs[1] - ledgeMins[1] ) ? 0 : 1;

		ledgeMins[thin] -= 32;
		ledgeMaxs[thin] += 32;
		RP_SpawnTrigger( ledgeMins, ledgeMaxs, "ledge_enemies", 1, -1.0f );	// CLIENTONLY, fire once
	}

	if ( uplift && (uplift->spawnflags & 1) )
	{
		vec3_t tmp, mins, maxs;

		// SP_func_door swapped the two positions for START_OPEN; swap them back, so pos1 -- where it
		// rests -- is the compiled position at street level
		VectorCopy( uplift->pos1, tmp );
		VectorCopy( uplift->pos2, uplift->pos1 );
		VectorCopy( tmp, uplift->pos2 );
		uplift->spawnflags &= ~(1 | 8);		// START_OPEN, TOGGLE
		uplift->wait = 5000;
		uplift->targetname = G_NewString( "rp_uplift" );
		G_SetOrigin( uplift, uplift->pos1 );
		// G_SetOrigin() zeroes s.pos.trDuration, the travel time SP_func_door worked out from the
		// speed -- and SetMoverState() turns a zero into 1 ms, which made the lift jump from end to
		// end. Work it out again from pos1, pos2 and the map's speed.
		InitMoverTrData( uplift );
		trap->LinkEntity( (sharedEntity_t *)uplift );

		RP_SpawnLiftCall( uplift, "rp_liftcall_uplift" );

		// one trigger over each landing: the deck's top surface at pos1 (street) and at pos2 (bottom)
		for ( i = 0; i < 2; i++ )
		{
			const float *deck = i ? uplift->pos2 : uplift->pos1;

			mins[0] = deck[0] + uplift->r.mins[0] - 48;
			mins[1] = deck[1] + uplift->r.mins[1] - 48;
			mins[2] = deck[2] + uplift->r.maxs[2];
			maxs[0] = deck[0] + uplift->r.maxs[0] + 48;
			maxs[1] = deck[1] + uplift->r.maxs[1] + 48;
			maxs[2] = mins[2] + 88;
			RP_SpawnTrigger( mins, maxs, "rp_liftcall_uplift", 1 | 4, 3.0f );	// CLIENTONLY | USE_BUTTON
		}
	}
}

/*
------------------
RP_FixArtusDetention

In single player Kyle arrives from the tram at the landing's west end and walks east across it.
On the way he crosses a one-shot trigger (x -592..-552, the full width of the landing) that runs
artus_detention/beam: a beam drops from the upper walkway and comes to rest as a 45-degree ramp
from the landing up to it, killing the two guards up there. That ramp is the way to the upper
booth, whose trigger unlocks the double door into the facility (t4) and arms the trigger that
spawns the next squad.

The three eastern spawn points are past where the beam lands, some 300 units short of that
trigger, and a player can walk from them to the door -- through its east half, even -- without
touching it. So a second one-shot, players-only trigger runs the same scriptrunner: a strip
across the landing just west of those points, so the beam falls ahead of whoever first walks
that way, as it did ahead of Kyle. The map's own trigger stays, for the ledge spawn point, whose
way up to the landing crosses it. The scriptrunner allows one run (count 1), so the beam falls
once whichever trigger fires first, and the other then does nothing.

Two locked double doors open only in the warden's forced march, and only inside its
affect("kyle") blocks, which MP skips: office_door (march_warden) and warden_door (warden_escape,
run from the same block). Both are made ordinary doors (RP_OpenUpDoor), and the "locked" sound
trigger in front of office_door (t424, which march_warden removes) is switched off.
------------------
*/
static void RP_FixArtusDetention( void )
{
	const vec3_t mins = { -320, 4112, 636 };
	const vec3_t maxs = { -288, 4568, 740 };

	RP_SpawnTrigger( mins, maxs, "run_beam", 1, -1.0f );	// CLIENTONLY, fire once

	RP_OpenUpDoor( "office_door" );
	RP_OpenUpDoor( "warden_door" );
	RP_DisableLockedTrigger( "t424", NULL );
}

/*
------------------
RP_FixJediOutcastDemo

demo and jodemo (the same map under two names). The demo's own scripts were never shipped, so
the two doors only they unlocked stay locked for good:
  - shieldgendoor, into the shield generator room;
  - baydoor, a small bay door on the upper level.
Both are unlocked here, before the first frame. SP_func_door gave every locked door the think that
builds its walk-up trigger one frame after spawn, and that still runs, so from then on each is an
ordinary door: it opens as anyone walks up and closes after its wait. The lock shader is stepped
to its unlocked frame, as UnLockDoors() does. shieldgendoor's opentarget (kyle_seecore) is
dropped: it runs a script the game never had, so every opening would only print an error.
------------------
*/
static void RP_FixJediOutcastDemo( void )
{
	gentity_t *ent;

	RP_FOR_EACH_ENTITY( ent )
	{
		if ( RP_IsBrushEntity( ent, "func_door", "*99" ) && ent->targetname && !Q_stricmp( ent->targetname, "shieldgendoor" ) )
		{
			ent->spawnflags &= ~16;		// MOVER_LOCKED
			ent->s.frame = 1;
			ent->opentarget = NULL;
		}
		else if ( RP_IsBrushEntity( ent, "func_door", "*106" ) && ent->targetname && !Q_stricmp( ent->targetname, "baydoor" ) )
		{
			ent->spawnflags &= ~16;		// MOVER_LOCKED
			ent->s.frame = 1;
		}
	}
}

/*
------------------
RP_FixNsStreets

ts1 and ts2 are the two sliding tables in the bar: doors the use key slides open, which start
inactive. Only the bartender conversation arms them (act_table), and that is a cutscene, so they
never could be used. They are made active here: anyone can slide one open with the use key, and it
slides back after its wait.

Three NPCs hide inside those tables -- table1_thug and table3_thug under ts1, table2_thug under
ts2 -- spawned cinematic (no AI), not solid and inside the table brushes, for the ambush the
bartender script springs by teleporting them out. Nothing here would, so an open table would
show three motionless figures anyone can walk through. Their spawners are removed before the
first frame, which is before they would spawn.
------------------
*/
static void RP_FixNsStreets( void )
{
	static const char *tableThugs[] = { "table1_thug", "table2_thug", "table3_thug" };
	gentity_t *ent;
	int i;

	RP_FOR_EACH_ENTITY( ent )
	{
		if ( !ent->inuse || !ent->classname )
		{
			continue;
		}

		if ( ( RP_IsBrushEntity( ent, "func_door", "*141" ) || RP_IsBrushEntity( ent, "func_door", "*140" ) ) &&
			ent->targetname && ( !Q_stricmp( ent->targetname, "ts1" ) || !Q_stricmp( ent->targetname, "ts2" ) ) )
		{
			ent->spawnflags &= ~128;	// MOVER_INACTIVE
			ent->flags &= ~FL_INACTIVE;
		}
		else if ( !Q_stricmpn( ent->classname, "NPC_", 4 ) && ent->NPC_targetname )
		{
			for ( i = 0; i < (int)ARRAY_LEN( tableThugs ); i++ )
			{
				if ( !Q_stricmp( ent->NPC_targetname, tableThugs[i] ) )
				{
					G_FreeEntity( ent );
					break;
				}
			}
		}
	}
}

/*
------------------
RP_FixNsStarpad

backdooropen2, a double door by the hangar, has its own walk-up trigger (named backdoor), but
that trigger starts inactive and only a target_activate called hangar_ramp arms it. Lando's
hangar-attack script fires that, after a voice line nothing in MP ever finishes, so the door
never opened. The trigger is armed here instead. hangar_ramp is not fired: the hangar ramp door
answers to the same name.

backdooropen, the locked double door further along, is unlocked by the same script, after Lando's
line. That now plays out in MP, but only if Lando is still alive when the first player crosses the
one-shot trigger that runs it; it is made an ordinary door here (RP_OpenUpDoor) so it does not
depend on him, and its "locked" use trigger (no3, which the door's target_deactivate switches off
in single player) is switched off.
------------------
*/
static void RP_FixNsStarpad( void )
{
	gentity_t *ent;

	RP_FOR_EACH_ENTITY( ent )
	{
		if ( RP_IsBrushEntity( ent, "trigger_multiple", "*111" ) &&
			ent->targetname && !Q_stricmp( ent->targetname, "backdoor" ) &&
			ent->target && !Q_stricmp( ent->target, "backdooropen2" ) )
		{
			ent->spawnflags &= ~128;	// INACTIVE
			ent->flags &= ~FL_INACTIVE;
		}
	}

	RP_OpenUpDoor( "backdooropen" );
	RP_DisableLockedTrigger( "no3", NULL );
}

/*
------------------
RP_FixKejimPost

Three passage doors that only single-player scripts open, none of which gets there in MP:
  - tower_door, a toggle door: check_bigdoor and finish_checking_door use it inside
    affect("kyle"), after Jan finds the big door jammed;
  - upper_door, a toggle door: the perimeter-defence script uses it inside affect("kyle"), with
    upper_door_deac, which switches off its Kyle-line use trigger upper_door_l;
  - monitor_room_door, two separate locked doors of one name, some 600 units apart: Jan opens
    them from a console, after a trigger only she can fire and three walks. The door's
    target_deactivate switches off their use triggers, green_room_l.
All three are made ordinary doors (RP_OpenUpDoor) and those use triggers are switched off. The
encounters the scripts start with them (tower troops, perimeter guns, the monitor-room squad)
stay unspawned, as they are without this.
------------------
*/
static void RP_FixKejimPost( void )
{
	RP_OpenUpDoor( "tower_door" );
	RP_OpenUpDoor( "upper_door" );
	RP_OpenUpDoor( "monitor_room_door" );
	RP_DisableLockedTrigger( "upper_door_l", NULL );
	RP_DisableLockedTrigger( "green_room_l", NULL );
}

/*
------------------
RP_FixKejimBase

lab_door, a locked door, is unlocked by place_mines: two stormtroopers placing mines, after one of
their lines. That now plays out in MP, but only if both miners -- spawned when someone first comes
through the doors before them -- are still alive when a player crosses the one-shot trigger that
runs it. Opening lab_door is what spawns lab_enemies, the officer carrying key3 among them, so a
lab_door that never unlocked also left keydoor3 shut. It is made an ordinary door (RP_OpenUpDoor):
its first opening still spawns lab_enemies (the spawners' count is 1, so later openings do not).
------------------
*/
static void RP_FixKejimBase( void )
{
	RP_OpenUpDoor( "lab_door" );
}

/*
------------------
RP_FixNsHideout

  - eye_door, the toggle door behind the eyeball: the eyeball script runs whole inside
    affect("kyle"), so the eye never asks and the door never opens. Made an ordinary door, and its
    "locked" use trigger (ed, which the door's target_deactivate switches off) is switched off;
  - end_door, the three-door team Lando runs out through once freed (only its master is locked):
    only the Lando rescue cutscene opens it (lando_free, run from cinematic19, with cutscene Kyle and Lando
    actors, walks and signal waits), which never finishes in MP. Made an ordinary door. Its
    "locked" use trigger is one of the many named roj (run_open_jail switches them all off when
    the cells open), so only that one, brush model *307, is switched off.
------------------
*/
static void RP_FixNsHideout( void )
{
	RP_OpenUpDoor( "eye_door" );
	RP_OpenUpDoor( "end_door" );
	RP_DisableLockedTrigger( "ed", NULL );
	RP_DisableLockedTrigger( "roj", "*307" );
}

/*
------------------
RP_FixBespinUndercity

vane_door is opened by reborn_dead, which the Reborn's death runs: it waits for a signal that is
only sent inside affect("kyle"), so in MP it waits for good. It is made an ordinary door
(RP_OpenUpDoor); its wait is -1, as the map has it, so the first player to walk up opens it for
good, which is how single player leaves it. Its "locked" use trigger (t292, which the door's
target_deactivate switches off) is switched off.
------------------
*/
static void RP_FixBespinUndercity( void )
{
	RP_OpenUpDoor( "vane_door" );
	RP_DisableLockedTrigger( "t292", NULL );
}

static void RP_JediOutcastMapFixes( const char *mapname )
{
	int m;

	for ( m = 0; m < (int)ARRAY_LEN( rp_jo_spawns ); m++ )
	{
		const rp_jo_spawns_t *map = &rp_jo_spawns[m];
		gentity_t *original;
		const char *target;
		int i;

		if ( Q_stricmp( mapname, map->map ) != 0 )
		{
			continue;
		}

		// the map's own start (SP_info_player_start renamed it); the string survives G_FreeEntity,
		// it lives in the level string pool
		original = G_Find( NULL, FOFS(classname), "info_player_deathmatch" );
		target = original ? original->target : NULL;

		if ( map->removeOriginal && original )
		{
			G_FreeEntity( original );
		}

		for ( i = 0; i < map->count; i++ )
		{
			RP_CreateSpawnPoint( map->points[i], target );
		}

		level.rp_spawn_target_once = qtrue;

		if ( !Q_stricmp( mapname, "bespin_streets" ) )
		{
			RP_FixBespinStreets();
		}
		else if ( !Q_stricmp( mapname, "artus_detention" ) )
		{
			RP_FixArtusDetention();
		}
		else if ( !Q_stricmp( mapname, "demo" ) || !Q_stricmp( mapname, "jodemo" ) )
		{
			RP_FixJediOutcastDemo();
		}
		else if ( !Q_stricmp( mapname, "ns_streets" ) )
		{
			RP_FixNsStreets();
		}
		else if ( !Q_stricmp( mapname, "ns_starpad" ) )
		{
			RP_FixNsStarpad();
		}
		else if ( !Q_stricmp( mapname, "kejim_post" ) )
		{
			RP_FixKejimPost();
		}
		else if ( !Q_stricmp( mapname, "kejim_base" ) )
		{
			RP_FixKejimBase();
		}
		else if ( !Q_stricmp( mapname, "ns_hideout" ) )
		{
			RP_FixNsHideout();
		}
		else if ( !Q_stricmp( mapname, "bespin_undercity" ) )
		{
			RP_FixBespinUndercity();
		}
		return;
	}
}


// GalaxyRP fix: [Guardian] Zyk_NPC_SpawnType() (and its local NPC_Spawn_Do extern) removed here — its only call sites were spawn_boss() and the dead quest guardians dispatch block, both permanently unreachable (spawn_boss has no callers)

/*
============
G_InitGame

============
*/
extern void RemoveAllWP(void);
extern void BG_ClearVehicleParseParms(void);
gentity_t *SelectRandomDeathmatchSpawnPoint( qboolean isbot );
void SP_info_jedimaster_start( gentity_t *ent );
extern void zyk_create_dir(char *file_path);
void G_InitGame( int levelTime, int randomSeed, int restart ) {
	int					i;
	int					rngDiscard;
	vmCvar_t	mapname;
	vmCvar_t	ckSum;
	char serverinfo[MAX_INFO_STRING] = {0};
	// zyk: variable used in the SP buged maps fix
	char zyk_mapname[128] = {0};
	FILE *zyk_entities_file = NULL;
	FILE *zyk_duel_arena_file = NULL;
	FILE *zyk_melee_arena_file = NULL;

	// GalaxyRP fix: [RNG] seed the module's random generator. Q_irand/Q_flrand/erandom all run off
	// a single "static uint32_t holdrand" in shared/qcommon/q_math.c, which only Rand_Init() ever
	// sets -- and nothing in codemp/ was calling it. The engine seeds its OWN copy in common.cpp,
	// but each module compiles q_math.c separately with hidden visibility, so jampgame, cgame and
	// ui each carry a private holdrand that stayed at its 0x89abcdef initialiser forever.
	//
	// That is not once per server start. SV_ShutdownGameProgs ("Called every time a map changes")
	// runs VM_Free -> Sys_UnloadDll, so the module is genuinely unloaded and reloaded on every map
	// load and holdrand returns to its initialiser. The first twenty Q_irand(0,100) values were
	// therefore 44 24 13 74 15 89 61 87 0 14 83 81 48 34 83 81 25 50 53 33 on every map, on every
	// server. 1066 Q_irand and 233 Q_flrand call sites read from it, including our own percentage
	// rolls -- the deflect check in g_cmds.c and the dismemberment roll in g_combat.c -- so those
	// produced an identical outcome pattern after every map load.
	//
	// Seeded at the very top so nothing can consume a value before the seed is set. Nothing
	// currently does (BG_VehicleLoadParms uses no RNG), but bg_panimate.c does, and this removes
	// the ordering question rather than relying on it.
	//
	// srand() is the C library generator, used separately by roll_dice() for /roll; it was already
	// being seeded further down and is moved up here to keep the two together.
	Rand_Init( randomSeed );
	srand( randomSeed );
	// GalaxyRP fix: [RNG] discard the first few outputs. The generator is a plain LCG
	//   holdrand = holdrand * 214013 + 2531011;  result = holdrand >> 17
	// and seeding it with a small number leaves the very first output poorly mixed. The seed here
	// is the engine's Com_Milliseconds(), which on a freshly started server is only a few thousand
	// at the first map load: measured over that band (2-8 s of uptime), the first Q_irand(0,100)
	// took only 31 of its 101 possible values. Discarding even one output takes it to all 101; four
	// costs nothing and leaves margin. OpenJK's fix (and TaystJK's port of it) seeds without this.
	for ( rngDiscard = 0; rngDiscard < 4; rngDiscard++ ) {
		Q_irand( 0, 1 );
	}

	//Init RMG to 0, it will be autoset to 1 if there is terrain on the level.
	trap->Cvar_Set("RMG", "0");
	RMG.integer = 0;

	//Clean up any client-server ghoul2 instance attachments that may still exist exe-side
	trap->G2API_CleanEntAttachments();

	BG_InitAnimsets(); //clear it out

	B_InitAlloc(); //make sure everything is clean

	trap->SV_RegisterSharedMemory( gSharedBuffer.raw );

	//Load external vehicle data
	BG_VehicleLoadParms();

	trap->Print ("------- Game Initialization -------\n");
	trap->Print ("gamename: %s\n", GAMEVERSION);
	trap->Print ("gamedate: %s\n", SOURCE_DATE);

	G_RegisterCvars();

	G_ProcessIPBans();

	G_InitMemory();

	// set some level globals
	memset( &level, 0, sizeof( level ) );
	level.time = levelTime;
	level.startTime = levelTime;

	// GalaxyRP: [Slot Reuse] the reuses already spent this map survive a map_restart, as the clients'
	// registrations do (RP_SlotRestore() in g_utils.c)
	RP_SlotRestore( restart );

	level.follow1 = level.follow2 = -1;

	level.snd_fry = G_SoundIndex("sound/player/fry.wav");	// FIXME standing in lava / slime

	level.snd_hack = G_SoundIndex("sound/player/hacking.wav");
	level.snd_medHealed = G_SoundIndex("sound/player/supp_healed.wav");
	level.snd_medSupplied = G_SoundIndex("sound/player/supp_supplied.wav");

	WP_RegisterForceLoopSounds();

	//trap->SP_RegisterServer("mp_svgame");

	if ( g_log.string[0] )
	{
		trap->FS_Open( g_log.string, &level.logFile, g_logSync.integer ? FS_APPEND_SYNC : FS_APPEND );
		if ( level.logFile )
			trap->Print( "Logging to %s\n", g_log.string );
		else
			trap->Print( "WARNING: Couldn't open logfile: %s\n", g_log.string );
	}
	else
		trap->Print( "Not logging game events to disk.\n" );

	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	G_LogPrintf( "------------------------------------------------------------\n" );
	G_LogPrintf( "InitGame: %s\n", serverinfo );

	if ( g_securityLog.integer )
	{
		if ( g_securityLog.integer == 1 )
			trap->FS_Open( SECURITY_LOG, &level.security.log, FS_APPEND );
		else if ( g_securityLog.integer == 2 )
			trap->FS_Open( SECURITY_LOG, &level.security.log, FS_APPEND_SYNC );

		if ( level.security.log )
			trap->Print( "Logging to "SECURITY_LOG"\n" );
		else
			trap->Print( "WARNING: Couldn't open logfile: "SECURITY_LOG"\n" );
	}
	else
		trap->Print( "Not logging security events to disk.\n" );


	G_LogWeaponInit();

	G_CacheGametype();

	G_InitWorldSession();

	// initialize all entities for this game
	// GalaxyRP: [Logical Entities] both regions -- the engine only ever hears about the first
	// MAX_GENTITIES of them, but the logical region is reused from map to map just the same.
	memset( g_entities, 0, MAX_ENTITIESTOTAL * sizeof(g_entities[0]) );
	level.gentities = g_entities;
	level.num_logicalents = 0;
	// GalaxyRP: [Logical Entities] the cvar is latched, but this copy is what the allocator reads,
	// so nothing can move between regions while a map is running.
	level.logical_entities_enabled = rp_logical_entities.integer ? qtrue : qfalse;

	// initialize all clients for this game
	level.maxclients = sv_maxclients.integer;
	memset( g_clients, 0, MAX_CLIENTS * sizeof(g_clients[0]) );
	level.clients = g_clients;

	// set client fields on player ents
	for ( i=0 ; i<level.maxclients ; i++ ) {
		g_entities[i].client = level.clients + i;
	}

	// always leave room for the max number of clients,
	// even if they aren't all used, so numbers inside that
	// range are NEVER anything but clients
	level.num_entities = MAX_CLIENTS;

	for ( i=0 ; i<MAX_CLIENTS ; i++ ) {
		g_entities[i].classname = "clientslot";
	}

	// let the server system know where the entites are
	trap->LocateGameData( (sharedEntity_t *)level.gentities, level.num_entities, sizeof( gentity_t ),
		&level.clients[0].ps, sizeof( level.clients[0] ) );

	//Load sabers.cfg data
	WP_SaberLoadParms();

	NPC_InitGame();

	TIMER_Clear();
	//
	//ICARUS INIT START

//	Com_Printf("------ ICARUS Initialization ------\n");

	trap->ICARUS_Init();

//	Com_Printf ("-----------------------------------\n");

	//ICARUS INIT END
	//

	// reserve some spots for dead player bodies
	InitBodyQue();

	ClearRegisteredItems();

	//make sure saber data is loaded before this! (so we can precache the appropriate hilts)
	InitSiegeMode();

	trap->Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
	G_CacheMapname( &mapname );
	trap->Cvar_Register( &ckSum, "sv_mapChecksum", "", CVAR_ROM );

	// navCalculatePaths	= ( trap->Nav_Load( mapname.string, ckSum.integer ) == qfalse );
	// zyk: commented line above. Was taking a lot of time to load some maps, example mp/duel7 and mp/siege_desert in FFA Mode
	// zyk: now it will always force calculating paths
	navCalculatePaths = qtrue;

	// zyk: getting mapname
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));
	strcpy(level.zykmapname, zyk_mapname);

	// GalaxyRP fix: [NPC] the is_vjun3_map flag that was set here is gone: its only reader dropped
	// protocol_imp and r2d2_imp on vjun3 unconditionally to stay under the old 16-entry
	// MAX_ANIM_FILES, which has been 128 since 3.47. Both types are on the rp_sp_npc_fix list.

	// GalaxyRP: [SP Maps] the single-player map lists moved into RP_IsJediAcademySPMap() and
	// RP_IsJediOutcastMap() above; sp_map is set from the first exactly as before.
	if ( RP_IsJediAcademySPMap( zyk_mapname ) )
	{
		level.sp_map = qtrue;
		level.rp_sp_game = RP_SP_GAME_JA;
		// GalaxyRP fix: [SP Maps] the start's target -- the map's intro / setup script, copied onto
		// every added spawn point by zyk_create_info_player_deathmatch() -- fires for the first
		// spawn only, as it already does on the Jedi Outcast maps (RP_JediOutcastMapFixes). Every
		// one of the 34 maps' start targets was checked: all are one-shot setup scriptrunners
		// (intros, cranes, clips, objectives, Kyle's setup on vjun1) or, on vjun2, the NPC_Kyle
		// spawner itself -- which spawned another Kyle on every respawn.
		level.rp_spawn_target_once = qtrue;
	}
	else if ( RP_IsJediOutcastMap( zyk_mapname ) )
	{
		level.rp_sp_game = RP_SP_GAME_JO;
	}

	// GalaxyRP fix: [Entity System] this MUST be reset before the line below and nowhere else.
	// The game module has no trap that reports how many inline models a map holds, so
	// zyk_brush_model_allowed() learns the bound by watching the map's own brush entities as they
	// spawn -- and that is the call below. It used to be zeroed 200 lines further down, still
	// inside this function, which threw the bound away as soon as it had been learned: from then
	// on the map believed it had no inline models past *0, and every /entload, /entadd and
	// /entedit that named a brush model was refused. Doors came back with no brush (invisible,
	// non-solid, so the doorway reads as permanently open), and every refused trigger collapsed
	// to a zero-size volume at the world origin.
	//
	// memset( &level, ... ) above already zeroes it; this is here to say where the bound is
	// filled in, and to make an assignment after the spawn look as wrong as it is.
	level.zyk_max_inline_model = 0;

	// parse the key/value pairs and spawn gentities
	G_SpawnEntitiesFromString(qfalse);

	if (level.gametype == GT_CTF)
	{ // zyk: maps that will now have support to CTF gametype (like some SP maps) must have the CTF flags placed before the G_CheckTeamItems function call
		if (Q_stricmp(zyk_mapname, "t1_fatal") == 0)
		{
			zyk_create_ctf_flag_spawn(-2366,-2561,4536,qtrue);
			zyk_create_ctf_flag_spawn(2484,1732,4656,qfalse);
		}
		else if (Q_stricmp(zyk_mapname, "t1_rail") == 0)
		{
			zyk_create_ctf_flag_spawn(-2607,-4,24,qtrue);
			zyk_create_ctf_flag_spawn(23146,-3,216,qfalse);
		}
		else if (Q_stricmp(zyk_mapname, "t1_surprise") == 0)
		{
			zyk_create_ctf_flag_spawn(1337,-6492,224,qtrue);
			zyk_create_ctf_flag_spawn(2098,4966,800,qfalse);
		}
		else if (Q_stricmp(zyk_mapname, "t2_dpred") == 0)
		{
			zyk_create_ctf_flag_spawn(3,-3974,664,qtrue);
			zyk_create_ctf_flag_spawn(-701,126,24,qfalse);
		}
		else if (Q_stricmp(zyk_mapname, "t2_trip") == 0)
		{
			zyk_create_ctf_flag_spawn(-20421,18244,1704,qtrue);
			zyk_create_ctf_flag_spawn(19903,-2638,1672,qfalse);
		}
		else if (Q_stricmp(zyk_mapname, "t3_bounty") == 0)
		{
			zyk_create_ctf_flag_spawn(-7538,-545,-327,qtrue);
			zyk_create_ctf_flag_spawn(614,-509,344,qfalse);
		}
	}

	// general initialization
	G_FindTeams();
	RP_ToggleMoverTeamsMatch();	// GalaxyRP: [SP Maps] a team of rotators/bobbers/pendulums starts as its master

	// make sure we have flags for CTF, etc
	if( level.gametype >= GT_TEAM ) {
		G_CheckTeamItems();
	}
	else if ( level.gametype == GT_JEDIMASTER )
	{
		trap->SetConfigstring ( CS_CLIENT_JEDIMASTER, "-1" );
	}

	if (level.gametype == GT_POWERDUEL)
	{
		trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("-1|-1|-1") );
	}
	else
	{
		trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("-1|-1") );
	}
// nmckenzie: DUEL_HEALTH: Default.
	trap->SetConfigstring ( CS_CLIENT_DUELHEALTHS, va("-1|-1|!") );
	trap->SetConfigstring ( CS_CLIENT_DUELWINNER, va("-1") );

	if (1)
	{ // zyk: registering all items because of entity system
		int item_it = 0;

		for (item_it = 0; item_it < bg_numItems; item_it++)
		{
			gitem_t *this_item = &bg_itemlist[item_it];
			if (this_item)
			{
				RegisterItem(this_item);
			}
		}
	}

	SaveRegisteredItems();

	//trap->Print ("-----------------------------------\n");

	if( level.gametype == GT_SINGLE_PLAYER || trap->Cvar_VariableIntegerValue( "com_buildScript" ) ) {
		G_ModelIndex( SP_PODIUM_MODEL );
		G_SoundIndex( "sound/player/gurp1.wav" );
		G_SoundIndex( "sound/player/gurp2.wav" );
	}

	if ( trap->Cvar_VariableIntegerValue( "bot_enable" ) ) {
		BotAISetup( restart );
		BotAILoadMap( restart );
		G_InitBots( );
	} else {
		G_LoadArenas();
	}

	if ( level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL )
	{
		G_LogPrintf("Duel Tournament Begun: kill limit %d, win limit: %d\n", fraglimit.integer, duel_fraglimit.integer );
	}

	if ( navCalculatePaths )
	{//not loaded - need to calc paths
		navCalcPathTime = level.time + START_TIME_NAV_CALC;//make sure all ents are in and linked
	}
	else
	{//loaded
		//FIXME: if this is from a loadgame, it needs to be sure to write this
		//out whenever you do a savegame since the edges and routes are dynamic...
		//OR: always do a navigator.CheckBlockedEdges() on map startup after nav-load/calc-paths
		//navigator.pathsCalculated = qtrue;//just to be safe?  Does this get saved out?  No... assumed
		trap->Nav_SetPathsCalculated(qtrue);
		//need to do this, because combatpoint waypoints aren't saved out...?
		CP_FindCombatPointWaypoints();
		navCalcPathTime = 0;

		/*
		if ( g_eSavedGameJustLoaded == eNO )
		{//clear all the failed edges unless we just loaded the game (which would include failed edges)
			trap->Nav_ClearAllFailedEdges();
		}
		*/
		//No loading games in MP.
	}

	if (level.gametype == GT_SIEGE)
	{ //just get these configstrings registered now...
		while (i < MAX_CUSTOM_SIEGE_SOUNDS)
		{
			if (!bg_customSiegeSoundNames[i])
			{
				break;
			}
			G_SoundIndex((char *)bg_customSiegeSoundNames[i]);
			i++;
		}
	}

	if ( level.gametype == GT_JEDIMASTER ) {
		gentity_t *ent = NULL;
		int i=0;
		for ( i=0, ent=g_entities; i<level.num_entities; i++, ent++ ) {
			if ( ent->isSaberEntity )
				break;
		}

		if ( i == level.num_entities ) {
			// no JM saber found. drop one at one of the player spawnpoints
			gentity_t *spawnpoint = SelectRandomDeathmatchSpawnPoint( qfalse );

			if( !spawnpoint ) {
				trap->Error( ERR_DROP, "Couldn't find an FFA spawnpoint to drop the jedimaster saber at!\n" );
				return;
			}

			ent = G_Spawn();
			G_SetOrigin( ent, spawnpoint->s.origin );
			SP_info_jedimaster_start( ent );
		}
	}

	// GalaxyRP fix: [Guardian] level.quest_effect_id init removed here — the quest_effect_id field
	// itself (g_local.h) was deleted as dead (its sole reader/writer, clean_effect(), is gone).

	// GalaxyRP fix: [Guardian] level.boss_battle_music_reset_timer init removed here — field removed as dead (see g_local.h)

	level.voting_player = -1;

	level.server_empty_change_map_timer = 0;
	level.num_fully_connected_clients = 0;

	// zyk: initializing Duel Tournament variables
	level.duel_tournament_mode = 0;
	level.duel_tournament_paused = qfalse;
	level.duelists_quantity = 0;
	level.duel_matches_quantity = 0;
	level.duel_matches_done = 0;
	level.duel_tournament_rounds = 0;
	level.duel_tournament_timer = 0;
	level.duelist_1_id = -1;
	level.duelist_2_id = -1;
	level.duel_tournament_model_id = -1;
	level.duel_arena_loaded = qfalse;

	// zyk: initializing Melee Battle variables
	level.melee_mode = 0;
	level.melee_model_id = -1;
	level.melee_mode_timer = 0;
	level.melee_mode_quantity = 0;
	level.melee_arena_loaded = qfalse;

	level.last_spawned_entity = NULL;

	level.ent_origin_set = qfalse;

	level.load_entities_timer = 0;
	strcpy(level.load_entities_file,"");

	{
		int zyk_iterator = 0;

		for (zyk_iterator = 0; zyk_iterator < MAX_CLIENTS; zyk_iterator++)
		{ // zyk: initializing duelist scores
			level.duel_players[zyk_iterator] = -1;
			level.melee_players[zyk_iterator] = -1;
		}

		for (zyk_iterator = 0; zyk_iterator < MAX_DUEL_MATCHES; zyk_iterator++)
		{ // zyk: initializing duel matches
			level.duel_matches[zyk_iterator][0] = -1;
			level.duel_matches[zyk_iterator][1] = -1;
			level.duel_matches[zyk_iterator][2] = 0;
			level.duel_matches[zyk_iterator][3] = 0;
		}

		// GalaxyRP fix: [Magic] the level.special_power_effects[] / _timer[] init loop used to be
		// here. Both arrays went with the magic effect engine. Worth recording: this loop ran to
		// ENTITYNUM_MAX_NORMAL while the arrays were sized MAX_ENTITIESTOTAL, so the logical-entity
		// half was never seeded with the -1 sentinel and sat at 0 -- "owned by entity 0". Nothing
		// reachable ever read those slots, but the inconsistency goes with the arrays.

		for (zyk_iterator = 0; zyk_iterator < MAX_CLIENTS; zyk_iterator++)
		{
			level.ignored_players[zyk_iterator][0] = 0;
			level.ignored_players[zyk_iterator][1] = 0;
		}
	}

	// zyk: added this fix for SP maps
	if (Q_stricmp(zyk_mapname, "academy1") == 0)
	{
		zyk_create_info_player_deathmatch(-1308,272,729,-90);
		zyk_create_info_player_deathmatch(-1508,272,729,-90);
	}
	else if (Q_stricmp(zyk_mapname, "academy2") == 0)
	{
		zyk_create_info_player_deathmatch(-1308,272,729,-90);
		zyk_create_info_player_deathmatch(-1508,272,729,-90);
	}
	else if (Q_stricmp(zyk_mapname, "academy3") == 0)
	{
		zyk_create_info_player_deathmatch(-1308,272,729,-90);
		zyk_create_info_player_deathmatch(-1508,272,729,-90);
	}
	else if (Q_stricmp(zyk_mapname, "academy4") == 0)
	{
		zyk_create_info_player_deathmatch(-1308,272,729,-90);
		zyk_create_info_player_deathmatch(-1508,272,729,-90);
	}
	else if (Q_stricmp(zyk_mapname, "academy5") == 0)
	{
		zyk_create_info_player_deathmatch(-1308,272,729,-90);
		zyk_create_info_player_deathmatch(-1508,272,729,-90);
	}
	else if (Q_stricmp(zyk_mapname, "academy6") == 0)
	{
		zyk_create_info_player_deathmatch(-1308,272,729,-90);
		zyk_create_info_player_deathmatch(-1508,272,729,-90);

		// zyk: hangar spawn points
		zyk_create_info_player_deathmatch(-23,458,-486,0);
		zyk_create_info_player_deathmatch(2053,3401,-486,-90);
		zyk_create_info_player_deathmatch(4870,455,-486,-179);
	}
	else if (Q_stricmp(zyk_mapname, "yavin1") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp( ent->targetname, "end_level") == 0)
			{ // zyk: remove the map change entity
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(472,-4833,437,74);
		zyk_create_info_player_deathmatch(-167,-4046,480,0);
	}
	else if (Q_stricmp(zyk_mapname, "yavin1b") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp( ent->targetname, "door1") == 0)
			{
				fix_sp_func_door(ent);
			}
			else if (Q_stricmp( ent->classname, "trigger_hurt") == 0 && Q_stricmp( ent->targetname, "tree_hurt_trigger") != 0)
			{ // zyk: trigger_hurt entity of the bridge area
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(472,-4833,437,74);
		zyk_create_info_player_deathmatch(-167,-4046,480,0);
	}
	else if (Q_stricmp(zyk_mapname, "yavin2") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp( ent->targetname, "t530") == 0 || Q_stricmp( ent->targetname, "Putz_door") == 0 || Q_stricmp( ent->targetname, "afterdroid_door") == 0 || Q_stricmp( ent->targetname, "pit_door") == 0 || Q_stricmp( ent->targetname, "door1") == 0)
			{
				fix_sp_func_door(ent);
			}
			else if (Q_stricmp( ent->classname, "trigger_hurt") == 0 && ent->spawnflags == 62)
			{ // zyk: removes the trigger hurt entity of the second bridge
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(2516,-5593,89,-179);
		zyk_create_info_player_deathmatch(2516,-5443,89,-179);
	}
	else if (Q_stricmp(zyk_mapname, "hoth2") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp( ent->targetname, "end_level") == 0)
			{ // zyk: remove the map change entity
				G_FreeEntity( ent );
			}
		}

		zyk_create_info_player_deathmatch(-2114,10195,1027,-14);
		zyk_create_info_player_deathmatch(-1808,9640,982,-17);
	}
	else if (Q_stricmp(zyk_mapname, "hoth3") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp( ent->targetname, "end_level") == 0)
			{ // zyk: remove the map change entity
				G_FreeEntity( ent );
			}
			// GalaxyRP: [SP Maps] these were entities 232 and 233 (New Zyk mod picks them by slot); matched
			// by what they are, which is the same two: the script runners for elevator1 near the end of
			// the map, hoth3/elevator (t146, the call button) and common/elevator (t145, the car). Their
			// name is removed, so neither trigger reaches its script any more.
			if (Q_stricmp(ent->classname, "target_scriptrunner") == 0 &&
				(Q_stricmp(ent->targetname, "t146") == 0 || Q_stricmp(ent->targetname, "t145") == 0))
			{ // zyk: fixing the final door
				ent->targetname = NULL;
				zyk_main_set_entity_field(ent, "targetname", "zykremovekey");

				zyk_main_spawn_entity(ent);
			}
		}

		zyk_create_info_player_deathmatch(-1908,562,992,-90);
		zyk_create_info_player_deathmatch(-1907,356,801,-90);
	}
	else if (Q_stricmp(zyk_mapname, "t1_danger") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp( ent->classname, "NPC_Monster_Sand_Creature") == 0)
			{ // zyk: remove the map change entity
				G_FreeEntity( ent );
			}
		}

		zyk_create_info_player_deathmatch(-3705,-3362,1121,90);
		zyk_create_info_player_deathmatch(-3705,-2993,1121,90);
	}
	else if (Q_stricmp(zyk_mapname, "t1_fatal") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{

			if (Q_stricmp(ent->targetname, "door_trap") == 0)
			{ // zyk: fixing this door so it will not lock
				fix_sp_func_door(ent);
			}
				
			if (Q_stricmp(ent->targetname, "lobbydoor1") == 0 || Q_stricmp(ent->targetname, "lobbydoor2") == 0)
			{ // zyk: fixing these doors so they will not lock
				GlobalUse(ent, ent, ent);
			}

			// GalaxyRP: [SP Maps] t7708017 and t7708018 are the key_2 and key_3 security doors. Zyk unlocked
			// them here at load because MP had no keys; it has them now (a dead key officer's body hands
			// the key over, the misc_security_panel takes it and unlocks the door for everyone), so they
			// stay locked until a player brings the key, as in single player. The key officers spawn only
			// once, so in CTF, whose bases this map was set up with, they are still unlocked at load.
			if (level.gametype == GT_CTF &&
				(Q_stricmp(ent->targetname, "t7708018") == 0 || Q_stricmp(ent->targetname, "t7708017") == 0))
			{ // zyk: fixing these doors so they will not lock
				GlobalUse(ent, ent, ent);
			}

			// GalaxyRP fix: [SP Maps] a removal of entity 443 ("trigger_hurt at the spawn area") used to
			// be here. On this map 443 is not that trigger: it is func_door *137, an ordinary automatic
			// door, and freeing it closed nothing but left the area portal under it shut -- the doorway
			// showed the void. The trigger it meant (*139, two slots later) is the falling-death volume
			// of the chasm around the spawn platform, and parts of that chasm have no floor at all, so
			// removing it would leave a player falling for ever. Both are left as the map made them.

		}
		zyk_create_info_player_deathmatch(-1563,-4241,4569,-157);
		zyk_create_info_player_deathmatch(-1135,-4303,4569,179);

		if (level.gametype == GT_CTF)
		{ // zyk: in CTF, add the team player spawns
			zyk_create_ctf_player_spawn(-3083,-2683,4696,-90,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-2371,-3325,4536,90,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-1726,-2957,4536,90,qtrue,qtrue);

			zyk_create_ctf_player_spawn(1277,2947,4540,-45,qfalse,qtrue);
			zyk_create_ctf_player_spawn(3740,482,4536,135,qfalse,qtrue);
			zyk_create_ctf_player_spawn(2489,1451,4536,135,qfalse,qtrue);

			zyk_create_ctf_player_spawn(-3083,-2683,4696,-90,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-2371,-3325,4536,90,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-1726,-2957,4536,90,qtrue,qfalse);

			zyk_create_ctf_player_spawn(1277,2947,4540,-45,qfalse,qfalse);
			zyk_create_ctf_player_spawn(3740,482,4536,135,qfalse,qfalse);
			zyk_create_ctf_player_spawn(2489,1451,4536,135,qfalse,qfalse);
		}
	}
	else if (Q_stricmp(zyk_mapname, "t1_inter") == 0)
	{
		zyk_create_info_player_deathmatch(-65,-686,89,90);
		zyk_create_info_player_deathmatch(56,-686,89,90);
		// GalaxyRP: [SP Maps] six more from New Zyk mod: two extending the row above, and four in the
		// gaps between the bunks along the side walls, facing into the room. Checked against the map:
		// clear standing room, flat floor at the start's height, clear of the NPCs and of the
		// intermission trigger on the original start.
		zyk_create_info_player_deathmatch(-329, -686, 89, 90);
		zyk_create_info_player_deathmatch(202, -686, 89, 90);
		zyk_create_info_player_deathmatch(-411, -491, 89, 0);
		zyk_create_info_player_deathmatch(280, -491, 89, 179);
		zyk_create_info_player_deathmatch(-411, -252, 89, 0);
		zyk_create_info_player_deathmatch(280, -252, 89, 179);
	}
	else if (Q_stricmp(zyk_mapname, "t1_rail") == 0)
	{
		zyk_create_info_player_deathmatch(-3135,1,33,0);
		zyk_create_info_player_deathmatch(-3135,197,25,0);

		if (level.gametype == GT_CTF)
		{ // zyk: in CTF, add the team player spawns
			zyk_create_ctf_player_spawn(-2569,-2,25,179,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-1632,257,136,-90,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-1743,0,500,0,qtrue,qtrue);

			zyk_create_ctf_player_spawn(22760,-128,152,90,qfalse,qtrue);
			zyk_create_ctf_player_spawn(22866,0,440,-179,qfalse,qtrue);
			zyk_create_ctf_player_spawn(21102,2,464,179,qfalse,qtrue);

			zyk_create_ctf_player_spawn(-2569,-2,25,179,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-1632,257,136,-90,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-1743,0,500,0,qtrue,qfalse);

			zyk_create_ctf_player_spawn(22760,-128,152,90,qfalse,qfalse);
			zyk_create_ctf_player_spawn(22866,0,440,-179,qfalse,qfalse);
			zyk_create_ctf_player_spawn(21102,2,464,179,qfalse,qfalse);
		}
	}
	else if (Q_stricmp(zyk_mapname, "t1_sour") == 0)
	{
		zyk_create_info_player_deathmatch(9828,-5521,153,90);
		zyk_create_info_player_deathmatch(9845,-5262,153,153);
	}
	else if (Q_stricmp(zyk_mapname, "t1_surprise") == 0)
	{
		gentity_t *ent;
		qboolean found_bugged_switch = qfalse;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{

			if (Q_stricmp( ent->targetname, "fire_hurt") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "droid_door") == 0)
			{
				fix_sp_func_door(ent);
			}
			if (Q_stricmp( ent->targetname, "tube_door") == 0)
			{
				fix_sp_func_door(ent);
			}
			if (found_bugged_switch == qfalse && Q_stricmp( ent->classname, "misc_model_breakable") == 0 && Q_stricmp( ent->model, "models/map_objects/desert/switch3.md3") == 0)
			{
				G_FreeEntity(ent);
				found_bugged_switch = qtrue;
			}
			if (Q_stricmp( ent->classname, "func_static") == 0 && (int)ent->s.origin[0] == 3064 && (int)ent->s.origin[1] == 5040 && (int)ent->s.origin[2] == 892)
			{ // zyk: elevator inside sand crawler near the wall fire
				G_FreeEntity( ent );
			}
			// GalaxyRP: [SP Maps] the brush model alone identifies it (a slot test above 200 went with it,
			// and matched the same door); it is tube_door's team partner, freed after the master
			if (Q_stricmp( ent->classname, "func_door") == 0 && Q_stricmp( ent->model, "*63") == 0)
			{ // zyk: tube door in which the droid goes in SP
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(1913,-6151,222,153);
		zyk_create_info_player_deathmatch(1921,-5812,222,-179);

		if (level.gametype == GT_CTF)
		{ // zyk: in CTF, add the team player spawns
			zyk_create_ctf_player_spawn(1948,-6020,222,138,qtrue,qtrue);
			zyk_create_ctf_player_spawn(1994,-4597,908,19,qtrue,qtrue);
			zyk_create_ctf_player_spawn(404,-4521,249,-21,qtrue,qtrue);

			zyk_create_ctf_player_spawn(2341,4599,1056,83,qfalse,qtrue);
			zyk_create_ctf_player_spawn(1901,5425,916,-177,qfalse,qtrue);
			zyk_create_ctf_player_spawn(918,3856,944,0,qfalse,qtrue);

			zyk_create_ctf_player_spawn(1948,-6020,222,138,qtrue,qfalse);
			zyk_create_ctf_player_spawn(1994,-4597,908,19,qtrue,qfalse);
			zyk_create_ctf_player_spawn(404,-4521,249,-21,qtrue,qfalse);

			zyk_create_ctf_player_spawn(2341,4599,1056,83,qfalse,qfalse);
			zyk_create_ctf_player_spawn(1901,5425,916,-177,qfalse,qfalse);
			zyk_create_ctf_player_spawn(918,3856,944,0,qfalse,qfalse);
		}
	}
	else if (Q_stricmp(zyk_mapname, "t2_rancor") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			
			if (Q_stricmp( ent->targetname, "t857") == 0)
			{
				fix_sp_func_door(ent);
			}
			if (Q_stricmp( ent->targetname, "Kill_Brush_Canyon") == 0)
			{ // zyk: trigger_hurt at the spawn area
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(-898,1178,1718,90);
		zyk_create_info_player_deathmatch(-898,1032,1718,90);
	}
	else if (Q_stricmp(zyk_mapname, "t2_rogue") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp( ent->targetname, "t475") == 0)
			{ // zyk: remove the invisible wall at the end of the bridge at start
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->target, "field_counter1") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->target, "field_counter2") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->target, "field_counter3") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "end_level") == 0)
			{ // zyk: remove the map change entity
				G_FreeEntity( ent );
			}
			if (Q_stricmp(ent->targetname, "ractoroomdoor") == 0)
			{ // zyk: remove office door
				G_FreeEntity(ent);
			}
			// GalaxyRP: [SP Maps] these two were entities 142 and 166 (New Zyk mod picks them by slot);
			// matched by brush model, which is the same two: the scripted elevator car
			// (penthouse_elevator1, whose *38 the func_plat below reuses) and its use-button
			if (Q_stricmp(ent->classname, "func_static") == 0 && Q_stricmp(ent->model, "*38") == 0)
			{ // zyk: remove the elevator
				G_FreeEntity(ent);
			}
			if (Q_stricmp(ent->classname, "trigger_multiple") == 0 && Q_stricmp(ent->model, "*47") == 0)
			{ // zyk: remove the elevator button
				G_FreeEntity(ent);
			}
		}

		// zyk: adding new elevator and buttons that work properly
		ent = G_Spawn();

		zyk_main_set_entity_field(ent, "classname", "func_plat");
		zyk_main_set_entity_field(ent, "spawnflags", "4096");
		zyk_main_set_entity_field(ent, "targetname", "zyk_lift_1");
		zyk_main_set_entity_field(ent, "lip", "8");
		zyk_main_set_entity_field(ent, "height", "1280");
		zyk_main_set_entity_field(ent, "speed", "200");
		zyk_main_set_entity_field(ent, "model", "*38");
		zyk_main_set_entity_field(ent, "origin", "2848 2144 700");
		zyk_main_set_entity_field(ent, "soundSet", "platform");

		zyk_main_spawn_entity(ent);

		ent = G_Spawn();

		zyk_main_set_entity_field(ent, "classname", "trigger_multiple");
		zyk_main_set_entity_field(ent, "spawnflags", "4");
		zyk_main_set_entity_field(ent, "target", "zyk_lift_1");
		zyk_main_set_entity_field(ent, "origin", "2664 2000 728");
		zyk_main_set_entity_field(ent, "mins", "-32 -32 -32");
		zyk_main_set_entity_field(ent, "maxs", "32 32 32");
		zyk_main_set_entity_field(ent, "wait", "1");
		zyk_main_set_entity_field(ent, "delay", "2");

		zyk_main_spawn_entity(ent);

		ent = G_Spawn();

		zyk_main_set_entity_field(ent, "classname", "trigger_multiple");
		zyk_main_set_entity_field(ent, "spawnflags", "4");
		zyk_main_set_entity_field(ent, "target", "zyk_lift_1");
		zyk_main_set_entity_field(ent, "origin", "2577 2023 -551");
		zyk_main_set_entity_field(ent, "mins", "-32 -32 -32");
		zyk_main_set_entity_field(ent, "maxs", "32 32 32");
		zyk_main_set_entity_field(ent, "wait", "1");
		zyk_main_set_entity_field(ent, "delay", "2");

		zyk_main_spawn_entity(ent);

		zyk_create_info_player_deathmatch(1974,-1983,-550,90);
		zyk_create_info_player_deathmatch(1779,-1983,-550,90);
	}
	else if (Q_stricmp(zyk_mapname, "t2_trip") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp( ent->targetname, "t546") == 0)
			{
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->targetname, "end_level") == 0)
			{
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->targetname, "cin_door") == 0)
			{
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->targetname, "endJaden") == 0)
			{
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->targetname, "endJaden2") == 0)
			{
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->targetname, "endswoop") == 0)
			{
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->classname, "func_door") == 0 && Q_stricmp( ent->model, "*79" ) == 0)
			{ // zyk: door in the far end of the map, past the teleports the old Race Mode used
			  // GalaxyRP: [SP Maps] matched by brush model; this was "any func_door above slot 200",
			  // and *79 is the only one there once cin_door (*78) has been freed by name above
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->targetname, "t547") == 0)
			{ // zyk: removes the swoop at the end of the map
			  // GalaxyRP: [Race Mode] this removal and the func_door one above were written for Race Mode,
			  // which is gone. Kept anyway: both are unconditional map fix-ups that run on every t2_trip
			  // load, so dropping them would put a stray swoop and a door back into the map for everyone --
			  // a content change rather than dead-code removal.
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(-5698,-22304,1705,90);
		zyk_create_info_player_deathmatch(-5433,-22328,1705,90);

		if (level.gametype == GT_CTF)
		{ // zyk: in CTF, add the team player spawns
			zyk_create_ctf_player_spawn(-20705,18794,1704,0,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-20729,17692,1704,0,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-20204,18736,1503,0,qtrue,qtrue);

			zyk_create_ctf_player_spawn(20494,-2922,1672,90,qfalse,qtrue);
			zyk_create_ctf_player_spawn(19321,-2910,1672,90,qfalse,qtrue);
			zyk_create_ctf_player_spawn(19428,-2404,1470,90,qfalse,qtrue);

			zyk_create_ctf_player_spawn(-20705,18794,1704,0,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-20729,17692,1704,0,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-20204,18736,1503,0,qtrue,qfalse);

			zyk_create_ctf_player_spawn(20494,-2922,1672,90,qfalse,qfalse);
			zyk_create_ctf_player_spawn(19321,-2910,1672,90,qfalse,qfalse);
			zyk_create_ctf_player_spawn(19428,-2404,1470,90,qfalse,qfalse);
		}
	}
	else if (Q_stricmp(zyk_mapname, "t2_wedge") == 0)
	{
		zyk_create_info_player_deathmatch(6328,539,-110,-178);
		zyk_create_info_player_deathmatch(6332,743,-110,-178);
	}
	else if (Q_stricmp(zyk_mapname, "t2_dpred") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp( ent->targetname, "prisonshield1") == 0)
			{
				G_FreeEntity( ent );
			}
			// GalaxyRP: [SP Maps] t556 is the loading-room security door (key "loadingroom_key", carried by
			// an officer of corridortroopers1), plus a target_deactivate of the same name that silences the
			// door's "locked" sound trigger once the panel opens it. MP has the keys now, so both stay, as
			// in single player; in CTF, whose bases this map was set up with, the door is still removed
			// (the key officer spawns only once).
			if (level.gametype == GT_CTF && Q_stricmp( ent->targetname, "t556") == 0)
			{
				fix_sp_func_door(ent);
			}
			if (Q_stricmp( ent->target, "field_counter1") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->target, "t62241") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->target, "t62243") == 0)
			{
				G_FreeEntity( ent );
			}
		}

		zyk_create_info_player_deathmatch(-2152,-3885,-134,90);
		zyk_create_info_player_deathmatch(-2152,-3944,-134,90);

		if (level.gametype == GT_CTF)
		{ // zyk: in CTF, add the team player spawns
			zyk_create_ctf_player_spawn(0,-4640,664,90,qtrue,qtrue);
			zyk_create_ctf_player_spawn(485,-3721,632,-179,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-212,-3325,656,-179,qtrue,qtrue);

			zyk_create_ctf_player_spawn(0,125,24,-90,qfalse,qtrue);
			zyk_create_ctf_player_spawn(-1242,128,24,0,qfalse,qtrue);
			zyk_create_ctf_player_spawn(369,67,296,-179,qfalse,qtrue);

			zyk_create_ctf_player_spawn(0,-4640,664,90,qtrue,qfalse);
			zyk_create_ctf_player_spawn(485,-3721,632,-179,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-212,-3325,656,-179,qtrue,qfalse);

			zyk_create_ctf_player_spawn(0,125,24,-90,qfalse,qfalse);
			zyk_create_ctf_player_spawn(-1242,128,24,0,qfalse,qfalse);
			zyk_create_ctf_player_spawn(369,67,296,-179,qfalse,qfalse);
		}
	}
	else if (Q_stricmp(zyk_mapname, "vjun1") == 0)
	{
		gentity_t *ent;
		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			// GalaxyRP: [SP Maps] entities 123 and 124 in New Zyk mod: the map's only two TIE bombers
			if (Q_stricmp(ent->classname, "misc_model_breakable") == 0 &&
				(Q_stricmp(ent->script_targetname, "tie1") == 0 || Q_stricmp(ent->script_targetname, "tie2") == 0))
			{ // zyk: removing tie fighter misc_model_breakable entities to prevent client crashes
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(-6897,7035,857,-90);
		zyk_create_info_player_deathmatch(-7271,7034,857,-90);
	}
	else if (Q_stricmp(zyk_mapname, "vjun2") == 0)
	{
		zyk_create_info_player_deathmatch(-831,166,217,90);
		zyk_create_info_player_deathmatch(-700,166,217,90);
	}
	else if (Q_stricmp(zyk_mapname, "vjun3") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp( ent->targetname, "end_level") == 0)
			{
				G_FreeEntity( ent );
			}
		}

		zyk_create_info_player_deathmatch(-8272,-391,1433,179);
		zyk_create_info_player_deathmatch(-8375,-722,1433,179);
	}
	else if (Q_stricmp(zyk_mapname, "t3_hevil") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			// GalaxyRP: [SP Maps] entity 42 in New Zyk mod: a 5000-damage trigger_hurt
			if (RP_IsBrushEntity(ent, "trigger_hurt", "*3"))
			{
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(512,-2747,-742,90);
		zyk_create_info_player_deathmatch(872,-2445,-742,108);
	}
	else if (Q_stricmp(zyk_mapname, "t3_bounty") == 0)
	{
		zyk_create_info_player_deathmatch(-3721,-726,73,75);
		zyk_create_info_player_deathmatch(-3198,-706,73,90);

		if (level.gametype == GT_CTF)
		{ // zyk: in CTF, add the team player spawns
			zyk_create_ctf_player_spawn(-7740,-543,-263,0,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-8470,-210,24,90,qtrue,qtrue);
			zyk_create_ctf_player_spawn(-7999,-709,-7,132,qtrue,qtrue);

			zyk_create_ctf_player_spawn(616,-978,344,0,qfalse,qtrue);
			zyk_create_ctf_player_spawn(595,482,360,-90,qfalse,qtrue);
			zyk_create_ctf_player_spawn(1242,255,36,-179,qfalse,qtrue);

			zyk_create_ctf_player_spawn(-7740,-543,-263,0,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-8470,-210,24,90,qtrue,qfalse);
			zyk_create_ctf_player_spawn(-7999,-709,-7,132,qtrue,qfalse);

			zyk_create_ctf_player_spawn(616,-978,344,0,qfalse,qfalse);
			zyk_create_ctf_player_spawn(595,482,360,-90,qfalse,qfalse);
			zyk_create_ctf_player_spawn(1242,255,36,-179,qfalse,qfalse);
		}
	}
	else if (Q_stricmp(zyk_mapname, "t3_byss") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			// GalaxyRP: [SP Maps] zyk removed wall_door1 here; it is the key1 security door, and MP has the
			// keys now (the officer in the t4 group carries it), so it stays, as in single player.
			if (Q_stricmp( ent->target, "field_counter1") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "wave1_tie1") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "wave1_tie2") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "wave1_tie3") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "wave2_tie1") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "wave2_tie2") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "wave2_tie3") == 0)
			{
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(968,111,25,-90);
		zyk_create_info_player_deathmatch(624,563,25,-90);
	}
	else if (Q_stricmp(zyk_mapname, "t3_rift") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp( ent->targetname, "fakewall1") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "t778") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "t779") == 0)
			{
				G_FreeEntity( ent );
			}
		}

		zyk_create_info_player_deathmatch(2195,7611,4380,-90);
		zyk_create_info_player_deathmatch(2305,7640,4380,-90);
	}
	else if (Q_stricmp(zyk_mapname, "t3_stamp") == 0)
	{
		zyk_create_info_player_deathmatch(1208,445,89,179);
		zyk_create_info_player_deathmatch(1208,510,89,179);
	}
	else if (Q_stricmp(zyk_mapname, "taspir1") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp(ent->targetname, "t278") == 0)
			{
				G_FreeEntity(ent);
			}
			// GalaxyRP: [SP Maps] zyk removed bldg2_ext_door here; it is a security door (key
			// "bldg2_ext_door_lock", carried by the commander in building2_3), with a target_deactivate of
			// the same name that silences its "locked" sound trigger once the panel opens it. MP has the
			// keys now, so both stay, as in single player.
			if (Q_stricmp(ent->targetname, "end_level") == 0)
			{
				G_FreeEntity(ent);
			}
		}
		zyk_create_info_player_deathmatch(-1609, -1792, 649, 112);
		zyk_create_info_player_deathmatch(-1791, -1838, 649, 90);
	}
	else if (Q_stricmp(zyk_mapname, "taspir2") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp(ent->targetname, "force_field") == 0)
			{
				G_FreeEntity(ent);
			}
			if (Q_stricmp(ent->targetname, "kill_toggle") == 0)
			{
				G_FreeEntity(ent);
			}
		}

		zyk_create_info_player_deathmatch(286, -2859, 345, 92);
		zyk_create_info_player_deathmatch(190, -2834, 345, 90);
	}
	else if (Q_stricmp(zyk_mapname, "kor1") == 0)
	{
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			// GalaxyRP: [SP Maps] entities 418-422 in New Zyk mod: the five floor slabs s9..s5 of the
			// first puzzle, brush models *78..*82
			if (RP_IsBrushEntity(ent, "func_static", "*78") || RP_IsBrushEntity(ent, "func_static", "*79") ||
				RP_IsBrushEntity(ent, "func_static", "*80") || RP_IsBrushEntity(ent, "func_static", "*81") ||
				RP_IsBrushEntity(ent, "func_static", "*82"))
			{ // zyk: remove part of the door on the floor on the first puzzle
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "end_level") == 0)
			{ // zyk: remove the map change entity
				G_FreeEntity( ent );
			}
		}
		zyk_create_info_player_deathmatch(190,632,-1006,-89);
		zyk_create_info_player_deathmatch(-249,952,-934,-89);
	}
	else if (Q_stricmp(zyk_mapname, "kor2") == 0)
	{
		zyk_create_info_player_deathmatch(2977,3137,-2526,0);
		zyk_create_info_player_deathmatch(3072,2992,-2526,0);
	}
	else if (Q_stricmp(zyk_mapname, "mp/siege_korriban") == 0 && g_gametype.integer == GT_FFA)
	{ // zyk: if its a FFA game, then remove some entities
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp( ent->targetname, "cyrstalsinplace") == 0)
			{
				G_FreeEntity( ent );
			}
			// GalaxyRP: [SP Maps] entities 236-238 in New Zyk mod: the three lava trigger_hurts
			if (RP_IsBrushEntity(ent, "trigger_hurt", "*21") || RP_IsBrushEntity(ent, "trigger_hurt", "*22") ||
				RP_IsBrushEntity(ent, "trigger_hurt", "*23"))
			{ // zyk: removing the trigger_hurt from the lava in Guardian of Universe arena
				G_FreeEntity( ent );
			}
		}
	}
	else if (Q_stricmp(zyk_mapname, "mp/siege_desert") == 0 && g_gametype.integer == GT_FFA)
	{ // zyk: if its a FFA game, then remove the shield in the final part
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp( ent->targetname, "rebel_obj_2_doors") == 0)
			{
				fix_sp_func_door(ent);
			}
			if (Q_stricmp( ent->targetname, "shield") == 0)
			{
				G_FreeEntity( ent );
			}
			if (Q_stricmp( ent->targetname, "gatedestroy_doors") == 0)
			{
				G_FreeEntity( ent );
			}
			// GalaxyRP: [SP Maps] entities 153-160 in New Zyk mod: the droid-head doors and what drives
			// them -- the t408 door pair (*22, *23), the t410 door (*26), the two breakables (*24, *27)
			// and the trigger (*25) that start the chain, and its relays t409 and t407
			if (RP_IsBrushEntity(ent, "func_door", "*22") || RP_IsBrushEntity(ent, "func_door", "*23") ||
				RP_IsBrushEntity(ent, "func_breakable", "*24") || RP_IsBrushEntity(ent, "trigger_once", "*25") ||
				RP_IsBrushEntity(ent, "func_door", "*26") || RP_IsBrushEntity(ent, "func_breakable", "*27") ||
				(Q_stricmp(ent->classname, "target_relay") == 0 &&
				 (Q_stricmp(ent->targetname, "t409") == 0 || Q_stricmp(ent->targetname, "t407") == 0)))
			{
				G_FreeEntity( ent );
			}
			// GalaxyRP: [SP Maps] from New Zyk mod: open the right-hand breach in the city wall. The
			// siege attackers blow one of two holes in it, each three func_breakable pieces feeding a
			// counter (rightholecounter / leftholecounter) whose only target drives the siege radar
			// icons. Those pieces take heavy weapons only and 1000-3000 damage, so in FFA the players
			// who spawn outside the wall could not get in -- everything beyond it (the gate, the
			// objective doors, the shield) is already opened above. The left breach is left as it is.
			// New Zyk mod picks these by slot (358, 359, 362); matched by what they are here, which is
			// the same three entities without depending on the spawn order.
			if (Q_stricmp( ent->classname, "func_breakable") == 0 && Q_stricmp( ent->target, "rightholecounter") == 0)
			{
				G_FreeEntity( ent );
			}
		}
	}
	else if (Q_stricmp(zyk_mapname, "mp/siege_destroyer") == 0 && g_gametype.integer == GT_FFA)
	{ // zyk: if its a FFA game, then remove the shield at the destroyer
		gentity_t *ent;

		// GalaxyRP: [Logical Entities] both regions -- the entity looked for may be logical now.
		RP_FOR_EACH_ENTITY( ent )
		{
			if (Q_stricmp( ent->targetname, "ubershield") == 0)
			{
				G_FreeEntity( ent );
			}
			else if (Q_stricmp( ent->classname, "info_player_deathmatch") == 0)
			{
				G_FreeEntity( ent );
			}
		}
		// zyk: rebel area spawnpoints
		zyk_create_info_player_deathmatch(31729,-32219,33305,90);
		zyk_create_info_player_deathmatch(31229,-32219,33305,90);
		zyk_create_info_player_deathmatch(30729,-32219,33305,90);
		zyk_create_info_player_deathmatch(32229,-32219,33305,90);
		zyk_create_info_player_deathmatch(32729,-32219,33305,90);
		zyk_create_info_player_deathmatch(31729,-32019,33305,90);

		// zyk: imperial area spawnpoints
		zyk_create_info_player_deathmatch(2545,8987,1817,-90);
		zyk_create_info_player_deathmatch(2345,8987,1817,-90);
		zyk_create_info_player_deathmatch(2745,8987,1817,-90);

		zyk_create_info_player_deathmatch(2597,7403,1817,90);
		zyk_create_info_player_deathmatch(2397,7403,1817,90);
		zyk_create_info_player_deathmatch(2797,7403,1817,90);
	}

	// GalaxyRP: [SP Maps] the Jedi Outcast SP maps -- spawn points, and bespin_streets' doors and lifts
	RP_JediOutcastMapFixes( zyk_mapname );

	level.sp_map = qfalse;

	// GalaxyRP: [ICARUS] sound indices are per map, and so is the voice-line length cache keyed by them
	RP_IcarusSoundCacheReset();

	if (Q_stricmp(level.default_map_music, "") == 0)
	{ // zyk: if the default map music is empty (the map has no music) then set a default music
		// GalaxyRP fix: [Quests] this used to key off level.quest_map, the number the map blocks
		// above gave each quest map (case-sensitively, so a "Yavin1b" build would not count).
		// The quest engine is gone and quest_map with it; the four maps that had their own music
		// keep it, matched the same way.
		if (Q_strncmp(zyk_mapname, "yavin1b", 8) == 0)
			strcpy(level.default_map_music,"music/yavin1/swamp_explore.mp3");
		else if (Q_strncmp(zyk_mapname, "t2_rogue", 9) == 0)
			strcpy(level.default_map_music,"music/t2_rogue/narshaada_explore.mp3");
		else if (Q_strncmp(zyk_mapname, "yavin2", 7) == 0)
			strcpy(level.default_map_music,"music/yavin2/yavtemp2_explore.mp3");
		else if (Q_strncmp(zyk_mapname, "t1_fatal", 9) == 0)
			strcpy(level.default_map_music,"music/t1_fatal/tunnels_explore.mp3");
		else
			strcpy(level.default_map_music,"music/hoth2/hoth2_explore.mp3");
	}

	zyk_create_dir(va("entities/%s", zyk_mapname));

	// GalaxyRP fix: [Spawning] remember where the map's first spawn point is, for the selectors to
	// fall back on if an entity preset later leaves the map with none -- see g_client.c. Here, after
	// every per-map fix above has added its spawn points and removed the bad originals (the Jedi
	// Outcast table does both), and before the default entity file below clears anything.
	RP_RecordFallbackSpawnPoint();

	// GalaxyRP: [Entity System] and, at the same point for the same reason, which entities are the map's
	// own: /entcut and /entrotate leave those alone. See RP_MarkMapEntities() in g_entgrab.c.
	RP_MarkMapEntities();

	// DAJ_RP: [Static Models] and say if some of the map's misc_model_static did not fit the list (g_statics.c)
	RP_StaticsReport();

	// GalaxyRP fix: [Entity System] CS_ITEMS once more, now that the per-map fixes above have spawned
	// their items: it was written before them (SaveRegisteredItems() with the map's own entities), so an
	// item class only a fix placed was never precached by the client -- a hologram cone until a restart.
	// The same string when nothing changed; the same bytes in the gamestate either way.
	SaveRegisteredItems();

	// zyk: loading entities set as default (Entity System)
	zyk_entities_file = fopen(va("GalaxyRP/entities/%s/default.txt",zyk_mapname),"r");

	if (zyk_entities_file != NULL)
	{ // zyk: default file exists. Load entities from it
		fclose(zyk_entities_file);

		// zyk: cleaning entities. Only the ones from the file will be in the map. Do not remove CTF flags
		// GalaxyRP fix: [Entity System] the surviving twin of the test fixed in Cmd_EntLoad_f. "target_ent"
		// is the address of a fixed array element and so can never be NULL, so every slot in the range was
		// freed unconditionally -- including the ones the map's own spawn pass had already freed, because
		// the current gametype did not want them (notsingle / notteam / notfree / gametype).
		//
		// A second G_FreeEntity() on an already-free entity is not merely wasted work. G_FreeEntity() ends
		// with memset(ed, 0, sizeof(*ed)) and never puts s.number back -- only G_InitGentity() does, when
		// the slot is handed out again -- so the second call runs with s.number == 0 and therefore unlinks
		// client slot 0 from the world sectors and calls ICARUS_FreeEnt() on it. Here that happens to be
		// harmless: G_InitGame() runs before any client enters the world (map_restart re-enters them only
		// after the game module has initialised) and gSequencers[0] is NULL, so both are no-ops. It is the
		// same call at runtime, through /entload, that had to be fixed -- there client 0 is a real player.
		//
		// Test what was actually meant, and first, so the two classname compares only ever see a live
		// entity rather than the "freed" placeholder.
		// GalaxyRP: [Logical Entities] both regions, as in Cmd_EntLoad_f.
		{
			gentity_t *target_ent;

			RP_FOR_EACH_ENTITY( target_ent )
			{
				i = target_ent - g_entities;
				if (i < (MAX_CLIENTS + BODY_QUEUE_SIZE))
					continue;

				// GalaxyRP fix: [Entity System] and not the spawn points the per-map fixes above built in
				// code: no preset can hold them, so clearing them here lost them for good -- see
				// RP_EntityIsCodeMadeSpawnPoint() in g_spawn.c.
				if (target_ent->inuse && RP_EntityIsCodeMadeSpawnPoint(target_ent))
					continue;

				if (target_ent->inuse && Q_stricmp(target_ent->classname, "team_CTF_redflag") != 0 && Q_stricmp(target_ent->classname, "team_CTF_blueflag") != 0)
					G_FreeEntity( target_ent );
			}
		}

		strcpy(level.load_entities_file, va("GalaxyRP/entities/%s/default.txt",zyk_mapname));

		level.load_entities_timer = level.time + 1050;
	}

	// zyk: loading default remaps
	// GalaxyRP fix: [security] this was a second, unhardened copy of /remapload's read loop, and
	// the more dangerous of the two because it runs at map start with nobody typing anything. Its
	// conversions were bare "%s" into char[128] with no field width -- a straight overflow from any
	// token of 128 characters or more in remaps/<map>/default.txt -- and the two follow-up reads
	// were unchecked, so a file ending mid-record registered a remap built from whatever the
	// previous iteration had left in the buffers. The command's copy had been given both of those
	// fixes at some point and this one was missed, which is the argument for there being one copy:
	// zyk_load_remap_file() (g_utils.c). It also validates each record now and skips the unusable
	// ones instead of trusting the file.
	zyk_load_remap_file(va("GalaxyRP/remaps/%s/default.txt", zyk_mapname));

	// DAJ_RP: [Locks] this map's account locks -- see g_locks.c
	RP_LocksLoad();

	// zyk: loading duel arena, if this map has one
	zyk_duel_arena_file = fopen(va("GalaxyRP/duelarena/%s/origin.txt", zyk_mapname), "r");
	if (zyk_duel_arena_file != NULL)
	{
		char duel_arena_content[16];

		strcpy(duel_arena_content, "");

		fscanf(zyk_duel_arena_file, "%s", duel_arena_content);
		level.duel_tournament_origin[0] = atoi(duel_arena_content);

		fscanf(zyk_duel_arena_file, "%s", duel_arena_content);
		level.duel_tournament_origin[1] = atoi(duel_arena_content);

		fscanf(zyk_duel_arena_file, "%s", duel_arena_content);
		level.duel_tournament_origin[2] = atoi(duel_arena_content);

		fclose(zyk_duel_arena_file);

		level.duel_arena_loaded = qtrue;
	}

	// zyk: loading melee arena, if this map has one
	zyk_melee_arena_file = fopen(va("GalaxyRP/meleearena/%s/origin.txt", zyk_mapname), "r");
	if (zyk_melee_arena_file != NULL)
	{
		char melee_arena_content[16];

		strcpy(melee_arena_content, "");

		fscanf(zyk_melee_arena_file, "%s", melee_arena_content);
		level.melee_mode_origin[0] = atoi(melee_arena_content);

		fscanf(zyk_melee_arena_file, "%s", melee_arena_content);
		level.melee_mode_origin[1] = atoi(melee_arena_content);

		fscanf(zyk_melee_arena_file, "%s", melee_arena_content);
		level.melee_mode_origin[2] = atoi(melee_arena_content);

		fclose(zyk_melee_arena_file);

		level.melee_arena_loaded = qtrue;
	}

	//alex: create tables required for storing stuff, and also create admin account. ONLY if those do not already exist.
	InitializeGalaxyRpTables(qtrue);

	// GalaxyRP fix: [Configstrings] every configstring the map and the mod register has now been
	// written, so count the gamestate once and start the running estimate from the truth. Without
	// this the estimate would begin at zero and the budget check in G_FindConfigstringIndex would
	// not look at the real total until it believed it had added 14976 bytes by itself -- by which
	// point the gamestate would be long past the 16000 every client enforces.
	//
	// GalaxyRP fix: [Configstrings] this count is no longer the one that gets reported, because it
	// cannot see the whole gamestate yet -- SV_SpawnServer writes CS_SYSTEMINFO and CS_SERVERINFO
	// only after InitGame returns. G_RunFrame takes the count again once they are there and logs it
	// then. This one stays so level.zyk_gamestate_bytes is a real number in the meantime.
	G_ResetGamestateEstimate();
}

/*
=================
G_ShutdownGame
=================
*/
// GalaxyRP: [Account] forward-declared so G_ShutdownGame (below) can flush every still-connected
// player's currently active character before this level's game module is torn down -- same pattern
// used for the ClientDisconnect fix in g_client.c, whose comment goes into the full history.
extern void save_account(gentity_t *ent, qboolean save_char_file);
void G_ShutdownGame( int restart ) {
	int i = 0;
	gentity_t *ent;

//	trap->Print ("==== ShutdownGame ====\n");

	// GalaxyRP: [Account] G_ShutdownGame runs on every level change -- a server-console /map, this
	// mod's own /admmap, map rotation, a vote, or a full server shutdown -- and never called
	// save_account() for anyone. The engine's own G_WriteSessionData() call further down persists a
	// player's team/spectator state and *which* account/character they were using (via session
	// cvars), but none of their actual RPG progress (credits, skill points, XP, saber colours,
	// etc.) -- those live in ent->client->pers, which is wiped fresh for the new level. Worse,
	// ClientBegin() on the new level unconditionally reloads every RPG-mode player's account and
	// default character straight from the database to restore their loadout before they spawn -- so
	// without a save here, a level change didn't just risk losing unsaved progress, it *guaranteed*
	// overwriting it with whatever the database already had, for every RPG-mode player on the server
	// at once, on every single level change. Flush everyone here, first, before any of the cleanup
	// below runs (none of it touches pers/sess, but this keeps the same save-before-teardown
	// ordering used in ClientDisconnect). save_account() itself is a no-op for anyone not actually in
	// RPG mode (sess.amrpgmode != 2), so this is harmless for spectators, bots, and logged-out
	// players.
	for (i = 0; i < level.maxclients; i++) {
		ent = &g_entities[i];

		if (ent->client) {
			save_account(ent, qtrue);
		}
	}
	i = 0;

	G_CleanAllFakeClients(); //get rid of dynamically allocated fake client structs.

	BG_ClearAnimsets(); //free all dynamic allocations made through the engine

//	Com_Printf("... Gameside GHOUL2 Cleanup\n");
	while (i < MAX_GENTITIES)
	{ //clean up all the ghoul2 instances
		ent = &g_entities[i];

		if (ent->ghoul2 && trap->G2API_HaveWeGhoul2Models(ent->ghoul2))
		{
			trap->G2API_CleanGhoul2Models(&ent->ghoul2);
			ent->ghoul2 = NULL;
		}
		if (ent->client)
		{
			int j = 0;

			while (j < MAX_SABERS)
			{
				if (ent->client->weaponGhoul2[j] && trap->G2API_HaveWeGhoul2Models(ent->client->weaponGhoul2[j]))
				{
					trap->G2API_CleanGhoul2Models(&ent->client->weaponGhoul2[j]);
				}
				j++;
			}
		}
		i++;
	}
	if (g2SaberInstance && trap->G2API_HaveWeGhoul2Models(g2SaberInstance))
	{
		trap->G2API_CleanGhoul2Models(&g2SaberInstance);
		g2SaberInstance = NULL;
	}
	if (precachedKyle && trap->G2API_HaveWeGhoul2Models(precachedKyle))
	{
		trap->G2API_CleanGhoul2Models(&precachedKyle);
		precachedKyle = NULL;
	}

//	Com_Printf ("... ICARUS_Shutdown\n");
	trap->ICARUS_Shutdown ();	//Shut ICARUS down

//	Com_Printf ("... Reference Tags Cleared\n");
	TAG_Init();	//Clear the reference tags

	G_LogWeaponOutput();

	if ( level.logFile ) {
		G_LogPrintf( "ShutdownGame:\n------------------------------------------------------------\n" );
		trap->FS_Close( level.logFile );
		level.logFile = 0;
	}

	if ( level.security.log )
	{
		G_SecurityLogPrintf( "ShutdownGame\n\n" );
		trap->FS_Close( level.security.log );
		level.security.log = 0;
	}

	// write all the client session data so we can get it back
	G_WriteSessionData();

	trap->ROFF_Clean();

	if ( trap->Cvar_VariableIntegerValue( "bot_enable" ) ) {
		BotAIShutdown( restart );
	}

	B_CleanupAlloc(); //clean up all allocations made with B_Alloc
}

/*
========================================================================

PLAYER COUNTING / SCORE SORTING

========================================================================
*/

/*
=============
AddTournamentPlayer

If there are less than two tournament players, put a
spectator in the game and restart
=============
*/
void AddTournamentPlayer( void ) {
	int			i;
	gclient_t	*client;
	gclient_t	*nextInLine;

	if ( level.numPlayingClients >= 2 ) {
		return;
	}

	// never change during intermission
//	if ( level.intermissiontime ) {
//		return;
//	}

	nextInLine = NULL;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		client = &level.clients[i];
		if ( client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if (!g_allowHighPingDuelist.integer && client->ps.ping >= 999)
		{ //don't add people who are lagging out if cvar is not set to allow it.
			continue;
		}
		if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
			continue;
		}
		// never select the dedicated follow or scoreboard clients
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD ||
			client->sess.spectatorClient < 0  ) {
			continue;
		}
		// Tr!Force: [Plugin] Don't allow
		if (rp_pluginRequired.integer == 2 && !client->pers.clientPlugin) {
			continue;
		}

		if ( !nextInLine || client->sess.spectatorNum > nextInLine->sess.spectatorNum )
			nextInLine = client;
	}

	if ( !nextInLine ) {
		return;
	}

	level.warmupTime = -1;

	// set them to free-for-all team
	SetTeam( &g_entities[ nextInLine - level.clients ], "f" );
}

/*
=======================
AddTournamentQueue

Add client to end of tournament queue
=======================
*/

void AddTournamentQueue( gclient_t *client )
{
	int index;
	gclient_t *curclient;

	for( index = 0; index < level.maxclients; index++ )
	{
		curclient = &level.clients[index];

		if ( curclient->pers.connected != CON_DISCONNECTED )
		{
			if ( curclient == client )
				curclient->sess.spectatorNum = 0;
			else if ( curclient->sess.sessionTeam == TEAM_SPECTATOR )
				curclient->sess.spectatorNum++;
		}
	}
}

/*
=======================
RemoveTournamentLoser

Make the loser a spectator at the back of the line
=======================
*/
void RemoveTournamentLoser( void ) {
	int			clientNum;

	if ( level.numPlayingClients != 2 ) {
		return;
	}

	clientNum = level.sortedClients[1];

	if ( level.clients[ clientNum ].pers.connected != CON_CONNECTED ) {
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[ clientNum ], "s" );
}

void G_PowerDuelCount(int *loners, int *doubles, qboolean countSpec)
{
	int i = 0;
	gclient_t *cl;

	while (i < MAX_CLIENTS)
	{
		cl = g_entities[i].client;

		if (g_entities[i].inuse && cl && (countSpec || cl->sess.sessionTeam != TEAM_SPECTATOR))
		{
			if (cl->sess.duelTeam == DUELTEAM_LONE)
			{
				(*loners)++;
			}
			else if (cl->sess.duelTeam == DUELTEAM_DOUBLE)
			{
				(*doubles)++;
			}
		}
		i++;
	}
}

qboolean g_duelAssigning = qfalse;
void AddPowerDuelPlayers( void )
{
	int			i;
	int			loners = 0;
	int			doubles = 0;
	int			nonspecLoners = 0;
	int			nonspecDoubles = 0;
	gclient_t	*client;
	gclient_t	*nextInLine;

	if ( level.numPlayingClients >= 3 )
	{
		return;
	}

	nextInLine = NULL;

	G_PowerDuelCount(&nonspecLoners, &nonspecDoubles, qfalse);
	if (nonspecLoners >= 1 && nonspecDoubles >= 2)
	{ //we have enough people, stop
		return;
	}

	//Could be written faster, but it's not enough to care I suppose.
	G_PowerDuelCount(&loners, &doubles, qtrue);

	if (loners < 1 || doubles < 2)
	{ //don't bother trying to spawn anyone yet if the balance is not even set up between spectators
		return;
	}

	//Count again, with only in-game clients in mind.
	loners = nonspecLoners;
	doubles = nonspecDoubles;
//	G_PowerDuelCount(&loners, &doubles, qfalse);

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		client = &level.clients[i];
		if ( client->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
			continue;
		}
		if (client->sess.duelTeam == DUELTEAM_FREE)
		{
			continue;
		}
		if (client->sess.duelTeam == DUELTEAM_LONE && loners >= 1)
		{
			continue;
		}
		if (client->sess.duelTeam == DUELTEAM_DOUBLE && doubles >= 2)
		{
			continue;
		}

		// never select the dedicated follow or scoreboard clients
		if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD ||
			client->sess.spectatorClient < 0  ) {
			continue;
		}
		// Tr!Force: [Plugin] Don't allow
		if (rp_pluginRequired.integer == 2 && !client->pers.clientPlugin) {
			continue;
		}

		if ( !nextInLine || client->sess.spectatorNum > nextInLine->sess.spectatorNum )
			nextInLine = client;
	}

	if ( !nextInLine ) {
		return;
	}

	level.warmupTime = -1;

	// set them to free-for-all team
	SetTeam( &g_entities[ nextInLine - level.clients ], "f" );

	//Call recursively until everyone is in
	AddPowerDuelPlayers();
}

qboolean g_dontFrickinCheck = qfalse;

void RemovePowerDuelLosers(void)
{
	int remClients[3];
	int remNum = 0;
	int i = 0;
	gclient_t *cl;

	while (i < MAX_CLIENTS && remNum < 3)
	{
		//cl = &level.clients[level.sortedClients[i]];
		cl = &level.clients[i];

		if (cl->pers.connected == CON_CONNECTED)
		{
			if ((cl->ps.stats[STAT_HEALTH] <= 0 || cl->iAmALoser) &&
				(cl->sess.sessionTeam != TEAM_SPECTATOR || cl->iAmALoser))
			{ //he was dead or he was spectating as a loser
                remClients[remNum] = i;
				remNum++;
			}
		}

		i++;
	}

	if (!remNum)
	{ //Time ran out or something? Oh well, just remove the main guy.
		remClients[remNum] = level.sortedClients[0];
		remNum++;
	}

	i = 0;
	while (i < remNum)
	{ //set them all to spectator
		SetTeam( &g_entities[ remClients[i] ], "s" );
		i++;
	}

	g_dontFrickinCheck = qfalse;

	//recalculate stuff now that we have reset teams.
	CalculateRanks();
}

void RemoveDuelDrawLoser(void)
{
	int clFirst = 0;
	int clSec = 0;
	int clFailure = 0;

	if ( level.clients[ level.sortedClients[0] ].pers.connected != CON_CONNECTED )
	{
		return;
	}
	if ( level.clients[ level.sortedClients[1] ].pers.connected != CON_CONNECTED )
	{
		return;
	}

	clFirst = level.clients[ level.sortedClients[0] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[0] ].ps.stats[STAT_ARMOR];
	clSec = level.clients[ level.sortedClients[1] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[1] ].ps.stats[STAT_ARMOR];

	if (clFirst > clSec)
	{
		clFailure = 1;
	}
	else if (clSec > clFirst)
	{
		clFailure = 0;
	}
	else
	{
		clFailure = 2;
	}

	if (clFailure != 2)
	{
		SetTeam( &g_entities[ level.sortedClients[clFailure] ], "s" );
	}
	else
	{ //we could be more elegant about this, but oh well.
		SetTeam( &g_entities[ level.sortedClients[1] ], "s" );
	}
}

/*
=======================
RemoveTournamentWinner
=======================
*/
void RemoveTournamentWinner( void ) {
	int			clientNum;

	if ( level.numPlayingClients != 2 ) {
		return;
	}

	clientNum = level.sortedClients[0];

	if ( level.clients[ clientNum ].pers.connected != CON_CONNECTED ) {
		return;
	}

	// make them a spectator
	SetTeam( &g_entities[ clientNum ], "s" );
}

/*
=======================
AdjustTournamentScores
=======================
*/
void AdjustTournamentScores( void ) {
	int			clientNum;

	if (level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE] ==
		level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE] &&
		level.clients[level.sortedClients[0]].pers.connected == CON_CONNECTED &&
		level.clients[level.sortedClients[1]].pers.connected == CON_CONNECTED)
	{
		int clFirst = level.clients[ level.sortedClients[0] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[0] ].ps.stats[STAT_ARMOR];
		int clSec = level.clients[ level.sortedClients[1] ].ps.stats[STAT_HEALTH] + level.clients[ level.sortedClients[1] ].ps.stats[STAT_ARMOR];
		int clFailure = 0;
		int clSuccess = 0;

		if (clFirst > clSec)
		{
			clFailure = 1;
			clSuccess = 0;
		}
		else if (clSec > clFirst)
		{
			clFailure = 0;
			clSuccess = 1;
		}
		else
		{
			clFailure = 2;
			clSuccess = 2;
		}

		if (clFailure != 2)
		{
			clientNum = level.sortedClients[clSuccess];

			level.clients[ clientNum ].sess.wins++;
			ClientUserinfoChanged( clientNum );
			trap->SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", clientNum ) );

			clientNum = level.sortedClients[clFailure];

			level.clients[ clientNum ].sess.losses++;
			ClientUserinfoChanged( clientNum );
		}
		else
		{
			clSuccess = 0;
			clFailure = 1;

			clientNum = level.sortedClients[clSuccess];

			level.clients[ clientNum ].sess.wins++;
			ClientUserinfoChanged( clientNum );
			trap->SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", clientNum ) );

			clientNum = level.sortedClients[clFailure];

			level.clients[ clientNum ].sess.losses++;
			ClientUserinfoChanged( clientNum );
		}
	}
	else
	{
		clientNum = level.sortedClients[0];
		if ( level.clients[ clientNum ].pers.connected == CON_CONNECTED ) {
			level.clients[ clientNum ].sess.wins++;
			ClientUserinfoChanged( clientNum );

			trap->SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", clientNum ) );
		}

		clientNum = level.sortedClients[1];
		if ( level.clients[ clientNum ].pers.connected == CON_CONNECTED ) {
			level.clients[ clientNum ].sess.losses++;
			ClientUserinfoChanged( clientNum );
		}
	}
}

/*
=============
SortRanks

=============
*/
int QDECL SortRanks( const void *a, const void *b ) {
	gclient_t	*ca, *cb;

	ca = &level.clients[*(int *)a];
	cb = &level.clients[*(int *)b];

	if (level.gametype == GT_POWERDUEL)
	{
		//sort single duelists first
		if (ca->sess.duelTeam == DUELTEAM_LONE && ca->sess.sessionTeam != TEAM_SPECTATOR)
		{
			return -1;
		}
		if (cb->sess.duelTeam == DUELTEAM_LONE && cb->sess.sessionTeam != TEAM_SPECTATOR)
		{
			return 1;
		}

		//others will be auto-sorted below but above spectators.
	}

	// sort special clients last
	if ( ca->sess.spectatorState == SPECTATOR_SCOREBOARD || ca->sess.spectatorClient < 0 ) {
		return 1;
	}
	if ( cb->sess.spectatorState == SPECTATOR_SCOREBOARD || cb->sess.spectatorClient < 0  ) {
		return -1;
	}

	// then connecting clients
	if ( ca->pers.connected == CON_CONNECTING ) {
		return 1;
	}
	if ( cb->pers.connected == CON_CONNECTING ) {
		return -1;
	}


	// then spectators
	if ( ca->sess.sessionTeam == TEAM_SPECTATOR && cb->sess.sessionTeam == TEAM_SPECTATOR ) {
		if ( ca->sess.spectatorNum > cb->sess.spectatorNum ) {
			return -1;
		}
		if ( ca->sess.spectatorNum < cb->sess.spectatorNum ) {
			return 1;
		}
		return 0;
	}
	if ( ca->sess.sessionTeam == TEAM_SPECTATOR ) {
		return 1;
	}
	if ( cb->sess.sessionTeam == TEAM_SPECTATOR ) {
		return -1;
	}

	// then sort by score
	if ( ca->ps.persistant[PERS_SCORE]
		> cb->ps.persistant[PERS_SCORE] ) {
		return -1;
	}
	if ( ca->ps.persistant[PERS_SCORE]
		< cb->ps.persistant[PERS_SCORE] ) {
		return 1;
	}
	return 0;
}

qboolean gQueueScoreMessage = qfalse;
int gQueueScoreMessageTime = 0;

//A new duel started so respawn everyone and make sure their stats are reset
qboolean G_CanResetDuelists(void)
{
	int i;
	gentity_t *ent;

	i = 0;
	while (i < 3)
	{ //precheck to make sure they are all respawnable
		ent = &g_entities[level.sortedClients[i]];

		if (!ent->inuse || !ent->client || ent->health <= 0 ||
			ent->client->sess.sessionTeam == TEAM_SPECTATOR ||
			ent->client->sess.duelTeam <= DUELTEAM_FREE)
		{
			return qfalse;
		}
		i++;
	}

	return qtrue;
}

qboolean g_noPDuelCheck = qfalse;
void G_ResetDuelists(void)
{
	int i;
	gentity_t *ent = NULL;

	i = 0;
	while (i < 3)
	{
		ent = &g_entities[level.sortedClients[i]];

		// GalaxyRP fix: [Death System] bookkeeping, not a death -- see g_bookkeepingDeath in
		// g_local.h. Resetting the duelists is the match machinery clearing the arena, not
		// something any of them did.
		g_noPDuelCheck = qtrue;
		g_bookkeepingDeath = qtrue;
		player_die(ent, ent, ent, 999, MOD_SUICIDE);
		g_bookkeepingDeath = qfalse;
		g_noPDuelCheck = qfalse;
		trap->UnlinkEntity ((sharedEntity_t *)ent);
		ClientSpawn(ent);
		i++;
	}
}

/*
============
CalculateRanks

Recalculates the score ranks of all players
This will be called on every client connect, begin, disconnect, death,
and team change.
============
*/
void CalculateRanks( void ) {
	int		i;
	int		rank;
	int		score;
	int		newScore;
//	int		preNumSpec = 0;
	//int		nonSpecIndex = -1;
	gclient_t	*cl;

//	preNumSpec = level.numNonSpectatorClients;

	level.follow1 = -1;
	level.follow2 = -1;
	level.numConnectedClients = 0;
	level.num_fully_connected_clients = 0;
	level.numNonSpectatorClients = 0;
	level.numPlayingClients = 0;
	level.numVotingClients = 0;		// don't count bots

	for ( i = 0; i < ARRAY_LEN(level.numteamVotingClients); i++ ) {
		level.numteamVotingClients[i] = 0;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected != CON_DISCONNECTED ) {
			level.sortedClients[level.numConnectedClients] = i;
			level.numConnectedClients++;

			if (level.clients[i].pers.connected == CON_CONNECTED)
			{
				level.num_fully_connected_clients++;
			}

			if ( level.clients[i].sess.sessionTeam != TEAM_SPECTATOR || level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL )
			{
				if (level.clients[i].sess.sessionTeam != TEAM_SPECTATOR)
				{
					level.numNonSpectatorClients++;
					//nonSpecIndex = i;
				}

				// decide if this should be auto-followed
				if ( level.clients[i].pers.connected == CON_CONNECTED )
				{
					if (level.clients[i].sess.sessionTeam != TEAM_SPECTATOR || level.clients[i].iAmALoser)
					{
						level.numPlayingClients++;
					}
					if ( !(g_entities[i].r.svFlags & SVF_BOT) )
					{
						level.numVotingClients++;
						if ( level.clients[i].sess.sessionTeam == TEAM_RED )
							level.numteamVotingClients[0]++;
						else if ( level.clients[i].sess.sessionTeam == TEAM_BLUE )
							level.numteamVotingClients[1]++;
					}
					if ( level.follow1 == -1 ) {
						level.follow1 = i;
					} else if ( level.follow2 == -1 ) {
						level.follow2 = i;
					}
				}
			}
		}
	}

	if ( !g_warmup.integer || level.gametype == GT_SIEGE )
		level.warmupTime = 0;

	/*
	if (level.numNonSpectatorClients == 2 && preNumSpec < 2 && nonSpecIndex != -1 && level.gametype == GT_DUEL && !level.warmupTime)
	{
		gentity_t *currentWinner = G_GetDuelWinner(&level.clients[nonSpecIndex]);

		if (currentWinner && currentWinner->client)
		{
			trap->SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s %s\n\"",
			currentWinner->client->pers.netname, G_GetStringEdString("MP_SVGAME", "VERSUS"), level.clients[nonSpecIndex].pers.netname));
		}
	}
	*/
	//NOTE: for now not doing this either. May use later if appropriate.

	qsort( level.sortedClients, level.numConnectedClients,
		sizeof(level.sortedClients[0]), SortRanks );

	// set the rank value for all clients that are connected and not spectators
	if ( level.gametype >= GT_TEAM ) {
		// in team games, rank is just the order of the teams, 0=red, 1=blue, 2=tied
		for ( i = 0;  i < level.numConnectedClients; i++ ) {
			cl = &level.clients[ level.sortedClients[i] ];
			if ( level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE] ) {
				cl->ps.persistant[PERS_RANK] = 2;
			} else if ( level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE] ) {
				cl->ps.persistant[PERS_RANK] = 0;
			} else {
				cl->ps.persistant[PERS_RANK] = 1;
			}
		}
	} else {
		rank = -1;
		score = 0;
		for ( i = 0;  i < level.numPlayingClients; i++ ) {
			cl = &level.clients[ level.sortedClients[i] ];
			newScore = cl->ps.persistant[PERS_SCORE];
			if ( i == 0 || newScore != score ) {
				rank = i;
				// assume we aren't tied until the next client is checked
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank;
			} else if(i != 0 ){
				// we are tied with the previous client
				level.clients[ level.sortedClients[i-1] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
			}
			score = newScore;
			if ( level.gametype == GT_SINGLE_PLAYER && level.numPlayingClients == 1 ) {
				level.clients[ level.sortedClients[i] ].ps.persistant[PERS_RANK] = rank | RANK_TIED_FLAG;
			}
		}
	}

	// set the CS_SCORES1/2 configstrings, which will be visible to everyone
	if ( level.gametype >= GT_TEAM ) {
		trap->SetConfigstring( CS_SCORES1, va("%i", level.teamScores[TEAM_RED] ) );
		trap->SetConfigstring( CS_SCORES2, va("%i", level.teamScores[TEAM_BLUE] ) );
	} else {
		if ( level.numConnectedClients == 0 ) {
			trap->SetConfigstring( CS_SCORES1, va("%i", SCORE_NOT_PRESENT) );
			trap->SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
		} else if ( level.numConnectedClients == 1 ) {
			trap->SetConfigstring( CS_SCORES1, va("%i", level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] ) );
			trap->SetConfigstring( CS_SCORES2, va("%i", SCORE_NOT_PRESENT) );
		} else {
			trap->SetConfigstring( CS_SCORES1, va("%i", level.clients[ level.sortedClients[0] ].ps.persistant[PERS_SCORE] ) );
			trap->SetConfigstring( CS_SCORES2, va("%i", level.clients[ level.sortedClients[1] ].ps.persistant[PERS_SCORE] ) );
		}

		if (level.gametype != GT_DUEL && level.gametype != GT_POWERDUEL)
		{ //when not in duel, use this configstring to pass the index of the player currently in first place
			if ( level.numConnectedClients >= 1 )
			{
				trap->SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", level.sortedClients[0] ) );
			}
			else
			{
				trap->SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
			}
		}
	}

	// see if it is time to end the level
	CheckExitRules();

	// if we are at the intermission or in multi-frag Duel game mode, send the new info to everyone
	if ( level.intermissiontime || level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL ) {
		gQueueScoreMessage = qtrue;
		gQueueScoreMessageTime = level.time + 500;
		//SendScoreboardMessageToAllClients();
		//rww - Made this operate on a "queue" system because it was causing large overflows
	}
}


/*
========================================================================

MAP CHANGING

========================================================================
*/

/*
========================
SendScoreboardMessageToAllClients

Do this at BeginIntermission time and whenever ranks are recalculated
due to enters/exits/forced team changes
========================
*/
void SendScoreboardMessageToAllClients( void ) {
	int		i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[ i ].pers.connected == CON_CONNECTED ) {
			DeathmatchScoreboardMessage( g_entities + i );
		}
	}
}

/*
========================
MoveClientToIntermission

When the intermission starts, this will be called for all players.
If a new client connects, this will be called after the spawn function.
========================
*/
extern void G_LeaveVehicle( gentity_t *ent, qboolean ConCheck );
void MoveClientToIntermission( gentity_t *ent ) {
	// take out of follow mode if needed
	if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
		StopFollowing( ent );
	}

	FindIntermissionPoint();
	// move to the spot
	VectorCopy( level.intermission_origin, ent->s.origin );
	VectorCopy( level.intermission_origin, ent->client->ps.origin );
	VectorCopy (level.intermission_angle, ent->client->ps.viewangles);
	ent->client->ps.pm_type = PM_INTERMISSION;

	// clean up powerup info
	memset( ent->client->ps.powerups, 0, sizeof(ent->client->ps.powerups) );

	G_LeaveVehicle( ent, qfalse );

	ent->client->ps.rocketLockIndex = ENTITYNUM_NONE;
	ent->client->ps.rocketLockTime = 0;

	ent->client->ps.eFlags = 0;
	ent->s.eFlags = 0;
	ent->client->ps.eFlags2 = 0;
	ent->s.eFlags2 = 0;
	ent->s.eType = ET_GENERAL;
	ent->s.modelindex = 0;
	ent->s.loopSound = 0;
	ent->s.loopIsSoundset = qfalse;
	ent->s.event = 0;
	ent->r.contents = 0;
}

/*
==================
FindIntermissionPoint

This is also used for spectator spawns
==================
*/
extern qboolean	gSiegeRoundBegun;
extern qboolean	gSiegeRoundEnded;
extern int	gSiegeRoundWinningTeam;
void FindIntermissionPoint( void ) {
	gentity_t	*ent = NULL;
	gentity_t	*target;
	vec3_t		dir;

	// find the intermission spot
	if ( level.gametype == GT_SIEGE
		&& level.intermissiontime
		&& level.intermissiontime <= level.time
		&& gSiegeRoundEnded )
	{
	   	if (gSiegeRoundWinningTeam == SIEGETEAM_TEAM1)
		{
			ent = G_Find (NULL, FOFS(classname), "info_player_intermission_red");
			if ( ent && ent->target2 )
			{
				G_UseTargets2( ent, ent, ent->target2 );
			}
		}
	   	else if (gSiegeRoundWinningTeam == SIEGETEAM_TEAM2)
		{
			ent = G_Find (NULL, FOFS(classname), "info_player_intermission_blue");
			if ( ent && ent->target2 )
			{
				G_UseTargets2( ent, ent, ent->target2 );
			}
		}
	}
	if ( !ent )
	{
		ent = G_Find (NULL, FOFS(classname), "info_player_intermission");
	}
	if ( !ent ) {	// the map creator forgot to put in an intermission point...
		SelectSpawnPoint ( vec3_origin, level.intermission_origin, level.intermission_angle, TEAM_SPECTATOR, qfalse );
	} else {
		VectorCopy (ent->s.origin, level.intermission_origin);
		VectorCopy (ent->s.angles, level.intermission_angle);
		// if it has a target, look towards it
		if ( ent->target ) {
			target = G_PickTarget( ent->target );
			if ( target ) {
				VectorSubtract( target->s.origin, level.intermission_origin, dir );
				vectoangles( dir, level.intermission_angle );
			}
		}
	}
}

qboolean DuelLimitHit(void);

/*
==================
BeginIntermission
==================
*/
void BeginIntermission( void ) {
	int			i;
	gentity_t	*client;

	if ( level.intermissiontime ) {
		return;		// already active
	}

	// if in tournament mode, change the wins / losses
	if ( level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL ) {
		trap->SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );

		if (level.gametype != GT_POWERDUEL)
		{
			AdjustTournamentScores();
		}
		if (DuelLimitHit())
		{
			gDuelExit = qtrue;
		}
		else
		{
			gDuelExit = qfalse;
		}
	}

	level.intermissiontime = level.time;

	// move all clients to the intermission point
	for (i=0 ; i< level.maxclients ; i++) {
		client = g_entities + i;
		if (!client->inuse)
			continue;
		// respawn if dead
		if (client->health <= 0) {
			if (level.gametype != GT_POWERDUEL ||
				!client->client ||
				client->client->sess.sessionTeam != TEAM_SPECTATOR)
			{ //don't respawn spectators in powerduel or it will mess the line order all up
				ClientRespawn(client);
			}
		}
		MoveClientToIntermission( client );
	}

	// send the current scoring to all clients
	SendScoreboardMessageToAllClients();
}

qboolean DuelLimitHit(void)
{
	int i;
	gclient_t *cl;

	for ( i=0 ; i< sv_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}

		if ( duel_fraglimit.integer && cl->sess.wins >= duel_fraglimit.integer )
		{
			return qtrue;
		}
	}

	return qfalse;
}

void DuelResetWinsLosses(void)
{
	int i;
	gclient_t *cl;

	for ( i=0 ; i< sv_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}

		cl->sess.wins = 0;
		cl->sess.losses = 0;
	}
}

/*
=============
ExitLevel

When the intermission has been exited, the server is either killed
or moved to a new level based on the "nextmap" cvar

=============
*/
extern void SiegeDoTeamAssign(void); //g_saga.c
extern siegePers_t g_siegePersistant; //g_saga.c
void ExitLevel (void) {
	int		i;
	gclient_t *cl;

	// if we are running a tournament map, kick the loser to spectator status,
	// which will automatically grab the next spectator and restart
	if ( level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL ) {
		if (!DuelLimitHit())
		{
			if ( !level.restarted ) {
				trap->SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
				level.restarted = qtrue;
				level.changemap = NULL;
				level.intermissiontime = 0;
			}
			return;
		}

		DuelResetWinsLosses();
	}


	if (level.gametype == GT_SIEGE &&
		g_siegeTeamSwitch.integer &&
		g_siegePersistant.beatingTime)
	{ //restart same map...
		trap->SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
	}
	else
	{
		trap->SendConsoleCommand( EXEC_APPEND, "vstr nextmap\n" );
	}
	level.changemap = NULL;
	level.intermissiontime = 0;

	if (level.gametype == GT_SIEGE &&
		g_siegeTeamSwitch.integer)
	{ //switch out now
		SiegeDoTeamAssign();
	}

	// reset all the scores so we don't enter the intermission again
	level.teamScores[TEAM_RED] = 0;
	level.teamScores[TEAM_BLUE] = 0;
	for ( i=0 ; i< sv_maxclients.integer ; i++ ) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		cl->ps.persistant[PERS_SCORE] = 0;
	}

	// we need to do this here before chaning to CON_CONNECTING
	G_WriteSessionData();

	// change all client states to connecting, so the early players into the
	// next level will know the others aren't done reconnecting
	for (i=0 ; i< sv_maxclients.integer ; i++) {
		if ( level.clients[i].pers.connected == CON_CONNECTED ) {
			level.clients[i].pers.connected = CON_CONNECTING;
		}
	}

}

/*
=================
G_LogPrintf

Print to the logfile with a time stamp if it is open
=================
*/
void QDECL G_LogPrintf( const char *fmt, ... ) {
	va_list		argptr;
	char		string[1024] = {0};
	int			mins, seconds, msec, l;

	msec = level.time - level.startTime;

	seconds = msec / 1000;
	mins = seconds / 60;
	seconds %= 60;
//	msec %= 1000;

	Com_sprintf( string, sizeof( string ), "%i:%02i ", mins, seconds );

	l = strlen( string );

	va_start( argptr, fmt );
	Q_vsnprintf( string + l, sizeof( string ) - l, fmt, argptr );
	va_end( argptr );

	if ( dedicated.integer )
		trap->Print( "%s", string + l );

	if ( !level.logFile )
		return;

	trap->FS_Write( string, strlen( string ), level.logFile );
}
/*
=================
G_SecurityLogPrintf

Print to the security logfile with a time stamp if it is open
=================
*/
void QDECL G_SecurityLogPrintf( const char *fmt, ... ) {
	va_list		argptr;
	char		string[1024] = {0};
	time_t		rawtime;
	int			timeLen=0;

	time( &rawtime );
	localtime( &rawtime );
	strftime( string, sizeof( string ), "[%Y-%m-%d] [%H:%M:%S] ", gmtime( &rawtime ) );
	timeLen = strlen( string );

	va_start( argptr, fmt );
	Q_vsnprintf( string+timeLen, sizeof( string ) - timeLen, fmt, argptr );
	va_end( argptr );

	if ( dedicated.integer )
		trap->Print( "%s", string + timeLen );

	if ( !level.security.log )
		return;

	trap->FS_Write( string, strlen( string ), level.security.log );
}

/*
================
LogExit

Append information about this game to the log file
================
*/
void LogExit( const char *string ) {
	int				i, numSorted;
	gclient_t		*cl;
//	qboolean		won = qtrue;
	G_LogPrintf( "Exit: %s\n", string );

	level.intermissionQueued = level.time;

	// this will keep the clients from playing any voice sounds
	// that will get cut off when the queued intermission starts
	trap->SetConfigstring( CS_INTERMISSION, "1" );

	// don't send more than 32 scores (FIXME?)
	numSorted = level.numConnectedClients;
	if ( numSorted > 32 ) {
		numSorted = 32;
	}

	if ( level.gametype >= GT_TEAM ) {
		G_LogPrintf( "red:%i  blue:%i\n",
			level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE] );
	}

	for (i=0 ; i < numSorted ; i++) {
		int		ping;

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		if ( cl->pers.connected == CON_CONNECTING ) {
			continue;
		}

		ping = cl->ps.ping < 999 ? cl->ps.ping : 999;

		if (level.gametype >= GT_TEAM) {
			G_LogPrintf( "(%s) score: %i  ping: %i  client: [%s] %i \"%s^7\"\n", TeamName(cl->ps.persistant[PERS_TEAM]), cl->ps.persistant[PERS_SCORE], ping, cl->pers.guid, level.sortedClients[i], cl->pers.netname );
		} else {
			G_LogPrintf( "score: %i  ping: %i  client: [%s] %i \"%s^7\"\n", cl->ps.persistant[PERS_SCORE], ping, cl->pers.guid, level.sortedClients[i], cl->pers.netname );
		}
//		if (g_singlePlayer.integer && (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)) {
//			if (g_entities[cl - level.clients].r.svFlags & SVF_BOT && cl->ps.persistant[PERS_RANK] == 0) {
//				won = qfalse;
//			}
//		}
	}

	//yeah.. how about not.
	/*
	if (g_singlePlayer.integer) {
		if (level.gametype >= GT_CTF) {
			won = level.teamScores[TEAM_RED] > level.teamScores[TEAM_BLUE];
		}
		trap->SendConsoleCommand( EXEC_APPEND, (won) ? "spWin\n" : "spLose\n" );
	}
	*/
}

qboolean gDidDuelStuff = qfalse; //gets reset on game reinit

/*
=================
CheckIntermissionExit

The level will stay at the intermission for a minimum of 5 seconds
If all players wish to continue, the level will then exit.
If one or more players have not acknowledged the continue, the game will
wait 10 seconds before going on.
=================
*/
void CheckIntermissionExit( void ) {
	int			ready, notReady;
	int			i;
	gclient_t	*cl;
	int			readyMask;

	// see which players are ready
	ready = 0;
	notReady = 0;
	readyMask = 0;
	for (i=0 ; i< sv_maxclients.integer ; i++) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		if ( g_entities[i].r.svFlags & SVF_BOT ) {
			continue;
		}

		if ( cl->readyToExit ) {
			ready++;
			if ( i < 16 ) {
				readyMask |= 1 << i;
			}
		} else {
			notReady++;
		}
	}

	if ( (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL) && !gDidDuelStuff &&
		(level.time > level.intermissiontime + 2000) )
	{
		gDidDuelStuff = qtrue;

		if ( g_austrian.integer && level.gametype != GT_POWERDUEL )
		{
			G_LogPrintf("Duel Results:\n");
			//G_LogPrintf("Duel Time: %d\n", level.time );
			G_LogPrintf("winner: %s, score: %d, wins/losses: %d/%d\n",
				level.clients[level.sortedClients[0]].pers.netname,
				level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE],
				level.clients[level.sortedClients[0]].sess.wins,
				level.clients[level.sortedClients[0]].sess.losses );
			G_LogPrintf("loser: %s, score: %d, wins/losses: %d/%d\n",
				level.clients[level.sortedClients[1]].pers.netname,
				level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE],
				level.clients[level.sortedClients[1]].sess.wins,
				level.clients[level.sortedClients[1]].sess.losses );
		}
		// if we are running a tournament map, kick the loser to spectator status,
		// which will automatically grab the next spectator and restart
		if (!DuelLimitHit())
		{
			if (level.gametype == GT_POWERDUEL)
			{
				RemovePowerDuelLosers();
				AddPowerDuelPlayers();
			}
			else
			{
				if (level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE] ==
					level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE] &&
					level.clients[level.sortedClients[0]].pers.connected == CON_CONNECTED &&
					level.clients[level.sortedClients[1]].pers.connected == CON_CONNECTED)
				{
					RemoveDuelDrawLoser();
				}
				else
				{
					RemoveTournamentLoser();
				}
				AddTournamentPlayer();
			}

			if ( g_austrian.integer )
			{
				if (level.gametype == GT_POWERDUEL)
				{
					G_LogPrintf("Power Duel Initiated: %s %d/%d vs %s %d/%d and %s %d/%d, kill limit: %d\n",
						level.clients[level.sortedClients[0]].pers.netname,
						level.clients[level.sortedClients[0]].sess.wins,
						level.clients[level.sortedClients[0]].sess.losses,
						level.clients[level.sortedClients[1]].pers.netname,
						level.clients[level.sortedClients[1]].sess.wins,
						level.clients[level.sortedClients[1]].sess.losses,
						level.clients[level.sortedClients[2]].pers.netname,
						level.clients[level.sortedClients[2]].sess.wins,
						level.clients[level.sortedClients[2]].sess.losses,
						fraglimit.integer );
				}
				else
				{
					G_LogPrintf("Duel Initiated: %s %d/%d vs %s %d/%d, kill limit: %d\n",
						level.clients[level.sortedClients[0]].pers.netname,
						level.clients[level.sortedClients[0]].sess.wins,
						level.clients[level.sortedClients[0]].sess.losses,
						level.clients[level.sortedClients[1]].pers.netname,
						level.clients[level.sortedClients[1]].sess.wins,
						level.clients[level.sortedClients[1]].sess.losses,
						fraglimit.integer );
				}
			}

			if (level.gametype == GT_POWERDUEL)
			{
				if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
				{
					trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
					trap->SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
				}
			}
			else
			{
				if (level.numPlayingClients >= 2)
				{
					trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
					trap->SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
				}
			}

			return;
		}

		if ( g_austrian.integer && level.gametype != GT_POWERDUEL )
		{
			G_LogPrintf("Duel Tournament Winner: %s wins/losses: %d/%d\n",
				level.clients[level.sortedClients[0]].pers.netname,
				level.clients[level.sortedClients[0]].sess.wins,
				level.clients[level.sortedClients[0]].sess.losses );
		}

		if (level.gametype == GT_POWERDUEL)
		{
			RemovePowerDuelLosers();
			AddPowerDuelPlayers();

			if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
			{
				trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
				trap->SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
			}
		}
		else
		{
			//this means we hit the duel limit so reset the wins/losses
			//but still push the loser to the back of the line, and retain the order for
			//the map change
			if (level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE] ==
				level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE] &&
				level.clients[level.sortedClients[0]].pers.connected == CON_CONNECTED &&
				level.clients[level.sortedClients[1]].pers.connected == CON_CONNECTED)
			{
				RemoveDuelDrawLoser();
			}
			else
			{
				RemoveTournamentLoser();
			}

			AddTournamentPlayer();

			if (level.numPlayingClients >= 2)
			{
				trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
				trap->SetConfigstring ( CS_CLIENT_DUELWINNER, "-1" );
			}
		}
	}

	if ((level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL) && !gDuelExit)
	{ //in duel, we have different behaviour for between-round intermissions
		if ( level.time > level.intermissiontime + 4000 )
		{ //automatically go to next after 4 seconds
			ExitLevel();
			return;
		}

		for (i=0 ; i< sv_maxclients.integer ; i++)
		{ //being in a "ready" state is not necessary here, so clear it for everyone
		  //yes, I also thinking holding this in a ps value uniquely for each player
		  //is bad and wrong, but it wasn't my idea.
			cl = level.clients + i;
			if ( cl->pers.connected != CON_CONNECTED )
			{
				continue;
			}
			cl->ps.stats[STAT_CLIENTS_READY] = 0;
		}
		return;
	}

	// copy the readyMask to each player's stats so
	// it can be displayed on the scoreboard
	for (i=0 ; i< sv_maxclients.integer ; i++) {
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED ) {
			continue;
		}
		cl->ps.stats[STAT_CLIENTS_READY] = readyMask;
	}

	// never exit in less than five seconds
	if ( level.time < level.intermissiontime + 5000 ) {
		return;
	}

	if (d_noIntermissionWait.integer)
	{ //don't care who wants to go, just go.
		ExitLevel();
		return;
	}

	// if nobody wants to go, clear timer
	if ( !ready ) {
		level.readyToExit = qfalse;
		return;
	}

	// if everyone wants to go, go now
	if ( !notReady ) {
		ExitLevel();
		return;
	}

	// the first person to ready starts the ten second timeout
	if ( !level.readyToExit ) {
		level.readyToExit = qtrue;
		level.exitTime = level.time;
	}

	// if we have waited ten seconds since at least one player
	// wanted to exit, go ahead
	if ( level.time < level.exitTime + 10000 ) {
		return;
	}

	ExitLevel();
}

/*
=============
ScoreIsTied
=============
*/
qboolean ScoreIsTied( void ) {
	int		a, b;

	if ( level.numPlayingClients < 2 ) {
		return qfalse;
	}

	if ( level.gametype >= GT_TEAM ) {
		return level.teamScores[TEAM_RED] == level.teamScores[TEAM_BLUE];
	}

	a = level.clients[level.sortedClients[0]].ps.persistant[PERS_SCORE];
	b = level.clients[level.sortedClients[1]].ps.persistant[PERS_SCORE];

	return a == b;
}

/*
=================
CheckExitRules

There will be a delay between the time the exit is qualified for
and the time everyone is moved to the intermission spot, so you
can see the last frag.
=================
*/
qboolean g_endPDuel = qfalse;
void CheckExitRules( void ) {
 	int			i;
	gclient_t	*cl;
	char *sKillLimit;
	qboolean printLimit = qtrue;
	// if at the intermission, wait for all non-bots to
	// signal ready, then go to next level
	if ( level.intermissiontime ) {
		CheckIntermissionExit ();
		return;
	}

	if (gDoSlowMoDuel)
	{ //don't go to intermission while in slow motion
		return;
	}

	if (gEscaping)
	{
		int numLiveClients = 0;

		for ( i=0; i < MAX_CLIENTS; i++ )
		{
			if (g_entities[i].inuse && g_entities[i].client && g_entities[i].health > 0)
			{
				if (g_entities[i].client->sess.sessionTeam != TEAM_SPECTATOR &&
					!(g_entities[i].client->ps.pm_flags & PMF_FOLLOW))
				{
					numLiveClients++;
				}
			}
		}
		if (gEscapeTime < level.time)
		{
			gEscaping = qfalse;
			LogExit( "Escape time ended." );
			return;
		}
		if (!numLiveClients)
		{
			gEscaping = qfalse;
			LogExit( "Everyone failed to escape." );
			return;
		}
	}

	if ( level.intermissionQueued ) {
		//int time = (g_singlePlayer.integer) ? SP_INTERMISSION_DELAY_TIME : INTERMISSION_DELAY_TIME;
		int time = INTERMISSION_DELAY_TIME;
		if ( level.time - level.intermissionQueued >= time ) {
			level.intermissionQueued = 0;
			BeginIntermission();
		}
		return;
	}

	/*
	if (level.gametype == GT_POWERDUEL)
	{
		if (level.numPlayingClients < 3)
		{
			if (!level.intermissiontime)
			{
				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Duel forfeit (1)\n");
				}
				LogExit("Duel forfeit.");
				return;
			}
		}
	}
	*/

	// check for sudden death
	if (level.gametype != GT_SIEGE)
	{
		if ( ScoreIsTied() ) {
			// always wait for sudden death
			if ((level.gametype != GT_DUEL) || !timelimit.value)
			{
				if (level.gametype != GT_POWERDUEL)
				{
					return;
				}
			}
		}
	}

	if (level.gametype != GT_SIEGE)
	{
		if ( timelimit.value > 0.0f && !level.warmupTime ) {
			if ( level.time - level.startTime >= timelimit.value*60000 ) {
//				trap->SendServerCommand( -1, "print \"Timelimit hit.\n\"");
				trap->SendServerCommand( -1, va("print \"%s.\n\"",G_GetStringEdString("MP_SVGAME", "TIMELIMIT_HIT")));
				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Timelimit hit (1)\n");
				}
				LogExit( "Timelimit hit." );
				return;
			}
		}

		if (rp_server_empty_change_map_time.integer > 0)
		{
			if (level.num_fully_connected_clients == 0)
			{ // zyk: changes map if server has no one for some time
				if (level.server_empty_change_map_timer == 0)
					level.server_empty_change_map_timer = level.time;

				if ((level.time - level.server_empty_change_map_timer) > rp_server_empty_change_map_time.integer)
					ExitLevel();
			}
			else
			{ // zyk: if someone connects, reset the counter
				level.server_empty_change_map_timer = 0;
			}
		}
	}

	if (level.gametype == GT_POWERDUEL && level.numPlayingClients >= 3)
	{
		if (g_endPDuel)
		{
			g_endPDuel = qfalse;
			LogExit("Powerduel ended.");
		}

		//yeah, this stuff was completely insane.
		/*
		int duelists[3];
		duelists[0] = level.sortedClients[0];
		duelists[1] = level.sortedClients[1];
		duelists[2] = level.sortedClients[2];

		if (duelists[0] != -1 &&
			duelists[1] != -1 &&
			duelists[2] != -1)
		{
			if (!g_entities[duelists[0]].inuse ||
				!g_entities[duelists[0]].client ||
				g_entities[duelists[0]].client->ps.stats[STAT_HEALTH] <= 0 ||
				g_entities[duelists[0]].client->sess.sessionTeam != TEAM_FREE)
			{ //The lone duelist lost, give the other two wins (if applicable) and him a loss
				if (g_entities[duelists[0]].inuse &&
					g_entities[duelists[0]].client)
				{
					g_entities[duelists[0]].client->sess.losses++;
					ClientUserinfoChanged(duelists[0]);
				}
				if (g_entities[duelists[1]].inuse &&
					g_entities[duelists[1]].client)
				{
					if (g_entities[duelists[1]].client->ps.stats[STAT_HEALTH] > 0 &&
						g_entities[duelists[1]].client->sess.sessionTeam == TEAM_FREE)
					{
						g_entities[duelists[1]].client->sess.wins++;
					}
					else
					{
						g_entities[duelists[1]].client->sess.losses++;
					}
					ClientUserinfoChanged(duelists[1]);
				}
				if (g_entities[duelists[2]].inuse &&
					g_entities[duelists[2]].client)
				{
					if (g_entities[duelists[2]].client->ps.stats[STAT_HEALTH] > 0 &&
						g_entities[duelists[2]].client->sess.sessionTeam == TEAM_FREE)
					{
						g_entities[duelists[2]].client->sess.wins++;
					}
					else
					{
						g_entities[duelists[2]].client->sess.losses++;
					}
					ClientUserinfoChanged(duelists[2]);
				}

				//Will want to parse indecies for two out at some point probably
				trap->SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", duelists[1] ) );

				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Coupled duelists won (1)\n");
				}
				LogExit( "Coupled duelists won." );
				gDuelExit = qfalse;
			}
			else if ((!g_entities[duelists[1]].inuse ||
				!g_entities[duelists[1]].client ||
				g_entities[duelists[1]].client->sess.sessionTeam != TEAM_FREE ||
				g_entities[duelists[1]].client->ps.stats[STAT_HEALTH] <= 0) &&
				(!g_entities[duelists[2]].inuse ||
				!g_entities[duelists[2]].client ||
				g_entities[duelists[2]].client->sess.sessionTeam != TEAM_FREE ||
				g_entities[duelists[2]].client->ps.stats[STAT_HEALTH] <= 0))
			{ //the coupled duelists lost, give the lone duelist a win (if applicable) and the couple both losses
				if (g_entities[duelists[1]].inuse &&
					g_entities[duelists[1]].client)
				{
					g_entities[duelists[1]].client->sess.losses++;
					ClientUserinfoChanged(duelists[1]);
				}
				if (g_entities[duelists[2]].inuse &&
					g_entities[duelists[2]].client)
				{
					g_entities[duelists[2]].client->sess.losses++;
					ClientUserinfoChanged(duelists[2]);
				}

				if (g_entities[duelists[0]].inuse &&
					g_entities[duelists[0]].client &&
					g_entities[duelists[0]].client->ps.stats[STAT_HEALTH] > 0 &&
					g_entities[duelists[0]].client->sess.sessionTeam == TEAM_FREE)
				{
					g_entities[duelists[0]].client->sess.wins++;
					ClientUserinfoChanged(duelists[0]);
				}

				trap->SetConfigstring ( CS_CLIENT_DUELWINNER, va("%i", duelists[0] ) );

				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Lone duelist won (1)\n");
				}
				LogExit( "Lone duelist won." );
				gDuelExit = qfalse;
			}
		}
		*/
		return;
	}

	if ( level.numPlayingClients < 2 ) {
		return;
	}

	if (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
	{
		if (fraglimit.integer > 1)
		{
			sKillLimit = "Kill limit hit.";
		}
		else
		{
			sKillLimit = "";
			printLimit = qfalse;
		}
	}
	else
	{
		sKillLimit = "Kill limit hit.";
	}
	if ( level.gametype < GT_SIEGE && fraglimit.integer ) {
		if ( level.teamScores[TEAM_RED] >= fraglimit.integer ) {
			trap->SendServerCommand( -1, va("print \"Red %s\n\"", G_GetStringEdString("MP_SVGAME", "HIT_THE_KILL_LIMIT")) );
			if (d_powerDuelPrint.integer)
			{
				Com_Printf("POWERDUEL WIN CONDITION: Kill limit (1)\n");
			}
			LogExit( sKillLimit );
			return;
		}

		if ( level.teamScores[TEAM_BLUE] >= fraglimit.integer ) {
			trap->SendServerCommand( -1, va("print \"Blue %s\n\"", G_GetStringEdString("MP_SVGAME", "HIT_THE_KILL_LIMIT")) );
			if (d_powerDuelPrint.integer)
			{
				Com_Printf("POWERDUEL WIN CONDITION: Kill limit (2)\n");
			}
			LogExit( sKillLimit );
			return;
		}

		for ( i=0 ; i< sv_maxclients.integer ; i++ ) {
			cl = level.clients + i;
			if ( cl->pers.connected != CON_CONNECTED ) {
				continue;
			}
			if ( cl->sess.sessionTeam != TEAM_FREE ) {
				continue;
			}

			if ( (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL) && duel_fraglimit.integer && cl->sess.wins >= duel_fraglimit.integer )
			{
				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Duel limit hit (1)\n");
				}
				LogExit( "Duel limit hit." );
				gDuelExit = qtrue;
				trap->SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " hit the win limit.\n\"",
					cl->pers.netname ) );
				return;
			}

			if ( cl->ps.persistant[PERS_SCORE] >= fraglimit.integer ) {
				if (d_powerDuelPrint.integer)
				{
					Com_Printf("POWERDUEL WIN CONDITION: Kill limit (3)\n");
				}
				LogExit( sKillLimit );
				gDuelExit = qfalse;
				if (printLimit)
				{
					trap->SendServerCommand( -1, va("print \"%s" S_COLOR_WHITE " %s.\n\"",
													cl->pers.netname,
													G_GetStringEdString("MP_SVGAME", "HIT_THE_KILL_LIMIT")
													)
											);
				}
				return;
			}
		}
	}

	if ( level.gametype >= GT_CTF && capturelimit.integer ) {

		if ( level.teamScores[TEAM_RED] >= capturelimit.integer )
		{
			trap->SendServerCommand( -1,  va("print \"%s \"", G_GetStringEdString("MP_SVGAME", "PRINTREDTEAM")));
			trap->SendServerCommand( -1,  va("print \"%s.\n\"", G_GetStringEdString("MP_SVGAME", "HIT_CAPTURE_LIMIT")));
			LogExit( "Capturelimit hit." );
			return;
		}

		if ( level.teamScores[TEAM_BLUE] >= capturelimit.integer ) {
			trap->SendServerCommand( -1,  va("print \"%s \"", G_GetStringEdString("MP_SVGAME", "PRINTBLUETEAM")));
			trap->SendServerCommand( -1,  va("print \"%s.\n\"", G_GetStringEdString("MP_SVGAME", "HIT_CAPTURE_LIMIT")));
			LogExit( "Capturelimit hit." );
			return;
		}
	}
}



/*
========================================================================

FUNCTIONS CALLED EVERY FRAME

========================================================================
*/

void G_RemoveDuelist(int team)
{
	int i = 0;
	gentity_t *ent;
	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent->inuse && ent->client && ent->client->sess.sessionTeam != TEAM_SPECTATOR &&
			ent->client->sess.duelTeam == team)
		{
			SetTeam(ent, "s");
		}
        i++;
	}
}

/*
=============
CheckTournament

Once a frame, check for changes in tournament player state
=============
*/
int g_duelPrintTimer = 0;
void CheckTournament( void ) {
	// check because we run 3 game frames before calling Connect and/or ClientBegin
	// for clients on a map_restart
//	if ( level.numPlayingClients == 0 && (level.gametype != GT_POWERDUEL) ) {
//		return;
//	}

	if (level.gametype == GT_POWERDUEL)
	{
		if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
		{
			trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
		}
	}
	else
	{
		if (level.numPlayingClients >= 2)
		{
			trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
		}
	}

	if ( level.gametype == GT_DUEL )
	{
		// pull in a spectator if needed
		if ( level.numPlayingClients < 2 && !level.intermissiontime && !level.intermissionQueued ) {
			AddTournamentPlayer();

			if (level.numPlayingClients >= 2)
			{
				trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i", level.sortedClients[0], level.sortedClients[1] ) );
			}
		}

		if (level.numPlayingClients >= 2)
		{
// nmckenzie: DUEL_HEALTH
			if ( g_showDuelHealths.integer >= 1 )
			{
				playerState_t *ps1, *ps2;
				ps1 = &level.clients[level.sortedClients[0]].ps;
				ps2 = &level.clients[level.sortedClients[1]].ps;
				trap->SetConfigstring ( CS_CLIENT_DUELHEALTHS, va("%i|%i|!",
					ps1->stats[STAT_HEALTH], ps2->stats[STAT_HEALTH]));
			}
		}

		//rww - It seems we have decided there will be no warmup in duel.
		//if (!g_warmup.integer)
		{ //don't care about any of this stuff then, just add people and leave me alone
			level.warmupTime = 0;
			return;
		}
#if 0
		// if we don't have two players, go back to "waiting for players"
		if ( level.numPlayingClients != 2 ) {
			if ( level.warmupTime != -1 ) {
				level.warmupTime = -1;
				trap->SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
				G_LogPrintf( "Warmup:\n" );
			}
			return;
		}

		if ( level.warmupTime == 0 ) {
			return;
		}

		// if the warmup is changed at the console, restart it
		if ( g_warmup.modificationCount != level.warmupModificationCount ) {
			level.warmupModificationCount = g_warmup.modificationCount;
			level.warmupTime = -1;
		}

		// if all players have arrived, start the countdown
		if ( level.warmupTime < 0 ) {
			if ( level.numPlayingClients == 2 ) {
				// fudge by -1 to account for extra delays
				level.warmupTime = level.time + ( g_warmup.integer - 1 ) * 1000;

				if (level.warmupTime < (level.time + 3000))
				{ //rww - this is an unpleasent hack to keep the level from resetting completely on the client (this happens when two map_restarts are issued rapidly)
					level.warmupTime = level.time + 3000;
				}
				trap->SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			}
			return;
		}

		// if the warmup time has counted down, restart
		if ( level.time > level.warmupTime ) {
			level.warmupTime += 10000;
			trap->Cvar_Set( "g_restarted", "1" );
			trap->SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
			level.restarted = qtrue;
			return;
		}
#endif
	}
	else if (level.gametype == GT_POWERDUEL)
	{
		if (level.numPlayingClients < 2)
		{ //hmm, ok, pull more in.
			g_dontFrickinCheck = qfalse;
		}

		if (level.numPlayingClients > 3)
		{ //umm..yes..lets take care of that then.
			int lone = 0, dbl = 0;

			G_PowerDuelCount(&lone, &dbl, qfalse);
			if (lone > 1)
			{
				G_RemoveDuelist(DUELTEAM_LONE);
			}
			else if (dbl > 2)
			{
				G_RemoveDuelist(DUELTEAM_DOUBLE);
			}
		}
		else if (level.numPlayingClients < 3)
		{ //hmm, someone disconnected or something and we need em
			int lone = 0, dbl = 0;

			G_PowerDuelCount(&lone, &dbl, qfalse);
			if (lone < 1)
			{
				g_dontFrickinCheck = qfalse;
			}
			else if (dbl < 1)
			{
				g_dontFrickinCheck = qfalse;
			}
		}

		// pull in a spectator if needed
		if (level.numPlayingClients < 3 && !g_dontFrickinCheck)
		{
			AddPowerDuelPlayers();

			if (level.numPlayingClients >= 3 &&
				G_CanResetDuelists())
			{
				gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_DUEL);
				te->r.svFlags |= SVF_BROADCAST;
				//this is really pretty nasty, but..
				te->s.otherEntityNum = level.sortedClients[0];
				te->s.otherEntityNum2 = level.sortedClients[1];
				te->s.groundEntityNum = level.sortedClients[2];

				trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );
				G_ResetDuelists();

				g_dontFrickinCheck = qtrue;
			}
			else if (level.numPlayingClients > 0 ||
				level.numConnectedClients > 0)
			{
				if (g_duelPrintTimer < level.time)
				{ //print once every 10 seconds
					int lone = 0, dbl = 0;

					G_PowerDuelCount(&lone, &dbl, qtrue);
					if (lone < 1)
					{
						trap->SendServerCommand( -1, va("cp \"%s\n\"", G_GetStringEdString("MP_SVGAME", "DUELMORESINGLE")) );
					}
					else
					{
						trap->SendServerCommand( -1, va("cp \"%s\n\"", G_GetStringEdString("MP_SVGAME", "DUELMOREPAIRED")) );
					}
					g_duelPrintTimer = level.time + 10000;
				}
			}

			if (level.numPlayingClients >= 3 && level.numNonSpectatorClients >= 3)
			{ //pulled in a needed person
				if (G_CanResetDuelists())
				{
					gentity_t *te = G_TempEntity(vec3_origin, EV_GLOBAL_DUEL);
					te->r.svFlags |= SVF_BROADCAST;
					//this is really pretty nasty, but..
					te->s.otherEntityNum = level.sortedClients[0];
					te->s.otherEntityNum2 = level.sortedClients[1];
					te->s.groundEntityNum = level.sortedClients[2];

					trap->SetConfigstring ( CS_CLIENT_DUELISTS, va("%i|%i|%i", level.sortedClients[0], level.sortedClients[1], level.sortedClients[2] ) );

					if ( g_austrian.integer )
					{
						G_LogPrintf("Duel Initiated: %s %d/%d vs %s %d/%d and %s %d/%d, kill limit: %d\n",
							level.clients[level.sortedClients[0]].pers.netname,
							level.clients[level.sortedClients[0]].sess.wins,
							level.clients[level.sortedClients[0]].sess.losses,
							level.clients[level.sortedClients[1]].pers.netname,
							level.clients[level.sortedClients[1]].sess.wins,
							level.clients[level.sortedClients[1]].sess.losses,
							level.clients[level.sortedClients[2]].pers.netname,
							level.clients[level.sortedClients[2]].sess.wins,
							level.clients[level.sortedClients[2]].sess.losses,
							fraglimit.integer );
					}
					//trap->SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
					//FIXME: This seems to cause problems. But we'd like to reset things whenever a new opponent is set.
				}
			}
		}
		else
		{ //if you have proper num of players then don't try to add again
			g_dontFrickinCheck = qtrue;
		}

		level.warmupTime = 0;
		return;
	}
	else if ( level.warmupTime != 0 ) {
		int		counts[TEAM_NUM_TEAMS];
		qboolean	notEnough = qfalse;

		if ( level.gametype > GT_TEAM ) {
			counts[TEAM_BLUE] = TeamCount( -1, TEAM_BLUE );
			counts[TEAM_RED] = TeamCount( -1, TEAM_RED );

			if (counts[TEAM_RED] < 1 || counts[TEAM_BLUE] < 1) {
				notEnough = qtrue;
			}
		} else if ( level.numPlayingClients < 2 ) {
			notEnough = qtrue;
		}

		if ( notEnough ) {
			if ( level.warmupTime != -1 ) {
				level.warmupTime = -1;
				trap->SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
				G_LogPrintf( "Warmup:\n" );
			}
			return; // still waiting for team members
		}

		if ( level.warmupTime == 0 ) {
			return;
		}

		// if the warmup is changed at the console, restart it
		/*
		if ( g_warmup.modificationCount != level.warmupModificationCount ) {
			level.warmupModificationCount = g_warmup.modificationCount;
			level.warmupTime = -1;
		}
		*/

		// if all players have arrived, start the countdown
		if ( level.warmupTime < 0 ) {
			// fudge by -1 to account for extra delays
			if ( g_warmup.integer > 1 ) {
				level.warmupTime = level.time + ( g_warmup.integer - 1 ) * 1000;
			} else {
				level.warmupTime = 0;
			}
			trap->SetConfigstring( CS_WARMUP, va("%i", level.warmupTime) );
			return;
		}

		// if the warmup time has counted down, restart
		if ( level.time > level.warmupTime ) {
			level.warmupTime += 10000;
			trap->Cvar_Set( "g_restarted", "1" );
			trap->Cvar_Update( &g_restarted );
			trap->SendConsoleCommand( EXEC_APPEND, "map_restart 0\n" );
			level.restarted = qtrue;
			return;
		}
	}
}

void G_KickAllBots(void)
{
	int i;
	gclient_t	*cl;

	for ( i=0 ; i< sv_maxclients.integer ; i++ )
	{
		cl = level.clients + i;
		if ( cl->pers.connected != CON_CONNECTED )
		{
			continue;
		}
		if ( !(g_entities[i].r.svFlags & SVF_BOT) )
		{
			continue;
		}
		trap->SendConsoleCommand( EXEC_INSERT, va("clientkick %d\n", i) );
	}
}

/*
==================
CheckVote
==================
*/
void CheckVote( void ) {
	if ( level.voteExecuteTime && level.voteExecuteTime < level.time ) {
		level.voteExecuteTime = 0;
		trap->SendConsoleCommand( EXEC_APPEND, va( "%s\n", level.voteString ) );

		if (level.votingGametype)
		{
			if (level.gametype != level.votingGametypeTo)
			{ //If we're voting to a different game type, be sure to refresh all the map stuff
				const char *nextMap = G_RefreshNextMap(level.votingGametypeTo, qtrue);

				if (level.votingGametypeTo == GT_SIEGE)
				{ //ok, kick all the bots, cause the aren't supported!
                    G_KickAllBots();
					//just in case, set this to 0 too... I guess...maybe?
					//trap->Cvar_Set("bot_minplayers", "0");
				}

				if (nextMap && nextMap[0] && rp_change_map_gametype_vote.integer)
				{
					trap->SendConsoleCommand( EXEC_APPEND, va("map %s\n", nextMap ) );
				}
				else
				{ // zyk: if rp_change_map_gametype_vote is 0, just restart the current map
					trap->SendConsoleCommand( EXEC_APPEND, "map_restart 0\n"  );
				}
			}
			else
			{ //otherwise, just leave the map until a restart
				G_RefreshNextMap(level.votingGametypeTo, qfalse);
			}

			if (g_fraglimitVoteCorrection.integer)
			{ //This means to auto-correct fraglimit when voting to and from duel.
				const int currentGT = level.gametype;
				const int currentFL = fraglimit.integer;
				const int currentTL = timelimit.integer;

				if ((level.votingGametypeTo == GT_DUEL || level.votingGametypeTo == GT_POWERDUEL) && currentGT != GT_DUEL && currentGT != GT_POWERDUEL)
				{
					if (currentFL > 1 || !currentFL)
					{ //if voting to duel, and fraglimit is more than 1 (or unlimited), then set it down to 1
						trap->SendConsoleCommand(EXEC_APPEND, "fraglimit 1\n"); // zyk: changed from 3 to 1
					}
					if (currentTL)
					{ //if voting to duel, and timelimit is set, make it unlimited
						trap->SendConsoleCommand(EXEC_APPEND, "timelimit 0\n");
					}
				}
				else if ((level.votingGametypeTo != GT_DUEL && level.votingGametypeTo != GT_POWERDUEL) &&
					(currentGT == GT_DUEL || currentGT == GT_POWERDUEL))
				{
					if (currentFL != 0)
					{ //if voting from duel, an fraglimit is different than 0, then set it up to 0
						trap->SendConsoleCommand(EXEC_APPEND, "fraglimit 0\n"); // zyk: changed from 20 to 0
					}
				}
			}

			level.votingGametype = qfalse;
			level.votingGametypeTo = 0;
		}
	}
	if ( !level.voteTime ) {
		return;
	}
	if ( level.time-level.voteTime >= VOTE_TIME) // || level.voteYes + level.voteNo == 0 ) zyk: no longer does this
	{
		if (level.voteYes > level.voteNo)
		{ // zyk: now vote pass if number of Yes is greater than number of No
			trap->SendServerCommand( -1, va("print \"%s (%s)\n\"", G_GetStringEdString("MP_SVGAME", "VOTEPASSED"), level.voteStringClean) );
			level.voteExecuteTime = level.time + level.voteExecuteDelay;
		}
		else
		{
			trap->SendServerCommand( -1, va("print \"%s (%s)\n\"", G_GetStringEdString("MP_SVGAME", "VOTEFAILED"), level.voteStringClean) );
			// GalaxyRP fix: [Vote] a failed gametype vote no longer leaves votingGametype set -- see below
			level.votingGametype = qfalse;
		}

		// zyk: set the timer for the next vote of this player
		if (rp_vote_timer.integer > 0 && level.voting_player > -1)
			g_entities[level.voting_player].client->sess.vote_timer = rp_vote_timer.integer;
	}
	else 
	{
		if ( level.voteYes > level.numVotingClients/2 ) {
			// execute the command, then remove the vote
			trap->SendServerCommand( -1, va("print \"%s (%s)\n\"", G_GetStringEdString("MP_SVGAME", "VOTEPASSED"), level.voteStringClean) );
			level.voteExecuteTime = level.time + level.voteExecuteDelay;
			// zyk: set the timer for the next vote of this player
			if (rp_vote_timer.integer > 0 && level.voting_player > -1)
				g_entities[level.voting_player].client->sess.vote_timer = rp_vote_timer.integer;
		}
		// same behavior as a timeout
		else if ( level.voteNo >= (level.numVotingClients+1)/2 )
		{
			trap->SendServerCommand( -1, va("print \"%s (%s)\n\"", G_GetStringEdString("MP_SVGAME", "VOTEFAILED"), level.voteStringClean) );
			// GalaxyRP fix: [Vote] G_VoteGametype() raises votingGametype when the vote is CALLED, and only
			// the execute block above lowered it again, so a gametype vote that failed left it standing
			// until the next /callvote. Whatever next set voteExecuteTime without going through
			// Cmd_CallVote_f() -- which is exactly what the team-vote timer bug in CheckTeamVote() did --
			// then ran the gametype switch (map change, bot kick, fraglimit reset) for a vote nobody
			// passed. Nothing can reach that any more, but a failed vote should not leave the flag armed.
			level.votingGametype = qfalse;
			// zyk: set the timer for the next vote of this player
			if (rp_vote_timer.integer > 0 && level.voting_player > -1)
				g_entities[level.voting_player].client->sess.vote_timer = rp_vote_timer.integer;
		}
		else // still waiting for a majority
			return;
	}
	level.voteTime = 0;

	// GalaxyRP fix: [Vote] level.voting_player was set when the vote was called and then never cleared,
	// so it went on naming a client slot after the vote had resolved. Client slots are reused: if the
	// player who called the vote disconnects and somebody else connects into the same slot before the
	// vote finishes, the rp_vote_timer cooldown above is applied to the newcomer, who is then refused
	// with "You cannot vote now" without ever having voted. Clearing it with the vote it belongs to
	// keeps the two in step. (Only matters on servers that set rp_vote_timer above 0.)
	level.voting_player = -1;

	trap->SetConfigstring( CS_VOTE_TIME, "" );
}

/*
==================
PrintTeam
==================
*/
void PrintTeam(int team, char *message) {
	int i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		trap->SendServerCommand( i, message );
	}
}

/*
==================
SetLeader
==================
*/
void SetLeader(int team, int client) {
	int i;

	if ( level.clients[client].pers.connected == CON_DISCONNECTED ) {
		PrintTeam(team, va("print \"%s is not connected\n\"", level.clients[client].pers.netname) );
		return;
	}
	if (level.clients[client].sess.sessionTeam != team) {
		PrintTeam(team, va("print \"%s is not on the team anymore\n\"", level.clients[client].pers.netname) );
		return;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		if (level.clients[i].sess.teamLeader) {
			level.clients[i].sess.teamLeader = qfalse;
			ClientUserinfoChanged(i);
		}
	}
	level.clients[client].sess.teamLeader = qtrue;
	ClientUserinfoChanged( client );
	PrintTeam(team, va("print \"%s %s\n\"", level.clients[client].pers.netname, G_GetStringEdString("MP_SVGAME", "NEWTEAMLEADER")) );
}

/*
==================
CheckTeamLeader
==================
*/
void CheckTeamLeader( int team ) {
	int i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if (level.clients[i].sess.sessionTeam != team)
			continue;
		if (level.clients[i].sess.teamLeader)
			break;
	}
	if (i >= level.maxclients) {
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			if (level.clients[i].sess.sessionTeam != team)
				continue;
			if (!(g_entities[i].r.svFlags & SVF_BOT)) {
				level.clients[i].sess.teamLeader = qtrue;
				break;
			}
		}
		if ( i >= level.maxclients ) {
			for ( i = 0 ; i < level.maxclients ; i++ ) {
				if ( level.clients[i].sess.sessionTeam != team )
					continue;
				level.clients[i].sess.teamLeader = qtrue;
				break;
			}
		}
	}
}

/*
==================
CheckTeamVote
==================
*/
void CheckTeamVote( int team ) {
	int cs_offset;

	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( level.teamVoteExecuteTime[cs_offset] && level.teamVoteExecuteTime[cs_offset] < level.time ) {
		level.teamVoteExecuteTime[cs_offset] = 0;
		if ( !Q_strncmp( "leader", level.teamVoteString[cs_offset], 6) ) {
			//set the team leader
			SetLeader(team, atoi(level.teamVoteString[cs_offset] + 7));
		}
		else {
			trap->SendConsoleCommand( EXEC_APPEND, va("%s\n", level.teamVoteString[cs_offset] ) );
		}
	}

	if ( !level.teamVoteTime[cs_offset] ) {
		return;
	}

	if ( level.time-level.teamVoteTime[cs_offset] >= VOTE_TIME || level.teamVoteYes[cs_offset] + level.teamVoteNo[cs_offset] == 0 ) {
		trap->SendServerCommand( -1, va("print \"%s (%s)\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEFAILED"), level.teamVoteStringClean[cs_offset]) );
	}
	else {
		if ( level.teamVoteYes[cs_offset] > level.numteamVotingClients[cs_offset]/2 ) {
			// execute the command, then remove the vote
			trap->SendServerCommand( -1, va("print \"%s (%s)\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEPASSED"), level.teamVoteStringClean[cs_offset]) );
			// GalaxyRP fix: [Vote] this set level.voteExecuteTime -- the GLOBAL vote's timer -- instead of
			// this team's own. teamVoteExecuteTime[] was read by the block at the top of this function but
			// never set anywhere, so a passed leader vote never made anyone leader; instead, three seconds
			// later CheckVote() re-ran level.voteString, which is never cleared: the last global vote of the
			// map (a map change, a gametype change, a restart), or -- with a global vote still being voted
			// on -- a command nobody had passed yet. Inherited from upstream (TaystJK has it too). Team
			// votes exist only in the Red/Blue gametypes, so FFA never reached it.
			level.teamVoteExecuteTime[cs_offset] = level.time + 3000;
		}

		// same behavior as a timeout
		else if ( level.teamVoteNo[cs_offset] >= (level.numteamVotingClients[cs_offset]+1)/2 )
			trap->SendServerCommand( -1, va("print \"%s (%s)\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEFAILED"), level.teamVoteStringClean[cs_offset]) );

		else // still waiting for a majority
			return;
	}
	level.teamVoteTime[cs_offset] = 0;
	trap->SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, "" );
}


/*
==================
CheckCvars
==================
*/
void CheckCvars( void ) {
	static int lastMod = -1;

	if ( g_password.modificationCount != lastMod ) {
		char password[MAX_INFO_STRING];
		char *c = password;
		lastMod = g_password.modificationCount;

		strcpy( password, g_password.string );
		while( *c )
		{
			if ( *c == '%' )
			{
				*c = '.';
			}
			c++;
		}
		trap->Cvar_Set("g_password", password );

		if (*g_password.string && Q_stricmp(g_password.string, "none")) {
			trap->Cvar_Set( "g_needpass", "1" );
		} else {
			trap->Cvar_Set( "g_needpass", "0" );
		}
	}
}

/*
=============
G_RunThink

Runs thinking code for this frame if necessary
=============
*/
void G_RunThink (gentity_t *ent) {
	float	thinktime;

	thinktime = ent->nextthink;
	if (thinktime <= 0) {
		goto runicarus;
	}
	if (thinktime > level.time) {
		goto runicarus;
	}

	ent->nextthink = 0;
	if (!ent->think) {
		//trap->Error( ERR_DROP, "NULL ent->think");
		goto runicarus;
	}
	ent->think (ent);

runicarus:
	// GalaxyRP: [Logical Entities] a logical entity has no ICARUS task manager (it was never
	// ICARUS_InitEnt'ed -- the engine does not know its number) and never an NPC, so there is
	// nothing to maintain and the number must not be handed over.
	if ( ent->inuse && !ent->isLogical )
	{
		SaveNPCGlobals();
		if(NPCS.NPCInfo == NULL && ent->NPC != NULL)
		{
			SetNPCGlobals( ent );
		}
		trap->ICARUS_MaintainTaskManager(ent->s.number);
		RestoreNPCGlobals();
	}
}

int g_LastFrameTime = 0;
int g_TimeSinceLastFrame = 0;

qboolean gDoSlowMoDuel = qfalse;
int gSlowMoDuelTime = 0;

//#define _G_FRAME_PERFANAL

void NAV_CheckCalcPaths( void )
{
	if ( navCalcPathTime && navCalcPathTime < level.time )
	{//first time we've ever loaded this map...
		vmCvar_t	mapname;
		vmCvar_t	ckSum;

		trap->Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
		trap->Cvar_Register( &ckSum, "sv_mapChecksum", "", CVAR_ROM );

		//clear all the failed edges
		trap->Nav_ClearAllFailedEdges();

		//Calculate all paths
		NAV_CalculatePaths( mapname.string, ckSum.integer );

		trap->Nav_CalculatePaths(qfalse);

#ifndef FINAL_BUILD
		if ( fatalErrors )
		{
			Com_Printf( S_COLOR_RED"Not saving .nav file due to fatal nav errors\n" );
		}
		else
#endif
		if ( trap->Nav_Save( mapname.string, ckSum.integer ) == qfalse )
		{
			Com_Printf("Unable to save navigations data for map \"%s\" (checksum:%d)\n", mapname.string, ckSum.integer );
		}
		navCalcPathTime = 0;
	}
}

//so shared code can get the local time depending on the side it's executed on
int BG_GetTime(void)
{
	return level.time;
}

// zyk: similar to TeleportPlayer(), but this one doesnt spit the player out at the destination
void zyk_TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles ) {
	gentity_t	*tent;
	qboolean	isNPC = qfalse;

	if (player->s.eType == ET_NPC)
	{
		isNPC = qtrue;
	}

	// GalaxyRP: [Grapple Hook] same as TeleportPlayer() in g_misc.c: the rope stays behind.
	if ( player->client && player->client->hook )
	{
		Weapon_HookFree( player->client->hook );
	}

	// use temp events at source and destination to prevent the effect
	// from getting dropped by a second player event
	if ( player->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		tent = G_TempEntity( player->client->ps.origin, EV_PLAYER_TELEPORT_OUT );
		tent->s.clientNum = player->s.clientNum;

		tent = G_TempEntity( origin, EV_PLAYER_TELEPORT_IN );
		tent->s.clientNum = player->s.clientNum;
	}

	// unlink to make sure it can't possibly interfere with G_KillBox
	trap->UnlinkEntity ((sharedEntity_t *)player);

	VectorCopy ( origin, player->client->ps.origin );
	player->client->ps.origin[2] += 1;

	// set angles
	SetClientViewAngle( player, angles );

	// toggle the teleport bit so the client knows to not lerp
	player->client->ps.eFlags ^= EF_TELEPORT_BIT;

	// kill anything at the destination
	if ( player->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		G_KillBox (player);
	}

	// save results of pmove
	BG_PlayerStateToEntityState( &player->client->ps, &player->s, qtrue );
	if (isNPC)
	{
		player->s.eType = ET_NPC;
	}

	// DAJ_RP: [Phase] /admhidden -- the copy above rebuilt s from ps
	RP_HiddenApplyState( player, &player->s );

	// use the precise origin for linking
	VectorCopy( player->client->ps.origin, player->r.currentOrigin );

	if ( player->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		trap->LinkEntity ((sharedEntity_t *)player);
	}

	// DAJ_RP fix: [Vehicles] a vehicle takes its riders along, as with TeleportPlayer() (RP_CarryRiders(), g_misc.c)
	RP_CarryRiders( player, angles );
}

// zyk: tests if ent has other as ally
/*
GalaxyRP fix: [Ally] the raw bitfield test, with no other conditions attached.

An ally list is two ints: ally1 holds client slots 0-15 and ally2 holds 16 and up at bit
(slot - 16). That split was open-coded in four places -- here, zyk_add_ally(), zyk_remove_ally()
and ClientDisconnect() -- which is three chances for them to disagree. Reading now goes through
this one function, and writing already went through the add/remove pair.

It also removes an undefined shift. zyk_is_ally() used to read:

    if (slot > 15 && (ally2 & (1 << (slot - 16)))) return qtrue;
    else if (ally1 & (1 << slot))                  return qtrue;

The else branch runs whenever the first condition is false -- including when the slot IS above 15
and its ally2 bit simply is not set. That evaluated 1 << slot for slots up to 31, and 1 << 31 on a
signed int is undefined behaviour. It also asked ally1 about a slot ally1 never stores, so a stray
bit in its high half would have reported a stranger as an ally. Branching on the slot number
rather than on the bit test makes both impossible.
*/
qboolean zyk_ally_bit_set(gentity_t *owner, int client_id)
{
	if (!owner || !owner->client || client_id < 0 || client_id >= MAX_CLIENTS)
		return qfalse;

	if (client_id > 15)
		return (owner->client->sess.ally2 & (1 << (client_id - 16))) ? qtrue : qfalse;

	return (owner->client->sess.ally1 & (1 << client_id)) ? qtrue : qfalse;
}

qboolean zyk_is_ally(gentity_t *ent, gentity_t *other)
{
	if (ent && other && !ent->NPC && !other->NPC && ent != other && ent->client && other->client && other->client->pers.connected == CON_CONNECTED)
	{
		return zyk_ally_bit_set(ent, other->s.number);
	}

	return qfalse;
}

/*
GalaxyRP fix: [NPC] drop the /order guard and /order cover bits.

These two bits are the only thing NPC_ValidEnemy() consults before it starts asking
zyk_is_ally(leader, ent) who is friendly, and a NULL leader answers "nobody" for everyone -- so an
NPC carrying an order bit without a leader treats its own side as valid enemies. The bits must
therefore never outlive the leader they were given under, which makes "clear the orders" an
operation worth naming rather than two lines to remember to copy. Every site that drops a leader
calls this: the release below, and Q3_SetLeader(), which sets and clears client->leader without
going anywhere near a bState.
*/
void zyk_clear_npc_order_bits(gentity_t *npc_ent)
{
	if (!npc_ent || !npc_ent->client)
		return;

	npc_ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_NPC_ORDER_GUARD);
	npc_ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_NPC_ORDER_COVER);
}

/*
GalaxyRP fix: [NPC] the one place that breaks an NPC's follow link to its leader.

The three-line dance -- drop the /order guard and /order cover bits, drop the leader, fall back
to BS_STAND_GUARD -- was open-coded in TryUse() and again in ClientDisconnect(), and is now
needed in two more places (see zyk_release_player_npcs() and zyk_npc_leader_lost() below). Four
hand-written copies is four chances for them to drift apart, which is exactly how
NPC_CheckCharmed() ended up clearing the leader without the order bits. One function now.

goalEntity is cleared too when it is the leader: NPC_BSFollowLeader() parks the leader there as
the move goal whenever it closes or backs off, and NPC_SlideMoveToGoal() reads
goalEntity->r.currentOrigin without an inuse test. G_ClearEnemy() does the same for the enemy
pointer, for the same reason.

Vehicles keep their behaviour untouched, matching the guard TryUse() and Cmd_Order_f() already
apply -- BS_STAND_GUARD is meaningless for a vehicle and its bState is driven by its rider.
*/
void zyk_release_npc_from_leader(gentity_t *npc_ent)
{
	if (!npc_ent || !npc_ent->client || !npc_ent->NPC)
		return;

	zyk_clear_npc_order_bits(npc_ent);

	if (npc_ent->client->leader && npc_ent->NPC->goalEntity == npc_ent->client->leader)
	{
		npc_ent->NPC->goalEntity = NULL;
	}

	npc_ent->client->leader = NULL;

	if (npc_ent->client->NPC_class != CLASS_VEHICLE)
	{
		npc_ent->NPC->tempBehavior = BS_STAND_GUARD;
	}
}

/*
GalaxyRP fix: [NPC] should this NPC still be following the player it is following?

TryUse() tests OnSameTeam() once, at the moment the Use key claims the NPC, and nothing ever
looked again. Everything that test depends on can change a second later, and none of it did
anything: the leader could go to spectator and keep an escort walking to wherever
SpectatorClientEndFrame() had copied their playerState origin to -- for a following spectator
that is the position of whoever they are watching, so an NPC could be aimed at a live player it
was never given to -- or simply switch team and keep NPCs claimed as the other side.

Note that re-running OnSameTeam() by itself catches none of this. Its player-versus-NPC arm
returns qtrue for any ET_PLAYER against an ET_NPC on NPCTEAM_PLAYER and returns before it ever
looks at sessionTeam, so it reads the same both before and after any team change, spectator
included. The conditions that actually change are spelled out here instead.

Red/blue switches are handled where they happen, in SetTeam() and SetTeamQuick(); this is the
per-frame net under them, for the routes that write sessionTeam directly and never call either
(StopFollowing() is the obvious one).

Two kinds of link are deliberately out of scope. Mind Trick 3 charm sets leader as well, but
NPC_CheckCharmed() owns that link and restores the NPC's original teams when charmedTime runs
out, so it is left alone. ICARUS SET_LEADER can name another NPC as the leader, and a map script
pointing one NPC at another has nothing to do with the Use key.
*/
/*
Stop an NPC under a /order guard from walking after the player it is guarding for.

/order guard sets NPC->tempBehavior = BS_STAND_GUARD, and the behaviour that name promises --
NPC_BSStandGuard(), which issues no movement at all -- is only ever reached from inside
NPC_BSFollowLeader(), whose opening test reads the guard bit. But BS_STAND_GUARD routes the NPC
AWAY from NPC_BSFollowLeader: NPC_RunBehavior() dispatches by class, and thirteen of the seventeen
behaviour sets map BS_STAND_GUARD onto their own <class>_Default AI instead (NPC_BehaviorSet_Jedi
-> NPC_BSJedi_Default, NPC_BehaviorSet_Stormtrooper -> NPC_BSST_Default, and so on). Those AIs end
with the shape

	if ( UpdateGoal() ) { ucmd.buttons |= BUTTON_WALKING; NPC_MoveToGoal( qtrue ); }

and goalEntity is still the leader, left there by NPC_BSFollowLeader() while the NPC was following
-- /order guard never clears it. So the NPC kept following, and because those paths force
BUTTON_WALKING unconditionally while the real follow code only walks inside a short close-range
band, it followed at walk speed instead of run speed. That speed difference is what the bug looked
like from the outside; the following itself was the actual defect.

Dropping the goal when it is the leader is deliberately the whole fix. It is the one thing that
makes the NPC walk toward the player, and clearing it leaves every other part of the class AI
untouched -- enemy acquisition, shooting, repositioning, cover, class-specific moves. NPCs whose
behaviour set does fall through to NPC_BehaviorSet_Default keep working exactly as before, because
NPC_BSDefault() reacts to a null goal by calling NPC_BSFollowLeader(), which reads the guard bit and
lands on NPC_BSStandGuard() -- the intended behaviour, reached by the route that already worked.

Scoped by the guard bit and by the goal actually being the leader, so a map- or script-spawned NPC
is untouched: those never carry bit 18, which only Cmd_Order_f() sets.
*/
/*
Can this NPC be given a /order?

The three verbs used to repeat one condition three times, which is how the two gaps below went
unnoticed. One test now, so the three branches cannot drift apart.

CLASS_WAMPA and CLASS_REMOTE are excluded for the same reason CLASS_VEHICLE already was: their AI
never reads the behaviour state, so no order can change what they do. NPC_RunBehavior() calls
NPC_BSWampa_Default() directly for a wampa and discards bState entirely, and
NPC_BehaviorSet_Remote() ignores its bState parameter and always calls NPC_BSRemote_Default(). But
/order guard and /order cover also set bits 18 and 19, and NPC_ValidEnemy() reads those to decide
that anyone not on the leader's ally list is a valid target -- reached from these classes through
NPC_CheckEnemyExt() -> NPC_FindEnemy() -> NPC_ValidEnemy(). So on those two classes an order moved
nothing and changed nothing visible, while quietly turning the NPC on its own side. Note this is
NOT true of the rancor, which looks similar but is fine: NPC_BehaviorSet_Rancor() has no
BS_FOLLOW_LEADER case and so falls through to NPC_BehaviorSet_Default(), which does handle it.

Charmed NPCs are excluded because the command was the only part of the order system that did not
already skip them. zyk_npc_leader_lost() and zyk_release_player_npcs() both bail on charmedTime, on
the grounds that NPC_CheckCharmed() owns the leader link for as long as the charm lasts -- but
Cmd_Order_f() would happily stamp an order onto a charmed NPC, and those same release paths then
refused to take it off again. Ordering one meant its leader and order bits survived a team change
or a switch to spectator that would have cleared them for any other NPC, leaving it hunting its own
team on behalf of someone no longer in the game.
*/
qboolean zyk_npc_can_take_orders(gentity_t *npc_ent, gentity_t *leader)
{
	if (!npc_ent || !npc_ent->client || !npc_ent->NPC)
		return qfalse;

	if (!leader || npc_ent->client->leader != leader)
		return qfalse;

	if (npc_ent->client->NPC_class == CLASS_VEHICLE ||
		npc_ent->client->NPC_class == CLASS_WAMPA ||
		npc_ent->client->NPC_class == CLASS_REMOTE)
		return qfalse;

	if (npc_ent->NPC->charmedTime > level.time)
		return qfalse;

	return qtrue;
}

void zyk_hold_guarding_npc(gentity_t *npc_ent)
{
	if (!npc_ent || !npc_ent->client || !npc_ent->NPC)
		return;

	if (!npc_ent->client->leader)
		return;

	if (!(npc_ent->client->pers.player_statuses & (1 << PLAYER_STATUS_NPC_ORDER_GUARD)))
		return;

	if (npc_ent->NPC->goalEntity == npc_ent->client->leader)
	{
		npc_ent->NPC->goalEntity = NULL;
	}
}

qboolean zyk_npc_leader_lost(gentity_t *npc_ent)
{
	gentity_t *leader = NULL;

	if (!npc_ent || !npc_ent->client || !npc_ent->NPC)
		return qfalse;

	leader = npc_ent->client->leader;

	if (!leader)
		return qfalse;

	if (npc_ent->NPC->charmedTime > level.time)
		return qfalse;

	if (leader->s.number >= MAX_CLIENTS || !leader->client)
		return qfalse;

	if (!leader->inuse || leader->client->pers.connected != CON_CONNECTED)
		return qtrue;

	if (leader->client->sess.sessionTeam == TEAM_SPECTATOR)
		return qtrue;

	return qfalse;
}

/*
GalaxyRP fix: [NPC] release every NPC this player is leading.

Called from SetTeam() and SetTeamQuick() when a player actually changes team. Charmed NPCs are
skipped for the reason given on zyk_npc_leader_lost() above -- the player is still in the game
here, so there is no dangling pointer to force the issue, unlike ClientDisconnect(), which
releases them too because it has to.

Starts at MAX_CLIENTS like Cmd_Order_f() and ClientDisconnect(): NPCs never hold a client slot.
*/
void zyk_release_player_npcs(gentity_t *ent)
{
	int i = 0;

	if (!ent || !ent->client)
		return;

	for (i = MAX_CLIENTS; i < level.num_entities; i++)
	{
		gentity_t *npc_ent = &g_entities[i];

		if (!npc_ent->inuse || !npc_ent->client || !npc_ent->NPC)
			continue;

		if (npc_ent->client->leader != ent)
			continue;

		if (npc_ent->NPC->charmedTime > level.time)
			continue;

		zyk_release_npc_from_leader(npc_ent);
	}
}

// zyk: counts how many allies this player has
int zyk_number_of_allies(gentity_t *ent, qboolean in_rpg_mode)
{
	int i = 0;
	int number_of_allies = 0;

	for (i = 0; i < level.maxclients; i++)
	{
		gentity_t *allied_player = &g_entities[i];

		if (zyk_is_ally(ent,allied_player) == qtrue && (in_rpg_mode == qfalse || (allied_player->client->sess.amrpgmode == 2 && allied_player->client->sess.sessionTeam != TEAM_SPECTATOR)))
			number_of_allies++;
	}

	return number_of_allies;
}

// GalaxyRP fix: [Guardian] zyk_start_boss_battle_music() and spawn_boss() removed here — guardian_mode/guardian_invoked_by_id are permanently dead (spawn_boss has no callers); this also removed the Challenge-Mode boss-HP-boost and boss-power-upgrade code that lived inside spawn_boss

// zyk: tests if this player is one of the Duel Tournament duelists
qboolean duel_tournament_is_duelist(gentity_t *ent)
{
	if (ent->s.number == level.duelist_1_id || ent->s.number == level.duelist_2_id)
	{
		return qtrue;
	}

	return qfalse;
}

// GalaxyRP fix: [Death System] the mini-games and the Death System were written apart and never
// introduced. G_Damage() only calls targ->die() for a player who is ALREADY downed; the first time
// their health reaches 0 it calls paralyze_player() instead, which downs them at RP_DOWNED_HEALTH health and
// never reaches player_die(). Both mini-games hang every piece of their death handling off
// player_die(), so the first knockdown registered as nothing at all: the Melee Battle left the
// player on the roster with melee_mode_quantity unchanged and gave the attacker no kill credit, so
// the count could never fall to 1 and melee_battle_winner() never fired; the Duel Tournament never
// set PLAYER_STATUS_DUEL_TOURNAMENT_LOSS, so the mode-4 early win never fired either. And a combat
// knockdown does not time out -- the auto-release in ClientEndFrame() is for admin paralysis only,
// so a downed combatant lies there until someone finishes them or they type /getup.
//
// Death is forced for them instead, exactly the way it already is for a player riding a vehicle:
// the caller tests this and drops through to the same targ->die() arm that a mounted player takes.
// player_die() calls RP_ClearDownedState() itself, so the arm that skips the explicit clear is
// still correct for a player who somehow arrives already downed.
//
// Scoped to the two states where the mini-games' OWN death handlers fire -- melee_mode 2 and
// duel_tournament_mode 4 with this player actually one of the two duelists -- and deliberately not
// to sign-up. A player who types /meleemode and then walks off is not in an arena, and forcing
// death on them anywhere on the map for the next twelve seconds would be a surprise with no
// purpose: nothing downstream is waiting on it.
//
// The private duel needs none of this. It is the one system that reads the downed state directly
// (see the duelInProgress block in ClientThink_real, g_active.c), ending the duel the moment either
// side goes down, so it resolves on the first knockdown without a forced death.
qboolean zyk_minigame_forces_death(gentity_t *ent)
{
	if (!ent || !ent->client || ent->s.number >= MAX_CLIENTS)
		return qfalse;

	if (level.melee_mode == 2 && level.melee_players[ent->s.number] != -1)
		return qtrue;

	if (level.duel_tournament_mode == 4 && level.duel_players[ent->s.number] != -1 &&
		duel_tournament_is_duelist(ent) == qtrue)
		return qtrue;

	return qfalse;
}

// GalaxyRP fix: [Guardian] removed zyk_can_hit_boss_battle_target() here — it was a stub always
// returning qtrue (its condition used to gate on being in a boss battle; guardian_mode was already
// permanently 0). Its 3 call sites in this file and g_active.c were simplified to drop the
// always-true conjunct.

// zyk: tests if the target player can be hit by the attacker gun/saber damage, force power or special power
// GalaxyRP fix: [Force] the "which force-power disable mask is actually in force right now" rule used
// to exist only as three inline lines inside WP_InitForcePowers (w_force.c), so every other place that
// consulted g_forcePowerDisable kept using the plain server-wide value even in Duel and Power Duel,
// where g_duelForcePowerDisable is meant to replace it. That left force powerups still spawning in
// duel gametypes and the jedi/merc split deciding on the wrong mask. Factored out here so there is one
// answer to that question and every caller gets the same one.
int G_ForcePowerDisableValue(void)
{
	if (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
	{
		return g_duelForcePowerDisable.integer;
	}

	return g_forcePowerDisable.integer;
}

qboolean zyk_can_hit_target(gentity_t *attacker, gentity_t *target)
{
	if (attacker && attacker->client && target && target->client && !attacker->NPC && !target->NPC)
	{
		// GalaxyRP fix: [Guardian] boss-battle non-quest/quest hit restriction removed here — guardian_mode is permanently 0 (spawn_boss has no callers)

		if (level.duel_tournament_mode == 4 && duel_tournament_is_duelist(attacker) != duel_tournament_is_duelist(target))
		{ // zyk: cannot hit duelists in Duel Tournament
			return qfalse;
		}

		if (level.melee_mode > 1 && ((level.melee_players[attacker->s.number] != -1 && level.melee_players[target->s.number] == -1) ||
			(level.melee_players[attacker->s.number] == -1 && level.melee_players[target->s.number] != -1)))
		{ // zyk: players outside melee battle cannot hit ones in it and vice-versa
			return qfalse;
		}

		// GalaxyRP: [nofight] the two PLAYER_STATUS_ADMIN_PARALYSIS checks that used to sit here -- "used
		// nofight command, cannot hit anyone" and "cannot be hit by anyone" -- are gone along with
		// the /nofight command itself; see the note where Cmd_NoFight_f used to live in g_cmds.c.
		// The second of the two was the one that actually produced the reported invulnerability: it
		// made a player immune to every other player, indefinitely, with no way for them to switch
		// it back off once they had joined a team, and no indication anywhere that it was on.

		if (attacker->client->noclip == qtrue || target->client->noclip == qtrue)
		{ // zyk: noclip does not allow hitting
			return qfalse;
		}
	}

	return qtrue;
}


// GalaxyRP fix: [Magic] magic_sense() removed. The player-facing magic dispatch in
// Cmd_ForceUse_f()/the grab-anim block in g_cmds.c was deleted earlier as permanently
// unreachable (every power it could trigger was gated on pers.defeated_guardians or
// pers.universe_quest_progress, which nothing in the codebase ever wrote -- they sat at the zero
// ClientConnect's memset gives them. This note used to credit add_new_char() with writing them at
// character creation; that function has since been removed as dead too, see g_cmds.c. Both fields
// have since been removed from clientPersistant_t outright, for exactly that reason), and that
// deletion took magic_sense()'s only call site with it. Its siblings magic_shield() and
// magic_disable() survived for a while as zero-caller reference code (the quest_mage chain that
// used to call them went with the magic engine -- see the note in G_RunFrame) and have since been
// deleted with the rest of the effect functions;
// magic_explosion() has since gone the same way as magic_sense(), when the custom-quest-NPC block
// that was its last caller was removed. Nothing anywhere called magic_sense(). It also wrote
// pers.skill_levels[4] straight into forcePowerLevel[FP_SEE] with no amrpgmode guard while
// activating the power, which would have left a logged-out player at Sense level 0 with Force
// Sight switched on. Its two cvars (zyk_enable_magic_sense, zyk_magic_sense_mp_cost) are gone
// from g_xcvar.h with it.



// GalaxyRP fix: [Magic] zyk_vertical_dfa_effect() used to be here -- the Duelist Vertical DFA
// impact effect, which spawned an fx_runner with targetname "zyk_vertical_dfa". It had no callers
// anywhere in the tree: the Duelist RPG class that invoked it went with rpg_class, which is
// permanently 0. Nothing else in the tree creates that targetname, so the three sites that still
// test for it -- g_misc.c's fx_runner_link() and SP_fx_runner(), and g_combat.c's radius-damage
// exclusion list -- can no longer be reached; they are left alone here on purpose, to be judged
// alongside the rest of the magic-effect removal rather than swept up behind this one.



// GalaxyRP fix: [Magic] zyk_bomb_model_think() and zyk_add_bomb_model() used to be here -- a
// three-second timed bomb prop ("zyk_timed_bomb") that counted down and then spawned a
// "zyk_timed_bomb_explosion" effect. zyk_add_bomb_model() had no callers at all, and it was the
// only thing that ever set zyk_bomb_model_think as a think, so the pair was unreachable together.
// Nothing else creates either targetname; the one remaining test for the explosion, in
// g_combat.c's radius-damage exclusion list, is left in place with the rest of that block.

// GalaxyRP fix: [Magic] zyk_spawn_ice_element() used to be here -- it ringed a player with four
// crystal_wall models named "zyk_elemental_ice". Its last caller went with elemental_attack() in
// the custom-quest-NPC dispatch removal, as the note further down this file records. Nothing else
// creates or tests that targetname, so this one comes out clean.




extern void Jedi_DecloakPair(gentity_t *self);


// GalaxyRP fix: [Magic] zyk_force_dash_effect() and zyk_force_dash() used to be here, the effect
// and the animation half of the Fast Dash ability. Both were orphans: their only caller was
// zyk_do_force_dash() in g_active.c, which had no callers of its own and has been removed with
// them, and pers.fast_dash_timer -- written only by those two -- went with it. Nothing else
// creates the "zyk_effect_force_dash" targetname; the two sites in g_misc.c that still test for
// it are left alone here, with the rest of that list.

// DAJ_RP: [Flame Thrower] where the flame leaves the stun baton, and which way it goes: start is the player's
// right hand -- the stun baton's -- on the server's own copy of the player's model (players have "*r_hand" from
// spawn, and the saber code keeps that skeleton animated every frame: WP_SaberPositionUpdate()), pushed
// RP_FLAME_FROM_HAND along the aim to the baton's tip. Without a model or the bolt, the start is the stun baton's
// own point -- the one WP_FireStunBaton() traces from. angles is the effect's: the aim REVERSED, ( -pitch,
// yaw + 180, 0 ). boba/fthrw sends its fire against the direction it is given -- given the aim, it burnt
// straight behind the player -- so it is given the opposite of the aim, and the fire goes along it, up and
// down too. zyk's ( -pitch, roll, yaw - 180 ) was this reversal with the yaw and roll slots swapped.
#define RP_FLAME_FROM_HAND	12.0f

static void RP_FlameThrowerStart( gentity_t *self, vec3_t start, vec3_t angles )
{
	vec3_t	aim, forward, right;
	int		bolt = -1;

	VectorSet( aim, self->client->ps.viewangles[PITCH], self->client->ps.viewangles[YAW], 0.0f );
	AngleVectors( aim, forward, right, NULL );
	VectorSet( angles, AngleNormalize180( -aim[PITCH] ), AngleNormalize360( aim[YAW] + 180.0f ), 0.0f );

	if ( self->ghoul2 && trap->G2API_HaveWeGhoul2Models( self->ghoul2 ) )
	{
		bolt = trap->G2API_AddBolt( self->ghoul2, 0, "*r_hand" );	// the one it has: 0 on a player
	}

	if ( bolt >= 0 )
	{
		mdxaBone_t	boltMatrix;
		vec3_t		yaw;

		VectorSet( yaw, 0.0f, self->client->ps.viewangles[YAW], 0.0f );	// as the saber code turns it
		if ( trap->G2API_GetBoltMatrix( self->ghoul2, 0, bolt, &boltMatrix, yaw, self->client->ps.origin, level.time,
			NULL, self->modelScale ) )
		{
			BG_GiveMeVectorFromMatrix( &boltMatrix, ORIGIN, start );
			VectorMA( start, RP_FLAME_FROM_HAND, forward, start );
			return;
		}
	}

	VectorCopy( self->client->ps.origin, start );
	start[2] += self->client->ps.viewheight - 6;
	VectorMA( start, 20.0f, forward, start );
	VectorMA( start, 4.0f, right, start );
}

// zyk: fires the Boba Fett flame thrower
void Player_FireFlameThrower( gentity_t *self )
{
	trace_t		tr;
	gentity_t	*traceEnt = NULL;

	int entityList[MAX_GENTITIES];
	int numListedEntities;
	int e = 0;
	int damage = rp_flame_thrower_damage.integer;

	vec3_t	tfrom, tto, fwd;
	vec3_t thispush_org, a;
	vec3_t mins, maxs, fwdangles, forward, right, center;
	vec3_t		origin, angles;

	int i;
	float visionArc = 120;
	float radius = 144;

	self->client->cloakDebReduce = level.time + rp_flame_thrower_cooldown.integer;

	// DAJ_RP: [Flame Thrower] from the stun baton, and where the player aims (RP_FlameThrowerStart above). It
	// came from the torso (the origin + 20) and went the same way whatever way the player faced: the angles
	// were ( -pitch, roll, yaw - 180 ), and the client makes a direction of an effect's angles from its pitch
	// and yaw (AngleVectors, EV_PLAY_EFFECT_ID) -- the yaw slot held the roll, near 0, so every burst went along
	// the same world axis. The damage below was always the aim's; now the flame is where it lands. The angles
	// are the aim reversed, as the effect fires against its direction -- see RP_FlameThrowerStart().
	RP_FlameThrowerStart( self, origin, angles );

	if ((self->client->pers.flame_thrower - level.time) > 500)
		G_PlayEffectID( G_EffectIndex("boba/fthrw"), origin, angles);

	if ((self->client->pers.flame_thrower - level.time) > 1250)
		G_Sound( self, CHAN_WEAPON, G_SoundIndex("sound/effects/fire_lp") );

	//Check for a direct usage on NPCs first
	VectorCopy(self->client->ps.origin, tfrom);
	tfrom[2] += self->client->ps.viewheight;
	AngleVectors(self->client->ps.viewangles, fwd, NULL, NULL);
	tto[0] = tfrom[0] + fwd[0]*radius/2;
	tto[1] = tfrom[1] + fwd[1]*radius/2;
	tto[2] = tfrom[2] + fwd[2]*radius/2;

	trap->Trace( &tr, tfrom, NULL, NULL, tto, self->s.number, MASK_PLAYERSOLID, qfalse, 0, 0 );

	VectorCopy( self->client->ps.viewangles, fwdangles );
	AngleVectors( fwdangles, forward, right, NULL );
	VectorCopy( self->client->ps.origin, center );

	for ( i = 0 ; i < 3 ; i++ ) 
	{
		mins[i] = center[i] - radius;
		maxs[i] = center[i] + radius;
	}

	numListedEntities = trap->EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	while (e < numListedEntities)
	{
		traceEnt = &g_entities[entityList[e]];

		if (traceEnt)
		{ //not in the arc, don't consider it
			if (traceEnt->client)
			{
				VectorCopy(traceEnt->client->ps.origin, thispush_org);
			}
			else
			{
				VectorCopy(traceEnt->s.pos.trBase, thispush_org);
			}

			VectorCopy(self->client->ps.origin, tto);
			tto[2] += self->client->ps.viewheight;
			VectorSubtract(thispush_org, tto, a);
			vectoangles(a, a);

			if (!InFieldOfVision(self->client->ps.viewangles, visionArc, a))
			{ //only bother with arc rules if the victim is a client
				entityList[e] = ENTITYNUM_NONE;
			}
		}
		traceEnt = &g_entities[entityList[e]];
		if (traceEnt && traceEnt != self)
		{
			G_Damage( traceEnt, self, self, self->client->ps.viewangles, tr.endpos, damage, DAMAGE_NO_KNOCKBACK|DAMAGE_IGNORE_TEAM, MOD_LAVA );
		}
		e++;
	}
}

// GalaxyRP fix: [Magic] clear_special_power_effect() used to be here. G_RunFrame called it on every
// live entity every frame; when level.special_power_effects[] held an owner for that slot and the
// matching timer had expired it cleared the slot and freed the effect entity (immediately for a
// misc_model_breakable, next think otherwise). It was the ONLY thing that ever freed an entity
// spawned by a magic power, and the only thing besides the init loop that ever wrote -1.
//
// It went with the arrays. Nothing produces those entries any more, so the function had become a
// per-entity, per-frame read of two ints out of a 32 KB structure that could only ever answer
// "no". Its removal also ends a latent hazard: G_FreeEntity() never reset the array, so an effect
// entity freed by any other path before its timer expired left a live owner id behind, and the
// next entity to take that slot number would have been treated as that player's magic effect.

// GalaxyRP fix: [Magic] magic_master_has_this_power(), zyk_print_special_power(),
// zyk_number_of_enabled_magic_powers(), and zyk_show_magic_master_powers()/
// zyk_show_left_magic_master_powers()/zyk_show_right_magic_master_powers() removed outright.
// This whole block gated and drove a "select which magic power occupies your left/right/main
// slot" system (sess.selected_special_power/selected_left_special_power/selected_right_special_power)
// that turned out to have zero live callers anywhere in the codebase -- no keybind, no console
// command, nothing ever invoked the three zyk_show_*_magic_master_powers() functions, so a
// player's selected slots could never move off their MAGIC_MAGIC_SENSE login default. Separately,
// the function that actually fires a magic power (TryGrapple() in g_cmds.c, via the grapple-hook
// key) never read any of those three selected-power fields either -- it only ever hardcodes
// MAGIC_ULTRA_STRENGTH/MAGIC_ULTRA_RESISTANCE/MAGIC_ENEMY_WEAKENING by movement direction (see the
// matching fix comment there), so magic_master_has_this_power()'s guardian-quest gating for the
// other ~27 powers was never actually consulted by anything reachable either. With all six
// functions confirmed to have no other callers (grep'd across the whole tree), removed the entire
// dead selection/gating layer rather than leave it half-connected. The individual magic power effect
// functions (water_splash(), earthquake(), etc.) were left untouched and are simply uncalled.
// This note used to add that defeated_guardians "has plenty of other live uses elsewhere"; that was
// already wrong when written -- the field had no live reader at all -- and it has since been removed
// from clientPersistant_t as declaration-only.


// zyk: controls the quest powers stuff
// GalaxyRP fix: [Magic] an extern for initialize_rpg_skills() used to be here. Its last caller in
// this file was the bit-10 Resurrection Power branch of quest_power_events(), removed just below;
// the function itself lives on with a caller in g_cmds.c.
extern void zyk_wind_down_seeker_drone(gentity_t *ent);
// GalaxyRP fix: [Minigames] both mini-game prepares call this before their loadout snapshot.
extern void zyk_release_mounts_for_minigame(gentity_t *ent);
// GalaxyRP: [Sniper Battle] the zyk_apply_character_loadout() declaration that sat here went with
// the removal -- sniper_battle_end() was this file's only caller.
// GalaxyRP fix: [Magic] quest_power_events() used to be here. It ran every frame for every client
// and every NPC, and by the end it held one reachable statement: clearing quest_power_status bit 0
// (Immunity Power) once quest_power1_timer had passed. The twenty-one status blocks around it had
// already gone with the magic engine.
//
// Bit 0's only remaining producer was duel_tournament_prepare(), and its only reader,
// zyk_check_immunity_power(), has been unreachable since the effect functions lost their caller --
// so the bit was written and cleared but never consulted. Rather than leave a per-frame call for
// that, the whole bit-0 pair went with this function: the set in prepare() and the clear in
// duel_tournament_restore_duelist(). quest_power1_timer, which had no other site, went too.
//
// If the magic system is ever revived, note that the set and the clear are a PAIR -- see the
// history kept on duel_tournament_restore_duelist() below for the bug that appears if one comes
// back without the other.

// zyk: damages target player with poison hits
void poison_dart_hits(gentity_t *ent)
{
	if (ent && ent->client && ent->health > 0 && ent->client->pers.player_statuses & (1 << PLAYER_STATUS_POISON_DART_HIT) && ent->client->pers.poison_dart_hit_counter > 0 && 
		ent->client->pers.poison_dart_hit_timer < level.time)
	{
		gentity_t *poison_user = &g_entities[ent->client->pers.poison_dart_user_id];

		G_Damage(ent,poison_user,poison_user,NULL,NULL,5,0,MOD_UNKNOWN);

		ent->client->pers.poison_dart_hit_counter--;
		ent->client->pers.poison_dart_hit_timer = level.time + 200;

		// zyk: no more do poison damage if counter is 0
		if (ent->client->pers.poison_dart_hit_counter == 0)
			ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_POISON_DART_HIT);
	}
}

// GalaxyRP fix: [Quests] first_second_act_objective, zyk_spawn_catwalk_prison,
// zyk_spawn_quest_reborns, zyk_validate_sages, universe_quest_artifacts_checker,
// universe_crystals_check, and zyk_try_get_dark_quest_note used to live here. All were part of the
// general (non-Guardian/Bounty) automated Universe/Dark Quest machinery driven by
// choose_new_player/quest_get_new_player (g_cmds.c) and the dead Touch_Item block in g_items.c, which
// are themselves fully unreachable now that quest_get_new_player's sole gate is permanently disabled
// (see the GalaxyRP fix comment on quest_get_new_player's old location in g_cmds.c). Deleted outright,
// along with their call sites in g_items.c.

// zyk: backup player force powers
extern void WP_EndForceEnlightenment( gentity_t *self );

void player_backup_force(gentity_t *ent)
{
	int i = 0;

	// GalaxyRP fix: [Duel Tournament] first backup wins until it has been restored. Backing up
	// again while one is outstanding would capture the ALREADY-STRIPPED state and the player
	// would never get their powers back. Nothing calls prepare twice per match today; this makes
	// that a property of the pair rather than of the caller.
	if (ent->client->pers.zyk_saved_force_valid == qtrue)
		return;

	ent->client->pers.zyk_saved_force_powers = ent->client->ps.fd.forcePowersKnown;

	for (i = 0; i < NUM_FORCE_POWERS; i++)
	{
		ent->client->pers.zyk_saved_force_power_levels[i] = ent->client->ps.fd.forcePowerLevel[i];
	}

	ent->client->pers.zyk_saved_force_valid = qtrue;
}

// zyk: backup the loadout duel_tournament_prepare() is about to take away
//
// GalaxyRP fix: [Duel Tournament] see the field declarations in g_local.h. Must be called BEFORE
// prepare strips anything -- unlike the force backup, which sits mid-function because the force
// strip comes later, the weapon/ammo/holdable strip is the first thing prepare does.
void player_backup_loadout(gentity_t *ent)
{
	int i = 0;

	if (ent->client->pers.zyk_saved_loadout_valid == qtrue)
		return;

	ent->client->pers.zyk_saved_weapons = ent->client->ps.stats[STAT_WEAPONS];

	for (i = 0; i < MAX_AMMO; i++)
	{
		ent->client->pers.zyk_saved_ammo[i] = ent->client->ps.ammo[i];
	}

	ent->client->pers.zyk_saved_holdable_items = ent->client->ps.stats[STAT_HOLDABLE_ITEMS];
	ent->client->pers.zyk_saved_holdable_item = ent->client->ps.stats[STAT_HOLDABLE_ITEM];

	ent->client->pers.zyk_saved_loadout_valid = qtrue;
}

// zyk: give back the loadout duel_tournament_prepare() took
void player_restore_loadout(gentity_t *ent)
{
	int i = 0;

	if (ent->client->pers.zyk_saved_loadout_valid == qfalse)
	{ // zyk: nothing was ever taken from this player
		return;
	}

	ent->client->pers.zyk_saved_loadout_valid = qfalse;

	if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS))
	{ // zyk: he died in his duel, so ClientSpawn already gave him a loadout. Putting the pre-duel
	  // one back over the top would undo the respawn and take away anything picked up since.
		return;
	}

	// GalaxyRP fix: [Duel Tournament] melee is OR'd back in rather than left to the saved value,
	// so this cannot undo the (1 << WP_MELEE) player_restore_force() grants regardless of which
	// of the two runs first.
	ent->client->ps.stats[STAT_WEAPONS] = ent->client->pers.zyk_saved_weapons | (1 << WP_MELEE);

	for (i = 0; i < MAX_AMMO; i++)
	{
		ent->client->ps.ammo[i] = ent->client->pers.zyk_saved_ammo[i];
	}

	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] = ent->client->pers.zyk_saved_holdable_items;
	ent->client->ps.stats[STAT_HOLDABLE_ITEM] = ent->client->pers.zyk_saved_holdable_item;

	// zyk: prepare left him holding the saber it granted. If he had no saber before the duel,
	// that bit has just gone away again and he would be holding a weapon he does not own.
	if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << ent->client->ps.weapon)))
	{
		ent->client->ps.weapon = WP_MELEE;
		ent->s.weapon = WP_MELEE;
	}
}

// zyk: restore player force powers
void player_restore_force(gentity_t *ent)
{
	int i = 0;

	if (ent->client->pers.zyk_saved_force_valid == qfalse)
	{ // zyk: nothing was ever backed up for this player -- see the field declaration in g_local.h
		return;
	}

	ent->client->pers.zyk_saved_force_valid = qfalse;

	if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS))
	{ // zyk: do not restore force to players that died in a Duel Tournament duel, because the force was already restored
		return;
	}

	ent->client->ps.fd.forcePowersKnown = ent->client->pers.zyk_saved_force_powers;

	for (i = 0; i < NUM_FORCE_POWERS; i++)
	{
		ent->client->ps.fd.forcePowerLevel[i] = ent->client->pers.zyk_saved_force_power_levels[i];
	}

	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
}

// GalaxyRP fix: [Minigames] drop a pending backup without applying it.
//
// The backup/restore pair is first-wins on the way in and consume-on-use on the way out, so a
// backup that is taken and never consumed is worse than no backup at all: the NEXT
// player_backup_loadout() sees the valid flag still set and returns early, keeping the stale
// snapshot, and the next restore then hands the player a loadout from a battle they left long ago
// -- or, because the same two functions serve both mini-games, from the other mini-game entirely.
//
// This is for the exits that re-equip the player by themselves and so must not have the snapshot
// applied: dying, and being dropped from a battle by ClientBegin. Both come back through
// ClientSpawn(), which rebuilds stats[STAT_WEAPONS] from scratch (see the WP_NONE reset there), so
// writing the snapshot first would only be overwritten a moment later.
//
// ClientDisconnect deliberately does NOT call this: ClientConnect memsets the whole gclient_t
// before it does anything else, so the flags cannot survive into the next occupant of the slot,
// and adding a call there would be dead code that reads like a live requirement.
void player_discard_backup(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	ent->client->pers.zyk_saved_force_valid = qfalse;
	ent->client->pers.zyk_saved_loadout_valid = qfalse;
}

// GalaxyRP fix: [Duel Tournament] everything duel_tournament_prepare() applied, undone in one
// place, so the two call sites in the mode-5 block below cannot drift apart.
//
// HISTORY, worth keeping because it is a trap to fall into twice. prepare() used to set
// quest_power_status bit 0 -- the Immunity Power, which zyk_check_immunity_power() tests to block
// quest and magic damage -- along with quest_power1_timer, the duel's SCHEDULED end. Nothing
// cleared the bit when a duel finished early, and the only other clear ran once that original
// deadline passed, so a duelist who won at ten seconds of a sixty-second match kept magic immunity
// for the remaining fifty -- through the score screen, the next pairing, and into their next duel
// or open play. Losers were covered by accident (player_die zeroes the whole of
// quest_power_status) and so was anyone who spectated (ClientSpawn does the same); only the
// survivor leaked it, which is exactly the player it most advantaged. The clear added here fixed
// that.
//
// Both halves have since gone: bit 0 lost its last reader when the magic effect functions lost
// theirs, so the set in prepare(), the clear below and quest_power1_timer were all removed. If the
// immunity is ever reintroduced, reintroduce the SET AND THE CLEAR TOGETHER -- one without the
// other is the bug described above.
void duel_tournament_restore_duelist(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	player_restore_force(ent);
	player_restore_loadout(ent);
}

// zyk: finished the duel tournament
void duel_tournament_end()
{
	int i = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		level.duel_players[i] = -1;
	}

	for (i = 0; i < MAX_DUEL_MATCHES; i++)
	{
		level.duel_matches[i][0] = -1;
		level.duel_matches[i][1] = -1;
		level.duel_matches[i][2] = 0;
		level.duel_matches[i][3] = 0;
	}

	if (level.duel_tournament_model_id != -1)
	{
		G_FreeEntity(&g_entities[level.duel_tournament_model_id]);
		level.duel_tournament_model_id = -1;
	}

	level.duel_tournament_mode = 0;
	level.duel_tournament_rounds = 0;
	level.duelists_quantity = 0;
	level.duel_matches_quantity = 0;
	level.duel_matches_done = 0;
	level.duelist_1_id = -1;
	level.duelist_2_id = -1;

	// GalaxyRP fix: [Duel Tournament] the paused flag was the one piece of tournament state this
	// function did not reset, and nothing else clears it except a map change. Because Cmd_DuelPause_f
	// refuses to run while no tournament exists ("There is no duel tournament now"), a tournament
	// that was paused and then ended -- e.g. paused during signup, then everyone left -- left the flag
	// stuck on with no way to clear it. Every later tournament on that map then sat in signup forever:
	// the mode-1 start transition lives inside the "not paused" guard, so it simply never fired, with
	// no message to explain why. Ending a tournament now leaves no state behind for the next one.
	level.duel_tournament_paused = qfalse;
}

// zyk: prepare duelist for duel
void duel_tournament_prepare(gentity_t *ent)
{
	int i = 0;

	// GalaxyRP fix: [Minigames] off any e-web or map emplaced gun first. While a player is mounted,
	// STAT_WEAPONS holds the gun and not their weapons, so the snapshot below would record the
	// wrong thing -- and a live e-web would keep writing over the saber handed out below. See
	// zyk_release_mounts_for_minigame() in g_cmds.c.
	zyk_release_mounts_for_minigame(ent);

	// GalaxyRP fix: [Duel Tournament] straight after the release above, before the strip below
	// takes the weapons, ammo and holdable items away. player_backup_force() further down is
	// placed the same way relative to the force strip that follows it.
	player_backup_loadout(ent);

	for (i = WP_STUN_BATON; i < WP_NUM_WEAPONS; i++)
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << i);
	}

	ent->client->ps.ammo[AMMO_BLASTER] = 0;
	ent->client->ps.ammo[AMMO_POWERCELL] = 0;
	ent->client->ps.ammo[AMMO_METAL_BOLTS] = 0;
	ent->client->ps.ammo[AMMO_ROCKETS] = 0;
	ent->client->ps.ammo[AMMO_THERMAL] = 0;
	ent->client->ps.ammo[AMMO_TRIPMINE] = 0;
	ent->client->ps.ammo[AMMO_DETPACK] = 0;
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] = (1 << HI_NONE);
	ent->client->ps.stats[STAT_HOLDABLE_ITEM] = 0;

	// zyk: removing the seeker drone in case if is activated
	// GalaxyRP fix: [Items] through the shared helper, which clamps one millisecond further. The
	// clamp that used to sit here landed on the far EDGE of SeekerDroneUpdate()'s wind-down window,
	// whose upper bound is strict -- so the drone stayed armed for the rest of this frame, and
	// WP_ForcePowersUpdate() runs later in this same G_RunFrame() call. The duelists are teleported
	// into the arena a few lines after this returns, so that frame is precisely the one in which a
	// stray seeker bolt lands in a saber duel. See zyk_wind_down_seeker_drone() in g_cmds.c.
	zyk_wind_down_seeker_drone(ent);

	// GalaxyRP fix: [Cloak Item] pair-aware, like every other decloak trigger -- a duelist prepped
	// while paired-cloaked used to leave the vehicle cloaked behind them.
	Jedi_DecloakPair(ent);

	// zyk: disable jetpack
	Jetpack_Off(ent);

	ent->client->ps.jetpackFuel = 100;
	ent->client->pers.jetpack_fuel = MAX_JETPACK_FUEL;

	// zyk: giving saber to the duelist
	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);
	ent->client->ps.weapon = WP_SABER;
	ent->s.weapon = WP_SABER;

	// GalaxyRP fix: [Magic] an Immunity Power set used to be here -- quest_power_status bit 0 plus
	// quest_power1_timer -- so that every magic status effect on the duelist was cancelled. The
	// blocks that did the cancelling went with the magic engine and bit 0 had no reader left, so
	// the set and its matching clear in duel_tournament_restore_duelist() were removed together.

	// zyk: reset hp and shield of duelist
	ent->health = 100;
	ent->client->ps.stats[STAT_ARMOR] = 100;

	// DAJ_RP: [Force Enlightenment] end any Enlightenment BEFORE the Force is saved. Saved while
	// boosted, the backup held the boosted levels; the powerup clear further down then left the
	// "boosted" flag behind, the next frame undid the boost mid-duel, and after the duel
	// player_restore_force() handed the boosted levels back with nothing left to ever undo them -- a
	// logged-out duelist kept level 3 in every power of their side until their next respawn. See
	// WP_EndForceEnlightenment() (w_force.c).
	WP_EndForceEnlightenment(ent);

	player_backup_force(ent);

	for (i = 0; i < NUM_FORCE_POWERS; i++)
	{
		if (i != FP_LEVITATION && i != FP_SABER_OFFENSE && i != FP_SABER_DEFENSE)
		{ // zyk: cannot use any force powers, except Jump, Saber Attack and Saber Defense
			if ((ent->client->ps.fd.forcePowersActive & (1 << i)))
			{//turn it off
				WP_ForcePowerStop(ent, (forcePowers_t)i);
			}

			ent->client->ps.fd.forcePowersKnown &= ~(1 << i);
			ent->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_0;
			ent->client->ps.fd.forcePowerDuration[i] = 0;
		}
	}

	// zyk: removing powerups
	ent->client->ps.powerups[PW_FORCE_BOON] = 0;
	ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = 0;
	ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = 0;

	// zyk: removing flag that is used to test if player died in a duel
	ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS);

	// zyk: stop any movement
	VectorSet(ent->client->ps.velocity, 0, 0, 0);
}

// zyk: generate the teams and validates them
int duel_tournament_generate_teams()
{
	// GalaxyRP: [Duel Tournament] this used to validate the 2v2 pairings in level.duel_allies[] and
	// then subtract one from the count for each pair, so a team of two entered as a single
	// competitor. With 2v2 gone (see G_TryUse in g_utils.c) every entry was permanently -1, both
	// loops were no-ops and the count always came back as the raw number of duelists. Kept as a
	// function, rather than folded into its one caller, because duel_number_of_teams is what
	// duel_tournament_generate_match_table() sizes its quota from.
	level.duel_number_of_teams = level.duelists_quantity;

	return level.duel_number_of_teams;
}

// zyk: generates the table with all the tournament matches
void duel_tournament_generate_match_table()
{
	int i = 0;
	int last_opponent_id = -1;
	int number_of_filled_positions = 0;
	int max_filled_positions = level.duel_number_of_teams - 1; // zyk: used to fill the player in current iteration in the table. It will always be the number of duelists (or teams) minus one
	int temp_matches[MAX_DUEL_MATCHES][2];
	int temp_remaining_matches = 0;

	level.duel_matches_quantity = 0;
	level.duel_matches_done = 0;

	for (i = 0; i < MAX_DUEL_MATCHES; i++)
	{ // zyk: initializing temporary array of matches
		temp_matches[i][0] = -1;
		temp_matches[i][1] = -1;
	}

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		int j = 0;

		if (level.duel_players[i] != -1)
		{ // zyk: player joined the tournament
			last_opponent_id = -1;

			// GalaxyRP fix: [Duel Tournament] number_of_filled_positions used to be reset ONLY by
			// the break below, i.e. only when this player managed to fill its full quota of
			// positions. With the maximum 32 duelists the array is exactly full (32 players x 31
			// opponents = 992 = 2 x MAX_DUEL_MATCHES), so the last players run the j loop to
			// completion without ever hitting that break -- the counter then carried over into the
			// next player, which stopped early and left matches with a first duelist but no second.
			// Those half-filled entries sat inside the counted range, so the tournament played
			// matches whose second duelist id was -1: an out-of-bounds entity index and a match nobody
			// could win. Resetting per player instead makes the counter's state independent of how
			// the loop below terminates. Verified by simulation:
			// with exactly 32 duelists this now produces the complete 496-match round robin with no
			// holes and no repeated pairings, and every smaller tournament is unchanged.
			number_of_filled_positions = 0;

			for (j = 0; j < MAX_DUEL_MATCHES; j++)
			{
				if (number_of_filled_positions >= max_filled_positions)
				{
					break;
				}

				if (temp_matches[j][0] == -1)
				{
					temp_matches[j][0] = i;
					number_of_filled_positions++;
				}
				else if (temp_matches[j][1] == -1 && last_opponent_id != temp_matches[j][0])
				{ // zyk: will not face the same opponent again (last_opponent_id)
					last_opponent_id = temp_matches[j][0];
					temp_matches[j][1] = i;
					number_of_filled_positions++;
					level.duel_matches_quantity++;
				}
			}
		}
	}

	temp_remaining_matches = level.duel_matches_quantity;

	// zyk: generating the ramdomized array with the matches of the tournament
	for (i = 0; i < level.duel_matches_quantity; i++)
	{
		int duel_chosen_index = Q_irand(0, (temp_remaining_matches - 1));
		int j = 0;

		level.duel_matches[i][0] = temp_matches[duel_chosen_index][0];
		level.duel_matches[i][1] = temp_matches[duel_chosen_index][1];
		level.duel_matches[i][2] = 0;
		level.duel_matches[i][3] = 0;

		for (j = (duel_chosen_index + 1); j < temp_remaining_matches; j++)
		{ // zyk: updating the match table to move all duels after the duel_chosen_index one index lower
			temp_matches[j - 1][0] = temp_matches[j][0];
			temp_matches[j - 1][1] = temp_matches[j][1];
		}

		temp_remaining_matches--;
	}
}

// zyk: gives prize to the winner
void duel_tournament_prize(gentity_t *ent)
{
	if (ent->health < 1)
	{ // zyk: if he is dead, respawn him so he can receive his prize
		ClientRespawn(ent);
	}

	ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 40000;

	if (ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE)
	{
		ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 40000;
	}
	else if (ent->client->ps.fd.forceSide == FORCE_DARKSIDE)
	{
		ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = level.time + 40000;
	}
	
	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL) | (1 << WP_BLASTER) | (1 << WP_DISRUPTOR) | (1 << WP_REPEATER);
	ent->client->ps.ammo[AMMO_BLASTER] = rp_max_blaster_pack_ammo.integer;
	ent->client->ps.ammo[AMMO_POWERCELL] = rp_max_power_cell_ammo.integer;
	ent->client->ps.ammo[AMMO_METAL_BOLTS] = rp_max_metal_bolt_ammo.integer;
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN) | (1 << HI_SEEKER) | (1 << HI_MEDPAC_BIG);

	ent->client->ps.jetpackFuel = 100;
	ent->client->pers.jetpack_fuel = MAX_JETPACK_FUEL;

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));
}

// DAJ_RP: [Duel Tournament] the Duel Tournament leaderboard (GalaxyRP/leaderboard.txt, written here
// and read by /duelboard) is gone. It only ever recorded a winner who was logged in, and /duelmode
// refuses logged-in players while /login, /new and /char are refused to anyone in the tournament, so
// no winner could ever reach it: the file was never written and /duelboard always said "No leaderboard
// yet". Removed with the command, its five-step writer in G_RunFrame() and its level fields.

// zyk: determines who is the tournament winner
// GalaxyRP: [Race Mode] the add_credits() declaration that sat here went with the removal -- the
// race prize block in G_RunFrame() was this file's only caller.
// GalaxyRP fix: [Duel Tournament] forward declaration -- duel_tournament_valid_duelist() is
// defined further down this file and duel_tournament_winner() now asks it who is still eligible.
extern qboolean duel_tournament_valid_duelist(gentity_t *ent);
void duel_tournament_winner()
{
	gentity_t *ent = NULL;
	int max_score = -1;
	int i = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		// GalaxyRP fix: [Duel Tournament] this used to test only "is a member" (duel_players[i]
		// != -1) and then dereference g_entities[i].client unguarded, while every other consumer
		// in the feature asks duel_tournament_valid_duelist() -- connected, not spectating, still
		// a member. The gap mattered: SetTeam() only clears membership through ClientBegin(), and
		// it skips that call while entities are being placed (level.load_entities_timer != 0), so
		// a player who went to spectator during an /entload stayed a member. They could then be
		// declared winner, have duel_tournament_prize() call ClientRespawn() on them mid-spectate,
		// and have sess.amrpgmode read through a client pointer nothing had checked. Asking the
		// feature's own validity test makes the winner agree with every match that was played.
		if (duel_tournament_valid_duelist(&g_entities[i]) == qtrue)
		{
			if (level.duel_players[i] > max_score)
			{ // zyk: player is in tournament and his score is higher than max_score, so for now he is the max score
				max_score = level.duel_players[i];
				ent = &g_entities[i];
			}
			else if (level.duel_players[i] == max_score && ent && level.duel_players_hp[i] > level.duel_players_hp[ent->s.number])
			{ // zyk: this guy has the same score as the max score guy. Test the remaining hp of all duels to untie
				ent = &g_entities[i];
			}
		}
	}

	if (ent)
	{ // zyk: found a winner
		char winner_info[128];

		duel_tournament_prize(ent);

		strcpy(winner_info, ent->client->pers.netname);

		trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7Winner is: %s^7. Prize: force power-ups, some guns and items\"", winner_info));
	}
	else
	{
		trap->SendServerCommand(-1, "chat \"^3Duel Tournament: ^7No one is winner\"");
	}
}

// zyk: returns the amount of hp and shield in a string, it is the total hp and shield of a team or single duelist in Duel Tournament
char *duel_tournament_remaining_health(gentity_t *ent)
{
	// GalaxyRP fix: [Leak] this used to end in G_NewString(health_info). G_Alloc's 4 MB pool is
	// never freed, and this runs once or twice per match end -- a 32-duelist tournament is 496
	// matches, so roughly 16-32 KB per tournament, and exhausting the pool calls
	// trap->Error(ERR_DROP), which is a process exit on a dedicated server.
	//
	// The buffer rotates, and it has to: the tie branch below calls this function TWICE inside a
	// single va() argument list. A single static buffer would hand both arguments the same
	// pointer, so both duelists would print the second one's health. Four slots is the same
	// trick va() itself uses, and leaves headroom for any future caller.
	static char health_buffers[4][128];
	static int  health_index = 0;
	char *health_info = health_buffers[health_index];

	health_index = (health_index + 1) % 4;

	// GalaxyRP fix: [Duel Tournament] callers can now legitimately pass NULL for a match slot that
	// holds no valid duelist (see duel_tournament_set_match_winner), and every line below reads
	// ent->s.number / ent->client. Return an empty health string rather than crashing.
	if (!ent || !ent->client)
	{
		return "";
	}

	health_info[0] = '\0';

	if (level.duel_tournament_mode == 4)
	{
		if (!(ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))
		{ // zyk: show health if the player did not die in duel
			Com_sprintf(health_info, sizeof(health_buffers[0]), " ^1%d^7/^2%d^7 ", ent->health, ent->client->ps.stats[STAT_ARMOR]);
		}

	}

	return health_info;
}

// zyk: sums the score and hp score to a single duelist or to a team
void duel_tournament_give_score(gentity_t *ent, int score)
{
	// GalaxyRP fix: [Duel Tournament] this used to dereference ent unconditionally and then do
	// "level.duel_players[ent->s.number] += score" with no check on the current value. Two problems.
	// First, its callers can legitimately pass NULL now that a match slot holding -1 no longer
	// produces a bogus entity pointer. Second, and worse, -1 in duel_players[] is the sentinel for
	// "not in the tournament" (a player who spectated, disconnected or was never signed up), so
	// adding a score to it turned -1 back into 0 -- i.e. silently re-enrolled them as a member,
	// without duelists_quantity being incremented to match. That happens on the "both teams invalid"
	// tie path, and the resurrected player then reappears in /dueltable, stays eligible to win the
	// whole tournament, and on their eventual real disconnect decrements duelists_quantity a second
	// time, driving it negative so the "everyone left" check can never end the tournament again.
	if (!ent || !ent->client || level.duel_players[ent->s.number] == -1)
	{
		return;
	}

	level.duel_players[ent->s.number] += score;
	if (level.duel_tournament_mode == 4 && !(ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))
	{ // zyk: add hp score if he did not die in duel
		level.duel_players_hp[ent->s.number] += (ent->health + ent->client->ps.stats[STAT_ARMOR]);
	}
}

// zyk: sets the winner of the Duel Tournament match
void duel_tournament_set_match_winner(gentity_t *winner)
{
	gentity_t *first_duelist = NULL;
	gentity_t *second_duelist = NULL;

	// GalaxyRP fix: [Duel Tournament] these two used to be initialised unconditionally as
	// &g_entities[level.duelist_N_id]. Both ids legitimately hold -1 (they are reset to it between
	// matches, and a malformed match table can carry -1 into a match -- see the packing fix in
	// duel_tournament_generate_match_table), and &g_entities[-1] is an out-of-bounds pointer to
	// whatever sits before the entity array. It is not NULL, so the "ent && ent->client" guards
	// further down accept it and then read a client pointer out of unrelated memory. The ally ids
	// immediately below were always guarded this way; the duelist ids themselves were not.
	if (level.duelist_1_id != -1)
	{
		first_duelist = &g_entities[level.duelist_1_id];
	}

	if (level.duelist_2_id != -1)
	{
		second_duelist = &g_entities[level.duelist_2_id];
	}

	// zyk: setting score
	if (winner && level.duel_matches[level.duel_matches_done][0] == winner->s.number)
	{ // zyk: first duelist won
		level.duel_matches[level.duel_matches_done][2]++;

		duel_tournament_give_score(winner, 3);
	}
	else if (winner && level.duel_matches[level.duel_matches_done][1] == winner->s.number)
	{ // zyk: second duelist won
		level.duel_matches[level.duel_matches_done][3]++;

		duel_tournament_give_score(winner, 3);
	}
	else
	{ // zyk: round tied
		duel_tournament_give_score(first_duelist, 1);
		duel_tournament_give_score(second_duelist, 1);
	}

	// zyk: showing round win message
	if (winner)
	{
		trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7%s ^7wins! %s\"", winner->client->pers.netname, duel_tournament_remaining_health(winner)));
	}
	else
	{
		trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7Tie! %s %s\"", duel_tournament_remaining_health(first_duelist), duel_tournament_remaining_health(second_duelist)));
	}

	// zyk: this match ended. Set this variable to get next match
	level.duel_matches_done++;
}

void duel_tournament_protect_duelists(gentity_t *duelist_1, gentity_t *duelist_2)
{
	duelist_1->client->ps.eFlags |= EF_INVULNERABLE;
	duelist_1->client->invulnerableTimer = level.time + DUEL_TOURNAMENT_PROTECT_TIME;

	duelist_2->client->ps.eFlags |= EF_INVULNERABLE;
	duelist_2->client->invulnerableTimer = level.time + DUEL_TOURNAMENT_PROTECT_TIME;
}

qboolean duel_tournament_valid_duelist(gentity_t *ent)
{
	if (ent && ent->client && ent->client->pers.connected == CON_CONNECTED &&
		ent->client->sess.sessionTeam != TEAM_SPECTATOR && level.duel_players[ent->s.number] != -1)
	{ // zyk: valid player
		return qtrue;
	}

	return qfalse;
}

// zyk: validates duelists in Duel Tournament
qboolean duel_tournament_validate_duelists()
{
	// GalaxyRP fix: [Duel Tournament] same out-of-bounds initialisation fixed in
	// duel_tournament_set_match_winner above -- &g_entities[-1] when a duelist id holds the -1
	// "no duelist" sentinel. duel_tournament_valid_duelist() below already treats a NULL entity as
	// invalid, which is exactly the right answer for an empty slot; it could not do that for a
	// pointer into out-of-bounds memory, which is non-NULL and passes its "ent && ent->client" test.
	gentity_t *first_duelist = (level.duelist_1_id != -1) ? &g_entities[level.duelist_1_id] : NULL;
	gentity_t *second_duelist = (level.duelist_2_id != -1) ? &g_entities[level.duelist_2_id] : NULL;
	qboolean first_valid = qfalse;
	qboolean second_valid = qfalse;

	// zyk: removing duelists from private duels
	// GalaxyRP fix: [Duel Tournament] NULL-guarded -- these now hold NULL for an empty match slot
	// instead of an out-of-bounds pointer, so they have to be checked before being dereferenced.
	// GalaxyRP fix: [Death System] both are bookkeeping, not deaths -- see g_bookkeepingDeath in
	// g_local.h. This is the tournament getting a private duel out of the way before its own match
	// starts; neither duelist lost anything, and neither should be charged for it. The mini-game
	// deaths that ARE deaths -- falling off the Melee catwalk, leaving the arena mid-match -- are
	// deliberately left counting.
	if (first_duelist && first_duelist->client->ps.duelInProgress == qtrue)
	{
		first_duelist->client->ps.stats[STAT_HEALTH] = first_duelist->health = -999;

		g_bookkeepingDeath = qtrue;
		player_die(first_duelist, first_duelist, first_duelist, 100000, MOD_SUICIDE);
		g_bookkeepingDeath = qfalse;
	}

	if (second_duelist && second_duelist->client->ps.duelInProgress == qtrue)
	{
		second_duelist->client->ps.stats[STAT_HEALTH] = second_duelist->health = -999;

		g_bookkeepingDeath = qtrue;
		player_die(second_duelist, second_duelist, second_duelist, 100000, MOD_SUICIDE);
		g_bookkeepingDeath = qfalse;
	}

	// zyk: testing if duelists are still valid
	// GalaxyRP fix: [Death System] a duelist who is downed when their match comes up forfeits it.
	// The join guard refuses a downed player, but nothing re-checked between signing up and being
	// picked, so anyone knocked down while waiting was teleported into the arena still downed --
	// health reset to 100 by duel_tournament_prepare() but unable to move or attack -- and lost
	// helplessly. Being invalid here routes them through the path a duelist who left or spectated
	// already takes: the opponent is given the match, duel_matches_done advances, and the
	// tournament moves on with them still in it for their remaining matches.
	//
	// Tested here rather than inside duel_tournament_valid_duelist(): that function is also what
	// duel_tournament_winner() uses to pick the overall winner, and a player who happens to be
	// downed at that instant must not be disqualified from winning the whole tournament.
	first_valid = duel_tournament_valid_duelist(first_duelist) == qtrue &&
		G_PlayerIsDowned(first_duelist) == qfalse;
	second_valid = duel_tournament_valid_duelist(second_duelist) == qtrue &&
		G_PlayerIsDowned(second_duelist) == qfalse;

	if (first_valid == qtrue && second_valid == qtrue)
	{ // zyk: valid match
		return qtrue;
	}

	if (first_valid == qtrue && second_valid == qfalse)
	{ // zyk: only the first duelist is valid. Gives score to him
		duel_tournament_set_match_winner(first_duelist);
	}
	else if (second_valid == qtrue && first_valid == qfalse)
	{ // zyk: only the second duelist is valid. Gives score to him
		duel_tournament_set_match_winner(second_duelist);
	}
	else
	{ // zyk: both teams invalid
		duel_tournament_set_match_winner(NULL);
	}

	return qfalse;
}

// GalaxyRP: [Sniper Battle] sniper_battle_end(), sniper_battle_prepare() and sniper_battle_winner()
// used to sit here. Their only callers were the Sniper Battle block in G_RunFrame below, which is
// gone along with the rest of the mode; see the note where Cmd_SniperMode_f used to live in g_cmds.c.

// GalaxyRP: [RPG LMS] rpg_lms_end(), rpg_lms_prepare() and rpg_lms_winner() used to sit here.
// Their only callers were the RPG LMS block in G_RunFrame below, which is gone; see the note where
// Cmd_RpgLmsMode_f used to live in g_cmds.c.

// zyk: restoring default guns and force powers to a player leaving the Melee Battle
//
// GalaxyRP fix: [Melee Battle] this used to be written inline in melee_battle_end()'s loop below,
// which meant it ran only for players who were still signed up when the battle ended. A player who
// walked out under their own steam with a second /meleemode never reached it: that branch (g_cmds.c)
// clears level.melee_players[] for them first, so melee_battle_end()'s loop skips them even when
// their departure is what ends the battle. They kept melee_battle_prepare()'s loadout -- WP_MELEE
// only, binoculars only, fourteen force powers cleared from forcePowersKnown -- until their next
// death or a map change. Lifted into its own function so the leave path and the end path restore
// through the same code and cannot drift apart.
//
// GalaxyRP fix: [Melee Battle] this did not restore anything -- it re-issued a baseline. The body
// was WP_InitForcePowers() plus an unconditional WP_BRYAR_PISTOL and a conditional saber, because
// melee_battle_prepare() took a loadout away without ever writing it down. Everyone who fought
// therefore walked out having permanently lost every holdable item (sentry gun, seeker, medpacs,
// jetpack, cloak -- prepare replaces the whole field with binoculars) and every weapon except
// saber and Bryar, including anything picked up off the map or handed to them with /give guns.
// WP_InitForcePowers() had the same shape of problem: it rebuilt force powers from the client's
// vanilla JKA Profile allocation rather than from what the player actually had.
//
// The Duel Tournament had exactly this defect and was given a real backup/restore pair; the
// Melee Battle now uses the same one, so the two mini-games strip and restore through identical
// code. Both halves self-guard on their validity flag, which is what makes this safe to call on
// a player who was never prepared: melee_battle_end() reaches the whole roster including players
// signed up for a battle that never started, and for them both calls are no-ops rather than the
// free Bryar Pistol and profile force-rebuild the old body handed out.
//
// Order matches the duel's mode-5 block: force first, then loadout. player_restore_loadout()
// ASSIGNS stats[STAT_WEAPONS] while player_restore_force() only ORs WP_MELEE into it, so the
// reverse order would discard the assignment.
//
// An initialize_rpg_skills(ent) call used to close this function, for RPG characters who could
// get into a battle while the amrpgmode==2 join guard in Cmd_MeleeMode_f was commented out. That
// guard is live again and /login, /new and /char are refused while a player is signed up, so
// nobody in a Melee Battle can be in RPG Mode and the call could no longer do anything.
void melee_battle_restore(gentity_t *ent)
{
	if (!ent || !ent->client)
		return;

	player_restore_force(ent);
	player_restore_loadout(ent);
}

// zyk: finishes the melee battle
void melee_battle_end()
{
	int i = 0;

	level.melee_mode = 0;
	level.melee_mode_quantity = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (level.melee_players[i] != -1)
		{ // zyk: restoring default guns and force powers to this player
			melee_battle_restore(&g_entities[i]);
		}

		level.melee_players[i] = -1;
	}

	if (level.melee_model_id != -1)
	{
		G_FreeEntity(&g_entities[level.melee_model_id]);
		level.melee_model_id = -1;
	}
}

// zyk: prepares players to fight in Melee Battle
void melee_battle_prepare()
{
	int i = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		gentity_t *ent = &g_entities[i];

		if (level.melee_players[i] != -1)
		{ // zyk: a player in the Melee Battle
			vec3_t origin;

			// GalaxyRP fix: [Death System] drop anyone who is downed when the bell rings. The join
			// guard refuses a downed player, but nothing re-checked during the twelve-second
			// countdown, so anyone knocked down while waiting was teleported onto the catwalk still
			// downed -- the health reset below hides it, but they cannot move or attack -- and just
			// lay there. The respawn above does not catch it either: a downed player sits on 50
			// health, not below 1.
			//
			// Dropped rather than released: the same state backs the admin /paralyze command, and
			// clearing it here would let a player shed an admin punishment by signing up.
			//
			// No special handling is needed when this takes the battle below two players. The
			// mode-1 handler counted before calling us, but the mode-2 block in G_RunFrame() picks
			// up whatever is left on the next frame -- one player left wins by default, none left
			// ends the battle -- exactly as it does when someone leaves or disconnects.
			if (G_PlayerIsDowned(ent))
			{
				level.melee_players[i] = -1;
				level.melee_mode_quantity--;

				trap->SendServerCommand(i, "print \"^3Melee Battle: ^7You were downed before the battle began, so you are out of it.\n\"");
				continue;
			}

			if (ent->health < 1)
			{ // zyk: respawn him if he is dead
				ClientRespawn(ent);
			}

			// GalaxyRP fix: [Minigames] off any e-web or map emplaced gun before the snapshot
			// below, for the reasons given at the same call in duel_tournament_prepare(). After
			// the respawn above: EWebDisattach() only gives weapons back to a living player.
			zyk_release_mounts_for_minigame(ent);

			// GalaxyRP fix: [Melee Battle] write down what the battle is about to take, so
			// melee_battle_restore() can give back exactly that. Taken AFTER the ClientRespawn()
			// above deliberately: a player who was dead at the starting bell has already been
			// re-equipped by ClientSpawn, and that respawn loadout -- not the corpse's -- is what
			// they should be holding when they walk away. First statement of the strip otherwise,
			// the same placement duel_tournament_prepare() uses.
			player_backup_loadout(ent);

			// A stale PLAYER_STATUS_DUEL_TOURNAMENT_LOSS would silently cost this player their
			// loadout. Both restore halves treat that bit as "this player died in a duel, so
			// ClientSpawn has already re-equipped them" and consume the backup without applying
			// it. The bit is cleared when a tournament picks its next pair and when it prepares a
			// duelist, so a player who lost a duel in a tournament that then ended still carries
			// it. Cleared here for the same reason duel_tournament_prepare() clears it, and safe
			// to clear because /meleemode refuses anyone signed up for a tournament, so nobody in
			// a battle can be a duelist whose bit still means something.
			ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS);

			ent->client->ps.stats[STAT_WEAPONS] = 0;
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
			ent->client->ps.weapon = WP_MELEE;

			ent->health = 100;
			ent->client->ps.stats[STAT_ARMOR] = 100;

			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] = (1 << HI_BINOCULARS);
			ent->client->ps.stats[STAT_HOLDABLE_ITEM] = 0;

			// zyk: disable jetpack at the start of a Melee Battle
			if (ent->client->jetPackOn)
			{
				Jetpack_Off(ent);
			}

			// zyk: removing the seeker drone in case if is activated
			// GalaxyRP fix: [Items] same one-frame edge case as duel_tournament_prepare() above, and
			// worse here: a Melee Battle is punch-only, so a drone still armed as its owner is
			// teleported onto the platform is the one ranged attack in the arena. See
			// zyk_wind_down_seeker_drone() in g_cmds.c.
			zyk_wind_down_seeker_drone(ent);

			// DAJ_RP: [Melee Battle] and the cloak, as duel_tournament_prepare() does. The holdables were
			// just set to the binoculars directly, which never switches an active cloak off, and nothing
			// else ends PW_CLOAKED -- while the cloak key itself wants the cloak item, which he no longer
			// owns. So a player cloaked at the bell stayed invisible for the whole battle and could not
			// even turn it off.
			Jedi_DecloakPair(ent);

			// DAJ_RP: [Force Enlightenment] end any Enlightenment before the Force is saved, as
			// duel_tournament_prepare() does and for the same reason: a backup taken while boosted is
			// handed back by melee_battle_restore() with nothing left to undo it. WP_ForcePowersUpdate()
			// then keeps a new Enlightenment from applying until the battle is over.
			WP_EndForceEnlightenment(ent);

			// DAJ_RP: [Melee Battle] end every power still running, as duel_tournament_prepare() does for
			// the ones it takes away. The strip below only takes the powers out of the menu; what was
			// switched on before the bell -- Protect, Rage, Absorb, Speed, a grip on somebody -- kept
			// running in the fight. This also covers the jump in progress the old line below stopped.
			zyk_stop_active_force_powers(ent);

			// DAJ_RP: [Melee Battle] and the Force powerups, as duel_tournament_prepare() does: a Boon kept
			// its faster regeneration through the battle. (The Enlightenments are already ended above.)
			ent->client->ps.powerups[PW_FORCE_BOON] = 0;
			ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = 0;
			ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = 0;

			// GalaxyRP fix: [Melee Battle] same as the loadout backup above, placed against the
			// force strip that follows it exactly as duel_tournament_prepare() places its own.
			player_backup_force(ent);

			// zyk: cannot use any force powers, except Jump
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PUSH);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PULL);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SPEED);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SEE);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_ABSORB);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_HEAL);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PROTECT);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TELEPATHY);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TEAM_HEAL);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_LIGHTNING);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_GRIP);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_DRAIN);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_RAGE);
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TEAM_FORCE);

			// zyk: stop jumping to avoid falling from the platform
			// DAJ_RP: [Melee Battle] done above now, with every other running power
			// (zyk_stop_active_force_powers()).

			VectorSet(origin, level.melee_mode_origin[0] - 120 + ((i % 6) * 45), level.melee_mode_origin[1] - 120 + ((i/6) * 45), level.melee_mode_origin[2] + 50);

			zyk_TeleportPlayer(ent, origin, ent->client->ps.viewangles);
		}
	}
}

// zyk: shows the winner of the Melee Battle
void melee_battle_winner()
{
	int i = 0;
	gentity_t *ent = NULL;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (level.melee_players[i] != -1)
		{
			ent = &g_entities[i];
			break;
		}
	}

	if (ent)
	{
		// GalaxyRP fix: [Melee Battle] restore before granting, because the prize below is applied
		// with |= on top of whatever the winner is holding. melee_battle_restore() now ASSIGNS
		// stats[STAT_WEAPONS] from the pre-battle snapshot, and melee_battle_end() calls it for
		// every player still on the roster three seconds after this runs -- so leaving the restore
		// until then would have wiped the prize weapons and holdables off the one player who
		// earned them. Restoring first puts the prize on top of the winner's real loadout instead
		// of on top of melee_battle_prepare()'s fists-and-binoculars, and the later call in
		// melee_battle_end() is a no-op because both validity flags have already been consumed.
		//
		// This is the ordering the Duel Tournament already has: mode 5 restores the pair, and
		// duel_tournament_prize() runs afterwards, from mode 2.
		melee_battle_restore(ent);

		ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 20000;
		ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_LIGHT] = level.time + 20000;
		ent->client->ps.powerups[PW_FORCE_ENLIGHTENED_DARK] = level.time + 20000;

		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER) | (1 << WP_BLASTER) | (1 << WP_DISRUPTOR) | (1 << WP_REPEATER);
		ent->client->ps.ammo[AMMO_BLASTER] = rp_max_blaster_pack_ammo.integer;
		ent->client->ps.ammo[AMMO_POWERCELL] = rp_max_power_cell_ammo.integer;
		ent->client->ps.ammo[AMMO_METAL_BOLTS] = rp_max_metal_bolt_ammo.integer;
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN) | (1 << HI_SEEKER) | (1 << HI_MEDPAC_BIG);

		G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));

		trap->SendServerCommand(-1, va("chat \"^3Melee Battle: ^7%s ^7is the winner! Kills: %d\"", ent->client->pers.netname, level.melee_players[ent->s.number]));
	}
	else
	{
		trap->SendServerCommand(-1, "chat \"^3Melee Battle: ^7No one is the winner!\"");
	}
}



/*
================
G_RunFrame

Advances the non-player objects in the world
================
*/
void ClearNPCGlobals( void );
void AI_UpdateGroups( void );
void ClearPlayerAlertEvents( void );
void SiegeCheckTimers(void);
void WP_SaberStartMissileBlockCheck( gentity_t *self, usercmd_t *ucmd );
extern void Jedi_Cloak( gentity_t *self );
qboolean G_PointInBounds( vec3_t point, vec3_t mins, vec3_t maxs );

int g_siegeRespawnCheck = 0;
void SetMoverState( gentity_t *ent, moverState_t moverState, int time );

extern void remove_credits(gentity_t *ent, int credits);
extern void set_max_health(gentity_t *ent);
extern void set_max_shield(gentity_t *ent);
extern void duel_show_table(gentity_t *ent);
extern void WP_DisruptorAltFire(gentity_t *ent);
extern void G_Kill( gentity_t *ent );

/*
================
zyk_entity_line_vehicle_npc_spawner

GalaxyRP fix: [Entity System] for one parsed entity-file line: if it is an npc_spawner whose
npc_type names a vehicle, return that type, else NULL. Keys are matched case-insensitively, and
the LAST classname and npc_type win, as they do when the line is spawned. Only npc_spawner: an
NPC_Vehicle line is how a vehicle is meant to come back, and the SP-style NPC_<name> spawner
classes set their own NPC_type. See BG_VehicleNameExists() for why the lookup is a silent one.
================
*/
static const char *zyk_entity_line_vehicle_npc_spawner( char (*keys)[ZYK_ENTITY_FILE_LINE_LENGTH], char (*values)[ZYK_ENTITY_FILE_LINE_LENGTH], int pairs_x2 )
{
	const char *classname = NULL;
	const char *npc_type = NULL;
	int m;

	for ( m = 0; m < pairs_x2; m += 2 )
	{
		if ( Q_stricmp( keys[m / 2], "classname" ) == 0 )
			classname = values[m / 2];
		else if ( Q_stricmp( keys[m / 2], "npc_type" ) == 0 )
			npc_type = values[m / 2];
	}

	if ( classname && npc_type && Q_stricmp( classname, "npc_spawner" ) == 0 && BG_VehicleNameExists( npc_type ) )
	{
		return npc_type;
	}

	return NULL;
}

/*
================
RP_SpeakerAudienceFrame

GalaxyRP: [Entity System] a target_speaker with a sound set is sent only to the clients that can hear it
(SVF_BROADCASTCLIENTS, see SP_target_speaker()): its r.broadcastClients bits are set here every frame,
after the clients have moved and before the server builds their snapshots, for every connected client
whose eye -- the point the client listens from and the engine sends from -- is within rpAudibleRange.
A client whose bit is clear is not sent it at all; one whose bit is set is sent it wherever it stands.
================
*/
void RP_SpeakerAudienceFrame( void )
{
	int e, c;

	for ( e = MAX_CLIENTS; e < level.num_entities && e < MAX_GENTITIES; e++ )
	{
		gentity_t *speaker = &g_entities[e];
		float rangeSq;

		if ( !speaker->inuse || speaker->rpAudibleRange <= 0.0f || !( speaker->r.svFlags & SVF_BROADCASTCLIENTS ) )
			continue;

		speaker->r.broadcastClients[0] = speaker->r.broadcastClients[1] = 0u;
		rangeSq = speaker->rpAudibleRange * speaker->rpAudibleRange;

		for ( c = 0; c < MAX_CLIENTS; c++ )
		{
			gentity_t *player = &g_entities[c];
			vec3_t eye, d;

			if ( !player->inuse || !player->client || player->client->pers.connected != CON_CONNECTED )
				continue;

			VectorCopy( player->client->ps.origin, eye );
			eye[2] += player->client->ps.viewheight;
			VectorSubtract( speaker->r.currentOrigin, eye, d );
			if ( VectorLengthSquared( d ) <= rangeSq )
				speaker->r.broadcastClients[c / 32] |= ( 1u << ( c % 32 ) );
		}
	}
}

void G_RunFrame( int levelTime ) {
	int			i;
	gentity_t	*ent;
#ifdef _G_FRAME_PERFANAL
	int			iTimer_ItemRun = 0;
	int			iTimer_ROFF = 0;
	int			iTimer_ClientEndframe = 0;
	int			iTimer_GameChecks = 0;
	int			iTimer_Queues = 0;
	void		*timer_ItemRun;
	void		*timer_ROFF;
	void		*timer_ClientEndframe;
	void		*timer_GameChecks;
	void		*timer_Queues;
#endif

	if (level.gametype == GT_SIEGE &&
		g_siegeRespawn.integer &&
		g_siegeRespawnCheck < level.time)
	{ //check for a respawn wave
		gentity_t *clEnt;
		for ( i=0; i < MAX_CLIENTS; i++ )
		{
			clEnt = &g_entities[i];

			if (clEnt->inuse && clEnt->client &&
				clEnt->client->tempSpectate >= level.time &&
				clEnt->client->sess.sessionTeam != TEAM_SPECTATOR)
			{
				ClientRespawn(clEnt);
				clEnt->client->tempSpectate = 0;
			}
		}

		g_siegeRespawnCheck = level.time + g_siegeRespawn.integer * 1000;
	}

	if (gDoSlowMoDuel)
	{
		if (level.restarted)
		{
			char buf[128];
			float tFVal = 0;

			trap->Cvar_VariableStringBuffer("timescale", buf, sizeof(buf));

			tFVal = atof(buf);

			trap->Cvar_Set("timescale", "1");
			if (tFVal == 1.0f)
			{
				gDoSlowMoDuel = qfalse;
			}
		}
		else
		{
			float timeDif = (level.time - gSlowMoDuelTime); //difference in time between when the slow motion was initiated and now
			float useDif = 0; //the difference to use when actually setting the timescale

			if (timeDif < 150)
			{
				trap->Cvar_Set("timescale", "0.1f");
			}
			else if (timeDif < 1150)
			{
				useDif = (timeDif/1000); //scale from 0.1 up to 1
				if (useDif < 0.1f)
				{
					useDif = 0.1f;
				}
				if (useDif > 1.0f)
				{
					useDif = 1.0f;
				}
				trap->Cvar_Set("timescale", va("%f", useDif));
			}
			else
			{
				char buf[128];
				float tFVal = 0;

				trap->Cvar_VariableStringBuffer("timescale", buf, sizeof(buf));

				tFVal = atof(buf);

				trap->Cvar_Set("timescale", "1");
				if (timeDif > 1500 && tFVal == 1.0f)
				{
					gDoSlowMoDuel = qfalse;
				}
			}
		}
	}

	// if we are waiting for the level to restart, do nothing
	if ( level.restarted ) {
		return;
	}

	level.framenum++;
	level.previousTime = level.time;
	level.time = levelTime;

	// GalaxyRP fix: [Configstrings] take the map-load count here rather than at the end of
	// G_InitGame, because at the end of G_InitGame the gamestate is not finished. SV_SpawnServer
	// clears every configstring, calls InitGame, runs four frames, and only THEN writes
	// CS_SYSTEMINFO and CS_SERVERINFO (sv_init.cpp: SV_ClearServer, SV_InitGameProgs, the settle
	// loop, then SV_SetConfigstring for both). So a count taken in InitGame sees neither of them --
	// and CS_SYSTEMINFO is the largest string on the server, since sv_paks and sv_pakNames hold one
	// entry per loaded pk3. On a pure server with a map pack that is kilobytes the old baseline, and
	// the line it logged, simply did not know about.
	//
	// Waiting for CS_SYSTEMINFO to appear rather than counting frames is deliberate: the number of
	// frames the engine runs before it writes that string is the engine's business, and map_restart
	// takes a different path through it (configstrings survive a restart, so it is already there on
	// the first frame). The time bound is only a backstop.
	if ( !level.zyk_gamestate_baseline_done )
	{
		char sysinfo[MAX_INFO_STRING];

		trap->GetConfigstring( CS_SYSTEMINFO, sysinfo, sizeof( sysinfo ) );

		if ( sysinfo[0] || level.time > (level.startTime + ZYK_GAMESTATE_BASELINE_WAIT) )
		{
			level.zyk_gamestate_baseline_done = qtrue;

			G_ResetGamestateEstimate();
			G_LogPrintf( "gamestate after map load: %d of %d bytes used\n",
				level.zyk_gamestate_bytes, ZYK_GAMESTATE_BUDGET );
		}
	}

	if (g_allowNPC.integer)
	{
		NAV_CheckCalcPaths();
	}

	AI_UpdateGroups();

	if (g_allowNPC.integer)
	{
		if ( d_altRoutes.integer )
		{
			trap->Nav_CheckAllFailedEdges();
		}
		trap->Nav_ClearCheckedNodes();

		//remember last waypoint, clear current one
		for ( i = 0; i < level.num_entities ; i++)
		{
			ent = &g_entities[i];

			if ( !ent->inuse )
				continue;

			if ( ent->waypoint != WAYPOINT_NONE
				&& ent->noWaypointTime < level.time )
			{
				ent->lastWaypoint = ent->waypoint;
				ent->waypoint = WAYPOINT_NONE;
			}
			if ( d_altRoutes.integer )
			{
				trap->Nav_CheckFailedNodes( (sharedEntity_t *)ent );
			}
		}

		//Look to clear out old events
		ClearPlayerAlertEvents();
	}

	g_TimeSinceLastFrame = (level.time - g_LastFrameTime);

	// get any cvar changes
	G_UpdateCvars();



#ifdef _G_FRAME_PERFANAL
	trap->PrecisionTimer_Start(&timer_ItemRun);
#endif

	// GalaxyRP fix: [Guardian] removed the boss_battle_music_reset_timer reset block here — field
	// removed as dead (see g_local.h); this reader never fired since the field could never become
	// nonzero.

	// zyk: Melee Battle
	if (level.melee_mode == 3 && level.melee_mode_timer < level.time)
	{
		melee_battle_end();
	}
	else if (level.melee_mode == 2)
	{
		if (level.melee_mode_timer < level.time)
		{
			melee_battle_end();
			trap->SendServerCommand(-1, "chat \"^3Melee Battle: ^7Time is up! No winner!\"");
		}
		else if (level.melee_mode_quantity == 1)
		{
			melee_battle_winner();

			// zyk: wait some time before ending the melee battle so the winner can escape the platform
			level.melee_mode_timer = level.time + 3000;
			level.melee_mode = 3;
		}
		// GalaxyRP fix: [Melee Battle] the "== 1" test above is the only early end, so a battle whose
		// player count reaches 0 rather than 1 never ends early at all. That is not a corner case: the
		// out-of-bounds kill loop that pushes players off the platform runs later in this same frame,
		// so the last two players falling together take the count straight from 2 to 0. The battle then
		// sat in mode 2 for its entire remaining timeout -- ten minutes at the shipped default --
		// rejecting every attempt to join with no explanation, until the "Time is up" message finally
		// fired. The sniper battle and the RPG LMS had the same defect; both have since been removed.
		else if (level.melee_mode_quantity <= 0)
		{
			melee_battle_end();
			trap->SendServerCommand(-1, "chat \"^3Melee Battle: ^7No players left! Melee Battle is over!\"");
		}
	}
	else if (level.melee_mode == 1 && level.melee_mode_timer < level.time)
	{
		if (level.melee_mode_quantity > 1)
		{ // zyk: if at least 2 players joined in it, start the battle
			melee_battle_prepare();
			level.melee_mode = 2;
			level.melee_mode_timer = level.time + 600000;
			trap->SendServerCommand(-1, "chat \"^3Melee Battle: ^7the battle has begun! The battle will have a max of 10 minutes!\"");
		}
		else
		{ // zyk: finish the battle
			melee_battle_end();
			trap->SendServerCommand(-1, "chat \"^3Melee Battle: ^7Not enough players. Melee Battle is over!\"");
		}
	}

	// zyk: Duel Tournament
	if (level.duel_tournament_mode == 4)
	{ // zyk: validations during a duel
		// GalaxyRP fix: [Duel Tournament] this block deliberately sits OUTSIDE the
		// "duel_tournament_paused == qfalse" wrapper further down, because leaver validation has to
		// keep running regardless. An earlier attempt to make /duelpause hold a duel that was
		// already under way added "&& level.duel_tournament_paused == qfalse" to the guard below,
		// which was wrong twice over: the else arm is the "this match is over" path, so pausing
		// abandoned the duel, and because that arm does not advance duel_matches_done the same
		// pairing was then replayed from scratch. Cmd_DuelPause_f now refuses in mode 4 instead
		// (see the note there), so a paused tournament can never be sitting in this state and the
		// guard is back to asking only what it can actually answer: are both duelists still valid.
		if (duel_tournament_validate_duelists() == qtrue)
		{
			gentity_t *first_duelist = &g_entities[level.duelist_1_id];
			gentity_t *second_duelist = &g_entities[level.duelist_2_id];

			if (!(first_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)) &&
				second_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS))
			{ // zyk: first duelist wins
				duel_tournament_set_match_winner(first_duelist);

				level.duel_tournament_mode = 5;
				level.duel_tournament_timer = level.time + 1500;
			}
			else if (!(second_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)) &&
				first_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS))
			{ // zyk: second duelist wins
				duel_tournament_set_match_winner(second_duelist);

				level.duel_tournament_mode = 5;
				level.duel_tournament_timer = level.time + 1500;
			}
			else if (level.duel_tournament_timer < level.time)
			{ // zyk: duel timed out
				int first_duelist_health = 0;
				int second_duelist_health = 0;

				if (!(first_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))
				{
					first_duelist_health = first_duelist->health + first_duelist->client->ps.stats[STAT_ARMOR];
				}

				if (!(second_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)))
				{
					second_duelist_health = second_duelist->health + second_duelist->client->ps.stats[STAT_ARMOR];
				}

				if (first_duelist_health > second_duelist_health)
				{ // zyk: first duelist wins
					duel_tournament_set_match_winner(first_duelist);
				}
				else if (first_duelist_health < second_duelist_health)
				{ // zyk: second duelist wins
					duel_tournament_set_match_winner(second_duelist);
				}
				else
				{ // zyk: tie
					duel_tournament_set_match_winner(NULL);
				}

				level.duel_tournament_mode = 5;
				level.duel_tournament_timer = level.time + 1500;
			}
			else if (first_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS) &&
				second_duelist->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS))
			{ // zyk: tie when both duelists die on the same frame
				duel_tournament_set_match_winner(NULL);

				level.duel_tournament_mode = 5;
				level.duel_tournament_timer = level.time + 1500;
			}
		}
		else
		{ // zyk: match ended because one of the duelists is no longer valid
			level.duel_tournament_mode = 5;
			level.duel_tournament_timer = level.time + 1500;
		}
	}

	// GalaxyRP fix: [Duel Tournament] /duelpause used to stop the state machine below without
	// stopping the clock. level.duel_tournament_timer is always an absolute level.time + N, and
	// level.time keeps advancing while paused, so every pending transition expired DURING the pause
	// and fired on the first frame after resuming. Pausing did not hold the tournament, it only
	// deferred it and then let it snap forward.
	//
	// Worst in signup, where the effect was the opposite of the intent: an admin pausing mode 1 to
	// let latecomers in burned the countdown while paused, and on resume the mode-1 branch below ran
	// immediately -- ending the tournament outright via duel_tournament_end() if the roster was still
	// under rp_duel_tournament_min_players. The pause killed the tournament it was meant to extend.
	// Milder elsewhere: mode 3's three-second "X vs Y" announcement collapsed to nothing, so duelists
	// were teleported into the arena the instant an admin resumed.
	//
	// Carrying the timer forward by the frame delta keeps it a valid absolute time at every instant,
	// so nothing that reads it has to know pausing exists -- duel_show_table() (g_cmds.c) and the
	// arena-entry freeze in bg_pmove.c both do, and both currently only run in mode 4, which cannot
	// be paused. Storing a remaining-time delta at pause and re-basing it at resume would work too,
	// but only while every clear of the flag goes through Cmd_DuelPause_f, and duel_tournament_end()
	// clears it directly.
	//
	// Safe against a competing write: every other assignment to this timer is either inside the
	// "not paused" block below, or in the mode-4 block above (mode 4 cannot be paused), or is
	// Cmd_DuelMode_f's sign-up write, which is guarded by "if (level.duel_tournament_mode != 1)" and
	// so cannot fire for a tournament already in signup -- and joining at mode 2 or later is refused
	// outright. level.previousTime is taken at the top of G_RunFrame, well above this, so the delta
	// here is exactly one frame.
	if (level.duel_tournament_paused == qtrue)
	{
		level.duel_tournament_timer += level.time - level.previousTime;
	}

	if (level.duel_tournament_paused == qfalse)
	{
		if (level.duel_tournament_mode == 5 && level.duel_tournament_timer < level.time)
		{ // zyk: show score table and reset duelists
			duel_show_table(NULL);

			if (level.duelist_1_id != -1)
			{
				duel_tournament_restore_duelist(&g_entities[level.duelist_1_id]);
			}

			if (level.duelist_2_id != -1)
			{
				duel_tournament_restore_duelist(&g_entities[level.duelist_2_id]);
			}

			level.duelist_1_id = -1;
			level.duelist_2_id = -1;

			level.duel_tournament_timer = level.time + 1500;
			level.duel_tournament_mode = 2;
		}
		else if (level.duel_tournament_mode == 3 && level.duel_tournament_timer < level.time)
		{
			if (duel_tournament_validate_duelists() == qtrue)
			{
				vec3_t zyk_origin, zyk_angles;
				gentity_t *duelist_1 = &g_entities[level.duelist_1_id];
				gentity_t *duelist_2 = &g_entities[level.duelist_2_id];
				qboolean zyk_has_respawned = qfalse;

				// zyk: respawning duelists that are still dead
				if (duelist_1->health < 1)
				{
					ClientRespawn(duelist_1);
					zyk_has_respawned = qtrue;
				}

				if (duelist_2->health < 1)
				{
					ClientRespawn(duelist_2);
					zyk_has_respawned = qtrue;
				}

				duel_tournament_protect_duelists(duelist_1, duelist_2);

				if (zyk_has_respawned == qfalse)
				{
					// zyk: setting the max time players can duel
					level.duel_tournament_timer = level.time + rp_duel_tournament_duel_time.integer;

					// zyk: prepare the duelists to start duel
					duel_tournament_prepare(duelist_1);
					duel_tournament_prepare(duelist_2);

					// zyk: put the duelists along the y axis
					VectorSet(zyk_angles, 0, 90, 0);
					VectorSet(zyk_origin, level.duel_tournament_origin[0], level.duel_tournament_origin[1] - 125, level.duel_tournament_origin[2] + 1);
					zyk_TeleportPlayer(duelist_1, zyk_origin, zyk_angles);

					VectorSet(zyk_angles, 0, -90, 0);
					VectorSet(zyk_origin, level.duel_tournament_origin[0], level.duel_tournament_origin[1] + 125, level.duel_tournament_origin[2] + 1);
					zyk_TeleportPlayer(duelist_2, zyk_origin, zyk_angles);

					level.duel_tournament_mode = 4;
				}
				else
				{ // zyk: must wait a bit more to guarantee the player is fully respawned before teleporting him to arena
					level.duel_tournament_timer = level.time + 500;
				}
			}
			else
			{ // zyk: duelists are no longer valid, get a new match
				level.duel_tournament_mode = 5;
				level.duel_tournament_timer = level.time + 1500;
			}
		}
		else if (level.duel_tournament_mode == 2 && level.duel_tournament_timer < level.time)
		{ // zyk: search for duelists and put them in the arena
			int zyk_it = 0;

			// GalaxyRP fix: [Guardian] removed the is_in_boss local here — its only setter ("someone
			// fighting a quest boss") was already removed as dead (guardian_mode is permanently 0,
			// spawn_boss has no callers), making it permanently qfalse. Both branches below that
			// tested it are simplified accordingly.
			for (zyk_it = 0; zyk_it < MAX_CLIENTS; zyk_it++)
			{
				gentity_t *this_ent = &g_entities[zyk_it];

				// zyk: cleaning flag from player
				if (this_ent && this_ent->client)
					this_ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS);
			}

			if (level.duel_matches_done < level.duel_matches_quantity)
			{ // zyk: if there are still matches to be chosen, try to choose now
				level.duelist_1_id = level.duel_matches[level.duel_matches_done][0];
				level.duelist_2_id = level.duel_matches[level.duel_matches_done][1];

				if (duel_tournament_validate_duelists() == qfalse)
				{ // zyk: if not valid, show score table
					level.duel_tournament_mode = 5;
					level.duel_tournament_timer = level.time + 1500;
				}
				else
				{
					gentity_t *duelist_1 = &g_entities[level.duelist_1_id];
					gentity_t *duelist_2 = &g_entities[level.duelist_2_id];

					level.duel_tournament_timer = level.time + 3000;
					level.duel_tournament_mode = 3;

					trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7%s ^7x %s\"", duelist_1->client->pers.netname, duelist_2->client->pers.netname));
				}
			}

			if (level.duel_matches_quantity == level.duel_matches_done)
			{ // zyk: current cycle ended. Go to next one
				level.duel_tournament_rounds++;

				if (level.duel_tournament_rounds < rp_duel_tournament_rounds_per_match.integer)
				{
					level.duel_matches_done = 0;
				}
			}

			if (level.duel_matches_quantity == level.duel_matches_done && level.duel_tournament_mode == 2)
			{ // zyk: all matches were done. Determine the tournament winner
				duel_tournament_winner();
				duel_tournament_end();
			}
			else if (level.duelists_quantity == 0)
			{
				duel_tournament_end();
				trap->SendServerCommand(-1, "chat \"^3Duel Tournament: ^7There are no duelists anymore. Tournament is over!\"");
			}
		}
		else if (level.duel_tournament_mode == 1 && level.duel_tournament_timer < level.time)
		{ // zyk: Duel tournament begins after validation on number of players
			if (level.duelists_quantity > 1 && level.duelists_quantity >= rp_duel_tournament_min_players.integer)
			{ // zyk: must have a minimum of 2 players
				int zyk_number_of_teams = duel_tournament_generate_teams();

				if (zyk_number_of_teams > 1 && zyk_number_of_teams >= rp_duel_tournament_min_players.integer)
				{
					level.duel_tournament_mode = 5;
					level.duel_tournament_timer = level.time + 1500;

					duel_tournament_generate_match_table();

					trap->SendServerCommand(-1, "chat \"^3Duel Tournament: ^7The tournament begins!\"");
				}
				else
				{
					duel_tournament_end();
					trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7Not enough duelists (minimum of %d). Tournament is over!\"", rp_duel_tournament_min_players.integer));
				}
			}
			else
			{
				duel_tournament_end();
				trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7Not enough duelists (minimum of %d). Tournament is over!\"", rp_duel_tournament_min_players.integer));
			}
		}
	}

	// GalaxyRP fix: [Quests] the "Guardian of Map abilities" block used to live here, driving periodic
	// special attacks for the map_guardian NPC spawned by Cmd_GuardianQuest_f. That command was deleted
	// as unreachable dead code (see the GalaxyRP fix comment in g_cmds.c), and level.guardian_quest --
	// its only setter -- has been removed along with it, so this block is removed too.

	if (level.load_entities_timer != 0 && level.load_entities_timer < level.time)
	{ // zyk: loading entities from the file specified in entload command, or the default file
		char content[ZYK_ENTITY_FILE_LINE_LENGTH];
		// GalaxyRP fix: [Entity System] one bounded slot per possible key/value pair, each as wide
		// as the line buffer so nothing a well-formed line can carry gets truncated. static rather
		// than automatic because the pair of arrays is far too large to put on G_RunFrame's stack.
		static char zyk_keys[ZYK_MAX_SPAWN_STRING_SLOTS / 2][sizeof(content)];
		static char zyk_values[ZYK_MAX_SPAWN_STRING_SLOTS / 2][sizeof(content)];
		FILE *this_file = NULL;
		// GalaxyRP fix: [Entity System] only so the refusal below can say where in the file it gave up
		int zyk_lines_read = 0;
		int zyk_spawned = 0;
		int zyk_sub_bsp_lines = 0;
		int zyk_logical_refused = 0;	// GalaxyRP: [Logical Entities] lines skipped for want of a logical slot

		strcpy(content,"");

		// zyk: loading the entities from the file
		this_file = fopen(level.load_entities_file,"r");

		if (this_file != NULL)
		{
			// GalaxyRP fix: [Entity System] this parser trusted the file completely. The two inner loops
			// copied into char[256] buffers with no bound on the write index and no bound on the read index
			// either, so a key or value longer than the buffer smashed the stack, and a line containing no
			// ';' at all ran straight off the end of content[] and kept writing until it happened to find a
			// ';' somewhere in unrelated stack memory. Both are reachable from an ordinary preset: keys and
			// values come from /entadd and /entedit arguments, which run to MAX_STRING_CHARS, and any file
			// can be hand-edited or truncated. It also called G_Spawn() once per line before looking at the
			// line, so a file of blank lines consumed the entity pool until G_Spawn's "no free entities"
			// dropped the server.
			//
			// The line is now parsed into bounded buffers first and an entity is taken only once the line is
			// known to be well formed. The buffers are as wide as content[] so nothing a valid preset can
			// hold is truncated -- a token can never be longer than the line it came from -- and a line that
			// is malformed, over-long or over-full is logged and skipped instead of being half-applied.
			while (fgets(content,sizeof(content),this_file) != NULL)
			{
				int j = 0; // zyk: the current key/value being used
				int k = 0; // zyk: current spawn string position
				int content_len = 0;
				qboolean line_ok = qtrue;
				qboolean line_full = qfalse;
				gentity_t *new_ent = NULL;

				zyk_lines_read++;

				content_len = strlen(content);

				if (content_len > 0 && content[content_len - 1] == '\n')
				{
					content[content_len - 1] = '\0';
					content_len--;
				}
				else if (content_len == (int)(sizeof(content) - 1))
				{ // zyk: no newline and the buffer is full, so fgets split this line -- it is not usable
					G_LogPrintf("entity file %s: line longer than %d characters, skipped\n", level.load_entities_file, (int)sizeof(content) - 1);

					// zyk: swallow the rest of the split line so its tail is not read as a line of its own
					while (fgets(content, sizeof(content), this_file) != NULL && strchr(content, '\n') == NULL)
					{
					}

					continue;
				}

				// GalaxyRP fix: [Entity System] a preset written or edited on Windows ends its lines
				// with CRLF. Read on Linux -- or in binary-identical form on any platform where the
				// runtime does not translate it -- only the '\n' was stripped, so the '\r' stayed on
				// the end of the line. The parse loop below then ran one more time, the decoder took
				// the '\r' as the start of a key and reached the end of the line looking for its ';',
				// and "key was not terminated" refused the line. Every line, so an admin who moved a
				// server from Windows to Linux, or who opened a preset in Notepad, lost every entity
				// in the file and got one log line per entity saying it was malformed.
				//
				// It cannot eat real data: zyk_entity_file_encode() writes a carriage return that is
				// part of a value as the two-character escape "\r", never as a bare one, so a raw
				// '\r' at the end of a line is a line terminator and nothing else. After the '\n'
				// test above rather than inside it, so a final line with no newline is handled too.
				if (content_len > 0 && content[content_len - 1] == '\r')
				{
					content[content_len - 1] = '\0';
					content_len--;
				}

				if (content_len == 0)
				{ // zyk: blank line, nothing to spawn
					continue;
				}

				// zyk: parse the whole line into the scratch buffers before allocating anything
				while (k < content_len && line_ok == qtrue)
				{
					if (j + 1 >= ZYK_MAX_SPAWN_STRING_SLOTS)
					{ // zyk: more key/value pairs than one entity can hold
						line_ok = qfalse;
						line_full = qtrue;
						break;
					}

					// zyk: getting the key
					// GalaxyRP fix: [Entity System] the two copy loops that used to sit here stopped at the
					// first ';' in the line, so a ';' inside a value silently became a new key. They now go
					// through zyk_entity_file_decode(), which stops only at a ';' /entsave did not escape and
					// turns an escaped semicolon, newline, carriage return and backslash back into the
					// characters they stand for.
					k = zyk_entity_file_decode(content, content_len, k, zyk_keys[j / 2], (int)sizeof(zyk_keys[0]));

					if (k >= content_len || content[k] != ';')
					{ // zyk: key was not terminated -- malformed line
						line_ok = qfalse;
						break;
					}
					k++;

					// zyk: getting the value
					k = zyk_entity_file_decode(content, content_len, k, zyk_values[j / 2], (int)sizeof(zyk_values[0]));

					if (k >= content_len || content[k] != ';')
					{ // zyk: value was not terminated -- malformed line
						line_ok = qfalse;
						break;
					}
					k++;

					j += 2;
				}

				if (line_ok == qfalse)
				{
					if (line_full == qtrue)
						G_LogPrintf("entity file %s: more than %d key/value pairs on one line, skipped\n", level.load_entities_file, ZYK_MAX_SPAWN_STRING_SLOTS / 2);
					else
						G_LogPrintf("entity file %s: malformed line, skipped\n", level.load_entities_file);

					continue;
				}

				if (j == 0)
				{ // zyk: nothing on this line to spawn
					continue;
				}

				// GalaxyRP fix: [Entity System] an npc_spawner naming a vehicle. /entsave used to write
				// every live NPC as one of these, vehicles included, and the NPC loader always refuses a
				// vehicle type that does not come from an NPC_Vehicle spawner -- two red console errors
				// and nothing spawned. /entsave no longer writes them; presets saved before still hold
				// them, so skip them here with one line in the log instead.
				{
					const char *vehicle_type = zyk_entity_line_vehicle_npc_spawner(zyk_keys, zyk_values, j);

					if (vehicle_type)
					{
						G_LogPrintf("entity file %s: line %d is an npc_spawner for vehicle %s, skipped "
							"(vehicles only spawn from NPC_Vehicle; re-save the preset to drop the line)\n",
							level.load_entities_file, zyk_lines_read, vehicle_type);
						continue;
					}
				}

				// GalaxyRP fix: [Entity System] a line /entsave wrote, before it learned not to, for an entity
				// a misc_bsp's sub-BSP spawned. Every such entity carries the BSPInstanceID key
				// HandleEntityAdjustment() gives it. The preset's misc_bsp line rebuilds those entities
				// itself now (see SP_misc_bsp), and this copy could not load right anyway -- its "*N" model
				// counts in the sub-BSP's numbering -- so loading it as well would put two of each in.
				{
					int m;
					qboolean sub_bsp_line = qfalse;

					for (m = 0; m < j; m += 2)
					{
						if (Q_stricmp(zyk_keys[m / 2], "BSPInstanceID") == 0)
						{
							sub_bsp_line = qtrue;
							break;
						}
					}

					if (sub_bsp_line)
					{
						zyk_sub_bsp_lines++;
						continue;
					}
				}

				// GalaxyRP fix: [Entity System] a good line is not enough on its own. This loop takes an
				// entity per line and nothing bounds the number of lines, so it was the one player-driven
				// spawn path with no check at all -- /entadd, /npc, npc_spawner and the asteroid field all
				// ask first. What it walks into is G_Spawn's trap->Error(ERR_DROP), which on a dedicated
				// server ends the process rather than dropping a player (see G_Spawn in g_utils.c).
				//
				// /entload frees the map's own entities before it reads the file, so a preset that fit once
				// fits again -- but the default.txt loaded automatically at map start frees nothing, it adds
				// to everything the map already spawned, and no preset is bounded by anything once it has
				// been hand-edited.
				//
				// The margin is /entadd's, for /entadd's reason: some classes allocate more than the one
				// entity asked for -- func_plat builds its own trigger, an npc_spawner with no targetname
				// spawns its NPC immediately. Stop reading rather than skipping the line: the table only
				// fills further from here, so every later line would be refused too, and a preset half
				// applied from the front is easier to reason about than one with holes through it.
				//
				// GalaxyRP fix: [Logical Entities] asked of the region the line's classname leads to, as
				// /entadd asks (RP_SpawnRouteHasRoom): this only looked at the networked table, and a
				// preset of logical lines walked straight into G_SpawnLogical()'s ERR_DROP -- on every
				// map load, for a default.txt. A logical refusal skips the line and reads on, since the
				// networked lines after it may still fit; a networked one stops the file as before.
				{
					rpSpawnRoute_t route;
					int m;

					RP_SpawnRouteInit(&route);
					for (m = 0; m < j; m += 2)
					{
						RP_SpawnRouteNoteKey(&route, zyk_keys[m / 2], zyk_values[m / 2]);
					}

					if (RP_SpawnRouteHasRoom(&route) == qfalse)
					{
						if (RP_SpawnRouteIsLogical(&route))
						{
							if (zyk_logical_refused == 0)
							{
								G_LogPrintf("entity file %s: line %d (%s) skipped -- %d logical entity slots free, "
									"%d of them reserved. Every later logical line is skipped too.\n",
									level.load_entities_file, zyk_lines_read, route.classname, G_FreeLogicalEntityCount(),
									ZYK_LOGICAL_ENTITY_RESERVE);
							}
							zyk_logical_refused++;
							continue;
						}

						G_LogPrintf("entity file %s: stopped at line %d after %d entities -- %d entity slots "
							"free, %d of them reserved. The rest of the file was not loaded.\n",
							level.load_entities_file, zyk_lines_read, zyk_spawned, G_FreeEntityCount(),
							ZYK_ENTITY_RESERVE);
						break;
					}

					// GalaxyRP fix: [Entity System] and room in the game's memory pool, which every line's
					// keys and values go into twice (the record and the spawn) and which is never freed
					// within a map: G_Alloc() ends the server when it runs out, and a preset is the one
					// place that reaches it unattended, at every map start. The line's own length bounds
					// its decoded pairs. Stopped, as above: the pool only fills from here.
					if (G_AllocRoomFor((int)strlen(content) * 2 + 64) == qfalse)
					{
						G_LogPrintf("entity file %s: stopped at line %d after %d entities -- %d bytes free in the "
							"game's memory pool, %d of them reserved. The rest of the file was not loaded.\n",
							level.load_entities_file, zyk_lines_read, zyk_spawned, G_AllocRemaining(), ZYK_ALLOC_RESERVE);
						break;
					}

					// zyk: the line is good, so now take an entity for it
					// GalaxyRP: [Logical Entities] in the region its classname belongs to, decided from
					// the pairs already parsed above -- the same rule the map loader and /entadd use.
					new_ent = RP_SpawnForRoute(&route);
				}

				if (new_ent)
				{
					int m = 0;

					zyk_spawned++;

					while (m < j)
					{
						// zyk: copying the key and value to the spawn string array
						// GalaxyRP fix: [Entity System] G_NewString here would translate a backslash-n a
						// second time. zyk_entity_file_decode() has already resolved every escape, so a
						// backslash that reaches this point is one the value really contains and must be
						// stored as-is -- hence the copy that does not translate.
						level.zyk_spawn_strings[new_ent->s.number][m] = G_NewStringRaw(zyk_keys[m / 2]);
						level.zyk_spawn_strings[new_ent->s.number][m + 1] = G_NewStringRaw(zyk_values[m / 2]);

						m += 2;
					}

					level.zyk_spawn_strings_values_count[new_ent->s.number] = j;

					// GalaxyRP: [Entity System] a line that is one of the map's own entities, as /entsave
					// wrote it, is still the map's: /entcut and /entrotate leave it alone. See
					// RP_MarkMapEntities() in g_entgrab.c.
					new_ent->rpMapEntity = RP_RecordIsMapEntity(new_ent->s.number);

					// zyk: spawns the entity
					zyk_main_spawn_entity(new_ent);
				}
			}

			fclose(this_file);

			if (zyk_sub_bsp_lines > 0)
			{
				G_LogPrintf("entity file %s: %d lines saved from a misc_bsp's sub-BSP skipped -- the misc_bsp "
					"rebuilds those entities itself. Re-save the preset to drop the lines.\n",
					level.load_entities_file, zyk_sub_bsp_lines);
			}

			if (zyk_logical_refused > 0)
			{
				G_LogPrintf("entity file %s: %d lines skipped for want of a logical entity slot (%d spawned).\n",
					level.load_entities_file, zyk_logical_refused, zyk_spawned);
			}
		}

		// GalaxyRP fix: [Logical Entities] the preset just respawned every entity through
		// RP_SpawnForRoute(), so a trigger_shipboundary's marker is back in the logical region --
		// where handing its number to the engine ends the server process. Promote them again, as
		// the map spawn pass does. Above G_FindTeams() for the same reason it is above it there:
		// that function builds teammaster/teamchain POINTER chains and moves a slave's targetname
		// onto its master, so a free-and-reallocate has to happen first. See g_spawn.c.
		RP_PromoteShipboundaryTargets();

		// zyk: CTF need to have the flags spawned again when an entity file is loaded
		// general initialization
		G_FindTeams();
		RP_ToggleMoverTeamsMatch();	// GalaxyRP: [SP Maps] see G_InitGame()

		// make sure we have flags for CTF, etc
		if( level.gametype >= GT_TEAM ) {
			G_CheckTeamItems();
		}

		// GalaxyRP fix: [Entity System] the load above started with every non-client entity freed,
		// the helper that runs the world's spawnscript among them, and no preset carries that helper
		// back. Start the script again, as the map loader does, so a map that drives its spawners from
		// it (the fighters on a siege space map) behaves after a preset load as it does at map start.
		// See G_StartWorldSpawnScript() in g_spawn.c.
		G_StartWorldSpawnScript( qtrue );

		level.load_entities_timer = 0;
	}

	//
	// go through all allocated objects
	//
	ent = &g_entities[0];
	for (i=0 ; i<level.num_entities ; i++, ent++) {
		if ( !ent->inuse ) {
			continue;
		}

		// GalaxyRP: [ICARUS] a voice line that has ended completes the script task waiting on it
		RP_IcarusSoundCheck( ent );

		// GalaxyRP: [Scripts] a map model animating under SET_STARTFRAME/ENDFRAME steps a frame
		if ( ent->rpAnimating )
		{
			RP_Animate( ent );
		}

		// clear events that are too old
		if ( level.time - ent->eventTime > EVENT_VALID_MSEC ) {
			if ( ent->s.event ) {
				ent->s.event = 0;	// &= EV_EVENT_BITS;
				if ( ent->client ) {
					ent->client->ps.externalEvent = 0;
					// predicted events should never be set to zero
					//ent->client->ps.events[0] = 0;
					//ent->client->ps.events[1] = 0;
				}
			}
			if ( ent->freeAfterEvent ) {
				// tempEntities or dropped items completely go away after their event
				if (ent->s.eFlags & EF_SOUNDTRACKER)
				{ //don't trigger the event again..
					ent->s.event = 0;
					ent->s.eventParm = 0;
					ent->s.eType = 0;
					ent->eventTime = 0;
				}
				else
				{
					G_FreeEntity( ent );
					continue;
				}
			} else if ( ent->unlinkAfterEvent ) {
				// items that will respawn will hide themselves after their pickup event
				ent->unlinkAfterEvent = qfalse;
				trap->UnlinkEntity( (sharedEntity_t *)ent );
			}
		}

		// temporary entities don't think
		if ( ent->freeAfterEvent ) {
			continue;
		}

		if ( !ent->r.linked && ent->neverFree ) {
			continue;
		}

		if ( ent->s.eType == ET_MISSILE ) {
			G_RunMissile( ent );
			continue;
		}

		if ( ent->s.eType == ET_ITEM || ent->physicsObject ) {
#if 0 //use if body dragging enabled?
			if (ent->s.eType == ET_BODY)
			{ //special case for bodies
				float grav = 3.0f;
				float mass = 0.14f;
				float bounce = 1.15f;

				G_RunExPhys(ent, grav, mass, bounce, qfalse, NULL, 0);
			}
			else
			{
				G_RunItem( ent );
			}
#else
			G_RunItem( ent );
#endif
			continue;
		}

		if ( ent->s.eType == ET_MOVER ) {
			G_RunMover( ent );
			continue;
		}

		//fix for self-deactivating areaportals in Siege
		if ( ent->s.eType == ET_MOVER && level.gametype == GT_SIEGE && level.intermissiontime)
		{
			if ( !Q_stricmp("func_door", ent->classname) && ent->moverState != MOVER_POS1 )
			{
				SetMoverState( ent, MOVER_POS1, level.time );
				if ( ent->teammaster == ent || !ent->teammaster )
				{
					trap->AdjustAreaPortalState( (sharedEntity_t *)ent, qfalse );
				}

				//stop the looping sound
				ent->s.loopSound = 0;
				ent->s.loopIsSoundset = qfalse;
			}
			continue;
		}

		if ( i < MAX_CLIENTS )
		{
			G_CheckClientTimeouts ( ent );

			if (ent->client->inSpaceIndex && ent->client->inSpaceIndex != ENTITYNUM_NONE)
			{ //we're in space, check for suffocating and for exiting
                gentity_t *spacetrigger = &g_entities[ent->client->inSpaceIndex];

				if (!spacetrigger->inuse ||
					!G_PointInBounds(ent->client->ps.origin, spacetrigger->r.absmin, spacetrigger->r.absmax))
				{ //no longer in space then I suppose
                    ent->client->inSpaceIndex = 0;
				}
				else
				{ //check for suffocation
                    if (ent->client->inSpaceSuffocation < level.time)
					{ //suffocate!
						if (ent->health > 0 && ent->takedamage)
						{ //if they're still alive..
							G_Damage(ent, spacetrigger, spacetrigger, NULL, ent->client->ps.origin, Q_irand(50, 70), DAMAGE_NO_ARMOR, MOD_SUICIDE);

							if (ent->health > 0)
							{ //did that last one kill them?
								//play the choking sound
								G_EntitySound(ent, CHAN_VOICE, G_SoundIndex(va( "*choke%d.wav", Q_irand( 1, 3 ) )));

								//make them grasp their throat
								ent->client->ps.forceHandExtend = HANDEXTEND_CHOKE;
								ent->client->ps.forceHandExtendTime = level.time + 2000;
							}
						}

						ent->client->inSpaceSuffocation = level.time + Q_irand(100, 200);
					}
				}
			}

			if (ent->client->isHacking)
			{ //hacking checks
				gentity_t *hacked = &g_entities[ent->client->isHacking];
				vec3_t angDif;

				VectorSubtract(ent->client->ps.viewangles, ent->client->hackingAngles, angDif);

				//keep him in the "use" anim
				if (ent->client->ps.torsoAnim != BOTH_CONSOLE1)
				{
					G_SetAnim( ent, NULL, SETANIM_TORSO, BOTH_CONSOLE1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0 );
				}
				else
				{
					ent->client->ps.torsoTimer = 500;
				}
				ent->client->ps.weaponTime = ent->client->ps.torsoTimer;

				if (!(ent->client->pers.cmd.buttons & BUTTON_USE))
				{ //have to keep holding use
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
				else if (!hacked || !hacked->inuse)
				{ //shouldn't happen, but safety first
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
				else if (!G_PointInBounds( ent->client->ps.origin, hacked->r.absmin, hacked->r.absmax ))
				{ //they stepped outside the thing they're hacking, so reset hacking time
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
				else if (VectorLength(angDif) > 10.0f)
				{ //must remain facing generally the same angle as when we start
					ent->client->isHacking = 0;
					ent->client->ps.hackingTime = 0;
				}
			}

			// zyk: new jetpack debounce and recharge code. It uses the new attribute jetpack_fuel in the pers struct
			//      then we scale and set it to the jetpackFuel attribute to display the fuel bar correctly to the player
			if (ent->client->jetPackOn && ent->client->jetPackDebReduce < level.time)
			{
				int jetpack_debounce_amount = 20;

				if (ent->client->sess.amrpgmode == 2)
				{ // zyk: RPG Mode jetpack skill. Each level decreases fuel debounce
					// GalaxyRP fix: [RPG Class] Bounty Hunter jetpack efficiency bonus removed — rpg_class is permanently 0
					jetpack_debounce_amount -= (ent->client->pers.skill_levels[34] * 3);

					if (ent->client->pers.skill_levels[34] == 3) // zyk: Jetpack Upgrade decreases fuel usage
						jetpack_debounce_amount -= 2;
				}

				if (ent->client->pers.cmd.upmove > 0)
				{ // zyk: jetpack thrusting
					jetpack_debounce_amount *= 2;
				}

				ent->client->pers.jetpack_fuel -= jetpack_debounce_amount;

				// GalaxyRP fix: [RPG Class] Magic Master Improvements jetpack-fuel-from-magic recovery removed — rpg_class is permanently 0

				if (ent->client->pers.jetpack_fuel <= 0)
				{ // zyk: out of fuel. Turn jetpack off
					ent->client->pers.jetpack_fuel = 0;
					Jetpack_Off(ent);
				}

				ent->client->ps.jetpackFuel = ent->client->pers.jetpack_fuel/JETPACK_SCALE;
				ent->client->jetPackDebReduce = level.time + 200; // zyk: JETPACK_DEFUEL_RATE. Original value: 200
			}

			// zyk: Duel Tournament. Do not let anyone enter or anyone leave the globe arena
			if (level.duel_tournament_mode == 4)
			{
				if (duel_tournament_is_duelist(ent) == qtrue && 
					!(ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS)) && // zyk: did not die in his duel yet
					Distance(ent->client->ps.origin, level.duel_tournament_origin) > (DUEL_TOURNAMENT_ARENA_SIZE * rp_duel_tournament_arena_scale.value / 100.0) &&
					ent->health > 0)
				{ // zyk: duelists cannot leave the arena after duel begins
					ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;

					player_die(ent, ent, ent, 100000, MOD_SUICIDE);
				}
				else if ((duel_tournament_is_duelist(ent) == qfalse || 
					(level.duel_players[ent->s.number] != -1 && ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DUEL_TOURNAMENT_LOSS))) && // zyk: not a duelist or died in his duel
					ent->client->sess.sessionTeam != TEAM_SPECTATOR && 
					Distance(ent->client->ps.origin, level.duel_tournament_origin) < (DUEL_TOURNAMENT_ARENA_SIZE * rp_duel_tournament_arena_scale.value / 100.0) &&
					ent->health > 0)
				{ // zyk: other players cannot enter the arena
					ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;

					player_die(ent, ent, ent, 100000, MOD_SUICIDE);
				}
			}

			if (level.melee_mode == 2 && level.melee_players[ent->s.number] != -1)
			{ // zyk: Melee Battle
				// GalaxyRP fix: [Death System] a downed combatant inside the arena is treated as
				// one who fell off it. With death forced (zyk_minigame_forces_death, above) nobody
				// can be knocked down here any more, and melee_battle_prepare() drops anyone who
				// arrived downed, so the only way to reach this is an admin /paralyze on a live
				// combatant -- which would otherwise leave a body on the catwalk that no punch can
				// remove and no timer releases, holding melee_mode_quantity above 1 until the
				// ten-minute timeout. player_die() clears the downed state on its way through.
				if (G_PlayerIsDowned(ent))
				{
					ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
					player_die(ent, ent, ent, 100000, MOD_SUICIDE);
				}
				else if (ent->client->ps.origin[2] < level.melee_mode_origin[2])
				{ // zyk: validating if player fell of the catwalk
					ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
					player_die(ent, ent, ent, 100000, MOD_SUICIDE);
				}
				else if (Distance(ent->client->ps.origin, level.melee_mode_origin) > 1000.0)
				{ // zyk: validating if player is too far from the platform
					ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
					player_die(ent, ent, ent, 100000, MOD_SUICIDE);
				}
			}

			poison_dart_hits(ent);

			// GalaxyRP fix: [Dead Code] the RPG tutorial stepper used to be here, paced by
			// pers.tutorial_timer and gated on PLAYER_STATUS_RPG_TUTORIAL -- a bit nothing set since
			// the account-creation path that started the tutorial went. zyk_text_message(), whose only
			// caller it was, went with it.

			if (ent->client->sess.amrpgmode == 2 && ent->client->sess.sessionTeam != TEAM_SPECTATOR)
			{ // zyk: RPG Mode skills and quests actions. Must be done if player is not at Spectator Mode
				// zyk: Weapon Upgrades
				if (ent->client->ps.weapon == WP_DISRUPTOR && ent->client->pers.skill_levels[21] == 2 && ent->client->ps.weaponTime > (weaponData[WP_DISRUPTOR].fireTime * 1.0)/1.4)
				{
					ent->client->ps.weaponTime = (weaponData[WP_DISRUPTOR].fireTime * 1.0)/1.4;
				}

				// GalaxyRP fix: [RPG Class] Stealth Attacker Unique Skill disruptor firerate bonus removed — rpg_class is permanently 0

				if (ent->client->ps.weapon == WP_REPEATER && ent->client->pers.skill_levels[23] == 2 && ent->client->ps.weaponTime > weaponData[WP_REPEATER].altFireTime/2)
				{
					ent->client->ps.weaponTime = weaponData[WP_REPEATER].altFireTime/2;
				}

				// GalaxyRP fix: [RPG Class] Monk faster-melee and Magic Master Faster Bolt fireTime bonuses removed — rpg_class is permanently 0

				if (ent->client->pers.flame_thrower > level.time && ent->client->cloakDebReduce < level.time)
				{ // zyk: fires the flame thrower
					Player_FireFlameThrower(ent);
				}

				// GalaxyRP fix: [RPG Class] Bounty Hunter/Monk/Stealth Attacker/Duelist rpg_class-gated ability dispatch removed — rpg_class is permanently 0
			}

			if (level.gametype == GT_SIEGE &&
				ent->client->siegeClass != -1 &&
				(bgSiegeClasses[ent->client->siegeClass].classflags & (1<<CFL_STATVIEWER)))
			{ //see if it's time to send this guy an update of extended info
				if (ent->client->siegeEDataSend < level.time)
				{
                    G_SiegeClientExData(ent);
					ent->client->siegeEDataSend = level.time + 1000; //once every sec seems ok
				}
			}

			if((!level.intermissiontime)&&!(ent->client->ps.pm_flags&PMF_FOLLOW) && ent->client->sess.sessionTeam != TEAM_SPECTATOR)
			{
				WP_ForcePowersUpdate(ent, &ent->client->pers.cmd );
				WP_SaberPositionUpdate(ent, &ent->client->pers.cmd);
				WP_SaberStartMissileBlockCheck(ent, &ent->client->pers.cmd);
			}

			if (g_allowNPC.integer)
			{
				//This was originally intended to only be done for client 0.
				//Make sure it doesn't slow things down too much with lots of clients in game.
				NAV_FindPlayerWaypoint(i);
			}

			trap->ICARUS_MaintainTaskManager(ent->s.number);

			G_RunClient( ent );
			continue;
		}
		else if (ent->s.eType == ET_NPC)
		{
			int j;

			// GalaxyRP fix: [Shield] NPCs never reach ClientEndFrame(), which published their shield
			// ceiling -- so it stayed 0 and BG_CanItemBeGrabbed() refused them every shield. Before they
			// think (and so move and touch items) below.
			G_PublishMaxArmor( ent );

			// turn off any expired powerups
			for ( j = 0 ; j < MAX_POWERUPS ; j++ ) {
				if ( ent->client->ps.powerups[ j ] < level.time ) {
					ent->client->ps.powerups[ j ] = 0;
				}
			}

			WP_ForcePowersUpdate(ent, &ent->client->pers.cmd );
			WP_SaberPositionUpdate(ent, &ent->client->pers.cmd);
			WP_SaberStartMissileBlockCheck(ent, &ent->client->pers.cmd);

			poison_dart_hits(ent);

			// GalaxyRP: [Space] leaving a trigger_space, and suffocating in one, for NPCs -- the player
			// half is in the i < MAX_CLIENTS branch above. See RP_NpcSpaceFrame() in g_trigger.c.
			RP_NpcSpaceFrame(ent);

			// GalaxyRP fix: [Quests] a per-frame top-up sat here that kept PW_FORCE_BOON alive on
			// "artifact holder" npcs. It was gated on universe_quest_artifact_holder_id != -1, and the only
			// value ever assigned to that field anywhere in the tree was -1, so the test never passed.

			// zyk: npcs cannot enter the Duel Tournament arena
			if (level.duel_tournament_mode == 4 && 
				Distance(ent->r.currentOrigin, level.duel_tournament_origin) < (DUEL_TOURNAMENT_ARENA_SIZE * rp_duel_tournament_arena_scale.value / 100.0))
			{
				ent->health = 0;
				ent->client->ps.stats[STAT_HEALTH] = 0;
				if (ent->die)
				{
					ent->die(ent, ent, ent, 100, MOD_UNKNOWN);
				}
			}

			// zyk: abilities of custom quest npcs
			// GalaxyRP fix: [Magic] the custom-quest-NPC ability dispatch used to be here, 211 lines of
			// it: a random-power chain for NPCs carrying PLAYER_STATUS_CUSTOM_QUEST_NPC, picking magic
			// powers out of sess.selected_left_special_power and special powers out of
			// sess.selected_special_power.
			//
			// Nothing has ever been able to set that status bit. The name appears exactly twice in the
			// whole tree -- its enum entry in g_local.h and the single read that used to be on this
			// line -- there is no "player_statuses |= (1 << PLAYER_STATUS_CUSTOM_QUEST_NPC)" anywhere,
			// every player_statuses set in the tree uses a literal PLAYER_STATUS_* name rather than a
			// variable bit index, and player_statuses is never restored from the database or the session
			// string -- it is only ever assigned 0 wholesale. So the bit sat at the zero ClientConnect's
			// memset gives it and the block never ran once.
			//
			// Removing it orphaned twelve functions further up this file, which went with it: chaos_power,
			// elemental_attack, force_scream, healing_area, immunity_power, lightning_dome,
			// magic_explosion, time_power, ultra_drain, zyk_force_storm, zyk_no_attack and
			// zyk_super_beam. This block was their only caller; the quest_mage chain that used to sit just
			// below called twenty-seven OTHER effect functions and none of these twelve (that chain has
			// since gone too, and the twenty-seven, kept for a while as reference code, are now deleted
			// as well). None of the
			// twelve was ever taken by address (the zyk_force_storm / zyk_super_beam matches elsewhere are entity
			// targetname strings that happen to share the names, not function pointers).
			//
			// Consequences worth knowing, all of them pre-existing dead weight rather than new:
			// quest_power_status bits 1, 2 and 26 had no setter other than chaos_power, time_power and
			// elemental_attack, so those three bits are now permanently 0 and their ~21 readers across
			// six files are permanently-false tests. Bits 0 and 5 keep other setters. The entity
			// targetname handling for "zyk_force_storm" and "zyk_super_beam" lives in g_misc.c, and
			// zyk_lightning_dome_detonate() (g_weapon.c) and zyk_spawn_ice_element() lost their last
			// callers. All left in place deliberately, to be judged on their own rather than swept up
			// behind this one.
			//
			// Two corrections to the above, made in a later pass. (1) This note also named g_combat.c
			// as holding some of that targetname handling; neither name appears in that file. (2) It
			// said the handling "can no longer be reached either", which was wrong and was the reason
			// a real bug sat unnoticed: the targetname handling is reached by any fx_runner carrying
			// the name, which /entadd, /entedit, a .ent preset or a map can all supply. What the loss
			// of zyk_super_beam() actually removed was the only writer of ent->parent on such an
			// entity -- so the handler ran with a NULL parent and dereferenced it. See the guard and
			// the full account in fx_runner_think() (g_misc.c).

			// GalaxyRP fix: [Guardian] quest guardians special abilities dispatch removed here -- guardian_mode/guardian_invoked_by_id are permanently dead (spawn_boss has no callers); the ~40 magic-power helper functions it called (healing_water, water_splash, ultra_strength, ice_block, earthquake, magic_shield, etc.) were kept as zero-caller code for a while and have since been deleted -- see the note below

			// GalaxyRP fix: [Guardian] ymir_boss and thor_boss ability sub-chains were removed here earlier,
			// and the quest_mage chain that used to follow them has now gone too -- 115 lines picking one of
			// twenty-seven magic powers at random (ultra_strength, poison_mushrooms, water_splash, hurricane,
			// rock_fall, ice_block, ...) every few seconds for any NPC whose NPC_type was "quest_mage".
			//
			// That NPC type no longer exists: it was defined in zyk_quest_enemies.npc, one of the three quest
			// asset files deleted with the magic orphans, so NPC_ParseParms() now fails the name outright and
			// the condition could never be true again. This chain was the last live entry point into the
			// magic effect system.
			//
			// The twenty-seven effect functions themselves were kept for a while as material to repurpose,
			// then deleted in the magic cleanup along with the quest_power_status bitfield they wrote and
			// every reader of it (G_Damage, ClientThink_real, the force-power checks, the flame thrower's
			// Flame Burst bonus). Anyone reviving one of those powers starts from the git history, and
			// needs to give it an owner link and a lifetime first: level.special_power_effects[] and
			// clear_special_power_effect(), which used to do that, went with the chain.
		}

		// zyk: added check for mind control on npcs here. NPCs being mind controlled cant think
		if (!(ent && ent->client && ent->client->pers.being_mind_controlled != -1))
			G_RunThink( ent );

		if (g_allowNPC.integer)
		{
			ClearNPCGlobals();
		}
	}

	// GalaxyRP: [Logical Entities] the upper region. A logical entity is never a client, missile,
	// mover, item or NPC and is never linked, so the only thing the loop above would do for it is
	// G_RunThink() -- and that is all it gets. Runs after the networked loop so a logical relay
	// fired by a networked trigger this frame thinks this frame, as it did when it was networked.
	ent = g_logicalents;
	for ( i = 0; i < level.num_logicalents; i++, ent++ ) {
		if ( !ent->inuse ) {
			continue;
		}
		G_RunThink( ent );
	}
#ifdef _G_FRAME_PERFANAL
	iTimer_ItemRun = trap->PrecisionTimer_End(timer_ItemRun);
#endif

	SiegeCheckTimers();

#ifdef _G_FRAME_PERFANAL
	trap->PrecisionTimer_Start(&timer_ROFF);
#endif
	trap->ROFF_UpdateEntities();
#ifdef _G_FRAME_PERFANAL
	iTimer_ROFF = trap->PrecisionTimer_End(timer_ROFF);
#endif



#ifdef _G_FRAME_PERFANAL
	trap->PrecisionTimer_Start(&timer_ClientEndframe);
#endif
	// perform final fixups on the players
	ent = &g_entities[0];
	for (i=0 ; i < level.maxclients ; i++, ent++ ) {
		if ( ent->inuse ) {
			ClientEndFrame( ent );
		}
	}
	// GalaxyRP: [Phase] and the same for the NPCs /npc effect has phased (g_active.c)
	RP_PhaseNpcEndFrame();
#ifdef _G_FRAME_PERFANAL
	iTimer_ClientEndframe = trap->PrecisionTimer_End(timer_ClientEndframe);
#endif



#ifdef _G_FRAME_PERFANAL
	trap->PrecisionTimer_Start(&timer_GameChecks);
#endif
	// see if it is time to do a tournament restart
	CheckTournament();

	// see if it is time to end the level
	CheckExitRules();

	// update to team status?
	CheckTeamStatus();

	// cancel vote if timed out
	CheckVote();

	// check team votes
	CheckTeamVote( TEAM_RED );
	CheckTeamVote( TEAM_BLUE );

	// for tracking changes
	CheckCvars();

#ifdef _G_FRAME_PERFANAL
	iTimer_GameChecks = trap->PrecisionTimer_End(timer_GameChecks);
#endif



#ifdef _G_FRAME_PERFANAL
	trap->PrecisionTimer_Start(&timer_Queues);
#endif
	// GalaxyRP: [Entity System] which clients each sound-set speaker is sent to this frame
	RP_SpeakerAudienceFrame();

	//At the end of the frame, send out the ghoul2 kill queue, if there is one
	G_SendG2KillQueue();

	if (gQueueScoreMessage)
	{
		if (gQueueScoreMessageTime < level.time)
		{
			SendScoreboardMessageToAllClients();

			gQueueScoreMessageTime = 0;
			gQueueScoreMessage = 0;
		}
	}
#ifdef _G_FRAME_PERFANAL
	iTimer_Queues = trap->PrecisionTimer_End(timer_Queues);
#endif



#ifdef _G_FRAME_PERFANAL
	Com_Printf("---------------\nItemRun: %i\nROFF: %i\nClientEndframe: %i\nGameChecks: %i\nQueues: %i\n---------------\n",
		iTimer_ItemRun,
		iTimer_ROFF,
		iTimer_ClientEndframe,
		iTimer_GameChecks,
		iTimer_Queues);
#endif

	g_LastFrameTime = level.time;
}

const char *G_GetStringEdString(char *refSection, char *refName)
{
	/*
	static char text[1024]={0};
	trap->SP_GetStringTextString(va("%s_%s", refSection, refName), text, sizeof(text));
	return text;
	*/

	//Well, it would've been lovely doing it the above way, but it would mean mixing
	//languages for the client depending on what the server is. So we'll mark this as
	//a stringed reference with @@@ and send the refname to the client, and when it goes
	//to print it will get scanned for the stringed reference indication and dealt with
	//properly.
	static char text[1024]={0};
	Com_sprintf(text, sizeof(text), "@@@%s", refName);
	return text;
}

static void G_SpawnRMGEntity( void ) {
	if ( G_ParseSpawnVars( qfalse ) )
		G_SpawnGEntityFromSpawnVars( qfalse );
}

static void _G_ROFF_NotetrackCallback( int entID, const char *notetrack ) {
	G_ROFF_NotetrackCallback( &g_entities[entID], notetrack );
}

static int G_ICARUS_PlaySound( void ) {
	T_G_ICARUS_PLAYSOUND *sharedMem = &gSharedBuffer.playSound;
	return Q3_PlaySound( sharedMem->taskID, sharedMem->entID, sharedMem->name, sharedMem->channel );
}
static qboolean G_ICARUS_Set( void ) {
	T_G_ICARUS_SET *sharedMem = &gSharedBuffer.set;
	return Q3_Set( sharedMem->taskID, sharedMem->entID, sharedMem->type_name, sharedMem->data );
}
static void G_ICARUS_Lerp2Pos( void ) {
	T_G_ICARUS_LERP2POS *sharedMem = &gSharedBuffer.lerp2Pos;
	Q3_Lerp2Pos( sharedMem->taskID, sharedMem->entID, sharedMem->origin, sharedMem->nullAngles ? NULL : sharedMem->angles, sharedMem->duration );
}
static void G_ICARUS_Lerp2Origin( void ) {
	T_G_ICARUS_LERP2ORIGIN *sharedMem = &gSharedBuffer.lerp2Origin;
	Q3_Lerp2Origin( sharedMem->taskID, sharedMem->entID, sharedMem->origin, sharedMem->duration );
}
static void G_ICARUS_Lerp2Angles( void ) {
	T_G_ICARUS_LERP2ANGLES *sharedMem = &gSharedBuffer.lerp2Angles;
	Q3_Lerp2Angles( sharedMem->taskID, sharedMem->entID, sharedMem->angles, sharedMem->duration );
}
static int G_ICARUS_GetTag( void ) {
	T_G_ICARUS_GETTAG *sharedMem = &gSharedBuffer.getTag;
	return Q3_GetTag( sharedMem->entID, sharedMem->name, sharedMem->lookup, sharedMem->info );
}
static void G_ICARUS_Lerp2Start( void ) {
	T_G_ICARUS_LERP2START *sharedMem = &gSharedBuffer.lerp2Start;
	Q3_Lerp2Start( sharedMem->entID, sharedMem->taskID, sharedMem->duration );
}
static void G_ICARUS_Lerp2End( void ) {
	T_G_ICARUS_LERP2END *sharedMem = &gSharedBuffer.lerp2End;
	Q3_Lerp2End( sharedMem->entID, sharedMem->taskID, sharedMem->duration );
}
static void G_ICARUS_Use( void ) {
	T_G_ICARUS_USE *sharedMem = &gSharedBuffer.use;
	Q3_Use( sharedMem->entID, sharedMem->target );
}
static void G_ICARUS_Kill( void ) {
	T_G_ICARUS_KILL *sharedMem = &gSharedBuffer.kill;
	Q3_Kill( sharedMem->entID, sharedMem->name );
}
static void G_ICARUS_Remove( void ) {
	T_G_ICARUS_REMOVE *sharedMem = &gSharedBuffer.remove;
	Q3_Remove( sharedMem->entID, sharedMem->name );
}
static void G_ICARUS_Play( void ) {
	T_G_ICARUS_PLAY *sharedMem = &gSharedBuffer.play;
	Q3_Play( sharedMem->taskID, sharedMem->entID, sharedMem->type, sharedMem->name );
}
static int G_ICARUS_GetFloat( void ) {
	T_G_ICARUS_GETFLOAT *sharedMem = &gSharedBuffer.getFloat;
	return Q3_GetFloat( sharedMem->entID, sharedMem->type, sharedMem->name, &sharedMem->value );
}
static int G_ICARUS_GetVector( void ) {
	T_G_ICARUS_GETVECTOR *sharedMem = &gSharedBuffer.getVector;
	return Q3_GetVector( sharedMem->entID, sharedMem->type, sharedMem->name, sharedMem->value );
}
static int G_ICARUS_GetString( void ) {
	T_G_ICARUS_GETSTRING *sharedMem = &gSharedBuffer.getString;
	char *crap = NULL; //I am sorry for this -rww
	char **morecrap = &crap; //and this
	int r = Q3_GetString( sharedMem->entID, sharedMem->type, sharedMem->name, morecrap );

	if ( crap )
		strcpy( sharedMem->value, crap );

	return r;
}
static void G_ICARUS_SoundIndex( void ) {
	T_G_ICARUS_SOUNDINDEX *sharedMem = &gSharedBuffer.soundIndex;
	G_SoundIndex( sharedMem->filename );
}
static int G_ICARUS_GetSetIDForString( void ) {
	T_G_ICARUS_GETSETIDFORSTRING *sharedMem = &gSharedBuffer.getSetIDForString;
	return GetIDForString( setTable, sharedMem->string );
}
static qboolean G_NAV_ClearPathToPoint( int entID, vec3_t pmins, vec3_t pmaxs, vec3_t point, int clipmask, int okToHitEnt ) {
	return NAV_ClearPathToPoint( &g_entities[entID], pmins, pmaxs, point, clipmask, okToHitEnt );
}
static qboolean G_NPC_ClearLOS2( int entID, const vec3_t end ) {
	return NPC_ClearLOS2( &g_entities[entID], end );
}
static qboolean	G_NAV_CheckNodeFailedForEnt( int entID, int nodeNum ) {
	return NAV_CheckNodeFailedForEnt( &g_entities[entID], nodeNum );
}

/*
============
GetModuleAPI
============
*/

gameImport_t *trap = NULL;

Q_EXPORT gameExport_t* QDECL GetModuleAPI( int apiVersion, gameImport_t *import )
{
	static gameExport_t ge = {0};

	assert( import );
	trap = import;
	Com_Printf	= trap->Print;
	Com_Error	= trap->Error;

	memset( &ge, 0, sizeof( ge ) );

	if ( apiVersion != GAME_API_VERSION ) {
		trap->Print( "Mismatched GAME_API_VERSION: expected %i, got %i\n", GAME_API_VERSION, apiVersion );
		return NULL;
	}

	ge.InitGame							= G_InitGame;
	ge.ShutdownGame						= G_ShutdownGame;
	ge.ClientConnect					= ClientConnect;
	ge.ClientBegin						= ClientBegin;
	ge.ClientUserinfoChanged			= ClientUserinfoChanged;
	ge.ClientDisconnect					= ClientDisconnect;
	ge.ClientCommand					= ClientCommand;
	ge.ClientThink						= ClientThink;
	ge.RunFrame							= G_RunFrame;
	ge.ConsoleCommand					= ConsoleCommand;
	ge.BotAIStartFrame					= BotAIStartFrame;
	ge.ROFF_NotetrackCallback			= _G_ROFF_NotetrackCallback;
	ge.SpawnRMGEntity					= G_SpawnRMGEntity;
	ge.ICARUS_PlaySound					= G_ICARUS_PlaySound;
	ge.ICARUS_Set						= G_ICARUS_Set;
	ge.ICARUS_Lerp2Pos					= G_ICARUS_Lerp2Pos;
	ge.ICARUS_Lerp2Origin				= G_ICARUS_Lerp2Origin;
	ge.ICARUS_Lerp2Angles				= G_ICARUS_Lerp2Angles;
	ge.ICARUS_GetTag					= G_ICARUS_GetTag;
	ge.ICARUS_Lerp2Start				= G_ICARUS_Lerp2Start;
	ge.ICARUS_Lerp2End					= G_ICARUS_Lerp2End;
	ge.ICARUS_Use						= G_ICARUS_Use;
	ge.ICARUS_Kill						= G_ICARUS_Kill;
	ge.ICARUS_Remove					= G_ICARUS_Remove;
	ge.ICARUS_Play						= G_ICARUS_Play;
	ge.ICARUS_GetFloat					= G_ICARUS_GetFloat;
	ge.ICARUS_GetVector					= G_ICARUS_GetVector;
	ge.ICARUS_GetString					= G_ICARUS_GetString;
	ge.ICARUS_SoundIndex				= G_ICARUS_SoundIndex;
	ge.ICARUS_GetSetIDForString			= G_ICARUS_GetSetIDForString;
	ge.NAV_ClearPathToPoint				= G_NAV_ClearPathToPoint;
	ge.NPC_ClearLOS2					= G_NPC_ClearLOS2;
	ge.NAVNEW_ClearPathBetweenPoints	= NAVNEW_ClearPathBetweenPoints;
	ge.NAV_CheckNodeFailedForEnt		= G_NAV_CheckNodeFailedForEnt;
	ge.NAV_EntIsUnlockedDoor			= G_EntIsUnlockedDoor;
	ge.NAV_EntIsDoor					= G_EntIsDoor;
	ge.NAV_EntIsBreakable				= G_EntIsBreakable;
	ge.NAV_EntIsRemovableUsable			= G_EntIsRemovableUsable;
	ge.NAV_FindCombatPointWaypoints		= CP_FindCombatPointWaypoints;
	ge.BG_GetItemIndexByTag				= BG_GetItemIndexByTag;

	return &ge;
}

/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .q3vm file
================
*/
Q_EXPORT intptr_t vmMain( int command, intptr_t arg0, intptr_t arg1, intptr_t arg2, intptr_t arg3, intptr_t arg4,
	intptr_t arg5, intptr_t arg6, intptr_t arg7, intptr_t arg8, intptr_t arg9, intptr_t arg10, intptr_t arg11 )
{
	switch ( command ) {
	case GAME_INIT:
		G_InitGame( arg0, arg1, arg2 );
		return 0;

	case GAME_SHUTDOWN:
		G_ShutdownGame( arg0 );
		return 0;

	case GAME_CLIENT_CONNECT:
		return (intptr_t)ClientConnect( arg0, arg1, arg2 );

	case GAME_CLIENT_THINK:
		ClientThink( arg0, NULL );
		return 0;

	case GAME_CLIENT_USERINFO_CHANGED:
		ClientUserinfoChanged( arg0 );
		return 0;

	case GAME_CLIENT_DISCONNECT:
		ClientDisconnect( arg0 );
		return 0;

	case GAME_CLIENT_BEGIN:
		ClientBegin( arg0, qtrue );
		return 0;

	case GAME_CLIENT_COMMAND:
		ClientCommand( arg0 );
		return 0;

	case GAME_RUN_FRAME:
		G_RunFrame( arg0 );
		return 0;

	case GAME_CONSOLE_COMMAND:
		return ConsoleCommand();

	case BOTAI_START_FRAME:
		return BotAIStartFrame( arg0 );

	case GAME_ROFF_NOTETRACK_CALLBACK:
		_G_ROFF_NotetrackCallback( arg0, (const char *)arg1 );
		return 0;

	case GAME_SPAWN_RMG_ENTITY:
		G_SpawnRMGEntity();
		return 0;

	case GAME_ICARUS_PLAYSOUND:
		return G_ICARUS_PlaySound();

	case GAME_ICARUS_SET:
		return G_ICARUS_Set();

	case GAME_ICARUS_LERP2POS:
		G_ICARUS_Lerp2Pos();
		return 0;

	case GAME_ICARUS_LERP2ORIGIN:
		G_ICARUS_Lerp2Origin();
		return 0;

	case GAME_ICARUS_LERP2ANGLES:
		G_ICARUS_Lerp2Angles();
		return 0;

	case GAME_ICARUS_GETTAG:
		return G_ICARUS_GetTag();

	case GAME_ICARUS_LERP2START:
		G_ICARUS_Lerp2Start();
		return 0;

	case GAME_ICARUS_LERP2END:
		G_ICARUS_Lerp2End();
		return 0;

	case GAME_ICARUS_USE:
		G_ICARUS_Use();
		return 0;

	case GAME_ICARUS_KILL:
		G_ICARUS_Kill();
		return 0;

	case GAME_ICARUS_REMOVE:
		G_ICARUS_Remove();
		return 0;

	case GAME_ICARUS_PLAY:
		G_ICARUS_Play();
		return 0;

	case GAME_ICARUS_GETFLOAT:
		return G_ICARUS_GetFloat();

	case GAME_ICARUS_GETVECTOR:
		return G_ICARUS_GetVector();

	case GAME_ICARUS_GETSTRING:
		return G_ICARUS_GetString();

	case GAME_ICARUS_SOUNDINDEX:
		G_ICARUS_SoundIndex();
		return 0;

	case GAME_ICARUS_GETSETIDFORSTRING:
		return G_ICARUS_GetSetIDForString();

	case GAME_NAV_CLEARPATHTOPOINT:
		return G_NAV_ClearPathToPoint( arg0, (float *)arg1, (float *)arg2, (float *)arg3, arg4, arg5 );

	case GAME_NAV_CLEARLOS:
		return G_NPC_ClearLOS2( arg0, (const float *)arg1 );

	case GAME_NAV_CLEARPATHBETWEENPOINTS:
		return NAVNEW_ClearPathBetweenPoints((float *)arg0, (float *)arg1, (float *)arg2, (float *)arg3, arg4, arg5);

	case GAME_NAV_CHECKNODEFAILEDFORENT:
		return NAV_CheckNodeFailedForEnt(&g_entities[arg0], arg1);

	case GAME_NAV_ENTISUNLOCKEDDOOR:
		return G_EntIsUnlockedDoor(arg0);

	case GAME_NAV_ENTISDOOR:
		return G_EntIsDoor(arg0);

	case GAME_NAV_ENTISBREAKABLE:
		return G_EntIsBreakable(arg0);

	case GAME_NAV_ENTISREMOVABLEUSABLE:
		return G_EntIsRemovableUsable(arg0);

	case GAME_NAV_FINDCOMBATPOINTWAYPOINTS:
		CP_FindCombatPointWaypoints();
		return 0;

	case GAME_GETITEMINDEXBYTAG:
		return BG_GetItemIndexByTag(arg0, arg1);
	}

	return -1;
}
