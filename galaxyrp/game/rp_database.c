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















// GalaxyRP (Alex): [Database] This method changes the character used currently by the player. It reassigns skills, weapons, userinfo, and changes the default character associated with the account.
// GalaxyRP fix: [Char] added announce_switch -- when qfalse, suppresses this function's own "X
// switched to: Y" broadcast at the end. /char new now calls this function immediately after
// creating a character (see Cmd_Char_f) so the player is placed straight onto it instead of having
// to run a separate /char use; without this flag that flow would broadcast both "X created a new
// character and is now using: Y" (sent by Cmd_Char_f) and "X switched to: Y" (this function's own,
// unconditional) back to back for the same action. /new (Cmd_Register_F) and /char use both still
// pass qtrue, unchanged.
// GalaxyRP fix: [Char] void -> qboolean. The row-not-found path below used to fall straight through
// into the success path, so a character that could not actually be loaded was still reported as
// loaded (see the comment on that branch). Callers now get told whether the switch really happened.
qboolean select_player_character(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt, qboolean announce_switch) {
	int numberOfChars = 0;

	if (ent->client->sess.loggedin == qfalse) {
		trap->SendServerCommand(ent - g_entities, "print \"^2You must be logged in load a character.\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^2You must be logged in load a character.\n\"");

		return qfalse;
	}

	// GalaxyRP (Alex): [Database] Check to see if character exists or not.
	numberOfChars = select_number_of_characters_with_name(ent, character_name, db, zErrMsg, rc, stmt);

	if (numberOfChars != 1) {
		trap->SendServerCommand(ent - g_entities, va("print \"^2Character %s ^2does not exist.\n\"", character_name));
		trap->SendServerCommand(ent - g_entities, va("cp \"^2Character %s ^2does not exist.\n\"", character_name));

		return qfalse;
	}

	// GalaxyRP (Alex): [Database] Select all info from all character related tables.
	// GalaxyRP fix: [security] this used to build the query text via
	// va("...Name = '%s'...", character_name, accountID) -- splicing the raw character name straight
	// into the SQL string. Reachable via /new (using the new account's own username as its first
	// character's name) and via /char use <name>. Bind both values as parameters instead.
	char select_character_query[] = "SELECT *\
		FROM Characters\
		INNER JOIN Skills\
		ON Skills.CharID = Characters.CharID\
		INNER JOIN Weapons\
		ON Weapons.CharID = Characters.CharID\
		WHERE Characters.Name = ? COLLATE NOCASE AND Characters.AccountID = ?";

	rc = sqlite3_prepare_v2(db, select_character_query, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return qfalse;
	}
	sqlite3_bind_text(stmt, 1, character_name, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, ent->client->sess.accountID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return qfalse;
	}
	if (rc == SQLITE_ROW)
	{
		char displayName[MAX_INFO_STRING], modelName[MAX_STRING_CHARS];

		// GalaxyRP fix: [Account] pers.player_statuses (bits like "was given force powers/guns via
		// admin /give while logged out") was never reset here, unlike pers.bitvalue and
		// pers.player_settings in Cmd_LogoutAccount_f -- so a flag set while logged out survived both
		// /new (this function is also the tail end of account creation) and /char use character
		// switches, even though it's only meant to describe a logged-out player's state. Reset it here
		// too, at the same "a character is now loaded" point as the rest of this block.
		//
		// GalaxyRP fix: [Admin] all bits but one. PLAYER_STATUS_SILENCED (bit 0) is an admin
		// punishment set by /silence, and it is stored nowhere else -- no database column, no sess
		// field -- so wiping the whole field here was an escape hatch: /new, /char use, /char new,
		// /login and /logout all reach one of the three resets that spell this line, none of them
		// announces anything, and the silence simply stopped applying with the admin never told.
		// That is the same defect the downed guard on these commands was written for -- a bit
		// cleared "as a side effect of account bookkeeping, not as a decision anyone made", in
		// Cmd_Char_f's words -- but the remedy has to differ. Refusing /login to a silenced player
		// would be absurd, so the bit is carried across the reset instead of the command refused.
		//
		// A mask rather than a save-and-restore pair, so the reset stays a single statement and
		// cannot drift out of order with the do_scale() call each of these resets is sequenced
		// against. Bit 0 is deliberately the only one kept: every other bit here either describes a
		// logged-out player's state, re-establishes itself just below, or expires on its own.
		ent->client->pers.player_statuses &= (1 << PLAYER_STATUS_SILENCED);

		// GalaxyRP (Alex): [Database] Grab info from characters table.
		ent->client->pers.CharID = sqlite3_column_int(stmt, 1);
		ent->client->pers.credits = sqlite3_column_int(stmt, 2);
		ent->client->pers.level = sqlite3_column_int(stmt, 3);
		do_scale(ent, sqlite3_column_int(stmt, 4));
		// GalaxyRP fix: [security] sess.rpgchar is a fixed 32-byte buffer (g_local.h). This used to copy
		// character_name -- the raw argument /char use <name> was typed with, only bounded by
		// MAX_STRING_CHARS (1024). create_new_character() rejects any new name that wouldn't fit, but a
		// row that predates that guard (or was added to the database directly) can still match this
		// query with an oversized name. Q_strncpyz() bounds the copy instead of trusting the match,
		// the same defense-in-depth applied to saber1Model/saber2Model just below.
		//
		// GalaxyRP fix: [Account] ...and it copies the row's own Name (column 5) rather than the typed
		// argument. Now that the lookup above folds case, "/char use ADMIN" loads the character stored
		// as "Admin", and it is the stored spelling that has to end up here: sess.rpgchar is written
		// back to Accounts.DefaultChar on every save, is what /list shows, and is what the "switched
		// to" broadcast below prints. select_account_and_default_character_data() -- the login path --
		// already reads its copy from the row; this brings the /char use path into line with it.
		//
		// GalaxyRP fix: [Database] every TEXT read in this block goes through zyk_db_column_string()
		// (see its definition, near the news code). Some of these were plain strcpy() with no bound at
		// all; the rest were Q_strncpyz(), which bounds the length but still dereferences the NULL
		// that sqlite3_column_text() returns for a NULL column -- Q_strncpyz only assert()s its source,
		// and asserts are compiled out of a release build. None of these columns is NULL or oversized
		// in a row the mod wrote itself; a row edited by hand or by another tool is what this guards
		// against, and the cost of not guarding was the server going down on every /char use of that
		// character. A normal row reads exactly as before.
		zyk_db_column_string(ent->client->sess.rpgchar, sizeof(ent->client->sess.rpgchar), stmt, 5);
		ent->client->pers.skillpoints = sqlite3_column_int(stmt, 6);
		zyk_db_column_string(ent->client->pers.description, sizeof(ent->client->pers.description), stmt, 7);
		zyk_db_column_string(displayName, sizeof(displayName), stmt, 8);
		zyk_db_column_string(modelName, sizeof(modelName), stmt, 9);

		// GalaxyRP (Alex): [XP System] Grab XP value from database.
		ent->client->pers.xp = sqlite3_column_int(stmt, 10);

		char saber1Model[30];
		char saber2Model[30];
		int saber1Color;
		int saber2Color;
		sqlite3_int64 stored1, stored2;	// GLX_U: [Cosmetics]
		// GalaxyRP fix: [security] strcpy() into these fixed 30-byte stack buffers had no length
		// check against the value read from the Characters table -- a saber-model name of 30+
		// characters in the database (whether from a corrupted row or a maliciously edited one)
		// would overflow the buffer. Q_strncpyz() bounds the copy to the destination's size instead.
		// GalaxyRP fix: [Database] now zyk_db_column_string(), which is NULL-safe as well as bounded,
		// and an empty result is put back to the default -- see zyk_default_empty_saber_models().
		zyk_db_column_string(saber1Model, sizeof(saber1Model), stmt, 11);
		// GLX_U: [Cosmetics] 64-bit reads: the hat and cape ids sit above the colour's 32 bits.
		stored1 = sqlite3_column_int64(stmt, 12);
		saber1Color = RP_STORED_COLOR(stored1);
		zyk_db_column_string(saber2Model, sizeof(saber2Model), stmt, 13);
		stored2 = sqlite3_column_int64(stmt, 14);
		saber2Color = RP_STORED_COLOR(stored2);
		zyk_default_empty_saber_models(saber1Model, sizeof(saber1Model), saber2Model, sizeof(saber2Model));

		// GalaxyRP: [Saber RGB] restore the character's custom blade colours and blade styles. These
		// two columns were read into unused locals before this feature existed -- they now actually
		// mean something. Applied here, ahead of update_saber() below, for the same reason the block
		// further down in select_account_and_default_character_data() is: update_saber() can trigger
		// a character save through ClientUserinfoChanged(), and that save writes pers.saberRGB[]/
		// pers.saberColorMode[] straight back to this row -- so both have to already hold this row's
		// values, not the previous character's.
		ent->client->pers.saberRGB[0] = (saber1Color & SABERRGB_SET) ? (saber1Color & SABERRGB_MASK) : 0;
		ent->client->pers.saberRGB[1] = (saber2Color & SABERRGB_SET) ? (saber2Color & SABERRGB_MASK) : 0;
		ent->client->pers.saberColorMode[0] = G_ValidateSaberColorMode(SABER_STORED_MODE(saber1Color));
		ent->client->pers.saberColorMode[1] = G_ValidateSaberColorMode(SABER_STORED_MODE(saber2Color));
		// GLX_U: [Cosmetics] and this character's hat and cape, for the same reason: the save that
		// update_saber() can trigger writes them back to this row. An id not on the list gives none.
		Q_strncpyz(ent->client->pers.cosmetic[RP_COSMETIC_HAT], G_RpCosmeticName(RP_COSMETIC_HAT, RP_STORED_COSMETIC(stored1)), sizeof(ent->client->pers.cosmetic[0]));
		Q_strncpyz(ent->client->pers.cosmetic[RP_COSMETIC_CAPE], G_RpCosmeticName(RP_COSMETIC_CAPE, RP_STORED_COSMETIC(stored2)), sizeof(ent->client->pers.cosmetic[0]));

		// GalaxyRP fix: [Saber RGB] patch the server's cached userinfo (cp_sbRGB1/cp_sbRGB2/color1/
		// color2) to match this character's just-loaded colours *before* update_saber() below runs.
		// update_saber() calls ClientUserinfoChanged() (directly, when the saber model differs from
		// before -- which a character switch usually does -- and again via ClientSpawn()'s own copy
		// of the same logic on the respawn this switch triggers) and that function unconditionally
		// re-adopts whatever cp_sbRGB1/cp_sbRGB2/color1/color2 currently sit in this cache, straight
		// into pers.saberRGB[]/pers.saberColorMode[] -- overwriting what we just set two lines above
		// with whichever colour was last cached here (a previous character's, on a switch; a stale
		// client echo, on login). ClientUserinfoChanged() then immediately re-saves pers.saberRGB[]
		// to the DB row named by pers.CharID (already this character's, set above), so the corrupted
		// value got written straight back out -- and update_saber_colors() below, which republishes
		// pers.saberRGB[] to the client and would otherwise be the fix, only runs *after* update_saber()
		// and simply trusts whatever pers.saberRGB[] already holds by then, so it faithfully re-saved
		// and re-published the corruption instead of catching it. Syncing the cache here first makes
		// that adopt step a no-op: there's nothing stale left for it to stomp our just-loaded value with.
		G_SyncSaberColorUserinfo(ent);

		// GalaxyRP fix: [Account] same reason, same fix, for this character's display name and player
		// model -- update_saber() below (and ClientSpawn()'s own copy of this same logic on the respawn
		// this switch triggers) can call ClientUserinfoChanged(), which unconditionally re-derives
		// pers.netname/the active model from whatever "name"/"model" currently sit in the cached
		// userinfo -- a previous character's, or this account's own username, still cached from before
		// -- and update_current_character_name_and_model() then immediately re-saves that stale value
		// to this row. set_netname()/set_model() below already correct this before the function
		// returns (they take displayName/modelName as explicit parameters rather than trusting
		// whatever pers already holds, so they're not fooled by an earlier corruption the way
		// update_saber_colors() used to be) -- but that only protects this synchronous call; syncing
		// the cache here too closes the same later-respawn/any-subsequent-userinfo-change window that
		// the saber-colour fix above closes.
		G_SyncNameModelUserinfo(ent, displayName, modelName);

		// GalaxyRP fix: [gameplay] number_of_sabers was initialized to 1 and only ever explicitly
		// re-set to 1 (the "if" branch never set it to 2), so this always evaluated to 1 regardless
		// of what saber2Model actually held. update_saber() treats number_of_args==2 (i.e.
		// number_of_sabers==1) as "no second saber" and force-overwrites saber2 with "none" --
		// so any saved dual/staff saber configuration was silently discarded and replaced with a
		// single saber on every character load, even though the database still had the real value.
		int number_of_sabers = 1;

		if (strcmp(saber2Model, "none") != 0) {
			number_of_sabers = 2;
		}
		update_saber(ent, saber1Model, saber2Model, number_of_sabers + 1);

		// GalaxyRP (Alex): [Database] Grab info from skills table. (column 15 is CharID, no need to grab that)
		for (int i = 0; i < NUM_OF_SKILLS; i++) {
			ent->client->pers.skill_levels[i] = sqlite3_column_int(stmt, i + 16);
		}

		// GalaxyRP fix: [gameplay] this used to be a contiguous loop (ps.ammo[i] = column(i + 71))
		// that assumed the ammo_t enum maps 1:1 onto the Weapons table's columns, and the "+71"
		// base offset was stale (the comment above still says "column 68 is CharID" -- it's
		// actually column 76 now). Both were wrong: (1) AMMO_EMPLACED sits between AMMO_ROCKETS
		// and AMMO_THERMAL in the enum but has no column in Weapons at all (the save path already
		// skips it), so a contiguous read can never line up past that gap; (2) Armor, Flamethrower,
		// ShieldRegen and HealthRegen were added to Skills after this offset was written, shifting
		// every column after them -- including all of Weapons -- eight places to the right. The net
		// effect was this loop silently reading Skills.Flamethrower, Skills.ShieldRegen,
		// Skills.HealthRegen and Weapons' own CharID column into ps.ammo instead of actual ammo
		// counts. Read each Weapons ammo column explicitly instead, mirroring the explicit list the
		// save path already uses -- verified against the live schema via a standalone harness.
		ent->client->ps.ammo[AMMO_BLASTER] = sqlite3_column_int(stmt, 77);
		ent->client->ps.ammo[AMMO_POWERCELL] = sqlite3_column_int(stmt, 78);
		ent->client->ps.ammo[AMMO_METAL_BOLTS] = sqlite3_column_int(stmt, 79);
		ent->client->ps.ammo[AMMO_ROCKETS] = sqlite3_column_int(stmt, 80);
		ent->client->ps.ammo[AMMO_THERMAL] = sqlite3_column_int(stmt, 81);
		ent->client->ps.ammo[AMMO_TRIPMINE] = sqlite3_column_int(stmt, 82);
		ent->client->ps.ammo[AMMO_DETPACK] = sqlite3_column_int(stmt, 83);

		// DAJ_RP: [Ammo] and never above the server's caps -- see RP_ClampAmmoToCaps() (g_items.c).
		RP_ClampAmmoToCaps(ent);

		// GalaxyRP (Alex): [Database] Apply the modelname and net name.
		set_netname(ent, displayName);
		set_model(ent, modelName);
		//ent->client->saber[1].model
		sqlite3_finalize(stmt);

		// GalaxyRP: [Saber RGB] republish the restored blade colours and push them into the player's
		// own cvars, so their console and saber menu show what was actually loaded rather than
		// whatever the client last sent (which the menu would otherwise re-send verbatim on its next
		// Apply, undoing the restore). Inside the row block, next to the set_netname()/set_model()
		// calls it mirrors -- with no row there is no character to have restored a colour from.
		update_saber_colors(ent);
	}
	else
	{
		// GalaxyRP fix: [stability] this row-not-found path (rc == SQLITE_DONE) used to leave stmt
		// unfinalized -- normally unreachable since numberOfChars was just confirmed to be exactly 1
		// a few lines above, but a race between that check and this query (e.g. the character being
		// removed by another command in between) could still hit it, and sqlite3_close() silently
		// fails to actually close the connection while any statement on it remains unfinalized. Same
		// leak class fixed in select_char_id_using_char_name() elsewhere in this file.
		sqlite3_finalize(stmt);

		// GalaxyRP fix: [Char] this branch used to only finalize and then fall straight through into
		// everything below -- initialize_rpg_skills(), the deferred kill/respawn,
		// update_accounts_table_row_with_default_char() and the "Character loaded sucessfully!"
		// message plus its public broadcast -- despite no character data having been read at all.
		// numberOfChars == 1 does NOT make this unreachable: that count comes from Characters alone,
		// while the query above INNER JOINs Skills and Weapons, so a character whose Skills or
		// Weapons row is missing lands here with its Characters row present. The player was then told
		// the character had loaded, everyone saw the broadcast, and Accounts.DefaultChar was rewritten
		// to a character that cannot be loaded -- while pers.CharID/sess.rpgchar still pointed at the
		// PREVIOUS character, so every later save silently wrote to the old character's row, and the
		// next /login resolved DefaultChar to the broken character and failed permanently until
		// someone edited the database by hand. Report the failure and leave the session untouched:
		// the player simply stays on the character they already had.
		trap->SendServerCommand(ent - g_entities, va("print \"^1Character %s ^1could not be loaded (its data is incomplete). Your current character is unchanged.\n\"", character_name));
		trap->SendServerCommand(ent - g_entities, va("cp \"^1Character %s ^1could not be loaded.\n\"", character_name));
		G_LogPrintf("select_player_character: character '%s' (account %i) has a Characters row but no matching Skills/Weapons row -- load aborted.\n",
			character_name, ent->client->sess.accountID);

		return qfalse;
	}

	// GalaxyRP fix: [Account] borrowed from a newer fork of this mod -- call initialize_rpg_skills()
	// directly here, synchronously, instead of relying solely on the G_Kill() respawn below to trigger
	// it (via ClientSpawn). This closes two real gaps in the old kill-only approach: (1) G_Kill() silently
	// no-ops for a paralyzed target (see its own PLAYER_STATUS_DOWNED check) and in GT_DUEL/GT_POWERDUEL
	// when g_allowDuelSuicide is off, so a player who is paralyzed or mid-duel when running /new or
	// /char use would otherwise never get this character's force powers/weapons applied at all; (2) even
	// when G_Kill() does succeed, the respawn it triggers is deferred until the client's next spawn tick,
	// leaving the previous character's loadout visibly equipped for a brief window right after the
	// command returns. initialize_rpg_skills() only touches force powers/weapons/health-shield-force caps
	// derived from pers.skill_levels[] (already loaded above), so calling it here is safe regardless of
	// whether this player is about to be killed. The respawn below, when it does fire, calls
	// initialize_rpg_skills() again via ClientSpawn -- that second call is idempotent (same skill_levels[]
	// in, same result out), so there's no double-apply side effect from calling it twice.
	// GalaxyRP fix: [Force] stop whatever the OUTGOING character had running first -- see
	// zyk_stop_active_force_powers(). It has to precede the line below, which overwrites the force
	// levels WP_ForcePowerStop() reads to clean up properly.
	zyk_stop_active_force_powers(ent);

	// DAJ_RP: [Force Enlightenment] and the Force powerups, before the next character's Force loads
	// -- see zyk_reset_force_powerups().
	zyk_reset_force_powerups(ent);

	// GalaxyRP fix: [Account] and the outgoing character's running HOLDABLES, for the same reason
	// and with the same ordering requirement -- see zyk_stop_active_holdables().
	zyk_stop_active_holdables(ent);

	RP_ClearPhaseMode(ent);

	// GalaxyRP fix: [Account] through the wrapper, so switching character while dead no longer
	// stands the corpse back up -- see zyk_apply_character_skills().
	zyk_apply_character_skills(ent);

	// GalaxyRP fix: [Account] finish the parts of the switch initialize_rpg_skills() does not cover --
	// see zyk_apply_character_loadout() for what and why. Covers /new, /char new and /char use, which
	// all reach the character through this function; /login has its own call in Cmd_Login_F().
	zyk_apply_character_loadout(ent);

	// GalaxyRP fix: [Char] a leftover debug print used to sit here, inside the non-spectator test that
	// once wrapped the G_Kill(): va("print \"%s\n\"", ent->team). ent->team is the map-entity "team"
	// spawn key, never set on a player entity, so every /char use and /char new printed a bare
	// "(null)" line to a non-spectator. Nothing read it; both the print and its now-empty test are gone.
	//
	// GalaxyRP: [Account] whether this still forces a respawn now depends on rp_seamlesslogin --
	// see zyk_relog_kill_required(), whose own spectator test means a spectator is never scheduled
	// either way.
	zyk_schedule_relog_kill(ent);

	// GalaxyRP (Alex): [Database] Assign the player the info from Accounts table.
	update_accounts_table_row_with_default_char(ent, character_name, db, zErrMsg, rc, stmt);

	// GalaxyRP (Alex): [Database] Display Messages.
	trap->SendServerCommand(ent - g_entities, "print \"^2Character loaded sucessfully!\n\"");
	trap->SendServerCommand(ent - g_entities, "cp \"^2Character loaded sucessfully!\n\"");
	if (announce_switch) {
		// GalaxyRP fix: [Account] announces the stored spelling (sess.rpgchar, read from the row above)
		// rather than whatever was typed, now that "/char use ADMIN" resolves to "Admin".
		trap->SendServerCommand(-1, va("chat \"%s switched to: %s\n\"", ent->client->pers.netname, ent->client->sess.rpgchar));
	}

	return qtrue;
}

