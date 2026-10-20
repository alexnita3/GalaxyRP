#include "../../codemp/game/g_local.h"
#include "sqlite/sqlite3.h"

/*
=================
HELPER METHODS for interacting with the database

The methods in the area are used to split up database actions into multiple helper methods, that can be used in multiple areas without code dupplication.
=================
*/

void run_db_query(const char* query, sqlite3* db)
{
	char* zErrMsg = NULL;
	int rc = sqlite3_exec(db, query, NULL, NULL, &zErrMsg);

	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}
}

/*
=====================================================================
Accounts Table Methods
=====================================================================
*/

// GalaxyRP (Alex): [Database] SELECT This method selects a row form the accounts table, and assigns the values to the entity.
qboolean select_accounts_table_row(gentity_t* ent, char* username, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	rc = sqlite3_prepare_v2(db, "SELECT AccountID, PlayerSettings, AdminLevel FROM Accounts WHERE Username = ? COLLATE NOCASE", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return qfalse;
	}
	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return qfalse;
	}
	if (rc == SQLITE_ROW)
	{
		ent->client->sess.accountID = sqlite3_column_int(stmt, 0);
		ent->client->pers.player_settings = sqlite3_column_int(stmt, 1);
		ent->client->pers.bitvalue = sqlite3_column_int(stmt, 2);
		sqlite3_finalize(stmt);

		return qtrue;
	}

	sqlite3_finalize(stmt);
	return qfalse;
}

// GalaxyRP (Alex): [Database] SELECT This method selects the id of an account going by the username provided. Usernames should be unique.
int select_account_id_from_username(gentity_t* ent, char* username, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	// GalaxyRP fix: [stability] accountID used to be declared uninitialized here and was only ever
	// assigned inside the "row found" branch below -- the not-found (rc == SQLITE_DONE) fallthrough
	// returned this uninitialized stack value as the function's result instead of a real sentinel.
	// Initialize it to -1 (the same "not found" sentinel select_char_id_using_char_name() elsewhere in
	// this file already uses) so a genuine not-found result is unambiguous.
	int accountID = -1;

	// GalaxyRP fix: [security] same va("...%s...")-into-SQL-text issue as select_accounts_table_row()
	// above -- bind username as a parameter instead of splicing it into the query text.
	// GalaxyRP fix: [stability] both error paths used to sqlite3_close(db) before returning. This
	// function does not own that handle -- it is passed one its caller opened and goes on using --
	// so closing it here left the caller working through a closed connection. Harmless while this
	// had no callers at all, which is how it survived; it has one now (the duplicate-session drop in
	// select_account_and_default_character_data), and that caller runs a further query afterwards.
	// Report and return the sentinel; let whoever opened the connection close it.
	rc = sqlite3_prepare_v2(db, "SELECT AccountID FROM Accounts WHERE Username = ? COLLATE NOCASE", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}
	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}
	if (rc == SQLITE_ROW)
	{
		accountID = sqlite3_column_int(stmt, 0);
	}

	// GalaxyRP fix: [stability] this used to only finalize inside the "row found" branch above -- the
	// not-found fallthrough left stmt unfinalized, the same leak class fixed elsewhere in this file.
	// Finalizing unconditionally here (mirroring select_char_id_using_char_name()'s style) closes that
	// leak on both paths.
	sqlite3_finalize(stmt);

	return accountID;
}

