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
// #define	GAMEVERSION	"OpenJK"

#define SECURITY_LOG "logs/security.log"

#define BODY_QUEUE_SIZE		8

// GalaxyRP fix: [Force] these two replace the zyk_max_force_power server cvar, which is gone. It
// was never safely configurable: fd.forcePower is an 8-bit netfield (see PSF(fd.forcePower) in
// qcommon/msg.cpp -- it appears in all three playerState tables at that width), so any pool above
// 255 wraps modulo 256 on the client. It also silently set the cost of Force Heal and Shield Heal,
// and the cgame force HUD hardcodes a full bar at 100 regardless (cg_draw.c), so no value other
// than the shipped one displayed correctly anyway.
//
// RP_MAX_FORCE_POWER is the ceiling a logged-in character reaches at Force Power skill 5. 250 is
// the practical maximum and should not be raised further: the netfield tops out at 255, leaving
// only five units of slack. That slack is never actually used -- every path that adds force
// (WP_ForcePowerRegenerate, the Absorb and Team Energize grants in w_force.c, Drain, the Healing
// Crystal tick in g_active.c, the force pickup in g_misc.c) clamps in the same statement or the
// very next line, inside the same frame, so the value can never be above the maximum at the point
// a snapshot is built -- but there is no room left for a future path that does not clamp.
//
// Note that raising this does not make anyone cast faster. Force regenerates at a flat 1 point per
// g_forceRegenTime (200ms) whatever the pool size, and the forcePowerNeeded[] costs are fixed
// absolute numbers, so a bigger pool buys burst capacity and a longer refill (250 points is 50
// seconds from empty), not a higher sustained rate.
// GalaxyRP fix: [Entity System] shortest interval an fx_runner carrying the damage spawnflag may
// re-apply its G_RadiusDamage at. With "delay" and "random" unset the think re-armed for the very
// next frame, so the damage landed at server framerate.
#define RP_FX_RUNNER_MIN_DAMAGE_DELAY	100

// GalaxyRP: [Weather] /admweather reserves a fixed, contiguous block of CS_EFFECTS slots and
// rewrites EVERY slot in it on every change, with a counter appended so each string differs from
// the last and therefore actually re-broadcasts.
//
// This shape is forced by how weather works. The engine's world-effect parser is a command stream,
// not a state assignment: "*rain" then "*snow" gives rain AND snow, and the only removal it offers
// is "*clear", which wipes everything. Meanwhile a client that joins later replays the whole
// CS_EFFECTS table in SLOT ORDER (cg_main.c), while a client already connected runs each string as
// its configstring changes. So the only way both populations end up looking at the same sky is for
// the teardown to be part of every rebuild rather than a separate command someone has to remember.
//
// The layers are right-aligned in the block and every slot below them holds the teardown, so a
// shorter recipe simply grows the padding and the surplus "*clear"s always land before the layers.
// The padding must never be an EMPTY configstring: the client's replay loop stops at the first
// empty slot, so a blank would silently hide every slot above it from everyone who joins later.
// GalaxyRP fix: [Configstrings] the gamestate's own ceiling, which no part of the server enforces
// -- see the long comment on G_FindConfigstringIndex in g_utils.c. The headroom is for configstrings
// this budget cannot see coming: a player connecting writes their userinfo into CS_PLAYERS after we
// have already decided we had room. It does not make the limit airtight, because nothing in the game
// module can refuse a connecting player's configstring, but it keeps a full-to-the-brim gamestate
// from being tipped over by the next join.
#define ZYK_GAMESTATE_HEADROOM		1024
#define ZYK_GAMESTATE_BUDGET		(MAX_GAMESTATE_CHARS - ZYK_GAMESTATE_HEADROOM)

// GalaxyRP: [Slot Reuse] how many names a map may put into model and effect slots taken back from
// removed Entity System props (RP_SlotReclaim() in g_utils.c). Each client keeps every effect and
// model it has registered until the map changes -- 256 effects, about 120 of them the game's own, and
// 1024 models -- so reuse is bounded to keep a player who has been on the map since it loaded inside
// those limits.
#define RP_SLOT_REUSE_FX			32
#define RP_SLOT_REUSE_MODELS		128
// ...and sound slots: /playsound holds at most RP_PLAYSOUND_POOL of the map's 255 at once, a new name
// then taking the one played least recently (RP_PlaySoundIndex() in g_utils.c). Clients keep every
// sound name they register (10000 a session, then the game closes: S_FindName in snd_dma.cpp), so
// the names a map can put into reused slots are capped too.
#define RP_SLOT_REUSE_SOUNDS		128
#define RP_PLAYSOUND_POOL			32
#define RP_PLAYSOUND_COOLDOWN		15000	// between two /playsound of one player

// GalaxyRP fix: [Configstrings] how long G_RunFrame waits for the engine to finish writing the
// gamestate before taking the map-load count anyway. SV_SpawnServer writes CS_SYSTEMINFO four
// frames -- 400ms of level time -- after InitGame returns, so this only has to outlast that; it is
// a backstop in case a future engine never writes it at all, not the path that normally fires.
#define ZYK_GAMESTATE_BASELINE_WAIT	2000

// GalaxyRP fix: [Configstrings] one "already reported" flag per indexed table, so a table filling up
// is logged once instead of once per refused name. See zyk_cs_table_slot() in g_utils.c.
#define ZYK_CS_TABLES				8

// GalaxyRP fix: [Entity System] slots held back from every spawn a player or admin can drive, so the
// transient allocations ordinary play depends on -- G_TempEntity for every effect and sound event,
// missiles, gibs -- always have somewhere to go. G_Spawn() cannot fail gracefully: it has ~70 call
// sites, G_TempEntity among them dereferences the result immediately, and when it runs out it calls
// trap->Error(ERR_DROP). Refusing the controllable spawns early is what keeps it from ever getting
// there.
//
// GalaxyRP fix: [Entity System] and what ERR_DROP does is worse than the name suggests, which is
// worth stating once here because several comments in this mod reach for it. gi.Error is Com_Error
// itself (sv_gameapi.cpp), and Com_Error opens with:
//
//     // ERR_DROPs on dedicated drop to an interactive console
//     // which doesn't make sense for dedicated as it's generally run unattended
//     if ( com_dedicated && com_dedicated->integer ) { code = ERR_FATAL; }
//
// ERR_FATAL runs CL_Shutdown, SV_Shutdown, Com_Shutdown and then Sys_Error, so on a dedicated
// server -- which is every server this mod is played on -- an ERR_DROP raised from game code is a
// PROCESS EXIT, not a disconnect. Nobody reconnects; the server is gone until something restarts
// it. (More than three ERR_DROPs inside 100ms escalate the same way even on a listen server.)
// That is the cost these guards exist to avoid, and the reason the one surviving call in G_Spawn
// writes its reason to the log first.
#define ZYK_ENTITY_RESERVE			64
// GalaxyRP: [Logical Entities] the same idea for the logical region. G_SpawnLogical() ends the
// server the way G_Spawn() does once every logical slot is in use, so the player-driven spawn paths
// (/entadd, the entity-file loader, a dropped /entcopy, a misc_bsp rebuild) refuse while this many
// logical slots are still free -- see RP_SpawnRouteHasRoom().
#define ZYK_LOGICAL_ENTITY_RESERVE	16
// GalaxyRP: [Entity System] NPCs alive at once (bodies included) past which NPC_Spawn_Do() refuses;
// a backstop for playability behind the entity reserve, not a crash guard -- see RP_NPCsAlive().
#define RP_NPC_MAX_LIVE				256
// GalaxyRP: [Entity System] an NPC or vehicle spawner with "respawn 1" fires again when one it made
// dies -- never sooner than this after the death (RP_SpawnerRespawn in NPC_spawn.c), and its own delay
// when that is longer. What its keys may ask for: a delay of at most an hour, a count of at most this many.
#define RP_SPAWNER_RESPAWN_MIN		2000
#define RP_SPAWNER_DELAY_MAX		3600000
#define RP_SPAWNER_COUNT_MAX		1000
// GalaxyRP fix: [Entity System] whether a float parsed from a spawn key is a number at all. atof() and
// sscanf("%f") accept "nan", "inf", "-infinity" and "1e400" (an overflow is +inf), and every spawn
// key reaches them: origin, angles, mins, maxs, speed, wait... A NaN origin makes an entity nobody can
// reach or remove and a NaN velocity goes out to every client. G_SpawnFloat, G_SpawnVector and
// G_ParseField keep the default and log instead (g_spawn.c).
#if defined(_MSC_VER)
#define RP_FINITE(x)				( _finite( (double)(x) ) != 0 )
#else
#define RP_FINITE(x)				( isfinite( (x) ) )
#endif
// GalaxyRP: [Entity System] bytes of the 4 MB game memory pool kept back from the entity commands --
// see G_AllocRoomFor() in g_mem.c
#define ZYK_ALLOC_RESERVE			(256 * 1024)

// GalaxyRP fix: [Entity System] how many "#name" sub-BSP models a map can teach the game -- the
// engine's own MAX_SUB_BSP (q_shared.h), past which CM_LoadSubBSP() Com_Error(ERR_DROP)s.
#define RP_MAX_SUBBSP_NAMES			MAX_SUB_BSP

// GalaxyRP fix: [Entity System] most asteroids one trigger_asteroid_field may keep alive. count is a
// spawn key, so without a ceiling /entadd could ask for thousands. See SP_trigger_asteroid_field.
#define ZYK_MAX_ASTEROIDS			64

#define ZYK_WEATHER_MAX_LAYERS		8	// most weather layers one recipe may hold
#define ZYK_WEATHER_SLOTS			(ZYK_WEATHER_MAX_LAYERS + 1)	// layers plus one teardown slot
// GalaxyRP fix: [Weather] was 4, which an ordinary map overruns without trying: SP_CreateSnow
// registers three commands on its own (*snow, *fog, *constantwind) and SP_CreateWind up to
// five, so anything past the cap was dropped and /admweather default could not put it back.
// This shares the ZYK_WEATHER_MAX_LAYERS budget with the admin's own layers, so it cannot go
// all the way to 8 without leaving a heavily-weathered map no room to add anything; 6 covers
// the maps that exist and still leaves two layers. A map past even this is told, not ignored.
#define ZYK_WEATHER_MAX_BASE		6	// most of the map's own weather commands we remember
#define ZYK_WEATHER_CMD_LENGTH		96	// longest command we build, "*constantwind ( x y z )"
#define ZYK_WEATHER_MAX_CLOUDS		5	// MAX_PARTICLE_CLOUDS in the renderer's tr_WorldEffects.cpp
#define ZYK_WEATHER_MAX_WINDS		10	// MAX_WIND_ZONES there
#define ZYK_WEATHER_DEBOUNCE		2000	// one change rewrites every slot, so do not allow a flood
#define ZYK_WEATHER_WIND_LIMIT		10000	// per-axis cap on a constant wind velocity
#define ZYK_WEATHER_DUST_MIN		1
#define ZYK_WEATHER_DUST_MAX		10000	// the renderer allocates this many particles unchecked
#define ZYK_WEATHER_DUST_DEFAULT	1000	// DAJ_RP: [Weather] spacedust without a density, from an entity

// DAJ_RP: [Weather] the weather entities (zyk_weather, fx_rain, fx_snow, fx_spacedust, fx_wind, and an
// fx_runner whose fxFile starts with *): what kind of weather a command is, and the command an entity's
// text names, built and checked as /admweather builds its own (g_cmds.c)
#define RP_WEATHER_NONE		0	// not one of the weather effects
#define RP_WEATHER_CLOUD	1	// a particle effect: the renderer draws at most ZYK_WEATHER_MAX_CLOUDS
#define RP_WEATHER_WIND		2	// a wind: at most ZYK_WEATHER_MAX_WINDS
int			RP_WeatherCommandKind( const char *command );
qboolean	RP_WeatherCommandFromText( const char *text, char *out, int outSize, char *why, int whySize );


#define RP_MAX_FORCE_POWER		250

// GalaxyRP: [Force] the pool a player who is not logged in gets, and the ceiling the two clamps
// in w_force.c (Absorb's conversion in WP_AbsorbConversion and the lightning-absorb grant in
// ForceLightningDamage) hold them to. Team Energize used to be a third; it caps every target at its
// own forcePowerMax now, which for a logged-out player is this same number. Logged-out players
// only ever reach Force level 3, where the dearest thing in forcePowerNeeded[] is Heal at 70, so
// nothing is priced out of reach at this number -- it costs them burst capacity against a
// logged-in character, not access.
#define RP_MAX_FORCE_POWER_LOGGED_OUT	100

// GalaxyRP fix: [Force] a logged-in character's force pool is this fraction of RP_MAX_FORCE_POWER
// per level of the Force Power skill (skill index 54). It used to divide by 4 while that skill's
// max level is 5 (see skills[] in g_cmds.c), so a maxed character ended up with 125% of the
// supposed maximum. Dividing by the skill's actual max level makes level 5 land exactly on
// RP_MAX_FORCE_POWER, matching how set_max_shield() already divides by the Max Shield skill's own
// max level of 5. 250 divides by 5 exactly, so the five steps are round: 50/100/150/200/250.
#define RP_FORCE_POWER_SKILL_MAX_LEVEL	5

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

#define FL_NO_IMPACT_DMG			0x08000000 // Will not take impact/falling damage (SET_NO_IMPACT_DAMAGE) - GalaxyRP: [Scripts]

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