// GalaxyRP (Alex): [Database] This method displays the list of characters to a player.
// GalaxyRP fix: [security] same reasoning as select_character_list_for_ui() below -- accountID is a
// server-derived int, never attacker-controlled text, so the old va()-spliced query text was never
// actually exploitable, but it was inconsistent with the bound-parameter style used everywhere else in
// this file. Bound here too, and the per-row strcpy() into CharName switched to Q_strncpyz() for the same
// defense-in-depth reason (character names are already capped to 31 characters at creation time, so this
// never overflowed the 1024-byte buffer in practice, but nothing here actually enforced that itself).
void select_character_list(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt)
{
	char CharName[MAX_STRING_CHARS];
	int charLevel;

	// GalaxyRP (Alex): [Database] Get list of char names.
	rc = sqlite3_prepare_v2(db, "SELECT Name, Level FROM Characters WHERE AccountID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	sqlite3_bind_int(stmt, 1, ent->client->sess.accountID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	int i = 1;
	while (rc == SQLITE_ROW) {
		Q_strncpyz(CharName, (const char*)sqlite3_column_text(stmt, 0), sizeof(CharName));
		charLevel = sqlite3_column_int(stmt, 1);
		trap->SendServerCommand(ent - g_entities, va("print \"^3%i.^2%s - Level:%i\n\"", i, CharName, charLevel));
		i++;
		rc = sqlite3_step(stmt);
	}

	// GalaxyRP fix: [Char] a bare /char only ever printed the character list above, with no reminder
	// of the subcommands that act on it -- print the same usage tip Cmd_Char_f's own malformed-input
	// fallback already uses right below the list, so a player doesn't have to already know the syntax.
	trap->SendServerCommand(ent - g_entities, "print \"^2Usage: /char <new/use/remove> <character name>\n\"");

	sqlite3_finalize(stmt);
}

// GalaxyRP (Alex): [Database] This method returns a list of character names readable by the UI.
// GalaxyRP fix: [security/stability] two related issues fixed together here:
//  1. the query text used to be built via va("...AccountID='%i'", ...) -- accountID is a server-derived
//     int (session state, never attacker-controlled text) so this was never actually exploitable, but it
//     was inconsistent with the bound-parameter style used everywhere else in this file. Bound like every
//     other query here now.
//  2. the accumulation loop used to be strcpy(CharString, va("%s&%s", CharString, CharName)) -- CharString
//     fed back into va() as its own first argument, growing with no bound against the caller's fixed-size
//     stack buffer (Cmd_ZykChars_f's char_string[MAX_STRING_CHARS]). create_new_character() now refuses to
//     create a character past MAX_CHARACTERS_PER_ACCOUNT, so a *new* account can no longer grow this past
//     that -- but that only protects accounts created after this fix; one that already has more rows than
//     that from before this fix existed would still reach this loop otherwise unbounded. Added an explicit
//     CharStringSize parameter and a MAX_CHARACTERS_PER_ACCOUNT loop cap (matching the client's own
//     ui_zyk_rpg_char_1.._15 slots and CG_ZykChars's identical cap in cg_servercmds.c) so a grandfathered
//     over-limit account safely stops filling the string instead of overflowing it, and switched to
//     Q_strcat/Q_strncpyz, which never write past their destination's declared size.
void select_character_list_for_ui(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt, char* CharString, int CharStringSize)
{
	char CharName[MAX_STRING_CHARS] = "";
	int charLevel = 0;
	int charCount = 0;

	// GalaxyRP (Alex): [Database] Get list of char names.
	rc = sqlite3_prepare_v2(db, "SELECT Name, Level FROM Characters WHERE AccountID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	sqlite3_bind_int(stmt, 1, ent->client->sess.accountID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	while (rc == SQLITE_ROW && charCount < MAX_CHARACTERS_PER_ACCOUNT) {
		Q_strncpyz(CharName, (const char*)sqlite3_column_text(stmt, 0), sizeof(CharName));
		charLevel = sqlite3_column_int(stmt, 1);
		Q_strcat(CharString, CharStringSize, va("&%s", CharName));
		charCount++;
		rc = sqlite3_step(stmt);
	}

	sqlite3_finalize(stmt);
}

// GalaxyRP (Alex): [Database] This method loads the account information, as well as the information related to the default character, and assigns it to the entity.
// GalaxyRP fix: [security] the username parameter used to be declared "char username[MAX_STRING_CHARS]"
// (1024) -- purely documentation in C (a parameter array decays to a pointer regardless of the size
// written here), but misleading enough that a previous fix bounded an internal copy to match one
// caller's real buffer size (Cmd_Login_F's own local char username[256]) without accounting for the
// other real caller, ClientBegin() (g_client.c), which passes ent->client->sess.filename directly --
// a genuinely fixed 32-byte buffer (g_local.h). GCC's -Wstringop-overflow caught the mismatch once
// that internal copy carried an explicit size to check the declared 1024 against each call site's
// real argument size. Declaring the true minimum safe size (32) here instead -- this value is always
// eventually copied into that same 32-byte sess.filename field regardless of which caller reached
// here -- keeps the signature honest and clears the warning for both callers at once. Keep this in
// sync with the matching extern declaration in g_client.c if it ever changes.
void select_account_and_default_character_data(gentity_t* ent, char username[32], sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	char password[256], name[256], description[MAX_STRING_CHARS], netName[MAX_STRING_CHARS], modelName[MAX_STRING_CHARS];
	int accountID, player_settings, adminLevel, charID, credits, level, modelScale, skillpoints;

	// GalaxyRP fix: [Account] drop any other connection still holding this account before a single
	// value is read below -- see zyk_drop_other_sessions_on_account() above for why, and why a drop
	// rather than a forced logout.
	//
	// The id is looked up separately rather than taken from the main query, and that is the whole
	// point of doing it here: the old session's ClientDisconnect() writes its state to the database,
	// so it has to finish BEFORE this function reads the row, not after. Reusing the accountID the
	// query below returns would have put the kick on the wrong side of the read and simply reversed
	// which session loses its progress. One extra indexed lookup on a UNIQUE column, on a path that
	// runs at most once per login.
	//
	// It also happens while no statement of ours is open, which keeps the dropped session's own
	// database connection from meeting a read transaction it would have to wait out.
	//
	// select_account_id_from_username() had no callers at all until now; it was already hardened
	// (bound parameter, a real -1 not-found sentinel, no leaked statement) and this is the use it
	// was waiting for.
	zyk_drop_other_sessions_on_account(ent, select_account_id_from_username(ent, username, db, zErrMsg, rc, stmt));

	// GalaxyRP fix: [security] this used to build the query text via
	// va("...Username = '%s'...Username = '%s'...", username, username) -- splicing the raw username
	// straight into the SQL string, twice. This is the main query /login runs once the password has
	// already been verified. Bind both placeholders as parameters instead.
	// GalaxyRP fix: [Account] this always looked up the account's DefaultChar, ignoring whichever
	// character was actually active -- harmless the first time this runs (a genuine /login, where no
	// character is active yet), but this same function is also called from ClientBegin() on every map
	// change for any already-logged-in player, which silently reverted an earlier /char use switch to
	// a non-default character back to the default one every time the map changed. That in turn made
	// pers.CharID (and therefore which character's row commands like /createcredits write to) wrong
	// until the player did /char use again. Added a third parameter for the currently-selected
	// character's name (sess.rpgchar): COALESCE/NULLIF picks it over the DefaultChar subquery whenever
	// it's non-empty, and falls back to DefaultChar exactly as before when it's empty (a true first
	// login, or right after /logout -- see the sess.rpgchar reset added to Cmd_LogoutAccount_f).
	// GalaxyRP fix: [security/Account] this query joined "FROM Accounts, Characters" with NO
	// Accounts.AccountID = Characters.AccountID predicate -- a cross join -- and its character-
	// resolving subquery was not scoped to the account either. Character names are only unique
	// *within* an account (select_number_of_characters_with_name() filters AccountID AND Name), so
	// two accounts can legitimately hold the same character name, and the subquery would then return
	// whichever CharID came first regardless of who owns it. The cross join happily paired that row
	// with the logging-in account's Accounts row, so /login loaded ANOTHER account's character: its
	// credits, level, skills and description, with pers.CharID set to that character -- meaning every
	// subsequent save wrote the logging-in player's state into the victim's row, destroying it. Any
	// player could trigger it deliberately with /char new <victim's character name>, and it re-fired
	// on every map change through ClientBegin(). Both halves are now scoped by account: the outer
	// join is constrained to this account's characters, and the subquery only resolves names within
	// it (Accounts.AccountID there is a correlated reference to the outer Accounts row -- the inner
	// FROM Characters shadows only the Characters alias, not Accounts).
	char select_account_table_row[] = "SELECT *\
		FROM Accounts, Characters\
		INNER JOIN Skills\
		ON Skills.CharID = Characters.CharID\
		INNER JOIN Weapons\
		ON Weapons.CharID = Characters.CharID\
		WHERE Accounts.Username = ? COLLATE NOCASE\
		AND Characters.AccountID = Accounts.AccountID\
		AND Characters.CharID = (\
			SELECT CharID\
			FROM Characters\
			Where Characters.AccountID = Accounts.AccountID\
			AND Characters.Name = COALESCE(NULLIF(?, ''), (\
				SELECT DefaultChar\
				FROM Accounts\
				WHERE Accounts.Username = ? COLLATE NOCASE)) COLLATE NOCASE\
			)";

	rc = sqlite3_prepare_v2(db, select_account_table_row, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return;
	}
	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, ent->client->sess.rpgchar, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, username, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return;
	}
	if (rc == SQLITE_ROW)
	{
		accountID = sqlite3_column_int(stmt, 0);
		player_settings = sqlite3_column_int(stmt, 1);
		adminLevel = sqlite3_column_int(stmt, 2);
		// GalaxyRP fix: [security] bound both copies instead of trusting the DB values' length. Not
		// reachable through a *new* /new registration any more (Cmd_Register_F now rejects a username/
		// password too long for the fixed buffers these ultimately feed, a few lines below), but an
		// account created before that guard existed could still have an oversized value sitting in the
		// database right now, and this restore runs on every /login for as long as that row exists.
		// GalaxyRP fix note: this function's own signature declares "char username[MAX_STRING_CHARS]",
		// but a parameter array decays to a plain pointer -- sizeof(username) here would silently give
		// the pointer's size (8), not 1024, and the declared 1024 doesn't reflect reality either. This
		// used to bound the copy to 256, matching Cmd_Login_F's own local char username[256] -- but
		// that's not this function's only caller: ClientBegin() (g_client.c) also calls this, passing
		// ent->client->sess.filename directly, a genuinely fixed 32-byte buffer (g_local.h). 256 was
		// safe for the first caller and a guaranteed overflow for the second -- caught by GCC's
		// -Wstringop-overflow once this copy carried an explicit size for it to check against the real
		// argument size at that call site. Bound to sizeof(ent->client->sess.filename) (32) instead:
		// the true minimum safe size across every real caller, since this value is ultimately copied
		// into that same 32-byte field a few lines below regardless of which caller reached here.
		//
		// GalaxyRP fix: [Database] and every TEXT read in this block goes through
		// zyk_db_column_string(), for the reasons given at the matching block in
		// select_player_character(): NULL-safe as well as bounded. The username keeps the 32-byte
		// bound explained above -- only the helper around it changed.
		zyk_db_column_string(password, sizeof(password), stmt, 3);
		zyk_db_column_string(username, sizeof(ent->client->sess.filename), stmt, 4);
		charID = sqlite3_column_int(stmt, 7);
		credits = sqlite3_column_int(stmt, 8);
		level = sqlite3_column_int(stmt, 9);
		modelScale = sqlite3_column_int(stmt, 10);
		zyk_db_column_string(name, sizeof(name), stmt, 11);
		skillpoints = sqlite3_column_int(stmt, 12);
		zyk_db_column_string(description, sizeof(description), stmt, 13);
		zyk_db_column_string(netName, sizeof(netName), stmt, 14);
		zyk_db_column_string(modelName, sizeof(modelName), stmt, 15);

		// GalaxyRP (Alex): [XP System] Grab XP value from database.
		ent->client->pers.xp = sqlite3_column_int(stmt, 16);

		char saber1Model[30];
		char saber2Model[30];
		int saber1Color;
		int saber2Color;
		sqlite3_int64 stored1, stored2;	// GLX_U: [Cosmetics]
		// GalaxyRP fix: [security] same fixed-size stack-buffer overflow as select_player_character()
		// above -- bound the copy to the destination's size instead of trusting the DB value's length.
		// GalaxyRP fix: [Database] NULL-safe too now, with the same empty-model default -- see there.
		zyk_db_column_string(saber1Model, sizeof(saber1Model), stmt, 17);
		// GLX_U: [Cosmetics] 64-bit reads: the hat and cape ids sit above the colour's 32 bits.
		stored1 = sqlite3_column_int64(stmt, 18);
		saber1Color = RP_STORED_COLOR(stored1);
		zyk_db_column_string(saber2Model, sizeof(saber2Model), stmt, 19);
		stored2 = sqlite3_column_int64(stmt, 20);
		saber2Color = RP_STORED_COLOR(stored2);
		zyk_default_empty_saber_models(saber1Model, sizeof(saber1Model), saber2Model, sizeof(saber2Model));

		// GalaxyRP fix: [Account] apply the loaded account/character fields to ent->client, and mark
		// the session logged in, here -- before update_saber() below -- instead of after it (which
		// is where this block, and the sess.loggedin assignment, used to sit). update_saber() can
		// trigger a database save via ClientUserinfoChanged() (see the "supdatesaber" fix above)
		// whenever the saber it's asked to set differs from what pers.saber1/2 already holds, which
		// is normally true on every login. That save is gated on sess.loggedin being true, and the
		// row it writes to is chosen by ent->client->pers.CharID -- both of which used to only be
		// set much further down, after update_saber() had already run and returned. loggedin being
		// false there meant the save was always silently skipped -- harmless only by coincidence,
		// since the data being "saved" was the same data just read from this very row a few lines
		// above, so there was nothing new to persist. Had that save instead been allowed to fire
		// with the old ordering, it would have written using whatever CharID/model-scale happened
		// to already be sitting in pers/ps from before this login -- a real, silent write to the
		// wrong character's row. Moving CharID/scale (and loggedin) ahead of update_saber() means
		// any save it triggers always targets this row with this row's own data. set_netname() and
		// set_model() below (unchanged, still after this block) each trigger their own save the
		// same way; by the time set_model() runs last, every field involved -- CharID, scale,
		// netname, saber1/2, model -- is already correct, so whatever transient state an earlier
		// save in this sequence wrote is simply overwritten before the function returns.
		ent->client->sess.accountID = accountID;
		ent->client->pers.player_settings = player_settings;
		ent->client->pers.bitvalue = adminLevel;
		// GalaxyRP fix: [security] pers.password and sess.filename are both fixed 32-byte buffers
		// (g_local.h); password/username here come straight from the database (already bounded to
		// 256 above, but still well over 32) and this restore runs on every /login. Bound the copy
		// instead of trusting it, for the same pre-existing-row reason given above.
		Q_strncpyz(ent->client->pers.password, password, sizeof(ent->client->pers.password));
		Q_strncpyz(ent->client->sess.filename, username, sizeof(ent->client->sess.filename));
		ent->client->pers.CharID = charID;
		ent->client->pers.credits = credits;
		ent->client->pers.level = level;

		// GalaxyRP fix: [Account] pers.player_statuses (bits like "was given force powers/guns via admin
		// /give while logged out") was never reset here, unlike pers.bitvalue and pers.player_settings
		// in Cmd_LogoutAccount_f -- so a flag set while logged out stayed set through this account's login
		// and every subsequent map-change reload, even though it's only meant to describe a logged-out
		// player's state. Reset it here, before do_scale() below, for the same reason bitvalue/
		// player_settings are reset above.
		// GalaxyRP fix: [Scale] this reset used to run *after* do_scale() (a few lines below, where the
		// old comment on this line used to sit, right before "sess.amrpgmode = 2") instead of before it.
		// do_scale() sets PLAYER_STATUS_SCALED ("player is scaled") as one of its side effects, so
		// resetting the whole field afterward silently cleared that bit again moments after it was set.
		// pers.player_scale itself stayed correct (do_scale() also sets that field, and this reset never
		// touched it directly), so the custom scale still applied visually right here on login -- but
		// ClientSpawn()'s own scale-reapply-on-respawn check (g_client.c) gates on this same bit 4, not on
		// player_scale. The next respawn after login -- the deferred pending_relog_kill_time kill a few
		// hundred ms later, see Cmd_Login_F below -- saw the bit already cleared and skipped reapplying
		// the scale, snapping the player back to default size right after the brief correctly-scaled
		// moment following login. select_player_character() above already resets player_statuses before
		// its own do_scale() call for exactly this reason; matching that order here fixes the same bug
		// for /login and for ClientBegin()'s map-change reload of an already-logged-in player.
		//
		// GalaxyRP fix: [Admin] the admin silence is carried across this reset too -- /login lands
		// here, and it was the easiest of the four ways out. See select_player_character() above
		// for the reasoning; this is the same line for the same reason. On the ClientBegin() path
		// the mask keeps nothing, because ClientConnect() has already zeroed the field by then.
		ent->client->pers.player_statuses &= (1 << PLAYER_STATUS_SILENCED);

		do_scale(ent, modelScale);
		// GalaxyRP fix: [security] same fixed-32-byte-buffer overflow risk as the matching strcpy() in
		// select_player_character() above (this is /login's own version of the same restore) -- bound
		// the copy instead of trusting a DB-column value that predates create_new_character()'s length
		// guard, or that never went through it (e.g. a first character named after the account's own
		// username at registration time -- see insert_chars_table_row()).
		Q_strncpyz(ent->client->sess.rpgchar, name, sizeof(ent->client->sess.rpgchar));
		ent->client->pers.skillpoints = skillpoints;
		// GalaxyRP fix: [Database] bounded, like every other copy out of this row. Both buffers are
		// MAX_STRING_CHARS today, so this could not overflow -- but only because they happen to match.
		Q_strncpyz(ent->client->pers.description, description, sizeof(ent->client->pers.description));

		// GalaxyRP: [Saber RGB] restore this character's custom blade colours and blade styles. Part
		// of this same pre-update_saber() block for exactly the reason spelled out above: the
		// character save that update_saber() can trigger writes pers.saberRGB[]/pers.saberColorMode[]
		// back to whichever row pers.CharID names, so both have to be this character's before it runs.
		ent->client->pers.saberRGB[0] = (saber1Color & SABERRGB_SET) ? (saber1Color & SABERRGB_MASK) : 0;
		ent->client->pers.saberRGB[1] = (saber2Color & SABERRGB_SET) ? (saber2Color & SABERRGB_MASK) : 0;
		ent->client->pers.saberColorMode[0] = G_ValidateSaberColorMode(SABER_STORED_MODE(saber1Color));
		ent->client->pers.saberColorMode[1] = G_ValidateSaberColorMode(SABER_STORED_MODE(saber2Color));
		// GLX_U: [Cosmetics] and this character's hat and cape, for the same reason: the save that
		// update_saber() can trigger writes them back to this row. An id not on the list gives none.
		Q_strncpyz(ent->client->pers.cosmetic[RP_COSMETIC_HAT], G_RpCosmeticName(RP_COSMETIC_HAT, RP_STORED_COSMETIC(stored1)), sizeof(ent->client->pers.cosmetic[0]));
		Q_strncpyz(ent->client->pers.cosmetic[RP_COSMETIC_CAPE], G_RpCosmeticName(RP_COSMETIC_CAPE, RP_STORED_COSMETIC(stored2)), sizeof(ent->client->pers.cosmetic[0]));

		// GalaxyRP fix: [Saber RGB] same reason as select_player_character()'s matching call above --
		// sync the cached userinfo to this character's just-loaded colours before update_saber() below
		// can trigger a ClientUserinfoChanged() that would otherwise re-adopt a stale cp_sbRGB1/
		// cp_sbRGB2/color1/color2 left over from whichever account/character was active here before
		// (or from this client's own not-yet-arrived userinfo echo) and immediately re-save that wrong
		// value to this character's row -- which the update_saber_colors() call further down, trusting
		// pers.saberRGB[] as already correct by then, would otherwise just faithfully re-publish.
		G_SyncSaberColorUserinfo(ent);

		// GalaxyRP fix: [Account] same reason as select_player_character()'s matching call above --
		// sync the cached userinfo's "name"/"model" keys to this character's own NetName/ModelName
		// before update_saber() below can trigger a ClientUserinfoChanged() that would otherwise
		// re-adopt whatever was cached here before this login (this account's raw username, another
		// character's display name, or a stale client echo) and immediately re-save it to this row.
		G_SyncNameModelUserinfo(ent, netName, modelName);

		ent->client->sess.loggedin = qtrue;

		// GalaxyRP: [Force Enlightenment] push the login state to the client immediately -- see the
		// matching comment in g_client.c -- so CG_GreyItem greys out both Enlightenment pickups for
		// this player right away, not only after the Profile UI is opened.
		trap->SendServerCommand(ent->s.number, va("supdateloggedin %i\n", ent->client->sess.loggedin));

		// GalaxyRP fix: [gameplay] Same bug as select_player_character() above, plus this copy
		// compared against "" instead of "none" -- saber2Model coming from the database is never
		// really an empty string (the column's schema default is 'saber_1', and update_saber()
		// always saves the literal string "none" for an unset second saber), so this "if" almost
		// never matched either way. number_of_sabers was initialized to 1 and never explicitly set
		// to 2, so it always evaluated to 1 regardless of what saber2Model actually held.
		// update_saber() treats number_of_args==2 (number_of_sabers==1) as "no second saber" and
		// force-overwrites saber2 with "none" -- so logging in (or respawning after a map change,
		// which re-runs this same restore) silently discarded any saved dual/staff saber
		// configuration and replaced it with a single saber, even though the database still had
		// the real value. This is also why a fresh character (whose saberTwoModel column defaults
		// to 'saber_1', not "none") never actually showed as dual-wielding on first login.
		int number_of_sabers = 1;

		if (strcmp(saber2Model, "none") != 0) {
			number_of_sabers = 2;
		}
		update_saber(ent, saber1Model, saber2Model, number_of_sabers + 1);

		// GalaxyRP (Alex): [Database] Column 21 is a duplicate of CharID, no need to grab that.
		for (int i = 0; i < NUM_OF_SKILLS; i++) {
			ent->client->pers.skill_levels[i] = sqlite3_column_int(stmt, i + 22);
		}
		// GalaxyRP fix: [gameplay] same bug as select_player_character() above (this function's
		// version of the loop used a "+78" base, with the comment claiming Weapons.CharID sits at
		// column 79 -- it's actually column 82 now that this query also joins in Accounts). Same
		// two causes: AMMO_EMPLACED has no Weapons column at all, and Skills gained four columns
		// (Armor, Flamethrower, ShieldRegen, HealthRegen) after this offset was written. Read each
		// Weapons ammo column explicitly instead, mirroring the explicit list the save path uses --
		// verified against the live schema via a standalone harness.
		ent->client->ps.ammo[AMMO_BLASTER] = sqlite3_column_int(stmt, 83);
		ent->client->ps.ammo[AMMO_POWERCELL] = sqlite3_column_int(stmt, 84);
		ent->client->ps.ammo[AMMO_METAL_BOLTS] = sqlite3_column_int(stmt, 85);
		ent->client->ps.ammo[AMMO_ROCKETS] = sqlite3_column_int(stmt, 86);
		ent->client->ps.ammo[AMMO_THERMAL] = sqlite3_column_int(stmt, 87);
		ent->client->ps.ammo[AMMO_TRIPMINE] = sqlite3_column_int(stmt, 88);
		ent->client->ps.ammo[AMMO_DETPACK] = sqlite3_column_int(stmt, 89);

		// DAJ_RP: [Ammo] and never above the server's caps -- see RP_ClampAmmoToCaps() (g_items.c).
		RP_ClampAmmoToCaps(ent);

		sqlite3_finalize(stmt);

		// GalaxyRP fix: [Account] set_netname()/set_model()/update_saber_colors() and the
		// sess.amrpgmode = 2 assignment below used to sit here unconditionally, AFTER this "row
		// found" block instead of inside it -- so when the account+character lookup above found no
		// matching row (rc == SQLITE_DONE: a stale DefaultChar, a legacy/imported account, or any
		// other case where the target character can't be resolved), all four still ran anyway.
		// set_netname()/set_model() were called with netName/modelName as uninitialized stack memory
		// (neither is assigned anywhere outside this block), and sess.amrpgmode was forced to 2 even
		// though sess.loggedin (correctly gated inside this block, a few lines above) stayed qfalse --
		// a real, correctly-authenticated /login (the password check in Cmd_Login_F already passed
		// before this function is ever called) whose character couldn't be resolved ended up
		// "logged in" as far as the ClientCommand dispatcher's CMD_LOGGEDIN gate is concerned (it
		// checks amrpgmode, not loggedin) while sess.accountID/pers.CharID were never actually set.
		// Moved inside this block so none of the four can run without real, just-loaded row data
		// behind them; see the trailing else below for what now happens instead when no row is found.
		set_netname(ent, netName);
		set_model(ent, modelName);

		// GalaxyRP: [Saber RGB] same as select_player_character() -- republish the restored colours and
		// push them into the client's own cvars so its console and saber menu agree with the server.
		update_saber_colors(ent);

		// GalaxyRP fix: [Scale] the pers.player_statuses reset that used to sit here (see the GalaxyRP fix:
		// [Scale] comment further up, right before do_scale()) was moved ahead of do_scale() instead --
		// it was wiping out the "player is scaled" bit do_scale() had just set a few lines above, which
		// broke scale restoration on the respawn that follows every login. See that comment for the full
		// explanation.

		ent->client->sess.amrpgmode = 2;

		return;
	}

	// GalaxyRP fix: [stability] this row-not-found path (rc == SQLITE_DONE) used to fall through to
	// the block moved into the "row found" branch above with the prepared statement never finalized --
	// the same leak class fixed in select_char_id_using_char_name() and select_player_character()
	// elsewhere in this file. Finalize it, and log the failure server-side so it's visible instead of
	// silently invisible either way. Deliberately does NOT touch sess.loggedin/sess.amrpgmode/
	// pers.CharID here: this function is also called from ClientBegin() on every map change for an
	// already-logged-in player (see g_client.c), where a transient lookup failure regressing an
	// already-valid session would be worse than just leaving this one reload's fields stale for a
	// single map. Cmd_Login_F (the /login path) checks sess.loggedin itself after calling this and
	// reports the failure to the player explicitly instead of claiming a successful login it didn't
	// deliver.
	sqlite3_finalize(stmt);
	G_LogPrintf("WARNING: select_account_and_default_character_data found no matching account+character row for username '%s' (selected character '%s') -- account/character data was not loaded.\n", username, ent->client->sess.rpgchar);

	return;
}

