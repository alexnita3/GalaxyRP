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

#pragma once

// g_local.h -- local definitions for game module

#include "qcommon/q_shared.h"
#include "bg_public.h"
#include "bg_vehicles.h"
#include "g_public.h"

typedef struct gentity_s gentity_t;
typedef struct gclient_s gclient_t;

//npc stuff
#include "b_public.h"

extern int gPainMOD;
extern int gPainHitLoc;
extern vec3_t gPainPoint;

//==================================================================

// the "gameversion" client command will print this plus compile date
#define	GAMEVERSION	"Galaxy RP Mod v0.2"

#define SECURITY_LOG "security.log"

#define BODY_QUEUE_SIZE		8

#ifndef INFINITE
#define INFINITE			1000000
#endif

#define	FRAMETIME			100					// msec
#define	CARNAGE_REWARD_TIME	3000
#define REWARD_SPRITE_TIME	2000

#define	INTERMISSION_DELAY_TIME	1000
#define	SP_INTERMISSION_DELAY_TIME	5000

//primarily used by NPCs
#define	START_TIME_LINK_ENTS		FRAMETIME*1 // time-delay after map start at which all ents have been spawned, so can link them
#define	START_TIME_FIND_LINKS		FRAMETIME*2 // time-delay after map start at which you can find linked entities
#define	START_TIME_MOVERS_SPAWNED	FRAMETIME*2 // time-delay after map start at which all movers should be spawned
#define	START_TIME_REMOVE_ENTS		FRAMETIME*3 // time-delay after map start to remove temporary ents
#define	START_TIME_NAV_CALC			FRAMETIME*4 // time-delay after map start to connect waypoints and calc routes
#define	START_TIME_FIND_WAYPOINT	FRAMETIME*5 // time-delay after map start after which it's okay to try to find your best waypoint

// gentity->flags
#define	FL_GODMODE				0x00000010
#define	FL_NOTARGET				0x00000020
#define	FL_TEAMSLAVE			0x00000400	// not the first on the team
#define FL_NO_KNOCKBACK			0x00000800
#define FL_DROPPED_ITEM			0x00001000
#define FL_NO_BOTS				0x00002000	// spawn point not for bot use
#define FL_NO_HUMANS			0x00004000	// spawn point just for bots
#define FL_FORCE_GESTURE		0x00008000	// force gesture on client
#define FL_INACTIVE				0x00010000	// inactive
#define FL_NAVGOAL				0x00020000	// for npc nav stuff
#define	FL_DONT_SHOOT			0x00040000
#define FL_SHIELDED				0x00080000
#define FL_UNDYING				0x00100000	// takes damage down to 1, but never dies

//ex-eFlags -rww
#define	FL_BOUNCE				0x00100000		// for missiles
#define	FL_BOUNCE_HALF			0x00200000		// for missiles
#define	FL_BOUNCE_SHRAPNEL		0x00400000		// special shrapnel flag

//vehicle game-local stuff -rww
#define	FL_VEH_BOARDING			0x00800000		// special shrapnel flag

//breakable flags -rww
#define FL_DMG_BY_SABER_ONLY		0x01000000 //only take dmg from saber
#define FL_DMG_BY_HEAVY_WEAP_ONLY	0x02000000 //only take dmg from explosives

#define FL_BBRUSH					0x04000000 //I am a breakable brush

#ifndef FINAL_BUILD
#define DEBUG_SABER_BOX
#endif

// make sure this matches game/match.h for botlibs
#define EC "\x19"

#define	MAX_G_SHARED_BUFFER_SIZE		8192
// used for communication with the engine
typedef union sharedBuffer_u {
	char							raw[MAX_G_SHARED_BUFFER_SIZE];
	T_G_ICARUS_PLAYSOUND			playSound;
	T_G_ICARUS_SET					set;
	T_G_ICARUS_LERP2POS				lerp2Pos;
	T_G_ICARUS_LERP2ORIGIN			lerp2Origin;
	T_G_ICARUS_LERP2ANGLES			lerp2Angles;
	T_G_ICARUS_GETTAG				getTag;
	T_G_ICARUS_LERP2START			lerp2Start;
	T_G_ICARUS_LERP2END				lerp2End;
	T_G_ICARUS_USE					use;
	T_G_ICARUS_KILL					kill;
	T_G_ICARUS_REMOVE				remove;
	T_G_ICARUS_PLAY					play;
	T_G_ICARUS_GETFLOAT				getFloat;
	T_G_ICARUS_GETVECTOR			getVector;
	T_G_ICARUS_GETSTRING			getString;
	T_G_ICARUS_SOUNDINDEX			soundIndex;
	T_G_ICARUS_GETSETIDFORSTRING	getSetIDForString;
} sharedBuffer_t;
extern sharedBuffer_t gSharedBuffer;

// movers are things like doors, plats, buttons, etc
typedef enum {
	MOVER_POS1,
	MOVER_POS2,
	MOVER_1TO2,
	MOVER_2TO1
} moverState_t;

#define SP_PODIUM_MODEL		"models/mapobjects/podium/podium4.md3"

typedef enum
{
	HL_NONE = 0,
	HL_FOOT_RT,
	HL_FOOT_LT,
	HL_LEG_RT,
	HL_LEG_LT,
	HL_WAIST,
	HL_BACK_RT,
	HL_BACK_LT,
	HL_BACK,
	HL_CHEST_RT,
	HL_CHEST_LT,
	HL_CHEST,
	HL_ARM_RT,
	HL_ARM_LT,
	HL_HAND_RT,
	HL_HAND_LT,
	HL_HEAD,
	HL_GENERIC1,
	HL_GENERIC2,
	HL_GENERIC3,
	HL_GENERIC4,
	HL_GENERIC5,
	HL_GENERIC6,
	HL_MAX
} hitLocation_t;

//============================================================================
extern void *precachedKyle;
extern void *g2SaberInstance;

extern qboolean gEscaping;
extern int gEscapeTime;

struct gentity_s {
	//rww - entstate must be first, to correspond with the bg shared entity structure
	entityState_t	s;				// communicated by server to clients
	playerState_t	*playerState;	//ptr to playerstate if applicable (for bg ents)
	Vehicle_t		*m_pVehicle; //vehicle data
	void			*ghoul2; //g2 instance
	int				localAnimIndex; //index locally (game/cgame) to anim data for this skel
	vec3_t			modelScale; //needed for g2 collision

	//From here up must be the same as centity_t/bgEntity_t

	entityShared_t	r;				// shared by both the server system and game

	//rww - these are shared icarus things. They must be in this order as well in relation to the entityshared structure.
	int				taskID[NUM_TIDS];
	parms_t			*parms;
	char			*behaviorSet[NUM_BSETS];
	char			*script_targetname;
	int				delayScriptTime;
	char			*fullName;

	//rww - targetname and classname are now shared as well. ICARUS needs access to them.
	char			*targetname;
	char			*classname;			// set in QuakeEd

	//rww - and yet more things to share. This is because the nav code is in the exe because it's all C++.
	int				waypoint;			//Set once per frame, if you've moved, and if someone asks
	int				lastWaypoint;		//To make sure you don't double-back
	int				lastValidWaypoint;	//ALWAYS valid -used for tracking someone you lost
	int				noWaypointTime;		//Debouncer - so don't keep checking every waypoint in existance every frame that you can't find one
	int				combatPoint;
	int				failedWaypoints[MAX_FAILED_NODES];
	int				failedWaypointCheckTime;

	int				next_roff_time; //rww - npc's need to know when they're getting roff'd

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!
	//================================

	struct gclient_s	*client;			// NULL if not a client

	gNPC_t		*NPC;//Only allocated if the entity becomes an NPC
	int			cantHitEnemyCounter;//HACK - Makes them look for another enemy on the same team if the one they're after can't be hit

	qboolean	noLumbar; //see note in cg_local.h

	qboolean	inuse;

	int			lockCount; //used by NPCs

	int			spawnflags;			// set in QuakeEd

	int			teamnodmg;			// damage will be ignored if it comes from this team

	char		*roffname;			// set in QuakeEd
	char		*rofftarget;		// set in QuakeEd

	char		*healingclass; //set in quakeed
	char		*healingsound; //set in quakeed
	int			healingrate; //set in quakeed
	int			healingDebounce; //debounce for generic object healing shiz

	char		*ownername;

	int			objective;
	int			side;

	int			passThroughNum;		// set to index to pass through (+1) for missiles

	int			aimDebounceTime;
	int			painDebounceTime;
	int			attackDebounceTime;
	int			alliedTeam;			// only useable by this team, never target this team

	int			roffid;				// if roffname != NULL then set on spawn

	qboolean	neverFree;			// if true, FreeEntity will only unlink
									// bodyque uses this

	int			flags;				// FL_* variables

	char		*model;
	char		*model2;
	int			freetime;			// level.time when the object was freed

	int			eventTime;			// events will be cleared EVENT_VALID_MSEC after set
	qboolean	freeAfterEvent;
	qboolean	unlinkAfterEvent;

	qboolean	physicsObject;		// if true, it can be pushed by movers and fall off edges
									// all game items are physicsObjects,
	float		physicsBounce;		// 1.0 = continuous bounce, 0.0 = no bounce
	int			clipmask;			// brushes with this content value will be collided against
									// when moving.  items and corpses do not collide against
									// players, for instance

//Only used by NPC_spawners
	char		*NPC_type;
	char		*NPC_targetname;
	char		*NPC_target;

	// movers
	moverState_t moverState;
	int			soundPos1;
	int			sound1to2;
	int			sound2to1;
	int			soundPos2;
	int			soundLoop;
	gentity_t	*parent;
	gentity_t	*nextTrain;
	gentity_t	*prevTrain;
	vec3_t		pos1, pos2;

	//for npc's
	vec3_t		pos3;

	char		*message;

	int			timestamp;		// body queue sinking, etc

	float		angle;			// set in editor, -1 = up, -2 = down
	char		*target;
	char		*target2;
	char		*target3;		//For multiple targets, not used for firing/triggering/using, though, only for path branches
	char		*target4;		//For multiple targets, not used for firing/triggering/using, though, only for path branches
	char		*target5;		//mainly added for siege items
	char		*target6;		//mainly added for siege items

	char		*team;
	char		*targetShaderName;
	char		*targetShaderNewName;
	gentity_t	*target_ent;

	char		*closetarget;
	char		*opentarget;
	char		*paintarget;

	char		*goaltarget;
	char		*idealclass;

	float		radius;

	int			maxHealth; //used as a base for crosshair health display

