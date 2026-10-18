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

#include "g_local.h"

//
// Cvar callbacks
//

/*
static void CVU_Derpity( void ) {
	// ...
}
*/

// GalaxyRP fix: [validation] rp_screen_message_timer and rp_flame_thrower_cooldown are read as
// plain countdown lengths (a value is copied out of the cvar once, then only ever decremented
// toward 0 -- see motdTime in g_client.c/g_active.c). Neither validated its value, so a negative
// setting produced a counter that counted away from zero forever instead of toward it, since
// decrementing a negative number never reaches 0 -- leaving a MOTD stuck on-screen indefinitely.
// Clamp back to 0 the moment the cvar changes, using this codebase's existing XCVAR
// update-callback mechanism (see G_UpdateCvars() below) rather than re-validating at every read
// site.
//
// The two downed-system timers used to share this helper. They have real ranges now rather than
// just a floor of 0, so they have their own callbacks further down; this one is unchanged and
// keeps its two remaining callers.
static void RP_ClampNonNegativeCvar(vmCvar_t* cvar, const char* cvarName)
{
	if (cvar->integer < 0)
	{
		trap->Cvar_Set(cvarName, "0");
		trap->Cvar_Update(cvar);
	}
}

// GalaxyRP fix: [Death System] the ranges rp_downed_timer and rp_downed_invulnerability_timer are
// held to. Both are read as plain countdown lengths, and neither was bounded above at all.
//
// rp_downed_timer is the sharper of the two, because it sets the PRICE of being downed while
// rp_downed_invulnerability_timer sets the REWARD: paralyze_player() puts a downed player on
// RP_DOWNED_HEALTH instead of killing them, and help_up() hands out that many seconds of
// EF_INVULNERABLE once they stand. At the shipped 30 against 10 that is a net loss, which is the
// point. Set the timer below the invulnerability and it inverts -- taking a lethal hit becomes worth
// more than it costs, and a player who can find someone to shoot them is invulnerable more or less
// continuously. Below 3 they could even stand while still invulnerable from the knockdown itself.
//
// So: 0 turns the downed system off outright (see RP_DownedSystemEnabled() in g_utils.c -- a lethal
// hit simply kills, the way it did before the system existed), anything else lands in 30..100, and
// a value over 100 comes down to 100 rather than being taken literally.
//
// The ceiling is tested BEFORE the floor, and that order is load-bearing rather than stylistic.
// Cvar values arrive through atoi(), which wraps: "99999999999" reads as 1215752191. An admin who
// types a huge number means "a very long time", so it has to land on 100 -- floor-first would work
// here by luck, but ceiling-first says what is meant. (Nothing rescues "2147483648", which atoi
// gives as -2147483648 and which therefore reads as 0; the engine still echoes what was typed.)
#define RP_DOWNED_TIMER_MIN				30
#define RP_DOWNED_TIMER_MAX				100
#define RP_DOWNED_INVULNERABILITY_MIN	0
#define RP_DOWNED_INVULNERABILITY_MAX	30

// GalaxyRP fix: [validation] snap a cvar to a value and say so on the console. The caller decides
// what the value should be; this exists so an admin who types 5 and gets 30 can find out why
// without reading the source. Silent on a no-op, so a value already in range prints nothing.
static void RP_SnapCvar(vmCvar_t* cvar, const char* cvarName, int newValue, const char* reason)
{
	if (cvar->integer == newValue)
	{
		return;
	}

	trap->Print("%s: %i is out of range (%s) -- using %i.\n", cvarName, cvar->integer, reason, newValue);

	trap->Cvar_Set(cvarName, va("%i", newValue));
	trap->Cvar_Update(cvar);
}