// GalaxyRP (Alex): [Database] This method creates a new character associated with the account the player is currently logged in, and adds the default data in all the tables. ASSUMES PLAYER IS LOGGED IN!

// GalaxyRP: [Char fix] added three checks ahead of the pre-existing uniqueness check below, none of
// which previously existed: an empty name (a bare /char new "" would otherwise insert a blank,
// unselectable-looking row that just clutters /char's own list output), a name too long for
// ent->client->sess.rpgchar to hold (a fixed 32-byte buffer -- see the matching Q_strncpyz() fixes
// in select_player_character() and select_account_and_default_character_data() above, both of which
// strcpy() a character name into that buffer once this one is ever loaded), and a name containing
// '&' (CG_ZykChars() on the client, cg_servercmds.c, splits the character list this command's
// caller pushes to the client on exactly that character via strtok(arg, "&"); a name containing '&'
// would be split into multiple bogus entries and desync every character slot after it in that
// player's own character-select UI). Unlike Cmd_Register_F()'s username handling, this deliberately
// does NOT strip color codes or force lowercase -- a character's name is meant to be a player-chosen
// display name (shown with its own colors throughout, e.g. select_character_list()'s listing), not
// a login credential that needs to be predictable/comparable the way an account username does.
qboolean create_new_character(gentity_t* ent, char char_name[MAX_STRING_CHARS], sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	int newCharID = 0;

	if (char_name[0] == '\0') {
		trap->SendServerCommand(ent - g_entities, "print \"^1Character name cannot be empty.\n\"");
		return qfalse;
	}

	if (strlen(char_name) > sizeof(ent->client->sess.rpgchar) - 1) {
		trap->SendServerCommand(ent - g_entities, va("print \"^1Character name can only have a maximum of %i characters.\n\"", (int)sizeof(ent->client->sess.rpgchar) - 1));
		return qfalse;
	}

	if (strchr(char_name, '&') != NULL) {
		trap->SendServerCommand(ent - g_entities, "print \"^1Character name cannot contain the '&' character.\n\"");
		return qfalse;
	}

	// GalaxyRP: [security] letters and numbers only. This was written because sess.rpgchar (this
	// name, once accepted) used to be spliced raw into file paths -- description_add()'s fopen(), and
	// before that zyk_config_filename() and zyk_remove_configs()'s system() calls -- where an
	// unvalidated name could escape the folder it was meant to stay inside. All three are gone now
	// (see the GalaxyRP fix: [Dead Code] and [Attributes] comments where they used to live), and the
	// rule stays: the name is still stored in the database, written back to Accounts.DefaultChar,
	// shown in the character-select UI and printed in broadcasts, and none of those is a place for
	// anything but a plain name. This also subsumes the '&' check above, but that one is left in
	// place for its own, more specific message.
	if (zyk_check_user_input(char_name, strlen(char_name)) == qfalse) {
		trap->SendServerCommand(ent - g_entities, "print \"^1Character name can only contain letters and numbers.\n\"");
		return qfalse;
	}

	// GalaxyRP fix: [Char] the character-select UI only ever had 15 slots to display characters in
	// (ingame_galaxyrp.menu's ui_zyk_rpg_char_1.._15, and CG_ZykChars's matching client-side cap in
	// cg_servercmds.c), but nothing here ever stopped an account from creating more than that -- a 16th+
	// character was still fully creatable and usable via /char use, just permanently invisible to the
	// menu. It also removed the safety margin select_character_list_for_ui() below relies on to stay
	// within its fixed-size output buffer. Reject a 16th character explicitly, with a clear reason,
	// rather than silently letting the count drift past what the rest of this system assumes.
	if (select_number_of_characters(ent, db, zErrMsg, rc, stmt) >= MAX_CHARACTERS_PER_ACCOUNT) {
		trap->SendServerCommand(ent - g_entities, va("print \"^1Character limit reached. You can only have %i characters per account.\n\"", MAX_CHARACTERS_PER_ACCOUNT));
		trap->SendServerCommand(ent - g_entities, va("cp \"^1Character limit reached (%i max).\n\"", MAX_CHARACTERS_PER_ACCOUNT));
		return qfalse;
	}

	// GalaxyRP (Alex): [Database] Character names should be unique, check for it here.
	if (select_number_of_characters_with_name(ent, char_name, db, zErrMsg, rc, stmt) > 0) {
		trap->SendServerCommand(ent - g_entities, va("print \"^1Character name ^7%s ^1is already in use.\n\"", char_name));
		trap->SendServerCommand(ent - g_entities, va("cp \"^1Character name ^7%s ^1is already in use.\n\"", char_name));
		return qfalse;
	}

	// GalaxyRP fix: [security] this used to go through run_db_query() as one combined
	// Characters+Skills+Weapons INSERT, with the player-chosen character name spliced straight into
	// the Characters portion via va("...'%s'..."). Reachable via /char new <name>, usable by any
	// logged-in player. The Skills and Weapons INSERTs are fully static (every column defaults to
	// '0', no placeholders at all), so only the Characters INSERT needs binding; it's split out and
	// prepared/bound/stepped on its own, while the still-static Skills+Weapons INSERT stays a single
	// run_db_query() call, unchanged, below.
	// GalaxyRP fix: [Char] both of these SQL failure paths used to fall through to "return;" (a bare,
	// unconditional success as far as any caller could tell -- this function returned void). Now that
	// the return value controls whether Cmd_Char_f switches the player onto this character and
	// announces it as created, a failed Characters insert has to report qfalse here too, or the
	// player would be told (and everyone else shown a chat broadcast) that a character was created
	// and is now in use when no row actually exists for it.
	// GalaxyRP fix: [Char] all three of this character's rows are now created inside one explicit
	// transaction, and the Skills/Weapons rows bind their CharID explicitly (see below) instead of
	// relying on the three tables' independent rowid counters happening to stay in step. Previously a
	// failure partway through left a Characters row with no Skills/Weapons rows behind it -- which is
	// exactly the state select_player_character()'s INNER JOINs cannot load, and which used to be
	// reported to the player as a successful load. All-or-nothing removes that state entirely.
	if (sqlite3_exec(db, "BEGIN", 0, 0, NULL) != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		return qfalse;
	}

	// GalaxyRP fix: [Saber] saberOneModel/saberTwoModel were left out of this INSERT entirely, so both
	// took the schema default the ALTER TABLE gives them, 'saber_1' (see InitializeGalaxyRpTables() in
	// g_main.c). The character-load paths treat any saberTwoModel other than the literal "none" as "this
	// character dual-wields" (see the number_of_sabers logic in select_player_character()), so EVERY
	// newly created character came up dual-wielding without ever having chosen it -- and since
	// 'saber_1' is not a real hilt, both slots fell back to the default saber. ClientUserinfoChanged()
	// then wrote that result straight back to the row, making it permanent. Set the second slot to the
	// "none" this system uses everywhere else for "no second saber". saberOneModel is deliberately left
	// to its existing default so the first slot keeps resolving exactly as it does today.
	rc = sqlite3_prepare_v2(db, "INSERT INTO Characters(AccountID, Credits, Level, ModelScale, Name, SkillPoints, Description, NetName, ModelName, xp, saberTwoModel) VALUES(?, '100', '1', '100', ?, '1', 'Nothing to show.', 'DefaultName', 'kyle', 0, 'none')", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_exec(db, "ROLLBACK", 0, 0, NULL);
		return qfalse;
	}
	sqlite3_bind_int(stmt, 1, ent->client->sess.accountID);
	sqlite3_bind_text(stmt, 2, char_name, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_exec(db, "ROLLBACK", 0, 0, NULL);
		return qfalse;
	}
	sqlite3_finalize(stmt);

	// Characters.CharID is an INTEGER PRIMARY KEY, i.e. an alias for the row's rowid, so this is the
	// CharID the row above just got -- the value the Skills and Weapons rows must be keyed on.
	newCharID = (int)sqlite3_last_insert_rowid(db);

	// GalaxyRP fix: [correctness] this buffer was sized at 900 bytes, but the two INSERT
	// statements it initializes (Skills, then Weapons) actually need 1081 bytes including the
	// terminating NUL -- confirmed by measuring the literal below. 900 predates the Armor,
	// Flamethrower, ShieldRegen and HealthRegen columns that were later added to Skills (the same
	// schema change behind the ammo-restore column-offset bug fixed earlier); the array size was
	// never revisited when the column list grew to match. MSVC flagged this (C4045, "array bounds
	// overflow") and silently truncates the initializer to fit -- with the buffer completely full
	// of literal content and no room left for a NUL terminator. That buffer is then handed straight
	// to run_db_query() -> sqlite3_exec(), which requires a NUL-terminated C string: without one,
	// it reads past the end of this stack array until it happens to find a zero byte elsewhere on
	// the stack, and executes whatever garbage text it finds appended to the truncated, syntactically
	// broken SQL. In practice this means every /char new likely failed to insert that character's
	// Skills and Weapons rows (a syntax error caught and logged by run_db_query(), not crashed on),
	// leaving a Characters row with no matching Skills/Weapons rows -- which the INNER JOINs in
	// select_player_character()/select_account_and_default_character_data() require, so the new
	// character could never actually be loaded. Sized to 1100 (matching the same margin already
	// used by the near-identical-size update_character_query[1100] elsewhere in this file) rather
	// than the exact 1081 needed, so it isn't this fragile again the next time a column is added.
	// GalaxyRP fix: [Char] these two INSERTs used to run as one combined string through
	// run_db_query() -> sqlite3_exec(). That had two problems, both fixed here. First, run_db_query()
	// returns void and swallows every error, yet this function went on to "return qtrue" regardless --
	// so a failed insert was reported to the caller (and to the player, and in a public broadcast) as
	// a successfully created character, leaving exactly the orphaned-Characters-row state that
	// select_player_character() cannot load. Second, neither INSERT named CharID at all: both relied
	// on Skills' and Weapons' own rowid counters independently landing on the same number as the
	// Characters row just inserted. Those three counters only stay in step for as long as nothing ever
	// fails or is deleted unevenly; once they diverge, every character created from then on is keyed
	// to some other character's Skills/Weapons row, or to none. Both statements are now prepared and
	// stepped individually with the real CharID bound, and any failure rolls the whole creation back.
	// They are plain string literals rather than fixed-size char arrays on purpose -- see the sizing
	// bug this array previously had (it was 900 bytes for 1081 bytes of SQL, silently truncated with
	// no room for a NUL terminator, so every /char new ran garbage SQL). A pointer to a literal cannot
	// have that problem again.
	const char* insert_skills_query = "INSERT INTO Skills(CharID, Jump, Push, Pull, Speed, Sense, SaberAttack, SaberDefense, SaberThrow, Absorb, Heal, Protect, MindTrick, TeamHeal, Lightning, Grip, Drain, Rage, TeamEnergize, StunBaton, BlasterPistol, BlasterRifle, Disruptor, Bowcaster, Repeater, DEMP2, Flechette, RocketLauncher, ConcussionRifle, BryarPistol, Melee, MaxShield, ShieldStrength, HealthStrength, DrainShield, Jetpack, SenseHealth, ShieldHeal, TeamShieldHeal, UniqueSkill, BlasterPack, PowerCell, MetalBolts, Rockets, Thermals, TripMines, Detpacks, Binoculars, BactaCanister, SentryGun, SeekerDrone, Eweb, BigBacta, ForceField, CloakItem, ForcePower, Improvements) VALUES(?, '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0')";
	const char* insert_weapons_query = "INSERT INTO Weapons(CharID, AmmoBlaster, AmmoPowercell, AmmoMetalBolts, AmmoRockets, AmmoThermal, AmmoTripmine, AmmoDetpack) VALUES(?, '0', '0', '0', '0', '0', '0', '0')";
	int i = 0;

	for (i = 0; i < 2; i++)
	{
		const char* query = (i == 0) ? insert_skills_query : insert_weapons_query;

		rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
		if (rc != SQLITE_OK)
		{
			trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			sqlite3_exec(db, "ROLLBACK", 0, 0, NULL);
			return qfalse;
		}
		sqlite3_bind_int(stmt, 1, newCharID);
		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
		{
			trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			sqlite3_exec(db, "ROLLBACK", 0, 0, NULL);
			return qfalse;
		}
		sqlite3_finalize(stmt);
	}

	if (sqlite3_exec(db, "COMMIT", 0, 0, NULL) != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_exec(db, "ROLLBACK", 0, 0, NULL);
		return qfalse;
	}

	// GalaxyRP fix: [Char] the "X created a char: Y" broadcast that used to be sent from here is now
	// sent by Cmd_Char_f instead, once it also knows the immediately-following switch onto this
	// character (select_player_character()) has happened -- see the comment on that call site.
	return qtrue;
}