	float		speed;
	vec3_t		movedir;
	float		mass;
	int			setTime;

//Think Functions
	int			nextthink;
	void		(*think)(gentity_t *self);
	void		(*reached)(gentity_t *self);	// movers call this when hitting endpoint
	void		(*blocked)(gentity_t *self, gentity_t *other);
	void		(*touch)(gentity_t *self, gentity_t *other, trace_t *trace);
	void		(*use)(gentity_t *self, gentity_t *other, gentity_t *activator);
	void		(*pain)(gentity_t *self, gentity_t *attacker, int damage);
	void		(*die)(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod);

	int			pain_debounce_time;
	int			fly_sound_debounce_time;	// wind tunnel
	int			last_move_time;

//Health and damage fields
	int			health;
	qboolean	takedamage;
	material_t	material;

	int			damage;
	int			dflags;
	int			splashDamage;	// quad will increase this without increasing radius
	int			splashRadius;
	int			methodOfDeath;
	int			splashMethodOfDeath;

	int			locationDamage[HL_MAX];		// Damage accumulated on different body locations

	int			count;
	int			bounceCount;
	qboolean	alt_fire;

	gentity_t	*chain;
	gentity_t	*enemy;
	gentity_t	*lastEnemy;
	gentity_t	*activator;
	gentity_t	*teamchain;		// next entity in team
	gentity_t	*teammaster;	// master of the team

	int			watertype;
	int			waterlevel;

	int			noise_index;

	// timing variables
	float		wait;
	float		random;
	int			delay;

	//generic values used by various entities for different purposes.
	int			genericValue1;
	int			genericValue2;
	int			genericValue3;
	int			genericValue4;
	int			genericValue5;
	int			genericValue6;
	int			genericValue7;
	int			genericValue8;
	int			genericValue9;
	int			genericValue10;
	int			genericValue11;
	int			genericValue12;
	int			genericValue13;
	int			genericValue14;
	int			genericValue15;

	char		*soundSet;

	qboolean	isSaberEntity;

	int			damageRedirect; //if entity takes damage, redirect to..
	int			damageRedirectTo; //this entity number

	vec3_t		epVelocity;
	float		epGravFactor;

	gitem_t		*item;			// for bonus items

	// OpenJK add
	int			useDebounceTime;	// for cultist_destroyer
};

#define DAMAGEREDIRECT_HEAD		1
#define DAMAGEREDIRECT_RLEG		2
#define DAMAGEREDIRECT_LLEG		3

typedef enum {
	CON_DISCONNECTED,
	CON_CONNECTING,
	CON_CONNECTED
} clientConnected_t;

typedef enum {
	SPECTATOR_NOT,
	SPECTATOR_FREE,
	SPECTATOR_FOLLOW,
	SPECTATOR_SCOREBOARD
} spectatorState_t;

typedef enum {
	TEAM_BEGIN,		// Beginning a team game, spawn at base
	TEAM_ACTIVE		// Now actively playing
} playerTeamStateState_t;

typedef struct playerTeamState_s {
	playerTeamStateState_t	state;

	int			location;

	int			captures;
	int			basedefense;
	int			carrierdefense;
	int			flagrecovery;
	int			fragcarrier;
	int			assists;

	float		lasthurtcarrier;
	float		lastreturnedflag;
	float		flagsince;
	float		lastfraggedcarrier;
} playerTeamState_t;

// the auto following clients don't follow a specific client
// number, but instead follow the first two active players
#define	FOLLOW_ACTIVE1	-1
#define	FOLLOW_ACTIVE2	-2

// client data that stays across multiple levels or tournament restarts
// this is achieved by writing all the data to cvar strings at game shutdown
// time and reading them back at connection time.  Anything added here
// MUST be dealt with in G_InitSessionData() / G_ReadSessionData() / G_WriteSessionData()
typedef struct clientSession_s {
	team_t		sessionTeam;
	int			spectatorNum;		// for determining next-in-line to play
	spectatorState_t	spectatorState;
	int			spectatorClient;	// for chasecam and follow mode
	int			wins, losses;		// tournament stats
	int			selectedFP;			// check against this, if doesn't match value in playerstate then update userinfo
	int			saberLevel;			// similar to above method, but for current saber attack level
	int			setForce;			// set to true once player is given the chance to set force powers
	int			updateUITime;		// only update userinfo for FP/SL if < level.time
	qboolean	teamLeader;			// true when this client is a team leader
	char		siegeClass[64];
	int			duelTeam;
	int			siegeDesiredTeam;

	char		IP[NET_ADDRSTRMAXLEN];

	// zyk: sets the Player Mode:
	// 0 - Player is not logged in: in this mode, player didnt login his account yet
	// 1 - Admin-Only mode: in this mode, player can use admin commands if he has them
	// 2 - RPG mode: in this mode, player can use admin commands and play the level system
	int	amrpgmode; // zyk: saved in session so the player account can be loaded again in map changes
	char filename[32]; // zyk: player account filename

	char rpgchar[32]; // zyk: file name of the RPG char

	// zyk: current power selected by Magic Master to use
	// 1 - Magic Sense
	// 2 - Healing Water
	// 3 - Water Splash
	// 4 - Earthquake
	// 5 - Rockfall
	// 6 - Sleeping Flowers
	// 7 - Poison Mushrooms
	// 8 - Magic Shield
	// 9 - Dome of Damage
	// 10 - Ultra Speed
	// 11 - Slow Motion
	// 12 - Flame Burst
	// 13 - Ultra Flame
	// 14 - Blowing Wind
	// 15 - Hurricane
	// 16 - Ultra Resistance
	// 17 - Ultra Strength
	// 18 - Ice Stalagmite
	// 19 - Ice Boulder
	// 20 - Healing Area
	int selected_special_power;

	// zyk: same as above but selects power to be used with A + melee kata
	int selected_left_special_power;

	// zyk: same as above but selects power to be used with D + melee kata
	int selected_right_special_power;
	
	// zyk: activates Magic Fist types
	// Possible values are:
	// 0 - Magic Fist
	// 1 - Fist Charged Attack
	// 2 - Fist Spray Attack
	// 3 - No fist attacks
	int magic_fist_selection;

	// zyk: magic powers that will not be shown in Magic power selection
	int magic_disabled_powers;
	int magic_more_disabled_powers;

	// zyk: vote timer, used to avoid vote spam
	int vote_timer;

	// zyk: used to set the ally ids. The allies dont receive damage from this player
	int ally1;
	int ally2;
} clientSession_t;

// playerstate mGameFlags
#define	PSG_VOTED				(1<<0)		// already cast a vote
#define PSG_TEAMVOTED			(1<<1)		// already cast a team vote

//
#define MAX_NETNAME			36
#define	MAX_VOTE_COUNT		3

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

// zyk: number of Light Quest guardians to be defeated 
#define NUMBER_OF_GUARDIANS 10

// zyk: number of Dark Quest objectives
#define NUMBER_OF_OBJECTIVES 10

// zyk: number of Eternity Quest objectives
#define NUMBER_OF_ETERNITY_QUEST_OBJECTIVES 11

// zyk: number of Universe Quest objectives
#define NUMBER_OF_UNIVERSE_QUEST_OBJECTIVES 22

// zyk: number of RPG Mode skills
#define NUMBER_OF_SKILLS 56

// zyk: max sentries a Bounty Hunter can have if he has the Upgrade
#define MAX_BOUNTY_HUNTER_SENTRIES 5

// zyk: max RPG chars an account can have
#define MAX_RPG_CHARS 15

// zyk: max characters an account or rpg char can have
#define MAX_ACC_NAME_SIZE 30

// zyk: max jetpack fuel the player can have
#define MAX_JETPACK_FUEL 10000
#define JETPACK_SCALE 100 // zyk: used to scale the MAX_JETPACK_FUEL to set the jetpackFuel attribute. Dividing MAX_JETPACK_FUEL per JETPACK_SCALE must result in 100

// zyk: quantity of items at the jawa seller
#define NUMBER_OF_SELLER_ITEMS 56

// zyk: default size of the globe model used as the Duel Tournament arena
#define DUEL_TOURNAMENT_ARENA_SIZE 64

// zyk: duration of the duelists protection in Duel Tournament
#define DUEL_TOURNAMENT_PROTECTION_TIME 2000