// GalaxyRP (Alex): [Database] INSERT This method inserts a new row into the accounts table, using the username and password provided, and default values for everything else.
qboolean insert_accounts_table_row(gentity_t* ent, char* username, char* password, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	// GalaxyRP fix: [security] this used to go through run_db_query(), which executes a fully
	// pre-formatted SQL string via sqlite3_exec() with no parameter binding -- username and password
	// (the two values a player directly controls via /new) were spliced straight into the INSERT
	// text via va("...%s...%s..."). A quote in either value could inject arbitrary SQL. Prepare/bind/
	// step directly instead so the values can never be interpreted as SQL syntax.
	// GalaxyRP fix: [Database] PlayerSettings used to be kept as an explicit literal 0 here rather
	// than bound -- a first pass at this fix bound ent->client->pers.player_settings instead, on the
	// reasoning that "a brand new account's player_settings is always still 0 at this point". That's
	// wrong: /new (this function's caller) has no guard requiring the connection be logged out first,
	// and /logout doesn't clear pers.player_settings the way it already clears pers.bitvalue -- so a
	// player who is logged into account A, runs /logout, then /new B, still has account A's settings
	// bitmask sitting in pers.player_settings when this INSERT runs, and binding it would seed the
	// brand new account B with a stranger's (well, their own other account's) settings. A new account
	// has no settings history to inherit; a fixed, hardcoded default is unconditionally correct here
	// regardless of what this connection was doing before. See the UPDATE sites below
	// (update_accounts_table_row_with_current_values() and update_current_character_and_account())
	// for the actual PlayerSettings-never-saved bug -- those bind the real value because they update
	// an EXISTING row for the CURRENTLY logged-in player, where pers.player_settings is guaranteed
	// current.
	// GalaxyRP fix: [Settings] that hardcoded default is no longer plain 0 -- bit 13 (Admin Protect) is
	// now set (bit 5, then "Language", was left clear for English; see below for its new use). Bit 13 is inverted
	// (clear == ON, set == OFF -- see the status-line block in Cmd_Settings_f), so a brand new account
	// used to come out of this INSERT with Admin Protect showing ON by default, which made no sense:
	// a fresh account has no admin permissions at all (AdminLevel comes from
	// rp_default_account_permissions, "0" by default) and, as of the fix in Cmd_Settings_f, could
	// never have turned Admin Protect on for itself anyway (that now requires the "Give Admin" admin
	// command). Setting bit 13 here makes the stored default match that: Admin Protect OFF until an
	// admin senior enough to grant admin commands turns it on for themselves.
	rc = sqlite3_prepare_v2(db, "INSERT INTO Accounts(Username, Password, AdminLevel, PlayerSettings, DefaultChar) VALUES(?, ?, ?, ?, ?)", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return qfalse;
	}
	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, password, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, rp_default_account_permissions.integer);
	// DAJ_RP: [Settings] bit 6 (Use Hint, /settings 4) is deliberately NOT set here: it is inverted
	// (clear == ON), so leaving it clear makes new accounts start with the Use Hint ON. (It used to be
	// set here, starting new accounts OFF.) Existing accounts keep whatever they have. The built-in
	// "admin" account created by InitializeGalaxyRpTables() (g_main.c) inserts PlayerSettings '0'
	// literally, so it starts with every setting ON, Use Hint included.
	// DAJ_RP: [Settings] bit 5 (Sense Health Toggle, /settings 1) is set for the same reason as bit 13:
	// inverted, and meant to start OFF. It used to be "Language", left clear here for English.
	// DAJ_RP: [Settings] and bit 16 (Ignore Chat Distance, /settings 5), inverted too and meant to start OFF.
	// DAJ_RP: [Entity Bounds] and bit 20 (Entity Bounds, /settings 6), the same.
	sqlite3_bind_int(stmt, 4, (1 << 13) | (1 << 5) | (1 << 16) | (1 << 20)); // Admin Protect + Sense Health Toggle + Ignore Chat Distance + Entity Bounds OFF by default; Use Hint (bit 6) ON
	sqlite3_bind_text(stmt, 5, username, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	// GalaxyRP fix: [stability] this used to never check the INSERT's own result -- an error here (most
	// notably SQLITE_CONSTRAINT from the new Accounts.Username UNIQUE index, see the doc comment above)
	// was logged server-side and then completely ignored, letting the caller carry on as though the row
	// existed. Report it instead.
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return qfalse;
	}
	sqlite3_finalize(stmt);

	return qtrue;
}

