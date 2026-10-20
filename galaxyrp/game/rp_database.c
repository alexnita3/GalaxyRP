#include "../../codemp/game/g_local.h"
#include "sqlite/sqlite3.h"

#define MAX_CHARACTERS_PER_ACCOUNT 15
#define MAX_NEWS_CHANNELS 16				// GalaxyRP (Alex): Max number of news channels.

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

void zyk_db_column_string(char* dest, int dest_size, sqlite3_stmt* stmt, int column)
{
	const unsigned char* value = NULL;

	if (!dest || dest_size < 1)
		return;

	value = sqlite3_column_text(stmt, column);

	if (!value)
	{
		dest[0] = '\0';
		return;
	}

	Q_strncpyz(dest, (const char*)value, dest_size);
}

/*
=====================================================================
All Table Methods
=====================================================================
*/

// GalaxyRP (Alex): [Database] This method loads the account information, as well as the information related to the default character, and assigns it to the entity.
void select_account_and_default_character_data(gentity_t* ent, char username[32], sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	char password[256], name[256], description[MAX_STRING_CHARS], netName[MAX_STRING_CHARS], modelName[MAX_STRING_CHARS];
	int accountID, player_settings, adminLevel, charID, credits, level, modelScale, skillpoints;

	// GalaxyRP fix: [Account] Don't allow two connections to log in on the same account.
	zyk_drop_other_sessions_on_account(ent, select_account_id_from_username(ent, username, db, zErrMsg, rc, stmt));

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
		zyk_db_column_string(saber1Model, sizeof(saber1Model), stmt, 17);
		// GLX_U: [Cosmetics] 64-bit reads: the hat and cape ids sit above the colour's 32 bits.
		stored1 = sqlite3_column_int64(stmt, 18);
		saber1Color = RP_STORED_COLOR(stored1);
		zyk_db_column_string(saber2Model, sizeof(saber2Model), stmt, 19);
		stored2 = sqlite3_column_int64(stmt, 20);
		saber2Color = RP_STORED_COLOR(stored2);
		zyk_default_empty_saber_models(saber1Model, sizeof(saber1Model), saber2Model, sizeof(saber2Model));

		ent->client->sess.accountID = accountID;
		ent->client->pers.player_settings = player_settings;
		ent->client->pers.bitvalue = adminLevel;
		Q_strncpyz(ent->client->pers.password, password, sizeof(ent->client->pers.password));
		Q_strncpyz(ent->client->sess.filename, username, sizeof(ent->client->sess.filename));
		ent->client->pers.CharID = charID;
		ent->client->pers.credits = credits;
		ent->client->pers.level = level;

		// Carry over silence
		ent->client->pers.player_statuses &= (1 << PLAYER_STATUS_SILENCED);

		do_scale(ent, modelScale);
		Q_strncpyz(ent->client->sess.rpgchar, name, sizeof(ent->client->sess.rpgchar));
		ent->client->pers.skillpoints = skillpoints;
		Q_strncpyz(ent->client->pers.description, description, sizeof(ent->client->pers.description));

		// GalaxyRP: [Saber RGB] restore this character's custom blade colours and blade styles.
		ent->client->pers.saberRGB[0] = (saber1Color & SABERRGB_SET) ? (saber1Color & SABERRGB_MASK) : 0;
		ent->client->pers.saberRGB[1] = (saber2Color & SABERRGB_SET) ? (saber2Color & SABERRGB_MASK) : 0;
		ent->client->pers.saberColorMode[0] = G_ValidateSaberColorMode(SABER_STORED_MODE(saber1Color));
		ent->client->pers.saberColorMode[1] = G_ValidateSaberColorMode(SABER_STORED_MODE(saber2Color));
		// GLX_U: [Cosmetics] and this character's hat and cape, for the same reason: the save that
		// update_saber() can trigger writes them back to this row. An id not on the list gives none.
		Q_strncpyz(ent->client->pers.cosmetic[RP_COSMETIC_HAT], G_RpCosmeticName(RP_COSMETIC_HAT, RP_STORED_COSMETIC(stored1)), sizeof(ent->client->pers.cosmetic[0]));
		Q_strncpyz(ent->client->pers.cosmetic[RP_COSMETIC_CAPE], G_RpCosmeticName(RP_COSMETIC_CAPE, RP_STORED_COSMETIC(stored2)), sizeof(ent->client->pers.cosmetic[0]));

		G_SyncSaberColorUserinfo(ent);

		G_SyncNameModelUserinfo(ent, netName, modelName);

		ent->client->sess.loggedin = qtrue;

		trap->SendServerCommand(ent->s.number, va("supdateloggedin %i\n", ent->client->sess.loggedin));

		int number_of_sabers = 1;

		if (strcmp(saber2Model, "none") != 0) {
			number_of_sabers = 2;
		}
		update_saber(ent, saber1Model, saber2Model, number_of_sabers + 1);

		// GalaxyRP (Alex): [Database] Column 21 is a duplicate of CharID, no need to grab that.
		for (int i = 0; i < NUM_OF_SKILLS; i++) {
			ent->client->pers.skill_levels[i] = sqlite3_column_int(stmt, i + 22);
		}

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

		set_netname(ent, netName);
		set_model(ent, modelName);


		update_saber_colors(ent);

		ent->client->sess.amrpgmode = 2;

		return;
	}

	sqlite3_finalize(stmt);
	G_LogPrintf("WARNING: select_account_and_default_character_data found no matching account+character row for username '%s' (selected character '%s') -- account/character data was not loaded.\n", username, ent->client->sess.rpgchar);

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