// client data that stays across multiple respawns, but is cleared
// on each level change or team change at ClientBegin()
typedef struct clientPersistant_s {
	clientConnected_t	connected;
	usercmd_t	cmd;				// we would lose angles if not persistant
	qboolean	localClient;		// true if "ip" info key is "localhost"
	qboolean	initialSpawn;		// the first spawn should be at a cool location
	qboolean	predictItemPickup;	// based on cg_predictItems userinfo
	qboolean	pmoveFixed;			//
	char		netname[MAX_NETNAME];
	char		netname_nocolor[MAX_NETNAME];
	int			netnameTime;				// Last time the name was changed
	int			maxHealth;			// for handicapping
	int			enterTime;			// level.time the client entered the game
	playerTeamState_t teamState;	// status in teamplay games
	qboolean	teamInfo;			// send team overlay updates?

	int			connectTime;

	char		saber1[MAX_QPATH], saber2[MAX_QPATH];

	int			vote, teamvote; // 0 = none, 1 = yes, 2 = no

	char		guid[33];

	// zyk: account system attributes

	// zyk: Possible bit values are:
	// 0 - Silenced by an admin
	// 1 - using an emote
	// 2 - Received Radar event - set after client-side receives the Bounty Hunter Upgrade event
	// 3 - Received Jetpack Flame event - set after client-side receives the Jetpack Upgrade event
	// 4 - Scaled player
	// 5 - Chat protection activated for this player
	// 6 - Paralyzed by an admin
	// 7 - send event so client-side mod knows if this is a Force User or not, to render the Force Shield effect
	// 8 - using Saber Armor
	// 9 - using Gun Armor
	// 10 - using Healing Crystal
	// 11 - using Energy Crystal
	// 12 - Give Command - Force
	// 13 - Give Command - Guns
	// 14 - Sending current Magic Power event
	// 15 - Sending Immunity Power event
	// 16 - Sending Ultra Strength event
	// 17 - Sending Ultra Resistance event
	// 18 - NPC has the guard order
	// 19 - NPC has the cover order
	// 20 - hit by poison dart
	// 21 - Unique Ability 1
	// 22 - Unique Ability 2
	// 23 - Unique Ability 3
	// 24 - hit by Ice Bomb
	// 25 - RPG Mode tutorial
	// 26 - Using nofight command
	// 27 - Has just lost his duel in Duel Tournament
	// 28 - Custom Quest npc
	int player_statuses;

	// zyk: used to backup player force powers before some event that does not allow them. They will be restored after event ends
	int zyk_saved_force_powers;
	int zyk_saved_force_power_levels[NUM_FORCE_POWERS];

	// zyk: time in which quest player is afk during a quest
	int quest_afk_timer;

	// zyk: amount of times player must be hit by poison
	int poison_dart_hit_counter;

	// zyk: player who hit the target with poison dart
	int poison_dart_user_id;

	// zyk: timer of the poison darts
	int poison_dart_hit_timer;

	// zyk: used by Wrist Shot ability
	int wrist_shot_counter;

	// zyk: used by Ice Bomb ability
	int ice_bomb_counter;

	// zyk: cooldown time to buy or sell
	int buy_sell_timer;

	int player_scale;

	// zyk: chat protection cooldown timer. After this time, player will be protected against damage
	int chat_protection_timer;

	// zyk: used to print all mission fields when editing a Custom Quest
	int custom_quest_print;
	int custom_quest_print_timer;
	int custom_quest_quest_number;
	int custom_quest_mission_number;

	// zyk: has the player id who called this seller
	int seller_invoked_by_id;

	// zyk: timer to send events to client game
	int send_event_timer;
	int send_event_interval;

	// zyk: point marked in map so player can teleport to this point
	vec3_t teleport_point;
	vec3_t teleport_angles;

	int	bitvalue; // zyk: player is considered as admin if bitvalue is > 0, because he has at least 1 admin command
	
	int level; // zyk: RPG mode level
	int level_up_score; // zyk: RPG mode Level Up Score
	int skillpoints; // zyk: RPG mode skillpoints

	char password[32]; // zyk: account password

	// zyk: turn on or off features of this player in his account file. It is a bit value attribute
	// Possible bit values are:
	// 0 - RPG quests
	// 1 - Light Power
	// 2 - Dark Power
	// 3 - Eternity Power
	// 4 - Universe Power
	// 5 - Custom Language
	// 6 - Allow Force Powers from allies
	// 7 - Show magic cast in chat
	// 9 - Allow Screen Message
	// 10 - Use healing force only at allied players
	// 11 - Start With Saber
	// 12 - Jetpack
	// 13 - Admin Protect
	// 14 - Boss Battle Music
	// 15 - Difficulty
	// 26 - Saber Starts with Single Yellow/Dual style
	// 27 - Saber Starts with Single Red/Dual/Staff style
	// 28 - Saber Starts with Desann/Dual/Staff style
	// 29 - Saber Starts with Tavion/Dual/Staff style
	int player_settings;

	// zyk: used to set the player class in the RPG Mode
	// Possible values are:
	// 0 - Free Warrior (default)
	// 1 - Force User
	// 2 - Bounty Hunter
	// 3 - Armored Soldier
	// 4 - Monk
	// 5 - Stealth Attacker
	// 6 - Duelist
	// 7 - Force Gunner
	// 8 - Magic Master
	// 9 - Force Guardian
	int rpg_class;

	// zyk: this is the cooldown timer of the Unique Skill used by some RPG classes
	int unique_skill_timer;

	// zyk: used to set the duration that some unique skills or unique abilities are active
	int unique_skill_duration;

	// zyk: Monk Spin Kick ability interval between hits. Also used by Monk Meditation Drain ability as interval between hits
	int monk_unique_timer;

	// zyk: used by Fast Dash ability
	int fast_dash_timer;

	// zyk: used by Aimed Shot ability
	int unique_skill_user_id;

	// zyk: stun baton 3/3 timer. This entity has less run speed during this time
	int stun_baton_less_speed_timer;

	// zyk: when a bounty hunter is using the thermal vision, it is set to qtrue
	// zyk: when a stealth attacker is using the sniper scope, it is set to true
	qboolean thermal_vision;

	int thermal_vision_cooldown_time;

	// zyk: timer to show effect of Vertical DFA ability
	int vertical_dfa_timer;

	// zyk: timer to keep this player stunned by No Attack ability
	int no_attack_timer;

	// zyk: RPG skills
	int skill_levels[NUMBER_OF_SKILLS];

	int max_rpg_health; // zyk: max health the player can have in RPG Mode. This is set to STAT_MAX_HEALTH for RPG players
	int max_rpg_shield; // zyk: max shield the player can have in RPG Mode based in the skill_levels[30] value

	int jetpack_fuel; // zyk: now this is the fuel that is spent. Then we scale this value to the 0 - 100 range to set it in the jetpackFuel attribute to show the fuel bar correctly to the client

	int sense_health_timer; // zyk: used to periodically show health of player or npc with Sense Health skill

	int flame_thrower; // zyk: used by stun baton. Its the flame thrower timer
	
	// zyk: tests if this player or npc is being mind controlled by a player. If -1, means that he is not being controlled, otherwise it has the player id of the player who is controlling this player or npc
	int being_mind_controlled;

	// zyk: entity ids of the mind controlled entities. Default -1, which means player is not controlling anyone
	int mind_controlled1_id;

	// zyk: bit flag, loaded in load_account()
	// Possible bit values (1 << bit_value) are:
	// 0 - Holdable Items Upgrade
	// 1 - Bounty Hunter Upgrade
	// 2 - Unique Ability 1
	// 3 - Unique Ability 2
	// 4 - Unique Ability 3
	// 5 - Unused
	// 6 - Unused
	// 7 - Stealth Attacker Upgrade
	// 8 - Force Gunner Upgrade
	// 9 - Impact Reducer
	// 10 - Flame Thrower
	// 11 - Power Cell Weapons Upgrade
	// 12 - Blaster Pack Weapons Upgrade
	// 13 - Metal Bolts Weapons Upgrade
	// 14 - Rocket Upgrade
	// 15 - Stun Baton Upgrade
	// 16 - Armored Soldier Upgrade
	// 17 - Jetpack Upgrade
	// 18 - Unused
	// 19 - Force Guardian Upgrade
	int secrets_found;

	// zyk: amount of sentries the Bounty Hunter starts with
	int bounty_hunter_sentries;

	// zyk: amount of sentries placed in map
	int bounty_hunter_placed_sentries;

	int max_force_power; // zyk: max force power the player can have based on skill_levels[54] value

	int score_modifier; // zyk: sets the amount of extra score a player can get by defeating some npcs

	int credits_modifier; // zyk: sets the amount of extra credits a player can get by killing rpg players or some npcs
	int credits; // zyk: the amount of credits (RPG Mode currency) this player has now

	int tutorial_step; // zyk: sets the current tutorial step, to display the correct message to hthe player
	int tutorial_timer; // zyk: used by the tutorial to set the interval between messages

	// zyk: Race Mode. Sets the initial position of this racer which is calculated in racemode command. Default 0. If greater than 0, player joined a race
	int race_position;

	// zyk: tests if this player can play a RPG Mode quest. Default 0. If 1, player can play a quest now
	int can_play_quest;

	// zyk: amount of skills used by the player. After a certain amount of uses, player gets 1 experience point (level up score)
	int skill_counter;

	// zyk: number of guardians the player already defeated
	// the value will be NUMBER_OF_GUARDIANS after completing the quest
	// before that, has bitvalue of each defeated guardian. Possible bitvalues are:
	// 4 - Guardian of Water
	// 5 - Guardian of Earth
	// 6 - Guardian of Forest
	// 7 - Guardian of Intelligence
	// 8 - Guardian of Agility
	// 9 - Guardian of Fire
	// 10 - Guardian of Wind
	// 11 - Guardian of Resistance
	// 12 - Guardian of Ice
	int defeated_guardians; 

	// zyk: number of notes collected in Dark Quest
	// the value will be NUMBER_OF_OBJECTIVES after completing the quest
	// before that, has bitvalue of each collected note. Possible bitvalues are:
	// 4 - Note at yavin1b
	// 5 - Note at t1_sour
	// 6 - Note at t1_surprise
	// 7 - Note at t3_rift
	// 8 - Note at hoth2
	// 9 - Note at t3_bounty
	// 10 - Note at t2_rogue
	// 11 - Note at t1_danger
	// 12 - Note at mp/duel6
	int hunter_quest_progress;

	// zyk: number of objectives completed in Eternity Quest
	int eternity_quest_progress;

	// zyk: Universe Quest progress of this player
	int universe_quest_progress;

	// zyk: counter used in some missions of Universe Quest
	// Possible bit values in artifacts objective:
	// 0 - Unused
	// 1 - Eternity Quest artifact got with the Sage of Eternity
	// 2 - Unused
	// 3 - Artifact in yavin1b
	// 4 - Artifact in t1_danger
	// 5 - Artifact in t1_fatal
	// 6 - Artifact in t3_bounty
	// 7 - Artifact in hoth3
	// 8 - Artifact in yavin2
	// 9 - Artifact in t2_dpred

	// Possible bit values in amulets objective:
	// 0 - Amulet of Light
	// 1 - Amulet of Darkness
	// 2 - Amulet of Eternity

	// The choosing mission can have permanent bit values with these possible values:
	// 0 - Player chose to allow the sages to get into the Sacred Dimension
	// 1 - Player chose to allow the guardians to get into the Sacred Dimension
	// 2 - Player chose to allow the Master of Evil to get into the Sacred Dimension
	// 3 - Player chose to allow the Guardian of Time to get into the Sacred Dimension
	
	// Possible bit values in crystals objective:
	// 0 - Player got the Crystal of Destiny
	// 1 - Player got the Crystal of Truth
	// 2 - Player got the Crystal of Time

	// If player chose Challenge Mode in settings, sets a bit value 29
	int universe_quest_counter;

	// zyk: used to control some events in Universe Quest. In the quest_reborn_red npc, sets the player id who is playing the quest. In the player, sets how much quest_reborn_red npcs the player must kill to complete the first objective
	int universe_quest_objective_control;

	// zyk: used to set the npc who holds the artifact in the third objective of Universe Quest
	int universe_quest_artifact_holder_id;

	// zyk: controls the timed events in Universe Quest
	int universe_quest_messages;
	int universe_quest_timer;

	// zyk: bitvalue. Sets the power this player is using or the power that is affecting this player
	// Possible values are:
	//  0 - using Immunity Power
	//  1 - hit by Chaos Power
	//  2 - hit by Time Power
	//  3 - using Ultra Strength
	//  4 - hit by Poison Mushrooms
	//  5 - hit by Hurricane
	//  6 - hit by Slow Motion
	//  7 - using Ultra Resistance
	//  8 - hit by Blowing Wind
	//  9 - using Ultra Speed
	// 10 - using Resurrection Power
	// 11 - using Magic Shield
	// 12 - using Flame Burst
	// 13 - using Universe Power
	// 14 - using Light Power
	// 15 - using Dark Power
	// 16 - using Eternity Power
	// 17 - using Shifting Sand
	// 18 - Shifting Sand after teleport
	// 19 - using Tree of Life
	// 20 - hit by Reverse Wind
	// 21 - hit by Enemy Nerf
	// 22 - using Ice Block
	// 23 - hit by Flaming Area
	// 24 - hit by Sleeping Flowers
	// 25 - hit by Ice Boulder
	// 26 - hit by Elemental Attack
	int quest_power_status;

	// zyk: cooldown between quest power uses
	int quest_power_usage_timer;

	// zyk: powers that hits the target player more than once need a hit counter
	int quest_power_hit_counter;
	int quest_power_hit2_counter;
	int quest_power_hit3_counter;
	int quest_power_hit4_counter;

	// zyk: timers of the quest powers used by this player
	int quest_power1_timer;
	int quest_power2_timer;
	int quest_power3_timer;
	int quest_power4_timer;
	int quest_power5_timer;
	int quest_power6_timer;
	int quest_power7_timer;

	// zyk: timers used by the quest powers hitting this player
	int quest_target1_timer;
	int quest_target2_timer;
	int quest_target3_timer;
	int quest_target4_timer;
	int quest_target5_timer;
	int quest_target6_timer;
	int quest_target7_timer;
	int quest_target8_timer;
	int quest_target9_timer;
	int quest_target10_timer;
	int quest_target11_timer;

	// zyk: quest powers debounce timer, for example, like Wind powers
	int quest_debounce1_timer;

	// zyk: player ids which are hitting the target player
	int quest_power_user1_id;
	int quest_power_user2_id;
	int quest_power_user3_id;
	int quest_power_user4_id;
	int quest_power_user5_id;

	// zyk: sets the id of the effect of the magic used by this player
	int quest_power_effect1_id;

	// zyk: magic power, required to use Special Powers
	int magic_power;

	// zyk: controls the timed events in Light Quest
	int light_quest_messages;
	int light_quest_timer;

	// zyk: controls the timed events in Dark Quest
	int hunter_quest_timer;
	int hunter_quest_messages; // zyk: also used by the Guardian of Universe to know she already spawned the other guardians

	// zyk: used to show the riddles from time to time
	int eternity_quest_timer;

	// zyk: this attribute sets the player as fighting a guardian
	// the values of guardian_mode are:
	// 0 - Player is not fighting a guardian
	// 1 - Guardian of Water
	// 2 - Guardian of Earth
	// 3 - Guardian of Forest
	// 4 - Guardian of Intelligence
	// 5 - Guardian of Agility
	// 6 - Guardian of Fire
	// 7 - Guardian of Wind
	// 8 - Guardian of Light
	// 9 - Guardian of Darkness
	// 10 - Guardian of Eternity
	// 11 - Guardian of Resistance
	// 12 - Master of Evil
	// 13 - Guardian of Universe
	// 14 - Guardian of Chaos
	// 15 - Ymir and Thor
	// 16 - Guardian of Ice
	// 17 - Guardian Trials Bosses
	// 18 - Guardian Trials Final Bosses
	// 19 - Ymir
	// 20 - Guardian of Time
	// 21 - Soul of Sorrow
	int guardian_mode; 

	// zyk: used by the last guardians in quests for their special abilities
	int guardian_timer;

	// zyk: player id that is fighting this guardian
	int guardian_invoked_by_id;
} clientPersistant_t;