// GalaxyRP (Alex): [Database] SELECT This method returns the number of accounts with one specific username. Useful for checking if a username is unique.
int select_number_of_accounts_with_username(gentity_t* ent, char* username, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	int numberOfAccounts = 0;

	// GalaxyRP fix: [security] this used to build the query text via va("...Username='%s'", username)
	// -- i.e. splicing the raw username straight into the SQL string. A username containing a single
	// quote (the SQL string delimiter) could break out of the literal and inject arbitrary SQL (e.g.
	// `' OR '1'='1`), executed with this game server's full database privileges. This is the very
	// first query both /login and /new run against attacker-supplied input, so it's reachable
	// unauthenticated. Bind the value as a parameter instead, so it can never be interpreted as SQL
	// syntax regardless of content.
	rc = sqlite3_prepare_v2(db, "SELECT count(Username) FROM Accounts WHERE Username = ? COLLATE NOCASE", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 1;
	}
	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 1;
	}
	if (rc == SQLITE_ROW)
	{

		numberOfAccounts = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);

		return numberOfAccounts;

	}

	// GalaxyRP fix: [stability] this fallthrough (rc == SQLITE_DONE, no row at all) used to return
	// without ever finalizing stmt -- effectively dead in practice for a bare COUNT(*) query (which
	// always yields exactly one row), but the same missing-finalize class of bug fixed elsewhere in this
	// file. Finalized here too so sqlite3_close() on this connection can't be silently left BUSY by it.
	sqlite3_finalize(stmt);
	return 1;
}