void RP_CVU_downedTimer(void)
{
	// rp_downed_timer is CVAR_LATCH (see g_xcvar.h), so this is only ever effective at
	// G_RegisterCvars() -- which is exactly when a latched value takes effect. It still applies
	// rather than latching again, because trap->Cvar_Set() forces (Cvar_VM_Set -> Cvar_Set2 with
	// force=qtrue) and the latch branch is inside "if (!force)".
	if (rp_downed_timer.integer > RP_DOWNED_TIMER_MAX)
	{
		RP_SnapCvar(&rp_downed_timer, "rp_downed_timer", RP_DOWNED_TIMER_MAX, "maximum is 100");
	}
	else if (rp_downed_timer.integer <= 0)
	{
		// Covers negatives as well. 0 is a real setting here, not a rejection: it disables the
		// downed system, so there is nothing to snap it up to.
		RP_SnapCvar(&rp_downed_timer, "rp_downed_timer", 0, "0 disables the downed system");
	}
	else if (rp_downed_timer.integer < RP_DOWNED_TIMER_MIN)
	{
		RP_SnapCvar(&rp_downed_timer, "rp_downed_timer", RP_DOWNED_TIMER_MIN, "minimum is 30, or 0 to disable");
	}
}

void RP_CVU_downedInvulnerabilityTimer(void)
{
	// 0 stays meaningful here -- help_up() tests "if (rp_downed_invulnerability_timer.integer)", so
	// 0 means a revived player gets no invulnerability at all. Only the ceiling and negatives need
	// handling, which makes this a plain range unlike the timer above.
	if (rp_downed_invulnerability_timer.integer > RP_DOWNED_INVULNERABILITY_MAX)
	{
		RP_SnapCvar(&rp_downed_invulnerability_timer, "rp_downed_invulnerability_timer",
			RP_DOWNED_INVULNERABILITY_MAX, "maximum is 30");
	}
	else if (rp_downed_invulnerability_timer.integer < RP_DOWNED_INVULNERABILITY_MIN)
	{
		RP_SnapCvar(&rp_downed_invulnerability_timer, "rp_downed_invulnerability_timer",
			RP_DOWNED_INVULNERABILITY_MIN, "minimum is 0");
	}
}

void RP_CVU_screenMessageTimer(void)
{
	RP_ClampNonNegativeCvar(&rp_screen_message_timer, "rp_screen_message_timer");
}

// GalaxyRP fix: [validation] rp_flame_thrower_cooldown is read unclamped in Player_FireFlameThrower()
// (g_main.c) as self->client->cloakDebReduce = level.time + rp_flame_thrower_cooldown.integer -- a
// negative value pushes cloakDebReduce into the past, so the "cloakDebReduce < level.time" cooldown
// gate is satisfied on effectively every server frame instead of respecting any cooldown at all,
// letting the flamethrower re-fire (and re-deal damage) as fast as the server tick rate allows. Clamp
// back to 0 the moment the cvar changes, same as the timer cvars above.
void RP_CVU_flameThrowerCooldown(void)
{
	RP_ClampNonNegativeCvar(&rp_flame_thrower_cooldown, "rp_flame_thrower_cooldown");
}

// DAJ_RP: [Corpses] the three corpse and limb lifetimes (see g_xcvar.h). Negative values would put the
// removal time in the past, and very large ones keep corpses and limbs around long enough to fill the
// entity table in a big fight, so each is snapped into 0..RP_CORPSE_TIME_MAX the moment it changes.
static void RP_ClampCorpseTimeCvar(vmCvar_t* cvar, const char* cvarName)
{
	if (cvar->integer < 0)
	{
		RP_SnapCvar(cvar, cvarName, 0, "minimum is 0");
	}
	else if (cvar->integer > RP_CORPSE_TIME_MAX)
	{
		RP_SnapCvar(cvar, cvarName, RP_CORPSE_TIME_MAX, "maximum is 200");
	}
}

void RP_CVU_npcCorpseTime(void)
{
	RP_ClampCorpseTimeCvar(&rp_npc_corpse_time, "rp_npc_corpse_time");
}