#include "../../galaxyrp/game/rp_local.h" // GalaxyRP: [GameGeneral] Main header

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

	// GalaxyRP: [ICARUS] level.time at which the voice line this entity is playing ends, 0 when
	// none. Q3_PlaySound() sets it for a CHAN_VOICE* sound and RP_IcarusSoundCheck() (g_main.c's
	// entity loop) completes the script's TID_CHAN_VOICE task when it passes -- the completion the
	// single-player engine does from its sound system and multiplayer never did, so a script's
	// task( sound(CHAN_VOICE, ...) ) dowait waited forever. See RP_SoundDuration() in g_ICARUScb.c.
	int				IcarusSoundTime;

	// GalaxyRP: [Scripts] map-model animation driven by the server (SET_STARTFRAME/ENDFRAME/
	// ANIMFRAME): RP_Animate() steps s.frame from startFrame to endFrame once rpAnimating is set
	// and completes TID_ANIM_BOTH when it gets there; loopAnim restarts instead (misc_model_*).
	int				startFrame;
	int				endFrame;
	qboolean		loopAnim;
	qboolean		rpAnimating;
	// GalaxyRP: [Entity System] rpAnimating was set by the AUTOANIMATE spawnflag of a
	// misc_model_breakable (not by a script), so a respawn without the flag turns it off again
	qboolean		rpAutoAnimate;

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

	// GalaxyRP: [Logical Entities] true for a slot at or above MAX_GENTITIES. Set once by
	// G_InitGentity() from the slot number and read wherever the entity would otherwise be handed
	// to the engine -- LinkEntity/UnlinkEntity, ICARUS, the areaportal macro -- because the engine
	// was never told those slots exist and SV_SvEntityForGentity() fatal-errors on a number that
	// high. See MAX_LOGICENTITIES in q_shared.h and G_SpawnLogical() in g_utils.c.
	qboolean	isLogical;

	// DAJ_RP: [Weather] the effect slots of the weather commands this weather entity added (zyk_weather,
	// fx_rain, fx_snow, fx_spacedust, fx_wind, an fx_runner whose fxFile starts with *), one bit each: what
	// another one asking for the same weather is told it would add nothing by -- RP_WeatherAdd, g_misc.c
	uint64_t	rpWeatherSlots;

	// DAJ_RP: [Locks] the "lock" key: the name of the account list in the map's lock file that may use this
	// entity (g_locks.c). Only the Entity System's entities have one; the map's own are locked by brush
	// model in the file instead. NULL when it has none.
	char		*rpLock;
	// DAJ_RP: [Locks] who set this entity off through /entuse, /enttrigger or a level 5 Stun Baton (or
	// through something they set off): 1 + that activator's number, and the client's pers.enterTime, so a
	// later player in the same slot is not taken for them. What this entity uses later -- after a delay,
	// when a door arrives -- with that same activator passes every lock, as the use itself did. 0: none.
	int			rpLockPassBy;
	int			rpLockPassStamp;

	// GalaxyRP: [Logical Entities] the slot number this entity would have been given had every
	// map entity been networked -- what its number WAS before this feature. Assigned only to
	// entities allocated while the map is spawning (level.spawning), by a small simulation of the
	// old allocator that runs beside the real one (RP_LegacySlotAssign/Release in g_utils.c).
	// 0 for anything spawned later. It was written for the SP-map fix-ups in G_InitGame that
	// picked entities by number, as New Zyk mod still does; those now match by classname and
	// brush model instead (see RP_IsBrushEntity() in g_main.c), so nothing reads it today. Kept
	// because it reproduces New Zyk mod's numbering exactly -- the way to find out which entity
	// one of its slot numbers means before porting a fix from it.
	int			legacySlot;

	// GalaxyRP: [Entity System] misc_bsp bookkeeping. rpBSPInstance is a misc_bsp's own instance
	// number (level.mNumBSPInstances when it spawned -- never reused within a map, unlike a slot);
	// rpSubBSPOf is set on every entity spawned from a misc_bsp's sub-BSP entity list to that
	// misc_bsp's instance. /entsave does not write those entities -- the misc_bsp rebuilds them when
	// it spawns again, which is the only way their "*N" models resolve (they count in the sub-BSP's
	// own numbering) -- and /entremove and /entedit on a misc_bsp take them along. 0 otherwise.
	// Both are zeroed with the rest of the entity when it is freed. See SP_misc_bsp() in g_misc.c.
	int			rpBSPInstance;
	int			rpSubBSPOf;

	// GalaxyRP: [Entity System] an entity another entity made for itself and that has no record of its
	// own -- a misc_turret's top, a misc_trip_mine's laser trap -- goes with its maker when the entity
	// commands remove it or spawn it again in place (RP_FreeEntityChildren), as a door's trigger does.
	// The link is the maker's slot plus a stamp the maker took when it first made one (RP_MarkChild);
	// a slot freed and reused is zeroed, so a stale link never matches. Same idea as zyk_spawner_id.
	int			rpMakerStamp;		// on a maker: its stamp, 0 until it has made a child
	gentity_t	*rpMadeBy;			// on a child: the maker's slot
	int			rpMadeByStamp;		// on a child: the maker's stamp at the time

	// GalaxyRP: [Entity System] true on an entity the map itself put there -- its own entity string or
	// a per-map fix -- which /entcut and /entrotate leave alone (/entcopy may copy it; the copy is not
	// one). Set once the map has loaded, and given back by the entity-file loader to a line identical
	// to one of those records (level.rp_map_fingerprints), so a preset does not strip it. See
	// RP_MarkMapEntities() in g_entgrab.c. Zeroed with the entity when it is freed; an in-place respawn
	// (/entedit) keeps it.
	qboolean	rpMapEntity;
	// GalaxyRP: [Entity System] 1 + the client number of the admin holding this entity with /entcopy or
	// /entcut, 0 otherwise. Zeroed with the entity, so a freed or reused slot can never pass for the
	// held one. See g_entgrab.c.
	int			rpHeldBy;

	// GalaxyRP: [SP Maps] true on a misc_turret spawned on a single-player map as the turret that
	// classname means there -- a misc_turretG2 (SP_misc_turret() in g_turret.c). It then behaves as
	// single player's does towards rpSpTurretTeam, its "team" key: it does not target clients of
	// that team (client->playerTeam) and takes no damage from them (G_Damage()). qfalse, and the
	// team unread, on every other entity.
	qboolean	rpSpTurret;
	npcteam_t	rpSpTurretTeam;

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

	// GalaxyRP fix: [Entity System] which spawner made an NPC, so /entsave can leave an NPC to a
	// spawner that is itself being saved instead of writing it a second time (it came back doubled
	// on every reload). zyk_spawner_id is given to a spawner by NPC_Spawn_Do() the first time it
	// spawns; the NPC keeps a pointer to the spawner's slot and that id. A slot that has since been
	// freed and reused is zeroed by G_FreeEntity(), so its id no longer matches and the link is
	// ignored -- the NPC is then saved as before. See zyk_entsave_npc_skip_reason() in g_cmds.c.
	int			zyk_spawner_id;			// on a spawner: its id, 0 until it has spawned
	gentity_t	*zyk_npc_spawner;		// on an NPC: the spawner that made it
	int			zyk_npc_spawner_id;		// on an NPC: that spawner's zyk_spawner_id at the time

	// GalaxyRP: [Entity System] on an NPC or vehicle spawner: its "respawn" key -- fired again when an
	// NPC or vehicle it made dies (RP_SpawnerRespawn() in NPC_spawn.c) -- and the respawns waiting for
	// the one it is already counting down to (a spawner has one think: a second fire would replace it)
	qboolean	rpSpawnerRespawn;
	int			rpSpawnerQueued;

	// GalaxyRP: [Entity System] on a target_speaker with a soundSet: the distance within which a client is
	// sent it (its set's radius + RP_SPEAKER_AUDIBLE_MARGIN), 0 on anything else -- RP_SpeakerAudienceFrame()
	float		rpAudibleRange;

	// GalaxyRP: [Entity System] on an npc_spawner: its "npceffect" key, the /npc effect mode every NPC it
	// makes starts with (an RP_PHASE_* value, RP_PHASE_NONE for none) -- see RP_NpcEffectFromName()
	int			rpSpawnerEffect;

	// DAJ_RP: [Phase] on a body-queue corpse: it was laid down by a player under /admhidden, so it is hidden
	// as he was and its severed limbs are not sent -- set (or cleared) on every CopyToBodyQue()
	qboolean	rpHiddenCorpse;

	// GalaxyRP: [Entity System] on an npc_spawner or NPC_Vehicle: its "npcteam" key, the /npc team every NPC or
	// vehicle it makes is given (an npcteam_t plus one, 0 for none) -- see RP_NpcTeamFromName(). Copied onto the
	// new NPC by NPC_Spawn_Do(), where NPC_Begin() applies it and sets it back to 0.
	int			rpSpawnerTeam;

	// DAJ_RP: [NPC System] on an npc_spawner: its "npcfreeze" key -- every NPC it makes starts under /npc freeze.
	// Applied by NPC_Spawn_Do() to the new NPC there and then (RP_NpcFreeze()), never on a vehicle.
	qboolean	rpSpawnerFreeze;

	// GalaxyRP: [Entity System] on an NPC or vehicle: the team it had at the end of its start-up, after its type's
	// defaults and before any "npcteam" -- RP_TEAM_RECORD() of its playerTeam and enemyTeam, 0 until NPC_Begin()
	// has run. /entsave writes "npcteam" on a one-time line only when its team differs from this, so an NPC whose
	// team nobody changed comes back exactly as its type makes it.
	int			rpBaseTeam;

	// DAJ_RP: [NPC Rewards] on an npc_spawner: its "npccredits" and "npcxp" keys, the credits and XP the
	// logged-in player who kills one of its NPCs is given (RP_SpawnerRewardKeys(), NPC_spawn.c). Copied onto
	// each NPC by NPC_Spawn_Do(); on the NPC, what its death pays out (RP_PayNpcKillReward(), g_combat.c),
	// set back to 0 once paid. 0 is none. Never set on a vehicle.
	int			rpRewardCredits;
	int			rpRewardXP;

	// DAJ_RP: [NPC Scale] on an NPC or vehicle spawner: its "npcscale" key, the size of each NPC or vehicle it makes in
	// percent, RP_NPC_SCALE_MIN..RP_NPC_SCALE_MAX, 0 for none (RP_SpawnerScaleKey(), NPC_spawn.c). Copied onto each one
	// by NPC_Spawn_Do(), which scales it with it; on the NPC, the size its spawner gave it, so /entsave can write it.
	int			rpNpcScale;
};

// DAJ_RP: [NPC Rewards] the most an npc_spawner's "npccredits" and "npcxp" may pay for one kill
#define RP_REWARD_MAX_CREDITS	100000
#define RP_REWARD_MAX_XP		100

// DAJ_RP: [Scale] the sizes a player's /scale takes, in percent (100 is normal) -- g_cmds.c
#define RP_SCALE_MIN			20
#define RP_SCALE_MAX			500

// DAJ_RP: [NPC Scale] the sizes an NPC or vehicle spawner's "npcscale" takes, in percent (100 is normal): a player's, a
// value outside them brought to the nearer end (RP_NpcScaleValue(), NPC_spawn.c)
#define RP_NPC_SCALE_MIN		RP_SCALE_MIN
#define RP_NPC_SCALE_MAX		RP_SCALE_MAX

// DAJ_RP: [NPC Scale] the sizes that can be sent to the clients at all, in percent: iModelScale goes in 10 bits, so 1024
// arrives as 0 (normal size) and anything bigger wrapped round. A .npc file's own scale, a scripted one and a vehicle's
// droid are kept within these.
#define RP_SENT_SCALE_MIN		1
#define RP_SENT_SCALE_MAX		1023

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

	qboolean loggedin;
	qboolean motdSeen; // Tr!Force; [Motd] Server motd seen

	int accountID;

	char filename[32]; // zyk: player account filename

	char rpgchar[32]; // zyk: file name of the RPG char

	// GalaxyRP fix: [Magic] selected_special_power, selected_left_special_power,
	// selected_right_special_power, magic_fist_selection, magic_disabled_powers and
	// magic_more_disabled_powers used to be here: the Magic Master's power selection. They were
	// only ever saved and restored with the session (never read), and the magic system is gone,
	// so they went; the session string is six numbers shorter (see g_session.c).

	// zyk: vote timer, used to avoid vote spam
	int vote_timer;

	//GalaxyRP (Alex): [Ammo Recharge] Ammo recharge timers. These get incremented every second and checked against the cvars.
	int weapon_recharge_timer;
	int explosive_recharge_timer;

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


// GalaxyRP: [cleanup] names for the player_statuses bitfield below, adopted from the New Zyk Mod's
// PLAYER_STATUS_* idea but numbered to OUR layout, not his. Use as (1 << PLAYER_STATUS_X).
//
// GalaxyRP fix: [Dead Code] seventeen bits nothing set any more, or whose only reader was a
// client that ignored the result, have been dropped and the rest renumbered: SENT_RADAR_EVENT,
// SENT_JETPACK_FLAME_EVENT, SABER_ARMOR, GUN_ARMOR, HEALING_CRYSTAL, ENERGY_CRYSTAL,
// SENT_FORCE_USER_EVENT, SENDING_MAGIC_POWER_EVENT, SENDING_IMMUNITY_EVENT,
// SENDING_ULTRA_STRENGTH_EVENT, SENDING_ULTRA_RESISTANCE_EVENT, UNIQUE_ABILITY_1/2/3,
// ICE_BOMB_HIT, RPG_TUTORIAL and CUSTOM_QUEST_NPC, whose features (the quest crystals, the RPG
// event cascade, the unique abilities, the ice bomb, the RPG tutorial, the custom-quest NPCs)
// are all gone. Renumbering is
// safe: player_statuses is never saved anywhere -- not in the session string, the account database
// or an entity file -- and both ClientConnect and ClientDisconnect zero the whole field.
typedef enum {
	PLAYER_STATUS_SILENCED = 0,              // silenced by an admin
	PLAYER_STATUS_EMOTE,                     // using an emote
	PLAYER_STATUS_SCALED,                    // /scale set a model scale other than 100
	PLAYER_STATUS_CHAT_PROTECTION,           // chat protection is active for this player
	// Downed: lying incapacitated and unable to act. Set both by the Death System (a lethal hit that
	// downs instead of killing) and by the admin /paralyze command. PLAYER_STATUS_ADMIN_PARALYSIS
	// says which -- this bit alone is a combat knockdown, this bit plus that one is an admin
	// paralysis. Read it through G_PlayerIsDowned().
	PLAYER_STATUS_DOWNED,
	PLAYER_STATUS_ADM_GIVE_FORCE,            // /give <player> force handed this player force powers
	PLAYER_STATUS_ADM_GIVE_GUNS,             // /give <player> guns handed this player weapons
	PLAYER_STATUS_NPC_ORDER_GUARD,           // NPC has the guard order
	PLAYER_STATUS_NPC_ORDER_COVER,           // NPC has the cover order
	PLAYER_STATUS_POISON_DART_HIT,           // taking poison dart damage over time
	// Paralyzed by an admin, as opposed to downed in combat. Always set together with
	// PLAYER_STATUS_DOWNED, never on its own, so /getup and /helpup can revive a combat knockdown
	// while refusing an admin punishment. Reused from the removed /nofight command; safe because
	// nothing read the old bit any more, and both ClientConnect and ClientDisconnect zero the whole
	// field, so no stale bit survives a rejoin. Read it through G_PlayerIsAdminParalyzed().
	PLAYER_STATUS_ADMIN_PARALYSIS,
	PLAYER_STATUS_DUEL_TOURNAMENT_LOSS       // has just lost his duel in the Duel Tournament
} playerStatus_t;

