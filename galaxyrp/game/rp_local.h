/*
=========================== GalaxyRP Mod ============================
Project based on OpenJK and Zyk Mod. Work copyrighted (C) 2020 - 2022
=====================================================================
[Description]: Local definitions for game module
=====================================================================
*/

#ifndef __RP_LOCAL_H__
#define __RP_LOCAL_H__

#include "../../galaxyrp/game/rp_version.h" // Version header

/*
=====================================================================
Global definitions
=====================================================================
*/

#define DB_PATH							"GalaxyRP/database/accounts.db"

// GalaxyRP fix: [Database] Centralised sqlite3_open() wrapper -- see definition in rp_database.c
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
#define RP_DB_BUSY_TIMEOUT_MS			5000
int RP_DB_Open(sqlite3 **db);
void run_db_query(const char *query, sqlite3 *db);
qboolean select_accounts_table_row(gentity_t* ent, char* username, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt *stmt);
int select_account_id_from_username(gentity_t* ent, char* username, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt *stmt);
qboolean insert_accounts_table_row(gentity_t* ent, char* username, char* password, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt *stmt);
int select_number_of_accounts_with_username(gentity_t* ent, char* username, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt *stmt);
void update_accounts_table_row_with_current_values(gentity_t* ent);
void update_accounts_table_row_with_default_char(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt *stmt);
qboolean is_password_correct(gentity_t* ent, char* username, char* password, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt *stmt);
void update_credits_value(gentity_t* ent);
qboolean insert_chars_table_row(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt *stmt, int* out_charID);
int select_number_of_characters_with_name(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt *stmt);
int select_number_of_characters(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt *stmt);
int select_char_id_using_char_name(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt *stmt);
void update_chars_table_row_with_current_values(gentity_t* ent);
qboolean insert_skills_table_row(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt *stmt, int charID);
void update_skills_table_row_with_current_values(gentity_t* ent);
qboolean insert_weapons_table_row(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt *stmt, int charID);
void select_weapons_table_row_from_entity(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt *stmt);

// GalaxyRP fix: [NPC] removed NUM_OF_GUARDIANS here. Its last user was the guardian_boss_1..10
// loop in the /npc spawn blocklist, which has been removed along with the rest of that list;
// the Light Quest it counted for was itself deleted as dead code in earlier work.
// GalaxyRP fix: [Quests] NUM_OF_OBJECTIVES (Dark Quest), NUM_OF_ETERNITY_QUEST_OBJ and
// NUM_OF_UNIVERSE_QUEST_OBJ used to be defined here; the quests are gone and nothing read them.
#define NUM_OF_SKILLS					60 // zyk: number of RPG Mode skills

#define MAX_SHADER_REMAPS				128
// GalaxyRP: [Race Mode] MAX_RACERS used to be defined here. Its only users were Cmd_RaceMode_f's
// starting-grid search and level.race_mode_vehicle[], both removed with Race Mode itself.
#define MAX_DUEL_MATCHES				496 // zyk: max matches a tournament may have
// GalaxyRP fix: [Quests] the MAX_CUSTOM_QUEST* sizes and MAX_BOUNTY_HUNTER_SENTRIES used to be
// defined here, for the custom-quest tables in level_locals_t and the Bounty Hunter class; both
// are gone.
#define MAX_RPG_CHARS					60 // zyk: max RPG chars an account can have
#define MAX_ACC_NAME_SIZE				30 // zyk: max characters an account or rpg char can have
#define MAX_JETPACK_FUEL				10000 // zyk: max jetpack fuel the player can have

#define JETPACK_SCALE					100 // zyk: used to scale the MAX_JETPACK_FUEL to set the jetpackFuel attribute. Dividing MAX_JETPACK_FUEL per JETPACK_SCALE must result in 100
// GalaxyRP fix: [Shop] NUMBER_OF_SELLER_ITEMS (56, one flat id space shared by every product the
// jawa seller ever sold, most of it long dead) replaced by two smaller counts for the surviving
// /buy item <n> and /buy upgrade <n> subcommands, each freshly numbered from 1.
#define NUMBER_OF_SHOP_ITEMS			12 // zyk: quantity of items sold via /buy item <n> and /stuff item <n>
#define NUMBER_OF_SHOP_UPGRADES			3 // zyk: quantity of upgrades sold via /buy upgrade <n> and /stuff upgrade <n>
#define DUEL_TOURNAMENT_ARENA_SIZE		64 // zyk: default size of the globe model used as the Duel Tournament arena
#define DUEL_TOURNAMENT_PROTECT_TIME	2000 // zyk: duration of the duelists protection in Duel Tournament

/*
=====================================================================
Player / world information
=====================================================================
*/