// GalaxyRP (Alex): [Database] This method removes a character form the database. It affects all tables that contain character information. ASSUMES THE PLAYER IS LOGGED IN!!!

// GalaxyRP: [Char fix] this used to unconditionally run its DELETEs against whatever CharID
// select_char_id_using_char_name() returned, including -1 (its "not found" sentinel) -- so
// /char remove <a name that doesn't exist> matched zero rows and silently did nothing, with no
// feedback either way, success or failure. It also never checked whether char_name was the
// character the player currently has loaded: deleting that row left ent->client->pers.CharID (and,
// in the common case, the account's own DefaultChar column -- /char use keeps it in sync with
// whatever was last selected, see update_accounts_table_row_with_default_char()) pointing at a
// now-nonexistent character. Any later save would then silently update nothing, and the account's
// next /login would find no matching character to restore at all. Both are checked explicitly now,
// each with its own message; Q_stricmp (case-insensitive) is used for the active-character check
// specifically so it can't be bypassed by retyping the name with different casing.
void remove_character(gentity_t* ent, char char_name[MAX_STRING_CHARS], sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {

	int charID;

	if (Q_stricmp(char_name, ent->client->sess.rpgchar) == 0) {
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot remove your currently active character. Switch to a different character first with /char use <character name>.\n\"");
		return;
	}

	charID = select_char_id_using_char_name(ent, char_name, db, zErrMsg, rc, stmt);

	if (charID == -1) {
		trap->SendServerCommand(ent - g_entities, va("print \"^2Character %s ^2does not exist.\n\"", char_name));
		return;
	}

	// GalaxyRP fix: [security] this used to build one combined, semicolon-separated DELETE query via
	// va()/%i splicing straight into the SQL text. charID only ever comes from
	// select_char_id_using_char_name()'s return value (its -1 "not found" sentinel is already
	// rejected above), so it was never actually attacker-controlled text and this was never
	// exploitable -- but it was the last query in this file's /char path still built that way instead
	// of bound, after the rest of the database layer was hardened. sqlite3_prepare_v2() (unlike
	// sqlite3_exec(), which run_db_query() wraps) only ever prepares a single statement, so the three
	// DELETEs run as three separate bound statements here instead of one combined multi-statement
	// text blob.
	//
	// GalaxyRP fix: [Items] and the character's items go with it, as a fourth DELETE. They never did:
	// the Items table was simply missing from this list. That left the rows behind for good -- and
	// worse, Characters.CharID is a plain INTEGER PRIMARY KEY with no AUTOINCREMENT, so SQLite hands
	// the next new character max(CharID) + 1. Removing the character with the highest CharID and then
	// creating any character, on any account, gave the new one the same CharID -- and every item the
	// removed character had owned. InitializeGalaxyRpTables() (g_main.c) sweeps up the rows this used
	// to leave behind.
	//
	// GalaxyRP fix: [Char] and all four now run inside one transaction, the way create_new_character()
	// creates the rows. A failed statement used to be logged and skipped with "continue", and the
	// player was still told the character had been removed -- whatever mix of rows was actually left.
	// Now either every row goes or none does, and the player is told which.
	const char* remove_character_queries[4] = {
		"DELETE FROM Characters WHERE CharID=?",
		"DELETE FROM Skills WHERE CharID=?",
		"DELETE FROM Weapons WHERE CharID=?",
		"DELETE FROM Items WHERE CharID=?"
	};
	int removed_items = 0;
	qboolean failed = qfalse;

	if (sqlite3_exec(db, "BEGIN", 0, 0, NULL) != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		failed = qtrue;
	}

	for (int i = 0; i < 4 && !failed; i++) {
		rc = sqlite3_prepare_v2(db, remove_character_queries[i], -1, &stmt, NULL);
		if (rc != SQLITE_OK)
		{
			trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			sqlite3_exec(db, "ROLLBACK", 0, 0, NULL);
			failed = qtrue;
			break;
		}
		sqlite3_bind_int(stmt, 1, charID);
		rc = sqlite3_step(stmt);
		if (rc != SQLITE_DONE)
		{
			trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			sqlite3_exec(db, "ROLLBACK", 0, 0, NULL);
			failed = qtrue;
			break;
		}
		if (i == 3)
		{
			removed_items = sqlite3_changes(db);
		}
		sqlite3_finalize(stmt);
	}

	if (!failed && sqlite3_exec(db, "COMMIT", 0, 0, NULL) != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_exec(db, "ROLLBACK", 0, 0, NULL);
		failed = qtrue;
	}

	if (failed)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"^1Could not remove character %s^1. Nothing was changed.\n\"", char_name));
		return;
	}

	if (removed_items > 0)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"^2Character %s ^2has been removed, along with its %d item%s.\n\"", char_name, removed_items, (removed_items == 1) ? "" : "s"));
	}
	else
	{
		trap->SendServerCommand(ent - g_entities, va("print \"^2Character %s ^2has been removed.\n\"", char_name));
	}
	trap->SendServerCommand(ent - g_entities, va("cp \"^2Character %s ^2has been removed.\n\"", char_name));

	return;
}

