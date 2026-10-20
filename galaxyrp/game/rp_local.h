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
void zyk_db_column_string(char *dest, int dest_size, sqlite3_stmt *stmt, int column);
void zyk_default_empty_saber_models(char *saber1, int saber1_size, char *saber2, int saber2_size);
void zyk_drop_other_sessions_on_account(gentity_t *ent, int accountID);
void G_SyncSaberColorUserinfo(gentity_t *ent);
void G_SyncNameModelUserinfo(gentity_t *ent, const char *netName, const char *modelName);

void do_scale(gentity_t *ent, int new_size);
void update_saber(gentity_t *ent, char *saber1Model, char *saber2Model, int number_of_args);
void set_netname(gentity_t *ent, char netName[MAX_STRING_CHARS]);
void set_model(gentity_t *ent, char modelName[MAX_STRING_CHARS]);
void zyk_reset_force_powerups(gentity_t *ent);
void zyk_stop_active_holdables(gentity_t *ent);
void zyk_apply_character_skills(gentity_t *ent);
void zyk_apply_character_loadout(gentity_t *ent);
void zyk_schedule_relog_kill(gentity_t *ent);
qboolean zyk_check_user_input(char *user_input, int user_input_size);
void zyk_print_long_line(gentity_t* ent, const char* prefix, const char* text);
void display_news(gentity_t* ent, int newsID, char* newsText, char* date);

#define NUM_OF_SKILLS					60 // zyk: number of RPG Mode skills

#define MAX_SHADER_REMAPS				128
#define MAX_DUEL_MATCHES				496 // zyk: max matches a tournament may have
#define MAX_RPG_CHARS					60 // zyk: max RPG chars an account can have
#define MAX_ACC_NAME_SIZE				30 // zyk: max characters an account or rpg char can have
#define MAX_JETPACK_FUEL				10000 // zyk: max jetpack fuel the player can have

#define JETPACK_SCALE					100 // zyk: used to scale the MAX_JETPACK_FUEL to set the jetpackFuel attribute. Dividing MAX_JETPACK_FUEL per JETPACK_SCALE must result in 100

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
	ADM_WEATHER,
	ADM_PHASE, // GalaxyRP (Alex): [Phase] Used to allow giving players different effects.
	ADM_NUM_CMDS

} zyk_admin_t;

// zyk: shader remap struct
typedef struct shaderRemap_s {

	char oldShader[MAX_QPATH];
	char newShader[MAX_QPATH];
	float timeOffset;

} shaderRemap_t;

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

/*
=====================================================================
Database functions
=====================================================================
*/

void run_db_query(const char* query, sqlite3* db);
qboolean select_accounts_table_row(gentity_t* ent, char* username, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
int select_account_id_from_username(gentity_t* ent, char* username, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
qboolean insert_accounts_table_row(gentity_t* ent, char* username, char* password, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
int select_number_of_accounts_with_username(gentity_t* ent, char* username, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
void update_accounts_table_row_with_current_values(gentity_t* ent);
void update_accounts_table_row_with_default_char(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
qboolean is_password_correct(gentity_t* ent, char* username, char* password, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
void update_credits_value(gentity_t* ent);
qboolean insert_chars_table_row(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt, int* out_charID);
int select_number_of_characters_with_name(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
int select_number_of_characters(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
int select_char_id_using_char_name(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
void update_chars_table_row_with_current_values(gentity_t* ent);
qboolean insert_skills_table_row(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt, int charID);
void update_skills_table_row_with_current_values(gentity_t* ent);
qboolean insert_weapons_table_row(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt, int charID);
void select_weapons_table_row_from_entity(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);

qboolean select_player_character(gentity_t* ent, char* character_name, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt, qboolean announce_switch);
void select_character_list(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
void select_character_list_for_ui(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt, char* CharString, int CharStringSize);
void select_account_and_default_character_data(gentity_t* ent, char username[32], sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
qboolean create_new_character(gentity_t* ent, char char_name[MAX_STRING_CHARS], sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
void remove_character(gentity_t* ent, char char_name[MAX_STRING_CHARS], sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
void update_current_character_and_account(gentity_t* ent);


void update_weapons_table_row_with_current_values(gentity_t* ent);
void zyk_db_column_string(char* dest, int dest_size, sqlite3_stmt* stmt, int column);
void select_news_channels(gentity_t* ent);
void select_news_from_channel(gentity_t* ent, char* channel, int numberOfEntries);
qboolean delete_news_table_row_with_id(gentity_t* ent, int newsID);
void update_current_character_name_and_model(gentity_t* ent, sqlite3* db, char* zErrMsg, int rc, sqlite3_stmt* stmt);
void update_current_character_scale(gentity_t* ent, sqlite3* db);
qboolean insert_news_table_row(gentity_t* ent, char* channel, char* news_text);
int news_channel_exists(const char* channel, sqlite3* db, sqlite3_stmt* stmt);
int news_channel_count(sqlite3* db, sqlite3_stmt* stmt);

#endif // __RP_LOCAL_H__