void RP_CVU_limbLifetime(void)
{
	RP_ClampCorpseTimeCvar(&rp_limb_lifetime, "rp_limb_lifetime");
}

void RP_CVU_playerCorpseTime(void)
{
	RP_ClampCorpseTimeCvar(&rp_player_corpse_time, "rp_player_corpse_time");
}

// DAJ_RP: [Items] rp_item_lifetime (dropped items) uses the same 0..200 second range, so it shares the
// clamp above and RP_CorpseSecondsToMs() below even though it is not a corpse.
void RP_CVU_itemLifetime(void)
{
	RP_ClampCorpseTimeCvar(&rp_item_lifetime, "rp_item_lifetime");
}

// Seconds to milliseconds for the three corpse readers and LaunchItem() (rp_item_lifetime), clamped again so the result is always 0..200000
// whatever the cvar holds -- the callbacks above run on change and at registration, but this keeps
// the timers safe on their own.
int RP_CorpseSecondsToMs(int seconds)
{
	if (seconds < 0)
		seconds = 0;
	else if (seconds > RP_CORPSE_TIME_MAX)
		seconds = RP_CORPSE_TIME_MAX;

	return seconds * 1000;
}

// DAJ_RP: [Ammo] rp_add_ammo_scale scales the ammo an ammo item or a picked-up weapon gives, and a
// dropped weapon stores its ammo divided by it (see RP_DropAmmoCount() in g_items.c). Out of range it
// was an exploit or a trap: above 1 a weapon dropped with a round or two stored 0, which the pickup
// reads as "the full default amount"; 0 used up items for nothing; a negative scale made pickups
// subtract ammo, down past zero, and that was saved. So it is held to 0.01..1.0 -- snapped the moment
// it changes and at registration, with a console line, so /rp_add_ammo_scale shows the value in
// force. A non-number reads as 0 and a NaN fails both tests; both land on the minimum.
// RP_AmmoScale() clamps again at every read, so the arithmetic never depends on this having run.
void RP_CVU_addAmmoScale(void)
{
	const float value = rp_add_ammo_scale.value;
	float snapped;

	if (value >= RP_AMMO_SCALE_MIN && value <= RP_AMMO_SCALE_MAX)
	{
		return;
	}

	snapped = (value > RP_AMMO_SCALE_MAX) ? RP_AMMO_SCALE_MAX : RP_AMMO_SCALE_MIN;

	trap->Print("rp_add_ammo_scale: %s is out of range (0.01 to 1.0) -- using %g.\n", rp_add_ammo_scale.string, snapped);

	trap->Cvar_Set("rp_add_ammo_scale", va("%g", snapped));
	trap->Cvar_Update(&rp_add_ammo_scale);
}

// GalaxyRP fix: [validation] same class as the timer cvars above, for the duel/minigame cvars that
// were missed by that pass. Each of these has a value range below which its feature does not merely
// behave oddly but becomes permanently unusable, so each clamps to the lowest value that still
// works rather than to 0 (which for most of them IS the broken value). A clamped-away setting is
// reported the same way the others are -- by simply correcting the cvar, so an admin who inspects it
// sees the value actually in force.
static void RP_ClampCvarMinimum(vmCvar_t* cvar, const char* cvarName, int minimum)
{
	if (cvar->integer < minimum)
	{
		trap->Cvar_Set(cvarName, va("%i", minimum));
		trap->Cvar_Update(cvar);
	}
}

// rp_duel_radius is compared against the distance between the two duelists every frame
// (g_active.c): at 0 or below, "too far apart" is true immediately, so every private duel ends on
// its very first frame and duelling is impossible. 100 units is close quarters but functional.
void RP_CVU_duelRadius(void)
{
	RP_ClampCvarMinimum(&rp_duel_radius, "rp_duel_radius", 100);
}