typedef struct renderInfo_s
{
	//In whole degrees, How far to let the different model parts yaw and pitch
	int		headYawRangeLeft;
	int		headYawRangeRight;
	int		headPitchRangeUp;
	int		headPitchRangeDown;

	int		torsoYawRangeLeft;
	int		torsoYawRangeRight;
	int		torsoPitchRangeUp;
	int		torsoPitchRangeDown;

	int		legsFrame;
	int		torsoFrame;

	float	legsFpsMod;
	float	torsoFpsMod;

	//Fields to apply to entire model set, individual model's equivalents will modify this value
	vec3_t	customRGB;//Red Green Blue, 0 = don't apply
	int		customAlpha;//Alpha to apply, 0 = none?

	//RF?
	int			renderFlags;

	//
	vec3_t		muzzlePoint;
	vec3_t		muzzleDir;
	vec3_t		muzzlePointOld;
	vec3_t		muzzleDirOld;
	//vec3_t		muzzlePointNext;	// Muzzle point one server frame in the future!
	//vec3_t		muzzleDirNext;
	int			mPCalcTime;//Last time muzzle point was calced

	//
	float		lockYaw;//

	//
	vec3_t		headPoint;//Where your tag_head is
	vec3_t		headAngles;//where the tag_head in the torso is pointing
	vec3_t		handRPoint;//where your right hand is
	vec3_t		handLPoint;//where your left hand is
	vec3_t		crotchPoint;//Where your crotch is
	vec3_t		footRPoint;//where your right hand is
	vec3_t		footLPoint;//where your left hand is
	vec3_t		torsoPoint;//Where your chest is
	vec3_t		torsoAngles;//Where the chest is pointing
	vec3_t		eyePoint;//Where your eyes are
	vec3_t		eyeAngles;//Where your eyes face
	int			lookTarget;//Which ent to look at with lookAngles
	lookMode_t	lookMode;
	int			lookTargetClearTime;//Time to clear the lookTarget
	int			lastVoiceVolume;//Last frame's voice volume
	vec3_t		lastHeadAngles;//Last headAngles, NOT actual facing of head model
	vec3_t		headBobAngles;//headAngle offsets
	vec3_t		targetHeadBobAngles;//head bob angles will try to get to targetHeadBobAngles
	int			lookingDebounceTime;//When we can stop using head looking angle behavior
	float		legsYaw;//yaw angle your legs are actually rendering at

	//for tracking legitimate bolt indecies
	void		*lastG2; //if it doesn't match ent->ghoul2, the bolts are considered invalid.
	int			headBolt;
	int			handRBolt;
	int			handLBolt;
	int			torsoBolt;
	int			crotchBolt;
	int			footRBolt;
	int			footLBolt;
	int			motionBolt;

	int			boltValidityTime;
} renderInfo_t;

// this structure is cleared on each ClientSpawn(),
// except for 'client->pers' and 'client->sess'
struct gclient_s {
	// ps MUST be the first element, because the server expects it
	playerState_t	ps;				// communicated by server to clients

	// the rest of the structure is private to game
	clientPersistant_t	pers;
	clientSession_t		sess;

	saberInfo_t	saber[MAX_SABERS];
	void		*weaponGhoul2[MAX_SABERS];

	int			tossableItemDebounce;

	int			bodyGrabTime;
	int			bodyGrabIndex;

	int			pushEffectTime;

	int			invulnerableTimer;

	int			saberCycleQueue;

	int			legsAnimExecute;
	int			torsoAnimExecute;
	qboolean	legsLastFlip;
	qboolean	torsoLastFlip;
	qboolean	isAutoWalking; //used for the autowalk feture

	qboolean	readyToExit;		// wishes to leave the intermission

	qboolean	noclip;

	int			lastCmdTime;		// level.time of last usercmd_t, for EF_CONNECTION
									// we can't just use pers.lastCommand.time, because
									// of the g_sycronousclients case
	int			buttons;
	int			oldbuttons;
	int			latched_buttons;

	vec3_t		oldOrigin;

	// sum up damage over an entire frame, so
	// shotgun blasts give a single big kick
	int			damage_armor;		// damage absorbed by armor
	int			damage_blood;		// damage taken out of health
	int			damage_knockback;	// impact damage
	vec3_t		damage_from;		// origin for vector calculation
	qboolean	damage_fromWorld;	// if true, don't use the damage_from vector

	int			damageBoxHandle_Head; //entity number of head damage box
	int			damageBoxHandle_RLeg; //entity number of right leg damage box
	int			damageBoxHandle_LLeg; //entity number of left leg damage box

	int			accurateCount;		// for "impressive" reward sound

	int			accuracy_shots;		// total number of shots
	int			accuracy_hits;		// total number of hits

	//
	int			lastkilled_client;	// last client that this client killed
	int			lasthurt_client;	// last client that damaged this client
	int			lasthurt_mod;		// type of damage the client did

	// timers
	int			respawnTime;		// can respawn when time > this, force after g_forcerespwan
	int			inactivityTime;		// kick players when time > this
	qboolean	inactivityWarning;	// qtrue if the five seoond warning has been given
	int			rewardTime;			// clear the EF_AWARD_IMPRESSIVE, etc when time > this

	int			airOutTime;

	int			lastKillTime;		// for multiple kill rewards

	qboolean	fireHeld;			// used for hook
	gentity_t	*hook;				// grapple hook if out

	int			switchTeamTime;		// time the player switched teams

	int			switchDuelTeamTime;		// time the player switched duel teams

	int			switchClassTime;	// class changed debounce timer

	// timeResidual is used to handle events that happen every second
	// like health / armor countdowns and regeneration
	int			timeResidual;

	char		*areabits;

	int			g2LastSurfaceHit; //index of surface hit during the most recent ghoul2 collision performed on this client.
	int			g2LastSurfaceTime; //time when the surface index was set (to make sure it's up to date)

	int			corrTime;

	vec3_t		lastHeadAngles;
	int			lookTime;

	int			brokenLimbs;

	qboolean	noCorpse; //don't leave a corpse on respawn this time.

	int			jetPackTime;

	qboolean	jetPackOn;
	int			jetPackToggleTime;
	int			jetPackDebRecharge;
	int			jetPackDebReduce;

	int			cloakToggleTime;
	int			cloakDebRecharge;
	int			cloakDebReduce;

	int			saberStoredIndex; //stores saberEntityNum from playerstate for when it's set to 0 (indicating saber was knocked out of the air)

	int			saberKnockedTime; //if saber gets knocked away, can't pull it back until this value is < level.time

	vec3_t		olderSaberBase; //Set before lastSaberBase_Always, to whatever lastSaberBase_Always was previously
	qboolean	olderIsValid;	//is it valid?

	vec3_t		lastSaberDir_Always; //every getboltmatrix, set to saber dir
	vec3_t		lastSaberBase_Always; //every getboltmatrix, set to saber base
	int			lastSaberStorageTime; //server time that the above two values were updated (for making sure they aren't out of date)

	qboolean	hasCurrentPosition;	//are lastSaberTip and lastSaberBase valid?

	int			dangerTime;		// level.time when last attack occured

	int			idleTime;		//keep track of when to play an idle anim on the client.

	int			idleHealth;		//stop idling if health decreases
	vec3_t		idleViewAngles;	//stop idling if viewangles change

	int			forcePowerSoundDebounce; //if > level.time, don't do certain sound events again (drain sound, absorb sound, etc)

	char		modelname[MAX_QPATH];

	qboolean	fjDidJump;

	qboolean	ikStatus;

	int			throwingIndex;
	int			beingThrown;
	int			doingThrow;

	float		hiddenDist;//How close ents have to be to pick you up as an enemy
	vec3_t		hiddenDir;//Normalized direction in which NPCs can't see you (you are hidden)

	renderInfo_t	renderInfo;