// GLX_U: [Cosmetics] a hat or cape name: up to 13 characters and the NUL, TaystJK's own
// MAX_COSMETIC_LENGTH, so a name either side accepts is accepted by the other.
#define RP_COSMETIC_NAME_SIZE	14
#define RP_COSMETIC_HAT			0	// rides on color1, drawn at *head_top
#define RP_COSMETIC_CAPE		1	// rides on color2, drawn at *back

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

	// GalaxyRP: [Saber RGB] the server-authoritative custom blade colour for each saber slot,
	// stored as a packed 24-bit r|g<<8|b<<16 value, or 0 for "no custom colour, use the ordinary
	// palette entry the client asked for". Set by /sabercolor, by an RGB-capable client's
	// cp_sbRGB1/cp_sbRGB2 userinfo cvars, or restored from the character's database row on login;
	// published to every other client through the "c3"/"c4" clientinfo configstring keys.
	int			saberRGB[2];

	// GalaxyRP: [Saber RGB] the server-authoritative saber_colors_t mode (0-11) selected for each
	// saber slot -- separate from saberRGB[] above, since a blade style (classic RGB vs. one of the
	// Flame/Electric variants vs. black) and its custom colour are now set independently (/sabercolor
	// vs. /saberblade). This is the field published through the "c1"/"c2" clientinfo configstring
	// keys; unlike saberRGB[], 0 is a legitimate, meaningful value here (SABER_RED), not "unset".
	int			saberColorMode[2];

	// GLX_U: [Cosmetics] the hat (slot 0) and cape (slot 1) the player wears, "" for none. They ride
	// on color1/color2 after the saber number, as TaystJK sends them ("5santahat"), and are published
	// the same way in "c1"/"c2", so TaystJK's own cgame shows them too. Read from the userinfo in
	// ClientUserinfoChanged(), and restored from the character's saber colour columns on login.
	char		cosmetic[2][RP_COSMETIC_NAME_SIZE];

	int			vote, teamvote; // 0 = none, 1 = yes, 2 = no

	char		guid[33];

	// zyk: account system attributes

	// zyk: a bitfield of playerStatus_t values -- see the enum above this struct.
	int player_statuses;

	// GalaxyRP: [Phase] /admwalkthrough, /admghost, /admholo, /admhidden -- an rpPhaseMode_t (bg_public.h). In pers, so
	// it survives death and respawn (ClientSpawn keeps pers); ClientConnect zeroes the whole client,
	// so a map change or a reconnect clears it, and RP_ClearPhaseMode() clears it on /login, /logout,
	// /new and /char. phase_releasing is the tail of a mode that has just been turned off: the player
	// keeps passing through bodies, and still sends RP_PHASE_NONSOLID, until nothing overlaps them --
	// see RP_PhaseUpdate() in g_active.c.
	int phase_mode;
	qboolean phase_releasing;

	// zyk: used to backup player force powers before some event that does not allow them. They will be restored after event ends
	int zyk_saved_force_powers;
	int zyk_saved_force_power_levels[NUM_FORCE_POWERS];
	// GalaxyRP fix: [Duel Tournament] says whether the force backup above actually holds this
	// player's powers. Without it player_restore_force() would happily apply a backup that was
	// never taken: a duelist chosen in mode 2 but invalidated before mode 3 prepares them (they
	// disconnect, go spectator, die) reaches the mode-5 restore having never been backed up, and
	// zyk_saved_force_powers is zero for anyone who has not duelled before -- so the restore
	// stripped every force power they had.
	qboolean zyk_saved_force_valid;

	// GalaxyRP fix: [Duel Tournament] duel_tournament_prepare() takes a duelist's weapons, ammo
	// and holdable items away, and nothing gave them back. player_restore_force() restores only
	// force powers (plus melee), and duel_tournament_end() touches no client at all -- it is pure
	// level teardown, which is what makes it safe to call from ClientDisconnect. A duelist who
	// LOST was covered by accident, because dying respawns them; a duelist who SURVIVED kept a
	// stripped loadout until they next died or the map changed. Backed up here so the restore can
	// be the exact inverse of the strip, the way the force pair above already is.
	int zyk_saved_weapons;
	int zyk_saved_ammo[MAX_AMMO];
	int zyk_saved_holdable_items;
	int zyk_saved_holdable_item;
	qboolean zyk_saved_loadout_valid;

	// GalaxyRP fix: [Quests] quest_afk_timer used to be declared here. Its only writer was inside
	// choose_new_player (deleted as unreachable dead code -- see the GalaxyRP fix comment on its old
	// location in g_cmds.c), and its only reader had already been removed in an earlier change (see
	// the GalaxyRP fix comment in g_active.c), leaving it completely unused. Removed outright.

	// zyk: amount of times player must be hit by poison
	int poison_dart_hit_counter;

	// zyk: player who hit the target with poison dart
	int poison_dart_user_id;

	// zyk: timer of the poison darts
	int poison_dart_hit_timer;

	// GalaxyRP fix: [Dead Fields] wrist_shot_counter (Wrist Shot ability) used to be here. It was
	// declaration-only -- nothing in the tree ever read or wrote it.

	// GalaxyRP fix: [Dead Fields] ice_bomb_counter (Ice Bomb ability) went the same way.

	// zyk: cooldown time to buy or sell
	int buy_sell_timer;

	// GalaxyRP: [Dice] cooldown time for /roll, /rollall, /flipcoin and /flipcoinall, shared by all
	// four so a player cannot sidestep it by alternating commands. Modelled on buy_sell_timer above.
	int dice_roll_timer;

	int player_scale;

	// zyk: chat protection cooldown timer. After this time, player will be protected against damage
	// GalaxyRP fix: [gameplay] holds the level.time at which the player opened chat (0 = not talking),
	// not the deadline -- see the block in ClientTimerActions() for why. Reset to 0 by ClientSpawn().
	int chat_protection_timer;

	// GalaxyRP fix: [Shop] seller_invoked_by_id removed -- it only supported Cmd_CallSeller_f
	// (/callseller), which has itself been removed for good (see g_cmds.c).

	// zyk: point marked in map so player can teleport to this point
	vec3_t teleport_point;
	vec3_t teleport_angles;

	int	bitvalue; // zyk: player is considered as admin if bitvalue is > 0, because he has at least 1 admin command
	
	int level; // zyk: RPG mode level
	// GalaxyRP fix: [Dead Fields] level_up_score used to be here -- declaration-only. (The UI cvar
	// ui_zyk_rpg_level_up_score, unrelated, has since gone too: nothing read or wrote it.)
	int xp;
	int skillpoints; // zyk: RPG mode skillpoints

	char description[MAX_STRING_CHARS];
	char password[32]; // zyk: account password

	//GalaxyRP (Alex): [Training Saber] Values for training saber.
	// GalaxyRP: [Training Saber fix] one stored damageScale/damageScale2 pair per saber slot
	// (index 0 = saber1, index 1 = saber2) so /training covers a player's second saber, not just
	// their primary one. Declared as float to exactly match saberInfo_t::damageScale/damageScale2
	// (bg_public.h) -- these used to be int, which silently truncated any fractional damage scale
	// on store and reapplied the wrong value on restore.
	qboolean training_mode;
	float training_stored_damageScale[MAX_SABERS];
	float training_stored_damageScale2[MAX_SABERS];

	// zyk: turn on or off features of this player in his account file. It is a bit value attribute
	// Possible bit values are:
	// GalaxyRP fix: [Shop] bits 0/1/2 (formerly "RPG quests"/"Light Power"/"Dark Power", already
	// removed and free; briefly the 3 permanent shop upgrades kept by the /buy item|upgrade refactor)
	// are now retired from this field entirely and permanently unused/free again. player_settings is
	// account-wide (Accounts.PlayerSettings, shared by every character on the account), which made the
	// 3 shop upgrades account-wide too -- buying one on any character silently granted it on every other
	// character on the same account. The 3 upgrades now live in pers.skill_levels[38] instead (the dead,
	// per-character UniqueSkill DB column -- see its doc comment on skill_levels below), keeping the same
	// bit values 0/1/2. See the matching GalaxyRP fix comments in Cmd_Buy_f (g_cmds.c) and every other
	// reader migrated alongside it (g_combat.c, g_weapon.c, bg_pmove.c, g_active.c, g_items.c). Like the
	// other already-retired bits below (7, 12, 14, 15), bits 0-2 were never reachable via /settings in
	// the first place -- settings_number_to_bit[] in Cmd_Settings_f (g_cmds.c) never mapped any
	// player-facing settings number to bit 0, 1, or 2 -- so no /settings-side change was needed to
	// "disable" them; this doc update is the only thing marking them retired here.
	// 3 - Eternity Power
	// 4 - Universe Power
	// 5 - Sense Health Toggle (/settings 1). Used to be "Custom Language", whose only reader went with
	//     the RPG tutorial; reused rather than retired. Inverted (clear == ON, set == OFF), and new
	//     accounts are created with it SET so it starts OFF -- see insert_accounts_table_row() (g_cmds.c).
	//     Existing accounts keep whatever the old Language choice left: English (clear) reads as ON.
	// GalaxyRP fix: [Settings] bit 6 used to be "Allow Force Powers from allies" -- that setting was
	// removed from /settings entirely (force powers between allies are now always allowed, see the fix
	// comment at its old gate in w_force.c's ForcePowerUsableOn()), freeing the bit for reuse rather
	// than retiring it. It is now the Use Hint (/settings 4): whether the gfx/hud/useableHint hand
	// icon is drawn while looking at a usable world entity. Inverted like the other toggles here
	// (clear == ON, set == OFF). New accounts are created with this bit CLEAR, so the hint starts ON
	// -- see the hardcoded default in insert_accounts_table_row() (g_cmds.c). (It used to be SET there,
	// starting new accounts OFF.)
	// IMPORTANT, for anyone reusing a "free" bit after this one: free of readers is not the same as
	// free of data. Bit 6 was /settings 2, "Allow Force Powers from allies", a live player-facing
	// toggle right up until the settings cleanup that retired it -- so accounts that predate this
	// feature hold whatever that player last set, not zero. In practice a pre-existing account starts
	// with the hint ON or OFF depending on a choice its owner made about an unrelated setting years
	// ago. That is accepted here rather than migrated (a deliberate call), and /settings 4 reports and
	// changes the current state either way. Bits 17-25 and 30-31 have never been used by any setting
	// in this mod's history and are the ones to reach for when a predictable default actually matters.
	// (Bit 16 was one of them until /settings 5 took it -- see 16 below.)
	// 6 - Use Hint (/settings 4)
	// 7 - Show magic cast in chat
	// GalaxyRP fix: [Settings] bit 9 ("Allow Screen Message") documentation removed here -- that
	// setting has been removed from /settings entirely; the screen message is now always shown (see
	// the fix comment at its old gate in g_client.c).
	// GalaxyRP fix: [Settings] bit 10 ("Use healing force only at allied players") documentation
	// removed here -- that setting has been removed from /settings entirely; Team Heal and Team
	// Energize in FFA now always restrict to allies outside non-RPG mode (see the fix comments at
	// their eligibility checks in w_force.c).
	// GalaxyRP fix: [Settings] bit 11 used to be "Start With Saber" -- that setting was removed from
	// /settings entirely (a player who owns a saber now always starts with it equipped, see the fix
	// comment at its old gate in zyk_load_common_settings()), freeing the bit up for reuse rather than
	// retiring it. It's now "Activate Saber on Spawn" (/settings 3): whether the blade is ignited
	// immediately on spawn (the default/ON) or starts selected but not ignited (OFF) -- see the gate
	// alongside the weapon-selection code in zyk_load_common_settings().
	// 11 - Activate Saber on Spawn (/settings 3)
	// GalaxyRP fix: [Settings] bit 12 ("Jetpack") documentation removed here -- that setting has been
	// removed from the game entirely (its bit was only ever read back by its own status line, never by
	// anything gating actual jetpack availability).
	// 13 - Admin Protect (/settings 2)
	// 16 - Ignore Chat Distance (/settings 5). DAJ_RP: never used before, so every older account holds a
	//      0 here and reads as ON (inverted: clear == ON, set == OFF) -- existing admins keep hearing
	//      everything. New accounts are created with it SET (OFF). Only has an effect together with the
	//      ADM_IGNORECHATDISTANCE admin power; see zyk_ignores_chat_distance() (g_cmds.c).
	// GalaxyRP fix: [Settings] bits 14 ("Boss Battle Music") and 15 ("Difficulty") documentation
	// removed here — those settings have been removed from the game entirely.
	// GalaxyRP fix: [Settings] bits 26-29 ("Starting Single Saber Style") documentation removed here
	// -- that setting has been removed from /settings entirely; starting single saber style is now
	// always whatever the base game/skill progression would pick, with no per-player override (see the
	// fix comment at its old cycling logic in zyk_load_common_settings()).
	int player_settings;

	// GalaxyRP fix: [RPG Class] rpg_class field (and its class-value documentation) removed here —
	// it is permanently 0 with zero live readers/writers left anywhere in the codebase.



	// zyk: used by Fast Dash ability
	// GalaxyRP fix: [Magic] fast_dash_timer used to be here. Its only writers were zyk_force_dash()
	// (g_main.c) and zyk_do_force_dash() (g_active.c), both removed as orphans; nothing read it.

	// GalaxyRP fix: [Dead Fields] unique_skill_user_id (Aimed Shot ability) used to be here --
	// declaration-only.

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

	// GalaxyRP (Alex): [Armor Skill] cooldown for the Armor skill's blaster-deflect proc (see
	// zyk_can_deflect_shots() in g_cmds.c) -- only one deflect attempt can succeed per cooldown
	// window, mirroring the existing Saber Defense "one block per ~200ms" throttle in g_missile.c
	int armor_deflect_timer;

	// zyk: RPG skills
	// GalaxyRP fix: [Shop] index 38 (UniqueSkill, "Unique Skill" in the skills[] table, g_cmds.c) is a
	// dead skill slot -- the last remnant of the fully-removed /unique Unique Abilities system, blocked
	// from purchase/leveling by do_upgrade_skill(), zeroed for new characters, excluded from the skill
	// display listing. Repurposed as a per-character bitmask (same 0/1/2 bit values formerly used in
	// player_settings, see that field's doc comment above) for the 3 permanent shop upgrades kept by the
	// /buy item|upgrade refactor: bit 0 Holdable Items Upgrade, bit 1 Impact Reducer, bit 2 Stun Baton
	// Upgrade. Unlike player_settings, this array is per-character (Skills.UniqueSkill, keyed by CharID)
	// rather than account-wide, so owning an upgrade on one character no longer leaks onto every other
	// character on the same account. Already saved/loaded generically alongside every other skill (the
	// NUM_OF_SKILLS load loops and the UniqueSkill column in update_skills_query/update_character_query),
	// so no new DB read/write path was needed for the migration.
	int skill_levels[NUM_OF_SKILLS];

	int max_rpg_health; // zyk: max health the player can have in RPG Mode. This is set to STAT_MAX_HEALTH for RPG players
	int max_rpg_shield; // zyk: max shield the player can have in RPG Mode based in the skill_levels[30] value

	int jetpack_fuel; // zyk: now this is the fuel that is spent. Then we scale this value to the 0 - 100 range to set it in the jetpackFuel attribute to show the fuel bar correctly to the client

	int sense_health_timer; // zyk: used to periodically show health of player or npc with Sense Health skill

	int flame_thrower; // zyk: used by stun baton. Its the flame thrower timer
	
	// zyk: tests if this player or npc is being mind controlled by a player. If -1, means that he is not being controlled, otherwise it has the player id of the player who is controlling this player or npc
	int being_mind_controlled;

	// zyk: entity ids of the mind controlled entities. Default -1, which means player is not controlling anyone
	int mind_controlled1_id;

	// GalaxyRP fix: [Dead Fields] the secrets_found bit flag used to be declared at the end of this
	// list. It was declaration-only -- never written, and never actually written to the database
	// either; the three upgrades that still mattered moved to player_settings bits 0/1/2. The list
	// below is KEPT as reference: thirteen comments across g_items.c, g_weapon.c, g_combat.c,
	// bg_pmove.c and g_cmds.c cite these bit numbers when explaining where each upgrade went.
	// Possible bit values (1 << bit_value) were:
	// 0 - Holdable Items Upgrade
	// 1 - Unused (was Bounty Hunter Upgrade, removed as inert/non-functional -- GalaxyRP fix: [Upgrades])
	// 2 - Unique Ability 1
	// 3 - Unique Ability 2
	// 4 - Unique Ability 3
	// 5 - Unused
	// 6 - Unused
	// 7 - Unused (was Stealth Attacker Upgrade, removed as inert/non-functional -- GalaxyRP fix: [Upgrades])
	// 8 - Unused (was Force Gunner Upgrade, removed as inert/non-functional -- GalaxyRP fix: [Upgrades])
	// 9 - Impact Reducer
	// 10 - Unused (was Flame Thrower Upgrade, removed as inert/non-functional -- GalaxyRP fix: [Upgrades].
	//      Flamethrower gameplay is entirely gated by skill_levels[57] instead; unrelated to this bitfield.)
	// 11 - Unused (was Power Cell Weapons Upgrade, removed as inert/non-functional -- GalaxyRP fix: [Upgrades])
	// 12 - Unused (was Blaster Pack Weapons Upgrade, removed as inert/non-functional -- GalaxyRP fix: [Upgrades])
	// 13 - Unused (was Metal Bolts Weapons Upgrade, removed as inert/non-functional -- GalaxyRP fix: [Upgrades])
	// 14 - Unused (was Rocket Upgrade, removed as inert/non-functional -- GalaxyRP fix: [Upgrades])
	// 15 - Stun Baton Upgrade
	// 16 - Unused (was Armored Soldier Upgrade, removed as inert/non-functional -- GalaxyRP fix: [Upgrades])
	// 17 - Unused (was documented as Jetpack Upgrade, but had zero code references anywhere -- actual
	//      Jetpack Upgrade gameplay is gated by skill_levels[34], unrelated to this bitfield)
	// 18 - Unused
	// 19 - Unused (was Force Guardian Upgrade, removed as inert/non-functional -- GalaxyRP fix: [Upgrades])

	// GalaxyRP (Alex): [Telemark] Saving the coordinates here
	vec3_t saved_origin;
	vec3_t saved_view_angles;

	// GalaxyRP fix: [Dead Fields] bounty_hunter_sentries and bounty_hunter_placed_sentries used to be
	// here, counting the Bounty Hunter's starting and placed sentries. Both were declaration-only.

	int max_force_power; // zyk: max force power the player can have based on skill_levels[54] value

	// DAJ_RP: [NPC Rewards] score_modifier and credits_modifier used to sit here. Kills wrote them -- an NPC's
	// own credits_modifier came from its spawner's spawnflags 32768 and genericvalue7 -- and nothing ever read
	// them back, so no kill paid anything. NPC kill rewards are gentity_t::rpRewardCredits/rpRewardXP now.

	int credits; // zyk: the amount of credits (RPG Mode currency) this player has now
	int CharID;


	// GalaxyRP: [Race Mode] race_position used to be declared here -- the racer's starting-grid slot,
	// written only by Cmd_RaceMode_f and read only by the race handlers. Removed with Race Mode.

	// GalaxyRP fix: [Quests] can_play_quest used to be declared here -- "if 1, player can play a quest
	// now". It never became 1: the only assignment anywhere in the tree was the `= 0` reset in
	// initialize_rpg_skills, so every `can_play_quest == 1` guard was unreachable. Those guards have
	// been removed one by one over earlier passes (g_utils.c's TryUse quest branch, g_active.c,
	// g_client.c's and g_cmds.c's boss-music resets, g_combat.c); the last one, the /scale refusal in
	// Cmd_Scale_f, went in this pass, so the field itself goes too.

	// zyk: amount of skills used by the player. After a certain amount of uses, player gets 1 experience point (level up score)
	int skill_counter;

	// GalaxyRP fix: [Dead Fields] defeated_guardians used to be here, a bitfield of which quest
	// guardians the player had beaten (bits 4-12, Water through Ice). It was declaration-only:
	// nothing ever wrote it, which is precisely why several dead branches gated on it were removed
	// in earlier passes. Comments elsewhere still name it when explaining those removals.

	// GalaxyRP fix: [Quests] removed hunter_quest_progress and eternity_quest_progress here — both
	// were write-only (only ever reset to 0), with zero readers anywhere in the codebase.

	// GalaxyRP fix: [Quests] universe_quest_progress used to be declared here, the player's step count
	// through the Universe Quest. Nothing in the tree ever wrote it -- no assignment, no database
	// column (the schema has no quest table), no session string, no spawn key -- so it sat at the zero
	// ClientConnect's memset gives it. Its readers all tested for a nonzero step (== 15 for Challenge
	// Mode, == NUM_OF_UNIVERSE_QUEST_OBJ for the completion bonus in player_die), so none could fire.
	// The last of them went in this pass and the field goes with them.

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

	// GalaxyRP fix: [Dead Fields] universe_quest_counter used to be declared here, holding the bit
	// values documented above plus bit 29 for Challenge Mode. It was declaration-only; nothing ever
	// wrote it, which is why the branches gated on it (including the Challenge-Mode-only path noted
	// in g_combat.c) were removed as unreachable. universe_quest_progress, a different field that
	// survived that pass with one live reader, has since gone the same way -- see the note above.

	// GalaxyRP fix: [Quests] removed universe_quest_objective_control here — its sole reader was the
	// dead universe_quest_messages==-10000 block in g_combat.c's player_die(), removed alongside it.

	// GalaxyRP fix: [Quests] universe_quest_artifact_holder_id used to be declared here, naming the npc
	// holding the artifact in the third Universe Quest objective. The only value ever assigned to it
	// anywhere was -1 (the NPC_spawn.c init and the initialize_rpg_skills reset), while both its
	// readers tested `!= -1` -- the per-frame PW_FORCE_BOON top-up in G_RunFrame and the
	// "zyk_quest_artifact" targetname stamp in TossClientItems. Neither could ever fire; both went in
	// this pass, along with the field.

	// GalaxyRP fix: [Quests] universe_quest_messages and universe_quest_timer used to be declared here,
	// driving the Universe Quest's timed events. Both were write-only: the only values ever assigned
	// were 0 (the initialize_rpg_skills resets) and, for the timer, a cooldown bump in magic_disable's
	// npc branch. Their readers all tested negative sentinels (-10000, -2000) that nothing ever wrote.
	// Those readers were removed in earlier passes; the writers and the fields go in this one.

	// GalaxyRP fix: [Magic] the quest-power state used to live here: quest_power_status (the
	// "using / hit by power N" bitfield), quest_power_usage_timer, the hit counters, the
	// quest_power*_timer / quest_target*_timer / quest_power_user*_id groups,
	// quest_debounce1_timer, quest_power_effect1_id and magic_power itself. Every writer other
	// than a reset lived in the magic effect functions, which have been deleted; every reader
	// in live code tested a bit nothing could set any more and has been simplified away (G_Damage,
	// ClientThink_real, ClientTimerActions, PM_CheckJump, PmoveSingle, the force-power checks in
	// w_force.c, Player_FireFlameThrower, TryGrapple).

	// GalaxyRP fix: [Quests] light_quest_messages and light_quest_timer used to be declared here,
	// driving the Light Quest's timed events. Both were write-only by the end -- reset to 0 in
	// initialize_rpg_skills, with the timer also bumped by magic_disable's npc branch -- and no reader
	// survived anywhere in the tree. Removed with the rest of the quest-progress fields.

	// GalaxyRP fix: [Quests] hunter_quest_timer and hunter_quest_messages used to be declared here,
	// driving the Dark Quest's timed events. hunter_quest_messages was additionally documented as the
	// Guardian of Universe's "already spawned the other guardians" flag; that second use went with the
	// guardian spawn chain in an earlier pass. Both fields were write-only afterwards -- reset to 0 in
	// initialize_rpg_skills and read nowhere -- so both are removed here.

	// GalaxyRP fix: [Quests] eternity_quest_timer used to be declared here, pacing the Eternity Quest
	// riddles. Write-only by the end: reset to 0 in initialize_rpg_skills, read nowhere in the tree.
	// Removed with the rest of the quest-progress fields.

	// GalaxyRP fix: [Guardian] guardian_mode field (and its boss-value documentation) removed here —
	// it is permanently 0 with zero live readers/writers left anywhere in the codebase (spawn_boss,
	// its sole setter, has no callers).

	// GalaxyRP fix: [Guardian] guardian_timer used to be here, pacing the quest guardians' special
	// abilities. Its last reader went with the quest_mage chain, leaving it write-only; the write
	// inside magic_disable() and the reset in Cmd_... (g_cmds.c) have gone with it.

	// GalaxyRP fix: [Guardian] guardian_invoked_by_id field removed here — it is permanently -1
	// with zero live readers/writers left anywhere in the codebase (spawn_boss, its sole setter,
	// has no callers).

	// Tr!Force: [Plugin] Client plugin check
	qboolean		clientPlugin;

	// GalaxyRP fix: [Model] non-zero while a forced kill triggered by /login, /char use, or /new is
	// waiting to fire -- set by select_player_character()/select_account_and_default_character_data()
	// in g_cmds.c instead of calling G_Kill() immediately, and consumed by ClientThink_real() in
	// g_active.c once level.time reaches it. The delay gives the client time to finish reloading the
	// new Ghoul2 model (triggered by set_model()'s configstring update, which is asynchronous on the
	// client) before the death animation's playerState update reaches it -- calling G_Kill() in the
	// same frame as set_model() could otherwise race the model reload and render as a T-pose instead
	// of the intended death animation.
	int				pending_relog_kill_time;

	// GalaxyRP fix: [Death System] seconds left on a downed player's countdown, decremented once per
	// second by ClientTimerActions() (g_active.c). This lives in pers, not in gclient_s, because it is
	// one half of a state whose other half -- the PLAYER_STATUS_DOWNED and ADMIN_PARALYSIS bits -- is already here.
	// ClientSpawn() preserves pers wholesale but memsets everything else, so while the two were split
	// a respawn kept the "downed" bits and silently zeroed the countdown. That let a downed player skip
	// the remainder of their timer by changing team (SetTeam -> ClientBegin -> ClientSpawn), and it
	// stranded an admin-paralyzed player completely: the auto-release only fires while the countdown is
	// running, and /getup and /helpup both refuse an admin paralysis, so they were stuck with no timer,
	// no message and no way out. Keeping both halves in pers gives them one lifetime.
	int				downedTime;

	// GalaxyRP: [SP Maps] single player's key inventory, per client: the one named security key
	// this player carries (taken from a dead key officer or an item_security_key; used up by the
	// misc_security_panel / func_security_panel whose "message" matches) and how many goodie keys
	// (func_goodie_panel). Cleared on death and on disconnect -- see RP_ClearKeys().
	char			rp_securityKey[MAX_QPATH];
	int				rp_goodieKeys;

	// GalaxyRP: [Entity System] what this admin holds with /entcopy or /entcut -- see g_entgrab.c. In
	// pers so dying and respawning does not drop it; a map change clears it with the rest of pers (and
	// frees every entity anyway). The held entity's own rpHeldBy is what is checked every frame.
	int			entHoldNum;				// entity number held, 0 for none
	int			entHoldMode;			// RP_HOLD_COPY, RP_HOLD_CUT, RP_HOLD_STATIC or RP_HOLD_ADD
	qboolean	entHoldRotated;			// entHoldAngles differs from what the record holds
	vec3_t		entHoldAngles;			// the orientation it will be dropped with
	vec3_t		entHoldOrigin;			// where it is now (moved live) or would land (preview only)
	int			entHoldNextBox;			// level.time the preview is redrawn
	int			entHoldGhost;			// entity number of the preview model, 0 for none
	int			entHoldStatic;			// DAJ_RP: [Static Models] RP_HOLD_STATIC: the static model it copies
	float		entHoldOffset;			// DAJ_RP: [Entity System] RP_HOLD_ADD: /entaddgrab's aimoffset, out from the surface

	// GalaxyRP: [Listings] level.time before which this player's next /list or /maplist listing is
	// refused -- see RP_ListCooldown() in g_rplist.c
	int			rpListNextTime;

	// GalaxyRP: [Slot Reuse] level.time before which this player's next /playsound is refused
	// (RP_PLAYSOUND_COOLDOWN) -- see Cmd_ZykSound_f in g_cmds.c
	int			rpPlaySoundNextTime;

	// GalaxyRP: [Extras] level.time before which a listing this player's Extras menu asks for is not built
	// (RPX_BUILD_COOLDOWN) -- see RP_ListDataCommand() in g_rplist.c
	int			rpxListNextTime;
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
	// GalaxyRP: [Grapple Hook] the two fields above are stock JKA leftovers (Q3's offhand hook) and are
	// live again; these two come from TaystJK. hookHasBeenFired is the edge detector -- one hook per
	// press of BUTTON_GRAPPLE -- and hookFireTime feeds g_hookFloodProtect. All four are owned by
	// ClientThink_real()'s hook block (g_active.c) and Weapon_HookFree() (g_weapon.c).
	qboolean	hookHasBeenFired;
	int			hookFireTime;

	int			switchTeamTime;		// time the player switched teams

	int			switchDuelTeamTime;		// time the player switched duel teams

	int			switchClassTime;	// class changed debounce timer

	// timeResidual is used to handle events that happen every second
	// like health / armor countdowns and regeneration
	int			timeResidual;

	// GalaxyRP fix: [Death System] the downed countdown's own accumulator, deliberately NOT
	// timeResidual. That one is fed by ClientThink_real()'s msec, which is only "time since this
	// client's last think" when Pmove() ran last think and stamped ps.commandTime -- true for a
	// player in the world and for a free-flying spectator, but false for a SPECTATOR_FOLLOW client,
	// whose whole playerState (commandTime included) is overwritten each frame with the followed
	// player's by SpectatorClientEndFrame(). This one is fed by the server frame delta instead
	// (level.time - level.previousTime, in RP_RunDownedTimer(), g_active.c), so a downed player's
	// countdown is served in wall-clock seconds wherever they are and whatever their client is doing.
	// Like timeResidual it is zeroed by ClientSpawn(), so a respawn loses at most the part-second in
	// progress; RP_EnterDownedState() zeroes it too so a countdown always starts on a whole second.
	int			downedTimeResidual;

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
	// GalaxyRP fix: [Cloak Item] separate debounce for /vehicle_cloak's own manual-use cooldown,
	// tracked on the rider (whoever runs the command). Kept distinct from cloakToggleTime, which is
	// reused for: (a) the solo-cloak debounce shared between /use_cloak and the cloak inventory item
	// (same action, same field, on whichever entity's own client this is), and (b) the "just got hit"
	// re-cloak lockout applied by G_Damage's decloak-on-damage check -- that second use applies to a
	// VEHICLE's own client just as much as a player's, so /vehicle_cloak checks the vehicle's
	// cloakToggleTime too (in addition to this field on the rider) before allowing a (re)cloak.
	int			vehicleCloakToggleTime;

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
	int			rpIdleAnim;		// DAJ_RP: [Saber Keys] the idle G_CheckClientIdle last started, while it plays: broken out of like the stock ones

	int			forcePowerSoundDebounce; //if > level.time, don't do certain sound events again (drain sound, absorb sound, etc)
	int			absorbBonusTime; // GalaxyRP fix: [Force] next level.time the Absorb 4+ bonus may be granted against a channelled power (see WP_AbsorbConversion)

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
	// GalaxyRP fix: [gameplay] dedicated debounce timestamp for the SABERATTACKCYCLE handling in
	// PlayerThink_Real() -- see the comment at its use site in g_active.c for why it can't share
	// lastGenCmdTime with the generic (300ms) generic_cmd debounce below.
	int			lastSaberAttackCycleTime;

	struct force {
		int		regenDebounce;
		int		drainDebounce;
		int		lightningDebounce;
	} force;

	int	motdTime; // Tr!Force: [Motd] Server motd time

	// DAJ_RP: [Entity Bounds] per-viewer state of the Entity Bounds display (/settings 6, see
	// g_entbounds.c). Zeroed with the rest of gclient_s when the client connects or spawns, which is also
	// what resets the level.time stamps on a map change.
	int			entBoundsAimed;			// entity number whose box is drawn now, 0 for none
	int			entBoundsAimedStatic;	// DAJ_RP: [Static Models] static model whose box is drawn now, plus 1; 0 for none
	int			entBoundsNextBox;		// level.time the aimed box is redrawn
	int			entBoundsLastBox;		// DAJ_RP: level.time a box was last drawn, 0 for none: the next comes no sooner than 300 ms after
	int			entBoundsNextMarkers;	// level.time the nearby point-entity markers are redrawn
	int			entBoundsNextPrint;		// level.time the aimed entity's centre-print may be (re)sent
	qboolean	entBoundsPrintPending;	// the aimed entity changed and its centre-print is still owed
	qboolean	entBoundsPrinted;		// a centre-print of ours is on screen and needs clearing

	// GalaxyRP fix: [Death System] downedTime used to live here, in gclient_s. It has moved into
	// clientPersistant_t above -- see the comment on it there for why the two halves of the downed
	// state must share a lifetime.
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