// The Duel Tournament arena's kill radius is DUEL_TOURNAMENT_ARENA_SIZE * scale / 100 (g_main.c),
// while duelists are teleported to +/-125 units from the arena centre. Below a scale of 200 that
// radius is smaller than the distance they spawn at, so both duelists are killed for leaving the
// arena on the first frame of every match.
void RP_CVU_duelTournamentArenaScale(void)
{
	RP_ClampCvarMinimum(&rp_duel_tournament_arena_scale, "rp_duel_tournament_arena_scale", 200);
}

// Duelists are frozen in place for the first DUEL_TOURNAMENT_PROTECT_TIME (2000ms) of a match
// (bg_pmove.c). A duel_time at or below that leaves them frozen for the entire match, so every
// match runs its clock out at full health and is scored as a tie -- no duel is ever actually
// fought. 5000ms gives a (very short) 3 seconds of real duelling.
void RP_CVU_duelTournamentDuelTime(void)
{
	RP_ClampCvarMinimum(&rp_duel_tournament_duel_time, "rp_duel_tournament_duel_time", 5000);
}

// This sets "duel_tournament_timer = level.time + cvar" when the first player signs up, and the
// mode's per-frame handler ends the event the moment that timer elapses with too few players. At 0
// or below the timer is already in the past, so the first person to join instantly ends the event
// they just started and the mode can never be entered at all.
void RP_CVU_duelTournamentTimeToStart(void)
{
	RP_ClampCvarMinimum(&rp_duel_tournament_time_to_start, "rp_duel_tournament_time_to_start", 1000);
}

// GalaxyRP: [Sniper Battle] RP_CVU_sniperBattleTimeToStart() used to sit here, clamping
// zyk_sniper_battle_time_to_start the same way. Both the callback and the cvar went with the
// Sniper Battle removal (see g_xcvar.h).

// GalaxyRP: [Dice] rp_dice_roll_cooldown is added to level.time to schedule pers.dice_roll_timer.
// 0 is a legitimate setting (it means "no cooldown"), and a negative value would mean the same thing
// while reading as if it did something else -- so the floor is 0 rather than a minimum that works.
// The ceiling matters more: level.time is milliseconds since the map loaded, so on a server that has
// been up for a day an unbounded cooldown could push level.time + cooldown past INT_MAX and wrap the
// timer into the past, silently disabling the very cooldown it was set to enforce. A minute is far
// beyond any sane RP setting and leaves the sum nowhere near the limit.
void RP_CVU_diceRollCooldown(void)
{
	if (rp_dice_roll_cooldown.integer < 0)
	{
		trap->Cvar_Set("rp_dice_roll_cooldown", "0");
		trap->Cvar_Update(&rp_dice_roll_cooldown);
	}
	else if (rp_dice_roll_cooldown.integer > 60000)
	{
		trap->Cvar_Set("rp_dice_roll_cooldown", "60000");
		trap->Cvar_Update(&rp_dice_roll_cooldown);
	}
}

// GalaxyRP fix: [validation] rp_max_rpg_credits is the upper bound both add_credits() and
// remove_credits() (g_cmds.c) clamp against. A negative setting used to produce negative balances
// from BOTH of them: each tests its two bounds in a single if/else-if chain, so whichever bound is
// hit first wins outright, and with a negative ceiling the ceiling is hit first in add_credits()
// while remove_credits()'s "already below zero?" test is simply false for any non-negative result.
// A negative balance then persists to the Characters table like any other. Both functions have been
// made order-independent as well (see their own fix comments), so this clamp is the second line of
// defence rather than the only one -- but it is the one that keeps the value an admin inspects
// honest.
//
// The floor is 1 rather than 0: a ceiling of 0 pins every balance at 0, which reads as a broken
// economy rather than a configured one, and the shop/give/spend commands would all still report
// success against it. No explicit upper clamp is needed -- vmCvar_t.integer is an int, so INT_MAX
// is already the highest value this can hold, and a config string beyond it can only come back as
// some other in-range int (negative ones are caught by the floor below).
void RP_CVU_maxRpgCredits(void)
{
	RP_ClampCvarMinimum(&rp_max_rpg_credits, "rp_max_rpg_credits", 1);
}

