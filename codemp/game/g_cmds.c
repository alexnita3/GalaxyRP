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
#include "bg_saga.h"

#include "ui/menudef.h"			// for the voice chats

#include "sqlite/sqlite3.h"

// GalaxyRP fix: [portability] for zyk_create_dir() below, which creates directories itself
// rather than shelling out to mkdir(1).
#if defined(_WIN32)
	#include <direct.h>
#else
	#include <sys/types.h>
	#include <sys/stat.h>
#endif

#define MAX_EMOTE_WORDS 11;
#define MAX_CHAT_MODIFIERS 24;

// GalaxyRP fix: [News] Maximum length of a single news channel name.
#define MAX_NEWS_CHANNEL_LENGTH 32

// GalaxyRP fix: [Attributes] how long a character description set with /attributes may be, and what
// an empty /attributes "" puts back.
#define MAX_DESCRIPTION_LENGTH 255
#define RP_DEFAULT_DESCRIPTION "Nothing to show."

// GalaxyRP fix: [Chat] how many characters of payload a single SendServerCommand may carry.
#define RP_LIST_FLUSH_AT 900

//rww - for getting bot commands...
int AcceptBotCommand(char *cmd, gentity_t *pl);
//end rww

void WP_SetSaber( int entNum, saberInfo_t *sabers, int saberNum, const char *saberName );

void Cmd_NPC_f( gentity_t *ent );
void SetTeamQuick(gentity_t *ent, int team, qboolean doBegin);

extern int check_xp(int currentLevel);
extern void Cmd_GalaxyRpUi_f(gentity_t* ent);
extern void Cmd_ZykChars_f(gentity_t* ent);
extern void save_account(gentity_t *ent, qboolean save_char_file);
extern void initialize_rpg_skills(gentity_t *ent);

// GalaxyRP (Alex): [Skills] This is used to display everything about a skill in various places throughout the mod.
const skill_t skills[] = {
	{5, "Jump",					"makes you use the force to jump higher. Level 5 has no height limit, you can continue jumping up until you run out of force, and it also lets you jump out of water",																																			"force",	"neutral",	FP_LEVITATION},
	{5, "Push",					"pushes the opponent forward",																																																																					"force",	"neutral",	FP_PUSH},
	{5, "Pull",					"pulls the opponents towards you. Also can pull a gun out of their hands",																																																																				"force",	"neutral",	FP_PULL},
	{5, "Speed",				"increases the speed of your movement",																																														"force",	"neutral",	FP_SPEED},
	{3, "Sense",				"allows you to see people through obstacles and see cloaked or invisible people. You can also dodge Disruptor shots and resist Mind Trick",																	"force",	"neutral",	FP_SEE},
	{5, "Saber Attack",			"gives you Lightsaber weapon. Each level gives more saber forms",																															"force",	"neutral",	FP_SABER_OFFENSE},
	{5, "Saber Defense",		"increases your ability to block, parry enemy saber attacks or enemy shots",																																																									"force",	"neutral",	FP_SABER_DEFENSE},
	{5, "Saber Throw",			"throws your saber at enemy and gets it back. Each level increases max distance and saber throw speed.",																																																		"force",	"neutral",	FP_SABERTHROW},
	{5, "Absorb",				"allows you to absorb force power attacks done to you",																																																															"force",	"light",	FP_ABSORB},
	{5, "Heal",					"restores your health. Level 1 restores 10 hp, level 2 - 20 hp, level 3 - 30 hp, level 4 - 40 hp and level 5 - 50 hp",																																																				"force",	"light",	FP_HEAL},
	{5, "Protect",				"decreases damage done to you at the cost of your Force power",																																														"force",	"light",	FP_PROTECT},
	{5, "Mind Trick",			"makes yourself invisible to the players you aim at. Higher levels affect more players around you",																								"force",	"neutral",	FP_TELEPATHY},
	{5, "Team Heal",			"restores health to your allies and friendly npcs near you. The range and HP restored grow with each level",																																																																		"force",	"light",	FP_TEAM_HEAL},
	{5, "Lightning",			"attacks with a powerful electric attack at players near you. At level 4, does more damage and pushes the enemy back",																																															"force",	"dark",		FP_LIGHTNING},
	{5, "Grip",					"attacks a player by holding and damaging him",																																																																	"force",	"dark",		FP_GRIP},
	{5, "Drain",				"drains force power from a player to restore your health",																																																														"force",	"dark",		FP_DRAIN},
	{5, "Rage",					"makes you faster, reduces damage taken and increases your saber attack speed and damage, all at the cost of your HP",															"force",	"dark",		FP_RAGE},
	{5, "Team Energize",		"restores Force power to your allies and friendly npcs near you. The range and Force restored grow with each level",																																																																	"force",	"dark",		FP_TEAM_FORCE},
	{5, "Stun Baton",			"attacks someone with a small electric charge. Levels 2 to 4 deal more damage. Level 5 deals as much as level 4, and the baton opens any door it hits, including locked ones, and moves elevators and other movers. With the Stun Baton Upgrade from the shop, each hit also slows enemies down for 1.5 seconds",																	"weapons",	"merc",		WP_STUN_BATON},
	{2, "Blaster Pistol",		"the popular Star Wars pistol used by Han Solo in the movies. Alt fire is a charged shot: the longer the charge, the more damage. Level 2 unlocks the alternate fire mode.",																	"weapons",	"merc",		WP_BRYAR_PISTOL},
	{2, "E11 Blaster Rifle",	"the rifle used by the Storm Troopers. Normal fire is a single shot, alt fire is rapid fire. Level 2 unlocks the alternate fire mode.",																																										"weapons",	"merc",		WP_BLASTER},
	{2, "Disruptor",			"the sniper, used by the rodians ingame. Normal fire is a single shot. Alt fire allows zoom and a charged shot. Level 2 unlocks the alternate fire mode.",																																					"weapons",	"merc",		WP_DISRUPTOR},
	{2, "Bowcaster",			"the famous weapon used by Chewbacca. Normal fire can be charged to fire several shots at once. Level 2 unlocks the alternate fire mode.",																																																				"weapons",	"merc",		WP_BOWCASTER},
	{2, "Repeater",				"a powerful weapon with a rapid fire and a plasma bomb. Normal fire is the rapid fire, alt fire launches the plasma bomb. Level 2 unlocks the alternate fire mode.",																																									"weapons",	"merc",		WP_REPEATER},
	{2, "DEMP2",				"very effective against machine npcs and some vehicles, stunning them for longer than other targets. Alt fire can be charged. Level 2 unlocks the alternate fire mode.",																																			"weapons",	"merc",		WP_DEMP2},
	{2, "Flechette",			"works like a shotgun. Alt fire shoots 2 bombs. Level 2 unlocks the alternate fire mode.",																																																					"weapons",	"merc",		WP_FLECHETTE},
	{2, "Rocket Launcher",		"a powerful explosive weapon. Normal fire shoots a rocket, alt fire a homing missile. Level 2 unlocks the alternate fire mode.",																																																	"weapons",	"merc",		WP_ROCKET_LAUNCHER},
	{2, "Concussion Rifle",		"shoots a powerful blast with a big damage area. Alt fire is a ray like the disruptor's, but it goes through force fields and knocks the enemy down. Level 2 unlocks the alternate fire mode.",																															"weapons",	"merc",		WP_CONCUSSION},
	{2, "Bryar Pistol",			"like the blaster pistol but fires twice as fast. Level 2 unlocks the alternate fire mode.",																																																					"weapons",	"merc",		WP_BRYAR_OLD},
	{3, "Melee",				"increases the damage of your punches, kicks and grabs. Each level deals more.",																																	"weapons",	"merc",		0},
	{5, "Max Shield",			"The max shield (armor) the player can have. Each level increases 20 per cent of max shield the player can have",																																																"protect",	"merc",		0},
	{4, "Shield Strength",		"Each level increases your shield resistance by 7 per cent",																																																													"protect",	"merc",		0},
	{4, "Health Strength",		"Each level increases your health resistance by 7 per cent",																																																													"protect",	"merc",		0},
	{1, "Drain Shield",			"When using Drain force power, and your health is full, restores some shield. It also makes Drain suck hp/shield from the enemy to restore your hp/shield",																																						"force",	"merc",		0},
	{3, "Jetpack",				"the jetpack, used by Boba Fett. Allows you to fly. To use it, jump and press the Use key (usually R) while in the middle of the jump. Each level uses less fuel, allowing you to fly for a longer time",																										"items",	"merc",		HI_JETPACK},
	{3, "Sense Health",			"allows you to see information about player, NPC or vehicle you aim at or who is near you. Requires Sense Force power",		"force",	"light",	0},
	{3, "Shield Heal",			"restores some of your shield. To use it, use Heal force power when you have full HP. Requires the Heal skill, and costs the same Force as a Heal",																																											"force",	"merc",		0},
	{3, "Team Shield Heal",		"restores shield to your allies near you who already have full HP. To use it, use Team Heal force power. Requires the Team Heal skill, and uses its range and Force cost",																									"force",	"merc",		0},
	{1, "Unique Skill",			"placeholder, does nothing",																																																																					"protect",	"merc",		0},
	{3, "Blaster Pack",			"used as ammo for Blaster Pistol, Bryar Pistol and E11 Blaster Rifle.",																																																											"ammo",		"merc",		0},
	{3, "Powercell",			"used as ammo for Disruptor, Bowcaster and DEMP2.",																																																																"ammo",		"merc",		0},
	{3, "Metal Bolts",			"used as ammo for Repeater, Flechette and Concussion Rifle.",																																																													"ammo",		"merc",		0},
	{3, "Rockets",				"used as ammo for Rocket Launcher.",																																																																			"ammo",		"merc",		0},
	{3, "Thermals",				"the famous detonator used by Leia in Ep 6 at the Jabba Palace. Normal fire throws it, which explodes after some seconds. Alt fire throws it and it explodes as soon as it touches something.",																													"ammo",		"merc",		WP_THERMAL},
	{3, "Trip Mines",			"a mine that can be planted somewhere. Normal fire plants a mine with a laser that when touched makes the mine explode. Alt fire plants proximity mines",																																						"ammo",		"merc",		WP_TRIP_MINE},
	{3, "Detpacks",				"a very powerful explosive, which you can detonate remotely with the alt fire button.",																																																							"ammo",		"merc",		WP_DET_PACK},
	{1, "Binoculars",			"this item allows you to see distant things better with its zoom.",																																																												"items",	"merc",		HI_BINOCULARS},
	{1, "Bacta Canister",		"allows you to recover 25 HP",																																																																					"items",	"merc",		HI_MEDPAC},
	{1, "Sentry Gun",			"after placed on the ground, shoots at any nearby enemy",																																																														"items",	"merc",		HI_SENTRY_GUN},
	{1, "Seeker Drone",			"a flying ball that flies around you, shooting anyone in its range",																																																											"items",	"merc",		HI_SEEKER},
	{1, "E-Web",				"allows you to shoot at people with it, it has a good fire rate",																																																												"items",	"merc",		HI_EWEB},
	{1, "Big Bacta",			"allows you to recover 50 HP",																																																																					"items",	"merc",		HI_MEDPAC_BIG},
	{1, "Force Field",			"a powerful shield that protects you from enemy attacks, it can resist a lot against any weapon",																																																				"items",	"merc",		HI_SHIELD},
	{2, "Cloak Item",			"makes you almost invisible to players and invisible to npcs. Level 2 also lets you cloak the vehicle you ride, together with yourself (/vehicle_cloak).",																																																													"items",	"merc",		HI_CLOAK},
	{5, "Force Power",			"increases the max force power you have. Necessary to allow you to use force powers and force-based skills",																																																	"force",	"neutral",	0},
	{1, "Grapple Hook",			"a grappling hook. Bind a key to +grapple (or +button12), aim, hold it to fire and be pulled to wherever the hook lands -- walls, ceilings, even other players -- and let go to release. Crouch to pause the pull without letting go.",	"items",	"merc",		0},
	{5, "Armor",				"Each level increases your damage resistance by 10 percent, but also decreases your movement speed by 10 percent.",																																																"protect",	"merc",		0},
	{2, "Flame Thrower",		"Allows you to use a flamethrower. Used by alt-firing with a stun baton.",																																																										"items",	"merc",		0},
	{5, "Shield Regeneration",	"Increases the rate at which shield is regenerated by one per second for each point. Meditating will make it faster.",																																															"protect",	"merc",		0},
	{5, "Health Regeneration",	"Increases the rate at which health is regenerated by one per second for each point. Meditating will make it faster.",																																															"protect",	"merc",		0},
};

#define MAX_WORDED_EMOTES 132
//alex: type for storing worde animations wo use with the emote system
typedef struct worded_animation_s {
	const char* animation_name;
	int			animation_code;
	const char* animation_category;
} worded_animation_t;

/*
alex: list of all worded emotes, ids, and categories, these are to be stored alphabetically
the animation code is to be exactly the same as the correcponding id in anims.h
*/
const worded_animation_t animations[MAX_WORDED_EMOTES] = {
	{"aim",				BOTH_STAND4TOATTACK2,		"Blaster"	},
	{"aim2",			BOTH_STAND5TOAIM,			"Blaster"	},
	{"aim3",			BOTH_ATTACK2,				"Blaster"	},
	{"aim4",			TORSO_WEAPONIDLE4,			"Blaster"	},
	{"aim5",			TORSO_WEAPONIDLE3,			"Blaster"	},
	{"anikata",			BOTH_ANAKINKATA,			"Saber"		},
	{"anikata2",		BOTH_ANAKINKATA2,			"Saber"		},
	{"anikata3",		BOTH_ANAKINKATA3,			"Saber"		},
	{"ataru",			BOTH_ATARU,					"Saber"		},
	{"beg",				BOTH_KNEES1,				"Body"		},
	{"beg2",			BOTH_DEATH14_SITUP,			"Body"		},
	{"beg3",			BOTH_CHOKE2,				"Body"		},
	{"bow",				BOTH_BOW,					"Body"		},
	{"bump",			BOTH_FISTBUMP,				"Body"		},
	{"carry",			BOTH_CARRY,					"Movement"	},
	{"choked",			BOTH_CHOKE3,				"Body"		},
	{"commlinkdown",	BOTH_TALKCOMM1STOP,			"Body"		},
	{"commlinkup",		BOTH_TALKCOMM1START,		"Body"		},
	{"cover",			BOTH_DODGE_HOLD_FL,			"Movement"	},
	{"cross",			BOTH_ARMSCROSSED,			"Body"		},
	{"cuffed",			BOTH_STAND4,				"Body"		},
	{"cufffront",		BOTH_CUFFEDFRONT,			"Body"		},
	{"cuffknees",		BOTH_CUFFEDKNEES,			"Body"		},
	{"cup",				BOTH_COFFEE_IDLE,			"Body"		},
	{"cupsip",			BOTH_COFFEE_SIP,			"Body"		},
	{"datapad",			BOTH_DATAPAD,				"Body"		},
	{"datapad2",		BOTH_DATAPAD2,				"Body"		},
	{"die",				BOTH_DEATH14_UNGRIP,		"Body"		},
	{"djemso",			BOTH_DJEMSO,				"Saber"		},
	{"djemso2",			BOTH_DJEMSO2,				"Saber"		},
	{"drainloop",		BOTH_FORCE_DRAIN_GRAB_HOLD,	"Force"		},
	{"drink",			BOTH_COFFEE_SIP,			"Body"		},
	{"facepalm",		BOTH_FACEPALM,				"Body"		},
	{"facepalm2",		BOTH_FACEPALM2,				"Body"		},
	{"fear",			BOTH_SONICPAIN_HOLD,		"Body"		},
	{"flourish",		BOTH_SHOWOFF_FAST,			"Saber"		},
	{"flourish2",		BOTH_SHOWOFF_MEDIUM,		"Saber"		},
	{"flourish3",		BOTH_SHOWOFF_STRONG,		"Saber"		},
	{"flourish4",		BOTH_SHOWOFF_DUAL,			"Saber"		},
	{"flourish5",		BOTH_SHOWOFF_STAFF,			"Saber"		},
	{"force",			BOTH_USEFORCE,				"Force"		},
	{"forcecasual",		BOTH_FORCECASUAL,			"Force"		},
	{"forcechoke",		BOTH_FORCEGRIP3,			"Force"		},
	{"forcelightning",	BOTH_FORCELIGHTNING_HOLD,	"Force"		},
	{"guard",			BOTH_GUARD,					"Saber"		},
	{"gunspin1",		BOTH_GUNSPINB,				"Blaster"	},
	{"gunspin2",		BOTH_GUNSPINF,				"Blaster"	},
	{"gunspin3",		BOTH_GUNSPINS,				"Blaster"	},
	{"handsback",		BOTH_HANDSBACK,				"Body"		},
	{"handsfront",		BOTH_HANDSFRONT,			"Body"		},
	{"handstand",		BOTH_HANDSTAND,				"Body"		},
	{"handstand2",		BOTH_HANDSTAND2,			"Body"		},
	{"headhold",		BOTH_HANDSHEAD,				"Body"		},
	{"helpedup",		BOTH_HELPEDUP,				"Body"		},
	{"helpup",			BOTH_HELPUP,				"Body"		},
	{"heroic",			BOTH_STAND5TOSTAND8,		"Body"		},
	{"hips",			BOTH_HANDSHIPS,				"Body"		},
	{"hips2",			BOTH_HANDSHIPS2,			"Body"		},
	{"holddetonator",	TORSO_WEAPONREADY10,		"Body"		},
	{"holdobject",		BOTH_OBJECT,				"Body"		},
	{"hug",				BOTH_HUGGER1,				"Body"		},
	{"hurt",			BOTH_HURT,					"Body"		},
	{"hurt2",			BOTH_HURT2,					"Body"		},
	{"idle",			BOTH_SABERIDLE,				"Saber"		},
	{"jarkai",			BOTH_JARKAI,				"Saber"		},
	{"jarkai2",			BOTH_JARKAIREVERSE,			"Saber"		},
	{"juyo",			BOTH_JUYO,					"Saber"		},
	{"kneel",			BOTH_CROUCH3,				"Body"		},
	{"lean",			BOTH_STAND10,				"Body"		},
	{"leanback",		BOTH_LEANWALL,				"Body"		},
	{"leanfront",		BOTH_LEANFRONT,				"Body"		},
	{"leantable",		BOTH_STAND7TOSTAND8,		"Body"		},
	{"makashi",			BOTH_MAKASHI,				"Saber"		},
	{"meditate",		BOTH_STAND5TOSIT2,			"Body"		},
	{"meditate2",		BOTH_MEDITATION2,			"Force"		},
	{"meditate3",		BOTH_MEDITATEFORCE,			"Force"		},
	{"mindtrick",		BOTH_MINDTRICK1,			"Force"		},
	{"niman",			BOTH_NIMAN,					"Saber"		},
	{"pistol",			BOTH_PISTOLREADY,			"Blaster"	},
	{"point",			BOTH_STAND5TOAIM,			"Body"		},
	{"ponder",			BOTH_PONDER,				"Body"		},
	{"ponder2",			BOTH_PONDER2,				"Body"		},
	{"pressbutton",		BOTH_BUTTON_HOLD,			"Body"		},
	{"pushup",			BOTH_PUSHUP,				"Body"		},
	{"quickdraw",		BOTH_QUICKDRAW,				"Blaster"	},
	{"quickdraw2",		BOTH_QUICKDRAW2,			"Blaster"	},
	{"read",			BOTH_READ,					"Body"		},
	{"relax",			BOTH_LAYDOWN,				"Body"		},
	{"saberdraw1",		BOTH_SABERDRAW1,			"Saber"		},
	{"saberdraw2",		BOTH_SABERDRAW2,			"Saber"		},
	{"saberdraw3",		BOTH_SABERDRAW3,			"Saber"		},
	{"saberdraw4",		BOTH_SABERDRAW4,			"Saber"		},
	{"saberdraw5",		BOTH_SABERDRAW5,			"Saber"		},
	{"saberpoint",		BOTH_SABERPOINT,			"Saber"		},
	{"saberpoint2",		BOTH_SABERPOINT2,			"Saber"		},
	{"saberthrow",		BOTH_SABERTHROW1START,		"Saber"		},
	{"salute",			BOTH_SALUTE,				"Body"		},
	{"scratch",			BOTH_HEADSCRATCH,			"Body"		},
	{"shien",			BOTH_SHIEN,					"Saber"		},
	{"shien2",			BOTH_SHIEN2,				"Saber"		},
	{"shien3",			BOTH_SHIEN3,				"Saber"		},
	{"shiicho",			BOTH_SHIICHO,				"Saber"		},
	{"sit",				BOTH_SIT2,					"Body"		},
	{"sit2",			BOTH_SIT3,					"Body"		},
	{"sit3",			BOTH_SIT6,					"Body"		},
	{"sit4",			BOTH_SITARMS,				"Body"		},
	{"sit5",			BOTH_SITCROSS,				"Body"		},
	{"sit6",			BOTH_SITCROSS2,				"Body"		},
	{"sit7",			BOTH_SITLEAN,				"Body"		},
	{"sitfeet",			BOTH_SITFEET,				"Body"		},
	{"sitpalm",			BOTH_SITPALM,				"Body"		},
	{"sitpalm2",		BOTH_SITPALM2,				"Body"		},
	{"sitpilot",		BOTH_GUNSIT1,				"Body"		},
	{"situp",			BOTH_SITUP,					"Body"		},
	{"sleep",			BOTH_SLEEP1,				"Body"		},
	{"sneak",			BOTH_CROUCH1,				"Movement"	},
	{"soresu",			BOTH_SORESU,				"Saber"		},
	{"soresu2",			BOTH_SORESU2,				"Saber"		},
	{"spreadlegs",		BOTH_KNEEL_TO_STAND,		"Body"		},
	{"stance",			BOTH_SABERSTANCE,			"Saber"		},
	{"stance2",			BOTH_SABERSTANCE2,			"Saber"		},
	{"stance3",			BOTH_SABERSTANCE3,			"Saber"		},
	{"surrender",		TORSO_SURRENDER_START,		"Body"		},
	{"tossleft",		BOTH_TOSS1,					"Force"		},
	{"tossright",		BOTH_TOSS2,					"Force"		},
	{"type",			BOTH_CONSOLE1,				"Body"		},
	{"victory",			BOTH_WINGS_CLOSE,			"Saber"		},
	{"victory2",		BOTH_DEATH14_UNGRIP,		"Saber"		},
	{"victory3",		BOTH_DEATH14_SITUP,			"Saber"		},
	{"victory4",		BOTH_VICTORY_STAFF,			"Saber"		},
	{"wave",			BOTH_SILENCEGESTURE1,		"Body"		},
	{"windy",			BOTH_WIND,					"Movement"	},
};

#define MAX_EMOTE_CATEGORIES 5

const char anim_headers[MAX_EMOTE_CATEGORIES][50] = {
	"Body",
	"Movement",
	"Blaster",
	"Saber",
	"Force"
};

#define BROADCAST_DISTANCE 999999999
#define VOICE_DISTANCE 600
#define VOICE_DISTANCE_LONG 2000
#define VOICE_DISTANCE_LOW 65
#define SHOUT_DISTANCE 1500
// DAJ_RP: /whisper and /whisperlow, the quiet counterpart of /shout.
#define WHISPER_DISTANCE 65
#define WHISPER_DISTANCE_LOW 50
#define ACTION_DISTANCE 1200
#define ACTION_DISTANCE_LOW 200
#define ACTION_DISTANCE_LONG 2000

#define RP_SHAKE_MIN_DISTANCE		0
#define RP_SHAKE_MAX_DISTANCE		1500
#define RP_SHAKE_MIN_INTENSITY		1
#define RP_SHAKE_MAX_INTENSITY		10
#define RP_SHAKE_MIN_SECONDS		1
#define RP_SHAKE_MAX_SECONDS		5

const chat_modifiers_t chat_modifiers[] = {
	{"/low",		"chat \"%s^3 lowers their voice:^2%s\n\"",			VOICE_DISTANCE_LOW	},
	{"/long",		"chat \"%s^3 raises their voice:^2%s\n\"",			VOICE_DISTANCE_LONG	},
	{"/all",		"chat \"%s^7:^2%s\n\"",								BROADCAST_DISTANCE	},
	{"/melow",		"chat \"%s^3%s\n\"",								ACTION_DISTANCE_LOW	},
	{"/meall",		"chat \"%s^3%s\n\"",								BROADCAST_DISTANCE	},
	{"/melong",		"chat \"%s^3%s\n\"",								ACTION_DISTANCE_LONG},
	{"/me",			"chat \"%s^3%s\n\"",								ACTION_DISTANCE		},
	{"/shoutlong",	"chat \"%s ^3shouts:^2%s\n\"",						VOICE_DISTANCE_LONG	},
	{"/shoutall",	"chat \"%s ^3shouts:^2%s\n\"",						BROADCAST_DISTANCE	},
	{"/shout",		"chat \"%s ^3shouts:^2%s\n\"",						SHOUT_DISTANCE		},
	{"/whisperlow",	"chat \"%s ^3whispers:^2%s\n\"",						WHISPER_DISTANCE_LOW},
	{"/whisper",	"chat \"%s ^3whispers:^2%s\n\"",						WHISPER_DISTANCE	},
	{"/dolow",		"chat \"^3(%s^3)%s\n\"",							ACTION_DISTANCE_LOW	},
	{"/dolong",		"chat \"^3(%s^3)%s\n\"",							ACTION_DISTANCE_LONG},
	{"/doall",		"chat \"^3(%s^3)%s\n\"",							BROADCAST_DISTANCE	},
	{"/do",			"chat \"^3(%s^3)%s\n\"",							ACTION_DISTANCE		},
	{"/forcelow",	"chat \"%s^5 uses the Force to%s\n\"",				ACTION_DISTANCE_LOW	},
	{"/forcelong",	"chat \"%s^5 uses the Force to%s\n\"",				ACTION_DISTANCE_LONG},
	{"/forceall",	"chat \"%s^5 uses the Force to%s\n\"",				BROADCAST_DISTANCE	},
	{"/force",		"chat \"%s^5 uses the Force to%s\n\"",				ACTION_DISTANCE		},
	{"/mylow",		"chat \"%s^3's%s\n\"",								ACTION_DISTANCE_LOW	},
	{"/myall",		"chat \"%s^3's%s\n\"",								BROADCAST_DISTANCE	},
	{"/mylong",		"chat \"%s^3's%s\n\"",								ACTION_DISTANCE_LONG},
	{"/my",			"chat \"%s^3's%s\n\"",								ACTION_DISTANCE		},
	{"/ryl2",		"chat \"%s ^3(Ryl - Lekku only):^2%s\n\"",			VOICE_DISTANCE		},
	{"/ryl",		"chat \"%s ^3(Ryl):^2%s\n\"",						VOICE_DISTANCE		},
	{"/rodian",		"chat \"%s ^3(Rodian):^2%s\n\"",					VOICE_DISTANCE		},
	{"/huttese",	"chat \"%s ^3(Huttese):^2%s\n\"",					VOICE_DISTANCE		},
	{"/catharese",	"chat \"%s ^3(Catharese):^2%s\n\"",					VOICE_DISTANCE		},
	{"/mando",		"chat \"%s ^3(Mando'a):^2%s\n\"",					VOICE_DISTANCE		},
	{"/npc",		"chat \"^3(%s^3) NPC:^4%s\n\"",						VOICE_DISTANCE		},
	{"/npclow",		"chat \"^3(%s^3) NPC lowers their voice:^4%s\n\"",	VOICE_DISTANCE_LOW	},
	{"/npcall",		"chat \"^3(%s^3) NPC:^4%s\n\"",						BROADCAST_DISTANCE	},
	{"/comm",		"chat \"^6<%s^6>^3 -C-^2%s\n\"",					BROADCAST_DISTANCE	},
	{"/c",			"chat \"^6<%s^6>^3 -C-^2%s\n\"",					BROADCAST_DISTANCE	},
	{"/thought",	"chat \"%s ^7is thinking:%s\n\"",					BROADCAST_DISTANCE	},
};

void play_animation(gentity_t *ent, int animation, int time) {
	ent->client->ps.forceHandExtend = HANDEXTEND_TAUNT;
	ent->client->ps.forceDodgeAnim = animation;
	ent->client->ps.forceHandExtendTime = level.time + time;
}

/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage( gentity_t *ent ) {
	char		entry[1024];
	char		string[1400];
	char		header[64];
	int			stringlength;
	int			i, j;
	gclient_t	*cl;
	int			numSorted, scoreFlags, accuracy, perfect;

	// send the latest information on all clients
	string[0] = 0;
	stringlength = 0;
	scoreFlags = 0;

	Com_sprintf( header, sizeof(header), "scores %i %i %i", level.numConnectedClients,
		level.teamScores[TEAM_RED], level.teamScores[TEAM_BLUE] );

	numSorted = level.numConnectedClients;

	if (numSorted > MAX_CLIENT_SCORE_SEND)
	{
		numSorted = MAX_CLIENT_SCORE_SEND;
	}

	for (i=0 ; i < numSorted ; i++) {
		int		ping;

		cl = &level.clients[level.sortedClients[i]];

		if ( cl->pers.connected == CON_CONNECTING ) {
			ping = -1;
		} else {
			ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
		}

		if( cl->accuracy_shots ) {
			accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
		}
		else {
			accuracy = 0;
		}
		perfect = ( cl->ps.persistant[PERS_RANK] == 0 && cl->ps.persistant[PERS_KILLED] == 0 ) ? 1 : 0;

		Com_sprintf (entry, sizeof(entry),
			" %i %i %i %i %i %i %i %i %i %i %i %i %i %i", level.sortedClients[i],
			cl->ps.persistant[PERS_SCORE], ping, (level.time - cl->pers.enterTime) / 60000,
			scoreFlags, g_entities[level.sortedClients[i]].s.powerups, accuracy,
			cl->pers.level,
			cl->ps.persistant[PERS_KNOCKED_DOWN],
			cl->ps.persistant[PERS_KILLED],
			cl->ps.persistant[PERS_DEFEND_COUNT],
			cl->ps.persistant[PERS_ASSIST_COUNT],
			perfect,
			cl->ps.persistant[PERS_CAPTURES]);
		j = strlen(entry);
		if (stringlength + j > 1022 - (int)strlen(header))
			break;
		strcpy (string + stringlength, entry);
		stringlength += j;
	}

	trap->SendServerCommand( ent-g_entities, va("%s%s", header, string ) );
}


/*
==================
Cmd_Score_f

Request current scoreboard information
==================
*/
void Cmd_Score_f( gentity_t *ent ) {
	DeathmatchScoreboardMessage( ent );
}

/*
==================
ConcatArgs
==================
*/
char	*ConcatArgs( int start ) {
	int		i, c, tlen;
	static char	line[MAX_STRING_CHARS];
	int		len;
	char	arg[MAX_STRING_CHARS];

	len = 0;
	c = trap->Argc();
	for ( i = start ; i < c ; i++ ) {
		trap->Argv( i, arg, sizeof( arg ) );
		tlen = strlen( arg );
		if ( len + tlen >= MAX_STRING_CHARS - 1 ) {
			break;
		}
		memcpy( line + len, arg, tlen );
		len += tlen;
		if ( i != c - 1 ) {
			line[len] = ' ';
			len++;
		}
	}

	line[len] = 0;

	return line;
}

/*
==================
StringIsInteger
==================
*/
qboolean StringIsInteger( const char *s ) {
	int			i=0, len=0;
	qboolean	foundDigit=qfalse;

	for ( i=0, len=strlen( s ); i<len; i++ )
	{
		if ( !isdigit( s[i] ) )
			return qfalse;

		foundDigit = qtrue;
	}

	return foundDigit;
}

/*
==================
ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int ClientNumberFromString( gentity_t *to, const char *s, qboolean allowconnecting ) {
	gclient_t	*cl;
	int			idnum;
	char		cleanInput[MAX_NETNAME];

	if ( StringIsInteger( s ) )
	{// numeric values could be slot numbers
		idnum = atoi( s );
		if ( idnum >= 0 && idnum < level.maxclients )
		{
			cl = &level.clients[idnum];
			if ( cl->pers.connected == CON_CONNECTED )
				return idnum;
			else if ( allowconnecting && cl->pers.connected == CON_CONNECTING )
				return idnum;
		}
	}

	Q_strncpyz( cleanInput, s, sizeof(cleanInput) );
	Q_StripColor( cleanInput );

	for ( idnum=0,cl=level.clients; idnum < level.maxclients; idnum++,cl++ )
	{// check for a name match
		if ( cl->pers.connected != CON_CONNECTED )
			if ( !allowconnecting || cl->pers.connected < CON_CONNECTING )
				continue;

		// zyk: changed this so the player name must contain the search string
		if ( Q_stristr( cl->pers.netname_nocolor, cleanInput ) ) // if ( !Q_stricmp( cl->pers.netname_nocolor, cleanInput ) )
			return idnum;
	}

	if (to)
		trap->SendServerCommand( to-g_entities, va( "print \"User %s is not on the server\n\"", s ) );
	else
		trap->Print(va( "User %s is not on the server\n", s ));

	return -1;
}

void print_table_horizontal_line(gentity_t *ent) {
	trap->SendServerCommand(ent - g_entities, "print \" ^9======================================\n\"");
	return;
}


int get_max_spaces_right(const char text[MAX_STRING_CHARS]) {
	return 33 - strlen(text);
}

void print_row(gentity_t *ent, const char text[MAX_STRING_CHARS]) {
	char emote_row[MAX_STRING_CHARS] = " ";

	strcat(emote_row, "^9|| ^3");

	strcat(emote_row, text);

	for (int i = 0; i < get_max_spaces_right(text); i++) {
		strcat(emote_row, " ");
	}

	strcat(emote_row, "^9||\n");
	
	trap->SendServerCommand(ent - g_entities, va("print \"%s\"", emote_row));
	
	return;
}

void print_heading_text_row(gentity_t *ent, const char header_text[MAX_STRING_CHARS]) {
	//34 characters left for space and text
	int length = strlen(header_text);

	char emote_row[MAX_STRING_CHARS] = " ";

	strcat(emote_row, "^9||^3");

	qboolean can_be_centered;

	if (length % 2 == 0) {
		can_be_centered = qtrue;
	}
	else {
		can_be_centered = qfalse;
	}

	int number_of_spaces = 34 - length;
	int left_spaces;
	int right_spaces;

	if (can_be_centered) {
		left_spaces = number_of_spaces / 2;
		right_spaces = left_spaces;
	}
	else {
		//it will be rounded down
		left_spaces = number_of_spaces / 2;
		right_spaces = left_spaces + 1;
	}

	//take care of the space to the left of the writing
	for (int i = 0; i < left_spaces; i++) {
		strcat(emote_row, " ");
	}

	strcat(emote_row, header_text);

	//take care of the space to the right of the writing
	for (int i = 0; i < right_spaces; i++) {
		strcat(emote_row, " ");
	}

	strcat(emote_row, "^9||\n");

	trap->SendServerCommand(ent - g_entities, va("print \"%s\"", emote_row));

	return;
}

void print_header(gentity_t *ent, const char text[MAX_STRING_CHARS]) {
	print_table_horizontal_line(ent);
	print_heading_text_row(ent, text);
	print_table_horizontal_line(ent);
}

typedef struct admin_command_description_s {
	const char* title;
	int			number;
} admin_command_description_t;

const admin_command_description_t admin_commands[ADM_NUM_CMDS] = {
	{ "NPC management",			ADM_NPC					},
	{ "No Clip",				ADM_NOCLIP				},
	{ "Give Admin",				ADM_GIVEADM				},
	{ "Teleport",				ADM_TELE				},
	{ "Admin Protect",			ADM_ADMPROTECT			},
	{ "Entities and Remaps",	ADM_ENTITYSYSTEM		},
	{ "Silence",				ADM_SILENCE				},
	{ "Client Print",			ADM_CLIENTPRINT			},
	{ "Shake Screen",			ADM_SHAKESCREEN			},
	{ "Kick / Kill Other",		ADM_KICK				},
	{ "Paralyze",				ADM_PARALYZE			},
	{ "Give",					ADM_GIVE				},
	{ "Scale",					ADM_SCALE				},
	{ "Players",				ADM_PLAYERS				},
	{ "Duel/Melee Arena",		ADM_DUELARENA			},
	{ "Change Map",				ADM_CHANGEMAP			},
	{ "Create Item",			ADM_CREATEITEM			},
	{ "God Mode / No Target",	ADM_GOD					},
	{ "Upgrade Levels",			ADM_LEVELUP				},
	{ "Upgrade Skills",			ADM_SKILL				},
	{ "Create Credits",			ADM_CREATECREDITS		},
	{ "Ignore Chat Distance",	ADM_IGNORECHATDISTANCE	},
	{ "Give XP",				ADM_XP					},
	{ "Update News",			ADM_UPDATENEWS			},
	{ "Remove News",			ADM_REMOVENEWS			},
	{ "Play Music",				ADM_MUSIC				},
	{ "Instant Revive",			ADM_GETUP				},
	{ "Weather",				ADM_WEATHER				},
	{ "Player effects",			ADM_PHASE				}
};

qboolean check_admin_command(gentity_t* ent, int admin_command, qboolean with_message) {
	if (!(ent->client->pers.bitvalue & (1 << admin_command)))
	{
		if (with_message) {
			trap->SendServerCommand(ent - g_entities, va("print \"^1You don't have the necessary admin command to execute this.\n^1You need the ^3%s ^1admin command.\n\"", admin_commands[admin_command].title));
		}
		return qfalse;
	}

	return qtrue;
}

// GalaxyRP: [security] adapted from the newer Zyk mod's own zyk_check_user_input() (found while
// comparing that mod's entity-manipulation commands against ours). Rejects any string that is empty,
// too long, or contains anything outside A-Z/a-z/0-9. Use this on any raw player-typed argument that
// is later going to be spliced into a filesystem path (or, worse, a system() call) -- e.g. the
// <filename> argument of /entsave, /entload and /entdeletefile, and a new character's name in
// create_new_character() -- so a value like "../../whatever" or a shell metacharacter can't reach
// fopen()/remove()/system() and escape the folder (or command) it was meant to stay inside.
qboolean zyk_check_user_input(char *user_input, int user_input_size) {
	int i = 0;

	static const char allowed_chars[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789";

	if (user_input_size > MAX_STRING_CHARS)
	{ // zyk: somehow the string is bigger than the max
		return qfalse;
	}

	if (user_input[0] == '\0')
	{ // zyk: empty string
		return qfalse;
	}

	while (user_input[i] != '\0' && i < user_input_size)
	{
		if (strchr(allowed_chars, user_input[i]) == NULL)
		{ // zyk: char is not one of the allowed ones
			return qfalse;
		}

		i++;
	}

	if (user_input[i] != '\0' && i == user_input_size)
	{ // zyk: string did not terminate with NULL
		return qfalse;
	}

	return qtrue;
}

qboolean RP_PasswordIsValid( const char *password, const char **reason )
{
	const char *s;
	qboolean nonBlank = qfalse;

	if ( !password )
	{
		*reason = "^1Password cannot be empty.";
		return qfalse;
	}

	for ( s = password; *s; s++ )
	{
		if ( *s != ' ' && *s != '\t' )
		{
			nonBlank = qtrue;
			break;
		}
	}

	if ( !nonBlank )
	{
		*reason = "^1Password cannot be empty.";
		return qfalse;
	}

	if ( strlen( password ) > RP_PASSWORD_MAX )
	{
		*reason = "^1Password can only have a maximum of " XSTRING( RP_PASSWORD_MAX ) " characters.";
		return qfalse;
	}

	*reason = NULL;
	return qtrue;
}

void show_animation_list(gentity_t* ent, int beginning_index, int end_index) {
	for (int i = beginning_index; i < end_index; i++) {
		print_header(ent, anim_headers[i]);
		for (int j = 0; j < MAX_WORDED_EMOTES; j++) {
			//alex: if animation is in that category
			if (Q_stricmp(animations[j].animation_category, anim_headers[i]) == 0) {
				print_row(ent, animations[j].animation_name);
			}
		}
	}
	//alex: end the table
	print_table_horizontal_line(ent);

	return;
}

// alex: plays an animation from anims.h by id OR a word (look for animation_t)
void Cmd_Emote_f( gentity_t *ent )
{
	char arg[MAX_TOKEN_CHARS] = {0};
	char anim_id[100] = "";

	if (rp_allow_emotes.integer < 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot use emotes in this server\n\"" );
		return;
	}

	if (level.gametype == GT_SIEGE)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot use emotes in Siege gametype\n\"" );
		return;
	}

	if (rp_allow_emotes.integer != 1 && ent->client->ps.duelInProgress == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot use emotes in private duel\n\"");
		return;
	}

	if ( trap->Argc () < 2 ) {
		trap->SendServerCommand( ent-g_entities, va("print \"Usage: emote <anim id between 0 and %d>\n\"",MAX_ANIMATIONS-1) );
		return;
	}

	//alex: string magic
	trap->Argv( 1, arg, sizeof( arg ) );
	Q_strncpyz(anim_id, arg, sizeof(anim_id));

	if (G_PlayerIsDowned(ent))
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot do this while you are downed.\n\"");
		return;
	}

	if (ent->client->ps.forceHandExtend == HANDEXTEND_KNOCKDOWN)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot use emotes while knocked down\n\"");
		return;
	}

	for (int i = 0; i < MAX_WORDED_EMOTES; i++)
	{
		//alex: here, the anim_id is the worded animation name
		if (Q_stricmp(anim_id, animations[i].animation_name) == 0)
		{
			play_animation(ent, animations[i].animation_code, 1000);

			ent->client->pers.player_statuses |= (1 << PLAYER_STATUS_EMOTE);

			return;
		}
	}

	if (strcmp(anim_id, "list") == 0)
	{
		if (trap->Argc() == 3) {
			trap->Argv(2, arg, sizeof(arg));
			int page = atoi(arg);

			if (page == 2) {
				show_animation_list(ent, 3, MAX_EMOTE_CATEGORIES);
			}
			else {
				trap->SendServerCommand(ent - g_entities, "print \"That is not a valid emote category!\n\"");
			}
		}
		else {
			show_animation_list(ent, 0, 3);
			trap->SendServerCommand(ent - g_entities, "print \"^3Page 1/2. To see the rest of the animations, do /emote list 2\n\"");
		}

		return;
	}

	char* end_ptr = NULL;
	long anim_id_int = strtol(anim_id, &end_ptr, 10);

	if (end_ptr != anim_id && *end_ptr == '\0')
	{
		if (anim_id_int >= 0 && anim_id_int < MAX_ANIMATIONS)
		{
			play_animation(ent, (int)anim_id_int, 1000);

			ent->client->pers.player_statuses |= (1 << PLAYER_STATUS_EMOTE);

			return;
		}

		trap->SendServerCommand( ent-g_entities, va("print \"Usage: anim ID must be between 0 and %d\n\"",MAX_ANIMATIONS-1) );
		return;
	}

	trap->SendServerCommand( ent-g_entities, va("print \"Unknown emote '%s'. Use /emote list to see available emotes.\n\"", anim_id) );
}

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void zyk_remove_force_powers( gentity_t *ent )
{
	int i = 0;

	for (i = FP_HEAL; i < NUM_FORCE_POWERS; i++)
	{
		ent->client->ps.fd.forcePowersKnown &= ~(1 << i);
		ent->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_0;
	}

	ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);

	ent->client->ps.weapon = WP_MELEE;

	// zyk: reset the force powers of this player
	WP_InitForcePowers( ent );

	if (ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] > FORCE_LEVEL_0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);

	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);
}

void zyk_adjust_holdable_items(gentity_t *ent);
void zyk_remove_guns( gentity_t *ent )
{
	int i = 0;

	for (i = WP_STUN_BATON; i < WP_NUM_WEAPONS; i++)
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << i);
	}

	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);
	ent->client->ps.weapon = WP_MELEE;

	ent->client->ps.ammo[AMMO_BLASTER] = 0;
	ent->client->ps.ammo[AMMO_POWERCELL] = 0;
	ent->client->ps.ammo[AMMO_METAL_BOLTS] = 0;
	ent->client->ps.ammo[AMMO_ROCKETS] = 0;
	ent->client->ps.ammo[AMMO_THERMAL] = 0;
	ent->client->ps.ammo[AMMO_TRIPMINE] = 0;
	ent->client->ps.ammo[AMMO_DETPACK] = 0;
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] = (1 << HI_NONE);

	zyk_adjust_holdable_items(ent);

	if (ent->client->jetPackOn)
	{
		Jetpack_Off(ent);
	}

	// zyk: reset the force powers of this player
	WP_InitForcePowers( ent );

	if (ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] > FORCE_LEVEL_0)
		ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);

	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);

	ent->client->ps.ammo[AMMO_BLASTER] = 100;
}

void zyk_add_force_powers( gentity_t *ent )
{
	int i = 0;

	// DAJ_RP: [Admin] every power at level 3, Saber Attack too: /give is for logged-out players, and levels 4
	// and 5 -- for Saber Attack the Desann and Tavion styles -- are for logged-in characters only. Saber Attack
	// used to be given at 5.
	for (i = FP_HEAL; i < NUM_FORCE_POWERS; i++)
	{
		ent->client->ps.fd.forcePowersKnown |= (1 << i);
		ent->client->ps.fd.forcePowerLevel[i] = FORCE_LEVEL_3;
	}

	ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);
}


void zyk_add_guns( gentity_t *ent )
{
	int i = 0;

	ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);

	for (i = WP_STUN_BATON; i < WP_NUM_WEAPONS; i++)
	{
		if (i != WP_SABER && i != WP_EMPLACED_GUN && i != WP_TURRET)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << i);
	}

	ent->client->ps.ammo[AMMO_BLASTER] = rp_max_blaster_pack_ammo.integer;
	ent->client->ps.ammo[AMMO_POWERCELL] = rp_max_power_cell_ammo.integer;
	ent->client->ps.ammo[AMMO_METAL_BOLTS] = rp_max_metal_bolt_ammo.integer;
	ent->client->ps.ammo[AMMO_ROCKETS] = rp_max_rocket_ammo.integer;
	ent->client->ps.ammo[AMMO_THERMAL] = rp_max_thermal_ammo.integer;
	ent->client->ps.ammo[AMMO_TRIPMINE] = rp_max_tripmine_ammo.integer;
	ent->client->ps.ammo[AMMO_DETPACK] = rp_max_detpack_ammo.integer;

	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_BINOCULARS);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SEEKER);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_EWEB);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC_BIG);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SHIELD);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_CLOAK);
	ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);
}

static const char *zyk_minigame_name( gentity_t *ent );
// DAJ_RP: [Admin] defined further down, beside zyk_stop_active_force_powers(); see Cmd_Give_f().
void zyk_stop_active_holdables( gentity_t *ent );

static void rp_give_reset_active( gentity_t *ent )
{
	if (ent->client->ewebIndex && ent->health > 0)
	{
		gentity_t *eweb = &g_entities[ent->client->ewebIndex];

		if (eweb->inuse && eweb->r.ownerNum == ent->s.number)
		{
			ent->client->ps.stats[STAT_WEAPONS] = eweb->genericValue11;
		}
	}

	zyk_stop_active_force_powers(ent);
	zyk_stop_active_holdables(ent);
}

static void rp_give_lift( gentity_t *ent )
{
	if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_ADM_GIVE_FORCE))
	{ // zyk: remove force powers
		rp_give_reset_active(ent);
		zyk_remove_force_powers(ent);

		ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_ADM_GIVE_FORCE);
		trap->SendServerCommand( -1, va("print \"Removed force powers from %s^7\n\"", ent->client->pers.netname) );
		trap->SendServerCommand( ent->s.number, "cp \"^7Your Force powers have been taken away\n\"" );
	}
	else if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_ADM_GIVE_GUNS))
	{ // zyk: remove guns
		rp_give_reset_active(ent);
		zyk_remove_guns(ent);

		ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_ADM_GIVE_GUNS);
		trap->SendServerCommand( -1, va("print \"Removed guns from %s^7\n\"", ent->client->pers.netname) );
		trap->SendServerCommand( ent->s.number, "cp \"^7Your weapons have been taken away\n\"" );
	}
}

void Cmd_Give_f( gentity_t *ent )
{
	char arg1[MAX_TOKEN_CHARS] = {0};
	char arg2[MAX_TOKEN_CHARS] = {0};
	int client_id = -1;
	const char *minigame = NULL;

	if (!check_admin_command(ent, ADM_GIVE, qtrue))
	{
		return;
	}

	if (trap->Argc() != 3)
	{
		trap->SendServerCommand( ent-g_entities, "print \"^1Command Usage: ^3/give ^2<player name or ID> <force|guns>\n"
			"^1Example: ^3/give ^2PlayerName force\n"
			"^7Toggles full force powers or full weapon loadout for the target player (logged-out only).\n\"" );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	trap->Argv( 2, arg2, sizeof( arg2 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode == 2)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot use on logged-in players.\n\"" );
		return;
	}

	if (g_entities[client_id].client->ps.duelInProgress == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot give stuff to players in private duels\n\"");
		return;
	}

	minigame = zyk_minigame_name(&g_entities[client_id]);
	if (minigame != NULL)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"Cannot give stuff to a player in a %s\n\"", minigame));
		return;
	}

	if (ent != &g_entities[client_id] && g_entities[client_id].client->sess.amrpgmode > 0 && g_entities[client_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client_id].client->pers.player_settings & (1 << 13)))
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
		return;
	}

	if (Q_stricmp(arg2, "force") == 0)
	{
		if (g_entities[client_id].client->pers.player_statuses & (1 << PLAYER_STATUS_ADM_GIVE_FORCE))
		{ // zyk: remove force powers
			rp_give_lift(&g_entities[client_id]);
		}
		else
		{ // zyk: add force powers
			rp_give_reset_active(&g_entities[client_id]);
			zyk_remove_guns(&g_entities[client_id]);
			zyk_add_force_powers(&g_entities[client_id]);

			g_entities[client_id].client->pers.player_statuses &= ~(1 << PLAYER_STATUS_ADM_GIVE_GUNS);
			g_entities[client_id].client->pers.player_statuses |= (1 << PLAYER_STATUS_ADM_GIVE_FORCE);
			trap->SendServerCommand( -1, va("print \"Added force powers to %s^7\n\"", g_entities[client_id].client->pers.netname) );
			trap->SendServerCommand( client_id, "cp \"^7You have been given full Force powers\n\"" );
		}
	}
	else if (Q_stricmp(arg2, "guns") == 0)
	{
		if (g_entities[client_id].client->pers.player_statuses & (1 << PLAYER_STATUS_ADM_GIVE_GUNS))
		{ // zyk: remove guns
			rp_give_lift(&g_entities[client_id]);
		}
		else
		{ // zyk: add guns
			rp_give_reset_active(&g_entities[client_id]);
			zyk_remove_force_powers(&g_entities[client_id]);
			zyk_add_guns(&g_entities[client_id]);

			g_entities[client_id].client->pers.player_statuses &= ~(1 << PLAYER_STATUS_ADM_GIVE_FORCE);
			g_entities[client_id].client->pers.player_statuses |= (1 << PLAYER_STATUS_ADM_GIVE_GUNS);
			trap->SendServerCommand( -1, va("print \"Added guns to %s^7\n\"", g_entities[client_id].client->pers.netname) );
			trap->SendServerCommand( client_id, "cp \"^7You have been given a full arsenal\n\"" );
		}
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid option. Must be ^3force ^7or ^3guns^7.\n\"" );
		return;
	}
}

/*
==================
Cmd_Scale_f

Scales player size
==================
*/
void do_scale(gentity_t *ent, int new_size)
{
	ent->client->ps.iModelScale = new_size;
	ent->client->pers.player_scale = new_size;

	if (new_size == 100) // zyk: default size
		ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_SCALED);
	else
		ent->client->pers.player_statuses |= (1 << PLAYER_STATUS_SCALED);
}

void display_scale_help(gentity_t *ent) {
	int i;
	int scale_table_min = 30;			// start scale
	int scale_table_max = 150;			// final scale
	float scale_table_meters = 0.35;	// start meters
	float scale_table_steps = 0.02;		// increased by

	trap->SendServerCommand(ent->s.number, "print \""
			"^9++============================++\n"
			"^9||^3 Scale ^9||^3 Meters ^9||^3 Feet    ^9||\n"
			"^9++============================++\n"
			"^7\"");

	for (i = scale_table_min; i <= scale_table_max; i++)
	{
		float scale_table_length = (100 * scale_table_meters / 2.54);
		float scale_table_feet = floor(scale_table_length / 12);
		float scale_table_inch = (scale_table_length - 12 * scale_table_feet);

		trap->SendServerCommand(ent->s.number, va("print \"^9||^3 %-5i ^9||^3 %-6.2f ^9||^3 %1.0f' %2.0f'' ^9||\n\"", i, scale_table_meters, scale_table_feet, scale_table_inch));
		if (i == 56 || i == 112) trap->SendServerCommand(ent->s.number, "print \"^9++============================++\n\"");
		scale_table_meters += scale_table_steps;
	}

	trap->SendServerCommand(ent->s.number, "print \"^9++============================++\n\"");
	return;
}

extern void update_current_character_scale(gentity_t* ent, sqlite3* db);

// DAJ_RP: [Scale] /scale's usage line; the sizes it takes, RP_SCALE_MIN and RP_SCALE_MAX, are in g_local.h, which an
// NPC spawner's "npcscale" shares
#define RP_SCALE_USAGE	"Usage: ^3/scale <size>^7 to scale yourself, ^3/scale <player name or ID> <size>^7 to scale another player, or ^3/scale help^7. Size 20 to 500, 100 is normal.\n"

// DAJ_RP: [Scale] the size a player is now: their saved scale while scaled (PLAYER_STATUS_SCALED), else the normal 100
// -- the same test ClientSpawn() makes to put the scale back on after a respawn
static int RP_PlayerScale( const gentity_t *ent )
{
	if ( ent->client->pers.player_statuses & (1 << PLAYER_STATUS_SCALED) )
		return ent->client->pers.player_scale;
	return 100;
}

// DAJ_RP: [Scale] a size argument: a whole number from RP_SCALE_MIN to RP_SCALE_MAX, digits only (atoi() used to take
// "80abc" as 80 and "abc" as 0), or 0 when it is not one
static int RP_ScaleArg( const char *arg )
{
	int i, value = 0;

	if ( !arg[0] || strlen( arg ) > 3 )
		return 0;
	for ( i = 0; arg[i]; i++ )
	{
		if ( arg[i] < '0' || arg[i] > '9' )
			return 0;
		value = value * 10 + ( arg[i] - '0' );
	}
	if ( value < RP_SCALE_MIN || value > RP_SCALE_MAX )
		return 0;
	return value;
}

/*
==================
Cmd_Scale_f
==================
*/
void Cmd_Scale_f( gentity_t *ent ) {
	char arg1[MAX_TOKEN_CHARS] = {0};
	char arg2[MAX_TOKEN_CHARS] = {0};
	gentity_t *target = ent;
	int argc = trap->Argc();
	int new_size = 0;

	if ( argc == 1 )
	{
		trap->SendServerCommand( ent - g_entities, va( "print \"Your size is ^3%d^7. \"", RP_PlayerScale( ent ) ) );
		trap->SendServerCommand( ent - g_entities, "print \"" RP_SCALE_USAGE "\"" );
		return;
	}

	if ( argc > 3 )
	{
		trap->SendServerCommand( ent - g_entities, "print \"" RP_SCALE_USAGE "\"" );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if ( argc == 2 )
	{
		if ( Q_stricmp( arg1, "help" ) == 0 )
		{
			display_scale_help( ent );
			return;
		}

		new_size = RP_ScaleArg( arg1 );
		if ( !new_size )
		{
			trap->SendServerCommand( ent - g_entities, "print \"Size must be a number between 20 and 500. To scale another player: ^3/scale <player name or ID> <size>^7.\n\"" );
			return;
		}
	}
	else
	{
		int client_id;

		trap->Argv( 2, arg2, sizeof( arg2 ) );

		client_id = ClientNumberFromString( ent, arg1, qfalse );
		if ( client_id == -1 )
		{
			return;
		}
		target = &g_entities[client_id];

		// only ask for admin permissions when scaling someone else
		if ( target != ent && !check_admin_command( ent, ADM_SCALE, qtrue ) )
		{
			return;
		}

		new_size = RP_ScaleArg( arg2 );
		if ( !new_size )
		{
			trap->SendServerCommand( ent - g_entities, "print \"Size must be a number between 20 and 500.\n\"" );
			return;
		}

		if ( target != ent && target->client->sess.amrpgmode > 0 &&
			target->client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(target->client->pers.player_settings & (1 << 13)) )
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
			return;
		}
	}

	do_scale( target, new_size );

	if ( target->client->sess.loggedin == qtrue )
	{
		sqlite3* db;
		int rc;

		rc = RP_DB_Open(&db);
		if (rc != SQLITE_OK)
		{
			trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		}
		else
		{
			update_current_character_scale( target, db );
		}
		sqlite3_close(db);
	}

	if ( target == ent )
	{
		trap->SendServerCommand( ent - g_entities, va( "print \"You are now size ^3%d^7.\n\"", new_size ) );
	}
	else
	{
		trap->SendServerCommand( -1, va( "print \"Scaled player %s ^7to ^3%d^7\n\"", target->client->pers.netname, new_size ) );
	}
}

/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f( gentity_t *ent ) {
	char *msg = NULL;

	if (!check_admin_command(ent, ADM_GOD, qtrue))
	{
		return;
	}

	if (G_PlayerIsDowned(ent))
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot do this while you are downed.\n\"");
		return;
	}

	ent->flags ^= FL_GODMODE;
	if (!(ent->flags & FL_GODMODE)) {
		trap->SendServerCommand(-1, va("chat \"^7%s ^7turned god mode ^1OFF\n\"", ent->client->pers.netname));
		msg = "godmode OFF";
	}
	else {
		trap->SendServerCommand(-1, va("chat \"^7%s ^7turned god mode ^2ON\n\"", ent->client->pers.netname));
		msg = "godmode ON";
	}
		
	trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", msg ) );
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	gentity_t *target = ent;
	char *msg = NULL;

	if (!check_admin_command(ent, ADM_GOD, qtrue))
	{
		return;
	}

	if ( trap->Argc() > 2 )
	{
		trap->SendServerCommand( ent-g_entities, "print \"^1Command Usage: ^3/notarget ^2<player name or ID (optional)>\n^7Without a name it applies to you.\n\"" );
		return;
	}

	if ( trap->Argc() == 2 )
	{
		int client_id;

		trap->Argv( 1, arg1, sizeof( arg1 ) );
		client_id = ClientNumberFromString( ent, arg1, qfalse );

		if ( client_id == -1 )
		{
			return;
		}

		target = &g_entities[client_id];
	}

	if ( !target->inuse || !target->client )
	{
		return;
	}

	// same Admin Protect rule as /give, /scale, /teleport and the phase commands
	if ( target != ent && target->client->sess.amrpgmode > 0 &&
		(target->client->pers.bitvalue & (1 << ADM_ADMPROTECT)) && !(target->client->pers.player_settings & (1 << 13)) )
	{
		trap->SendServerCommand( ent-g_entities, "print \"Target player is adminprotected\n\"" );
		return;
	}

	// what CMD_ALIVE used to check, for the target. FL_NOTARGET is cleared by the respawn that ends a
	// death, and a spectator is targeted by nothing, so setting it on either would mean nothing.
	if ( target->health <= 0 || target->client->tempSpectate >= level.time || target->client->sess.sessionTeam == TEAM_SPECTATOR )
	{
		if ( target == ent )
			trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "MUSTBEALIVE" ) ) );
		else
			trap->SendServerCommand( ent-g_entities, "print \"Target player must be alive and not spectating.\n\"" );
		return;
	}

	if (G_PlayerIsDowned(target))
	{
		if ( target == ent )
			trap->SendServerCommand(ent - g_entities, "print \"^1You cannot do this while you are downed.\n\"");
		else
			trap->SendServerCommand(ent - g_entities, va("print \"^1%s ^1is downed. Notarget cannot be changed while downed.\n\"", target->client->pers.netname));
		return;
	}

	if ( target->client->ps.powerups[PW_CLOAKED] )
	{
		if ( target == ent )
			trap->SendServerCommand( ent-g_entities, "print \"^1You are cloaked. Uncloak first -- while cloaked, NPCs and turrets already ignore you.\n\"" );
		else
			trap->SendServerCommand( ent-g_entities, va("print \"^1%s ^1is cloaked. Notarget cannot be changed until they uncloak -- while cloaked, NPCs and turrets already ignore them.\n\"", target->client->pers.netname) );
		return;
	}

	target->flags ^= FL_NOTARGET;
	if ( !(target->flags & FL_NOTARGET) )
		msg = "^1OFF";
	else
		msg = "^2ON";

	if ( target == ent )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"notarget %s\n\"", msg ) );
		trap->SendServerCommand(-1, va("chat \"^7%s ^7turned notarget %s\n\"", ent->client->pers.netname, msg));
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"Notarget %s ^7for %s\n\"", msg, target->client->pers.netname ) );
		trap->SendServerCommand( target-g_entities, va( "print \"An admin turned your notarget %s\n\"", msg ) );
		trap->SendServerCommand(-1, va("chat \"^7%s ^7turned notarget %s ^7for %s\n\"", ent->client->pers.netname, msg, target->client->pers.netname));
	}
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f( gentity_t *ent ) {
	char *msg = NULL;

	if (!check_admin_command(ent, ADM_NOCLIP, qtrue))
	{
		return;
	}

	if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DOWNED)) {
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot noClip while downed!\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^1You cannot noClip while downed!\n\"");

		return;
	}

	if (ent->client->ps.eFlags2 & EF2_HELD_BY_MONSTER)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Cannot noclip while being eaten by a rancor.\n\"" );
		return;
	}

	// zyk: deactivating saber
	if ( ent->client->ps.saberHolstered < 2 )
	{
		Cmd_ToggleSaber_f(ent);
	}

	ent->client->noclip = !ent->client->noclip;
	if ( !ent->client->noclip )
		msg = "noclip OFF";
	else
		msg = "noclip ON";

	trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", msg ) );
}


/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f( gentity_t *ent )
{
	if ( !ent->client->pers.localClient )
	{
		trap->SendServerCommand(ent-g_entities, "print \"The levelshot command must be executed by a local client\n\"");
		return;
	}

	// doesn't work in single player
	if ( level.gametype == GT_SINGLE_PLAYER )
	{
		trap->SendServerCommand(ent-g_entities, "print \"Must not be in singleplayer mode for levelshot\n\"" );
		return;
	}

	BeginIntermission();
	trap->SendServerCommand( ent-g_entities, "clientLevelShot" );
}

#if 0
/*
==================
Cmd_TeamTask_f

From TA.
==================
*/
void Cmd_TeamTask_f( gentity_t *ent ) {
	char userinfo[MAX_INFO_STRING];
	char		arg[MAX_TOKEN_CHARS];
	int task;
	int client = ent->client - level.clients;

	if ( trap->Argc() != 2 ) {
		return;
	}
	trap->Argv( 1, arg, sizeof( arg ) );
	task = atoi( arg );

	trap->GetUserinfo(client, userinfo, sizeof(userinfo));
	Info_SetValueForKey(userinfo, "teamtask", va("%d", task));
	trap->SetUserinfo(client, userinfo);
	ClientUserinfoChanged(client);
}
#endif

void G_Kill( gentity_t *ent ) {
	if ((level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL) &&
		level.numPlayingClients > 1 && !level.warmupTime)
	{
		if (!g_allowDuelSuicide.integer)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "ATTEMPTDUELKILL")) );
			return;
		}
	}

	// zyk: target has been paralyzed by an admin
	if (ent && ent->client && !ent->NPC && ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DOWNED))
		return;

	ent->flags &= ~FL_GODMODE;
	ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;

	if (ent->client->ps.duelInProgress == qtrue)
	{ // zyk: if player is in a private duel, gives kill to the other duelist
		gentity_t *other = &g_entities[ent->client->ps.duelIndex];

		player_die(ent, other, other, 100000, MOD_SUICIDE);
	}
	else if (ent->client->ps.otherKillerTime > level.time && ent->client->ps.otherKiller != ENTITYNUM_NONE)
	{ // zyk: self killing while otherKiller is set gives kill to the otherKiller
		gentity_t *other = &g_entities[ent->client->ps.otherKiller];

		player_die(ent, other, other, 100000, MOD_SUICIDE);
	}
	else
	{
		player_die(ent, ent, ent, 100000, MOD_SUICIDE);
	}
}

extern void Jedi_DecloakPair( gentity_t *self );
extern qboolean Jedi_PairIsCloaked( gentity_t *self );

#define RP_PARALYZE_DEFAULT_SECONDS		30
#define RP_PARALYZE_MIN_SECONDS			30
#define RP_PARALYZE_MAX_SECONDS			900

#define RP_DOWNED_HEALTH				30


void zyk_release_mounts_for_minigame( gentity_t *ent );

static void RP_EnterDownedState( gentity_t *ent, int downedSeconds, qboolean adminParalysis )
{
	if ( !ent || !ent->client )
	{
		return;
	}

	if ( ent->client->hook )
	{
		Weapon_HookFree( ent->client->hook );
	}

	if ( Jedi_PairIsCloaked( ent ) )
	{
		Jedi_DecloakPair( ent );
	}

	ent->flags |= FL_NOTARGET;

	ent->client->pers.player_statuses |= (1 << PLAYER_STATUS_DOWNED);

	if ( adminParalysis )
	{
		ent->client->pers.player_statuses |= (1 << PLAYER_STATUS_ADMIN_PARALYSIS);
	}
	else
	{
		ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_ADMIN_PARALYSIS);
	}

	ent->client->pers.downedTime = downedSeconds;

	ent->client->downedTimeResidual = 0;

	if ( adminParalysis )
	{
		ent->client->invulnerableTimer = level.time + (downedSeconds * 1000) + 3000;
	}
	else
	{
		ent->client->invulnerableTimer = level.time + 3000;
	}

	ent->client->ps.eFlags |= EF_INVULNERABLE;

	Jetpack_Off( ent );

	if ( ent->client->ps.emplacedIndex || ent->client->ewebIndex )
	{
		zyk_release_mounts_for_minigame( ent );
	}

	if ( ent->client->ps.weapon == WP_SABER && ent->client->ps.saberHolstered != 2 )
	{
		const int was_holstered = ent->client->ps.saberHolstered;

		ent->client->ps.saberHolstered = 2;

		if ( ent->client->saber[0].soundOff )
		{
			G_Sound( ent, CHAN_AUTO, ent->client->saber[0].soundOff );
		}
		if ( was_holstered == 0 && ent->client->saber[1].soundOff && ent->client->saber[1].model[0] )
		{
			G_Sound( ent, CHAN_AUTO, ent->client->saber[1].soundOff );
		}
	}

	ent->client->ps.forceHandExtend = HANDEXTEND_KNOCKDOWN;
	ent->client->ps.forceHandExtendTime = level.time + 500;
	ent->client->ps.velocity[2] += 150;
	ent->client->ps.forceDodgeAnim = 0;
	ent->client->ps.quickerGetup = qtrue;
}

/*
==================
RP_ClearDownedState
==================
*/
void RP_ClearDownedState( gentity_t *ent )
{
	if ( !ent || !ent->client )
	{
		return;
	}

	if ( G_PlayerIsDowned( ent ) && (ent->flags & FL_NOTARGET) )
	{
		ent->flags &= ~FL_NOTARGET;
	}

	if ( G_PlayerIsAdminParalyzed( ent ) )
	{
		ent->client->ps.eFlags &= ~EF_INVULNERABLE;
		ent->client->invulnerableTimer = 0;
	}

	ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_DOWNED);
	ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_ADMIN_PARALYSIS);
	ent->client->pers.downedTime = 0;
}

void RP_ReleaseFromDownedState( gentity_t *ent )
{
	int anim_to_play;

	if ( !ent || !ent->client )
	{
		return;
	}

	RP_ClearDownedState( ent );

	// same selection help_up() uses for a player getting up unaided
	switch ( ent->client->pers.skill_levels[0] )
	{
	case FORCE_LEVEL_1:
	case FORCE_LEVEL_2:
	case FORCE_LEVEL_3:
	case FORCE_LEVEL_4:
	case FORCE_LEVEL_5:
		anim_to_play = BOTH_BACK_FLIP_UP;
		break;
	case FORCE_LEVEL_0:
	default:
		anim_to_play = BOTH_GETUP1;
		break;
	}

	play_animation( ent, anim_to_play, 1000 );
}

void paralyze_player( gentity_t *ent )
{
	if ( !ent || !ent->client )
	{
		return;
	}

	//GalaxyRP (Alex): [Death System] Set their HP so they don't die the old way instantly.
	ent->client->ps.stats[STAT_HEALTH] = RP_DOWNED_HEALTH;
	ent->health = RP_DOWNED_HEALTH;

	RP_EnterDownedState( ent, rp_downed_timer.integer, qfalse );

	ent->client->ps.persistant[PERS_KNOCKED_DOWN]++;
}

qboolean can_player_get_up(gentity_t* ent, gentity_t* target) {
	
	if (!G_PlayerIsDowned(target)) {
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot help them because they're not downed!\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^1You cannot help them because they're not downed!\n\"");

		return qfalse;
	}

	// GalaxyRP (Alex): No getting up if paralyzed.
	if (G_PlayerIsAdminParalyzed(target)) {
		if (ent == target) {
			trap->SendServerCommand(ent - g_entities, "print \"^1You were paralyzed by an admin and cannot get up.\n\"");
			trap->SendServerCommand(ent - g_entities, "cp \"^1You were paralyzed by an admin and cannot get up.\"");
		}
		else {
			trap->SendServerCommand(ent - g_entities, va("print \"^1%s ^1was paralyzed by an admin and cannot be helped up.\n\"", target->client->pers.netname));
			trap->SendServerCommand(ent - g_entities, va("cp \"^1%s ^1was paralyzed by an admin and cannot be helped up.\"", target->client->pers.netname));
		}
		return qfalse;
	}

	//GalaxyRP (Alex): [Death System] Ent has permission, they can revive anyone.
	if (check_admin_command(ent, ADM_GETUP, qfalse)) {
		if (ent == target) {
			trap->SendServerCommand(ent - g_entities, "print \"^2You got up!\n\"");
			trap->SendServerCommand(ent - g_entities, "cp \"^2You got up!\n\"");
		}
		else {
			trap->SendServerCommand(ent - g_entities, va("cp \"^2You helped %s up.\"", target->client->pers.netname));
			trap->SendServerCommand(ent - g_entities, va("print \"^2You helped %s up.\n\"", target->client->pers.netname));

			trap->SendServerCommand(target - g_entities, va("cp \"^2%s helped you up!\"", ent->client->pers.netname));
			trap->SendServerCommand(target - g_entities, va("print \"^2%s helped you up!\n\"", ent->client->pers.netname));
		}

		return qtrue;
	}

	if (ent == target) {
		//GalaxyRP (Alex): [Death System] If player's timer is done, allow them to get up.
		if (ent->client->pers.downedTime == 0) {
			trap->SendServerCommand(ent - g_entities, "print \"^2You got up!\n\"");
			trap->SendServerCommand(ent - g_entities, "cp \"^2You got up!\n\"");
			return qtrue;
		}
		else {
			trap->SendServerCommand(ent - g_entities, "print \"^1You cannot get up until the timer is finished!\n\"");
			trap->SendServerCommand(ent - g_entities, "cp \"^1You cannot get up until the timer is finished!\n\"");
			return qfalse;
		}
	}
	//GalaxyRP (Alex): [Death System] Ent and target are different.
	else {
		//GalaxyRP (Alex): [Death System] Can't help someone else get up if you're also down.
		if (G_PlayerIsDowned(ent)) {
			trap->SendServerCommand(ent - g_entities, "print \"^1You cannot help someone else while you're downed!\n\"");
			trap->SendServerCommand(ent - g_entities, "cp \"^1You cannot help someone else while you're downed!\n\"");

			return qfalse;
		}

		if (Distance(ent->client->ps.origin, target->client->ps.origin) > 65) {
			trap->SendServerCommand(ent - g_entities, va("print \"^1You are too far away to help %s up!\n\"", target->client->pers.netname));
			trap->SendServerCommand(ent - g_entities, va("cp \"^1You are too far away to help %s up!\"", target->client->pers.netname));

			return qfalse;
		}

		trap->SendServerCommand(ent - g_entities, va("cp \"^2You helped %s up.\"", target->client->pers.netname));
		trap->SendServerCommand(ent - g_entities, va("print \"^2You helped %s up.\n\"", target->client->pers.netname));

		trap->SendServerCommand(target - g_entities, va("cp \"^2%s helped you up!\"", ent->client->pers.netname));
		trap->SendServerCommand(target - g_entities, va("print \"^2%s helped you up!\n\"", ent->client->pers.netname));

		return qtrue;
	}
}

void help_up(gentity_t* ent, gentity_t* target) {

	if (can_player_get_up(ent, target)) {
		//GalaxyRP (Alex): [Death System] No longer paralyzed.
		
		RP_ClearDownedState( target );

		if (ent != target) {
			play_animation(ent, BOTH_HELPUP, 1000);
			play_animation(target, BOTH_HELPEDUP, 1000);
		}
		else {
			int anim_to_play;
			switch (ent->client->pers.skill_levels[0])
			{
			case FORCE_LEVEL_0:
				anim_to_play = BOTH_GETUP1;
				break;
			case FORCE_LEVEL_1:
			case FORCE_LEVEL_2:
			case FORCE_LEVEL_3:
			case FORCE_LEVEL_4:
			case FORCE_LEVEL_5:
				anim_to_play = BOTH_BACK_FLIP_UP;
				break;
			default:
				anim_to_play = BOTH_GETUP1;
				break;
			}
			play_animation(target, anim_to_play, 1000);
		}


		if (rp_downed_invulnerability_timer.integer) {
			target->client->invulnerableTimer = level.time + rp_downed_invulnerability_timer.integer * 1000;
			target->client->ps.eFlags |= EF_INVULNERABLE;
		}
	}

	return;
}

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f( gentity_t *ent ) {
	G_Kill(ent);
}

void Cmd_Helpup_f(gentity_t* ent) {
	char targetIndex[MAX_TOKEN_CHARS];

	if (trap->Argc() < 2) {
		trap->SendServerCommand(ent - g_entities, "print \"Usage: /helpup <player name or ID>\n\"");
		return;
	}

	trap->Argv(1, targetIndex, sizeof(targetIndex));
	int i = ClientNumberFromString(ent, targetIndex, qfalse);
	if (i == -1) {
		return;
	}

	gentity_t* target;

	target = &g_entities[i];

	help_up(ent, target);
	
	return;
}

void Cmd_Getup_f(gentity_t* ent) {
	if (!G_PlayerIsDowned(ent)) {
		trap->SendServerCommand(ent - g_entities, "print \"^1You are not downed.\n\"");
		return;
	}

	if (G_PlayerIsAdminParalyzed(ent)) {
		trap->SendServerCommand(ent - g_entities, "print \"^1You were paralyzed by an admin and cannot get up.\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^1You were paralyzed by an admin and cannot get up.\"");
		return;
	}

	//GalaxyRP (Alex): [Death System] If player's timer is done or he is an admin, allow them to get up.
	if (ent->client->pers.downedTime == 0 || check_admin_command(ent, ADM_GETUP, qfalse)) {
		help_up(ent, ent);
		return;
	}
	return;
}

// GalaxyRP (Alex): [Database] This method forces a netname onto a player.
void set_netname(gentity_t* ent, char netName[MAX_STRING_CHARS])
{
	char userinfo[MAX_INFO_STRING];
	// GalaxyRP fix: [Char] Re-derive "this player's" own client slot.
	int clientNum = ent->s.number;

	//Alex: set display name
	Q_strncpyz(ent->client->pers.netname, netName, sizeof(ent->client->pers.netname));
	Q_strncpyz(ent->client->pers.netname_nocolor, netName, sizeof(ent->client->pers.netname_nocolor));
	Q_StripColor(ent->client->pers.netname_nocolor);
	trap->GetUserinfo(clientNum, userinfo, sizeof(userinfo));
	Info_SetValueForKey(userinfo, "name", netName);
	trap->SetUserinfo(clientNum, userinfo);
	ClientUserinfoChanged(clientNum);

	trap->SendServerCommand(clientNum, va("supdatename \"%s\"\n", ent->client->pers.netname));

	return;
}

// GalaxyRP (Alex): [Database] This method forces a model onto a player.
void set_model(gentity_t* ent, char modelName[MAX_STRING_CHARS])
{
	char userinfo[MAX_INFO_STRING];
	// GalaxyRP fix: [Char] Re-derive "this player's" own client slot.
	int clientNum = ent->s.number;

	trap->GetUserinfo(clientNum, userinfo, sizeof(userinfo));

	//Alex: this is how u get the current model
	Info_SetValueForKey(userinfo, "model", modelName);
	trap->SetUserinfo(clientNum, userinfo);
	ClientUserinfoChanged(clientNum);

	trap->SendServerCommand(clientNum, va("supdatemodel \"%s\"\n", modelName));

	return;
}

// GalaxyRP fix: [Force] stop every force power this player currently has running.
void zyk_stop_active_force_powers( gentity_t *ent )
{
	int i = 0;

	if (!ent || !ent->client)
		return;

	for (i = 0; i < NUM_FORCE_POWERS; i++)
	{
		if (ent->client->ps.fd.forcePowersActive & (1 << i))
		{
			WP_ForcePowerStop(ent, (forcePowers_t)i);
		}
	}
}

// GalaxyRP fix: [Items] send a deployed seeker drone into its death spiral.
#define SEEKER_WINDDOWN_MS 5000
void zyk_wind_down_seeker_drone( gentity_t *ent )
{
	const int windDownAt = level.time + SEEKER_WINDDOWN_MS - 1;

	if (!ent || !ent->client)
		return;

	// conditional, not an assignment: a drone already closer to death than this must not have its
	// life EXTENDED to the full window by being "wound down".
	if (ent->client->ps.droneExistTime > windDownAt)
	{
		ent->client->ps.droneExistTime = windDownAt;
	}
}

static void zyk_release_map_emplaced_gun( gentity_t *ent )
{
	if (ent->client->ps.emplacedIndex)
	{
		gentity_t *gun = &g_entities[ent->client->ps.emplacedIndex];

		if (gun->inuse && gun->classname && Q_stricmp(gun->classname, "emplaced_gun") == 0
			&& gun->activator == ent)
		{
			zyk_release_from_emplaced_gun(gun, ent);
		}
		else
		{
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_EMPLACED_GUN);
			ent->client->ps.emplacedIndex = 0;
			ent->client->ps.saberHolstered = 0;
		}
	}
}

// Turn off the binoculars (zoomMode 2) and the disruptor scope (zoomMode 1), together.
static void zyk_cancel_zoom( gentity_t *ent )
{
	if (ent->client->ps.zoomMode)
	{
		ent->client->ps.zoomMode = 0;

		ent->client->ps.zoomTime = level.time;
		ent->client->ps.zoomLocked = qfalse;
		ent->client->ps.zoomLockTime = 0;
	}
}

// GalaxyRP fix: [Account] stop every HOLDABLE this player currently has running.
void zyk_stop_active_holdables( gentity_t *ent )
{
	if (!ent || !ent->client)
		return;

	// 1. the seeker drone
	zyk_wind_down_seeker_drone(ent);

	// 2. the e-web, freed outright -- see the note above on why not EWebDisattach()/EWebDie()
	if (ent->client->ewebIndex)
	{
		gentity_t *eweb = &g_entities[ent->client->ewebIndex];

		ent->client->ps.emplacedIndex = 0;
		ent->client->ewebIndex = 0;

		ent->client->ewebHealth = 0;

		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_EMPLACED_GUN);

		if (eweb->inuse && eweb->r.ownerNum == ent->s.number)
		{
			G_FreeEntity(eweb);
		}
	}

	zyk_release_map_emplaced_gun(ent);

	zyk_cancel_zoom(ent);
}

// GalaxyRP fix: [Minigames] get a player off anything they are manning before a mini-game takes
// their loadout away.
static void RP_RideOffForMinigame( gentity_t *ent )
{
	gentity_t *veh;

	if ( !ent->client->ps.m_iVehicleNum || ent->client->sess.sessionTeam == TEAM_SPECTATOR )
		return;

	veh = &g_entities[ent->client->ps.m_iVehicleNum];
	if ( !veh->inuse || !veh->client || !veh->m_pVehicle || !veh->m_pVehicle->m_pVehicleInfo || ent->r.ownerNum != veh->s.number )
		return;

	if ( !veh->m_pVehicle->m_pVehicleInfo->Eject( veh->m_pVehicle, (bgEntity_t *)ent, qtrue ) && ent->r.ownerNum == veh->s.number )
	{
		const int health = ent->health;

		ent->health = 0;
		veh->m_pVehicle->m_pVehicleInfo->Eject( veh->m_pVehicle, (bgEntity_t *)ent, qtrue );
		ent->health = health;
	}

	if ( !ent->client->ps.m_iVehicleNum )
	{
		trap->SendServerCommand( ent->s.number, "print \"You were taken off your vehicle.\n\"" );
	}
}

void zyk_release_mounts_for_minigame( gentity_t *ent )
{
	if (!ent || !ent->client)
		return;

	RP_RideOffForMinigame(ent);

	if (ent->client->ewebIndex)
	{
		gentity_t *eweb = &g_entities[ent->client->ewebIndex];

		if (eweb->inuse && eweb->r.ownerNum == ent->s.number)
		{
			EWebDisattach(ent, eweb);
		}
		else
		{
			if (ent->client->ps.emplacedIndex == ent->client->ewebIndex)
			{
				ent->client->ps.emplacedIndex = 0;
			}
			ent->client->ewebIndex = 0;
		}
	}

	zyk_release_map_emplaced_gun(ent);

	ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_EMPLACED_GUN);

	zyk_cancel_zoom(ent);
}

// GalaxyRP fix: [Account] load the incoming character's skills without resurrecting a corpse.
void zyk_apply_character_skills( gentity_t *ent )
{
	qboolean was_dead = qfalse;
	int health = 0, health_stat = 0, armor = 0;

	if (!ent || !ent->client)
		return;

	was_dead = (ent->health <= 0) ? qtrue : qfalse;
	health = ent->health;
	health_stat = ent->client->ps.stats[STAT_HEALTH];
	armor = ent->client->ps.stats[STAT_ARMOR];

	initialize_rpg_skills(ent);

	if (was_dead == qtrue)
	{
		ent->health = health;
		ent->client->ps.stats[STAT_HEALTH] = health_stat;
		ent->client->ps.stats[STAT_ARMOR] = armor;
	}
}

void zyk_reset_fuel( gentity_t *ent )
{
	if (!ent || !ent->client)
		return;

	ent->client->pers.jetpack_fuel = MAX_JETPACK_FUEL;
	ent->client->ps.jetpackFuel = 100;
	ent->client->ps.cloakFuel = 100;
}

extern void WP_EndForceEnlightenment( gentity_t *self );
void zyk_reset_force_powerups( gentity_t *ent )
{
	if (!ent || !ent->client)
		return;

	if (level.gametype == GT_SIEGE)
		return;

	WP_EndForceEnlightenment(ent);

	ent->client->ps.powerups[PW_FORCE_BOON] = 0;
	ent->client->ps.powerups[PW_YSALAMIRI] = 0;
}

extern qboolean WP_SaberStyleValidForSaber( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int saberAnimLevel );
extern qboolean WP_UseFirstValidSaberStyle( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int *saberAnimLevel );

/*
GalaxyRP fix: [Saber] put the saber style back to a known-good one whenever the Saber Attack level
the style depends on may have changed without a respawn.
*/
void zyk_reset_saber_style( gentity_t *ent )
{
	if (!ent || !ent->client)
		return;

	ent->client->saberCycleQueue = 0;

	if (level.gametype == GT_SIEGE)
		return;

	if (!(ent->client->saber[0].model[0] && ent->client->saber[1].model[0]) && !(ent->client->saber[0].saberFlags & SFL_TWO_HANDED))
	{ // single saber
		ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel = SS_FAST;
	}

	if (!WP_SaberStyleValidForSaber(&ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, ent->client->ps.fd.saberAnimLevel))
	{
		WP_UseFirstValidSaberStyle(&ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &ent->client->ps.fd.saberAnimLevel);
		ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberDrawAnimLevel = ent->client->ps.fd.saberAnimLevel;
	}
}

void zyk_apply_character_loadout( gentity_t *ent )
{
	if (!ent || !ent->client)
		return;

	if (!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK)) && ent->client->jetPackOn)
	{
		Jetpack_Off(ent);
	}

	zyk_adjust_holdable_items(ent);

	if (ent->client->ps.weapon <= WP_NONE || ent->client->ps.weapon >= WP_NUM_WEAPONS ||
		!(ent->client->ps.stats[STAT_WEAPONS] & (1 << ent->client->ps.weapon)))
	{
		if (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER))
			ent->client->ps.weapon = WP_SABER;
		else if (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_PISTOL))
			ent->client->ps.weapon = WP_BRYAR_PISTOL;
		else
			ent->client->ps.weapon = WP_MELEE;
	}

	zyk_reset_fuel(ent);

	zyk_reset_saber_style(ent);
}

// GalaxyRP: [Account] does this account command need to kill the player to take effect?
qboolean zyk_relog_kill_required( gentity_t *ent )
{
	if (!ent || !ent->client)
		return qfalse;

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
		return qfalse;

	// anything but a positive value means "always respawn", so a mistyped cvar fails safe
	return (rp_seamlesslogin.integer <= 0) ? qtrue : qfalse;
}

void zyk_schedule_relog_kill( gentity_t *ent )
{
	if (zyk_relog_kill_required(ent) == qtrue)
		ent->client->pers.pending_relog_kill_time = level.time + 300;
}

void zyk_default_empty_saber_models(char *saber1, int saber1_size, char *saber2, int saber2_size)
{
	if (saber1 && saber1_size > 0 && saber1[0] == '\0')
		Q_strncpyz(saber1, "saber_1", saber1_size);

	if (saber2 && saber2_size > 0 && saber2[0] == '\0')
		Q_strncpyz(saber2, "none", saber2_size);
}

/*
GalaxyRP fix: [Database] print a line of unknown length, splitting it rather than losing it.
*/
void zyk_print_long_line(gentity_t *ent, const char *prefix, const char *text)
{
	char chunk[RP_LIST_FLUSH_AT + 1];
	int prefix_len = 0;
	int budget = 0;
	int len = 0;
	int pos = 0;

	if (!ent || !ent->client)
		return;

	if (!prefix)
		prefix = "";

	if (!text)
		text = "";

	prefix_len = (int)strlen(prefix);
	budget = RP_LIST_FLUSH_AT - prefix_len;
	len = (int)strlen(text);

	if (budget < 1)
		budget = 1;

	if (len == 0)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"%s\n\"", prefix));
		return;
	}

	while (pos < len)
	{
		int take = ((len - pos) > budget) ? budget : (len - pos);

		if (take > 1 && (pos + take) < len && text[pos + take - 1] == '^')
			take--;

		memcpy(chunk, text + pos, take);
		chunk[take] = '\0';

		// GalaxyRP fix: [security] no "@@@" reaches the client -- see RP_BreakStringEdRefs()
		// (g_utils.c). Per chunk, on the copy: the text may be a row written by hand or by an older
		// build, and each chunk is its own message, so a run split across two is already harmless.
		RP_BreakStringEdRefs(chunk);

		trap->SendServerCommand(ent - g_entities, va("print \"%s%s\n\"", prefix, chunk));

		pos += take;
	}
}

void display_news(gentity_t* ent, int newsID, char* newsText, char* date) {

	trap->SendServerCommand(ent - g_entities, va("print \"^3--------------------------|%i|%s|----------------------\n\"",newsID, date));
	// GalaxyRP fix: [Database] the entry text is the one part of this whose length is not known in
	// advance, so it goes through the splitter -- see zyk_print_long_line(). An entry that fits is
	// still sent as exactly one print, unchanged.
	zyk_print_long_line(ent, "^2", newsText);
	// GalaxyRP fix: [cleanup] dropped a stray `date` argument to this format string, which has no
	// conversion specifier to consume it. Harmless to varargs, but it read as though the footer was
	// meant to show the date and silently did not.
	trap->SendServerCommand(ent - g_entities, "print \"^3-------------------------------------------------------------\n\"");
	return;
}

void update_saber(gentity_t* ent, char* saber1Model, char* saber2Model, int number_of_args);

// GalaxyRP fix: [Account] Stop more than one client logging into the same account.
void zyk_drop_other_sessions_on_account( gentity_t *ent, int accountID )
{
	int i;
	int dropped = 0;

	// zyk: accountID 0 is what a logged-out client carries, so it must never match
	if ( !ent || !ent->client || accountID <= 0 )
	{
		return;
	}

	for ( i = 0; i < level.maxclients; i++ )
	{
		gentity_t *other = &g_entities[i];

		if ( other == ent || !other->client )
		{
			continue;
		}

		if ( other->client->pers.connected == CON_DISCONNECTED )
		{
			continue;
		}

		if ( other->client->sess.loggedin == qfalse || other->client->sess.accountID != accountID )
		{
			continue;
		}

		G_LogPrintf( "account %d: client %d (%s) logged in, dropping client %d (%s) which still held the same account\n",
			accountID, ent->s.number, ent->client->pers.netname, i, other->client->pers.netname );

		// zyk: ClientDisconnect() runs inside this call and saves that session before it goes
		trap->DropClient( i, "Logged in from another connection" );

		dropped++;
	}

	if ( dropped > 0 )
	{
		trap->SendServerCommand( ent - g_entities,
			va("print \"^3%d other connection(s) on this account were dropped.\n\"", dropped) );
	}
}

// GalaxyRP: [Minigames] This player is mid-minigame, so their loadout is not their own right now
static const char *zyk_minigame_name( gentity_t *ent )
{
	if (!ent || !ent->client)
		return NULL;

	if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
		return "Duel Tournament";

	if (level.melee_mode > 0 && level.melee_players[ent->s.number] != -1)
		return "Melee Battle";

	return NULL;
}

static qboolean zyk_account_change_blocked( gentity_t *ent, const char *verb )
{
	const char *minigame = NULL;

	if (!ent || !ent->client)
		return qfalse;

	minigame = zyk_minigame_name(ent);

	if (minigame != NULL)
	{
		trap->SendServerCommand(ent->s.number, va("print \"Cannot %s while in a %s\n\"", verb, minigame));

		return qtrue;
	}

	if (ent->client->ps.duelInProgress == qtrue)
	{
		trap->SendServerCommand(ent->s.number, va("print \"Cannot %s while in a private duel\n\"", verb));

		return qtrue;
	}

	return qfalse;
}

void Cmd_Register_F(gentity_t * ent)
{
	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	sqlite3_stmt *stmt = NULL;
	char username[256] = { 0 }, password[256] = { 0 };
	int accountID = 0, i = 0;

	if (G_PlayerIsDowned(ent))
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot do this while you are downed.\n\"");
		return;
	}

	if (zyk_account_change_blocked(ent, "register"))
	{
		return;
	}

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	if (trap->Argc() != 3)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^2Command Usage: /new <username> <password>\n\"");
		sqlite3_close(db);
		return;
	}

	if (ent->client->sess.loggedin == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You are already logged in to your account.\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^1You are already logged in to your account.\n\"");
		sqlite3_close(db);
		return;
	}

	trap->Argv(1, username, sizeof(username));
	trap->Argv(2, password, sizeof(password));

	Q_StripColor(username);
	Q_strlwr(username);

	if (strlen(username) > sizeof(ent->client->sess.filename) - 1) {
		trap->SendServerCommand(ent - g_entities, va("print \"^1Username can only have a maximum of %i characters.\n\"", (int)sizeof(ent->client->sess.filename) - 1));
		sqlite3_close(db);
		return;
	}

	{
		const char *reason = NULL;

		if (RP_PasswordIsValid(password, &reason) == qfalse) {
			trap->SendServerCommand(ent - g_entities, va("print \"%s\n\"", reason));
			sqlite3_close(db);
			return;
		}
	}

	if (zyk_check_user_input(username, strlen(username)) == qfalse) {
		trap->SendServerCommand(ent - g_entities, "print \"^1Username can only contain letters and numbers.\n\"");
		sqlite3_close(db);
		return;
	}

	if (select_number_of_accounts_with_username(ent, username, db, zErrMsg, rc, stmt) != 0) {
		trap->SendServerCommand(ent - g_entities, va("print \"^1Username ^7%s ^1is already in use.\n\"", username));
		trap->SendServerCommand(ent - g_entities, va("cp \"^1Username ^7%s ^1is already in use.\n\"", username));

		sqlite3_close(db);
		return;
	}

	if (insert_accounts_table_row(ent, username, password, db, zErrMsg, rc, stmt) == qfalse) {
		trap->SendServerCommand(ent - g_entities, "print \"^1Registration failed. Please try again in a moment.\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^1Registration failed. Please try again.\n\"");
		sqlite3_close(db);
		return;
	}

	if (select_accounts_table_row(ent, username, db, zErrMsg, rc, stmt) == qfalse) {
		trap->SendServerCommand(ent - g_entities, "print \"^1Registration failed: could not load the newly created account. Please contact an admin.\n\"");
		G_LogPrintf("WARNING: Cmd_Register_F could not re-select the Accounts row it just inserted for username '%s'.\n", username);
		sqlite3_close(db);
		return;
	}

	Q_strncpyz(ent->client->sess.filename, username, sizeof(ent->client->sess.filename));
	Q_strncpyz(ent->client->pers.password, password, sizeof(ent->client->pers.password));

	int newCharID = 0;

	if (insert_chars_table_row(ent, username, db, zErrMsg, rc, stmt, &newCharID) == qfalse) {
		trap->SendServerCommand(ent - g_entities, "print \"^1Registration failed: could not create your starting character. Please contact an admin.\n\"");
		G_LogPrintf("WARNING: Cmd_Register_F failed to insert the initial Characters row for username '%s' (AccountID %i); the account was created but has no character.\n", username, ent->client->sess.accountID);
		sqlite3_close(db);
		return;
	}

	if (insert_skills_table_row(ent, db, zErrMsg, rc, stmt, newCharID) == qfalse) {
		trap->SendServerCommand(ent - g_entities, "print \"^1Registration failed: could not create your starting skills. Please contact an admin.\n\"");
		G_LogPrintf("WARNING: Cmd_Register_F failed to insert the initial Skills row for username '%s' (AccountID %i); the account/character were created but have no skills row.\n", username, ent->client->sess.accountID);
		sqlite3_close(db);
		return;
	}

	if (insert_weapons_table_row(ent, db, zErrMsg, rc, stmt, newCharID) == qfalse) {
		trap->SendServerCommand(ent - g_entities, "print \"^1Registration failed: could not create your starting weapons. Please contact an admin.\n\"");
		G_LogPrintf("WARNING: Cmd_Register_F failed to insert the initial Weapons row for username '%s' (AccountID %i); the account/character were created but have no weapons row.\n", username, ent->client->sess.accountID);
		sqlite3_close(db);
		return;
	}

	ent->client->sess.amrpgmode = 2; //always 2, kept for backwards compatibility
	ent->client->sess.loggedin = qtrue;

	// GalaxyRP: [Force Enlightenment] see the matching comment in g_client.c -- keeps cgame's
	// CG_GreyItem in sync with login state right away.
	trap->SendServerCommand(ent->s.number, va("supdateloggedin %i\n", ent->client->sess.loggedin));

	select_player_character(ent, username, db, zErrMsg, rc, stmt, qtrue);

	trap->SendServerCommand(ent - g_entities, "print \"^2Your account has been successfully created and you are now logged in.\n\"");
	trap->SendServerCommand(ent - g_entities, "cp \"^2Your account has been successfully created and you are now logged in.\n\"");

	sqlite3_close(db);

	return;
}

void Cmd_Login_F(gentity_t * ent)
{
	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	sqlite3_stmt *stmt = 0;
	char username[256] = { 0 }, password[256] = { 0 }, comparisonUsername[256] = { 0 }, comparisonPassword[256] = { 0 }, defaultChar[256] = { 0 };

	if (G_PlayerIsDowned(ent))
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot do this while you are downed.\n\"");
		return;
	}

	if (zyk_account_change_blocked(ent, "login"))
	{
		return;
	}

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	if (trap->Argc() != 3)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^2Command Usage: /login <username> <password>\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^2Command Usage: /login <username> <password>\n\"");
		sqlite3_close(db);
		return;
	}

	if (ent->client->sess.loggedin == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You are already logged in to your account.\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^1You are already logged in to your account.\n\"");
		sqlite3_close(db);
		return;
	}

	trap->Argv(1, username, sizeof(username));
	trap->Argv(2, password, sizeof(password));

	Q_StripColor(username);
	Q_strlwr(username);

	if (select_number_of_accounts_with_username(ent, username, db, zErrMsg, rc, stmt) == 0)
	{
		//The account does not exist, thus, the error does.
		trap->SendServerCommand(ent - g_entities, va("print \"^1An account with the username %s does not exist.\n\"", username));
		trap->SendServerCommand(ent - g_entities, va("cp \"^1An account with the username %s does not exist.\n\"", username));

		sqlite3_close(db);
		return;
	}
	
	if (is_password_correct(ent, username, password, db, zErrMsg, rc, stmt) == qfalse) {
		trap->SendServerCommand(ent - g_entities, "print \"^1Incorrect password.\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^1Incorrect password.\n\"");

		sqlite3_close(db);
		return;
	}

	select_account_and_default_character_data(ent, username, db, zErrMsg, rc, stmt);

	if (ent->client->sess.loggedin == qfalse)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1Login failed: no character data could be found for this account. Please contact an admin.\n\"");
		trap->SendServerCommand(ent - g_entities, "cp \"^1Login failed: character data not found.\n\"");

		sqlite3_close(db);
		return;
	}

	zyk_stop_active_force_powers(ent);

	zyk_reset_force_powerups(ent);

	zyk_stop_active_holdables(ent);

	RP_ClearPhaseMode(ent);

	zyk_apply_character_skills(ent);

	zyk_apply_character_loadout(ent);

	zyk_schedule_relog_kill(ent);

	trap->SendServerCommand(ent - g_entities, "print \"^2You have sucessfully logged in.\n\"");
	trap->SendServerCommand(ent - g_entities, "cp \"^2You have sucessfully logged in.\n\"");

	sqlite3_close(db);

	Cmd_ZykChars_f(ent);
	Cmd_GalaxyRpUi_f(ent);

	return;
}

void Cmd_Char_f(gentity_t *ent) {
	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	sqlite3_stmt *stmt = 0;
	int argc = trap->Argc();
	char command[MAX_STRING_CHARS];
	char charName[MAX_STRING_CHARS];

	if (G_PlayerIsDowned(ent))
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot do this while you are downed.\n\"");
		return;
	}

	if (zyk_account_change_blocked(ent, "char"))
	{
		return;
	}

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	save_account(ent, qtrue);

	if (argc == 1)
	{
		select_character_list(ent, db, zErrMsg, rc, stmt);
		sqlite3_close(db);
		return;
	}
	if (argc == 3)
	{
		trap->Argv(1, command, sizeof(command));
		trap->Argv(2, charName, sizeof(charName));

		//Create New Character
		if (Q_stricmp(command, "new") == 0) {

			if (create_new_character(ent, charName, db, zErrMsg, rc, stmt)) {
				if (select_player_character(ent, charName, db, zErrMsg, rc, stmt, qfalse)) {
					trap->SendServerCommand(-1, va("chat \"%s created a new character and is now using: %s\n\"", ent->client->pers.netname, charName));
				}
				Cmd_GalaxyRpUi_f(ent);
			}
			sqlite3_close(db);
			Cmd_ZykChars_f(ent);
			return;
		}

		//Switch character
		if (Q_stricmp(command, "use") == 0) {

			if (Q_stricmp(charName, ent->client->sess.rpgchar) == 0) {
				trap->SendServerCommand(ent - g_entities, va("print \"^1You are already using character %s.\n\"", charName));
				sqlite3_close(db);
				return;
			}

			select_player_character(ent, charName, db, zErrMsg, rc, stmt, qtrue);
			sqlite3_close(db);

			Cmd_ZykChars_f(ent);
			Cmd_GalaxyRpUi_f(ent);

			return;

		}

		//Remove character
		if (Q_stricmp(command, "remove") == 0) {

			remove_character(ent, charName, db, zErrMsg, rc, stmt);
			sqlite3_close(db);
			Cmd_ZykChars_f(ent);
			return;
		}
	}

	sqlite3_close(db);
	trap->SendServerCommand(ent - g_entities, "print \"^2Command Usage: /char <new/use/remove> <character name>. Run with no arguments to list your characters.\n\"");
}

/*
==================
Cmd_Inventory_f
==================
*/
void Cmd_Inventory_f(gentity_t *ent) {
	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	sqlite3_stmt *stmt = 0;
	char username[256] = { 0 }, password[256] = { 0 }, comparisonUsername[256] = { 0 }, comparisonPassword[256] = { 0 }, defaultChar[256] = { 0 };

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	inventory_display_beginning(ent);
	inventory_display_items(ent, db, zErrMsg, rc, stmt);
	inventory_display_end(ent);
	sqlite3_close(db);
	return;
}

/*
==================
Cmd_CreateItem_f
==================
*/
void Cmd_CreateItem_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];

	if (!check_admin_command(ent, ADM_CREATEITEM, qtrue))
	{
		return;
	}

	if (trap->Argc() != 2) {
		trap->SendServerCommand(ent->s.number, "print \"Usage: /createitem <itemname>\nPut the name in double quotes if it has more than one word.\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	if (zyk_clean_text(arg1) == 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"^1Item names cannot be empty.\n\"");
		return;
	}

	if ((int)strlen(arg1) > MAX_ITEM_NAME_LENGTH)
	{
		trap->SendServerCommand(ent->s.number, va("print \"^1Item names are limited to %d characters.\n\"", MAX_ITEM_NAME_LENGTH));
		return;
	}

	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	sqlite3_stmt *stmt = 0;
	char username[256] = { 0 }, password[256] = { 0 }, comparisonUsername[256] = { 0 }, comparisonPassword[256] = { 0 }, defaultChar[256] = { 0 };

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	if (inventory_add_item(ent, arg1, db, zErrMsg, rc, stmt) == qtrue) {
		inventory_add_create_item_log(ent, arg1);
	}

	sqlite3_close(db);

	return;
}

/*
==================
Cmd_TrashItem_f
==================
*/
void Cmd_TrashItem_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];

	if (trap->Argc() != 2) {
		trap->SendServerCommand(ent->s.number, "print \"Usage: /trashitem <itemid>\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));
	int id_to_be_removed = atoi(arg1);

	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	sqlite3_stmt *stmt = 0;
	char username[256] = { 0 }, password[256] = { 0 }, comparisonUsername[256] = { 0 }, comparisonPassword[256] = { 0 }, defaultChar[256] = { 0 };

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	inventory_remove_item(ent, id_to_be_removed, db, zErrMsg, rc, stmt);

	sqlite3_close(db);

	return;
}

/*
==================
Cmd_GiveItem_f
==================
*/
void Cmd_GiveItem_f(gentity_t *ent) {
	char player_name[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];

	if (trap->Argc() != 3) {
		trap->SendServerCommand(ent->s.number, "print \"Usage: /giveitem <itemid> <playerName>\n\"");
		return;
	}
	trap->Argv(1, arg2, sizeof(arg2));
	trap->Argv(2, player_name, sizeof(player_name));

	int item_id = atoi(arg2);

	int player_id = ClientNumberFromString(ent, player_name, qfalse);

	//player not found, no point in going on
	if (player_id == -1) {
		return;
	}

	if (player_id == ent->s.number) {
		trap->SendServerCommand(ent->s.number, "print \"You cannot give an item to yourself.\n\"");
		return;
	}

	if (g_entities[player_id].client->sess.loggedin != qtrue) {
		trap->SendServerCommand(ent->s.number, "print \"That player is not logged into an account.\n\"");
		return;
	}

	if (Distance(ent->client->ps.origin, g_entities[player_id].client->ps.origin) > 1000) {
		trap->SendServerCommand(ent->s.number, "print \"You are too far away from that person.\n\"");
		return;
	}

	sqlite3 *db;
	char *zErrMsg = 0;
	int rc;
	sqlite3_stmt *stmt = 0;
	char username[256] = { 0 }, password[256] = { 0 }, comparisonUsername[256] = { 0 }, comparisonPassword[256] = { 0 }, defaultChar[256] = { 0 };

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	if (inventory_does_player_own_item(ent, item_id, db, zErrMsg, rc, stmt) == qfalse) {
		sqlite3_close(db);
		return;
	}

	int recipient_items = inventory_item_count(g_entities[player_id].client->pers.CharID, db, stmt);

	if (recipient_items < 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"^1Could not read that player's inventory. Item not transferred.\n\"");
		sqlite3_close(db);
		return;
	}

	if (recipient_items >= MAX_ITEMS_PER_CHARACTER)
	{
		trap->SendServerCommand(ent->s.number, va("print \"^1That player's inventory is full (%d items).\n\"", MAX_ITEMS_PER_CHARACTER));
		sqlite3_close(db);
		return;
	}

	char item_name[MAX_STRING_CHARS];

	if (inventory_item_name(item_id, ent->client->pers.CharID, item_name, sizeof(item_name), db, stmt) == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"^1The item could not be transferred. Nothing changed.\n\"");
		sqlite3_close(db);
		return;
	}

	if (inventory_transfer_item(ent, &g_entities[player_id], item_id, db, zErrMsg, rc, stmt) == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"^1The item could not be transferred. Nothing changed.\n\"");
		sqlite3_close(db);
		return;
	}

	trap->SendServerCommand(ent->s.number, va("print \"^2You gave ^3%s^2 to %s^2.\n\"", item_name, g_entities[player_id].client->pers.netname));
	trap->SendServerCommand(player_id, va("print \"%s^2 gave you ^3%s^2 (item %d). Type /inv to see your inventory.\n\"", ent->client->pers.netname, item_name, item_id));
	trap->SendServerCommand(player_id, va("cp \"^2You received an item from %s^2.\n\"", ent->client->pers.netname));

	sqlite3_close(db); // GalaxyRP fix: this success path never closed the connection it opened above.

	return;
}

void Cmd_KillOther_f( gentity_t *ent )
{
	int			i;
	char		otherindex[MAX_TOKEN_CHARS];
	gentity_t	*otherEnt = NULL;

	if (!check_admin_command(ent, ADM_KICK, qtrue))
	{
		return;
	}

	if (trap->Argc() < 2) {
		trap->SendServerCommand(ent - g_entities, "print \"Usage: killother <player id>\n\"");
		return;
	}

	trap->Argv(1, otherindex, sizeof(otherindex));
	i = ClientNumberFromString(ent, otherindex, qfalse);
	if (i == -1) {
		return;
	}

	otherEnt = &g_entities[i];
	if (!otherEnt->inuse || !otherEnt->client) {
		return;
	}

	if ((otherEnt->health <= 0 || otherEnt->client->tempSpectate >= level.time || otherEnt->client->sess.sessionTeam == TEAM_SPECTATOR))
	{
		// Intentionally displaying for the command user
		trap->SendServerCommand(ent - g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBEALIVE")));
		return;
	}

	RP_ClearDownedState( otherEnt );

	G_Kill(otherEnt);

	if (otherEnt->health < 1)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"Killed %s^7\n\"", otherEnt->client->pers.netname));
	}
}

/*
=================
BroadCastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange( gclient_t *client, int oldTeam )
{
	client->ps.fd.forceDoInit = 1; //every time we change teams make sure our force powers are set right

	if (level.gametype == GT_SIEGE)
	{ //don't announce these things in siege
		return;
	}

	if ( client->sess.sessionTeam == TEAM_RED ) {
		trap->SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
			client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEREDTEAM")) );
	} else if ( client->sess.sessionTeam == TEAM_BLUE ) {
		trap->SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
		client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBLUETEAM")));
	} else if ( client->sess.sessionTeam == TEAM_SPECTATOR && oldTeam != TEAM_SPECTATOR ) {
		trap->SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
		client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHESPECTATORS")));
	} else if ( client->sess.sessionTeam == TEAM_FREE ) {
		trap->SendServerCommand( -1, va("cp \"%s" S_COLOR_WHITE " %s\n\"",
		client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBATTLE")));
	}

	G_LogPrintf( "ChangeTeam: %i [%s] (%s) \"%s^7\" %s -> %s\n", (int)(client - level.clients), client->sess.IP, client->pers.guid, client->pers.netname, TeamName( oldTeam ), TeamName( client->sess.sessionTeam ) );
}

qboolean G_PowerDuelCheckFail(gentity_t *ent)
{
	int			loners = 0;
	int			doubles = 0;

	if (!ent->client || ent->client->sess.duelTeam == DUELTEAM_FREE)
	{
		return qtrue;
	}

	G_PowerDuelCount(&loners, &doubles, qfalse);

	if (ent->client->sess.duelTeam == DUELTEAM_LONE && loners >= 1)
	{
		return qtrue;
	}

	if (ent->client->sess.duelTeam == DUELTEAM_DOUBLE && doubles >= 2)
	{
		return qtrue;
	}

	return qfalse;
}

/*
=================
SetTeam
=================
*/
qboolean g_dontPenalizeTeam = qfalse;
qboolean g_preventTeamBegin = qfalse;
void SetTeam( gentity_t *ent, char *s ) {
	int					team, oldTeam;
	gclient_t			*client;
	int					clientNum;
	spectatorState_t	specState;
	int					specClient;
	int					teamLeader;

	// fix: this prevents rare creation of invalid players
	if (!ent->inuse)
	{
		return;
	}

	//
	// see what change is requested
	//
	client = ent->client;

	clientNum = client - level.clients;
	specClient = 0;
	specState = SPECTATOR_NOT;
	if ( !Q_stricmp( s, "scoreboard" ) || !Q_stricmp( s, "score" )  ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FREE; // SPECTATOR_SCOREBOARD disabling this for now since it is totally broken on client side
	} else if ( !Q_stricmp( s, "follow1" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -1;
	} else if ( !Q_stricmp( s, "follow2" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FOLLOW;
		specClient = -2;
	} else if ( !Q_stricmp( s, "spectator" ) || !Q_stricmp( s, "s" ) ) {
		team = TEAM_SPECTATOR;
		specState = SPECTATOR_FREE;
	} else if ( level.gametype >= GT_TEAM ) {
		// if running a team game, assign player to one of the teams
		specState = SPECTATOR_NOT;
		if ( !Q_stricmp( s, "red" ) || !Q_stricmp( s, "r" ) ) {
			team = TEAM_RED;
		} else if ( !Q_stricmp( s, "blue" ) || !Q_stricmp( s, "b" ) ) {
			team = TEAM_BLUE;
		} else {
			// pick the team with the least number of players
			//For now, don't do this. The legalize function will set powers properly now.
			/*
			if (g_forceBasedTeams.integer)
			{
				if (ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE)
				{
					team = TEAM_BLUE;
				}
				else
				{
					team = TEAM_RED;
				}
			}
			else
			{
			*/
				team = PickTeam( clientNum );
			//}
		}

		if ( g_teamForceBalance.integer && !g_jediVmerc.integer ) {
			int		counts[TEAM_NUM_TEAMS];

			//JAC: Invalid clientNum was being used
			counts[TEAM_BLUE] = TeamCount( ent-g_entities, TEAM_BLUE );
			counts[TEAM_RED] = TeamCount( ent-g_entities, TEAM_RED );

			// We allow a spread of two
			if ( team == TEAM_RED && counts[TEAM_RED] - counts[TEAM_BLUE] > 1 ) {
				//For now, don't do this. The legalize function will set powers properly now.
				/*
				if (g_forceBasedTeams.integer && ent->client->ps.fd.forceSide == FORCE_DARKSIDE)
				{
					// GalaxyRP fix: [Chat] ent-g_entities, matching the //JAC "Invalid clientNum was
					// being used" correction already applied to the sibling branch below -- this
					// force-based-teams branch was missed by it. SetTeam is reachable from a following
					// spectator running /team, whose ps.clientNum is the followed player's, so the
					// rejection message went to the wrong person entirely.
					trap->SendServerCommand( ent-g_entities,
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYRED_SWITCH")) );
				}
				else
				*/
				{
					//JAC: Invalid clientNum was being used
					trap->SendServerCommand( ent-g_entities,
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYRED")) );
				}
				return; // ignore the request
			}
			if ( team == TEAM_BLUE && counts[TEAM_BLUE] - counts[TEAM_RED] > 1 ) {
				//For now, don't do this. The legalize function will set powers properly now.
				/*
				if (g_forceBasedTeams.integer && ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE)
				{
					// GalaxyRP fix: [Chat] ent-g_entities, matching the //JAC "Invalid clientNum was
					// being used" correction already applied to the sibling branch below -- this
					// force-based-teams branch was missed by it. SetTeam is reachable from a following
					// spectator running /team, whose ps.clientNum is the followed player's, so the
					// rejection message went to the wrong person entirely.
					trap->SendServerCommand( ent-g_entities,
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYBLUE_SWITCH")) );
				}
				else
				*/
				{
					//JAC: Invalid clientNum was being used
					trap->SendServerCommand( ent-g_entities,
						va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYBLUE")) );
				}
				return; // ignore the request
			}

			// It's ok, the team we are switching to has less or same number of players
		}

		//For now, don't do this. The legalize function will set powers properly now.
		/*
		if (g_forceBasedTeams.integer)
		{
			if (team == TEAM_BLUE && ent->client->ps.fd.forceSide != FORCE_LIGHTSIDE)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBELIGHT")) );
				return;
			}
			if (team == TEAM_RED && ent->client->ps.fd.forceSide != FORCE_DARKSIDE)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBEDARK")) );
				return;
			}
		}
		*/

	} else {
		// force them to spectators if there aren't any spots free
		team = TEAM_FREE;
	}

	oldTeam = client->sess.sessionTeam;

	if (level.gametype == GT_SIEGE)
	{
		if (client->tempSpectate >= level.time &&
			team == TEAM_SPECTATOR)
		{ //sorry, can't do that.
			return;
		}

		if ( team == oldTeam && team != TEAM_SPECTATOR )
			return;

		client->sess.siegeDesiredTeam = team;
		//oh well, just let them go.
		/*
		if (team != TEAM_SPECTATOR)
		{ //can't switch to anything in siege unless you want to switch to being a fulltime spectator
			//fill them in on their objectives for this team now
			trap->SendServerCommand(ent-g_entities, va("sb %i", client->sess.siegeDesiredTeam));

			trap->SendServerCommand( ent-g_entities, va("print \"You will be on the selected team the next time the round begins.\n\"") );
			return;
		}
		*/
		if (client->sess.sessionTeam != TEAM_SPECTATOR &&
			team != TEAM_SPECTATOR)
		{ //not a spectator now, and not switching to spec, so you have to wait til you die.
			//trap->SendServerCommand( ent-g_entities, va("print \"You will be on the selected team the next time you respawn.\n\"") );
			qboolean doBegin;
			if (ent->client->tempSpectate >= level.time)
			{
				doBegin = qfalse;
			}
			else
			{
				doBegin = qtrue;
			}

			if (doBegin)
			{
				// Kill them so they automatically respawn in the team they wanted.
				if (ent->health > 0)
				{
					ent->flags &= ~FL_GODMODE;
					ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
					// GalaxyRP fix: [Death System] bookkeeping, not a death -- see
					// g_bookkeepingDeath in g_local.h.
					g_bookkeepingDeath = qtrue;
					player_die( ent, ent, ent, 100000, MOD_TEAM_CHANGE );
					g_bookkeepingDeath = qfalse;
				}
			}

			if (ent->client->sess.sessionTeam != ent->client->sess.siegeDesiredTeam)
			{
				SetTeamQuick(ent, ent->client->sess.siegeDesiredTeam, qfalse);
			}

			return;
		}
	}

	// override decision if limiting the players
	if ( (level.gametype == GT_DUEL)
		&& level.numNonSpectatorClients >= 2 )
	{
		team = TEAM_SPECTATOR;
	}
	else if ( (level.gametype == GT_POWERDUEL)
		&& (level.numPlayingClients >= 3 || G_PowerDuelCheckFail(ent)) )
	{
		team = TEAM_SPECTATOR;
	}
	else if ( g_maxGameClients.integer > 0 &&
		level.numNonSpectatorClients >= g_maxGameClients.integer )
	{
		team = TEAM_SPECTATOR;
	}

	//
	// decide if we will allow the change
	//
	if ( team == oldTeam && team != TEAM_SPECTATOR ) {
		return;
	}

	//
	// execute the team change
	//

	zyk_release_player_npcs( ent );

	//If it's siege then show the mission briefing for the team you just joined.
//	if (level.gametype == GT_SIEGE && team != TEAM_SPECTATOR)
//	{
//		trap->SendServerCommand(clientNum, va("sb %i", team));
//	}

	// if the player was dead leave the body
	if ( client->ps.stats[STAT_HEALTH] <= 0 && client->sess.sessionTeam != TEAM_SPECTATOR ) {
		MaintainBodyQueue(ent);
	}

	// he starts at 'base'
	client->pers.teamState.state = TEAM_BEGIN;
	if ( oldTeam != TEAM_SPECTATOR ) {
		// Kill him (makes sure he loses flags, etc)
		ent->flags &= ~FL_GODMODE;
		ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
		// GalaxyRP fix: [Death System] bookkeeping, not a death -- see g_bookkeepingDeath in
		// g_local.h. Leaving a team should not cost a scoreboard death, and under rp_loginRequired
		// this is the path every /logout takes on its way to Spectator.
		g_dontPenalizeTeam = qtrue;
		g_bookkeepingDeath = qtrue;
		player_die (ent, ent, ent, 100000, MOD_SUICIDE);
		g_bookkeepingDeath = qfalse;
		g_dontPenalizeTeam = qfalse;

	}
	// they go to the end of the line for tournaments
	if ( team == TEAM_SPECTATOR && oldTeam != team )
		AddTournamentQueue( client );

	// clear votes if going to spectator (specs can't vote)
	if ( team == TEAM_SPECTATOR )
		G_ClearVote( ent );
	// also clear team votes if switching red/blue or going to spec
	G_ClearTeamVote( ent, oldTeam );

	client->sess.sessionTeam = (team_t)team;
	client->sess.spectatorState = specState;
	client->sess.spectatorClient = specClient;

	client->sess.teamLeader = qfalse;
	if ( team == TEAM_RED || team == TEAM_BLUE ) {
		teamLeader = TeamLeader( team );
		// if there is no team leader or the team leader is a bot and this client is not a bot
		if ( teamLeader == -1 || ( !(g_entities[clientNum].r.svFlags & SVF_BOT) && (g_entities[teamLeader].r.svFlags & SVF_BOT) ) ) {
			//SetLeader( team, clientNum );
		}
	}
	// make sure there is a team leader on the team the player came from
	if ( oldTeam == TEAM_RED || oldTeam == TEAM_BLUE ) {
		CheckTeamLeader( oldTeam );
	}

	BroadcastTeamChange( client, oldTeam );

	//make a disappearing effect where they were before teleporting them to the appropriate spawn point,
	//if we were not on the spec team
	if (oldTeam != TEAM_SPECTATOR)
	{
		gentity_t *tent = G_TempEntity( client->ps.origin, EV_PLAYER_TELEPORT_OUT );
		tent->s.clientNum = clientNum;
	}

	// get and distribute relevent paramters
	if ( !ClientUserinfoChanged( clientNum ) )
		return;

	if (!g_preventTeamBegin && level.load_entities_timer == 0)
	{ // zyk: do not call this while entities are being placed in map
		ClientBegin( clientNum, qfalse );
	}
}

/*
=================
StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
extern void G_LeaveVehicle( gentity_t *ent, qboolean ConCheck );
void StopFollowing( gentity_t *ent ) {
	int i=0;
	ent->client->ps.persistant[ PERS_TEAM ] = TEAM_SPECTATOR;
	ent->client->sess.sessionTeam = TEAM_SPECTATOR;
	ent->client->sess.spectatorState = SPECTATOR_FREE;
	ent->client->ps.pm_flags &= ~PMF_FOLLOW;
	ent->r.svFlags &= ~SVF_BOT;
	ent->client->ps.clientNum = ent - g_entities;
	ent->client->ps.weapon = WP_NONE;
	G_LeaveVehicle( ent, qfalse ); // clears m_iVehicleNum as well
	ent->client->ps.emplacedIndex = 0;
	//ent->client->ps.m_iVehicleNum = 0;
	ent->client->ps.viewangles[ROLL] = 0.0f;
	ent->client->ps.forceHandExtend = HANDEXTEND_NONE;
	ent->client->ps.forceHandExtendTime = 0;
	ent->client->ps.zoomMode = 0;
	ent->client->ps.zoomLocked = qfalse;
	ent->client->ps.zoomLockTime = 0;
	ent->client->ps.saberMove = LS_NONE;
	ent->client->ps.legsAnim = 0;
	ent->client->ps.legsTimer = 0;
	ent->client->ps.torsoAnim = 0;
	ent->client->ps.torsoTimer = 0;
	ent->client->ps.isJediMaster = qfalse; // major exploit if you are spectating somebody and they are JM and you reconnect
	ent->client->ps.cloakFuel = 100; // so that fuel goes away after stop following them
	ent->client->ps.jetpackFuel = 100; // so that fuel goes away after stop following them
	ent->health = ent->client->ps.stats[STAT_HEALTH] = 100; // so that you don't keep dead angles if you were spectating a dead person
	ent->client->ps.bobCycle = 0;
	ent->client->ps.pm_type = PM_SPECTATOR;
	ent->client->ps.eFlags &= ~EF_DISINTEGRATION;
	for ( i=0; i<PW_NUM_POWERUPS; i++ )
		ent->client->ps.powerups[i] = 0;
}

/*
=================
Cmd_Team_f
=================
*/
void Cmd_Team_f( gentity_t *ent ) {
	int			oldTeam;
	char		s[MAX_TOKEN_CHARS];

	oldTeam = ent->client->sess.sessionTeam;

	if ( trap->Argc() != 2 ) {
		switch ( oldTeam ) {
		case TEAM_BLUE:
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTBLUETEAM")) );
			break;
		case TEAM_RED:
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTREDTEAM")) );
			break;
		case TEAM_FREE:
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTFREETEAM")) );
			break;
		case TEAM_SPECTATOR:
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PRINTSPECTEAM")) );
			break;
		}
		return;
	}

	if (G_PlayerIsDowned(ent) && ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot do this while you are downed.\n\"");
		return;
	}

	if ( ent->client->switchTeamTime > level.time ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	if (gEscaping)
	{
		return;
	}

	// if they are playing a tournament game, count as a loss
	if ( level.gametype == GT_DUEL
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {//in a tournament game
		//disallow changing teams
		trap->SendServerCommand( ent-g_entities, "print \"Cannot switch teams in Duel\n\"" );
		return;
		//FIXME: why should this be a loss???
		//ent->client->sess.losses++;
	}

	if (level.gametype == GT_POWERDUEL)
	{ //don't let clients change teams manually at all in powerduel, it will be taken care of through automated stuff
		trap->SendServerCommand( ent-g_entities, "print \"Cannot switch teams in Power Duel\n\"" );
		return;
	}

	// Tr!Force: [Plugin] Don't allow
	if (rp_pluginRequired.integer == 2 && !ent->client->pers.clientPlugin)
	{
		ClientBegin(ent->s.number, qfalse);
		return;
	}
	else
	{
		trap->Argv( 1, s, sizeof( s ) );
	}

	SetTeam( ent, s );

	// fix: update team switch time only if team change really happend
	if (oldTeam != ent->client->sess.sessionTeam)
		ent->client->switchTeamTime = level.time + 5000;
}

/*
=================
Cmd_DuelTeam_f
=================
*/
void Cmd_DuelTeam_f(gentity_t *ent)
{
	int			oldTeam;
	char		s[MAX_TOKEN_CHARS];

	if (level.gametype != GT_POWERDUEL)
	{ //don't bother doing anything if this is not power duel
		return;
	}

	/*
	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You cannot change your duel team unless you are a spectator.\n\""));
		return;
	}
	*/

	if ( trap->Argc() != 2 )
	{ //No arg so tell what team we're currently on.
		oldTeam = ent->client->sess.duelTeam;
		switch ( oldTeam )
		{
		case DUELTEAM_FREE:
			trap->SendServerCommand( ent-g_entities, va("print \"None\n\"") );
			break;
		case DUELTEAM_LONE:
			trap->SendServerCommand( ent-g_entities, va("print \"Single\n\"") );
			break;
		case DUELTEAM_DOUBLE:
			trap->SendServerCommand( ent-g_entities, va("print \"Double\n\"") );
			break;
		default:
			break;
		}
		return;
	}

	if ( ent->client->switchDuelTeamTime > level.time )
	{ //debounce for changing
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	trap->Argv( 1, s, sizeof( s ) );

	oldTeam = ent->client->sess.duelTeam;

	if (!Q_stricmp(s, "free"))
	{
		ent->client->sess.duelTeam = DUELTEAM_FREE;
	}
	else if (!Q_stricmp(s, "single"))
	{
		ent->client->sess.duelTeam = DUELTEAM_LONE;
	}
	else if (!Q_stricmp(s, "double"))
	{
		ent->client->sess.duelTeam = DUELTEAM_DOUBLE;
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"'%s' not a valid duel team.\n\"", s) );
	}

	if (oldTeam == ent->client->sess.duelTeam)
	{ //didn't actually change, so don't care.
		return;
	}

	if (ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{ //ok..die
		int curTeam = ent->client->sess.duelTeam;
		ent->client->sess.duelTeam = oldTeam;
		G_Damage(ent, ent, ent, NULL, ent->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
		ent->client->sess.duelTeam = curTeam;
	}
	//reset wins and losses
	ent->client->sess.wins = 0;
	ent->client->sess.losses = 0;

	//get and distribute relevent paramters
	if ( ClientUserinfoChanged( ent->s.number ) )
		return;

	ent->client->switchDuelTeamTime = level.time + 5000;
}

int G_TeamForSiegeClass(const char *clName)
{
	int i = 0;
	int team = SIEGETEAM_TEAM1;
	siegeTeam_t *stm = BG_SiegeFindThemeForTeam(team);
	siegeClass_t *scl;

	if (!stm)
	{
		return 0;
	}

	while (team <= SIEGETEAM_TEAM2)
	{
		scl = stm->classes[i];

		if (scl && scl->name[0])
		{
			if (!Q_stricmp(clName, scl->name))
			{
				return team;
			}
		}

		i++;
		if (i >= MAX_SIEGE_CLASSES || i >= stm->numClasses)
		{
			if (team == SIEGETEAM_TEAM2)
			{
				break;
			}
			team = SIEGETEAM_TEAM2;
			stm = BG_SiegeFindThemeForTeam(team);
			i = 0;
		}
	}

	return 0;
}

/*
=================
Cmd_SiegeClass_f
=================
*/
void Cmd_SiegeClass_f( gentity_t *ent )
{
	char className[64];
	int team = 0;
	int preScore;
	qboolean startedAsSpec = qfalse;

	if (level.gametype != GT_SIEGE)
	{ //classes are only valid for this gametype
		return;
	}

	if (!ent->client)
	{
		return;
	}

	if (trap->Argc() < 1)
	{
		return;
	}

	if ( ent->client->switchClassTime > level.time )
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCLASSSWITCH")) );
		return;
	}

	// GalaxyRP fix: [Death System] the third player-driven route into SetTeam() -- a class change
	// can switch team and then respawns with ClientBegin(). Same refusal as /team and /follow.
	if (G_PlayerIsDowned(ent) && ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot do this while you are downed.\n\"");
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		startedAsSpec = qtrue;
	}

	trap->Argv( 1, className, sizeof( className ) );

	team = G_TeamForSiegeClass(className);

	if (!team)
	{ //not a valid class name
		return;
	}

	if (ent->client->sess.sessionTeam != team)
	{ //try changing it then
		g_preventTeamBegin = qtrue;
		if (team == TEAM_RED)
		{
			SetTeam(ent, "red");
		}
		else if (team == TEAM_BLUE)
		{
			SetTeam(ent, "blue");
		}
		g_preventTeamBegin = qfalse;

		if (ent->client->sess.sessionTeam != team)
		{ //failed, oh well
			if (ent->client->sess.sessionTeam != TEAM_SPECTATOR ||
				ent->client->sess.siegeDesiredTeam != team)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOCLASSTEAM")) );
				return;
			}
		}
	}

	//preserve 'is score
	preScore = ent->client->ps.persistant[PERS_SCORE];

	//Make sure the class is valid for the team
	BG_SiegeCheckClassLegality(team, className);

	//Set the session data
	strcpy(ent->client->sess.siegeClass, className);

	// get and distribute relevent paramters
	if ( !ClientUserinfoChanged( ent->s.number ) )
		return;

	if (ent->client->tempSpectate < level.time)
	{
		// Kill him (makes sure he loses flags, etc)
		if (ent->health > 0 && !startedAsSpec)
		{
			ent->flags &= ~FL_GODMODE;
			ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
			// GalaxyRP fix: [Death System] bookkeeping, not a death -- see g_bookkeepingDeath in
			// g_local.h.
			g_bookkeepingDeath = qtrue;
			player_die (ent, ent, ent, 100000, MOD_SUICIDE);
			g_bookkeepingDeath = qfalse;
		}

		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR || startedAsSpec)
		{ //respawn them instantly.
			ClientBegin( ent->s.number, qfalse );
		}
	}
	//set it back after we do all the stuff
	ent->client->ps.persistant[PERS_SCORE] = preScore;

	ent->client->switchClassTime = level.time + 5000;
}

/*
=================
Cmd_ForceChanged_f
=================
*/
void Cmd_ForceChanged_f( gentity_t *ent )
{
	char fpChStr[1024];
	const char *buf;
//	Cmd_Kill_f(ent);
	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{ //if it's a spec, just make the changes now
		//trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "FORCEAPPLIED")) );
		//No longer print it, as the UI calls this a lot.
		WP_InitForcePowers( ent );
		goto argCheck;
	}

	buf = G_GetStringEdString("MP_SVGAME", "FORCEPOWERCHANGED");

	strcpy(fpChStr, buf);

	trap->SendServerCommand( ent-g_entities, va("print \"%s%s\n\"", S_COLOR_GREEN, fpChStr) );

	ent->client->ps.fd.forceDoInit = 1;
argCheck:
	if (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
	{ //If this is duel, don't even bother changing team in relation to this.
		return;
	}

	if (trap->Argc() > 1)
	{
		char	arg[MAX_TOKEN_CHARS];

		trap->Argv( 1, arg, sizeof( arg ) );

		if ( arg[0] )
		{ //if there's an arg, assume it's a combo team command from the UI.
			Cmd_Team_f(ent);
		}
	}
}

extern qboolean duel_tournament_is_duelist(gentity_t *ent);

static qboolean force_switch_allowed(gentity_t* ent)
{
	if (ent->client->sess.loggedin == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"This command is only available to players who are not logged into an account.\n\"");
		return qfalse;
	}

	if (ent->client->ps.duelInProgress == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot use this command in private duels.\n\"");
		return qfalse;
	}

	if ((level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1) ||
		(level.duel_tournament_mode == 4 && duel_tournament_is_duelist(ent) == qtrue))
	{
		if (level.duel_tournament_mode == 1)
			trap->SendServerCommand(ent - g_entities, "print \"Cannot use this command while signed up for the Duel Tournament. Leave it first with /duelmode.\n\"");
		else
			trap->SendServerCommand(ent - g_entities, "print \"Cannot use this command while taking part in the Duel Tournament.\n\"");
		return qfalse;
	}

	if (level.melee_mode > 0 && level.melee_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot use this command while taking part in the Melee Battle. Leave it first with /meleemode.\n\"");
		return qfalse;
	}

	if (ent->client->pers.player_statuses & ((1 << PLAYER_STATUS_ADM_GIVE_FORCE) | (1 << PLAYER_STATUS_ADM_GIVE_GUNS)))
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot use this command while you have force powers or weapons given by an admin.\n\"");
		return qfalse;
	}

	return qtrue;
}

void zyk_deselect_weapon_if_active(gentity_t* ent, int weapon);

/*
==================
Cmd_UpdateForce_f

GalaxyRP: [Force] "/updateforce" -- takes no arguments. Re-applies whatever force power
allocation the player already picked in the in-game force power menu immediately, without needing
a /kill or death to force a respawn.
==================
*/
void Cmd_UpdateForce_f( gentity_t *ent ) {
	if (!force_switch_allowed(ent))
		return;

	WP_InitForcePowers(ent);
	ent->client->ps.fd.forceDoInit = 0;

	if (level.gametype != GT_SIEGE && level.gametype != GT_JEDIMASTER && level.gametype != GT_HOLOCRON &&
		!(level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1) &&
		!(level.melee_mode > 0 && level.melee_players[ent->s.number] != -1))
	{
		if (ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] > FORCE_LEVEL_0)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);
		}
		else
		{
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);
			zyk_deselect_weapon_if_active(ent, WP_SABER);
		}
	}

	// GalaxyRP fix: [Saber] and the style, which WP_InitForcePowers() does not hold to the new level
	zyk_reset_saber_style(ent);
}

extern qboolean WP_SaberStyleValidForSaber( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int saberAnimLevel );
extern qboolean WP_UseFirstValidSaberStyle( saberInfo_t *saber1, saberInfo_t *saber2, int saberHolstered, int *saberAnimLevel );
qboolean G_SetSaber(gentity_t *ent, int saberNum, char *saberName, qboolean siegeOverride)
{
	char truncSaberName[MAX_QPATH] = {0};

	if ( !siegeOverride && level.gametype == GT_SIEGE && ent->client->siegeClass != -1 &&
		(bgSiegeClasses[ent->client->siegeClass].saberStance || bgSiegeClasses[ent->client->siegeClass].saber1[0] || bgSiegeClasses[ent->client->siegeClass].saber2[0]) )
	{ //don't let it be changed if the siege class has forced any saber-related things
		return qfalse;
	}

	Q_strncpyz( truncSaberName, saberName, sizeof( truncSaberName ) );

	if ( saberNum == 0 && (!Q_stricmp( "none", truncSaberName ) || !Q_stricmp( "remove", truncSaberName )) )
	{ //can't remove saber 0 like this
		Q_strncpyz( truncSaberName, DEFAULT_SABER, sizeof( truncSaberName ) );
	}

	//Set the saber with the arg given. If the arg is
	//not a valid sabername defaults will be used.
	WP_SetSaber( ent->s.number, ent->client->saber, saberNum, truncSaberName );

	if ( !ent->client->saber[0].model[0] )
	{
		assert(0); //should never happen!
		Q_strncpyz( ent->client->pers.saber1, DEFAULT_SABER, sizeof( ent->client->pers.saber1 ) );
	}
	else
		Q_strncpyz( ent->client->pers.saber1, ent->client->saber[0].name, sizeof( ent->client->pers.saber1 ) );

	if ( !ent->client->saber[1].model[0] )
		Q_strncpyz( ent->client->pers.saber2, "none", sizeof( ent->client->pers.saber2 ) );
	else
		Q_strncpyz( ent->client->pers.saber2, ent->client->saber[1].name, sizeof( ent->client->pers.saber2 ) );

	if ( !WP_SaberStyleValidForSaber( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, ent->client->ps.fd.saberAnimLevel ) )
	{
		WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &ent->client->ps.fd.saberAnimLevel );
		ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = ent->client->ps.fd.saberAnimLevel;
	}

	return qtrue;
}

/*
=================
Cmd_Follow_f
=================
*/
void Cmd_Follow_f( gentity_t *ent ) {
	int		i;
	char	arg[MAX_TOKEN_CHARS];

	if (G_PlayerIsDowned(ent) && ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot do this while you are downed.\n\"");
		return;
	}

	if ( ent->client->sess.spectatorState == SPECTATOR_NOT && ent->client->switchTeamTime > level.time ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	if ( trap->Argc() != 2 ) {
		if ( ent->client->sess.spectatorState == SPECTATOR_FOLLOW ) {
			StopFollowing( ent );
		}
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );
	i = ClientNumberFromString( ent, arg, qfalse );
	if ( i == -1 ) {
		return;
	}

	// can't follow self
	if ( &level.clients[ i ] == ent->client ) {
		return;
	}

	// can't follow another spectator
	if ( level.clients[ i ].sess.sessionTeam == TEAM_SPECTATOR ) {
		return;
	}

	if ( level.clients[ i ].tempSpectate >= level.time ) {
		return;
	}

	// if they are playing a tournament game, count as a loss
	if ( (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {
		//WTF???
		ent->client->sess.losses++;
	}

	// first set them to spectator
	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		SetTeam( ent, "spectator" );
		// fix: update team switch time only if team change really happend
		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
			ent->client->switchTeamTime = level.time + 5000;
	}

	ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
	ent->client->sess.spectatorClient = i;
}

/*
=================
Cmd_FollowCycle_f
=================
*/
void Cmd_FollowCycle_f( gentity_t *ent, int dir ) {
	int		clientnum;
	int		original;
	qboolean	looped = qfalse;

	if (G_PlayerIsDowned(ent) && ent->client->sess.sessionTeam != TEAM_SPECTATOR)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot do this while you are downed.\n\"");
		return;
	}

	if ( ent->client->sess.spectatorState == SPECTATOR_NOT && ent->client->switchTeamTime > level.time ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")) );
		return;
	}

	// Tr!Force: [Plugin] Don't allow
	if (rp_pluginRequired.integer == 2 && !ent->client->pers.clientPlugin) {
		return;
	}

	// if they are playing a tournament game, count as a loss
	if ( (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
		&& ent->client->sess.sessionTeam == TEAM_FREE ) {\
		//WTF???
		ent->client->sess.losses++;
	}
	// first set them to spectator
	if ( ent->client->sess.spectatorState == SPECTATOR_NOT ) {
		SetTeam( ent, "spectator" );
		// fix: update team switch time only if team change really happend
		if (ent->client->sess.sessionTeam == TEAM_SPECTATOR)
			ent->client->switchTeamTime = level.time + 5000;
	}

	if ( dir != 1 && dir != -1 ) {
		trap->Error( ERR_DROP, "Cmd_FollowCycle_f: bad dir %i", dir );
	}

	clientnum = ent->client->sess.spectatorClient;
	original = clientnum;

	do {
		clientnum += dir;
		if ( clientnum >= level.maxclients )
		{
			// Avoid /team follow1 crash
			if ( looped )
			{
				clientnum = original;
				break;
			}
			else
			{
				clientnum = 0;
				looped = qtrue;
			}
		}
		if ( clientnum < 0 ) {
			if ( looped )
			{
				clientnum = original;
				break;
			}
			else
			{
				clientnum = level.maxclients - 1;
				looped = qtrue;
			}
		}

		// can only follow connected clients
		if ( level.clients[ clientnum ].pers.connected != CON_CONNECTED ) {
			continue;
		}

		// can't follow another spectator
		if ( level.clients[ clientnum ].sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}

		// can't follow another spectator
		if ( level.clients[ clientnum ].tempSpectate >= level.time ) {
			return;
		}

		// this is good, we can use it
		ent->client->sess.spectatorClient = clientnum;
		ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
		return;
	} while ( clientnum != original );

	// leave it where it was
}

void Cmd_FollowNext_f( gentity_t *ent ) {
	Cmd_FollowCycle_f( ent, 1 );
}

void Cmd_FollowPrev_f( gentity_t *ent ) {
	Cmd_FollowCycle_f( ent, -1 );
}

extern void save_account(gentity_t *ent, qboolean save_char_file);

/*
==================
G_Say
==================
*/

/*
==================
check_if_player_ignores_other

GalaxyRP fix: [Chat] true when 'other' has /ignore'd 'ent'.
==================
*/
static qboolean check_if_player_ignores_other( gentity_t *ent, gentity_t *other )
{
	if ( !ent || !other || !other->client )
		return qfalse;

	if ( ent->s.number < MAX_CLIENTS - 1)
		return (level.ignored_players[other->s.number][0] & (1 << ent->s.number)) ? qtrue : qfalse;

	return (level.ignored_players[other->s.number][1] & (1 << (ent->s.number - 31))) ? qtrue : qfalse;
}

static qboolean zyk_ignores_chat_distance( const gentity_t *other )
{
	if ( !other || !other->client )
		return qfalse;

	return ( (other->client->pers.bitvalue & (1 << ADM_IGNORECHATDISTANCE)) &&
		!(other->client->pers.player_settings & (1 << 16)) ) ? qtrue : qfalse;
}

static void G_SayTo( gentity_t *ent, gentity_t *other, int mode, int color, const char *name, const char *message, char *locMsg )
{
	if (!other) {
		return;
	}
	if (!other->inuse) {
		return;
	}
	if (!other->client) {
		return;
	}
	if ( other->client->pers.connected != CON_CONNECTED ) {
		return;
	}
	if ( mode == SAY_TEAM  && !OnSameTeam(ent, other) ) {
		return;
	}
	if ( mode == SAY_ALLY && ent != other && zyk_is_ally(ent, other) == qfalse) { // zyk: allychat. Send it only to allies and to the player himself
		return;
	}
	/*
	// no chatting to players in tournaments
	if ( (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
		&& other->client->sess.sessionTeam == TEAM_FREE
		&& ent->client->sess.sessionTeam != TEAM_FREE ) {
		//Hmm, maybe some option to do so if allowed?  Or at least in developer mode...
		return;
	}
	*/
	//They've requested I take this out.

	if (level.gametype == GT_SIEGE &&
		ent->client && (ent->client->tempSpectate >= level.time || ent->client->sess.sessionTeam == TEAM_SPECTATOR) &&
		other->client->sess.sessionTeam != TEAM_SPECTATOR &&
		other->client->tempSpectate < level.time)
	{ //siege temp spectators should not communicate to ingame players
		return;
	}

	// zyk: if player is ignored, then he cant say anything to the target player
	if ( check_if_player_ignores_other( ent, other ) )
	{
		return;
	}

	if (locMsg)
	{
		trap->SendServerCommand( other-g_entities, va("%s \"%s\" \"%s\" \"%c\" \"%s\" %i",
			mode == SAY_TEAM ? "ltchat" : "lchat",
			name, locMsg, color, message, ent->s.number));
	}
	else
	{
		trap->SendServerCommand( other-g_entities, va("%s \"%s%c%c%s\" %i",
			mode == SAY_TEAM ? "tchat" : "chat",
			name, Q_COLOR_ESCAPE, color, message, ent->s.number));
	}
}

static qboolean is_character_not_a_word_delimiter( char c )
{
	unsigned char u = (unsigned char)c;

	return ( (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') ||
	         (u >= '0' && u <= '9') || u == '\'' || u == '_' || u >= 0x80 ) ? qtrue : qfalse;
}

void delete_chat_command(char *original_text, int no_of_chars) {
	char text[MAX_SAY_TEXT] = "";
	for (int i = no_of_chars; i < strlen(original_text); i++) {
		strncat(text, &original_text[i], 1);
	}
	strcpy(original_text, text);
}

void insertString(char text[], const char text_to_add[], int position, int buffer_size) {
	int text_length = strlen(text);
	int addition_length = strlen(text_to_add);

	if (position < 0 || position > text_length || text_length + addition_length >= buffer_size)
		return;

	for (int i = text_length; i >= position; --i)
		text[i + addition_length] = text[i];

	for (int i = 0; i < addition_length; ++i)
		text[position + i] = text_to_add[i];
}

void format_text_in_quotes(char* original_text, int buffer_size) {
	int text_length = strlen(original_text);
	// [Alex]: Counter for how many complete ''..'' spans are in the text.
	int quote_pairs = 0;

	for (int i = 0; i + 1 < text_length;) {
		// [Alex]: The game turns " into '' when sending chat messages, so look for that.
		if (original_text[i] == '\'' && original_text[i + 1] == '\'') {
			int closing_quote_index = i + 2;

			while (closing_quote_index + 1 < text_length &&
				(original_text[closing_quote_index] != '\'' || original_text[closing_quote_index + 1] != '\''))
				closing_quote_index++;

			if (closing_quote_index + 1 >= text_length)
				break;

			quote_pairs++;
			i = closing_quote_index + 2;
		} else {
			i++;
		}
	}

	// [Alex]: Check that adding the formatting codes won't exceed the buffer size.
	if (quote_pairs == 0 || text_length + quote_pairs * 4 >= buffer_size)
		return;

	for (int i = 0; i + 1 < text_length;) {
		if (original_text[i] == '\'' && original_text[i + 1] == '\'') {
			int closing_quote_index = i + 2;

			while (closing_quote_index + 1 < text_length &&
				(original_text[closing_quote_index] != '\'' || original_text[closing_quote_index + 1] != '\''))
				closing_quote_index++;

			if (closing_quote_index + 1 >= text_length)
				break;

			// [Alex] Defaults to white text after the quotes, if nothing's found. (hopefulyl shouldn't ever happen)
			char restore_color[3] = "^7";
			for (int color_index = 0; color_index + 1 < i;) {
				if (original_text[color_index] == '^' &&
					original_text[color_index + 1] >= '0' && original_text[color_index + 1] <= '9') {
					restore_color[1] = original_text[color_index + 1];
					color_index += 2;
				} else {
					color_index++;
				}
			}

			insertString(original_text, "^2", i + 2, buffer_size);
			insertString(original_text, restore_color, closing_quote_index + 2, buffer_size);
			text_length += 4;
			i = closing_quote_index + 6;
		} else {
			i++;
		}
	}
}

void G_Say( gentity_t *ent, gentity_t *target, int mode, const char *chatText ) {
	int			j;
	gentity_t	*other;
	int			color;
	char		name[64];
	// don't let text be too long for malicious reasons. Or let it be VERY long for RP purposes ;)
	char		text[MAX_SAY_TEXT];
	char		location[64];
	char		*locMsg = NULL;
	//distance for distance-based chat
	int distance = 999999999;
	//variable used for OOC chat (or team chat)
	int ooc_flag = 0;
	char ooc_text[700] = "";
	
	// [Alex]: SAY_OOC exists to set this flag. Then it's treated as SAY_ALL, but altered in OOC format.
	if ( mode == SAY_OOC ) {
		ooc_flag = 1;
		mode = SAY_ALL;
	}

	// Team chat is becomes OOC chat in non-team gametypes.
	if ( level.gametype < GT_TEAM && mode == SAY_TEAM ) {
		ooc_flag = 1;
		mode = SAY_ALL;
	}

	Q_strncpyz( text, chatText, sizeof(text) );

	Q_strstrip( text, "\n\r", "  " );

	switch ( mode ) {
	default:
	case SAY_ALL:
		// zyk: if player is silenced by an admin, he cannot say anything
		if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_SILENCED))
			return;

		//ooc chat case
		if (ooc_flag == 1) 
		{
			//add paranthesis for OOC chat (I know it's a workaround and it should be done better but it works)
			char beginning[] = "((";
			char end[] = "^1))";

			strcat(ooc_text, beginning);
			strcat(ooc_text, text);
			strcat(ooc_text, end);

			G_LogPrintf("ooc: %s: %s\n", ent->client->pers.netname, text);
			Com_sprintf(name, sizeof(name), "%s%c%c"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
			color = COLOR_RED;

			break;
		}

		// GalaxyRP fix: [Chat] searches for chat modifiers. Longest match wins. Case sensitive.
		int best_match_index = -1;
		int best_match_length = 0;

		for (int i = 0; i < ARRAY_LEN(chat_modifiers); i++) {
			int modifier_length = strlen(chat_modifiers[i].chat_modifier);

			if (strncmp(text, chat_modifiers[i].chat_modifier, modifier_length) != 0)
				continue;

			if (is_character_not_a_word_delimiter(text[modifier_length]))
				continue;

			if (modifier_length > best_match_length) {
				best_match_index = i;
				best_match_length = modifier_length;
			}
		}

		if (best_match_index != -1) {
			char formatted_chat[MAX_STRING_CHARS];

			delete_chat_command(text, best_match_length);
			// [Alex]: format text and place it into formatted_chat
			Com_sprintf(formatted_chat, sizeof(formatted_chat),
				chat_modifiers[best_match_index].chat_format, ent->client->pers.netname, text);
			format_text_in_quotes(formatted_chat, sizeof(formatted_chat));

			// [Alex]: Log chat to the server log.
			G_LogPrintf("%s: %s: %s\n", chat_modifiers[best_match_index].chat_modifier, ent->client->pers.netname, text);

			for (j = 0; j < level.maxclients; j++) {

				other = &g_entities[j];
				if (!other->inuse || !other->client || other->client->pers.connected != CON_CONNECTED)
					continue;
				
				if (check_if_player_ignores_other(ent, other))
					continue;

				if (Distance(ent->client->ps.origin, other->client->ps.origin) <= chat_modifiers[best_match_index].distance || zyk_ignores_chat_distance(other) || other->client->sess.sessionTeam == TEAM_SPECTATOR)
				{
					// [Alex]: Send the chat message to the client.
					trap->SendServerCommand(j, formatted_chat);
				}
				else
					continue;
			}

			return;
		}

		G_LogPrintf( "say: %s: %s\n", ent->client->pers.netname, text );
		Com_sprintf (name, sizeof(name), "%s%c%c"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		color = COLOR_GREEN;
		//set the desired distance here
		distance = 700;
		break;
	case SAY_TEAM:
		// zyk: if player is silenced by an admin, he cannot say anything
		if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_SILENCED))
			return;
		
		G_LogPrintf( "sayteam: %s: %s\n", ent->client->pers.netname, text);
		if (Team_GetLocationMsg(ent, location, sizeof(location)))
		{
			Com_sprintf (name, sizeof(name), EC"(%s%c%c"EC")"EC": ",
				ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
			locMsg = location;
		}
		else
		{
		Com_sprintf (name, sizeof(name), EC"(%s%c%c"EC")"EC": ",
			ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		}
		color = COLOR_CYAN;
		break;
	case SAY_TELL:

		if (target && target->inuse && target->client && level.gametype >= GT_TEAM &&
			target->client->sess.sessionTeam == ent->client->sess.sessionTeam &&
			Team_GetLocationMsg(ent, location, sizeof(location)))
		{
			Com_sprintf (name, sizeof(name), EC"[%s%c%c"EC"]"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
			locMsg = location;
		}
		else
		{
			Com_sprintf (name, sizeof(name), EC"[%s%c%c"EC"]"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );
		}
		color = COLOR_MAGENTA;
		break;
	case SAY_ALLY: // zyk: say to allies
		// zyk: if player is silenced by an admin, he cannot say anything
		if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_SILENCED))
			return;

		G_LogPrintf( "sayally: %s: %s\n", ent->client->pers.netname, text );
		Com_sprintf (name, sizeof(name), EC"{%s%c%c"EC"}"EC": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE );

		color = COLOR_WHITE;
		break;
	}

	if ( target ) {
		G_SayTo( ent, target, mode, color, name, text, locMsg );
		return;
	}

	// echo the text to the console
	if ( dedicated.integer ) {
		trap->Print( "%s%s\n", name, text);
	}

	// send it to all the appropriate clients, within the desired distance
	for (j = 0; j < level.maxclients; j++) {
		other = &g_entities[j];
		//I know there can be a switch here, but i'm too lazy
		if (mode == SAY_ALL || mode == SAY_TEAM)
		{
			if (mode == SAY_ALL) {
				if (Distance(ent->client->ps.origin, other->client->ps.origin) <= distance || zyk_ignores_chat_distance(other) || other->client->sess.sessionTeam == TEAM_SPECTATOR)
				{
					if (ooc_flag == 1) {
						G_SayTo(ent, other, mode, color, name, ooc_text, locMsg);
					}else
						G_SayTo(ent, other, mode, color, name, text, locMsg);
				}
				else
					continue;
			}
			else
			{
				G_SayTo(ent, other, mode, color, name, text, locMsg);
			}
		}
		else
			G_SayTo(ent, other, mode, color, name, text, locMsg);
	}
}


/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f( gentity_t *ent ) {
	char *p = NULL;

	if ( trap->Argc () < 2 )
		return;

	p = ConcatArgs( 1 );

	if ( strlen( p ) >= MAX_SAY_TEXT ) {
		p[MAX_SAY_TEXT-1] = '\0';
		G_SecurityLogPrintf( "Cmd_Say_f from %d (%s) has been truncated: %s\n", ent->s.number, ent->client->pers.netname, p );
	}

	G_Say( ent, NULL, SAY_ALL, p );
}

/*
==================
Cmd_SayTeam_f
==================
*/
static void Cmd_SayTeam_f( gentity_t *ent ) {
	char *p = NULL;

	if ( trap->Argc () < 2 )
		return;

	p = ConcatArgs( 1 );

	if ( strlen( p ) >= MAX_SAY_TEXT ) {
		p[MAX_SAY_TEXT-1] = '\0';
		G_SecurityLogPrintf( "Cmd_SayTeam_f from %d (%s) has been truncated: %s\n", ent->s.number, ent->client->pers.netname, p );
	}

	// zyk: if not in TEAM gametypes and player has allies, use allychat (SAY_ALLY) instead of SAY_ALL
	if (zyk_number_of_allies(ent,qfalse) > 0)
		G_Say( ent, NULL, (level.gametype>=GT_TEAM) ? SAY_TEAM : SAY_ALLY, p );
	else
		G_Say( ent, NULL, (level.gametype>=GT_TEAM) ? SAY_TEAM : SAY_TEAM, p );
}

/*
==================
Cmd_OOC_f

GalaxyRP: [Chat] out-of-character chat as a command of its own.
==================
*/
static void Cmd_OOC_f( gentity_t *ent ) {
	char *p = NULL;

	if ( trap->Argc () < 2 )
		return;

	p = ConcatArgs( 1 );

	if ( strlen( p ) >= MAX_SAY_TEXT ) {
		p[MAX_SAY_TEXT-1] = '\0';
		G_SecurityLogPrintf( "Cmd_OOC_f from %d (%s) has been truncated: %s\n", ent->s.number, ent->client->pers.netname, p );
	}

	G_Say( ent, NULL, SAY_OOC, p );
}

/*
==================
Cmd_Tell_f
==================
*/
static void Cmd_Tell_f( gentity_t *ent ) {
	int			targetNum;
	gentity_t	*target;
	char		*p;
	char		arg[MAX_TOKEN_CHARS];

	if ( trap->Argc () < 3 ) {
		trap->SendServerCommand( ent-g_entities, "print \"Usage: tell <player id or name> <message>\n\"" );
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );
	targetNum = ClientNumberFromString( ent, arg, qfalse ); // zyk: changed this. Now it will use new function
	if ( targetNum == -1 ) {
		return;
	}

	target = &g_entities[targetNum];

	p = ConcatArgs( 2 );

	if ( strlen( p ) >= MAX_SAY_TEXT ) {
		p[MAX_SAY_TEXT-1] = '\0';
		G_SecurityLogPrintf( "Cmd_Tell_f from %d (%s) has been truncated: %s\n", ent->s.number, ent->client->pers.netname, p );
	}

	G_LogPrintf( "tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, p );
	G_Say( ent, target, SAY_TELL, p );
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if ( ent != target && !(ent->r.svFlags & SVF_BOT)) {
		G_Say( ent, ent, SAY_TELL, p );
	}
}

//siege voice command
static void Cmd_VoiceCommand_f(gentity_t *ent)
{
	gentity_t *te;
	char arg[MAX_TOKEN_CHARS];
	char *s;
	char content[MAX_TOKEN_CHARS];
	int i = 0;

	/* zyk: now the voice commands will be allowed in all gametypes
	if (level.gametype < GT_TEAM)
	{
		return;
	}
	*/

	strcpy(content,"");

	if (trap->Argc() < 2)
	{
		// zyk: other gamemodes will show info of how to use the voice_cmd
		if (level.gametype < GT_TEAM)
		{
			for (i = 0; i < MAX_CUSTOM_SIEGE_SOUNDS; i++)
			{
				strcpy(content,va("%s%s\n",content,bg_customSiegeSoundNames[i]));
			}
			trap->SendServerCommand( ent-g_entities, va("print \"Usage: /voice_cmd <arg> [f]\nThe f argument is optional, it will make the command use female voice.\nThe arg may be one of the following:\n %s\"",content) );
		}
		return;
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR ||
		ent->client->tempSpectate >= level.time)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOICECHATASSPEC")) );
		return;
	}

	trap->Argv(1, arg, sizeof(arg));

	if (arg[0] == '*')
	{ //hmm.. don't expect a * to be prepended already. maybe someone is trying to be sneaky.
		return;
	}

	s = va("*%s", arg);

	//now, make sure it's a valid sound to be playing like this.. so people can't go around
	//screaming out death sounds or whatever.
	while (i < MAX_CUSTOM_SIEGE_SOUNDS)
	{
		if (!bg_customSiegeSoundNames[i])
		{
			break;
		}
		if (!Q_stricmp(bg_customSiegeSoundNames[i], s))
		{ //it matches this one, so it's ok
			break;
		}
		i++;
	}

	if (i == MAX_CUSTOM_SIEGE_SOUNDS || !bg_customSiegeSoundNames[i])
	{ //didn't find it in the list
		return;
	}

	if (level.gametype >= GT_TEAM)
	{
		te = G_TempEntity(vec3_origin, EV_VOICECMD_SOUND);
		te->s.groundEntityNum = ent->s.number;
		te->s.eventParm = G_SoundIndex((char *)bg_customSiegeSoundNames[i]);
		te->r.svFlags |= SVF_BROADCAST;
	}
	else
	{ // zyk: in other gamemodes that are not Team ones, just do a G_Sound call to each allied player
		char arg2[MAX_TOKEN_CHARS];
		char voice_dir[32];

		strcpy(voice_dir,"mp_generic_male");

		if (trap->Argc() == 3)
		{
			trap->Argv(2, arg2, sizeof(arg2));
			if (Q_stricmp(arg2, "f") == 0)
				strcpy(voice_dir,"mp_generic_female");
		}

		G_Sound(ent,CHAN_VOICE,G_SoundIndex(va("sound/chars/%s/misc/%s.mp3",voice_dir,arg)));

		for (i = 0; i < MAX_CLIENTS; i++)
		{
			if (zyk_is_ally(ent,&g_entities[i]) == qtrue)
			{
				trap->SendServerCommand(i, va("chat \"%s: ^3%s\"",ent->client->pers.netname,arg));
				G_Sound(&g_entities[i],CHAN_VOICE,G_SoundIndex(va("sound/chars/%s/misc/%s.mp3",voice_dir,arg)));
			}
		}
	}
}


static char	*gc_orders[] = {
	"hold your position",
	"hold this position",
	"come here",
	"cover me",
	"guard location",
	"search and destroy",
	"report"
};
static size_t numgc_orders = ARRAY_LEN( gc_orders );

void Cmd_GameCommand_f( gentity_t *ent ) {
	int				targetNum;
	unsigned int	order;
	gentity_t		*target;
	char			arg[MAX_TOKEN_CHARS] = {0};

	if ( trap->Argc() != 3 ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"Usage: gc <player id> <order 0-%d>\n\"", numgc_orders - 1 ) );
		return;
	}

	trap->Argv( 2, arg, sizeof( arg ) );
	order = atoi( arg );

	if ( order >= numgc_orders ) {
		trap->SendServerCommand( ent-g_entities, va("print \"Bad order: %i\n\"", order));
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );
	targetNum = ClientNumberFromString( ent, arg, qfalse );
	if ( targetNum == -1 )
		return;

	target = &g_entities[targetNum];
	if ( !target->inuse || !target->client )
		return;

	G_LogPrintf( "tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, gc_orders[order] );
	G_Say( ent, target, SAY_TELL, gc_orders[order] );
	// don't tell to the player self if it was already directed to this player
	// also don't send the chat back to a bot
	if ( ent != target && !(ent->r.svFlags & SVF_BOT) )
		G_Say( ent, ent, SAY_TELL, gc_orders[order] );
}

/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f( gentity_t *ent ) {
	// zyk: changed code, so it will always use ps.origin and the ps.viewangles
	if(ent->client)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"origin: %s angles: %s\n\"", vtos(ent->client->ps.origin), vtos(ent->client->ps.viewangles)));
	}
}

static const char *gameNames[] = {
	"Free For All",
	"Holocron FFA",
	"Jedi Master",
	"Duel",
	"Power Duel",
	"Single Player",
	"Team FFA",
	"Siege",
	"Capture the Flag",
	"Capture the Ysalamiri"
};

/*
==================
Cmd_CallVote_f
==================
*/
extern void SiegeClearSwitchData(void); //g_saga.c

static qboolean G_VoteRefusePinnedLimit( gentity_t *ent, const char *cvarName ) {
	trap->SendServerCommand( ent-g_entities, va( "print \"%s is fixed at 0 on this server.\n\"", cvarName ) );
	return qfalse;
}

qboolean G_VoteCapturelimit( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	return G_VoteRefusePinnedLimit( ent, "capturelimit" );
}

qboolean G_VoteClientkick( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int n = atoi ( arg2 );

	if ( n < 0 || n >= level.maxclients ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"invalid client number %d.\n\"", n ) );
		return qfalse;
	}

	if ( g_entities[n].client->pers.connected == CON_DISCONNECTED ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"there is no client with the client number %d.\n\"", n ) );
		return qfalse;
	}

	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %d", arg1, n );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s %s", arg1, g_entities[n].client->pers.netname );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteFraglimit( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	return G_VoteRefusePinnedLimit( ent, "fraglimit" );
}

qboolean G_VoteGametype( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int gt = atoi( arg2 );

	// ffa, ctf, tdm, etc
	if ( arg2[0] && isalpha( arg2[0] ) ) {
		gt = BG_GetGametypeForString( arg2 );
		if ( gt == -1 )
		{
			trap->SendServerCommand( ent-g_entities, va( "print \"Gametype (%s) unrecognised, defaulting to FFA/Deathmatch\n\"", arg2 ) );
			gt = GT_FFA;
		}
	}
	// numeric but out of range
	else if ( gt < 0 || gt >= GT_MAX_GAME_TYPE ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"Gametype (%i) is out of range, defaulting to FFA/Deathmatch\n\"", gt ) );
		gt = GT_FFA;
	}

	// logically invalid gametypes, or gametypes not fully implemented in MP
	if ( gt == GT_SINGLE_PLAYER ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"This gametype is not supported (%s).\n\"", arg2 ) );
		return qfalse;
	}

	level.votingGametype = qtrue;
	level.votingGametypeTo = gt;

	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %d", arg1, gt );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "%s %s", arg1, gameNames[gt] );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteKick( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int clientid = ClientNumberFromString( ent, arg2, qtrue );
	gentity_t *target = NULL;

	if ( clientid == -1 )
		return qfalse;

	target = &g_entities[clientid];
	if ( !target || !target->inuse || !target->client )
		return qfalse;

	Com_sprintf( level.voteString, sizeof( level.voteString ), "clientkick %d", clientid );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "kick %s", target->client->pers.netname );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

const char *G_GetArenaInfoByMap( const char *map );

/*
==================
Cmd_MapList_f
==================
*/
void Cmd_MapList_f( gentity_t *ent ) {
	char arg[MAX_STRING_CHARS];
	int argn = 1;
	int page = 1;

	if ( trap->Argc() >= 2 ) {
		trap->Argv( 1, arg, sizeof( arg ) );
		if ( Q_stricmp( arg, "bsp" ) == 0 ) {
			argn = 2;
		}
	}

	if ( trap->Argc() > argn + 1 ) {
		trap->SendServerCommand( ent-g_entities, "print \"Usage: ^3/maplist <page number (optional)>\n\"" );
		return;
	}

	if ( trap->Argc() == argn + 1 ) {
		trap->Argv( argn, arg, sizeof( arg ) );
		page = RP_ListParsePage( arg );
		if ( page <= 0 ) {
			trap->SendServerCommand( ent-g_entities, "print \"Invalid page number\n\"" );
			return;
		}
	}

	RP_ListVotableMaps( ent, page );
}

qboolean G_VotePoll( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	// zyk: did not put message
	if ( numArgs < 3 ) {
		return qfalse;
	}

	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s", arg1, arg2 );
	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "poll %s", arg2 );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );

	// zyk: now the vote poll will appear in chat
	trap->SendServerCommand( -1, va("chat \"^3Poll System: ^7%s ^2Yes^3^1/No^7\"",arg2));

	return qtrue;
}

qboolean G_VoteMap( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	char s[MAX_CVAR_VALUE_STRING] = {0}, bspName[MAX_QPATH] = {0}, *mapName = NULL, *mapName2 = NULL;
	fileHandle_t fp = NULL_FILE;
	const char *arenaInfo;

	// didn't specify a map, show available maps
	// GalaxyRP: [Listings] the first page of /maplist (g_rplist.c), which points to the next one.
	if ( numArgs < 3 ) {
		trap->SendServerCommand( ent-g_entities, "print \"Usage: ^3/callvote map <map name>\n\"" );
		RP_ListVotableMaps( ent, 1 );
		return qfalse;
	}

	if ( strchr( arg2, '\\' ) ) {
		trap->SendServerCommand( ent-g_entities, "print \"Can't have mapnames with a \\\n\"" );
		return qfalse;
	}

	Com_sprintf( bspName, sizeof(bspName), "maps/%s.bsp", arg2 );
	if ( trap->FS_Open( bspName, &fp, FS_READ ) <= 0 ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"Can't find map %s on server\n\"", bspName ) );
		if( fp != NULL_FILE )
			trap->FS_Close( fp );
		return qfalse;
	}
	trap->FS_Close( fp );

	if ( !G_DoesMapSupportGametype( arg2, level.gametype ) ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NOVOTE_MAPNOTSUPPORTEDBYGAME" ) ) );
		return qfalse;
	}

	// preserve the map rotation
	trap->Cvar_VariableStringBuffer( "nextmap", s, sizeof( s ) );
	if ( *s )
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s; set nextmap \"%s\"", arg1, arg2, s );
	else
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %s", arg1, arg2 );

	arenaInfo = G_GetArenaInfoByMap(arg2);
	if ( arenaInfo ) {
		mapName = Info_ValueForKey( arenaInfo, "longname" );
		mapName2 = Info_ValueForKey( arenaInfo, "map" );
	}

	if ( !mapName || !mapName[0] )
		mapName = "ERROR";

	if ( !mapName2 || !mapName2[0] )
		mapName2 = "ERROR";

	Com_sprintf( level.voteDisplayString, sizeof( level.voteDisplayString ), "map %s (%s)", mapName, mapName2 );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteMapRestart( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int n = Com_Clampi( 0, 60, atoi( arg2 ) );
	if ( numArgs < 3 )
		n = 5;
	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %i", arg1, n );
	Q_strncpyz( level.voteDisplayString, level.voteString, sizeof( level.voteDisplayString ) );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteNextmap( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	char s[MAX_CVAR_VALUE_STRING];

	trap->Cvar_VariableStringBuffer( "nextmap", s, sizeof( s ) );
	if ( !*s ) {
		trap->SendServerCommand( ent-g_entities, "print \"nextmap not set.\n\"" );
		return qfalse;
	}
	SiegeClearSwitchData();
	Com_sprintf( level.voteString, sizeof( level.voteString ), "vstr nextmap");
	Q_strncpyz( level.voteDisplayString, level.voteString, sizeof( level.voteDisplayString ) );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

qboolean G_VoteTimelimit( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	return G_VoteRefusePinnedLimit( ent, "timelimit" );
}

qboolean G_VoteWarmup( gentity_t *ent, int numArgs, const char *arg1, const char *arg2 ) {
	int n = Com_Clampi( 0, 1, atoi( arg2 ) );
	Com_sprintf( level.voteString, sizeof( level.voteString ), "%s %i", arg1, n );
	Q_strncpyz( level.voteDisplayString, level.voteString, sizeof( level.voteDisplayString ) );
	Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	return qtrue;
}

typedef struct voteString_s {
	const char	*string;
	const char	*aliases;	// space delimited list of aliases, will always show the real vote string
	qboolean	(*func)(gentity_t *ent, int numArgs, const char *arg1, const char *arg2);
	int			numArgs;	// number of REQUIRED arguments, not total/optional arguments
	uint32_t	validGT;	// bit-flag of valid gametypes
	qboolean	voteDelay;	// if true, will delay executing the vote string after it's accepted by g_voteDelay
	const char	*shortHelp;	// NULL if no arguments needed
} voteString_t;

static voteString_t validVoteStrings[] = {
	//	vote string				aliases										# args	valid gametypes							exec delay		short help
	{	"capturelimit",			"caps",				G_VoteCapturelimit,		1,		GTB_CTF|GTB_CTY,						qtrue,			"<num>" },
	{	"clientkick",			NULL,				G_VoteClientkick,		1,		GTB_ALL,								qfalse,			"<clientnum>" },
	{	"fraglimit",			"frags",			G_VoteFraglimit,		1,		GTB_ALL & ~(GTB_SIEGE|GTB_CTF|GTB_CTY),	qtrue,			"<num>" },
	{	"g_doWarmup",			"dowarmup warmup",	G_VoteWarmup,			1,		GTB_ALL,								qtrue,			"<0-1>" },
	{	"g_gametype",			"gametype gt mode",	G_VoteGametype,			1,		GTB_ALL,								qtrue,			"<num or name>" },
	{	"kick",					NULL,				G_VoteKick,				1,		GTB_ALL,								qfalse,			"<client name>" },
	{	"map",					NULL,				G_VoteMap,				0,		GTB_ALL,								qtrue,			"<name>" },
	{	"map_restart",			"restart",			G_VoteMapRestart,		0,		GTB_ALL,								qtrue,			"<optional delay>" },
	{	"nextmap",				NULL,				G_VoteNextmap,			0,		GTB_ALL,								qtrue,			NULL },
	{	"poll",					NULL,				G_VotePoll,				0,		GTB_ALL,								qtrue,			"<message>" },
	{	"timelimit",			"time",				G_VoteTimelimit,		1,		GTB_ALL &~GTB_SIEGE,					qtrue,			"<num>" },
};
static const int validVoteStringsSize = ARRAY_LEN( validVoteStrings );

void Svcmd_ToggleAllowVote_f( void ) {
	if ( trap->Argc() == 1 ) {
		int i = 0;
		for ( i = 0; i<validVoteStringsSize; i++ ) {
			if ( (g_allowVote.integer & (1 << i)) )	trap->Print( "%2d [X] %s\n", i, validVoteStrings[i].string );
			else									trap->Print( "%2d [ ] %s\n", i, validVoteStrings[i].string );
		}
		return;
	}
	else {
		char arg[8] = { 0 };
		int index;

		trap->Argv( 1, arg, sizeof( arg ) );
		index = atoi( arg );

		if ( index < 0 || index >= validVoteStringsSize ) {
			Com_Printf( "ToggleAllowVote: Invalid range: %i [0, %i]\n", index, validVoteStringsSize - 1 );
			return;
		}

		trap->Cvar_Set( "g_allowVote", va( "%i", (1 << index) ^ (g_allowVote.integer & ((1 << validVoteStringsSize) - 1)) ) );
		trap->Cvar_Update( &g_allowVote );

		Com_Printf( "%s %s^7\n", validVoteStrings[index].string, ((g_allowVote.integer & (1 << index)) ? "^2Enabled" : "^1Disabled") );
	}
}

void Cmd_CallVote_f( gentity_t *ent ) {
	int				i=0, numArgs=0;
	char			arg1[MAX_CVAR_VALUE_STRING] = {0};
	char			arg2[MAX_CVAR_VALUE_STRING] = {0};
	voteString_t	*vote = NULL;

	// not allowed to vote at all
	if ( !g_allowVote.integer ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NOVOTE" ) ) );
		return;
	}

	// vote in progress
	else if ( level.voteTime ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "VOTEINPROGRESS" ) ) );
		return;
	}

	else if ( level.voteExecuteTime ) {
		trap->SendServerCommand( ent-g_entities, "print \"A vote has just passed and is about to take effect. Try again in a moment.\n\"" );
		return;
	}

	// can't vote as a spectator, except in (power)duel
	else if ( level.gametype != GT_DUEL && level.gametype != GT_POWERDUEL && ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NOSPECVOTE" ) ) );
		return;
	}

	// zyk: tests if this player can vote now
	if (rp_vote_timer.integer > 0 && ent->client->sess.vote_timer > 0)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You cannot vote now, wait %d seconds and try again.\n\"", ent->client->sess.vote_timer));
		return;
	}

	level.voting_player = ent->s.number;

	// make sure it is a valid command to vote on
	numArgs = trap->Argc();
	trap->Argv( 1, arg1, sizeof( arg1 ) );
	if ( numArgs > 1 )
		Q_strncpyz( arg2, ConcatArgs( 2 ), sizeof( arg2 ) );

	// filter ; \n \r
	if ( Q_strchrs( arg1, ";\r\n" ) || Q_strchrs( arg2, ";\r\n" ) || RP_HasStringEdRef( arg1 ) || RP_HasStringEdRef( arg2 ) ) {
		trap->SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		return;
	}

	// check for invalid votes
	for ( i=0; i<validVoteStringsSize; i++ ) {
		if ( !(g_allowVote.integer & (1<<i)) )
			continue;

		if ( !Q_stricmp( arg1, validVoteStrings[i].string ) )
			break;

		// see if they're using an alias, and set arg1 to the actual vote string
		if ( validVoteStrings[i].aliases ) {
			char tmp[MAX_TOKEN_CHARS] = {0}, *p = NULL;
			const char *delim = " ";
			Q_strncpyz( tmp, validVoteStrings[i].aliases, sizeof( tmp ) );
			p = strtok( tmp, delim );
			while ( p != NULL ) {
				if ( !Q_stricmp( arg1, p ) ) {
					Q_strncpyz( arg1, validVoteStrings[i].string, sizeof( arg1 ) );
					goto validVote;
				}
				p = strtok( NULL, delim );
			}
		}
	}
	// invalid vote string, abandon ship
	if ( i == validVoteStringsSize ) {
		char buf[1024] = {0};
		int toggle = 0;
		trap->SendServerCommand( ent-g_entities, "print \"Invalid vote string.\n\"" );
		trap->SendServerCommand( ent-g_entities, "print \"Allowed vote strings are: \"" );
		for ( i=0; i<validVoteStringsSize; i++ ) {
			if ( !(g_allowVote.integer & (1<<i)) )
				continue;

			toggle = !toggle;
			if ( validVoteStrings[i].shortHelp ) {
				Q_strcat( buf, sizeof( buf ), va( "^%c%s %s ",
					toggle ? COLOR_GREEN : COLOR_YELLOW,
					validVoteStrings[i].string,
					validVoteStrings[i].shortHelp ) );
			}
			else {
				Q_strcat( buf, sizeof( buf ), va( "^%c%s ",
					toggle ? COLOR_GREEN : COLOR_YELLOW,
					validVoteStrings[i].string ) );
			}
		}

		//FIXME: buffer and send in multiple messages in case of overflow
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", buf ) );
		return;
	}

validVote:
	vote = &validVoteStrings[i];
	if ( !(vote->validGT & (1<<level.gametype)) ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s is not applicable in this gametype.\n\"", arg1 ) );
		return;
	}

	if ( numArgs < vote->numArgs+2 ) {
		trap->SendServerCommand( ent-g_entities, va( "print \"%s requires more arguments: %s\n\"", arg1, vote->shortHelp ) );
		return;
	}

	level.votingGametype = qfalse;

	level.voteExecuteDelay = vote->voteDelay ? g_voteDelay.integer : 0;

	// pass the args onto vote-specific handlers for parsing/filtering
	if ( vote->func ) {
		if ( !vote->func( ent, numArgs, arg1, arg2 ) )
			return;
	}
	// otherwise assume it's a command
	else {
		Com_sprintf( level.voteString, sizeof( level.voteString ), "%s \"%s\"", arg1, arg2 );
		Q_strncpyz( level.voteDisplayString, level.voteString, sizeof( level.voteDisplayString ) );
		Q_strncpyz( level.voteStringClean, level.voteString, sizeof( level.voteStringClean ) );
	}
	Q_strstrip( level.voteStringClean, "\"\n\r", NULL );

	trap->SendServerCommand( -1, va( "print \"%s^7 %s (%s)\n\"", ent->client->pers.netname, G_GetStringEdString( "MP_SVGAME", "PLCALLEDVOTE" ), level.voteStringClean ) );

	// start the voting, the caller automatically votes yes
	level.voteTime = level.time;
	level.voteYes = 0; // zyk: the caller no longer counts as yes, because it may be a poll or the caller may regret the vote
	level.voteNo = 0;

	for ( i=0; i<level.maxclients; i++ ) {
		level.clients[i].mGameFlags &= ~PSG_VOTED;
		level.clients[i].pers.vote = 0;
	}

	//ent->client->mGameFlags |= PSG_VOTED; // zyk: no longer count the caller
	//ent->client->pers.vote = 1; // zyk: no longer count the caller

	trap->SetConfigstring( CS_VOTE_TIME,	va( "%i", level.voteTime ) );
	trap->SetConfigstring( CS_VOTE_STRING,	level.voteDisplayString );
	trap->SetConfigstring( CS_VOTE_YES,		va( "%i", level.voteYes ) );
	trap->SetConfigstring( CS_VOTE_NO,		va( "%i", level.voteNo ) );
}

/*
==================
Cmd_Vote_f
==================
*/
void Cmd_Vote_f( gentity_t *ent ) {
	char		msg[64] = {0};

	if ( !level.voteTime ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEINPROG")) );
		return;
	}
	if ( ent->client->mGameFlags & PSG_VOTED ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "VOTEALREADY")) );
		return;
	}
	if (level.gametype != GT_DUEL && level.gametype != GT_POWERDUEL)
	{
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEASSPEC")) );
			return;
		}
	}

	trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLVOTECAST")) );

	ent->client->mGameFlags |= PSG_VOTED;

	trap->Argv( 1, msg, sizeof( msg ) );

	if ( tolower( msg[0] ) == 'y' || msg[0] == '1' ) {
		level.voteYes++;
		ent->client->pers.vote = 1;
		trap->SetConfigstring( CS_VOTE_YES, va("%i", level.voteYes ) );
	} else {
		level.voteNo++;
		ent->client->pers.vote = 2;
		trap->SetConfigstring( CS_VOTE_NO, va("%i", level.voteNo ) );
	}

	// a majority will be determined in CheckVote, which will also account
	// for players entering or leaving
}

qboolean G_TeamVoteLeader( gentity_t *ent, int cs_offset, team_t team, int numArgs, const char *arg1, const char *arg2 ) {
	int clientid = numArgs == 2 ? ent->s.number : ClientNumberFromString( ent, arg2, qfalse );
	gentity_t *target = NULL;

	if ( clientid == -1 )
		return qfalse;

	target = &g_entities[clientid];
	if ( !target || !target->inuse || !target->client )
		return qfalse;

	if ( target->client->sess.sessionTeam != team )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"User %s is not on your team\n\"", arg2 ) );
		return qfalse;
	}

	Com_sprintf( level.teamVoteString[cs_offset], sizeof( level.teamVoteString[cs_offset] ), "leader %d", clientid );
	Q_strncpyz( level.teamVoteDisplayString[cs_offset], level.teamVoteString[cs_offset], sizeof( level.teamVoteDisplayString[cs_offset] ) );
	Q_strncpyz( level.teamVoteStringClean[cs_offset], level.teamVoteString[cs_offset], sizeof( level.teamVoteStringClean[cs_offset] ) );
	return qtrue;
}

/*
==================
Cmd_CallTeamVote_f
==================
*/
void Cmd_CallTeamVote_f( gentity_t *ent ) {
	team_t	team = ent->client->sess.sessionTeam;
	int		i=0, cs_offset=0, numArgs=0;
	char	arg1[MAX_CVAR_VALUE_STRING] = {0};
	char	arg2[MAX_CVAR_VALUE_STRING] = {0};

	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	// not allowed to vote at all
	if ( !g_allowTeamVote.integer ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTE")) );
		return;
	}

	// vote in progress
	else if ( level.teamVoteTime[cs_offset] ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEALREADY")) );
		return;
	}

	// can't vote as a spectator
	else if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOSPECVOTE")) );
		return;
	}

	// make sure it is a valid command to vote on
	numArgs = trap->Argc();
	trap->Argv( 1, arg1, sizeof( arg1 ) );
	if ( numArgs > 1 )
		Q_strncpyz( arg2, ConcatArgs( 2 ), sizeof( arg2 ) );

	// filter ; \n \r
	if ( Q_strchrs( arg1, ";\r\n" ) || Q_strchrs( arg2, ";\r\n" ) ) {
		trap->SendServerCommand( ent-g_entities, "print \"Invalid team vote string.\n\"" );
		return;
	}

	// pass the args onto vote-specific handlers for parsing/filtering
	if ( !Q_stricmp( arg1, "leader" ) ) {
		if ( !G_TeamVoteLeader( ent, cs_offset, team, numArgs, arg1, arg2 ) )
			return;
	}
	else {
		trap->SendServerCommand( ent-g_entities, "print \"Invalid team vote string.\n\"" );
		trap->SendServerCommand( ent-g_entities, va("print \"Allowed team vote strings are: ^%c%s %s\n\"", COLOR_GREEN, "leader", "<optional client name or number>" ));
		return;
	}

	Q_strstrip( level.teamVoteStringClean[cs_offset], "\"\n\r", NULL );

	for ( i=0; i<level.maxclients; i++ ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED )
			continue;
		if ( level.clients[i].sess.sessionTeam == team )
			trap->SendServerCommand( i, va("print \"%s^7 called a team vote (%s)\n\"", ent->client->pers.netname, level.teamVoteStringClean[cs_offset] ) );
	}

	// start the voting, the caller autoamtically votes yes
	level.teamVoteTime[cs_offset] = level.time;
	level.teamVoteYes[cs_offset] = 1;
	level.teamVoteNo[cs_offset] = 0;

	for ( i=0; i<level.maxclients; i++ ) {
		if ( level.clients[i].pers.connected == CON_DISCONNECTED )
			continue;
		if ( level.clients[i].sess.sessionTeam == team ) {
			level.clients[i].mGameFlags &= ~PSG_TEAMVOTED;
			level.clients[i].pers.teamvote = 0;
		}
	}
	ent->client->mGameFlags |= PSG_TEAMVOTED;
	ent->client->pers.teamvote = 1;

	trap->SetConfigstring( CS_TEAMVOTE_TIME + cs_offset, va("%i", level.teamVoteTime[cs_offset] ) );
	trap->SetConfigstring( CS_TEAMVOTE_STRING + cs_offset, level.teamVoteDisplayString[cs_offset] );
	trap->SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
	trap->SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );
}

/*
==================
Cmd_TeamVote_f
==================
*/
void Cmd_TeamVote_f( gentity_t *ent ) {
	team_t		team = ent->client->sess.sessionTeam;
	int			cs_offset=0;
	char		msg[64] = {0};

	if ( team == TEAM_RED )
		cs_offset = 0;
	else if ( team == TEAM_BLUE )
		cs_offset = 1;
	else
		return;

	if ( !level.teamVoteTime[cs_offset] ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOTEAMVOTEINPROG")) );
		return;
	}
	if ( ent->client->mGameFlags & PSG_TEAMVOTED ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEALREADYCAST")) );
		return;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEASSPEC")) );
		return;
	}

	trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "PLTEAMVOTECAST")) );

	ent->client->mGameFlags |= PSG_TEAMVOTED;

	trap->Argv( 1, msg, sizeof( msg ) );

	if ( tolower( msg[0] ) == 'y' || msg[0] == '1' ) {
		level.teamVoteYes[cs_offset]++;
		ent->client->pers.teamvote = 1;
		trap->SetConfigstring( CS_TEAMVOTE_YES + cs_offset, va("%i", level.teamVoteYes[cs_offset] ) );
	} else {
		level.teamVoteNo[cs_offset]++;
		ent->client->pers.teamvote = 2;
		trap->SetConfigstring( CS_TEAMVOTE_NO + cs_offset, va("%i", level.teamVoteNo[cs_offset] ) );
	}

	// a majority will be determined in TeamCheckVote, which will also account
	// for players entering or leaving
}


/*
=================
Cmd_SetViewpos_f
=================
*/
void Cmd_SetViewpos_f( gentity_t *ent ) {
	vec3_t		origin, angles;
	char		buffer[MAX_TOKEN_CHARS];
	int			i;

	if ( trap->Argc() != 5 ) {
		trap->SendServerCommand( ent-g_entities, va("print \"usage: setviewpos x y z yaw\n\""));
		return;
	}

	VectorClear( angles );
	for ( i = 0 ; i < 3 ; i++ ) {
		trap->Argv( i + 1, buffer, sizeof( buffer ) );
		origin[i] = atof( buffer );
	}

	trap->Argv( 4, buffer, sizeof( buffer ) );
	angles[YAW] = atof( buffer );

	// DAJ_RP: [Admin] not a rider, as /teleport (RP_TeleRiderRefused() below): the vehicle would put them
	// straight back
	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR && ent->client->ps.m_iVehicleNum ) {
		trap->SendServerCommand( ent-g_entities, va("print \"%s ^7is riding a vehicle.\n\"", ent->client->pers.netname) );
		return;
	}

	TeleportPlayer( ent, origin, angles );
}

extern void Jedi_Decloak( gentity_t *self );
void G_LeaveVehicle( gentity_t* ent, qboolean ConCheck ) {

	if (ent->client->ps.m_iVehicleNum)
	{ //tell it I'm getting off
		gentity_t *veh = &g_entities[ent->client->ps.m_iVehicleNum];

		if (veh->inuse && veh->client && veh->m_pVehicle)
		{
			if ( ent->r.ownerNum == veh->s.number && veh->m_pVehicle->m_pPilot == (bgEntity_t *)ent )
			{
				Jedi_Decloak( veh );
			}

			if ( ConCheck ) { // check connection
				clientConnected_t pCon = ent->client->pers.connected;
				ent->client->pers.connected = CON_DISCONNECTED;
				veh->m_pVehicle->m_pVehicleInfo->Eject(veh->m_pVehicle, (bgEntity_t *)ent, qtrue);
				ent->client->pers.connected = pCon;
			} else { // or not.
				veh->m_pVehicle->m_pVehicleInfo->Eject(veh->m_pVehicle, (bgEntity_t *)ent, qtrue);
			}
		}
	}

	ent->client->ps.m_iVehicleNum = 0;
}

int G_ItemUsable(playerState_t *ps, int forcedUse)
{
	vec3_t fwd, fwdorg, dest, pos;
	vec3_t yawonly;
	vec3_t mins, maxs;
	vec3_t trtest;
	trace_t tr;

	// fix: dead players shouldn't use items
	if (ps->stats[STAT_HEALTH] <= 0) {
		return 0;
	}

	if (ps->m_iVehicleNum)
	{
		return 0;
	}

	if (ps->pm_flags & PMF_USE_ITEM_HELD)
	{ //force to let go first
		return 0;
	}

	if (BG_HoldablesBlocked(ps))
	{
		return 0;
	}

	if (!forcedUse)
	{
		forcedUse = bg_itemlist[ps->stats[STAT_HOLDABLE_ITEM]].giTag;
	}

	if (!BG_IsItemSelectable(ps, forcedUse))
	{
		return 0;
	}

	switch (forcedUse)
	{
	case HI_MEDPAC:
	case HI_MEDPAC_BIG:
		if (ps->stats[STAT_HEALTH] >= ps->stats[STAT_MAX_HEALTH])
		{
			return 0;
		}

		if (ps->stats[STAT_HEALTH] <= 0)
		{
			return 0;
		}

		return 1;
	case HI_SEEKER:
		if (ps->eFlags & EF_SEEKERDRONE)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SEEKER_ALREADYDEPLOYED);
			return 0;
		}

		return 1;
	case HI_SENTRY_GUN:
		if (ps->fd.sentryDeployed)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SENTRY_ALREADYPLACED);
			return 0;
		}

		yawonly[ROLL] = 0;
		yawonly[PITCH] = 0;
		yawonly[YAW] = ps->viewangles[YAW];

		VectorSet( mins, -8, -8, 0 );
		VectorSet( maxs, 8, 8, 24 );

		AngleVectors(yawonly, fwd, NULL, NULL);

		fwdorg[0] = ps->origin[0] + fwd[0]*64;
		fwdorg[1] = ps->origin[1] + fwd[1]*64;
		fwdorg[2] = ps->origin[2] + fwd[2]*64;

		trtest[0] = fwdorg[0] + fwd[0]*16;
		trtest[1] = fwdorg[1] + fwd[1]*16;
		trtest[2] = fwdorg[2] + fwd[2]*16;

		trap->Trace(&tr, ps->origin, mins, maxs, trtest, ps->clientNum, MASK_PLAYERSOLID, qfalse, 0, 0);

		if ((tr.fraction != 1 && tr.entityNum != ps->clientNum) || tr.startsolid || tr.allsolid)
		{
			G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SENTRY_NOROOM);
			return 0;
		}

		return 1;
	case HI_SHIELD:
		mins[0] = -8;
		mins[1] = -8;
		mins[2] = 0;

		maxs[0] = 8;
		maxs[1] = 8;
		maxs[2] = 8;

		AngleVectors (ps->viewangles, fwd, NULL, NULL);
		fwd[2] = 0;
		VectorMA(ps->origin, 64, fwd, dest);
		trap->Trace(&tr, ps->origin, mins, maxs, dest, ps->clientNum, MASK_SHOT, qfalse, 0, 0 );
		if (tr.fraction > 0.9 && !tr.startsolid && !tr.allsolid)
		{
			VectorCopy(tr.endpos, pos);
			VectorSet( dest, pos[0], pos[1], pos[2] - 4096 );
			trap->Trace( &tr, pos, mins, maxs, dest, ps->clientNum, MASK_SOLID, qfalse, 0, 0 );
			if ( !tr.startsolid && !tr.allsolid )
			{
				return 1;
			}
		}
		G_AddEvent(&g_entities[ps->clientNum], EV_ITEMUSEFAIL, SHIELD_NOROOM);
		return 0;
	case HI_JETPACK: //do something?
		return 1;
	case HI_HEALTHDISP:
		return 1;
	case HI_AMMODISP:
		return 1;
	case HI_EWEB:
		return 1;
	case HI_CLOAK:
		return 1;
	default:
		return 1;
	}
}

void saberKnockDown(gentity_t *saberent, gentity_t *saberOwner, gentity_t *other);

void Cmd_ToggleSaber_f(gentity_t *ent)
{
	if (ent->client->ps.fd.forceGripCripple)
	{ //if they are being gripped, don't let them unholster their saber
		if (ent->client->ps.saberHolstered)
		{
			return;
		}
	}

	if (ent->client->ps.saberInFlight)
	{
		if (ent->client->ps.saberEntityNum)
		{ //turn it off in midair
			saberKnockDown(&g_entities[ent->client->ps.saberEntityNum], ent, ent);
		}
		return;
	}

	if (ent->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{
		return;
	}

	if (ent->client->ps.weapon != WP_SABER)
	{
		return;
	}

//	if (ent->client->ps.duelInProgress && !ent->client->ps.saberHolstered)
//	{
//		return;
//	}

	if (ent->client->ps.duelTime >= level.time)
	{
		return;
	}

	if (ent->client->ps.saberLockTime >= level.time)
	{
		return;
	}

	// zyk: noclip does not allow toggle saber
	if ( ent->client->noclip == qtrue )
	{
		return;
	}

	if (ent->client && ent->client->ps.weaponTime < 1)
	{
		if (ent->client->ps.saberHolstered == 2)
		{
			ent->client->ps.saberHolstered = 0;

			if (ent->client->saber[0].soundOn)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOn);
			}
			if (ent->client->saber[1].soundOn &&
				ent->client->saber[1].model[0])
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOn);
			}
		}
		else
		{
			ent->client->ps.saberHolstered = 2;
			if (ent->client->saber[0].soundOff)
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
			}
			if (ent->client->saber[1].soundOff &&
				ent->client->saber[1].model[0])
			{
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
			}
			//prevent anything from being done for 400ms after holster
			ent->client->ps.weaponTime = 400;
		}
	}
}

extern vmCvar_t		d_saberStanceDebug;

extern qboolean WP_SaberCanTurnOffSomeBlades( saberInfo_t *saber );
void Cmd_SaberAttackCycle_f(gentity_t *ent)
{
	int selectLevel = 0;
	qboolean usingSiegeStyle = qfalse;

	if ( !ent || !ent->client )
	{
		return;
	}

	if ( level.intermissionQueued || level.intermissiontime )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"%s (saberAttackCycle)\n\"", G_GetStringEdString( "MP_SVGAME", "CANNOT_TASK_INTERMISSION" ) ) );
		return;
	}

	if ( ent->health <= 0
			|| ent->client->tempSpectate >= level.time
			|| ent->client->sess.sessionTeam == TEAM_SPECTATOR )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "MUSTBEALIVE" ) ) );
		return;
	}


	if ( ent->client->ps.weapon != WP_SABER )
	{
        return;
	}
	/*
	if (ent->client->ps.weaponTime > 0)
	{ //no switching attack level when busy
		return;
	}
	*/

	if (ent->client->saber[0].model[0] && ent->client->saber[1].model[0])
	{ //no cycling for akimbo
		if ( WP_SaberCanTurnOffSomeBlades( &ent->client->saber[1] ) )
		{//can turn second saber off
			if ( ent->client->ps.saberHolstered == 1 )
			{//have one holstered
				//unholster it
				G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOn);
				ent->client->ps.saberHolstered = 0;
				//g_active should take care of this, but...
				ent->client->ps.fd.saberAnimLevel = SS_DUAL;
			}
			else if ( ent->client->ps.saberHolstered == 0 )
			{//have none holstered
				if ( (ent->client->saber[1].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE) )
				{//can't turn it off manually
				}
				else if ( ent->client->saber[1].bladeStyle2Start > 0
					&& (ent->client->saber[1].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE2) )
				{//can't turn it off manually
				}
				else
				{
					//turn it off
					G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
					ent->client->ps.saberHolstered = 1;
					//g_active should take care of this, but...
					ent->client->ps.fd.saberAnimLevel = SS_FAST;
				}
			}

			if (d_saberStanceDebug.integer)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle dual saber blade.\n\"") );
			}
			return;
		}
	}
	else if (ent->client->saber[0].numBlades > 1
		&& WP_SaberCanTurnOffSomeBlades( &ent->client->saber[0] ) )
	{ //use staff stance then.
		if ( ent->client->ps.saberHolstered == 1 )
		{//second blade off
			if ( ent->client->ps.saberInFlight )
			{//can't turn second blade back on if it's in the air, you naughty boy!
				if (d_saberStanceDebug.integer)
				{
					trap->SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle staff blade in air.\n\"") );
				}
				return;
			}
			//turn it on
			G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOn);
			ent->client->ps.saberHolstered = 0;
			//g_active should take care of this, but...
			if ( ent->client->saber[0].stylesForbidden )
			{//have a style we have to use
				WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &selectLevel );
				if ( ent->client->ps.weaponTime <= 0 )
				{ //not busy, set it now
					ent->client->ps.fd.saberAnimLevel = selectLevel;
				}
				else
				{ //can't set it now or we might cause unexpected chaining, so queue it
					ent->client->saberCycleQueue = selectLevel;
				}
			}
		}
		else if ( ent->client->ps.saberHolstered == 0 )
		{//both blades on
			if ( (ent->client->saber[0].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE) )
			{//can't turn it off manually
			}
			else if ( ent->client->saber[0].bladeStyle2Start > 0
				&& (ent->client->saber[0].saberFlags2&SFL2_NO_MANUAL_DEACTIVATE2) )
			{//can't turn it off manually
			}
			else
			{
				//turn second one off
				G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
				ent->client->ps.saberHolstered = 1;
				//g_active should take care of this, but...
				if ( ent->client->saber[0].singleBladeStyle != SS_NONE )
				{
					if ( ent->client->ps.weaponTime <= 0 )
					{ //not busy, set it now
						ent->client->ps.fd.saberAnimLevel = ent->client->saber[0].singleBladeStyle;
					}
					else
					{ //can't set it now or we might cause unexpected chaining, so queue it
						ent->client->saberCycleQueue = ent->client->saber[0].singleBladeStyle;
					}
				}
			}
		}
		if (d_saberStanceDebug.integer)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to toggle staff blade.\n\"") );
		}
		return;
	}

	if (ent->client->saberCycleQueue)
	{ //resume off of the queue if we haven't gotten a chance to update it yet
		selectLevel = ent->client->saberCycleQueue;
	}
	else
	{
		selectLevel = ent->client->ps.fd.saberAnimLevel;
	}

	if (level.gametype == GT_SIEGE &&
		ent->client->siegeClass != -1 &&
		bgSiegeClasses[ent->client->siegeClass].saberStance)
	{ //we have a flag of useable stances so cycle through it instead
		int i = selectLevel+1;

		usingSiegeStyle = qtrue;

		while (i != selectLevel)
		{ //cycle around upward til we hit the next style or end up back on this one
			if (i >= SS_NUM_SABER_STYLES)
			{ //loop back around to the first valid
				i = SS_FAST;
			}

			if (bgSiegeClasses[ent->client->siegeClass].saberStance & (1 << i))
			{ //we can use this one, select it and break out.
				selectLevel = i;
				break;
			}
			i++;
		}

		if (d_saberStanceDebug.integer)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to cycle given class stance.\n\"") );
		}
	}
	else
	{
		selectLevel++;
		if ( selectLevel > ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] )
		{
			selectLevel = FORCE_LEVEL_1;
		}
		if (d_saberStanceDebug.integer)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"SABERSTANCEDEBUG: Attempted to cycle stance normally.\n\"") );
		}
	}
/*
#ifndef FINAL_BUILD
	switch ( selectLevel )
	{
	case FORCE_LEVEL_1:
		trap->SendServerCommand( ent-g_entities, va("print \"Lightsaber Combat Style: %sfast\n\"", S_COLOR_BLUE) );
		break;
	case FORCE_LEVEL_2:
		trap->SendServerCommand( ent-g_entities, va("print \"Lightsaber Combat Style: %smedium\n\"", S_COLOR_YELLOW) );
		break;
	case FORCE_LEVEL_3:
		trap->SendServerCommand( ent-g_entities, va("print \"Lightsaber Combat Style: %sstrong\n\"", S_COLOR_RED) );
		break;
	}
#endif
*/
	if ( !usingSiegeStyle )
	{
		//make sure it's valid, change it if not
		WP_UseFirstValidSaberStyle( &ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &selectLevel );
	}

	if (ent->client->ps.weaponTime <= 0)
	{ //not busy, set it now
		ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = selectLevel;
	}
	else
	{ //can't set it now or we might cause unexpected chaining, so queue it
		ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = selectLevel;
	}
}

qboolean G_OtherPlayersDueling(void)
{
	int i = 0;
	gentity_t *ent;

	while (i < MAX_CLIENTS)
	{
		ent = &g_entities[i];

		if (ent && ent->inuse && ent->client && ent->client->ps.duelInProgress)
		{
			return qtrue;
		}
		i++;
	}

	return qfalse;
}

/*
==================
Cmd_ForceDuel_f

GalaxyRP: [Force Duel] "/engage_fullforceduel" -- the full force half of the private duel system,
adapted from TaystJK (which took it from JAPRO); JA++ spells the command the same way. Both of them
add a SEPARATE command rather than lifting the restriction inside /duel, which is the whole point:
challenging someone for a saber duel gets you a saber duel.
==================
*/
void Cmd_ForceDuel_f(gentity_t *ent)
{
	if (rp_allow_force_duel.integer != 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Force duels are not allowed on this server\n\"");
		return;
	}

	Cmd_EngageDuel_f(ent, 1);
}

// GalaxyRP: [Force Duel] duel_type picks which kind of private duel this press is offering or
// accepting -- 0 an ordinary saber duel, 1 a full force duel. Both kinds run through this one
// function on purpose, the way TaystJK and JA++ both do it, so a full force duel inherits every
// guard below rather than growing a second, drifting copy of them.
void Cmd_EngageDuel_f(gentity_t *ent, int duel_type)
{
	trace_t tr;
	vec3_t forward, fwdOrg;
	const char *minigame = NULL;

	if (!g_privateDuel.integer)
	{
		return;
	}

	if (level.gametype == GT_DUEL || level.gametype == GT_POWERDUEL)
	{ //rather pointless in this mode..
		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", G_GetStringEdString("MP_SVGAME", "NODUEL_GAMETYPE")) );
		return;
	}

	if (ent->health < 1 || ent->client->ps.stats[STAT_HEALTH] < 1)
	{
		return;
	}

	if (G_PlayerIsDowned(ent))
	{
		trap->SendServerCommand(ent->s.number, "print \"^1You cannot do this while you are downed.\n\"");
		return;
	}


	if (ent->client->ps.duelTime >= level.time)
	{
		return;
	}

	if (ent->client->ps.weapon != WP_SABER)
	{
		return;
	}

	// zyk: dont engage if held by a rancor to prevent player being invisible after eaten
	if (ent->client->ps.eFlags2 & EF2_HELD_BY_MONSTER)
	{
		return;
	}

	if (ent->client->ps.saberInFlight)
	{
		return;
	}

	if (ent->client->ps.duelInProgress)
	{
		return;
	}

	minigame = zyk_minigame_name(ent);
	if (minigame != NULL)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"Cannot start a private duel while in a %s\n\"", minigame));
		return;
	}

	AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );

	fwdOrg[0] = ent->client->ps.origin[0] + forward[0]*256;
	fwdOrg[1] = ent->client->ps.origin[1] + forward[1]*256;
	fwdOrg[2] = (ent->client->ps.origin[2]+ent->client->ps.viewheight) + forward[2]*256;

	trap->Trace(&tr, ent->client->ps.origin, NULL, NULL, fwdOrg, ent->s.number, MASK_PLAYERSOLID, qfalse, 0, 0);

	if (tr.fraction != 1 && tr.entityNum < MAX_CLIENTS)
	{
		gentity_t *challenged = &g_entities[tr.entityNum];

		if (!challenged || !challenged->client || !challenged->inuse ||
			challenged->health < 1 || challenged->client->ps.stats[STAT_HEALTH] < 1 ||
			challenged->client->ps.weapon != WP_SABER || challenged->client->ps.duelInProgress ||
			challenged->client->ps.saberInFlight ||
			challenged->client->ps.eFlags2 & EF2_HELD_BY_MONSTER) // zyk: added this condition to prevent player being invisible after eaten by a rancor
		{
			return;
		}

		minigame = zyk_minigame_name(challenged);
		if (minigame != NULL)
		{
			trap->SendServerCommand(ent - g_entities, va("print \"That player is in a %s\n\"", minigame));
			return;
		}

		if (challenged->client->ps.duelIndex == ent->s.number && challenged->client->ps.duelTime >= level.time &&
			level.duel_types[challenged->s.number] == duel_type)
		{
			if (duel_type == 1)
			{
				trap->SendServerCommand( -1, va("print \"%s ^7and %s ^7have begun a ^3Force duel^7!\n\"", challenged->client->pers.netname, ent->client->pers.netname) );
			}
			else
			{
				trap->SendServerCommand( /*challenged-g_entities*/-1, va("print \"%s ^7%s %s!\n\"", challenged->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELACCEPT"), ent->client->pers.netname) );
			}

			level.duel_types[ent->s.number] = duel_type;

			ent->client->ps.duelInProgress = qtrue;
			challenged->client->ps.duelInProgress = qtrue;

			// DAJ_RP: [Training Saber] both fight at their saber's real damage -- see RP_TrainingOff()
			RP_TrainingOff(ent, "for the duel");
			RP_TrainingOff(challenged, "for the duel");

			// GalaxyRP: [Grapple Hook] a duel is sealed off from the map; neither duellist keeps a rope
			// into it. Their hooks are refused for the duel's length by RP_HookMayStayOut().
			if ( ent->client->hook )
			{
				Weapon_HookFree( ent->client->hook );
			}
			if ( challenged->client->hook )
			{
				Weapon_HookFree( challenged->client->hook );
			}

			// zyk: reset hp and shield of both players
			ent->health = 100;
			ent->client->ps.stats[STAT_ARMOR] = 100;

			challenged->health = 100;
			challenged->client->ps.stats[STAT_ARMOR] = 100;

			// zyk: disable jetpack of both players
			Jetpack_Off(ent);
			Jetpack_Off(challenged);

			zyk_wind_down_seeker_drone(ent);
			zyk_wind_down_seeker_drone(challenged);

			if (Jedi_PairIsCloaked(ent))
			{
				Jedi_DecloakPair(ent);
			}
			if (Jedi_PairIsCloaked(challenged))
			{
				Jedi_DecloakPair(challenged);
			}

			ent->client->ps.duelTime = level.time + 2000;
			challenged->client->ps.duelTime = level.time + 2000;

			G_AddEvent(ent, EV_PRIVATE_DUEL, (duel_type == 1) ? 3 : 1);
			G_AddEvent(challenged, EV_PRIVATE_DUEL, (duel_type == 1) ? 3 : 1);

			//Holster their sabers now, until the duel starts (then they'll get auto-turned on to look cool)

			if (!ent->client->ps.saberHolstered)
			{
				if (ent->client->saber[0].soundOff)
				{
					G_Sound(ent, CHAN_AUTO, ent->client->saber[0].soundOff);
				}
				if (ent->client->saber[1].soundOff &&
					ent->client->saber[1].model[0])
				{
					G_Sound(ent, CHAN_AUTO, ent->client->saber[1].soundOff);
				}
				ent->client->ps.weaponTime = 400;
				ent->client->ps.saberHolstered = 2;
			}
			if (!challenged->client->ps.saberHolstered)
			{
				if (challenged->client->saber[0].soundOff)
				{
					G_Sound(challenged, CHAN_AUTO, challenged->client->saber[0].soundOff);
				}
				if (challenged->client->saber[1].soundOff &&
					challenged->client->saber[1].model[0])
				{
					G_Sound(challenged, CHAN_AUTO, challenged->client->saber[1].soundOff);
				}
				challenged->client->ps.weaponTime = 400;
				challenged->client->ps.saberHolstered = 2;
			}
		}
		else
		{
			if (duel_type == 1)
			{
				trap->SendServerCommand( challenged-g_entities, va("cp \"%s ^7has challenged you to a ^3Force duel\n\"", ent->client->pers.netname) );
				trap->SendServerCommand( ent-g_entities, va("cp \"^3Force duel ^7challenge sent to %s\n\"", challenged->client->pers.netname) );
			}
			else
			{
				trap->SendServerCommand( challenged-g_entities, va("cp \"%s ^7%s\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGE")) );
				trap->SendServerCommand( ent-g_entities, va("cp \"%s %s\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGED"), challenged->client->pers.netname) );
			}
		}

		challenged->client->ps.fd.privateDuelTime = 0; //reset the timer in case this player just got out of a duel. He should still be able to accept the challenge.

		ent->client->ps.forceHandExtend = HANDEXTEND_DUELCHALLENGE;
		ent->client->ps.forceHandExtendTime = level.time + 1000;

		ent->client->ps.duelIndex = challenged->s.number;
		ent->client->ps.duelTime = level.time + 5000;

		level.duel_types[ent->s.number] = duel_type;
	}
}

#ifndef FINAL_BUILD
extern stringID_table_t animTable[MAX_ANIMATIONS+1];

void Cmd_DebugSetSaberMove_f(gentity_t *self)
{
	int argNum = trap->Argc();
	char arg[MAX_STRING_CHARS];

	if (argNum < 2)
	{
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );

	if (!arg[0])
	{
		return;
	}

	self->client->ps.saberMove = atoi(arg);
	self->client->ps.saberBlocked = BLOCKED_BOUNCE_MOVE;

	if (self->client->ps.saberMove >= LS_MOVE_MAX)
	{
		self->client->ps.saberMove = LS_MOVE_MAX-1;
	}

	Com_Printf("Anim for move: %s\n", animTable[saberMoveData[self->client->ps.saberMove].animToUse].name);
}

void Cmd_DebugSetBodyAnim_f(gentity_t *self)
{
	int argNum = trap->Argc();
	char arg[MAX_STRING_CHARS];
	int i = 0;

	if (argNum < 2)
	{
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );

	if (!arg[0])
	{
		return;
	}

	while (i < MAX_ANIMATIONS)
	{
		if (!Q_stricmp(arg, animTable[i].name))
		{
			break;
		}
		i++;
	}

	if (i == MAX_ANIMATIONS)
	{
		Com_Printf("Animation '%s' does not exist\n", arg);
		return;
	}

	G_SetAnim(self, NULL, SETANIM_BOTH, i, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);

	Com_Printf("Set body anim to %s\n", arg);
}
#endif

void StandardSetBodyAnim(gentity_t *self, int anim, int flags)
{
	G_SetAnim(self, NULL, SETANIM_BOTH, anim, flags, 0);
}

void DismembermentTest(gentity_t *self);

void Bot_SetForcedMovement(int bot, int forward, int right, int up);

#ifndef FINAL_BUILD
extern void DismembermentByNum(gentity_t *self, int num);
extern void G_SetVehDamageFlags( gentity_t *veh, int shipSurf, int damageLevel );
#endif

qboolean TryGrapple(gentity_t *ent)
{
	if (ent->client->ps.weaponTime > 0)
	{ //weapon busy
		return qfalse;
	}
	if (ent->client->ps.forceHandExtend != HANDEXTEND_NONE)
	{ //force power or knockdown or something
		return qfalse;
	}
	if (ent->client->grappleState)
	{ //already grappling? but weapontime should be > 0 then..
		return qfalse;
	}

	if (ent->client->ps.weapon != WP_SABER && ent->client->ps.weapon != WP_MELEE)
	{
		return qfalse;
	}

	if (ent->client->ps.weapon == WP_SABER && !ent->client->ps.saberHolstered)
	{
		Cmd_ToggleSaber_f(ent);
		if (!ent->client->ps.saberHolstered)
		{ //must have saber holstered
			return qfalse;
		}
	}

	//G_SetAnim(ent, &ent->client->pers.cmd, SETANIM_BOTH, BOTH_KYLE_PA_1, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);
	G_SetAnim(ent, &ent->client->pers.cmd, SETANIM_BOTH, BOTH_KYLE_GRAB, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0);
	if (ent->client->ps.torsoAnim == BOTH_KYLE_GRAB)
	{ //providing the anim set succeeded..
		ent->client->ps.torsoTimer += 500; //make the hand stick out a little longer than it normally would
		if (ent->client->ps.legsAnim == ent->client->ps.torsoAnim)
		{
			ent->client->ps.legsTimer = ent->client->ps.torsoTimer;
		}
		ent->client->ps.weaponTime = ent->client->ps.torsoTimer;
		ent->client->dangerTime = level.time;

		return qtrue;
	}

	return qfalse;
}

void Cmd_TargetUse_f( gentity_t *ent )
{
	if ( trap->Argc() > 1 )
	{
		char sArg[MAX_STRING_CHARS] = {0};
		gentity_t *targ;

		trap->Argv( 1, sArg, sizeof( sArg ) );
		targ = G_Find( NULL, FOFS( targetname ), sArg );

		while ( targ )
		{
			if ( targ->use )
				targ->use( targ, ent, ent );
			targ = G_Find( targ, FOFS( targetname ), sArg );
		}
	}
}

void Cmd_TheDestroyer_f( gentity_t *ent ) {
	if ( !ent->client->ps.saberHolstered || ent->client->ps.weapon != WP_SABER )
		return;

	Cmd_ToggleSaber_f( ent );
}

void Cmd_BotMoveForward_f( gentity_t *ent ) {
	int arg = 4000;
	int bCl = 0;
	char sarg[MAX_STRING_CHARS];

	assert( trap->Argc() > 1 );
	trap->Argv( 1, sarg, sizeof( sarg ) );

	assert( sarg[0] );
	bCl = atoi( sarg );
	Bot_SetForcedMovement( bCl, arg, -1, -1 );
}

void Cmd_BotMoveBack_f( gentity_t *ent ) {
	int arg = -4000;
	int bCl = 0;
	char sarg[MAX_STRING_CHARS];

	assert( trap->Argc() > 1 );
	trap->Argv( 1, sarg, sizeof( sarg ) );

	assert( sarg[0] );
	bCl = atoi( sarg );
	Bot_SetForcedMovement( bCl, arg, -1, -1 );
}

void Cmd_BotMoveRight_f( gentity_t *ent ) {
	int arg = 4000;
	int bCl = 0;
	char sarg[MAX_STRING_CHARS];

	assert( trap->Argc() > 1 );
	trap->Argv( 1, sarg, sizeof( sarg ) );

	assert( sarg[0] );
	bCl = atoi( sarg );
	Bot_SetForcedMovement( bCl, -1, arg, -1 );
}

void Cmd_BotMoveLeft_f( gentity_t *ent ) {
	int arg = -4000;
	int bCl = 0;
	char sarg[MAX_STRING_CHARS];

	assert( trap->Argc() > 1 );
	trap->Argv( 1, sarg, sizeof( sarg ) );

	assert( sarg[0] );
	bCl = atoi( sarg );
	Bot_SetForcedMovement( bCl, -1, arg, -1 );
}

void Cmd_BotMoveUp_f( gentity_t *ent ) {
	int arg = 4000;
	int bCl = 0;
	char sarg[MAX_STRING_CHARS];

	assert( trap->Argc() > 1 );
	trap->Argv( 1, sarg, sizeof( sarg ) );

	assert( sarg[0] );
	bCl = atoi( sarg );
	Bot_SetForcedMovement( bCl, -1, -1, arg );
}

void Cmd_AddBot_f( gentity_t *ent ) {
	//because addbot isn't a recognized command unless you're the server, but it is in the menus regardless
	trap->SendServerCommand( ent-g_entities, va( "print \"%s.\n\"", G_GetStringEdString( "MP_SVGAME", "ONLY_ADD_BOTS_AS_SERVER" ) ) );
}

// zyk: new functions

// zyk: sets the Max HP a player can have in RPG Mode
void set_max_health(gentity_t *ent)
{
	ent->client->pers.max_rpg_health = 100 + (ent->client->pers.level * 2);
	ent->client->ps.stats[STAT_MAX_HEALTH] = ent->client->pers.max_rpg_health;
}

// zyk: sets the Max Shield a player can have in RPG Mode
void set_max_shield(gentity_t *ent)
{
	ent->client->pers.max_rpg_shield = (int)ceil(((ent->client->pers.skill_levels[30] * 1.0)/5) * ent->client->pers.max_rpg_health);
}

// zyk: gives credits to the player
void add_credits(gentity_t *ent, int credits)
{
	long long new_credits = (long long)ent->client->pers.credits + (long long)credits;

	if (new_credits > rp_max_rpg_credits.integer)
		new_credits = rp_max_rpg_credits.integer;

	if (new_credits < 0)
		new_credits = 0;

	ent->client->pers.credits = (int)new_credits;
}

void remove_credits(gentity_t *ent, int credits)
{
	long long new_credits = (long long)ent->client->pers.credits - (long long)credits;

	if (new_credits > rp_max_rpg_credits.integer)
		new_credits = rp_max_rpg_credits.integer;

	if (new_credits < 0)
		new_credits = 0;

	ent->client->pers.credits = (int)new_credits;
}

// zyk: loads settings valid both to Admin-Only Mode and to RPG Mode
void zyk_load_common_settings(gentity_t *ent)
{
	if (ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_SABER))
	{
		ent->client->ps.weapon = WP_SABER;

		if (ent->client->pers.player_settings & (1 << 11))
		{
			ent->client->ps.saberHolstered = 2;
		}
	}
	else
	{
		ent->client->ps.weapon = WP_MELEE;
	}

	if (!(ent->client->saber[0].model[0] && ent->client->saber[1].model[0]) && !(ent->client->saber[0].saberFlags&SFL_TWO_HANDED))
	{ // zyk: Single Saber
		ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel = ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE];

		if (ent->client->sess.amrpgmode == 2 && ent->client->pers.skill_levels[5] >= 1)
		{
			ent->client->ps.fd.saberAnimLevel = SS_FAST;
		}
	}
}

// zyk: saves info into the player account file. If save_char_file is qtrue, this function must save the char file
void save_account(gentity_t *ent, qboolean save_char_file)
{
	// zyk: used to prevent account save in map change time or before loading account after changing map
	if (level.voteExecuteTime < level.time && ent->client->pers.connected == CON_CONNECTED &&
		ent->client->sess.amrpgmode == 2
		)
	{ // zyk: players can only save things if server is not at RP Mode or if it is allowed in config
		if (save_char_file == qtrue)
		{  // zyk: save the RPG char
			update_current_character_and_account(ent);
		}
		else
		{ // zyk: save the main account file
			update_accounts_table_row_with_current_values(ent);
		}
	}
}

#define DICE_MAX_DICE 10
#define DICE_MAX_FACES 100

static int zyk_roll_die(int max_value)
{
	unsigned int range = (unsigned int)RAND_MAX + 1u;
	unsigned int usable = 0;
	unsigned int result = 0;

	if (max_value < 2)
	{
		return 1;
	}

	usable = range - (range % (unsigned int)max_value);

	do
	{
		result = (unsigned int)rand();
	} while (result >= usable);

	return (int)(result % (unsigned int)max_value) + 1;
}

#define DICE_ARG_TOO_BIG 1000000

static qboolean zyk_parse_dice_digits(const char **cursor, int *out_value)
{
	const char *scan = *cursor;
	int value = 0;

	if (!isdigit((unsigned char)*scan))
	{
		return qfalse;
	}

	while (isdigit((unsigned char)*scan))
	{
		if (value < DICE_ARG_TOO_BIG)
		{
			value = (value * 10) + (*scan - '0');
		}

		scan++;
	}

	if (value > DICE_ARG_TOO_BIG)
	{
		value = DICE_ARG_TOO_BIG;
	}

	*cursor = scan;
	*out_value = value;

	return qtrue;
}

static qboolean zyk_parse_dice_arg(const char *arg, int *number_of_dice, int *max_value)
{
	const char *cursor = arg;
	int first = 0;
	int second = 0;

	if (zyk_parse_dice_digits(&cursor, &first) == qfalse)
	{
		return qfalse;
	}

	if (*cursor == 'd' || *cursor == 'D')
	{
		cursor++;

		if (zyk_parse_dice_digits(&cursor, &second) == qfalse)
		{
			return qfalse;
		}

		if (*cursor != '\0')
		{ // zyk: trailing junk, e.g. "2d6x"
			return qfalse;
		}

		*number_of_dice = first;
		*max_value = second;

		return qtrue;
	}

	if (*cursor != '\0')
	{ // zyk: trailing junk, e.g. "20x"
		return qfalse;
	}

	*number_of_dice = 1;
	*max_value = first;

	return qtrue;
}

static void zyk_send_chat_within_distance(gentity_t *ent, int distance, const char *message)
{
	int i = 0;
	gentity_t *other = NULL;

	for (i = 0; i < level.maxclients; i++)
	{
		other = &g_entities[i];

		if (!other->inuse || !other->client || other->client->pers.connected != CON_CONNECTED)
			continue;

		if (check_if_player_ignores_other(ent, other))
			continue;

		if (Distance(ent->client->ps.origin, other->client->ps.origin) <= distance ||
			zyk_ignores_chat_distance(other) ||
			other->client->sess.sessionTeam == TEAM_SPECTATOR)
		{
			trap->SendServerCommand(i, message);
		}
	}
}

static qboolean zyk_dice_cooldown_ok(gentity_t *ent)
{
	if (ent->client->pers.dice_roll_timer > level.time)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"^1You must wait ^3%.1f ^1more seconds before rolling again.\n\"",
			(ent->client->pers.dice_roll_timer - level.time) / 1000.0f));
		return qfalse;
	}

	ent->client->pers.dice_roll_timer = level.time + rp_dice_roll_cooldown.integer;

	return qtrue;
}

/*
==================
Cmd_Roll_f
==================
*/

static void zyk_dice_usage(gentity_t *ent, const char *cmdName)
{
	trap->SendServerCommand(ent - g_entities,
		va("print \"^1Command Usage: ^2/%s ^3<faces> ^1or ^2/%s ^3<dice>d<faces>^1.\n^1Examples: ^2/%s ^320^1, ^2/%s ^32d6\n\"",
			cmdName, cmdName, cmdName, cmdName));
}

static void zyk_roll(gentity_t *ent, int distance, const char *cmdName)
{
	char arg1[MAX_STRING_CHARS] = { 0 };
	char results[MAX_STRING_CHARS] = { 0 };
	char message[MAX_STRING_CHARS] = { 0 };
	int rolls[DICE_MAX_DICE];
	int number_of_dice = 0;
	int max_value = 0;
	int total = 0;
	int i = 0;

	if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_SILENCED))
		return;

	if (trap->Argc() != 2)
	{
		zyk_dice_usage(ent, cmdName);
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	if (zyk_parse_dice_arg(arg1, &number_of_dice, &max_value) == qfalse)
	{
		zyk_dice_usage(ent, cmdName);
		return;
	}

	if (number_of_dice < 1 || number_of_dice > DICE_MAX_DICE)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"^1You can roll between ^31 ^1and ^3%d ^1dice at once.\n\"", DICE_MAX_DICE));
		return;
	}

	if (max_value < 2 || max_value > DICE_MAX_FACES)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"^1A die must have between ^32 ^1and ^3%d ^1faces.\n\"", DICE_MAX_FACES));
		return;
	}

	if (zyk_dice_cooldown_ok(ent) == qfalse)
	{
		return;
	}

	for (i = 0; i < number_of_dice; i++)
	{
		rolls[i] = zyk_roll_die(max_value);
		total += rolls[i];
	}

	if (number_of_dice == 1)
	{
		Com_sprintf(message, sizeof(message), "chat \"^3<Dice Roll> %s^2 rolled a ^3%d^2 out of ^3%d\n\"",
			ent->client->pers.netname, rolls[0], max_value);
	}
	else
	{
		for (i = 0; i < number_of_dice; i++)
		{
			Q_strcat(results, sizeof(results), va("%s^3%d", (i == 0) ? "" : "^2, ", rolls[i]));
		}

		Com_sprintf(message, sizeof(message), "chat \"^3<Dice Roll> %s^2 rolled %s ^2on ^3%dd%d^2, for a total of ^3%d\n\"",
			ent->client->pers.netname, results, number_of_dice, max_value, total);
	}

	zyk_send_chat_within_distance(ent, distance, message);
}

void Cmd_Roll_f(gentity_t *ent) {
	zyk_roll(ent, ACTION_DISTANCE, "roll");
}

void Cmd_RollAll_f(gentity_t *ent) {
	zyk_roll(ent, BROADCAST_DISTANCE, "rollall");
}

/*
==================
Cmd_FlipCoin_f
==================
*/

static void zyk_flip_coin(gentity_t *ent, int distance, const char *cmdName)
{
	char message[MAX_STRING_CHARS] = { 0 };

	if (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_SILENCED))
		return;

	if (trap->Argc() != 1)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"^1Command Usage: ^2/%s\n\"", cmdName));
		return;
	}

	if (zyk_dice_cooldown_ok(ent) == qfalse)
	{
		return;
	}

	Com_sprintf(message, sizeof(message), "chat \"^3%s^2 flipped a coin that landed on ^3%s.\n\"",
		ent->client->pers.netname, (zyk_roll_die(2) == 1) ? "HEADS" : "TAILS");

	zyk_send_chat_within_distance(ent, distance, message);
}

void Cmd_FlipCoin_f(gentity_t *ent) {
	zyk_flip_coin(ent, ACTION_DISTANCE, "flipcoin");
}

void Cmd_FlipCoinAll_f(gentity_t *ent) {
	zyk_flip_coin(ent, BROADCAST_DISTANCE, "flipcoinall");
}

qboolean jetpack_command_allowed(gentity_t *ent)
{
	if (!ent || !ent->client)
		return qfalse;

	if (rp_allow_jetpack_command.integer <= 0)
		return qfalse;

	if (ent->client->sess.loggedin == qtrue)
		return (ent->client->pers.skill_levels[34] > 0) ? qtrue : qfalse;

	return (rp_allow_jetpack_command.integer == 1) ? qtrue : qfalse;
}

qboolean RP_JetpackGrantAllowed(gentity_t *ent)
{
	if (!jetpack_command_allowed(ent))
		return qfalse;

	if (level.gametype == GT_SIEGE || level.gametype == GT_JEDIMASTER)
		return qfalse;

	return qtrue;
}

qboolean RP_GrappleAllowed(gentity_t *ent)
{
	if (!ent || !ent->client)
		return qfalse;

	if (rp_allow_grapple_hook.integer <= 0)
		return qfalse;

	if (ent->client->sess.loggedin == qtrue)
		return (ent->client->pers.skill_levels[55] > 0) ? qtrue : qfalse;

	return (rp_allow_grapple_hook.integer == 1) ? qtrue : qfalse;
}

// zyk: initialize RPG skills of this player
void initialize_rpg_skills(gentity_t *ent)
{
	if (ent->client->sess.amrpgmode == 2)
	{

		// zyk: loading Jump value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_LEVITATION)) && ent->client->pers.skill_levels[0] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_LEVITATION);
		if (ent->client->pers.skill_levels[0] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_LEVITATION);
		ent->client->ps.fd.forcePowerLevel[FP_LEVITATION] = ent->client->pers.skill_levels[0];

		// zyk: loading Push value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_PUSH)) && ent->client->pers.skill_levels[1] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_PUSH);
		if (ent->client->pers.skill_levels[1] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PUSH);
		ent->client->ps.fd.forcePowerLevel[FP_PUSH] = ent->client->pers.skill_levels[1];

		// zyk: loading Pull value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_PULL)) && ent->client->pers.skill_levels[2] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_PULL);
		if (ent->client->pers.skill_levels[2] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PULL);
		ent->client->ps.fd.forcePowerLevel[FP_PULL] = ent->client->pers.skill_levels[2];

		// zyk: loading Speed value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SPEED)) && ent->client->pers.skill_levels[3] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SPEED);
		if (ent->client->pers.skill_levels[3] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SPEED);
		ent->client->ps.fd.forcePowerLevel[FP_SPEED] = ent->client->pers.skill_levels[3];

		// zyk: loading Sense value
		// GalaxyRP fix: [Classes] the rpg_class==2/3/5/8 (force-less classes skip Sense) branch used to
		// be here. rpg_class is permanently 0 now that character classes are gone, so this was
		// unreachable; only the else branch (load Sense normally) is kept, unconditionally.
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SEE)) && ent->client->pers.skill_levels[4] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SEE);
		if (ent->client->pers.skill_levels[4] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SEE);
		ent->client->ps.fd.forcePowerLevel[FP_SEE] = ent->client->pers.skill_levels[4];

		// zyk: loading Saber Offense value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SABER_OFFENSE)) && ent->client->pers.skill_levels[5] > 0)
		{
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SABER_OFFENSE);
		}
		if (ent->client->pers.skill_levels[5] == 0)
		{
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SABER_OFFENSE);
		}
		ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] = ent->client->pers.skill_levels[5];

		// zyk: giving the saber if he has Saber Attack skill level greater than 0
		if (ent->client->pers.skill_levels[5] > 0)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);
		}
		else
		{
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);
		}

		// zyk: loading Saber Defense value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SABER_DEFENSE)) && ent->client->pers.skill_levels[6] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SABER_DEFENSE);
		if (ent->client->pers.skill_levels[6] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SABER_DEFENSE);
		ent->client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE] = ent->client->pers.skill_levels[6];

		// zyk: loading Saber Throw value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_SABERTHROW)) && ent->client->pers.skill_levels[7] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_SABERTHROW);
		if (ent->client->pers.skill_levels[7] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_SABERTHROW);
		ent->client->ps.fd.forcePowerLevel[FP_SABERTHROW] = ent->client->pers.skill_levels[7];

		// zyk: loading Absorb value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_ABSORB)) && ent->client->pers.skill_levels[8] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_ABSORB);
		if (ent->client->pers.skill_levels[8] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_ABSORB);

		if (ent->client->pers.skill_levels[8] < 4)
			ent->client->ps.fd.forcePowerLevel[FP_ABSORB] = ent->client->pers.skill_levels[8];
		else
			ent->client->ps.fd.forcePowerLevel[FP_ABSORB] = FORCE_LEVEL_3;

		// zyk: loading Heal value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_HEAL)) && ent->client->pers.skill_levels[9] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_HEAL);
		if (ent->client->pers.skill_levels[9] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_HEAL);
		ent->client->ps.fd.forcePowerLevel[FP_HEAL] = ent->client->pers.skill_levels[9];

		// zyk: loading Protect value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_PROTECT)) && ent->client->pers.skill_levels[10] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_PROTECT);
		if (ent->client->pers.skill_levels[10] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_PROTECT);

		ent->client->ps.fd.forcePowerLevel[FP_PROTECT] = ent->client->pers.skill_levels[10];

		// zyk: loading Mind Trick value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_TELEPATHY)) && ent->client->pers.skill_levels[11] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_TELEPATHY);
		if (ent->client->pers.skill_levels[11] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TELEPATHY);
		ent->client->ps.fd.forcePowerLevel[FP_TELEPATHY] = ent->client->pers.skill_levels[11];

		// zyk: loading Team Heal value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_HEAL)) && ent->client->pers.skill_levels[12] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_TEAM_HEAL);
		if (ent->client->pers.skill_levels[12] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TEAM_HEAL);
		ent->client->ps.fd.forcePowerLevel[FP_TEAM_HEAL] = ent->client->pers.skill_levels[12];

		// zyk: loading Lightning value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_LIGHTNING)) && ent->client->pers.skill_levels[13] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_LIGHTNING);
		if (ent->client->pers.skill_levels[13] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_LIGHTNING);

		if (ent->client->pers.skill_levels[13] < 4)
			ent->client->ps.fd.forcePowerLevel[FP_LIGHTNING] = ent->client->pers.skill_levels[13];
		else
			ent->client->ps.fd.forcePowerLevel[FP_LIGHTNING] = FORCE_LEVEL_3;

		// zyk: loading Grip value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_GRIP)) && ent->client->pers.skill_levels[14] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_GRIP);
		if (ent->client->pers.skill_levels[14] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_GRIP);
		ent->client->ps.fd.forcePowerLevel[FP_GRIP] = ent->client->pers.skill_levels[14];

		// zyk: loading Drain value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_DRAIN)) && ent->client->pers.skill_levels[15] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_DRAIN);
		if (ent->client->pers.skill_levels[15] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_DRAIN);
		ent->client->ps.fd.forcePowerLevel[FP_DRAIN] = ent->client->pers.skill_levels[15];

		// zyk: loading Rage value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_RAGE)) && ent->client->pers.skill_levels[16] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_RAGE);
		if (ent->client->pers.skill_levels[16] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_RAGE);

		ent->client->ps.fd.forcePowerLevel[FP_RAGE] = ent->client->pers.skill_levels[16];

		// zyk: loading Team Energize value
		if (!(ent->client->ps.fd.forcePowersKnown & (1 << FP_TEAM_FORCE)) && ent->client->pers.skill_levels[17] > 0)
			ent->client->ps.fd.forcePowersKnown |= (1 << FP_TEAM_FORCE);
		if (ent->client->pers.skill_levels[17] == 0)
			ent->client->ps.fd.forcePowersKnown &= ~(1 << FP_TEAM_FORCE);
		ent->client->ps.fd.forcePowerLevel[FP_TEAM_FORCE] = ent->client->pers.skill_levels[17];

		// zyk: loading Melee
		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_MELEE)))
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_MELEE);

		zyk_load_common_settings(ent);

		ent->client->pers.sense_health_timer = 0;

		// zyk: used to add a cooldown between each flame
		ent->client->cloakDebReduce = 0;

		ent->client->pers.max_force_power = (int)ceil((RP_MAX_FORCE_POWER / (double)RP_FORCE_POWER_SKILL_MAX_LEVEL) * ent->client->pers.skill_levels[54]);
		ent->client->ps.fd.forcePowerMax = ent->client->pers.max_force_power;
		ent->client->ps.fd.forcePower = ent->client->ps.fd.forcePowerMax;

		// zyk: setting rpg control attributes
		ent->client->pers.thermal_vision = qfalse;
		ent->client->pers.thermal_vision_cooldown_time = 0;

		ent->client->pers.buy_sell_timer = 0;
		ent->client->pers.vertical_dfa_timer = 0;

		// zyk: loading initial RPG weapons
		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_STUN_BATON)) && ent->client->pers.skill_levels[18] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_STUN_BATON);
		if (ent->client->pers.skill_levels[18] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_STUN_BATON);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_PISTOL)) && ent->client->pers.skill_levels[19] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_PISTOL);
		if (ent->client->pers.skill_levels[19] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BRYAR_PISTOL);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BLASTER)) && ent->client->pers.skill_levels[20] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BLASTER);
		if (ent->client->pers.skill_levels[20] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BLASTER);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_DISRUPTOR)) && ent->client->pers.skill_levels[21] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DISRUPTOR);
		if (ent->client->pers.skill_levels[21] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_DISRUPTOR);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BOWCASTER)) && ent->client->pers.skill_levels[22] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BOWCASTER);
		if (ent->client->pers.skill_levels[22] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BOWCASTER);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_REPEATER)) && ent->client->pers.skill_levels[23] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_REPEATER);
		if (ent->client->pers.skill_levels[23] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_REPEATER);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_DEMP2)) && ent->client->pers.skill_levels[24] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DEMP2);
		if (ent->client->pers.skill_levels[24] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_DEMP2);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_FLECHETTE)) && ent->client->pers.skill_levels[25] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_FLECHETTE);
		if (ent->client->pers.skill_levels[25] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_FLECHETTE);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_ROCKET_LAUNCHER)) && ent->client->pers.skill_levels[26] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_ROCKET_LAUNCHER);
		if (ent->client->pers.skill_levels[26] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_ROCKET_LAUNCHER);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_CONCUSSION)) && ent->client->pers.skill_levels[27] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_CONCUSSION);
		if (ent->client->pers.skill_levels[27] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_CONCUSSION);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_BRYAR_OLD)) && ent->client->pers.skill_levels[28] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_BRYAR_OLD);
		if (ent->client->pers.skill_levels[28] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BRYAR_OLD);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_THERMAL)) && ent->client->pers.skill_levels[43] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_THERMAL);
		if (ent->client->pers.skill_levels[43] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_THERMAL);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_TRIP_MINE)) && ent->client->pers.skill_levels[44] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_TRIP_MINE);
		if (ent->client->pers.skill_levels[44] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_TRIP_MINE);

		if (!(ent->client->ps.stats[STAT_WEAPONS] & (1 << WP_DET_PACK)) && ent->client->pers.skill_levels[45] > 0)
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DET_PACK);
		if (ent->client->pers.skill_levels[45] == 0)
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_DET_PACK);

		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_SEEKER) & ~(1 << HI_BINOCULARS) & ~(1 << HI_SENTRY_GUN) & ~(1 << HI_EWEB) & ~(1 << HI_CLOAK) & ~(1 << HI_SHIELD) & ~(1 << HI_MEDPAC) & ~(1 << HI_MEDPAC_BIG) & ~(1 << HI_JETPACK);

		if (ent->client->pers.skill_levels[46] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_BINOCULARS);

		if (ent->client->pers.skill_levels[47] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC);

		if (ent->client->pers.skill_levels[48] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SENTRY_GUN);

		if (ent->client->pers.skill_levels[49] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SEEKER);

		if (ent->client->pers.skill_levels[50] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_EWEB);

		if (ent->client->pers.skill_levels[51] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_MEDPAC_BIG);

		if (ent->client->pers.skill_levels[52] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_SHIELD);

		if (ent->client->pers.skill_levels[53] > 0)
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_CLOAK);

		if (RP_JetpackGrantAllowed(ent))
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);

		// zyk: loading initial health of the player
		set_max_health(ent);
		ent->health = ent->client->pers.max_rpg_health;
		ent->client->ps.stats[STAT_HEALTH] = ent->health;

		// zyk: loading initial shield of the player
		set_max_shield(ent);
		ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.max_rpg_shield;
	}
}

/*
==================
Cmd_DateTime_f
==================
*/
void Cmd_DateTime_f( gentity_t *ent ) {
	time_t current_time;

	time(&current_time);
	// zyk: shows current server date and time
	trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", ctime(&current_time)) ); 
}

// GalaxyRP: Sets up the GalaxyRP directory
static void zyk_mkdir_one( const char *path )
{
#if defined(_WIN32)
	_mkdir( path );
#else
	mkdir( path, 0755 );
#endif
}

void zyk_create_dir(char *file_path)
{
	char path[MAX_OSPATH];
	int i;

	if (!file_path || !file_path[0])
	{
		return;
	}

	Com_sprintf(path, sizeof(path), "GalaxyRP/%s", file_path);

	// zyk: i starts at 1 so a leading separator is never taken for a component of its own
	for (i = 1; path[i] != '\0'; i++)
	{
		if (path[i] == '/' || path[i] == '\\')
		{
			char separator = path[i];

			path[i] = '\0';
			zyk_mkdir_one(path);
			path[i] = separator;
		}
	}

	zyk_mkdir_one(path);
}

extern void zyk_set_entity_field(gentity_t *ent, char *key, char *value);
extern void zyk_spawn_entity(gentity_t *ent);
extern void zyk_main_set_entity_field(gentity_t *ent, char *key, char *value);
extern void zyk_main_spawn_entity(gentity_t *ent);
extern qboolean zyk_spawn_strings_full(gentity_t *ent);

/*
==================
Cmd_LogoutAccount_f
==================
*/
void Cmd_LogoutAccount_f( gentity_t *ent ) {

	if (G_PlayerIsDowned(ent))
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot do this while you are downed.\n\"");
		return;
	}

	if ( ent->client->hook )
	{
		Weapon_HookFree( ent->client->hook );
	}

	save_account(ent, qtrue);

	if (ent->client->pers.being_mind_controlled != -1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You cant logout while being mind-controlled.\n\"" );
		return;
	}

	if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot logout while in a Duel Tournament\n\"");
		return;
	}

	if (level.melee_mode > 0 && level.melee_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot logout while in a Melee Battle\n\"");
		return;
	}

	if (ent->client->ps.duelInProgress == qtrue)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot logout while in a private duel\n\"");
		return;
	}

	RP_EntGrabCancel(ent, qtrue);

	// zyk: saving the not logged player mode in session
	ent->client->sess.amrpgmode = 0;

	ent->client->sess.rpgchar[0] = '\0';

	ent->client->sess.accountID = 0;

	ent->client->pers.CharID = 0;

	ent->client->pers.description[0] = '\0';

	ent->client->pers.bitvalue = 0;

	ent->client->pers.player_settings = 0;

	ent->client->pers.player_statuses &= (1 << PLAYER_STATUS_SILENCED);

	// zyk: initializing mind control attributes used in RPG mode
	ent->client->pers.being_mind_controlled = -1;
	ent->client->pers.mind_controlled1_id = -1;

	// zyk: resetting the forcePowerMax to the logged-out maximum (RP_MAX_FORCE_POWER_LOGGED_OUT, g_local.h)
	ent->client->ps.fd.forcePowerMax = RP_MAX_FORCE_POWER_LOGGED_OUT;

	do_scale(ent, 100);

	ent->client->pers.max_rpg_health = 100;
	ent->client->pers.max_rpg_shield = 0;
	memset(ent->client->pers.skill_levels, 0, sizeof(ent->client->pers.skill_levels));

	// zyk: resetting max hp and shield to 100
	ent->client->ps.stats[STAT_MAX_HEALTH] = 100;

	if (ent->health > 100)
		ent->health = 100;

	if (ent->client->ps.stats[STAT_ARMOR] > 100)
		ent->client->ps.stats[STAT_ARMOR] = 100;

	zyk_stop_active_force_powers(ent);

	zyk_reset_force_powerups(ent);

	zyk_stop_active_holdables(ent);

	RP_ClearPhaseMode(ent);

	zyk_remove_guns(ent);

	zyk_reset_fuel(ent);

	zyk_reset_saber_style(ent);

	if (level.gametype == GT_JEDIMASTER || level.gametype == GT_SIEGE)
	{
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);
		ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_BRYAR_PISTOL);
	}

	ent->flags &= ~(FL_GODMODE | FL_NOTARGET);
	ent->client->noclip = qfalse;

	ent->client->sess.loggedin = qfalse;

	trap->SendServerCommand(ent->s.number, va("supdateloggedin %i\n", ent->client->sess.loggedin));

	trap->SendServerCommand(-1, va("chat \"^3%s ^2logged out\n\"", ent->client->pers.netname));

	trap->SendServerCommand(ent - g_entities, "print \"^2You have sucessfully logged out.\n\"");
	trap->SendServerCommand(ent - g_entities, "cp \"^2You have sucessfully logged out.\n\"");

	zyk_schedule_relog_kill(ent);
}

// GalaxyRP (Alex): [Skill Display] This method returns a color string based on the ability alignment. Used in displaying the skill to the user.
char *color_ability(skill_t skill) {
	if (strcmp(skill.alignment, "light") == 0) {
		return "^4";
	}
	else if (strcmp(skill.alignment, "dark") == 0) {
		return "^1";
	}
	else if (strcmp(skill.alignment, "neutral") == 0) {
		return "^5";
	}
	else if (strcmp(skill.alignment, "merc") == 0) {
		return "^3";
	}

	return "^7";
}

char *add_spacing_for_columns(skill_t skill, char* message, int skill_id) {

	int array_stop = 20 - strlen(skill.skill_name);

	if (skill_id >= 10) {
		array_stop--;
	}

	for (int i = 0; i < array_stop; i++)
	{
		strcpy(message, va("%s ", message));
	}

	strcpy(message, va("%s", message));

	return message;
}

static const char *zyk_skill_category_name( const char *typed )
{
	static const char *categories[] = { "force", "weapons", "protect", "ammo", "items" };
	int i;

	for ( i = 0; i < (int)ARRAY_LEN( categories ); i++ )
	{
		if ( typed && Q_stricmp( typed, categories[i] ) == 0 )
		{
			return categories[i];
		}
	}

	return NULL;
}

void zyk_list_player_skills(gentity_t *ent, gentity_t *target_ent, const char *arg1)
{
	char message[1024];
	int i = 0;
	int display_counter = 0;

	strcpy(message,"");

	for (int i = 0; i < ARRAY_LEN(skills); i++)
	{
		// GalaxyRP (Alex) Ignore skill 38 (Unique Skill)
		if (i == 38)
		{
			continue;
		}

		if (strcmp(skills[i].category, arg1) == 0) {
			strcpy(message, va("%s%s%d - %s: %d/%d", message, color_ability(skills[i]), i + 1, skills[i].skill_name, ent->client->pers.skill_levels[i], skills[i].max_level));

			if (display_counter % 2 != 0) {
				strcpy(message, va("%s\n", message));
			}
			else {
				strcpy(message, va("%s", add_spacing_for_columns(skills[i], message, i + 1)));
			}
			display_counter++;
		}
	}

	trap->SendServerCommand(target_ent->s.number, va("print \"%s\n\n\"", message));
}

void list_rpg_info(gentity_t *ent, gentity_t *target_ent)
{ // zyk: lists general RPG info of this player
	// DAJ_RP fix: [Admin] the two tips at the end are for a player reading their own info (/list); an admin
	// looking at someone else's (/players <player>) used to get them too, worded as if to that player
	trap->SendServerCommand(target_ent->s.number, va("print \"\n^2Account: ^7%s\n^2Character: ^7%s\n\n^3Level: ^7%d/%d\n^3XP: ^7%d/%d\n^3Skill Points: ^7%d\n\n%s\"", ent->client->sess.filename, ent->client->sess.rpgchar, ent->client->pers.level, rp_rpg_max_level.integer, ent->client->pers.xp, check_xp(ent->client->pers.level), ent->client->pers.skillpoints,
		(ent == target_ent) ? "^7Use ^2/list help ^7to see console commands\n^7Use ^2/list <skill number> ^7to see information about a specific skill\n\n" : ""));
}

// GalaxyRP: [Admin] /clientprint's limit and its one help line -- the usage message, /list commands and
// /adminlist all print this same text. The number in the text is the limit; change both together.
#define RP_CLIENTPRINT_MAX 200
#define RP_CLIENTPRINT_HELP "^3/clientprint <player name or ID, or -1 for everyone> <message>: ^7Shows the message in the middle of their screen. The message is the rest of the line (at most 200 characters)."

/*
==================
Cmd_ListAccount_f
==================
*/
void Cmd_ListAccount_f( gentity_t *ent ) {
	if (ent->client->sess.amrpgmode == 2)
	{
		if (trap->Argc() == 1)
		{ // zyk: if player didnt pass arguments, lists general info
			list_rpg_info(ent, ent);
		}
		else
		{
			char message[1024];
			char arg1[MAX_STRING_CHARS];
			int i = 0;

			strcpy(message,"");

			trap->Argv(1, arg1, sizeof( arg1 ));

			if (Q_stricmp( arg1, "help" ) == 0)
			{
				trap->SendServerCommand(ent-g_entities, "print \"\n^2/list force: ^7lists force power skills\n^2/list weapons: ^7lists weapon skills\n^2/list protect: ^7lists protection skills\n^2/list ammo: ^7lists ammo skills\n^2/list items: ^7lists holdable items skills\n^2/list [skill number]: ^7lists info about a skill\n^2/list commands: ^7lists the Galaxy Mod console commands\n^2/list chat: ^7lists chat commands and RP chat modifiers\n\"");
				trap->SendServerCommand(ent-g_entities, "print \"^2/list md3 <folder> <page>: ^7lists the .md3 model files, starting in map_objects. ^2/ ^7for the models folder itself\n\
^2/list glm <folder> <page>: ^7lists the .glm model files, starting in players. ^2/ ^7for the models folder itself\n\
^2/list effects <folder> <page>: ^7lists the effect files\n\
^2/list sounds <folder> <page>: ^7lists the sound files\n\
^2/list music <folder> <page>: ^7lists the music files\n\
^2/list maps <page>: ^7lists every map file on the server\n\
^2/list npcs <text> <page>: ^7lists the NPC types, or those with that text in their name\n\
^2/list vehicles <text> <page>: ^7lists the vehicle types, or those with that text in their name\n\
^7The folder, text and page are all optional.\n\n\"");
			}
			else if (zyk_skill_category_name(arg1))
			{ // DAJ_RP fix: [Skills] no pool copy of the typed name -- see zyk_skill_category_name()
				zyk_list_player_skills(ent, ent, zyk_skill_category_name(arg1));
			}
			else if (Q_stricmp( arg1, "commands" ) == 0)
			{
			trap->SendServerCommand(ent - g_entities, "print \"\n^2Commands\n\n^3--------Account--------\n\
^3/new ^7or ^3/register <login> <password>: ^7Creates a new account.\n\
^3/login <login> <password>: ^7Loads the account.\n\
^3/logout: ^7Logs out the account.\n\
^3/changepassword <new password>: ^7Changes the account password.\n\
^3/settings <number (optional)>: ^7Turns on or off account settings. Run with no arguments to list your settings.\n\n\
^3--------Character--------\n\
^3/attributes <description>: ^7Sets your character's description, up to 255 characters. Put it in double quotes; empty double quotes reset it. Others can view it with ^3/ex ^7command.\n\
^3/examine ^7or ^3/ex <player name>: ^7Displays someone's character description.\n\
^3/char <new/use/remove (optional)> <character name (optional)>: ^7Creates/switches to/deletes a character. Run with no arguments to list your characters.\n\n\" ");
				trap->SendServerCommand(ent - g_entities, "print \"^3--------Admin Commands--------\n\
^3/adminlist <command ID (optional)>: ^7Lists admin commands and their availability for current player. If you pass a command ID as argument, shows info about it.\n\
^3/adminlist show <player id or name>: ^7Shows admin commands of another player. ^1(only for Admins with Give Admin)\n\
^3/adminup <player name> <command number or all>: ^7Gives the player an admin command, or all of them. ^1(only for Admins with Give Admin)\n\
^3/admindown <player name> <command number or all>: ^7Removes an admin command, or all of them, from the player. ^1(only for Admins with Give Admin)\n\
^3/entitiesandremaps: ^7Shows the commands to add, edit, move and save entities, and to remap shaders. ^1(only for Admins with Entities and Remaps)\n\
^3/playmusic <file path>: ^7Replaces the current map music for all players with the song given.\n\"");
				trap->SendServerCommand(ent - g_entities, "print \"^3/levelup <player name> <number of levels (optional)>: ^7Raises the player's level by one, or by that many.\n\
^3/leveldown <player name> <number of levels (optional)>: ^7Lowers the player's level by one, or by that many.\n\
^3/givexp <player name>: ^7Gives the player one xp.\n\
^3/removexp <player name>: ^7Removes one xp from the player.\n\
^3/skillup <player name> <skill number> <number of levels (optional)>: ^7upgrades a skill.\n\
^3/skilldown <player name> <skill number> <number of levels (optional)>: ^7downgrades a skill.\n\
^3/god: ^7Makes you invincible.\n\
^3/notarget <player name (optional)>: ^7Makes NPCs, turrets and seekers ignore you, or another player.\n\
^3/players <player name (optional)> <force/weapons/protect/ammo/items (optional)>: ^7With no name, lists every player's ID, name, IP and login status. With a name, shows their account, character, level, XP and skill points; add a category to see their skill levels in it.\n\"");
				trap->SendServerCommand(ent - g_entities, "print \"^3/telemark: ^7Sets a marker you can teleport to later.\n\
^3/teleport ^7or ^3/tele <player name (optional)> <player name (optional)>: ^7Teleports first player to the second player. Using one argument teleports current player to another player. Use with no arguments to teleport to your telemark.\n\
^3/teleportaim ^7or ^3/teleaim <player name or ID (optional)>: ^7Teleports you, or the player, to where you aim: onto the floor, or just off a wall or ceiling.\n\
^3/silence <player name>: ^7Silences the player.\n\
^3/paralyze <player name> <seconds (optional, 30-900, default 30)>: ^7Paralyzes the player.\n\
^3/unparalyze <player name>: ^7Releases a player paralyzed by an admin.\n\
^3/admkick <player name>: ^7Kicks player from the server.\n\
^3/killother <player name>: ^7Kills a player.\n\
^3/give <player name> <guns/force>: ^7Gives guns or Force powers to a player who is not logged in.\n\" ");
				trap->SendServerCommand(ent - g_entities, "print \"" RP_CLIENTPRINT_HELP "\n\
^3/shakescreen <distance (0-1500)> <intensity (1-10)> <seconds (1-5)>: ^7Shakes the screen of everyone within that distance of you.\n\
^3/duelarena: ^7Sets or unsets the Duel Tournament arena in current map.\n\
^3/meleearena: ^7Sets or unsets the Melee Battle arena in current map.\n\
^3/duelpause: ^7Pauses/resumes the Duel Tournament.\n\
^3/admmap <gametype number> <map name>: ^7Changes the server to a different map and gametype.\n\" ");
				trap->SendServerCommand(ent - g_entities, "print \"^3/admweather <effect (optional)>: ^7Sets the weather for everyone on the server. Run with no arguments to see the current weather, or ^3/admweather list ^7for the effects.\n\
^3/admweather add <effect>: ^7Layers another effect on top of the current weather. ^3/admweather remove <number> ^7drops one, ^3/admweather default ^7restores the map's own and ^3/admweather clear ^7switches it all off.\n\
^3/noclip: ^7Makes you able to go through walls.\n\" ");
				trap->SendServerCommand(ent - g_entities, "print \"^3/ghost <player name (optional)>: ^7Turns a player (or you) into a Force ghost: see-through, glowing, passes through people, off the radar. Again to turn it off.\n\
^3/holo <player name (optional)>: ^7Turns a player (or you) into a hologram: blue, scanlines, passes through people, off the radar. Again to turn it off.\n\
^3/admwalkthrough <player name (optional)>: ^7Lets a player (or you) walk through other players and NPCs, with no change in look. Again to turn it off.\n\
^3/admhidden <player name (optional)>: ^7Makes a player (or you) invisible to everyone else and walk-through, off the radar. They can still be hit and targeted; Force Sight sees them. Again to turn it off.\n\n\" ");
				trap->SendServerCommand(ent - g_entities, "print \"^3--------RP Inventory System--------\n\
^3/inventory ^7or ^3/inv: ^7Displays player's RP inventory.\n\
^3/createitem <itemname>: ^7Creates an item with a given name. Items containing more than one word need double quotes around the argument. ^1(Admin only)\n\
^3/trashitem <itemid>: ^7Deletes an item.\n\
^3/giveitem <itemid> <playername>: ^7Transfers an item to the desired player.\n\n\
^3--------News System--------\n\
^3/news <channel> <number of entries (optional)>: ^7Displays the news of the chosen channel.\n\
^3/newschannels: ^7Lists the news channels.\n\
^3/newsadd <channel> <text>: ^7Adds the news to a channel. The text has to be enclosed in double quotes for it to register properly. ^1(Admin only)\n\
^3/newsremove <news ID>: ^7Removes the news from a channel. ^1(Admin only)\n\n\" ");
				trap->SendServerCommand(ent - g_entities, "print \"^3--------Credits and Trading--------\n\
^3/createcredits <player name> <amount>: ^7Creates credits and gives them to a player. ^1(Admin only)\n\
^3/spendcredits <amount>: ^7Deletes credits from your inventory and displays a message (For paying NPCs).\n\
^3/givecredits <player name> <amount>: ^7Transfers credits from you to a player.\n\
^3/buy item <number> ^7or ^3/buy upgrade <number>: ^7Exchanges credits for merchandise. Stuff bought from the upgrade category is permanent.\n\
^3/stuff item <number (optional)> ^7or ^3/stuff upgrade <number (optional)>: ^7Shows info about merchandise. Run without a number to list available options in that category.\n\n\" ");
				trap->SendServerCommand(ent - g_entities, "print \"^3--------Ally System--------\n\
^3/allyadd <player name>: ^7Adds a player as an ally.\n\
^3/allychat <text>: ^7Sends message to your allies. Any distance, every gametype.\n\
^3/allyremove <player name>: ^7Removes player from allies.\n\
^3/allylist: ^7Lists your allies.\n\n\" ");
				trap->SendServerCommand(ent - g_entities, "print \"^3--------NPC System--------\n\
^7The ^3/npc ^7commands require the ^3NPC ^7admin command. See ^3/adminlist^7.\n\
^3/npc spawn <type> <targetname (optional)>: ^7Spawns an npc.\n\
^3/npc spawn vehicle <type> <targetname (optional)>: ^7Spawns a vehicle.\n\
^3/npc kill <targetname or type>: ^7Kills npcs with that targetname or type.\n\
^3/npc kill all: ^7Kills every npc.\n\
^3/npc kill team <player/enemy/neutral/free or nonally>: ^7Kills a whole team, or ^3nonally ^7for every npc but your allies.\n\
^3/npc killaim: ^7Kills the npc or vehicle in your crosshair.\n\
^3/npc team <player/enemy/neutral/free>: ^7Sets the team of the npc in your crosshair.\n\
^3/npc effect <holo/ghost/walkthrough/clear>: ^7Hologram, Force ghost or walk-through npc in your crosshair; ^3clear ^7undoes it.\n\
^3/npc freeze: ^7Freezes the npc in your crosshair in place, or unfreezes it.\n\" ");
				trap->SendServerCommand(ent - g_entities, "print \"^3/order <follow/guard/cover>: ^7Orders your NPCs to follow you, stand and fight, or follow and fight. Press ^3Use ^7on a friendly NPC to make it follow commands and press again to dismiss it.\n\n\" ");
				trap->SendServerCommand(ent - g_entities, "print \"^3--------Mini-Games--------\n\
^7Joining is for logged-out players only; anyone can view the tables. Both modes are toggles -- run the command again to leave.\n\
^3/duelmode: ^7Joins the Duel Tournament, a one-on-one saber bracket in the map's duel arena.\n\
^3/dueltable [page]: ^7Shows the Duel Tournament live. Page 1 is the standings, page 2 and up the matches.\n\
^3/meleemode: ^7Joins the Melee Battle, a fists-only free-for-all on the map's melee catwalk.\n\
^3/meleetable: ^7Shows the Melee Battle live: time left and the fighters still in, with their kills.\n\n\"");
				trap->SendServerCommand(ent - g_entities, "print \"^3--------Misc--------\n\
^3/roll <faces> ^7or ^3/roll <dice>d<faces>: ^7Rolls 1-10 dice of 2-100 faces. Seen by players near you.\n\
^3/rollall <faces> ^7or ^3/rollall <dice>d<faces>: ^7Same roll, seen by the whole server. Usable while dead or spectating.\n\
^3/flipcoin: ^7Flips a coin. Seen by players near you.\n\
^3/flipcoinall: ^7Same flip, seen by the whole server. Usable while dead or spectating.\n\"");
				trap->SendServerCommand(ent - g_entities, "print \"\
^3/engage_duel: ^7Challenges the player you are looking at to a private saber duel. Most Force powers are disabled for both of you.\n\
^3/engage_fullforceduel: ^7Challenges the player you are looking at to a private saber duel with Force powers allowed.\n\
^3/anim ^7or ^3/emote <id/name/list>: ^7Plays an animation by id or name. ^3List ^7and ^3list 2 ^7are for listing all the available animations.\n\
^3/playsound <channel (optional)> <file path>: ^7Plays a sound, once every 15 seconds. With no arguments, lists the channels; ^3/list sounds ^7lists the files.\n\
^3/datetime: ^7Shows current server date and time.\n\
^3/drop: ^7Drops the current weapon of the player. If current weapon is melee, drops the selected Holdable Item from inventory.\n\
^3/ignore <player name or id>: ^7Enable/disable ignoring a player. Covers every chat type.\n\
^3/ignorelist: ^7Lists ignored players.\n\
^3/jetpack: ^7Gives or removes jetpack from the player.\n\"");
				trap->SendServerCommand(ent - g_entities, "print \"^3/use_cloak: ^7Activates or deactivates your Cloak Item, on foot or while riding a vehicle. Never cloaks the vehicle itself.\n\
^3/vehicle_cloak: ^7While piloting a vehicle with the Cloak Item at level 2, cloaks or decloaks the vehicle together with you.\n\
^3/updateforce: ^7Applies your force power menu pick instantly, no respawn needed (logged-out players only).\n\"");
				trap->SendServerCommand(ent - g_entities, "print \"^3/maplist <page (optional)>: ^7Lists the maps you can vote for in the current gametype, for /callvote map.\n\
^3/saber <saber1> <saber2>: ^7Changes lightsabers of the player.\n\
^3/sabercolor <1|2> <r g b>/<color name>: ^7Sets the RGB or a preset color of saber 1 or 2. Run with no arguments to see current colors.\n\
^3/saberblade <1|2> <type>: ^7Sets the RGB blade style (classic/flame1/electric1/flame2/electric2) of saber 1 or 2.\n\
^3/updatesaber: ^7Applies your saber menu pick instantly, no respawn needed.\n\
^3/scale <size> ^7or ^3/scale <player name or ID> <size>: ^7Scales your character model, 20 to 500 (default 100). ^3/scale help ^7lists sizes next to real-life heights. ^1Scaling other players needs the Scale admin flag.\n\
^3/cosmetics <hats|capes|clear> <number or name>: ^7Puts on a hat or cape, or takes it off if you already wear it. ^3/cosmetics hats ^7or ^3capes ^7lists them. Saved with a logged-in character.\n\"");
				trap->SendServerCommand(ent - g_entities, "print \"\
^3/getup: ^7Gets you up from a combat knockdown, once your timer runs out. Cannot end an admin paralysis.\n\
^3/helpup <player name>: ^7Helps a nearby player up from a combat knockdown. Cannot end an admin paralysis.\n\
^3/training <on|off>: ^7Turns training saber mode (near-zero damage) on or off.\n\
^3/voice_cmd <arg> <f or m>: ^7Activates the voice chat system.\n\
^3/where: ^7Displays your current coordinates.\n\n\"");
			}
			else if (RP_ListCommand(ent, arg1))
			{ // GalaxyRP: [Listings] /list md3, glm, effects, sounds, music, maps, npcs, vehicles -- g_rplist.c
			}
			else if (Q_stricmp( arg1, "chat" ) == 0)
			{
				trap->SendServerCommand(ent - g_entities, "print \"\n\
^2Chat\n\
\n\
^3--------Chat Commands--------\n\
^3/say <text>: ^7Normal speech, 700 units. This is what the chat box sends.\n\
^3/say_team <text>: ^7Team chat in team gametypes. Otherwise ally chat if you have allies, OOC if not.\n\
^3/ooc <text>: ^7Out-of-character chat. Server-wide, in every gametype.\n\
^3/allychat <text>: ^7Sends message to your allies. Any distance, every gametype.\n\
^3/tell <player name or id> <text>: ^7Private message to one player.\n\
^3/ignore <player name or id>: ^7Enable/disable ignoring a player. Covers every chat type.\n\
^3/ignorelist: ^7Lists ignored players.\n\
^3/voice_cmd <arg> <f or m>: ^7Activates the voice chat system.\n\
\n\"");
				trap->SendServerCommand(ent - g_entities, "print \"^3--------RP Chat Modifiers--------\n\
^7Type a modifier at the start of a chat box message: ^3/me draws a blaster.\n\
^7Modifiers work on normal speech only - the chat box, or ^3/say^7.\n\
^7Most take a range suffix: ^3low ^7to speak quietly, ^3long ^7to carry further, ^3all ^7for server-wide.\n\
\n\
^7Speech ranges: ^3whisper ^765, ^3low ^765, normal 700, ^3shout ^71500, ^3long ^72000, ^3all ^7server-wide.\n\
^7Action ranges: ^3low ^7200, normal 1200, ^3long ^72000, ^3all ^7server-wide.\n\
\n\"");
				trap->SendServerCommand(ent - g_entities, "print \"^3/low ^7or ^3/long ^7or ^3/all <text>: ^7Normal speech at another range.\n\
^3/me <action>: ^7Actions. ^3/melow /melong /meall\n\
^3/do <text>: ^7Descriptions of the world around you. ^3/dolow /dolong /doall\n\
^3/my <text>: ^7Descriptions of something of yours. ^3/mylow /mylong /myall\n\
^3/force <action>: ^7Force actions. ^3/forcelow /forcelong /forceall\n\
^3/shout <text>: ^7Shouting, 1500 units. ^3/shoutlong /shoutall ^7(no low variant)\n\
^3/whisper <text>: ^7Whispering, 65 units. ^3/whisperlow ^7(50 units, no long or all variant)\n\
^3/npc <text>: ^7Speak as an NPC, 600 units. ^3/npclow /npcall ^7(no long variant)\n\
^3/c ^7or ^3/comm <text>: ^7Comlink message. Server-wide.\n\
^3/thought <text>: ^7A thought. Server-wide.\n\
^3/ryl /ryl2 /rodian /huttese /catharese /mando <text>: ^7Speak a language, 600 units.\n\
\n\"");
			}
			else
			{ // zyk: the player can also list the specific info of a skill passing the skill number as argument
				i = atoi(arg1);
				// GalaxyRP fix: [Skills audit] vlock skill 38 (Unique Ability)
				if (i >= 1 && i <= NUM_OF_SKILLS && (i - 1) != 38)
				{
					trap->SendServerCommand(ent - g_entities, va("print \"^3%s: ^7%s\n\"", skills[i-1].skill_name, skills[i-1].skill_description));
				}
				else
				{
					trap->SendServerCommand( ent-g_entities, "print \"Invalid skill number.\n\"" );
				}
			}
		}
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, "print \"\n^1Account System\n\
^7Create a new account with ^3/new ^7or ^3/register <account name> <password>^7, where account name and password are of your choice.\n\
^7Use ^3/login <account name> <password> ^7if you already have an account.\n\n\"" );
	}
}

/*
==================
Cmd_Stuff_f
==================
*/
void Cmd_Stuff_f( gentity_t *ent ) {
	if (trap->Argc() == 1)
	{ // zyk: shows the categories of stuff
		trap->SendServerCommand( ent-g_entities, "print \"\n^7Use ^2/stuff item ^7or ^2/stuff upgrade ^7to see the list of products in that category\n^7Use ^2/stuff item <number> ^7or ^2/stuff upgrade <number> ^7to see info about a specific product\n\n^7Use ^2/buy item <number> ^7or ^2/buy upgrade <number> ^7to buy\nStuff bought from the ^3upgrade ^7category is permanent\n\n\"");
		return;
	}
	else
	{
		char arg1[MAX_STRING_CHARS];
		char arg2[MAX_STRING_CHARS];
		int value = 0;
		qboolean has_number = (trap->Argc() > 2) ? qtrue : qfalse;

		trap->Argv(1, arg1, sizeof( arg1 ));

		if (has_number)
		{
			trap->Argv(2, arg2, sizeof( arg2 ));
			value = atoi(arg2);
		}

		if (Q_stricmp(arg1, "item") == 0)
		{
			if (!has_number)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n"
					"^31 - Blaster Pack: ^7Buy: 150\n"
					"^32 - Power Cell: ^7Buy: 200\n"
					"^33 - Metal Bolts: ^7Buy: 250\n"
					"^34 - Rockets: ^7Buy: 500\n"
					"^35 - Thermals: ^7Buy: 50\n"
					"^36 - Trip Mines: ^7Buy: 100\n"
					"^37 - Det Packs: ^7Buy: 200\n"
					"^38 - Flame Thrower Fuel: ^7Buy: 500\n"
					"^39 - Ammo All: ^7Buy: 1450\n"
					"^310 - Jetpack Fuel: ^7Buy: 500\n"
					"^311 - Ysalamiri: ^7Buy: 2000\n"
					"^312 - Force Boon: ^7Buy: 2000\n\n\"");
				return;
			}

			if (value == 1)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^3Blaster Pack: ^7recovers 100 ammo of E11 Blaster Rifle, Blaster Pistol and Bryar Pistol weapons\n\n\"");
			}
			else if (value == 2)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^3Power Cell: ^7recovers 100 ammo of Disruptor, Bowcaster and DEMP2 weapons\n\n\"");
			}
			else if (value == 3)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^3Metal Bolts: ^7recovers 100 ammo of Repeater, Flechette and Concussion Rifle weapons\n\n\"");
			}
			else if (value == 4)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^3Rockets: ^7recovers 5 ammo of Rocket Launcher weapon\n\n\"");
			}
			else if (value == 5)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^3Thermals: ^7recovers 4 ammo of thermals\n\n\"");
			}
			else if (value == 6)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^3Trip Mines: ^7recovers 3 ammo of trip mines\n\n\"");
			}
			else if (value == 7)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^3Det Packs: ^7recovers 2 ammo of det packs\n\n\"");
			}
			else if (value == 8)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^3Flame Thrower Fuel: ^7recovers all fuel of the flame thrower\n\n\"");
			}
			else if (value == 9)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^3Ammo All: ^7recovers all ammo types, including flame thrower fuel\n\n\"");
			}
			else if (value == 10)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^3Jetpack Fuel: ^7recovers all fuel of the jetpack\n\n\"");
			}
			else if (value == 11)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^3Ysalamiri: ^7disables the player force powers but also protects the player from enemy force powers\n\n\"");
			}
			else if (value == 12)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^3Force Boon: ^7allows the player to regenerate force faster\n\n\"");
			}
			else
			{
				trap->SendServerCommand( ent-g_entities, "print \"Invalid item number.\n\"" );
			}
		}
		else if (Q_stricmp(arg1, "upgrade") == 0)
		{
			if (!has_number)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n"
					"^31 - Holdable Items Upgrade: ^7Buy: 30000\n"
					"^32 - Impact Reducer: ^7Buy: 40000\n"
					"^33 - Stun Baton Upgrade: ^7Buy: 15000\n\n\"");
				return;
			}

			if (value == 1)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^3Holdable Items Upgrade: ^7Bacta Canister recovers more health, Big Bacta recovers more HP and Force Field resists more\n\n\"");
			}
			else if (value == 2)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^3Impact Reducer: ^7reduces the knockback of some weapons attacks by 80 per cent\n\n\"");
			}
			else if (value == 3)
			{
				trap->SendServerCommand( ent-g_entities, "print \"\n^3Stun Baton Upgrade: ^7each stun baton hit halves the enemy's movement speed for 1.5 seconds. Opening doors and moving elevators comes with Stun Baton skill level 5\n\n\"");
			}
			else
			{
				trap->SendServerCommand( ent-g_entities, "print \"Invalid upgrade number.\n\"" );
			}
		}
		else
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid category. Use ^3item ^7or ^3upgrade^7.\n\"" );
		}
	}
}

/*
==================
Cmd_Buy_f
==================
*/
void Cmd_Buy_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	int value = 0;
	int cost = 0;
	qboolean is_upgrade = qfalse;
	int item_costs[NUMBER_OF_SHOP_ITEMS] = {
		150,	// item 1: Blaster Pack
		200,	// item 2: Power Cell
		250,	// item 3: Metal Bolts
		500,	// item 4: Rockets
		50,		// item 5: Thermals
		100,	// item 6: Trip Mines
		200,	// item 7: Det Packs
		500,	// item 8: Flame Thrower Fuel
		1450,	// item 9: Ammo All
		500,	// item 10: Jetpack Fuel
		2000,	// item 11: Ysalamiri
		2000 };	// item 12: Force Boon
	int upgrade_costs[NUMBER_OF_SHOP_UPGRADES] = {
		30000,	// upgrade 1: Holdable Items Upgrade
		40000,	// upgrade 2: Impact Reducer
		15000 };	// upgrade 3: Stun Baton Upgrade

	if (trap->Argc() < 3)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Use ^2/buy item <number> ^7or ^2/buy upgrade <number>^7.\n\"" );
		return;
	}

	trap->Argv(1, arg1, sizeof( arg1 ));
	trap->Argv(2, arg2, sizeof( arg2 ));
	value = atoi(arg2);

	if (Q_stricmp(arg1, "item") == 0)
	{
		is_upgrade = qfalse;
	}
	else if (Q_stricmp(arg1, "upgrade") == 0)
	{
		is_upgrade = qtrue;
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid category. Use ^3item ^7or ^3upgrade^7.\n\"" );
		return;
	}

	// zyk: tests the cooldown time to buy or sell
	if (ent->client->pers.buy_sell_timer > level.time)
	{
		trap->SendServerCommand(ent->s.number, "print \"In Buy/Sell cooldown time.\n\"");
		return;
	}

	if (is_upgrade)
	{
		if (value < 1 || value > NUMBER_OF_SHOP_UPGRADES)
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid upgrade number.\n\"" );
			return;
		}

		if (value == 1 && ent->client->pers.skill_levels[38] & (1 << 0))
		{
			trap->SendServerCommand( ent-g_entities, "print \"You already have the Holdable Items Upgrade.\n\"" );
			return;
		}
		else if (value == 2 && ent->client->pers.skill_levels[38] & (1 << 1))
		{
			trap->SendServerCommand( ent-g_entities, "print \"You already have the Impact Reducer.\n\"" );
			return;
		}
		else if (value == 3 && ent->client->pers.skill_levels[38] & (1 << 2))
		{
			trap->SendServerCommand( ent-g_entities, "print \"You already have the Stun Baton Upgrade.\n\"" );
			return;
		}

		cost = upgrade_costs[value-1];
	}
	else
	{
		if (value < 1 || value > NUMBER_OF_SHOP_ITEMS)
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid item number.\n\"" );
			return;
		}

		cost = item_costs[value-1];
	}

	// zyk: buying the item if player has enough credits
	if (ent->client->pers.credits < cost)
	{
		trap->SendServerCommand( ent-g_entities, va("chat \"^3Jawa Seller: ^7%s^7, my products are not free! Give me the money!\n\"",ent->client->pers.netname) );
		return;
	}

	if (is_upgrade)
	{
		if (value == 1)
		{
			ent->client->pers.skill_levels[38] |= (1 << 0);
		}
		else if (value == 2)
		{
			ent->client->pers.skill_levels[38] |= (1 << 1);
		}
		else if (value == 3)
		{
			ent->client->pers.skill_levels[38] |= (1 << 2);
		}
	}
	else
	{
		if (value == 1)
		{
			Add_Ammo(ent,AMMO_BLASTER,100);
		}
		else if (value == 2)
		{
			Add_Ammo(ent,AMMO_POWERCELL,100);
		}
		else if (value == 3)
		{
			Add_Ammo(ent,AMMO_METAL_BOLTS,100);
		}
		else if (value == 4)
		{
			Add_Ammo(ent,AMMO_ROCKETS,5);
		}
		else if (value == 5)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_THERMAL);
			Add_Ammo(ent,AMMO_THERMAL,1);
		}
		else if (value == 6)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_TRIP_MINE);
			Add_Ammo(ent,AMMO_TRIPMINE,1);
		}
		else if (value == 7)
		{
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DET_PACK);
			Add_Ammo(ent,AMMO_DETPACK,1);
		}
		else if (value == 8)
		{
			ent->client->ps.cloakFuel = 100;
		}
		else if (value == 9)
		{
			Add_Ammo(ent,AMMO_BLASTER,100);

			Add_Ammo(ent,AMMO_POWERCELL,100);

			Add_Ammo(ent,AMMO_METAL_BOLTS,100);

			Add_Ammo(ent,AMMO_ROCKETS,10);

			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_THERMAL);
			Add_Ammo(ent,AMMO_THERMAL,4);

			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_TRIP_MINE);
			Add_Ammo(ent,AMMO_TRIPMINE,3);

			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_DET_PACK);
			Add_Ammo(ent,AMMO_DETPACK,2);

			ent->client->ps.cloakFuel = 100;
		}
		else if (value == 10)
		{
			ent->client->pers.jetpack_fuel = MAX_JETPACK_FUEL;
			ent->client->ps.jetpackFuel = 100;
		}
		else if (value == 11)
		{
			if (ent->client->ps.powerups[PW_YSALAMIRI] < level.time)
				ent->client->ps.powerups[PW_YSALAMIRI] = level.time + 60000;
			else
				ent->client->ps.powerups[PW_YSALAMIRI] += 60000;
		}
		else if (value == 12)
		{
			if (ent->client->ps.powerups[PW_FORCE_BOON] < level.time)
				ent->client->ps.powerups[PW_FORCE_BOON] = level.time + 60000;
			else
				ent->client->ps.powerups[PW_FORCE_BOON] += 60000;
		}
	}

	G_Sound(ent, CHAN_AUTO, G_SoundIndex("sound/player/pickupenergy.wav"));

	ent->client->pers.credits -= cost;
	save_account(ent, qtrue);

	ent->client->pers.buy_sell_timer = level.time + rp_buying_cooldown.integer;

	trap->SendServerCommand( ent-g_entities, va("chat \"^3Jawa Seller: ^7Thanks %s^7!\n\"",ent->client->pers.netname) );
}

// zyk: if an item left the inventory, makes some adjustments on the player
extern void Jedi_Decloak(gentity_t *self);
extern void Jedi_DecloakPair(gentity_t *self);
void zyk_adjust_holdable_items(gentity_t *ent)
{
	if (!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_BINOCULARS)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC)) &&
		!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SENTRY_GUN)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SEEKER)) &&
		!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_EWEB)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_MEDPAC_BIG)) &&
		!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_SHIELD)) && !(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK)))
	{ // zyk: if player has no items left, deselect the held item
		ent->client->ps.stats[STAT_HOLDABLE_ITEM] = 0;
	}

	// zyk: if player no longer has Cloak Item and is cloaked, decloaks
	if (!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK)) && ent->client->ps.powerups[PW_CLOAKED])
		Jedi_DecloakPair(ent);

	if (!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK)) && ent->client->jetPackOn)
		Jetpack_Off(ent);

	if (!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_BINOCULARS)) && ent->client->ps.zoomMode == 2)
		zyk_cancel_zoom(ent);
}

/*
==================
Cmd_ChangePassword_f
==================
*/
void Cmd_ChangePassword_f( gentity_t *ent ) {
	char arg1[1024];

	if (trap->Argc() != 2)
	{
		trap->SendServerCommand( ent-g_entities, "print \"^1Command Usage: ^3/changepassword ^2<new_password>\n^1Example: ^3/changepassword ^2password123\n\"" );
		return;
	}

	// zyk: gets the new password
	trap->Argv(1, arg1, sizeof( arg1 ));

	{
		const char *reason = NULL;

		if (RP_PasswordIsValid(arg1, &reason) == qfalse)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", reason) );
			return;
		}
	}

	Q_strncpyz(ent->client->pers.password, arg1, sizeof(ent->client->pers.password));

	update_accounts_table_row_with_current_values(ent);

	trap->SendServerCommand( ent-g_entities, "print \"^3Your password was changed successfully.\n\"" );
}

extern void zyk_TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles );

static qboolean RP_TeleRiderRefused( gentity_t *ent, gentity_t *target )
{
	if ( target->client->sess.sessionTeam == TEAM_SPECTATOR || !target->client->ps.m_iVehicleNum )
		return qfalse;

	trap->SendServerCommand( ent-g_entities, va("print \"%s ^7is riding a vehicle.\n\"", target->client->pers.netname) );
	return qtrue;
}

/*
==================
Cmd_Teleport_f
==================
*/
void Cmd_Teleport_f( gentity_t *ent )
{
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	char arg3[MAX_STRING_CHARS];
	char arg4[MAX_STRING_CHARS];

	if (!check_admin_command(ent, ADM_TELE, qtrue))
	{
		return;
	}

	if (trap->Argc() == 1)
	{
		if (ent->client->pers.saved_origin[0] == 0 && ent->client->pers.saved_origin[1] == 0 && ent->client->pers.saved_origin[2] == 0 &&
			ent->client->pers.saved_view_angles[0] == 0 && ent->client->pers.saved_view_angles[1] == 0 && ent->client->pers.saved_view_angles[2] == 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"No telemark set. Use /telemark to mark a spot first.\n\"" );
			return;
		}

		if (RP_TeleRiderRefused(ent, ent))
		{
			return;
		}

		zyk_TeleportPlayer(ent, ent->client->pers.saved_origin, ent->client->pers.saved_view_angles);
	}
	else if (trap->Argc() == 2)
	{
		int client_id = -1;

		trap->Argv( 1,  arg1, sizeof( arg1 ) );

		vec3_t target_origin;

		client_id = ClientNumberFromString( ent, arg1, qfalse );

		if (client_id == -1)
		{
			return;
		}

		if (ent != &g_entities[client_id] && g_entities[client_id].client->sess.amrpgmode > 0 && g_entities[client_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client_id].client->pers.player_settings & (1 << 13)))
			{
				trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
				return;
			}

		if (RP_TeleRiderRefused(ent, ent))
		{
			return;
		}

		VectorCopy(g_entities[client_id].client->ps.origin,target_origin);
		target_origin[2] = target_origin[2] + 100;

		zyk_TeleportPlayer(ent,target_origin,g_entities[client_id].client->ps.viewangles);
	}
	else if (trap->Argc() == 3)
	{
		// zyk: teleporting a player to another
		int client1_id;
		int client2_id;

		vec3_t target_origin;

		trap->Argv( 1,  arg1, sizeof( arg1 ) );
		trap->Argv( 2,  arg2, sizeof( arg2 ) );

		client1_id = ClientNumberFromString( ent, arg1, qfalse );
		client2_id = ClientNumberFromString( ent, arg2, qfalse );

		if (client1_id == -1)
		{
			return;
		}

		if (ent != &g_entities[client1_id] && g_entities[client1_id].client->sess.amrpgmode > 0 && g_entities[client1_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client1_id].client->pers.player_settings & (1 << 13)))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
			return;
		}

		if (client2_id == -1)
		{
			return;
		}

		if (ent != &g_entities[client2_id] && g_entities[client2_id].client->sess.amrpgmode > 0 && g_entities[client2_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client2_id].client->pers.player_settings & (1 << 13)))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
			return;
		}

		if (RP_TeleRiderRefused(ent, &g_entities[client1_id]))
		{
			return;
		}

		VectorCopy(g_entities[client2_id].client->ps.origin,target_origin);
		target_origin[2] = target_origin[2] + 100;

		zyk_TeleportPlayer(&g_entities[client1_id],target_origin,g_entities[client2_id].client->ps.viewangles);
	}
	else if (trap->Argc() == 4)
	{
		// zyk: teleporting to coordinates
		vec3_t target_origin;

		trap->Argv( 1,  arg1, sizeof( arg1 ) );
		trap->Argv( 2,  arg2, sizeof( arg2 ) );
		trap->Argv( 3,  arg3, sizeof( arg3 ) );

		VectorSet(target_origin,atoi(arg1),atoi(arg2),atoi(arg3));

		if (RP_TeleRiderRefused(ent, ent))
		{
			return;
		}

		zyk_TeleportPlayer(ent,target_origin,ent->client->ps.viewangles);
	}
	else if (trap->Argc() == 5)
	{
		// zyk: teleporting a player to coordinates
		vec3_t target_origin;
		int client_id;

		trap->Argv( 1,  arg1, sizeof( arg1 ) );

		client_id = ClientNumberFromString( ent, arg1, qfalse );

		if (client_id == -1)
		{
			return;
		}

		// GalaxyRP fix: [Admin] added the "ent != target" self-exemption already used by /give and
		// /scale, so an admin with their own Admin Protect enabled can still target themselves.
		if (ent != &g_entities[client_id] && g_entities[client_id].client->sess.amrpgmode > 0 && g_entities[client_id].client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(g_entities[client_id].client->pers.player_settings & (1 << 13)))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
			return;
		}

		trap->Argv( 2,  arg2, sizeof( arg2 ) );
		trap->Argv( 3,  arg3, sizeof( arg3 ) );
		trap->Argv( 4,  arg4, sizeof( arg4 ) );

		VectorSet(target_origin,atoi(arg2),atoi(arg3),atoi(arg4));

		if (RP_TeleRiderRefused(ent, &g_entities[client_id]))
		{
			return;
		}

		zyk_TeleportPlayer(&g_entities[client_id],target_origin,g_entities[client_id].client->ps.viewangles);
	}
}

/*
==================
Cmd_Telemark_f
==================
*/
void Cmd_Telemark_f(gentity_t* ent)
{
	if (!check_admin_command(ent, ADM_TELE, qtrue))
	{
		return;
	}

	VectorCopy(ent->client->ps.origin, ent->client->pers.saved_origin);
	VectorCopy(ent->client->ps.viewangles, ent->client->pers.saved_view_angles);

	trap->SendServerCommand(ent - g_entities, va("print \"Marked point %s with angles %s\n\"", vtos(ent->client->pers.saved_origin), vtos(ent->client->pers.saved_view_angles)));

	return;
}

/*
==================
Cmd_TeleportAim_f

DAJ_RP: [Admin] /teleportaim or /teleaim <player name or ID (optional)>: teleports the admin, or the
player, to where the admin aims. Same power as /teleport and /telemark (ADM_TELE), same Admin Protect rule.

The aim is /entaddaim's (RP_TeleAimPoint, g_entgrab.c): across any map, through players, NPCs and player
clips. The sky is refused -- the player would be left at the top of the map. A standing player's box is
set 1 unit off the surface aimed at, along the surface's normal: on a floor the feet are 1 unit above it,
off a wall the player stands 1 unit from it, under a ceiling the head is 1 unit below it and the player
drops from there. On a slope or a slanted wall the box touches the surface with its nearest corner or edge,
1 unit off; RP_EntGrabPlace() pushes a box out along one axis only, which would bury a corner of it in such
a surface and leave the player stuck.

The spot needs room for the box among the world, solid entities and player clips (RP_TELEAIM_MASK). Bodies
do not count: whoever stands there is telefragged -- or walked out of, when the damage is turned away --
by G_KillBox() in zyk_TeleportPlayer(), as with /teleport. Without room, a place nearby is looked for by
sweeping the box to the spot from up to 48 units above (a wall aimed at low down puts the box into the
floor), from 48 below (a wall near a ceiling, or a ceiling over a low gap) and from 64 back towards the
admin (a corner). The first sweep that starts in the open, ends with room and ends in sight of the point
aimed at is taken -- in sight, so that one starting beyond a thin floor or wall does not put the player on
its far side. None: refused.

The player keeps the view angles they had. Refused: a dead player; one riding a vehicle (the vehicle would
carry them straight back); a spectator following someone (their view is the followed player's, copied every
frame); and an admin following someone himself, whose aim would be the followed player's.
==================
*/
#define RP_TELEAIM_GAP		1.0f	// how far off the surface aimed at the player's box is set
#define RP_TELEAIM_LIFT		48.0f	// the sweeps from above and below when the spot has no room
#define RP_TELEAIM_BACK		64.0f	// the sweep from back towards the admin
#define RP_TELEAIM_MASK		( MASK_PLAYERSOLID & ~CONTENTS_BODY )	// what the player could be stuck in

// DAJ_RP: [Admin] room for the box at origin
static qboolean RP_TeleAimRoom( const vec3_t origin, const vec3_t mins, const vec3_t maxs, int passent )
{
	trace_t tr;

	trap->Trace( &tr, origin, mins, maxs, origin, passent, RP_TELEAIM_MASK, qfalse, 0, 0 );

	return ( !tr.startsolid && !tr.allsolid ) ? qtrue : qfalse;
}

// DAJ_RP: [Admin] the box swept from start to the spot: where it stops, if it started in the open, has
// room there and its centre is in sight of seen, the point just off the surface aimed at
static qboolean RP_TeleAimSweep( const vec3_t start, const vec3_t spot, const vec3_t mins, const vec3_t maxs, int passent, const vec3_t seen, vec3_t result )
{
	trace_t tr;
	vec3_t end, centre;

	trap->Trace( &tr, start, mins, maxs, spot, passent, RP_TELEAIM_MASK, qfalse, 0, 0 );
	if ( tr.startsolid || tr.allsolid )
		return qfalse;

	VectorCopy( tr.endpos, end );
	if ( !RP_TeleAimRoom( end, mins, maxs, passent ) )
		return qfalse;

	centre[0] = end[0] + ( mins[0] + maxs[0] ) * 0.5f;
	centre[1] = end[1] + ( mins[1] + maxs[1] ) * 0.5f;
	centre[2] = end[2] + ( mins[2] + maxs[2] ) * 0.5f;

	trap->Trace( &tr, seen, vec3_origin, vec3_origin, centre, passent, RP_TELEAIM_MASK, qfalse, 0, 0 );
	if ( tr.startsolid || tr.allsolid || tr.fraction < 1.0f )
		return qfalse;

	VectorCopy( end, result );
	return qtrue;
}

void Cmd_TeleportAim_f( gentity_t *ent )
{
	const vec3_t mins = { -15, -15, DEFAULT_MINS_2 };	// a standing player's box (g_client.c's playerMins/playerMaxs)
	const vec3_t maxs = { 15, 15, DEFAULT_MAXS_2 };
	gentity_t *target = ent;
	vec3_t point, normal, dir, spot, seen, start, angles;
	float reach = 0.0f;
	int surfaceFlags = 0;
	int k;

	if (!check_admin_command(ent, ADM_TELE, qtrue))
	{
		return;
	}

	if (trap->Argc() > 2)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Usage: ^3/teleportaim ^7or ^3/teleaim <player name or ID (optional)>^7. Teleports you, or the player, to where you aim.\n\"" );
		return;
	}

	if (trap->Argc() == 2)
	{
		char arg1[MAX_STRING_CHARS];
		int client_id;

		trap->Argv( 1, arg1, sizeof( arg1 ) );

		client_id = ClientNumberFromString( ent, arg1, qfalse );
		if (client_id == -1)
		{
			return;
		}

		target = &g_entities[client_id];

		// same Admin Protect rule as /teleport
		if (ent != target && target->client->sess.amrpgmode > 0 && target->client->pers.bitvalue & (1 << ADM_ADMPROTECT) && !(target->client->pers.player_settings & (1 << 13)))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Target player is adminprotected\n\"") );
			return;
		}
	}

	if (ent->client->sess.sessionTeam == TEAM_SPECTATOR && ent->client->sess.spectatorState == SPECTATOR_FOLLOW)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Stop following someone to aim.\n\"" );
		return;
	}

	if (target->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		if (target->client->sess.spectatorState == SPECTATOR_FOLLOW)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"%s ^7is spectating another player.\n\"", target->client->pers.netname) );
			return;
		}
	}
	else if (target->health <= 0)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s ^7is dead.\n\"", target->client->pers.netname) );
		return;
	}
	else if (target->client->ps.m_iVehicleNum)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s ^7is riding a vehicle.\n\"", target->client->pers.netname) );
		return;
	}

	if (RP_TeleAimPoint(ent, point, normal, &surfaceFlags) == qfalse)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You are not aiming at anything.\n\"" );
		return;
	}

	if (surfaceFlags & SURF_SKY)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You are aiming at the sky.\n\"" );
		return;
	}

	// the box set 1 unit off the surface: its centre that far out along the normal beyond the box's own
	// reach along it
	VectorNormalize( normal );
	for (k = 0; k < 3; k++)
	{
		reach += fabs( normal[k] ) * ( maxs[k] - mins[k] ) * 0.5f;
	}
	for (k = 0; k < 3; k++)
	{
		spot[k] = point[k] + normal[k] * ( reach + RP_TELEAIM_GAP ) - ( mins[k] + maxs[k] ) * 0.5f;
	}

	if (!RP_TeleAimRoom(spot, mins, maxs, target->s.number))
	{
		vec3_t found;
		qboolean ok = qfalse;

		VectorMA( point, RP_TELEAIM_GAP, normal, seen );
		AngleVectors( ent->client->ps.viewangles, dir, NULL, NULL );

		VectorCopy( spot, start );
		start[2] += RP_TELEAIM_LIFT;
		ok = RP_TeleAimSweep( start, spot, mins, maxs, target->s.number, seen, found );

		if (!ok)
		{
			VectorCopy( spot, start );
			start[2] -= RP_TELEAIM_LIFT;
			ok = RP_TeleAimSweep( start, spot, mins, maxs, target->s.number, seen, found );
		}

		if (!ok)
		{
			VectorMA( spot, -RP_TELEAIM_BACK, dir, start );
			ok = RP_TeleAimSweep( start, spot, mins, maxs, target->s.number, seen, found );
		}

		if (!ok)
		{
			trap->SendServerCommand( ent-g_entities, "print \"There is no room for a player where you aim.\n\"" );
			return;
		}

		VectorCopy( found, spot );
	}

	// zyk_TeleportPlayer() lifts the player 1 unit: the spot checked is where they end up
	{
		vec3_t dest;

		VectorCopy( spot, dest );
		dest[2] -= 1.0f;
		VectorCopy( target->client->ps.viewangles, angles );
		zyk_TeleportPlayer( target, dest, angles );
	}

	if (target != ent)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Teleported %s ^7to (%d %d %d).\n\"", target->client->pers.netname,
			(int)floor(spot[0] + 0.5f), (int)floor(spot[1] + 0.5f), (int)floor(spot[2] + 0.5f)) );
	}
}

/*
==================
Cmd_CreditSpend_f
==================
*/
void Cmd_CreditSpend_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];
	int value = 0;

	if (trap->Argc() != 2)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1Command Usage: ^3/spendcredits ^2<value>\n^1Example: ^3/spendcredits ^2350\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	value = atoi(arg1);

	if (value < 1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Can only use positive values.\n\"");
		return;
	}

	if ((ent->client->pers.credits - value) < 0)
	{
		trap->SendServerCommand(ent - g_entities, va(("print \"^1You can\'t spend ^3%i ^1credits.\n^7You only have ^3%i ^7credits.\n\""), value, ent->client->pers.credits));
		return;
	}

	remove_credits(ent, value);

	update_credits_value(ent);

	G_LogPrintf("CreditSpend: %s^7 spent %d credits\n", ent->client->pers.netname, value);

	trap->SendServerCommand(-1, va("chat \"^3Credit System: ^7%s ^7spent ^2%d ^7credits.\n\"", ent->client->pers.netname, value));

	trap->SendServerCommand(ent - g_entities, "print \"Done.\n\"");
}

/*
==================
Cmd_CreditCreate_f
==================
*/
void Cmd_CreditCreate_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	int client_id = 0, value = 0;

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"You must specify a player.\n\"");
		return;
	}

	if (trap->Argc() == 2)
	{
		trap->SendServerCommand(ent - g_entities, "print \"You must specify the amount of credits.\n\"");
		return;
	}

	// player must have adminup permissions
	if (!check_admin_command(ent, ADM_CREATECREDITS, qtrue))
	{
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));
	trap->Argv(2, arg2, sizeof(arg2));

	client_id = ClientNumberFromString(ent, arg1, qfalse);
	value = atoi(arg2);

	if (client_id == -1)
	{
		return;
	}

	if (value < 1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Can only use positive values.\n\"");
		return;
	}

	if (value > rp_max_rpg_credits.integer)
		value = rp_max_rpg_credits.integer;

	if (g_entities[client_id].client->sess.amrpgmode < 2)
	{
		trap->SendServerCommand(ent - g_entities, "print \"The player is not logged in\n\"");
		return;
	}

	add_credits(&g_entities[client_id], value);
	update_credits_value(&g_entities[client_id]);

	G_LogPrintf("CreditCreate: %s^7 created %d credits and gave them to %s^7\n", ent->client->pers.netname, value, g_entities[client_id].client->pers.netname);

	//broadcast the transaction to the whole server

	trap->SendServerCommand(-1, va("chat \"^3Credit System ^5(Admin)^3: ^7%s ^7created ^2%d ^7credits and transferred them to %s\n\"", ent->client->pers.netname, value, g_entities[client_id].client->pers.netname));

	trap->SendServerCommand(ent - g_entities, "print \"Done.\n\"");
}

/*
==================
Cmd_CreditGive_f
==================
*/
void Cmd_CreditGive_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	int client_id = 0, value = 0;

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand( ent - g_entities, "print \"You must specify a player.\n\"" );
		return;
	}

	if (trap->Argc() == 2)
	{
		trap->SendServerCommand( ent - g_entities, "print \"You must specify the amount of credits.\n\"" );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	trap->Argv( 2, arg2, sizeof( arg2 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );
	value = atoi(arg2);

	if (client_id == -1)
	{
		return;
	}

	if (value < 1)
	{
		trap->SendServerCommand( ent - g_entities, "print \"Can only use positive values.\n\"" );
		return;
	}

	if (value > rp_max_rpg_credits.integer)
		value = rp_max_rpg_credits.integer;

	if (g_entities[client_id].client->sess.amrpgmode < 2)
	{
		trap->SendServerCommand( ent - g_entities, "print \"The player is not logged in\n\"" );
		return;
	}

	if ((ent->client->pers.credits - value) < 0)
	{
		trap->SendServerCommand(ent - g_entities, va(("print \"^1You can\'t give ^3%i ^1credits.\n^7You only have ^3%i ^7credits.\n\""), value, ent->client->pers.credits));
		return;
	}


	add_credits(&g_entities[client_id], value);
	update_credits_value(&g_entities[client_id]);

	remove_credits(ent, value);
	update_credits_value(ent);

	G_LogPrintf("CreditGive: %s^7 transferred %d credits to %s^7\n", ent->client->pers.netname, value, g_entities[client_id].client->pers.netname);

	//broadcast the transaction to the whole server

	trap->SendServerCommand(-1, va("chat \"^3Credit System: ^7%s ^7transferred ^2%d ^7credits to %s\n\"", ent->client->pers.netname, value, g_entities[client_id].client->pers.netname));

	trap->SendServerCommand(ent - g_entities, "print \"Done.\n\"");
}

/*
==================
Cmd_AllyList_f
==================
*/
void Cmd_AllyList_f( gentity_t *ent ) {
	char message[MAX_STRING_CHARS];
	char entry[MAX_NETNAME + 64];
	int i = 0;
	int listed = 0;

	strcpy(message,"");

	for (i = 0; i < level.maxclients; i++)
	{
		gentity_t *this_ent = &g_entities[i];
		qboolean mine = (zyk_is_ally(ent, this_ent) == qtrue);
		qboolean theirs = (zyk_is_ally(this_ent, ent) == qtrue);

		// zyk_is_ally() already requires the other side to be a connected client, so an empty
		// slot can satisfy neither test and netname below is always a real name.
		if (mine == qfalse && theirs == qfalse)
		{
			continue;
		}

		Com_sprintf(entry, sizeof(entry), "^7%s%s%s\n",
			this_ent->client->pers.netname,
			(mine == qtrue) ? " ^3(ally)" : "",
			(theirs == qtrue) ? " ^3(added you)" : "");

		if ((int)(strlen(message) + strlen(entry)) > RP_LIST_FLUSH_AT)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"%s\"", message) );
			strcpy(message,"");
		}

		Q_strcat(message, sizeof(message), entry);
		listed++;
	}

	// GalaxyRP fix: [Ally] say so, rather than printing a bare empty line.
	if (listed == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"^7You have no allies.\n\"" );
		return;
	}

	if (message[0] != '\0')
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s\"", message) );
	}
}

// GalaxyRP: [Radar] push this player's ally set to their own client. sess.ally1/ally2 are a bit per
// client slot (ally1 for 0-15, ally2 for 16-31), and cgame mirrors them in cg.ally1/cg.ally2 so the
// radar can colour an ally differently from an enemy below GT_TEAM.
//
// A reliable server command rather than an entity event, which is what this used to be. The events
// carried one slot each and were delivered through the snapshot, so they could be lost and were
// never re-sent; sess.ally1/ally2 meanwhile ride the session string and survive a map change. The
// server therefore went on believing in allies the client had forgotten, and every ally read as an
// enemy on the radar after every map change until the player re-ran /allyadd. Sending the whole set
// in one message, and again from ClientBegin, removes the class of bug rather than the instance.
void send_ally_update(gentity_t *ent)
{
	if (!ent || !ent->client)
	{
		return;
	}

	trap->SendServerCommand(ent->s.number, va("supdateally %i %i\n",
		ent->client->sess.ally1, ent->client->sess.ally2));
}

void zyk_add_ally(gentity_t *ent, int client_id)
{
	if (client_id > 15)
	{
		ent->client->sess.ally2 |= (1 << (client_id - 16));
	}
	else
	{
		ent->client->sess.ally1 |= (1 << client_id);
	}
}

void zyk_remove_ally(gentity_t *ent, int client_id)
{
	if (client_id > 15)
	{
		ent->client->sess.ally2 &= ~(1 << (client_id - 16));
	}
	else
	{
		ent->client->sess.ally1 &= ~(1 << client_id);
	}
}

/*
==================
Cmd_AllyAdd_f
==================
*/
void Cmd_AllyAdd_f( gentity_t *ent ) {
	if (trap->Argc() == 1)
	{
		trap->SendServerCommand(ent->s.number, va("print \"^1Command Usage: ^3/allyadd ^2<player name or id>\n^1Example: ^3/allyadd ^2Alex\n\"") );
	}
	else
	{
		char arg1[MAX_STRING_CHARS];
		int client_id = -1;

		// GalaxyRP fix: [Guardian] a guardian_mode>0 guard blocking /allyadd during a boss battle used
		// to be here. guardian_mode is permanently 0 now, so it was unreachable.

		if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
		{ // zyk: cant add allies while in Duel Tournament
			trap->SendServerCommand(ent->s.number, va("print \"You cannot add allies while in a Duel Tournament.\n\""));
			return;
		}

		trap->Argv(1, arg1, sizeof( arg1 ));
		client_id = ClientNumberFromString( ent, arg1, qfalse ); 

		if (client_id == -1)
		{
			return;
		}

		if (client_id == (ent-g_entities))
		{ // zyk: player cant add himself as ally
			trap->SendServerCommand(ent->s.number, va("print \"You cannot add yourself as ally\n\"") );
			return; 
		}

		if (zyk_is_ally(ent,&g_entities[client_id]) == qtrue)
		{
			trap->SendServerCommand(ent->s.number, va("print \"You already have this ally.\n\"") );
			return;
		}

		// zyk: add this player as an ally
		zyk_add_ally(ent, client_id);

		// zyk: sending the updated ally set to the client-side radar
		// GalaxyRP fix: [Radar] was G_AddEvent(ent, EV_USE_ITEM14, client_id), one bit of a per-slot
		// client cache. The whole set goes in one reliable server command now; see send_ally_update().
		send_ally_update(ent);

		trap->SendServerCommand(ent->s.number, va("print \"Added ally %s^7\n\"", g_entities[client_id].client->pers.netname) );
		trap->SendServerCommand( client_id, va("print \"%s^7 added you as ally\n\"", ent->client->pers.netname) );
	}
}

void Cmd_AllyChat_f( gentity_t *ent ) { // zyk: allows chatting with allies
	char *p = NULL;

	if ( trap->Argc () < 2 )
		return;

	p = ConcatArgs( 1 );

	if ( strlen( p ) >= MAX_SAY_TEXT ) {
		p[MAX_SAY_TEXT-1] = '\0';
		G_SecurityLogPrintf( "Cmd_AllyChat_f from %d (%s) has been truncated: %s\n", ent->s.number, ent->client->pers.netname, p );
	}

	G_Say( ent, NULL, SAY_ALLY, p );
}

/*
==================
Cmd_AllyRemove_f
==================
*/
void Cmd_AllyRemove_f( gentity_t *ent ) {
	if (trap->Argc() == 1)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"^1Command Usage: ^3/allyremove <player name or id>\n^1Example: ^3/allyremove ^2Alex\n\"") );
	}
	else
	{
		char arg1[MAX_STRING_CHARS];
		int client_id = -1;

		if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
		{ // zyk: cant remove allies while in Duel Tournament
			trap->SendServerCommand(ent->s.number, va("print \"You cannot remove allies while in a Duel Tournament.\n\""));
			return;
		}

		trap->Argv(1, arg1, sizeof( arg1 ));
		client_id = ClientNumberFromString( ent, arg1, qfalse ); 

		if (client_id == -1)
		{
			return;
		}

		if (zyk_is_ally(ent,&g_entities[client_id]) == qfalse)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"You do not have this ally.\n\"") );
			return;
		}

		// zyk: removes this ally
		zyk_remove_ally(ent, client_id);

		// zyk: sending the updated ally set to the client-side radar
		// GalaxyRP fix: [Radar] was G_AddEvent(ent, EV_USE_ITEM14, client_id + MAX_CLIENTS), the
		// "no longer an ally" half of the same per-slot cache. See send_ally_update().
		send_ally_update(ent);

		trap->SendServerCommand( ent-g_entities, va("print \"Removed ally %s^7\n\"", g_entities[client_id].client->pers.netname) );
		trap->SendServerCommand( client_id, va("print \"%s^7 removed you as an ally\n\"", ent->client->pers.netname) );
	}
}
// TODO: JLH start here
/*
==================
Cmd_Settings_f
==================
*/
void Cmd_Settings_f( gentity_t *ent ) {
	if (trap->Argc() == 1)
	{
		char message[1024];
		int len = 0;
		strcpy(message,"");

		if (ent->client->pers.player_settings & (1 << 5))
		{
			len += sprintf(message + len, "\n^3 1 - Sense Health Toggle (Requires Sense Health) - ^1OFF");
		}
		else
		{
			len += sprintf(message + len, "\n^3 1 - Sense Health Toggle (Requires Sense Health) - ^2ON");
		}

		if (ent->client->pers.player_settings & (1 << 13))
		{
			len += sprintf(message + len, "\n^3 2 - Admin Protect (Requires Admin Protect admin power) - ^1OFF");
		}
		else
		{
			len += sprintf(message + len, "\n^3 2 - Admin Protect (Requires Admin Protect admin power) - ^2ON");
		}

		if (ent->client->pers.player_settings & (1 << 11))
		{
			len += sprintf(message + len, "\n^3 3 - Activate Saber on Spawn - ^1OFF");
		}
		else
		{
			len += sprintf(message + len, "\n^3 3 - Activate Saber on Spawn - ^2ON");
		}

		if (ent->client->pers.player_settings & (1 << 6))
		{
			len += sprintf(message + len, "\n^3 4 - Use Hint - ^1OFF");
		}
		else
		{
			len += sprintf(message + len, "\n^3 4 - Use Hint - ^2ON");
		}

		if (ent->client->pers.player_settings & (1 << 16))
		{
			len += sprintf(message + len, "\n^3 5 - Ignore Chat Distance (Requires Ignore Chat Distance admin power) - ^1OFF");
		}
		else
		{
			len += sprintf(message + len, "\n^3 5 - Ignore Chat Distance (Requires Ignore Chat Distance admin power) - ^2ON");
		}

		if (ent->client->pers.player_settings & (1 << 20))
		{
			len += sprintf(message + len, "\n^3 6 - Entity Bounds (Requires Entities and Remaps admin power) - ^1OFF");
		}
		else
		{
			len += sprintf(message + len, "\n^3 6 - Entity Bounds (Requires Entities and Remaps admin power) - ^2ON");
		}


		trap->SendServerCommand( ent-g_entities, va("print \"%s\n\n^7Choose a setting above and use ^3/settings <number> ^7to turn it ^2ON ^7or ^1OFF^7\n\"", message) );
	}
	else
	{
		char arg1[MAX_STRING_CHARS];
		char new_status[32];
		int value = 0;

		trap->Argv(1, arg1, sizeof( arg1 ));
		value = atoi(arg1);

		static const int settings_number_to_bit[] = { 0, 5, 13, 11, 6, 16, 20 }; // index 0 unused (rejected below)

		if (value <= 0 || value >= (int)ARRAY_LEN(settings_number_to_bit))
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid settings value.\n\"" );
			return;
		}

		value = settings_number_to_bit[value];

		if (value == 13 && (ent->client->pers.player_settings & (1 << value)) && !check_admin_command(ent, ADM_ADMPROTECT, qtrue))
		{
			return;
		}

		if (value == 16 && (ent->client->pers.player_settings & (1 << value)) && !check_admin_command(ent, ADM_IGNORECHATDISTANCE, qtrue))
		{
			return;
		}

		if (value == 20 && (ent->client->pers.player_settings & (1 << value)) && !check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
		{
			return;
		}

		if (ent->client->pers.player_settings & (1 << value))
		{
			ent->client->pers.player_settings &= ~(1 << value);

			strcpy(new_status,"^2ON^7");
		}
		else
		{
			ent->client->pers.player_settings |= (1 << value);

			strcpy(new_status,"^1OFF^7");
		}

		save_account(ent, qfalse);

		// GalaxyRP fix: [Quests] the value==0 (RPG quests) print branch used to be here. Removed since
		// 0 is now rejected above as an invalid settings value.
		if (value == 5)
		{
			// DAJ_RP: [Settings] the Sense Health Toggle can be switched on without the skill -- it just
			// has nothing to show until the skill is bought -- so say so rather than refusing.
			// skill_levels[35] is the Sense Health skill; /settings is logged-in only, so it is current.
			if (!(ent->client->pers.player_settings & (1 << 5)) && ent->client->pers.skill_levels[35] < 1)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"Sense Health Toggle %s\n^3You don't have the Sense Health skill, so this has no effect until you do.\n\"", new_status) );
			}
			else
			{
				trap->SendServerCommand( ent-g_entities, va("print \"Sense Health Toggle %s\n\"", new_status) );
			}
		}
		else if (value == 13)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Admin Protect %s\n\"", new_status) );
		}
		else if (value == 11)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Activate Saber on Spawn %s\n\"", new_status) );
		}
		else if (value == 6)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Use Hint %s\n\"", new_status) );
		}
		else if (value == 16)
		{
			// DAJ_RP: [Settings] no "no effect" warning any more: turning it ON without the admin power
			// is now refused by the gate above, so ON always means it works.
			trap->SendServerCommand( ent-g_entities, va("print \"Ignore Chat Distance %s\n\"", new_status) );
		}
		else if (value == 20)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity Bounds %s\n\"", new_status) );
		}

	}
}

/*
==================
Cmd_Drop_f
==================
*/
extern qboolean saberKnockOutOfHand(gentity_t *saberent, gentity_t *saberOwner, vec3_t velocity);
void Cmd_Drop_f( gentity_t *ent ) {
	vec3_t vel;
	gitem_t *item = NULL;
	gentity_t *launched;
	int weapon = ent->client->ps.weapon;
	vec3_t uorg, vecnorm, thispush_org;
	int current_ammo = 0;
	int ammo_count = 0;
	qboolean has_ammo_type = qfalse;

	if (G_PlayerIsDowned(ent))
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot do this while you are downed.\n\"");
		return;
	}

	if (ent->client->ps.m_iVehicleNum)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1You cannot do this while riding a vehicle.\n\"");
		return;
	}

	if (weapon == WP_NONE || weapon == WP_EMPLACED_GUN || weapon == WP_TURRET)
	{ //can't have this
		return;
	}

	VectorCopy(ent->client->ps.origin, thispush_org);

	VectorCopy(ent->client->ps.origin, uorg);
	uorg[2] += 64;

	VectorSubtract(uorg, thispush_org, vecnorm);
	VectorNormalize(vecnorm);

	if (weapon == WP_SABER)
	{
		vel[0] = vecnorm[0]*100;
		vel[1] = vecnorm[1]*100;
		vel[2] = vecnorm[2]*100;
		saberKnockOutOfHand(&g_entities[ent->client->ps.saberEntityNum],ent,vel);
		return;
	}

	// zyk: velocity with which the item will be tossed
	vel[0] = vecnorm[0] * 500;
	vel[1] = vecnorm[1] * 500;
	vel[2] = vecnorm[2] * 500;

	// zyk: when using melee, drop items
	if (weapon == WP_MELEE && ent->client->ps.stats[STAT_HOLDABLE_ITEM] > 0 && 
		bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giType == IT_HOLDABLE)
	{
		item = BG_FindItemForHoldable(bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giTag);

		if (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << item->giTag))
		{ // zyk: if player has the item in inventory, drop it
			launched = LaunchItem(item, ent->client->ps.origin, vel);

			// zyk: this player cannot get this item for 1 second
			launched->genericValue10 = level.time + 1000;
			launched->genericValue11 = ent->s.number;

			// zyk: remove item from inventory
			ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << item->giTag);

			// GalaxyRP fix: [Classes] a rpg_class==2 (Bounty Hunter) sentry-gun-counter decrement used to
			// be here. rpg_class is permanently 0 now that character classes are gone, so this was
			// unreachable.

			zyk_adjust_holdable_items(ent);
		}
	}
	else if (weapon != WP_MELEE)
	{
		// find the item type for this weapon
		item = BG_FindItemForWeapon((weapon_t)weapon);

		launched = LaunchItem(item, ent->client->ps.origin, vel);

		launched->s.generic1 = ent->s.number;
		launched->s.powerups = level.time + 1500;

		launched->count = bg_itemlist[BG_GetItemIndexByTag(weapon, IT_WEAPON)].quantity;

		has_ammo_type = (weaponData[weapon].ammoIndex != AMMO_NONE) ? qtrue : qfalse;

		if (has_ammo_type == qtrue)
		{
			current_ammo = ent->client->ps.ammo[weaponData[weapon].ammoIndex];
			ammo_count = RP_ScaledAmmo(bg_itemlist[BG_GetItemIndexByTag(weapon, IT_WEAPON)].quantity);

			if (current_ammo < ammo_count)
			{ // zyk: player does not have the default ammo to set in the weapon, so set the current_ammo of the player in the weapon
				ent->client->ps.ammo[weaponData[weapon].ammoIndex] -= current_ammo;
				launched->count = RP_DropAmmoCount(current_ammo); // zyk: -1 when the player has no ammo, so weapon should add no ammo to the player who picks up this weapon
			}
			else
			{
				ent->client->ps.ammo[weaponData[weapon].ammoIndex] -= ammo_count;
				launched->count = RP_DropAmmoCount(ammo_count);
			}
		}
		else
		{ // no ammo type, so the dropped weapon gives none -- the same -1 the empty case above uses
			launched->count = -1;
		}

		if ((has_ammo_type == qtrue && ent->client->ps.ammo[weaponData[weapon].ammoIndex] < 1) ||
			(weapon != WP_THERMAL && weapon != WP_DET_PACK && weapon != WP_TRIP_MINE))
		{
			ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << weapon);

			ent->s.weapon = WP_MELEE;
			ent->client->ps.weapon = WP_MELEE;

			G_AddEvent(ent, EV_NOAMMO, weapon);
		}
	}
}

/*
==================
Cmd_Jetpack_f
==================
*/
void Cmd_Jetpack_f( gentity_t *ent ) {

	if (level.melee_mode > 0 && level.melee_players[ent->s.number] != -1)
	{ // zyk: cannot get jetpack in Melee Battle
		return;
	}

	if (!(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK)) && jetpack_command_allowed(ent) &&
		level.gametype != GT_SIEGE && level.gametype != GT_JEDIMASTER)
	{ // zyk: gets jetpack if player does not have it. RPG players need jetpack skill to get it
		// zyk: Jedi Master gametype will not allow jetpack
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << HI_JETPACK);
	}
	else
	{
		if (ent->client->jetPackOn)
			Jetpack_Off(ent);
		ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << HI_JETPACK);
	}
}

void Cmd_VehicleCloak_f( gentity_t *ent ) {
	extern void Jedi_Cloak( gentity_t *self );
	extern void Jedi_DecloakPair( gentity_t *self );

	if ( ent->client->ps.m_iVehicleNum )
	{
		gentity_t *veh = &g_entities[ent->client->ps.m_iVehicleNum];

		if ( !veh->m_pVehicle || veh->m_pVehicle->m_pPilot != (bgEntity_t *)ent )
		{
			return;
		}

		if ( veh->client && veh->client->ps.powerups[PW_CLOAKED] )
		{
			if ( ent->client->ps.powerups[PW_CLOAKED] )
			{//both cloaked -- decloak the pair, gated only by the manual-use cooldown
				if ( ent->client->vehicleCloakToggleTime < level.time )
				{
					Jedi_DecloakPair( veh );
					ent->client->vehicleCloakToggleTime = level.time + 1000;
				}
			}
			else if ( ent->client->vehicleCloakToggleTime < level.time &&
				veh->client->cloakToggleTime < level.time &&
				ent->client->ps.stats[STAT_HEALTH] > 0 && !(ent->client->ps.eFlags & EF_DEAD) &&
				ent->client->ps.pm_type != PM_DEAD &&
				!(ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DOWNED)) &&
				// DAJ_RP: [Cloak Item] Cloak Item level 2 cloaks the vehicle, in place of the Holdable
				// Items Upgrade (skill_levels[38] bit 0) that used to -- see the skills[] row.
				ent->client->pers.skill_levels[53] >= 2 &&
				(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK)) )
			{//safeguard: vehicle cloaked but rider isn't -- resync by cloaking the rider too
				Jedi_Cloak( ent );
				ent->client->vehicleCloakToggleTime = level.time + 1000;
			}
		}
		else if ( ent->client->vehicleCloakToggleTime < level.time &&
			veh->client && veh->client->cloakToggleTime < level.time &&
			ent->client->ps.stats[STAT_HEALTH] > 0 && !(ent->client->ps.eFlags & EF_DEAD) &&
			ent->client->ps.pm_type != PM_DEAD &&
			!(ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DOWNED)) &&
			// DAJ_RP: [Cloak Item] Cloak Item level 2, as above.
			ent->client->pers.skill_levels[53] >= 2 &&
			(ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_CLOAK)) )
		{//vehicle not cloaked -- cloak vehicle + rider together
			Jedi_Cloak( veh );
			Jedi_Cloak( ent );
			ent->client->vehicleCloakToggleTime = level.time + 1000;
		}
	}
}

static qboolean zyk_preset_name_too_long( gentity_t *ent, const char *name )
{
	if ( name && (int)strlen(name) > ZYK_PRESET_NAME_MAX )
	{
		trap->SendServerCommand( ent-g_entities,
			va("print \"File name is too long. At most %d characters.\n\"", ZYK_PRESET_NAME_MAX) );
		return qtrue;
	}

	return qfalse;
}

/*
==================
Cmd_Remap_f
==================
*/
void Cmd_Remap_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	float f = level.time * 0.001;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( number_of_args < 3)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify the old shader and new shader. Ex: ^3/remap models/weapons2/heavy_repeater/heavy_repeater_w.glm models/items/bacta^7\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	trap->Argv( 2, arg2, sizeof( arg2 ) );

	if (zyk_valid_shader_name(arg1) == qfalse || zyk_valid_shader_name(arg2) == qfalse)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Invalid shader name. Each must be 1-%i characters, with no spaces and none of ^3= : @^7\n\"", MAX_QPATH - 1) );
		return;
	}

	AddRemap(arg1, arg2, f);
	trap->SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());

	trap->SendServerCommand( ent-g_entities, "print \"Shader remapped\n\"" );
}

/*
==================
Cmd_RemapList_f
==================
*/
void Cmd_RemapList_f(gentity_t *ent) {
	int page = 1; // zyk: page the user wants to see
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char content[MAX_STRING_CHARS];
	int i = 0;
	int results_per_page = 8; // zyk: number of results per page

	strcpy(content, "");

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if (number_of_args < 2)
	{
		trap->SendServerCommand(ent->s.number, va("print \"You must specify the page. Example: ^3/remaplist 1^7\n\""));
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));
	
	page = atoi(arg1);

	if (page < 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Invalid page number\n\"");
		return;
	}

	// zyk: makes i start from the first result of the correct page
	i = results_per_page * (page - 1);

	strcpy(content, "\n^3Old Shader   -   New Shader\n\n^7");

	while (i < (results_per_page * page) && i < zyk_get_remap_count())
	{
		char entry[(MAX_QPATH * 2) + 16];

		Com_sprintf(entry, sizeof(entry), "%s - %s\n", remappedShaders[i].oldShader, remappedShaders[i].newShader);

		if ((int)(strlen(content) + strlen(entry)) > RP_LIST_FLUSH_AT)
		{
			trap->SendServerCommand(ent->s.number, va("print \"%s\"", content));
			strcpy(content, "");
		}

		Q_strcat(content, sizeof(content), entry);
		i++;
	}

	Q_strcat(content, sizeof(content), "\n");
	trap->SendServerCommand(ent->s.number, va("print \"%s\"", content));
}

/*
==================
Cmd_RemapReset_f

GalaxyRP fix: [Shader Remap] clears every remap the map is running and puts the shaders back,
without waiting for a map change.
==================
*/
void Cmd_RemapReset_f( gentity_t *ent ) {
	int cleared;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	cleared = zyk_clear_all_remaps();

	if (cleared > 0)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%d shader remap(s) cleared. Remaps built into the map itself are not affected.\n\"", cleared) );
	}
	else
	{
		// Says so rather than returning in silence, which reads the same as a command that is
		// broken or that the admin lacks rights for -- the lesson from /entundo's quiet no-op.
		trap->SendServerCommand( ent-g_entities, "print \"There are no shader remaps to clear.\n\"" );
	}
}

/*
==================
Cmd_RemapDeleteFile_f
==================
*/
void Cmd_RemapDeleteFile_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	FILE *this_file = NULL;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify a file name.\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid file name. Only letters and numbers allowed.\n\"" );
		return;
	}

	if (zyk_preset_name_too_long(ent, arg1) == qtrue)
	{
		return;
	}

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("remaps/%s", zyk_mapname));

	this_file = fopen(va("GalaxyRP/remaps/%s/%s.txt",zyk_mapname,arg1),"r");
	if (this_file)
	{
		fclose(this_file);

		remove(va("GalaxyRP/remaps/%s/%s.txt",zyk_mapname,arg1));

		trap->SendServerCommand( ent-g_entities, va("print \"File %s deleted from server\n\"", arg1) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"File %s does not exist\n\"", arg1) );
	}
}

/*
==================
Cmd_RemapSave_f
==================
*/
void Cmd_RemapSave_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	int i = 0;
	FILE *remap_file = NULL;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify the file name\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid file name. Only letters and numbers allowed.\n\"" );
		return;
	}

	if (zyk_preset_name_too_long(ent, arg1) == qtrue)
	{
		return;
	}

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("remaps/%s", zyk_mapname));

	// zyk: saving remaps in the file
	remap_file = fopen(va("GalaxyRP/remaps/%s/%s.txt",zyk_mapname,arg1),"w");

	// GalaxyRP fix: [stability] the handle was used without being checked, the same gap /entsave had
	if (!remap_file)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Could not open file %s for writing.\n\"", arg1) );
		return;
	}

	for (i = 0; i < zyk_get_remap_count(); i++)
	{
		fprintf(remap_file,"%s\n%s\n%f\n",remappedShaders[i].oldShader,remappedShaders[i].newShader,remappedShaders[i].timeOffset);
	}
	fclose(remap_file);

	trap->SendServerCommand( ent-g_entities, va("print \"Remaps saved in %s file\n\"", arg1) );
}

/*
==================
Cmd_RemapLoad_f
==================
*/
void Cmd_RemapLoad_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify the file name\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid file name. Only letters and numbers allowed.\n\"" );
		return;
	}

	// GalaxyRP fix: [security] and refuse a name too long to build a sane path with -- see
	// zyk_preset_name_too_long() above Cmd_Remap_f().
	if (zyk_preset_name_too_long(ent, arg1) == qtrue)
	{
		return;
	}

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("remaps/%s", zyk_mapname));

	if (zyk_load_remap_file(va("GalaxyRP/remaps/%s/%s.txt", zyk_mapname, arg1)) == qtrue)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Remaps loaded from %s file\n\"", arg1) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Remaps not loaded. File %s not found\n\"", arg1) );
	}
}

static int RP_EntRemoveFree( gentity_t *target, int *subEntities );	// GalaxyRP: [Entity System] below, with /entremove

/*
==================
Cmd_EntUndo_f
==================
*/
void Cmd_EntUndo_f(gentity_t *ent) {
	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	// DAJ_RP: [Entity System] not while this admin holds an entity with /entcopy or /entcut (g_entgrab.c)
	if (RP_EntRefuseWhileHolding(ent))
	{
		return;
	}

	if (level.last_spawned_entity && level.last_spawned_entity->rpHeldBy)
	{
		trap->SendServerCommand(ent->s.number, va("print \"Entity %d is %s: wait until they drop it or let go.\n\"", level.last_spawned_entity->s.number,
			RP_EntHeldByText(level.last_spawned_entity)));
	}
	else if (level.last_spawned_entity)
	{ // zyk: removes the last entity spawned by /entadd, /spawnplatform or /spawndummy
		gentity_t *undone = level.last_spawned_entity;
		int undone_number = undone->s.number;

		if (RP_IsNpcSpawnerClass(undone->classname))
		{
			int ridden = 0;
			int removed = RP_SpawnerRemoveChildren(undone, &ridden);

			if (removed > 0 || ridden > 0)
			{
				trap->SendServerCommand(ent->s.number, va("print \"Entity %d cleaned, with the %d NPC(s) or vehicle(s) it made%s\n\"",
					undone_number, removed, ridden > 0 ? va("; %d vehicle(s) with someone riding left alone", ridden) : ""));
			}
			else
			{
				trap->SendServerCommand(ent->s.number, va("print \"Entity %d cleaned\n\"", undone_number));
			}

			if (level.last_spawned_entity != undone)
			{
				return;
			}
		}
		else
		trap->SendServerCommand(ent->s.number, va("print \"Entity %d cleaned\n\"", undone_number));

		RP_EntRemoveFree(undone, NULL);

		level.last_spawned_entity = NULL;
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "print \"Nothing to undo. Only the most recent /entadd, /spawnplatform or /spawndummy can be undone, and only once.\n\"");
	}
}

// GalaxyRP: [Entity System] a name or type shown back in a message: no quote to end the print, nothing
// unprintable, short
static const char *zyk_shown_name( const char *name )
{
	static char shown[48];
	int i;

	Q_strncpyz( shown, name, sizeof( shown ) );
	for ( i = 0; shown[i]; i++ )
	{
		if ( shown[i] == '"' || (unsigned char)shown[i] < 32 )
			shown[i] = ' ';
	}
	return shown;
}

/*
==================
Cmd_EntUse_f

GalaxyRP: [Entity System] /entuse <name>: uses every entity with that targetname, as a trigger or a button
targeting it would, with the admin as the one who used it -- an NPC or vehicle spawner, a light, an
effect, a door, a relay. Not an NPC or a vehicle (anything with a client): using a vehicle makes whoever
used it board it, from anywhere on the map (NPC_Use()). Nor an entity held with /entcopy or /entcut. The
ones to use are found first and used after, so an entity that using one makes, or a freed slot taken
again, is not used as well; each is used only if it still has that name.
==================
*/
void Cmd_EntUse_f( gentity_t *ent ) {
	static int found[MAX_ENTITIESTOTAL];
	char name[MAX_STRING_CHARS];
	gentity_t *e = NULL;
	int num_found = 0, used = 0, npcs = 0, held = 0, inert = 0, i;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	// DAJ_RP: [Entity System] not while this admin holds an entity with /entcopy or /entcut (g_entgrab.c)
	if (RP_EntRefuseWhileHolding(ent))
	{
		return;
	}

	if (trap->Argc() < 2)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Usage: ^3/entuse <name>^7. Uses every entity with that targetname, as a trigger or a button would: NPC and vehicle spawners, lights, effects, doors, relays. NPCs and vehicles are left alone.\n\"" );
		return;
	}

	trap->Argv( 1, name, sizeof( name ) );
	if (!name[0])
	{
		trap->SendServerCommand( ent-g_entities, "print \"Give the name to use: ^3/entuse <name>^7.\n\"" );
		return;
	}

	while ((e = G_Find(e, FOFS(targetname), name)) != NULL && num_found < MAX_ENTITIESTOTAL)
	{
		found[num_found++] = e - g_entities;
	}

	for (i = 0; i < num_found; i++)
	{
		e = &g_entities[found[i]];

		if (!e->inuse || !e->targetname || Q_stricmp(e->targetname, name) != 0)
		{
			continue;	// gone, or another entity now, since the search
		}
		// DAJ_RP: [Entity System] NPCs and vehicles -- a misc_weapon_shooter's client is only its pool's, and it
		// fires when used (RP_EntityIsCharacter, g_misc.c)
		if (RP_EntityIsCharacter(e))
		{
			npcs++;
			continue;
		}
		if (e->rpHeldBy)
		{
			held++;
			continue;
		}
		if (!e->use)
		{
			inert++;
			continue;
		}
		// DAJ_RP: [Locks] an admin's use: account locks do not stop it, nor what it sets off (g_locks.c)
		// and what it sets off later, with the admin as activator, passes too (RP_LockMarkPass)
		RP_LockBypass(qtrue);
		RP_LockMarkPass(e, ent);
		e->use(e, ent, ent);
		RP_LockBypass(qfalse);
		used++;
	}

	G_LogPrintf("/entuse '%s' by %s: %d used\n", name, ent->client->pers.netname, used);

	if (num_found == 0)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Nothing is named ^3%s^7.\n\"", zyk_shown_name(name)) );
		return;
	}

	{
		char skipped[256];

		skipped[0] = '\0';
		if (npcs > 0)
			Q_strcat(skipped, sizeof(skipped), va("; left alone: %d NPC(s) or vehicle(s)", npcs));
		if (held > 0)
			Q_strcat(skipped, sizeof(skipped), va("; %d held with /entcopy or /entcut", held));
		if (inert > 0)
			Q_strcat(skipped, sizeof(skipped), va("; %d that do nothing when used", inert));

		trap->SendServerCommand( ent-g_entities, va("print \"Used %d entit%s named ^3%s^7%s.\n\"", used, used == 1 ? "y" : "ies",
			zyk_shown_name(name), skipped) );
	}
}

/*
==================
Cmd_EntOrigin_f
==================
*/
void Cmd_EntOrigin_f(gentity_t *ent) {
	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if (level.ent_origin_set == qfalse)
	{
		VectorCopy(ent->client->ps.origin, level.ent_origin);
		VectorCopy(ent->client->ps.viewangles, level.ent_angles);
		level.ent_origin_set = qtrue;

		trap->SendServerCommand(ent->s.number, va("print \"Entity origin: (%f %f %f) angles: (%f %f %f)\n\"", level.ent_origin[0], level.ent_origin[1], level.ent_origin[2], level.ent_angles[0], level.ent_angles[1], level.ent_angles[2]));
	}
	else
	{
		level.ent_origin_set = qfalse;
		trap->SendServerCommand(ent->s.number, "print \"Entity origin unset\n\"");
	}
}

static const char *zyk_spawner_effect_problem( qboolean vehicle, const char *value )
{
	static char problem[256];

	if ( !value || !value[0] )
	{
		return NULL;
	}

	if ( vehicle )
	{
		return "Vehicles cannot be given an effect: ^3npceffect^7 is for an ^3npc_spawner^7.";
	}

	if ( RP_NpcEffectFromName( value ) < 0 )
	{
		Com_sprintf( problem, sizeof( problem ), "^3%s^7 is not an NPC effect: ^3npceffect^7 takes ^3holo^7, ^3ghost^7 or ^3walkthrough^7.", zyk_shown_name( value ) );
		return problem;
	}

	return NULL;
}

static const char *zyk_spawner_team_problem( const char *value )
{
	static char problem[256];

	if ( !value || !value[0] )
	{
		return NULL;
	}

	if ( RP_NpcTeamFromName( value ) < 0 )
	{
		Com_sprintf( problem, sizeof( problem ), "^3%s^7 is not an NPC team: ^3npcteam^7 takes ^3player^7, ^3enemy^7, ^3neutral^7 or ^3free^7.", zyk_shown_name( value ) );
		return problem;
	}

	return NULL;
}

static const char *zyk_spawner_freeze_problem( qboolean vehicle, const char *value )
{
	static char problem[256];

	if ( !value || !value[0] )
	{
		return NULL;
	}

	if ( vehicle )
	{
		return "Vehicles cannot be frozen: ^3npcfreeze^7 is for an ^3npc_spawner^7.";
	}

	if ( RP_NpcFreezeFromValue( value ) < 0 )
	{
		Com_sprintf( problem, sizeof( problem ), "^3npcfreeze %s^7: use ^31^7 to spawn its NPCs frozen, or ^30^7.", zyk_shown_name( value ) );
		return problem;
	}

	return NULL;
}

static const char *zyk_spawner_reward_problem( qboolean vehicle, const char *key, const char *value )
{
	static char problem[256];
	const qboolean credits = !Q_stricmp( key, "npccredits" ) ? qtrue : qfalse;
	const int max = credits ? RP_REWARD_MAX_CREDITS : RP_REWARD_MAX_XP;
	const char *p;

	if ( !value || !value[0] )
	{
		return NULL;
	}

	if ( vehicle )
	{
		Com_sprintf( problem, sizeof( problem ), "Vehicles pay no kill reward: ^3%s^7 is for an ^3npc_spawner^7.", credits ? "npccredits" : "npcxp" );
		return problem;
	}

	for ( p = value; *p; p++ )
	{
		if ( *p < '0' || *p > '9' || ( p - value ) >= 9 )
		{
			break;
		}
	}

	if ( *p || atoi( value ) > max )
	{
		Com_sprintf( problem, sizeof( problem ), "^3%s %s^7: the reward is a whole number from ^30^7 to ^3%d^7 (%s per kill).",
			credits ? "npccredits" : "npcxp", zyk_shown_name( value ), max, credits ? "credits" : "XP" );
		return problem;
	}

	return NULL;
}

static const char *zyk_spawner_scale_problem( const char *value )
{
	static char problem[256];

	if ( !value || !value[0] )
	{
		return NULL;
	}

	if ( RP_NpcScaleValue( value, qtrue ) < 0 )
	{
		Com_sprintf( problem, sizeof( problem ), "^3npcscale %s^7: the size is a whole number in percent, ^3%d^7 to ^3%d^7 (^3100^7 is normal, ^30^7 its type's own).",
			zyk_shown_name( value ), RP_NPC_SCALE_MIN, RP_NPC_SCALE_MAX );
		return problem;
	}

	return NULL;
}

static void RP_EntScaleValueStored( const char *classname, const char *key, char *value, int size )
{
	if ( classname && RP_IsNpcSpawnerClass( classname ) && !Q_stricmp( key, "npcscale" ) && value[0] &&
		Q_stricmp( value, "zykremovekey" ) && RP_NpcScaleValue( value, qtrue ) >= 0 )
	{
		Q_strncpyz( value, va( "%d", RP_NpcScaleValue( value, qtrue ) ), size );
	}
}

static const char *zyk_entadd_spawner_problem( const char *classname, int number_of_args )
{
	static char problem[256];
	char key[MAX_STRING_CHARS], value[MAX_STRING_CHARS];
	char targetname[MAX_STRING_CHARS], npc_type[MAX_STRING_CHARS], effect[MAX_STRING_CHARS], team[MAX_STRING_CHARS];
	char reward_credits[MAX_STRING_CHARS], reward_xp[MAX_STRING_CHARS], freeze[MAX_STRING_CHARS], scale[MAX_STRING_CHARS];
	qboolean has_type = qfalse;
	qboolean vehicle = !Q_stricmp( classname, "npc_vehicle" ) ? qtrue : qfalse;
	int i;

	targetname[0] = npc_type[0] = effect[0] = team[0] = reward_credits[0] = reward_xp[0] = freeze[0] = scale[0] = '\0';
	for ( i = 2; i + 1 < number_of_args; i += 2 )
	{
		trap->Argv( i, key, sizeof( key ) );
		trap->Argv( i + 1, value, sizeof( value ) );
		if ( !Q_stricmp( key, "targetname" ) )
		{
			Q_strncpyz( targetname, value, sizeof( targetname ) );
		}
		else if ( !Q_stricmp( key, "npc_type" ) )
		{
			Q_strncpyz( npc_type, value, sizeof( npc_type ) );
			has_type = qtrue;
		}
		else if ( !Q_stricmp( key, "npceffect" ) )
		{
			Q_strncpyz( effect, value, sizeof( effect ) );
		}
		else if ( !Q_stricmp( key, "npcteam" ) )
		{
			Q_strncpyz( team, value, sizeof( team ) );
		}
		else if ( !Q_stricmp( key, "npccredits" ) )
		{
			Q_strncpyz( reward_credits, value, sizeof( reward_credits ) );
		}
		else if ( !Q_stricmp( key, "npcxp" ) )
		{
			Q_strncpyz( reward_xp, value, sizeof( reward_xp ) );
		}
		else if ( !Q_stricmp( key, "npcfreeze" ) )
		{
			Q_strncpyz( freeze, value, sizeof( freeze ) );
		}
		else if ( !Q_stricmp( key, "npcscale" ) )
		{
			Q_strncpyz( scale, value, sizeof( scale ) );
		}
	}

	// GalaxyRP: [Entity System] "npceffect" -- see RP_NpcEffectFromName() (NPC_spawn.c)
	if ( effect[0] )
	{
		const char *problem_effect = zyk_spawner_effect_problem( vehicle, effect );

		if ( problem_effect )
		{
			return problem_effect;
		}
	}

	// GalaxyRP: [Entity System] "npcteam" -- see RP_NpcTeamFromName() (NPC_spawn.c)
	if ( team[0] )
	{
		const char *problem_team = zyk_spawner_team_problem( team );

		if ( problem_team )
		{
			return problem_team;
		}
	}

	// DAJ_RP: [NPC Rewards] "npccredits" and "npcxp" -- see RP_SpawnerRewardKeys() (NPC_spawn.c)
	if ( reward_credits[0] || reward_xp[0] )
	{
		const char *problem_reward = zyk_spawner_reward_problem( vehicle, "npccredits", reward_credits );

		if ( !problem_reward )
		{
			problem_reward = zyk_spawner_reward_problem( vehicle, "npcxp", reward_xp );
		}

		if ( problem_reward )
		{
			return problem_reward;
		}
	}

	// DAJ_RP: [NPC System] "npcfreeze" -- see RP_NpcFreezeFromValue() (NPC_spawn.c)
	if ( freeze[0] )
	{
		const char *problem_freeze = zyk_spawner_freeze_problem( vehicle, freeze );

		if ( problem_freeze )
		{
			return problem_freeze;
		}
	}

	// DAJ_RP: [NPC Scale] "npcscale" -- see RP_SpawnerScaleKey() (NPC_spawn.c)
	if ( scale[0] )
	{
		const char *problem_scale = zyk_spawner_scale_problem( scale );

		if ( problem_scale )
		{
			return problem_scale;
		}
	}

	// zyk: SP_NPC_spawner() removes itself at once then; SP_NPC_Vehicle() does not ask
	if ( !vehicle && !g_allowNPC.integer )
	{
		return "NPCs are switched off on this server (^3g_allowNPC 0^7): an NPC spawner would remove itself.";
	}

	if ( !targetname[0] )
	{
		return "An NPC or vehicle spawner needs a ^3targetname^7: the name a trigger, a button or ^3/entuse^7 fires it by. To spawn one NPC or vehicle now, use ^3/npc spawn^7.";
	}

	if ( !Q_stricmp( classname, "npc_spawner" ) )
	{
		if ( !npc_type[0] )
		{
			return "An npc_spawner needs an ^3npc_type^7 (see ^3/list npcs^7).";
		}
		if ( RP_NpcTypeIsVehicle( npc_type ) )
		{
			Com_sprintf( problem, sizeof( problem ), "^3%s^7 is a vehicle's npc entry: use ^3NPC_Vehicle^7 with a vehicle type (see ^3/list vehicles^7).", zyk_shown_name( npc_type ) );
			return problem;
		}
		if ( !RP_NpcTypeKnown( npc_type ) )
		{
			Com_sprintf( problem, sizeof( problem ), "^3%s^7 is not an NPC type on this server (see ^3/list npcs^7).", zyk_shown_name( npc_type ) );
			return problem;
		}
	}
	else if ( vehicle )
	{
		// zyk: SP_NPC_Vehicle() spawns a swoop when no type is given
		const char *type = has_type ? npc_type : "swoop";

		if ( !type[0] )
		{
			return "An NPC_Vehicle's ^3npc_type^7 cannot be empty (see ^3/list vehicles^7).";
		}
		if ( !RP_VehicleTypeKnown( type ) )
		{
			Com_sprintf( problem, sizeof( problem ), "^3%s^7 is not a vehicle type on this server (see ^3/list vehicles^7).", zyk_shown_name( type ) );
			return problem;
		}
		// zyk: NPC_Spawn_Do() reads the vehicle's NPC entry of the same name too, and frees the spawner without
		// one -- every vehicle of the game has one, a custom vehicle may not
		if ( !RP_NpcTypeKnown( type ) )
		{
			Com_sprintf( problem, sizeof( problem ), "The vehicle ^3%s^7 has no NPC entry of its name on this server, so it cannot be spawned.", zyk_shown_name( type ) );
			return problem;
		}
	}

	return NULL;
}

static int zyk_args_bytes( int first, int count )
{
	char arg[MAX_STRING_CHARS];
	int i, bytes = 64;

	for ( i = first; i < count; i++ )
	{
		trap->Argv( i, arg, sizeof( arg ) );
		bytes += ( ( (int)strlen( arg ) + 1 + 31 ) & ~31 ) * 2;
	}

	return bytes;
}

static void zyk_entadd( gentity_t *ent, qboolean aim, qboolean grab ) {
	gentity_t *new_ent = NULL;
	int number_of_args = trap->Argc();
	int i = 0;
	char key[MAX_STRING_CHARS];
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	qboolean has_origin_set = qfalse; // zyk: if player do not pass an origin key, use the one set with /entorigin
	qboolean has_angles_set = qfalse; // zyk: if player do not pass an angles key, use the one set with /entorigin

	vec3_t aim_point, aim_normal;
	qboolean aim_origin_ignored = qfalse;
	qboolean typed_origin = qfalse;	// GalaxyRP: an origin key among the arguments, known before the spawn
	float aim_offset = 0.0f;		// GalaxyRP: /entaddaim's "aimoffset": how far out from the surface
	rpSpawnRoute_t route;			// GalaxyRP: [Logical Entities] which region the classname and keys lead to

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if (RP_EntRefuseWhileHolding(ent))
	{
		return;
	}

	if ( number_of_args < 2)
	{
		if (grab)
			trap->SendServerCommand( ent-g_entities, "print \"Usage: ^3/entaddgrab <classname> <key> <value> <key> <value>^7. Picks up a new entity, not spawned yet; aim where it should go and use ^3/entaddgrab^7 again to place it there. ^3aimoffset <units>^7 (0 to 512) puts it that far out from the surface.\n\"" );
		else if (aim)
			trap->SendServerCommand( ent-g_entities, "print \"Usage: ^3/entaddaim <classname> <key> <value> <key> <value>^7. Adds the entity on the surface you are aiming at; ^3aimoffset <units>^7 (0 to 512) puts it that far out from the surface.\n\"" );
		else
		trap->SendServerCommand( ent-g_entities, va("print \"Usage: ^3/entadd <classname> <key> <value> <key> <value>^7. You must specify at least the entity class.\n\
			^7Example: ^3/entadd info_player_deathmatch^7, which spawns a spawn point in the map\n\"") );
		return;
	}

	if ( number_of_args % 2 != 0)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify an even number of arguments after the classname, because they are key/value pairs\n\"") );
		return;
	}

	if ((1 + ((number_of_args - 2) / 2)) > (ZYK_MAX_SPAWN_STRING_SLOTS / 2))
	{
		trap->SendServerCommand( ent-g_entities, va("print \"An entity can hold at most %d key/value pairs, classname included.\n\"", ZYK_MAX_SPAWN_STRING_SLOTS / 2) );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	// GalaxyRP: [Entity System] "aimoffset <units>": /entaddaim moves the point aimed at that far out from
	// the surface, along its normal, before placing the entity there (a light above a floor, say). It is
	// an instruction to the command, not a key of the entity: never written to its record, so no entity
	// file ever holds it. /entadd places no entity by aim, so it has no use for it and refuses it rather
	// than record it as a key nothing reads.
	for (i = 2; i + 1 < number_of_args; i += 2)
	{
		trap->Argv( i, key, sizeof( key ) );
		if (Q_stricmp(key, "aimoffset") == 0)
		{
			char *end = NULL;
			double value;

			if (!aim)
			{
				trap->SendServerCommand( ent-g_entities, "print \"aimoffset is for ^3/entaddaim^7: /entadd does not place the entity where you aim.\n\"" );
				return;
			}

			trap->Argv( i + 1, arg2, sizeof( arg2 ) );
			value = strtod( arg2, &end );
			if (!arg2[0] || !end || *end != '\0' || !( value >= 0.0 ))
			{
				trap->SendServerCommand( ent-g_entities, va("print \"aimoffset must be a number of units from 0 to %d.\n\"", (int)RP_AIMOFFSET_MAX) );
				return;
			}
			aim_offset = ( value > RP_AIMOFFSET_MAX ) ? RP_AIMOFFSET_MAX : (float)value;
		}
	}

	// GalaxyRP: [Entity System] worldspawn is the map itself, not an entity class (SP_worldspawn runs once,
	// from the map's own spawn pass): say so rather than "did not survive being spawned"
	if (Q_stricmp(arg1, "worldspawn") == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"worldspawn is the map itself, not an entity class.\n\"" );
		return;
	}

	// GalaxyRP: [Entity System] an NPC or vehicle spawner: a targetname and a type the server has
	if (RP_IsNpcSpawnerClass(arg1))
	{
		const char *problem = zyk_entadd_spawner_problem(arg1, number_of_args);

		if (problem)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", problem) );
			return;
		}
	}

	if (aim)
	{
		if ((ent->client->ps.pm_flags & PMF_FOLLOW) || ent->client->sess.spectatorState == SPECTATOR_FOLLOW)
		{
			trap->SendServerCommand( ent-g_entities, "print \"You are following another player. Stop following first.\n\"" );
			return;
		}

		if (!grab && RP_EntAddAimPoint(ent, aim_point, aim_normal) == qfalse)
		{
			trap->SendServerCommand( ent-g_entities, "print \"You are not aiming at a surface.\n\"" );
			return;
		}

		if (!grab && aim_offset > 0.0f)
			VectorMA(aim_point, aim_offset, aim_normal, aim_point);
	}
	else
	{
		for (i = 2; i + 1 < number_of_args; i += 2)
		{
			trap->Argv( i, key, sizeof( key ) );
			if (Q_stricmp(key, "origin") == 0)
				typed_origin = qtrue;
		}

		if (typed_origin == qfalse && level.ent_origin_set == qfalse &&
			((ent->client->ps.pm_flags & PMF_FOLLOW) || ent->client->sess.spectatorState == SPECTATOR_FOLLOW))
		{
			trap->SendServerCommand( ent-g_entities, "print \"You are following another player. Stop following first, or give an origin.\n\"" );
			return;
		}
	}

	RP_SpawnRouteInit(&route);
	RP_SpawnRouteNoteKey(&route, "classname", arg1);
	for (i = 2; i + 1 < number_of_args; i += 2)
	{
		trap->Argv( i, key, sizeof( key ) );
		trap->Argv( i + 1, arg2, sizeof( arg2 ) );
		RP_SpawnRouteNoteKey(&route, key, arg2);
	}

	if ( RP_SpawnRouteHasRoom( &route ) == qfalse )
	{
		if ( RP_SpawnRouteIsLogical( &route ) )
		{
			trap->SendServerCommand( ent-g_entities,
				va("print \"Cannot add an entity: the server is near its logical entity limit. %d logical slots free, %d held in reserve.\n\"",
					G_FreeLogicalEntityCount(), ZYK_LOGICAL_ENTITY_RESERVE) );
			G_LogPrintf( "/entadd '%s' by %s refused: %d logical entity slots free\n",
				arg1, ent->client->pers.netname, G_FreeLogicalEntityCount() );
			return;
		}
		trap->SendServerCommand( ent-g_entities,
			va("print \"Cannot add an entity: the server is near its entity limit. %d slots free, %d held in reserve.\n\"",
				G_FreeEntityCount(), ZYK_ENTITY_RESERVE) );
		G_LogPrintf( "/entadd '%s' by %s refused: %d entity slots free\n",
			arg1, ent->client->pers.netname, G_FreeEntityCount() );
		return;
	}

	if ( G_AllocRoomFor( zyk_args_bytes( 1, number_of_args ) ) == qfalse )
	{
		trap->SendServerCommand( ent-g_entities,
			va("print \"Cannot add an entity: the game's memory pool is nearly full (%d KB free, %d KB held in reserve). It is freed when the map changes.\n\"",
				G_AllocRemaining() / 1024, ZYK_ALLOC_RESERVE / 1024) );
		G_LogPrintf( "/entadd '%s' by %s refused: %d bytes free in the memory pool\n",
			arg1, ent->client->pers.netname, G_AllocRemaining() );
		return;
	}

	// zyk: spawns the new entity
	new_ent = RP_SpawnForRoute(&route);

	if (new_ent)
	{
		strcpy(key,"");

		// zyk: setting the entity classname
		zyk_main_set_entity_field(new_ent, "classname", arg1);

		for(i = 2; i < number_of_args; i++)
		{
			if (i % 2 == 0)
			{ // zyk: key
				trap->Argv( i, arg2, sizeof( arg2 ) );
				Q_strncpyz(key, arg2, sizeof(key));

				if (Q_stricmp(key, "origin") == 0)
				{ // zyk: if origin was passed
					has_origin_set = qtrue;
					// GalaxyRP: [Entity System] /entaddaim places it itself
					if (aim)
						aim_origin_ignored = qtrue;
				}

				if (Q_stricmp(key, "angles") == 0)
				{ // zyk: if angles was passed
					has_angles_set = qtrue;
				}
			}
			else
			{ // zyk: value
				trap->Argv( i, arg2, sizeof( arg2 ) );

				if (aim && Q_stricmp(key, "origin") == 0)
					continue;
				// GalaxyRP: [Entity System] an instruction to /entaddaim, not a key of the entity (see above)
				if (Q_stricmp(key, "aimoffset") == 0)
					continue;
				RP_EntScaleValueStored(arg1, key, arg2, sizeof(arg2));	// DAJ_RP: [NPC Scale] npcscale 5 is stored as 20
				zyk_main_set_entity_field(new_ent, key, arg2);
			}
		}

		if (grab)
		{
			RP_EntAddGrabStart(ent, new_ent, aim_offset, aim_origin_ignored);
			return;
		}

		if (aim)
		{ // GalaxyRP: [Entity System] /entaddaim: on the surface aimed at, by the box its class is placed by
			vec3_t mins, maxs, origin;

			RP_EntGrabPlaceBox(arg1, NULL, mins, maxs);
			RP_EntGrabPlace(aim_point, aim_normal, mins, maxs, origin);
			zyk_main_set_entity_field(new_ent, "origin", va("%i %i %i", (int)origin[0], (int)origin[1], (int)origin[2]));
		}
		else if (level.ent_origin_set == qtrue && (has_origin_set == qfalse || has_angles_set == qfalse))
		{ // zyk: if origin or angles were not passed, use the origin or angles set with /entorigin
			if (has_origin_set == qfalse)
			{
				zyk_main_set_entity_field(new_ent, "origin", va("%f %f %f", level.ent_origin[0], level.ent_origin[1], level.ent_origin[2]));
			}

			if (has_angles_set == qfalse)
			{
				zyk_main_set_entity_field(new_ent, "angles", va("%f %f %f", level.ent_angles[0], level.ent_angles[1], level.ent_angles[2]));
			}
		}
		else if (has_origin_set == qfalse)
		{ 
			zyk_main_set_entity_field(new_ent, "origin", va("%f %f %f", ent->client->ps.origin[0], ent->client->ps.origin[1], ent->client->ps.origin[2]));
		}

		level.rp_spawn_refusal[0] = '\0';

		if (aim)
		{
			vec3_t mins, maxs;
			int free_before = G_FreeEntityCount();

			RP_EntGrabPlaceBox(arg1, NULL, mins, maxs);

			zyk_main_spawn_entity(new_ent);

			// GalaxyRP: [Entity System] now the box is known: set it against the surface where it rests,
			// unless it was placed by a player's box already -- see RP_EntGrabSettle() in g_entgrab.c
			if (new_ent->inuse && VectorCompare(mins, vec3_origin) && VectorCompare(maxs, vec3_origin))
			{
				gentity_t *settled = RP_EntGrabSettle(new_ent, aim_point, aim_normal, free_before);

				if (!settled)
				{
					trap->SendServerCommand( ent-g_entities, va("print \"The %s could not be placed%s%s.\n\"", arg1,
						level.rp_spawn_refusal[0] ? ": " : " (no free entity slot, or it did not survive being spawned again)",
						level.rp_spawn_refusal) );
					return;
				}
				new_ent = settled;
			}
		}
		else
		zyk_main_spawn_entity(new_ent);

		// GalaxyRP: [Entity System] the spawn can remove the entity at once: a model or effect file the
		// server does not have (RP_EntitySystemSpawnRefused), or a class that removes itself. It used to
		// report "Entity 0 spawned" -- the number of a freed slot.
		if (!new_ent->inuse)
		{
			if (level.rp_spawn_refusal[0])
				trap->SendServerCommand( ent-g_entities, va("print \"The %s was refused: %s.\n\"", arg1, level.rp_spawn_refusal) );
			else
				trap->SendServerCommand( ent-g_entities, va("print \"The %s did not survive being spawned (a class that removes itself, or a spawn its spawn function refused).\n\"", arg1) );
			return;
		}

		if (new_ent->s.number != 0)
		{
			level.last_spawned_entity = new_ent;
		}

		// GalaxyRP: [Logical Entities] say which region it went to; an id at or above
		// MAX_GENTITIES is a logical entity and the admin will see those ids in /entlist too.
		// GalaxyRP: [Entity System] where it went, for both; and a solid one built around someone (the admin
		// standing there for /entadd, a player or NPC aimed through for /entaddaim) says so
		trap->SendServerCommand( ent-g_entities, va("print \"Entity %d spawned at (%i %i %i)%s%s\n\"", new_ent->s.number,
			(int)new_ent->s.origin[0], (int)new_ent->s.origin[1], (int)new_ent->s.origin[2],
			new_ent->isLogical ? " (logical, not networked)" : "",
			aim_origin_ignored ? ". /entaddaim places it where you aim: the origin typed was not used." : "") );
		if (RP_EntitySolidAroundSomeone(new_ent))
			trap->SendServerCommand( ent-g_entities, "print \"^3It is solid and someone is inside it: step away, or ^7/entundo^3.\n\"" );
		if (level.rp_spawn_note[0])
			trap->SendServerCommand( ent-g_entities, va("print \"^3%s\n\"", level.rp_spawn_note) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Error in entity spawn\n\"") );
		return;
	}
}

void Cmd_EntAdd_f( gentity_t *ent ) {
	zyk_entadd( ent, qfalse, qfalse );
}

void Cmd_EntAddAim_f( gentity_t *ent ) {
	zyk_entadd( ent, qtrue, qfalse );
}

/*
==================
Cmd_EntAddGrab_f

DAJ_RP: [Entity System] /entaddgrab <classname> <key> <value>...: /entaddaim's arguments and every check /entadd
and /entaddaim make, but the new entity is picked up rather than added -- held unspawned, with a preview where it
would land -- and /entaddgrab again places it where the admin aims, spawning it there as /entaddaim would. See
RP_EntAddGrabStart() and RP_EntAddGrabHeld() in g_entgrab.c. /entcancel lets it go, and nothing is added.
==================
*/
void Cmd_EntAddGrab_f( gentity_t *ent ) {
	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	// zyk: holding one already: placed, or refused if another is typed (the other holds refuse in zyk_entadd)
	if (ent->client->pers.entHoldNum && ent->client->pers.entHoldMode == RP_HOLD_ADD)
	{
		RP_EntAddGrabHeld(ent);
		return;
	}

	zyk_entadd( ent, qtrue, qtrue );
}

/*
==================
Cmd_EntEdit_f
==================
*/
void Cmd_EntEdit_f( gentity_t *ent ) {
	gentity_t *this_ent = NULL;
	int number_of_args = trap->Argc();
	int entity_id = -1;
	int first_pair = 2;
	int i = 0;
	char key[MAX_STRING_CHARS];
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	// DAJ_RP: [Entity System] not while this admin holds an entity with /entcopy or /entcut (g_entgrab.c)
	if (RP_EntRefuseWhileHolding(ent))
	{
		return;
	}

	if (number_of_args >= 2)
	{
		trap->Argv( 1, arg1, sizeof( arg1 ) );
	}
	else
	{
		arg1[0] = '\0';
	}

	if ((arg1[0] >= '0' && arg1[0] <= '9') || arg1[0] == '-')
	{
		first_pair = 2;
		entity_id = atoi(arg1);
	}
	else
	{
		gentity_t *aimed;

		first_pair = 1;

		if (RP_EntAimFollowing(ent))
		{
			trap->SendServerCommand( ent-g_entities, "print \"You are following another player. Stop following first, or give the entity id.\n\"" );
			return;
		}

		aimed = RP_EntAimTarget(ent);
		if (!aimed)
		{
			trap->SendServerCommand( ent-g_entities, "print \"You are not aiming at an entity. Aim at one, or give its id: ^3/entedit <entity id> [key value ...]^7.\n\"" );
			return;
		}

		entity_id = aimed->s.number;
	}

	if (entity_id < 0 || entity_id >= MAX_GENTITIES + level.num_logicalents ||
		(entity_id >= level.num_entities && entity_id < MAX_GENTITIES))
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Invalid Entity ID.\n\"") );
		return;
	}

	this_ent = &g_entities[entity_id];

	if (number_of_args == first_pair)
	{
		// zyk: players have their origin and yaw set in ps struct
		if (entity_id < (MAX_CLIENTS + BODY_QUEUE_SIZE))
			trap->SendServerCommand( ent-g_entities, va("print \"\n^3entity: ^7%d\n^3classname: ^7%s\n^3origin: ^7%f %f %f\n\n\"", entity_id, this_ent->classname, this_ent->r.currentOrigin[0], this_ent->r.currentOrigin[1], this_ent->r.currentOrigin[2]) );
		else
		{
			char content[1024];

			// the old single send was va("print \"\n%s\n\"", content) -- the leading newline now
			// lives in the buffer so it is written once, ahead of any flush
			strcpy(content, "\n");

			// GalaxyRP: [Logical Entities] say which region the entity lives in, so an admin looking
			// at an id from /entlist knows why it is at or above MAX_GENTITIES and what an edit may
			// not do to it.
			if (this_ent->inuse)
			{
				// GalaxyRP: [Entity System] the id first: picked by aim, the admin has not typed it
				Q_strcat(content, sizeof(content), va("^3entity: ^7%d\n", entity_id));
				Q_strcat(content, sizeof(content), va("^3region: ^7%s\n",
					this_ent->isLogical ? "logical (not networked)" : "networked"));
			}

			// GalaxyRP fix: [Entity System] an entity the game created has no key/value record, so the
			// listing below would print nothing at all for it. Show what it is and who it belongs to
			// instead, and say why it cannot be edited or removed -- see RP_EntityHasSpawnKeys().
			if (this_ent->inuse && RP_EntityHasSpawnKeys(this_ent) == qfalse)
			{
				Q_strcat(content, sizeof(content), va("^3classname: ^7%s\n^3origin: ^7%f %f %f\n",
					this_ent->classname ? this_ent->classname : "<none>",
					this_ent->r.currentOrigin[0], this_ent->r.currentOrigin[1], this_ent->r.currentOrigin[2]));
				if (this_ent->r.ownerNum != ENTITYNUM_NONE)
				{
					Q_strcat(content, sizeof(content), va("^3owner: ^7%d\n", this_ent->r.ownerNum));
				}
				if (this_ent->parent)
				{
					Q_strcat(content, sizeof(content), va("^3parent: ^7%d\n", (int)(this_ent->parent - g_entities)));
				}
				Q_strcat(content, sizeof(content), "^3Created by the game, not by the map or the entity system: it cannot be edited or removed.\n");
			}

			if (this_ent->inuse)
			{
				while (i < level.zyk_spawn_strings_values_count[entity_id])
				{
					char entry[RP_LIST_FLUSH_AT];

					Com_sprintf(entry, sizeof(entry), "^3%s: ^7%s\n", RP_ShownText(level.zyk_spawn_strings[this_ent->s.number][i]), RP_ShownText(level.zyk_spawn_strings[this_ent->s.number][i + 1]));

					if ((int)(strlen(content) + strlen(entry)) > RP_LIST_FLUSH_AT)
					{
						trap->SendServerCommand(ent - g_entities, va("print \"%s\"", content));
						strcpy(content, "");
					}

					Q_strcat(content, sizeof(content), entry);

					i += 2;
				}

				Q_strcat(content, sizeof(content), "\n");
				trap->SendServerCommand(ent - g_entities, va("print \"%s\"", content));
			}
			else
			{
				trap->SendServerCommand(ent - g_entities, va("print \"Entity %d is not in use\n\"", entity_id));
			}
		}
	}
	else
	{
		if (entity_id < (MAX_CLIENTS + BODY_QUEUE_SIZE))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity ID %d is a reserved player or body slot and cannot be edited.\n\"", entity_id) );
			return;
		}
		
		if (!this_ent->inuse)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity %d is not in use\n\"", entity_id) );
			return;
		}

		if (RP_EntityHasSpawnKeys(this_ent) == qfalse)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity %d %s.\n\"", entity_id, RP_EntityRefusalReason(this_ent)) );
			return;
		}

		if (RP_MapEntityProtected(this_ent))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity %d is part of the map (marked M in /entlist) and cannot be edited%s. Use ^3/entcopy^7 to place a copy of it and edit that instead.\n\"",
				entity_id, RP_MapEntityRefusalNote(this_ent)) );
			return;
		}

		if (this_ent->rpSubBSPOf > 0 && RP_MiscBspForInstance(this_ent->rpSubBSPOf))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity %d belongs to misc_bsp %d: edit or remove the misc_bsp instead.\n\"",
				entity_id, RP_MiscBspForInstance(this_ent->rpSubBSPOf)->s.number) );
			return;
		}

		// GalaxyRP: [Entity System] an admin is holding it with /entcopy or /entcut (g_entgrab.c)
		if (this_ent->rpHeldBy)
		{
			// DAJ_RP: [Entity System] by another admin: who, and with what (g_entgrab.c)
			trap->SendServerCommand( ent-g_entities, va("print \"Entity %d is %s: wait until they drop it or let go.\n\"", entity_id, RP_EntHeldByText(this_ent)) );
			return;
		}

		// GalaxyRP: [Entity System] counted from where the pairs start, with or without an id
		if ( (number_of_args - first_pair) % 2 != 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"Each key needs a value: give key/value pairs.\n\"" );
			return;
		}

		for (i = first_pair; i + 1 < number_of_args; i += 2)
		{
			trap->Argv(i, key, sizeof(key));
			if (Q_stricmp(key, "classname") == 0)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"Entity %d: the classname cannot be changed with /entedit. Use ^3/entremove^7 and ^3/entadd^7 the new class instead.\n\"", entity_id) );
				return;
			}
		}

		if (RP_IsNpcSpawnerClass(this_ent->classname))
		{
			for (i = first_pair; i + 1 < number_of_args; i += 2)
			{
				trap->Argv(i, key, sizeof(key));
				trap->Argv(i + 1, arg2, sizeof(arg2));
				if (Q_stricmp(key, "targetname") == 0 && (!arg2[0] || Q_stricmp(arg2, "zykremovekey") == 0))
				{
					trap->SendServerCommand( ent-g_entities, va("print \"Entity %d is an NPC or vehicle spawner: it needs a ^3targetname^7. Give it another name instead.\n\"", entity_id) );
					return;
				}
				if (Q_stricmp(key, "npc_type") == 0 && !Q_stricmp(this_ent->classname, "npc_spawner"))
				{
					if (!arg2[0] || Q_stricmp(arg2, "zykremovekey") == 0)
					{
						trap->SendServerCommand( ent-g_entities, va("print \"Entity %d is an npc_spawner: it needs an ^3npc_type^7 (see ^3/list npcs^7).\n\"", entity_id) );
						return;
					}
					if (RP_NpcTypeIsVehicle(arg2) || !RP_NpcTypeKnown(arg2))
					{
						trap->SendServerCommand( ent-g_entities, va("print \"^3%s^7 is not an NPC type on this server (see ^3/list npcs^7; a vehicle goes in an ^3NPC_Vehicle^7).\n\"", zyk_shown_name(arg2)) );
						return;
					}
				}
				if (Q_stricmp(key, "npc_type") == 0 && !Q_stricmp(this_ent->classname, "npc_vehicle") &&
					Q_stricmp(arg2, "zykremovekey") != 0 && (!RP_VehicleTypeKnown(arg2) || !RP_NpcTypeKnown(arg2)))
				{
					trap->SendServerCommand( ent-g_entities, va("print \"^3%s^7 is not a vehicle type this server can spawn (see ^3/list vehicles^7).\n\"", zyk_shown_name(arg2)) );
					return;
				}
				// GalaxyRP: [Entity System] "npceffect" -- removing it is always fine
				if (Q_stricmp(key, "npceffect") == 0 && Q_stricmp(arg2, "zykremovekey") != 0)
				{
					const char *problem_effect = zyk_spawner_effect_problem(!Q_stricmp(this_ent->classname, "npc_vehicle") ? qtrue : qfalse, arg2);

					if (problem_effect)
					{
						trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", problem_effect) );
						return;
					}
				}
				// GalaxyRP: [Entity System] "npcteam" -- removing it is always fine
				if (Q_stricmp(key, "npcteam") == 0 && Q_stricmp(arg2, "zykremovekey") != 0)
				{
					const char *problem_team = zyk_spawner_team_problem(arg2);

					if (problem_team)
					{
						trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", problem_team) );
						return;
					}
				}
				// DAJ_RP: [NPC Rewards] "npccredits" and "npcxp" -- removing them is always fine
				if ((Q_stricmp(key, "npccredits") == 0 || Q_stricmp(key, "npcxp") == 0) && Q_stricmp(arg2, "zykremovekey") != 0)
				{
					const char *problem_reward = zyk_spawner_reward_problem(!Q_stricmp(this_ent->classname, "npc_vehicle") ? qtrue : qfalse, key, arg2);

					if (problem_reward)
					{
						trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", problem_reward) );
						return;
					}
				}
				// DAJ_RP: [NPC System] "npcfreeze" -- removing it is always fine
				if (Q_stricmp(key, "npcfreeze") == 0 && Q_stricmp(arg2, "zykremovekey") != 0)
				{
					const char *problem_freeze = zyk_spawner_freeze_problem(!Q_stricmp(this_ent->classname, "npc_vehicle") ? qtrue : qfalse, arg2);

					if (problem_freeze)
					{
						trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", problem_freeze) );
						return;
					}
				}
				// DAJ_RP: [NPC Scale] "npcscale" -- removing it is always fine
				if (Q_stricmp(key, "npcscale") == 0 && Q_stricmp(arg2, "zykremovekey") != 0)
				{
					const char *problem_scale = zyk_spawner_scale_problem(arg2);

					if (problem_scale)
					{
						trap->SendServerCommand( ent-g_entities, va("print \"%s\n\"", problem_scale) );
						return;
					}
				}
			}
		}

		for (i = first_pair; i + 1 < number_of_args; i += 2)
		{
			trap->Argv(i, key, sizeof(key));
			trap->Argv(i + 1, arg2, sizeof(arg2));
			if (Q_stricmp(key, "model") == 0 && Q_stricmp(arg2, "zykremovekey") != 0)
			{
				const char *problem_model = RP_BrushModelProblem(arg2);

				if (problem_model)
				{
					trap->SendServerCommand( ent-g_entities, va("print \"Entity %d was not edited: %s.\n\"", entity_id, problem_model) );
					return;
				}
			}
		}

		if (zyk_spawn_strings_full(this_ent) == qtrue ||
			(level.zyk_spawn_strings_values_count[entity_id] + (number_of_args - first_pair)) > ZYK_MAX_SPAWN_STRING_SLOTS)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity %d cannot hold that many key/value pairs (maximum %d).\n\"", entity_id, ZYK_MAX_SPAWN_STRING_SLOTS / 2) );
			return;
		}

		if ( G_AllocRoomFor( RP_EntityRecordBytes( this_ent ) + zyk_args_bytes( first_pair, number_of_args ) ) == qfalse )
		{
			trap->SendServerCommand( ent-g_entities,
				va("print \"Cannot edit entity %d: the game's memory pool is nearly full (%d KB free, %d KB held in reserve). It is freed when the map changes.\n\"",
					entity_id, G_AllocRemaining() / 1024, ZYK_ALLOC_RESERVE / 1024) );
			G_LogPrintf( "/entedit %d by %s refused: %d bytes free in the memory pool\n",
				entity_id, ent->client->pers.netname, G_AllocRemaining() );
			return;
		}

		{
			rpSpawnRoute_t route;
			int j = 0;

			RP_SpawnRouteInit(&route);
			while (j + 1 < level.zyk_spawn_strings_values_count[entity_id])
			{
				RP_SpawnRouteNoteKey(&route, level.zyk_spawn_strings[entity_id][j], level.zyk_spawn_strings[entity_id][j + 1]);
				j += 2;
			}
			for (i = first_pair; i + 1 < number_of_args; i += 2)
			{
				trap->Argv(i, key, sizeof(key));
				trap->Argv(i + 1, arg2, sizeof(arg2));
				// zyk: "zykremovekey" removes the key, so for the decision it is as if it were absent
				if (Q_stricmp(arg2, "zykremovekey") == 0)
				{
					if (Q_stricmp(key, "classname") == 0)
						route.classname[0] = '\0';
					else if (Q_stricmp(key, "nological") == 0)
						route.nological = qfalse;
					else if (Q_stricmp(key, "script_targetname") == 0)
						route.hasScriptTargetname = qfalse;
					continue;
				}
				RP_SpawnRouteNoteKey(&route, key, arg2);
			}

			if (RP_SpawnRouteIsLogical(&route) != this_ent->isLogical)
			{
				trap->SendServerCommand( ent-g_entities, va("print \"Entity %d is %s and this edit would make it %s. An entity cannot change between the networked and logical regions in place: /entremove it and /entadd it again instead.\n\"",
					entity_id, this_ent->isLogical ? "logical" : "networked", this_ent->isLogical ? "networked" : "logical") );
				return;
			}
		}

		strcpy(key,"");

		for(i = first_pair; i < number_of_args; i++)
		{
			if ((i - first_pair) % 2 == 0)
			{ // zyk: key
				trap->Argv(i, arg2, sizeof(arg2));
				Q_strncpyz(key, arg2, sizeof(key));
			}
			else
			{ // zyk: value
				trap->Argv(i, arg2, sizeof(arg2));

				// GalaxyRP fix: [Entity System] as in Cmd_EntAdd_f -- the helper copies both
				// arguments itself, so these two allocations were discarded the moment it returned.
				RP_EntScaleValueStored(this_ent->classname, key, arg2, sizeof(arg2));	// DAJ_RP: [NPC Scale] as /entadd
				zyk_main_set_entity_field(this_ent, key, arg2);
			}
		}

		this_ent->rpMapEntity = qfalse;

		RP_EntRespawnPrepare(this_ent);

		zyk_main_spawn_entity(this_ent);

		if (this_ent->inuse)
		{
			trap->SendServerCommand(ent-g_entities, va("print \"Entity %d edited\n\"", entity_id) );
			if (level.rp_spawn_note[0])
				trap->SendServerCommand( ent-g_entities, va("print \"^3%s\n\"", level.rp_spawn_note) );
		}
		else if (level.rp_spawn_refusal[0])
			trap->SendServerCommand(ent-g_entities, va("print \"Entity %d was edited, but it was refused when spawned again (%s), so it is gone.\n\"", entity_id, level.rp_spawn_refusal) );
		else
			trap->SendServerCommand(ent-g_entities, va("print \"Entity %d was edited, but it did not survive being spawned again (a class that removes itself on the server, or an edit its spawn function refused), so it is gone.\n\"", entity_id) );
	}
}

typedef enum {
	ZYK_ENTSAVE_NPC_SAVE = 0,
	ZYK_ENTSAVE_NPC_SAVE_VEHICLE,
	ZYK_ENTSAVE_NPC_DEAD,
	ZYK_ENTSAVE_NPC_DROID,
	ZYK_ENTSAVE_NPC_RIDER,
	ZYK_ENTSAVE_NPC_FROM_SPAWNER
} zyk_entsave_npc_t;

static qboolean zyk_entsave_is_vehicle(const gentity_t *npc)
{
	return (npc->m_pVehicle || npc->s.NPC_class == CLASS_VEHICLE || (npc->client && npc->client->NPC_class == CLASS_VEHICLE)) ? qtrue : qfalse;
}

void NPC_Begin(gentity_t *ent);

static zyk_entsave_npc_t zyk_entsave_npc_skip_reason(gentity_t *npc)
{
	const qboolean starting = (npc->think == NPC_Begin && !npc->rpBaseTeam && !(npc->s.eFlags & EF_DEAD)) ? qtrue : qfalse;
	const gentity_t *spawner = npc->zyk_npc_spawner;
	const qboolean is_vehicle = zyk_entsave_is_vehicle(npc);
	const int veh_num = npc->s.m_iVehicleNum;

	// zyk: dead, or a vehicle already counting down to its explosion (StartDeathDelay() sets
	// m_iDieTime, and nothing but the vehicle's reset clears it).
	if (!starting && (npc->health <= 0 || (npc->s.eFlags & EF_DEAD) || (npc->client && npc->client->ps.pm_type == PM_DEAD) ||
		(npc->m_pVehicle && npc->m_pVehicle->m_iDieTime != 0)))
	{
		return ZYK_ENTSAVE_NPC_DEAD;
	}

	// zyk: a droid or a rider is only ever a non-vehicle. A vehicle's own m_iVehicleNum holds its
	// PILOT's number (plus one), so the tests below must never be asked about a vehicle.
	if (!is_vehicle && veh_num > 0 && veh_num < ENTITYNUM_WORLD)
	{
		const gentity_t *veh = &g_entities[veh_num];

		if (veh->inuse && veh->m_pVehicle)
		{
			int k;

			// zyk: only a droid its vehicle is actually carrying -- the vehicle names it as m_pDroidUnit
			if (veh->m_pVehicle->m_pDroidUnit == (bgEntity_t *)npc)
			{
				return ZYK_ENTSAVE_NPC_DROID;
			}

			// zyk: and only a rider the vehicle actually has, as its pilot or in a passenger seat
			if (veh->m_pVehicle->m_pPilot == (bgEntity_t *)npc)
			{
				return ZYK_ENTSAVE_NPC_RIDER;
			}

			for (k = 0; k < VEH_MAX_PASSENGERS; k++)
			{
				if (veh->m_pVehicle->m_ppPassengers[k] == (bgEntity_t *)npc)
				{
					return ZYK_ENTSAVE_NPC_RIDER;
				}
			}
		}
	}

	// zyk: the spawner must still be the one that made this NPC or vehicle (same slot, same id -- a
	// freed and reused slot is zeroed, so its id cannot match) and must have a record, i.e. /entsave
	// is writing it out too. The /npc spawn command's one-frame spawner never has a record.
	if (spawner && spawner != npc && npc->zyk_npc_spawner_id != 0 && spawner->inuse &&
		spawner->zyk_spawner_id == npc->zyk_npc_spawner_id &&
		level.zyk_spawn_strings_values_count[spawner->s.number] > 0)
	{
		return ZYK_ENTSAVE_NPC_FROM_SPAWNER;
	}

	return is_vehicle ? ZYK_ENTSAVE_NPC_SAVE_VEHICLE : ZYK_ENTSAVE_NPC_SAVE;
}

static char zyk_entsave_line[ZYK_ENTITY_FILE_ENCODED_LENGTH * 2];

// the NPC_spawner / NPC_Vehicle spawnflags these lines carry by name (b_local.h has the first three, which this
// file does not include)
#define ZYK_ENTSAVE_CINEMATIC		32
#define ZYK_ENTSAVE_NOTSOLID		64
#define ZYK_ENTSAVE_STARTINSOLID	128

static qboolean zyk_entsave_add(const char *key, const char *value)
{
	static char escaped[ZYK_ENTITY_FILE_ENCODED_LENGTH];
	const int used = (int)strlen(zyk_entsave_line);

	if (zyk_entity_file_encode(value, escaped, sizeof(escaped)) == qfalse)
	{
		return qfalse;
	}

	if (used + (int)strlen(key) + (int)strlen(escaped) + 2 >= (int)sizeof(zyk_entsave_line))
	{
		return qfalse;
	}

	Q_strcat(zyk_entsave_line, sizeof(zyk_entsave_line), va("%s;%s;", key, escaped));
	return qtrue;
}

// the word "npcteam" takes for a team, or NULL for none of the four
static const char *zyk_entsave_team_word(int team)
{
	switch (team)
	{
	case NPCTEAM_PLAYER:	return "player";
	case NPCTEAM_ENEMY:		return "enemy";
	case NPCTEAM_NEUTRAL:	return "neutral";
	case NPCTEAM_FREE:		return "free";
	default:				return NULL;
	}
}

static const char *zyk_entsave_team(const gentity_t *ent)
{
	if (!ent->rpBaseTeam)
	{
		return (ent->rpSpawnerTeam > 0) ? zyk_entsave_team_word(ent->rpSpawnerTeam - 1) : NULL;
	}

	if (RP_TEAM_RECORD(ent->client->playerTeam, ent->client->enemyTeam) == ent->rpBaseTeam)
	{
		return NULL;
	}

	return zyk_entsave_team_word(ent->client->playerTeam);
}

// The keys a one-time line shares between NPCs and vehicles. with_text qfalse leaves out the free-text ones
// (name, on-death target, droid type) -- the fallback when they make the line too long to load back.
static qboolean zyk_entsave_add_common(const gentity_t *ent, int spawnflags, qboolean with_text)
{
	const char *team = zyk_entsave_team(ent);

	if (spawnflags && !zyk_entsave_add("spawnflags", va("%d", spawnflags)))
		return qfalse;
	if (team && !zyk_entsave_add("npcteam", team))
		return qfalse;
	if (with_text && ent->targetname && ent->targetname[0] && !zyk_entsave_add("NPC_targetname", ent->targetname))
		return qfalse;
	if (with_text && ent->target && ent->target[0] && !zyk_entsave_add("NPC_target", ent->target))
		return qfalse;
	if (ent->s.shouldtarget && !zyk_entsave_add("showhealth", "1"))
		return qfalse;
	// DAJ_RP: [NPC Scale] the size its spawner's "npcscale" gave it (gentity_t::rpNpcScale), so it comes back that size. One
	// scaled only by its .npc file writes nothing: the file stays in charge.
	if (ent->rpNpcScale > 0 && !zyk_entsave_add("npcscale", va("%d", ent->rpNpcScale)))
		return qfalse;

	return qtrue;
}

// the npceffect word for an /npc effect mode, or NULL for none
static const char *zyk_entsave_effect_word(int mode)
{
	switch (mode)
	{
	case RP_PHASE_HOLO:		return "holo";
	case RP_PHASE_GHOST:	return "ghost";
	case RP_PHASE_NONSOLID:	return "walkthrough";	// DAJ_RP: [NPC System] was "nonsolid" -- RP_NpcEffectFromName()
	default:				return NULL;
	}
}

static const char *zyk_entsave_npc_line(gentity_t *ent, const char *escaped_type, qboolean with_text)
{
	int spawnflags = ent->spawnflags & (1 | 2 | 4 | 8 | ZYK_ENTSAVE_CINEMATIC | ZYK_ENTSAVE_STARTINSOLID);
	const char *effect = zyk_entsave_effect_word(ent->client->pers.phase_mode);

	if ((ent->spawnflags & ZYK_ENTSAVE_NOTSOLID) && (!ent->rpBaseTeam || ent->r.contents == 0))
	{
		spawnflags |= ZYK_ENTSAVE_NOTSOLID;
	}

	zyk_entsave_line[0] = '\0';
	Q_strcat(zyk_entsave_line, sizeof(zyk_entsave_line), va("classname;npc_spawner;npc_type;%s;origin;%f %f %f;angles;%f %f %f;", escaped_type,
		ent->client->ps.origin[0], ent->client->ps.origin[1], ent->client->ps.origin[2],
		ent->client->ps.viewangles[0], ent->client->ps.viewangles[1], ent->client->ps.viewangles[2]));

	if (!zyk_entsave_add_common(ent, spawnflags, with_text))
		return NULL;
	if (ent->rpRewardCredits > 0 && !zyk_entsave_add("npccredits", va("%d", ent->rpRewardCredits)))
		return NULL;
	if (ent->rpRewardXP > 0 && !zyk_entsave_add("npcxp", va("%d", ent->rpRewardXP)))
		return NULL;
	if (effect && !zyk_entsave_add("npceffect", effect))
		return NULL;
	if (RP_NpcIsFrozen(ent) && !zyk_entsave_add("npcfreeze", "1"))
		return NULL;
	if ((ent->r.svFlags & SVF_NO_BASIC_SOUNDS) && !zyk_entsave_add("noBasicSounds", "1"))
		return NULL;
	if ((ent->r.svFlags & SVF_NO_COMBAT_SOUNDS) && !zyk_entsave_add("noCombatSounds", "1"))
		return NULL;
	if ((ent->r.svFlags & SVF_NO_EXTRA_SOUNDS) && !zyk_entsave_add("noExtraSounds", "1"))
		return NULL;

	return zyk_entsave_line;
}

static const char *zyk_entsave_vehicle_line(gentity_t *ent, const char *escaped_type, qboolean with_text)
{
	const int spawnflags = ent->spawnflags & (1 | 2 | ZYK_ENTSAVE_CINEMATIC | ZYK_ENTSAVE_NOTSOLID | ZYK_ENTSAVE_STARTINSOLID);

	// zyk: SP_NPC_Vehicle() and G_VehicleSpawn() use only the yaw, so pitch and roll go out as 0
	zyk_entsave_line[0] = '\0';
	Q_strcat(zyk_entsave_line, sizeof(zyk_entsave_line), va("classname;NPC_Vehicle;NPC_type;%s;origin;%f %f %f;angles;0 %f 0;", escaped_type,
		ent->client->ps.origin[0], ent->client->ps.origin[1], ent->client->ps.origin[2],
		ent->client->ps.vehOrientation[YAW]));

	if (!zyk_entsave_add_common(ent, spawnflags, with_text))
		return NULL;
	if ((spawnflags & 1) && (!zyk_entsave_add("dmg", va("%d", ent->damage)) || !zyk_entsave_add("speed", va("%.9g", ent->speed))))
		return NULL;
	if ((spawnflags & 2) && ent->fly_sound_debounce_time > 0 &&
		!zyk_entsave_add("dropTime", va("%.4f", (ent->fly_sound_debounce_time - 0.5) / 1000.0)))
		return NULL;
	if (with_text && ent->model2 && ent->model2[0] && !zyk_entsave_add("model2", ent->model2))
		return NULL;

	return zyk_entsave_line;
}

static char *zyk_entsave_finish(gentity_t *ent, const char *escaped_type, qboolean vehicle)
{
	const char *line = vehicle ? zyk_entsave_vehicle_line(ent, escaped_type, qtrue) : zyk_entsave_npc_line(ent, escaped_type, qtrue);

	if (!line || (int)strlen(line) >= ZYK_ENTITY_FILE_LINE_LENGTH - 1)
	{
		line = vehicle ? zyk_entsave_vehicle_line(ent, escaped_type, qfalse) : zyk_entsave_npc_line(ent, escaped_type, qfalse);

		if (!line || (int)strlen(line) >= ZYK_ENTITY_FILE_LINE_LENGTH - 1)
		{
			G_LogPrintf("entsave: %s %d (%s) not saved: its line does not fit in %d characters\n",
				vehicle ? "vehicle" : "NPC", ent->s.number, ent->NPC_type, ZYK_ENTITY_FILE_LINE_LENGTH - 2);
			return NULL;
		}

		G_LogPrintf("entsave: %s %d (%s) saved without its name, on-death target and droid type: with them its line would be too long to load back\n",
			vehicle ? "vehicle" : "NPC", ent->s.number, ent->NPC_type);
	}

	Q_strcat(zyk_entsave_line, sizeof(zyk_entsave_line), "\n");
	return zyk_entsave_line;
}

char *create_vehicle_spawner_for_vehicle(gentity_t *ent) {
	static char escaped_type[ZYK_ENTITY_FILE_ENCODED_LENGTH];

	if (!ent->NPC_type || zyk_entity_file_encode(ent->NPC_type, escaped_type, sizeof(escaped_type)) == qfalse)
	{
		G_LogPrintf("entsave: vehicle %d (%s) not saved: its type is missing or does not fit in %d characters once escaped\n",
			ent->s.number, ent->NPC_type ? ent->NPC_type : "no type", (int)sizeof(escaped_type) - 1);

		return NULL;
	}

	return zyk_entsave_finish(ent, escaped_type, qtrue);
}

// GalaxyRP (Alex): Builds an npc spawner string based on an npc entity.
char *create_npc_spawner_for_npc(gentity_t *ent) {
	static char escaped_type[ZYK_ENTITY_FILE_ENCODED_LENGTH];

	if (zyk_entity_file_encode(ent->NPC_type, escaped_type, sizeof(escaped_type)) == qfalse)
	{
		G_LogPrintf("entsave: NPC %d (%s) not saved: its type does not fit in %d characters once escaped\n",
			ent->s.number, ent->NPC_type ? ent->NPC_type : "no type", (int)sizeof(escaped_type) - 1);

		return NULL;
	}

	return zyk_entsave_finish(ent, escaped_type, qfalse);
}

/*
==================
Cmd_EntSave_f
==================
*/
void Cmd_EntSave_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	int i = 0;
	int j = 0;
	gentity_t *this_ent = NULL;
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	FILE *this_file = NULL;
	// zyk: worst case every character of a token is escaped, so twice the longest line the loader
	// will read. static because the pair is far too large for this function's stack.
	static char escaped_key[ZYK_ENTITY_FILE_ENCODED_LENGTH];
	static char escaped_value[ZYK_ENTITY_FILE_ENCODED_LENGTH];
	// GalaxyRP fix: [Entity System] counted so the admin gets one line at the end instead of one
	// server command per offending entity -- see the log calls in the loop below
	int over_long = 0;
	int not_encodable = 0;
	// GalaxyRP fix: [Entity System] live NPCs left out on purpose -- see zyk_entsave_npc_skip_reason()
	int npcs_left_to_spawners = 0;
	int riders_skipped = 0;
	// DAJ_RP: corpses and exploding vehicles left out, and vehicles written as NPC_Vehicle lines
	int dead_skipped = 0;
	int vehicles_saved = 0;
	int sub_bsp_skipped = 0;
	char skipped_note[256] = {0};

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent->s.number, va("print \"You must specify a file name.\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand( ent->s.number, "print \"Invalid file name. Only letters and numbers allowed.\n\"" );
		return;
	}

	if (zyk_preset_name_too_long(ent, arg1) == qtrue)
	{
		return;
	}

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("entities/%s", zyk_mapname));

	// zyk: saving the entities into the file
	this_file = fopen(va("GalaxyRP/entities/%s/%s.txt",zyk_mapname,arg1),"w");

	if (!this_file)
	{
		trap->SendServerCommand( ent->s.number, va("print \"Could not open file %s for writing.\n\"", arg1) );
		return;
	}

	RP_FOR_EACH_ENTITY( this_ent )
	{
		i = this_ent - g_entities;
		if (i < (MAX_CLIENTS + BODY_QUEUE_SIZE))
			continue;

		if (this_ent->inuse && this_ent->rpSubBSPOf > 0)
		{
			if (level.zyk_spawn_strings_values_count[this_ent->s.number] > 0)
				sub_bsp_skipped++;
			continue;
		}

		if (RP_EntHeldUnplaced(this_ent))
		{
			continue;
		}

		if (this_ent && this_ent->inuse)
		{ // zyk: freed entities will not be saved
			int line_length = 0;
			qboolean encoded_all = qtrue;

			j = 0;

			while (j < level.zyk_spawn_strings_values_count[this_ent->s.number])
			{
				if (zyk_entity_file_encode(level.zyk_spawn_strings[this_ent->s.number][j], escaped_key, sizeof(escaped_key)) == qfalse ||
					zyk_entity_file_encode(level.zyk_spawn_strings[this_ent->s.number][j + 1], escaped_value, sizeof(escaped_value)) == qfalse)
				{
					encoded_all = qfalse;
					break;
				}

				fprintf(this_file, "%s;%s;", escaped_key, escaped_value);

				line_length += strlen(escaped_key) + strlen(escaped_value) + 2;

				j += 2;
			}

			if (encoded_all == qfalse)
			{
				fprintf(this_file, "\\");

				not_encodable++;
				G_LogPrintf("entsave: entity %d (%s) not saved: a key or value does not fit in %d characters once escaped\n",
					this_ent->s.number, this_ent->classname ? this_ent->classname : "no classname",
					(int)sizeof(escaped_key) - 1);
			}

			if (j > 0 || encoded_all == qfalse)
			{ // zyk: break line only if the entity had keys and values to save
				fprintf(this_file, "\n");
			}

			if (line_length >= (ZYK_ENTITY_FILE_LINE_LENGTH - 1))
			{
				over_long++;
				G_LogPrintf("entsave: entity %d (%s) needs %d characters and will not load back (limit %d)\n",
					this_ent->s.number, this_ent->classname ? this_ent->classname : "no classname",
					line_length, ZYK_ENTITY_FILE_LINE_LENGTH - 1);
			}

			if (this_ent->client && this_ent->classname && strcmp(this_ent->classname, "NPC") == 0) {
				const zyk_entsave_npc_t skip = zyk_entsave_npc_skip_reason(this_ent);

				if (skip == ZYK_ENTSAVE_NPC_FROM_SPAWNER)
				{
					npcs_left_to_spawners++;
				}
				else if (skip == ZYK_ENTSAVE_NPC_DEAD)
				{
					dead_skipped++;
					G_LogPrintf("entsave: %s %d (%s) not saved: dead or being destroyed\n",
						zyk_entsave_is_vehicle(this_ent) ? "vehicle" : "NPC", this_ent->s.number,
						this_ent->NPC_type ? this_ent->NPC_type : "no type");
				}
				else if (skip == ZYK_ENTSAVE_NPC_DROID || skip == ZYK_ENTSAVE_NPC_RIDER)
				{
					riders_skipped++;
					G_LogPrintf("entsave: %s %d (%s) not saved: %s\n",
						skip == ZYK_ENTSAVE_NPC_DROID ? "droid" : "NPC", this_ent->s.number,
						this_ent->NPC_type ? this_ent->NPC_type : "no type",
						skip == ZYK_ENTSAVE_NPC_DROID ? "its vehicle spawns it again"
						                              : "it is riding a vehicle, which is saved empty");
				}
				else
				{
					const char *npc_line = (skip == ZYK_ENTSAVE_NPC_SAVE_VEHICLE) ?
						create_vehicle_spawner_for_vehicle(this_ent) : create_npc_spawner_for_npc(this_ent);

					// zyk: NULL means the type did not encode in full, and it has already logged why
					if (npc_line)
					{
						fprintf(this_file, "%s", npc_line);

						if (skip == ZYK_ENTSAVE_NPC_SAVE_VEHICLE)
							vehicles_saved++;
					}
					else
						not_encodable++;
				}
			}
		}
	}

	fclose(this_file);

	if (npcs_left_to_spawners > 0 || riders_skipped > 0 || dead_skipped > 0)
	{
		Com_sprintf(skipped_note, sizeof(skipped_note), "^7Not written: %d NPC(s)/vehicle(s) their spawners recreate, %d rider(s)/droid(s), %d dead or exploding.\n",
			npcs_left_to_spawners, riders_skipped, dead_skipped);
	}

	if (vehicles_saved > 0)
	{
		Q_strcat(skipped_note, sizeof(skipped_note), va("^7Vehicles saved at their current positions: %d.\n", vehicles_saved));
	}

	if (sub_bsp_skipped > 0)
	{
		Q_strcat(skipped_note, sizeof(skipped_note), va("^7Not written: %d entities a misc_bsp rebuilds.\n", sub_bsp_skipped));
	}

	if (over_long > 0 || not_encodable > 0)
	{
		trap->SendServerCommand( ent->s.number, va("print \"Entities saved in %s file. ^3%d entity(s) will not load back and %d could not be written; see the server log.\n%s\"",
			arg1, over_long, not_encodable, skipped_note) );
	}
	else
	{
		trap->SendServerCommand( ent->s.number, va("print \"Entities saved in %s file\n%s\"", arg1, skipped_note) );
	}
}

/*
==================
RP_EntLoadKeepsEntity / RP_EntLoadPickKept
==================
*/
static qboolean RP_EntLoadKeepsEntity( gentity_t *target )
{
	int i;

	if ( RP_EntityIsCodeMadeSpawnPoint( target ) )
		return qtrue;

	if ( RP_EntityHasSpawnKeys( target ) )
		return qfalse;

	if ( target->r.ownerNum >= 0 && target->r.ownerNum < MAX_CLIENTS
		&& g_entities[target->r.ownerNum].inuse && g_entities[target->r.ownerNum].client )
		return qtrue;

	if ( target->parent && target->parent >= g_entities && target->parent < g_entities + MAX_CLIENTS
		&& target->parent->inuse && target->parent->client )
		return qtrue;

	for ( i = 0; i < level.maxclients; i++ )
	{
		gclient_t *cl = &level.clients[i];

		if ( cl->pers.connected == CON_DISCONNECTED )
			continue;
		if ( cl->ps.saberEntityNum == target->s.number || cl->saberStoredIndex == target->s.number )
			return qtrue;
	}

	return qfalse;
}

static qboolean rp_entload_kept[MAX_ENTITIESTOTAL];

// zyk: fills kept[] for every slot from MAX_CLIENTS + BODY_QUEUE_SIZE up: the entities kept directly,
// then anything without a record whose owner or parent is kept, until a pass adds nothing
static void RP_EntLoadPickKept( qboolean *kept )
{
	gentity_t *target_ent;
	qboolean added = qtrue;
	int passes = 0;

	memset( kept, 0, sizeof( qboolean ) * MAX_ENTITIESTOTAL );

	RP_FOR_EACH_ENTITY( target_ent )
	{
		int i = target_ent - g_entities;

		if ( i < (MAX_CLIENTS + BODY_QUEUE_SIZE) || !target_ent->inuse )
			continue;

		kept[i] = RP_EntLoadKeepsEntity( target_ent );
	}

	// zyk: each pass can only add; a chain is at most as long as there are entities, and in practice
	// three links (player, vehicle, droid, goal), so the bound is only a guard
	while ( added && passes < 16 )
	{
		added = qfalse;
		passes++;

		RP_FOR_EACH_ENTITY( target_ent )
		{
			int i = target_ent - g_entities;
			int owner = target_ent->r.ownerNum;
			int parent = target_ent->parent ? (int)( target_ent->parent - g_entities ) : -1;

			if ( i < (MAX_CLIENTS + BODY_QUEUE_SIZE) || !target_ent->inuse || kept[i] )
				continue;

			if ( RP_EntityHasSpawnKeys( target_ent ) )
				continue;

			if ( ( owner >= (MAX_CLIENTS + BODY_QUEUE_SIZE) && owner < MAX_GENTITIES && owner != ENTITYNUM_WORLD
					&& owner != ENTITYNUM_NONE && kept[owner] && g_entities[owner].inuse ) ||
				( parent >= (MAX_CLIENTS + BODY_QUEUE_SIZE) && parent < MAX_ENTITIESTOTAL && kept[parent]
					&& g_entities[parent].inuse ) )
			{
				kept[i] = qtrue;
				added = qtrue;
			}
		}
	}
}

/*
==================
Cmd_EntLoad_f
==================
*/
void Cmd_EntLoad_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	int i = 0;
	FILE *this_file = NULL;
	gentity_t *target_ent = NULL;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	// DAJ_RP: [Entity System] not while this admin holds an entity with /entcopy or /entcut (g_entgrab.c)
	if (RP_EntRefuseWhileHolding(ent))
	{
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify a file name.\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid file name. Only letters and numbers allowed.\n\"" );
		return;
	}

	if (zyk_preset_name_too_long(ent, arg1) == qtrue)
	{
		return;
	}

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("entities/%s", zyk_mapname));

	Q_strncpyz(level.load_entities_file, va("GalaxyRP/entities/%s/%s.txt",zyk_mapname,arg1), sizeof(level.load_entities_file));

	this_file = fopen(level.load_entities_file,"r");
	if (this_file)
	{ // zyk: loads entities from the file if it exists
		fclose(this_file);

		// zyk: cleaning entities. Only the ones from the file will be in the map
		RP_EntLoadPickKept( rp_entload_kept );

		RP_FOR_EACH_ENTITY( target_ent )
		{
			i = target_ent - g_entities;
			if (i < (MAX_CLIENTS + BODY_QUEUE_SIZE))
				continue;

			if (!target_ent->inuse)
				continue;

			// GalaxyRP fix: [Entity System] but leave what the players are holding.
			if (rp_entload_kept[i])
				continue;

			G_FreeEntity( target_ent );
		}

		level.load_entities_timer = level.time + 1050;

		trap->SendServerCommand( ent-g_entities, va("print \"Loading entities from %s file\n\"", arg1) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"File %s does not exist\n\"", arg1) );
	}
}

/*
==================
Cmd_EntDeleteFile_f
==================
*/
void Cmd_EntDeleteFile_f( gentity_t *ent ) {
	int number_of_args = trap->Argc();
	char arg1[MAX_STRING_CHARS];
	char serverinfo[MAX_INFO_STRING] = {0};
	char zyk_mapname[128] = {0};
	FILE *this_file = NULL;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( number_of_args < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify a file name.\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (zyk_check_user_input(arg1, strlen(arg1)) == qfalse)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid file name. Only letters and numbers allowed.\n\"" );
		return;
	}

	if (zyk_preset_name_too_long(ent, arg1) == qtrue)
	{
		return;
	}

	// zyk: getting mapname
	trap->GetServerinfo( serverinfo, sizeof( serverinfo ) );
	Q_strncpyz(zyk_mapname, Info_ValueForKey( serverinfo, "mapname" ), sizeof(zyk_mapname));

	zyk_create_dir(va("entities/%s", zyk_mapname));

	this_file = fopen(va("GalaxyRP/entities/%s/%s.txt",zyk_mapname,arg1),"r");
	if (this_file)
	{
		fclose(this_file);

		remove(va("GalaxyRP/entities/%s/%s.txt",zyk_mapname,arg1));

		trap->SendServerCommand( ent-g_entities, va("print \"File %s deleted from server\n\"", arg1) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"File %s does not exist\n\"", arg1) );
	}
}

static void zyk_entnear_append(char *message, int message_size, gentity_t *this_ent)
{
	char row[RP_LIST_FLUSH_AT];

	Com_sprintf(row, sizeof(row), "\n%d%s%s%s%s - %s", this_ent->s.number,
		this_ent->isLogical ? "L" : "",
		RP_EntityHasSpawnKeys(this_ent) ? "" : "G",
		RP_MapEntityProtected(this_ent) ? "M" : RP_MapEntityExempt(this_ent) ? "E" : "",
		this_ent->rpHeldBy ? "H" : "",
		this_ent->classname ? RP_ShownText(this_ent->classname) : "<none>");

	Q_strcat(message, message_size, row);
}

/*
==================
Cmd_EntNear_f
==================
*/
void Cmd_EntNear_f( gentity_t *ent ) {
	int i = 0;
	int distance = 200;
	int numListedEntities = 0;
	int entityList[MAX_GENTITIES];
	vec3_t mins, maxs, center;
	char message[MAX_STRING_CHARS * 2];
	char arg1[MAX_STRING_CHARS];
	gentity_t *this_ent = NULL;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	strcpy(message,"");

	if (trap->Argc() > 1)
	{
		trap->Argv(1, arg1, sizeof(arg1));

		distance = atoi(arg1);
	}

	VectorCopy(ent->client->ps.origin, center);

	for (i = 0; i < 3; i++)
	{
		mins[i] = center[i] - distance;
		maxs[i] = center[i] + distance;
	}

	numListedEntities = trap->EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

	for (i = 0; i < numListedEntities; i++)
	{
		this_ent = &g_entities[entityList[i]];

		if (this_ent && ent != this_ent && this_ent->s.number >= (MAX_CLIENTS + BODY_QUEUE_SIZE) && this_ent->inuse == qtrue)
		{
			zyk_entnear_append(message, sizeof(message), this_ent);
		}

		if ((int)strlen(message) > RP_LIST_FLUSH_AT)
		{
			trap->SendServerCommand(ent->s.number, "print \"Too much info. Decrease the distance argument\n\"");
			return;
		}
	}

	RP_FOR_EACH_ENTITY( this_ent )
	{
		int j = 0;
		qboolean already_found = qfalse;

		i = this_ent - g_entities;
		if (i < (MAX_CLIENTS + BODY_QUEUE_SIZE))
			continue;

		for (j = 0; j < numListedEntities; j++)
		{
			if (entityList[j] == i)
			{ // zyk: this entity was already listed
				already_found = qtrue;
				break;
			}
		}

		if (this_ent && ent != this_ent && already_found == qfalse && this_ent->inuse == qtrue && (int)Distance(ent->client->ps.origin, this_ent->r.currentOrigin) < distance && this_ent->s.eType != ET_MOVER)
		{ // zyk: do not list mover entities in this old method, they are listed with EntitiesInBox
			zyk_entnear_append(message, sizeof(message), this_ent);
		}

		if ((int)strlen(message) > RP_LIST_FLUSH_AT)
		{
			trap->SendServerCommand(ent->s.number, "print \"Too much info. Decrease the distance argument\n\"");
			return;
		}
	}

	if (Q_stricmp(message, "") == 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"No entities found\n\"");
		return;
	}

	trap->SendServerCommand( ent->s.number, va("print \"%s\n\"", message) );
}

static void zyk_entlist_append(gentity_t *ent, char *message, int message_size, int *len, int id, gentity_t *target_ent)
{
	char row[RP_LIST_FLUSH_AT];
	char lock[64];
	const char *lockName = RP_LockNameOf(target_ent);

	// DAJ_RP: [Locks] the lock it carries, if any (g_locks.c)
	lock[0] = '\0';
	if (lockName)
		Com_sprintf(lock, sizeof(lock), " - ^3lock:%s^7", lockName);

	Com_sprintf(row, sizeof(row), "\n%d%s%s%s%s - %s - %s - %s%s", id,
		(target_ent && target_ent->isLogical) ? "L" : "",
		(target_ent && target_ent->inuse && !RP_EntityHasSpawnKeys(target_ent)) ? "G" : "",
		(target_ent && target_ent->inuse && RP_MapEntityProtected(target_ent)) ? "M" :
		(target_ent && target_ent->inuse && RP_MapEntityExempt(target_ent)) ? "E" : "",
		(target_ent && target_ent->inuse && target_ent->rpHeldBy) ? "H" : "",
		(target_ent && target_ent->classname) ? RP_ShownText(target_ent->classname) : "<none>",
		(target_ent && target_ent->targetname) ? RP_ShownText(target_ent->targetname) : "<none>",
		(target_ent && target_ent->target) ? RP_ShownText(target_ent->target) : "<none>", lock);

	if ((*len + (int)strlen(row)) > RP_LIST_FLUSH_AT)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"^7%s\n\"", message) );
		message[0] = '\0';
		*len = 0;
	}

	Q_strcat(message, message_size, row);
	*len = (int)strlen(message);
}

static void zyk_copy_translated( char *dst, int size, const char *src )
{
	int i = 0, o = 0;

	if ( !dst || size <= 0 )
		return;

	for ( i = 0; src && src[i] && o < size - 1; i++ )
	{
		if ( src[i] == '\\' && src[i + 1] == 'n' )
		{
			dst[o++] = '\n';
			i++;
		}
		else
		{
			dst[o++] = src[i];
		}
	}

	dst[o] = '\0';
}

/*
==================
Cmd_EntList_f
==================
*/
void Cmd_EntList_f( gentity_t *ent ) {
	int i = 0;
	int page_number = 0;
	gentity_t *target_ent;
	char arg1[MAX_STRING_CHARS];
	char message[1024];
	int len = 0;

	strcpy(message,"");

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	if ( trap->Argc() < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify a page number greater than 0. Example: ^3/entlist 5^7. You can also search for classname, targetname and target that matches at least part of it. Example: ^3/entlist info_player_deathmatch^7\n\"") );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	page_number = atoi(arg1);

	if (page_number > 0)
	{
		// GalaxyRP: [Logical Entities] pages run through the networked region and then straight on
		// into the logical one, so page N is the Nth ten slots in the order both regions are
		// walked; the ids printed are the real ones, with a gap where the region changes.
		int row = 0;

		RP_FOR_EACH_ENTITY( target_ent )
		{
			if (row >= ((page_number - 1) * 10) && row < (page_number * 10))
			{ // zyk: this command lists 10 entities per page
				i = target_ent - g_entities;
				zyk_entlist_append(ent, message, sizeof(message), &len, i, target_ent);
			}
			row++;
		}
	}
	else
	{ // zyk: search by classname, targetname or target
		int found_entities = 0;

		char search_term[MAX_STRING_CHARS];

		zyk_copy_translated(search_term, sizeof(search_term), arg1);

		// GalaxyRP: [Logical Entities] both regions.
		RP_FOR_EACH_ENTITY( target_ent )
		{
			i = target_ent - g_entities;

			if (target_ent && 
				((target_ent->classname && strstr(target_ent->classname, search_term)) ||
				 (target_ent->targetname && strstr(target_ent->targetname, search_term)) ||
				 (target_ent->target && strstr(target_ent->target, search_term))))
			{
				zyk_entlist_append(ent, message, sizeof(message), &len, i, target_ent);
				found_entities++;
			}

			// zyk: max entities to list
			if (found_entities == 14)
				break;
		}
	}

	trap->SendServerCommand( ent-g_entities, va("print \"^3\nID - classname - targetname - target\n^7%s\n\n\"",message) );
}

qboolean is_entity_a_pickup(gentity_t* ent);	// GalaxyRP: below, with /removepickups

static int RP_EntRemoveFree( gentity_t *target, int *subEntities )
{
	int children;

	if ( target->rpBSPInstance > 0 )
	{
		int freed = RP_FreeSubBSPEntities( target->rpBSPInstance );

		if ( subEntities )
			*subEntities += freed;
	}

	// GalaxyRP: [Entity System] its triggers, and the entities stamped as its children (a turret's top, a
	// trip mine's laser trap) -- see RP_FreeEntityChildren() in g_spawn.c
	children = RP_FreeEntityChildren( target );

	G_FreeEntity( target );
	return children;
}

/*
==================
Cmd_EntRemove_f
==================
*/
void Cmd_EntRemove_f( gentity_t *ent ) {
	int i = 0;
	int entity_id = -1;
	int entity_id2 = -1;
	gentity_t *target_ent;
	char   arg1[MAX_STRING_CHARS];
	char   arg2[MAX_STRING_CHARS];
	char   classname[MAX_QPATH];

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	// DAJ_RP: [Entity System] not while this admin holds an entity with /entcopy or /entcut (g_entgrab.c)
	if (RP_EntRefuseWhileHolding(ent))
	{
		return;
	}

	if ( trap->Argc() < 2)
	{
		gentity_t *aimed;

		if (RP_EntAimFollowing(ent))
		{
			trap->SendServerCommand( ent-g_entities, "print \"You are following another player. Stop following first, or give the entity id.\n\"" );
			return;
		}

		aimed = RP_EntAimTarget(ent);
		if (!aimed)
		{
			trap->SendServerCommand( ent-g_entities, "print \"You are not aiming at an entity. Aim at one, or give its id: ^3/entremove <entity id>^7.\n\"" );
			return;
		}

		entity_id = aimed->s.number;
	}

	if (trap->Argc() <= 2)
	{
		if (trap->Argc() == 2)
		{
			trap->Argv( 1, arg1, sizeof( arg1 ) );
			entity_id = atoi(arg1);
		}

		if (entity_id < 0)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Invalid Entity ID %d.\n\"",entity_id) );
			return;
		}

		if (entity_id >= 0 && entity_id < (MAX_CLIENTS + BODY_QUEUE_SIZE))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity ID %d is a reserved player or body slot and cannot be removed.\n\"",entity_id) );
			return;
		}

		// GalaxyRP: [Logical Entities] both regions.
		RP_FOR_EACH_ENTITY( target_ent )
		{
			i = target_ent - g_entities;

			if ((target_ent-g_entities) == entity_id && target_ent->inuse)
			{
				int children;
				int subEntities = 0;

				if (RP_EntityHasSpawnKeys(target_ent) == qfalse)
				{
					trap->SendServerCommand( ent-g_entities, va("print \"Entity %d %s.\n\"", i, RP_EntityRefusalReason(target_ent)) );
					return;
				}

				if (RP_MapEntityProtected(target_ent))
				{
					trap->SendServerCommand( ent-g_entities, va("print \"Entity %d (%s) is part of the map (marked M in /entlist) and cannot be removed%s%s.\n\"", i,
						target_ent->classname ? target_ent->classname : "noclass", RP_MapEntityRefusalNote(target_ent),
						is_entity_a_pickup(target_ent) ? " (^3/removepickups^7 removes all pickups)" : "") );
					return;
				}

				if (RP_SpawnPointProtected(target_ent))
				{
					trap->SendServerCommand( ent-g_entities, va("print \"Entity %d is a spawn point (%s) of the map or its fixes and cannot be removed.\n\"", i, target_ent->classname) );
					return;
				}

				if (target_ent->neverFree)
				{
					trap->SendServerCommand( ent-g_entities, va("print \"Entity %d is a permanent game entity and cannot be removed.\n\"", i) );
					return;
				}

				if (target_ent->rpSubBSPOf > 0 && RP_MiscBspForInstance(target_ent->rpSubBSPOf))
				{
					trap->SendServerCommand( ent-g_entities, va("print \"Entity %d belongs to misc_bsp %d: edit or remove the misc_bsp instead.\n\"",
						i, RP_MiscBspForInstance(target_ent->rpSubBSPOf)->s.number) );
					return;
				}

				// GalaxyRP: [Entity System] an admin is holding it with /entcopy or /entcut (g_entgrab.c)
				if (target_ent->rpHeldBy)
				{
					// DAJ_RP: [Entity System] by another admin: who, and with what (g_entgrab.c)
					trap->SendServerCommand( ent-g_entities, va("print \"Entity %d is %s: wait until they drop it or let go.\n\"", i, RP_EntHeldByText(target_ent)) );
					return;
				}

				// GalaxyRP: [Entity System] its class, for the message: freeing it clears the entity
				Q_strncpyz(classname, target_ent->classname ? target_ent->classname : "noclass", sizeof(classname));

				children = RP_EntRemoveFree( target_ent, &subEntities );
				if (target_ent->inuse)
				{ // zyk: G_FreeEntity() can still keep an entity alive (a Jedi Master saber, for one)
					trap->SendServerCommand( ent-g_entities, va("print \"Entity %d cannot be removed.\n\"", i) );
					return;
				}
				trap->SendServerCommand( ent-g_entities, va("print \"Entity %d (%s) removed%s%s.\n\"", i, classname,
					children == 1 ? " (and the entity it made for itself)" : children > 1 ? va(" (and %d entities it made for itself)", children) : "",
					subEntities > 0 ? va(" (and the %d entities of its sub-BSP)", subEntities) : "") );
				return;
			}
		}

		trap->SendServerCommand( ent-g_entities, va("print \"Entity %d not found.\n\"",entity_id) );
	}
	else
	{
		trap->Argv( 1, arg1, sizeof( arg1 ) );
		entity_id = atoi(arg1);

		if (entity_id < 0)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Invalid Entity 1 ID %d.\n\"",entity_id) );
			return;
		}

		if (entity_id >= 0 && entity_id < (MAX_CLIENTS + BODY_QUEUE_SIZE))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity 1 ID %d is a reserved player or body slot and cannot be removed.\n\"",entity_id) );
			return;
		}

		trap->Argv( 2, arg2, sizeof( arg2 ) );
		entity_id2 = atoi(arg2);

		if (entity_id2 < 0)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Invalid Entity 2 ID %d.\n\"",entity_id2) );
			return;
		}

		if (entity_id2 >= 0 && entity_id2 < (MAX_CLIENTS + BODY_QUEUE_SIZE))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity 2 ID %d is a reserved player or body slot and cannot be removed.\n\"",entity_id2) );
			return;
		}

		if (entity_id2 < entity_id)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Entity 2 ID %d must not be lower than Entity 1 ID %d.\n\"",entity_id2,entity_id) );
			return;
		}

		if (entity_id < MAX_GENTITIES && entity_id2 >= MAX_GENTITIES)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"A range cannot cross from the networked entities (below %d) into the logical ones (from %d): remove the two ranges separately.\n\"", MAX_GENTITIES, MAX_GENTITIES) );
			return;
		}

		{
			static qboolean rp_entremove_pick[MAX_ENTITIESTOTAL];
			int removed = 0;
			int children = 0;
			int skipped = 0;
			int spawnPoints = 0;
			int permanent = 0;
			int subEntities = 0;
			int subKept = 0;
			int held = 0;
			int mapKept = 0;

			// GalaxyRP: [Logical Entities] both regions.
			RP_FOR_EACH_ENTITY( target_ent )
			{
				i = target_ent - g_entities;
				// GalaxyRP fix: [Entity System] same guard as the single-id branch above. A range of
				// a few hundred slots is mostly free slots, and each one was being freed again.
				rp_entremove_pick[i] = (i >= entity_id && i <= entity_id2 && target_ent->inuse
					&& RP_EntityHasSpawnKeys(target_ent) && !RP_MapEntityProtected(target_ent) && !RP_SpawnPointProtected(target_ent)
					&& !target_ent->neverFree && !target_ent->rpHeldBy
					&& !(target_ent->rpSubBSPOf > 0 && RP_MiscBspForInstance(target_ent->rpSubBSPOf))) ? qtrue : qfalse;
			}

			RP_FOR_EACH_ENTITY( target_ent )
			{
				i = target_ent - g_entities;
				if (rp_entremove_pick[i] && target_ent->inuse)
				{
					children += RP_EntRemoveFree( target_ent, &subEntities );
					if (!target_ent->inuse)
						removed++;
				}
				rp_entremove_pick[i] = qfalse;
			}

			RP_FOR_EACH_ENTITY( target_ent )
			{
				i = target_ent - g_entities;
				if (i >= entity_id && i <= entity_id2 && target_ent->inuse)
				{
					// GalaxyRP: [Entity System] the map's own entities (M) first: a map spawn point or a
					// held map entity is counted once, as part of the map
					if (RP_MapEntityProtected(target_ent) && RP_EntityHasSpawnKeys(target_ent))
						mapKept++;
					else if (target_ent->rpHeldBy && RP_EntityHasSpawnKeys(target_ent))
						held++;
					else if (RP_SpawnPointProtected(target_ent))
						spawnPoints++;
					else if (target_ent->neverFree && RP_EntityHasSpawnKeys(target_ent))
						permanent++;
					else if (target_ent->rpSubBSPOf > 0 && RP_MiscBspForInstance(target_ent->rpSubBSPOf))
						subKept++;
					else
						skipped++;
				}
			}

			{
				char childNote[64] = "";
				char skipNote[128] = "";
				char spawnNote[128] = "";
				char permanentNote[96] = "";
				char subNote[224] = "";
				char mapNote[96] = "";

				if (children > 0)
					Com_sprintf(childNote, sizeof(childNote), " (and %d entit%s they made for themselves)", children, children == 1 ? "y" : "ies");
				if (skipped > 0)
					Com_sprintf(skipNote, sizeof(skipNote), " Skipped %d that cannot be removed (created by the game: marked G in /entlist).", skipped);
				if (spawnPoints > 0)
					Com_sprintf(spawnNote, sizeof(spawnNote), " Kept %d spawn point%s (the map's and its fixes' spawn points cannot be removed).", spawnPoints, spawnPoints == 1 ? "" : "s");
				if (permanent > 0)
					Com_sprintf(permanentNote, sizeof(permanentNote), " Kept %d permanent entit%s.", permanent, permanent == 1 ? "y" : "ies");
				if (subEntities > 0)
					Q_strcat(subNote, sizeof(subNote), va(" %d sub-BSP entit%s went with their misc_bsp.", subEntities, subEntities == 1 ? "y" : "ies"));
				if (subKept > 0)
					Q_strcat(subNote, sizeof(subNote), va(" Kept %d that belong to a misc_bsp outside the range.", subKept));
				if (held > 0)
					Q_strcat(subNote, sizeof(subNote), va(" Kept %d held with /entcopy or /entcut.", held));
				if (mapKept > 0)
					Com_sprintf(mapNote, sizeof(mapNote), " Kept %d that %s part of the map (marked M in /entlist).", mapKept, mapKept == 1 ? "is" : "are");

				trap->SendServerCommand( ent-g_entities, va("print \"Removed %d entit%s%s.%s%s%s%s%s\n\"",
					removed, removed == 1 ? "y" : "ies", childNote, mapNote, skipNote, spawnNote, permanentNote, subNote) );
			}
		}
		return;
	}
}

#define RP_PLATFORM_HEIGHT_DEFAULT	0
#define RP_PLATFORM_HEIGHT_MAX		4096
#define RP_PLATFORM_HALF_THICKNESS	8	// its mins/maxs below: the top surface is this far above its origin

void Cmd_SpawnPlatform_f(gentity_t* ent) 
{
	gentity_t* new_ent = NULL;
	int height = RP_PLATFORM_HEIGHT_DEFAULT;
	int asked;
	char arg[MAX_STRING_CHARS];
	char ceilingNote[96] = "";
	vec3_t feet;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	// DAJ_RP: [Entity System] not while this admin holds an entity with /entcopy or /entcut (g_entgrab.c)
	if (RP_EntRefuseWhileHolding(ent))
	{
		return;
	}

	if (trap->Argc() > 1)
	{
		int i;

		trap->Argv(1, arg, sizeof(arg));
		for (i = 0; arg[i] && i < 6; i++)
		{
			if (arg[i] < '0' || arg[i] > '9')
				break;
		}
		if (!arg[0] || arg[i] || atoi(arg) > RP_PLATFORM_HEIGHT_MAX)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Usage: /spawnplatform <height (optional)>: how far it rises when someone steps onto it, 0 to %d units (default %d: it stays put).\n\"",
				RP_PLATFORM_HEIGHT_MAX, RP_PLATFORM_HEIGHT_DEFAULT) );
			return;
		}
		height = atoi(arg);
	}
	asked = height;

	if ( G_EntitySlotsAvailable( 2 ) == qfalse )
	{
		trap->SendServerCommand( ent-g_entities,
			va("print \"Cannot spawn a platform: the server is near its entity limit. %d slots free, %d held in reserve.\n\"",
				G_FreeEntityCount(), ZYK_ENTITY_RESERVE) );
		G_LogPrintf( "/spawnplatform by %s refused: %d entity slots free\n",
			ent->client->pers.netname, G_FreeEntityCount() );
		return;
	}

	// where the admin's feet are: the bottom of the player box
	VectorCopy( ent->client->ps.origin, feet );
	feet[2] += DEFAULT_MINS_2;

	// the room above for a rider standing where the admin stands -- a player's box carried up height
	// units; nothing to measure when the admin is already inside something
	if (height > 0)
	{
		const vec3_t riderMins = { -15, -15, DEFAULT_MINS_2 };
		const vec3_t riderMaxs = { 15, 15, DEFAULT_MAXS_2 };
		vec3_t top;
		trace_t tr;

		VectorCopy( ent->client->ps.origin, top );
		top[2] += height;
		trap->Trace( &tr, ent->client->ps.origin, riderMins, riderMaxs, top, ent->s.number, MASK_PLAYERSOLID & ~CONTENTS_BODY, qfalse, 0, 0 );
		if (!tr.startsolid && !tr.allsolid && tr.fraction < 1.0f)
		{
			height = (int)floor( tr.endpos[2] - ent->client->ps.origin[2] );
			if (height < 0)
				height = 0;
			Com_sprintf( ceilingNote, sizeof(ceilingNote), " (lowered from %d to fit under what is above)", asked );
		}
	}

	new_ent = G_Spawn();

	if (new_ent)
	{
		zyk_main_set_entity_field(new_ent, "classname", "func_plat");
		zyk_main_set_entity_field(new_ent, "origin", va("%f %f %f", feet[0], feet[1], feet[2] + height - RP_PLATFORM_HALF_THICKNESS));
		zyk_main_set_entity_field(new_ent, "angles", "0 0 0");
		zyk_main_set_entity_field(new_ent, "spawnflags", "1024");
		zyk_main_set_entity_field(new_ent, "mins", "-64 -64 -8");
		zyk_main_set_entity_field(new_ent, "maxs", "64 64 8");
		zyk_main_set_entity_field(new_ent, "height", va("%d", height));
		zyk_main_set_entity_field(new_ent, "model", "models/map_objects/factory/catw2_b.md3");

		zyk_main_spawn_entity(new_ent);

		// GalaxyRP: [Entity System] its model file must be on the server (RP_EntitySystemSpawnRefused)
		if (!new_ent->inuse)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"The platform was refused%s%s.\n\"",
				level.rp_spawn_refusal[0] ? ": " : "", level.rp_spawn_refusal) );
			return;
		}

		if (new_ent->s.number != 0)
		{
			level.last_spawned_entity = new_ent;
		}

		if (height > 0)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Platform %d spawned at (%i %i %i): a lift rising %d units when someone steps onto it%s. ^3/entedit %d height <units>^7 changes it.\n\"",
				new_ent->s.number, (int)feet[0], (int)feet[1], (int)feet[2], height, ceilingNote, new_ent->s.number) );
		}
		else
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Platform %d spawned at (%i %i %i): it stays where it is%s. ^3/entedit %d height <units>^7 makes it a lift.\n\"",
				new_ent->s.number, (int)feet[0], (int)feet[1], (int)feet[2], ceilingNote, new_ent->s.number) );
		}
	}

}

#define RP_DUMMY_DISTANCE	64	// from the admin's origin to the dummy's: 34 units between the two boxes
#define RP_DUMMY_STEP		18	// a step up this high in front is climbed, not taken for a wall
#define RP_DUMMY_DROP		64	// how far below the spot a floor may be

void Cmd_SpawnDummy_f(gentity_t* ent)
{
	gentity_t* new_ent = NULL;
	const vec3_t dummyMins = { -15, -15, DEFAULT_MINS_2 };	// SP_ZykTrainingPole()'s default box
	const vec3_t dummyMaxs = { 15, 15, DEFAULT_MAXS_2 };
	vec3_t spot, start, end, forward;
	float yaw, facing;
	qboolean inFront = qfalse;
	trace_t tr;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	// DAJ_RP: [Entity System] not while this admin holds an entity with /entcopy or /entcut (g_entgrab.c)
	if (RP_EntRefuseWhileHolding(ent))
	{
		return;
	}

	// GalaxyRP fix: [Entity System] ask before allocating, as /entadd and /spawnplatform do. One
	// slot here: SP_ZykTrainingPole builds nothing of its own.
	if ( G_EntitySlotsAvailable( 1 ) == qfalse )
	{
		trap->SendServerCommand( ent-g_entities,
			va("print \"Cannot spawn a dummy: the server is near its entity limit. %d slots free, %d held in reserve.\n\"",
				G_FreeEntityCount(), ZYK_ENTITY_RESERVE) );
		G_LogPrintf( "/spawndummy by %s refused: %d entity slots free\n",
			ent->client->pers.netname, G_FreeEntityCount() );
		return;
	}

	// where it goes: in front of the admin, on the floor, if its box can get there
	yaw = ent->client->ps.viewangles[YAW];
	VectorSet( forward, cos( DEG2RAD( yaw ) ), sin( DEG2RAD( yaw ) ), 0 );
	VectorCopy( ent->client->ps.origin, spot );

	VectorCopy( ent->client->ps.origin, start );
	start[2] += RP_DUMMY_STEP;
	VectorMA( start, RP_DUMMY_DISTANCE, forward, end );
	trap->Trace( &tr, start, dummyMins, dummyMaxs, end, ent->s.number, MASK_PLAYERSOLID, qfalse, 0, 0 );
	if ( !tr.startsolid && !tr.allsolid && tr.fraction >= 1.0f )
	{
		VectorCopy( end, start );
		end[2] -= RP_DUMMY_STEP + RP_DUMMY_DROP;
		trap->Trace( &tr, start, dummyMins, dummyMaxs, end, ent->s.number, MASK_PLAYERSOLID, qfalse, 0, 0 );
		if ( !tr.startsolid && !tr.allsolid && tr.fraction < 1.0f )
		{
			VectorCopy( tr.endpos, spot );
			inFront = qtrue;
		}
	}

	// facing the admin
	facing = AngleNormalize360( yaw + 180.0f );

	new_ent = G_Spawn();

	if (new_ent)
	{
		zyk_main_set_entity_field(new_ent, "classname", "zyk_training_pole");
		// GalaxyRP fix: [Entity System] va() strings: zyk_main_set_entity_field() copies them into the record,
		// and the G_NewString() copy of the origin made here was never used and stayed in the level's memory pool
		zyk_main_set_entity_field(new_ent, "origin", va("%f %f %f", spot[0], spot[1], spot[2]));
		zyk_main_set_entity_field(new_ent, "angles", va("0 %f 0", facing));
		zyk_main_set_entity_field(new_ent, "spawnflags", "1");

		zyk_main_spawn_entity(new_ent);

		// GalaxyRP: [Entity System] refused -- its model must be on the server and have a slot
		// (RP_EntitySystemSpawnRefused) -- or gone in its spawn: say so, as /spawnplatform does
		if (!new_ent->inuse)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"The dummy was refused%s%s.\n\"",
				level.rp_spawn_refusal[0] ? ": " : "", level.rp_spawn_refusal) );
			return;
		}

		if (new_ent->s.number != 0)
		{
			level.last_spawned_entity = new_ent;
		}

		if (inFront)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Dummy %d spawned at (%i %i %i), in front of you.\n\"",
				new_ent->s.number, (int)spot[0], (int)spot[1], (int)spot[2]) );
		}
		else
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Dummy %d spawned where you stand (%i %i %i): there was no room in front of you. Step out of it.\n\"",
				new_ent->s.number, (int)spot[0], (int)spot[1], (int)spot[2]) );
		}
	}
}

static qboolean zyk_classname_has_prefix(const char *classname, const char *prefix) {
	return (Q_strncmp(classname, prefix, strlen(prefix)) == 0) ? qtrue : qfalse;
}

qboolean is_entity_a_pickup(gentity_t* ent) {
	if (!ent || !ent->inuse || !ent->classname) {
		return qfalse;
	}

	// GalaxyRP (Alex): [Entity System] Remove all weapon pickups
	if (zyk_classname_has_prefix(ent->classname, "weapon_") == qtrue) {
		return qtrue;
	}

	// GalaxyRP (Alex): [Entity System] Remove all ammo pickups
	if (zyk_classname_has_prefix(ent->classname, "ammo_") == qtrue) {
		return qtrue;
	}

	// GalaxyRP (Alex): [Entity System] Remove all item pickups
	if (zyk_classname_has_prefix(ent->classname, "item_") == qtrue) {
		return qtrue;
	}

	return qfalse;
}

void Cmd_RemovePickups_f(gentity_t* ent) {

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	// DAJ_RP: [Entity System] not while this admin holds an entity with /entcopy or /entcut (g_entgrab.c)
	if (RP_EntRefuseWhileHolding(ent))
	{
		return;
	}

	gentity_t* target_ent;
	int removed = 0;
	int held = 0;

	for (int i = (MAX_CLIENTS + BODY_QUEUE_SIZE); i < level.num_entities; i++)
	{
		target_ent = &g_entities[i];

		if (is_entity_a_pickup(target_ent) == qtrue) {
			// DAJ_RP: [Entity System] one another admin is holding with /entcopy or /entcut (g_entgrab.c)
			// is kept, as a ranged /entremove keeps it: freeing it took a cut pickup away for good
			if (target_ent->rpHeldBy) {
				held++;
				continue;
			}

			G_FreeEntity(target_ent);
			removed++;
		}
	}
	trap->SendServerCommand(ent - g_entities, va("print \"All pickups have been removed (%d).%s\n\"", removed,
		held ? va(" Kept %d held with /entcopy or /entcut.", held) : ""));
	
	return;
}

/*
==================
Cmd_ClientPrint_f
==================
*/
void Cmd_ClientPrint_f( gentity_t *ent ) {
	int client_id = -1;
	int len = 0;
	char arg1[MAX_STRING_CHARS];
	char message[MAX_STRING_CHARS];

	if (!check_admin_command(ent, ADM_CLIENTPRINT, qtrue))
	{
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	message[0] = '\0';
	if ( trap->Argc() > 2 )
	{
		Q_strncpyz( message, ConcatArgs( 2 ), sizeof( message ) );
	}

	len = zyk_clean_text( message );

	if ( len == 0 )
	{
		trap->SendServerCommand( ent-g_entities, "print \"Usage: " RP_CLIENTPRINT_HELP "\n\"" );
		return;
	}

	if ( len > RP_CLIENTPRINT_MAX )
	{
		trap->SendServerCommand( ent-g_entities, va( "print \"Your message is %d characters long; at most %d can be shown.\n\"", len, RP_CLIENTPRINT_MAX ) );
		return;
	}

	if ( !arg1[0] )
	{
		trap->SendServerCommand( ent-g_entities, "print \"Give a player name or ID, or -1 for everyone.\n\"" );
		return;
	}

	if ( strcmp( arg1, "-1" ) != 0 )
	{
		client_id = ClientNumberFromString( ent, arg1, qfalse );

		if ( client_id == -1 )
		{
			return;
		}
	}

	// Players see only the message, as before -- who sent it goes to the server log.
	trap->SendServerCommand( client_id, va( "cp \"%s\"", message ) );
	trap->SendServerCommand( client_id, va( "print \"^3* %s ^3*\n\"", message ) );

	if ( client_id == -1 )
	{
		G_LogPrintf( "ClientPrint: %s^7 -> all players: %s\n", ent->client->pers.netname, message );
	}
	else
	{
		G_LogPrintf( "ClientPrint: %s^7 -> %s^7: %s\n", ent->client->pers.netname, g_entities[client_id].client->pers.netname, message );

		// the sender sees nothing of a message to one other player, so say it went
		if ( client_id != ent->s.number )
		{
			trap->SendServerCommand( ent-g_entities, va( "print \"Shown to %s^7.\n\"", g_entities[client_id].client->pers.netname ) );
		}
	}
}

/*
==================
Cmd_Silence_f
==================
*/
void Cmd_Silence_f( gentity_t *ent ) {
	int client_id = -1;
	char   arg[MAX_STRING_CHARS];

	if (!check_admin_command(ent, ADM_SILENCE, qtrue))
	{
		return;
	}

	if ( trap->Argc() < 2)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You must specify a player name or ID.\n\"") );
		return;
	}

	trap->Argv( 1, arg, sizeof( arg ) );
	client_id = ClientNumberFromString( ent, arg, qfalse );

	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->pers.player_statuses & (1 << PLAYER_STATUS_SILENCED))
	{
		g_entities[client_id].client->pers.player_statuses &= ~(1 << PLAYER_STATUS_SILENCED);
		trap->SendServerCommand( -1, va("chat \"^3Admin System: ^7player %s^7 is no longer silenced!\n\"", g_entities[client_id].client->pers.netname) );
	}
	else
	{
		g_entities[client_id].client->pers.player_statuses |= (1 << PLAYER_STATUS_SILENCED);
		trap->SendServerCommand( -1, va("chat \"^3Admin System: ^7player %s^7 is silenced!\n\"", g_entities[client_id].client->pers.netname) );
	}
}

void zyk_show_admin_commands(gentity_t *whose_commands, gentity_t *shown_to)
{
	char message[1024];
	// GalaxyRP fix: [cleanup] removed an unused 'char message_content[ADM_NUM_CMDS + 1][80]' local
	// (never read anywhere in this function).
	strcpy(message,"");

	// GalaxyRP fix: [cleanup] removed a dead outer 'int i = 0;' that was immediately shadowed by
	// this loop's own 'int i' -- the outer one was never read.
	for (int i = 0; i < ADM_NUM_CMDS; i++) {
		if ((whose_commands->client->pers.bitvalue & (1 << admin_commands[i].number)))
		{
			trap->SendServerCommand(shown_to - g_entities, va("print \"^3%d ^7- %s: ^2yes\n\"", admin_commands[i].number, admin_commands[i].title));
		}
		else
		{
			trap->SendServerCommand(shown_to - g_entities, va("print \"^3%d ^7- %s: ^1no\n\"", admin_commands[i].number, admin_commands[i].title));
		}
	}

	return;
}

/*
==================
Cmd_AdminList_f
==================
*/
void Cmd_AdminList_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];

	if (trap->Argc() == 1)
	{
		zyk_show_admin_commands(ent, ent);
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (Q_stricmp(arg1, "show") == 0)
	{
		gentity_t *player_ent = NULL;
		char arg2[MAX_STRING_CHARS];
		int client_id = -1;

		if (!check_admin_command(ent, ADM_GIVEADM, qtrue))
		{
			return;
		}

		if (trap->Argc() < 3)
		{
			trap->SendServerCommand( ent-g_entities, "print \"You must specify a player name or ID. Usage: /adminlist show <player name or ID>\n\"" );
			return;
		}

		trap->Argv( 2, arg2, sizeof( arg2 ) );

		client_id = ClientNumberFromString( ent, arg2, qfalse );
		if (client_id == -1)
		{
			return;
		}

		player_ent = &g_entities[client_id];

		if (player_ent->client->sess.amrpgmode == 0)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Player %s ^7is not logged in.\n\"", player_ent->client->pers.netname) );
			return;
		}

		// zyk: player is logged in. Show his admin commands
		zyk_show_admin_commands(player_ent, ent);
		return;
	}

	// zyk: display help info for an admin command
	if (!StringIsInteger(arg1))
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid admin command number. Use ^3/adminlist ^7to see your admin commands and their numbers.\n\"" );
		return;
	}

	{
		int command_number = atoi(arg1);

		if (command_number == ADM_NPC)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n^3/npc spawn <type> <targetname (optional)>^7: spawns an npc. ^3/npc spawn vehicle <type> <targetname (optional)>^7: spawns a vehicle.\n\
^3/npc kill <targetname or type>^7: kills npcs with that targetname or type. ^3/npc kill all^7: kills every npc.\n\
^3/npc kill team <player/enemy/neutral/free or nonally>^7: kills a whole team, or ^3nonally ^7for every npc but your allies.\n\
^3/npc killaim^7: kills the npc or vehicle in your crosshair (not one with a player aboard).\n\
^3/npc team <player/enemy/neutral/free>^7: sets the team of the npc in your crosshair.\n\
^3/npc effect <holo/ghost/walkthrough/clear>^7: gives the npc in your crosshair a hologram or Force ghost look (both walk-through) or makes it walk-through only; ^3clear ^7restores it, and makes an npc that spawned non-solid solid.\n\
^3/npc freeze^7: freezes the npc in your crosshair in place (no moving, attacking or defending; it can still be hurt), or unfreezes it.\n\n\"" );
		}
		else if (command_number == ADM_NOCLIP)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/noclip ^7to toggle noclip\n\n\"" );
		}
		else if (command_number == ADM_GIVEADM)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nThis flag allows admins to give or remove admin commands from a player with ^3/adminup <name> <command number> ^7and ^3/admindown <name> <command number>^7. Use ^3/adminlist show <player name or ID> ^7to see admin commands of a player\n\n\"" );
		}
		else if (command_number == ADM_TELE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nThis command can be ^3/teleport^7 or ^3/tele^7. Use ^3/telemark ^7to mark a spot in map, then use ^3/teleport ^7to go there. Use ^3/teleport <player name or ID> ^7to teleport to a player. \nUse ^3/teleport <player name or ID> <player name or ID> ^7to teleport a player to another. Use ^3/teleport <x> <y> <z> ^7to teleport to coordinates. Use ^3/teleport <player name or ID> <x> <y> <z> ^7to teleport a player to coordinates. Use ^3/teleportaim ^7or ^3/teleaim <player name or ID (optional)> ^7to teleport yourself, or the player, to where you aim. A player riding a vehicle is refused.\n\n\"" );
		}
		else if (command_number == ADM_ADMPROTECT)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nWith this flag, a player can use the Admin Protect option (^3/settings 2^7) to self-protect from gameplay-related admin commands\n\n\"" );
		}
		else if (command_number == ADM_ENTITYSYSTEM)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/entitiesandremaps ^7to see the entity and shader remap commands this flag enables, including the account lock commands. It also lets a player turn on Entity Bounds (^3/settings 6^7) and see the name of a lock that stops them.\n\n\"" );
		}
		else if (command_number == ADM_SILENCE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/silence <player name or ID> ^7to silence that player\n\n\"" );
		}
		else if (command_number == ADM_CLIENTPRINT)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\n" RP_CLIENTPRINT_HELP "\n\n\"" );
		}
		else if (command_number == ADM_SHAKESCREEN)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"\nUse ^3/shakescreen <distance (%d-%d)> <intensity (%d-%d)> <length in seconds (%d-%d)> ^7to shake the screen of everyone within that distance of you. Example: ^3/shakescreen 600 4 2\n\n\"",
				RP_SHAKE_MIN_DISTANCE, RP_SHAKE_MAX_DISTANCE, RP_SHAKE_MIN_INTENSITY, RP_SHAKE_MAX_INTENSITY,
				RP_SHAKE_MIN_SECONDS, RP_SHAKE_MAX_SECONDS) );
		}
		else if (command_number == ADM_KICK)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/admkick <player name or ID> ^7to kick a player from the server, or ^3/killother <player name or ID> ^7to instantly kill a player.\n\n\"" );
		}
		else if (command_number == ADM_PARALYZE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/paralyze <player name or ID> <seconds (optional, 30-900, default 30)> ^7to paralyze a player for that long, or ^3/unparalyze <player name or ID> ^7to release them early.\n\n\"" );
		}
		else if (command_number == ADM_GIVE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/give <player name or ID> <option> ^7to give guns or Force powers to a player who is not logged in. Option may be ^3guns ^7or ^3force ^7\n\n\"" );
		}
		else if (command_number == ADM_SCALE)
		{
			trap->SendServerCommand( ent-g_entities, "print \"\nThis flag enables scaling other players. Use ^3/scale <player name or ID> <size between 20 and 500> ^7to change another player's model size.\n\n\"" );
		}
		else if (command_number == ADM_PLAYERS)
		{
			// DAJ_RP fix: [Admin] the categories said "other", which was renamed "protect" (see zyk_list_player_skills())
			trap->SendServerCommand( ent-g_entities, "print \"\nUse ^3/players ^7to list the connected players: ID, name, IP, and whether they are logged in or an admin. Use ^3/players <player name or ID> ^7to see a logged-in player's account, character, level, XP and skill points. Add ^3force^7, ^3weapons^7, ^3protect^7, ^3ammo ^7or ^3items ^7to see their skill levels in that category.\n\n\"" );
		}
		else if (command_number == ADM_DUELARENA)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/duelarena ^7to set or unset the Duel Tournament arena in this map, and ^3/meleearena ^7for the Melee Battle arena. Both are saved automatically. Also, use ^3/duelpause ^7to pause/resume the tournament\n\n\"");
		}
		else if (command_number == ADM_CHANGEMAP)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/admmap <gametype number> <map name> ^7to change the server to a different map and gametype\n\n\"");
		}
		else if (command_number == ADM_CREATEITEM)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/createitem <itemname> ^7to create an item with a given name. Items containing more than one word need double quotes around the argument\n\n\"");
		}
		else if (command_number == ADM_GOD)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/god ^7to make yourself invincible, or ^3/notarget <player name (optional)> ^7to make NPCs and turrets ignore you or another player.\n\n\"");
		}
		else if (command_number == ADM_LEVELUP)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/levelup ^7or ^3/leveldown <player name> <number of levels (optional)> ^7to increase or decrease the level of a player respectively\n\n\"");
		}
		else if (command_number == ADM_SKILL)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/skillup ^7or ^3/skilldown <player name> <skill number> <number of levels (optional)> ^7to increase or decrease the skill levels of a player respectively\n\n\"");
		}
		else if (command_number == ADM_CREATECREDITS)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/createcredits <player name> <amount> ^7to create credits and place them to a player's account\n\n\"");
		}
		else if (command_number == ADM_IGNORECHATDISTANCE)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nWith this flag a player hears every range-limited chat (speech, RP modifiers, rolls) on the map, whatever the distance. Can be turned off with ^3/settings 5^7\n\n\"");
		}
		else if (command_number == ADM_XP)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/givexp ^7or ^3/removexp <player name> ^7to give or remove xp points respectively\n\n\"");
		}
		else if (command_number == ADM_UPDATENEWS)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/newsadd <channel> <text> ^7to add the news to a channel. The text has to be enclosed in double quotes for it to register properly\n\n\"");
		}
		else if (command_number == ADM_REMOVENEWS)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/newsremove <news ID> ^7to remove the news from a channel\n\n\"");
		}
		else if (command_number == ADM_MUSIC)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/playmusic <path> ^7to replace the current map music for all players with the song given\n\n\"");
		}
		else if (command_number == ADM_GETUP)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nThis flag lifts two limits: ^3/getup ^7gets you up at once without waiting for the countdown, and ^3/helpup <player name> ^7works on other players from any distance, even while you are downed yourself.\n\n\"");
		}
		else if (command_number == ADM_WEATHER)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/admweather <effect> ^7to set the weather for everyone on the server, and ^3/admweather add <effect> ^7to layer another one on top of it.\n\
^3/admweather default ^7puts back the weather the map was built with, ^3/admweather clear ^7switches it all off and ^3/admweather remove <number> ^7drops one layer.\n\
Run ^3/admweather ^7on its own to see the current weather, or ^3/admweather list ^7for the effects you can use\n\n\"");
		}
		else if (command_number == ADM_PHASE)
		{
			trap->SendServerCommand(ent - g_entities, "print \"\nUse ^3/ghost^7, ^3/holo^7, ^3/admwalkthrough ^7or ^3/admhidden <player name (optional)> ^7to make a player a Force ghost, a hologram, just walk-through, or hidden. Without a name it applies to you.\n\
All four let the player pass through other players and NPCs. The ghost and hologram also change how they look and hide them from the radar; a hidden player is not drawn at all and is off the radar too, though Force Sight still sees them. The same command again turns it off, and each one replaces the others.\n\
Damage and targeting are not affected. The ghost and hologram look is only seen by players with the client plugin; hidden works for everyone.\n\n\"");
		}
		else
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid admin command number. Use ^3/adminlist ^7to see your admin commands and their numbers.\n\"" );
		}
	}
}

/*
==================
Cmd_AdminUp_f
==================
*/
void Cmd_AdminUp_f( gentity_t *ent ) {
	char	arg1[MAX_STRING_CHARS];
	char	arg2[MAX_STRING_CHARS];
	int client_id = -1;
	int i = 0;
	int bitvaluecommand = 0;

	if (!check_admin_command(ent, ADM_GIVEADM, qtrue))
	{
		return;
	}

	if ( trap->Argc() != 3 )
	{
		trap->SendServerCommand( ent-g_entities, "print \"You must write the player name and the admin command number.\n\"" );
		return;
	}
	trap->Argv( 1,  arg1, sizeof( arg1 ) );
	trap->Argv( 2,  arg2, sizeof( arg2 ) );
	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode == 0)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Player is not logged in\n\"") );
		return;
	}
	if (Q_stricmp (arg2, "all") == 0)
	{ // zyk: if player wrote all, give all commands to the target player
		for (i = 0; i < ADM_NUM_CMDS; i++)
			g_entities[client_id].client->pers.bitvalue |= (1 << i);

		G_LogPrintf( "AdminUp: %s^7 granted all admin commands to %s^7\n", ent->client->pers.netname, g_entities[client_id].client->pers.netname );

		trap->SendServerCommand( ent-g_entities, va("print \"You granted all admin commands to %s^7.\n\"", g_entities[client_id].client->pers.netname) );
	}
	else
	{
		if (!StringIsInteger(arg2))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Invalid admin command\n\"") );
			return;
		}

		bitvaluecommand = atoi(arg2);
		if (bitvaluecommand < 0 || bitvaluecommand >= ADM_NUM_CMDS)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Invalid admin command\n\"") );
			return;
		}
		g_entities[client_id].client->pers.bitvalue |= (1 << bitvaluecommand);

		G_LogPrintf( "AdminUp: %s^7 granted %s ^7to %s^7\n", ent->client->pers.netname, admin_commands[bitvaluecommand].title, g_entities[client_id].client->pers.netname );
		trap->SendServerCommand( ent-g_entities, va("print \"You granted %s ^7to %s^7.\n\"", admin_commands[bitvaluecommand].title, g_entities[client_id].client->pers.netname) );
	}

	save_account(&g_entities[client_id], qfalse);
}

/*
==================
Cmd_AdminDown_f
==================
*/
void Cmd_AdminDown_f( gentity_t *ent ) {
	char	arg1[MAX_STRING_CHARS];
	char	arg2[MAX_STRING_CHARS];
	int client_id = -1;
	int bitvaluecommand = 0;

	if (!check_admin_command(ent, ADM_GIVEADM, qtrue))
	{
		return;
	}

	if ( trap->Argc() != 3 )
	{
		trap->SendServerCommand( ent-g_entities, "print \"You must write a player name and the admin command number.\n\"" );
		return;
	}
	trap->Argv( 1,  arg1, sizeof( arg1 ) );
	trap->Argv( 2,  arg2, sizeof( arg2 ) );
	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode == 0)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Player is not logged in\n\"") );
		return;
	}

	if (Q_stricmp (arg2, "all") == 0)
	{ // zyk: if player wrote all, take away all admin commands from target player
		g_entities[client_id].client->pers.bitvalue = 0;

		G_LogPrintf( "AdminDown: %s^7 revoked all admin commands from %s^7\n", ent->client->pers.netname, g_entities[client_id].client->pers.netname );

		trap->SendServerCommand( ent-g_entities, va("print \"You revoked all admin commands from %s^7.\n\"", g_entities[client_id].client->pers.netname) );
	}
	else
	{
		if (!StringIsInteger(arg2))
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Invalid admin command\n\"") );
			return;
		}

		bitvaluecommand = atoi(arg2);
		if (bitvaluecommand < 0 || bitvaluecommand >= ADM_NUM_CMDS)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Invalid admin command\n\"") );
			return;
		}
		g_entities[client_id].client->pers.bitvalue &= ~(1 << bitvaluecommand);

		G_LogPrintf( "AdminDown: %s^7 revoked %s ^7from %s^7\n", ent->client->pers.netname, admin_commands[bitvaluecommand].title, g_entities[client_id].client->pers.netname );
		trap->SendServerCommand( ent-g_entities, va("print \"You revoked %s ^7from %s^7.\n\"", admin_commands[bitvaluecommand].title, g_entities[client_id].client->pers.netname) );
	}

	save_account(&g_entities[client_id], qfalse);
}

void show_skill_change_message(gentity_t* ent, gentity_t* ent2, qboolean downgrade, qboolean success, int skill_id, int number_of_changes) {
	char success_message[256];

	if (downgrade == qtrue) {
		if (success == qtrue) {

			if (ent != ent2) {

				strcpy(success_message, "print \"^2You downgraded the target's ^3%s ^2skill by ^3%d ^2points. Current value: ^3%d^2.\n\"");
			}
			else {
				strcpy(success_message, "print \"^2You downgraded the ^3%s ^2skill by ^3%d ^2points. Current value: ^3%d^2.\n\"");
			}
		}
		else {
			if (ent != ent2) {

				strcpy(success_message, "print \"^1Target already reached the minimum level of ^3%s ^1skill. You can only downgrade by ^3%d ^1points. Nothing was updated. Current value: ^3%d^2.\n\"");
			}
			else {
				strcpy(success_message, "print \"^1You reached the minimum level of ^3%s ^1skill. You can only downgrade by ^3%d ^1points. Nothing was updated. Current value: ^3%d^2.\n\"");
			}
		}
	}
	else {
		if (success == qtrue) {

			if (ent != ent2) {

				strcpy(success_message, "print \"^2You upgraded the target's ^3%s ^2skill by ^3%d ^2points. Current value: ^3%d^2.\n\"");
			}
			else {
				strcpy(success_message, "print \"^2You upgraded the ^3%s ^2skill by ^3%d ^2points. Current value: ^3%d^2.\n\"");
			}


		}
		else {
			if (ent != ent2) {

				strcpy(success_message, "print \"^1Target already reached the maximum level of ^3%s ^1skill. You can only upgrade by ^3%d ^1points. Nothing was updated. Current value: ^3%d^2.\n\"");
			}
			else {
				strcpy(success_message, "print \"^1You already reached the maximum level of ^3%s ^1skill. You can only upgrade by ^3%d ^1points. Nothing was updated. Current value: ^3%d^2.\n\"");
			}
		}
	}

	trap->SendServerCommand(ent - g_entities, va(success_message, skills[skill_id].skill_name, number_of_changes, ent2->client->pers.skill_levels[skill_id]));

	if (success == qtrue && ent != ent2)
	{
		if (downgrade == qtrue)
		{
			trap->SendServerCommand(ent2 - g_entities, va("print \"^1Your ^3%s ^1skill was downgraded by ^3%d ^1points. Current value: ^3%d^1.\n\"", skills[skill_id].skill_name, number_of_changes, ent2->client->pers.skill_levels[skill_id]));
		}
		else
		{
			trap->SendServerCommand(ent2 - g_entities, va("print \"^2Your ^3%s ^2skill was upgraded by ^3%d ^2points. Current value: ^3%d^2.\n\"", skills[skill_id].skill_name, number_of_changes, ent2->client->pers.skill_levels[skill_id]));
		}
	}
}

// GalaxyRP (Alex) Used in cases where downgrading a skill removes a weapon. 
void zyk_deselect_weapon_if_active(gentity_t* ent, int weapon) {
	int new_weapon = WP_NONE;
	int i;

	if (ent->client->ps.weapon != weapon) {
		return;
	}

	for (i = 0; i < WP_NUM_WEAPONS; i++) {
		if ((ent->client->ps.stats[STAT_WEAPONS] & (1 << i)) && i != WP_NONE) {
			new_weapon = i;
			break;
		}
	}

	ent->s.weapon = new_weapon;
	ent->client->ps.weapon = new_weapon;

	G_AddEvent(ent, EV_NOAMMO, weapon);
}

void apply_skill_change_in_game(gentity_t* ent, int skill_id, qboolean upgrade) {
	switch (skill_id) {
	case 30:
		//GalaxyRP (Alex): [Skill] Reset max shield immediately.
		set_max_shield(ent);
		if (ent->client->ps.stats[STAT_ARMOR] > ent->client->pers.max_rpg_shield) {
			ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.max_rpg_shield;
		}
		break;
	case 54:
		ent->client->pers.max_force_power = (int)ceil((RP_MAX_FORCE_POWER / (double)RP_FORCE_POWER_SKILL_MAX_LEVEL) * ent->client->pers.skill_levels[skill_id]);
		ent->client->ps.fd.forcePowerMax = ent->client->pers.max_force_power;

		if (ent->client->ps.fd.forcePower > ent->client->ps.fd.forcePowerMax) {
			ent->client->ps.fd.forcePower = ent->client->ps.fd.forcePowerMax;
		}
		break;
	default:
		//GalaxyRP (Alex): [Skill] Do nothing for standard skills.
		break;
	}

	if (strcmp(skills[skill_id].category,"force") == 0 && (skills[skill_id].value_internal != 0 || skill_id == 9)) {
		if ((skills[skill_id].value_internal == FP_ABSORB || skills[skill_id].value_internal == FP_LIGHTNING)
			&& ent->client->pers.skill_levels[skill_id] >= 4) {
			ent->client->ps.fd.forcePowerLevel[skills[skill_id].value_internal] = FORCE_LEVEL_3;
		}
		else {
			ent->client->ps.fd.forcePowerLevel[skills[skill_id].value_internal] = ent->client->pers.skill_levels[skill_id];
		}

		if (upgrade) {
			if (!(ent->client->ps.fd.forcePowersKnown & (1 << skills[skill_id].value_internal))) {
				ent->client->ps.fd.forcePowersKnown |= (1 << skills[skill_id].value_internal);
			}
		}
		else {
			if (ent->client->ps.fd.forcePowerLevel[skills[skill_id].value_internal] == 0)
			{
				ent->client->ps.fd.forcePowersKnown &= ~(1 << skills[skill_id].value_internal);
			}
		}
	}

	//GalaxyRP (Alex): [Skill] Give them the weapon.
	if ((strcmp(skills[skill_id].category, "weapons") == 0 || strcmp(skills[skill_id].category, "ammo") == 0) && skills[skill_id].value_internal != 0) {
		if (upgrade) {
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << skills[skill_id].value_internal);
		}
		else {
			if (ent->client->pers.skill_levels[skill_id] == 0) {
				ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << skills[skill_id].value_internal);
				zyk_deselect_weapon_if_active(ent, skills[skill_id].value_internal);
			}
		}
	}
	
	//GalaxyRP (Alex): [Skill] Give or take away a lightsaber if they got the skill for it.
	if (skill_id == 5) {
		if (upgrade) {
			ent->client->ps.stats[STAT_WEAPONS] |= (1 << WP_SABER);
		}
		else {
			if (ent->client->pers.skill_levels[skill_id] == 0) {
				ent->client->ps.stats[STAT_WEAPONS] &= ~(1 << WP_SABER);
				zyk_deselect_weapon_if_active(ent, WP_SABER);
			}

			zyk_reset_saber_style(ent);
		}
	}

	//GalaxyRP (Alex): [Skill] Give them the item.
	if (strcmp(skills[skill_id].category, "items") == 0 && skills[skill_id].value_internal != 0) {
		if (upgrade) {
			if (skills[skill_id].value_internal != HI_JETPACK ||
				(RP_JetpackGrantAllowed(ent) &&
				 !(level.melee_mode >= 2 && level.melee_players[ent->s.number] != -1)))
			{
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] |= (1 << skills[skill_id].value_internal);
			}
		}
		else {
			if (ent->client->pers.skill_levels[skill_id] == 0) {
				ent->client->ps.stats[STAT_HOLDABLE_ITEMS] &= ~(1 << skills[skill_id].value_internal);

				zyk_adjust_holdable_items(ent);
			}
		}
	}
}

qboolean do_upgrade_skill(gentity_t* upgrader, gentity_t* upgradee, int skill_id, qboolean dont_show_message, int number_of_upgrades)
{
	if (skill_id < 0 || skill_id >= NUM_OF_SKILLS)
	{
		trap->SendServerCommand(upgrader - g_entities, "print \"Invalid skill number.\n\"");
		return qfalse;
	}

	if (skill_id == 38)
	{
		trap->SendServerCommand(upgrader - g_entities, "print \"This skill is no longer in use.\n\"");
		return qfalse;
	}

	int number_of_possible_upgrades = skills[skill_id].max_level - upgradee->client->pers.skill_levels[skill_id];
	if (number_of_possible_upgrades < number_of_upgrades) {
		number_of_upgrades = number_of_possible_upgrades;
	}

	if (number_of_possible_upgrades == 0) {
		show_skill_change_message(upgrader, upgradee, qfalse, qfalse, skill_id, number_of_upgrades);

		return qfalse;
	}

	if (upgradee->client->pers.skillpoints < number_of_upgrades)
	{
		if (dont_show_message == qfalse) {
			trap->SendServerCommand(upgradee - g_entities, "print \"^1You don't have enough skillpoints.\n\"");
			if (upgrader != upgradee) {
				trap->SendServerCommand(upgrader - g_entities, "print \"^1Target player doesn't have enough skillpoints.\n\"");
			}
		}
		return qfalse;
	}

	for (int i = 0; i < number_of_upgrades; i++) {
		upgradee->client->pers.skill_levels[skill_id]++;
		upgradee->client->pers.skillpoints--;
	}

	apply_skill_change_in_game(upgradee, skill_id, qtrue);
	show_skill_change_message(upgrader, upgradee, qfalse, qtrue, skill_id, number_of_upgrades);

	return qtrue;
}

qboolean do_downgrade_skill(gentity_t* downgrader, gentity_t* downgradee, int skill_id, int number_of_downgrades)
{
	qboolean dont_show_message = qfalse;

	// zyk: validation on the upgrade level, which must be in the range of valid skills.
	if (skill_id < 0 || skill_id >= NUM_OF_SKILLS)
	{
		trap->SendServerCommand(downgrader - g_entities, "print \"Invalid skill number.\n\"");
		return qfalse;
	}

	if (skill_id == 38)
	{
		trap->SendServerCommand(downgrader - g_entities, "print \"This skill is no longer in use.\n\"");
		return qfalse;
	}

	int number_of_possible_downgrades = downgradee->client->pers.skill_levels[skill_id];

	if (number_of_possible_downgrades < number_of_downgrades) {
		number_of_downgrades = number_of_possible_downgrades;
	}

	if (number_of_possible_downgrades == 0) {
		show_skill_change_message(downgrader, downgradee, qtrue, qfalse, skill_id, number_of_downgrades);

		return qfalse;
	}

	for (int i = 0; i < number_of_downgrades; i++) {
		downgradee->client->pers.skill_levels[skill_id]--;
		downgradee->client->pers.skillpoints++;
	}

	apply_skill_change_in_game(downgradee, skill_id, qfalse);
	show_skill_change_message(downgrader, downgradee, qtrue, qtrue, skill_id, number_of_downgrades);

	return qtrue;
}

/*
==================
Cmd_RpModeUp_f
==================
*/
void Cmd_RpModeUp_f( gentity_t *ent ) {
	char	arg1[MAX_STRING_CHARS];
	char	arg2[MAX_STRING_CHARS];
	char	arg3[MAX_STRING_CHARS];
	int client_id = -1;
	int number_of_upgrades = 1;

	if (!check_admin_command(ent, ADM_SKILL, qtrue))
	{
		return;
	}

	if ( trap->Argc() < 3 || trap->Argc() > 4)
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"You must write a player name or ID and the skill number.\n\"" ); 
		return; 
	}

	trap->Argv( 1,  arg1, sizeof( arg1 ) );
	trap->Argv( 2,  arg2, sizeof( arg2 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Player not found on server.\n\"");
		return;
	}

	if (trap->Argc() == 4) {
		trap->Argv( 3,  arg3, sizeof( arg3 ) );

		number_of_upgrades = atoi(arg3);

		if (number_of_upgrades <= 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid number of upgrades. Must be a positive number.\n\"" );
			return;
		}
	}

	if (g_entities[client_id].client->sess.amrpgmode < 2)
	{
		trap->SendServerCommand(ent - g_entities, "print \"The player is not logged in\n\"");
		return;
	}

	qboolean is_upgraded = qfalse;

	// zyk: the upgrade is done if it doesnt go above the maximum level of the skill
	is_upgraded = do_upgrade_skill(ent, &g_entities[client_id], atoi(arg2) - 1, qfalse, number_of_upgrades);

	if (is_upgraded == qtrue) {
		// GalaxyRP (Alex): [Database] Only update the skills table. Also update the characters table to save the skill point
		update_skills_table_row_with_current_values(&g_entities[client_id]);
	}
}

/*
==================
Cmd_RpModeDown_f
==================
*/
void Cmd_RpModeDown_f( gentity_t *ent ) {
	char	arg1[MAX_STRING_CHARS];
	char	arg2[MAX_STRING_CHARS];
	char	arg3[MAX_STRING_CHARS];
	int client_id = -1;
	int number_of_downgrades = 1;

	if (!check_admin_command(ent, ADM_SKILL, qtrue))
	{
		return;
	}

	if (trap->Argc() < 3 || trap->Argc() > 4)
	{
		trap->SendServerCommand(ent - g_entities, "print \"You must write a player name or ID and the skill number.\n\"");
		return;
	}

	trap->Argv( 1,  arg1, sizeof( arg1 ) );
	trap->Argv( 2,  arg2, sizeof( arg2 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Player not found on server.\n\"");
		return;
	}

	if (trap->Argc() == 4) {
		trap->Argv( 3,  arg3, sizeof( arg3 ) );

		number_of_downgrades = atoi(arg3);

		if (number_of_downgrades <= 0)
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid number of downgrades. Must be a positive number.\n\"" );
			return;
		}
	}

	if (g_entities[client_id].client->sess.amrpgmode < 2)
	{
		trap->SendServerCommand(ent - g_entities, "print \"The player is not logged in\n\"");
		return;
	}

	qboolean is_upgraded = qfalse;

	// zyk: the upgrade is done if it doesnt go above the maximum level of the skill
	is_upgraded = do_downgrade_skill(ent, &g_entities[client_id], atoi(arg2) - 1, number_of_downgrades);

	if (is_upgraded == qtrue) {
		// GalaxyRP (Alex): [Database] Only update the skills table.
		update_skills_table_row_with_current_values(&g_entities[client_id]);
	}
}

int calculate_skillpoints_for_level(int level) {
	int skillpoints = 0;

	if (level % 10 == 0) // zyk: every level divisible by 10 the player will get bonus skillpoints
		skillpoints += (level / 10) + 1;
	else
		skillpoints++;

	return skillpoints;
}

void increase_level(gentity_t* ent, qboolean admin_rp_mode, int number_of_levels)
{
	int levels_gained = 0;

	for (int i = 1; i <= number_of_levels; i++) {
		if (ent->client->pers.level < rp_rpg_max_level.integer)
		{
			ent->client->pers.level++;
			levels_gained++;

			ent->client->pers.skillpoints += calculate_skillpoints_for_level(ent->client->pers.level);

			// zyk: got a new level, so change the max health and max shield
			set_max_health(ent);
			set_max_shield(ent);
		}
	}

	if (levels_gained > 0)
	{
		trap->SendServerCommand(ent - g_entities, va("chat \"^3New Level: ^7%d^3, Skillpoints: ^7%d\n\"", ent->client->pers.level, ent->client->pers.skillpoints));
	}

	if (ent->client->pers.level >= rp_rpg_max_level.integer)
	{
		trap->SendServerCommand(ent - g_entities, va("chat \"^3You have reached maximum level!\n\""));
	}
}

/*
==================
Cmd_LevelGive_f
==================
*/
void Cmd_LevelGive_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	int client_id = -1;
	int number_of_levels = 1;

	if (!check_admin_command(ent, ADM_LEVELUP, qtrue))
	{
		return;
	}
   
	if ( trap->Argc() < 2 || trap->Argc() > 3)
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"^1Usage: ^2/levelup <player> <no of levels>(optional)\n\"" ); 
		return;
	}
	else {
		if (trap->Argc() == 3) {
			trap->Argv(2, arg2, sizeof(arg2));

			number_of_levels = atoi(arg2);

			if (number_of_levels <= 0)
			{
				trap->SendServerCommand( ent-g_entities, "print \"Invalid number of levels. Must be a positive number.\n\"" );
				return;
			}
		}
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Player not found on server.\n\"");
		return;
	}

	if (g_entities[client_id].client->sess.amrpgmode < 2)
	{
		trap->SendServerCommand(ent - g_entities, "print \"The player is not logged in\n\"");
		return;
	}

	if (g_entities[client_id].client->pers.level + number_of_levels > rp_rpg_max_level.integer) {
		int max_possible_value = rp_rpg_max_level.integer - g_entities[client_id].client->pers.level;

		if (max_possible_value <= 0)
		{
			trap->SendServerCommand(ent - g_entities, va("print \"^1That player is already at or above the maximum level (%d), operation not done.\n\"", rp_rpg_max_level.integer));
			return;
		}

		trap->SendServerCommand(ent - g_entities, va("print \"^1Too many levels selected, operation not done. Maximum allowed: %d\n\"", max_possible_value));
		return;
	}

	increase_level(&g_entities[client_id], qtrue, number_of_levels);

	trap->SendServerCommand(ent - g_entities, va("print \"^2Target player leveled up. Their current level is: ^3%i^2. Their skillpoint count is: ^3%i^2.\n\"", g_entities[client_id].client->pers.level, g_entities[client_id].client->pers.skillpoints));

	update_chars_table_row_with_current_values(&g_entities[client_id]);
}

// GalaxyRP (Alex): [Levelling] Check to see if there's enough free skillpoints to level down. (prevents skillpoints going negative).
qboolean check_if_player_can_level_down(gentity_t* ent, gentity_t* target, int number_of_levels) {
	int skillpoints_needed = 0;
	int current_level = target->client->pers.level;

	int level_at_end = current_level - number_of_levels;

	for(int i = current_level; i > level_at_end; i--) {
		if (current_level > 1)
		{
			skillpoints_needed += calculate_skillpoints_for_level(current_level);
			current_level--;
		}
	}

	if (target->client->pers.skillpoints < skillpoints_needed) {
		trap->SendServerCommand(ent - g_entities, va("print \"^1Operation could not be done. Player needs %d skillpoints, but only has %d available.\n\"", skillpoints_needed, target->client->pers.skillpoints));

		return qfalse;
	}

	return qtrue;
}

void decrease_level(gentity_t* ent, qboolean admin_rp_mode, int number_of_levels)
{
	int levels_lost = 0;

	int final_level = ent->client->pers.level - number_of_levels;

	for (int i = ent->client->pers.level; i > final_level; i--) {
		if (ent->client->pers.level > 1)
		{
			if (ent->client->pers.level % 10 == 0) // zyk: every level divisible by 10 the player will get bonus skillpoints
				ent->client->pers.skillpoints -= (ent->client->pers.level / 10) + 1;
			else
				ent->client->pers.skillpoints--;

			ent->client->pers.level--;
			levels_lost++;

			// zyk: got a new level, so change the max health and max shield
			set_max_health(ent);
			set_max_shield(ent);
		}
	}

	if (levels_lost > 0)
	{
		trap->SendServerCommand(ent - g_entities, va("chat \"^3New Level: ^7%d^3, Skillpoints: ^7%d\n\"", ent->client->pers.level, ent->client->pers.skillpoints));
	}

	if (ent->client->pers.level <= 1) {
		trap->SendServerCommand(ent - g_entities, va("chat \"^3You have reached minimum level!\n\""));
	}
}

/*
==================
Cmd_LevelTake_f
==================
*/
void Cmd_LevelTake_f(gentity_t* ent) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	int client_id = -1;
	int number_of_levels = 1;

	if (!check_admin_command(ent, ADM_LEVELUP, qtrue))
	{
		return;
	}

	if (trap->Argc() < 2 || trap->Argc() > 3)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^1Usage: ^2/leveldown <player> <no of levels>(optional)\n\"");
		return;
	}
	else {
		if (trap->Argc() == 3) {
			trap->Argv(2, arg2, sizeof(arg2));

			number_of_levels = atoi(arg2);

			// GalaxyRP fix: [Levelling] see the matching fix in Cmd_LevelGive_f above -- this optional
			// count argument was never validated. Rejected outright here instead.
			if (number_of_levels <= 0)
			{
				trap->SendServerCommand( ent-g_entities, "print \"Invalid number of levels. Must be a positive number.\n\"" );
				return;
			}
		}
	}

	trap->Argv(1, arg1, sizeof(arg1));

	client_id = ClientNumberFromString(ent, arg1, qfalse);

	if (client_id == -1)
	{
		// GalaxyRP fix: [validation] say so -- see the matching fix in Cmd_LevelGive_f above.
		trap->SendServerCommand(ent - g_entities, "print \"Player not found on server.\n\"");
		return;
	}

	// GalaxyRP fix: [validation] hoisted above the two checks below, same reasoning as Cmd_LevelGive_f
	// above: both of them read fields (pers.level, pers.skillpoints) that logout does not reset, so on
	// a logged-out target they reported on a stale character instead of saying the target is not
	// logged in. Wording unified with the rest of the admin commands.
	if (g_entities[client_id].client->sess.amrpgmode < 2)
	{
		trap->SendServerCommand(ent - g_entities, "print \"The player is not logged in\n\"");
		return;
	}

	// GalaxyRP fix: [Cvars] this used to also require zyk_rp_mode.integer == 1 here ("The server is not
	// at RP Mode"), but the server was always meant to be considered in RP Mode (that cvar's default),
	// and it has been removed -- so leveling down is unconditionally allowed to admins with ADM_LEVELUP now.

	if (g_entities[client_id].client->pers.level - number_of_levels < 1) {
		int min_possible_value = g_entities[client_id].client->pers.level - 1;
		trap->SendServerCommand(ent - g_entities, va("print \"^1Too many levels selected, operation not done. Cannot lower someone's level below 1. Maximum allowed: %d\n\"", min_possible_value));
		return;
	}
	if (check_if_player_can_level_down(ent, &g_entities[client_id], number_of_levels) == qfalse) {
		return;
	}

	// GalaxyRP fix: [Dead Code] the score_modifier/credits_modifier pair that used to sit here is gone
	// for the reasons given in Cmd_LevelGive_f above -- both fields are write-only across the tree.
	decrease_level(&g_entities[client_id], qtrue, number_of_levels);

	trap->SendServerCommand(ent - g_entities, va("print \"^2Target player leveled down. Their current level is: ^3%i^2. Their skillpoint count is: ^3%i^2.\n\"", g_entities[client_id].client->pers.level, g_entities[client_id].client->pers.skillpoints));

	// GalaxyRP fix: [Database] see the matching fix in Cmd_LevelGive_f above -- this used to pass
	// ent (the admin) instead of the target, so the level-down was applied live but never actually
	// persisted. Fixed to save the actual target.
	update_chars_table_row_with_current_values(&g_entities[client_id]);
}

// GalaxyRP (Alex): [XP System] This method returns the amount of XP needed to get to the next level, based on a given level.
int check_xp(int currentLevel) {
	int expNeeded = (currentLevel / 10) + 2;
	return expNeeded;
}

/*
==================
RP_LevelUpFromXP

DAJ_RP: [XP System] turns the XP a character holds into levels: while it covers the next level's
check_xp() and the character is under rp_rpg_max_level, that much is taken off and one level counted;
what is left is kept toward the next one. The levels are then granted in ONE increase_level() call --
skillpoints, max health and shield still per level in there -- so the player gets one "New Level" line
however many it was. A character this brings to the cap keeps no XP, as there is no next level to fill.
Returns the levels gained; adding the XP, saving the row and any message of their own are the callers'
(Cmd_GiveXp_f(), RP_PayNpcKillReward()), which used to do this each their own way: /givexp set XP to 0
after a level, throwing any remainder away, and the reward granted level by level, a line each.
==================
*/
static int RP_LevelUpFromXP( gentity_t *ent )
{
	int level = ent->client->pers.level;
	int levels = 0;

	while ( level < rp_rpg_max_level.integer && ent->client->pers.xp >= check_xp( level ) )
	{
		ent->client->pers.xp -= check_xp( level );
		level++;
		levels++;
	}

	if ( levels > 0 )
	{
		increase_level( ent, qtrue, levels );

		if ( ent->client->pers.level >= rp_rpg_max_level.integer )
		{
			ent->client->pers.xp = 0;
		}
	}

	return levels;
}

/*
==================
RP_PayNpcKillReward

DAJ_RP: [NPC Rewards] called from player_die() (g_combat.c) for everything that dies, with the attacker it
has resolved. An NPC -- not a vehicle -- that carries a reward from its spawner's "npccredits" or "npcxp"
(gentity_t::rpRewardCredits/rpRewardXP, RP_SpawnerRewardKeys() in NPC_spawn.c) pays it to that attacker
if it is a player logged in to a character; anyone else, logged out, an NPC, the world, is paid nothing.
Whatever its team: a reward is the admin's choice for that spawner. The reward is cleared either way, so
it is paid at most once.

Credits go through add_credits(), so rp_max_rpg_credits caps them. XP carries over from level to level, as
many levels as it covers, through RP_LevelUpFromXP() as /givexp's does; at rp_rpg_max_level there is nothing
for XP to do and it is not given, the same refusal /givexp makes. One
write saves credits, XP, level and skillpoints together, and one console line tells the player.
==================
*/
void RP_PayNpcKillReward( gentity_t *npc, gentity_t *killer )
{
	int credits, xp;
	int credits_given = 0, xp_given = 0;
	char line[512];
	const char *name;

	if ( !npc || !npc->NPC || !npc->client )
		return;

	credits = npc->rpRewardCredits;
	xp = npc->rpRewardXP;
	npc->rpRewardCredits = 0;
	npc->rpRewardXP = 0;

	if ( credits <= 0 && xp <= 0 )
		return;

	if ( npc->client->NPC_class == CLASS_VEHICLE )
		return;

	if ( !killer || !killer->inuse || !killer->client || killer->s.number < 0 || killer->s.number >= MAX_CLIENTS ||
		killer->client->pers.connected != CON_CONNECTED || killer->client->sess.amrpgmode != 2 || killer->client->pers.CharID <= 0 )
		return;

	name = ( npc->NPC_type && npc->NPC_type[0] ) ? npc->NPC_type : "an NPC";
	Com_sprintf( line, sizeof( line ), "^3Reward for killing ^7%s^3:", name );

	if ( credits > 0 )
	{
		const int before = killer->client->pers.credits;

		add_credits( killer, credits );
		credits_given = killer->client->pers.credits - before;

		if ( credits_given <= 0 )
			Q_strcat( line, sizeof( line ), " ^7no credits (already at the maximum)" );
		else if ( credits_given < credits )
			Q_strcat( line, sizeof( line ), va( " ^2+%d credits ^7(the maximum)", credits_given ) );
		else
			Q_strcat( line, sizeof( line ), va( " ^2+%d credits", credits_given ) );
	}

	if ( xp > 0 )
	{
		if ( credits > 0 )
			Q_strcat( line, sizeof( line ), "^3," );

		if ( killer->client->pers.level >= rp_rpg_max_level.integer )
		{
			Q_strcat( line, sizeof( line ), " ^7no XP (maximum level)" );
		}
		else
		{
			xp_given = xp;
			killer->client->pers.xp += xp;
			Q_strcat( line, sizeof( line ), va( " ^2+%d XP", xp ) );
		}
	}

	trap->SendServerCommand( killer->s.number, va( "print \"%s\n\"", line ) );

	// zyk: each level the XP covers, granted at once -- see RP_LevelUpFromXP()
	if ( xp_given > 0 )
	{
		RP_LevelUpFromXP( killer );
	}

	if ( credits_given > 0 || xp_given > 0 )
	{
		update_chars_table_row_with_current_values( killer );
		G_LogPrintf( "NPC reward: %s^7 killed %s (%d): %d credits, %d XP\n", killer->client->pers.netname, name, npc->s.number, credits_given, xp_given );
	}
}

/*
==================
Cmd_GiveXp_f
==================
*/
void Cmd_GiveXp_f(gentity_t* ent) {
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;

	if (!check_admin_command(ent, ADM_XP, qtrue))
	{
		return;
	}

	if (trap->Argc() != 2)
	{
		trap->SendServerCommand(ent - g_entities, "print \"You must specify the player name or ID.\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	client_id = ClientNumberFromString(ent, arg1, qfalse);

	if (client_id == -1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Player not found on server.\n\"");
		return;
	}

	// GalaxyRP fix: [validation] CMD_LOGGEDIN in the command table gates the ADMIN issuing this, not
	// the target, and ClientNumberFromString(..., qfalse) only requires the target be connected. A
	// not-yet-logged-in target has pers.CharID == 0, so update_chars_table_row_with_current_values()
	// below runs "UPDATE Characters ... WHERE CharID=0", which matches no row (CharID is a SQLite
	// INTEGER PRIMARY KEY, always >= 1 for a real character): the XP change was applied to that
	// connection's memory, never persisted, and lost the moment they logged in or disconnected --
	// while this command still reported success to both the admin and the target. Same check, same
	// message, as Cmd_CreditCreate_f and Cmd_LevelGive_f already carry.
	if (g_entities[client_id].client->sess.amrpgmode < 2)
	{
		trap->SendServerCommand(ent - g_entities, "print \"The player is not logged in\n\"");
		return;
	}

	// GalaxyRP fix: [XP System] refuse outright when the target is already at or above the level cap,
	// instead of taking the XP and then claiming a level-up that did not happen. Previously the XP was
	// incremented, increase_level() was called and did nothing (its loop is gated on
	// "level < rp_rpg_max_level"), and the admin was told "Target player leveled up. Their current
	// level is: 100" regardless -- a flatly false report. The XP was then reset to 0 as well, so the
	// counter cycled 0 -> threshold -> 0 forever on a maxed character, consuming XP that could never
	// convert into anything. ">=" rather than "==" so a character sitting ABOVE the cap (which is what
	// happens whenever rp_rpg_max_level is lowered below an existing character's level) is refused too.
	// Same refusal, same wording, as Cmd_LevelGive_f already gives for this case.
	//
	// This also makes the "leveled up" message below true whenever it is sent: with level < the cap,
	// RP_LevelUpFromXP() gains at least one level once the XP reaches the threshold.
	if (g_entities[client_id].client->pers.level >= rp_rpg_max_level.integer)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"^1That player is already at or above the maximum level (%d), operation not done.\n\"", rp_rpg_max_level.integer));
		return;
	}

	g_entities[client_id].client->pers.xp++;

	trap->SendServerCommand(&g_entities[client_id] - g_entities, va("chat \"^2You were given XP! Your current XP is: ^3%i^2/^3%i^2\n\"", g_entities[client_id].client->pers.xp, check_xp(g_entities[client_id].client->pers.level)));
	trap->SendServerCommand(ent - g_entities, va("print \"^2Target player was given XP. Their current XP is: ^3%i^2/^3%i^2\n\"", g_entities[client_id].client->pers.xp, check_xp(g_entities[client_id].client->pers.level)));

	// GalaxyRP (Alex): [XP System] If player has reached max xp for this level, level them up.
	// GalaxyRP fix: [XP System] ">=" rather than "==". XP only ever moves one point at a time today,
	// so an exact match is reached in practice -- but a stored value that ever sits ABOVE its
	// threshold (a row written under a different check_xp() formula, or edited directly in the
	// database) could never satisfy "==" again, permanently freezing that character's level with no
	// symptom other than XP that climbs and never converts. ">=" costs nothing and cannot get stuck.
	//
	// DAJ_RP: [XP System] through RP_LevelUpFromXP(), which keeps what is left over toward the next level.
	// This set XP to 0 after the level instead -- the same thing while XP only ever reached its threshold
	// exactly, but a character holding more (after a /leveldown, whose lower level needs less) lost the
	// rest. A stored excess big enough now takes the levels it covers, at once, as an NPC reward does.
	if (RP_LevelUpFromXP(&g_entities[client_id]) > 0) {
		trap->SendServerCommand(ent - g_entities, va("print \"^2Target player leveled up. Their current level is: ^3%i^2. Their skillpoint count is: ^3%i^2.\n\"", g_entities[client_id].client->pers.level, g_entities[client_id].client->pers.skillpoints));
	}

	update_chars_table_row_with_current_values(&g_entities[client_id]);

	return;
}

/*
==================
Cmd_RemoveXp_f
==================
*/
void Cmd_RemoveXp_f(gentity_t* ent) {
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;

	if (!check_admin_command(ent, ADM_XP, qtrue))
	{
		return;
	}

	if (trap->Argc() != 2)
	{
		trap->SendServerCommand(ent - g_entities, "print \"You must specify the player name or ID.\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	client_id = ClientNumberFromString(ent, arg1, qfalse);

	if (client_id == -1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Player not found on server.\n\"");
		return;
	}

	// GalaxyRP fix: [validation] CMD_LOGGEDIN in the command table gates the ADMIN issuing this, not
	// the target, and ClientNumberFromString(..., qfalse) only requires the target be connected. A
	// not-yet-logged-in target has pers.CharID == 0, so update_chars_table_row_with_current_values()
	// below runs "UPDATE Characters ... WHERE CharID=0", which matches no row (CharID is a SQLite
	// INTEGER PRIMARY KEY, always >= 1 for a real character): the XP change was applied to that
	// connection's memory, never persisted, and lost the moment they logged in or disconnected --
	// while this command still reported success to both the admin and the target. Same check, same
	// message, as Cmd_CreditCreate_f and Cmd_LevelGive_f already carry.
	if (g_entities[client_id].client->sess.amrpgmode < 2)
	{
		trap->SendServerCommand(ent - g_entities, "print \"The player is not logged in\n\"");
		return;
	}

	// GalaxyRP fix: [XP System] "<= 0" rather than "== 0", the same robustness the level-up threshold
	// in Cmd_GiveXp_f above was given. XP cannot go negative through any live code path, but a value
	// that ever did (a row edited directly in the database) would fail an exact-zero test and be
	// decremented further away from zero on every use, with no floor to stop it.
	if (g_entities[client_id].client->pers.xp <= 0) {
		trap->SendServerCommand(ent - g_entities, va("print \"^2Target player's XP is already at 0. Nothing was done.\n\""));
		return;
	}

	g_entities[client_id].client->pers.xp--;

	// GalaxyRP fix: [XP System] tell the target as well. Cmd_GiveXp_f announces itself to the player
	// it credits; this one reported only to the admin, so a player's XP could drop with no indication
	// to them at all. Same channel and same shape as the give side, so the two read as a pair.
	trap->SendServerCommand(&g_entities[client_id] - g_entities, va("chat \"^1One XP point was removed. Your current XP is: ^3%i^1/^3%i^1\n\"", g_entities[client_id].client->pers.xp, check_xp(g_entities[client_id].client->pers.level)));
	trap->SendServerCommand(ent - g_entities, va("print \"^2Target player's XP was reduced with one point. Their current XP is: ^3%i^2/^3%i^2\n\"", g_entities[client_id].client->pers.xp, check_xp(g_entities[client_id].client->pers.level)));

	update_chars_table_row_with_current_values(&g_entities[client_id]);

	return;
}

/*
==================
Cmd_EntitiesAndRemaps_f

GalaxyRP: [Entity System] /entitiesandremaps (it was /entitysystem): the commands of the Entities and
Remaps admin power, in two parts -- the entity commands, in groups, and the shader remap commands. The
lines are sent packed into as few prints as fit: one server command carries at most 1022 characters
(the whole print, its wrapper included), and a print that does not fit is dropped whole. A group may run
on into the next print; on the console it reads the same.
==================
*/
#define ZYK_HELP_PRINT_MAX	1000	// the text of one print: with "print \"" and the closing quote, 1008

static void zyk_print_lines( gentity_t *ent, const char * const *lines, int count )
{
	char text[ZYK_HELP_PRINT_MAX + 1];
	int i;

	text[0] = '\0';
	for (i = 0; i < count; i++)
	{
		if (text[0] && (int)(strlen(text) + strlen(lines[i])) > ZYK_HELP_PRINT_MAX)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"%s\"", text) );
			text[0] = '\0';
		}
		Q_strcat( text, sizeof( text ), lines[i] );
	}
	if (text[0])
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s\"", text) );
	}
}

void Cmd_EntitiesAndRemaps_f( gentity_t *ent ) {
	static char entlist_line[512];
	const char *lines[64];	// 50 now
	int n = 0;

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	Com_sprintf( entlist_line, sizeof( entlist_line ), "^3/entlist <page number>: ^7Lists all entities present on the map. Ids from %d up (marked ^3L^7) are logical entities: spawn points, targets, NPC spawners and other never-networked classes. Ids marked ^3G^7 were created by the game and ^3M^7 are part of the map: neither can be edited or removed. ^3E^7 are the map's pickups, dispensers and decor that nothing in the map links to, which can. ^3H^7 are being held.\n", MAX_GENTITIES );

	lines[n++] = "\n^2Entities and Remaps\n\n";
	lines[n++] = "^3--------Entity System--------\n";

	lines[n++] = "^5Placing\n";
	lines[n++] = "^3/entadd <classname> <key> <value> <key> <value>...: ^7Adds a new entity where you stand, at the /entorigin position, or at an origin you give (origin 0 0 0 keeps a map brush in place).\n";
	lines[n++] = "^3/entaddaim <classname> <key> <value>...: ^7Like /entadd, but puts the entity on the surface you aim at (through players and NPCs); ^3aimoffset <units>^7 puts it that far out from the surface.\n";
	lines[n++] = "^3/entorigin: ^7Sets your position as origin for new entities. Use again to unset.\n";
	lines[n++] = "^3/entundo: ^7Removes last added entity; a spawner takes the NPCs it made with it. Only works once.\n";
	lines[n++] = "^7NPC spawners: ^3npc_spawner^7 and ^3NPC_Vehicle^7 (npc_type) need a ^3targetname^7, the name they are fired by; ^3spawnnow 1^7 also spawns one at once, ^3respawn 1^7 fires it again when one it made dies, ^3count -1^7 is no limit; ^3npceffect holo^7, ^3ghost^7 or ^3walkthrough^7 spawns NPCs with that ^3/npc effect^7; ^3npcteam player^7, ^3enemy^7, ^3neutral^7 or ^3free^7 sets their side, as ^3/npc team^7 does; ^3npcfreeze 1^7 spawns them frozen, as ^3/npc freeze^7 leaves them; ^3npcscale <20-500>^7 sets their size in percent (^3100^7 is normal).\n";
	lines[n++] = "^7NPC kill rewards: on an ^3npc_spawner^7, ^3npccredits <0-100000>^7 and ^3npcxp <0-100>^7 are paid to the logged-in player who kills one of its NPCs. Vehicles pay nothing.\n";

	lines[n++] = "^5Finding and changing\n";
	lines[n++] = entlist_line;
	lines[n++] = "^3/entnear <distance>: ^7Lists entities in less than 200 map units or distance passed as argument.\n";
	lines[n++] = "^3/enthelp <classname, entity id or part of a name> <key (optional)>: ^7Shows what an entity class does, its keys and spawnflags, with an example; with a key, that key alone. Without a name: the entity you aim at. ^3/enthelp list^7 lists every class.\n";
	lines[n++] = "^3/entedit <entity id (optional)> <key> <value>...: ^7Edits the entity you aim at, or that id; without key/value pairs it shows its info. The classname cannot be changed.\n";
	lines[n++] = "^3/entremove <entity id (optional)> <last entity id (optional)>: ^7Removes the entity you aim at, or that id, or every entity from the first id to the second when two are given (a range cannot cross from networked to logical ids). A removed door or platform takes its trigger with it. Spawn points cannot be removed.\n";
	lines[n++] = "^7/entedit and /entremove do not change the map's own entities (^3M^7; those marked ^3E^7 can be changed) or those the game creates (^3G^7: saber entities, door triggers, missiles, NPCs, dropped items). View them with /entedit, copy them with /entcopy; use ^3/npc kill^7 for NPCs.\n";

	lines[n++] = "^5Moving\n";
	lines[n++] = "^3/entcopy <entity id (optional)>: ^7Picks up a copy of the entity you aim at, or of that id. Aim where it should go and use /entcopy again to drop it there.\n";
	lines[n++] = "^3/entcut <entity id (optional)>: ^7Picks up the entity itself; it is gone until /entcut again drops it where you aim. Map entities (^3M^7) and brush entities cannot be cut.\n";
	lines[n++] = "^3/entrotate <yaw> or <pitch> <yaw> <roll> (optional): ^7Turns what you hold, or the entity you aim at: 45 degrees of yaw, that much yaw, or exactly those angles. Not ^3M^7 or brush entities in place.\n";
	lines[n++] = "^3/entcopystatic <#number (optional)>: ^7Picks up a solid misc_model_breakable copy of the static model you aim at (^1red^7 in Entity Bounds), or of that number; use it again to drop it. The static model stays, and the copy's model takes a model slot until the map changes.\n";
	lines[n++] = "^3/entaddgrab <classname> <key> <value>...: ^7Like /entaddaim, but picks the new entity up instead, not spawned yet; use /entaddgrab again to place it where you aim. Takes ^3aimoffset <units>^7 too.\n";
	lines[n++] = "^3/entcancel: ^7Lets go of what you hold; a cut entity goes back where it was, a copy of a static model or a new /entaddgrab entity is deleted. While you hold one, commands that add, change, use or remove entities are refused.\n";

	lines[n++] = "^5Saving\n";
	lines[n++] = "^3/entsave <filename>: ^7Saves current entities into a preset file. Use ^3default ^7name to make it load with the map.\n";
	lines[n++] = "^3/entload <filename>: ^7Loads entities from a preset file.\n";
	lines[n++] = "^3/entdeletefile <filename>: ^7Deletes entity preset file.\n";

	// DAJ_RP: [Locks] g_locks.c
	lines[n++] = "^5Locks\n";
	lines[n++] = "^3/entlockadd <lock> <account> [more accounts]: ^7Lets those accounts use whatever carries that lock on this map. Each must be an existing account. Makes the lock when it is new.\n";
	lines[n++] = "^3/entlockremove <lock> <account> [more accounts] ^7or ^3/entlockremove <lock> all: ^7Takes accounts off the lock, or all of them. What carries a lock with nobody on it lets nobody through.\n";
	lines[n++] = "^3/entlockset <entity id (optional)> <lock | none>: ^7Puts a lock on the door, lift, button, trigger or any entity you aim at, or that id, or takes it off. The map's own get it by their brush model, kept in the map's lock file. A locked door team is locked whole.\n";
	lines[n++] = "^3/entlocklist <lock (optional)>: ^7Lists this map's locks, or one lock's accounts and the entities carrying it.\n";
	lines[n++] = "^7A locked entity lets through only logged-in players whose account is on its lock (a vehicle: its pilot's). NPCs never do. Timers and map logic with no player behind them pass, and so do /entuse, /enttrigger and a level 5 Stun Baton, with what they set off, later too. Harmful triggers and target_kill cannot be locked.\n";

	lines[n++] = "^5Other\n";
	lines[n++] = "^3/entuse <name>: ^7Uses every entity with that targetname, as a trigger or a button would: spawners, lights, effects, doors. NPCs and vehicles are left alone.\n";
	lines[n++] = "^3/enttrigger <entity id (optional)>: ^7Activates the entity you aim at, or that id, as a level 5 Stun Baton does: a door, lift or button opens even when locked or inactive, anything else is used. Account locks do not stop it.\n";
	lines[n++] = "^3/entslots: ^7Shows how full the map's model, effect and sound slots are, and how many can be reused.\n";
	lines[n++] = "^3/list md3^7, ^3/list glm ^7and ^3/list effects^7: Show the .md3 and .glm model files and the effect files the server has, for props.\n";
	lines[n++] = "^3/settings 6: ^7Entity Bounds -- draws the box of the entity you aim at, and marks nearby spawn points, targets and other point entities. The map's static models are drawn and marked in ^1red^7.\n";
	lines[n++] = "^3/removepickups: ^7Removes all pickups from the current map (ammo, health, shield, and weapons), the map's own (^3M^7) included.\n";
	lines[n++] = "^3/spawnplatform <height (optional)>: ^7Spawns a platform under your feet. Without a height it stays put; with one it is a lift that rises that many units when someone steps onto it, lowered to fit under a ceiling.\n";
	lines[n++] = "^3/spawndummy: ^7Spawns a saber training dummy in front of you, facing you (where you stand when there is no room), showing the damage each series of hits does.\n";
	lines[n++] = "^7Props: ^3misc_model_breakable^7 (model, modelscale, light, color; spawnflags 1 solid, 2 animated), ^3misc_model_ghoul^7 (a .glm model), ^3rp_light^7 (light, color), ^3fx_runner^7 (fxFile). Model and effect files must be on the server. See ^3/enthelp^7.\n";

	lines[n++] = "\n^3--------Shader Remaps--------\n";
	lines[n++] = "^3/remap <shader> <new shader>: ^7Remaps shader in the map.\n";
	lines[n++] = "^3/remaplist <page number>: ^7Lists already remapped shaders in the map, eight per page.\n";
	lines[n++] = "^3/remapsave <file name>: ^7Saves current remaps in a preset file. Use ^3default ^7name to make it load with the map.\n";
	lines[n++] = "^3/remapload <file name>: ^7Loads remaps from preset file.\n";
	lines[n++] = "^3/remapdeletefile <file name>: ^7Deletes remap preset file.\n";
	lines[n++] = "^3/remapreset: ^7Clears all shader remaps in the map. Does not undo remaps built into the map itself.\n\n";

	zyk_print_lines( ent, lines, n );
}

/*
==================
Cmd_EntSlots_f

GalaxyRP: [Slot Reuse] /entslots: how full the map's effect, model and sound slots and its gamestate are,
how many slots only Entity System props asked for, how many of those nothing uses now (the ones a new
name can be given once the table or the gamestate is full), and how many of this map's reuses are
spent -- see RP_SlotReclaim() in g_utils.c. For the admin whose prop was refused for lack of room.
==================
*/
void Cmd_EntSlots_f( gentity_t *ent ) {
	int used, slots, esOnly, reusable, reuses, limit;
	char text[MAX_STRING_CHARS];

	if (!check_admin_command(ent, ADM_ENTITYSYSTEM, qtrue))
	{
		return;
	}

	RP_SlotStats( CS_EFFECTS, &used, &slots, &esOnly, &reusable, &reuses, &limit );
	Com_sprintf( text, sizeof( text ), "\n^3Effects: ^7%d of %d slots in use. %d only by Entity System props, %d of them free to reuse now. Reused this map: %d of %d.\n",
		used, slots, esOnly, reusable, reuses, limit );

	RP_SlotStats( CS_MODELS, &used, &slots, &esOnly, &reusable, &reuses, &limit );
	Q_strcat( text, sizeof( text ), va( "^3Models: ^7%d of %d slots in use. %d only by Entity System props, %d of them free to reuse now. Reused this map: %d of %d.\n",
		used, slots, esOnly, reusable, reuses, limit ) );

	RP_SlotStats( CS_SOUNDS, &used, &slots, &esOnly, &reusable, &reuses, &limit );
	Q_strcat( text, sizeof( text ), va( "^3Sounds: ^7%d of %d slots in use. %d only by /playsound (at most %d at once), %d of them free to reuse now. Reused this map: %d of %d.\n",
		used, slots, esOnly, RP_PLAYSOUND_POOL, reusable, reuses, limit ) );

	Q_strcat( text, sizeof( text ), va( "^3Gamestate: ^7%d of %d bytes.\n", G_GamestateBytesUsed(), ZYK_GAMESTATE_BUDGET ) );
	Q_strcat( text, sizeof( text ), va( "^3Memory pool: ^7%d of %d KB used, %d KB held in reserve; freed when the map changes.\n",
		( G_AllocPoolSize() - G_AllocRemaining() ) / 1024, G_AllocPoolSize() / 1024, ZYK_ALLOC_RESERVE / 1024 ) );
	Q_strcat( text, sizeof( text ), va( "^7A slot is reused only when there is no free one, or no room in the gamestate, for a new name -- or, for /playsound, once it holds %d.\n\n", RP_PLAYSOUND_POOL ) );

	trap->SendServerCommand( ent-g_entities, va( "print \"%s\"", text ) );
}

/*
==================
Cmd_AdmKick_f
==================
*/
void Cmd_AdmKick_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;

	if (!check_admin_command(ent, ADM_KICK, qtrue))
	{
		return;
	}
   
	if ( trap->Argc() != 2) 
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"You must specify the player name or ID.\n\"" ); 
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	client_id = ClientNumberFromString( ent, arg1, qtrue );

	if (client_id == -1)
	{
		return;
	}

	trap->SendConsoleCommand( EXEC_APPEND, va( "kick %d\n", client_id) );
}

/*
==================
Cmd_AdmMap_f

GalaxyRP fix: [Admin] new admin command, adapted from JAPro's "ammap" command (bundled with the
TaystJK engine this mod now targets -- see https://github.com/taysta/TaystJK). Renamed "ammap" to
"admmap" to match this mod's existing "adm*" admin command naming (admkick, adminup, etc). JAPro's
version gates the command on its own two-rank, server-config-driven admin system
(G_AdminAllowed(..., JAPRO_ACCOUNTFLAG_A_CHANGEMAP, ...)); GalaxyRP has no such ranks, so that
check is replaced with this mod's per-account admin bit system (check_admin_command(ent,
ADM_CHANGEMAP, ...)), using the previously-unused "Placeholder" bit (32768) freed up by a removed
command. The gametype bound was also switched from JAPro's hardcoded '0'-'8' char check to this
mod's own GT_MAX_GAME_TYPE, since the two projects' gametype lists don't necessarily match, and a
message is now sent back to the admin when the requested map isn't found instead of silently doing
nothing.
==================
*/
void Cmd_AdmMap_f( gentity_t *ent ) {
	char	gametype[8];
	int		gtype = 0;
	char	mapname[MAX_MAPNAMELENGTH];

	if (!check_admin_command(ent, ADM_CHANGEMAP, qtrue))
	{
		return;
	}

	if ( trap->Argc() != 3 )
	{
		trap->SendServerCommand( ent-g_entities, "print \"Usage: /admmap <gametype number> <map name>.\n\"" );
		return;
	}

	trap->Argv( 1, gametype, sizeof( gametype ) );
	trap->Argv( 2, mapname, sizeof( mapname ) );

	if (strchr(mapname, ';') || strchr(mapname, '\r') || strchr(mapname, '\n'))
	{ // zyk: block console command injection through the map name argument
		trap->SendServerCommand( ent-g_entities, "print \"Invalid map name.\n\"" );
		return;
	}

	// GalaxyRP fix: [Admin] a backslash is refused for the same reason G_VoteMap() refuses one: it is not
	// a character a map path uses, and the path below is handed to the engine's file system.
	if (strchr(mapname, '\\'))
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid map name.\n\"" );
		return;
	}

	// GalaxyRP fix: [Admin] atoi() silently returns 0 for a non-numeric string (e.g. "dfd"
	// would parse as gametype 0 / FFA instead of being rejected), so require the argument to
	// actually be an integer first using this file's existing StringIsInteger() helper.
	//
	// GalaxyRP fix: [Admin] the old check only rejected values outside 0..GT_MAX_GAME_TYPE-1,
	// which let an admin "successfully" switch the server to a gametype Jedi Academy doesn't
	// actually support (GT_HOLOCRON, GT_JEDIMASTER, GT_SINGLE_PLAYER, GT_CTY -- 1, 2, 5, 9),
	// leaving the server in a broken/inert state. Only the six gametypes this game actually
	// implements are accepted now.
	if (!StringIsInteger(gametype))
	{
		trap->SendServerCommand( ent-g_entities, "print \"Invalid gametype. Choose: 0 - Free For All, 3 - Duel, 4 - Power Duel, 6 - Team Free For All, 7 - Siege, 8 - Capture the Flag.\n\"" );
		return;
	}

	gtype = atoi(gametype);

	switch (gtype)
	{
		case GT_FFA:
		case GT_DUEL:
		case GT_POWERDUEL:
		case GT_TEAM:
		case GT_SIEGE:
		case GT_CTF:
			break;
		default:
			trap->SendServerCommand( ent-g_entities, "print \"Invalid gametype. Choose: 0 - Free For All, 3 - Duel, 4 - Power Duel, 6 - Team Free For All, 7 - Siege, 8 - Capture the Flag.\n\"" );
			return;
	}

	{ // zyk: make sure the requested map actually exists before changing to it
		// GalaxyRP fix: [Admin] opened directly, the way G_VoteMap() checks a /callvote map, instead of
		// looking the name up in FS_GetFileList( "maps", ".bsp" ). That listing missed real maps in two
		// ways (qcommon/files.cpp): FS_GetFileList() stops without a word once its buffer is full, and this
		// one was 4096 bytes -- a couple of hundred map names -- so on a large map collection the maps
		// past that point were "not found"; and for loose files on disk (not inside a pk3) the listing
		// only reads maps/ itself, never a subfolder, so a loose maps/mp/<name>.bsp could not be loaded
		// at all. FS_Open() finds a map anywhere the engine's own "map" command would. The length test
		// matches what that command can take: it builds the same "maps/<name>.bsp" in a MAX_QPATH buffer.
		char			bspName[MAX_QPATH] = {0};
		fileHandle_t	fp = NULL_FILE;
		qboolean		found = qfalse;

		if (strlen(mapname) + strlen("maps/.bsp") < sizeof(bspName))
		{
			Com_sprintf( bspName, sizeof(bspName), "maps/%s.bsp", mapname );

			if ( trap->FS_Open( bspName, &fp, FS_READ ) > 0 )
			{
				found = qtrue;
			}

			if ( fp != NULL_FILE )
			{
				trap->FS_Close( fp );
			}
		}

		if (!found)
		{
			// GalaxyRP fix: [Admin] this engine's console tokenizer (Cmd_TokenizeString2 in
			// cmd.cpp) does not support backslash-escaped quotes inside a quoted string -- a
			// literal \" here isn't an escaped quote, it's a backslash followed by the quote
			// that closes the string early, cutting the printed message down to "Map \" and
			// dropping everything after it. Use single quotes around the map name instead
			// (same convention already used elsewhere in this file, e.g. the duel team
			// validation message above) so the double-quoted print string stays intact.
			trap->SendServerCommand( ent-g_entities, va("print \"Map '%s' not found.\n\"", mapname) );
			return;
		}
	}

	trap->SendServerCommand( -1, va("print \"^3Map change triggered by ^7%s\n\"", ent->client->pers.netname) );
	G_LogPrintf( "Map change triggered by ^7%s\n", ent->client->pers.netname );

	trap->SendConsoleCommand( EXEC_APPEND, va("g_gametype %i\n", gtype) );
	trap->SendConsoleCommand( EXEC_APPEND, va("map %s\n", mapname) );
}

/*
==================
/admweather -- weather for the whole server

Weather is not state anywhere. The game module owns a table of effect strings (CS_EFFECTS), the
client runs every "*"-prefixed one through the renderer's world-effect parser, and that parser is a
COMMAND STREAM: "*rain" followed by "*snow" gives rain AND snow, and the only removal it offers is
"*clear", which wipes the lot. On top of that, a client joining later replays the whole table in
slot order while a client already connected runs each string as its configstring changes.

Left alone, that gives you a choice of two broken behaviours, and the two mods that have tried this
picked one each. Registering a fresh effect string per change (what /entadd zyk_weather does today)
keeps every client consistent but spends one of ~63 slots each time, and running out is a server
drop for everyone. Reusing a single slot (what JA++'s /amweather does) is cheap but incoherent:
connected players accumulate every command ever issued while a late joiner sees only the last one,
so the same server shows different skies to different people, permanently.

This command does neither. It reserves ZYK_WEATHER_SLOTS contiguous slots once, and every change
rewrites ALL of them -- the teardown in the low slots, the layers right-aligned above it, and a
counter on each so the strings differ from last time and actually re-broadcast. Whichever way a
client arrives at that block, it reads the same sequence in the same order, so everyone ends up
looking at the same sky. See the block comment on ZYK_WEATHER_MAX_LAYERS in g_local.h.
==================
*/

typedef enum {
	ZYK_WFX_CLOUD,		// a particle cloud -- the renderer keeps at most ZYK_WEATHER_MAX_CLOUDS
	ZYK_WFX_WIND,		// a wind zone -- at most ZYK_WEATHER_MAX_WINDS
	ZYK_WFX_UNKNOWN
} zyk_weather_kind_t;

typedef struct zyk_weather_effect_s {
	const char			*name;
	int					args;	// how many whole numbers the admin supplies
	zyk_weather_kind_t	kind;
} zyk_weather_effect_t;

// zyk: the effects /admweather offers. The engine parses nineteen commands; these are the ones
// worth handing an admin.
//
// Left out on purpose:
//   outsidepain, outsideshake -- parsed, but dead in multiplayer. They set a flag and nothing in
//       codemp reads it (R_IsShaking and R_IsOutsideCausingPain have no callers at all); only the
//       single-player build wires them up.
//   die -- a heavier teardown than clear that also drops the map's WEATHER ZONES, and a zone cannot
//       be put back at runtime, because misc_weather_zone is read from the BSP's own entity lump by
//       cgame and never from an entity spawned here. Losing them costs indoor/outdoor culling for
//       the rest of the map.
//   zone -- an indoor/outdoor culling hint rather than weather, and one added after the client has
//       built its point cache renders nothing at all inside it. More misleading than useful.
//   freeze -- incompatible with this command by construction. It is a TOGGLE ("mFrozen = !mFrozen"),
//       so a rebuild would flip it every time; nothing resets it, not even clear or die; and it
//       would therefore end up in a different state on a late joiner than on everyone else, which is
//       the exact problem the rest of this design exists to avoid.
// DAJ_RP: [Weather] the weather entities are held to this same list now (RP_WeatherCommandFromText below):
// /entadd zyk_weather used to reach all nineteen.
static const zyk_weather_effect_t zyk_weather_effects[] = {
	{ "lightrain",		0,	ZYK_WFX_CLOUD	},
	{ "rain",			0,	ZYK_WFX_CLOUD	},
	{ "heavyrain",		0,	ZYK_WFX_CLOUD	},
	{ "acidrain",		0,	ZYK_WFX_CLOUD	},
	{ "snow",			0,	ZYK_WFX_CLOUD	},
	{ "sand",			0,	ZYK_WFX_CLOUD	},
	{ "fog",			0,	ZYK_WFX_CLOUD	},
	{ "heavyrainfog",	0,	ZYK_WFX_CLOUD	},
	{ "light_fog",		0,	ZYK_WFX_CLOUD	},
	{ "spacedust",		1,	ZYK_WFX_CLOUD	},
	{ "wind",			0,	ZYK_WFX_WIND	},
	{ "constantwind",	3,	ZYK_WFX_WIND	},
	{ "gustingwind",	0,	ZYK_WFX_WIND	}
};

#define ZYK_WEATHER_NUM_EFFECTS ((int)(sizeof(zyk_weather_effects) / sizeof(zyk_weather_effects[0])))

// zyk: StringIsInteger() rejects a leading minus, and a wind velocity needs one
static qboolean zyk_weather_is_number( const char *s )
{
	int i = 0;

	if (!s || !s[0])
		return qfalse;

	if (s[0] == '-')
		i = 1;

	if (!s[i])
		return qfalse;

	for (; s[i] != '\0'; i++)
	{
		if (!isdigit((unsigned char)s[i]))
			return qfalse;
	}

	return qtrue;
}

static const zyk_weather_effect_t *zyk_weather_find_effect( const char *name )
{
	int i = 0;

	for (i = 0; i < ZYK_WEATHER_NUM_EFFECTS; i++)
	{
		if (Q_stricmp(zyk_weather_effects[i].name, name) == 0)
			return &zyk_weather_effects[i];
	}

	return NULL;
}

// zyk: which kind of effect a finished command string is, so the renderer's own limits can be
// checked before a layer is accepted rather than letting it be dropped in silence
static zyk_weather_kind_t zyk_weather_command_kind( const char *command )
{
	char name[32];
	const zyk_weather_effect_t *effect = NULL;
	int i = 0;

	if (!command || command[0] != '*')
		return ZYK_WFX_UNKNOWN;

	command++;

	while (command[i] != '\0' && command[i] != ' ' && i < (int)(sizeof(name) - 1))
	{
		name[i] = command[i];
		i++;
	}
	name[i] = '\0';

	effect = zyk_weather_find_effect(name);

	return effect ? effect->kind : ZYK_WFX_UNKNOWN;
}

static void zyk_weather_count_kinds( int *clouds, int *winds )
{
	int i = 0;

	*clouds = 0;
	*winds = 0;

	if (level.zyk_weather_use_base == qtrue)
	{
		for (i = 0; i < level.zyk_weather_base_count; i++)
		{
			if (zyk_weather_command_kind(level.zyk_weather_base[i]) == ZYK_WFX_CLOUD)
				(*clouds)++;
			else if (zyk_weather_command_kind(level.zyk_weather_base[i]) == ZYK_WFX_WIND)
				(*winds)++;
		}
	}

	for (i = 0; i < level.zyk_weather_layer_count; i++)
	{
		if (zyk_weather_command_kind(level.zyk_weather_layers[i]) == ZYK_WFX_CLOUD)
			(*clouds)++;
		else if (zyk_weather_command_kind(level.zyk_weather_layers[i]) == ZYK_WFX_WIND)
			(*winds)++;
	}
}

static int zyk_weather_total_layers( void )
{
	int total = level.zyk_weather_layer_count;

	if (level.zyk_weather_use_base == qtrue)
		total += level.zyk_weather_base_count;

	return total;
}

// zyk: writes the whole block. Called for every change, never for part of one
static void zyk_weather_rebuild( void )
{
	int i = 0;
	int slot = 0;

	if (level.zyk_weather_slot == 0)
		return;

	level.zyk_weather_counter++;

	// zyk: the teardown, repeated until the layers are reached. Repeating it is free -- clear is
	// idempotent -- and it is what guarantees the layers below always start from a clean sky
	for (slot = 0; slot < (ZYK_WEATHER_SLOTS - zyk_weather_total_layers()); slot++)
	{
		trap->SetConfigstring(CS_EFFECTS + level.zyk_weather_slot + slot,
			va("*clear %d", level.zyk_weather_counter));
	}

	if (level.zyk_weather_use_base == qtrue)
	{
		for (i = 0; i < level.zyk_weather_base_count; i++, slot++)
		{
			trap->SetConfigstring(CS_EFFECTS + level.zyk_weather_slot + slot,
				va("%s %d", level.zyk_weather_base[i], level.zyk_weather_counter));
		}
	}

	// zyk: the counter goes on the end of every command. Every one of these the engine parses reads
	// a fixed number of tokens and discards the rest, so a trailing number changes the string
	// without changing what it does -- which is the point, because an unchanged configstring is
	// never re-sent and would leave connected players out of step with anyone joining later.
	for (i = 0; i < level.zyk_weather_layer_count; i++, slot++)
	{
		trap->SetConfigstring(CS_EFFECTS + level.zyk_weather_slot + slot,
			va("%s %d", level.zyk_weather_layers[i], level.zyk_weather_counter));
	}
}

static qboolean zyk_weather_claim_block( gentity_t *ent )
{
	char content[MAX_STRING_CHARS];
	int i = 0;
	int first_free = 0;
	int base_index = 0;
	// GalaxyRP fix: [Weather] counted here as the base is captured rather than read back out of
	// zyk_weather_count_kinds(), which reports the base only once level.zyk_weather_use_base is
	// set -- and that does not happen until this whole function has finished
	int base_clouds = 0;
	int base_winds = 0;

	if (level.zyk_weather_slot != 0)
		return qtrue;

	// GalaxyRP fix: [Weather] this used to read "G_EffectIndex drops the server when the table is
	// full". It no longer does -- it refuses and returns 0 -- but confirming the room up front is
	// still what this needs, because a block claimed half way is worse than one not claimed at all.
	// zyk: the room has to be confirmed before a single slot is taken
	for (first_free = 1; first_free < MAX_FX; first_free++)
	{
		trap->GetConfigstring(CS_EFFECTS + first_free, content, sizeof(content));

		if (!content[0])
			break;
	}

	if ((first_free + ZYK_WEATHER_SLOTS) > MAX_FX)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"^1This map has no room left for the weather system: %d effect slots free, %d needed.\n\"",
			MAX_FX - first_free, ZYK_WEATHER_SLOTS) );
		return qfalse;
	}

	// GalaxyRP fix: [Weather] the slot count above is not the only way this can run out. Registering
	// a name also spends gamestate bytes, and that budget is checked one name at a time -- fine for
	// a caller that wants one configstring, useless for one that needs ZYK_WEATHER_SLOTS and cannot
	// use a partial block. Without this the claim could take some of its slots, be refused partway,
	// and have no way to give back the ones it holds. ZYK_WEATHER_CMD_LENGTH bounds every string
	// zyk_weather_rebuild() can write into the block, so this is a real upper bound on the whole
	// block for the rest of the map, not just on the reservation strings.
	if (G_ConfigstringBytesAvailable(ZYK_WEATHER_SLOTS * ZYK_WEATHER_CMD_LENGTH) == qfalse)
	{
		trap->SendServerCommand( ent-g_entities, "print \"^1There is not enough room left in this map's gamestate for the weather system.\n\"" );
		return qfalse;
	}

	// zyk: remember the weather the map set up for itself, so /admweather default can put it back.
	// Only commands this file knows are kept: anything else would be re-run on every rebuild, and an
	// effect string the engine does not recognise prints its whole help list to the player's console.
	// GalaxyRP fix: [Weather] the scan no longer stops at the cap -- it keeps counting so a map with
	// more weather than the block can hold is REPORTED rather than quietly losing the excess on the
	// first /admweather default.
	for (i = 1; i < first_free; i++)
	{
		zyk_weather_kind_t kind = ZYK_WFX_UNKNOWN;

		trap->GetConfigstring(CS_EFFECTS + i, content, sizeof(content));

		kind = zyk_weather_command_kind(content);

		if (content[0] != '*' || kind == ZYK_WFX_UNKNOWN)
			continue;

		if (level.zyk_weather_base_count >= ZYK_WEATHER_MAX_BASE)
		{
			level.zyk_weather_base_truncated = qtrue;
			continue;
		}

		// GalaxyRP fix: [Weather] the count above used to be the only cap here, so a map whose
		// weather fit inside ZYK_WEATHER_MAX_BASE but asked for more particle clouds than the
		// renderer holds had every one of them captured. The renderer refuses the excess itself --
		// every push_back in tr_WorldEffects.cpp is guarded by full(), so nothing is corrupted and
		// the sky looks the same either way -- but it refuses them SILENTLY, which left the block
		// carrying a command that can never be shown, /admweather default listing a "map" line
		// nobody sees, and the status screen reporting a figure like "6 of 5 particle effects".
		// The admin's own layers were already held to these two limits by zyk_weather_add_layer();
		// the map's weather simply was not. Refusing it here costs nothing real and makes every
		// count downstream mean what it says. Recorded as truncation for the same reason the count
		// cap is: something the map shipped is not in the recipe.
		//
		// The wind arm cannot fire while ZYK_WEATHER_MAX_BASE (8) is below ZYK_WEATHER_MAX_WINDS
		// (10) -- capture stops long before eleven winds. It is written anyway so the two limits
		// are applied as a pair, and so raising the cap later cannot open the wind case silently.
		if ((kind == ZYK_WFX_CLOUD && base_clouds >= ZYK_WEATHER_MAX_CLOUDS)
			|| (kind == ZYK_WFX_WIND && base_winds >= ZYK_WEATHER_MAX_WINDS))
		{
			level.zyk_weather_base_truncated = qtrue;
			continue;
		}

		if (kind == ZYK_WFX_CLOUD)
			base_clouds++;
		else if (kind == ZYK_WFX_WIND)
			base_winds++;

		Q_strncpyz(level.zyk_weather_base[level.zyk_weather_base_count], content,
			sizeof(level.zyk_weather_base[0]));

		level.zyk_weather_base_count++;
	}

	// zyk: take the block. Each string has to be distinct or G_EffectIndex hands back the slot it
	// already gave out, and every one of them is a valid teardown, so the block starts out clean
	for (i = 0; i < ZYK_WEATHER_SLOTS; i++)
	{
		int index = G_EffectIndex(va("*clear %d", 900000 + i));

		if (i == 0)
			base_index = index;

		// GalaxyRP fix: [Weather] a refusal and a non-contiguous index are the same problem -- the
		// block cannot be used -- and they have to be handled together, before anything is given
		// back. They were not: the contiguity test ran first and the "base_index <= 0" test sat
		// AFTER the loop, where it could no longer help. A refusal on the very first slot left
		// base_index at 0, the second iteration then failed the contiguity test with base_index 0,
		// and the rollback blanked CS_EFFECTS+0 and CS_EFFECTS+1 -- the second of which is the
		// map's own first effect. That both destroys a live effect and puts a hole at index 1,
		// which is worse than it sounds: the client's replay loop (CG_RegisterSounds) STOPS at the
		// first empty slot, so every effect the map registered would be lost for everyone who
		// joined afterwards.
		//
		// Unreachable as the code stands -- the slot count and the byte budget are both checked
		// before the first G_EffectIndex, and the byte pre-check reserves ZYK_WEATHER_SLOTS *
		// ZYK_WEATHER_CMD_LENGTH against an actual spend of about a tenth of that -- but "cannot
		// happen" is exactly what the contiguity test already says about itself, and this is the
		// branch that runs when it does.
		if (index <= 0 || index != (base_index + i))
		{
			int undo = 0;

			// zyk: give back exactly what was taken and nothing else. That is the i contiguous
			// slots from base_index, plus this one if it was handed out at all. They are the
			// highest occupied slots in the table -- G_FindConfigstringIndex always returns the
			// first free slot, and these names are unique, so nothing can have been placed below
			// them -- which is what makes blanking them leave no hole for the replay loop.
			for (undo = 0; base_index > 0 && undo < i; undo++)
				trap->SetConfigstring(CS_EFFECTS + base_index + undo, "");

			if (index > 0)
				trap->SetConfigstring(CS_EFFECTS + index, "");

			level.zyk_weather_base_count = 0;
			level.zyk_weather_base_truncated = qfalse;
			base_index = 0;

			trap->SendServerCommand( ent-g_entities, "print \"^1Could not reserve a contiguous block of effect slots for the weather system.\n\"" );
			return qfalse;
		}
	}

	level.zyk_weather_slot = base_index;
	level.zyk_weather_use_base = qtrue;

	// GalaxyRP fix: [Weather] write the block out before returning. The slots were reserved by
	// registering "*clear" strings, and those are live the moment they are written -- so the claim
	// alone switches the map's weather off for everyone. That was invisible while the caller always
	// went on to rebuild, but the caller claims BEFORE it has validated its arguments, so a typo,
	// an out-of-range /admweather remove or a missing wind value left the sky wiped and told the
	// admin only that the effect name was wrong. Rebuilding here means the block starts out
	// reproducing the map's own weather, and a command that goes on to fail changes nothing.
	zyk_weather_rebuild();

	if (level.zyk_weather_base_truncated == qtrue)
	{
		// GalaxyRP fix: [Weather] report what was actually kept rather than ZYK_WEATHER_MAX_BASE.
		// The count is no longer the only thing that can truncate -- the cloud limit above can bite
		// first, and then fewer than the cap are kept and naming the cap would be a plain lie. The
		// log line carries the breakdown so the two causes can be told apart afterwards.
		trap->SendServerCommand( ent-g_entities, va("print \"^3This map sets up more weather than the system can hold, so only %d of its command(s) are kept. ^3/admweather default ^7will restore those.\n\"",
			level.zyk_weather_base_count) );
		G_LogPrintf("admweather: map weather truncated to %d command(s) (%d/%d particle effects, %d/%d winds, cap %d)\n",
			level.zyk_weather_base_count, base_clouds, ZYK_WEATHER_MAX_CLOUDS,
			base_winds, ZYK_WEATHER_MAX_WINDS, ZYK_WEATHER_MAX_BASE);
	}

	return qtrue;
}

static void zyk_weather_status( gentity_t *ent )
{
	int i = 0;
	int clouds = 0;
	int winds = 0;

	if (level.zyk_weather_slot == 0)
	{
		// GalaxyRP fix: [Weather] name "add" here. This screen and /admweather list are where an
		// admin looks, and neither of them used to mention the one verb that builds a recipe of
		// more than one layer -- it appeared only in /adminlist's per-command help, which nobody
		// reaches by accident.
		trap->SendServerCommand( ent-g_entities, "print \"\n^3Weather: ^7this map is showing whatever weather it was built with.\n^7Use ^3/admweather <effect> ^7to change it, ^3/admweather add <effect> ^7to layer a second one on top, or ^3/admweather list ^7to see the effects.\n\n\"" );
		return;
	}

	zyk_weather_count_kinds(&clouds, &winds);

	trap->SendServerCommand( ent-g_entities, va("print \"\n^3Weather ^7(%d of %d layers, %d of %d particle effects)\n\"",
		zyk_weather_total_layers(), ZYK_WEATHER_MAX_LAYERS, clouds, ZYK_WEATHER_MAX_CLOUDS) );

	if (zyk_weather_total_layers() == 0)
	{
		trap->SendServerCommand( ent-g_entities, "print \"^7  nothing\n\"" );
	}

	if (level.zyk_weather_use_base == qtrue)
	{
		for (i = 0; i < level.zyk_weather_base_count; i++)
		{ // zyk: +1 on the stored string skips the '*' the engine needs but an admin never types
			trap->SendServerCommand( ent-g_entities, va("print \"^2  map ^7- %s\n\"", level.zyk_weather_base[i] + 1) );
		}
	}

	for (i = 0; i < level.zyk_weather_layer_count; i++)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"^3  %d ^7- %s\n\"", i + 1, level.zyk_weather_layers[i] + 1) );
	}

	// GalaxyRP fix: [Weather] the layers are numbered above, which implies a command that takes one
	// of those numbers, but nothing here ever said what it was -- and "add" was unmentioned too, so
	// the only way to find out this command can do more than one layer was to read the source.
	trap->SendServerCommand( ent-g_entities, "print \"^7Use ^3/admweather add <effect> ^7to layer another on top, ^3/admweather remove <number> ^7to drop one, ^3/admweather default ^7for the map's own weather or ^3/admweather clear ^7for none.\n\"" );

	if (level.zyk_weather_base_truncated == qtrue)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"^3Note: ^7this map sets up more weather than the system can hold; only %d of its own command(s) are kept.\n\"",
			level.zyk_weather_base_count) );
	}

	trap->SendServerCommand( ent-g_entities, "print \"\n\"" );
}

static void zyk_weather_list( gentity_t *ent )
{
	char message[1024];
	int i = 0;

	strcpy(message, "");

	for (i = 0; i < ZYK_WEATHER_NUM_EFFECTS; i++)
	{
		const char *args = "";

		if (zyk_weather_effects[i].args == 1)
			args = " ^7<density>";
		else if (zyk_weather_effects[i].args == 3)
			args = " ^7<x> <y> <z>";

		Q_strcat(message, sizeof(message), va("^3%s%s\n", zyk_weather_effects[i].name, args));

		// GalaxyRP: [Weather] SV_SendServerCommand silently drops a formatted message over 1022
		// characters, so flush well before that rather than losing the tail of the list
		if ((int)strlen(message) > RP_LIST_FLUSH_AT)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"%s\"", message) );
			strcpy(message, "");
		}
	}

	trap->SendServerCommand( ent-g_entities, "print \"\n^3Weather effects\n\"" );

	if (message[0])
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s\"", message) );
	}

	// GalaxyRP fix: [Weather] this footer advertised a limit of N layers without ever naming the
	// command that makes one. Say how to build a recipe, here, where someone choosing an effect is
	// already looking.
	trap->SendServerCommand( ent-g_entities, va("print \"\n^7At most %d layers and %d particle effects at once. Wind values are %d to %d, density is %d to %d.\n^3/admweather <effect> ^7sets the weather and ^3/admweather add <effect> ^7layers another on top of it.\n\n\"",
		ZYK_WEATHER_MAX_LAYERS, ZYK_WEATHER_MAX_CLOUDS, -ZYK_WEATHER_WIND_LIMIT, ZYK_WEATHER_WIND_LIMIT,
		ZYK_WEATHER_DUST_MIN, ZYK_WEATHER_DUST_MAX) );
}

// zyk: turns an effect name and the admin's numbers into the exact string the engine parses.
// Nothing the admin typed is ever copied into it -- the name comes from the table above and the
// numbers are re-printed from ints -- so no separator, quote or control character can reach a
// configstring through this command.
static qboolean zyk_weather_build_command( gentity_t *ent, const zyk_weather_effect_t *effect, int first_arg, char *out, int out_size )
{
	char arg[MAX_STRING_CHARS];
	int values[3];
	int i = 0;

	if (effect->args == 0)
	{
		Com_sprintf(out, out_size, "*%s", effect->name);
		return qtrue;
	}

	if (trap->Argc() < (first_arg + effect->args))
	{
		if (effect->args == 1)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"^3/admweather %s <density>^7: how thick the dust is, %d to %d.\n\"",
				effect->name, ZYK_WEATHER_DUST_MIN, ZYK_WEATHER_DUST_MAX) );
		}
		else
		{
			trap->SendServerCommand( ent-g_entities, va("print \"^3/admweather %s <x> <y> <z>^7: the wind velocity, each value %d to %d.\n\"",
				effect->name, -ZYK_WEATHER_WIND_LIMIT, ZYK_WEATHER_WIND_LIMIT) );
		}

		return qfalse;
	}

	for (i = 0; i < effect->args; i++)
	{
		trap->Argv( first_arg + i, arg, sizeof( arg ) );

		if (zyk_weather_is_number(arg) == qfalse)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"^1'%s' is not a whole number.\n\"", arg) );
			return qfalse;
		}

		values[i] = atoi(arg);
	}

	if (effect->args == 1)
	{ // zyk: the renderer allocates this many particles with no bound of its own, and a negative
	  // count reaches a new[] with an enormous size -- which takes down every connected client, and
	  // every client that connects afterwards, because the string lives in a configstring
		values[0] = Com_Clampi(ZYK_WEATHER_DUST_MIN, ZYK_WEATHER_DUST_MAX, values[0]);

		Com_sprintf(out, out_size, "*%s %d", effect->name, values[0]);

		return qtrue;
	}

	for (i = 0; i < 3; i++)
		values[i] = Com_Clampi(-ZYK_WEATHER_WIND_LIMIT, ZYK_WEATHER_WIND_LIMIT, values[i]);

	// zyk: the engine's vector parser wants the parentheses as separate whitespace-delimited tokens
	// -- "( 100 0 0 )" parses and "(100 0 0)" does not -- so build it here and let the admin type
	// plain numbers
	Com_sprintf(out, out_size, "*%s ( %d %d %d )", effect->name, values[0], values[1], values[2]);

	return qtrue;
}

/*
==================
RP_WeatherCommandKind / RP_WeatherCommandFromText

DAJ_RP: [Weather] the weather entities' side of this table. zyk_weather's message, and an fx_runner's
fxFile starting with *, used to go to the players exactly as typed: anything at all reached the renderer's
command stream, where an unknown word prints its whole help list on every player's console (each time they
join, too), "clear" wipes the weather the entity cannot give back, "freeze" is a switch each player's game
flips on its own, "die" deletes the map's weather zones, and "spacedust -5" crashes them. Now the text has
to name one of the effects above -- the ones /admweather offers -- and the command is built from that name
and the numbers, re-printed from ints, as zyk_weather_build_command() builds /admweather's:
  spacedust [density]          1 to 10000, ZYK_WEATHER_DUST_DEFAULT without one, clamped
  constantwind <x> <y> <z>     each -10000 to 10000, clamped; "( x y z )", brackets and all, works too
Anything else is refused, and why says why. A leading * is skipped (an fx_runner's fxFile has one).
==================
*/
int RP_WeatherCommandKind( const char *command )
{
	switch ( zyk_weather_command_kind( command ) )
	{
	case ZYK_WFX_CLOUD:
		return RP_WEATHER_CLOUD;
	case ZYK_WFX_WIND:
		return RP_WEATHER_WIND;
	default:
		return RP_WEATHER_NONE;
	}
}

// DAJ_RP: [Weather] a whole number, as zyk_weather_is_number() takes it; one too long for an int is kept
// past every limit, so the clamp after it gives the limit rather than whatever the overflow made of it
static qboolean RP_WeatherNumber( const char *s, int *value )
{
	int i = 0;
	int v = 0;
	qboolean negative = qfalse;

	if (zyk_weather_is_number(s) == qfalse)
		return qfalse;

	if (s[0] == '-')
	{
		negative = qtrue;
		i = 1;
	}

	for (; s[i] != '\0'; i++)
	{
		if (v < 1000000)
			v = (v * 10) + (s[i] - '0');
	}

	*value = negative ? -v : v;
	return qtrue;
}

// DAJ_RP: [Weather] the words of the text, split at white space; a bracket is a word of its own, so
// "(100" and "( 100" read alike. Past RP_WEATHER_TOKENS words the rest is left unread and overflow set.
// A word longer than a token holds is cut short.
#define RP_WEATHER_TOKENS		8
#define RP_WEATHER_TOKEN_LENGTH	32
static int RP_WeatherTokens( const char *text, char tokens[RP_WEATHER_TOKENS][RP_WEATHER_TOKEN_LENGTH], qboolean *overflow )
{
	int n = 0;

	*overflow = qfalse;

	while (text && *text)
	{
		int len = 0;

		while (*text && (unsigned char)*text <= ' ')
			text++;

		if (!*text)
			break;

		if (n >= RP_WEATHER_TOKENS)
		{
			*overflow = qtrue;
			break;
		}

		if (*text == '(' || *text == ')')
		{
			tokens[n][0] = *text;
			tokens[n][1] = '\0';
			text++;
			n++;
			continue;
		}

		while (*text && (unsigned char)*text > ' ' && *text != '(' && *text != ')')
		{
			// zyk: what an admin typed goes back to him in the reason, inside a print command
			char c = (*text == '"' || *text == ';' || *text == '\\') ? '?' : *text;

			if (len < RP_WEATHER_TOKEN_LENGTH - 1)
				tokens[n][len++] = c;
			text++;
		}

		tokens[n][len] = '\0';
		n++;
	}

	return n;
}

qboolean RP_WeatherCommandFromText( const char *text, char *out, int outSize, char *why, int whySize )
{
	char tokens[RP_WEATHER_TOKENS][RP_WEATHER_TOKEN_LENGTH];
	const zyk_weather_effect_t *effect = NULL;
	qboolean overflow = qfalse;
	int values[3];
	int n = 0;
	int first = 1;
	int given = 0;
	int i = 0;

	if (outSize > 0)
		out[0] = '\0';
	if (whySize > 0)
		why[0] = '\0';

	if (text && text[0] == '*')
		text++;

	n = RP_WeatherTokens(text, tokens, &overflow);

	if (n == 0)
	{
		Q_strncpyz(why, "it names no weather", whySize);
		return qfalse;
	}

	if (Q_stricmp(tokens[0], "clear") == 0)
	{
		Q_strncpyz(why, "clear wipes all the weather, the map's own included, and nothing could bring it back; use /admweather clear", whySize);
		return qfalse;
	}

	if (Q_stricmp(tokens[0], "freeze") == 0)
	{
		Q_strncpyz(why, "freeze is a switch each player's game flips on its own, so players would see different things and nothing could switch it back", whySize);
		return qfalse;
	}

	if (Q_stricmp(tokens[0], "die") == 0)
	{
		Q_strncpyz(why, "die deletes the map's indoor and outdoor zones until the map reloads, so later weather would fall indoors too", whySize);
		return qfalse;
	}

	if (Q_stricmp(tokens[0], "zone") == 0)
	{
		Q_strncpyz(why, "zone is not weather, and a zone added once players have loaded the map does nothing", whySize);
		return qfalse;
	}

	if (Q_stricmp(tokens[0], "outsidepain") == 0 || Q_stricmp(tokens[0], "outsideshake") == 0)
	{
		Com_sprintf(why, whySize, "%s does nothing in multiplayer", tokens[0]);
		return qfalse;
	}

	effect = zyk_weather_find_effect(tokens[0]);

	if (!effect)
	{
		char list[256];

		list[0] = '\0';

		for (i = 0; i < ZYK_WEATHER_NUM_EFFECTS; i++)
		{
			const char *args = "";

			if (zyk_weather_effects[i].args == 1)
				args = " [density]";
			else if (zyk_weather_effects[i].args == 3)
				args = " <x> <y> <z>";

			Q_strcat(list, sizeof(list), va("%s%s%s", i ? ", " : "", zyk_weather_effects[i].name, args));
		}

		Com_sprintf(why, whySize, "%s is not a weather effect; use one of: %s", tokens[0], list);
		return qfalse;
	}

	if (overflow)
	{
		Com_sprintf(why, whySize, "too many values for %s", effect->name);
		return qfalse;
	}

	given = n - 1;

	// zyk: "( x y z )" -- the brackets the engine's own form has -- reads as the three numbers
	if (effect->args == 3 && given == 5 && strcmp(tokens[1], "(") == 0 && strcmp(tokens[5], ")") == 0)
	{
		first = 2;
		given = 3;
	}

	if (effect->args == 0)
	{
		if (given != 0)
		{
			Com_sprintf(why, whySize, "%s takes no values", effect->name);
			return qfalse;
		}

		Com_sprintf(out, outSize, "*%s", effect->name);
		return qtrue;
	}

	if (effect->args == 1)
	{
		values[0] = ZYK_WEATHER_DUST_DEFAULT;

		if (given > 1 || (given == 1 && RP_WeatherNumber(tokens[1], &values[0]) == qfalse))
		{
			Com_sprintf(why, whySize, "%s takes one whole number, its density from %d to %d: %s [density]",
				effect->name, ZYK_WEATHER_DUST_MIN, ZYK_WEATHER_DUST_MAX, effect->name);
			return qfalse;
		}

		values[0] = Com_Clampi(ZYK_WEATHER_DUST_MIN, ZYK_WEATHER_DUST_MAX, values[0]);
		Com_sprintf(out, outSize, "*%s %d", effect->name, values[0]);
		return qtrue;
	}

	for (i = 0; i < 3 && given == 3; i++)
	{
		if (RP_WeatherNumber(tokens[first + i], &values[i]) == qfalse)
			break;
	}

	if (given != 3 || i < 3)
	{
		Com_sprintf(why, whySize, "%s takes three whole numbers, the wind velocity, each from %d to %d: %s <x> <y> <z>",
			effect->name, -ZYK_WEATHER_WIND_LIMIT, ZYK_WEATHER_WIND_LIMIT, effect->name);
		return qfalse;
	}

	for (i = 0; i < 3; i++)
		values[i] = Com_Clampi(-ZYK_WEATHER_WIND_LIMIT, ZYK_WEATHER_WIND_LIMIT, values[i]);

	Com_sprintf(out, outSize, "*%s ( %d %d %d )", effect->name, values[0], values[1], values[2]);
	return qtrue;
}

static qboolean zyk_weather_add_layer( gentity_t *ent, const char *command )
{
	zyk_weather_kind_t kind = zyk_weather_command_kind(command);
	int clouds = 0;
	int winds = 0;

	if (zyk_weather_total_layers() >= ZYK_WEATHER_MAX_LAYERS)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"^1The weather already has %d layers. Remove one with ^3/admweather remove <number> ^1first.\n\"",
			ZYK_WEATHER_MAX_LAYERS) );
		return qfalse;
	}

	zyk_weather_count_kinds(&clouds, &winds);

	// zyk: the renderer keeps a fixed number of each and DROPS anything past it without a word, on
	// the client, where nobody sees the failure. Refuse here instead, where the admin is listening.
	if (kind == ZYK_WFX_CLOUD && clouds >= ZYK_WEATHER_MAX_CLOUDS)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"^1The game draws at most %d particle effects at once, and the weather already has that many.\n\"",
			ZYK_WEATHER_MAX_CLOUDS) );
		return qfalse;
	}

	if (kind == ZYK_WFX_WIND && winds >= ZYK_WEATHER_MAX_WINDS)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"^1The game holds at most %d winds at once, and the weather already has that many.\n\"",
			ZYK_WEATHER_MAX_WINDS) );
		return qfalse;
	}

	Q_strncpyz(level.zyk_weather_layers[level.zyk_weather_layer_count], command,
		sizeof(level.zyk_weather_layers[0]));

	level.zyk_weather_layer_count++;

	return qtrue;
}

void Cmd_AdmWeather_f( gentity_t *ent )
{
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	char command[ZYK_WEATHER_CMD_LENGTH];
	const zyk_weather_effect_t *effect = NULL;
	int first_arg = 2;

	if (!check_admin_command(ent, ADM_WEATHER, qtrue))
	{
		return;
	}

	if (trap->Argc() == 1)
	{
		zyk_weather_status(ent);
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if (Q_stricmp(arg1, "list") == 0)
	{
		zyk_weather_list(ent);
		return;
	}

	// zyk: everything past here changes the weather, and one change rewrites every slot in the
	// block -- that is a configstring broadcast each, so do not let it be held down
	if (level.zyk_weather_debounce_time > level.time)
	{
		trap->SendServerCommand( ent-g_entities, va("print \"^3Wait %d more second(s) before changing the weather again.\n\"",
			((level.zyk_weather_debounce_time - level.time) / 1000) + 1) );
		return;
	}

	if (zyk_weather_claim_block(ent) == qfalse)
	{
		return;
	}

	if (Q_stricmp(arg1, "default") == 0)
	{ // zyk: back to the weather the map was built with
		level.zyk_weather_use_base = qtrue;
		level.zyk_weather_layer_count = 0;
	}
	else if (Q_stricmp(arg1, "clear") == 0)
	{ // zyk: no weather at all, the map's own included
		level.zyk_weather_use_base = qfalse;
		level.zyk_weather_layer_count = 0;
	}
	else if (Q_stricmp(arg1, "remove") == 0)
	{
		int index = 0;
		int i = 0;

		if (trap->Argc() < 3)
		{
			trap->SendServerCommand( ent-g_entities, "print \"Usage: ^3/admweather remove <layer number>^7. Run ^3/admweather ^7to see the numbers.\n\"" );
			return;
		}

		trap->Argv( 2, arg2, sizeof( arg2 ) );

		if (StringIsInteger(arg2) == qfalse)
		{
			trap->SendServerCommand( ent-g_entities, "print \"You must give the number of the layer to remove.\n\"" );
			return;
		}

		index = atoi(arg2);

		if (index < 1 || index > level.zyk_weather_layer_count)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"^1There is no layer %d. Run ^3/admweather ^1to see the numbers.\n\"", index) );
			return;
		}

		for (i = index - 1; i < (level.zyk_weather_layer_count - 1); i++)
		{
			Q_strncpyz(level.zyk_weather_layers[i], level.zyk_weather_layers[i + 1],
				sizeof(level.zyk_weather_layers[0]));
		}

		level.zyk_weather_layer_count--;
	}
	else
	{
		qboolean adding = qfalse;
		int saved_count = level.zyk_weather_layer_count;

		if (Q_stricmp(arg1, "add") == 0)
		{
			if (trap->Argc() < 3)
			{
				trap->SendServerCommand( ent-g_entities, "print \"Usage: ^3/admweather add <effect>^7. Run ^3/admweather list ^7to see the effects.\n\"" );
				return;
			}

			adding = qtrue;
			trap->Argv( 2, arg1, sizeof( arg1 ) );
			first_arg = 3;
		}

		effect = zyk_weather_find_effect(arg1);

		if (!effect)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"^1There is no weather effect called '%s'. Run ^3/admweather list ^1to see them.\n\"", arg1) );
			return;
		}

		if (zyk_weather_build_command(ent, effect, first_arg, command, sizeof(command)) == qfalse)
		{
			return;
		}

		// zyk: without "add", the effect named replaces whatever the admin had set before -- the
		// map's own weather is only touched by "default" and "clear"
		if (adding == qfalse)
			level.zyk_weather_layer_count = 0;

		if (zyk_weather_add_layer(ent, command) == qfalse)
		{ // zyk: put back what was there, so a refused layer does not clear the sky as a side effect
			level.zyk_weather_layer_count = saved_count;
			return;
		}
	}

	zyk_weather_rebuild();

	level.zyk_weather_debounce_time = level.time + ZYK_WEATHER_DEBOUNCE;

	G_LogPrintf("admweather: %s set the weather to %d layer(s)%s\n", ent->client->pers.netname,
		zyk_weather_total_layers(), (level.zyk_weather_use_base == qtrue) ? " (map weather included)" : "");

	zyk_weather_status(ent);
}

/*
==================
[Phase] Alex: Converts the phase mode enum into a string for printing to the player.
==================
*/
const char *convert_phase_mode_to_text( int mode )
{
	switch ( mode )
	{
	case RP_PHASE_NONSOLID:
		return "walk-through";
	case RP_PHASE_GHOST:
		return "a Force ghost";
	case RP_PHASE_HOLO:
		return "a hologram";
	case RP_PHASE_HIDDEN:
		return "hidden";
	default:
		return "back to normal";
	}
}

void RP_ClearPhaseMode( gentity_t *ent )
{
	if ( !ent || !ent->client )
	{
		return;
	}

	if ( ent->client->pers.phase_mode != RP_PHASE_NONE )
	{
		ent->client->pers.phase_mode = RP_PHASE_NONE;
		ent->client->pers.phase_releasing = qtrue;
	}
}

static void RP_PhaseCommand( gentity_t *ent, int mode, const char *cmdName )
{
	char arg1[MAX_STRING_CHARS];
	// [Alex] Default to yourself, changes later from args.
	gentity_t *target = ent;
	int newMode;
	qboolean isSelfTargeted = (trap->Argc() == 1);
	qboolean isHoloOrGhost = (mode == RP_PHASE_HOLO || mode == RP_PHASE_GHOST);

	// [Alex] Don't check admin rights for self targeted holo or ghost commands.
	if (!(isHoloOrGhost && isSelfTargeted))
	{
		if (!check_admin_command(ent, ADM_PHASE, qtrue))
		{
			return;
		}
	}

	if ( trap->Argc() > 2 )
	{
		trap->SendServerCommand( ent-g_entities, va("print \"^1Command Usage: ^3/%s ^2<player name or ID (optional)>\n^7Without a name it applies to you. Run it again to turn it off.\n\"", cmdName) );
		return;
	}

	if ( trap->Argc() == 2 )
	{
		int client_id;

		trap->Argv( 1, arg1, sizeof( arg1 ) );
		client_id = ClientNumberFromString( ent, arg1, qfalse );

		if ( client_id == -1 )
		{
			return;
		}

		target = &g_entities[client_id];
	}

	if ( !target->inuse || !target->client )
	{
		return;
	}

	// same Admin Protect rule as /give, /scale and /teleport
	if ( target != ent && target->client->sess.amrpgmode > 0 &&
		(target->client->pers.bitvalue & (1 << ADM_ADMPROTECT)) && !(target->client->pers.player_settings & (1 << 13)) )
	{
		trap->SendServerCommand( ent-g_entities, "print \"Target player is adminprotected\n\"" );
		return;
	}

	if ( target->client->sess.sessionTeam == TEAM_SPECTATOR || target->client->tempSpectate >= level.time )
	{
		if ( target == ent )
			trap->SendServerCommand( ent-g_entities, "print \"You cannot use this while spectating.\n\"" );
		else
			trap->SendServerCommand( ent-g_entities, "print \"Target player is spectating.\n\"" );
		return;
	}

	if ( target->client->pers.phase_mode == mode )
	{ // the same command again: back to normal
		RP_ClearPhaseMode( target );
	}
	else
	{ // off, or another mode: switch straight to this one
		target->client->pers.phase_mode = mode;
		target->client->pers.phase_releasing = qfalse;
	}

	newMode = target->client->pers.phase_mode;

	if ( target == ent )
	{
		trap->SendServerCommand( ent-g_entities, va("print \"You are %s%s.\n\"", (newMode == RP_PHASE_NONE) ? "" : "now ", convert_phase_mode_to_text( newMode )) );
	}
	else
	{
		trap->SendServerCommand( ent-g_entities, va("print \"%s ^7is %s%s.\n\"", target->client->pers.netname, (newMode == RP_PHASE_NONE) ? "" : "now ", convert_phase_mode_to_text( newMode )) );
		trap->SendServerCommand( target-g_entities, va("print \"An admin made you %s.\n\"", (newMode == RP_PHASE_NONE) ? "normal again" : convert_phase_mode_to_text( newMode )) );
	}

	G_LogPrintf( "%s: %s ^7made %s ^7%s\n", cmdName, ent->client->pers.netname, target->client->pers.netname,
		(newMode == RP_PHASE_NONE) ? "normal" : convert_phase_mode_to_text( newMode ) );
}

// DAJ_RP: [Phase] was /admsolid
void Cmd_AdmWalkthrough_f( gentity_t *ent )
{
	RP_PhaseCommand( ent, RP_PHASE_NONSOLID, "admwalkthrough" );
}

void Cmd_AdmHidden_f( gentity_t *ent )
{
	RP_PhaseCommand( ent, RP_PHASE_HIDDEN, "admhidden" );
}

void Cmd_Ghost_f( gentity_t *ent )
{
	RP_PhaseCommand( ent, RP_PHASE_GHOST, "ghost" );
}

void Cmd_Holo_f( gentity_t *ent )
{
	RP_PhaseCommand( ent, RP_PHASE_HOLO, "holo" );
}

/*
==================
Cmd_Order_f
==================
*/
void Cmd_Order_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int i = 0;
   
	if ( trap->Argc() == 1) 
	{ 
		trap->SendServerCommand( ent-g_entities, "print \"^3/order follow: ^7npc will follow the leader\n^3/order guard: ^7npc will stand still shooting at everyone except the leader and his allies\n^3/order cover: ^7npc will follow the leader shooting at everyone except the leader and his allies\n\"" );
	}
	else
	{
		trap->Argv( 1, arg1, sizeof( arg1 ) );

		if (Q_stricmp(arg1, "follow") == 0)
		{
			for (i = MAX_CLIENTS; i < level.num_entities; i++)
			{
				gentity_t *this_ent = &g_entities[i];

				if (zyk_npc_can_take_orders(this_ent, ent))
				{
					this_ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_NPC_ORDER_GUARD);
					this_ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_NPC_ORDER_COVER);
					this_ent->NPC->tempBehavior = BS_FOLLOW_LEADER;
				}
			}
			trap->SendServerCommand( ent-g_entities, "print \"Order given.\n\"" );
		}
		else if (Q_stricmp(arg1, "guard") == 0)
		{
			for (i = MAX_CLIENTS; i < level.num_entities; i++)
			{
				gentity_t *this_ent = &g_entities[i];

				if (zyk_npc_can_take_orders(this_ent, ent))
				{
					this_ent->NPC->tempBehavior = BS_STAND_GUARD;
					this_ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_NPC_ORDER_COVER);
					this_ent->client->pers.player_statuses |= (1 << PLAYER_STATUS_NPC_ORDER_GUARD);
				}
			}
			trap->SendServerCommand( ent-g_entities, "print \"Order given.\n\"" );
		}
		else if (Q_stricmp(arg1, "cover") == 0)
		{
			for (i = MAX_CLIENTS; i < level.num_entities; i++)
			{
				gentity_t *this_ent = &g_entities[i];

				if (zyk_npc_can_take_orders(this_ent, ent))
				{
					this_ent->NPC->tempBehavior = BS_FOLLOW_LEADER;
					this_ent->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_NPC_ORDER_GUARD);
					this_ent->client->pers.player_statuses |= (1 << PLAYER_STATUS_NPC_ORDER_COVER);
				}
			}
			trap->SendServerCommand( ent-g_entities, "print \"Order given.\n\"" );
		}
		else
		{
			trap->SendServerCommand( ent-g_entities, "print \"Invalid npc order.\n\"" );
		}
	}
}

/*
==================
Cmd_Paralyze_f
==================
*/
void Cmd_Paralyze_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	int client_id = -1;
	int seconds = RP_PARALYZE_DEFAULT_SECONDS;
	gentity_t *target = NULL;

	if (!check_admin_command(ent, ADM_PARALYZE, qtrue))
	{
		return;
	}

	// GalaxyRP fix: [Death System] optional duration argument. Without it the target is paralyzed for
	// RP_PARALYZE_DEFAULT_SECONDS; with it, for the number of seconds given.
	if ( trap->Argc() != 2 && trap->Argc() != 3 )
	{
		trap->SendServerCommand( ent-g_entities, va("print \"^1Command Usage: ^3/paralyze ^2<player name or ID> <seconds (optional, %d-%d, default %d)>\n^7Use ^3/unparalyze ^7to release a paralyzed player early.\n\"",
			RP_PARALYZE_MIN_SECONDS, RP_PARALYZE_MAX_SECONDS, RP_PARALYZE_DEFAULT_SECONDS) );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	if ( trap->Argc() == 3 )
	{
		trap->Argv( 2, arg2, sizeof( arg2 ) );
		seconds = atoi( arg2 );

		// Rejected rather than clamped, matching /scale's out-of-range handling, so an admin who
		// mistypes the duration is told instead of silently getting a different one.
		if ( seconds < RP_PARALYZE_MIN_SECONDS || seconds > RP_PARALYZE_MAX_SECONDS )
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Duration must be between %d and %d seconds.\n\"",
				RP_PARALYZE_MIN_SECONDS, RP_PARALYZE_MAX_SECONDS) );
			return;
		}
	}

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	target = &g_entities[client_id];

	// GalaxyRP fix: [Death System] /paralyze only ever paralyzes now -- releasing is /unparalyze's job
	// (Cmd_Unparalyze_f below). This used to be a toggle, which made one command mean two opposite
	// things depending on state the admin could not see, so a repeated or mistyped /paralyze silently
	// pardoned the player it was meant to punish. Refusing here leaves the running countdown untouched.
	//
	// Note this refuses an ADMIN paralysis only. A player merely knocked down in combat is still
	// paralyzed by this command -- that state is replaced with an admin paralysis of the requested
	// length -- because refusing those would let a lucky knockdown shield someone from the command.
	// An admin can still free a merely-downed player with /helpup: can_player_get_up()'s ADM_GETUP
	// bypass allows it, and that bypass sits below the bit-26 refusal so it cannot lift a paralysis.
	if (G_PlayerIsAdminParalyzed(target))
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Target player %s ^7is already paralyzed. Use ^3/unparalyze ^7to release them.\n\"", target->client->pers.netname) );
		return;
	}

	// GalaxyRP fix: [Death System] refuse a dead or spectating target. The command had no
	// target-state checks at all: paralyzing a corpse or a spectator set a status bit that
	// survives respawn in pers, so they came back already paralyzed with no way to have earned it.
	if (target->health <= 0 || target->client->ps.stats[STAT_HEALTH] <= 0 ||
		(target->client->ps.eFlags & EF_DEAD) || target->client->ps.pm_type == PM_DEAD)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Target player must be alive.\n\"" );
		return;
	}

	if (target->client->sess.sessionTeam == TEAM_SPECTATOR || target->client->tempSpectate >= level.time)
	{
		trap->SendServerCommand( ent-g_entities, "print \"Target player is spectating.\n\"" );
		return;
	}

	// GalaxyRP fix: [Death System] get the target off any vehicle FIRST. The Death System refuses
	// to down a mounted player at all, because being downed never touches ps.m_iVehicleNum or a
	// hideRider vehicle's Ghost()-applied EF_NODRAW/SVF_NOCLIENT state -- only player_die()'s
	// rider-dismount block clears those, and it does not run for a player who is merely downed.
	// A rider paralyzed this way was left invisible, non-collidable and welded to the vehicle for
	// the whole duration. Ejecting here reaches the same clean state by a different route, so an
	// admin can still paralyze someone who happens to be riding something.
	if (target->client->NPC_class != CLASS_VEHICLE && target->client->ps.m_iVehicleNum)
	{
		gentity_t *veh = &g_entities[target->client->ps.m_iVehicleNum];

		if (veh->inuse && veh->client && veh->m_pVehicle)
		{
			veh->m_pVehicle->m_pVehicleInfo->Eject(veh->m_pVehicle, (bgEntity_t *)target, qtrue);
		}
	}

	// GalaxyRP fix: [Death System] this used to set the downed state up by hand and had drifted
	// from the Death System's own copy -- most importantly it never set downedTime, so the
	// countdown sat at 0, the target was told "You may get up by using /getup", and one command
	// released them. Shares RP_EnterDownedState() now, which sets the countdown, FL_NOTARGET and
	// the brief spawn-style invulnerability alongside the status bit.
	// Deliberately NOT paralyze_player(): that is the Death System's own wrapper and additionally
	// forces the target to RP_DOWNED_HEALTH health and counts a death against them. An admin paralysis leaves
	// health exactly as it was and is not a death.
	RP_EnterDownedState( target, seconds, qtrue );

	trap->SendServerCommand( ent-g_entities, va("print \"Paralyzed the target player %s ^7for %d seconds\n\"", target->client->pers.netname, seconds) );
	trap->SendServerCommand( client_id, va("print \"You were paralyzed by an admin for %d seconds\n\"", seconds) );
}

/*
==================
Cmd_Unparalyze_f

GalaxyRP fix: [Death System] the release half of /paralyze, split out into a command of its own.
/paralyze used to toggle, so the same words meant "punish" or "pardon" depending on state the admin
could not see; the two are separate commands now and each does exactly one thing.

Deliberately refuses anyone who is not ADMIN-paralyzed, a player knocked down in combat included.
A combat knockdown is served by the Death System's own countdown and ends with /helpup, or with the
player's own /getup once that countdown runs out -- an admin holding Instant Revive can already do it
through /helpup's ADM_GETUP bypass, so there is nothing for this command to add there.

Shares the Paralyze admin command rather than taking a permission bit of its own, the same way
/killother shares Kick: the admin who can apply a paralysis is the admin who should be able to lift
one, and a new bit would have to be granted separately to every admin who already has Paralyze.
==================
*/
void Cmd_Unparalyze_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;
	gentity_t *target = NULL;

	if (!check_admin_command(ent, ADM_PARALYZE, qtrue))
	{
		return;
	}

	if ( trap->Argc() != 2 )
	{
		trap->SendServerCommand( ent-g_entities, "print \"^1Command Usage: ^3/unparalyze ^2<player name or ID>\n^7Releases a player paralyzed by an admin.\n\"" );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	target = &g_entities[client_id];

	if (!G_PlayerIsAdminParalyzed(target))
	{
		trap->SendServerCommand( ent-g_entities, va("print \"Target player %s ^7is not paralyzed.\n\"", target->client->pers.netname) );
		return;
	}

	// Shared with the auto-release that fires when the countdown expires, so a paralysis always ends
	// the same way: both status bits cleared, the countdown zeroed, FL_NOTARGET released and the
	// get-up animation played.
	RP_ReleaseFromDownedState( target );

	trap->SendServerCommand( ent-g_entities, va("print \"Target player %s ^7is no longer paralyzed\n\"", target->client->pers.netname) );
	trap->SendServerCommand( client_id, "print \"You are no longer paralyzed\n\"" );
}

/*
==================
Cmd_Players_f
==================
*/
void Cmd_Players_f( gentity_t *ent ) {
	char content[MAX_STRING_CHARS];
	int i = 0;
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;
	int number_of_args = trap->Argc();

	strcpy(content,"ID - Name - IP - Type\n");

	if (!check_admin_command(ent, ADM_PLAYERS, qtrue))
	{
		return;
	}

	if (number_of_args == 1)
	{
		// GalaxyRP fix: [overflow] each row was appended with strcpy(content, va("%s...", content,
		// ...)). va() formats into a 32000-byte buffer and knows nothing about the destination, so
		// once the list passed content's size that strcpy wrote off the end of a stack array. This
		// is the widest of these: a row carries a 35-character netname AND a 47-byte IP field
		// (NET_ADDRSTRMAXLEN), so a full server builds about 3700 bytes into char
		// content[MAX_STRING_CHARS] and nine connected players is already enough. Rows are now
		// formatted into their own bounded buffer and appended with Q_strcat, and the message is
		// flushed and continued whenever the next row would not fit. Same text, same order.
		for (i = 0; i < level.maxclients; i++)
		{
			gentity_t *player = &g_entities[i];

			if (player && player->client && player->client->pers.connected != CON_DISCONNECTED)
			{
				char entry[MAX_NETNAME + NET_ADDRSTRMAXLEN + 64];

				Com_sprintf(entry, sizeof(entry), "%d - %s ^7- %s - ", player->s.number, player->client->pers.netname, player->client->sess.IP);

				// DAJ_RP: [Admin] every login is "RPG mode" (amrpgmode 2) and has been since 2022, so the old
				// "(rpg)" tag after (admin) or (logged) said nothing; a player who is not logged in had no tag
				if (player->client->sess.amrpgmode > 0)
				{
					if (player->client->pers.bitvalue != 0)
						Q_strcat(entry, sizeof(entry), "^3(admin)");
					else
						Q_strcat(entry, sizeof(entry), "^3(logged in)");
				}
				else
				{
					Q_strcat(entry, sizeof(entry), "^7(not logged in)");
				}

				Q_strcat(entry, sizeof(entry), "^7\n");

				if ((int)(strlen(content) + strlen(entry)) > RP_LIST_FLUSH_AT)
				{
					trap->SendServerCommand( ent-g_entities, va("print \"%s\"", content) );
					strcpy(content, "");
				}

				Q_strcat(content, sizeof(content), entry);
			}
		}

		trap->SendServerCommand( ent-g_entities, va("print \"%s\"", content) );
	}
	else
	{
		gentity_t *player_ent = NULL;

		trap->Argv( 1, arg1, sizeof( arg1 ) );

		client_id = ClientNumberFromString( ent, arg1, qfalse );

		if (client_id == -1)
		{
			return;
		}

		player_ent = &g_entities[client_id];

		if (player_ent->client->sess.amrpgmode != 2)
		{
			trap->SendServerCommand( ent-g_entities, va("print \"Player %s ^7is not logged in.\n\"", player_ent->client->pers.netname) );
			return;
		}

		if (number_of_args == 2)
		{
			list_rpg_info(player_ent, ent);
		}
		else
		{
			char arg2[MAX_STRING_CHARS];

			trap->Argv( 2, arg2, sizeof( arg2 ) );

			if (zyk_skill_category_name(arg2))
			{ // zyk: show skills of the player
			  // DAJ_RP fix: [Skills] no pool copy of the typed name -- see zyk_skill_category_name()
				zyk_list_player_skills(player_ent, ent, zyk_skill_category_name(arg2));
			}
			else
			{
				trap->SendServerCommand( ent-g_entities, "print \"Invalid option.\n\"" );
			}
		}
	}
}

/*
==================
Cmd_Ignore_f
==================
*/
void Cmd_Ignore_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	int client_id = -1;
	gentity_t *player = NULL;

	if (trap->Argc() == 1)
	{
		trap->SendServerCommand( ent-g_entities, "print \"You must pass a player name or ID as argument.\n\"" );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );

	client_id = ClientNumberFromString( ent, arg1, qfalse );

	if (client_id == -1)
	{
		return;
	}

	player = &g_entities[client_id];

	if (client_id < 31 && !(level.ignored_players[ent->s.number][0] & (1 << client_id)))
	{
		level.ignored_players[ent->s.number][0] |= (1 << client_id);
		trap->SendServerCommand( ent-g_entities, va("print \"Ignored player %s^7\n\"", player->client->pers.netname) );
	}
	else if (client_id >= 31 && !(level.ignored_players[ent->s.number][1] & (1 << (client_id - 31))))
	{
		level.ignored_players[ent->s.number][1] |= (1 << (client_id - 31));
		trap->SendServerCommand( ent-g_entities, va("print \"Ignored player %s^7\n\"", player->client->pers.netname) );
	}
	else if (client_id < 31 && level.ignored_players[ent->s.number][0] & (1 << client_id))
	{
		level.ignored_players[ent->s.number][0] &= ~(1 << client_id);
		trap->SendServerCommand( ent-g_entities, va("print \"No longer ignore player %s^7\n\"", player->client->pers.netname) );
	}
	else if (client_id >= 31 && level.ignored_players[ent->s.number][1] & (1 << (client_id - 31)))
	{
		level.ignored_players[ent->s.number][1] &= ~(1 << (client_id - 31));
		trap->SendServerCommand( ent-g_entities, va("print \"No longer ignore player %s^7\n\"", player->client->pers.netname) );
	}
}

/*
==================
Cmd_IgnoreList_f
==================
*/
void Cmd_IgnoreList_f(gentity_t *ent) {
	int i = 0;
	int listed = 0;
	char ignored_players[MAX_STRING_CHARS];
	gentity_t *player = NULL;

	strcpy(ignored_players, "");

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		// GalaxyRP fix: [Chat] the two arms of this test used to differ only in which word of the
		// bitfield they read, and each then repeated the same body. Folded into one condition.
		if (i < 31)
		{
			if (!(level.ignored_players[ent->s.number][0] & (1 << i)))
				continue;
		}
		else if (!(level.ignored_players[ent->s.number][1] & (1 << (i - 31))))
		{
			continue;
		}

		// GalaxyRP fix: [Chat] skip a slot nobody is in. This read pers.netname unconditionally,
		// so a bit left over from a player who had since disconnected printed whatever name was
		// stale in that slot. (ClientDisconnect now clears those bits, so this is a backstop.)
		player = &g_entities[i];
		if (!player->inuse || !player->client || player->client->pers.connected != CON_CONNECTED)
			continue;

		// GalaxyRP fix: [Chat] flush before this entry could carry the message past the limit.
		// The list was built into one MAX_STRING_CHARS buffer and sent in a single print, so a
		// player with enough ignored names simply got NOTHING -- SV_SendServerCommand drops an
		// oversized message whole and says nothing. 31 names at the 36-byte maximum is about 1200
		// characters, comfortably past the 1022 ceiling. Now it continues in another message
		// instead, the same way /list commands already splits its sections.
		if ((int)(strlen(ignored_players) + strlen(player->client->pers.netname) + 3) > RP_LIST_FLUSH_AT)
		{
			trap->SendServerCommand(ent->s.number, va("print \"%s\"", ignored_players));
			strcpy(ignored_players, "");
		}

		Q_strcat(ignored_players, sizeof(ignored_players), va("%s^7\n", player->client->pers.netname));
		listed++;
	}

	// GalaxyRP fix: [Chat] say so, rather than printing a bare empty line.
	if (listed == 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"^7You are not ignoring anyone.\n\"");
		return;
	}

	if (ignored_players[0] != '\0')
	{
		trap->SendServerCommand(ent->s.number, va("print \"%s\"", ignored_players));
	}
}

// GalaxyRP fix: these were only forward-declared further down in this file (right before
// Cmd_Saber_f), *after* update_saber() below already calls both of them. That left the compiler
// seeing an implicit (and wrong, int-returning) declaration at first use, which is a hard error
// under any strict/C++ compilation (and silently wrong even where it's only a warning) -- moving
// the declarations up here, before their first use, fixes it without touching the later ones.
extern qboolean duel_tournament_is_duelist(gentity_t *ent);
extern qboolean G_SaberModelSetup(gentity_t *ent);

// GalaxyRP: [Saber] shared by update_saber() (the "/saber <a> <b>" path), Cmd_UpdateSaber_f()
// (the argument-less "/updatesaber" path below), and -- since the UI's saber menu Apply button
// now auto-fires "/updatesaber" itself, see UI_UpdateSaberCvars() in ui_main.c -- the ordinary
// in-game Apply button too. All three are just server-side saber changes, so all three are
// subject to the same restriction checks. These used to only be active at zyk_allow_saber_command
// level 2; that cvar is gone (see the instant-saber-switching change above this one) and this mod
// only ever wanted level-2 behaviour, so the checks now run unconditionally for any command that
// can change a player's saber. Prints the matching refusal message and returns qfalse if the
// change is currently blocked. None of these checks affect the player's *next* respawn -- only
// the instant, no-death application: ClientSpawn()'s own userinfo-diff-and-apply (g_client.c)
// doesn't call this function at all, so a blocked instant switch still takes effect normally the
// next time the player actually spawns.
static qboolean saber_switch_allowed(gentity_t* ent)
{
	if (ent->client->ps.duelInProgress == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot use this command in private duels.\n\"");
		return qfalse;
	}

	if (level.duel_tournament_mode == 4 && duel_tournament_is_duelist(ent) == qtrue)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot use this command while duelling in Duel Tournament.\n\"");
		return qfalse;
	}

	// GalaxyRP fix: [Melee Battle] the battle was covered by neither this guard nor its force
	// counterpart. Scoped to melee_mode 2 -- the fight itself -- for the same reason the
	// tournament test above is scoped to mode 4: G_RunFrame() (g_main.c) calls
	// melee_battle_prepare() and sets mode 2 in one block, and melee_battle_winner() restores
	// before setting mode 3, so mode 2 is exactly the window in which a combatant is stripped.
	// Sign-up is deliberately left alone: nothing has been taken from anyone yet.
	if (level.melee_mode == 2 && level.melee_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot use this command while fighting in the Melee Battle.\n\"");
		return qfalse;
	}

	// GalaxyRP: [Saber] mirrors TaystJK's own restriction on its equivalent automatic instant-
	// saber-switch feature (its UI_UpdateSaberCvars() only auto-fires the switch when
	// g_gametype < GT_TEAM). Team-based and Siege rounds shouldn't let a player instantly swap
	// their saber type/hilt mid-round (e.g. single -> staff for a different reach/style matchup
	// against whoever they're currently fighting) without at least dying first -- GT_TEAM and
	// everything after it in the gametype_t enum (bg_public.h) is every team-based mode this
	// codebase has (Team, Siege, and, if ever enabled, CTF/CTY), matching the "team games go
	// after this" boundary already used the same way elsewhere (e.g. ClientUserinfoChanged()'s
	// teamInfo check in g_client.c).
	if (level.gametype >= GT_TEAM)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot instantly switch sabers in Team or Siege game types. It will apply on your next respawn.\n\"");
		return qfalse;
	}

	// GalaxyRP fix: [Guardian] a guardian_mode>0 guard blocking this command in boss battles used to be
	// here. guardian_mode is permanently 0 now, so it was unreachable.

	return qtrue;
}

// GalaxyRP: [Saber] shared tail of update_saber() -- re-reads this client's own userinfo, diffs
// its saber1/saber2 keys against pers.saber1/2, and applies any difference server-side
// (G_SetSaber/ClientUserinfoChanged/G_SaberModelSetup/saber-style reset), then syncs the client's
// own display. update_saber() calls this right after pushing saber1Model/saber2Model into
// userinfo itself (the "/saber <a> <b>" path, which names the sabers explicitly). Cmd_UpdateSaber_f
// below calls it directly with no such push, picking up whatever this client's own "saber1"/
// "saber2" userinfo cvars already hold -- adapted from JA++'s japp_allowSaberSwitch idea (see
// C:\Users\richa\TaystJK\japp-master\game\g_client.cpp's ClientSpawn userinfo-diff block, which is
// what actually performs this apply there, only on respawn), but exposed as its own always-on
// command instead of folding it into "/saber" with no arguments (which here still just prints the
// player's current saber names, unchanged) and instead of JA++'s separate enable cvar.
//
// syncClientAlways distinguishes those two callers, and only affects the client-cvar echo at the
// very end -- see the fix comment there. Pass qtrue when the SERVER decided the saber and has
// already written it into userinfo itself (update_saber(): /saber, and login/character restores),
// where the client's own cvars may be stale even when nothing here changes. Pass qfalse when the
// CLIENT proposed the change through its own userinfo (Cmd_UpdateSaber_f), where echoing on a
// no-op would overwrite a selection that simply hasn't arrived yet.
static void apply_saber_from_userinfo(gentity_t* ent, qboolean syncClientAlways)
{
	qboolean changedSaber = qfalse;
	// GalaxyRP fix: [Saber] see the syncClientAlways comment above -- set the moment the userinfo we
	// just read asks for something other than what the player currently has, whether or not
	// G_SetSaber() below then accepts it, so a rejected/corrected request is still echoed back.
	qboolean requestedChange = qfalse;
	char userinfo[MAX_INFO_STRING] = { 0 }, * saber = NULL, * key = NULL, * value = NULL;

	//first we want the userinfo so we can see if we should update this client's saber -rww
	trap->GetUserinfo(ent->s.number, userinfo, sizeof(userinfo));

	for (int i = 0; i < MAX_SABERS; i++)
	{
		saber = (i & 1) ? ent->client->pers.saber2 : ent->client->pers.saber1;
		value = Info_ValueForKey(userinfo, va("saber%i", i + 1));
		if (saber && value &&
			(Q_stricmp(value, saber) || !saber[0] || !ent->client->saber[0].model[0]))
		{ //doesn't match up (or our saber is BS), we want to try setting it
			requestedChange = qtrue;

			if (G_SetSaber(ent, i, value, qfalse))
				changedSaber = qtrue;

			//Well, we still want to say they changed then (it means this is siege and we have some overrides)
			else if (!saber[0] || !ent->client->saber[0].model[0])
				changedSaber = qtrue;
		}
	}

	if (changedSaber)
	{ //make sure our new info is sent out to all the other clients, and give us a valid stance
		if (!ClientUserinfoChanged(ent->s.number))
			return;

		//make sure the saber models are updated
		G_SaberModelSetup(ent);

		for (int i = 0; i < MAX_SABERS; i++)
		{
			saber = (i & 1) ? ent->client->pers.saber2 : ent->client->pers.saber1;
			key = va("saber%d", i + 1);
			value = Info_ValueForKey(userinfo, key);
			if (Q_stricmp(value, saber))
			{// they don't match up, force the user info
				Info_SetValueForKey(userinfo, key, saber);
				trap->SetUserinfo(ent->s.number, userinfo);
			}
		}

		if (ent->client->saber[0].model[0] && ent->client->saber[1].model[0])
		{ //dual
			ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = SS_DUAL;
		}
		else if ((ent->client->saber[0].saberFlags & SFL_TWO_HANDED))
		{ //staff
			ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = SS_STAFF;
		}
		else
		{
			ent->client->sess.saberLevel = Com_Clampi(SS_FAST, SS_STRONG, ent->client->sess.saberLevel);
			ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel;

			// limit our saber style to our force points allocated to saber offense
			if (level.gametype != GT_SIEGE && ent->client->ps.fd.saberAnimLevel > ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE])
				ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = ent->client->ps.fd.saberDrawAnimLevel = ent->client->sess.saberLevel = ent->client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE];
		}
		if (level.gametype != GT_SIEGE)
		{// let's just make sure the styles we chose are cool
			if (!WP_SaberStyleValidForSaber(&ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, ent->client->ps.fd.saberAnimLevel))
			{
				WP_UseFirstValidSaberStyle(&ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &ent->client->ps.fd.saberAnimLevel);
				ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = ent->client->ps.fd.saberAnimLevel;
			}
		}
	}

	// GalaxyRP fix: [Saber] push the final, authoritative saber1/saber2 down to the client's own
	// local cvars -- see CG_SaberUpdate_f in cg_servercmds.c for why this is needed (in short:
	// trap->SetUserinfo above only updates the SERVER's cached copy of this client's userinfo, it
	// never tells the client itself, so its own console/UI kept showing whatever it last sent,
	// including after a DB-driven login restore or a rejected/corrected invalid saber combo).
	//
	// GalaxyRP fix: [Saber] this used to be sent unconditionally on every path. That is right for
	// update_saber()'s callers (syncClientAlways), where the SERVER is the authority and has just
	// written userinfo itself -- a DB-driven login/character restore legitimately leaves
	// saber1/saber2 unchanged while the client's own cvars are stale, and only this echo can fix
	// them. It is actively harmful on the client-proposed /updatesaber path, though: there, "the
	// userinfo matches what the player already has" usually means the client's newly-picked saber
	// simply hasn't reached the server yet, and echoing the CURRENT saber back overwrites the pick
	// the player just made (CG_SaberUpdate_f sets those cvars unconditionally), destroying it before
	// it can ever be applied -- so the selection could not land on the next respawn either, and even
	// re-issuing the command by hand could never help. Echoing only when the userinfo actually asked
	// for something different (requestedChange, set whether or not G_SetSaber then accepted it, so a
	// rejection/correction is still pushed back) makes that case fail safe instead: an apply with
	// nothing to do now simply leaves the client's own cvars alone.
	if (changedSaber || requestedChange || syncClientAlways)
		trap->SendServerCommand(ent - g_entities, va("supdatesaber \"%s\" \"%s\"\n", ent->client->pers.saber1, ent->client->pers.saber2));
}

void update_saber(gentity_t* ent, char* saber1Model, char* saber2Model, int number_of_args) {
	char userinfo[MAX_INFO_STRING] = { 0 }, * value = NULL;
	// GalaxyRP fix: [Saber] this used to be a single "if (!saber_switch_allowed(ent)) return;" at the
	// top, which skipped BOTH the userinfo persist below AND the instant apply. That meant a switched-
	// to character's saved saber hilt (from select_player_character()/select_account_and_default_
	// character_data()), or a plain "/saber <a> <b>", was silently dropped with no lasting effect
	// whenever the player happened to be in a duel, Duel Tournament, or a GT_TEAM+ gametype -- and it
	// never self-corrected, because ClientSpawn()'s own userinfo-vs-pers.saber1/2 diff (g_client.c),
	// which *does* apply unconditionally on every respawn, had nothing to catch: userinfo was never
	// updated with the target saber in the first place. Splitting this into "persist unconditionally,
	// apply only if allowed" (mirroring TaystJK's own UI_UpdateSaberCvars(), which always Cvar_Set()s
	// saber1/saber2 first and only conditionally fires the instant-apply command) means a blocked
	// instant switch now still lands correctly on the player's next respawn, exactly as
	// saber_switch_allowed()'s own refusal messages already promise.
	qboolean instantApplyAllowed = saber_switch_allowed(ent);

	if (number_of_args == 1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Usage: /saber <saber1> <saber2>. Examples: /saber single_1, /saber single_1 single_1, /saber dual_1\n\"");
		return;
	}

	//first we want the userinfo so we can see if we should update this client's saber -rww
	trap->GetUserinfo(ent->s.number, userinfo, sizeof(userinfo));

	// GalaxyRP fix: [stability] both of these used to be wrapped in G_NewString() before being handed
	// to Info_SetValueForKey. That was pure waste with a real cost: Info_SetValueForKey copies the
	// value into the userinfo buffer (and trap->SetUserinfo copies again), so the allocations were
	// never needed -- but G_NewString allocates out of the game's 4MB permanent pool (g_mem.c), which
	// is never freed until the map changes and calls trap->Error(ERR_DROP) when exhausted. g_mem.c
	// warns in as many words that it must not be used from a client command, and this is reached
	// straight from /saber, whose arguments are player-supplied and unbounded in length. Repeated use
	// therefore consumed the pool with no way to reclaim it, ending in a dropped game VM. Pass the
	// caller's strings directly; nothing reads them again after these calls.
	// GalaxyRP fix: [Saber] normalise an empty hilt before it can reach userinfo. Info_SetValueForKey
	// REMOVES a key outright when handed an empty value (Info_RemoveKey, then an early return on
	// !strlen(value) -- q_shared.c), so an empty saber1Model/saber2Model did not store "" here, it
	// deleted the key from this client's userinfo entirely. Two things went wrong from there.
	//
	// First, the second saber. "" is not "none", so WP_SetSaber() (bg_saberLoad.c) does not take its
	// WP_RemoveSaber branch -- it falls through to the invalid-name path and parses DEFAULT_SABER
	// instead, handing the player a real second hilt they never asked for. That is the same
	// two-sabers-from-an-empty-value bug UI_UpdateSaberCvars() (ui_main.c) already normalises against
	// on the menu's Apply path; the "/saber <a> <b>" console path never did, and it is reachable with
	// a literal empty argument.
	//
	// Second, and worse since the refused-switch echo below was added: that echo reports what userinfo
	// actually holds, so a deleted key made it send an empty argument, CG_SaberUpdate_f (cg_servercmds.c)
	// cleared the client's own saber1/saber2 cvar, and Cvar_InfoString() then dropped the key from the
	// CLIENT's userinfo too. G_ValidateUserinfo() (g_client.c) requires exactly one saber1 and one
	// saber2 -- UIF( saberN, 1, 1 ), both enabled by g_userinfoValidate's default -- and returns
	// "saberN field not found" on the client's next resend, which ClientUserinfoChanged() answers with
	// trap->DropClient(). A player could kick themselves off the server by typing /saber <hilt> "".
	//
	// Slot 0's substitution is behaviour-neutral, only well-formed: WP_SetSaber() already routes any
	// name that fails WP_SaberValidForPlayerInMP() -- "" included -- through
	// WP_SaberParseParms(DEFAULT_SABER), so the player ended up with the default hilt either way. Slot
	// 1's is a real change, and the intended one: "none" is what every other path in this file means by
	// "no second saber", and it is what the player asking for an empty second hilt actually wants.
	if (saber1Model && saber1Model[0])
	{
		value = saber1Model;
	}
	else
	{
		value = DEFAULT_SABER;
	}

	Info_SetValueForKey(userinfo, "saber1", value);

	if (number_of_args == 2 || !saber2Model || !saber2Model[0])
	{
		value = "none";
	}
	else
	{
		value = saber2Model;
	}

	Info_SetValueForKey(userinfo, "saber2", value);

	trap->SetUserinfo(ent->s.number, userinfo);

	if (instantApplyAllowed)
	{
		apply_saber_from_userinfo(ent, qtrue);
	}
	else
	{
		// GalaxyRP fix: [Saber] the SetUserinfo above only updates the SERVER's cached copy of this
		// client's userinfo. On the allowed path apply_saber_from_userinfo() ends by pushing the
		// result down into the client's own saber1/saber2 cvars ("supdatesaber"); on this path
		// nothing did, so the two sides were left disagreeing -- and saber1/saber2 are CVAR_USERINFO
		// cvars whose authoritative copy lives on the CLIENT, not here.
		//
		// Any later change to ANY userinfo cvar makes CL_CheckUserinfo() (cl_main.cpp) resend the
		// client's WHOLE userinfo string, and SV_UpdateUserinfo_f (sv_client.cpp) replaces the
		// server's copy wholesale with it -- so the hilt stored just above was silently reverted
		// before the respawn that was supposed to apply it. ClientUserinfoChanged() cannot catch
		// that: its own G_SetSaber() calls are gated on pers.saber1/2 being invalid, i.e. first
		// connect only.
		//
		// On a character switch that revert was automatic rather than incidental. update_saber() is
		// called by select_player_character() and select_account_and_default_character_data() with
		// the incoming character's saved hilt, and Cmd_GalaxyRpUi_f's zykmod payload then sets that
		// client's own "name" and "model" cvars (CG_ZykMod, cg_servercmds.c) -- both CVAR_USERINFO,
		// both changing on essentially every switch, so the resend fired every time. The outgoing
		// character's hilt came back, ClientSpawn()'s userinfo-vs-pers.saber1/2 diff then found the
		// two already equal so it neither corrected the hilt nor sent its own echo (that send sits
		// inside its changedSaber branch), and the next save_account() wrote the wrong hilt into the
		// incoming character's row. Echoing here makes the resend carry the value the server chose,
		// turning a revert into a no-op.
		//
		// Read back OUT of userinfo rather than reusing saber1Model/value: Info_SetValueForKey
		// refuses a value containing '\\', '"' or ';' and leaves the previous one in place, so this
		// is what the server actually stored -- and it already accounts for the "none" substitution
		// above. Copied into locals because Info_ValueForKey returns one of only two rotating static
		// buffers and va() has a ring of its own; two live pointers inside one va() happens to work
		// today but is not a property worth depending on.
		//
		// This does not conflict with the requestedChange rule in apply_saber_from_userinfo(). That
		// guards the CLIENT-proposed /updatesaber path (syncClientAlways qfalse), where echoing on a
		// no-op overwrites a pick that simply has not arrived yet. Here the SERVER chose the value
		// and has just written it, which is exactly the syncClientAlways case.
		char pendingSaber1[MAX_QPATH] = { 0 }, pendingSaber2[MAX_QPATH] = { 0 };

		Q_strncpyz(pendingSaber1, Info_ValueForKey(userinfo, "saber1"), sizeof(pendingSaber1));
		Q_strncpyz(pendingSaber2, Info_ValueForKey(userinfo, "saber2"), sizeof(pendingSaber2));

		trap->SendServerCommand(ent - g_entities, va("supdatesaber \"%s\" \"%s\"\n", pendingSaber1, pendingSaber2));
	}
}

/*
==================
Cmd_Saber_f
==================
*/
extern qboolean duel_tournament_is_duelist(gentity_t *ent);
extern qboolean G_SaberModelSetup(gentity_t *ent);
void Cmd_Saber_f( gentity_t *ent ) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	int number_of_args = trap->Argc();

	trap->Argv(1, arg1, sizeof(arg1));
	trap->Argv(2, arg2, sizeof(arg2));

	update_saber(ent, arg1, arg2, number_of_args);
}

/*
==================
Cmd_UpdateSaber_f

GalaxyRP: [Saber] "/updatesaber" -- takes no arguments. Re-applies whatever saber hilt/type the
player already picked in the in-game saber menu and pressed Apply on (which writes to this
client's own "saber1"/"saber2" userinfo cvars, see UI_UpdateSaberCvars() in ui_main.c)
immediately, without needing a /kill or death to force a respawn -- ClientSpawn() in g_client.c
does this same userinfo diff-and-apply, but only ever runs it when the player is placed fresh in
the world. Adapted from JA++'s japp_allowSaberSwitch feature (see
C:\Users\richa\TaystJK\japp-master\game\g_client.cpp's ClientSpawn), kept as its own command
instead of folding it into "/saber" with no arguments (which stays a pure status query here) and
subject to the same restriction checks as any other saber change instead of a separate enable
cvar -- see saber_switch_allowed() above.

GalaxyRP fix: [Saber] the in-game saber menu's Apply button used to auto-fire this command to get
its instant apply. It no longer does -- it fires "saber <a> <b>" with the hilts as explicit
arguments instead, because an argument-less command always overtook the very userinfo cvars it
needed to read (see the fix comment in UI_UpdateSaberCvars(), ui_main.c). This command is
unchanged and still works on its own: typing it manually re-applies whatever saber1/saber2 this
client's cvars currently hold, which is still useful for forcing a re-apply without going back
through the menu. Because it is the CLIENT proposing the change here, it passes qfalse for
apply_saber_from_userinfo()'s syncClientAlways -- see that function's comment.
==================
*/
void Cmd_UpdateSaber_f( gentity_t *ent ) {
	if (!saber_switch_allowed(ent))
		return;

	apply_saber_from_userinfo(ent, qfalse);
}

// GalaxyRP: [Saber RGB] defined in bg_saberLoad.c (shared across game/cgame/ui) -- Cmd_SaberColor_f
// below needs the palette-name string for a saber's currently-selected mode.
const char *SaberColorToString( saber_colors_t color );

/*
==================
GalaxyRP: [Saber RGB] custom blade colours

pers.saberRGB[] is the authoritative copy of a player's custom blade colours (see g_local.h). Three
different things can change it -- the /sabercolor command below, an RGB-capable client's own saber
menu writing its cp_sbRGB1/cp_sbRGB2 userinfo cvars, and a database restore on login -- and after
any of them the same three things have to happen, which is what update_saber_colors() below does:

  1. the "c3"/"c4" clientinfo configstring keys have to be rebuilt so everyone else's cgame sees
     the new colour, and the character's database row has to be updated. Both fall out of a single
     ClientUserinfoChanged() call: it republishes the configstring from pers.saberRGB[], and it
     already saves the character on every userinfo change (see update_current_character_name_and_model).
  2. the player's own cp_sbRGB1/cp_sbRGB2 and color1/color2 cvars have to be corrected, because
     those live on the CLIENT and the server cannot write them directly. Exactly the same problem
     -- and the same fix -- as saber models and the "supdatesaber" command: without this, a colour
     the server chose (restored from the database, or set by /sabercolor) would be invisible to the
     client's own console and saber menu, and the menu would happily re-send its stale value on the
     next Apply and undo the change.
==================
*/
// GalaxyRP fix: [Saber RGB] patches the server's own CACHED copy of this client's userinfo so its
// cp_sbRGB1/cp_sbRGB2/color1/color2 keys match ent->client->pers.saberRGB[]/pers.saberColorMode[].
// This used to be inlined at the top of update_saber_colors() below (right before its own
// ClientUserinfoChanged() call, for exactly the reason explained in the comment that's still there);
// factored out so select_player_character() and select_account_and_default_character_data() (both in
// this file) can call it too, immediately after loading a character's saved colours and *before*
// calling update_saber() -- see the GalaxyRP fix: [Saber RGB] comment at each of those call sites for
// why that matters: without this pre-sync, update_saber() (and the respawn a character switch or
// login triggers, via ClientSpawn()'s own copy of this same adopt-from-userinfo logic in g_client.c)
// can end up re-adopting a stale cached colour from before this character/session and immediately
// re-saving it as if it were this character's own -- and the update_saber_colors() call those two
// functions already make *after* update_saber() only republishes whatever pers.saberRGB[] holds by
// then, so it was faithfully re-saving and re-publishing the corruption instead of catching it.
void G_SyncSaberColorUserinfo( gentity_t *ent ) {
	char userinfo[MAX_INFO_STRING] = { 0 };

	if ( !ent || !ent->client )
		return;

	trap->GetUserinfo( ent->s.number, userinfo, sizeof( userinfo ) );
	Info_SetValueForKey( userinfo, "cp_sbRGB1", va( "%i", ent->client->pers.saberRGB[0] ) );
	Info_SetValueForKey( userinfo, "cp_sbRGB2", va( "%i", ent->client->pers.saberRGB[1] ) );
	// GLX_U: [Cosmetics] with the hat and cape after the number, or ClientUserinfoChanged() would read
	// them back out of this cache as none
	Info_SetValueForKey( userinfo, "color1", va( "%i%s", ent->client->pers.saberColorMode[0], ent->client->pers.cosmetic[RP_COSMETIC_HAT] ) );
	Info_SetValueForKey( userinfo, "color2", va( "%i%s", ent->client->pers.saberColorMode[1], ent->client->pers.cosmetic[RP_COSMETIC_CAPE] ) );
	trap->SetUserinfo( ent->s.number, userinfo );
}

// GalaxyRP fix: [Account] same idea as G_SyncSaberColorUserinfo() just above, for a character's
// display name and player model: patches the server's own CACHED copy of this client's userinfo so
// its "name"/"model" keys already hold the values a caller is about to apply, before that caller
// calls (or triggers) ClientUserinfoChanged(). set_netname()/set_model() further down in this file
// already do this same patch for an organic, live name/model change -- this is the same fix, called
// by select_player_character() and select_account_and_default_character_data() right after reading
// a character's/account's saved NetName/ModelName off its database row and before update_saber()
// runs, for the same reason G_SyncSaberColorUserinfo() is: update_saber() (and ClientSpawn()'s own
// copy of this same adopt-from-userinfo logic on the respawn a character switch or login triggers)
// can call ClientUserinfoChanged(), which unconditionally re-derives pers.netname and the active
// model from whatever's cached here -- a previous character's, this account's own raw username, or a
// stale client echo -- and then immediately re-saves that stale value to the row just loaded, via
// update_current_character_name_and_model(). set_netname()/set_model() take their value as an
// explicit parameter rather than trusting pers, so they already correct this before the loading
// function returns (unlike update_saber_colors(), which used to just trust pers.saberRGB[] and
// faithfully re-publish whatever corruption was already there) -- but that only protects the
// synchronous character-load call itself; syncing the cache here too closes the same later window
// (a further respawn, or any other unrelated userinfo change) that G_SyncSaberColorUserinfo() closes
// for saber colour.
void G_SyncNameModelUserinfo( gentity_t *ent, const char *netName, const char *modelName ) {
	char userinfo[MAX_INFO_STRING] = { 0 };

	if ( !ent || !ent->client || !netName || !modelName )
		return;

	trap->GetUserinfo( ent->s.number, userinfo, sizeof( userinfo ) );
	Info_SetValueForKey( userinfo, "name", netName );
	Info_SetValueForKey( userinfo, "model", modelName );
	trap->SetUserinfo( ent->s.number, userinfo );
}

void update_saber_colors( gentity_t *ent ) {
	if ( !ent || !ent->client )
		return;

	// GalaxyRP: [Saber RGB] patch the server's own CACHED copy of this client's userinfo before
	// calling ClientUserinfoChanged() below -- mirroring the same fix already applied to saber
	// models (see update_saber() above). ClientUserinfoChanged() reads cp_sbRGB1/cp_sbRGB2/color1/
	// color2 back out of this cache, not out of the values just decided by our three callers, so
	// without this a stale cached value (left over from any earlier RGB use this session) could
	// immediately stomp a freshly-set or freshly-cleared value within this same call.
	G_SyncSaberColorUserinfo( ent );

	// Rebuild the clientinfo configstring (publishing c1..c4) and save the character.
	ClientUserinfoChanged( ent->s.number );

	// Push the authoritative values into the client's own cvars. Sent unconditionally, the same way
	// supdatesaber is, so a login restore that happens to leave the colour unchanged still leaves
	// the client's menu holding the right value rather than whatever it last sent. Four ints now
	// (mode1 rgb1 mode2 rgb2) instead of two, so the receiving end can restore color1/color2 and
	// the palette-name cvars too, not just the RGB payload -- see CG_SaberColorUpdate_f.
	// GLX_U: [Cosmetics] then the hat and cape, "-" for none, so the client's own color1/color2 carry
	// what this character wears (restored on login) rather than sending back what it had before. An
	// older client reads the first four and ignores these.
	trap->SendServerCommand( ent - g_entities, va( "supdatesabercolor %i %i %i %i %s %s\n",
		ent->client->pers.saberColorMode[0], ent->client->pers.saberRGB[0],
		ent->client->pers.saberColorMode[1], ent->client->pers.saberRGB[1],
		ent->client->pers.cosmetic[RP_COSMETIC_HAT][0] ? ent->client->pers.cosmetic[RP_COSMETIC_HAT] : "-",
		ent->client->pers.cosmetic[RP_COSMETIC_CAPE][0] ? ent->client->pers.cosmetic[RP_COSMETIC_CAPE] : "-" ) );
}

// Reads one 0-255 colour channel from a /sabercolor argument.
static int SaberColorArg( int argNum ) {
	char arg[MAX_TOKEN_CHARS];

	trap->Argv( argNum, arg, sizeof( arg ) );

	return Com_Clampi( 0, 255, atoi( arg ) );
}

// GalaxyRP: [Saber RGB] the ordinary (non-RGB, non-blade-type) palette entries /sabercolor accepts
// by name -- black included, since it has its own fixed shader rather than needing a custom colour.
static const struct { const char *name; saber_colors_t color; } saberOrdinaryColors[] = {
	{ "red",	SABER_RED },
	{ "orange",	SABER_ORANGE },
	{ "yellow",	SABER_YELLOW },
	{ "green",	SABER_GREEN },
	{ "blue",	SABER_BLUE },
	{ "purple",	SABER_PURPLE },
	{ "black",	SABER_BLACK },
};

static void SaberColorUsage( gentity_t *ent ) {
	trap->SendServerCommand( ent - g_entities,
		"print \"Usage: ^5/sabercolor <1|2> <red> <green> <blue>^7 (0-255 each)\n"
		"       ^5/sabercolor <1|2> <red|orange|yellow|green|blue|purple|black>^7\n\"" );
}

/*
==================
Cmd_SaberColor_f

  /sabercolor                          -- show both blades' current colours and the usage
  /sabercolor <1|2> <r> <g> <b>        -- set that blade's custom RGB colour (0-255 each channel)
  /sabercolor <1|2> <colour name>      -- set that blade to an ordinary palette colour, switching
                                           it out of RGB mode
==================
*/
void Cmd_SaberColor_f( gentity_t *ent ) {
	int argc = trap->Argc();
	int saberNum;
	char arg1[MAX_TOKEN_CHARS], arg2[MAX_TOKEN_CHARS];

	if ( argc == 1 ) {
		int i;

		SaberColorUsage( ent );

		for ( i = 0; i < 2; i++ ) {
			int mode = ent->client->pers.saberColorMode[i];

			if ( mode >= SABER_RGB && mode <= SABER_ELEC2 ) {
				trap->SendServerCommand( ent - g_entities, va( "print \"Saber %i is currently RGB %i %i %i.\n\"",
					i + 1,
					SABERRGB_R( ent->client->pers.saberRGB[i] ),
					SABERRGB_G( ent->client->pers.saberRGB[i] ),
					SABERRGB_B( ent->client->pers.saberRGB[i] ) ) );
			} else {
				trap->SendServerCommand( ent - g_entities, va( "print \"Saber %i is currently %s.\n\"",
					i + 1, SaberColorToString( (saber_colors_t)mode ) ) );
			}
		}
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	saberNum = atoi( arg1 );
	if ( saberNum != 1 && saberNum != 2 ) {
		SaberColorUsage( ent );
		return;
	}

	if ( argc == 5 ) {
		// /sabercolor <n> <r> <g> <b>
		int rgb = SABERRGB_PACK( SaberColorArg( 2 ), SaberColorArg( 3 ), SaberColorArg( 4 ) );

		ent->client->pers.saberRGB[saberNum - 1] = rgb;
		// GalaxyRP: [Saber RGB] entering an RGB triplet defaults the blade style to "classic" (6)
		// only if it wasn't already one of the five RGB-family values -- so re-picking the colour
		// on an already-blade-typed saber (via /saberblade) doesn't reset the style back to classic.
		if ( ent->client->pers.saberColorMode[saberNum - 1] < SABER_RGB ||
			ent->client->pers.saberColorMode[saberNum - 1] > SABER_ELEC2 )
			ent->client->pers.saberColorMode[saberNum - 1] = SABER_RGB;

		update_saber_colors( ent );

		trap->SendServerCommand( ent - g_entities, va( "print \"Saber %i colour set to %i %i %i.\n\"",
			saberNum, SABERRGB_R( rgb ), SABERRGB_G( rgb ), SABERRGB_B( rgb ) ) );
		return;
	}

	if ( argc == 3 ) {
		// /sabercolor <n> <name>
		int i;

		trap->Argv( 2, arg2, sizeof( arg2 ) );

		for ( i = 0; i < (int)ARRAY_LEN( saberOrdinaryColors ); i++ ) {
			if ( !Q_stricmp( arg2, saberOrdinaryColors[i].name ) ) {
				ent->client->pers.saberColorMode[saberNum - 1] = saberOrdinaryColors[i].color;
				// An ordinary named colour carries no custom RGB payload -- clear it so a later
				// /saberblade (which requires the mode to already be RGB-family) can't inherit a
				// stale colour left over from a previous RGB session on this saber.
				ent->client->pers.saberRGB[saberNum - 1] = 0;

				update_saber_colors( ent );

				trap->SendServerCommand( ent - g_entities, va( "print \"Saber %i colour set to %s.\n\"",
					saberNum, saberOrdinaryColors[i].name ) );
				return;
			}
		}

		trap->SendServerCommand( ent - g_entities, va(
			"print \"Unknown saber colour '%s'. Valid names: red, orange, yellow, green, blue, purple, black.\n\"",
			arg2 ) );
		return;
	}

	SaberColorUsage( ent );
}

/*
==================
Cmd_SaberBlade_f

  /saberblade <1|2> <bladeType>  -- pick a blade style among an already-RGB saber. Blade types:
                                     classic, flame1, electric1, flame2, electric2.
                                     The target saber must already be in RGB mode (set via
                                     /sabercolor <n> <r> <g> <b>) -- this only changes which of the
                                     five RGB-family shaders is used, not the custom colour itself.
==================
*/
static const struct { const char *name; saber_colors_t color; } saberBladeTypes[] = {
	{ "classic",	SABER_RGB },
	{ "flame1",		SABER_FLAME1 },
	{ "electric1",	SABER_ELEC1 },
	{ "flame2",		SABER_FLAME2 },
	{ "electric2",	SABER_ELEC2 },
};

void Cmd_SaberBlade_f( gentity_t *ent ) {
	int argc = trap->Argc();
	int saberNum, mode;
	char arg1[MAX_TOKEN_CHARS], arg2[MAX_TOKEN_CHARS];
	int i;

	if ( argc != 3 ) {
		trap->SendServerCommand( ent - g_entities,
			"print \"Usage: ^5/saberblade <1|2> <classic|flame1|electric1|flame2|electric2>^7\n\"" );
		return;
	}

	trap->Argv( 1, arg1, sizeof( arg1 ) );
	saberNum = atoi( arg1 );
	if ( saberNum != 1 && saberNum != 2 ) {
		trap->SendServerCommand( ent - g_entities,
			"print \"Usage: ^5/saberblade <1|2> ...^7 -- saber number must be 1 or 2.\n\"" );
		return;
	}

	trap->Argv( 2, arg2, sizeof( arg2 ) );
	mode = -1;
	for ( i = 0; i < (int)ARRAY_LEN( saberBladeTypes ); i++ ) {
		if ( !Q_stricmp( arg2, saberBladeTypes[i].name ) ) {
			mode = saberBladeTypes[i].color;
			break;
		}
	}
	if ( mode == -1 ) {
		trap->SendServerCommand( ent - g_entities,
			"print \"Unknown blade type. Valid types: classic, flame1, electric1, flame2, electric2.\n\"" );
		return;
	}

	if ( ent->client->pers.saberColorMode[saberNum - 1] < SABER_RGB ||
		ent->client->pers.saberColorMode[saberNum - 1] > SABER_ELEC2 ) {
		trap->SendServerCommand( ent - g_entities, va(
			"print \"Saber %i isn't set to a custom RGB colour yet -- use ^5/sabercolor %i <r> <g> <b>^7 first.\n\"",
			saberNum, saberNum ) );
		return;
	}

	ent->client->pers.saberColorMode[saberNum - 1] = mode;
	update_saber_colors( ent );

	trap->SendServerCommand( ent - g_entities, va( "print \"Saber %i blade style set to %s.\n\"",
		saberNum, arg2 ) );
}

// GalaxyRP (Alex): [Armor Skill] minimum time between successful Armor-skill blaster-deflect procs.
// Matches the existing Saber Defense "one block per ~200ms" throttle in g_missile.c, so a burst of
// blaster fire can't be deflected shot-for-shot once a player has high-level armor.
#define ARMOR_DEFLECT_COOLDOWN_MS 200

// GalaxyRP (Alex): [Armor Skill] revived zyk_can_deflect_shots() (previously a dead stub removed in
// an earlier cleanup pass -- see g_missile.c for where its call site was simplified away) to give
// level 1-5 Armor skill a percent chance to deflect an incoming blaster-type shot back at its
// shooter: 10% at level 1, 25% at level 2, 40% at level 3, 55% at level 4, 70% at level 5. Only one
// proc can succeed per ARMOR_DEFLECT_COOLDOWN_MS (see armor_deflect_timer in g_local.h), and the
// caller in g_missile.c only reaches this check for weapons that are already eligible for the
// existing FL_SHIELDED deflection path (Disruptor rifle shots are hitscan and never call this).
qboolean zyk_can_deflect_shots(gentity_t *ent)
{
	int armor_level = 0;
	int deflect_chance = 0;

	if (ent == NULL || ent->client == NULL || ent->client->sess.amrpgmode != 2)
	{
		return qfalse;
	}

	if (ent->health < 1)
	{ // zyk: a dead player cannot deflect anything
		return qfalse;
	}

	armor_level = ent->client->pers.skill_levels[56];

	if (armor_level < 1)
	{
		return qfalse;
	}

	if (ent->client->pers.armor_deflect_timer >= level.time)
	{ // zyk: still on cooldown from a previous deflect
		return qfalse;
	}

	deflect_chance = 10 + ((armor_level - 1) * 15); // zyk: 10/25/40/55/70% at levels 1-5

	if (Q_irand(0, 100) < deflect_chance)
	{
		ent->client->pers.armor_deflect_timer = level.time + ARMOR_DEFLECT_COOLDOWN_MS;
		return qtrue;
	}

	return qfalse;
}

// GalaxyRP fix: [Skills] removed zyk_can_use_unique() and Cmd_Unique_f() here -- the /unique command
// was already disabled (its console command table entry was commented out, further down this file),
// and its three Unique Abilities were the last remnant of a fully-removed 10-class system. Neither is
// coming back, so the dead command and its helper are gone rather than left disabled. The command
// table's own commented-out "unique" entry is removed below where the other command registrations are.


/*
==================
Cmd_DuelMode_f
==================
*/
extern void duel_tournament_end();
void Cmd_DuelMode_f(gentity_t *ent) {
	if (rp_allow_duel_tournament.integer != 1)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^3Duel Tournament: ^7this mode is not allowed in this server\n\""));
		return;
	}

	// GalaxyRP fix: [Death System] a downed player keeps RP_DOWNED_HEALTH health, so nothing else here stopped
	// them signing up while incapacitated -- they would just be teleported into the arena and left
	// lying there. Same explicit test /meleemode already carries.
	if (G_PlayerIsDowned(ent))
	{
		trap->SendServerCommand(ent->s.number, "print \"^1You cannot do this while you are downed.\n\"");
		return;
	}

	// GalaxyRP: [Duel Tournament] the tournament is for logged-out players, the same rule the
	// Melee Battle carries above and the one New Zyk Mod enforces here ("This tournament is for
	// non-rpg players"). Ours never had the check at all, so RPG characters could enter and
	// duel_tournament_prepare() would strip a loadout built from their account skills.
	//
	// Gated on "not already signed up" for the same reason as the melee one: /duelmode is a
	// toggle during signup, and refusing unconditionally would strand anyone already in it.
	if (ent->client->sess.amrpgmode == 2 && level.duel_players[ent->s.number] == -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"The Duel Tournament is only for players who are not logged in. Use ^3/logout ^7to join.\n\"");
		return;
	}

	if (level.duel_arena_loaded == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"There is no duel arena in this map\n\"");
		return;
	}

	if (level.melee_mode > 0 && level.melee_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a Melee Battle\n\"");
		return;
	}

	// GalaxyRP fix: [Duel Tournament] refuse while in a private duel. G_Damage (g_combat.c) makes
	// a private duellist mutually immune with everyone except their own opponent, and even with
	// them only through MOD_SABER, so bringing that state into a tournament means a duelist who
	// cannot be hurt and cannot hurt back. duel_tournament_validate_duelists() already kills such
	// a player when their match comes up -- this refuses at the door instead, so they keep the
	// match they would otherwise have lost without being told why.
	//
	// Gated on "not already signed up", like the RPG-mode guard above: /duelmode is a toggle, and
	// an unconditional refusal would strand anyone who ended up in both states.
	if (ent->client->ps.duelInProgress == qtrue && level.duel_players[ent->s.number] == -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a private duel\n\"");
		return;
	}

	if (level.duel_players[ent->s.number] == -1 && level.duel_tournament_mode > 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join the duel tournament now\n\"");
		return;
	}
	else if (level.duel_players[ent->s.number] == -1)
	{ // zyk: join the tournament
		if (level.duelists_quantity == MAX_CLIENTS)
		{
			trap->SendServerCommand(ent->s.number, va("print \"There are already %d duelists in tournament\n\"", MAX_CLIENTS));
			return;
		}
		else
		{
			if (level.duelists_quantity == 0)
			{ // zyk: first duelist joined. Put the globe model in the duel arena and set its origin point
				gentity_t *new_ent = NULL;

				// GalaxyRP fix: [Entity System] ask for the slot before taking it. G_Spawn() calls
				// trap->Error(ERR_DROP) when the entity table is full, and Com_Error promotes
				// ERR_DROP to ERR_FATAL under com_dedicated -- i.e. the server process exits. Every
				// other command that spawns (/spawnplatform, /spawndummy, the NPC spawners,
				// /entload) was given this guard already; this one was missed, so any player typing
				// /duelmode on a full map could drop the whole server.
				if (G_EntitySlotsAvailable(1) == qfalse)
				{
					trap->SendServerCommand(ent->s.number, "print \"^3Duel Tournament: ^7no free entity slots to build the arena right now\n\"");
					return;
				}

				new_ent = G_Spawn();

				zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
				zyk_set_entity_field(new_ent, "spawnflags", "0");
				zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)level.duel_tournament_origin[0], (int)level.duel_tournament_origin[1], (int)level.duel_tournament_origin[2]));
				zyk_set_entity_field(new_ent, "model", "models/map_objects/vjun/globe.md3");
				zyk_set_entity_field(new_ent, "targetname", "zyk_duel_globe");
				// GalaxyRP fix: [Leak] dropped a G_NewString() wrapper here. "zykmodelscale" is an
				// F_INT field, so zyk_set_entity_field() runs atoi() on the string and never keeps
				// the pointer -- the copy was pure waste out of the never-freed G_Alloc pool.
				zyk_set_entity_field(new_ent, "zykmodelscale", rp_duel_tournament_arena_scale.string);

				zyk_spawn_entity(new_ent);

				level.duel_tournament_model_id = new_ent->s.number;
			}

			// GalaxyRP fix: [Duel Tournament] start the signup countdown once, when signups open,
			// instead of restarting it on every join. Resetting it per join let a single player
			// hold the tournament in signup indefinitely by toggling /duelmode on and off faster
			// than the countdown (12 seconds by default), and each toggle also broadcast a chat
			// line to every client. Late joiners still get in -- they just do not push the start back.
			if (level.duel_tournament_mode != 1)
			{
				level.duel_tournament_timer = level.time + rp_duel_tournament_time_to_start.integer;
			}

			// DAJ_RP: [Admin] whatever an admin gave him with /give goes now, on signing up: he duels as a
			// plain logged-out player, his own force profile and nothing more (rp_give_lift()). After every
			// refusal above, so a refused sign-up keeps it.
			rp_give_lift(ent);
			RP_TrainingOff(ent, "for the Duel Tournament");	// DAJ_RP: [Training Saber] and his training saber, the same way

			level.duel_tournament_mode = 1;
			level.duel_players[ent->s.number] = 0;
			level.duel_players_hp[ent->s.number] = 0;
			
			level.duelists_quantity++;

			// zyk: removing allies from this player
			ent->client->sess.ally1 = 0;
			ent->client->sess.ally2 = 0;

			trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7%s ^7joined the tournament!\n\"", ent->client->pers.netname));
		}
	}
	else if (level.duel_tournament_mode == 1)
	{ // zyk: leave the tournament
		level.duel_players[ent->s.number] = -1;
		level.duelists_quantity--;

		if (level.duelists_quantity == 0)
		{ // zyk: everyone left the tournament. End it
			duel_tournament_end();
		}

		trap->SendServerCommand(-1, va("chat \"^3Duel Tournament: ^7%s ^7left the tournament!\n\"", ent->client->pers.netname));
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot leave the duel tournament after it started\n\"");
	}
}

/*
==================
Mini-game tables

DAJ_RP: [Mini-Games] the live views of the two mini-games: /dueltable for the Duel Tournament and
/meleetable for the Melee Battle. Both read the state the games already keep and change nothing.

Built a line at a time through RP_TableAdd(), which sends what it has and starts again whenever the
next line would pass RP_LIST_FLUSH_AT -- SV_SendServerCommand drops a whole message past 1022
characters, and a full 32-player list runs to roughly twice that.
==================
*/
static void RP_TableAdd( int clientNum, char *buf, size_t size, const char *line )
{
	if ( buf[0] && (int)(strlen(buf) + strlen(line)) > RP_LIST_FLUSH_AT )
	{
		trap->SendServerCommand( clientNum, va("print \"%s\"", buf) );
		buf[0] = '\0';
	}

	Q_strcat( buf, size, line );
}

static void RP_TableSend( int clientNum, char *buf )
{
	if ( buf[0] )
	{
		trap->SendServerCommand( clientNum, va("print \"%s\"", buf) );
		buf[0] = '\0';
	}
}

// zyk: time left on a mini-game timer as m:ss, never negative
static const char *RP_TableTimeLeft( int timer )
{
	int seconds = ( timer - level.time + 999 ) / 1000;

	if ( seconds < 0 )
		seconds = 0;

	return va( "%d:%02d", seconds / 60, seconds % 60 );
}

// zyk: a duelist's name for the status line, or a placeholder when the slot holds nobody
static const char *RP_DuelistName( int id )
{
	if ( id >= 0 && id < MAX_CLIENTS && g_entities[id].client && g_entities[id].client->pers.connected == CON_CONNECTED )
		return g_entities[id].client->pers.netname;

	return "^3(left)";
}

/*
==================
duel_show_table

The Duel Tournament standings: a status line, the match count, then every duelist ranked by points,
with the health and shield they kept across their duels breaking a tie and then the player slot.
ent NULL sends it to everyone, which is what the tournament does after each match.
==================
*/
void duel_show_table(gentity_t *ent)
{
	int i = 0;
	int j = 0;
	int array_length = 0;
	char content[1024];
	char line[(MAX_NETNAME * 2) + 96];
	int sorted_players[MAX_CLIENTS]; // zyk: used to show score of players by ordering from the highest score to lowest
	int show_table_id = -1;
	const char *paused = level.duel_tournament_paused ? " ^1(paused)" : "";

	if (ent)
	{
		show_table_id = ent->s.number;
	}

	if (level.duel_tournament_mode == 0)
	{
		trap->SendServerCommand(show_table_id, "print \"There is no duel tournament now\n\"");
		return;
	}

	content[0] = '\0';

	// DAJ_RP: [Mini-Games] what the tournament is doing right now, so the table can be read live
	if (level.duel_tournament_mode == 1)
	{
		Com_sprintf(line, sizeof(line), "\n^3Duel Tournament: ^7signing up, starts in ^3%s^7. Duelists: ^3%d%s\n",
			RP_TableTimeLeft(level.duel_tournament_timer), level.duelists_quantity, paused);
	}
	else if (level.duel_tournament_mode == 3)
	{
		Com_sprintf(line, sizeof(line), "\n^3Duel Tournament: ^7next match: %s ^7vs %s%s\n",
			RP_DuelistName(level.duelist_1_id), RP_DuelistName(level.duelist_2_id), paused);
	}
	else if (level.duel_tournament_mode == 4)
	{
		Com_sprintf(line, sizeof(line), "\n^3Duel Tournament: ^7duel: %s ^7vs %s^7, ^3%s ^7left%s\n",
			RP_DuelistName(level.duelist_1_id), RP_DuelistName(level.duelist_2_id),
			RP_TableTimeLeft(level.duel_tournament_timer), paused);
	}
	else
	{
		Com_sprintf(line, sizeof(line), "\n^3Duel Tournament: ^7between matches%s\n", paused);
	}
	RP_TableAdd(show_table_id, content, sizeof(content), line);

	// zyk: put the number of matches
	Com_sprintf(line, sizeof(line), "^7Total: %d\nPlayed: %d\n\n", level.duel_matches_quantity, level.duel_matches_done);
	RP_TableAdd(show_table_id, content, sizeof(content), line);

	for (i = 0; i < MAX_CLIENTS; i++)
	{ // zyk: adding players to sorted_players and calculating the array length
		if (level.duel_players[i] != -1 && g_entities[i].client)
		{
			sorted_players[array_length] = i;
			array_length++;
		}
	}

	for (i = 1; i < array_length; i++)
	{ // zyk: highest score first, then the most hp and shield kept, then the lowest player id
		const int this_id = sorted_players[i];

		for (j = i; j > 0; j--)
		{
			const int prev_id = sorted_players[j - 1];

			if (level.duel_players[this_id] > level.duel_players[prev_id] ||
				(level.duel_players[this_id] == level.duel_players[prev_id] && level.duel_players_hp[this_id] > level.duel_players_hp[prev_id]) ||
				(level.duel_players[this_id] == level.duel_players[prev_id] && level.duel_players_hp[this_id] == level.duel_players_hp[prev_id] && this_id < prev_id))
			{
				sorted_players[j] = prev_id;
			}
			else
			{
				break;
			}
		}

		sorted_players[j] = this_id;
	}

	for (i = 0; i < array_length; i++)
	{
		const int id = sorted_players[i];

		Com_sprintf(line, sizeof(line), "^7%d. %s^7: ^3%d pts  ^1%d hp\n", i + 1, g_entities[id].client->pers.netname, level.duel_players[id], level.duel_players_hp[id]);
		RP_TableAdd(show_table_id, content, sizeof(content), line);
	}

	RP_TableAdd(show_table_id, content, sizeof(content), "\n");
	RP_TableSend(show_table_id, content);
}

extern qboolean duel_tournament_valid_duelist(gentity_t *ent);

/*
==================
Cmd_DuelTable_f

/dueltable [page]: page 1 (and no page at all) is the standings, from page 2 on the match list, eight
matches a page.
==================
*/
void Cmd_DuelTable_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];
	int page = 1;
	int i = 0;
	const int results_per_page = 8;
	int last_page = 1;
	char content[MAX_STRING_CHARS];

	content[0] = '\0';

	// DAJ_RP: [Mini-Games] no page number shows page 1, the standings, rather than an error
	if (trap->Argc() > 1)
	{
		trap->Argv(1, arg1, sizeof(arg1));

		page = atoi(arg1);

		if (page < 1)
		{
			trap->SendServerCommand(ent->s.number, "print \"Use ^3/dueltable [page]^7: page 1 is the standings, page 2 and up the matches.\n\"");
			return;
		}
	}

	if (page == 1)
	{
		duel_show_table(ent);
		return;
	}

	// DAJ_RP: [Mini-Games] every page past the first used to print a blank page whenever it had no
	// matches to show -- with no tournament on, during sign-up, and past the last match. Each of
	// those now says which.
	if (level.duel_tournament_mode == 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"There is no duel tournament now\n\"");
		return;
	}

	if (level.duel_matches_quantity <= 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"The match list is drawn up when the tournament begins.\n\"");
		return;
	}

	last_page = 1 + (level.duel_matches_quantity + results_per_page - 1) / results_per_page;

	if (page > last_page)
	{
		if (last_page == 2)
			trap->SendServerCommand(ent->s.number, va("print \"No page %d. The matches are on page 2.\n\"", page));
		else
			trap->SendServerCommand(ent->s.number, va("print \"No page %d. The matches are on pages 2 to %d.\n\"", page, last_page));
		return;
	}

	Q_strcat(content, sizeof(content), va("\n^3Matches, page %d of %d\n", page - 1, last_page - 1));

	// zyk: makes i start from the first result of the correct page
	for (i = results_per_page * (page - 2); i < results_per_page * (page - 1) && i < level.duel_matches_quantity; i++)
	{
		const int first_id = level.duel_matches[i][0];
		const int second_id = level.duel_matches[i][1];
		gentity_t *first_duelist = (first_id >= 0 && first_id < MAX_CLIENTS) ? &g_entities[first_id] : NULL;
		gentity_t *second_duelist = (second_id >= 0 && second_id < MAX_CLIENTS) ? &g_entities[second_id] : NULL;
		char first_name[MAX_NETNAME + 8];
		char second_name[MAX_NETNAME + 8];
		char line[(MAX_NETNAME * 2) + 96];

		Q_strncpyz(first_name, "^3Left Tournament", sizeof(first_name));
		Q_strncpyz(second_name, "^3Left Tournament", sizeof(second_name));

		if (first_duelist && duel_tournament_valid_duelist(first_duelist) == qtrue)
		{ // zyk: first duelist still in Tournament
			Com_sprintf(first_name, sizeof(first_name), "^7%s", first_duelist->client->pers.netname);
		}

		if (second_duelist && duel_tournament_valid_duelist(second_duelist) == qtrue)
		{ // zyk: second duelist still in Tournament
			Com_sprintf(second_name, sizeof(second_name), "^7%s", second_duelist->client->pers.netname);
		}

		if (i < level.duel_matches_done)
		{ // zyk: this match was already played in this tournament cycle
			Com_sprintf(line, sizeof(line), "%s ^3%d x %d %s\n", first_name, level.duel_matches[i][2], level.duel_matches[i][3], second_name);
		}
		else if (i == level.duel_matches_done)
		{ // zyk: current match, with the time left while the duel is on
			Com_sprintf(line, sizeof(line), "%s ^1%d x %d %s%s\n", first_name, level.duel_matches[i][2], level.duel_matches[i][3], second_name,
				level.duel_tournament_mode == 4 ? va("   ^3Time: ^7%s", RP_TableTimeLeft(level.duel_tournament_timer)) : "");
		}
		else
		{ // zyk: match not played yet in this tournament cycle
			Com_sprintf(line, sizeof(line), "%s ^7%d x %d %s\n", first_name, level.duel_matches[i][2], level.duel_matches[i][3], second_name);
		}

		Q_strcat(content, sizeof(content), line);
	}

	trap->SendServerCommand(ent->s.number, va("print \"%s\n\"", content));
}

/*
==================
Cmd_MeleeTable_f

DAJ_RP: [Mini-Games] /meleetable: the Melee Battle as it stands -- its phase and time, then every
fighter still in, ranked by kills and then by player slot. A fighter who is knocked out leaves the
roster (melee_players[] goes back to -1), so only those still in can be listed.
==================
*/
void Cmd_MeleeTable_f(gentity_t *ent) {
	char content[1024];
	char line[MAX_NETNAME + 64];
	int sorted[MAX_CLIENTS];
	int count = 0;
	int i, j;

	if (level.melee_mode == 0)
	{
		trap->SendServerCommand(ent->s.number, "print \"There is no Melee Battle now\n\"");
		return;
	}

	content[0] = '\0';

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (level.melee_players[i] != -1 && g_entities[i].client && g_entities[i].client->pers.connected == CON_CONNECTED)
		{
			sorted[count++] = i;
		}
	}

	for (i = 1; i < count; i++)
	{ // zyk: most kills first, then the lowest player id
		const int this_id = sorted[i];

		for (j = i; j > 0; j--)
		{
			const int prev_id = sorted[j - 1];

			if (level.melee_players[this_id] > level.melee_players[prev_id] ||
				(level.melee_players[this_id] == level.melee_players[prev_id] && this_id < prev_id))
			{
				sorted[j] = prev_id;
			}
			else
			{
				break;
			}
		}

		sorted[j] = this_id;
	}

	if (level.melee_mode == 1)
	{
		Com_sprintf(line, sizeof(line), "\n^3Melee Battle: ^7signing up, starts in ^3%s^7. Fighters: ^3%d\n\n",
			RP_TableTimeLeft(level.melee_mode_timer), count);
	}
	else if (level.melee_mode == 2)
	{
		Com_sprintf(line, sizeof(line), "\n^3Melee Battle: ^7in progress, ^3%s ^7left. Fighters left: ^3%d\n\n",
			RP_TableTimeLeft(level.melee_mode_timer), count);
	}
	else
	{
		Com_sprintf(line, sizeof(line), "\n^3Melee Battle: ^7battle over\n\n");
	}
	RP_TableAdd(ent->s.number, content, sizeof(content), line);

	for (i = 0; i < count; i++)
	{
		const int id = sorted[i];

		if (level.melee_mode == 1)
		{ // zyk: nobody has fought yet, so there are no kills to show
			Com_sprintf(line, sizeof(line), "^7%d. %s\n", i + 1, g_entities[id].client->pers.netname);
		}
		else if (level.melee_mode == 3)
		{
			Com_sprintf(line, sizeof(line), "^7Winner: %s^7: ^3%d kills\n", g_entities[id].client->pers.netname, level.melee_players[id]);
		}
		else
		{
			Com_sprintf(line, sizeof(line), "^7%d. %s^7: ^3%d kills\n", i + 1, g_entities[id].client->pers.netname, level.melee_players[id]);
		}

		RP_TableAdd(ent->s.number, content, sizeof(content), line);
	}

	RP_TableAdd(ent->s.number, content, sizeof(content), "\n");
	RP_TableSend(ent->s.number, content);
}

/*
==================
Cmd_DuelArena_f
==================
*/
void Cmd_DuelArena_f(gentity_t *ent) {
	FILE *duel_arena_file;
	char content[1024];
	char zyk_info[MAX_INFO_STRING] = { 0 };
	char zyk_mapname[128] = { 0 };

	// zyk: getting the map name
	trap->GetServerinfo(zyk_info, sizeof(zyk_info));
	Q_strncpyz(zyk_mapname, Info_ValueForKey(zyk_info, "mapname"), sizeof(zyk_mapname));

	strcpy(content, "");

	if (!check_admin_command(ent, ADM_DUELARENA, qtrue))
	{
		return;
	}

	// GalaxyRP fix: [Duel Tournament] refuse while the mode is switched off. Cmd_DuelMode_f checks
	// this same cvar as its first guard, so no arena entity could ever be spawned with tournaments
	// disabled -- but nothing stopped an admin creating, or worse DELETING, a map's saved arena
	// origin for a mode that is off. This command is a toggle keyed on whether origin.txt exists:
	// run on a map that already has an arena it removes it, with no confirmation and no mention of
	// the cvar, so an admin merely checking whether an arena was set would silently destroy it.
	//
	// Placed after the admin check rather than before it, so a non-admin still gets the permission
	// refusal they get today instead of learning the server's cvar state; and before the
	// zyk_create_dir() below, so a disabled mode touches no files or directories at all.
	if (rp_allow_duel_tournament.integer != 1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^3Duel Tournament: ^7this mode is not allowed in this server\n\"");
		return;
	}

	// GalaxyRP fix: [Duel Tournament] refuse while a tournament is running. The add branch below
	// does VectorCopy(ent->client->ps.origin, level.duel_tournament_origin) -- it re-points the
	// arena at whoever typed the command, while the globe entity stays where it was spawned. In
	// mode 4 the arena bounds in G_RunFrame() (g_main.c) kill, by suicide, any duelist FURTHER
	// from that origin than the arena radius and any non-duelist CLOSER to it than the radius. So
	// moving the origin kills both duelists at once (they are suddenly outside) and everyone
	// standing near the admin (they are suddenly inside) -- the admin included.
	//
	// Reachable in two commands even where an arena is already set: one /duelarena to remove the
	// saved origin, a second to re-add it underfoot. Covers both branches rather than only the
	// add, because removing the origin mid-tournament also clears duel_arena_loaded, which is what
	// Cmd_DuelMode_f tests for "There is no duel arena in this map" -- it would silently stop
	// anyone else joining a tournament that is still running. Signup (mode 1) counts as running:
	// the globe is spawned by the first sign-up, before the mode ever reaches 2.
	if (level.duel_tournament_mode > 0)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot change the duel arena while a Duel Tournament is running\n\"");
		return;
	}

	zyk_create_dir("duelarena");

	duel_arena_file = fopen(va("GalaxyRP/duelarena/%s/origin.txt", zyk_mapname), "r");
	if (duel_arena_file == NULL)
	{ // zyk: arena file does not exist yet, create one
		VectorCopy(ent->client->ps.origin, level.duel_tournament_origin);

		zyk_create_dir(va("duelarena/%s", zyk_mapname));

		duel_arena_file = fopen(va("GalaxyRP/duelarena/%s/origin.txt", zyk_mapname), "w");
		fprintf(duel_arena_file, "%d\n%d\n%d\n", (int)level.duel_tournament_origin[0], (int)level.duel_tournament_origin[1], (int)level.duel_tournament_origin[2]);
		fclose(duel_arena_file);

		level.duel_arena_loaded = qtrue;

		trap->SendServerCommand(ent - g_entities, va("print \"Added duel arena at %d %d %d\n\"", (int)level.duel_tournament_origin[0], (int)level.duel_tournament_origin[1], (int)level.duel_tournament_origin[2]));
	}
	else
	{ // zyk: arena file already exists, remove it
		fclose(duel_arena_file);

		remove(va("GalaxyRP/duelarena/%s/origin.txt", zyk_mapname));

		level.duel_arena_loaded = qfalse;

		trap->SendServerCommand(ent - g_entities, va("print \"Removed duel arena of this map\n\""));
	}
}

/*
==================
Cmd_DuelPause_f
==================
*/
void Cmd_DuelPause_f(gentity_t *ent) {
	if (!check_admin_command(ent, ADM_DUELARENA, qtrue))
	{
		return;
	}

	if (level.duel_tournament_mode == 0)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^3Duel Tournament: ^7There is no duel tournament now\n\"");
		return;
	}

	// GalaxyRP fix: [Duel Tournament] refuse while a duel is actually being fought (mode 4).
	// Pausing there never worked: the mode-4 block in G_RunFrame decides the outcome inside
	// "if (validate() == qtrue && paused == qfalse)", and its else arm is the "this match is
	// over" path. Setting paused made that guard false while both duelists were still perfectly
	// valid, so the duel was abandoned -- and because the else arm does not advance
	// duel_matches_done, mode 5 sent the SAME pairing round again from mode 2, with both
	// duelists restored to full health and everyone's "died in duel" bit cleared. In other
	// words /duelpause silently restarted the duel, and holding it down meant the match could
	// never finish. Every other mode is paused by the "paused == qfalse" wrapper further down
	// in G_RunFrame, which is where pausing does work.
	if (level.duel_tournament_mode == 4)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^3Duel Tournament: ^7cannot pause while a duel is being fought. Wait for the match to end\n\"");
		return;
	}

	if (level.duel_tournament_paused == qfalse)
	{ // zyk: pauses the tournament
		level.duel_tournament_paused = qtrue;
		trap->SendServerCommand(-1, "chat \"^3Duel Tournament: ^7pausing the tournament\"");
	}
	else
	{ // zyk: resumes the tournament
		level.duel_tournament_paused = qfalse;
		trap->SendServerCommand(-1, "chat \"^3Duel Tournament: ^7resuming the tournament\"");
	}
}

// GalaxyRP: [Sniper Battle] Cmd_SniperMode_f() and Cmd_SniperTable_f() -- the "/snipermode" and
// "/snipertable" commands -- used to sit here, and their rows in commands[] below went with them.
// Cmd_SniperMode_f was the only way into the Sniper Battle: joining set level.sniper_players[client]
// to 0 and level.sniper_mode to 1, which is what every other Sniper Battle branch in the mod tested
// for. With the zyk_allow_sniper_battle and zyk_sniper_battle_time_to_start cvars gone and the whole
// feature removed, nothing can raise that state any more, so all of it -- sniper_battle_end/prepare/
// winner and the G_RunFrame block that drove them (g_main.c), the hit restriction in
// zyk_can_hit_target() (g_main.c), the death handling in player_die() (g_combat.c), the pickup block
// in Touch_Item() (g_items.c), the /give and /logout guards and the /duelmode and /meleemode join
// refusals in this file, the spectator/disconnect resets (g_client.c), the spawnflag-1 branch of
// zyk_mini_gamer_joiner_do() (g_misc.c) and the level_locals_t fields themselves (g_local.h) -- has
// been removed with it.

/*
==================
Cmd_MeleeMode_f
==================
*/
extern void melee_battle_end();
extern void melee_battle_restore(gentity_t *ent);
void Cmd_MeleeMode_f(gentity_t *ent) {
	if (rp_allow_melee_battle.integer != 1)
	{
		trap->SendServerCommand(ent->s.number, va("chat \"^3Melee Battle: ^7this mode is not allowed in this server\n\""));
		return;
	}

	// GalaxyRP fix: [Death System] a downed player keeps RP_DOWNED_HEALTH health, so nothing else here stops
	// them signing up while incapacitated -- they would just be teleported in and left lying
	// there. Same explicit test the force powers and holdable items use.
	if (G_PlayerIsDowned(ent))
	{
		trap->SendServerCommand(ent->s.number, "print \"^1You cannot do this while you are downed.\n\"");
		return;
	}


	// GalaxyRP: [Melee Battle] the Melee Battle is for logged-out players. This guard was
	// commented out at some point, which let RPG characters in and is why melee_battle_end()
	// had to grow an initialize_rpg_skills() call to put their real skills back afterwards --
	// restoring from the client's vanilla Profile would otherwise have handed them a loadout
	// they never earned. Live again, matching New Zyk Mod, which refuses ACC_MODE_RPG here.
	//
	// Gated on "not already signed up" so it only ever refuses JOINING. /meleemode is a toggle,
	// and a player who is somehow both logged in and in a battle must still be able to leave
	// it; an unconditional refusal here would trap them in the arena permanently.
	//
	// Tested on amrpgmode, not sess.loggedin: amrpgmode is the durable field (it is the one
	// carried across a map change in the session string), loggedin is derived from it in
	// ClientBegin, and every other RPG gate in this file reads amrpgmode.
	if (ent->client->sess.amrpgmode == 2 && level.melee_players[ent->s.number] == -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"The Melee Battle is only for players who are not logged in. Use ^3/logout ^7to join.\n\"");
		return;
	}

	if (level.melee_arena_loaded == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "print \"There is no melee arena in this map\n\"");
		return;
	}

	if (level.duel_tournament_mode > 0 && level.duel_players[ent->s.number] != -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a Duel Tournament\n\"");
		return;
	}

	// GalaxyRP fix: [Melee Battle] refuse while in a private duel, and this one was not merely
	// untidy -- it stalled the battle outright. G_Damage (g_combat.c) makes a private duellist
	// mutually immune with everyone but their opponent, and only through MOD_SABER even then, so
	// two players who signed up together and then challenged each other arrived on the catwalk
	// unable to be punched by anyone, unable to punch anyone, and unable to punch EACH OTHER --
	// melee damage is MOD_MELEE. Force Push is one of the fourteen powers the battle strips and
	// G_Damage returns before applying knockback, so they could not even be pushed off.
	// melee_mode_quantity never reached 1, melee_battle_winner() never fired, and the battle sat
	// out its full ten-minute timeout. The arena teleport does not break the duel either: the
	// starting grid is 45 units per slot and rp_duel_radius defaults to 1024.
	//
	// The Duel Tournament survives the same trick only because validate_duelists() kills such a
	// duelist on sight; the Melee Battle has no equivalent, which is why it has to refuse here.
	if (ent->client->ps.duelInProgress == qtrue && level.melee_players[ent->s.number] == -1)
	{
		trap->SendServerCommand(ent->s.number, "print \"You are already in a private duel\n\"");
		return;
	}

	if (level.melee_players[ent->s.number] == -1 && level.melee_mode > 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"Cannot join the Melee Battle now\n\"");
		return;
	}
	else if (level.melee_players[ent->s.number] == -1)
	{ // zyk: join the melee battle
		if (level.melee_mode_quantity == 0)
		{ // zyk: first player joined. Put the model in the melee arena and set its origin point
			gentity_t *new_ent = NULL;

			// GalaxyRP fix: [Entity System] ask for the slot before taking it. G_Spawn() calls
			// trap->Error(ERR_DROP) when the entity table is full, and Com_Error promotes
			// ERR_DROP to ERR_FATAL under com_dedicated -- i.e. the server process exits. Every
			// other command that spawns (/spawnplatform, /spawndummy, the NPC spawners, /entload,
			// /duelmode) was given this guard already; this one was missed, so any player typing
			// /meleemode on a full map could drop the whole server.
			if (G_EntitySlotsAvailable(1) == qfalse)
			{
				trap->SendServerCommand(ent->s.number, "print \"^3Melee Battle: ^7no free entity slots to build the arena right now\n\"");
				return;
			}

			new_ent = G_Spawn();

			zyk_set_entity_field(new_ent, "classname", "misc_model_breakable");
			zyk_set_entity_field(new_ent, "spawnflags", "65537");
			zyk_set_entity_field(new_ent, "origin", va("%d %d %d", (int)level.melee_mode_origin[0], (int)level.melee_mode_origin[1], (int)level.melee_mode_origin[2]));
			zyk_set_entity_field(new_ent, "model", "models/map_objects/factory/catw2_b.md3");
			zyk_set_entity_field(new_ent, "targetname", "zyk_melee_catwalk");
			zyk_set_entity_field(new_ent, "zykmodelscale", "400");
			// DAJ_RP: [Melee Battle] the solid box is the drawn ring: from its bottom at the origin up to its
			// floor grating (RP_MELEE_RING_TOP, g_local.h). It was -32 to 32 -- a top 10 units over the
			// grating, so everyone stood in the air, and an invisible slab under the ring that reached
			// below the floor where /meleearena was set, trapping whoever stood there.
			zyk_set_entity_field(new_ent, "mins", "-256 -256 0");
			zyk_set_entity_field(new_ent, "maxs", va("256 256 %d", RP_MELEE_RING_TOP));

			zyk_spawn_entity(new_ent);

			level.melee_model_id = new_ent->s.number;

			// DAJ_RP: [Melee Battle] anyone the ring has just appeared around is lifted onto it, rather than
			// left stuck inside it (RP_MeleeLiftOffRing(), g_main.c)
			if (new_ent->inuse)
			{
				RP_MeleeLiftOffRing(new_ent);
			}
		}

		// GalaxyRP fix: [Melee Battle] only start the countdown, never restart it. This was
		// unconditional, so every new sign-up pushed the start twelve seconds further out and a
		// trickle of joiners could hold a battle in signup indefinitely. Cmd_DuelMode_f already
		// guards its own timer this way -- it resets only when the tournament was not already in
		// signup -- so the two mini-games now behave the same way at the door.
		if (level.melee_mode != 1)
		{
			level.melee_mode_timer = level.time + 12000;
		}

		// DAJ_RP: [Admin] whatever an admin gave him with /give goes now, on signing up: he fights as a
		// plain logged-out player (rp_give_lift()). After every refusal above, so a refused sign-up keeps it.
		rp_give_lift(ent);
		RP_TrainingOff(ent, "for the Melee Battle");	// DAJ_RP: [Training Saber] and his training saber, the same way

		level.melee_players[ent->s.number] = 0;
		level.melee_mode = 1;
		level.melee_mode_quantity++;

		trap->SendServerCommand(-1, va("chat \"^3Melee Battle: ^7%s ^7joined the battle!\n\"", ent->client->pers.netname));
	}
	else
	{
		// GalaxyRP fix: [Melee Battle] give this player their weapons and force powers back before
		// dropping them from the roster. Every other way out of a battle already restores: death
		// runs WP_InitForcePowers() in player_die() (g_combat.c), and going to spectator or
		// disconnecting comes back through ClientSpawn(). Only this one -- typing /meleemode a
		// second time while the fight is running -- restored nothing at all, because the three
		// statements below clear level.melee_players[] first and melee_battle_end()'s loop then
		// skips the very player who triggered it. They walked away with melee_battle_prepare()'s
		// loadout (WP_MELEE only, binoculars only, fourteen force powers cleared) and kept it until
		// they next died or the map changed.
		//
		// This used to be gated on melee_mode > 1 -- the "has been prepared" test -- because the
		// old restore was a baseline re-issue that granted a Bryar Pistol and rebuilt force powers
		// from the client profile, so running it on someone who merely changed their mind during
		// signup handed them things they never had. It restores from a real snapshot now and both
		// halves return early when no snapshot was taken, so the call is a no-op for a player
		// leaving during signup and the gate has nothing left to decide. Called unconditionally
		// instead, which also means this path flushes a stray backup rather than stranding it --
		// the same contract melee_battle_end() has.
		melee_battle_restore(ent);

		level.melee_players[ent->s.number] = -1;
		level.melee_mode_quantity--;

		if (level.melee_mode_quantity == 0)
		{ // zyk: everyone left the battle. Remove the catwalk
			melee_battle_end();
		}

		trap->SendServerCommand(-1, va("chat \"^3Melee Battle: ^7%s ^7left the battle!\n\"", ent->client->pers.netname));
	}
}

/*
==================
Cmd_MeleeArena_f
==================
*/
void Cmd_MeleeArena_f(gentity_t *ent) {
	FILE *duel_arena_file;
	char content[1024];
	char zyk_info[MAX_INFO_STRING] = { 0 };
	char zyk_mapname[128] = { 0 };

	// zyk: getting the map name
	trap->GetServerinfo(zyk_info, sizeof(zyk_info));
	Q_strncpyz(zyk_mapname, Info_ValueForKey(zyk_info, "mapname"), sizeof(zyk_mapname));

	strcpy(content, "");

	if (!check_admin_command(ent, ADM_DUELARENA, qtrue))
	{
		return;
	}

	// GalaxyRP fix: [Melee Battle] refuse while the mode is switched off -- the same guard, and for
	// the same reasons, as the one on Cmd_DuelArena_f above. Cmd_MeleeMode_f already checks this
	// cvar first, so nothing could be spawned with the mode disabled, but the destructive half of
	// this toggle was reachable regardless.
	if (rp_allow_melee_battle.integer != 1)
	{
		trap->SendServerCommand(ent - g_entities, "print \"^3Melee Battle: ^7this mode is not allowed in this server\n\"");
		return;
	}

	// GalaxyRP fix: [Melee Battle] refuse while a battle is running. The add branch below does
	// VectorCopy(ent->client->ps.origin, level.melee_mode_origin) -- it re-points the arena at
	// whoever typed the command, while the catwalk entity stays where it was spawned. G_RunFrame()
	// (g_main.c) kills, by suicide, any combatant whose origin is below melee_mode_origin[2]
	// ("fell off the catwalk") or more than 1000 units away from it ("too far from the platform"),
	// so moving the origin kills every player in the battle in the same frame.
	//
	// Covers both branches and counts signup as running, for the same reasons set out on
	// Cmd_DuelArena_f above. Mode 3 is the battle winding down and is refused too: melee_mode_origin
	// is still the live arena until melee_battle_end() runs.
	if (level.melee_mode > 0)
	{
		trap->SendServerCommand(ent - g_entities, "print \"Cannot change the melee arena while a Melee Battle is running\n\"");
		return;
	}

	zyk_create_dir("meleearena");

	duel_arena_file = fopen(va("GalaxyRP/meleearena/%s/origin.txt", zyk_mapname), "r");
	if (duel_arena_file == NULL)
	{ // zyk: arena file does not exist yet, create one
		VectorCopy(ent->client->ps.origin, level.melee_mode_origin);

		zyk_create_dir(va("meleearena/%s", zyk_mapname));

		duel_arena_file = fopen(va("GalaxyRP/meleearena/%s/origin.txt", zyk_mapname), "w");
		fprintf(duel_arena_file, "%d\n%d\n%d\n", (int)level.melee_mode_origin[0], (int)level.melee_mode_origin[1], (int)level.melee_mode_origin[2]);
		fclose(duel_arena_file);

		level.melee_arena_loaded = qtrue;

		trap->SendServerCommand(ent - g_entities, va("print \"Added melee arena at %d %d %d\n\"", (int)level.melee_mode_origin[0], (int)level.melee_mode_origin[1], (int)level.melee_mode_origin[2]));
	}
	else
	{ // zyk: arena file already exists, remove it
		fclose(duel_arena_file);

		remove(va("GalaxyRP/meleearena/%s/origin.txt", zyk_mapname));

		level.melee_arena_loaded = qfalse;

		trap->SendServerCommand(ent - g_entities, va("print \"Removed melee arena of this map\n\""));
	}
}

// GalaxyRP: [RPG LMS] Cmd_RpgLmsMode_f() and Cmd_RpgLmsTable_f() -- the "/rpglmsmode" and
// "/rpglmstable" commands -- used to sit here, and their rows in commands[] below went with them.
// They were the only way into the RPG LMS: joining set level.rpg_lms_players[client] to 0 and
// level.rpg_lms_mode to 1, which is what every other RPG LMS branch in the mod tested for. With the
// zyk_allow_rpg_lms cvar gone and the whole feature removed, nothing can raise that state any more,
// so all of it -- rpg_lms_end/prepare/winner and the G_RunFrame block that drove them (g_main.c),
// the hit restriction in zyk_can_hit_target() (g_main.c), the death handling in player_die()
// (g_combat.c), the spectator/disconnect resets (g_client.c) and the level_locals_t fields
// themselves (g_local.h) -- has been removed with it.

/*
==================
Cmd_News_f
==================
*/
void Cmd_News_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS], arg2[MAX_STRING_CHARS];
	int numberOfEntries = 10;

	if (trap->Argc() > 3 || trap->Argc() < 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: /news <channel name> <number of entries>\n\"");
		return;
	}

	if (trap->Argc() == 3)
	{
		trap->Argv(2, arg2, sizeof(arg2));
		numberOfEntries = atoi(arg2);
		// GalaxyRP fix: [validation] this used to print the error and fall through anyway, so
		// /news <channel> <count> would still run with the out-of-range count -- e.g. a count over 10
		// bypassed the cap entirely (the display loop's only bound is numberOfEntries), and a negative
		// count got bound straight into the SQL LIMIT, which SQLite treats as "no limit" and fetches
		// every row in the channel for nothing.
		if (numberOfEntries < 1 || numberOfEntries > 10) {
			trap->SendServerCommand(ent->s.number, "print \"Error: Can only display between one and ten entries at a time.\n\"");
			return;
		}
	}

	trap->Argv(1, arg1, sizeof(arg1));
	trap->SendServerCommand(ent->s.number, va("print \"^3Viewing entries in channel ^2%s^3:\n\"", arg1));
	select_news_from_channel(ent, arg1, numberOfEntries);
}

void Cmd_NewsChannels_f(gentity_t* ent) {
	select_news_channels(ent);
	return;
}

void Cmd_NewsRemove_f(gentity_t* ent) {
	char arg1[MAX_STRING_CHARS];
	int newsID;

	if (!check_admin_command(ent, ADM_REMOVENEWS, qtrue))
	{
		return;
	}

	if (trap->Argc() != 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: /newsremove <news id>\n\"");
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));
	newsID = atoi(arg1);
	// GalaxyRP fix: [validation] newsID is the News table's INTEGER PRIMARY KEY, which (like CharID)
	// auto-starts at 1 -- 0 is never a real row, so this used to let "ID must be positive" through for
	// an ID of exactly 0.
	if (newsID <= 0) {
		trap->SendServerCommand(ent->s.number, "print \"ID must be positive.\n\"");
		return;
	}

	// GalaxyRP fix: [Database] delete_news_table_row_with_id used to go through run_db_query(), which
	// is a bare sqlite3_exec() wrapper that reports neither rows-affected nor failure back to the
	// caller -- so this command claimed "Removed news entry with ID %i!" unconditionally, even for an
	// ID that matched nothing. It now reports whether a row was actually deleted.
	if (delete_news_table_row_with_id(ent, newsID))
	{
		trap->SendServerCommand(ent->s.number, va("print \"Removed news entry with ID %i!\n\"", newsID));
	}
	else
	{
		trap->SendServerCommand(ent->s.number, va("print \"No news entry found with ID %i.\n\"", newsID));
	}

	return;
}

/*
==================
Cmd_UpdateNews_f
==================
*/
void Cmd_UpdateNews_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];
	// GalaxyRP fix: [cleanup] removed a dead 'FILE *news_file = NULL;' local here, left over from
	// when news was file-backed rather than stored in the News table.

	if (!check_admin_command(ent, ADM_UPDATENEWS, qtrue))
	{
		return;
	}

	if (trap->Argc() != 3)
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: /newsadd <channel> <news text>\n\"");
		return;
	}
	trap->Argv(1, arg1, sizeof(arg1));
	trap->Argv(2, arg2, sizeof(arg2));

	// GalaxyRP fix: [validation] nothing stopped an admin from posting a blank news entry (e.g.
	// /newsadd general "" or a channel name that trims to nothing) -- reject empty channel/text instead
	// of silently inserting an empty row.
	if (arg1[0] == '\0' || arg2[0] == '\0')
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: /newsadd <channel> <news text>\n\"");
		return;
	}

	// GalaxyRP fix: [News] refuse a channel name past MAX_NEWS_CHANNEL_LENGTH -- see the constant for
	// what an unbounded one costs. Checked here, before insert_news_table_row() opens a database
	// connection below, so a rejected name never opens one. Deliberately unconditional rather than
	// scoped to new channels: a pre-existing channel with a longer name stays readable and listed by
	// /newschannels, it simply cannot take new entries, and one plain rule is easier to explain than
	// "this long name works and that one does not". strlen is safe on arg1 -- trap->Argv() always
	// NUL-terminates within the buffer it is given.
	if ((int)strlen(arg1) > MAX_NEWS_CHANNEL_LENGTH)
	{
		trap->SendServerCommand(ent->s.number, va("print \"^1Channel names are limited to %d characters.\n\"", MAX_NEWS_CHANNEL_LENGTH));
		return;
	}

	// GalaxyRP fix: [News] announce the addition only when a row was actually written.
	// insert_news_table_row() now reports its outcome, and it has real refusal paths of its own (the
	// channel cap), so claiming success unconditionally would have told an admin their entry had
	// been posted when it had not. Its refusals print their own reason, so there is nothing to add
	// on the failing branch.
	if (insert_news_table_row(ent, arg1, arg2) == qtrue)
	{
		trap->SendServerCommand(ent->s.number, va("print \"Added news to channel %s\n\"", arg1));
	}

}

void description_display_beginning(gentity_t *ent, char netname[MAX_STRING_CHARS]) {
	trap->SendServerCommand(ent->s.number, va("print \"^2Looking over %s^2 you notice:\n\"", netname));
	trap->SendServerCommand(ent->s.number, "print \"^2================================================================================\n\"");
}

void description_display_end(gentity_t *ent) {
	trap->SendServerCommand(ent->s.number, "print \"^2================================================================================\n\"");
}

// GalaxyRP fix: [Attributes] description_add() used to be here. It wrote a description to
// GalaxyRP/descriptions/<character name>.txt with fopen(), and nothing had called it since
// descriptions moved into the Characters table -- /attributes saves through
// update_chars_table_row_with_current_values() and /examine reads pers.description. Removed rather
// than left as a file writer keyed on a player-chosen name that the next person might wire back up.
//
// Its neighbour zyk_clean_description() now lives near the news code as zyk_clean_text(), shared
// with the item commands.

/*
==================
Cmd_Examine_f
==================
*/
void Cmd_Examine_f(gentity_t *ent) {
	char player_name[MAX_STRING_CHARS];
	char description[MAX_STRING_CHARS];
	gentity_t *target = NULL;
	int player_id = -1;

	if (trap->Argc() != 2) {
		trap->SendServerCommand(ent->s.number, "print \"Usage: /examine <playername>\n\"");
		return;
	}

	trap->Argv(1, player_name, sizeof(player_name));
	player_id = ClientNumberFromString(ent, player_name, qfalse);

	//player not found, no point in going on
	if (player_id == -1) {
		return;
	}

	target = &g_entities[player_id];

	// GalaxyRP fix: [cleanup] see the identical fix/comment where this same line appears above.
	if (Distance(ent->client->ps.origin, target->client->ps.origin) > 1000) {
		trap->SendServerCommand(ent->s.number, "print \"You are too far away from that person.\n\"");
		return;
	}

	// GalaxyRP fix: [Attributes] a player who is not logged in has no character, so there is no
	// description to show. Before /logout started emptying pers.description this printed whatever
	// their last character had written; checked on amrpgmode, the same field the CMD_LOGGEDIN gate
	// uses, so "logged in" means one thing throughout.
	if (target->client->sess.amrpgmode == 0) {
		trap->SendServerCommand(ent->s.number, va("print \"%s^7 is not logged in.\n\"", target->client->pers.netname));
		return;
	}

	// GalaxyRP fix: [Attributes] shown from a cleaned copy -- see zyk_clean_text() -- so a
	// description that reached the database by some other road cannot put line breaks into this
	// console either. The stored text is left as it is.
	Q_strncpyz(description, target->client->pers.description, sizeof(description));

	if (zyk_clean_text(description) == 0) {
		trap->SendServerCommand(ent->s.number, va("print \"%s^7 has no description.\n\"", target->client->pers.netname));
		return;
	}

	// GalaxyRP fix: [cleanup] the body used to sit inside an "if (trap->Argc())" that could never be
	// false by this point (it had just been required to be exactly 2), and passed
	// &pers.description -- a pointer to the array, the same stray & already fixed for pers.netname
	// in the call just below.
	//
	// GalaxyRP fix: [cleanup] same "extra &" pattern as the Distance() calls above --
	// pers.netname is already a char[]; MSVC flagged the pointer-to-array this produced
	// (C4047/C4024) against description_display_beginning()'s char* parameter.
	description_display_beginning(ent, target->client->pers.netname);
	// GalaxyRP fix: [Attributes] through the news system's line splitter rather than one print.
	// SV_SendServerCommand drops a message past 1022 characters whole, not truncated; nothing
	// /attributes accepts now comes near that (MAX_DESCRIPTION_LENGTH), but a longer description
	// already in the database is still shown -- in pieces, instead of as an empty gap.
	zyk_print_long_line(ent, "", description);
	description_display_end(ent);
}

/*
==================
Cmd_Attributes_f
==================
*/
// GalaxyRP fix: [Attributes] in order: the usage line for anything but exactly one argument; the
// text cleaned (zyk_clean_text); nothing left means reset to RP_DEFAULT_DESCRIPTION; past
// MAX_DESCRIPTION_LENGTH is refused; the same text as now is not saved again; anything else is
// stored and saved. Every outcome now says what happened -- this used to print nothing at all, not
// even on success.
//
// One argument, not the rest of the line, is deliberate and unchanged: the description has to be
// quoted, and the usage line and the /list commands entry now say so. The "unchanged" test saves a
// database round trip -- update_chars_table_row_with_current_values() opens the database and
// rewrites the whole character row on every call.
void Cmd_Attributes_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];
	int len = 0;

	if (trap->Argc() != 2) {
		trap->SendServerCommand(ent->s.number, va("print \"Usage: /attributes <description>\nPut your description in double quotes if it has more than one word (up to %d characters). Empty double quotes reset it.\n\"", MAX_DESCRIPTION_LENGTH));
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	len = zyk_clean_text(arg1);

	if (len == 0) {
		if (strcmp(ent->client->pers.description, RP_DEFAULT_DESCRIPTION) == 0) {
			trap->SendServerCommand(ent->s.number, "print \"Your description is already reset.\n\"");
			return;
		}

		Q_strncpyz(ent->client->pers.description, RP_DEFAULT_DESCRIPTION, sizeof(ent->client->pers.description));
		update_chars_table_row_with_current_values(ent);
		trap->SendServerCommand(ent->s.number, "print \"Description reset.\n\"");
		return;
	}

	if (len > MAX_DESCRIPTION_LENGTH) {
		trap->SendServerCommand(ent->s.number, va("print \"Descriptions are limited to %d characters (yours is %d).\n\"", MAX_DESCRIPTION_LENGTH, len));
		return;
	}

	if (strcmp(ent->client->pers.description, arg1) == 0) {
		trap->SendServerCommand(ent->s.number, "print \"That is already your description.\n\"");
		return;
	}

	Q_strncpyz(ent->client->pers.description, arg1, sizeof(ent->client->pers.description));
	update_chars_table_row_with_current_values(ent);

	// Exactly at the limit is the one length that cannot be told apart from a longer description the
	// engine cut down on the way in (see MAX_DESCRIPTION_LENGTH), so say so.
	if (len == MAX_DESCRIPTION_LENGTH) {
		trap->SendServerCommand(ent->s.number, va("print \"Description set. (%d characters is the limit; anything longer was cut off.)\n\"", MAX_DESCRIPTION_LENGTH));
	}
	else {
		trap->SendServerCommand(ent->s.number, "print \"Description set.\n\"");
	}
}

/*
==================
Cmd_ShakeScreen_f
==================
*/
// The RP_SHAKE_* bounds this enforces, and the reasoning behind each one, are defined beside the
// chat distance ladder near the top of this file.
void Cmd_ShakeScreen_f(gentity_t* ent)
{
	int i, distanceFromPlayer, intensity, seconds, duration;
	char arg1[MAX_STRING_CHARS], arg2[MAX_STRING_CHARS], arg3[MAX_STRING_CHARS];
	gentity_t *other;

	if (!check_admin_command(ent, ADM_SHAKESCREEN, qtrue))
	{
		return;
	}

	if (trap->Argc() != 4)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"^2Command Usage: /shakescreen <distance (%d-%d)> <intensity (%d-%d)> <length in seconds (%d-%d)>\nExample: /shakescreen 600 4 2\n\"",
			RP_SHAKE_MIN_DISTANCE, RP_SHAKE_MAX_DISTANCE, RP_SHAKE_MIN_INTENSITY, RP_SHAKE_MAX_INTENSITY, RP_SHAKE_MIN_SECONDS, RP_SHAKE_MAX_SECONDS));
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));
	distanceFromPlayer = atoi(arg1);
	trap->Argv(2, arg2, sizeof(arg2));
	intensity = atoi(arg2);
	trap->Argv(3, arg3, sizeof(arg3));
	seconds = atoi(arg3);

	// Rejected rather than clamped, matching /paralyze's and /scale's out-of-range handling, so an
	// admin who mistypes an argument is told instead of silently getting a different shake. Checked
	// in argument order so the message names the first one that is wrong.
	if (distanceFromPlayer < RP_SHAKE_MIN_DISTANCE || distanceFromPlayer > RP_SHAKE_MAX_DISTANCE)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"Distance must be between %d and %d.\n\"",
			RP_SHAKE_MIN_DISTANCE, RP_SHAKE_MAX_DISTANCE));
		return;
	}

	if (intensity < RP_SHAKE_MIN_INTENSITY || intensity > RP_SHAKE_MAX_INTENSITY)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"Intensity must be between %d and %d.\n\"",
			RP_SHAKE_MIN_INTENSITY, RP_SHAKE_MAX_INTENSITY));
		return;
	}

	if (seconds < RP_SHAKE_MIN_SECONDS || seconds > RP_SHAKE_MAX_SECONDS)
	{
		trap->SendServerCommand(ent - g_entities, va("print \"Length must be between %d and %d seconds.\n\"",
			RP_SHAKE_MIN_SECONDS, RP_SHAKE_MAX_SECONDS));
		return;
	}

	// range-checked above, so this cannot overflow
	duration = seconds * 1000;

	for (i = 0; i < level.maxclients; i++)
	{
		other = &g_entities[i];

		if (g_entities[i].inuse && g_entities[i].client && g_entities[i].client->pers.connected == CON_CONNECTED && Distance(ent->client->ps.origin, other->client->ps.origin) <= distanceFromPlayer)
		{
			// GalaxyRP fix: [Admin] the event's position is the player's live origin, the same field the
			// distance test above already reads. It used to be g_entities[i].s.origin, which is not a
			// stale copy of that -- it is an unrelated point that never moves. The per-frame sync
			// (BG_PlayerStateToEntityState, called from ClientEndFrame) copies ps.origin into
			// s.pos.trBase and never into s.origin, and G_SetOrigin -- what ClientSpawn uses -- writes
			// s.pos.trBase and r.currentOrigin, also not s.origin. The only things that ever write a
			// player's s.origin are SpectatorThink (every frame, but only while spectating) and
			// MoveClientToIntermission. So for someone actually playing it holds either (0,0,0) -- the
			// slot as g_entities[] was memset at map load -- or wherever they last floated as a
			// spectator, frozen there for the rest of the map.
			//
			// Nothing reads it today, twice over: SVF_BROADCAST short-circuits the snapshot loop before
			// any PVS test, so the position cannot affect who receives this, and the cgame handler
			// (EV_SCREENSHAKE) passes only angles[0] and time to CGCam_Shake, which has no origin
			// parameter at all. It is fixed because the two things that would start reading it are both
			// plausible -- dropping the broadcast so this is PVS-culled instead, or giving the shake a
			// distance falloff the way CG_DoCameraShake does -- and either one would silently be
			// culling against, or measuring from, the world origin. A temp entity at (0,0,0) usually
			// lands in solid, where numClusters is 0 and the snapshot loop drops it for everyone.
			//
			// s.origin is the right field for a non-client entity -- Use_Target_Screenshake passes it
			// for a target_screenshake, correctly -- which is where the confusion comes from. For a
			// client it is ps.origin (or r.currentOrigin, which ClientThink_real keeps in step); see
			// the explicit if(ent->client) branch in ai_main.c for the same distinction spelled out.
			G_ScreenShake(g_entities[i].client->ps.origin, &g_entities[i], intensity, duration, qtrue);
			trap->SendServerCommand(i, "print \"^3An admin shook your screen.\n\"");
		}
	}
	trap->SendServerCommand(ent - g_entities, "print \"^2You shook people's screen.\n\"");
	return;
}

// GalaxyRP: [nofight] Cmd_NoFight_f() used to sit here -- the "/nofight" command (added by Zyk in
// 2017 and never modified since), which toggled the PLAYER_STATUS_ADMIN_PARALYSIS bit to make a player unable to
// damage other players and, more importantly, unable to be damaged BY them. It has been removed
// entirely, along with every check that read that bit: the two in zyk_can_hit_target() (g_main.c),
// the sentry-gun one in G_Damage() (g_combat.c), and the "cannot join X while being in nofight
// mode" guards on /duelmode and /melee above. There were three more -- on /rpglms, /race and
// /sniper -- which went with the RPG LMS, Race Mode and Sniper Battle removals.
//
// It could only be toggled while spectating, so a player who enabled it and then joined had no way
// to turn it back off, and nothing in the game told them it was on -- the only documentation was an
// in-game tutorial line that was dropped when the tutorial text moved to language files. The result
// was a player who looked normal but could not be hurt by anyone, with no console output and no
// obvious cause, which is exactly how it was finally tracked down. Since the mode also had to be
// entered from spectator (a couple of keystrokes away at any time), the restriction never actually
// prevented anyone from granting themselves PvP immunity either.
//
// Bit 26 of player_statuses is now unused and free for reuse -- see g_local.h.
/*
==================
Cmd_ModVersion_f
==================
*/

void Cmd_ModVersion_f(gentity_t *ent) {
	trap->SendServerCommand(ent->s.number, va("print \"\n%s\n\n\"", GAMEVERSION));
}

typedef struct sound_channels_s {
	const char* channel_name;
	int			channel_code;
} sound_channels_t;

// GalaxyRP fix: [cleanup] declared with an explicit size ([14]) that had to be kept in sync by hand with
// the two hardcoded loop bounds in Cmd_ZykSound_f below -- let the compiler derive it from the
// initializer instead, and use ARRAY_LEN() at each call site, so adding/removing an entry here can't
// silently desync from those loops.
const sound_channels_t sound_channels[] = {
	{"auto",				CHAN_AUTO			},
	{"local",				CHAN_LOCAL			},
	{"weapon",				CHAN_WEAPON			},
	{"voice",				CHAN_VOICE			},
	{"voice_attenuate",		CHAN_VOICE_ATTEN	},
	{"item",				CHAN_ITEM			},
	{"body",				CHAN_BODY			},
	{"ambient",				CHAN_AMBIENT		},
	{"local_sound",			CHAN_LOCAL_SOUND	},
	{"announcer",			CHAN_ANNOUNCER		},
	{"less_attenuate",		CHAN_LESS_ATTEN		},
	{"menu1",				CHAN_MENU1			},
	{"voice_global",		CHAN_VOICE_GLOBAL	},
	{"music",				CHAN_MUSIC			},
};

/*
==================
RP_PlaySound

/playsound's play, for both of its forms: the player's cooldown, the file, the slot, the sound.

GalaxyRP: [Slot Reuse] one sound every RP_PLAYSOUND_COOLDOWN (15 s) per player, whatever the name: the
engine's flood protection lets a command through every second, or every frame when a server turns it
off, and each new name is a configstring every client downloads and a sound file every client loads.
Only a sound that plays starts the wait -- a typo, a missing file or a refusal do not.

GalaxyRP: [Listings] a name no client can load took a slot of the sound table until the map changed,
and played nothing -- see RP_SoundFileExists() in g_rplist.c.

GalaxyRP fix: [security] G_SoundIndex() used to crash the whole server (ERR_DROP) once its sound table
filled up with never-before-seen names, and this is a raw player-typed string. The slot now comes from
RP_PlaySoundIndex() (g_utils.c): /playsound holds at most RP_PLAYSOUND_POOL slots at once, reusing the
one played least recently, and a refusal says why instead of handing G_Sound() an invalid index. The
name goes in as typed: the configstring is a copy, and a G_NewString() of it spent level memory that is
never given back.
==================
*/
static void RP_PlaySound(gentity_t *ent, int channel, const char *path)
{
	char reason[256];
	int soundIndex;

	if (ent->client->pers.rpPlaySoundNextTime > level.time + RP_PLAYSOUND_COOLDOWN)
	{
		ent->client->pers.rpPlaySoundNextTime = 0;	// only a guard: pers is cleared on every map
	}
	if (level.time < ent->client->pers.rpPlaySoundNextTime)
	{
		int seconds = (ent->client->pers.rpPlaySoundNextTime - level.time + 999) / 1000;

		trap->SendServerCommand(ent->s.number, va("print \"You can play another sound in %d second%s.\n\"", seconds, seconds == 1 ? "" : "s"));
		return;
	}

	if (!RP_SoundFileExists(path))
	{
		trap->SendServerCommand(ent->s.number, va("print \"Sound file not found: ^3%.64s ^7(see ^3/list sounds^7)\n\"", path));
		return;
	}

	soundIndex = RP_PlaySoundIndex(path, reason, sizeof(reason));
	if (soundIndex == 0)
	{
		trap->SendServerCommand(ent->s.number, va("print \"Cannot play this sound right now: %s.\n\"", reason));
		return;
	}

	G_Sound(ent, channel, soundIndex);
	ent->client->pers.rpPlaySoundNextTime = level.time + RP_PLAYSOUND_COOLDOWN;
}

/*
==================
Cmd_ZykSound_f
==================
*/
void Cmd_ZykSound_f(gentity_t *ent) {
	char arg1[MAX_STRING_CHARS];
	char arg2[MAX_STRING_CHARS];

	if (rp_allow_playsound_command.integer < 1)
	{
		trap->SendServerCommand(ent->s.number, "print \"This command is not allowed in this server\n\"");
		return;
	}

	if (trap->Argc() < 2 || trap->Argc() > 3)
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/playsound <channel> <sound file path>\n ^2Sound channels available: \n\"");
		// GalaxyRP fix: [cleanup] this hardcoded the array length (14) instead of deriving it, so
		// adding/removing a channel here without also updating this literal (and the matching one
		// below) would silently desync the printed list from the array, or read past its end.
		for (int i = 0; i < ARRAY_LEN(sound_channels); i++) {
			trap->SendServerCommand(ent->s.number, va("print \"^3%s\n\"", sound_channels[i].channel_name));
		}
		return;
	}

	trap->Argv(1, arg1, sizeof(arg1));

	if (trap->Argc() == 3)
	{
		trap->Argv(2, arg2, sizeof(arg2));

		// GalaxyRP fix: [validation] nothing rejected an empty sound path (e.g. /playsound auto "") --
		// that fed straight into G_SoundIndex()/G_Sound(), both of which assert() a non-empty/non-zero
		// value, so this could trip an assertion in a debug build for no reason.
		if (arg2[0] == '\0')
		{
			trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/playsound <channel> <sound file path>\n\"");
			return;
		}

		for (int i = 0; i < ARRAY_LEN(sound_channels); i++) {
			if (strcmp(arg1, sound_channels[i].channel_name) == 0) {
				RP_PlaySound(ent, sound_channels[i].channel_code, arg2);
				return;
			}
		}

		trap->SendServerCommand(ent->s.number, "print \"Channel not found! \n\"");
		return;
	}

	if (trap->Argc() == 2)
	{
		if (arg1[0] == '\0')
		{
			trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/playsound <channel> <sound file path>\n\"");
			return;
		}

		RP_PlaySound(ent, CHAN_AUTO, arg1);
		return;
	}

	return;
}

/*
==================
Cmd_Music_f
==================
*/
void Cmd_Music_f(gentity_t* ent) {
	char audioPath[MAX_STRING_CHARS];

	if (!check_admin_command(ent, ADM_MUSIC, qtrue)) {
		return;
	}

	if (trap->Argc() != 2)
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/playmusic <sound file path>\n\"");
		return;
	}

	trap->Argv(1, audioPath, sizeof(audioPath));

	// GalaxyRP fix: [validation] nothing rejected an empty path (e.g. /playmusic ""), which would
	// silently clear the level's music track instead of doing anything resembling "play a file".
	if (audioPath[0] == '\0')
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/playmusic <sound file path>\n\"");
		return;
	}

	// GalaxyRP: [Listings] a mistyped path stopped the music for everyone and played nothing -- see
	// RP_MusicFileExists() in g_rplist.c.
	if (!RP_MusicFileExists(audioPath))
	{
		trap->SendServerCommand(ent->s.number, va("print \"Music file not found: ^3%.64s ^7(see ^3/list music^7)\n\"", audioPath));
		return;
	}

	// GalaxyRP fix: [Music] through G_SetMusic(): two file names at most, each short of MAX_QPATH, and
	// only while the gamestate has room -- this wrote the whole argument into CS_MUSIC
	{
		const char *reason = "";

		if (!G_SetMusic(audioPath, &reason))
		{
			trap->SendServerCommand(ent->s.number, va("print \"Music not started: %s.\n\"", reason));
			return;
		}
	}
	trap->SendServerCommand(ent - g_entities, va("print \"^2You started playing the music file: ^7%.128s\n\"", audioPath));

	return;
}

// GalaxyRP: [Training Saber fix] pers.training_mode is only ever written by this command and by
// ClientBegin's reset -- it is never told about a saber reselection (/saber, the saber-select UI,
// or a DB-driven saber reload on spawn), which fully re-parses the new saber's parms and resets
// damageScale/damageScale2 back to that saber's own defaults. That leaves a window where the flag
// still claims training mode is ACTIVE even though the player's current saber(s) are already back
// to full damage. Rather than trust the flag blindly, treat training mode as active only when the
// flag says so AND every saber slot's live damage scale is still actually zeroed, and self-heal the
// flag the moment that stops being true. This is what "true status" means below, and using it for
// the on/off guards too (not just the status print) keeps every code path agreeing with what's
// actually applied to the player right now.
static qboolean Training_IsLiveActive(gentity_t* ent)
{
	int i;

	if (!ent->client->pers.training_mode)
		return qfalse;

	for (i = 0; i < MAX_SABERS; i++)
	{
		if (ent->client->saber[i].damageScale != 0 || ent->client->saber[i].damageScale2 != 0)
		{
			// Stale flag: a saber slot's live damage scale no longer matches what training mode
			// applied, almost certainly because the player picked a new saber since. Self-heal so
			// nothing downstream (status display, on/off guards) is fooled by the old flag value.
			ent->client->pers.training_mode = qfalse;
			return qfalse;
		}
	}

	return qtrue;
}

// DAJ_RP: [Training Saber] training mode's undo, shared by "/training off" and RP_TrainingOff(): both saber slots'
// damage scales back to what "/training on" stored, and the flag down.
static void Training_Restore(gentity_t* ent)
{
	int i;

	for (i = 0; i < MAX_SABERS; i++)
	{
		ent->client->saber[i].damageScale = ent->client->pers.training_stored_damageScale[i];
		ent->client->saber[i].damageScale2 = ent->client->pers.training_stored_damageScale2[i];
	}

	ent->client->pers.training_mode = qfalse;
}

// DAJ_RP: [Training Saber] is this player's training saber on right now (Training_IsLiveActive(), which also clears a
// flag gone stale)? A thrown saber reads it (CheckThrownSaberDamaged(), w_saber.c): its damage is the throw's own, not
// the saber's damage scales that "/training on" zeroes, so it used to hit at full damage.
qboolean RP_TrainingActive(gentity_t* ent)
{
	if (!ent || !ent->client)
		return qfalse;

	return Training_IsLiveActive(ent);
}

// DAJ_RP: [Training Saber] turns a player's training saber off, as "/training off" does, and tells him why ("for the
// duel"), when a private duel he is in is accepted (Cmd_EngageDuel_f()) and when he signs up for the Duel Tournament or
// the Melee Battle (Cmd_DuelMode_f(), Cmd_MeleeMode_f()): he fights at his saber's real damage. Nothing, and no word,
// when it is not on. "/training on" is not refused afterwards: it is a handicap he may take on, not a gain.
void RP_TrainingOff(gentity_t* ent, const char* why)
{
	if (!RP_TrainingActive(ent))
		return;

	Training_Restore(ent);
	trap->SendServerCommand(ent->s.number, va("print \"Training saber is ^1INACTIVE^7: turned off %s.\n\"", why));
}

//GalaxyRP (Alex): [Training Saber] This method activates and deactivates the training saber, making it so that when active, you do very little damage.

// GalaxyRP: [Training Saber fix] rewritten from a single no-argument toggle to explicit
// "/training on" / "/training off" verbs. The old toggle couldn't tell "turn on" from "turn off"
// apart from its own stored state, so if that state and the live saber values ever drifted --
// e.g. from picking a new saber while training was active, which resets damageScale/damageScale2
// back to that saber's own defaults without training_mode being told -- the next toggle would
// silently do the wrong thing (store the already-zeroed value as "original", or leave full damage
// live while claiming to be active). Explicit on/off can't cross those states the same way: "on"
// while already on and "off" while already off are both harmless no-ops that don't touch anything
// live, and turning it on is the point where the player is warned about the exact conditions that
// deactivate it, rather than needing to be told after the fact.
// "/training" with no argument now reports the true current status (see Training_IsLiveActive)
// alongside the usage tip, so a player who switched saber, /kill'd, or rejoined always sees what's
// actually applied rather than a possibly-stale flag.
void Cmd_TrainingMode_f(gentity_t* ent) {
	char arg[MAX_TOKEN_CHARS];
	int i;
	qboolean isActive;

	// Resolving this up front self-heals pers.training_mode if it had gone stale, so the status
	// print and the on/off guards below all agree with reality.
	isActive = Training_IsLiveActive(ent);

	if (trap->Argc() == 1)
	{
		if (isActive)
			trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/training <on|off>^7. Training saber is currently ^2ACTIVE\n\"");
		else
			trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/training <on|off>^7. Training saber is currently ^1INACTIVE\n\"");
		return;
	}

	trap->Argv(1, arg, sizeof(arg));

	if (!Q_stricmp(arg, "on"))
	{
		if (isActive)
		{
			trap->SendServerCommand(ent->s.number, "print \"Training saber is already ^2ACTIVE\n\"");
			return;
		}

		// GalaxyRP: [Training Saber fix] cover both saber slots (saber1 and saber2), not just the
		// primary one -- previously only ent->client->saber[0] (equivalent to the old
		// ent->client->saber-> shorthand) was touched, so a player's second, independently-wielded
		// saber kept dealing full damage the whole time training mode was "active".
		for (i = 0; i < MAX_SABERS; i++)
		{
			ent->client->pers.training_stored_damageScale[i] = ent->client->saber[i].damageScale;
			ent->client->pers.training_stored_damageScale2[i] = ent->client->saber[i].damageScale2;
			ent->client->saber[i].damageScale = 0;
			ent->client->saber[i].damageScale2 = 0;
		}

		ent->client->pers.training_mode = qtrue;

		trap->SendServerCommand(ent->s.number, "print \"Training saber is ^2ACTIVE^7. Warning: changing your saber or rejoining the game (reconnecting, or a map change) will turn this off.\n\"");
	}
	else if (!Q_stricmp(arg, "off"))
	{
		if (!isActive)
		{
			trap->SendServerCommand(ent->s.number, "print \"Training saber is already ^1INACTIVE\n\"");
			return;
		}

		Training_Restore(ent);

		trap->SendServerCommand(ent->s.number, "print \"Training saber is ^1INACTIVE\n\"");
	}
	else
	{
		trap->SendServerCommand(ent->s.number, "print \"Usage: ^3/training <on|off>\n\"");
	}

	return;
}

// GalaxyRP fix: [Settings] zyk_setting_status_text() used to sit here -- plain-text status words for
// the ui_zyk_setting_N_value cvars, fed to the client via the settings_to_sync loop that used to be in
// Cmd_GalaxyRpUi_f just below (see CG_ZykMod/ui_cvars_in_order in cg_servercmds.c). Those cvars only
// ever fed the Settings panel in ingame_galaxyrp.menu, which was removed entirely in an earlier pass of
// this cleanup, and the settings this function reported on (Allow Force Powers from allies, Starting
// Single Saber Style, Allow Screen Message, Use healing force only at allied players, Start With Saber)
// are themselves removed in this pass (see settings_number_to_bit in Cmd_Settings_f) -- so both the
// function and its ui_cvars_in_order/XCVAR_DEF plumbing in cg_servercmds.c and ui_xcvar.h are now
// entirely without a consumer and have been removed together.

void Cmd_GalaxyRpUi_f(gentity_t* ent) {
	// zyk: sends info to the client-side menu if player has the client-side plugin
	char userinfo[MAX_INFO_STRING];
	char modelname[MAX_STRING_CHARS];
	char saber1Model[MAX_STRING_CHARS];
	char saber2Model[MAX_STRING_CHARS];
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

	//GalaxyRP (Alex): [User Info] We grab values from the current user info, so that we can set ui cvars based on that. Otherwise, when you use the ui, there's strange behavior.
	Q_strncpyz(modelname, Info_ValueForKey(userinfo, "model"), sizeof(modelname));
	Q_strncpyz(saber1Model, Info_ValueForKey(userinfo, "saber1"), sizeof(saber1Model));
	Q_strncpyz(saber2Model, Info_ValueForKey(userinfo, "saber2"), sizeof(saber2Model));

	// GalaxyRP fix: [stability] model/saber1/saber2 come straight from client-editable userinfo, and
	// unlike character names (rejected outright if they contain '&' or anything non-alphanumeric,
	// see create_new_character()'s zyk_check_user_input() check) nothing ever stripped a '~' out of
	// these. The engine's Cvar_ValidateString (cvar.cpp) rejects '\', '"' and ';' in a userinfo value
	// but not '~' -- and '~' is exactly the field delimiter the content string below is built from, so
	// a player who sets e.g. their model cvar to include a literal '~' shifts every field after it in
	// their own zykmod payload (CG_ZykMod's mismatch warning, added separately, will now at least log
	// this instead of silently misapplying it -- but stripping it here means it can't happen at all).
	// Purely self-inflicted either way (this command only ever sends back to ent->s.number, the same
	// player whose cvars these are), but there's no reason to leave the gap open. netname is handled
	// the same way, into its own local copy, a few lines below.
	Q_strstrip(modelname, "~", NULL);
	Q_strstrip(saber1Model, "~", NULL);
	Q_strstrip(saber2Model, "~", NULL);

	// GalaxyRP: [Profile UI] tell the client whether it is currently logged into an account, so the
	// Profile menu can gate the Force icon and the Character Information/Skills/Characters/Shop/Games
	// sections on login state (see CG_LoggedInUpdate_f). Sent unconditionally and before the NOGUID
	// return below, so a logged-out player (who by definition has no GUID-backed account session)
	// still gets a fresh "0" every time the menu is opened rather than being skipped entirely.
	trap->SendServerCommand(ent->s.number, va("supdateloggedin %i\n", ent->client->sess.loggedin));

	if (Q_stricmp(ent->client->pers.guid, "NOGUID") == 0)
	{
		return;
	}

	if (ent->client->sess.loggedin == qtrue)
	{
		char content[1024];
		int level = ent->client->pers.level;
		int xp = ent->client->pers.xp;
		int xpToLevel = check_xp(level);
		int skillpoints = ent->client->pers.skillpoints;
		int credits = ent->client->pers.credits;

		// GalaxyRP fix: [security/stability] this whole payload used to be accumulated via
		// strcpy(content, va("%s...", content, ...)) at every step below -- content fed back into va() as
		// its own first argument, growing with no bound before being strcpy()'d back into this fixed
		// 1024-byte stack buffer. Every field here is bounded in practice today (MAX_NETNAME, the client's
		// own MAX_QPATH-capped model-packing buffer, skill levels that never exceed single digits, etc.),
		// so this never actually overflowed in real play -- but nothing here enforced that, and a future
		// change to any of these fields (a longer name cap, more skills, larger credit/xp values) could
		// silently corrupt this stack buffer with no warning at all. Q_strcat never writes past the
		// destination's declared size, so switched every append below to it: a field that wouldn't fit is
		// safely dropped instead of corrupting memory. See the length sanity check right before this is
		// sent, further down, for the other half of this fix.
		content[0] = '\0';

		// GalaxyRP fix: [stability] see the Q_strstrip() comment above modelname/saber1Model/
		// saber2Model further up -- netname gets the same '~'-stripping treatment here, into its own
		// local copy rather than mutating ent->client->pers.netname in place (that field is the
		// player's actual display name used everywhere else; only this one payload needs a delimiter-
		// safe copy of it).
		char netnameForUi[MAX_NETNAME];
		Q_strncpyz(netnameForUi, ent->client->pers.netname, sizeof(netnameForUi));
		Q_strstrip(netnameForUi, "~", NULL);

		Q_strcat(content, sizeof(content), va("%s~%s~%s~%s~%d~%d/%d~%d~%d~%s~",
			netnameForUi, modelname, saber1Model, saber2Model, level, xp, xpToLevel, skillpoints, credits, ent->client->sess.rpgchar));

		for (int i = 0; i < ARRAY_LEN(skills); i++) {
			Q_strcat(content, sizeof(content), va("%d/%d~", ent->client->pers.skill_levels[i], skills[i].max_level));
		}

		// GalaxyRP fix: [Settings] a settings_to_sync loop used to be appended here, after the skills
		// loop and before the packet is sent, pushing 6 setting-status words (via the now-removed
		// zyk_setting_status_text() above) into 6 entries that used to sit here in ui_cvars_in_order[]
		// in cg_servercmds.c. Those cvars only ever fed the Settings panel in ingame_galaxyrp.menu, which
		// was removed entirely in an earlier pass of this cleanup, and the settings themselves are
		// removed in this pass (see settings_number_to_bit in Cmd_Settings_f) -- so the loop, its
		// ui_cvars_in_order[] entries and their XCVAR_DEFs in ui_xcvar.h have all been removed together.

		// GalaxyRP fix: [Shop] appended after where the settings loop above used to be, same piggyback-on-zykmod
		// approach, in the same fixed order as the 3 new entries added to ui_cvars_in_order[] in
		// cg_servercmds.c (ui_zyk_upgrade_1/2/3_owned). Reports whether each of the 3 kept shop
		// upgrades (skill_levels[38] bits 0/1/2, per-character -- see the duplicate-purchase guards in
		// Cmd_Buy_f and the doc comment on skill_levels in g_local.h; formerly player_settings bits
		// 0/1/2, account-wide) is already owned, so the Shop -> Upgrades panel (ingame_galaxyrp.menu)
		// can grey out and disable a button for an upgrade the player already has, instead of letting
		// them click a purchase Cmd_Buy_f's own duplicate-purchase guard would reject anyway with a
		// "You already have..." message.
		{
			int owned_bit;
			int upgrade_bits_to_sync[] = { 0, 1, 2 };

			for (int i = 0; i < ARRAY_LEN(upgrade_bits_to_sync); i++) {
				owned_bit = upgrade_bits_to_sync[i];
				Q_strcat(content, sizeof(content), va("%d~", (ent->client->pers.skill_levels[38] & (1 << owned_bit)) ? 1 : 0));
			}
		}

		// GalaxyRP fix: [stability] defensive sanity check -- if this payload ever grows close to the
		// buffer's real capacity (a future change to NUM_OF_SKILLS, a longer field, or a later feature
		// piggybacking on this command), log it loudly server-side instead of letting it fail silently.
		// Two different silent failure modes exist downstream of this buffer: Q_strcat above quietly drops
		// whatever wouldn't fit once content is full, and even a content string that fits in this buffer
		// can still be silently discarded in full by SV_SendServerCommand (sv_main.cpp), which refuses to
		// send any reliable command string over 1022 bytes with no error to anyone. 900 leaves headroom
		// for the "zykmod \"...\"" wrapper this content is embedded in below.
		if (strlen(content) > 900) {
			G_LogPrintf("WARNING: Cmd_GalaxyRpUi_f content for %s is %d bytes, close to the zykmod payload's safe limit.\n", ent->client->pers.netname, (int)strlen(content));
		}

		trap->SendServerCommand(ent->s.number, va("zykmod \"%s\"", content));
	}
	else
	{
		return;
	}
}

/*
==================
Cmd_ZykChars_f
==================
*/
void Cmd_ZykChars_f(gentity_t* ent) {
	// zyk: sends info to the client-side menu if player has the client-side plugin
	if (Q_stricmp(ent->client->pers.guid, "NOGUID") == 0)
	{
		return;
	}

	// GalaxyRP fix: [Account/security] the "zykchars" command has no CMD_LOGGEDIN flag in the commands[]
	// table below (by design -- like /login and /new, it has to be usable before a session is logged in),
	// but unlike Cmd_GalaxyRpUi_f (/zykmod), which explicitly checks sess.loggedin before sending any
	// character-specific payload, this function never checked it at all. It queried
	// select_character_list_for_ui() with sess.accountID unconditionally -- and until the matching fix in
	// Cmd_LogoutAccount_f, that field was never reset on logout, so a /zykchars sent by the client after
	// logging out (but before a fresh login) still returned this connection's *previous* account's
	// character list. accountID is now reset to 0 on logout, which already stops the query from matching
	// a real account, but check sess.loggedin here too, explicitly, matching the same defensive pattern
	// Cmd_GalaxyRpUi_f already uses -- and send an empty list rather than nothing, so the client's own
	// CG_ZykChars() (cg_servercmds.c) clears out any character names still cached in its
	// ui_zyk_rpg_char_1.._15 cvars from a previous session instead of leaving them stale.
	if (ent->client->sess.loggedin == qfalse)
	{
		trap->SendServerCommand(ent->s.number, "zykchars \"\"");
		return;
	}

	sqlite3* db;
	char* zErrMsg = 0;
	int rc;
	sqlite3_stmt* stmt = 0;
	char char_string[MAX_STRING_CHARS] = "";

	rc = RP_DB_Open(&db);
	if (rc != SQLITE_OK)
	{
		trap->Print("Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	select_character_list_for_ui(ent, db, zErrMsg, rc, stmt, char_string, sizeof(char_string));

	trap->SendServerCommand(ent->s.number, va("zykchars \"%s\"", char_string));
	sqlite3_close(db);
	return;
}

/*
=================
ClientCommand
=================
*/

#define CMD_NOINTERMISSION		(1<<0)
#define CMD_CHEAT				(1<<1)
#define CMD_ALIVE				(1<<2)
#define CMD_LOGGEDIN			(1<<3) // zyk: player must be logged in his account
#define CMD_RPG					(1<<4) // zyk: player must be in RPG Mode

typedef struct command_s {
	const char	*name;
	void		(*func)(gentity_t *ent);
	int			flags;
} command_t;

// GalaxyRP: [Extras] see RP_ListDataCommand() in g_rplist.c
static void Cmd_RpxList_f( gentity_t *ent ) {
	RP_ListDataCommand( ent );
}

int cmdcmp( const void *a, const void *b ) {
	return Q_stricmp( (const char *)a, ((command_t*)b)->name );
}


// GalaxyRP fix: [cleanup] dropped the old "This array MUST be sorted correctly by alphabetical
// name field" comment. It has not been true for a long time -- "spawnplatform" and "spawndummy"
// sit between "paralyze" and "playsound", among others -- and nothing requires it: ClientCommand()
// looks entries up with Q_LinearSearch() (q_shared.c), which walks the array from index 0 and does
// not care about order. Grouping related commands together is fine; new entries go wherever they
// read best.
command_t commands[] = {
	{ "addbot",				Cmd_AddBot_f,				0 },
	{ "admkick",			Cmd_AdmKick_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "callteamvote",		Cmd_CallTeamVote_f,			CMD_NOINTERMISSION },
	{ "callvote",			Cmd_CallVote_f,				CMD_NOINTERMISSION },
	{ "datetime",			Cmd_DateTime_f,				CMD_NOINTERMISSION },
	{ "debugBMove_Back",	Cmd_BotMoveBack_f,			CMD_CHEAT|CMD_ALIVE },
	{ "debugBMove_Forward",	Cmd_BotMoveForward_f,		CMD_CHEAT|CMD_ALIVE },
	{ "debugBMove_Left",	Cmd_BotMoveLeft_f,			CMD_CHEAT|CMD_ALIVE },
	{ "debugBMove_Right",	Cmd_BotMoveRight_f,			CMD_CHEAT|CMD_ALIVE },
	{ "debugBMove_Up",		Cmd_BotMoveUp_f,			CMD_CHEAT|CMD_ALIVE },
	{ "drop",				Cmd_Drop_f,					CMD_ALIVE|CMD_NOINTERMISSION },
	{ "follow",				Cmd_Follow_f,				CMD_NOINTERMISSION },
	{ "follownext",			Cmd_FollowNext_f,			CMD_NOINTERMISSION },
	{ "followprev",			Cmd_FollowPrev_f,			CMD_NOINTERMISSION },
	{ "forcechanged",		Cmd_ForceChanged_f,			0 },
	{ "updateforce",		Cmd_UpdateForce_f,			CMD_NOINTERMISSION },	// GalaxyRP: [Force] instant-apply from the in-game force power menu, logged-out players only
	{ "gc",					Cmd_GameCommand_f,			CMD_NOINTERMISSION },
	{ "give",				Cmd_Give_f,					CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "jetpack",			Cmd_Jetpack_f,				CMD_ALIVE|CMD_NOINTERMISSION },
	{ "kill",				Cmd_Kill_f,					CMD_ALIVE|CMD_NOINTERMISSION },
	{ "killother",			Cmd_KillOther_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "levelshot",			Cmd_LevelShot_f,			CMD_CHEAT|CMD_ALIVE|CMD_NOINTERMISSION },
	{ "maplist",			Cmd_MapList_f,				CMD_NOINTERMISSION },
	{ "saber",				Cmd_Saber_f,				CMD_NOINTERMISSION },
	{ "saberblade",			Cmd_SaberBlade_f,			CMD_NOINTERMISSION },	// GalaxyRP: [Saber RGB]
	{ "sabercolor",			Cmd_SaberColor_f,			CMD_NOINTERMISSION },	// GalaxyRP: [Saber RGB]
	{ "updatesaber",		Cmd_UpdateSaber_f,			CMD_NOINTERMISSION },	// GalaxyRP: [Saber] instant-apply from the in-game saber menu, see saber/saberblade/sabercolor above
	{ "say",				Cmd_Say_f,					0 },
	{ "say_team",			Cmd_SayTeam_f,				0 },
	{ "score",				Cmd_Score_f,				0 },
	{ "settings",			Cmd_Settings_f,				CMD_LOGGEDIN|CMD_NOINTERMISSION },
	{ "setviewpos",			Cmd_SetViewpos_f,			CMD_CHEAT|CMD_NOINTERMISSION },
	{ "shakescreen",		Cmd_ShakeScreen_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "siegeclass",			Cmd_SiegeClass_f,			CMD_NOINTERMISSION },
	{ "team",				Cmd_Team_f,					CMD_NOINTERMISSION },
	{ "teamvote",			Cmd_TeamVote_f,				CMD_NOINTERMISSION },
	{ "tell",				Cmd_Tell_f,					0 },
	{ "t_use",				Cmd_TargetUse_f,			CMD_CHEAT|CMD_ALIVE },
	{ "voice_cmd",			Cmd_VoiceCommand_f,			CMD_NOINTERMISSION },
	{ "vote",				Cmd_Vote_f,					CMD_NOINTERMISSION },
	/*
	=============
	GalaxyRP (Alex): [Commands] This is where GalaxyRP commands begin
	=============
	*/
	{ "admindown",			Cmd_AdminDown_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "adminlist",			Cmd_AdminList_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "adminup",			Cmd_AdminUp_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "admmap",				Cmd_AdmMap_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "admweather",			Cmd_AdmWeather_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "admwalkthrough",		Cmd_AdmWalkthrough_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "admhidden",			Cmd_AdmHidden_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "anim",				Cmd_Emote_f,				CMD_ALIVE | CMD_NOINTERMISSION },
	{ "allyadd",			Cmd_AllyAdd_f,				CMD_NOINTERMISSION },
	{ "allychat",			Cmd_AllyChat_f,				0 },					// GalaxyRP: [Chat] a say mode, so no flags -- see Cmd_AllyChat_f
	{ "allylist",			Cmd_AllyList_f,				CMD_NOINTERMISSION },
	{ "allyremove",			Cmd_AllyRemove_f,			CMD_NOINTERMISSION },
	{ "attributes",			Cmd_Attributes_f,			CMD_LOGGEDIN },
	{ "buy",				Cmd_Buy_f,					CMD_RPG | CMD_ALIVE | CMD_NOINTERMISSION },
	{ "changepassword",		Cmd_ChangePassword_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "char",				Cmd_Char_f,					CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "clientprint",		Cmd_ClientPrint_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "createcredits",		Cmd_CreditCreate_f,			CMD_RPG | CMD_NOINTERMISSION },
	{ "createitem",			Cmd_CreateItem_f,			CMD_LOGGEDIN},
	{ "duelarena",			Cmd_DuelArena_f,			CMD_LOGGEDIN | CMD_ALIVE | CMD_NOINTERMISSION },
	{ "duelmode",			Cmd_DuelMode_f,				CMD_ALIVE | CMD_NOINTERMISSION },
	{ "duelpause",			Cmd_DuelPause_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "dueltable",			Cmd_DuelTable_f,			CMD_NOINTERMISSION },
	{ "duelteam",			Cmd_DuelTeam_f,				CMD_NOINTERMISSION },
	{ "emote",				Cmd_Emote_f,				CMD_ALIVE | CMD_NOINTERMISSION },
	{ "engage_fullforceduel",	Cmd_ForceDuel_f,		CMD_ALIVE | CMD_NOINTERMISSION },	// GalaxyRP: [Force Duel] the ordinary duel has no row here -- it arrives as GENCMD_ENGAGE_DUEL
	{ "entadd",				Cmd_EntAdd_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entaddaim",			Cmd_EntAddAim_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entaddgrab",			Cmd_EntAddGrab_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },	// DAJ_RP: [Entity System]
	{ "entcancel",			Cmd_EntCancel_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entcopy",			Cmd_EntCopy_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entcopystatic",		Cmd_EntCopyStatic_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },	// DAJ_RP: [Static Models] g_entgrab.c
	{ "entcut",				Cmd_EntCut_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entdeletefile",		Cmd_EntDeleteFile_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entedit",			Cmd_EntEdit_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "enthelp",			Cmd_EntHelp_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },	// DAJ_RP: [Entity Help] g_enthelp.c
	{ "entitiesandremaps",	Cmd_EntitiesAndRemaps_f,	CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entlist",			Cmd_EntList_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entload",			Cmd_EntLoad_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entlockadd",			Cmd_EntLockAdd_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },	// DAJ_RP: [Locks] g_locks.c
	{ "entlocklist",		Cmd_EntLockList_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entlockremove",		Cmd_EntLockRemove_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entlockset",			Cmd_EntLockSet_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entnear",			Cmd_EntNear_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entorigin",			Cmd_EntOrigin_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entremove",			Cmd_EntRemove_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entrotate",			Cmd_EntRotate_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entsave",			Cmd_EntSave_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entslots",			Cmd_EntSlots_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "enttrigger",			Cmd_EntTrigger_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },	// DAJ_RP: [Locks] g_locks.c
	{ "entundo",			Cmd_EntUndo_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "entuse",				Cmd_EntUse_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "ex",					Cmd_Examine_f,				CMD_LOGGEDIN },
	{ "examine",			Cmd_Examine_f,				CMD_LOGGEDIN },
	{ "flipcoin",			Cmd_FlipCoin_f,				CMD_NOINTERMISSION|CMD_ALIVE },
	{ "flipcoinall",		Cmd_FlipCoinAll_f,			CMD_NOINTERMISSION },
	{ "ghost",				Cmd_Ghost_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "givecredits",		Cmd_CreditGive_f,			CMD_RPG | CMD_NOINTERMISSION },
	{ "giveitem",			Cmd_GiveItem_f,				CMD_LOGGEDIN},
	// GalaxyRP fix: [XP System] CMD_NOINTERMISSION added -- /levelup, /leveldown and /removexp all
	// carry it and this one did not, so it was the only command in the levelling group that could
	// still mutate and persist a character during intermission.
	{ "givexp",				Cmd_GiveXp_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "god",				Cmd_God_f,					CMD_LOGGEDIN | CMD_ALIVE | CMD_NOINTERMISSION },
	{ "helpup",				Cmd_Helpup_f,				CMD_ALIVE | CMD_NOINTERMISSION},
	{ "getup",				Cmd_Getup_f,				CMD_ALIVE | CMD_NOINTERMISSION},
	{ "holo",				Cmd_Holo_f,					CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "ignore",				Cmd_Ignore_f,				CMD_NOINTERMISSION },
	{ "ignorelist",			Cmd_IgnoreList_f,			CMD_NOINTERMISSION },
	{ "inv",				Cmd_Inventory_f,			CMD_LOGGEDIN},
	{ "inventory",			Cmd_Inventory_f,			CMD_LOGGEDIN},
	{ "levelup",			Cmd_LevelGive_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "leveldown",			Cmd_LevelTake_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "list",				Cmd_ListAccount_f,			CMD_NOINTERMISSION },
	{ "listaccount",		Cmd_ListAccount_f,			CMD_NOINTERMISSION },
	{ "login",				Cmd_Login_F,				CMD_NOINTERMISSION },
	{ "logout",				Cmd_LogoutAccount_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "meleearena",			Cmd_MeleeArena_f,			CMD_LOGGEDIN | CMD_ALIVE | CMD_NOINTERMISSION },
	{ "meleemode",			Cmd_MeleeMode_f,			CMD_ALIVE | CMD_NOINTERMISSION },
	{ "meleetable",			Cmd_MeleeTable_f,			CMD_NOINTERMISSION },
	{ "modversion",			Cmd_ModVersion_f,			CMD_NOINTERMISSION },
	{ "new",				Cmd_Register_F,				CMD_NOINTERMISSION },
	{ "register",			Cmd_Register_F,				CMD_NOINTERMISSION },	// DAJ_RP: [Account] an alias of /new, as /anim is of /emote
	{ "news",				Cmd_News_f,					0 },
	// GalaxyRP fix: [Admin] both news-writing commands gain CMD_NOINTERMISSION, matching the 121
	// other rows in this table that carry it -- writing to the news table during intermission served
	// no purpose and was simply an omission. /news and /newschannels are deliberately left alone:
	// reading the board while waiting for the map to change is exactly when you would want to.
	{ "newsadd",			Cmd_UpdateNews_f,					CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "newschannels",		Cmd_NewsChannels_f,					0 },
	{ "newsremove",			Cmd_NewsRemove_f,					CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "noclip",				Cmd_Noclip_f,				CMD_LOGGEDIN | CMD_ALIVE | CMD_NOINTERMISSION },
	{ "notarget",			Cmd_Notarget_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	// GalaxyRP fix: [NPC] CMD_NOINTERMISSION added. "/npc spawn" reaches NPC_Spawn_f, which takes
	// entity slots exactly as /entadd does, and every other command that places or removes entities
	// -- the whole Entity System and shader-remap set, /spawnplatform, /spawndummy, /removepickups --
	// has carried this flag all along. This row was the one that did not, so an admin could still
	// spawn NPCs into a map that was already on its way out. The other subcommands (kill, team,
	// effect) have nothing to do during intermission either.
	{ "npc",				Cmd_NPC_f,					CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "ooc",				Cmd_OOC_f,					0 },					// GalaxyRP: [Chat] out-of-character chat, forced in every gametype -- see Cmd_OOC_f for the flags
	{ "order",				Cmd_Order_f,				CMD_ALIVE | CMD_NOINTERMISSION },
	{ "paralyze",			Cmd_Paralyze_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "unparalyze",			Cmd_Unparalyze_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "spawnplatform",		Cmd_SpawnPlatform_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "spawndummy",			Cmd_SpawnDummy_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	// GalaxyRP fix: [validation] this used to have no CMD_LOGGEDIN flag at all -- a connected but not
	// logged-in player could run /playsound. Adding it here reuses the same central "You must be logged
	// in" check (and message) every other logged-in-only command already goes through, rather than
	// duplicating that check inside Cmd_ZykSound_f itself.
	{ "playsound",			Cmd_ZykSound_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "playmusic",			Cmd_Music_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "players",			Cmd_Players_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "remap",				Cmd_Remap_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "remapdeletefile",	Cmd_RemapDeleteFile_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "remaplist",			Cmd_RemapList_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "remapload",			Cmd_RemapLoad_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "remapreset",			Cmd_RemapReset_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "remapsave",			Cmd_RemapSave_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "removexp",			Cmd_RemoveXp_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "removepickups",		Cmd_RemovePickups_f,		CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "roll",				Cmd_Roll_f,				CMD_NOINTERMISSION|CMD_ALIVE },
	{ "rollall",			Cmd_RollAll_f,			CMD_NOINTERMISSION },
	// GalaxyRP: [Extras] the listings the Extras menus show (g_rplist.c); it checks the login itself, so the
	// menu is told why instead of the chat line CMD_LOGGEDIN prints
	{ "rpxlist",			Cmd_RpxList_f,				0 },
	{ "scale",				Cmd_Scale_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "silence",			Cmd_Silence_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "skilldown",			Cmd_RpModeDown_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "skillup",			Cmd_RpModeUp_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "spendcredits",		Cmd_CreditSpend_f,			CMD_RPG | CMD_NOINTERMISSION },
	{ "stuff",				Cmd_Stuff_f,				CMD_RPG | CMD_NOINTERMISSION },
	{ "tele",				Cmd_Teleport_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "teleaim",			Cmd_TeleportAim_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },	// DAJ_RP: [Admin] /teleportaim's short name
	{ "telemark",			Cmd_Telemark_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "teleport",			Cmd_Teleport_f,				CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "teleportaim",		Cmd_TeleportAim_f,			CMD_LOGGEDIN | CMD_NOINTERMISSION },
	{ "training",			Cmd_TrainingMode_f,			CMD_ALIVE | CMD_NOINTERMISSION },
	{ "trashitem",			Cmd_TrashItem_f,			CMD_LOGGEDIN },
	// GalaxyRP fix: [Cloak Item] replaces the old GENCMD_USE_CLOAK_VEHICLE generic-command mechanism
	// (see Cmd_VehicleCloak_f's own doc comment for why that was unreachable). No CMD_ALIVE, matching
	// the original generic_cmd switch, which never gated on aliveness either.
	{ "vehicle_cloak",		Cmd_VehicleCloak_f,			CMD_NOINTERMISSION|CMD_ALIVE }, // CMD_ALIVE: a following spectator inherits the followed player's ps, vehicle and cloak state -- see Cmd_VehicleCloak_f
	{ "where",				Cmd_Where_f,				CMD_NOINTERMISSION },
	// GalaxyRP fix: no CMD_ALIVE -- Cmd_GalaxyRpUi_f's supdateloggedin push is meant to reach the
	// client unconditionally (see its own comment above the send), including while spectating, so a
	// player who quit logged in and reconnected logged out/spectating gets corrected as soon as the
	// menu's "exec zykmod" fires on open, instead of being stuck showing the logged-in menu state
	// until they're alive again.
	{ "zykmod",				Cmd_GalaxyRpUi_f,			CMD_NOINTERMISSION },
	// GalaxyRP fix: [Char] dropped CMD_ALIVE -- this required the player be alive, not spectating, and not
	// in tempSpectate just to refresh the character list, while its sibling zykmod (just above) was
	// deliberately fixed to have no such requirement, and Cmd_Char_f itself (the actual "char use"/"char
	// new"/"char remove" switch, g_cmds.c) has never required it either. The asymmetry meant a dead or
	// spectating player opening the Characters tab got a "must be alive" refusal instead of a refreshed
	// list, then could still click Use against whatever character name was left over in the client's
	// ui_zyk_rpg_char_N cvar from the last successful refresh -- stale data with no indication anything
	// was wrong. Character management doesn't need the player to be alive in the world any more than
	// switching characters itself does.
	{ "zykchars",			Cmd_ZykChars_f,			CMD_NOINTERMISSION }
//	{ "thedestroyer",		Cmd_TheDestroyer_f,			CMD_CHEAT|CMD_ALIVE|CMD_NOINTERMISSION },
//	{ "teamtask",			Cmd_TeamTask_f,				CMD_NOINTERMISSION },
//	{ "kylesmash",			TryGrapple,					0 },

};
static const size_t numCommands = ARRAY_LEN( commands );

void ClientCommand( int clientNum ) {
	gentity_t	*ent = NULL;
	char		cmd[MAX_TOKEN_CHARS] = {0};
	command_t	*command = NULL;

	ent = g_entities + clientNum;
	if ( !ent->client || ent->client->pers.connected != CON_CONNECTED ) {
		G_SecurityLogPrintf( "ClientCommand(%d) without an active connection\n", clientNum );
		return;		// not fully in game yet
	}

	trap->Argv( 0, cmd, sizeof( cmd ) );

	//rww - redirect bot commands
	if ( strstr( cmd, "bot_" ) && AcceptBotCommand( cmd, ent ) )
		return;
	//end rww

	command = (command_t *)Q_LinearSearch( cmd, commands, numCommands, sizeof( commands[0] ), cmdcmp );
	if ( !command )
	{
		trap->SendServerCommand( clientNum, va( "print \"Unknown command %s\n\"", cmd ) );
		return;
	}

	else if ( (command->flags & CMD_NOINTERMISSION)
		&& ( level.intermissionQueued || level.intermissiontime ) )
	{
		trap->SendServerCommand( clientNum, va( "print \"%s (%s)\n\"", G_GetStringEdString( "MP_SVGAME", "CANNOT_TASK_INTERMISSION" ), cmd ) );
		return;
	}

	else if ( (command->flags & CMD_CHEAT)
		&& !sv_cheats.integer )
	{
		trap->SendServerCommand( clientNum, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "NOCHEATS" ) ) );
		return;
	}

	else if ( (command->flags & CMD_ALIVE)
		&& (ent->health <= 0
			|| ent->client->tempSpectate >= level.time
			|| ent->client->sess.sessionTeam == TEAM_SPECTATOR) )
	{
		trap->SendServerCommand( clientNum, va( "print \"%s\n\"", G_GetStringEdString( "MP_SVGAME", "MUSTBEALIVE" ) ) );
		return;
	}

	else if ( (command->flags & CMD_LOGGEDIN)
		&& ent->client->sess.amrpgmode == 0 )
	{ // zyk: new condition
		trap->SendServerCommand( clientNum, "print \"You must be logged in\n\"" );
		return;
	}

	else if ( (command->flags & CMD_RPG)
		&& ent->client->sess.amrpgmode < 2 )
	{ // zyk: new condition
		trap->SendServerCommand( clientNum, "print \"You must be logged in\n\"" );
		return;
	}

	// GalaxyRP fix: [Dice] an "else if (Q_stricmp(cmd, \"roll\") == 0) Cmd_Roll_f(ent);" branch used to
	// sit here. It was pure duplication: "roll" is an ordinary entry in the commands[] table above, so
	// the fallthrough below already dispatches it to exactly the same handler after exactly the same
	// flag checks. Worse, it was actively wrong once /roll gained flags -- it would have run whatever
	// the table's handler was for any command reaching this point, bypassing nothing but confusing
	// everything. Removed; the table is now the single source of truth for every command.
	else
		command->func( ent );
}
