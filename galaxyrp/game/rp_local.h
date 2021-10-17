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

#define NUM_OF_GUARDIANS				10 // zyk: number of Light Quest guardians to be defeated 
#define NUM_OF_OBJECTIVES				10 // zyk: number of Dark Quest objectives
#define NUM_OF_ETERNITY_QUEST_OBJ		11 // zyk: number of Eternity Quest objectives
#define NUM_OF_UNIVERSE_QUEST_OBJ		22 // zyk: number of Universe Quest objectives
#define NUM_OF_SKILLS					56 // zyk: number of RPG Mode skills

#define MAX_SHADER_REMAPS				128
#define MAX_RACERS						16 // zyk: Max racers in the map
#define MAX_DUEL_MATCHES				496 // zyk: max matches a tournament may have
#define MAX_CUSTOM_QUESTS				64 // zyk: max amount of custom quests
#define MAX_CUSTOM_QUEST_MISSIONS		512 // zyk: max missions a custom quest can have
#define MAX_MISSION_FIELD_LINES			8 // zyk: max lines of custom quest mission fields to send to client
#define MAX_CUSTOM_QUEST_FIELDS			512 // zyk: max fields a custom quest mission can have
#define MAX_BOUNTY_HUNTER_SENTRIES		5 // zyk: max sentries a Bounty Hunter can have if he has the Upgrade
#define MAX_RPG_CHARS					60 // zyk: max RPG chars an account can have
#define MAX_ACC_NAME_SIZE				30 // zyk: max characters an account or rpg char can have
#define MAX_JETPACK_FUEL				10000 // zyk: max jetpack fuel the player can have

#define JETPACK_SCALE					100 // zyk: used to scale the MAX_JETPACK_FUEL to set the jetpackFuel attribute. Dividing MAX_JETPACK_FUEL per JETPACK_SCALE must result in 100
#define NUMBER_OF_SELLER_ITEMS			56 // zyk: quantity of items at the jawa seller
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
	ADM_RPMODE,
	ADM_KICK,
	ADM_PARALYZE,
	ADM_GIVE,
	ADM_SCALE,
	ADM_PLAYERS,
	ADM_DUELARENA,
	ADM_CUSTOMQUEST,
	ADM_CREATEITEM,
	ADM_GOD,
	ADM_LEVELUP,
	ADM_SKILL,
	ADM_CREATECREDITS,
	ADM_IGNORECHATDISTANCE,
	ADM_NUM_CMDS

} zyk_admin_t;

// zyk: magic powers values
typedef enum {
	MAGIC_MAGIC_SENSE,
	MAGIC_HEALING_WATER,
	MAGIC_WATER_SPLASH,
	MAGIC_WATER_ATTACK,
	MAGIC_EARTHQUAKE,
	MAGIC_ROCKFALL,
	MAGIC_SHIFTING_SAND,
	MAGIC_SLEEPING_FLOWERS,
	MAGIC_POISON_MUSHROOMS,
	MAGIC_TREE_OF_LIFE,
	MAGIC_MAGIC_SHIELD,
	MAGIC_DOME_OF_DAMAGE,
	MAGIC_MAGIC_DISABLE,
	MAGIC_ULTRA_SPEED,
	MAGIC_SLOW_MOTION,
	MAGIC_FAST_AND_SLOW,
	MAGIC_FLAME_BURST,
	MAGIC_ULTRA_FLAME,
	MAGIC_FLAMING_AREA,
	MAGIC_BLOWING_WIND,
	MAGIC_HURRICANE,
	MAGIC_REVERSE_WIND,
	MAGIC_ULTRA_RESISTANCE,
	MAGIC_ULTRA_STRENGTH,
	MAGIC_ENEMY_WEAKENING,
	MAGIC_ICE_STALAGMITE,
	MAGIC_ICE_BOULDER,
	MAGIC_ICE_BLOCK,
	MAGIC_HEALING_AREA,
	MAGIC_MAGIC_EXPLOSION,
	MAGIC_LIGHTNING_DOME,
	MAX_MAGIC_POWERS

} zyk_magic_t;

// zyk: shader remap struct
typedef struct shaderRemap_s {

	char oldShader[MAX_QPATH];
	char newShader[MAX_QPATH];
	float timeOffset;

} shaderRemap_t;

shaderRemap_t remappedShaders[MAX_SHADER_REMAPS];

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

char		*zyk_get_mission_value(int custom_quest, int mission, char *key);
void		zyk_set_quest_field(int quest_number, int mission_number, char *key, char *value);
qboolean	zyk_is_ally(gentity_t *ent, gentity_t *other);
int			zyk_number_of_allies(gentity_t *ent, qboolean in_rpg_mode);
void		send_rpg_events(int send_event_timer);
int			zyk_get_remap_count();
void		zyk_text_message(gentity_t *ent, char *filename, qboolean show_in_chat, qboolean broadcast_message, ...);
qboolean	zyk_can_deflect_shots(gentity_t *ent);

#endif // __RP_LOCAL_H__