// GalaxyRP (Alex): [Database] This method updates the account, Characters, Skills and Weapons tables with current values. ASSUMES THE PLAYER IS LOGGED IN!!!
void update_current_character_and_account(gentity_t* ent) {
	sqlite3* db;
	char* zErrMsg = 0;
	int rc;
	sqlite3_stmt* stmt = NULL;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	char userinfo[MAX_INFO_STRING], modelName[MAX_INFO_STRING];
	// GalaxyRP fix: [Char] this used to re-derive "this player's" own client slot via
	// ClientNumberFromString(), which either treats the name as a raw slot index (if it's purely
	// numeric) or does a case-insensitive SUBSTRING search across every connected player's name and
	// returns the first match by ascending slot -- neither of which is guaranteed to be this player.
	// A numeric player name, or a name that happens to be a substring of another, earlier-slot
	// player's name (e.g. "Vader" vs. an earlier-joined "DarthVaderFan"), returned that OTHER
	// player's slot instead, and the userinfo (model/saber1/saber2) fetched below then belonged to
	// them, not to ent. In the functions that save this to the database (e.g.
	// update_current_character_and_account()) that wrong value gets written straight into ent's own
	// Characters.ModelName row -- real save-path data corruption from nothing more than an ordinary
	// name collision. ent is already the exact right entity at every one of these call sites, so this
	// never needed to be looked up by name at all.
	int clientNum = ent->s.number;

	trap->GetUserinfo(clientNum, userinfo, sizeof(userinfo));
	Q_strncpyz(modelName, Info_ValueForKey(userinfo, "model"), sizeof(modelName));

	// GalaxyRP fix: [security] this used to go through run_db_query() as one combined
	// Characters+Skills+Weapons+Accounts UPDATE, with the player's description, netname, model name
	// (all player-controlled) and DefaultChar (the character name) spliced straight into their
	// respective portions via va("...\"%s\"...'%s'..."). This runs via save_account() -> any
	// logged-in player being periodically/opportunistically saved -- a frequently-reachable,
	// unauthenticated injection point. sqlite3_bind_*() only binds a single prepared statement, so
	// the Characters and Accounts UPDATEs (the two with string values) are split out and each
	// prepared/bound/stepped on their own; the Skills and Weapons UPDATEs are all-integer and
	// unaffected, so they stay combined via run_db_query() below, unchanged.
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

	// GalaxyRP fix: [Database] same PlayerSettings-hardcoded-to-'0' bug as
	// update_accounts_table_row_with_current_values() above -- this is the other write site
	// (save_account(ent, qtrue), the RPG-char save path) that was silently resetting a logged-in
	// player's /settings toggles back to 0 in the DB. Bind the real value instead.
	rc = sqlite3_prepare_v2(db, "UPDATE Accounts SET PlayerSettings=?, AdminLevel=?, DefaultChar=? WHERE AccountID=?", -1, &stmt, NULL);
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
	sqlite3_bind_int(stmt, 4, ent->client->sess.accountID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	// GalaxyRP fix: [Database] this query used to stop at Improvements, silently omitting the
	// Armor/Flamethrower/ShieldRegen/HealthRegen skill columns that update_skills_query() elsewhere
	// in this file already saves in full -- not an active data-loss bug (those 4 skills can only
	// change via /rpmodeup and /rpmodedown, which always call update_skills_table_row_with_current_values()
	// immediately afterward and save all 60 columns themselves), but this save path should still
	// write a complete, self-consistent row rather than relying on another function to cover the gap.
	char update_character_query[] = "UPDATE Skills SET Jump='%i', Push='%i', Pull='%i', Speed='%i', Sense='%i', SaberAttack='%i', SaberDefense='%i', SaberThrow='%i', Absorb='%i', Heal='%i', Protect='%i', MindTrick='%i', TeamHeal='%i', Lightning='%i', Grip='%i', Drain='%i', Rage='%i', TeamEnergize='%i', StunBaton='%i', BlasterPistol='%i', BlasterRifle='%i', Disruptor='%i', Bowcaster='%i', Repeater='%i', DEMP2='%i', Flechette='%i', RocketLauncher='%i', ConcussionRifle='%i', BryarPistol='%i', Melee='%i', MaxShield='%i', ShieldStrength='%i', HealthStrength='%i', DrainShield='%i', Jetpack='%i', SenseHealth='%i', ShieldHeal='%i', TeamShieldHeal='%i', UniqueSkill='%i', BlasterPack='%i', PowerCell='%i', MetalBolts='%i', Rockets='%i', Thermals='%i', TripMines='%i', Detpacks='%i', Binoculars='%i', BactaCanister='%i', SentryGun='%i', SeekerDrone='%i', Eweb='%i', BigBacta='%i', ForceField='%i', CloakItem='%i', ForcePower='%i', Improvements='%i', Armor='%i', Flamethrower='%i', ShieldRegen='%i', HealthRegen='%i' WHERE CharID='%i';\
		UPDATE Weapons SET AmmoBlaster='%i', AmmoPowercell='%i', AmmoMetalBolts='%i', AmmoRockets='%i', AmmoThermal='%i', AmmoTripmine='%i', AmmoDetpack='%i' WHERE CharID='%i'";

	run_db_query(va(update_character_query,
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
		ent->client->ps.ammo[AMMO_BLASTER],
		ent->client->ps.ammo[AMMO_POWERCELL],
		ent->client->ps.ammo[AMMO_METAL_BOLTS],
		ent->client->ps.ammo[AMMO_ROCKETS],
		ent->client->ps.ammo[AMMO_THERMAL],
		ent->client->ps.ammo[AMMO_TRIPMINE],
		ent->client->ps.ammo[AMMO_DETPACK],
		ent->client->pers.CharID
	), db);

	sqlite3_close(db);

	return;
}