// GalaxyRP fix: [validation] rp_rpg_max_level bounds every RPG character's level, and level feeds
// straight into set_max_health() (100 + level*2) whose result is published in ps.stats
// [STAT_MAX_HEALTH] -- a 16-bit netfield (MSG_WriteShort in qcommon/msg.cpp), so a high enough cap
// wraps every client's health readout negative. At the low end, a cap of 0 or less makes
// increase_level()'s loop a permanent no-op, silently freezing progression server-wide. Neither
// failure reports itself, so the value is bounded to a range that is comfortably safe at both ends:
// 10 is low but playable, 300 is three times the shipped default and nowhere near the netfield
// limit.
void RP_CVU_rpgMaxLevel(void)
{
	if (rp_rpg_max_level.integer < 10)
	{
		trap->Cvar_Set("rp_rpg_max_level", "10");
		trap->Cvar_Update(&rp_rpg_max_level);
	}
	else if (rp_rpg_max_level.integer > 300)
	{
		trap->Cvar_Set("rp_rpg_max_level", "300");
		trap->Cvar_Update(&rp_rpg_max_level);
	}
}

// GalaxyRP fix: [validation] rp_starting_shield is written straight into ps.stats[STAT_ARMOR], which
// is a 16-bit netfield (MSG_WriteShort in qcommon/msg.cpp): a value above 32767 arrives at the client
// as a negative number while the server keeps running damage against the real one. A NEGATIVE value
// is worse than cosmetic -- in G_Damage (g_combat.c) a negative STAT_ARMOR fails the ">= scaled_damage"
// test and falls into the proportional branch, where "asave = take * (STAT_ARMOR / scaled_damage)" is
// itself negative and the following "take -= asave" therefore INCREASES the damage the player takes
// before the armour is zeroed. One free extra-damage hit per spawn, from a setting that reads like it
// should merely give less shield. Bounded to [0, 100]: 0 is a legitimate "no starting shield", and 100
// is the ceiling everything else already works against -- the cvar only ever reaches logged-out
// players (ClientSpawn() writes it, then initialize_rpg_skills() overwrites it for characters with
// pers.max_rpg_shield), their STAT_MAX_HEALTH is Com_Clampi(1, 100, handicap), and STAT_MAX_ARMOR /
// Pickup_Armor() refuse any shield above that. The bound used to be 200: nothing capped the value
// itself, so a player spawned with 200 and kept it until hit, but no pickup could ever refill above
// 100, which made the top half of the range a one-shot bonus rather than a setting. Whatever the
// bound, it must stay <= 255: PERS_ATTACKEE_ARMOR packs the armour into eight bits (g_combat.c).
void RP_CVU_startingShield(void)
{
	if (rp_starting_shield.integer < 0)
	{
		trap->Cvar_Set("rp_starting_shield", "0");
		trap->Cvar_Update(&rp_starting_shield);
	}
	else if (rp_starting_shield.integer > 100)
	{
		trap->Cvar_Set("rp_starting_shield", "100");
		trap->Cvar_Update(&rp_starting_shield);
	}
}

// GalaxyRP fix: [Melee] g_debugMelee is pinned on -- see the long comment on its XCVAR_DEF in
// g_xcvar.h for why it is pinned rather than removed. It is not a validation clamp like the ones
// above: there is no range to enforce, only one supported value, so this snaps anything else back.
// G_RegisterCvars() runs update callbacks at registration as well as on change, so a config that
// still says "set g_debugMelee 0" is corrected before the first frame rather than at some later
// point, and an admin who inspects the cvar always sees the value actually in force.
void RP_CVU_debugMelee(void)
{
	if (g_debugMelee.integer != 1)
	{
		trap->Cvar_Set("g_debugMelee", "1");
		trap->Cvar_Update(&g_debugMelee);
	}
}

