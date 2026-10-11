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

#ifdef XCVAR_PROTO
	#define XCVAR_DEF( name, defVal, update, flags ) extern vmCvar_t name;
#endif

#ifdef XCVAR_DECL
	#define XCVAR_DEF( name, defVal, update, flags ) vmCvar_t name;
#endif

#ifdef XCVAR_LIST
	#define XCVAR_DEF( name, defVal, update, flags ) { & name , #name , defVal , update , flags },
#endif

XCVAR_DEF( capturelimit,					"0",					NULL,				CVAR_ARCHIVE|CVAR_NORESTART|CVAR_SERVERINFO ) // fixme init'd to 8 in game module
XCVAR_DEF( cg_drawCrosshair,				"1",					NULL,				CVAR_ARCHIVE )
XCVAR_DEF( cg_drawCrosshairNames,			"1",					NULL,				CVAR_ARCHIVE )
XCVAR_DEF( cg_marks,						"1",					NULL,				CVAR_ARCHIVE )
XCVAR_DEF( cg_selectedPlayer,				"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( cg_selectedPlayerName,			"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( g_botsFile,						"",						NULL,				CVAR_INIT|CVAR_ROM )
XCVAR_DEF( g_spSkill,						"2",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( g_warmup,						"20",					NULL,				CVAR_ARCHIVE )
XCVAR_DEF( se_language,						"english",				NULL,				CVAR_ARCHIVE|CVAR_NORESTART )
XCVAR_DEF( ui_galaxyrp_game,				"0",					NULL,				CVAR_ROM ) // Tr!Force: [AssetsCache] Set game cvar
// GalaxyRP: [TaystJK] the same cvar cgame registers in cg_xcvar.h, with the same flags (flags OR
// together across modules); see RP_AdoptTaystJKWriteFolder() in bg_misc.c.
XCVAR_DEF( rp_taystjk_writefolder,				"1",					NULL,				CVAR_ARCHIVE )
// DAJ_RP: [Menus] the TaystJK engine's widescreen switch (its own default and flags; jaPRO's and TaystJK's ui register
// it too): on, the connect screen uses its 16:9 art and the cursor is not stretched -- see UI_WidthRatioCoef()
XCVAR_DEF( cl_ratioFix,						"1",					NULL,				CVAR_ARCHIVE )
XCVAR_DEF( ui_PrecacheModels,				"0",					NULL,				CVAR_ARCHIVE )
XCVAR_DEF( ui_actualNetGametype,			"3",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_blueteam,						DEFAULT_BLUETEAM_NAME,	NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_blueteam1,					"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_blueteam2,					"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_blueteam3,					"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_blueteam4,					"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_blueteam5,					"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_blueteam6,					"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_blueteam7,					"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_blueteam8,					"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_browserFilterInvalidInfo,		"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_browserShowEmpty,				"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_browserShowFull,				"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_browserShowPasswordProtected,	"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_bypassMainMenuLoad,			"0",					NULL,				CVAR_INTERNAL )
XCVAR_DEF( ui_captureLimit,					"5",					NULL,				CVAR_INTERNAL )
XCVAR_DEF( ui_char_anim,					"BOTH_WALK1",			NULL,				CVAR_ROM|CVAR_INTERNAL )
XCVAR_DEF( ui_char_color_blue,				"255",					NULL,				CVAR_ROM|CVAR_INTERNAL )
XCVAR_DEF( ui_char_color_green,				"255",					NULL,				CVAR_ROM|CVAR_INTERNAL )
XCVAR_DEF( ui_char_color_red,				"255",					NULL,				CVAR_ROM|CVAR_INTERNAL )
XCVAR_DEF( ui_char_model,					"jedi_tf",				NULL,				CVAR_ROM|CVAR_INTERNAL )
XCVAR_DEF( ui_char_skin_head,				"head_a1",				NULL,				CVAR_ROM|CVAR_INTERNAL )
XCVAR_DEF( ui_char_skin_legs,				"lower_a1",				NULL,				CVAR_ROM|CVAR_INTERNAL )
XCVAR_DEF( ui_char_skin_torso,				"torso_a1",				NULL,				CVAR_ROM|CVAR_INTERNAL )
XCVAR_DEF( ui_ctf_capturelimit,				"8",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_ctf_friendly,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_ctf_timelimit,				"30",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_currentMap,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_currentNetMap,				"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_currentOpponent,				"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_dedicated,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_ffa_fraglimit,				"20",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_ffa_timelimit,				"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_findPlayer,					"Kyle",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_forcePowerDisable,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_fragLimit,					"10",					NULL,				CVAR_INTERNAL )
XCVAR_DEF( ui_freeSaber,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_gametype,						"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_joinGametype,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_lastServerRefresh_0,			"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_lastServerRefresh_1,			"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_lastServerRefresh_2,			"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_lastServerRefresh_3,			"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_lastServerRefresh_4,			"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_lastServerRefresh_5,			"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_lastServerRefresh_6,			"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_mapIndex,						"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_menuFilesMP,					"ui/jampmenus.txt",		NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_netGametype,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_netSource,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_opponentName,					DEFAULT_BLUETEAM_NAME,	NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_rankChange,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_recordSPDemo,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_redteam,						DEFAULT_REDTEAM_NAME,	NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_redteam1,						"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL ) //rww - these used to all default to 0 (closed).. I changed them to 1 (human)
XCVAR_DEF( ui_redteam2,						"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_redteam3,						"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_redteam4,						"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_redteam5,						"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_redteam6,						"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_redteam7,						"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_redteam8,						"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_saber,						DEFAULT_SABER,			NULL,				CVAR_ROM|CVAR_INTERNAL )
XCVAR_DEF( ui_saber2,						"none",					NULL,				CVAR_ROM|CVAR_INTERNAL )
XCVAR_DEF( ui_saber2_color,					"yellow",				NULL,				CVAR_ROM|CVAR_INTERNAL )
XCVAR_DEF( ui_saber_color,					"yellow",				NULL,				CVAR_ROM|CVAR_INTERNAL )
XCVAR_DEF( ui_saber_type,					"single",				NULL,				CVAR_ROM|CVAR_INTERNAL )
// GalaxyRP: [Saber RGB] the saber menu's three colour sliders per blade. These are what the menu's
// ITEM_TYPE_INTSLIDER items bind to; UI_UpdateSaberCvars() packs them into cp_sbRGB1/cp_sbRGB2 on
// Apply, and UI_GetSaberCvars() unpacks them back when the menu is opened. Names match
// TaystJK/JAPro's so their saber menu, if a player is using a client that has it, drives ours too.
XCVAR_DEF( ui_sab1_r,						"255",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_sab1_g,						"255",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_sab1_b,						"255",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_sab2_r,						"255",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_sab2_g,						"255",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_sab2_b,						"255",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
// The packed values the sliders above feed into. Registered here as well as in cg_xcvar.h because
// the saber menu is reachable from the main menu, with no cgame loaded to have registered them --
// and a cvar first created by a plain Cvar_Set would not carry CVAR_USERINFO, so the server would
// never see the colour at all. See the matching comment in cg_xcvar.h.
XCVAR_DEF( cp_sbRGB1,						"0",					NULL,				CVAR_ARCHIVE|CVAR_USERINFO )
XCVAR_DEF( cp_sbRGB2,						"0",					NULL,				CVAR_ARCHIVE|CVAR_USERINFO )
XCVAR_DEF( ui_scoreAccuracy,				"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_scoreAssists,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_scoreBase,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_scoreCaptures,				"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_scoreDefends,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_scoreExcellents,				"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_scoreGauntlets,				"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_scoreImpressives,				"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_scorePerfect,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_scoreScore,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_scoreShutoutBonus,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_scoreSkillBonus,				"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_scoreTeam,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_scoreTime,					"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_scoreTimeBonus,				"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_selectedModelIndex,			"16",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_serverFilterType,				"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_serverStatusTimeOut,			"7000",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_screenshotType,				"jpg",					UI_UpdateScreenshot,	CVAR_ARCHIVE )
XCVAR_DEF( ui_singlePlayerActive,			"0",					NULL,				CVAR_INTERNAL )
XCVAR_DEF( ui_team_fraglimit,				"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_team_friendly,				"1",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_team_timelimit,				"20",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
// GalaxyRP: [Profile UI] whether the local player is currently logged into an account, pushed by
// the "supdateloggedin" server command (see CG_LoggedInUpdate_f / Cmd_GalaxyRpUi_f). Registered
// with an explicit "0" default, same as the other zyk rpg fields below, so the Profile menu's
// cvarTest gating has a defined (logged-out) value even before the first server response arrives.
// GalaxyRP fix: must be CVAR_ROM (matching cgame's own registration of the same cvar in
// cg_xcvar.h), not CVAR_ARCHIVE -- cvar flags OR together across modules (see Cvar_Get() in
// cvar.cpp), so an ARCHIVE flag here alone was enough to make the whole cvar archive to the
// client's config and reload stale on the next launch, showing a logged-in menu for a player who
// quit, reconnected, and stayed logged out/spectating (see the CMD_ALIVE fix on "zykmod" in
// g_cmds.c for the other half of this bug).
XCVAR_DEF( ui_loggedin,					"0",					NULL,				CVAR_ROM|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_level,				"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_skillpoints,			"20",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_credits,				"20",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_1_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_2_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_3_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_4_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_5_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_6_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_7_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_8_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_9_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_10_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_11_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_12_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_13_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_14_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_15_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_16_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_17_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_18_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_19_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_20_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_21_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_22_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_23_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_24_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_25_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_26_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_27_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_28_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_29_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_30_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_31_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_32_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_33_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_34_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_35_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_36_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_37_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_38_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_39_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_40_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_41_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_42_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_43_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_44_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_45_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_46_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_47_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_48_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_49_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_50_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_51_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_52_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_53_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_54_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_55_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_skill_56_level,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_rp_skill_57_level,			"0",					NULL,				CVAR_ARCHIVE | CVAR_INTERNAL)
XCVAR_DEF( ui_rp_skill_58_level,			"0",					NULL,				CVAR_ARCHIVE | CVAR_INTERNAL)
XCVAR_DEF( ui_rp_skill_59_level,			"0",					NULL,				CVAR_ARCHIVE | CVAR_INTERNAL)
XCVAR_DEF( ui_rp_skill_60_level,			"0",					NULL,				CVAR_ARCHIVE | CVAR_INTERNAL)
// GalaxyRP fix: [Skills] ui_rp_skill_61_level and ui_rp_skill_62_level used to follow here, and
// ui_zyk_rpg_skillcounter used to sit up with the other ui_zyk_rpg_* cvars. All three are removed:
// NUM_OF_SKILLS (rp_local.h) is 60, so the server only ever sends 60 skill levels and 61/62 could
// never receive a value -- they were in fact the cause of the settings-panel misalignment fixed in
// ui_cvars_in_order[] (cg_servercmds.c), where two extra slots shifted every setting value that
// followed. ui_zyk_rpg_skillcounter had no reader or writer anywhere at all.
// NOTE for future cleanups: skills 39 and 56 are NOT dead even though no menu file references
// ui_zyk_skill_39_level or ui_zyk_skill_56_level. Those are the two reserved skills ("Unique Skill"
// and "Improvements") that do_upgrade_skill()/do_downgrade_skill() block from purchase (g_cmds.c);
// the server still sends a value for them, because Cmd_GalaxyRpUi_f walks all NUM_OF_SKILLS entries.
// CG_ZykMod assigns those values into ui_cvars_in_order[] purely by position, so their two cvars have
// to stay as placeholders -- removing them would shift every skill level after them by one.
// GalaxyRP fix: [Settings] ui_zyk_setting_0_value through _15_value (the full 0-15 range) have now all
// been removed here. _0_value through _4_value, _7_value, _12_value, _14_value and _15_value went in
// earlier passes of this cleanup; _5_value, _6_value, _8_value, _9_value, _10_value, _11_value and
// _13_value are removed in this pass along with the settings_to_sync loop that used to feed the last
// six of them (see the fix comment on ui_cvars_in_order[] in cg_servercmds.c) and, for the settings
// that toggled them, Cmd_Settings_f itself (g_cmds.c). None of these cvars was ever read or written by
// any C code anywhere in codemp/game, codemp/cgame or codemp/ui -- they only ever fed the Settings
// panel in ingame_galaxyrp.menu, which was removed entirely in an earlier pass of this cleanup.
// GalaxyRP fix: [Shop] "1" if the player already owns the matching shop upgrade (player_settings bit
// 0/1/2), "0" otherwise -- fed by Cmd_GalaxyRpUi_f's zykmod sync (g_cmds.c) via the 3 new entries
// appended to ui_cvars_in_order[] in cg_servercmds.c. Drives the Shop -> Upgrades panel's
// cvarTest/disableCvar gate in ingame_galaxyrp.menu, which greys out and disables the button for an
// upgrade already owned. Numbered 1-3 to match the /buy upgrade 1-3 numbering used everywhere else in
// this area of code. The pre-existing ui_zyk_upgrade_0_value through _16_value cvars that used to sit
// directly below these three have now been removed: they were leftovers of Zyk's original "Unique
// Upgrades" panel, whose writer (a block of trap->Cvar_Set calls in CG_ZykMod, cg_servercmds.c) was
// deleted in "Began fixing the GalaxyRP UI" and whose only reader (the matching itemDef cvar bindings
// in the old ingame_zykmod.menu) was deleted in "More UI refactoring, also fixed the buy button" --
// leaving 17 XCVAR_DEFs with nothing on either end of them. They were never part of the positional
// ui_cvars_in_order[] sync, so removing them cannot shift that array's alignment.
XCVAR_DEF( ui_zyk_upgrade_1_owned,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_upgrade_2_owned,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_upgrade_3_owned,			"0",					NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
// GalaxyRP fix: [Quests] 40 quest-era cvars used to be defined here and just above:
// ui_zyk_rpg_level_up_score, ui_zyk_rpg_magic_power, ui_zyk_rpg_rpgclass, the light / dark /
// eternity / universe / ultimate / resurrection _power switches, ui_zyk_special_powers,
// ui_zyk_quest_player, and the 29 quest text cvars (ui_zyk_universe_chapter, ui_zyk_universe_text..4,
// ui_zyk_light_text..11, ui_zyk_dark_text..11, ui_zyk_eternity_text, _text2). No code in any module
// reads or writes them; the only mention left is the orphaned Quests section of ingame_galaxyrp.menu,
// which no button opens, and a menu reads an unregistered cvar as empty.
XCVAR_DEF( ui_zyk_action_value,				"^2upgrade",			NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
// GalaxyRP fix: [Shop] ui_zyk_buy_action_value removed -- it only backed the orphaned "buyaction"
// uiScript branch removed from ui_main.c (zykmod handler), which no button ever triggered and whose
// value no menu ever displayed.
XCVAR_DEF( ui_zyk_rpg_char_1,				"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_char_2,				"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_char_3,				"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_char_4,				"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_char_5,				"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_char_6,				"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_char_7,				"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_char_8,				"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_char_9,				"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_char_10,				"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_char_11,				"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_char_12,				"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_char_13,				"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_char_14,				"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_char_15,				"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_zyk_rpg_current_char,			"",						NULL,				CVAR_ARCHIVE|CVAR_INTERNAL )
XCVAR_DEF( ui_rp_xp_value,					"0",					NULL,				CVAR_ARCHIVE | CVAR_INTERNAL)
XCVAR_DEF(ui_rp_max_xp,			"0",					NULL,				CVAR_ARCHIVE | CVAR_INTERNAL)

#undef XCVAR_DEF