// GalaxyRP (Alex): [Database] This method saves the player's name, scale and model to the database. All the information is taken from ent. (Characters tables)
void update_current_character_name_and_model(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt) {
	char userinfo[MAX_INFO_STRING], modelName[MAX_INFO_STRING];

	int clientNum = ent->s.number;

	trap->GetUserinfo(clientNum, userinfo, sizeof(userinfo));
	Q_strncpyz(modelName, Info_ValueForKey(userinfo, "model"), sizeof(modelName));

	rc = sqlite3_prepare_v2(db, "UPDATE Characters SET ModelScale=?, NetName=?, ModelName=?, saberOneModel=?, saberTwoModel=?, saberOneColor=?, saberTwoColor=? WHERE CharID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	sqlite3_bind_int(stmt, 1, ent->client->ps.iModelScale);
	sqlite3_bind_text(stmt, 2, ent->client->pers.netname, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, modelName, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, ent->client->pers.saber1, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 5, ent->client->pers.saber2, -1, SQLITE_TRANSIENT);
	// GLX_U: [Cosmetics] the hat goes with the first saber's colour, the cape with the second's, as
	// an id above the colour's 32 bits -- see RP_STORED_PACK (g_local.h). A name not on the saved list
	// is stored as 0, none.
	sqlite3_bind_int64(stmt, 6, RP_STORED_PACK(SABER_STORED_PACK(ent->client->pers.saberColorMode[0], ent->client->pers.saberRGB[0]),
		G_RpCosmeticId(RP_COSMETIC_HAT, ent->client->pers.cosmetic[RP_COSMETIC_HAT])));
	sqlite3_bind_int64(stmt, 7, RP_STORED_PACK(SABER_STORED_PACK(ent->client->pers.saberColorMode[1], ent->client->pers.saberRGB[1]),
		G_RpCosmeticId(RP_COSMETIC_CAPE, ent->client->pers.cosmetic[RP_COSMETIC_CAPE])));
	sqlite3_bind_int(stmt, 8, ent->client->pers.CharID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	return;
}

// GalaxyRP (Alex): [Database] This method saves the player's scale to the database. All the information is taken from ent. (Characters tables)
void update_current_character_scale(gentity_t* ent, sqlite3* db) {
	int rc;
	sqlite3_stmt* stmt = 0;

	rc = sqlite3_prepare_v2(db, "UPDATE Characters SET ModelScale=? WHERE CharID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return;
	}
	sqlite3_bind_int(stmt, 1, ent->client->ps.iModelScale);
	sqlite3_bind_int(stmt, 2, ent->client->pers.CharID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	return;
}

// GalaxyRP (Alex): [Database] This method changes the character used currently by the player. It reassigns skills, weapons, userinfo, and changes the default character associated with the account.
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

		ent->client->pers.player_statuses &= (1 << PLAYER_STATUS_SILENCED);

		// GalaxyRP (Alex): [Database] Grab info from characters table.
		ent->client->pers.CharID = sqlite3_column_int(stmt, 1);
		ent->client->pers.credits = sqlite3_column_int(stmt, 2);
		ent->client->pers.level = sqlite3_column_int(stmt, 3);
		do_scale(ent, sqlite3_column_int(stmt, 4));

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

		zyk_db_column_string(saber1Model, sizeof(saber1Model), stmt, 11);
		// GLX_U: [Cosmetics] 64-bit reads: the hat and cape ids sit above the colour's 32 bits.
		stored1 = sqlite3_column_int64(stmt, 12);
		saber1Color = RP_STORED_COLOR(stored1);
		zyk_db_column_string(saber2Model, sizeof(saber2Model), stmt, 13);
		stored2 = sqlite3_column_int64(stmt, 14);
		saber2Color = RP_STORED_COLOR(stored2);
		zyk_default_empty_saber_models(saber1Model, sizeof(saber1Model), saber2Model, sizeof(saber2Model));

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
		G_SyncSaberColorUserinfo(ent);

		// GalaxyRP fix: [Account] same reason, same fix, but for display name and player model.
		G_SyncNameModelUserinfo(ent, displayName, modelName);

		int number_of_sabers = 1;

		if (strcmp(saber2Model, "none") != 0) {
			number_of_sabers = 2;
		}
		update_saber(ent, saber1Model, saber2Model, number_of_sabers + 1);

		// GalaxyRP (Alex): [Database] Grab info from skills table. (column 15 is CharID, no need to grab that)
		for (int i = 0; i < NUM_OF_SKILLS; i++) {
			ent->client->pers.skill_levels[i] = sqlite3_column_int(stmt, i + 16);
		}


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
		// whatever the client last sent.
		update_saber_colors(ent);
	}
	else
	{
		sqlite3_finalize(stmt);

		trap->SendServerCommand(ent - g_entities, va("print \"^1Character %s ^1could not be loaded (its data is incomplete). Your current character is unchanged.\n\"", character_name));
		trap->SendServerCommand(ent - g_entities, va("cp \"^1Character %s ^1could not be loaded.\n\"", character_name));
		G_LogPrintf("select_player_character: character '%s' (account %i) has a Characters row but no matching Skills/Weapons row -- load aborted.\n",
			character_name, ent->client->sess.accountID);

		return qfalse;
	}

	zyk_stop_active_force_powers(ent);
	zyk_reset_force_powerups(ent);
	zyk_stop_active_holdables(ent);
	RP_ClearPhaseMode(ent);
	zyk_apply_character_skills(ent);
	zyk_apply_character_loadout(ent);
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

	trap->SendServerCommand(ent - g_entities, "print \"^2Usage: /char <new/use/remove> <character name>\n\"");

	sqlite3_finalize(stmt);
}

// GalaxyRP (Alex): [Database] This method returns a list of character names readable by the UI.
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

// GalaxyRP (Alex): [Database] This method creates a new character associated with the account the player is currently logged in, and adds the default data in all the tables. ASSUMES PLAYER IS LOGGED IN!
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

	if (zyk_check_user_input(char_name, strlen(char_name)) == qfalse) {
		trap->SendServerCommand(ent - g_entities, "print \"^1Character name can only contain letters and numbers.\n\"");
		return qfalse;
	}

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

	if (sqlite3_exec(db, "BEGIN", 0, 0, NULL) != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		return qfalse;
	}

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

	return qtrue;
}