// GalaxyRP fix: [Jedi vs Merc] g_jediVmerc is pinned off -- see the long comment on its XCVAR_DEF in
// g_xcvar.h for what it does and why it is pinned rather than removed. Like the melee pin above this
// is not a range clamp: zero is the only supported value, so this snaps anything else back.
//
// Written WITHOUT the "if the value is wrong" guard that RP_CVU_debugMelee uses, and the difference
// matters. An admin's "set g_jediVmerc 1" from the console or rcon goes through Cvar_Set2() with
// force = qfalse, which for a CVAR_LATCH cvar leaves .integer at 0 and parks "1" in latchedString
// for the next map -- while still bumping modificationCount, so G_UpdateCvars() does call us. A
// guarded callback would look at .integer, see 0, and leave that "1" queued. The unconditional call
// below takes Cvar_Set2()'s early-out for "the latched value is being superseded by the value
// already in force", which frees latchedString and returns. So the queue is cleared at the moment
// the admin types it, and /cvarlist tells them the truth for the rest of the map.
//
// It is cheap to call unconditionally: with no latch pending and the value already "0", Cvar_Set2()
// returns on its own "not changed" test without touching cvar_modifiedFlags, so there is no
// serverinfo churn.
//
// G_RegisterCvars() runs update callbacks at registration as well as on change, and runs at the top
// of G_InitGame() -- before any client can spawn -- so a config line or an archived value saying 1
// is corrected before the first frame rather than after somebody has already spawned under it.
void RP_CVU_jediVmerc(void)
{
	trap->Cvar_Set("g_jediVmerc", "0");
	trap->Cvar_Update(&g_jediVmerc);
}

// GalaxyRP fix: [Scoreboard] timelimit, fraglimit, capturelimit and duel_fraglimit are pinned at 0.
// Maps on an RP server are not meant to end on their own, and since the scoreboard switched to
// downs/deaths the kill score these limits were built around is no longer counted (AddScore() in
// g_combat.c leaves PERS_SCORE at 0), which left them half-working:
//   timelimit      -- in FFA with two or more players everyone is tied at 0, and CheckExitRules()
//                     (g_main.c) waits out a tie ("sudden death") before it even looks at the time,
//                     so the limit never fired there; in other gametypes it would end the map
//   fraglimit      -- did nothing in FFA and Duel (PERS_SCORE never reaches it), but in Team FFA it
//                     would still end the map on the team kill total, which AddScore() does keep
//   capturelimit   -- CTF/CTY, reachable because players may vote the gametype
//   duel_fraglimit -- Duel/Power Duel win limit (and Power Duel's lone-player health/force-regen
//                     scaling, which has its own fallback for 0)
// Kept registered and in serverinfo, so server browsers and every client read an honest 0. Like
// RP_CVU_debugMelee, guarded rather than unconditional: none of these is CVAR_LATCH, so a bad value
// is already in effect when the callback runs and there is nothing queued to clear. The test is on
// the string, which also catches values that only look like 0 as a number (a fractional timelimit,
// or junk). G_RegisterCvars() runs the callbacks at registration, so an archived value or a config
// line is corrected before the first frame; the console, rcon, a passed vote and the "fraglimit 1"
// that g_fraglimitVoteCorrection issues when a vote switches to Duel are all corrected on change.
static void RP_PinCvarToZero( vmCvar_t *vmCvar, const char *name )
{
	if ( strcmp( vmCvar->string, "0" ) )
	{
		trap->Print( "%s is fixed at 0 on this server.\n", name );
		trap->Cvar_Set( name, "0" );
		trap->Cvar_Update( vmCvar );
	}
}

void RP_CVU_timelimit(void)			{ RP_PinCvarToZero( &timelimit, "timelimit" ); }
void RP_CVU_fraglimit(void)			{ RP_PinCvarToZero( &fraglimit, "fraglimit" ); }
void RP_CVU_capturelimit(void)		{ RP_PinCvarToZero( &capturelimit, "capturelimit" ); }
void RP_CVU_duelFraglimit(void)		{ RP_PinCvarToZero( &duel_fraglimit, "duel_fraglimit" ); }