// GalaxyRP (Alex): [Database] UPDATE This method updates an accounts table row with information contained within the entity with which it's called.
void update_accounts_table_row_with_current_values(gentity_t* ent) {
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

	// GalaxyRP fix: [security] this used to go through run_db_query() with the password (and
	// character name) spliced straight into the UPDATE text via va("...%s..."). This is the query
	// /changepassword ultimately writes through, so a password containing a quote could inject
	// arbitrary SQL. Bind every value as a parameter instead.
	// GalaxyRP fix: [Database] PlayerSettings was hardcoded as the literal '0' instead of being
	// bound, on every single write to this table -- since this function is what /settings' toggle
	// commands ultimately save through (via save_account(ent, qfalse)), every settings change was
	// silently reset back to 0 in the DB on the very next save, and came back off on the player's
	// next login. Bind the player's actual in-memory bitmask instead.
	rc = sqlite3_prepare_v2(db, "UPDATE Accounts SET PlayerSettings=?, AdminLevel=?, DefaultChar=?, Password=? WHERE AccountID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return;
	}
	sqlite3_bind_int(stmt, 1, ent->client->pers.player_settings);
	sqlite3_bind_int(stmt, 2, ent->client->pers.bitvalue);
	sqlite3_bind_text(stmt, 3, ent->client->sess.rpgchar, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, ent->client->pers.password, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 5, ent->client->sess.accountID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	sqlite3_close(db);

	return;
}

// GalaxyRP (Alex): [Database] UPDATE This method updated a accounts table row with information contained within the entity with which it's called. (NEEDS A CHAR NAME AND FOR THE USER TO BE LOGGED IN)
void update_accounts_table_row_with_default_char(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	// GalaxyRP fix: [security] same run_db_query()-with-va()-splice issue as
	// update_accounts_table_row_with_current_values() above -- bind character_name instead. Reachable
	// via /new (Cmd_Register_F -> select_player_character -> here) with the new account's own
	// username used as the character name.
	rc = sqlite3_prepare_v2(db, "UPDATE Accounts SET DefaultChar=? WHERE AccountID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	sqlite3_bind_text(stmt, 1, character_name, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, ent->client->sess.accountID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	return;
}

qboolean is_password_correct(gentity_t* ent, char* username, char* password, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	rc = sqlite3_prepare_v2(db, "SELECT Password FROM Accounts WHERE Username = ? COLLATE NOCASE", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return qfalse;
	}
	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return qfalse;
	}
	if (rc == SQLITE_ROW)
	{
		char comparisonPassword[MAX_STRING_CHARS];
		Q_strncpyz(comparisonPassword, (const char*)sqlite3_column_text(stmt, 0), sizeof(comparisonPassword));
		sqlite3_finalize(stmt);

		if (strcmp(password, comparisonPassword) == 0) {
			return qtrue;
		}

		return qfalse;
	}

	sqlite3_finalize(stmt);
	return qfalse;
}

/*
=====================================================================
Characters Table Methods
=====================================================================
*/

void update_credits_value(gentity_t* ent) {
	sqlite3* db;
	int rc;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	char update_char_query[] = "UPDATE Characters SET Credits='%i' WHERE CharID='%i'";
	run_db_query(va(update_char_query,
		ent->client->pers.credits,
		ent->client->pers.CharID
	), db);

	sqlite3_close(db);

	return;
}

// GalaxyRP (Alex): [Database] INSERT This method inserts a new row in the character table, with default values. ASSUMES PLAYER IS ALREADY LOGGED IN.
qboolean insert_chars_table_row(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt, int* out_charID) {
	rc = sqlite3_prepare_v2(db, "INSERT INTO Characters(AccountID, Credits, Level, ModelScale, Name, SkillPoints, Description, NetName, ModelName, xp, saberTwoModel) VALUES(?, '100', '1', '100', ?, '1', 'Nothing to show.', 'DefaultName', 'kyle', 0, 'none')", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return qfalse;
	}
	sqlite3_bind_int(stmt, 1, ent->client->sess.accountID);
	sqlite3_bind_text(stmt, 2, character_name, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return qfalse;
	}
	sqlite3_finalize(stmt);

	if (out_charID)
	{
		*out_charID = (int)sqlite3_last_insert_rowid(db);
	}

	return qtrue;
}

// GalaxyRP (Alex): [Database] SELECT This method returns the number of characters that exist with one name. (Useful for preventing duplicates)
int select_number_of_characters_with_name(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	rc = sqlite3_prepare_v2(db, "SELECT count(CharID) FROM Characters WHERE AccountID=? AND Name = ? COLLATE NOCASE", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 0;
	}
	sqlite3_bind_int(stmt, 1, ent->client->sess.accountID);
	sqlite3_bind_text(stmt, 2, character_name, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 0;
	}
	if (rc == SQLITE_ROW)
	{
		int numberOfChars;

		numberOfChars = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);

		return numberOfChars;

	}

	sqlite3_finalize(stmt);
	return 0;
}

// GalaxyRP (Alex): [Database] Counts the number of total characters an account has.
int select_number_of_characters(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {

	rc = sqlite3_prepare_v2(db, "SELECT count(CharID) FROM Characters WHERE AccountID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 0;
	}
	sqlite3_bind_int(stmt, 1, ent->client->sess.accountID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return 0;
	}
	if (rc == SQLITE_ROW)
	{
		int numberOfChars;

		numberOfChars = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);

		return numberOfChars;
	}

	sqlite3_finalize(stmt);
	return 0;
}

// GalaxyRP (Alex): [Database] SELECT This method returns the character ID associated with the character name given, AND which belongs to the account the player is currently logged in with.
int select_char_id_using_char_name(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	int charID = -1;

	rc = sqlite3_prepare_v2(db, "SELECT CharID FROM Characters WHERE AccountID=? AND Name = ? COLLATE NOCASE", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}
	sqlite3_bind_int(stmt, 1, ent->client->sess.accountID);
	sqlite3_bind_text(stmt, 2, character_name, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}
	if (rc == SQLITE_ROW)
	{
		charID = sqlite3_column_int(stmt, 0);
	}

	sqlite3_finalize(stmt);

	return charID;
}