	//mostly NPC stuff:
	npcteam_t	playerTeam;
	npcteam_t	enemyTeam;
	char		*squadname;
	gentity_t	*team_leader;
	gentity_t	*leader;
	gentity_t	*follower;
	int			numFollowers;
	gentity_t	*formationGoal;
	int			nextFormGoal;
	class_t		NPC_class;

	vec3_t		pushVec;
	int			pushVecTime;

	int			siegeClass;
	int			holdingObjectiveItem;

	//time values for when being healed/supplied by supplier class
	int			isMedHealed;
	int			isMedSupplied;

	//seperate debounce time for refilling someone's ammo as a supplier
	int			medSupplyDebounce;

	//used in conjunction with ps.hackingTime
	int			isHacking;
	vec3_t		hackingAngles;

	//debounce time for sending extended siege data to certain classes
	int			siegeEDataSend;

	int			ewebIndex; //index of e-web gun if spawned
	int			ewebTime; //e-web use debounce
	int			ewebHealth; //health of e-web (to keep track between deployments)

	int			inSpaceIndex; //ent index of space trigger if inside one
	int			inSpaceSuffocation; //suffocation timer

	int			tempSpectate; //time to force spectator mode

	//keep track of last person kicked and the time so we don't hit multiple times per kick
	int			jediKickIndex;
	int			jediKickTime;

	//special moves (designed for kyle boss npc, but useable by players in mp)
	int			grappleIndex;
	int			grappleState;

	int			solidHack;

	int			noLightningTime;

	unsigned	mGameFlags;

	//fallen duelist
	qboolean	iAmALoser;

	int			lastGenCmd;
	int			lastGenCmdTime;

	struct force {
		int		regenDebounce;
		int		drainDebounce;
		int		lightningDebounce;
	} force;
};

//Interest points

#define MAX_INTEREST_POINTS		64

typedef struct
{
	vec3_t		origin;
	char		*target;
} interestPoint_t;

//Combat points

#define MAX_COMBAT_POINTS		512

typedef struct
{
	vec3_t		origin;
	int			flags;
//	char		*NPC_targetname;
//	team_t		team;
	qboolean	occupied;
	int			waypoint;
	int			dangerTime;
} combatPoint_t;

// Alert events

#define	MAX_ALERT_EVENTS	32

typedef enum
{
	AET_SIGHT,
	AET_SOUND,
} alertEventType_e;

typedef enum
{
	AEL_MINOR,			//Enemy responds to the sound, but only by looking
	AEL_SUSPICIOUS,		//Enemy looks at the sound, and will also investigate it
	AEL_DISCOVERED,		//Enemy knows the player is around, and will actively hunt
	AEL_DANGER,			//Enemy should try to find cover
	AEL_DANGER_GREAT,	//Enemy should run like hell!
} alertEventLevel_e;

typedef struct alertEvent_s
{
	vec3_t				position;	//Where the event is located
	float				radius;		//Consideration radius
	alertEventLevel_e	level;		//Priority level of the event
	alertEventType_e	type;		//Event type (sound,sight)
	gentity_t			*owner;		//Who made the sound
	float				light;		//ambient light level at point
	float				addLight;	//additional light- makes it more noticable, even in darkness
	int					ID;			//unique... if get a ridiculous number, this will repeat, but should not be a problem as it's just comparing it to your lastAlertID
	int					timestamp;	//when it was created
} alertEvent_t;

//
// this structure is cleared as each map is entered
//
typedef struct waypointData_s {
	char	targetname[MAX_QPATH];
	char	target[MAX_QPATH];
	char	target2[MAX_QPATH];
	char	target3[MAX_QPATH];
	char	target4[MAX_QPATH];
	int		nodeID;
} waypointData_t;

typedef struct {
	char	message[MAX_SPAWN_VARS_CHARS];
	int		count;
	int		cs_index;
	vec3_t	origin;
} locationData_t;

// zyk: Max racers in the map
#define MAX_RACERS 16

// zyk: max matches a tournament may have
#define MAX_DUEL_MATCHES 496

// zyk: max amount of custom quests
#define MAX_CUSTOM_QUESTS 64

// zyk: max missions a custom quest can have
#define MAX_CUSTOM_QUEST_MISSIONS 512

// zyk: max lines of custom quest mission fields to send to client
#define MAX_MISSION_FIELD_LINES 8

// zyk: max fields a custom quest mission can have
#define MAX_CUSTOM_QUEST_FIELDS 512