// GalaxyRP: [Grapple Hook] g_allowGrapple is the mode switch and only the mode switch. TaystJK reads
// 0 as "off" and >1 as the winch; here who may use the hook is rp_allow_grapple_hook, so a 0 typed
// here by habit would otherwise leave the hook ON and pulling in a mode nobody asked for (the winch
// branch is the fallthrough). Anything but 1 or 2 is put back to the default with a note naming the
// cvar the admin probably wanted. Guarded on the value, unlike the jediVmerc pin above, because this
// cvar is not latched: a bad value is in .integer by the time we run, so there is nothing queued to
// clear and a guard is enough.
void RP_CVU_allowGrapple(void)
{
	if (g_allowGrapple.integer != 1 && g_allowGrapple.integer != 2)
	{
		trap->Print("g_allowGrapple: \"%s\" is not a mode (1 = swing, 2 = winch), reset to 2. "
					"To control who may use the hook, set rp_allow_grapple_hook.\n", g_allowGrapple.string);
		trap->Cvar_Set("g_allowGrapple", "2");
		trap->Cvar_Update(&g_allowGrapple);
	}
}

// GalaxyRP: [Grapple Hook] rp_allow_grapple_hook is a three-way tier (0 nobody, 1 logged-out too,
// 2 skill holders only); below 0 is 0, above 2 is 2. RP_GrappleAllowed() (g_cmds.c) only ever tests
// "<= 0" and "== 1", so an out-of-range value would silently behave like 2 anyway -- the clamp makes
// /cvarlist tell the truth about it.
void RP_CVU_allowGrappleHook(void)
{
	if (rp_allow_grapple_hook.integer < 0)
	{
		trap->Cvar_Set("rp_allow_grapple_hook", "0");
		trap->Cvar_Update(&rp_allow_grapple_hook);
	}
	else if (rp_allow_grapple_hook.integer > 2)
	{
		trap->Cvar_Set("rp_allow_grapple_hook", "2");
		trap->Cvar_Update(&rp_allow_grapple_hook);
	}
}


//
// Cvar table
//

typedef struct cvarTable_s {
	vmCvar_t	*vmCvar;
	char		*cvarName;
	char		*defaultString;
	void		(*update)( void );
	uint32_t	cvarFlags;
	qboolean	trackChange; // announce if value changes
} cvarTable_t;

#define XCVAR_DECL
	#include "g_xcvar.h"
#undef XCVAR_DECL

static const cvarTable_t gameCvarTable[] = {
	#define XCVAR_LIST
		#include "g_xcvar.h"
	#undef XCVAR_LIST
};
static const size_t gameCvarTableSize = ARRAY_LEN( gameCvarTable );

void G_RegisterCvars( void ) {
	size_t i = 0;
	const cvarTable_t *cv = NULL;

	for ( i=0, cv=gameCvarTable; i<gameCvarTableSize; i++, cv++ ) {
		trap->Cvar_Register( cv->vmCvar, cv->cvarName, cv->defaultString, cv->cvarFlags );
		if ( cv->update )
			cv->update();
	}
}

void G_UpdateCvars( void ) {
	size_t i = 0;
	const cvarTable_t *cv = NULL;

	for ( i=0, cv=gameCvarTable; i<gameCvarTableSize; i++, cv++ ) {
		if ( cv->vmCvar ) {
			int modCount = cv->vmCvar->modificationCount;
			trap->Cvar_Update( cv->vmCvar );
			if ( cv->vmCvar->modificationCount != modCount ) {
				if ( cv->update )
					cv->update();

				if ( cv->trackChange )
					trap->SendServerCommand( -1, va("print \"Server: %s changed to %s\n\"", cv->cvarName, cv->vmCvar->string ) );
			}
		}
	}
}