// GalaxyRP (Alex): [Database] SELECT This method returns the character ID associated with the character name given, AND which belongs to the account the player is currently logged in with.
saber_db_info_t select_saber_info_using_char_id(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	char saber1Model[50] = "";
	char saber2Model[50] = "";
	saber_db_info_t saber_info = { "none", "none" };

	rc = sqlite3_prepare_v2(db, "SELECT saberOneModel, saberTwoModel FROM Characters WHERE CharID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);

		return saber_info;
	}
	sqlite3_bind_int(stmt, 1, ent->client->pers.CharID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return saber_info;
	}
	if (rc == SQLITE_ROW)
	{
		Q_strncpyz(saber1Model, sqlite3_column_text(stmt, 0), sizeof(saber1Model));
		Q_strncpyz(saber2Model, sqlite3_column_text(stmt, 1), sizeof(saber2Model));

		strcpy(saber_info.saber1Model, saber1Model);
		strcpy(saber_info.saber2Model, saber2Model);
	}

	sqlite3_finalize(stmt);

	return saber_info;
}

// GalaxyRP (Alex): [Database] UPDATE This method updated a characters table row with information contained within the entity with which it's called.
void update_chars_table_row_with_current_values(gentity_t* ent) {
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


	// GalaxyRP (Alex): [Database] Grab the model and display name, so they can be saved in the database.
	char userinfo[MAX_INFO_STRING], modelName[MAX_INFO_STRING];
	int clientNum = ent->s.number;

	trap->GetUserinfo(clientNum, userinfo, sizeof(userinfo));
	Q_strncpyz(modelName, Info_ValueForKey(userinfo, "model"), sizeof(modelName));

	rc = sqlite3_prepare_v2(db, "UPDATE Characters SET Credits=?, Level=?, ModelScale=?, Skillpoints=?, Description=?, NetName=?, ModelName=?, xp=? WHERE CharID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return;
	}
	sqlite3_bind_int(stmt, 1, ent->client->pers.credits);
	sqlite3_bind_int(stmt, 2, ent->client->pers.level);
	sqlite3_bind_int(stmt, 3, ent->client->ps.iModelScale);
	sqlite3_bind_int(stmt, 4, ent->client->pers.skillpoints);
	sqlite3_bind_text(stmt, 5, ent->client->pers.description, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 6, ent->client->pers.netname, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 7, modelName, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 8, ent->client->pers.xp);
	sqlite3_bind_int(stmt, 9, ent->client->pers.CharID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	sqlite3_close(db);

	return;
}

/*
=====================================================================
Skills Table Methods
=====================================================================
*/

// GalaxyRP (Alex): [Database] INSERT This method inserts a new row in the skills table, with default values.
qboolean insert_skills_table_row(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt, int charID) {
	char insert_new_entry_to_skills_table[] = "INSERT INTO Skills(CharID, Jump, Push, Pull, Speed, Sense, SaberAttack, SaberDefense, SaberThrow, Absorb, Heal, Protect, MindTrick, TeamHeal, Lightning, Grip, Drain, Rage, TeamEnergize, StunBaton, BlasterPistol, BlasterRifle, Disruptor, Bowcaster, Repeater, DEMP2, Flechette, RocketLauncher, ConcussionRifle, BryarPistol, Melee, MaxShield, ShieldStrength, HealthStrength, DrainShield, Jetpack, SenseHealth, ShieldHeal, TeamShieldHeal, UniqueSkill, BlasterPack, PowerCell, MetalBolts, Rockets, Thermals, TripMines, Detpacks, Binoculars, BactaCanister, SentryGun, SeekerDrone, Eweb, BigBacta, ForceField, CloakItem, ForcePower, Improvements) VALUES('%i', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0')";

	rc = sqlite3_exec(db, va(insert_new_entry_to_skills_table, charID), 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return qfalse;
	}

	return qtrue;
}

// GalaxyRP (Alex): [Database] UPDATE This method updated a skills table row with information contained within the entity with which it's called. Also updates the skillpoint values, since there's no instance where a skill is updated and the skillpoints are not.
void update_skills_table_row_with_current_values(gentity_t* ent) {
	sqlite3* db;
	int rc;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	char update_skills_query[] = "UPDATE Skills SET Jump='%i', Push='%i', Pull='%i', Speed='%i', Sense='%i', SaberAttack='%i', SaberDefense='%i', SaberThrow='%i', Absorb='%i', Heal='%i', Protect='%i', MindTrick='%i', TeamHeal='%i', Lightning='%i', Grip='%i', Drain='%i', Rage='%i', TeamEnergize='%i', StunBaton='%i', BlasterPistol='%i', BlasterRifle='%i', Disruptor='%i', Bowcaster='%i', Repeater='%i', DEMP2='%i', Flechette='%i', RocketLauncher='%i', ConcussionRifle='%i', BryarPistol='%i', Melee='%i', MaxShield='%i', ShieldStrength='%i', HealthStrength='%i', DrainShield='%i', Jetpack='%i', SenseHealth='%i', ShieldHeal='%i', TeamShieldHeal='%i', UniqueSkill='%i', BlasterPack='%i', PowerCell='%i', MetalBolts='%i', Rockets='%i', Thermals='%i', TripMines='%i', Detpacks='%i', Binoculars='%i', BactaCanister='%i', SentryGun='%i', SeekerDrone='%i', Eweb='%i', BigBacta='%i', ForceField='%i', CloakItem='%i', ForcePower='%i', Improvements='%i', Armor='%i', Flamethrower='%i', ShieldRegen='%i', HealthRegen='%i' WHERE CharID='%i'; UPDATE Characters SET SkillPoints='%i' WHERE CharID='%i';";
	run_db_query(va(update_skills_query,
		ent->client->pers.skill_levels[0],	//Jump
		ent->client->pers.skill_levels[1],	//Push
		ent->client->pers.skill_levels[2],	//Pull
		ent->client->pers.skill_levels[3],	//Speed
		ent->client->pers.skill_levels[4],	//Sense
		ent->client->pers.skill_levels[5],	//SaberAttack
		ent->client->pers.skill_levels[6],	//SaberDefense
		ent->client->pers.skill_levels[7],	//SaberThrow
		ent->client->pers.skill_levels[8],	//Absorb
		ent->client->pers.skill_levels[9],	//Heal
		ent->client->pers.skill_levels[10],	//Protect
		ent->client->pers.skill_levels[11],	//MindTrick
		ent->client->pers.skill_levels[12],	//TeamHeal
		ent->client->pers.skill_levels[13],	//Lightning
		ent->client->pers.skill_levels[14],	//Grip
		ent->client->pers.skill_levels[15],	//Drain
		ent->client->pers.skill_levels[16],	//Rage
		ent->client->pers.skill_levels[17],	//TeamEnergize
		ent->client->pers.skill_levels[18],	//StunBaton
		ent->client->pers.skill_levels[19],	//BlasterPistol
		ent->client->pers.skill_levels[20],	//BlasterRifle
		ent->client->pers.skill_levels[21],	//Disruptor
		ent->client->pers.skill_levels[22],	//Bowcaster
		ent->client->pers.skill_levels[23],	//Repeater
		ent->client->pers.skill_levels[24],	//DEMP2
		ent->client->pers.skill_levels[25],	//Flechette
		ent->client->pers.skill_levels[26],	//RocketLauncher
		ent->client->pers.skill_levels[27],	//ConcussionRifle
		ent->client->pers.skill_levels[28],	//BryarPistol
		ent->client->pers.skill_levels[29],	//Melee
		ent->client->pers.skill_levels[30],	//MaxShield
		ent->client->pers.skill_levels[31],	//ShieldStrength
		ent->client->pers.skill_levels[32],	//HealthStrength
		ent->client->pers.skill_levels[33],	//DrainShield
		ent->client->pers.skill_levels[34],	//Jetpack
		ent->client->pers.skill_levels[35],	//SenseHealth
		ent->client->pers.skill_levels[36],	//ShieldHeal
		ent->client->pers.skill_levels[37],	//TeamShieldHeal
		ent->client->pers.skill_levels[38],	//UniqueSkill
		ent->client->pers.skill_levels[39],	//BlasterPack
		ent->client->pers.skill_levels[40],	//PowerCell
		ent->client->pers.skill_levels[41],	//MetalBolts
		ent->client->pers.skill_levels[42],	//Rockets
		ent->client->pers.skill_levels[43],	//Thermals
		ent->client->pers.skill_levels[44],	//TripMines
		ent->client->pers.skill_levels[45],	//Detpacks
		ent->client->pers.skill_levels[46],	//Binoculars
		ent->client->pers.skill_levels[47],	//BactaCanister
		ent->client->pers.skill_levels[48],	//SentryGun
		ent->client->pers.skill_levels[49],	//SeekerDrone
		ent->client->pers.skill_levels[50],	//Eweb
		ent->client->pers.skill_levels[51],	//BigBacta
		ent->client->pers.skill_levels[52],	//ForceField
		ent->client->pers.skill_levels[53],	//CloakItem
		ent->client->pers.skill_levels[54],	//ForcePower
		ent->client->pers.skill_levels[55], //Improvements
		ent->client->pers.skill_levels[56], //Armor
		ent->client->pers.skill_levels[57], //Flamethrower
		ent->client->pers.skill_levels[58], //Shield Regen
		ent->client->pers.skill_levels[59], //Health Regen
		ent->client->pers.CharID,
		ent->client->pers.skillpoints,
		ent->client->pers.CharID), db);

	sqlite3_close(db);

	return;
}

/*
=====================================================================
Weapons Table Methods
=====================================================================
*/

// GalaxyRP (Alex): [Database] INSERT This method inserts a new row in the weapons table, with default values.
qboolean insert_weapons_table_row(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt, int charID) {
	char insert_new_entry_to_weapons_table[] = "INSERT INTO Weapons(CharID, AmmoBlaster, AmmoPowercell, AmmoMetalBolts, AmmoRockets, AmmoThermal, AmmoTripmine, AmmoDetpack) VALUES('%i', '0', '0', '0', '0', '0', '0', '0')";

	rc = sqlite3_exec(db, va(insert_new_entry_to_weapons_table, charID), 0, 0, &zErrMsg);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return qfalse;
	}

	return qtrue;
}