// GalaxyRP (Alex): [Database] This method removes a character form the database. It affects all tables that contain character information. ASSUMES THE PLAYER IS LOGGED IN!!!
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

// GalaxyRP (Alex): [Database] UPDATE This method updated a weapons table row with information contained within the entity with which it's called.
void update_weapons_table_row_with_current_values(gentity_t* ent) {

	sqlite3* db;
	int rc;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	char update_ammo_query[] = "UPDATE Weapons SET AmmoBlaster='%i', AmmoPowercell='%i', AmmoMetalBolts='%i', AmmoRockets='%i', AmmoThermal='%i', AmmoTripmine='%i', AmmoDetpack='%i' WHERE CharID='%i'";
	run_db_query(va(update_ammo_query,
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

/*
=====================================================================
NEWS Table Methods
=====================================================================
*/

// GalaxyRP (Alex): [Database] SELECT This method selects all the unique channels from the news table.
void select_news_channels(gentity_t* ent) {
	sqlite3* db;
	int rc;
	sqlite3_stmt* stmt = 0;
	char channelName[MAX_STRING_CHARS];

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	trap->SendServerCommand(ent - g_entities, "print \"^3Existing channels:\n\"");

	// GalaxyRP (Alex): [Database] Select all info from all character related tables.
	char select_channels_query[] = "SELECT DISTINCT channel COLLATE NOCASE \
		from News\
		ORDER BY channel COLLATE NOCASE";

	rc = sqlite3_prepare_v2(db, select_channels_query, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);

		sqlite3_close(db);
		return;
	}
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return;
	}
	while (rc == SQLITE_ROW)
	{
		// GalaxyRP (Alex): [Database] Grab all the channels line by line.
		zyk_db_column_string(channelName, sizeof(channelName), stmt, 0);
		zyk_print_long_line(ent, "", channelName);
		rc = sqlite3_step(stmt);
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return;
}

void select_news_from_channel(gentity_t* ent, char* channel, int numberOfEntries) {
	sqlite3* db;
	int rc;
	sqlite3_stmt* stmt = 0;
	char newsText[MAX_STRING_CHARS], date[MAX_STRING_CHARS];
	int newsID = 0;
	int i = 1;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	char select_news_query[] = "SELECT newsID, text, date\
		from(SELECT newsID, text, date from News WHERE channel = ? COLLATE NOCASE ORDER BY newsID DESC LIMIT ?) \
		ORDER BY newsID ASC";

	rc = sqlite3_prepare_v2(db, select_news_query, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);

		sqlite3_close(db);
		return;
	}
	sqlite3_bind_text(stmt, 1, channel, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, numberOfEntries);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW && rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return;
	}
	while (rc == SQLITE_ROW)
	{
		if (i <= numberOfEntries) {
			// GalaxyRP (Alex): [Database] Grab all the news texts line by line.
			newsID = sqlite3_column_int(stmt, 0);
			zyk_db_column_string(newsText, sizeof(newsText), stmt, 1);
			zyk_db_column_string(date, sizeof(date), stmt, 2);

			display_news(ent, newsID, newsText, date);
		}

		rc = sqlite3_step(stmt);
		i++;
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return;
}

// GalaxyRP (Alex): [Database] DELETE This method deletes a news table row which is associated with the ID given.
qboolean delete_news_table_row_with_id(gentity_t* ent, int newsID) {
	sqlite3* db;
	int rc;
	sqlite3_stmt* stmt = 0;
	qboolean deleted = qfalse;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return qfalse;
	}

	rc = sqlite3_prepare_v2(db, "DELETE FROM News WHERE newsID=?", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return qfalse;
	}
	sqlite3_bind_int(stmt, 1, newsID);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	else
	{
		deleted = (sqlite3_changes(db) > 0) ? qtrue : qfalse;
	}
	sqlite3_finalize(stmt);

	sqlite3_close(db);

	return deleted;
}

