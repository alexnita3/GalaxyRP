/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
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

#include "qcommon/q_version.h"

#ifdef XCVAR_PROTO
	#define XCVAR_DEF( name, defVal, update, flags, announce ) extern vmCvar_t name;
#endif

#ifdef XCVAR_DECL
	#define XCVAR_DEF( name, defVal, update, flags, announce ) vmCvar_t name;
#endif

#ifdef XCVAR_LIST
	#define XCVAR_DEF( name, defVal, update, flags, announce ) { & name , #name , defVal , update , flags , announce },
#endif

XCVAR_DEF( bg_fighterAltControl,		"0",			NULL,				CVAR_SYSTEMINFO,								qtrue )
// GalaxyRP fix: [Scoreboard] capturelimit, duel_fraglimit, fraglimit and timelimit are pinned at 0 by
// RP_CVU_timelimit() and friends (g_cvar.c) -- see the comment there.
XCVAR_DEF( capturelimit,				"0",			RP_CVU_capturelimit,	CVAR_SERVERINFO|CVAR_ARCHIVE|CVAR_NORESTART,	qtrue )
XCVAR_DEF( com_optvehtrace,				"0",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( d_altRoutes,					"0",			NULL,				CVAR_CHEAT,										qfalse )
XCVAR_DEF( d_asynchronousGroupAI,		"0",			NULL,				CVAR_CHEAT,										qfalse )
XCVAR_DEF( d_break,						"0",			NULL,				CVAR_CHEAT,										qfalse )
XCVAR_DEF( d_JediAI,					"0",			NULL,				CVAR_CHEAT,										qfalse )
XCVAR_DEF( d_noGroupAI,					"0",			NULL,				CVAR_CHEAT,										qfalse )
XCVAR_DEF( d_noroam,					"0",			NULL,				CVAR_CHEAT,										qfalse )
XCVAR_DEF( d_npcai,						"0",			NULL,				CVAR_CHEAT,										qfalse )
XCVAR_DEF( d_npcaiming,					"0",			NULL,				CVAR_CHEAT,										qfalse )
XCVAR_DEF( d_npcfreeze,					"0",			NULL,				CVAR_CHEAT,										qfalse )
XCVAR_DEF( d_noIntermissionWait,		"0",			NULL,				CVAR_NONE,										qfalse )
XCVAR_DEF( d_patched,					"0",			NULL,				CVAR_CHEAT,										qfalse )
XCVAR_DEF( d_perPlayerGhoul2,			"0",			NULL,				CVAR_CHEAT,										qtrue )
XCVAR_DEF( d_powerDuelPrint,			"0",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( d_projectileGhoul2Collision,	"1",			NULL,				CVAR_CHEAT,										qtrue )
XCVAR_DEF( d_saberAlwaysBoxTrace,		"0",			NULL,				CVAR_CHEAT,										qtrue )
XCVAR_DEF( d_saberBoxTraceSize,			"0",			NULL,				CVAR_CHEAT,										qtrue )
XCVAR_DEF( d_saberCombat,				"0",			NULL,				CVAR_CHEAT,										qfalse )
XCVAR_DEF( d_saberGhoul2Collision,		"1",			NULL,				CVAR_CHEAT,										qtrue )
XCVAR_DEF( d_saberInterpolate,			"0",			NULL,				CVAR_CHEAT,										qtrue )
XCVAR_DEF( d_saberKickTweak,			"1",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( d_saberSPStyleDamage,		"1",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( d_saberStanceDebug,			"0",			NULL,				CVAR_NONE,										qfalse )
XCVAR_DEF( d_siegeSeekerNPC,			"0",			NULL,				CVAR_CHEAT,										qtrue )
XCVAR_DEF( dedicated,					"0",			NULL,				CVAR_NONE,										qfalse )
XCVAR_DEF( developer,					"0",			NULL,				CVAR_NONE,										qfalse )
XCVAR_DEF( dmflags,						"0",			NULL,				CVAR_SERVERINFO|CVAR_ARCHIVE,					qtrue )
XCVAR_DEF( duel_fraglimit,				"0",			RP_CVU_duelFraglimit,	CVAR_SERVERINFO|CVAR_ARCHIVE|CVAR_NORESTART,	qtrue )	// pinned at 0, see capturelimit
XCVAR_DEF( fraglimit,					"0",			RP_CVU_fraglimit,	CVAR_SERVERINFO|CVAR_ARCHIVE|CVAR_NORESTART,	qtrue )	// pinned at 0, see capturelimit
XCVAR_DEF( g_adaptRespawn,				"1",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( g_allowDuelSuicide,			"1",			NULL,				CVAR_ARCHIVE,									qtrue )
// GalaxyRP: [Grapple Hook] the MODE switch, not the permission: 1 is the swing (TaystJK's "Tarzan"),
// 2 the JA+ winch. Who may use the hook at all is rp_allow_grapple_hook below. Anything but 1 or 2 --
// including the 0 that means "off" in TaystJK -- is put back to 2 by RP_CVU_allowGrapple() (g_cvar.c)
// with a console note saying which cvar to use instead. SERVERINFO so our cgame can predict the same
// mode (cgs.grappleMode, cg_servercmds.c); the four g_hook* numbers below that the pull reads are
// published for the same reason. TaystJK reads none of them on a server it does not recognise.
XCVAR_DEF( g_allowGrapple,				"2",			RP_CVU_allowGrapple,	CVAR_ARCHIVE|CVAR_SERVERINFO|CVAR_NORESTART,	qtrue )
XCVAR_DEF( g_allowHighPingDuelist,		"1",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( g_allowNPC,					"1",			NULL,				CVAR_CHEAT,										qtrue )
XCVAR_DEF( g_allowTeamVote,				"1",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_allowVote,					"-1",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_antiFakePlayer,			"1",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_armBreakage,				"0",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( g_austrian,					"0",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_autoMapCycle,				"0",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( g_banIPs,					"",				NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_charRestrictRGB,			"1",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_duelWeaponDisable,			"1",			NULL,				CVAR_SERVERINFO|CVAR_ARCHIVE|CVAR_LATCH,		qtrue )
XCVAR_DEF( g_debugAlloc,				"0",			NULL,				CVAR_NONE,										qfalse )
XCVAR_DEF( g_debugDamage,				"0",			NULL,				CVAR_NONE,										qfalse )
// GalaxyRP fix: [Melee] pinned to 1. Despite the name this was never a debug switch -- it is the
// master gate for the whole hand-to-hand system: kicks and the grapple (bg_pmove.c), infinite
// wall-hold, and the wall-interaction rules in g_combat.c/w_force.c. This mod is designed around
// it being on and the shipped config set it to 1, but the stock default was 0, so any server
// running its own config without that line silently lost melee combat with no error to explain
// it. Default is 1 now and RP_CVU_debugMelee (g_cvar.c) forces it back to 1 on any change.
//
// Deliberately left registered and CVAR_SERVERINFO rather than removed. pm->debugMelee is read by
// SHARED pmove code that both the server and the client run for prediction, and the client learns
// the value from serverinfo (cgs.debugMelee, cg_servercmds.c). Deleting the key would make
// Info_ValueForKey() return "" -> atoi() 0 on any client still running an older cgame, which
// would then predict no kick, no grapple and a normal wall-release while this server performs all
// three. With sv_pure 0 and sv_allowDownload 0 in the shipped config, clients with a stale pk3
// are the expected case, not an edge case -- so the key stays, pinned.
XCVAR_DEF( g_debugMelee,				"1",			RP_CVU_debugMelee,	CVAR_SERVERINFO,								qtrue )
XCVAR_DEF( g_debugMove,					"0",			NULL,				CVAR_NONE,										qfalse )
XCVAR_DEF( g_debugSaberLocks,			"0",			NULL,				CVAR_CHEAT,										qfalse )
XCVAR_DEF( g_debugServerSkel,			"0",			NULL,				CVAR_CHEAT,										qfalse )
#ifdef _DEBUG
XCVAR_DEF( g_disableServerG2,			"0",			NULL,				CVAR_NONE,										qtrue )
#endif
XCVAR_DEF( g_dismember,					"0",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_doWarmup,					"0",			NULL,				CVAR_NONE,										qtrue )
//XCVAR_DEF( g_engineModifications,		"1",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_ff_objectives,				"0",			NULL,				CVAR_CHEAT|CVAR_NORESTART,						qtrue )
XCVAR_DEF( g_filterBan,					"1",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_forceBasedTeams,			"0",			NULL,				CVAR_SERVERINFO|CVAR_ARCHIVE|CVAR_LATCH,		qfalse )
XCVAR_DEF( g_forceClientUpdateRate,		"250",			NULL,				CVAR_NONE,										qfalse )
XCVAR_DEF( g_forceDodge,				"1",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( g_forcePowerDisable,			"0",			NULL,				CVAR_SERVERINFO|CVAR_ARCHIVE|CVAR_LATCH,		qtrue )
XCVAR_DEF( g_forceRegenTime,			"200",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_forceDuelForceRegenTime,	"200",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_saberDuelForceRegenTime,	"200",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_forceRespawn,				"60",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( g_fraglimitVoteCorrection,	"1",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_friendlyFire,				"0",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_friendlySaber,				"0",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_g2TraceLod,				"3",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( g_gametype,					"0",			NULL,				CVAR_SERVERINFO|CVAR_LATCH,						qfalse )
XCVAR_DEF( g_gravity,					"800",			NULL,				CVAR_NONE,										qtrue )
// GalaxyRP: [Grapple Hook] TaystJK's tuning cvars, same names and defaults, so a config written for
// its hook carries over. Speed and inheritance shape the projectile (fire_grapple, g_missile.c);
// strength is the winch's reel speed and the swing's target speed; strength1/2 are the swing's
// acceleration far from / near the anchor (PM_GrappleMoveTarzan, bg_pmove.c); floodProtect is the
// minimum ms between shots (ClientThink_real, g_active.c). The three the pull reads are SERVERINFO
// because the client predicts the pull with them -- see g_allowGrapple above.
XCVAR_DEF( g_hookFloodProtect,			"600",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( g_hookInheritance,			"0.5",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( g_hookSpeed,					"2400",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( g_hookStrength,				"800",			NULL,				CVAR_ARCHIVE|CVAR_SERVERINFO|CVAR_NORESTART,	qtrue )
XCVAR_DEF( g_hookStrength1,				"20",			NULL,				CVAR_ARCHIVE|CVAR_SERVERINFO|CVAR_NORESTART,	qtrue )
XCVAR_DEF( g_hookStrength2,				"40",			NULL,				CVAR_ARCHIVE|CVAR_SERVERINFO|CVAR_NORESTART,	qtrue )
XCVAR_DEF( g_inactivity,				"0",			NULL,				CVAR_NONE,										qtrue )
// GalaxyRP fix: [Jedi vs Merc] g_jediVmerc is pinned OFF. Stock JKA's "Jedi vs Mercenaries" mode
// splits every player at ClientSpawn (g_client.c) by WP_HasForcePowers() -- which reads the force
// levels the CLIENT sent in its own "forcepowers" userinfo string -- and stamps one of two
// playerState flags on them. Note what is NOT in that gate: it is not restricted to team gametypes,
// so it applies in our FFA just as much as in CTF.
//
// The spawn loadout is the visible half. The lasting half is those flags, which are networked one
// bit each (PSF(trueJedi)/PSF(trueNonJedi) in qcommon/msg.cpp) and are read by four pieces of
// always-on code, none of them gametype-gated:
//
//   bg_pmove.c PM_Weapon   -- trueJedi has stats[STAT_WEAPONS] re-assigned to the saber alone, and
//                             ps.weapon forced to WP_SABER, on EVERY frame
//   bg_misc.c  BG_CanUseFPNow      -- trueNonJedi can never use any force power, tested before
//                                    everything else
//   bg_misc.c  BG_CanItemBeGrabbed -- trueJedi may pick up almost nothing; trueNonJedi may not pick
//                                     up force powerups, the seeker or a saber
//   g_combat.c G_Damage            -- trueJedi takes half splash damage, trueNonJedi takes
//                                    multiplied saber damage
//
// Every one of those fights the RPG system. initialize_rpg_skills() runs LATER in ClientSpawn than
// the jediVmerc block, so a "jedi" is granted their skill loadout and then stripped back to a bare
// saber by the next pmove frame; a "merc" keeps the guns but loses every force skill, magic power
// and unique ability that routes through BG_CanUseFPNow. Which side a player lands on is decided by
// their client's force configuration, not by their account. There is no setting of this cvar that
// an RP server wants.
//
// Deliberately left registered, and still CVAR_SERVERINFO, rather than removed -- the same reasoning
// as g_debugMelee above. The key is read by cgs.jediVmerc (cg_servercmds.c), by UI_TrueJediEnabled()
// (ui_main.c, which shows or hides the jedi/non-jedi selector in the ingame player menu) and by the
// server browser's "truejedi" column (sv_main.cpp -> cl_main.cpp). Removing the key would make
// Info_ValueForKey() return "" for all three; publishing an explicit "0" is honest and costs nothing.
//
// CVAR_LATCH is kept as stock. It does not get in the way: trap->Cvar_Set from the game module
// reaches Cvar_Set2() with force = qtrue (Cvar_VM_Set, qcommon/cvar.cpp), which skips the latch
// branch entirely and frees any pending latched string, so RP_CVU_jediVmerc (g_cvar.c) applies
// immediately rather than "upon restarting".
XCVAR_DEF( g_jediVmerc,					"0",			RP_CVU_jediVmerc,	CVAR_SERVERINFO|CVAR_LATCH|CVAR_ARCHIVE,		qtrue )
XCVAR_DEF( g_knockback,					"1000",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( g_locationBasedDamage,		"1",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( g_log,						"games.log",	NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_logClientInfo,				"0",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_logSync,					"0",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_maxConnPerIP,				"3",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_maxForceRank,				"7",			NULL,				CVAR_SERVERINFO|CVAR_ARCHIVE|CVAR_LATCH,		qfalse )
XCVAR_DEF( g_maxGameClients,			"0",			NULL,				CVAR_SERVERINFO|CVAR_LATCH|CVAR_ARCHIVE,		qfalse )
XCVAR_DEF( g_maxHolocronCarry,			"3",			NULL,				CVAR_LATCH,										qfalse )
XCVAR_DEF( g_motd,						"",				NULL,				CVAR_NONE,										qfalse )
XCVAR_DEF( g_needpass,					"0",			NULL,				CVAR_SERVERINFO|CVAR_ROM,						qfalse )
XCVAR_DEF( g_noSpecMove,				"0",			NULL,				CVAR_SERVERINFO,								qtrue )
XCVAR_DEF( g_npcspskill,				"0",			NULL,				CVAR_ARCHIVE|CVAR_INTERNAL,						qfalse )
XCVAR_DEF( g_password,					"",				NULL,				CVAR_NONE,										qfalse )
XCVAR_DEF( g_powerDuelEndHealth,		"90",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_powerDuelStartHealth,		"150",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_privateDuel,				"1",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_randFix,					"1",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_restarted,					"0",			NULL,				CVAR_ROM,										qfalse )
XCVAR_DEF( g_saberBladeFaces,			"1",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( g_saberDamageScale,			"1",			NULL,				CVAR_ARCHIVE,									qtrue )
#ifdef DEBUG_SABER_BOX
XCVAR_DEF( g_saberDebugBox,				"0",			NULL,				CVAR_CHEAT,										qfalse )
#endif
#ifndef FINAL_BUILD
XCVAR_DEF( g_saberDebugPrint,			"0",			NULL,				CVAR_CHEAT,										qfalse )
#endif
XCVAR_DEF( g_saberDmgDelay_Idle,		"350",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_saberDmgDelay_Wound,		"0",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_saberDmgVelocityScale,		"0",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_saberLockFactor,			"2",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_saberLocking,				"1",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_saberLockRandomNess,		"2",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_saberRealisticCombat,		"0",			NULL,				CVAR_CHEAT,										qfalse )
XCVAR_DEF( g_saberRestrictForce,		"0",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_saberTraceSaberFirst,		"0",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_saberWallDamageScale,		"0.4",			NULL,				CVAR_NONE,										qfalse )
XCVAR_DEF( g_securityLog,				"1",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_showDuelHealths,			"0",			NULL,				CVAR_SERVERINFO,								qfalse )
XCVAR_DEF( g_siegeRespawn,				"20",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_siegeTeam1,				"none",			NULL,				CVAR_ARCHIVE|CVAR_SERVERINFO,					qfalse )
XCVAR_DEF( g_siegeTeam2,				"none",			NULL,				CVAR_ARCHIVE|CVAR_SERVERINFO,					qfalse )
XCVAR_DEF( g_siegeTeamSwitch,			"1",			NULL,				CVAR_SERVERINFO|CVAR_ARCHIVE,					qfalse )
XCVAR_DEF( g_slowmoDuelEnd,				"0",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_smoothClients,				"1",			NULL,				CVAR_NONE,										qfalse )
XCVAR_DEF( g_spawnInvulnerability,		"3000",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_speed,						"250",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( g_statLog,					"0",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_statLogFile,				"statlog.log",	NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_stepSlideFix,				"1",			NULL,				CVAR_SERVERINFO,								qtrue )
XCVAR_DEF( g_synchronousClients,		"0",			NULL,				CVAR_SYSTEMINFO,								qfalse )
XCVAR_DEF( g_teamAutoJoin,				"0",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_teamForceBalance,			"0",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_timeouttospec,				"70",			NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_userinfoValidate,			"528482303",		NULL,				CVAR_ARCHIVE,									qfalse )
XCVAR_DEF( g_useWhileThrowing,			"1",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( g_voteDelay,					"3000",			NULL,				CVAR_NONE,										qfalse )
XCVAR_DEF( g_warmup,					"20",			NULL,				CVAR_ARCHIVE,									qtrue )
XCVAR_DEF( g_weaponDisable,				"0",			NULL,				CVAR_SERVERINFO|CVAR_ARCHIVE|CVAR_LATCH,		qtrue )
XCVAR_DEF( g_weaponRespawn,				"5",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( gamedate,					SOURCE_DATE,	NULL,				CVAR_ROM,										qfalse )
XCVAR_DEF( gamename,					GAMEVERSION,	NULL,				CVAR_SERVERINFO|CVAR_ROM,						qfalse )
XCVAR_DEF( pmove_fixed,					"0",			NULL,				CVAR_SYSTEMINFO|CVAR_ARCHIVE,					qtrue )
XCVAR_DEF( pmove_float,					"0",			NULL,				CVAR_SYSTEMINFO|CVAR_ARCHIVE,					qtrue )
XCVAR_DEF( pmove_msec,					"8",			NULL,				CVAR_SYSTEMINFO|CVAR_ARCHIVE,					qtrue )
XCVAR_DEF( RMG,							"0",			NULL,				CVAR_NONE,										qtrue )
XCVAR_DEF( sv_cheats,					"1",			NULL,				CVAR_NONE,										qfalse )
XCVAR_DEF( sv_fps,						"40",			NULL,				CVAR_ARCHIVE|CVAR_SERVERINFO,					qtrue )
XCVAR_DEF( sv_maxclients,				"8",			NULL,				CVAR_SERVERINFO|CVAR_LATCH|CVAR_ARCHIVE,		qfalse )
// GalaxyRP: [Saber RGB] capability advertisement for TaystJK clients, which is what most players
// connect with. TaystJK picks a "server mod" purely from the serverinfo gamename string
// (CG_ParseServerinfo, cg_servercmds.c) and ours matches none of its patterns, so it files us as
// SVMOD_BASEJKA. Its ClampSaberColor() (cg_players.c) then throws away any colour above
// SABER_PURPLE unless the server is JA+/JAPro OR advertises the matching taystJKinfo bit -- and
// because those two tests are ORed with the client's own cg_noRGBSabers, the server half fires on
// its own. Every TaystJK client was therefore seeing "color -= SABER_RGB": our RGB blades came out
// red, the flame/elec blade styles came out orange/yellow/green/blue, and black came out orange.
//
// Nothing else was wrong -- the wire format already matches TaystJK exactly and always did (same
// saber_colors_t ordinals in q_shared.h, same c1/c2 mode and c3/c4 packed-RGB configstring keys in
// ClientUserinfoChanged, same r|g<<8|b<<16 packing). The data was arriving intact and being
// discarded on arrival for want of this one key.
//
// The value is a bitmask; TaystJK's own names for the bits (bg_public.h in its tree) are:
//     1<<0 RGBSABERS   1<<1 BLACKSABERS   1<<2 FLIPKICK   1<<3 GRAPPLE
//     1<<4 FIXROLL_1   1<<5 FIXROLL_2     1<<6 FIXROLL_3
// We advertise 11 -- RGBSABERS|BLACKSABERS|GRAPPLE -- and deliberately stop there. The first two
// are read only in cg_players.c, i.e. they are purely about how a blade is drawn. Every bit from
// 1<<2 up is read in bg_pmove.c instead: they tell the client to PREDICT MOVEMENT under rules the
// server is promising to implement. GRAPPLE is the one of those our pmove now does implement (the
// grapple hook, PMF_GRAPPLE, same pull code as TaystJK's), so the promise holds; FLIPKICK and the
// three FIXROLLs it does not, so setting any of those would desync client prediction from the
// server and produce rubber-banding. Do not widen this value without making our bg_pmove.c actually
// match the behaviour the added bit claims.
//
// [Grapple Hook] Stated honestly, the GRAPPLE bit buys nothing from the TaystJK client as it is
// today: its rope drawing and asset loading are keyed on the server being JA+/JAPro (cg_ents.c,
// cg_main.c), and its pmove dispatch reaches the branch that tests this bit only after a branch
// that a server it files as basejka always takes. A TaystJK client on this server therefore fires
// and is pulled (the server does both), but predicts no pull and draws no rope. The bit is set
// anyway, and our hook wears the JAPro wire signature (WP_BRYAR_PISTOL + saberInFlight), so that a
// TaystJK build which honours the bit needs nothing more from us.
//
// CVAR_ROM for the same reason gamename above is: this states what the mod IS, not a knob, and a
// well-meaning "taystJKinfo 127" in a server config would break movement for every TaystJK player.
// Black is included because our palette really does offer it (/sabercolor black, the UI palette,
// and its own shaders in ui_saber.c/cg_main.c) and TaystJK gates it on the same mechanism.
XCVAR_DEF( taystJKinfo,					"11",			NULL,				CVAR_SERVERINFO|CVAR_ROM,						qfalse )
XCVAR_DEF( timelimit,					"0",			RP_CVU_timelimit,	CVAR_SERVERINFO|CVAR_ARCHIVE|CVAR_NORESTART,	qtrue )	// pinned at 0, see capturelimit
XCVAR_DEF( rp_max_blaster_pack_ammo,	"300",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_max_power_cell_ammo,		"300",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_max_metal_bolt_ammo,		"300",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_max_rocket_ammo,			"25",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_max_thermal_ammo,		"10",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_max_tripmine_ammo,		"10",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_max_detpack_ammo,		"10",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_blaster_pistol_damage,	"10",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_blaster_pistol_velocity,	"1600",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_e11_blaster_rifle_damage,	"20",		NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_e11_blaster_rifle_velocity,	"2300",		NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_disruptor_damage,	"40",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_disruptor_alt_damage,	"125",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_bowcaster_damage,	"50",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_bowcaster_velocity,	"1300",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_repeater_damage,	"14",					NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_repeater_velocity,	"1600",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_repeater_alt_damage,	"60",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_repeater_alt_velocity,	"1100",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_repeater_alt_splash_damage,	"60",		NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_demp2_damage,	"35",					NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_demp2_velocity,	"3500",					NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_demp2_alt_damage,	"12",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_flechette_damage,	"15",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_flechette_velocity,	"3500",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_flechette_alt_damage,	"110",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_flechette_alt_velocity,	"1800",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_flechette_alt_splash_damage,	"110",		NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_rocket_damage,	"100",					NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_rocket_velocity,	"900",					NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_rocket_alt_velocity,	"450",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_rocket_splash_damage,	"100",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_concussion_damage,	"75",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_concussion_velocity,	"3000",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_concussion_splash_damage,	"40",		NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_concussion_alt_damage,	"25",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_stun_baton_damage,	"20",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_melee_left_hand_damage,	"10",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_melee_right_hand_damage,	"12",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_melee_kick_damage,	"10",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP: [Jump Kick] damage of the jump kick (jump, then jump again next to someone) -- see RP_JumpKickLand(), g_active.c
XCVAR_DEF( rp_melee_jump_kick_damage,	"20",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_thermal_damage,	"70",					NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_thermal_splash_damage,	"90",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_thermal_velocity,	"900",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_tripmine_damage,	"100",					NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_tripmine_splash_damage,	"105",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_detpack_damage,	"100",					NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_detpack_splash_damage,	"200",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_saber_throw_damage,	"30",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_ammo_respawn_time,	"40",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_shield_respawn_time,	"20",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_health_respawn_time,	"30",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_holdable_item_respawn_time,	"60",		NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP fix: [Items] Force Enlightenment, Force Boon and Ysalamiri; was a fixed 90 (RESPAWN_POWERUP). Not
// shortened by g_adaptRespawn, as vanilla never did. See G_ItemRespawnTime in g_items.c.
XCVAR_DEF( rp_powerup_respawn_time,	"90",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP fix: [Quests] zyk_allow_guardian_quest and zyk_allow_bounty_quest used to be defined here.
// Their only consumers, Cmd_GuardianQuest_f and Cmd_BountyQuest_f, were deleted as unreachable dead
// code (see the GalaxyRP fix comment in g_cmds.c), leaving these cvars orphaned. Removed outright.
// GalaxyRP: [RPG LMS] zyk_allow_rpg_lms used to be defined here. Its only consumer was the join
// guard in Cmd_RpgLmsMode_f, and the whole RPG LMS feature has been removed; see the note where
// that command used to live in g_cmds.c. Also dropped from assets/server/galaxyrp_server.cfg.
// GalaxyRP: [Race Mode] zyk_allow_race_mode and zyk_start_race_timer used to be defined here and
// below. Their only consumers were Cmd_RaceMode_f's join guard and its start timer, and the whole
// Race Mode feature has been removed; see the note where that command used to live in g_cmds.c.
// GalaxyRP: [Sniper Battle] zyk_allow_sniper_battle and zyk_sniper_battle_time_to_start used to be
// defined here and below, the latter with an RP_CVU_sniperBattleTimeToStart callback. Their only
// consumers were Cmd_SniperMode_f's join guard and its start timer, and the whole Sniper Battle
// feature has been removed; see the note where that command used to live in g_cmds.c. Both modes
// were also dropped from assets/server/galaxyrp_server.cfg.
// GalaxyRP fix: [Minigames] both are CVAR_LATCH, so a change waits for the next map.
//
// Switching either off mid-event does not stop the event: the state machines in G_RunFrame test
// level.duel_tournament_mode / level.melee_mode and never read these cvars, so a running tournament
// or battle carries on to completion regardless. What the cvar guard at the top of Cmd_DuelMode_f
// and Cmd_MeleeMode_f does is refuse the command outright -- and those commands are the only way
// OUT, so switching the mode off during sign-up trapped everyone already in it. Their account
// commands are refused too, by zyk_account_change_blocked(), and no admin command ends a running
// event: duel_tournament_end() and melee_battle_end() are reachable only from those same leave
// branches and from inside G_RunFrame. So the mode became both inescapable and unstoppable until
// the map changed.
//
// Latching costs an admin nothing they actually had. Both modes reset to 0 in G_InitGame, so an
// event cannot survive a map change either way -- the moment a latched value takes effect is the
// same moment the event ends. trackChange is qfalse for the reason it is on rp_downed_timer:
// G_UpdateCvars broadcasts vmCvar->string, still the OLD value for a latched cvar, which would
// contradict the engine's own "will be changed upon restarting" a moment after it printed.
XCVAR_DEF( rp_allow_duel_tournament, "1",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART|CVAR_LATCH,			qfalse )
XCVAR_DEF( rp_allow_melee_battle, "1",					NULL,				CVAR_ARCHIVE|CVAR_NORESTART|CVAR_LATCH,			qfalse )
// GalaxyRP fix: [Magic] the 73 magic-system cvars that used to be defined here and just above were
// removed. They were the per-power MP costs (*_mp_cost), the per-power on/off switches
// (zyk_enable_*), zyk_universe_mp_cost_factor, and the magic-fist trio (zyk_magic_fist_damage /
// _velocity / _mp_cost). Every one of them was read by exactly one thing: the player's magic-power
// dispatch in TryGrapple()'s RPG branch (g_cmds.c), which was itself removed as permanently
// unreachable -- all seven powers it could trigger were gated on pers.defeated_guardians or
// pers.universe_quest_progress/universe_quest_counter, fields nothing anywhere ever writes. With the
// dispatch gone the cvars had no readers left at all, in any module or asset.
//
// This changed nothing in game. The magic effect functions in g_main.c (earthquake(), hurricane(),
// ...) were called only by the quest_mage NPC's random-power chain, with hardcoded arguments and
// never through a cvar, and an zyk_enable_* switch only ever gated a PLAYER's access to a power a
// player can no longer invoke. That chain went with the magic engine, and the effect functions
// themselves -- kept for a while as zero-caller reference code -- have now been deleted too, along
// with zyk_max_special_power_targets, the one cvar they still read (it capped how many entities
// one effect could hit, through zyk_special_power_can_hit_target()).
//
// zyk_magic_fist_velocity and zyk_magic_fist_mp_cost went with magic_fist_velocity() in g_weapon.c,
// their sole reader, which had no callers of its own (see the note at its old location there).
//
// GalaxyRP fix: [Quests] zyk_enable_light_power, zyk_enable_dark_power, zyk_enable_eternity_power,
// and zyk_enable_universe_power used to be defined here. Their only consumers were the /settings 1-4
// (Light/Dark/Eternity/Universe Power) special-casing in Cmd_Settings_f, which was removed since those
// quest-completion-granted powers can no longer be earned (see the GalaxyRP fix comment in g_cmds.c).
// Removed outright.
// GalaxyRP fix: [cleanup] renamed from zyk_screen_message/zyk_screen_message_timer to
// rp_screen_message/rp_screen_message_timer -- matches this codebase's rp_ naming convention for
// GalaxyRP-authored cvars (see rp_allow_playsound_command/rp_allow_emotes above).
XCVAR_DEF( rp_screen_message,	"",						NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP fix: [validation] RP_CVU_screenMessageTimer (g_cvar.c) clamps a negative value back to
// 0 -- see its comment for why.
XCVAR_DEF( rp_screen_message_timer,	"5",			RP_CVU_screenMessageTimer,	CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// DAJ_RP: [Corpses] how long, in seconds, the dead stay around -- used to be three hard-coded timers.
// The defaults are exactly the old values. Each is held to 0..RP_CORPSE_TIME_MAX (g_local.h) by its
// update callback in g_cvar.c and again where it is read (RP_CorpseSecondsToMs()), since corpses and
// limbs that never leave pile up and use up entity slots. Not latched: a change applies from the
// next death -- and for NPC corpses already lying there too, since that timer is re-read every frame.
//  - rp_npc_corpse_time: after an NPC's death animation, before its body fades out (BodyRemovalPadTime,
//    NPC.c). Droids and similar machines are still removed at once, as before.
//  - rp_limb_lifetime: a cut-off limb stays a random time between this and twice this (G_Dismember,
//    g_combat.c), so the limbs from one fight do not all vanish on the same frame. Was 8-16 seconds.
//  - rp_player_corpse_time: a player's corpse lies this long before it starts fading (BodySink,
//    g_client.c); the server drops it 18 seconds after that, as before.
XCVAR_DEF( rp_npc_corpse_time,		"10",			RP_CVU_npcCorpseTime,		CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_limb_lifetime,		"8",			RP_CVU_limbLifetime,		CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_player_corpse_time,	"30",			RP_CVU_playerCorpseTime,	CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// DAJ_RP: [Items] how long, in seconds, a dropped item stays on the ground (LaunchItem, g_items.c):
// weapons dropped on death or pulled out of a hand, items and powerups dropped on death, and anything
// thrown with /drop. Was a hard-coded 300 (5 minutes); the default is deliberately lower. Same 0..200
// clamp as the corpse times above. 0 removes a dropped item at once, i.e. nothing can be picked up.
// Set when the item is dropped, so a change applies to later drops only. CTF flags keep their 30 s.
XCVAR_DEF( rp_item_lifetime,		"100",			RP_CVU_itemLifetime,		CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP fix: [validation] RP_CVU_flameThrowerCooldown (g_cvar.c) clamps a negative value back to
// 0 -- see its comment for why.
XCVAR_DEF( rp_flame_thrower_cooldown,	"50",			RP_CVU_flameThrowerCooldown,	CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_flame_thrower_damage,	"2",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// DAJ_RP: [Ammo] RP_CVU_addAmmoScale (g_cvar.c) snaps it into RP_AMMO_SCALE_MIN..RP_AMMO_SCALE_MAX.
XCVAR_DEF( rp_add_ammo_scale,	"0.5",					RP_CVU_addAmmoScale,	CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_chat_protection_timer,	"0",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GLX_U: [AFK] seconds without input before a player goes AFK (bubble + comlink animation), 0 = off, /afk too.
// RP_CVU_afkTime (g_cvar.c) holds it to 0..3600.
XCVAR_DEF( rp_afk_time,				"120",		RP_CVU_afkTime,		CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_change_map_gametype_vote, "1",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_vote_timer,	"0",						NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_allow_saber_touch_damage, "1",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_allow_duel_saber_touch_damage, "0",		NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_duel_saberDmgDelay_Idle,		"350",		NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_duel_saberDamageScale,		"1",		NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_duel_radius,					"1024",		RP_CVU_duelRadius,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP fix: [Duel Tournament] CVAR_LATCH, for the same reason as rp_duel_tournament_duel_time
// below. The globe is spawned once, when the tournament starts, with "zykmodelscale" set from this
// cvar's value at that moment (Cmd_DuelMode_f) -- but every boundary test afterwards reads the LIVE
// cvar: duelists are killed for leaving the arena and everyone else for entering it (G_RunFrame),
// NPCs are killed and missiles, mines, detpacks and placed items removed inside it (g_main.c,
// g_missile.c, g_weapon.c, g_items.c, g_object.c). Change it mid-tournament and the visible globe no
// longer matches the invisible boundary: shrink it and a duelist standing inside the globe dies
// for "leaving", grow it and a spectator standing outside the globe dies for "entering". The
// clamp in RP_CVU_duelTournamentArenaScale() only bounds the value; the latch removes the mid-event
// route. trackChange qfalse for the reason given on the allow cvars above.
XCVAR_DEF( rp_duel_tournament_arena_scale, "800",		RP_CVU_duelTournamentArenaScale,				CVAR_ARCHIVE|CVAR_NORESTART|CVAR_LATCH,			qfalse )
// GalaxyRP fix: [Duel Tournament] CVAR_LATCH, because lowering this mid-match freezes the duelists
// for the rest of it. The arena-entry freeze in bg_pmove.c asks
// "(level.duel_tournament_timer - level.time) > (rp_duel_tournament_duel_time.integer - DUEL_TOURNAMENT_PROTECT_TIME)",
// and duel_tournament_timer was baked from this cvar's value when the match began. Drop the cvar
// from 180000 to 20000 while a match is running and the left side is still huge while the right
// side collapses, so the test goes true again and PM_ pins the duelist in place until the last
// couple of seconds of the match.
//
// RP_CVU_duelTournamentDuelTime()'s floor of 5000 does not cover this: it stops the value being set
// too low BEFORE a match, and 20000 passes it. The latch removes the mid-match route instead, which
// is the only one left. trackChange qfalse for the same reason as the two allow cvars above.
XCVAR_DEF( rp_duel_tournament_duel_time, "180000",	RP_CVU_duelTournamentDuelTime,				CVAR_ARCHIVE|CVAR_NORESTART|CVAR_LATCH,			qfalse )
XCVAR_DEF( rp_duel_tournament_min_players, "2",		NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_allow_jetpack_command,		"2",		NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue ) // GalaxyRP: logged-in players with the Jetpack skill only; see jetpack_command_allowed() in g_cmds.c
// GalaxyRP: [Grapple Hook] who may fire the hook, with rp_allow_jetpack_command's tiers: 0 nobody,
// 1 logged-out players and logged-in players with the Grapple Hook skill, 2 (default) logged-in
// players with the skill only. Clamped to that range by RP_CVU_allowGrappleHook() (g_cvar.c). See
// RP_GrappleAllowed() in g_cmds.c; the mode the hook pulls in is g_allowGrapple.
XCVAR_DEF( rp_allow_grapple_hook,		"2",		RP_CVU_allowGrappleHook,	CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP: [Logical Entities] whether the map loader and the Entity System may put spawn points,
// target_* relays, NPC spawners and the other never-networked classes into the logical entity
// region above MAX_GENTITIES (see q_shared.h). Latched and read ONCE at G_InitGame into
// level.logical_entities_enabled, so a change takes effect at the next map and never moves a live
// entity between regions. 0 puts everything in the networked table, exactly as before the feature.
XCVAR_DEF( rp_logical_entities,			"1",		NULL,				CVAR_ARCHIVE|CVAR_LATCH,						qfalse )
XCVAR_DEF( rp_server_empty_change_map_time, "0",		NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP: [NPC] a content filter, not a fix any more. 1 frees the spawners of the SP story cast
// (Tavion, the Kothos twins, Rosh, Kyle, Luke, Chewie, Alora, Boba), the droids, merchants and
// creatures on SP maps (level.sp_map, g_main.c) -- see the list in SP_NPC_spawner(). It was written
// against two problems that no longer exist: the 16-entry MAX_ANIM_FILES that crashed clients when
// too many non-humanoid models were loaded (128 since 3.47), and networked entity pressure (logical
// entities and the ZYK_ENTITY_RESERVE guards). The unconditional vjun3 protocol_imp/r2d2_imp drop
// that lived beside it has been folded in here; both types are on the list.
XCVAR_DEF( rp_sp_npc_fix,					"0",		NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_scale_siege_damage,		"0.7",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP fix: [Shop] renamed from zyk_allow_stun_baton_upgrade -- this only ever gated the Stun
// Baton Upgrade's door-unlock effect (its speed-debuff effect was never gated by it), so the old
// name overstated its scope; matches the rp_ prefix convention used by other RPG-specific cvars
// (e.g. rp_downed_timer).
// DAJ_RP: [Skills] the door-unlock effect it gates comes with Stun Baton skill level 5 now, no longer the upgrade.
XCVAR_DEF( rp_stun_baton_door_unlock,		"1",	NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP fix: [cleanup] renamed from zyk_allow_emotes to rp_allow_emotes -- matches this
// codebase's rp_ naming convention for GalaxyRP-authored cvars (see rp_allow_playsound_command above).
XCVAR_DEF( rp_allow_emotes,					"1",	NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP fix: [cleanup] renamed from zyk_allow_zyksound_command to rp_allow_playsound_command --
// matches the /playsound command name it actually gates instead of the old internal "zyksound" naming,
// and moves it onto this mod's rp_ prefix instead of the inherited zyk_ one.
XCVAR_DEF( rp_allow_playsound_command, "1",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP fix: [cleanup] renamed from zyk_allow_force_duel to rp_allow_force_duel -- it gates a
// GalaxyRP-authored command (/engage_fullforceduel), so it belongs on this mod's rp_ prefix rather
// than the inherited zyk_ one, the same move rp_allow_emotes and rp_allow_playsound_command above
// already made. Moved here from beside rp_allow_duel_tournament for the same reason: the name and
// the block it sits in should agree.
XCVAR_DEF( rp_allow_force_duel,			"1",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP fix: [Private Duel] CVAR_LATCH. This is read every frame (ClientThink_real sets the
// pmove tracemask from it), and at 1 duellists pass through other players, so two of them are
// often overlapping. Flip it 1 -> 0 at that moment and both start their next move inside a body
// that is solid again; PM's slide move never ejects from an all-solid start, so they stay stuck
// until the duel ends by death or distance. 0 -> 1 is harmless, but a setting that is only ever
// a server-style choice gains nothing from applying mid-map. trackChange qfalse as above.
XCVAR_DEF( rp_duel_no_collision,		"1",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART|CVAR_LATCH,			qfalse )
XCVAR_DEF( rp_duel_tournament_time_to_start, "12000", RP_CVU_duelTournamentTimeToStart,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_duel_tournament_rounds_per_match, "1",	NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP fix: [cleanup] renamed from zyk_buying_selling_cooldown to rp_buying_cooldown -- there has
// never been a working /sell command (see the Shop wording fixes elsewhere), so "selling" no longer
// belongs in the name, and this moves it onto this mod's rp_ prefix instead of the inherited zyk_ one.
XCVAR_DEF( rp_buying_cooldown,			"100",			NULL,				CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP: [Dice] time in milliseconds a player must wait between dice/coin commands. Shared by
// /roll, /rollall, /flipcoin and /flipcoinall. 0 disables the cooldown entirely.
XCVAR_DEF( rp_dice_roll_cooldown,		"3000",			RP_CVU_diceRollCooldown,	CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP fix: [cleanup] renamed from zyk_max_rpg_credits -- moves it onto this mod's rp_ prefix
// alongside the other GalaxyRP-authored cvars, and groups it with them here instead of leaving it
// stranded among the inherited zyk_ block. Maximum credits a character can hold in RPG Mode.
XCVAR_DEF( rp_max_rpg_credits,			"500000",		RP_CVU_maxRpgCredits,		CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP fix: [cleanup] renamed from zyk_rpg_max_level, same reasoning as rp_max_rpg_credits
// above. Highest level an RPG character can reach.
XCVAR_DEF( rp_rpg_max_level,			"100",			RP_CVU_rpgMaxLevel,			CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
// GalaxyRP fix: [cleanup] renamed from zyk_starting_shield, same reasoning as the two above.
// Shield a LOGGED-OUT player spawns with; an RPG character's starting shield comes from their Max
// Shield skill instead (pers.max_rpg_shield, see set_max_shield in g_cmds.c).
XCVAR_DEF( rp_starting_shield,			"25",			RP_CVU_startingShield,		CVAR_ARCHIVE|CVAR_NORESTART,					qtrue )
XCVAR_DEF( g_duelForcePowerDisable,	"0",			NULL,				CVAR_ARCHIVE|CVAR_LATCH|CVAR_SERVERINFO,						qtrue )

XCVAR_DEF( rp_default_account_permissions,		"0",	NULL,					CVAR_ARCHIVE | CVAR_NORESTART,					qtrue )
XCVAR_DEF( rp_pluginRequired,					"1",	RP_CVU_pluginRequired,	CVAR_ARCHIVE | CVAR_SERVERINFO,					qtrue )
XCVAR_DEF( rp_loginRequired,					"0",	NULL,					CVAR_ARCHIVE | CVAR_SERVERINFO,					qtrue )
// GLX_U: [Cosmetics] 1 = hats and capes are drawn, 0 = nobody sees them (they stay saved). Serverinfo so
// /cosmetics can say so -- see RP_CVU_allowCosmetics (g_cvar.c) and G_RpRepublishCosmetics (g_client.c).
XCVAR_DEF( rp_allow_cosmetics,				"1",	RP_CVU_allowCosmetics,	CVAR_ARCHIVE | CVAR_NORESTART | CVAR_SERVERINFO,	qtrue )
// GalaxyRP: [Account] 0 (default) -- /login, /new, /char new, /char use and /logout always force a
// respawn, the behaviour this mod has always had. 1 -- they apply in place and only force the
// respawn when the player is in a private duel or a live Duel Tournament match, which is the same
// restriction /updatesaber and /updateforce already use. Any value above 0 counts as on; a
// negative value behaves like 0, so no validator is needed.
XCVAR_DEF( rp_seamlesslogin,					"1",	NULL,					CVAR_ARCHIVE | CVAR_NORESTART,					qtrue )
// GalaxyRP fix: [Death System] rp_downed_timer decides whether the downed system runs at all, so it
// is CVAR_LATCH: a change waits for the next map. Without that, an admin flipping it to 0 mid-round
// would leave whoever was lying on the floor in a state that no longer exists -- /getup and /helpup
// both refuse a player who is not downed, and with the system off nothing would put them back. The
// engine prints "rp_downed_timer will be changed upon restarting." on the console, which is the
// feedback an admin needs; the game's own trackChange broadcast is qfalse here precisely because it
// would contradict that, reading vmCvar->string (still the OLD value until the map restarts) and
// announcing "changed to 30" a moment after the engine said the change was deferred.
//
// The clamp still works despite the latch: trap->Cvar_Set() from a VM goes through Cvar_VM_Set(),
// which calls Cvar_Set2() with force=qtrue, and the latch branch sits inside "if (!force)". So
// RP_CVU_downedTimer() below applies immediately -- at G_RegisterCvars(), which is exactly when a
// latched value takes effect.
//
// RP_CVU_downedTimer / RP_CVU_downedInvulnerabilityTimer (g_cvar.c) hold the ranges: 0 disables the
// downed system entirely, 30..100 is the live range, and the invulnerability timer is 0..30.
XCVAR_DEF( rp_downed_timer,						"30",	RP_CVU_downedTimer,	CVAR_ARCHIVE | CVAR_NORESTART | CVAR_LATCH,		qfalse )
XCVAR_DEF( rp_downed_invulnerability_timer,		"10",	RP_CVU_downedInvulnerabilityTimer,	CVAR_ARCHIVE | CVAR_NORESTART,					qtrue)
XCVAR_DEF( rp_allow_passive_regen,				"1",	NULL,					CVAR_ARCHIVE | CVAR_NORESTART,					qtrue)
XCVAR_DEF( rp_ammo_regen_timer,					"5",	NULL,					CVAR_ARCHIVE | CVAR_NORESTART,					qtrue)
XCVAR_DEF( rp_explosives_recharge_timer,		"30",	NULL,					CVAR_ARCHIVE | CVAR_NORESTART,					qtrue)
XCVAR_DEF( rp_allow_ammo_regen,					"1",	NULL,					CVAR_ARCHIVE | CVAR_NORESTART,					qtrue)
XCVAR_DEF( rp_allow_explosives_regen,			"1",	NULL,					CVAR_ARCHIVE | CVAR_NORESTART,					qtrue)

#undef XCVAR_DEF