// GalaxyRP (Alex): [Database] SELECT This method grabs all the values from a weapons table row (ASSUMES THE PLAYERS IS ALREADY LOGGED IN), and assigns them to the entity.
void select_weapons_table_row_from_entity(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	rc = sqlite3_prepare_v2(db, va("SELECT * FROM Weapons WHERE CharID='%i'", ent->client->pers.CharID), -1, &stmt, NULL);
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
		ent->client->ps.ammo[AMMO_BLASTER] = sqlite3_column_int(stmt, 1);
		ent->client->ps.ammo[AMMO_POWERCELL] = sqlite3_column_int(stmt, 2);
		ent->client->ps.ammo[AMMO_METAL_BOLTS] = sqlite3_column_int(stmt, 3);
		ent->client->ps.ammo[AMMO_ROCKETS] = sqlite3_column_int(stmt, 4);
		ent->client->ps.ammo[AMMO_THERMAL] = sqlite3_column_int(stmt, 5);
		ent->client->ps.ammo[AMMO_TRIPMINE] = sqlite3_column_int(stmt, 6);
		ent->client->ps.ammo[AMMO_DETPACK] = sqlite3_column_int(stmt, 7);

		// DAJ_RP: [Ammo] and never above the server's caps -- see RP_ClampAmmoToCaps() (g_items.c).
		RP_ClampAmmoToCaps(ent);
	}

	sqlite3_finalize(stmt);
}