typedef struct level_locals_s {
	struct gclient_s	*clients;		// [maxclients]

	struct gentity_s	*gentities;
	int			gentitySize;
	int			num_entities;		// current number, <= MAX_GENTITIES

	// GalaxyRP: [Logical Entities] high-water mark of the logical region, counted from
	// MAX_GENTITIES: slots MAX_GENTITIES .. MAX_GENTITIES+num_logicalents-1 have been handed out at
	// least once this map. Never reported to the engine (trap->LocateGameData only ever gets
	// num_entities). Grown by G_SpawnLogical(), rolled back by G_FreeEntity().
	int			num_logicalents;	// <= MAX_LOGICENTITIES

	// GalaxyRP: [Logical Entities] the legacy-allocator simulation behind gentity_t::legacySlot.
	// legacy_slot_inuse mirrors "inuse" as the old, all-networked allocator would have seen it
	// during the map's spawn pass; legacy_slot_next is its num_entities. Started by
	// RP_LegacySlotsBegin() at the top of the spawn pass, consulted by G_Spawn/G_SpawnLogical and
	// G_FreeEntity only while level.spawning is set.
	byte		legacy_slot_inuse[MAX_GENTITIES];
	int			legacy_slot_next;

	// GalaxyRP: [Logical Entities] rp_logical_entities, read once at G_InitGame. The cvar is
	// latched, but this is the value the map actually started with, so a mid-map change to the
	// cvar can never move an entity between regions while it is alive.
	qboolean	logical_entities_enabled;

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

	// GalaxyRP: [Race Mode] race_mode, race_map, race_start_timer, race_countdown_timer,
	// race_countdown and race_last_player_position used to be declared here. Nothing can raise
	// race_mode any more now that Cmd_RaceMode_f is gone, so all six went with the feature; see the
	// note where that command used to live in g_cmds.c.

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
	// GalaxyRP: [Force Duel] which kind of PRIVATE duel each client last asked for -- 0 an ordinary
	// saber duel, 1 a full force duel. Nothing to do with the Duel Tournament above; it shares this
	// struct because bg_misc.c already includes g_local.h under _GAME and BG_CanUseFPNow() has to
	// read it. Written when a challenge is issued and again when one is accepted, and only ever read
	// while ps.duelInProgress is set -- so a stale entry cannot be reached, because every path that
	// sets duelInProgress writes this first. Zeroed with the rest of level at G_InitGame(), and reset
	// per client in ClientConnect()/ClientDisconnect() so a reused slot cannot inherit a type.
	int duel_types[MAX_CLIENTS];
	int duel_tournament_model_id; // zyk: model id of the globe
	qboolean duel_arena_loaded; // zyk: tests if the arena is loaded on this map
	vec3_t duel_tournament_origin; // zyk: origin of the duel tournament arena, which has the globe around it. Used to validate position of players. If a duelist leaves the arena, he loses
	int duelist_1_id; // zyk: id of the first duelist
	int duelist_2_id; // zyk: id of the second duelist

	// zyk: the table with all the matches between the duelists, with their ids in first and second position
	// zyk: third is the number of rounds won by first duelist
	// zyk: fourth position is the number of rounds won by second duelist
	int duel_matches[MAX_DUEL_MATCHES][4];

	// zyk: number of rounds already played per match
	int duel_tournament_rounds;

	int duel_matches_quantity; // zyk: quantity of matches in this tournament
	int duel_matches_done; // zyk: how many matches were already done
	// GalaxyRP fix: [Duel Tournament] was declared qboolean, which is `typedef enum { qfalse, qtrue }`
	// -- a two-value enum holding client numbers 0..MAX_CLIENTS-1 plus a -1 sentinel. It worked only
	// because the compiler happens to pick an int-sized underlying type; the values were always out
	// of the enum's range. Now int, matching duel_players[] and the duelist_*_ally_id fields it feeds.

	// GalaxyRP: [Sniper Battle] sniper_mode, sniper_players[MAX_CLIENTS], sniper_mode_timer and
	// sniper_mode_quantity used to be declared here. Nothing can raise sniper_mode any more now that
	// Cmd_SniperMode_f is gone, so all four went with the feature; see the note where that command
	// used to live in g_cmds.c.

	// zyk: Melee Battle
	int melee_mode; // zyk: Default 0. Sets 1 when someone joins, and 2 after battle begins
	int melee_players[MAX_CLIENTS]; // zyk: default -1, when a player joins, sets 0. It is the amount of enemies defeated in Melee Battle
	int melee_mode_timer; // zyk: timer used in Melee Battle
	int melee_mode_quantity; // zyk: amount of players who joined the Melee Battle
	int melee_model_id; // zyk: model id of the catwalk
	qboolean melee_arena_loaded; // zyk: tests if the arena is loaded on this map
	vec3_t melee_mode_origin; // zyk: origin of the melee mode arena, which has the catwalk

	// GalaxyRP: [RPG LMS] rpg_lms_mode, rpg_lms_players[MAX_CLIENTS], rpg_lms_timer and
	// rpg_lms_quantity used to be declared here. Nothing writes or reads them any more -- see the
	// note where Cmd_RpgLmsMode_f used to live in g_cmds.c.

	// GalaxyRP fix: [Quests] bounty_quest_target_id, bounty_quest_choose_target, quest_crystal_id,
	// quest_note_id, universe_quest_note_id, guardian_quest, guardian_quest_timer,
	// initial_map_guardian_weapons, and quest_puzzle_order used to be declared here. They were only
	// ever used by the Guardian/Bounty Quest commands and the general automated quest system's
	// map-note/map-crystal spawning and NPC-dialogue/puzzle interactions, all of which have been
	// deleted as unreachable dead code (see the GalaxyRP fix comments in g_cmds.c, g_main.c, and
	// g_utils.c). Removed outright.

	// GalaxyRP fix: [Guardian] quest_effect_id field removed here — its sole reader/writer,
	// clean_effect() in g_cmds.c, was already deleted as unreachable; the matching level.quest_effect_id
	// init line in g_main.c is removed alongside this one.

	// zyk: default map music. After a boss battle, resets music to this one
	char default_map_music[128];

	// GalaxyRP fix: [Guardian] removed boss_battle_music_reset_timer here — its only non-zero writers
	// sat inside two dead `can_play_quest == 1` guards (can_play_quest can no longer become 1
	// anywhere), so its reader in g_main.c never fired; that reader has also been removed. Note:
	// default_map_music above is now itself write-only (that reader was its only consumer) but is
	// left in place -- untangling it further would mean following level.quest_map's per-map fallback
	// selection logic too, which is out of scope for this pass.

	// GalaxyRP fix: [Magic] special_power_effects[MAX_ENTITIESTOTAL] and
	// special_power_effects_timer[MAX_ENTITIESTOTAL] used to be here -- 32 KB holding, per entity
	// slot, which player owned the magic effect occupying it and when that effect expired. Their
	// only producer was the quest_mage power chain in g_main.c, and that NPC type no longer
	// exists, so every entry was permanently -1. The readers went with them: the two blocks in
	// G_RadiusDamage() (g_combat.c) and clear_special_power_effect() (g_main.c).
	//
	// The comment that stood here justified the MAX_ENTITIESTOTAL sizing with "G_Damage() reads
	// this at attacker->s.number, and a logical target_kill passes itself as the attacker". Both
	// halves were wrong and are not carried forward: G_Damage() never read these arrays (the
	// reader was G_RadiusDamage), and target_kill_use() passes NULL as the attacker, not itself.

	// GalaxyRP: [Race Mode] race_mode_vehicle[MAX_RACERS] used to be declared here, holding the swoop
	// entity ids used to validate racers. Removed with Race Mode, along with MAX_RACERS itself.

	// zyk: the player who called the last vote
	int voting_player;

	// zyk: tests if it is a sp map in loading time
	qboolean sp_map;

	// GalaxyRP: [SP Maps] which single-player campaign this map comes from, by name: a Jedi
	// Academy SP map (the sp_map list) or a Jedi Outcast one (the JO spawn-point table), both in
	// g_main.c. Set before the map's entities spawn and, unlike sp_map, kept until the next map,
	// so what an admin or a preset spawns later is read the same way. Classnames whose meaning
	// differs in single player go by it -- misc_turret (SP_misc_turret() in g_turret.c).
	int rp_sp_game;

	// GalaxyRP: [SP Maps] set on a Jedi Outcast SP map: a spawn point's target -- the map's
	// single-player start scripts, copied onto the added spawn points -- fires for the first
	// player who spawns after the map loads, not for every spawn. See ClientSpawn() and
	// RP_JediOutcastMapFixes() (g_main.c).
	qboolean rp_spawn_target_once;

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
	// GalaxyRP fix: [Entity System] highest inline brush model index the map's own
	// entities referenced, which is what bounds a "*N" typed into /entadd later.
	int zyk_max_inline_model;

	// GalaxyRP fix: [Entity System] the "#name" sub-BSP models the map itself loaded while it
	// spawned. After map load a "#name" is only accepted if it is one of these, because the engine
	// Com_Error(ERR_DROP)s on a sub-BSP file it cannot load and on the 33rd unique one -- see
	// zyk_brush_model_allowed() in g_spawn.c. RP_MAX_SUBBSP_NAMES is the engine's MAX_SUB_BSP.
	char rp_subbsp_names[RP_MAX_SUBBSP_NAMES][MAX_QPATH];
	int rp_num_subbsp_names;

	// GalaxyRP fix: [Entity System] the highest "*N" each of those sub-BSPs' own entities named at map
	// load. A sub-BSP entity's "*N" counts in the sub-BSP's numbering (the engine adds the offset
	// while that sub-BSP is active), so when a misc_bsp rebuilds its entities after map load their
	// models are checked against this, not the main map's bound. rp_active_subbsp is 1 + the slot in
	// rp_subbsp_names of the sub-BSP whose entities are spawning right now (0 none, so the zeroed
	// level needs no setup), and rp_subbsp_spawning_instance the instance number of the misc_bsp
	// spawning them (0 none).
	int rp_subbsp_max_inline[RP_MAX_SUBBSP_NAMES];

	// true once a misc_bsp spawned that sub-BSP's entity list at map load: only then is the list known
	// to parse, and only then does a misc_bsp rebuild it later (a "#name" the map used only as a
	// brush model's model never had its entity list read)
	qboolean rp_subbsp_entities_spawned[RP_MAX_SUBBSP_NAMES];
	int rp_active_subbsp;
	int rp_subbsp_spawning_instance;

	// GalaxyRP: [Entity System] fingerprints of the key/value records of the map's own entities, taken
	// when the map finished loading, sorted -- see gentity_t::rpMapEntity and g_entgrab.c
	uint64_t rp_map_fingerprints[MAX_ENTITIESTOTAL];
	int rp_num_map_fingerprints;
	// GalaxyRP: [Entity System] true once the map and its per-map fixes have spawned
	// (RP_MarkMapEntities): from then on an entity with a record that is not the map's is one the
	// Entity System made -- see RP_EntitySystemMade() in g_spawn.c
	qboolean rp_map_loaded;
	// GalaxyRP: [Entity System] why the last Entity System spawn was refused ("" if it was not),
	// for the command that asked for it to say -- see RP_EntitySystemSpawnRefused() in g_spawn.c
	char rp_spawn_refusal[256];
	// GalaxyRP: [Entity System] the ambient sound set names this server knows, read from sound/sound.txt once
	// per map: 0 not read yet, 1 read, -1 the file could not be read -- see RP_SoundSetKnown() in g_spawn.c
	int rp_soundsets_state;
	// GalaxyRP: [Entity System] a note about the last spawn that went through, for the command to print
	// (an item the map had not precached -- see G_SpawnItem). Cleared with rp_spawn_refusal.
	char rp_spawn_note[256];
	// GalaxyRP: [Entity System] who set the sky portal (CS_SKYBOXORG): 0 nobody, -1 the map's own
	// misc_skyportal, N+1 the Entity System's entity N -- see SP_misc_skyportal() and RP_SkyPortalRelease()
	int rp_skyportal_owner;
	// GalaxyRP: [Entity System] what the md3 file behind each model index holds, read once per map
	// the first time it is asked for (RP_ModelInfo() in g_utils.c): 0 not read yet, 1 read, -1 not
	// readable (no such file, or not an md3 this can trust)
	signed char rp_model_info_state[MAX_MODELS];
	short rp_model_frames[MAX_MODELS];
	vec3_t rp_model_mins[MAX_MODELS];
	vec3_t rp_model_maxs[MAX_MODELS];
	// DAJ_RP: [Static Models] how many of the map's misc_model_static are noted, different models among
	// them, bytes of their paths, and how many did not fit -- the lists are in g_statics.c
	int rp_num_statics;
	int rp_num_static_models;
	int rp_static_names_used;
	int rp_statics_dropped;
	// GalaxyRP: [Listings] the NPC and vehicle type names have been read for /list npcs and /list
	// vehicles this map -- see RP_ListTypes() in g_rplist.c
	qboolean rp_list_npcs_ready;
	qboolean rp_list_vehicles_ready;
	// GalaxyRP: [Listings] level.time before which no player's file listing runs -- a file listing walks
	// every name in every pk3 several times, and the per-player cooldown alone lets every logged-in
	// player start one each second (RP_ListCommand() in g_rplist.c)
	int rp_list_fs_next_time;
	// GalaxyRP: [Extras] the listings kept for the Extras menus were emptied for this map -- see
	// RP_ListDataCommand() in g_rplist.c
	qboolean rp_rpx_cache_ready;

	// GalaxyRP fix: [Spawning] where the map's first info_player_deathmatch stood when the map
	// finished loading. Spawn selection falls back to it, instead of ending the server with
	// "Couldn't find a spawn point", if every spawn point has since gone -- see
	// RP_FallbackSpawnPoint() in g_client.c.
	qboolean rp_fallback_spawn_set;
	qboolean rp_fallback_spawn_warned;

	// GalaxyRP fix: [Entity System] GlobalUse() has logged a use loop it cut short on this map --
	// once per map is enough to find it (g_utils.c)
	qboolean rp_use_limit_warned;
	int rp_use_depth;	// GlobalUse(): uses nested inside each other right now
	int rp_use_chain;	// GlobalUse(): uses so far under the current outermost one
	vec3_t rp_fallback_spawn_origin;
	vec3_t rp_fallback_spawn_angles;

	qboolean ent_origin_set;
	vec3_t ent_origin;
	vec3_t ent_angles;

	// zyk: used by Entity System to save and load spawnstring of entities
	// GalaxyRP fix: [Entity System] the row length was the bare literal 128 here and was never
	// checked anywhere that writes to it. Named so the bound can be asserted at every write, and
	// defined as MAX_SPAWN_VARS pairs because that is what it has to be: zyk_main_spawn_entity()
	// copies one pair per slot into level.spawnVars[MAX_SPAWN_VARS], so the two limits are the same
	// limit and must move together.
#define ZYK_MAX_SPAWN_STRING_SLOTS (MAX_SPAWN_VARS * 2)
	// GalaxyRP: [Logical Entities] both tables are indexed by ent->s.number for every entity the
	// map loader or the Entity System spawns, and a logical entity's number is >= MAX_GENTITIES,
	// so they cover both regions.
	char *zyk_spawn_strings[MAX_ENTITIESTOTAL][ZYK_MAX_SPAWN_STRING_SLOTS];

	// zyk: amount of keys and values stored in this entity
	int zyk_spawn_strings_values_count[MAX_ENTITIESTOTAL];

	// GalaxyRP fix: [Entity System] the last id handed out as gentity_t::zyk_spawner_id. Starts at
	// 0 with the rest of level, so ids are never 0 and never repeat within a map.
	int zyk_next_spawner_id;
	// GalaxyRP: [Entity System] the last stamp handed out as gentity_t::rpMakerStamp (never 0, never repeats)
	int rp_next_maker_stamp;

	// GalaxyRP: [Weather] /admweather state. The block is claimed lazily, on the first use of the
	// command in a map, and that timing is deliberate: claiming it in G_InitGame would put it below
	// the effects the entity preset registers a second into the map, and those would then replay
	// AFTER the teardown for anyone joining later. Claimed on demand, the block is always the
	// highest weather slots in use, so it always has the last word.
	// GalaxyRP fix: [Configstrings] the gamestate's byte total as of the last time it was counted:
	// once at the end of G_InitGame, once more on the first frame that can see the whole gamestate
	// (see zyk_gamestate_baseline_done below), and again inside G_FindConfigstringIndex before every
	// name it registers for the first time. It is a measurement, never an estimate carried forward --
	// bytes land in the gamestate without passing through that function (player userinfo in
	// CS_PLAYERS, serverinfo, systeminfo), so anything carried forward would be wrong in the unsafe
	// direction.
	int zyk_gamestate_bytes;
	qboolean zyk_gamestate_full;					// so the refusal is logged once, not per name
	qboolean zyk_gamestate_baseline_done;			// the map-load count has been taken and reported
	qboolean zyk_configstring_table_full[ZYK_CS_TABLES];	// same, per indexed table
	qboolean zyk_entity_reserve_warned;				// G_Spawn warns once when the reserve is breached
	qboolean zyk_entity_force_reuse_warned;			// ...and once more when it has to recycle a fresh slot
	qboolean zyk_weather_late_effect_warned;		// a weather effect was refused for arriving after the block
	// GalaxyRP: [Slot Reuse] 1 for a model or effect slot whose name only Entity System props have
	// asked for -- the only slots that may be given to a new name once the table or the gamestate is
	// full, and only while nothing points at them; 0 (permanent) is what every slot starts as each map,
	// and what any other request for the name makes it. See RP_SlotReclaim() in g_utils.c.
	byte rp_slot_es_model[MAX_MODELS];
	byte rp_slot_es_fx[MAX_FX];
	byte rp_slot_ps_sound[MAX_SOUNDS];				// the same for sound slots only /playsound asked for
	int rp_slot_sound_played[MAX_SOUNDS];			// level.time /playsound last played each of those
	int rp_slot_reuses_model;						// names put into a reused slot this map
	int rp_slot_reuses_fx;
	int rp_slot_reuses_sound;
	qboolean rp_slot_reuse_spent_warned[3];			// the per-map budget ran out: logged once each
	qboolean rp_slot_es_context;					// the name being registered is an Entity System prop's own
													// (or, for sounds, /playsound's): a slot it makes is reusable
	qboolean rp_shipboundary_logical_warned;	// shipboundary_touch reports a logical target once per map
	qboolean rp_shipboundary_target_warned;		// ...and a missing one, likewise once per map
	// DAJ_RP: [NPC Debug] the AI debug lines (RP_DebugLines, ai_wpnav.c): the current window and its count,
	// this frame's check of free entity slots, and the refusals logged once per map
	int rp_debugline_window;
	int rp_debugline_count;
	int rp_debugline_slot_time;
	qboolean rp_debugline_slot_checked;
	qboolean rp_debugline_slots_ok;
	qboolean rp_debugline_slots_warned;
	qboolean rp_debugline_capped_warned;
	qboolean rp_hyperspace_target_warned;		// hyperspace_touch, same idea for its two targets

	int zyk_weather_slot;			// first CS_EFFECTS index of the block, 0 while unclaimed
	int zyk_weather_counter;		// appended to every string so a rewrite always re-broadcasts
	int zyk_weather_debounce_time;
	qboolean zyk_weather_use_base;	// whether the map's own weather is part of the current recipe
	int zyk_weather_base_count;
	qboolean zyk_weather_base_truncated;	// the map had more weather than ZYK_WEATHER_MAX_BASE
	char zyk_weather_base[ZYK_WEATHER_MAX_BASE][ZYK_WEATHER_CMD_LENGTH];
	int zyk_weather_layer_count;
	char zyk_weather_layers[ZYK_WEATHER_MAX_LAYERS][ZYK_WEATHER_CMD_LENGTH];

	// GalaxyRP fix: [Quests] the custom-quest level state used to be here: the
	// zyk_custom_quest_missions / _main_fields tables, their counts, the mission origin, radius and
	// hold flags, the npc / ally / item counters, zyk_custom_quest_effect_id and custom_quest_map,
	// plus quest_map (which map's quest this is) and chaos_portal_id above. G_InitGame filled the
	// tables from the GalaxyRP/customquests/ files and nothing ever read them; the quest engine that did
	// is long gone. The loader went with them.

	// zyk: current map name without the path from maps folder
	char zykmapname[128];

	char		mapname[MAX_QPATH];
	char		rawmapname[MAX_QPATH];
} level_locals_t;

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
qboolean	G_StartWorldSpawnScript( qboolean checkSlots );
char *G_NewString( const char *string );