// zyk: admin bit values
typedef enum {

	ADM_NPC,
	ADM_NOCLIP,
	ADM_GIVEADM,
	ADM_TELE,
	ADM_ADMPROTECT,
	ADM_ENTITYSYSTEM,
	ADM_SILENCE,
	ADM_CLIENTPRINT,
	ADM_SHAKESCREEN,
	ADM_KICK,
	ADM_PARALYZE,
	ADM_GIVE,
	ADM_SCALE,
	ADM_PLAYERS,
	ADM_DUELARENA,
	// GalaxyRP fix: [Admin] this slot used to be ADM_CUSTOMQUEST, a leftover bit for a command
	// that was removed from the mod. Repurposed for the /admmap map-change command (bit value
	// stays 32768 -- 1 << 15 -- so existing accounts/configs that already granted this bit keep
	// working, and the default admin account's full-permission bitmask already covers it).
	ADM_CHANGEMAP,
	ADM_CREATEITEM,
	ADM_GOD,
	ADM_LEVELUP,
	ADM_SKILL,
	ADM_CREATECREDITS,
	ADM_IGNORECHATDISTANCE,
	ADM_XP,
	ADM_UPDATENEWS,
	ADM_REMOVENEWS,
	ADM_MUSIC,
	ADM_GETUP,
	// GalaxyRP: [Weather] /admweather. APPENDED, not inserted: an admin permission is a bit
	// POSITION in this enum and every account stores the resulting bitmask in the database, so
	// adding a name anywhere above this point renumbers every bit below it and silently changes
	// what every saved permission means. New bits go here, immediately before ADM_NUM_CMDS.
	// pers.bitvalue is a 32-bit int, so this one (bit 27) leaves three more before the sign bit.
	ADM_WEATHER,
	// GalaxyRP: [Phase] "Player effects": /ghost, /holo, /admwalkthrough and /admhidden -- appended for the same reason as above
	// (bit 28, bit value 268435456). Two bits remain before the sign bit.
	ADM_PHASE,
	ADM_NUM_CMDS

} zyk_admin_t;

// GalaxyRP fix: [Magic] the zyk_magic_t enum (MAGIC_MAGIC_SENSE ... MAX_MAGIC_POWERS) used to be
// here. Its last use was the session's power-selection defaults, removed with the magic system.

// zyk: shader remap struct
typedef struct shaderRemap_s {

	char oldShader[MAX_QPATH];
	char newShader[MAX_QPATH];
	float timeOffset;

} shaderRemap_t;

// GalaxyRP fix: [Shader Remap] this was "#ifdef __linux__ extern ... #else <definition> #endif",
// which gave the table no storage at all on Linux and one copy per platform accident on Windows.
//
// On Windows every translation unit that includes this header took the #else branch and emitted a
// tentative definition; the linker merges those as a common symbol, so it happened to work. On
// Linux every unit saw only the extern and nothing anywhere defined the array -- "nm -D" on
// jampgamex86_64.so shows remappedShaders as the one unresolved symbol in the whole module, and a
// data relocation is resolved when the module is loaded, not lazily, so dlopen() fails and the game
// module does not come up at all. Every shader-remap command, and the default remap preset, are
// dead on a Linux server for that reason.
//
// One declaration for both platforms, with the single definition beside remapCount in g_utils.c --
// the file that owns AddRemap(), BuildShaderStateConfig() and zyk_clear_all_remaps(). cgame and ui
// include this header too but reference nothing in the table, so neither module changes.
extern shaderRemap_t remappedShaders[MAX_SHADER_REMAPS];

typedef struct saber_db_info_s {
	char	saber1Model[50];
	char	saber2Model[50];
} saber_db_info_t;

saber_db_info_t select_saber_info_using_char_id(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt *stmt);

typedef struct chat_modifiers_s {
	const char* chat_modifier;
	const char* chat_format;
	int			distance;
} chat_modifiers_t;

typedef struct skill_s {
	int			max_level;
	const char* skill_name;
	const char* skill_description;
	const char* category;
	const char* alignment;

	//GalaxyRP (Alex): [Skills] Value to be used for internal code stuff. (such as force power enums, weapon enums etc..)
	int value_internal;
} skill_t;

/*
=====================================================================
Re-routed functions
=====================================================================
*/

/*
=====================================================================
Cvar registration
=====================================================================
*/

/*
=====================================================================
Common / new functions
=====================================================================
*/

qboolean	zyk_ally_bit_set(gentity_t *owner, int client_id);
qboolean	zyk_is_ally(gentity_t *ent, gentity_t *other);
int			zyk_number_of_allies(gentity_t *ent, qboolean in_rpg_mode);
void		zyk_remove_ally(gentity_t *ent, int client_id);
void		send_rpg_events(int send_event_timer);
int			zyk_get_remap_count();
void		zyk_text_message(gentity_t *ent, char *filename, qboolean show_in_chat, qboolean broadcast_message, ...);
qboolean	zyk_can_deflect_shots(gentity_t *ent);
void		zyk_clear_npc_order_bits(gentity_t *npc_ent);
void		zyk_release_npc_from_leader(gentity_t *npc_ent);
qboolean	zyk_npc_leader_lost(gentity_t *npc_ent);
void		zyk_hold_guarding_npc(gentity_t *npc_ent);
qboolean	zyk_npc_can_take_orders(gentity_t *npc_ent, gentity_t *leader);
void		zyk_release_player_npcs(gentity_t *ent);

#endif // __RP_LOCAL_H__