typedef struct level_locals_s {
	struct gclient_s	*clients;		// [maxclients]

	struct gentity_s	*gentities;
	int			gentitySize;
	int			num_entities;		// current number, <= MAX_GENTITIES

	int			warmupTime;			// restart match at this time

	fileHandle_t	logFile;

	// store latched cvars here that we want to get at often
	int			maxclients;

	int			framenum;
	int			time;					// in msec
	int			previousTime;			// so movers can back up when blocked

	int			startTime;				// level.time the map was started

	int			teamScores[TEAM_NUM_TEAMS];
	int			lastTeamLocationTime;		// last time of client team location update

	qboolean	newSession;				// don't use any old session data, because
										// we changed gametype

	qboolean	restarted;				// waiting for a map_restart to fire

	int			numConnectedClients;
	int			numNonSpectatorClients;	// includes connecting clients
	int			numPlayingClients;		// connected, non-spectators
	int			sortedClients[MAX_CLIENTS];		// sorted by score
	int			follow1, follow2;		// clientNums for auto-follow spectators

	int			snd_fry;				// sound index for standing in lava

	int			snd_hack;				//hacking loop sound
    int			snd_medHealed;			//being healed by supply class
	int			snd_medSupplied;		//being supplied by supply class

	// voting state
	char		voteString[MAX_STRING_CHARS];
	char		voteStringClean[MAX_STRING_CHARS];
	char		voteDisplayString[MAX_STRING_CHARS];
	int			voteTime;				// level.time vote was called
	int			voteExecuteTime;		// time the vote is executed
	int			voteExecuteDelay;		// set per-vote
	int			voteYes;
	int			voteNo;
	int			numVotingClients;		// set by CalculateRanks

	qboolean	votingGametype;
	int			votingGametypeTo;

	// team voting state
	char		teamVoteString[2][MAX_STRING_CHARS];
	char		teamVoteStringClean[2][MAX_STRING_CHARS];
	char		teamVoteDisplayString[2][MAX_STRING_CHARS];
	int			teamVoteTime[2];		// level.time vote was called
	int			teamVoteExecuteTime[2];		// time the vote is executed
	int			teamVoteYes[2];
	int			teamVoteNo[2];
	int			numteamVotingClients[2];// set by CalculateRanks

	// spawn variables
	qboolean	spawning;				// the G_Spawn*() functions are valid
	int			numSpawnVars;
	char		*spawnVars[MAX_SPAWN_VARS][2];	// key / value pairs
	int			numSpawnVarChars;
	char		spawnVarChars[MAX_SPAWN_VARS_CHARS];

	// intermission state
	int			intermissionQueued;		// intermission was qualified, but
										// wait INTERMISSION_DELAY_TIME before
										// actually going there so the last
										// frag can be watched.  Disable future
										// kills during this delay
	int			intermissiontime;		// time the intermission was started
	char		*changemap;
	qboolean	readyToExit;			// at least one client wants to exit
	int			exitTime;
	vec3_t		intermission_origin;	// also used for spectator spawns
	vec3_t		intermission_angle;

	int			bodyQueIndex;			// dead bodies
	gentity_t	*bodyQue[BODY_QUEUE_SIZE];
	int			portalSequence;

	alertEvent_t	alertEvents[ MAX_ALERT_EVENTS ];
	int				numAlertEvents;
	int				curAlertID;

	AIGroupInfo_t	groups[MAX_FRAME_GROUPS];

	//Interest points- squadmates automatically look at these if standing around and close to them
	interestPoint_t	interestPoints[MAX_INTEREST_POINTS];
	int			numInterestPoints;

	//Combat points- NPCs in bState BS_COMBAT_POINT will find their closest empty combat_point
	combatPoint_t	combatPoints[MAX_COMBAT_POINTS];
	int			numCombatPoints;

	//rwwRMG - added:
	int			mNumBSPInstances;
	int			mBSPInstanceDepth;
	vec3_t		mOriginAdjust;
	float		mRotationAdjust;
	char		*mTargetAdjust;

	char		mTeamFilter[MAX_QPATH];

	struct {
		fileHandle_t	log;
	} security;

	struct {
		int num;
		char *infos[MAX_BOTS];
	} bots;

	struct {
		int num;
		char *infos[MAX_ARENAS];
	} arenas;

	struct {
		int num;
		qboolean linked;
		locationData_t data[MAX_LOCATIONS];
	} locations;

	gametype_t	gametype;

	// zyk: Race Mode
	int race_mode; // zyk: sets 1 when someone joined the race and 2 when countdown starts and 3 when race starts. Default 0
	int race_map; // zyk: the map where this race is being done
	int race_start_timer; // zyk: timer to determine the time to start the race if race_mode is 1
	int race_countdown_timer; // zyk: shows the countdown on the players screens
	int race_countdown; // zyk: used to print each of the countdown messages
	int race_last_player_position; // zyk: after race starts, sets the position of the last player who crossed the finish line

	// zyk: Duel Tournament

	// zyk: Default 0. Possible values are:
	// 1 when someone joined
	// 2 to choose the duelists
	// 3 to announce the duelists
	// 4 when duel begins
	// 5 to show score
	// 6 to print match winner or match tie
	int duel_tournament_mode;

	qboolean duel_tournament_paused; // zyk: when an admin uses /duelpause, sets qtrue. If it is already paused, sets qfalse. Default qfalse
	int duelists_quantity; // zyk: number of players in the duel tournament. Default 0
	int duel_number_of_teams; // zyk: used to generate the match table based on the number of duel teams in DUel Tournament
	int duel_tournament_timer; // zyk: timer of duel tournament events. Default 0
	int duel_players[MAX_CLIENTS]; // zyk: has the score each player in the tournament. Default -1
	int duel_players_hp[MAX_CLIENTS]; // zyk: used as a untie criteria. If the tournament ends with players tied at score, the sum of remaining hp in all duels is used to untie
	int duel_tournament_model_id; // zyk: model id of the globe
	qboolean duel_arena_loaded; // zyk: tests if the arena is loaded on this map
	vec3_t duel_tournament_origin; // zyk: origin of the duel tournament arena, which has the globe around it. Used to validate position of players. If a duelist leaves the arena, he loses
	int duelist_1_id; // zyk: id of the first duelist
	int duelist_2_id; // zyk: id of the second duelist
	int duelist_1_ally_id; // zyk: ally of the first duelist
	int duelist_2_ally_id; // zyk: ally of the second duelist

	// zyk: the table with all the matches between the duelists, with their ids in first and second position
	// zyk: third is the number of rounds won by first duelist
	// zyk: fourth position is the number of rounds won by second duelist
	int duel_matches[MAX_DUEL_MATCHES][4];

	// zyk: number of rounds already played per match
	int duel_tournament_rounds;

	int duel_matches_quantity; // zyk: quantity of matches in this tournament
	int duel_matches_done; // zyk: how many matches were already done
	int duel_leaderboard_step; // zyk: used to calculate the leaderboard position of the current duel tournament winner
	int duel_leaderboard_timer; // zyk: timer used when calculating the leaderboard
	int duel_leaderboard_score; // zyk: number of tournaments won by the current winner
	char duel_leaderboard_acc[32]; // zyk: account of the current winner
	char duel_leaderboard_name[36]; // zyk: current name of the current winner
	char duel_leaderboard_ally_acc[32]; // zyk: account of the current winner ally
	char duel_leaderboard_ally_name[36]; // zyk: current name of the current winner ally
	qboolean duel_leaderboard_add_ally; // zyk: if qtrue, will also add the ally to the leaderboard
	int duel_leaderboard_index; // zyk: index of the line in the leaderboard file in which the current winner must be inserted (winners are sorted by the number of tournament wins in the file)
	qboolean duel_allies[MAX_CLIENTS]; // zyk: has the players who added another player as ally

	// zyk: Sniper Battle
	int sniper_mode; // zyk: Default 0. Sets 1 when someone joins, and 2 after battle begins
	int sniper_players[MAX_CLIENTS]; // zyk: default -1, when a player joins, sets 0. It is the amount of enemies defeated in Sniper Battle
	int sniper_mode_timer; // zyk: timer used in Sniper Battle
	int sniper_mode_quantity; // zyk: amount of players who joined the Sniper Battle

	// zyk: Melee Battle
	int melee_mode; // zyk: Default 0. Sets 1 when someone joins, and 2 after battle begins
	int melee_players[MAX_CLIENTS]; // zyk: default -1, when a player joins, sets 0. It is the amount of enemies defeated in Melee Battle
	int melee_mode_timer; // zyk: timer used in Melee Battle
	int melee_mode_quantity; // zyk: amount of players who joined the Melee Battle
	int melee_model_id; // zyk: model id of the catwalk
	qboolean melee_arena_loaded; // zyk: tests if the arena is loaded on this map
	vec3_t melee_mode_origin; // zyk: origin of the melee mode arena, which has the catwalk

	// zyk: RPG LMS
	int rpg_lms_mode;
	int rpg_lms_players[MAX_CLIENTS];
	int rpg_lms_timer;
	int rpg_lms_quantity;

	// zyk: the player id who is the target in Bounty Quest. Default 0
	int bounty_quest_target_id;

	// zyk: determines if a target can be chosen now. Default qtrue
	qboolean bounty_quest_choose_target;

	// zyk: sets the map in which the player must complete a quest objective
	int quest_map;

	// zyk: has the quest crystal ids that a player must get. Default -1 for each position.
	// crystal gets cleaned when the player gets the crystal
	int quest_crystal_id[3];

	// zyk: has the quest note id that a player must get. Default -1.
	// gets cleaned when the player gets the note
	int quest_note_id;

	// zyk: id of the note spawned in the fourth Universe Quest mission
	int universe_quest_note_id;

	// zyk: has the quest effect id of the fx_runner entity in the guardian area. Default -1.
	// gets cleaned when the player arrives at it
	int quest_effect_id;

	// zyk: has the order of each quest item that must be used to solve a quest puzzle
	int quest_puzzle_order[9];

	// zyk: id of the portal effect entity at last universe quest mission, so players can go through the teleport
	int chaos_portal_id;

	// zyk: Guardian Quest. Default 0. After the guardian is spawned, guardian_quest will have the guardian npc id
	int guardian_quest;

	// zyk: timer to start the Guardian Quest
	int guardian_quest_timer;

	// zyk: allows guardian of map to get his weapons back
	int initial_map_guardian_weapons;

	// zyk: default map music. After a boss battle, resets music to this one
	char default_map_music[128];

	// zyk: timer to reset boss song after battle ends
	int boss_battle_music_reset_timer;

	// zyk: each index has the effect id. The value is the owner of the effect used in Special Powers
	int special_power_effects[ENTITYNUM_MAX_NORMAL];

	// zyk: timer to remove each effect used in Special Powers
	int special_power_effects_timer[ENTITYNUM_MAX_NORMAL];

	// zyk: vehicle ids of the swoops used in Race Mode. Used to validate if player is using the correct vehicle
	int race_mode_vehicle[MAX_RACERS];

	// zyk: sets the players who already read the screen message
	qboolean read_screen_message[MAX_CLIENTS];

	// zyk: how much time it must show the message for the player
	int screen_message_timer[MAX_CLIENTS];

	// zyk: the player who called the last vote
	int voting_player;

	// zyk: fix for vjun3 map. It crashs clients in MP because of the npc skin limit (16). It will not load the protocol_imp npc
	qboolean is_vjun3_map;

	// zyk: tests if it is a sp map in loading time
	qboolean sp_map;

	// zyk: level.time when the server becomes empty (no players)
	int server_empty_change_map_timer;

	// zyk: used to cound how many clients are already connected
	int num_fully_connected_clients;

	// zyk: entities will be loaded after sometime so the server will reused the entities that were just removed
	int load_entities_timer;
	char load_entities_file[512];

	// zyk: has the player_ids that are ignored for each player
	int ignored_players[MAX_CLIENTS][2];

	// zyk: last entity spawned with /entadd. Used by /entundo command
	gentity_t *last_spawned_entity;

	// zyk: these variables test if an origin is set in the map to set the origin of a new entity spawned with /entadd command
	qboolean ent_origin_set;
	vec3_t ent_origin;
	vec3_t ent_angles;

	// zyk: used by Entity System to save and load spawnstring of entities
	char *zyk_spawn_strings[ENTITYNUM_MAX_NORMAL][128];

	// zyk: amount of keys and values stored in this entity
	int zyk_spawn_strings_values_count[ENTITYNUM_MAX_NORMAL];

	// zyk: Custom Quests, missions and fields
	char *zyk_custom_quest_missions[MAX_CUSTOM_QUESTS][MAX_CUSTOM_QUEST_MISSIONS][MAX_CUSTOM_QUEST_FIELDS];

	// zyk: amounf of keys and values stored for each mission of each quest
	int zyk_custom_quest_mission_values_count[MAX_CUSTOM_QUESTS][MAX_CUSTOM_QUEST_MISSIONS];

	// zyk: amount of missions of each custom quest
	int zyk_custom_quest_mission_count[MAX_CUSTOM_QUESTS];

	// zyk: saves the current mission number of the quest loaded for the current map
	int zyk_custom_quest_current_mission;

	// zyk: custom quest timer
	int zyk_custom_quest_timer;

	// zyk: custom quest counter, used for keys that can have multiple values (text1, text2, etc)
	int zyk_custom_quest_counter;

	// zyk: origin point of the quest mission
	vec3_t zyk_quest_mission_origin;

	// zyk: radius from the quest mission origin point the player must be within for the mission to start
	int zyk_quest_radius;

	// zyk: do not pass the mission until a certain event has happened (e.g killing all quest npcs)
	qboolean zyk_hold_quest_mission;

	// zyk: amount of custom quest npcs still alive to be defeated
	int zyk_quest_npc_count;

	// zyk: amount of ally npcs
	int zyk_quest_ally_npc_count;

	// zyk: amount of items to get in the mission
	int zyk_quest_item_count;

	// zyk: if qfalse, any place in the map starts the quest mission
	qboolean zyk_quest_test_origin;

	// zyk: set an effect on the quest origin point
	int zyk_custom_quest_effect_id;

	// zyk: custom quest main fields. It will saved in the first quest file line. Order of fields: name, active (value: on or off), count (integer value, number of completed missions)
	char* zyk_custom_quest_main_fields[MAX_CUSTOM_QUESTS][4];

	// zyk: used to test if the current map is a custom quest one. Sets the custom quest id who will first be played in this map
	int custom_quest_map;

	// zyk: current map name without the path from maps folder
	char zykmapname[128];

	char		mapname[MAX_QPATH];
	char		rawmapname[MAX_QPATH];
} level_locals_t;


// zyk: functions used in a lot of places
char *zyk_get_mission_value(int custom_quest, int mission, char *key);
void zyk_set_quest_field(int quest_number, int mission_number, char *key, char *value);
qboolean zyk_is_ally(gentity_t *ent, gentity_t *other);
int zyk_number_of_allies(gentity_t *ent, qboolean in_rpg_mode);
void send_rpg_events(int send_event_timer);
int zyk_get_remap_count();
void zyk_text_message(gentity_t *ent, char *filename, qboolean show_in_chat, qboolean broadcast_message, ...);
qboolean zyk_can_deflect_shots(gentity_t *ent);

// zyk: shader remap struct
typedef struct shaderRemap_s {
  char oldShader[MAX_QPATH];
  char newShader[MAX_QPATH];
  float timeOffset;
} shaderRemap_t;

#define MAX_SHADER_REMAPS 128

shaderRemap_t remappedShaders[MAX_SHADER_REMAPS];

//
// g_spawn.c
//
qboolean	G_SpawnString( const char *key, const char *defaultString, char **out );
// spawn string returns a temporary reference, you must CopyString() if you want to keep it
qboolean	G_SpawnFloat( const char *key, const char *defaultString, float *out );
qboolean	G_SpawnInt( const char *key, const char *defaultString, int *out );
qboolean	G_SpawnVector( const char *key, const char *defaultString, float *out );
qboolean	G_SpawnBoolean( const char *key, const char *defaultString, qboolean *out );
void		G_SpawnEntitiesFromString( qboolean inSubBSP );
char *G_NewString( const char *string );

//
// g_cmds.c
//
void Cmd_Score_f (gentity_t *ent);
void StopFollowing( gentity_t *ent );
void BroadcastTeamChange( gclient_t *client, int oldTeam );
void SetTeam( gentity_t *ent, char *s );
void Cmd_FollowCycle_f( gentity_t *ent, int dir );
void Cmd_SaberAttackCycle_f(gentity_t *ent);
int G_ItemUsable(playerState_t *ps, int forcedUse);
void Cmd_ToggleSaber_f(gentity_t *ent);
void Cmd_EngageDuel_f(gentity_t *ent);

//
// g_items.c
//
void ItemUse_Binoculars(gentity_t *ent);
void ItemUse_Shield(gentity_t *ent);
void ItemUse_Sentry(gentity_t *ent);