// GalaxyRP fix: [Entity System] the entity-file loader reads one entity per line into a buffer of
// this size, so it is also the longest line /entsave can write and expect to load back again.

#define ZYK_ENTITY_FILE_LINE_LENGTH 2048

// GalaxyRP fix: [security] the longest preset name the six file commands -- /entsave, /entload,
// /entdeletefile, /remapsave, /remapload, /remapdeletefile -- will accept.
//
// They all splice the name into "GalaxyRP/<kind>/<map>/<name>.txt", and /entload then copied that
// path into level.load_entities_file with a plain strcpy(). The name reaches them through
// zyk_check_user_input(), which rejects anything but letters and digits but bounds the LENGTH only
// by MAX_STRING_CHARS -- so a 1023-character name of perfectly legal characters built a 1054-byte
// path and wrote it into a char[512], straight over ignored_players[] and the last_spawned_entity
// pointer that /entundo then hands to G_FreeEntity().
//
// The copy is bounded now as well; this is the limit that stops the path being built over-long in
// the first place, and it cannot live inside zyk_check_user_input() because character names and
// account usernames share that helper and have their own lengths.
#define ZYK_PRESET_NAME_MAX 64

// GalaxyRP fix: [Account] the one place an account password's length limit is stated. /new and
// /changepassword used to carry the literal 30 each, in their own "> 30" tests, with a comment on one
// promising it matched the other. RP_PasswordIsValid() (g_cmds.c) is the only reader now, and both
// commands go through it. Must stay below the size of clientPersistant_t's password[] -- 32 --
// because the commands copy the accepted value straight into that buffer.
#define RP_PASSWORD_MAX 30

// GalaxyRP fix: [Account] see the definition in g_cmds.c. Returns qtrue when the password may be
// stored; otherwise qfalse with *reason pointing at the message to print to the player.
qboolean RP_PasswordIsValid( const char *password, const char **reason );

// GalaxyRP fix: [Entity System] the buffer one encoded token needs, in one place. Every character
// of a token can escape to two, plus the terminator, so a token as long as a whole line needs
// 2 * (ZYK_ENTITY_FILE_LINE_LENGTH - 1) + 1 bytes -- which this covers with one byte spare.
//
// It exists as a name because the coupling is the whole safety argument and it used to be spelled
// out separately at each of the three buffers. zyk_entity_file_encode() cannot write more than it
// is given, so a buffer sized on its own would not overflow -- it would TRUNCATE, and a truncated
// key is not a malformed record, it is a different record: "targetname" cut to "targetnam" loads
// as an entity that quietly lost its targetname and gained a junk key. One name means the 2x
// cannot drift away from the line length it is derived from, and test_round57.py pins every
// declaration to it.
#define ZYK_ENTITY_FILE_ENCODED_LENGTH (ZYK_ENTITY_FILE_LINE_LENGTH * 2)

char *G_NewStringRaw( const char *string );
// GalaxyRP fix: [Entity System] returns whether the WHOLE input fitted. It used to return void,
// so a caller whose buffer was too small got a silently shortened token and no way to know.
qboolean zyk_entity_file_encode( const char *in, char *out, int out_size );
int zyk_entity_file_decode( const char *content, int content_len, int k, char *out, int out_size );

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
void Cmd_EngageDuel_f(gentity_t *ent, int duel_type);
// GalaxyRP: [Saber RGB] republish a player's custom blade colours (configstring + database save)
// and push the authoritative values back down to their own client cvars.
void update_saber_colors(gentity_t *ent);
// GalaxyRP fix: [macOS/Clang build failure] used from g_combat.c with no visible declaration
// there (g_svcmds.c already carries its own local "extern int ClientNumberFromString(...)" for
// the same reason). On Linux/GCC this was only a warning; Clang rejects it as a hard error on
// macOS (-Wimplicit-function-declaration).
int ClientNumberFromString( gentity_t *to, const char *s, qboolean allowconnecting );

//
// g_items.c
//
void ItemUse_Binoculars(gentity_t *ent);
void ItemUse_Shield(gentity_t *ent);
void ItemUse_Sentry(gentity_t *ent);