// GalaxyRP (Alex): [Database] INSERT This method inserts a new row in the news table, requires the channel and text be passed into it.
qboolean insert_news_table_row(gentity_t* ent, char* channel, char* news_text) {
	sqlite3* db;
	int rc;
	sqlite3_stmt* stmt = 0;
	int exists = 0;
	int channels = 0;

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return qfalse;
	}

	exists = news_channel_exists(channel, db, stmt);

	if (exists < 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"^1Could not read the news channels. Nothing was added.\n\"");
		sqlite3_close(db);
		return qfalse;
	}

	if (exists == 0)
	{
		channels = news_channel_count(db, stmt);

		if (channels < 0)
		{
			trap->SendServerCommand(ent->s.number, "print \"^1Could not read the news channels. Nothing was added.\n\"");
			sqlite3_close(db);
			return qfalse;
		}

		if (channels >= MAX_NEWS_CHANNELS)
		{
			trap->SendServerCommand(ent->s.number, va("print \"^1There are already %d news channels. Post to an existing one, or clear a channel out with /newsremove first.\n\"", MAX_NEWS_CHANNELS));
			sqlite3_close(db);
			return qfalse;
		}
	}

	rc = sqlite3_prepare_v2(db, "INSERT INTO News(channel, text) VALUES (?, ?)", -1, &stmt, NULL);
	if (rc != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return qfalse;
	}
	sqlite3_bind_text(stmt, 1, channel, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, news_text, -1, SQLITE_TRANSIENT);
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}
	sqlite3_finalize(stmt);

	sqlite3_close(db);

	return (rc == SQLITE_DONE) ? qtrue : qfalse;
}

// GalaxyRP (Alex): [Database] Check if a channel name exists.
int news_channel_exists(const char* channel, sqlite3* db, sqlite3_stmt* stmt)
{
	int found = -1;

	if (sqlite3_prepare_v2(db, "SELECT count(*) FROM News WHERE channel = ? COLLATE NOCASE", -1, &stmt, NULL) != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	sqlite3_bind_text(stmt, 1, channel, -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		found = (sqlite3_column_int(stmt, 0) > 0) ? 1 : 0;
	}
	else
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}

	sqlite3_finalize(stmt);

	return found;
}

// GalaxyRP (Alex): [News] how many distinct channels exist?
int news_channel_count(sqlite3* db, sqlite3_stmt* stmt)
{
	int count = -1;

	if (sqlite3_prepare_v2(db, "SELECT count(*) FROM (SELECT DISTINCT channel COLLATE NOCASE FROM News)", -1, &stmt, NULL) != SQLITE_OK)
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return -1;
	}

	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		count = sqlite3_column_int(stmt, 0);
	}
	else
	{
		trap->Print("SQL error: %s\n", sqlite3_errmsg(db));
	}

	sqlite3_finalize(stmt);

	return count;
}