void Jetpack_Off(gentity_t *ent);
void Jetpack_On(gentity_t *ent);
void ItemUse_Jetpack(gentity_t *ent);
void ItemUse_UseCloak( gentity_t *ent );
void ItemUse_UseDisp(gentity_t *ent, int type);
void ItemUse_UseEWeb(gentity_t *ent);
void G_PrecacheDispensers(void);

void ItemUse_Seeker(gentity_t *ent);
void ItemUse_MedPack(gentity_t *ent);
void ItemUse_MedPack_Big(gentity_t *ent);

void G_CheckTeamItems( void );
void G_RunItem( gentity_t *ent );
void RespawnItem( gentity_t *ent );

gentity_t *Drop_Item( gentity_t *ent, gitem_t *item, float angle );
gentity_t *LaunchItem( gitem_t *item, vec3_t origin, vec3_t velocity );
void G_SpawnItem (gentity_t *ent, gitem_t *item);
void FinishSpawningItem( gentity_t *ent );
void	Add_Ammo (gentity_t *ent, int weapon, int count);
void Touch_Item (gentity_t *ent, gentity_t *other, trace_t *trace);

void ClearRegisteredItems( void );
void RegisterItem( gitem_t *item );
void SaveRegisteredItems( void );

//
// g_utils.c
//
int		G_ModelIndex( const char *name );
int		G_SoundIndex( const char *name );
int		G_SoundSetIndex(const char *name);
int		G_EffectIndex( const char *name );
int		G_BSPIndex( const char *name );
int		G_IconIndex( const char* name );

qboolean	G_PlayerHasCustomSkeleton(gentity_t *ent);

void	G_TeamCommand( team_t team, char *cmd );
void	G_ScaleNetHealth(gentity_t *self);
void	G_KillBox (gentity_t *ent);
gentity_t *G_Find (gentity_t *from, int fieldofs, const char *match);
int		G_RadiusList ( vec3_t origin, float radius,	gentity_t *ignore, qboolean takeDamage, gentity_t *ent_list[MAX_GENTITIES]);

void	G_Throw( gentity_t *targ, vec3_t newDir, float push );

void	G_FreeFakeClient(gclient_t **cl);
void	G_CreateFakeClient(int entNum, gclient_t **cl);
void	G_CleanAllFakeClients(void);

void	G_SetAnim(gentity_t *ent, usercmd_t *ucmd, int setAnimParts, int anim, int setAnimFlags, int blendTime);
gentity_t *G_PickTarget (char *targetname);
void	GlobalUse(gentity_t *self, gentity_t *other, gentity_t *activator);
void	G_UseTargets2( gentity_t *ent, gentity_t *activator, const char *string );
void	G_UseTargets (gentity_t *ent, gentity_t *activator);
void	G_SetMovedir ( vec3_t angles, vec3_t movedir);
void	G_SetAngles( gentity_t *ent, vec3_t angles );

void	G_InitGentity( gentity_t *e );
gentity_t	*G_Spawn (void);
gentity_t *G_TempEntity( vec3_t origin, int event );
gentity_t	*G_PlayEffect(int fxID, vec3_t org, vec3_t ang);
gentity_t	*G_PlayEffectID(const int fxID, vec3_t org, vec3_t ang);
gentity_t *G_ScreenShake(vec3_t org, gentity_t *target, float intensity, int duration, qboolean global);
void	G_MuteSound( int entnum, int channel );
void	G_Sound( gentity_t *ent, int channel, int soundIndex );
void	G_SoundAtLoc( vec3_t loc, int channel, int soundIndex );
void	G_EntitySound( gentity_t *ent, int channel, int soundIndex );
void	TryUse( gentity_t *ent );
void	G_SendG2KillQueue(void);
void	G_KillG2Queue(int entNum);
void	G_FreeEntity( gentity_t *e );
qboolean	G_EntitiesFree( void );

qboolean G_ActivateBehavior (gentity_t *self, int bset );

void	G_TouchTriggers (gentity_t *ent);
void	G_TouchSolids (gentity_t *ent);
void	GetAnglesForDirection( const vec3_t p1, const vec3_t p2, vec3_t out );

//
// g_object.c
//

extern void G_RunObject			( gentity_t *ent );


float	*tv (float x, float y, float z);
char	*vtos( const vec3_t v );

void G_AddPredictableEvent( gentity_t *ent, int event, int eventParm );
void G_AddEvent( gentity_t *ent, int event, int eventParm );
void G_SetOrigin( gentity_t *ent, vec3_t origin );
qboolean G_CheckInSolid (gentity_t *self, qboolean fix);
void AddRemap(const char *oldShader, const char *newShader, float timeOffset);
const char *BuildShaderStateConfig(void);
/*
Ghoul2 Insert Start
*/
int G_BoneIndex( const char *name );

/*
Ghoul2 Insert End
*/

//
// g_combat.c
//
qboolean CanDamage (gentity_t *targ, vec3_t origin);
void G_Damage (gentity_t *targ, gentity_t *inflictor, gentity_t *attacker, vec3_t dir, vec3_t point, int damage, int dflags, int mod);
qboolean G_RadiusDamage (vec3_t origin, gentity_t *attacker, float damage, float radius, gentity_t *ignore, gentity_t *missile, int mod);
void body_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath );
void TossClientWeapon(gentity_t *self, vec3_t direction, float speed);
void TossClientItems( gentity_t *self );
void TossClientCubes( gentity_t *self );
void ExplodeDeath( gentity_t *self );
void G_CheckForDismemberment(gentity_t *ent, gentity_t *enemy, vec3_t point, int damage, int deathAnim, qboolean postDeath);
extern int gGAvoidDismember;


// damage flags
#define DAMAGE_NORMAL				0x00000000	// No flags set.
#define DAMAGE_RADIUS				0x00000001	// damage was indirect
#define DAMAGE_NO_ARMOR				0x00000002	// armour does not protect from this damage
#define DAMAGE_NO_KNOCKBACK			0x00000004	// do not affect velocity, just view angles
#define DAMAGE_NO_PROTECTION		0x00000008  // armor, shields, invulnerability, and godmode have no effect
#define DAMAGE_NO_TEAM_PROTECTION	0x00000010  // armor, shields, invulnerability, and godmode have no effect
//JK2 flags
#define DAMAGE_EXTRA_KNOCKBACK		0x00000040	// add extra knockback to this damage
#define DAMAGE_DEATH_KNOCKBACK		0x00000080	// only does knockback on death of target
#define DAMAGE_IGNORE_TEAM			0x00000100	// damage is always done, regardless of teams
#define DAMAGE_NO_DAMAGE			0x00000200	// do no actual damage but react as if damage was taken
#define DAMAGE_HALF_ABSORB			0x00000400	// half shields, half health
#define DAMAGE_HALF_ARMOR_REDUCTION	0x00000800	// This damage doesn't whittle down armor as efficiently.
#define DAMAGE_HEAVY_WEAP_CLASS		0x00001000	// Heavy damage
#define DAMAGE_NO_HIT_LOC			0x00002000	// No hit location
#define DAMAGE_NO_SELF_PROTECTION	0x00004000	// Dont apply half damage to self attacks
#define DAMAGE_NO_DISMEMBER			0x00008000	// Dont do dismemberment
#define DAMAGE_SABER_KNOCKBACK1		0x00010000	// Check the attacker's first saber for a knockbackScale
#define DAMAGE_SABER_KNOCKBACK2		0x00020000	// Check the attacker's second saber for a knockbackScale
#define DAMAGE_SABER_KNOCKBACK1_B2	0x00040000	// Check the attacker's first saber for a knockbackScale2
#define DAMAGE_SABER_KNOCKBACK2_B2	0x00080000	// Check the attacker's second saber for a knockbackScale2
//
// g_exphysics.c
//
void G_RunExPhys(gentity_t *ent, float gravity, float mass, float bounce, qboolean autoKill, int *g2Bolts, int numG2Bolts);

//
// g_missile.c
//
void G_ReflectMissile( gentity_t *ent, gentity_t *missile, vec3_t forward );

void G_RunMissile( gentity_t *ent );

gentity_t *CreateMissile( vec3_t org, vec3_t dir, float vel, int life,
							gentity_t *owner, qboolean altFire);
void G_BounceProjectile( vec3_t start, vec3_t impact, vec3_t dir, vec3_t endout );
void G_ExplodeMissile( gentity_t *ent );

void WP_FireBlasterMissile( gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire );


//
// g_mover.c
//
extern int	BMS_START;
extern int	BMS_MID;
extern int	BMS_END;

#define SPF_BUTTON_USABLE		1
#define SPF_BUTTON_FPUSHABLE	2
void G_PlayDoorLoopSound( gentity_t *ent );
void G_PlayDoorSound( gentity_t *ent, int type );
void G_RunMover( gentity_t *ent );
void Touch_DoorTrigger( gentity_t *ent, gentity_t *other, trace_t *trace );

//
// g_trigger.c
//
void trigger_teleporter_touch (gentity_t *self, gentity_t *other, trace_t *trace );


//
// g_misc.c
//
#define MAX_REFNAME	32
#define	START_TIME_LINK_ENTS		FRAMETIME*1

#define	RTF_NONE	0
#define	RTF_NAVGOAL	0x00000001

typedef struct reference_tag_s
{
	char		name[MAX_REFNAME];
	vec3_t		origin;
	vec3_t		angles;
	int			flags;	//Just in case
	int			radius;	//For nav goals
	qboolean	inuse;
} reference_tag_t;

void TAG_Init( void );
reference_tag_t	*TAG_Find( const char *owner, const char *name );
reference_tag_t	*TAG_Add( const char *name, const char *owner, vec3_t origin, vec3_t angles, int radius, int flags );
int	TAG_GetOrigin( const char *owner, const char *name, vec3_t origin );
int	TAG_GetOrigin2( const char *owner, const char *name, vec3_t origin );
int	TAG_GetAngles( const char *owner, const char *name, vec3_t angles );
int TAG_GetRadius( const char *owner, const char *name );
int TAG_GetFlags( const char *owner, const char *name );

void TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles );

//
// g_weapon.c
//
void WP_FireTurretMissile( gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire, int damage, int velocity, int mod, gentity_t *ignore );
void WP_FireGenericBlasterMissile( gentity_t *ent, vec3_t start, vec3_t dir, qboolean altFire, int damage, int velocity, int mod );
qboolean LogAccuracyHit( gentity_t *target, gentity_t *attacker );
void CalcMuzzlePoint ( gentity_t *ent, const vec3_t inForward, const vec3_t inRight, const vec3_t inUp, vec3_t muzzlePoint );
void SnapVectorTowards( vec3_t v, vec3_t to );
qboolean CheckGauntletAttack( gentity_t *ent );