void zyk_training_pole_damage(gentity_t *ent);
void Cmd_AdmWeather_f( gentity_t *ent );
void		zyk_learn_inline_model( const char *name );
void		zyk_learn_subbsp_name( const char *name );
void		RP_RecordFallbackSpawnPoint( void );
qboolean	zyk_subbsp_name_known( const char *name );
int			zyk_subbsp_name_slot( const char *name );
int			RP_FreeSubBSPEntities( int instance );
int			RP_FreeEntityTriggers( gentity_t *target );
// GalaxyRP: [Entity System] the child link (gentity_t::rpMadeBy) and the one call that frees everything
// an entity made for itself -- its triggers and its stamped children. g_spawn.c.
void		RP_MarkChild( gentity_t *maker, gentity_t *child );
// DAJ_RP: [Dispensers] the model each dispenser and cargo crate shows when its "model" key does not say, and
// the one each rack always shows (a rack's key is not used, as in single player) -- the spawn functions
// (g_misc.c) and the Entity System's model check (RP_EntitySystemSpawnRefused, g_spawn.c) use the same names
#define RP_MODEL_AMMO_FLOOR_UNIT	"/models/items/a_pwr_converter.md3"
#define RP_MODEL_SHIELD_FLOOR_UNIT	"/models/items/a_shield_converter.md3"
#define RP_MODEL_POWER_CONVERTER	"models/items/power_converter.md3"
#define RP_MODEL_GUN_RACK			"models/map_objects/kejim/weaponsrack.md3"
#define RP_MODEL_AMMO_RACK			"models/map_objects/kejim/weaponsrung.md3"
#define RP_MODEL_CARGO_SMALL		"models/map_objects/kejim/cargo_small.md3"
// GalaxyRP: [Entity System] and the saber training dummy's (zyk_training_pole), the same way
#define RP_MODEL_TRAINING_POLE		"models/map_objects/rift/statue.md3"
// DAJ_RP: [Dispensers] the model one of those shows: its "model" key's .md3 if the server has it, else the
// default (logged for a key that is not there); sets ent->model. g_spawn.c.
const char	*RP_DispenserModel( gentity_t *ent, const char *defaultModel );
// DAJ_RP: [SP Maps] where a model key's file is (1 as typed, 2 the path in candidate, 0 not on the server),
// with ext added to a name without one; g_spawn.c. And misc_model_ghoul's checks for one the Entity
// System makes -- what is wrong with its model, frames or radius, or NULL with the model's path and the
// skin entry it registers ("" for none) filled in; g_misc.c.
int			RP_FindModelFileExt( const char *model, const char *ext, char *candidate, int candidateSize );
const char	*RP_GhoulSpawnProblem( gentity_t *ent, char *path, int pathSize, char *skinEntry, int skinSize );
qboolean	RP_IsChildOf( const gentity_t *maker, const gentity_t *child );
int			RP_FreeEntityChildren( gentity_t *maker );
int			G_AllocRemaining( void );
int			G_AllocPoolSize( void );
qboolean	G_AllocRoomFor( int bytes );
// GalaxyRP: [Entity System] what spawning an entity from its record costs the pool, worst case: the
// record's strings twice over (the record and G_ParseField's copies), each rounded as G_Alloc rounds.
int			RP_EntityRecordBytes( const gentity_t *e );
int			RP_PairsBytes( char **pairs, int count );
gentity_t	*RP_MiscBspForInstance( int instance );
qboolean zyk_brush_model_allowed( gentity_t *ent, const char *name );
// DAJ_RP: [Entity System] the reason a brush model ("*N", "#name") is not one of this map's, or NULL -- g_spawn.c
const char *RP_BrushModelProblem( const char *name );
void zyk_set_brush_model( gentity_t *ent );
void Jetpack_Off(gentity_t *ent);
void Jetpack_On(gentity_t *ent);
void ItemUse_Jetpack(gentity_t *ent);
void ItemUse_UseCloak( gentity_t *ent );
void ItemUse_UseDisp(gentity_t *ent, int type);
void ItemUse_UseEWeb(gentity_t *ent);
// GalaxyRP fix: [Minigames] also called by zyk_release_mounts_for_minigame() in g_cmds.c, which
// puts a player's e-web away before a mini-game snapshots their weapons.
void EWebDisattach(gentity_t *owner, gentity_t *eweb);
void G_PrecacheDispensers(void);

void ItemUse_Seeker(gentity_t *ent);
void ItemUse_MedPack(gentity_t *ent);
void ItemUse_MedPack_Big(gentity_t *ent);

void G_CheckTeamItems( void );
void G_RunItem( gentity_t *ent );
void RespawnItem( gentity_t *ent );
int G_ItemRespawnTime( const gentity_t *ent );
int G_ItemPushReturnTime( const gentity_t *ent );
void G_ReturnPushedItem( gentity_t *ent );
void G_PublishMaxArmor( gentity_t *ent ); // g_active.c

gentity_t *Drop_Item( gentity_t *ent, gitem_t *item, float angle );
gentity_t *LaunchItem( gitem_t *item, vec3_t origin, vec3_t velocity );
void G_SpawnItem (gentity_t *ent, gitem_t *item);
void FinishSpawningItem( gentity_t *ent );
void RP_ItemSetStill( gentity_t *ent );	// DAJ_RP: [Still Weapons] g_items.c
void	Add_Ammo (gentity_t *ent, int weapon, int count);
int		RP_MaxAmmo( int ammoType );
void	RP_ClampAmmoToCaps( gentity_t *ent );
// DAJ_RP: [Ammo] rp_add_ammo_scale, and the one rule every dropped weapon's ammo follows (g_items.c)
#define RP_AMMO_SCALE_MIN	0.01f
#define RP_AMMO_SCALE_MAX	1.0f
float	RP_AmmoScale( void );
int		RP_ScaledAmmo( int quantity );
int		RP_DropAmmoCount( int give );
void Touch_Item (gentity_t *ent, gentity_t *other, trace_t *trace);

void ClearRegisteredItems( void );
void RegisterItem( gitem_t *item );
void SaveRegisteredItems( void );

//
// g_utils.c
//
// GalaxyRP fix: [Death System] qtrue while the player is downed (the PLAYER_STATUS_DOWNED bit).
// See the long comment on the definition in g_utils.c for why the state blocks nothing by itself.
// GalaxyRP fix: [Death System] rp_downed_timer 0 switches the downed system off entirely -- see
// RP_DownedSystemEnabled() in g_utils.c. The cvar is CVAR_LATCH, so this cannot change mid-map.
qboolean RP_DownedSystemEnabled( void );
qboolean RP_MinigameFighter( gentity_t *ent );	// DAJ_RP: [Minigames] g_main.c
// GalaxyRP fix: [Death System] set around a player_die() call that is bookkeeping rather than a
// death -- a team or class change, a forced move to Spectator, the respawn an account command
// charges, or a mini-game tearing down a private duel it needs out of the way. player_die() is the
// one reader: it skips the PERS_KILLED increment while this is set and does everything else as
// normal. Same shape as g_noPDuelCheck and g_dontPenalizeTeam, which already bracket several of
// these very calls. Always cleared on the line after the call it wraps, never left set.
extern qboolean g_bookkeepingDeath;
// GalaxyRP fix: [security] shader-name validation and the one remap-preset reader -- see their
// definitions in g_utils.c for what a name is allowed to contain and why the two reader copies
// became one.
qboolean zyk_valid_shader_name( const char *name );
qboolean zyk_load_remap_file( const char *file_path );
// GalaxyRP fix: [security] player-written text never reaches a client as a StringEd reference
// ("@@@KEY") -- see RP_BreakStringEdRefs() in g_utils.c for why.
int			RP_BreakStringEdRefs( char *text );
qboolean	RP_HasStringEdRef( const char *text );
const char	*RP_ShownText( const char *text );
void		RP_PayNpcKillReward( gentity_t *npc, gentity_t *killer );
qboolean	RP_SoundSetKnown( const char *name );
// GalaxyRP fix: [Shader Remap] /remapreset's worker -- returns how many remaps it cleared. See the
// definition in g_utils.c for why it sends the configstring twice and in that order.
int zyk_clear_all_remaps( void );
qboolean G_PlayerIsDowned( gentity_t *ent );
// GalaxyRP fix: [Death System] qtrue only for an ADMIN paralysis (the PLAYER_STATUS_ADMIN_PARALYSIS bit, always
// accompanied by bit 6). G_PlayerIsDowned() stays true for both states -- everything that merely
// asks "can this player act?" wants that one; only the revive paths care which it is.
qboolean G_PlayerIsAdminParalyzed( gentity_t *ent );
// GalaxyRP fix: [Death System] clears the downed state's four fields together -- player_statuses
// the DOWNED and ADMIN_PARALYSIS bits, pers.downedTime and FL_NOTARGET -- and nothing else: no animation, no messages, no
// grace period. Every exit from the state goes through it (RP_ReleaseFromDownedState() and help_up()
// in g_cmds.c, player_die() and G_Damage()'s finish-off branch in g_combat.c) so none of them can
// clear one field and forget another, which is how the ADMIN_PARALYSIS bit used to be left behind. FL_NOTARGET is
// only touched when bit 6 was actually set, so a /notarget cheat on an undowned player survives.
void RP_ClearDownedState( gentity_t *ent );
// GalaxyRP fix: [Death System] ends a downed state: clears both status bits and the countdown,
// releases FL_NOTARGET and plays the get-up animation. Defined in g_cmds.c beside its counterpart
// RP_EnterDownedState(). Two callers: RP_DownedTimerTick() (g_active.c) when an admin paralysis
// serves out its countdown, and Cmd_Unparalyze_f() when an admin ends one early. help_up() does the
// same job for a combat knockdown with its own copy, because it additionally handles the two-player
// case and the post-revive grace period.
void RP_ReleaseFromDownedState( gentity_t *ent );
//
// GalaxyRP: [Saber RGB] the Characters.saberOneColor/saberTwoColor database columns predate this
// feature: a previous author added them (INTEGER DEFAULT 1) and a read path, but never a write
// path, so every existing row still holds the untouched schema default. We reuse those columns
// rather than adding new ones, because both character-load queries are "SELECT *" over a JOIN and
// read their results by hard-coded positional index -- appending a column to Characters would
// silently shift every Skills/Weapons index after it. To keep a real colour from ever being
// confused with the legacy default, a stored value is tagged with this bit; anything without it
// (0, or the old default of 1) means "no custom colour set".
#define SABERRGB_SET	( 1 << 24 )

// GalaxyRP: [Saber RGB] the saberColorMode[] value (0-11, see saber_colors_t) is packed into the
// same stored int, in the bits above SABERRGB_SET's bit 24 -- bits 25-28, 4 bits being enough to
// hold NUM_SABER_COLORS-1 (11). Max possible packed value (mode=11, SABERRGB_SET set, full 24-bit
// RGB payload) is 0x16FFFFFF, well inside a signed 32-bit int. A pre-existing row (schema default
// 1, predates this field entirely) decodes via SABER_STORED_MODE to 0 (SABER_RED), consistent with
// how this column already treats its own legacy default.
#define SABERCOLORMODE_SHIFT	25
#define SABERCOLORMODE_MASK	0xF
#define SABER_STORED_MODE(stored)			( ((stored) >> SABERCOLORMODE_SHIFT) & SABERCOLORMODE_MASK )
#define SABER_STORED_PACK(mode, packedRGB)	\
	( (((mode) & SABERCOLORMODE_MASK) << SABERCOLORMODE_SHIFT) | ((packedRGB) ? (SABERRGB_SET | ((packedRGB) & SABERRGB_MASK)) : 0) )

// Parses a packed saber colour as sent by a client (cp_sbRGB1/cp_sbRGB2) or read from the
// database, and clamps it to something safe to hand on to the renderer. Returns 0 for "unset".
int		G_ParseSaberRGB( const char *str );
// GalaxyRP fix: [Saber RGB] SABERCOLORMODE_MASK is 4 bits (0-15), but only 0..NUM_SABER_COLORS-1
// (0-11) is a valid saber_colors_t -- SABER_STORED_MODE() only masks the raw bits, it doesn't know
// about that upper bound. No current write path can store 12-15 in those bits (every route that
// sets pers.saberColorMode[] before it's persisted already validates its input -- see
// ClientUserinfoChanged()'s own "fromClient >= 0 && fromClient < NUM_SABER_COLORS" check on the
// live-client path), but a hand-edited or corrupted database row still could, and the two DB-restore
// call sites (select_player_character(), select_account_and_default_character_data()) applied
// SABER_STORED_MODE()'s result directly with no such check, unlike that live-client path. Wrap
// SABER_STORED_MODE()'s result in this before assigning it to pers.saberColorMode[] anywhere it
// comes from a stored value; falls back to SABER_RED, matching how this column's pre-RGB legacy
// default (schema default of 1) already decodes.
int		G_ValidateSaberColorMode( int decodedMode );

// GLX_U: [Cosmetics] a hat or cape is saved in the same saberOneColor/saberTwoColor column as its
// saber's colour (the hat with the first saber, the cape with the second), as an id in bits 32-39.
// SQLite keeps every INTEGER as 64 bits, so this needs no schema change, and a build that reads the
// columns with sqlite3_column_int() still gets the colour, from the low 32 bits. Only the items in
// the fixed list in g_utils.c have an id; any other name is worn but saved as 0, "none".
#define RP_STORED_COSMETIC_SHIFT	32
#define RP_STORED_COSMETIC_MASK		0xFF
#define RP_STORED_COLOR(stored)		( (int)( (stored) & 0x7FFFFFFF ) )
#define RP_STORED_COSMETIC(stored)	( (int)( ( (stored) >> RP_STORED_COSMETIC_SHIFT ) & RP_STORED_COSMETIC_MASK ) )
#define RP_STORED_PACK(color, cosmeticId)	\
	( ( (long long)( (cosmeticId) & RP_STORED_COSMETIC_MASK ) << RP_STORED_COSMETIC_SHIFT ) | (long long)( (color) & 0x7FFFFFFF ) )