//
// g_client.c
//
int TeamCount( int ignoreClientNum, team_t team );
int TeamLeader( int team );
team_t PickTeam( int ignoreClientNum );
void SetClientViewAngle( gentity_t *ent, vec3_t angle );
gentity_t *SelectSpawnPoint ( vec3_t avoidPoint, vec3_t origin, vec3_t angles, team_t team, qboolean isbot );
void MaintainBodyQueue(gentity_t *ent);
void ClientRespawn (gentity_t *ent);
void BeginIntermission (void);
void InitBodyQue (void);
void ClientSpawn( gentity_t *ent );
void player_die (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod);
void AddScore( gentity_t *ent, vec3_t origin, int score );
void CalculateRanks( void );
qboolean SpotWouldTelefrag( gentity_t *spot );

extern gentity_t *gJMSaberEnt;

//
// g_svcmds.c
//
qboolean	ConsoleCommand( void );
void G_ProcessIPBans(void);
qboolean G_FilterPacket (char *from);

//
// g_weapon.c
//
void FireWeapon( gentity_t *ent, qboolean altFire );
void BlowDetpacks(gentity_t *ent);
void RemoveDetpacks(gentity_t *ent);

//
// p_hud.c
//
void MoveClientToIntermission (gentity_t *client);
void G_SetStats (gentity_t *ent);
void DeathmatchScoreboardMessage (gentity_t *client);

//
// g_cmds.c
//

//
// g_pweapon.c
//


//
// g_main.c
//
extern vmCvar_t g_ff_objectives;
extern qboolean gDoSlowMoDuel;
extern int gSlowMoDuelTime;

void G_PowerDuelCount(int *loners, int *doubles, qboolean countSpec);

void FindIntermissionPoint( void );
void SetLeader(int team, int client);
void CheckTeamLeader( int team );
void G_RunThink (gentity_t *ent);
void AddTournamentQueue(gclient_t *client);
void QDECL G_LogPrintf( const char *fmt, ... );
void QDECL G_SecurityLogPrintf( const char *fmt, ... );
void SendScoreboardMessageToAllClients( void );
const char *G_GetStringEdString(char *refSection, char *refName);

//
// g_client.c
//
char *ClientConnect( int clientNum, qboolean firstTime, qboolean isBot );
qboolean ClientUserinfoChanged( int clientNum );
void ClientDisconnect( int clientNum );
void ClientBegin( int clientNum, qboolean allowTeamReset );
void G_BreakArm(gentity_t *ent, int arm);
void G_UpdateClientAnims(gentity_t *self, float animSpeedScale);
void ClientCommand( int clientNum );
void G_ClearVote( gentity_t *ent );
void G_ClearTeamVote( gentity_t *ent, int team );

//
// g_active.c
//
void G_CheckClientTimeouts	( gentity_t *ent );
void ClientThink			( int clientNum, usercmd_t *ucmd );
void ClientEndFrame			( gentity_t *ent );
void G_RunClient			( gentity_t *ent );

//
// g_team.c
//
qboolean OnSameTeam( gentity_t *ent1, gentity_t *ent2 );
void Team_CheckDroppedItem( gentity_t *dropped );

//
// g_mem.c
//
void *G_Alloc( int size );
void G_InitMemory( void );
void Svcmd_GameMem_f( void );

//
// g_session.c
//
void G_ReadSessionData( gclient_t *client );
void G_InitSessionData( gclient_t *client, char *userinfo, qboolean isBot );

void G_InitWorldSession( void );
void G_WriteSessionData( void );

//
// NPC_senses.cpp
//
extern void AddSightEvent( gentity_t *owner, vec3_t position, float radius, alertEventLevel_e alertLevel, float addLight ); //addLight = 0.0f
extern void AddSoundEvent( gentity_t *owner, vec3_t position, float radius, alertEventLevel_e alertLevel, qboolean needLOS ); //needLOS = qfalse
extern qboolean G_CheckForDanger( gentity_t *self, int alertEvent );
extern int G_CheckAlertEvents( gentity_t *self, qboolean checkSight, qboolean checkSound, float maxSeeDist, float maxHearDist, int ignoreAlert, qboolean mustHaveOwner, int minAlertLevel ); //ignoreAlert = -1, mustHaveOwner = qfalse, minAlertLevel = AEL_MINOR
extern qboolean G_CheckForDanger( gentity_t *self, int alertEvent );
extern qboolean G_ClearLOS( gentity_t *self, const vec3_t start, const vec3_t end );
extern qboolean G_ClearLOS2( gentity_t *self, gentity_t *ent, const vec3_t end );
extern qboolean G_ClearLOS3( gentity_t *self, const vec3_t start, gentity_t *ent );
extern qboolean G_ClearLOS4( gentity_t *self, gentity_t *ent );
extern qboolean G_ClearLOS5( gentity_t *self, const vec3_t end );

//
// g_bot.c
//
void G_InitBots( void );
char *G_GetBotInfoByNumber( int num );
char *G_GetBotInfoByName( const char *name );
void G_CheckBotSpawn( void );
void G_RemoveQueuedBotBegin( int clientNum );
qboolean G_BotConnect( int clientNum, qboolean restart );
void Svcmd_AddBot_f( void );
void Svcmd_BotList_f( void );
void BotInterbreedEndMatch( void );
qboolean G_DoesMapSupportGametype(const char *mapname, int gametype);
const char *G_RefreshNextMap(int gametype, qboolean forced);
void G_LoadArenas( void );

// w_force.c / w_saber.c
gentity_t *G_PreDefSound(vec3_t org, int pdSound);
qboolean HasSetSaberOnly(void);
void WP_ForcePowerStop( gentity_t *self, forcePowers_t forcePower );
void WP_SaberPositionUpdate( gentity_t *self, usercmd_t *ucmd );
int WP_SaberCanBlock(gentity_t *self, vec3_t point, int dflags, int mod, qboolean projectile, int attackStr);
void WP_SaberInitBladeData( gentity_t *ent );
void WP_InitForcePowers( gentity_t *ent );
void WP_SpawnInitForcePowers( gentity_t *ent );
void WP_ForcePowersUpdate( gentity_t *self, usercmd_t *ucmd );
int ForcePowerUsableOn(gentity_t *attacker, gentity_t *other, forcePowers_t forcePower);
void ForceHeal( gentity_t *self );
void ForceSpeed( gentity_t *self, int forceDuration );
void ForceRage( gentity_t *self );
void ForceGrip( gentity_t *self );
void ForceProtect( gentity_t *self );
void ForceAbsorb( gentity_t *self );
void ForceTeamHeal( gentity_t *self );
void ForceTeamForceReplenish( gentity_t *self );
void ForceSeeing( gentity_t *self );
void ForceThrow( gentity_t *self, qboolean pull );
void ForceTelepathy(gentity_t *self);
qboolean Jedi_DodgeEvasion( gentity_t *self, gentity_t *shooter, trace_t *tr, int hitLoc );

// g_log.c
void QDECL G_LogWeaponPickup(int client, int weaponid);
void QDECL G_LogWeaponFire(int client, int weaponid);
void QDECL G_LogWeaponDamage(int client, int mod, int amount);
void QDECL G_LogWeaponKill(int client, int mod);
void QDECL G_LogWeaponDeath(int client, int weaponid);
void QDECL G_LogWeaponFrag(int attacker, int deadguy);
void QDECL G_LogWeaponPowerup(int client, int powerupid);
void QDECL G_LogWeaponItem(int client, int itemid);
void QDECL G_LogWeaponInit(void);
void QDECL G_LogWeaponOutput(void);
void QDECL G_LogExit( const char *string );
void QDECL G_ClearClientLog(int client);

// g_siege.c
void InitSiegeMode(void);
void G_SiegeClientExData(gentity_t *msgTarg);

// g_timer
//Timing information
void		TIMER_Clear( void );
void		TIMER_Clear2( gentity_t *ent );
void		TIMER_Set( gentity_t *ent, const char *identifier, int duration );
int			TIMER_Get( gentity_t *ent, const char *identifier );
qboolean	TIMER_Done( gentity_t *ent, const char *identifier );
qboolean	TIMER_Start( gentity_t *self, const char *identifier, int duration );
qboolean	TIMER_Done2( gentity_t *ent, const char *identifier, qboolean remove );
qboolean	TIMER_Exists( gentity_t *ent, const char *identifier );
void		TIMER_Remove( gentity_t *ent, const char *identifier );

float NPC_GetHFOVPercentage( vec3_t spot, vec3_t from, vec3_t facing, float hFOV );
float NPC_GetVFOVPercentage( vec3_t spot, vec3_t from, vec3_t facing, float vFOV );


extern void G_SetEnemy (gentity_t *self, gentity_t *enemy);
qboolean InFront( vec3_t spot, vec3_t from, vec3_t fromAngles, float threshHold );

// ai_main.c
#define MAX_FILEPATH			144

int		OrgVisible		( vec3_t org1, vec3_t org2, int ignore);
void	BotOrder		( gentity_t *ent, int clientnum, int ordernum);
int		InFieldOfVision	( vec3_t viewangles, float fov, vec3_t angles);

// ai_util.c
void B_InitAlloc(void);
void B_CleanupAlloc(void);

//bot settings
typedef struct bot_settings_s
{
	char personalityfile[MAX_FILEPATH];
	float skill;
	char team[MAX_FILEPATH];
} bot_settings_t;

int BotAISetup( int restart );
int BotAIShutdown( int restart );
int BotAILoadMap( int restart );
int BotAISetupClient(int client, struct bot_settings_s *settings, qboolean restart);
int BotAIShutdownClient( int client, qboolean restart );
int BotAIStartFrame( int time );

#include "g_team.h" // teamplay specific stuff


extern	level_locals_t	level;
extern	gentity_t		g_entities[MAX_GENTITIES];

#define	FOFS(x) offsetof(gentity_t, x)

// userinfo validation bitflags
// default is all except extended ascii
// numUserinfoFields + USERINFO_VALIDATION_MAX should not exceed 31
typedef enum userinfoValidationBits_e {
	// validation & (1<<(numUserinfoFields+USERINFO_VALIDATION_BLAH))
	USERINFO_VALIDATION_SIZE=0,
	USERINFO_VALIDATION_SLASH,
	USERINFO_VALIDATION_EXTASCII,
	USERINFO_VALIDATION_CONTROLCHARS,
	USERINFO_VALIDATION_MAX
} userinfoValidationBits_t;

void Svcmd_ToggleUserinfoValidation_f( void );
void Svcmd_ToggleAllowVote_f( void );

// g_cvar.c
#define XCVAR_PROTO
	#include "g_xcvar.h"
#undef XCVAR_PROTO
void G_RegisterCvars( void );
void G_UpdateCvars( void );

extern gameImport_t *trap;