// the hat or cape name carried after the saber number in a color1/color2 value, checked: letters,
// digits, '_' and '-', 1-13 of them, starting with a letter or '_'. "none", an empty, overlong or
// otherwise bad name give "".
void		G_RpCosmeticFromColor( const char *colorValue, char *out, int outSize );
int			G_RpCosmeticId( int slot, const char *name );		// 0 for none or not on the list
const char	*G_RpCosmeticName( int slot, int id );				// "" for 0 or an id not on the list
void	RPMod_StringEscape(char *in, char *out, int outSize);
int		G_ModelIndex( const char *name );
int		G_SoundIndex( const char *name );
// GalaxyRP fix: [security] see the matching comment on the definition in g_utils.c -- a player-facing
// caller (like /playsound) should use this instead of G_SoundIndex() so a full sound table degrades to
// a friendly message instead of crashing the server via G_FindConfigstringIndex's ERR_DROP.
int		G_SoundIndexSafe( const char *name );
// GalaxyRP fix: [Configstrings] recount the gamestate byte total from scratch. G_InitGame calls this
// once the map's own content is registered, so the running estimate starts from the truth.
void	G_ResetGamestateEstimate( void );
// GalaxyRP fix: [Entity System] how many entity slots G_Spawn() could still hand out, and whether a
// caller that can be driven by a player or admin should be allowed to take "needed" of them. See
// ZYK_ENTITY_RESERVE above and the comment on the definitions in g_utils.c.
// (named G_FreeEntityCount rather than G_EntitiesFree -- that name is already taken further down
// by an older "is there at least one free slot" predicate, which this does not replace.)
int		G_FreeEntityCount( void );
qboolean G_EntitySlotsAvailable( int needed );
// DAJ_RP: [NPC Debug] the AI debug lines -- d_npcai, d_JediAI, g_saberDebugBox, bot_wp_edit -- each a temporary
// entity sent to every player: at most this many per window, none while fewer than the margin are free above the
// reserve, and a batch drawn whole or not at all (ai_wpnav.c)
#define RP_DEBUGLINE_WINDOW_MSEC	100
#define RP_DEBUGLINE_MAX_PER_WINDOW	24
#define RP_DEBUGLINE_SLOT_MARGIN	128
qboolean RP_DebugLines( const vec3_t start, const vec3_t *ends, int count, int color, int time );
void	G_TestLine( vec3_t start, vec3_t end, int color, int time );
void		RP_EntBoundsFrame( gentity_t *ent );
// DAJ_RP: [Entity Bounds] the drawing and aiming helpers, shared with /entcopy and /entcut's preview
void		RP_EntBoundsLine( vec3_t start, vec3_t end, int color, int msec, int clientNum );
void		RP_EntBoundsDrawBoxAt( int clientNum, const vec3_t origin, const vec3_t mins, const vec3_t maxs, const vec3_t angles, int color, int msec );
gentity_t	*RP_EntBoundsAim( const gentity_t *viewer );
gentity_t	*RP_EntAimTarget( gentity_t *ent );				// g_entgrab.c: what /entcopy, /entcut, /entedit, /entremove, /entlockset and /enttrigger aim at
qboolean	RP_EntAimFollowing( const gentity_t *ent );
// GalaxyRP: [Entity System] /entcopy, /entcut, /entrotate, /entcancel and /entaddaim -- g_entgrab.c
#define RP_HOLD_COPY	1
#define RP_HOLD_CUT		2
#define RP_HOLD_STATIC	3	// DAJ_RP: [Static Models] /entcopystatic's copy of a static model
#define RP_HOLD_ADD		4	// DAJ_RP: [Entity System] /entaddgrab's new entity: its record, spawned when it is placed
// DAJ_RP: [Static Models] the EV_TESTLINE colour that is drawn red. CG_TestLine() (cg_effects.c, the stock
// client's too) draws 0 white, and SABER_RED is 0; a value past the saber colours is taken as an RGB of its
// own (CGDEBUG_SaberColor), red in the low byte. The line's colour travels in entityState weapon, 8 bits.
#define RP_LINE_RED		0xff
void		RP_MarkMapEntities( void );
qboolean	RP_RecordIsMapEntity( int num );
// GalaxyRP: [Entity System] the map's pickups, dispensers and decor nothing in the map links to (tagged E):
// changed like any other entity; every other map entity (M) is protected -- see g_entgrab.c
qboolean	RP_MapEntityExempt( const gentity_t *ent );
qboolean	RP_MapEntityProtected( const gentity_t *ent );
qboolean	RP_MapExemptClass( const char *classname );	// DAJ_RP: [Entity Help] the class alone, as /enthelp tells it
// DAJ_RP: [Entity Help] /enthelp (g_enthelp.c), the spawn table it lists (g_spawn.c), and its own table, read-only, for the tests
void		Cmd_EntHelp_f( gentity_t *ent );
int			RP_SpawnClassCount( void );
const char	*RP_SpawnClassName( int i, qboolean *logical );
int			RP_EntHelpCount( void );
const char	*RP_EntHelpField( int i, int field, int *flags );
const char	*RP_MapEntityLinkKey( const gentity_t *ent );
const char	*RP_MapEntityRefusalNote( const gentity_t *ent );
qboolean	RP_EntitySystemMade( const gentity_t *ent );
qboolean	RP_EntitySystemSpawnRefused( gentity_t *ent );
// GalaxyRP: [Entity System] for a spawn function: the entity, when the Entity System made it (/entadd,
// /entedit, a copy, a preset line that is not one of the map's own), is refused with this reason -- the
// reason reaches the admin through level.rp_spawn_refusal -- and freed; returns qtrue then, and the
// spawn function must return. For the map's own entities it does nothing and returns qfalse.
// RP_SpawnSaysWhy(): the reason alone, for a spawn function that frees the entity itself anyway (a
// class with nothing to do in multiplayer, a key it cannot do without), so the admin reads why instead
// of "did not survive being spawned". g_spawn.c.
qboolean	RP_RefuseAtRuntime( gentity_t *ent, const char *reason );
void		RP_SpawnSaysWhy( gentity_t *ent, const char *reason );
char		*RP_EffectKeyValue( gentity_t *ent, const char *key, char *value );	// DAJ_RP: [Weather] a * effect key: "" for a map's own
qboolean	RP_FileExists( const char *path );
qboolean	RP_ModelInfo( int modelIndex, const char *path, int *frames, vec3_t mins, vec3_t maxs );
qboolean	RP_ReadModelBounds( const char *path, vec3_t mins, vec3_t maxs );	// DAJ_RP: [Static Models] by path, not kept
// DAJ_RP: [Static Models] the map's misc_model_static, as the server knows them -- g_statics.c
#define RP_MAX_STATICS			4000	// the client's own limit (MAX_STATIC_MODELS, cg_local.h)
#define RP_MAX_STATIC_MODELS	1024
void		RP_StaticsRecord( void );
void		RP_StaticsReport( void );
int			RP_StaticsCount( void );
qboolean	RP_StaticGet( int n, const char **model, vec3_t origin, vec3_t angles, vec3_t scale );
qboolean	RP_StaticBox( int n, vec3_t mins, vec3_t maxs );
int			RP_StaticAim( const vec3_t eye, const vec3_t dir, float maxDist, float *dist );
const char	*RP_StaticLabel( int n );
int			RP_EntBoundsAimStatic( const gentity_t *viewer );	// g_entbounds.c: the static model the display shows
qboolean	RP_EntHeldAsStaticCopy( const gentity_t *e );	// g_entgrab.c: /entcopystatic's copy, not dropped yet
qboolean	RP_EntHeldUnplaced( const gentity_t *e );		// DAJ_RP: that, or /entaddgrab's new entity: /entsave leaves both out
const char	*RP_EntHeldByText( const gentity_t *e );		// DAJ_RP: "held by <name> with /entcut", for the commands that refuse it
void		RP_EntAddGrabStart( gentity_t *ent, gentity_t *pending, float offset, qboolean originIgnored );	// DAJ_RP: /entaddgrab, g_entgrab.c
void		RP_EntAddGrabHeld( gentity_t *ent );
int			RP_PackConstantLight( float light, const vec3_t color );
gentity_t	*RP_EntGrabSettle( gentity_t *e, const vec3_t point, const vec3_t normal, int freeBefore );
void		RP_EntGrabFrame( gentity_t *ent );
void		RP_EntGrabCancel( gentity_t *ent, qboolean tell );
qboolean	RP_EntRefuseWhileHolding( gentity_t *ent );	// DAJ_RP: the entity commands that wait for a hold to end
qboolean	RP_EntGrabAimPoint( gentity_t *ent, vec3_t point, vec3_t normal );
qboolean	RP_EntAddAimPoint( gentity_t *ent, vec3_t point, vec3_t normal );
qboolean	RP_TeleAimPoint( gentity_t *ent, vec3_t point, vec3_t normal, int *surfaceFlags );	// DAJ_RP: [Admin] /teleportaim
#define RP_AIMOFFSET_MAX	512.0f	// /entaddaim's "aimoffset": the furthest out from the surface aimed at
qboolean	RP_EntitySolidAroundSomeone( const gentity_t *e );
void		RP_EntGrabPlaceBox( const char *classname, const gentity_t *e, vec3_t mins, vec3_t maxs );
void		RP_EntGrabPlace( const vec3_t point, const vec3_t normal, const vec3_t mins, const vec3_t maxs, vec3_t origin );
void		RP_EntGrabRespawnInPlace( gentity_t *e );
// GalaxyRP: [Entity System] everything that must go before a live entity's spawn function runs again
// in its slot (/entedit, /entrotate, a dropped /entcut): its sub-BSP entities, its triggers and
// children, its Ghoul2 model. g_entgrab.c.
void		RP_EntRespawnPrepare( gentity_t *e );
// GalaxyRP fix: [Entity System] the last step of RP_EntRespawnPrepare(): what the record's keys set, and the
// state a fresh entity starts without, back to nothing before the spawn function runs again. g_spawn.c.
void		RP_EntResetForRespawn( gentity_t *e );
// GalaxyRP fix: [Entity System] a team leader spawned again in place takes its followers' name back, as
// G_FindTeams() gives it at map load (zyk_main_spawn_entity). g_spawn.c.
void		RP_TeamLeaderTakeName( gentity_t *e );
// GalaxyRP: [Entity System] an NPC or vehicle spawner counting down to a spawn (NPC_spawn.c)
qboolean	RP_SpawnerCountingDown( const gentity_t *spawner );
// GalaxyRP: [Entity System] room in the pool to spawn this entity again from its record (g_entgrab.c)
qboolean	RP_EntRespawnHasRoom( const gentity_t *e );
// GalaxyRP: [Entity System] team links (teammaster / teamchain, built by G_FindTeams() at map load) kept
// right as entities come and go at runtime: RP_TeamDetach() takes an entity out of its team (G_FreeEntity,
// and before a spawn in place), RP_TeamLinkEntity() puts a newly spawned one into the team its "team"
// key names (zyk_main_spawn_entity). g_utils.c / g_main.c.
void		RP_TeamDetach( gentity_t *ed );
// GalaxyRP: [Music] CS_MUSIC through one door: at most two file names (intro, loop), each short of MAX_QPATH,
// and only while the gamestate has room for the change -- g_utils.c. G_MusicStringValid() is the check
// alone, for a spawn function; G_SetMusic() checks and sets. A refusal gives its reason.
qboolean	G_MusicStringValid( const char *music, char *normalized, int size, const char **reason );
qboolean	G_SetMusic( const char *music, const char **reason );
void		RP_TeamLinkEntity( gentity_t *e );
void		Cmd_EntCopy_f( gentity_t *ent );
void		Cmd_EntCut_f( gentity_t *ent );
void		Cmd_EntCopyStatic_f( gentity_t *ent );	// DAJ_RP: [Static Models] g_entgrab.c
void		Cmd_EntRotate_f( gentity_t *ent );
void		Cmd_EntCancel_f( gentity_t *ent );
// GalaxyRP: [Entity System] NPC and vehicle spawners: spawnnow, respawn, and the NPCs a spawner made --
// NPC_spawn.c
qboolean	RP_IsNpcSpawnerClass( const char *classname );
qboolean	RP_SpawnerMadeIt( const gentity_t *spawner, const gentity_t *npc );
gentity_t	*RP_SpawnerLiveChild( const gentity_t *spawner );
void		RP_SpawnerRespawn( gentity_t *npc );
int			RP_SpawnerRemoveChildren( gentity_t *spawner, int *ridden );
int			RP_NpcScaleValue( const char *text, qboolean strict );	// DAJ_RP: [NPC Scale] an "npcscale" value
// DAJ_RP: [Training Saber] g_cmds.c: is /training on for this player; turn it off, telling him why
qboolean	RP_TrainingActive( gentity_t *ent );
void		RP_TrainingOff( gentity_t *ent, const char *why );
// GalaxyRP: [Slot Reuse] model and effect slots of removed Entity System props given to new names --
// g_utils.c
int			RP_EntityModelIndex( gentity_t *ent, const char *name );
int			RP_EntityEffectIndex( gentity_t *ent, const char *name );
qboolean	RP_SlotRoomFor( int start, const char **names, int count, int otherBytes, char *reason, int reasonSize );
void		RP_SlotStats( int start, int *used, int *slots, int *esOnly, int *reusableNow, int *reuses, int *reuseLimit );
int			G_GamestateBytesUsed( void );
void		RP_SlotRestore( int restart );
int			RP_PlaySoundIndex( const char *name, char *reason, int reasonSize );
void		Cmd_EntSlots_f( gentity_t *ent );
// GalaxyRP: [Listings] /list models|effects|sounds|music|maps|npcs|vehicles, /maplist and the file
// checks of /playsound and /playmusic -- g_rplist.c
qboolean	RP_ListCommand( gentity_t *ent, const char *what );
void		RP_ListVotableMaps( gentity_t *ent, int page );
void		RP_ListDataCommand( gentity_t *ent );
qboolean	RP_NpcTypeIsVehicle( const char *type );
qboolean	RP_NpcTypeKnown( const char *type );
qboolean	RP_VehicleTypeKnown( const char *type );
int			RP_ListParsePage( const char *s );
qboolean	RP_SoundFileExists( const char *name );
qboolean	RP_MusicFileExists( const char *music );
// GalaxyRP fix: [Entity System] only entities with a spawn-key record may be edited or removed by
// the entity commands -- g_spawn.c
qboolean RP_EntityHasSpawnKeys( const gentity_t *ent );
const char *RP_EntityRefusalReason( const gentity_t *ent );
#define RP_SPEAKER_AUDIBLE_MARGIN	512.0f	// GalaxyRP: [Entity System] see SP_target_speaker()
qboolean RP_SoundSetRadius( const char *name, int *radius );	// GalaxyRP: [Entity System] g_spawn.c
void RP_SpeakerAudienceFrame( void );	// GalaxyRP: [Entity System] g_main.c
void RP_SkyPortalUnflag( void );	// GalaxyRP: [Entity System] g_misc.c
void RP_ToggleMoverTeamMatch( gentity_t *master );	// GalaxyRP: [SP Maps] g_mover.c
void RP_ToggleMoverTeamsMatch( void );
qboolean RP_ClassIsSpawnPoint( const char *classname );	// GalaxyRP: [Entity System] info_player_* and the four CTF spawn classes
qboolean RP_EntityIsSpawnPoint( const gentity_t *ent );
void RP_PushTriggerNetSync( gentity_t *self );	// GalaxyRP: [SP Maps] g_trigger.c
qboolean RP_EntityIsCodeMadeSpawnPoint( const gentity_t *ent );
qboolean RP_SpawnPointProtected( const gentity_t *ent );
void RP_SkyPortalRelease( gentity_t *ent );
// GalaxyRP fix: [Configstrings] whether the gamestate can still take "needed" more bytes of
// configstring, for a caller that is about to claim several at once and wants to find out before
// it has claimed any of them.
qboolean G_ConfigstringBytesAvailable( int needed );
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
// GalaxyRP: [Shop] a level 5 Stun Baton's effect on a mover it hits (once the shop's Stun Baton Upgrade's) -- see g_mover.c.
void	RP_StunBatonUseMover( gentity_t *mover, gentity_t *user );
// DAJ_RP: [Locks] account-locked entities -- see g_locks.c.
void		RP_LocksLoad( void );
void		Touch_Multi( gentity_t *self, gentity_t *other, trace_t *trace );	// g_trigger.c: checks the lock of a use or fire trigger itself
qboolean	RP_LockAllows( gentity_t *ent, gentity_t *other, gentity_t *activator );
void		RP_LockMarkPass( gentity_t *ent, gentity_t *activator );
void		RP_LockSpawnNote( gentity_t *ent );
const char	*RP_LockNameOf( const gentity_t *ent );
void		RP_LockBypass( qboolean on );
void		Cmd_EntLockAdd_f( gentity_t *ent );
void		Cmd_EntLockRemove_f( gentity_t *ent );
void		Cmd_EntLockList_f( gentity_t *ent );
void		Cmd_EntLockSet_f( gentity_t *ent );
void		Cmd_EntTrigger_f( gentity_t *ent );
void	G_UseTargets2( gentity_t *ent, gentity_t *activator, const char *string );
void	G_UseTargets (gentity_t *ent, gentity_t *activator);
void	G_SetMovedir ( vec3_t angles, vec3_t movedir);
void	G_SetAngles( gentity_t *ent, vec3_t angles );

void	G_InitGentity( gentity_t *e );
gentity_t	*G_Spawn (void);
// GalaxyRP: [Logical Entities] a slot in the upper, engine-invisible region. Only for entities
// whose class never links, never networks and is never referenced by number from a networked
// entity -- callers should go through RP_SpawnForClassname() (g_spawn.c), which knows which
// classes those are, rather than call this directly.
gentity_t	*G_SpawnLogical( void );
// GalaxyRP: [Logical Entities] free slots left in the logical region. The player-driven spawn paths
// gate on it through RP_SpawnRouteHasRoom() (ZYK_LOGICAL_ENTITY_RESERVE), because G_SpawnLogical()
// ends the server, as G_Spawn() does, once the region is full.
int		G_FreeLogicalEntityCount( void );

// GalaxyRP: [SP Maps] level.rp_sp_game
#define RP_SP_GAME_NONE		0	// a multiplayer map, or any map not on either list
#define RP_SP_GAME_JA		1	// a Jedi Academy single-player map
#define RP_SP_GAME_JO		2	// a Jedi Outcast single-player map

// GalaxyRP: [Logical Entities] see gentity_t::legacySlot.
void	RP_LegacySlotsBegin( void );
void	RP_LegacySlotAssign( gentity_t *e );
void	RP_LegacySlotRelease( gentity_t *e );
// GalaxyRP: [Logical Entities] the one allocator every key/value spawn path uses -- the map
// loader, /entadd, the entity-file loader and zyk_spawn_entity()'s callers -- so that the same
// classname always lands in the same region. Logical when rp_logical_entities was on at map
// start, the spawn table marks the class logical, the entity does not carry "nological 1" and it
// has no script_targetname (ICARUS needs an engine-side entity); G_Spawn() otherwise.
gentity_t	*RP_SpawnForClassname( const char *classname, qboolean nological, qboolean hasScriptTargetname );

// GalaxyRP fix: [Logical Entities] the trigger_shipboundary target promotion, see g_spawn.c.
// Called at the end of the map spawn pass, and again after an Entity System preset load --
// /entload and the automatic default.txt load both free every entity and respawn from the file,
// which puts the markers straight back in the logical region with nothing to promote them.
void		RP_PromoteShipboundaryTargets( void );
qboolean	G_IsLogicalEntity( const char *classname );
// GalaxyRP: [Logical Entities] the same decision for a caller that holds its key/value pairs as
// strings rather than in level.spawnVars (/entadd, the entity-file loader): note every pair, then
// spawn. Only classname, nological and script_targetname are looked at.
typedef struct rpSpawnRoute_s {
	char		classname[MAX_TOKEN_CHARS];
	qboolean	nological;
	qboolean	hasScriptTargetname;	// script_targetname, or any *script behaviour-set key: ICARUS needs a networked entity
} rpSpawnRoute_t;
void		RP_SpawnRouteInit( rpSpawnRoute_t *route );
void		RP_SpawnRouteNoteKey( rpSpawnRoute_t *route, const char *key, const char *value );
gentity_t	*RP_SpawnForRoute( const rpSpawnRoute_t *route );
// GalaxyRP: [Logical Entities] the decision without the allocation, for /entedit to check that an
// edit does not move an entity across the region boundary (a slot cannot change region in place).
qboolean	RP_ClassnameWantsLogical( const char *classname, qboolean nological, qboolean hasScriptTargetname );
qboolean	RP_SpawnRouteIsLogical( const rpSpawnRoute_t *route );
// GalaxyRP: [Logical Entities] room for an entity on this route, in the region it leads to: /entadd's
// margin for a networked one (G_EntitySlotsAvailable(4)), ZYK_LOGICAL_ENTITY_RESERVE for a logical one.
// Every player-driven caller of RP_SpawnForRoute() asks this first; g_entgrab.c.
qboolean	RP_SpawnRouteHasRoom( const rpSpawnRoute_t *route );
int			RP_NPCsAlive( void );	// GalaxyRP: [Entity System] NPC_spawn.c, for RP_NPC_MAX_LIVE
qboolean	RP_HasUpper( const char *s );
void		RP_SpawnerClampDelay( gentity_t *self, int scale );	// GalaxyRP: [Entity System] NPC_spawn.c
void		RP_SpawnerClampCount( gentity_t *self );
// GalaxyRP: [Entity System] misc_weapon_shooter's client pool (g_misc.c)
qboolean	G_IsShooterClient( const gclient_t *cl );
void		G_FreeClientForShooter( gclient_t *cl );
qboolean	RP_EntityIsCharacter( const gentity_t *e );	// DAJ_RP: [Entity System] a player, NPC or vehicle (g_misc.c)
gentity_t *G_TempEntity( vec3_t origin, int event );
gentity_t	*G_PlayEffect(int fxID, vec3_t org, vec3_t ang);
gentity_t	*G_PlayEffectID(const int fxID, vec3_t org, vec3_t ang);
gentity_t *G_ScreenShake(vec3_t org, gentity_t *target, float intensity, int duration, qboolean global);
void	G_MuteSound( int entnum, int channel );
void	G_Sound( gentity_t *ent, int channel, int soundIndex );
void	G_SoundAtLoc( vec3_t loc, int channel, int soundIndex );
void	G_EntitySound( gentity_t *ent, int channel, int soundIndex );
// GalaxyRP: [Space] an NPC's per-frame trigger_space check -- see g_trigger.c
void	RP_NpcSpaceFrame( gentity_t *ent );
void	TryUse( gentity_t *ent );
// GalaxyRP: [Use hint] read-only companion to TryUse -- see its comment in g_utils.c
qboolean G_CanUseInFrontOf( gentity_t *ent );
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
qboolean RP_SpawnPointInSolid( const vec3_t origin, qboolean playerBox );
qboolean RP_SpawnPointRefusedAtRuntime( gentity_t *ent, qboolean playerBox );

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
// GalaxyRP fix: [Emplaced Gun] shared by the gun's own dismount and by the account commands --
// see zyk_stop_active_holdables() in g_cmds.c.
void zyk_release_from_emplaced_gun( gentity_t *gun, gentity_t *rider );

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
// GalaxyRP: [ICARUS] see gentity_t::IcarusSoundTime.
void RP_IcarusSoundCheck( gentity_t *ent );
const char *RP_ScriptPath( const char *name );
void RP_Animate( gentity_t *ent );
// GalaxyRP: [SP Maps] security / goodie keys (g_misc.c)
qboolean RP_GiveSecurityKey( gentity_t *player, const char *keyname );
qboolean RP_HasSecurityKey( gentity_t *player, const char *keyname );
void RP_TakeSecurityKey( gentity_t *player );
qboolean RP_GiveGoodieKey( gentity_t *player );
qboolean RP_TakeGoodieKey( gentity_t *player );
void RP_ClearKeys( gentity_t *player );
// GalaxyRP: [SP Maps] the "message" of the trigger_location the entity stands in, or NULL (g_trigger.c)
const char *RP_TriggerLocationName( gentity_t *ent );
void RP_IcarusAnimTaskCheck( gentity_t *ent );
int  RP_SoundDuration( const char *soundName );
void RP_IcarusSoundCacheReset( void );
void AddTournamentQueue(gclient_t *client);
// GalaxyRP fix: restores the prototype for G_Printf (defined in g_syscalls.c), dropped from this
// header by upstream OpenJK commit ce073087 ("Stripping the QVM layer for MP", 2013) alongside
// G_Error's -- G_Printf's lone remaining caller (g_misc.c's misc_model_breakable_die) has been
// calling it as an implicitly-declared function ever since, which GCC only warns about but Clang
// (15+) treats as a hard error, breaking the Clang build.
void QDECL G_Printf( const char *msg, ... );
void QDECL G_LogPrintf( const char *fmt, ... );
void QDECL G_SecurityLogPrintf( const char *fmt, ... );
void SendScoreboardMessageToAllClients( void );
const char *G_GetStringEdString(char *refSection, char *refName);
void RP_CVU_pluginRequired(void);

// GalaxyRP fix: [validation] cvar-update callbacks (see g_cvar.c) that clamp these timer/cooldown
// cvars back to 0 the moment they're set to a negative value -- see RP_ClampNonNegativeCvar's
// comment in g_cvar.c for the bug this closes.
void RP_CVU_downedTimer(void);
void RP_CVU_downedInvulnerabilityTimer(void);
void RP_CVU_screenMessageTimer(void);
void RP_CVU_flameThrowerCooldown(void);

// DAJ_RP: [Corpses] rp_npc_corpse_time, rp_limb_lifetime and rp_player_corpse_time, all in seconds and
// all held to 0..RP_CORPSE_TIME_MAX. RP_CorpseSecondsToMs() is how every reader turns one into
// milliseconds, clamping again so a bad value can never reach a timer. See g_xcvar.h and g_cvar.c.
#define RP_CORPSE_TIME_MAX 200
// DAJ_RP: [Corpses] how long after its death an NPC's body may still be kept because a player is near
// it or looking at it -- the longest rp_npc_corpse_time allows, in ms (NPC_RemoveBody, NPC.c)
#define RP_CORPSE_WATCH_LIMIT (RP_CORPSE_TIME_MAX * 1000)
qboolean RP_CorpseKeptByWatchers(gentity_t *self);
void RP_CVU_npcCorpseTime(void);
void RP_CVU_limbLifetime(void);
void RP_CVU_playerCorpseTime(void);
// DAJ_RP: [Items] rp_item_lifetime shares the same 0..RP_CORPSE_TIME_MAX clamp and conversion.
void RP_CVU_itemLifetime(void);
// DAJ_RP: [Ammo] rp_add_ammo_scale is held to RP_AMMO_SCALE_MIN..RP_AMMO_SCALE_MAX.
void RP_CVU_addAmmoScale(void);
int RP_CorpseSecondsToMs(int seconds);

// GalaxyRP fix: [validation] duel and minigame cvars whose feature becomes permanently unusable
// below a certain value rather than merely odd -- each clamps to its own lowest working value
// instead of 0. See the individual comments in g_cvar.c for what breaks and why.
void RP_CVU_duelRadius(void);
void RP_CVU_duelTournamentArenaScale(void);
void RP_CVU_duelTournamentDuelTime(void);
void RP_CVU_duelTournamentTimeToStart(void);
void RP_CVU_diceRollCooldown(void);
void RP_CVU_maxRpgCredits(void);
void RP_CVU_rpgMaxLevel(void);
void RP_CVU_startingShield(void);
void RP_CVU_debugMelee(void);
void RP_CVU_jediVmerc(void);
void RP_CVU_timelimit(void);
void RP_CVU_fraglimit(void);
void RP_CVU_capturelimit(void);
void RP_CVU_duelFraglimit(void);
// GalaxyRP: [Grapple Hook] g_allowGrapple back to 1 or 2, rp_allow_grapple_hook into 0..2.
void RP_CVU_allowGrapple(void);
void RP_CVU_allowGrappleHook(void);

// GalaxyRP fix: [Force] returns the force-power disable mask actually in effect: g_duelForcePowerDisable
// in Duel/Power Duel, g_forcePowerDisable everywhere else. See its definition in g_main.c.
int G_ForcePowerDisableValue(void);

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
// GalaxyRP: [Grapple Hook] -- g_weapon.c, g_missile.c, g_active.c, g_cmds.c
//
// The hook is a server-authoritative missile: Weapon_HookFire() spawns it (fire_grapple), the impact
// branch at the top of G_MissileImpact() parks it and hands its position to the owner's ps.lastHitLoc
// every frame through Weapon_HookThink(), and ClientThink_real() raises PMF_GRAPPLE while it is
// parked and the owner still qualifies. Weapon_HookFree() is the one way out and every state change
// that should end a pull -- death, respawn, disconnect, teleport, logout, going down, a duel or a
// mini-game starting, mounting a vehicle -- calls it. RP_GrappleAllowed() is the permission
// (rp_allow_grapple_hook + the Grapple Hook skill); RP_HookMayStayOut()/RP_CanKeepHook()/
// RP_CanFireHook() are the full release, pause and fire gates around it (g_active.c).
//
void Weapon_HookFire( gentity_t *ent );
void Weapon_HookFree( gentity_t *ent );
void Weapon_HookThink( gentity_t *ent );
gentity_t *fire_grapple( gentity_t *self, vec3_t start, vec3_t dir );
qboolean RP_GrappleAllowed( gentity_t *ent );
qboolean RP_HookMayStayOut( gentity_t *ent );
qboolean RP_CanKeepHook( gentity_t *ent );
qboolean RP_CanFireHook( gentity_t *ent );
#define RP_HOOK_CLASSNAME "rp_hook"

// GalaxyRP: [Phase] /admwalkthrough, /admghost, /admholo, /admhidden and /npc effect -- g_active.c,
// g_cmds.c and NPC_spawn.c
qboolean RP_PhasePassesThrough( const gentity_t *ent );
// DAJ_RP: [Phase] /admhidden -- see RP_HiddenApplyState() in g_active.c
qboolean RP_PlayerIsHidden( const gentity_t *ent );
void RP_HiddenApplyState( const gentity_t *player, entityState_t *s );
void RP_ClearPhaseMode( gentity_t *ent );
const char *RP_PhaseModeName( int mode );
void RP_PhaseTrackNpc( gentity_t *npc );
// GalaxyRP: [Entity System] an npc_spawner's "npceffect" value: RP_PHASE_* for holo/ghost/walkthrough,
// RP_PHASE_NONE for an empty one, -1 for anything else -- see NPC_spawn.c
int RP_NpcEffectFromName( const char *name );
// GalaxyRP: [Entity System] an npc_spawner's or NPC_Vehicle's "npcteam" value, the words /npc team takes:
// the npcteam_t, -1 for an empty one, -2 for anything else -- see NPC_spawn.c
int RP_NpcTeamFromName( const char *name );
// DAJ_RP: [NPC System] /npc freeze and the spawner key "npcfreeze" -- see NPC_spawn.c.
// RP_NpcIsFrozen: ent is an NPC under the freeze. RP_NpcFreeze / RP_NpcUnfreeze: put it on / take it off.
qboolean RP_NpcIsFrozen( const gentity_t *ent );
void RP_NpcFreeze( gentity_t *npc );
void RP_NpcUnfreeze( gentity_t *npc );
// DAJ_RP: [NPC System] an npc_spawner's "npcfreeze" value: 1 or 0, 0 for an empty one, -1 for anything else
int RP_NpcFreezeFromValue( const char *value );
// GalaxyRP: [Entity System] gentity_t::rpBaseTeam's value for a playerTeam and enemyTeam; never 0
#define RP_TEAM_RECORD( playerTeam, enemyTeam ) ( 0x10000 | ( ( (playerTeam) & 0xff ) << 8 ) | ( (enemyTeam) & 0xff ) )
void RP_StartOverlapRelease( gentity_t *ent );
// DAJ_RP: [Vehicles] the vehicle an entity is part of (itself for a vehicle, the one ridden for a rider), 0 for none
// -- g_utils.c; and a teleported vehicle's riders seated with it at once -- g_misc.c
int RP_VehicleOf( const gentity_t *ent );
qboolean RP_IsVehicleRider( const gentity_t *ent );

// DAJ_RP: [Melee Battle] the ring (the catwalk Cmd_MeleeMode_f() spawns at level.melee_mode_origin). Its solid box is
// z 0 to RP_MELEE_RING_TOP over that origin, matching the drawn model: catw2_b.md3 at 400% has its bottom at the origin
// and its floor grating at 5.4 * 4 = 21.6 (the outer rim, at 32, is drawn only). See g_main.c.
#define RP_MELEE_RING_TOP		22
float RP_MeleeRingTop( void );
qboolean RP_MeleeOffRing( gentity_t *ent );
void RP_MeleeLiftOffRing( gentity_t *ring );
void RP_CarryRiders( gentity_t *veh, vec3_t angles );
// DAJ_RP: [Cloak Item] a vehicle's droid unit, while aboard, and its cloak kept with the vehicle's -- NPC_AI_Jedi.c
gentity_t *RP_VehicleDroid( gentity_t *veh );
gentity_t *RP_DroidVehicle( gentity_t *droid );
void RP_VehicleDroidCloakSync( gentity_t *veh );
void RP_PhaseNpcEndFrame( void );

//
// bg_pmove.c (server-only part)
//
// GalaxyRP fix: [Weapons] the alt-fire policy. Server-only because it reads sess.loggedin and
// pers.skill_levels[]; ClientEndFrame() publishes its answer in ps.stats[STAT_ALT_FIRE_OK] and the
// shared PM_Weapon() reads that on both sides. See the stat's comment in bg_public.h.
qboolean canAltFireWeapon( gentity_t *ent );

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
int WP_SaberCanBlock(gentity_t *self, gentity_t *attacker, vec3_t point, int dflags, int mod, qboolean projectile, int attackStr);
void WP_SaberInitBladeData( gentity_t *ent );
void WP_InitForcePowers( gentity_t *ent );
void WP_RegisterForceLoopSounds( void );
// GalaxyRP fix: [Force] stop every force power a player currently has running. The account
// commands need this: nothing on their path reaches any of the four places that normally do it.
void zyk_stop_active_force_powers( gentity_t *ent );
void WP_SpawnInitForcePowers( gentity_t *ent );
void WP_ForcePowersUpdate( gentity_t *self, usercmd_t *ucmd );
int ForcePowerUsableOn(gentity_t *attacker, gentity_t *other, forcePowers_t forcePower);
// DAJ_RP: [Force] Grip may not take hold of this one: a vehicle, Galak's mech, the AT-ST or the Rancor
qboolean RP_GripRefused( const gentity_t *target );
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
// GalaxyRP: [Logical Entities] one array, two regions: [0, MAX_GENTITIES) is the networked table
// the engine knows about, [MAX_GENTITIES, MAX_ENTITIESTOTAL) the logical region it does not.
// g_logicalents aliases the start of the upper region so a loop over it reads as one.
extern	gentity_t		g_entities[MAX_ENTITIESTOTAL];
extern	gentity_t		*g_logicalents;

// GalaxyRP: [Logical Entities] walk every allocated slot in both regions, in use or not, exactly as
// "for (i = 0; i < level.num_entities; i++)" walks the networked one -- the loop body keeps its
// own inuse test. For a loop that looks for map entities by classname/targetname and cannot be
// written on G_Find(); the plain form only sees the networked region and would miss a logical
// spawn point or target. RP_NextEntityInAnyRegion(NULL) is the first slot, NULL is the end.
gentity_t	*RP_NextEntityInAnyRegion( gentity_t *from );
#define RP_FOR_EACH_ENTITY(ent) \
	for ( ent = RP_NextEntityInAnyRegion( NULL ); ent; ent = RP_NextEntityInAnyRegion( ent ) )

#define	FOFS(x) offsetof(gentity_t, x)

// userinfo validation bitflags
// default is all except extended ascii
//
// GalaxyRP: [Userinfo] g_userinfoValidate bit layout (see userinfoFields[] in g_client.c):
//   bits  0-20  the original 21 userinfo field checks
//   bits 21-24  the extra checks below (USERINFO_VALIDATION_LEGACY_FIELDS + USERINFO_VALIDATION_*)
//   bits 25-28  userinfo field checks added later (cp_sbRGB1, cp_sbRGB2, rpmod_client, ja_guid)
// Upstream places the extra checks straight after however many fields there are, so adding a field
// used to shift them: an existing g_userinfoValidate value would silently lose its size/slash/
// control-character checks. The extras are pinned at 21-24 instead and new fields go from bit 25
// up, so every existing value keeps its meaning. Stay below bit 31 (the cvar is a signed int).
typedef enum userinfoValidationBits_e {
	// validation & (1<<(USERINFO_VALIDATION_LEGACY_FIELDS+USERINFO_VALIDATION_BLAH))
	USERINFO_VALIDATION_SIZE=0,
	USERINFO_VALIDATION_SLASH,
	USERINFO_VALIDATION_EXTASCII,
	USERINFO_VALIDATION_CONTROLCHARS,
	USERINFO_VALIDATION_MAX
} userinfoValidationBits_t;

#define USERINFO_VALIDATION_LEGACY_FIELDS	21	// bits 0-20: the original field checks
#define USERINFO_VALIDATION_NEW_FIELDS		(USERINFO_VALIDATION_LEGACY_FIELDS + USERINFO_VALIDATION_MAX)	// 25: first bit of the later field checks
#define USERINFO_VALIDATION_NUM_BITS		29	// bits 0-28 are in use

void Svcmd_ToggleUserinfoValidation_f( void );
void Svcmd_ToggleAllowVote_f( void );

// g_cvar.c
#define XCVAR_PROTO
	#include "g_xcvar.h"
#undef XCVAR_PROTO
void G_RegisterCvars( void );
void G_UpdateCvars( void );

extern gameImport_t *trap;
