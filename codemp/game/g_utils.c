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

// g_utils.c -- misc utility functions for game module

#include "g_local.h"
#include "bg_saga.h"
#include "qcommon/q_shared.h"
#include "g_nav.h"
#include "../../galaxyrp/game/rp_lua.h"	// GLX_U: [Lua] entity frees and uses reach scripts

int remapCount = 0;

// GalaxyRP fix: [Shader Remap] the one definition of the remap table. It used to be a per-file
// tentative definition in rp_local.h, merged by the Windows linker and simply absent on Linux --
// see the comment on the declaration there. It belongs here, beside the count and the three
// functions that own the table.
shaderRemap_t remappedShaders[MAX_SHADER_REMAPS];

int zyk_get_remap_count()
{
	return remapCount;
}

// GalaxyRP fix: [security] is this string usable as a shader name?
//
// Two separate reasons, and both of them bite:
//
// Length. remappedShaders[] holds char[MAX_QPATH] (rp_local.h), so anything longer cannot be
// stored. AddRemap() below bounds the copy now, but a name that only fits after truncation is not
// the name anybody meant, so it is refused rather than silently shortened.
//
// Content. BuildShaderStateConfig() joins entries as "%s=%s:%5.2f@" and the client takes them
// apart with strstr on those same three characters (CG_ShaderStateChanged, cg_servercmds.c), so a
// name containing '=', ':' or '@' splits in the wrong place on every machine that parses it. And
// /remapsave writes the table as whitespace-separated tokens, which fscanf reads back the same
// way, so a name containing a space or a newline comes back as two records and shifts every
// record after it -- the whole file silently changes meaning. Q3's tokenizer keeps quoted
// arguments intact (Cmd_TokenizeString, cmd.cpp), so /remap "foo bar" baz really does reach us
// with a space in it.
//
// Rejecting those six characters is what lets /remapsave's format stay as simple as it is: no
// token can contain a separator, so nothing needs escaping the way the entity files do.
qboolean zyk_valid_shader_name( const char *name )
{
	int i;

	if ( !name || !name[0] )
	{
		return qfalse;
	}

	if ( strlen(name) >= MAX_QPATH )
	{
		return qfalse;
	}

	for ( i = 0; name[i] != '\0'; i++ )
	{
		if ( name[i] <= ' ' )
		{ // space, tab, newline, carriage return and every control character
			return qfalse;
		}

		if ( name[i] == '=' || name[i] == ':' || name[i] == '@' )
		{ // the separators BuildShaderStateConfig() and the client's parser rely on
			return qfalse;
		}
	}

	return qtrue;
}

/*
GalaxyRP fix: [security] keep player-written text from reaching a client as a StringEd reference.

Every "print" and "cp" a client receives goes through CG_CheckSVStringEdRef() (cg_servercmds.c),
which treats three '@' in a row as "insert the server string named by the next word here". That
expansion is how the server's own phrases arrive (G_GetStringEdString() returns "@@@KEY"), but it
also applies to anything a player typed, and the stock client copies the rest of the message after
an expansion with no bound: a line full of "@@@NOCHEATS " overflows its 1024-byte stack buffer and
takes down every client that receives it. Our own cgame is bounded now, but players on the stock
client are not, so the text has to be made harmless before it is sent.

The rule is the one ClientCleanName() (g_client.c) already applies to player names: never three '@'
in a row. The third and every later '@' of a run are dropped; one or two stay as they are, so an
e-mail address or "@@" in a description still reads the same. Only literal runs count -- the client
compares the characters themselves, so "@^1@@" is not a reference and is left alone.

This is for text people write -- descriptions, item names, news, messages, entity keys shown in a
listing. Never run it on a whole line the server built from its own phrases: those are references
on purpose. Works in place; returns the resulting length.
*/
int RP_BreakStringEdRefs( char *text )
{
	char *in = NULL;
	char *out = NULL;
	int ats = 0;

	if ( !text )
	{
		return 0;
	}

	for ( in = out = text; *in; in++ )
	{
		if ( *in == '@' )
		{
			if ( ++ats > 2 )
			{
				continue;
			}
		}
		else
		{
			ats = 0;
		}

		*out++ = *in;
	}

	*out = '\0';

	return (int)( out - text );
}

/*
GalaxyRP fix: [security] whether text holds what a client would expand as a StringEd reference --
see RP_BreakStringEdRefs() above. Used where the text cannot simply be cleaned because it is also
executed (a vote string) and so is refused instead.
*/
qboolean RP_HasStringEdRef( const char *text )
{
	return ( text && strstr( text, "@@@" ) ) ? qtrue : qfalse;
}

/*
GalaxyRP fix: [security] RP_BreakStringEdRefs() on a copy, for the places that show text they must
not change: an admin listing shows an entity's keys, but the stored value is what the entity, its
file and /entsave keep. Text without a reference comes back as the same pointer. A cleaned copy
lives in one of eight rotating buffers of its own -- not va()'s, which has only four and would be
overwritten when several shown values go into one va() line -- and is cut at MAX_STRING_CHARS,
past which no single print could carry it anyway.
*/
const char *RP_ShownText( const char *text )
{
	static char copies[8][MAX_STRING_CHARS];
	static int next = 0;
	char *copy = NULL;

	if ( !text )
	{
		return "";
	}

	if ( !RP_HasStringEdRef( text ) )
	{
		return text;
	}

	copy = copies[next++ & 7];
	Q_strncpyz( copy, text, MAX_STRING_CHARS );
	RP_BreakStringEdRefs( copy );

	return copy;
}

void AddRemap(const char *oldShader, const char *newShader, float timeOffset) {
	int i;

	// GalaxyRP fix: [security] validate here, not only at the callers.
	//
	// There are four ways into this function and only two of them are commands. /remap checks its
	// arguments itself so it can say why it refused; zyk_load_remap_file() checks each record so it
	// can skip one bad line instead of the file. The other two arrive from map data:
	// zyk_remap_quest_item() (g_main.c) and G_UseTargets2() (below), and that second one is the
	// reason this check is here rather than only up there.
	//
	// G_UseTargets2() passes ent->targetShaderName and ent->targetShaderNewName straight through.
	// Those are F_STRING spawn fields (g_spawn.c), which G_NewString() allocates at whatever length
	// the value happens to be -- no cap anywhere -- and the remap fires ABOVE that function's
	// "if (!string || !string[0]) return", so the entity does not even need a target key. Any of
	// the sixty-odd G_UseTargets() call sites reaches it, including Touch_Item(): an entity with a
	// long targetshadername is triggered by any player walking over it, not by the admin who
	// placed it. That made an ordinary pickup a buffer overflow.
	if ( zyk_valid_shader_name(oldShader) == qfalse || zyk_valid_shader_name(newShader) == qfalse )
	{
		trap->Print("AddRemap: refused a shader remap -- a name is empty, longer than %i characters, or contains whitespace or one of = : @\n", MAX_QPATH - 1);
		return;
	}

	for (i = 0; i < remapCount; i++) {
		if (Q_stricmp(oldShader, remappedShaders[i].oldShader) == 0) {
			// found it, just update this one
			// GalaxyRP fix: [security] bounded. This was a bare strcpy() into char[MAX_QPATH], as
			// were the two below -- the "remapCount < MAX_SHADER_REMAPS" test bounds the index, and
			// nothing bounded the length. The validation above already refuses anything too long,
			// so this is the guarantee rather than the gate: it holds even if a fifth caller
			// appears that forgets to check.
			Q_strncpyz(remappedShaders[i].newShader, newShader, sizeof(remappedShaders[i].newShader));
			remappedShaders[i].timeOffset = timeOffset;
			return;
		}
	}
	if (remapCount < MAX_SHADER_REMAPS) {
		Q_strncpyz(remappedShaders[remapCount].newShader, newShader, sizeof(remappedShaders[remapCount].newShader));
		Q_strncpyz(remappedShaders[remapCount].oldShader, oldShader, sizeof(remappedShaders[remapCount].oldShader));
		remappedShaders[remapCount].timeOffset = timeOffset;
		remapCount++;
	}
}

// GalaxyRP fix: [Shader Remap] clear every remap and put the shaders back on the clients.
//
// Returns how many entries were cleared, so the caller can word its own message.
//
// Nothing could take a remap back before this: /remapdeletefile removes the preset FILE, which does
// not touch the running map, and the table only ever grew until the map changed. The renderer is
// what makes the undo possible -- R_RemapShader() (tr_shader.cpp) ends with
//
//     if (sh != sh2) { sh->remappedShader = sh2; } else { sh->remappedShader = NULL; }
//
// so a shader remapped TO ITSELF has its remap cleared. That is stock renderer code, so it works on
// a vanilla client and a TaystJK one alike; nothing client-side has to know this command exists.
//
// Two passes, and the order is the whole trick:
//
// First point every entry at its own shader, with a zero time offset, and send that. Each client
// runs R_RemapShader(S, S, "0") for every entry and restores it. The zero matters: that function
// ends with "sh2->timeOffset = atof(timeOffset)", and on an identity remap sh2 IS the shader being
// restored, so sending the old offset would leave its animation phase shifted.
//
// Only then empty the table and send again, so anyone connecting later gets no remaps either.
// Doing it the other way round would send an empty configstring first and the identity entries
// would never reach anyone -- the remaps would stay on screen until the map changed. Two sends
// rather than one is free here: SV_SetConfigstring (sv_init.cpp) transmits on each call rather than
// deferring to the snapshot, and it is two either way whether the table holds three entries or 128.
//
// What it cannot undo: shader remaps built into the map itself, the worldspawn "remapshader" and
// "vertexremapshader" keys that tr_bsp.cpp applies client-side at load. Those never passed through
// this table, so there is no record of them here.
int zyk_clear_all_remaps( void )
{
	int i;
	int cleared = remapCount;

	if ( cleared == 0 )
	{
		return 0;
	}

	for ( i = 0; i < remapCount; i++ )
	{
		Q_strncpyz(remappedShaders[i].newShader, remappedShaders[i].oldShader, sizeof(remappedShaders[i].newShader));
		remappedShaders[i].timeOffset = 0.0f;
	}

	trap->SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());

	remapCount = 0;

	trap->SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());

	return cleared;
}

// GalaxyRP fix: [security] the one reader for a remap preset file.
//
// There were two copies of this loop -- Cmd_RemapLoad_f() (g_cmds.c) and the default-preset load in
// G_InitGame() (g_main.c) -- and they had already drifted apart: the command's copy was given
// field widths and short-record handling at some point, and its twin was left with bare "%s"
// conversions into char[128] and two unchecked follow-up reads. So the file an admin loads by hand
// was safe and the identical file loaded automatically at map start was not, which is the more
// dangerous of the two because nobody types anything to trigger it. One copy now.
//
// Returns qtrue if the file existed and was read, qfalse if it could not be opened, so each caller
// can word its own "not found" message.
qboolean zyk_load_remap_file( const char *file_path )
{
	// Deliberately wider than MAX_QPATH: a token that does not fit is a record to REJECT, and to
	// reject one it has to be read whole first. Narrowing the conversion to the destination instead
	// would split an over-long token in two and shift every record after it -- the corruption this
	// is here to prevent.
	char old_shader[128] = {0};
	char new_shader[128] = {0};
	char time_offset[128] = {0};
	int skipped = 0;
	FILE *remap_file = fopen(file_path, "r");

	if ( !remap_file )
	{
		return qfalse;
	}

	while ( fscanf(remap_file, "%127s", old_shader) == 1 )
	{
		if ( fscanf(remap_file, "%127s", new_shader) != 1 )
		{ // a record that stops halfway ends the read -- it used to re-use the previous
		  // iteration's buffers and register a remap built out of them
			break;
		}

		if ( fscanf(remap_file, "%127s", time_offset) != 1 )
		{
			break;
		}

		if ( zyk_valid_shader_name(old_shader) == qfalse || zyk_valid_shader_name(new_shader) == qfalse )
		{
			skipped++;
			continue;
		}

		// GalaxyRP fix: [cleanup] no G_NewString() here any more. AddRemap() copies into its own
		// fixed buffers, so allocating a level-lifetime copy of each name bought nothing -- and
		// G_NewString() turns a literal backslash-n in the file into a real linefeed, which would
		// have put a newline inside a shader name and through into the configstring.
		AddRemap(old_shader, new_shader, atof(time_offset));
	}

	fclose(remap_file);

	if ( skipped > 0 )
	{
		trap->Print("%s: skipped %i unusable remap record(s).\n", file_path, skipped);
	}

	trap->SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());

	return qtrue;
}

const char *BuildShaderStateConfig(void) {
	static char	buff[MAX_STRING_CHARS*4];
	// GalaxyRP fix: [Shader Remap] this was (MAX_QPATH * 2) + 5, which cannot hold a maximal entry.
	// The format below needs two names of up to MAX_QPATH-1, three separators and whatever %5.2f
	// makes of timeOffset -- and timeOffset is level.time * 0.001, so it grows all session: five
	// characters at the start of a map, six after ten minutes, more after that. Two 62-character
	// names already overflowed 133 bytes, and Com_sprintf() truncates (it is Q_vsnprintf plus a
	// console warning), which drops the trailing '@'.
	//
	// That '@' is load-bearing. CG_ShaderStateChanged() finds each field with strstr, so a record
	// without one makes the client read the NEXT record's text as this record's time offset and
	// then resume after that record's '@' -- one remap silently lost and one applied with a
	// nonsense offset, on every machine parsing the configstring. 32 covers a thirty-character
	// numeric field, which %5.2f cannot reach in any plausible uptime.
	char out[(MAX_QPATH * 2) + 32];
	int i;
	int dropped = 0;
	// Reported only when the number changes, so a map that is permanently over budget says so once
	// rather than on every remap trigger. Deliberately not reset anywhere: the module is unloaded
	// and reloaded on a map change (SV_UnbindGame -> VM_Free), which reinitialises it along with
	// remapCount itself.
	static int lastReported = -1;

	// GalaxyRP fix: [Shader Remap] sizeof(buff), not MAX_STRING_CHARS. buff is MAX_STRING_CHARS*4,
	// so this cleared the first quarter of it. Harmless as the code stands -- Q_strcat() appends at
	// strlen(buff) and buff[0] was zeroed, so the stale tail always sat past the terminator -- but
	// it is not what the line says it does, and it becomes real the moment anything writes to this
	// buffer by index instead of by strlen.
	memset(buff, 0, sizeof(buff));
	for (i = 0; i < remapCount; i++) {
		Com_sprintf(out, sizeof(out), "%s=%s:%5.2f@", remappedShaders[i].oldShader, remappedShaders[i].newShader, remappedShaders[i].timeOffset);

		// GalaxyRP fix: [Shader Remap] say when a remap does not reach the clients.
		//
		// MAX_SHADER_REMAPS is 128 and this configstring is 4096 bytes, which holds between about
		// 30 and 83 entries depending on how long the shader paths are -- so the table can always
		// hold more than the configstring can carry. Q_strcat() refuses an entry that does not fit
		// whole (both of its Com_Error calls are commented out upstream, so it simply returns), and
		// the result was that remaps past the limit vanished with nothing said anywhere, while
		// /remaplist still listed them.
		//
		// Deliberately NOT fixed by enlarging buff. 128 maximal entries is roughly 17KB and
		// MAX_GAMESTATE_CHARS is 16000 for every configstring combined; a full CS_SHADERSTATE is
		// already a quarter of that budget, and the overflow check is on the CLIENT
		// (cl_parse.cpp -- Com_Error(ERR_DROP, "MAX_GAMESTATE_CHARS exceeded")). A bigger buffer
		// would turn "some remaps do not arrive" into "players cannot connect to this map". So the
		// behaviour is unchanged and only the silence is fixed.
		if ( (int)strlen(buff) + (int)strlen(out) + 1 > (int)sizeof(buff) )
		{
			dropped++;
			continue;
		}

		Q_strcat( buff, sizeof( buff ), out);
	}

	if ( dropped != lastReported )
	{
		if ( dropped > 0 )
		{
			trap->Print("BuildShaderStateConfig: %i of %i remap(s) did not fit the shader state configstring and were not sent to clients.\n", dropped, remapCount);
		}

		lastReported = dropped;
	}

	return buff;
}

/*
=========================================================================

model / sound configstring indexes

=========================================================================
*/

/*
=====================================================================
GalaxyRP: [Saber RGB] Parse a packed 24-bit saber colour.

The value arrives either from a client's cp_sbRGB1/cp_sbRGB2 userinfo cvar (freely settable by the
player, so entirely untrusted) or from the character's database row, and ends up driving a render
tint on every other player's machine -- so it is masked down to the three channels it is allowed to
occupy here rather than anywhere further downstream. Returns 0 for "no custom colour", which is
also what an absent, empty, malformed or negative value degrades to.

Note that 0 is not merely the "unset" marker but also indistinguishable from a literal, deliberately
requested (0,0,0). That ambiguity is fine now: a client-facing packed RGB of 0 falls back to plain
red on the render side (CG_NewClientInfo, matching TaystJK's own legacy-compatibility behaviour), so
"unset" and "asked for pure black" end up rendering the same reasonable way either side of this
function. Anyone who actually wants a true black blade has the dedicated SABER_BLACK palette entry
(/sabercolor <n> black) instead, which carries no RGB payload at all and isn't affected by this.
=====================================================================
*/
int G_ParseSaberRGB( const char *str ) {
	int packed, i;

	if ( !str || !str[0] )
		return 0;

	// The largest value this can legitimately be is SABERRGB_MASK, eight digits. Reject anything
	// longer (or not a plain decimal number) up front rather than handing it to atoi(), whose
	// behaviour on a value too large for an int is undefined -- and this string comes straight off
	// the wire from a client that is free to put anything it likes in it.
	for ( i = 0; str[i]; i++ ) {
		if ( str[i] < '0' || str[i] > '9' || i >= 8 )
			return 0;
	}

	packed = atoi( str );
	if ( packed <= 0 )
		return 0;

	return packed & SABERRGB_MASK;
}

/*
=====================================================================
GalaxyRP fix: [Saber RGB] validates a saberColorMode decoded from a stored (database) value -- see
the comment on this function's prototype in g_local.h for why SABER_STORED_MODE()'s raw 4-bit decode
isn't enough on its own. Anything inside 0..NUM_SABER_COLORS-1 is a real saber_colors_t and passes
through unchanged; anything else (reachable only via a hand-edited or corrupted database row, not
through any current in-game write path) falls back to SABER_RED, same as this column's existing
pre-RGB legacy-default behaviour.
=====================================================================
*/
int G_ValidateSaberColorMode( int decodedMode ) {
	if ( decodedMode < 0 || decodedMode >= NUM_SABER_COLORS )
		return SABER_RED;

	return decodedMode;
}

/*
=====================================================================
GLX_U: [Cosmetics] the hats and capes that can be saved with a character, by id (index + 1). They
are TaystJK's own (assets/japro/models/cosmetics), shipped in our client pk3 at the same paths.
The order is FROZEN: an id is what the database holds, so a new item goes at the end of its list
and an item is never removed or moved, or saved characters would come back wearing another one.
At most RP_STORED_COSMETIC_MASK (255) per list.
=====================================================================
*/
static const char *rpCosmeticHats[] = {
	"afro", "beard", "bucket", "cap", "cringe", "crown", "fedora", "fedora2", "fedora3", "fedora4",
	"glasses", "gradcap", "headcrab", "horns", "mario", "mask", "metalhelm", "plaguemask",
	"predatorhelm", "pumpkin", "santahat", "sombrero", "supersaiyan", "tophat"
};
static const char *rpCosmeticCapes[] = {
	"ak47", "crowbar", "goose", "grogucape", "royalcape", "rpg", "vadercape", "yodacape"
};

static const char **G_RpCosmeticList( int slot, int *count ) {
	if ( slot == RP_COSMETIC_HAT ) {
		*count = (int)ARRAY_LEN( rpCosmeticHats );
		return rpCosmeticHats;
	}
	if ( slot == RP_COSMETIC_CAPE ) {
		*count = (int)ARRAY_LEN( rpCosmeticCapes );
		return rpCosmeticCapes;
	}
	*count = 0;
	return NULL;
}

int G_RpCosmeticId( int slot, const char *name ) {
	int count, i;
	const char **list = G_RpCosmeticList( slot, &count );

	if ( !name || !name[0] )
		return 0;

	for ( i = 0; i < count; i++ ) {
		if ( !Q_stricmp( list[i], name ) )
			return i + 1;
	}
	return 0;
}

const char *G_RpCosmeticName( int slot, int id ) {
	int count;
	const char **list = G_RpCosmeticList( slot, &count );

	if ( id < 1 || id > count )
		return "";
	return list[id - 1];
}

void G_RpCosmeticFromColor( const char *colorValue, char *out, int outSize ) {
	const char *s = colorValue;
	int len, i;

	if ( !out || outSize < 1 )
		return;
	out[0] = '\0';

	if ( !s )
		return;

	// the saber number first, as TaystJK strips it: its digits only
	while ( *s >= '0' && *s <= '9' )
		s++;

	len = (int)strlen( s );
	if ( len < 1 || len >= RP_COSMETIC_NAME_SIZE || len >= outSize )
		return;
	if ( !( ( s[0] >= 'a' && s[0] <= 'z' ) || ( s[0] >= 'A' && s[0] <= 'Z' ) || s[0] == '_' ) )
		return;
	for ( i = 0; i < len; i++ ) {
		const char c = s[i];
		if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '-' ) )
			return;
	}
	if ( !Q_stricmp( s, "none" ) )
		return;

	Q_strncpyz( out, s, outSize );
}

/*
=====================================================================
Cleans the given string from newlines and such
=====================================================================
*/
void RPMod_StringEscape(char *in, char *out, int outSize)
{
	char	ch, ch1;
	int len = 0;
	outSize--;

	while (1)
	{
		ch = *in++;
		ch1 = *in;
		if (ch == '\\' && ch1 == 'n') {
			in++;
			*out++ = '\n';
		} else {
			*out++ = ch;
		}
		if (len > outSize - 1) break;
		len++;
	}
	return;
}

// GalaxyRP fix: [Configstrings] total bytes the gamestate currently holds, counted exactly the way
// a client counts them when it parses one: strlen + 1 for every configstring that is not empty (see
// CL_ParseGamestate in cl_parse.cpp and CL_ConfigstringModified in cl_cgame.cpp -- both of them
// Com_Error(ERR_DROP, "MAX_GAMESTATE_CHARS exceeded") the moment the running total passes 16000).
//
// This is the limit nobody guards. SV_SetConfigstring only range-checks the index; there is no
// aggregate check anywhere on the server, so the game module can keep registering names until the
// total passes 16000 -- at which point every connected client drops and no new client can ever
// finish connecting. The per-table limits are not what you hit first: MAX_MODELS alone is 512
// slots of up to MAX_QPATH each, which is twice the entire byte budget.
//
// The scan is a syscall per configstring. The caller below runs it before every name it is about to
// register for the first time -- about 44 microseconds each, and only on a create, never on the
// lookup of a name already in the table.
//
// GalaxyRP fix: [Configstrings] read the long ones at their real length. SV_GetConfigstring copies
// with Q_strncpyz and says nothing when it has to cut the string short, so a MAX_STRING_CHARS
// buffer silently counted any configstring over 1023 bytes as 1023 -- and configstrings that long
// are normal, not an error: SV_SendConfigstring splits anything over that into bcs0/bcs1/bcs2
// chunks precisely so the engine can send them. Two on this server can exceed it:
//
//   CS_SYSTEMINFO   built by Cvar_InfoString_Big into a BIG_INFO_STRING buffer, so up to 8191
//                   bytes. sv_paks and sv_pakNames carry one entry per LOADED pk3 and
//                   sv_referencedPaks/sv_referencedPakNames one per referenced pk3, so a pure
//                   server with a map pack puts several kilobytes in this one string.
//   CS_SHADERSTATE  built by BuildShaderStateConfig() into MAX_STRING_CHARS*4, so up to 4095.
//
// (CS_SERVERINFO is built in a MAX_INFO_STRING buffer, so it stops one byte short of the problem.)
//
// The client counts their true length, and under-counting here fails in the dangerous direction:
// the budget below would believe it had kilobytes of room it does not have, allow the registration
// that carries the real total past 16000, and drop every connected client -- the exact failure this
// whole file exists to prevent, and the same shape as the fixed foreign-byte allowance removed from
// G_FindConfigstringIndex().
//
// Reading everything into a 16000-byte buffer would fix it and cost fifteen times as much, because
// Q_strncpyz is strncpy, which pads the whole destination on every one of the 1700 reads. So read
// cheaply first and re-read only the ones that come back filling the small buffer exactly -- the
// only ones that can have been cut short. In practice that is two strings out of 1700, so the scan
// costs what it always did.
static int zyk_gamestate_bytes_used( void ) {
	// zyk: static rather than automatic -- 16000 bytes is too much to put on the stack of something
	// reached from every spawn path, and the game module is single-threaded, so one buffer is safe
	static char	long_s[MAX_GAMESTATE_CHARS];
	int		i, len, total = 0;
	char	s[MAX_STRING_CHARS];

	for ( i = 0; i < MAX_CONFIGSTRINGS; i++ ) {
		trap->GetConfigstring( i, s, sizeof( s ) );

		if ( !s[0] ) {
			continue;
		}

		len = (int)strlen( s );

		if ( len == (int)sizeof( s ) - 1 ) {
			// zyk: it filled the buffer exactly, so it is either that long or longer and was cut.
			// Ask again with room for the whole gamestate: nothing can legitimately be longer than
			// that, and anything that somehow were would read back long enough to refuse on, which
			// is the safe answer
			trap->GetConfigstring( i, long_s, sizeof( long_s ) );
			len = (int)strlen( long_s );
		}

		total += len + 1;
	}

	return total;
}

// GalaxyRP fix: [Configstrings] which "this table is full" flag a given table start owns, so the
// refusal below is logged once per table rather than once per refused name -- a map that trips this
// would otherwise write the same line thousands of times. Anything unrecognised shares the last
// slot; being coarse there only means two odd tables would share one log line.
static int zyk_cs_table_slot( int start ) {
	switch ( start ) {
	case CS_MODELS:			return 0;
	case CS_SOUNDS:			return 1;
	case CS_ICONS:			return 2;
	case CS_EFFECTS:		return 3;
	case CS_BSP_MODELS:		return 4;
	case CS_AMBIENT_SET:	return 5;
	case CS_G2BONES:		return 6;
	default:				return ZYK_CS_TABLES - 1;
	}
}

// GalaxyRP fix: [Configstrings] recount from scratch. Called once at the end of G_InitGame, so the
// running estimate starts from the truth rather than from zero on a map that loads a lot of content.
void G_ResetGamestateEstimate( void ) {
	level.zyk_gamestate_bytes = zyk_gamestate_bytes_used();
}

// GalaxyRP fix: [Configstrings] for a caller that needs SEVERAL configstrings and cannot use half of
// them. G_FindConfigstringIndex() refuses one name at a time, which is right for the callers that
// want one; a caller claiming a block has to know before it starts, or it ends up holding slots it
// cannot use and has no way to give back. /admweather is the one that needs this.
qboolean G_ConfigstringBytesAvailable( int needed ) {
	if ( needed < 0 ) {
		needed = 0;
	}

	return ((zyk_gamestate_bytes_used() + needed) <= ZYK_GAMESTATE_BUDGET) ? qtrue : qfalse;
}

/*
=================
Slot reuse: RP_SlotTable / RP_SlotMarkUsed / RP_SlotReclaim / RP_SlotRoomFor

GalaxyRP: [Slot Reuse] a model or effect name keeps its configstring slot until the map changes. The
effect table has 64 slots -- shared by the map's effects, every NPC and vehicle type the first time
one spawns, and /admweather's block -- and the models share the gamestate's ~15000 bytes with the
players' info, so a map that stays up for days runs out, and what asked next was drawn with nothing.

A slot whose name only Entity System props ever asked for (level.rp_slot_es_model / _fx is 1:
RP_EntityModelIndex() and RP_EntityEffectIndex() at a prop's own model and effect) can be given to
a new name once nothing points at it any more -- its props were removed -- but only when the new name
cannot be added the ordinary way: the table is full, or the gamestate has no room for another name.
Whatever takes it over decides what it is from then on: an Entity System prop keeps it reusable, any
other caller (an NPC, a vehicle, the map) makes it permanent. Any other request for a name makes its
slot permanent, since the caller may keep the index.

The slot is overwritten in place, never emptied: a client joining later registers the table only up
to the first empty slot (CG_RegisterSounds, CG_RegisterGraphics), so a gap would hide everything after
it. Clients already connected register the new name when the configstring changes. Names starting
with "*" (weather commands, player-model sounds) never take a reused slot: /admweather claims its
block by appending and checks that it is contiguous.

The number of reuses a map allows is capped (RP_SLOT_REUSE_FX, RP_SLOT_REUSE_MODELS in g_local.h):
each client keeps everything it has registered until the map changes, and has limits of its own.

Sound slots work the same way for the names only /playsound asked for (level.rp_slot_ps_sound), with
one difference: /playsound holds at most RP_PLAYSOUND_POOL of them at once, so past that a new name
takes the one played least recently even while the table has room -- see RP_PlaySoundIndex(). A
"*" sound name (a player-model sound) is never written into a reused slot: the client does not
register those, so it would go on playing the handle of the sound the slot held before
(CG_ConfigStringModified, EV_GENERAL_SOUND in cg_event.c).
=================
*/
#define RP_SLOT_REUSES_CVAR "rp_slot_reuses"
#define RP_PLAYSOUND_SLOTS_CVAR "rp_playsound_slots"

/*
=================
RP_SlotRestore

The reuses a map has spent are kept in the cvar RP_SLOT_REUSES_CVAR ("<effects> <models> <sounds>") as well as
in level: map_restart reloads the game module and clears level, while every client keeps what it
registered, so a restart must not give the map a fresh budget. A new map starts from 0. The owner
markers are not kept -- after a restart every slot is permanent, which is the safe side.
=================
*/
void RP_SlotRestore( int restart ) {
	char buf[64];
	const char *space;

	if ( !restart ) {
		trap->Cvar_Set( RP_SLOT_REUSES_CVAR, "0 0 0" );
		trap->Cvar_Set( RP_PLAYSOUND_SLOTS_CVAR, "" );
		return;
	}

	trap->Cvar_VariableStringBuffer( RP_SLOT_REUSES_CVAR, buf, sizeof( buf ) );
	level.rp_slot_reuses_fx = Com_Clampi( 0, RP_SLOT_REUSE_FX, atoi( buf ) );
	space = strchr( buf, ' ' );
	level.rp_slot_reuses_model = Com_Clampi( 0, RP_SLOT_REUSE_MODELS, space ? atoi( space + 1 ) : 0 );
	space = space ? strchr( space + 1, ' ' ) : NULL;
	level.rp_slot_reuses_sound = Com_Clampi( 0, RP_SLOT_REUSE_SOUNDS, space ? atoi( space + 1 ) : 0 );

	// the /playsound slots too: a restart (which any player can vote for) would otherwise make each of
	// them the map's for good and let /playsound take RP_PLAYSOUND_POOL more. Anything the reloaded
	// game asks for by name still becomes permanent, as it registers after this.
	{
		char list[MAX_CVAR_VALUE_STRING];
		const char *p = list;

		trap->Cvar_VariableStringBuffer( RP_PLAYSOUND_SLOTS_CVAR, list, sizeof( list ) );
		while ( *p ) {
			int k = atoi( p );

			if ( k > 0 && k < MAX_SOUNDS )
				level.rp_slot_ps_sound[k] = 1;
			while ( *p && *p != ' ' )
				p++;
			while ( *p == ' ' )
				p++;
		}
	}
}

static void RP_SlotSaveReuses( void ) {
	trap->Cvar_Set( RP_SLOT_REUSES_CVAR, va( "%d %d %d", level.rp_slot_reuses_fx, level.rp_slot_reuses_model, level.rp_slot_reuses_sound ) );
}

// the /playsound slots, as "<slot> <slot> ...", for RP_SlotRestore() -- at most RP_PLAYSOUND_POOL of
// them unless the table has room past it, 3 digits and a space each
static void RP_SlotSaveSoundPool( void ) {
	char list[MAX_CVAR_VALUE_STRING];
	int k;

	list[0] = '\0';
	for ( k = 1; k < MAX_SOUNDS; k++ ) {
		if ( level.rp_slot_ps_sound[k] == 1 )
			Q_strcat( list, sizeof( list ), va( "%d ", k ) );
	}
	trap->Cvar_Set( RP_PLAYSOUND_SLOTS_CVAR, list );
}

// the owner marker of a slot, saving the /playsound slots whenever one of them comes or goes
static void RP_SlotSetOwner( byte *owners, int which, int k, int value ) {
	if ( owners[k] == value )
		return;
	owners[k] = (byte)value;
	if ( which == 2 )
		RP_SlotSaveSoundPool();
}

typedef struct {
	int		start;
	int		max;
	byte	*owners;
	int		*reuses;
	int		limit;
	int		which;			// 0 models, 1 effects, 2 sounds
	const char *what;
} rpSlotTable_t;

static qboolean RP_SlotTable( int start, rpSlotTable_t *t ) {
	if ( start == CS_MODELS ) {
		t->start = CS_MODELS; t->max = MAX_MODELS; t->owners = level.rp_slot_es_model;
		t->reuses = &level.rp_slot_reuses_model; t->limit = RP_SLOT_REUSE_MODELS; t->which = 0; t->what = "model";
		return qtrue;
	}
	if ( start == CS_EFFECTS ) {
		t->start = CS_EFFECTS; t->max = MAX_FX; t->owners = level.rp_slot_es_fx;
		t->reuses = &level.rp_slot_reuses_fx; t->limit = RP_SLOT_REUSE_FX; t->which = 1; t->what = "effect";
		return qtrue;
	}
	if ( start == CS_SOUNDS ) {
		t->start = CS_SOUNDS; t->max = MAX_SOUNDS; t->owners = level.rp_slot_ps_sound;
		t->reuses = &level.rp_slot_reuses_sound; t->limit = RP_SLOT_REUSE_SOUNDS; t->which = 2; t->what = "sound";
		return qtrue;
	}
	return qfalse;
}

// which slots of the table something in the game still points at. Deliberately generous -- a number
// that only might be this table's index still keeps its slot -- except where a field is known to mean
// something else: a brush entity's modelindex is its inline model, an fx_runner's modelindex2 its
// state, and anything but an fx_runner's modelindex is not an effect.
static void RP_SlotMarkUsed( const rpSlotTable_t *t, byte *used ) {
	gentity_t *e;

#define RP_SLOT_MARK(v) do { int v_ = (v); if ( v_ > 0 && v_ < t->max ) used[v_] = 1; } while ( 0 )

	memset( used, 0, t->max );

	RP_FOR_EACH_ENTITY( e ) {
		if ( !e->inuse )
			continue;

		if ( t->which == 2 ) {
			// a sound event still on its way to the clients (G_Sound and the like): the only way a name
			// only /playsound asked for is ever pointed at. A player's own events are left out: whatever
			// they carry came from a lookup by name (which makes the slot permanent), and their last
			// values stay behind until the player's next events, so they would hold a slot for nothing.
			if ( e->s.eType >= ET_EVENTS || ( e->s.event && !e->client ) )
				RP_SLOT_MARK( e->s.eventParm );
			RP_SLOT_MARK( e->s.loopSound );
		} else if ( t->which == 1 ) {
			if ( e->s.eType == ET_FX )
				RP_SLOT_MARK( e->s.modelindex );
			// an effect event still on its way to the clients (G_PlayEffectID and the like); not a
			// player's own (see the sounds above)
			if ( e->s.eType >= ET_EVENTS || ( e->s.event && !e->client ) )
				RP_SLOT_MARK( e->s.eventParm );
		} else {
			if ( e->s.eType != ET_FX ) {
				if ( !e->r.bmodel )
					RP_SLOT_MARK( e->s.modelindex );
				RP_SLOT_MARK( e->s.modelindex2 );
			}
			// a misc_model_breakable keeps its main and its use model here (misc_model_use)
			if ( e->classname && !Q_stricmp( e->classname, "misc_model_breakable" ) ) {
				RP_SLOT_MARK( e->sound1to2 );
				RP_SLOT_MARK( e->sound2to1 );
			}
		}
	}

#undef RP_SLOT_MARK
}

// the reusable slots of the table, with the length (terminator included) of the name each holds; 0
// for the others. Returns how many there are.
static int RP_SlotCandidates( const rpSlotTable_t *t, int *candLen, int *firstFree ) {
	static byte used[MAX_MODELS];
	char s[MAX_STRING_CHARS];
	int k, n = 0;

	RP_SlotMarkUsed( t, used );

	for ( k = 0; k < t->max; k++ )
		candLen[k] = 0;

	for ( k = 1; k < t->max; k++ ) {
		trap->GetConfigstring( t->start + k, s, sizeof( s ) );
		if ( !s[0] )
			break;
		// an effect named "*" is a weather command (see the top), and models keep them as they were; a
		// sound one (a player sound) may be written over, the client registering the new name
		if ( t->owners[k] != 1 || ( t->which != 2 && s[0] == '*' ) || used[k] )
			continue;
		candLen[k] = (int)strlen( s ) + 1;
		n++;
	}

	if ( firstFree )
		*firstFree = k;
	return n;
}

// /playsound replaces the sound played least recently; the others the longest name
static const int *RP_SlotLru( const rpSlotTable_t *t ) {
	return ( t->which == 2 && level.rp_slot_es_context ) ? level.rp_slot_sound_played : NULL;
}

// the candidate a new name of length len (terminator included) should take, with bytes in use now:
// the one holding the longest name, which leaves the gamestate the most room -- or, given lastUsed,
// the one used least recently. 0 if none will do.
static int RP_SlotPick( const rpSlotTable_t *t, const int *candLen, int len, int bytes, const int *lastUsed ) {
	int k, best = 0;

	for ( k = 1; k < t->max; k++ ) {
		if ( !candLen[k] )
			continue;
		if ( bytes - candLen[k] + len > ZYK_GAMESTATE_BUDGET )
			continue;
		if ( !best || ( lastUsed ? lastUsed[k] < lastUsed[best] : candLen[k] > candLen[best] ) )
			best = k;
	}
	return best;
}

// give name (length len with its terminator; bytes in use now) a slot of a removed Entity System
// prop (or a sound only /playsound played), if there is one and the map's reuses are not spent; the
// least recently used one given lastUsed. Returns the slot, or 0.
static int RP_SlotReclaim( const rpSlotTable_t *t, const char *name, int len, int bytes, const int *lastUsed ) {
	static int candLen[MAX_MODELS];
	char old[MAX_STRING_CHARS];
	int best;

	if ( !VALIDSTRING( name ) || name[0] == '*' )
		return 0;

	if ( *t->reuses >= t->limit ) {
		if ( !level.rp_slot_reuse_spent_warned[t->which] ) {
			level.rp_slot_reuse_spent_warned[t->which] = qtrue;
			G_LogPrintf( "%s slots: this map's %d reuses are spent; \"%s\" gets no slot.\n", t->what, t->limit, name );
		}
		return 0;
	}

	// nothing to look for when no slot is the props' alone -- spares a full table the entity scan on
	// every name it refuses
	for ( best = 1; best < t->max && t->owners[best] != 1; best++ )
		;
	if ( best >= t->max )
		return 0;

	if ( !RP_SlotCandidates( t, candLen, NULL ) )
		return 0;

	best = RP_SlotPick( t, candLen, len, bytes, lastUsed );
	if ( !best )
		return 0;

	trap->GetConfigstring( t->start + best, old, sizeof( old ) );
	trap->SetConfigstring( t->start + best, name );
	level.zyk_gamestate_bytes = bytes - candLen[best] + len;
	RP_SlotSetOwner( t->owners, t->which, best, level.rp_slot_es_context ? 1 : 0 );
	(*t->reuses)++;
	RP_SlotSaveReuses();
	if ( t->start == CS_MODELS )
		level.rp_model_info_state[best] = 0;	// RP_ModelInfo() reads the new file

	G_LogPrintf( "%s slot %d reused: \"%s\" (no longer in use) -> \"%s\" (%d of %d reuses this map)\n",
		t->what, best, old, name, *t->reuses, t->limit );
	return best;
}

/*
=================
RP_SlotRoomFor

Whether names[0..count) -- what one Entity System prop is about to register in the table at start,
in the order its spawn function registers them -- will all get a slot: already there, a free slot the
gamestate has room for, or a reusable one. otherBytes is what the spawn function registers elsewhere
along the way (a sound), counted as spent from the start. Walks the same choices
G_FindConfigstringIndex() and RP_SlotReclaim() will make, on copies. When not, reason says why.
=================
*/
qboolean RP_SlotRoomFor( int start, const char **names, int count, int otherBytes, char *reason, int reasonSize ) {
	static int candLen[MAX_MODELS];
	static byte replaced[MAX_MODELS];	// slots this walk has given to an earlier name: theirs is gone
	rpSlotTable_t t;
	char s[MAX_STRING_CHARS];
	int i, j, k, firstFree, bytes, reusesLeft;

	if ( reason && reasonSize > 0 )
		reason[0] = '\0';
	if ( !RP_SlotTable( start, &t ) )
		return qtrue;

	RP_SlotCandidates( &t, candLen, &firstFree );
	memset( replaced, 0, t.max );
	bytes = zyk_gamestate_bytes_used() + ( otherBytes > 0 ? otherBytes : 0 );
	reusesLeft = t.limit - *t.reuses;

	for ( i = 0; i < count; i++ ) {
		const char *name = names[i];
		qboolean known = qfalse;
		int len, pick;

		if ( !VALIDSTRING( name ) )
			continue;

		for ( j = 0; j < i && !known; j++ ) {
			if ( VALIDSTRING( names[j] ) && !strcmp( names[j], name ) )
				known = qtrue;
		}
		for ( k = 1; k < firstFree && !known; k++ ) {
			if ( replaced[k] )
				continue;
			trap->GetConfigstring( t.start + k, s, sizeof( s ) );
			if ( !strcmp( s, name ) ) {
				known = qtrue;
				candLen[k] = 0;	// the prop will point at it: no later name of its own can take it
			}
		}
		if ( known )
			continue;

		len = (int)strlen( name ) + 1;

		if ( firstFree < t.max && bytes + len <= ZYK_GAMESTATE_BUDGET ) {
			firstFree++;
			bytes += len;
			continue;
		}

		pick = ( name[0] != '*' && reusesLeft > 0 ) ? RP_SlotPick( &t, candLen, len, bytes, NULL ) : 0;
		if ( pick ) {
			bytes = bytes - candLen[pick] + len;
			candLen[pick] = 0;
			replaced[pick] = 1;
			reusesLeft--;
			continue;
		}

		if ( reason && reasonSize > 0 ) {
			if ( firstFree >= t.max ) {
				Com_sprintf( reason, reasonSize, "the map has no room for another %s: all %d %s slots are in use%s",
					t.what, t.max - 1, t.what, reusesLeft > 0 ? " and none can be reused" : va( ", and its %d reuses are spent", t.limit ) );
			} else {
				Com_sprintf( reason, reasonSize, "the map's gamestate is full (%d of %d bytes) and no %s slot can be reused%s",
					bytes, ZYK_GAMESTATE_BUDGET, t.what, reusesLeft > 0 ? "" : va( " (its %d reuses are spent)", t.limit ) );
			}
		}
		return qfalse;
	}

	return qtrue;
}

/*
=================
RP_SlotStats / G_GamestateBytesUsed

For /entslots: the slots in use (the table's first empty slot), how many only Entity System props
asked for, how many of those nothing points at now, and the reuses spent.
=================
*/
void RP_SlotStats( int start, int *used, int *slots, int *esOnly, int *reusableNow, int *reuses, int *reuseLimit ) {
	static int candLen[MAX_MODELS];
	rpSlotTable_t t;
	int k, firstFree, n = 0;

	if ( !RP_SlotTable( start, &t ) )
		return;

	*reusableNow = RP_SlotCandidates( &t, candLen, &firstFree );
	for ( k = 1; k < firstFree; k++ ) {
		if ( t.owners[k] == 1 )
			n++;
	}
	*used = firstFree - 1;
	*slots = t.max - 1;
	*esOnly = n;
	*reuses = *t.reuses;
	*reuseLimit = t.limit;
}

int G_GamestateBytesUsed( void ) {
	return zyk_gamestate_bytes_used();
}

/*
================
G_FindConfigstringIndex

================
*/
static int G_FindConfigstringIndex( const char *name, int start, int max, qboolean create ) {
	int		i, len;
	char	s[MAX_STRING_CHARS];
	rpSlotTable_t slotTable;
	qboolean tracked = RP_SlotTable( start, &slotTable );

	if ( !VALIDSTRING( name ) ) {
		return 0;
	}

	for ( i=1 ; i<max ; i++ ) {
		trap->GetConfigstring( start + i, s, sizeof( s ) );
		if ( !s[0] ) {
			break;
		}
		if ( !strcmp( s, name ) ) {
			// GalaxyRP: [Slot Reuse] anything but an Entity System prop asking for it may keep the
			// index, so the slot can never be given away again
			if ( tracked && !level.rp_slot_es_context ) {
				RP_SlotSetOwner( slotTable.owners, slotTable.which, i, 0 );
			}
			return i;
		}
	}

	if ( !create ) {
		return 0;
	}

	// GalaxyRP fix: [Configstrings] this was trap->Error(ERR_DROP, "G_FindConfigstringIndex:
	// overflow") -- the whole server dropped, every player disconnected at once, the instant a
	// table filled up and was asked for one more name it had not seen.
	//
	// That is a fine assertion for the game code's own developer-authored references, which are a
	// fixed set. It is not fine for anything a player or admin can drive, and plenty can: every
	// SP_* function that does G_ModelIndex(ent->model) takes its name straight from a spawn key,
	// and "model" is a key /entadd will set to whatever it is given. 512 /entadd calls with
	// distinct model names used to be a one-line way for an admin to take the server process down
	// (see ZYK_ENTITY_RESERVE in g_local.h on what ERR_DROP does on a dedicated build).
	//
	// Refusing instead returns 0, which is the index of the empty configstring: the client draws
	// no model, plays no sound, shows no effect for that entity. Degraded, but alive, and every
	// caller already copes with it -- G_SoundIndexSafe() has returned 0 on a full sound table
	// since the /playsound fix, and index 0 is what an entity with no model carries anyway.
	if ( i == max ) {
		int slot = zyk_cs_table_slot( start );

		// GalaxyRP: [Slot Reuse] a slot a removed Entity System prop no longer needs
		if ( tracked ) {
			int reused = RP_SlotReclaim( &slotTable, name, (int)strlen( name ) + 1, zyk_gamestate_bytes_used(), RP_SlotLru( &slotTable ) );

			if ( reused ) {
				return reused;
			}
		}

		if ( !level.zyk_configstring_table_full[slot] ) {
			level.zyk_configstring_table_full[slot] = qtrue;
			G_LogPrintf( "configstring table at %d is full (%d slots); refusing \"%s\" and any "
				"further new names for it. Entities asking for one will render without it.\n",
				start, max, name );
		}
		return 0;
	}

	// GalaxyRP fix: [Configstrings] ...and the table filling up is not even the first wall. The
	// gamestate has a 16000-byte ceiling that every client enforces and no part of the server does,
	// so a map plus a few hundred long /entadd model paths can push past it while every table still
	// has free slots. At MAX_QPATH names, the model table ALONE is twice the whole byte budget;
	// measured across every indexed table, the byte ceiling binds at about 20% of slot capacity.
	//
	// The count is taken fresh every time a new name is about to be registered, and the decision is
	// made against that measurement and nothing else.
	//
	// An earlier version of this tried to avoid the count by tracking only the bytes this function
	// had registered and assuming a fixed ceiling for everything else -- serverinfo, systeminfo, the
	// CS_PLAYERS entry per client. That assumption is not sound: a CS_PLAYERS entry is built in a
	// MAX_INFO_STRING buffer, so 32 clients alone can reach 32768 bytes, twice the entire gamestate.
	// When the assumption broke it broke in the dangerous direction -- it would skip the count and
	// allow the registration that pushed the gamestate past the limit, which is the exact failure
	// this exists to prevent. Measured cost of counting every time: about 44 microseconds, roughly
	// 22ms spread across a whole map load. That is a very cheap price for a decision that is
	// always made on a real number.
	len = (int)strlen( name ) + 1;
	level.zyk_gamestate_bytes = zyk_gamestate_bytes_used();

	if ( (level.zyk_gamestate_bytes + len) > ZYK_GAMESTATE_BUDGET ) {
		// GalaxyRP: [Slot Reuse] a slot a removed Entity System prop no longer needs, whose name makes
		// room for this one
		if ( tracked ) {
			int reused = RP_SlotReclaim( &slotTable, name, len, level.zyk_gamestate_bytes, RP_SlotLru( &slotTable ) );

			if ( reused ) {
				return reused;
			}
		}

		if ( !level.zyk_gamestate_full ) {
			level.zyk_gamestate_full = qtrue;
			G_LogPrintf( "gamestate is at %d of %d bytes; refusing \"%s\" and any further new "
				"configstring names. Registering it would drop every connected client.\n",
				level.zyk_gamestate_bytes, ZYK_GAMESTATE_BUDGET, name );
		}
		return 0;
	}

	trap->SetConfigstring( start + i, name );
	level.zyk_gamestate_bytes += len;

	if ( tracked ) {
		RP_SlotSetOwner( slotTable.owners, slotTable.which, i, level.rp_slot_es_context ? 1 : 0 );
	}

	return i;
}

/*
Ghoul2 Insert Start
*/

int G_BoneIndex( const char *name ) {
	return G_FindConfigstringIndex (name, CS_G2BONES, MAX_G2BONES, qtrue);
}
/*
Ghoul2 Insert End
*/

/*
=================
RP_FileExists / RP_ModelInfo / RP_PackConstantLight

GalaxyRP: [Entity System] whether the server's file system holds this file (the pk3s included) -- the
Entity System refuses a model or effect it does not have, rather than registering a name no client
can load into one of the map's 512 model or 64 effect slots and a slice of the gamestate, until the
map changes (see RP_EntitySystemSpawnRefused() in g_spawn.c).

RP_ModelInfo(): the frame count and the frame-0 bounds of the md3 behind a model index, read from the
file once per map and kept in level.rp_model_*. The header is checked before any of it is believed --
the ident and version, a frame count and frame offset that fit in the file, bounds that are numbers of
a sane size -- so a truncated, foreign or hostile file just reads as "unknown" and every caller keeps
the behaviour it had without it. qfalse then, and for a path that is not an .md3.

RP_PackConstantLight(): the "light" radius and "color" (0..1 each) packed into entityState_t's
constantLight the way InitMover() does for movers: red, green, blue and radius/4 in the four bytes.
=================
*/
qboolean RP_FileExists( const char *path )
{
	fileHandle_t f = 0;
	int len;

	if ( !VALIDSTRING( path ) )
		return qfalse;

	len = trap->FS_Open( path, &f, FS_READ );
	if ( f )
		trap->FS_Close( f );

	return ( f && len > 0 ) ? qtrue : qfalse;
}

typedef struct {
	int		ident;
	int		version;
	char	name[MAX_QPATH];
	int		flags;
	int		numFrames;
	int		numTags;
	int		numSurfaces;
	int		numSkins;
	int		ofsFrames;
	int		ofsTags;
	int		ofsSurfaces;
	int		ofsEnd;
} rpMd3Header_t;	// md3Header_t (qcommon/qfiles.h), which the game module does not include

typedef struct {
	vec3_t	bounds[2];
	vec3_t	localOrigin;
	float	radius;
	char	name[16];
} rpMd3Frame_t;		// md3Frame_t

#define RP_MD3_IDENT		(('3'<<24)+('P'<<16)+('D'<<8)+'I')
#define RP_MD3_VERSION		15
#define RP_MD3_MAX_FRAMES	1024
#define RP_MD3_MAX_BOUND	65536.0f

static qboolean RP_ReadModelInfo( const char *path, int *frames, vec3_t mins, vec3_t maxs )
{
	fileHandle_t f = 0;
	rpMd3Header_t header;
	rpMd3Frame_t frame;
	int len, skip, k;
	qboolean ok = qfalse;

	if ( !VALIDSTRING( path ) || strlen( path ) < 5 || Q_stricmp( path + strlen( path ) - 4, ".md3" ) != 0 )
		return qfalse;

	len = trap->FS_Open( path, &f, FS_READ );
	if ( !f )
		return qfalse;

	if ( len < (int)sizeof( header ) + (int)sizeof( frame ) )
		goto done;

	trap->FS_Read( &header, sizeof( header ), f );
	header.ident = LittleLong( header.ident );
	header.version = LittleLong( header.version );
	header.numFrames = LittleLong( header.numFrames );
	header.ofsFrames = LittleLong( header.ofsFrames );

	if ( header.ident != RP_MD3_IDENT || header.version != RP_MD3_VERSION )
		goto done;
	if ( header.numFrames < 1 || header.numFrames > RP_MD3_MAX_FRAMES )
		goto done;
	if ( header.ofsFrames < (int)sizeof( header ) || header.ofsFrames > len - header.numFrames * (int)sizeof( frame ) )
		goto done;

	// zyk: there is no seek, so read our way to the frames (they follow the header in every md3 the
	// tools write, so this is normally nothing)
	for ( skip = header.ofsFrames - (int)sizeof( header ); skip > 0; )
	{
		char scratch[256];
		int n = skip > (int)sizeof( scratch ) ? (int)sizeof( scratch ) : skip;

		trap->FS_Read( scratch, n, f );
		skip -= n;
	}

	trap->FS_Read( &frame, sizeof( frame ), f );

	for ( k = 0; k < 3; k++ )
	{
		float lo = LittleFloat( frame.bounds[0][k] );
		float hi = LittleFloat( frame.bounds[1][k] );

		// zyk: the negated comparisons also refuse a NaN
		if ( !( lo > -RP_MD3_MAX_BOUND && lo < RP_MD3_MAX_BOUND ) || !( hi > -RP_MD3_MAX_BOUND && hi < RP_MD3_MAX_BOUND ) || !( lo <= hi ) )
			goto done;

		mins[k] = lo;
		maxs[k] = hi;
	}

	*frames = header.numFrames;
	ok = qtrue;

done:
	trap->FS_Close( f );
	return ok;
}

// DAJ_RP: [Static Models] an md3's frame-0 bounds by its path, for a model no index stands for (a
// misc_model_static's, which the server never registers -- see g_statics.c); not kept, the caller keeps it
qboolean RP_ReadModelBounds( const char *path, vec3_t mins, vec3_t maxs )
{
	int frames = 0;

	return RP_ReadModelInfo( path, &frames, mins, maxs );
}

qboolean RP_ModelInfo( int modelIndex, const char *path, int *frames, vec3_t mins, vec3_t maxs )
{
	if ( modelIndex <= 0 || modelIndex >= MAX_MODELS )
		return qfalse;

	if ( level.rp_model_info_state[modelIndex] == 0 )
	{
		int n = 0;
		vec3_t lo, hi;

		if ( RP_ReadModelInfo( path, &n, lo, hi ) )
		{
			level.rp_model_info_state[modelIndex] = 1;
			level.rp_model_frames[modelIndex] = (short)n;
			VectorCopy( lo, level.rp_model_mins[modelIndex] );
			VectorCopy( hi, level.rp_model_maxs[modelIndex] );
		}
		else
		{
			level.rp_model_info_state[modelIndex] = -1;
		}
	}

	if ( level.rp_model_info_state[modelIndex] != 1 )
		return qfalse;

	if ( frames )
		*frames = level.rp_model_frames[modelIndex];
	if ( mins )
		VectorCopy( level.rp_model_mins[modelIndex], mins );
	if ( maxs )
		VectorCopy( level.rp_model_maxs[modelIndex], maxs );

	return qtrue;
}

int RP_PackConstantLight( float light, const vec3_t color )
{
	int c[3], i, k;

	// zyk: the negated tests also turn a NaN into 0 rather than into whatever converting it does
	for ( k = 0; k < 3; k++ )
	{
		if ( !( color[k] > 0.0f ) )
			c[k] = 0;
		else if ( color[k] >= 1.0f )
			c[k] = 255;
		else
			c[k] = (int)( color[k] * 255 );
	}

	if ( !( light > 0.0f ) )
		i = 0;
	else if ( light >= 1020.0f )
		i = 255;
	else
		i = (int)( light / 4 );

	return c[0] | ( c[1] << 8 ) | ( c[2] << 16 ) | ( i << 24 );
}

int G_ModelIndex( const char *name ) {
#ifdef _DEBUG_MODEL_PATH_ON_SERVER
	//debug to see if we are shoving data into configstrings for models that don't exist, and if
	//so, where we are doing it from -rww
	fileHandle_t fh;

	trap->FS_Open(name, &fh, FS_READ);
	if (!fh)
	{ //try models/ then, this is assumed for registering models
		trap->FS_Open(va("models/%s", name), &fh, FS_READ);
		if (!fh)
		{
			Com_Printf("ERROR: Server tried to modelindex %s but it doesn't exist.\n", name);
		}
	}

	if (fh)
	{
		trap->FS_Close(fh);
	}
#endif
	return G_FindConfigstringIndex (name, CS_MODELS, MAX_MODELS, qtrue);
}

int	G_IconIndex( const char* name )
{
	assert(name && name[0]);
	return G_FindConfigstringIndex (name, CS_ICONS, MAX_ICONS, qtrue);
}

int G_SoundIndex( const char *name ) {
	assert(name && name[0]);
	return G_FindConfigstringIndex (name, CS_SOUNDS, MAX_SOUNDS, qtrue);
}

// GalaxyRP fix: [security] G_SoundIndex() (above) is built on G_FindConfigstringIndex(), which calls
// trap->Error(ERR_DROP, ...) -- a full server crash -- the instant its backing table (here, the
// MAX_SOUNDS slots behind CS_SOUNDS) is already full and asked to register one more name it hasn't seen
// before. That's a reasonable "this should never happen" safety net for the game code's own internal,
// developer-authored sound references, but /playsound (Cmd_ZykSound_f in g_cmds.c) hands this a raw
// player-typed string with no cap on how many distinct ones get requested -- any connected player could
// crash the whole server with a couple hundred rapid /playsound calls using unique nonsense paths.
// This variant runs the identical scan/reuse logic but returns 0 instead of erroring out when the table
// is full and the name isn't already registered, so a player-facing caller can print a friendly message
// and refuse the request instead of taking the whole server down.
//
// GalaxyRP: [Listings] now simply G_FindConfigstringIndex(), which has since learned to return 0 on a
// full table too -- and which also refuses a name that would take the gamestate past the 16000 bytes
// every client enforces. This copy never checked that: a couple of hundred long /playsound names fit
// in the 256 slots and dropped every connected client.
int G_SoundIndexSafe( const char *name ) {
	return G_FindConfigstringIndex( name, CS_SOUNDS, MAX_SOUNDS, qtrue );
}

/*
=================
RP_PlaySoundIndex

GalaxyRP: [Slot Reuse] the sound slot for a /playsound name, or 0 with the reason in reason.

A name already in the table plays at no cost: the map's own, or one of /playsound's (the played time is
noted). A new name goes into the table marked as /playsound's while /playsound holds fewer than
RP_PLAYSOUND_POOL slots; after that it takes the /playsound slot played least recently that no sound
still on its way to the clients uses, overwriting it in place. That is a reuse, counted against
RP_SLOT_REUSE_SOUNDS for the map. A "*" name only ever goes into a free slot (see the top).

So /playsound never holds more than RP_PLAYSOUND_POOL of the map's slots, nor more than that many names
of gamestate, and the rest stay for the map and the game -- which may in turn take an idle /playsound
slot when the table or the gamestate is full (G_FindConfigstringIndex -> RP_SlotReclaim).
=================
*/
int RP_PlaySoundIndex( const char *name, char *reason, int reasonSize ) {
	static int candLen[MAX_MODELS];
	rpSlotTable_t t;
	char s[MAX_STRING_CHARS];
	int k, firstFree, held = 0, index;

	if ( reason && reasonSize > 0 )
		reason[0] = '\0';
	if ( !VALIDSTRING( name ) || !RP_SlotTable( CS_SOUNDS, &t ) )
		return 0;

	for ( k = 1; k < t.max; k++ ) {
		trap->GetConfigstring( t.start + k, s, sizeof( s ) );
		if ( !s[0] )
			break;
		if ( !strcmp( s, name ) ) {
			if ( t.owners[k] == 1 )
				level.rp_slot_sound_played[k] = level.time;
			return k;
		}
		if ( t.owners[k] == 1 )
			held++;
	}
	firstFree = k;

	if ( held < RP_PLAYSOUND_POOL ) {
		level.rp_slot_es_context = qtrue;
		index = G_FindConfigstringIndex( name, CS_SOUNDS, MAX_SOUNDS, qtrue );
		level.rp_slot_es_context = qfalse;

		if ( index ) {
			level.rp_slot_sound_played[index] = level.time;
			return index;
		}
	}
	else if ( name[0] != '*' ) {
		level.rp_slot_es_context = qtrue;	// stays /playsound's
		index = RP_SlotReclaim( &t, name, (int)strlen( name ) + 1, zyk_gamestate_bytes_used(), level.rp_slot_sound_played );
		level.rp_slot_es_context = qfalse;

		if ( index ) {
			level.rp_slot_sound_played[index] = level.time;
			return index;
		}
	}

	if ( reason && reasonSize > 0 ) {
		qboolean spent = ( *t.reuses >= t.limit ) ? qtrue : qfalse;

		if ( name[0] == '*' ) {
			if ( held >= RP_PLAYSOUND_POOL )
				Com_sprintf( reason, reasonSize, "a player sound needs one of /playsound's %d slots for itself, and all of them hold sounds already played", RP_PLAYSOUND_POOL );
			else
				Com_sprintf( reason, reasonSize, "a player sound needs a free slot, and the map's %s", firstFree >= t.max ? "sound table is full" : "gamestate is full" );
		}
		else if ( spent && ( held >= RP_PLAYSOUND_POOL || RP_SlotCandidates( &t, candLen, NULL ) ) )
			Com_sprintf( reason, reasonSize, "this map has used all its %d sound slot reuses; the sounds already played still work", t.limit );
		else if ( held >= RP_PLAYSOUND_POOL ) {
			if ( RP_SlotCandidates( &t, candLen, NULL ) )
				Com_sprintf( reason, reasonSize, "the map's gamestate has no room for a name this long" );
			else
				Com_sprintf( reason, reasonSize, "every /playsound sound is still playing; try again in a moment" );
		}
		else if ( firstFree >= t.max )
			Com_sprintf( reason, reasonSize, "the map's sound table is full" );
		else
			Com_sprintf( reason, reasonSize, "the map's gamestate is full" );
	}
	return 0;
}

int G_SoundSetIndex(const char *name)
{
	return G_FindConfigstringIndex (name, CS_AMBIENT_SET, MAX_AMBIENT_SETS, qtrue);
}

/*
=================
RP_WeatherSafeName

DAJ_RP: [Weather] a spacedust command whose density the renderer would take as it is, and crash on. The
renderer reads the number after "spacedust" with atoi() and allocates that many particles with no bound of
its own, and a negative count reaches a new[] of an enormous size -- every connected player crashes, and so
does every player joining later, since the command lives in a configstring. /admweather and the Entity
System's weather entities clamp their own (1 to 10000); this catches the rest -- a map's own fx_spacedust
with a negative count, a map's own zyk_weather or fx_runner with "spacedust -5" -- and holds the number to
0 to 10000. 0 stays 0: no particles, which is what such a map shows today. Read the way the renderer reads
it (the first word, then atoi() of the second) and rebuilt only when the number is outside that range;
any other name is returned as it is.
=================
*/
static const char *RP_WeatherSafeName( const char *name, char *buf, int bufSize )
{
	const char *p;
	int value = 0;
	qboolean negative = qfalse;

	if ( !VALIDSTRING( name ) || name[0] != '*' )
		return name;

	p = name + 1;
	while ( *p && (unsigned char)*p <= ' ' )
		p++;
	if ( Q_stricmpn( p, "spacedust", 9 ) != 0 || ( p[9] && (unsigned char)p[9] > ' ' ) )
		return name;

	p += 9;
	while ( *p && (unsigned char)*p <= ' ' )
		p++;
	if ( *p == '-' || *p == '+' )
	{
		negative = ( *p == '-' ) ? qtrue : qfalse;
		p++;
	}
	while ( *p >= '0' && *p <= '9' )
	{
		if ( value <= ZYK_WEATHER_DUST_MAX )
			value = ( value * 10 ) + ( *p - '0' );
		p++;
	}
	if ( negative )
		value = -value;

	if ( value >= 0 && value <= ZYK_WEATHER_DUST_MAX )
		return name;

	Com_sprintf( buf, bufSize, "*spacedust %d", Com_Clampi( 0, ZYK_WEATHER_DUST_MAX, value ) );
	G_LogPrintf( "weather command \"%s\" holds a density the game cannot draw; sent as \"%s\"\n", name, buf );
	return buf;
}

int G_EffectIndex( const char *name )
{
	char safe[ZYK_WEATHER_CMD_LENGTH];

	name = RP_WeatherSafeName( name, safe, sizeof( safe ) );

	// GalaxyRP fix: [Weather] a "*"-prefixed effect is not an effect at all -- it is a command for
	// the renderer's world-effect parser, which is a command stream shared by everyone on the
	// server. /admweather owns that stream once it has reserved its block, and the whole design
	// rests on the block being the HIGHEST weather slots in use: a client joining later replays the
	// table in slot order, so anything registered above the block is read AFTER the block's
	// teardown and survives it, while a client already connected only re-runs slots that changed
	// and loses it on the next rebuild. One new weather command after the claim is therefore enough
	// to leave the two halves of the server looking at permanently different skies.
	//
	// So refuse it. The callers are the misc weather entities (SP_CreateWind, SP_CreateSnow,
	// SP_CreateRain, SP_CreateSpaceDust, SP_CreateWeather), none of which reads this return value,
	// and at map load they all run long before any claim -- their weather is captured as the map's
	// own. This only bites an entity spawned afterwards, by /entadd or a script, which today is not
	// "weather that works" but "weather that splits the server". /admweather add does the same job
	// and keeps everyone on one sky.
	//
	// DAJ_RP: [Weather] the weather entities refuse themselves before asking now, with the reason
	// (RP_WeatherRefusedAtRuntime, g_misc.c), and read the index they get back (RP_WeatherAdd); this
	// stays for anything else that registers a weather command.
	//
	// The block's own reservation is not caught by this: it runs while zyk_weather_slot is still 0.
	//
	// This refuses even a command already in the table, rather than handing back the index it would
	// have found. That is deliberate and costs nothing: a command registered before the claim was
	// captured as the map's own weather and is already part of every rebuild, so the entity asking
	// for it again is asking for something it already has -- and no caller here reads the index.
	if ( VALIDSTRING( name ) && name[0] == '*' && level.zyk_weather_slot != 0 )
	{
		if ( !level.zyk_weather_late_effect_warned )
		{
			level.zyk_weather_late_effect_warned = qtrue;
			G_LogPrintf( "refused the weather command \"%s\": /admweather is managing the weather "
				"on this map, and an effect registered after its block would show a different sky "
				"to players who join later. Use /admweather add instead.\n", name );
		}

		return 0;
	}

	return G_FindConfigstringIndex (name, CS_EFFECTS, MAX_FX, qtrue);
}

/*
=================
RP_EntityModelIndex / RP_EntityEffectIndex

GalaxyRP: [Slot Reuse] G_ModelIndex() / G_EffectIndex() for a prop's own model or effect: for one the
Entity System made (RP_EntitySystemMade()), a name it registers first stays reusable once the props
using it are gone -- see RP_SlotReclaim() above. Any other entity is the plain call.
=================
*/
int RP_EntityModelIndex( gentity_t *ent, const char *name ) {
	int index;

	if ( !RP_EntitySystemMade( ent ) ) {
		return G_ModelIndex( name );
	}

	level.rp_slot_es_context = qtrue;
	index = G_ModelIndex( name );
	level.rp_slot_es_context = qfalse;
	return index;
}

int RP_EntityEffectIndex( gentity_t *ent, const char *name ) {
	int index;

	if ( !RP_EntitySystemMade( ent ) ) {
		return G_EffectIndex( name );
	}

	level.rp_slot_es_context = qtrue;
	index = G_EffectIndex( name );
	level.rp_slot_es_context = qfalse;
	return index;
}

int G_BSPIndex( const char *name )
{
	return G_FindConfigstringIndex (name, CS_BSP_MODELS, MAX_SUB_BSP, qtrue);
}

//=====================================================================


//see if we can or should allow this guy to use a custom skeleton -rww
qboolean G_PlayerHasCustomSkeleton(gentity_t *ent)
{
	/*
	siegeClass_t *scl;

	if (level.gametype != GT_SIEGE)
	{ //only in siege
		return qfalse;
	}

	if (ent->s.number >= MAX_CLIENTS ||
		!ent->client ||
		ent->client->siegeClass == -1)
	{ //invalid class
		return qfalse;
	}

	scl = &bgSiegeClasses[ent->client->siegeClass];
	if (!(scl->classflags & (1<<CFL_CUSTOMSKEL)))
	{ //class is not flagged for this
		return qfalse;
	}

	return qtrue;
	*/
	return qfalse;
}

/*
================
G_TeamCommand

Broadcasts a command to only a specific team
================
*/
void G_TeamCommand( team_t team, char *cmd ) {
	int		i;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		if ( level.clients[i].pers.connected == CON_CONNECTED ) {
			if ( level.clients[i].sess.sessionTeam == team ) {
				trap->SendServerCommand( i, va("%s", cmd ));
			}
		}
	}
}


/*
=============
G_Find

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the entity after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
gentity_t *G_Find (gentity_t *from, int fieldofs, const char *match)
{
	char	*s;

	// GalaxyRP: [Logical Entities] the networked region first, then the logical one, so every
	// caller -- G_UseTargets, G_PickTarget, spawn-point selection, the quest code -- finds a
	// target_relay or an info_player_deathmatch wherever it was allocated. Resuming from a
	// logical entity continues in the logical region; resuming from the last networked one
	// crosses over. RP_NextEntityInAnyRegion() encodes that walk.
	for ( from = RP_NextEntityInAnyRegion( from ); from; from = RP_NextEntityInAnyRegion( from ) )
	{
		if (!from->inuse)
			continue;
		s = *(char **) ((byte *)from + fieldofs);
		if (!s)
			continue;
		if (!Q_stricmp (s, match))
			return from;
	}

	return NULL;
}



/*
============
G_RadiusList - given an origin and a radius, return all entities that are in use that are within the list
============
*/
int G_RadiusList ( vec3_t origin, float radius,	gentity_t *ignore, qboolean takeDamage, gentity_t *ent_list[MAX_GENTITIES])
{
	float		dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	int			i, e;
	int			ent_count = 0;

	if ( radius < 1 )
	{
		radius = 1;
	}

	for ( i = 0 ; i < 3 ; i++ )
	{
		mins[i] = origin[i] - radius;
		maxs[i] = origin[i] + radius;
	}

	numListedEntities = trap->EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for ( e = 0 ; e < numListedEntities ; e++ )
	{
		ent = &g_entities[entityList[ e ]];

		if ((ent == ignore) || !(ent->inuse) || ent->takedamage != takeDamage)
			continue;

		// find the distance from the edge of the bounding box
		for ( i = 0 ; i < 3 ; i++ )
		{
			if ( origin[i] < ent->r.absmin[i] )
			{
				v[i] = ent->r.absmin[i] - origin[i];
			} else if ( origin[i] > ent->r.absmax[i] )
			{
				v[i] = origin[i] - ent->r.absmax[i];
			} else
			{
				v[i] = 0;
			}
		}

		dist = VectorLength( v );
		if ( dist >= radius )
		{
			continue;
		}

		// ok, we are within the radius, add us to the incoming list
		ent_list[ent_count] = ent;
		ent_count++;

	}
	// we are done, return how many we found
	return(ent_count);
}


//----------------------------------------------------------
void G_Throw( gentity_t *targ, vec3_t newDir, float push )
//----------------------------------------------------------
{
	vec3_t	kvel;
	float	mass;

	if ( targ->physicsBounce > 0 )	//overide the mass
	{
		mass = targ->physicsBounce;
	}
	else
	{
		mass = 200;
	}

	if ( g_gravity.value > 0 )
	{
		VectorScale( newDir, g_knockback.value * (float)push / mass * 0.8, kvel );
		kvel[2] = newDir[2] * g_knockback.value * (float)push / mass * 1.5;
	}
	else
	{
		VectorScale( newDir, g_knockback.value * (float)push / mass, kvel );
	}

	if ( targ->client )
	{
		VectorAdd( targ->client->ps.velocity, kvel, targ->client->ps.velocity );
	}
	else if ( targ->s.pos.trType != TR_STATIONARY && targ->s.pos.trType != TR_LINEAR_STOP && targ->s.pos.trType != TR_NONLINEAR_STOP )
	{
		VectorAdd( targ->s.pos.trDelta, kvel, targ->s.pos.trDelta );
		VectorCopy( targ->r.currentOrigin, targ->s.pos.trBase );
		targ->s.pos.trTime = level.time;
	}

	// set the timer so that the other client can't cancel
	// out the movement immediately
	if ( targ->client && !targ->client->ps.pm_time )
	{
		int		t;

		t = push * 2;

		if ( t < 50 )
		{
			t = 50;
		}
		if ( t > 200 )
		{
			t = 200;
		}
		targ->client->ps.pm_time = t;
		targ->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
	}
}

//methods of creating/freeing "fake" dynamically allocated client entity structures.
//You ABSOLUTELY MUST free this client after creating it before game shutdown. If
//it is left around you will have a memory leak, because true dynamic memory is
//allocated by the exe.
void G_FreeFakeClient(gclient_t **cl)
{ //or not, the dynamic stuff is busted somehow at the moment. Yet it still works in the test.
  //I think something is messed up in being able to cast the memory to stuff to modify it,
  //while modifying it directly seems to work fine.
	//trap->TrueFree((void **)cl);
}

//allocate a veh object
#define MAX_VEHICLES_AT_A_TIME		512//128
static Vehicle_t g_vehiclePool[MAX_VEHICLES_AT_A_TIME];
static qboolean g_vehiclePoolOccupied[MAX_VEHICLES_AT_A_TIME];
static qboolean g_vehiclePoolInit = qfalse;
void G_AllocateVehicleObject(Vehicle_t **pVeh)
{
	int i = 0;

	if (!g_vehiclePoolInit)
	{
		g_vehiclePoolInit = qtrue;
		memset(g_vehiclePoolOccupied, 0, sizeof(g_vehiclePoolOccupied));
	}

	while (i < MAX_VEHICLES_AT_A_TIME)
	{ //iterate through and try to find a free one
		if (!g_vehiclePoolOccupied[i])
		{
			g_vehiclePoolOccupied[i] = qtrue;
			memset(&g_vehiclePool[i], 0, sizeof(Vehicle_t));
			*pVeh = &g_vehiclePool[i];
			return;
		}
		i++;
	}
	Com_Error(ERR_DROP, "Ran out of vehicle pool slots.");
}

//free the pointer, sort of a lame method
void G_FreeVehicleObject(Vehicle_t *pVeh)
{
	int i = 0;
	while (i < MAX_VEHICLES_AT_A_TIME)
	{
		if (g_vehiclePoolOccupied[i] &&
			&g_vehiclePool[i] == pVeh)
		{ //guess this is it
			g_vehiclePoolOccupied[i] = qfalse;
			break;
		}
		i++;
	}
}

gclient_t *gClPtrs[MAX_GENTITIES];

void G_CreateFakeClient(int entNum, gclient_t **cl)
{
	//trap->TrueMalloc((void **)cl, sizeof(gclient_t));
	if (!gClPtrs[entNum])
	{
		gClPtrs[entNum] = (gclient_t *) BG_Alloc(sizeof(gclient_t));
	}
	*cl = gClPtrs[entNum];
}

//call this on game shutdown to run through and get rid of all the lingering client pointers.
void G_CleanAllFakeClients(void)
{
	int i = MAX_CLIENTS; //start off here since all ents below have real client structs.
	gentity_t *ent;

	while (i < MAX_GENTITIES)
	{
		ent = &g_entities[i];

		if (ent->inuse && ent->s.eType == ET_NPC && ent->client)
		{
			G_FreeFakeClient(&ent->client);
		}
		i++;
	}
}

/*
=============
G_SetAnim

Finally reworked PM_SetAnim to allow non-pmove calls, so we take our
local anim index into account and make the call -rww
=============
*/
void BG_SetAnim(playerState_t *ps, animation_t *animations, int setAnimParts,int anim,int setAnimFlags, int blendTime);

void G_SetAnim(gentity_t *ent, usercmd_t *ucmd, int setAnimParts, int anim, int setAnimFlags, int blendTime)
{
#if 0 //old hackish way
	pmove_t pmv;

	assert(ent && ent->inuse && ent->client);

	memset (&pmv, 0, sizeof(pmv));
	pmv.ps = &ent->client->ps;
	pmv.animations = bgAllAnims[ent->localAnimIndex].anims;
	if (!ucmd)
	{
		pmv.cmd = ent->client->pers.cmd;
	}
	else
	{
		pmv.cmd = *ucmd;
	}
	pmv.trace = trap->Trace;
	pmv.pointcontents = trap->PointContents;
	pmv.gametype = level.gametype;

	//don't need to bother with ghoul2 stuff, it's not even used in PM_SetAnim.
	pm = &pmv;
	PM_SetAnim(setAnimParts, anim, setAnimFlags, blendTime);
#else //new clean and shining way!
	assert(ent->client);
    BG_SetAnim(&ent->client->ps, bgAllAnims[ent->localAnimIndex].anims, setAnimParts, anim, setAnimFlags, blendTime);
#endif
}


/*
=============
G_PickTarget

Selects a random entity from among the targets
=============
*/
#define MAXCHOICES	32

gentity_t *G_PickTarget (char *targetname)
{
	gentity_t	*ent = NULL;
	int		num_choices = 0;
	gentity_t	*choice[MAXCHOICES];

	if (!targetname)
	{
		trap->Print("G_PickTarget called with NULL targetname\n");
		return NULL;
	}

	while(1)
	{
		ent = G_Find (ent, FOFS(targetname), targetname);
		if (!ent)
			break;
		choice[num_choices++] = ent;
		if (num_choices == MAXCHOICES)
			break;
	}

	if (!num_choices)
	{
		trap->Print("G_PickTarget: target %s not found\n", targetname);
		return NULL;
	}

	return choice[rand() % num_choices];
}

/*
==============================
GlobalUse

GalaxyRP fix: [Entity System] every entity that uses another -- G_UseTargets2() for all target_*,
trigger_*, func_* and path_corner firing, target_relay's random pick, ICARUS "use", RP_LiftCallUse --
reaches the next one's use function through here, so this is the one place that can see a chain of
uses nesting. Nothing bounded it. G_UseTargets2() only skips an entity that targets itself, so two
target_relays naming each other (three /entadd lines and a trigger) recursed until the stack
overflowed and the server process died, and a random target_relay could pick itself even past that
check. Worse, a loop that FANS OUT -- A targets "B" and two entities are named B, each targeting
"A" -- doubles its calls at every level, so a depth limit alone would still leave ~2^64 calls:
a hang instead of a crash.

So two limits, both far above anything a map does (a button that fires 300 lights is 300 calls at
depth 1; a long relay chain is a few dozen deep). 128 nested uses is a few hundred KB of stack at
most, well inside even a 1 MB main-thread stack:
  RP_MAX_USE_DEPTH   uses nested inside each other;
  RP_MAX_USE_CHAIN   uses in total under one outermost use (the count restarts at depth 0).
Past either, that use is skipped; the chain above it finishes normally. One log line per map names
the entity where it stopped. The counters live in level, so a new map always starts them at zero,
and the depth is restored on every return path.
==============================
*/
#define RP_MAX_USE_DEPTH	128
#define RP_MAX_USE_CHAIN	2048

void GlobalUse(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	if (!self || (self->flags & FL_INACTIVE))
	{
		return;
	}

	if (!self->use)
	{
		return;
	}

	// DAJ_RP: [Locks] a player whose account is not on the lock's list cannot use it (g_locks.c)
	if (other != self && !RP_LockAllows(self, other, activator))
	{
		return;
	}

	if (level.rp_use_depth == 0)
	{
		level.rp_use_chain = 0;
	}

	if (level.rp_use_depth >= RP_MAX_USE_DEPTH || level.rp_use_chain >= RP_MAX_USE_CHAIN)
	{
		if (!level.rp_use_limit_warned)
		{
			level.rp_use_limit_warned = qtrue;
			G_LogPrintf("Entity %d (%s, targetname \"%s\") was not used: its targets use each other in a loop "
				"(stopped at %s). Check the entities that target \"%s\".\n",
				self->s.number, self->classname ? self->classname : "?", self->targetname ? self->targetname : "",
				level.rp_use_depth >= RP_MAX_USE_DEPTH ? va("%d nested uses", RP_MAX_USE_DEPTH) : va("%d uses in one chain", RP_MAX_USE_CHAIN),
				self->targetname ? self->targetname : "");
		}
		return;
	}

	level.rp_use_depth++;
	level.rp_use_chain++;
	// GLX_U: [Lua] the script's on_use, when it is a script's entity, and the "use" event for its
	// targetname: either can stop the use by returning false (rp_lua_world.c). After the lock and the
	// loop limits, so a script never sees a use the game would refuse anyway, and inside the counted
	// depth, so uses a script sets off count towards those limits too.
	if (RP_Lua_EntityUsed(self, other, activator, qfalse) && self->inuse && self->use)
	{
		self->use(self, other, activator);
	}
	level.rp_use_depth--;
}

void G_UseTargets2( gentity_t *ent, gentity_t *activator, const char *string ) {
	gentity_t		*t;

	if ( !ent ) {
		return;
	}

	if (ent->targetShaderName && ent->targetShaderNewName) {
		float f = level.time * 0.001;
		AddRemap(ent->targetShaderName, ent->targetShaderNewName, f);
		trap->SetConfigstring(CS_SHADERSTATE, BuildShaderStateConfig());
	}

	if ( !string || !string[0] ) {
		return;
	}

	t = NULL;
	while ( (t = G_Find (t, FOFS(targetname), string)) != NULL ) {
		if ( t == ent ) {
			trap->Print ("WARNING: Entity used itself.\n");
		} else {
			if ( t->use ) {
				GlobalUse(t, ent, activator);
			}
		}
		if ( !ent->inuse ) {
			trap->Print("entity was removed while using targets\n");
			return;
		}
	}
}
/*
==============================
G_UseTargets

"activator" should be set to the entity that initiated the firing.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function

==============================
*/
void G_UseTargets( gentity_t *ent, gentity_t *activator )
{
	if (!ent)
	{
		return;
	}
	G_UseTargets2(ent, activator, ent->target);
}


/*
=============
TempVector

This is just a convenience function
for making temporary vectors for function calls
=============
*/
float	*tv( float x, float y, float z ) {
	static	int		index;
	static	vec3_t	vecs[8];
	float	*v;

	// use an array so that multiple tempvectors won't collide
	// for a while
	v = vecs[index];
	index = (index + 1)&7;

	v[0] = x;
	v[1] = y;
	v[2] = z;

	return v;
}


/*
=============
VectorToString

This is just a convenience function
for printing vectors
=============
*/
char	*vtos( const vec3_t v ) {
	static	int		index;
	static	char	str[8][32];
	char	*s;

	// use an array so that multiple vtos won't collide
	s = str[index];
	index = (index + 1)&7;

	Com_sprintf (s, 32, "(%i %i %i)", (int)v[0], (int)v[1], (int)v[2]);

	return s;
}


/*
===============
G_SetMovedir

The editor only specifies a single value for angles (yaw),
but we have special constants to generate an up or down direction.
Angles will be cleared, because it is being used to represent a direction
instead of an orientation.
===============
*/
void G_SetMovedir( vec3_t angles, vec3_t movedir ) {
	static vec3_t VEC_UP		= {0, -1, 0};
	static vec3_t MOVEDIR_UP	= {0, 0, 1};
	static vec3_t VEC_DOWN		= {0, -2, 0};
	static vec3_t MOVEDIR_DOWN	= {0, 0, -1};

	if ( VectorCompare (angles, VEC_UP) ) {
		VectorCopy (MOVEDIR_UP, movedir);
	} else if ( VectorCompare (angles, VEC_DOWN) ) {
		VectorCopy (MOVEDIR_DOWN, movedir);
	} else {
		AngleVectors (angles, movedir, NULL, NULL);
	}
	VectorClear( angles );
}

// GLX_U: [Lua] see g_local.h
int g_entityGen[MAX_ENTITIESTOTAL];

void G_InitGentity( gentity_t *e ) {
	e->inuse = qtrue;
	g_entityGen[e - g_entities]++;	// GLX_U: [Lua] a new occupant: older handles to this slot stop working
	e->classname = "noclass";
	e->s.number = e - g_entities;
	// GalaxyRP: [Logical Entities] the region is a property of the slot, decided here and nowhere
	// else. (TaystJK tests "< 1023" -- the world slot -- which happens never to matter; the constant
	// it means is MAX_GENTITIES.)
	e->isLogical = (e->s.number >= MAX_GENTITIES) ? qtrue : qfalse;
	e->r.ownerNum = ENTITYNUM_NONE;
	e->s.modelGhoul2 = 0; //assume not

	// zyk: setting default count
	level.zyk_spawn_strings_values_count[e->s.number] = 0;

	// GalaxyRP: [Logical Entities] the engine indexes its ICARUS bookkeeping by entity number and
	// was never told the logical region exists, so a logical slot must not be passed to it.
	if ( !e->isLogical )
	{
		trap->ICARUS_FreeEnt( (sharedEntity_t *)e );	//ICARUS information must be added after this point
	}
}

//give us some decent info on all the active ents -rww
static void G_SpewEntList(void)
{
	int i = 0;
	int numNPC = 0;
	int numProjectile = 0;
	int numTempEnt = 0;
	int numTempEntST = 0;
	char className[MAX_STRING_CHARS];
	gentity_t *ent;
	char *str;

#ifdef _DEBUG
	fileHandle_t fh;
	trap->FS_Open("entspew.txt", &fh, FS_WRITE);
#endif

	while (i < ENTITYNUM_MAX_NORMAL)
	{
		ent = &g_entities[i];
		if (ent->inuse)
		{
			if (ent->s.eType == ET_NPC)
			{
				numNPC++;
			}
			else if (ent->s.eType == ET_MISSILE)
			{
				numProjectile++;
			}
			else if (ent->freeAfterEvent)
			{
				numTempEnt++;
				if (ent->s.eFlags & EF_SOUNDTRACKER)
				{
					numTempEntST++;
				}

				str = va("TEMPENT %4i: EV %i\n", ent->s.number, ent->s.eType-ET_EVENTS);
				Com_Printf(str);
#ifdef _DEBUG
				if (fh)
				{
					trap->FS_Write(str, strlen(str), fh);
				}
#endif
			}

			if (ent->classname && ent->classname[0])
			{
				strcpy(className, ent->classname);
			}
			else
			{
				strcpy(className, "Unknown");
			}
			str = va("ENT %4i: Classname %s\n", ent->s.number, className);
			Com_Printf(str);
#ifdef _DEBUG
			if (fh)
			{
				trap->FS_Write(str, strlen(str), fh);
			}
#endif
		}

		i++;
	}

	str = va("TempEnt count: %i\nTempEnt ST: %i\nNPC count: %i\nProjectile count: %i\n", numTempEnt, numTempEntST, numNPC, numProjectile);
	Com_Printf(str);
#ifdef _DEBUG
	if (fh)
	{
		trap->FS_Write(str, strlen(str), fh);
		trap->FS_Close(fh);
	}
#endif
}

/*
=================
G_Spawn

Either finds a free entity, or allocates a new one.

  The slots from 0 to MAX_CLIENTS-1 are always reserved for clients, and will
never be used by anything else.

Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
// GalaxyRP fix: [Entity System] how many slots G_Spawn() below can still hand out: every unused one
// under level.num_entities, plus everything above it that has never been opened.
//
// The freetime rule G_Spawn() applies on its first pass is deliberately not repeated here, and that
// is only correct because its second pass really does ignore it. It did not always: the break that
// ends the search used to test "i != MAX_GENTITIES", which is always true, so the second pass never
// ran and a slot freed moments ago was not available at all. This counter was made to match that,
// and then the break was fixed to Raven's own "i != ENTITYNUM_MAX_NORMAL", which is what makes a
// recently-freed slot reusable again -- so the extra test came back out. The two have to move
// together: if the second pass is ever disabled again, this must skip those slots again, or it will
// report room G_Spawn cannot give and wave a guarded caller into the ERR_DROP below.
int G_FreeEntityCount( void ) {
	int			i, count = 0;
	gentity_t	*e;

	e = &g_entities[MAX_CLIENTS];
	for ( i = MAX_CLIENTS; i < level.num_entities; i++, e++ ) {
		if ( !e->inuse ) {
			count++;
		}
	}

	if ( level.num_entities < ENTITYNUM_MAX_NORMAL ) {
		count += ENTITYNUM_MAX_NORMAL - level.num_entities;
	}

	return count;
}

// GalaxyRP fix: [Entity System] the gate every player- or admin-driven spawn goes through. G_Spawn()
// itself cannot be made to fail politely -- it never returns NULL, ~70 call sites rely on that, and
// G_TempEntity() dereferences the result on the next line -- so when it runs out it calls
// trap->Error(ERR_DROP), which on a dedicated server ends the process rather than disconnecting
// anyone (see ZYK_ENTITY_RESERVE in g_local.h). The fix is not to make
// G_Spawn() fail better but to stop it ever being the one that runs out: keep ZYK_ENTITY_RESERVE
// slots that only the uncontrollable allocations can reach, and refuse the controllable ones first.
//
// "needed" is what the caller is about to allocate in one go -- an NPC costs more than one slot.
qboolean G_EntitySlotsAvailable( int needed ) {
	if ( needed < 1 ) {
		needed = 1;
	}

	return (G_FreeEntityCount() >= (needed + ZYK_ENTITY_RESERVE)) ? qtrue : qfalse;
}

gentity_t *G_Spawn( void ) {
	int			i, force;
	gentity_t	*e;

	e = NULL;	// shut up warning
	i = 0;		// shut up warning
	for ( force = 0 ; force < 2 ; force++ ) {
		// if we go through all entities and can't find one to free,
		// override the normal minimum times before use
		e = &g_entities[MAX_CLIENTS];
		for ( i = MAX_CLIENTS ; i<level.num_entities ; i++, e++) {
			if ( e->inuse ) {
				continue;
			}

			// the first couple seconds of server time can involve a lot of
			// freeing and allocating, so relax the replacement policy
			if ( !force && e->freetime > level.startTime + 2000 && level.time - e->freetime < 1000 )
			{
				continue;
			}

			// GalaxyRP fix: [Entity System] the second pass has just rescued a spawn that would
			// otherwise have dropped the server: the table is completely full and this slot was
			// freed less than a second ago, so a client may briefly see the old entity morph
			// rather than disappear. Worth exactly one line in the log, and no more.
			if ( force && !level.zyk_entity_force_reuse_warned ) {
				level.zyk_entity_force_reuse_warned = qtrue;
				G_LogPrintf( "entity table is completely full (%d slots); reusing slot %d, freed %dms "
					"ago, rather than dropping. Expect brief visual glitches until pressure eases.\n",
					level.num_entities, i, level.time - e->freetime );
			}

			// reuse this slot
			G_InitGentity( e );
			RP_LegacySlotAssign( e );
			return e;
		}
		// GalaxyRP fix: [Entity System] this was "i != MAX_GENTITIES", and that is a typo with a
		// 25-year head start -- Raven's own singleplayer code (code/game/g_utils.cpp and
		// codeJK2/game/g_utils.cpp) writes ENTITYNUM_MAX_NORMAL here; only the multiplayer copy
		// says MAX_GENTITIES, and every JKA fork inherited it.
		//
		// The inner loop above always ends with i == level.num_entities, and level.num_entities can
		// never exceed ENTITYNUM_MAX_NORMAL (1022) because the check just below ends the server at
		// that point -- it can never be MAX_GENTITIES (1024). So the test was always true, the break
		// always fired after the first pass, and the whole "force" pass was unreachable code. The
		// comment at the top of this loop describes something that had never once run.
		//
		// What that cost: with the table full and nothing free except slots released in the last
		// second, the server EXITED rather than reusing one -- see ZYK_ENTITY_RESERVE in
		// g_local.h for why ERR_DROP is not a drop here. Temp entities are
		// the easy way to get there -- each is freed a frame after it is created, so a busy moment
		// puts hundreds of slots inside that window at once.
		//
		// Reading it correctly: break out only when there is still room to open a new slot. When
		// there is not, fall through to force == 1 and take a recently-freed one, which costs at
		// worst a brief interpolation glitch on one entity.
		if ( i != ENTITYNUM_MAX_NORMAL ) {
			break;
		}
	}
	if ( i == ENTITYNUM_MAX_NORMAL ) {
		/*
		for (i = 0; i < MAX_GENTITIES; i++) {
			trap->Print("%4i: %s\n", i, g_entities[i].classname);
		}
		*/
		G_SpewEntList();

		// GalaxyRP fix: [Entity System] say why, in the log, before the error. On a dedicated
		// server trap->Error(ERR_DROP) is a process exit -- Com_Error promotes ERR_DROP to
		// ERR_FATAL under com_dedicated, see ZYK_ENTITY_RESERVE in g_local.h -- so an admin
		// investigating afterwards has only whatever reached games.log. G_SpewEntList() above
		// writes its census with trap->Print, which goes to the console and not to the log, so
		// without this line the log simply stops mid-map with no reason given. G_ShutdownGame
		// closes level.logFile on the way down, which flushes it.
		G_LogPrintf( "entity table exhausted at %d slots: no free entities and nothing recently "
			"freed to recycle. The server is about to exit; see the console for the entity "
			"census.\n", level.num_entities );

		trap->Error( ERR_DROP, "G_Spawn: no free entities" );
	}

	// GalaxyRP fix: [Entity System] one-time early warning. The guards on the player-driven spawn
	// paths keep ZYK_ENTITY_RESERVE slots back for allocations nothing can refuse -- temp entities,
	// missiles, gibs -- but sustained pressure from those can still eat the reserve, and when it is
	// gone, all that stands between this function and trap->Error(ERR_DROP) -- which ends the
	// server process on a dedicated build -- is the force pass recycling a slot freed moments ago.
	// Say so once while there is still room to act, rather than leaving the log silent until the
	// exit. Deliberately not a refusal: this path has ~70 callers that cannot handle one.
	if ( !level.zyk_entity_reserve_warned
		&& (ENTITYNUM_MAX_NORMAL - level.num_entities) < ZYK_ENTITY_RESERVE )
	{
		level.zyk_entity_reserve_warned = qtrue;
		G_LogPrintf( "entity table is into its reserve: %d of %d slots used. Player-driven spawns "
			"are being refused; if this keeps climbing the server will drop.\n",
			level.num_entities, ENTITYNUM_MAX_NORMAL );
	}

	// open up a new slot
	level.num_entities++;

	// let the server system know that there are more entities
	trap->LocateGameData( (sharedEntity_t *)level.gentities, level.num_entities, sizeof( gentity_t ),
		&level.clients[0].ps, sizeof( level.clients[0] ) );

	G_InitGentity( e );
	RP_LegacySlotAssign( e );
	return e;
}

/*
=================
G_SpawnLogical

GalaxyRP: [Logical Entities] the upper-region counterpart of G_Spawn(): a slot at or above
MAX_GENTITIES that the engine never hears about. Same search as G_Spawn -- reuse a free slot,
preferring one not freed in the last second, then open a new one -- over
[MAX_GENTITIES, MAX_GENTITIES + level.num_logicalents). No LocateGameData: there is nothing to tell
the server. Two things are deliberately unlike TaystJK's version: the second (force) pass really
runs when the region is full of recently-freed slots (their "i != MAX_ENTITIESTOTAL" break made it
unreachable, the same typo G_Spawn had), and the exhaustion test is ">=" rather than their
"== MAX_ENTITIESTOTAL - 1", which is one short and let them return the slot past the array.
Exhausting 3072 logical slots is a map that is broken, not a server that must stay up, so this
one keeps the ERR_DROP -- but says why in the log first, as G_Spawn does.
=================
*/
gentity_t *G_SpawnLogical( void ) {
	int			i, force;
	gentity_t	*e;

	e = NULL;
	i = 0;
	for ( force = 0; force < 2; force++ ) {
		e = g_logicalents;
		for ( i = MAX_GENTITIES; i < MAX_GENTITIES + level.num_logicalents; i++, e++ ) {
			if ( e->inuse ) {
				continue;
			}

			// the first couple seconds of server time can involve a lot of
			// freeing and allocating, so relax the replacement policy
			if ( !force && e->freetime > level.startTime + 2000 && level.time - e->freetime < 1000 )
			{
				continue;
			}

			// reuse this slot
			G_InitGentity( e );
			RP_LegacySlotAssign( e );
			return e;
		}
		if ( i < MAX_ENTITIESTOTAL ) {
			break;	// there is still room to open a new slot; no need to force-reuse one
		}
	}
	if ( i >= MAX_ENTITIESTOTAL ) {
		G_LogPrintf( "logical entity region exhausted at %d slots: no free logical entities and "
			"nothing recently freed to recycle.\n", level.num_logicalents );
		trap->Error( ERR_DROP, "G_SpawnLogical: no free logical entities" );
		return NULL;
	}

	// open up a new slot
	level.num_logicalents++;

	G_InitGentity( e );
	RP_LegacySlotAssign( e );
	return e;
}

// GalaxyRP: [Logical Entities] the legacy-allocator simulation, see gentity_t::legacySlot in
// g_local.h. The old G_Spawn, during a map's spawn pass, always took the lowest slot at or above
// MAX_CLIENTS that was not in use, and opened a new one only when there was none -- the "do not
// reuse a slot freed less than a second ago" rule never applied then, because every free during
// the pass carries freetime == level.startTime. That is the whole algorithm, so it is cheap to
// run beside the real allocator: every allocation during the pass, in either region, takes the
// next legacy slot, and every free during the pass gives its legacy slot back. Outside the pass
// nothing here runs and legacySlot stays 0. With rp_logical_entities 0 the simulation and the
// real allocator agree slot for slot, which is what the harness checks.
void RP_LegacySlotsBegin( void ) {
	int i;

	memset( level.legacy_slot_inuse, 0, sizeof( level.legacy_slot_inuse ) );
	for ( i = 0; i < level.num_entities && i < MAX_GENTITIES; i++ ) {
		// the client slots are never handed out whatever their state; above them, the table as it
		// stands (the body queue, in practice)
		level.legacy_slot_inuse[i] = ( i < MAX_CLIENTS || g_entities[i].inuse ) ? 1 : 0;
	}
	level.legacy_slot_next = level.num_entities;
}

void RP_LegacySlotAssign( gentity_t *e ) {
	int i;

	if ( !level.spawning || !e ) {
		return;
	}

	for ( i = MAX_CLIENTS; i < level.legacy_slot_next; i++ ) {
		if ( !level.legacy_slot_inuse[i] ) {
			level.legacy_slot_inuse[i] = 1;
			e->legacySlot = i;
			return;
		}
	}

	if ( level.legacy_slot_next >= ENTITYNUM_MAX_NORMAL ) {
		// the old allocator would have dropped the server here; there is no legacy number to give
		e->legacySlot = 0;
		return;
	}

	level.legacy_slot_inuse[level.legacy_slot_next] = 1;
	e->legacySlot = level.legacy_slot_next++;
}

void RP_LegacySlotRelease( gentity_t *e ) {
	if ( !level.spawning || !e || e->legacySlot <= 0 || e->legacySlot >= MAX_GENTITIES ) {
		return;
	}

	level.legacy_slot_inuse[e->legacySlot] = 0;
}

// GalaxyRP: [Logical Entities] see g_local.h. Informational only.
int G_FreeLogicalEntityCount( void ) {
	int			i, count = 0;
	gentity_t	*e;

	e = g_logicalents;
	for ( i = 0; i < level.num_logicalents; i++, e++ ) {
		if ( !e->inuse ) {
			count++;
		}
	}

	return count + (MAX_LOGICENTITIES - level.num_logicalents);
}

// GalaxyRP: [Logical Entities] see RP_FOR_EACH_ENTITY in g_local.h: the networked slots
// [0, num_entities), then the logical ones [MAX_GENTITIES, MAX_GENTITIES + num_logicalents).
gentity_t *RP_NextEntityInAnyRegion( gentity_t *from ) {
	int next;

	if ( !from ) {
		next = 0;
	} else {
		next = (from - g_entities) + 1;
	}

	if ( next < MAX_GENTITIES && next >= level.num_entities ) {
		next = MAX_GENTITIES;	// the end of the networked region: jump to the logical one
	}

	if ( next >= MAX_GENTITIES + level.num_logicalents ) {
		return NULL;
	}

	return &g_entities[next];
}

/*
=================
G_EntitiesFree
=================
*/
qboolean G_EntitiesFree( void ) {
	int			i;
	gentity_t	*e;

	e = &g_entities[MAX_CLIENTS];
	for ( i = MAX_CLIENTS; i < level.num_entities; i++, e++) {
		if ( e->inuse ) {
			continue;
		}
		// slot available
		return qtrue;
	}
	return qfalse;
}

#define MAX_G2_KILL_QUEUE 256

int gG2KillIndex[MAX_G2_KILL_QUEUE];
int gG2KillNum = 0;

void G_SendG2KillQueue(void)
{
	char g2KillString[1024];
	int i = 0;

	if (!gG2KillNum)
	{
		return;
	}

	Com_sprintf(g2KillString, 1024, "kg2");

	while (i < gG2KillNum && i < 64)
	{ //send 64 at once, max...
		Q_strcat(g2KillString, 1024, va(" %i", gG2KillIndex[i]));
		i++;
	}

	trap->SendServerCommand(-1, g2KillString);

	//Clear the count because we just sent off the whole queue
	gG2KillNum -= i;
	if (gG2KillNum < 0)
	{ //hmm, should be impossible, but I'm paranoid as we're far past beta.
		assert(0);
		gG2KillNum = 0;
	}
}

void G_KillG2Queue(int entNum)
{
	if (gG2KillNum >= MAX_G2_KILL_QUEUE)
	{ //This would be considered a Bad Thing.
#ifdef _DEBUG
		Com_Printf("WARNING: Exceeded the MAX_G2_KILL_QUEUE count for this frame!\n");
#endif
		//Since we're out of queue slots, just send it now as a seperate command (eats more bandwidth, but we have no choice)
		trap->SendServerCommand(-1, va("kg2 %i", entNum));
		return;
	}

	gG2KillIndex[gG2KillNum] = entNum;
	gG2KillNum++;
}

/*
=================
G_FreeEntity

Marks the entity as free
=================
*/
extern qboolean EjectAll( Vehicle_t *pVeh );
void G_FreeEntity( gentity_t *ed ) {
	//gentity_t *te;

	if (ed->isSaberEntity)
	{
#ifdef _DEBUG
		Com_Printf("Tried to remove JM saber!\n");
#endif
		return;
	}

	// zyk: if npc is cleaned directly, test if goal entity is still there. If it is, clean the goal entity too
	if (ed->NPC && ed->NPC->tempGoal)
	{
		G_FreeEntity(ed->NPC->tempGoal);
		ed->NPC->tempGoal = NULL;
	}

	// zyk: if entity is a vehicle with a player inside, make player get out of vehicle first
	if (ed->client && ed->NPC && ed->client->NPC_class == CLASS_VEHICLE && ed->m_pVehicle && ed->m_pVehicle->m_pPilot)
	{
		EjectAll(ed->m_pVehicle);
	}

	// GalaxyRP: [Grapple Hook] the same idea as the three id fields around this: a hook's owner holds a
	// bare pointer to it (client->hook), and G_Spawn() recycles a freed slot after a second. Any free
	// that does not go through Weapon_HookFree() -- /entremove, the duel-tournament arena sweep in
	// G_RunMissile(), a map change's mass free -- would otherwise leave that pointer aimed at whatever
	// took the slot, and the owner's next button release would Weapon_HookFree() THAT. Detach here,
	// whoever frees it and however: pointer, pull flag and both fire latches, exactly what
	// Weapon_HookFree() clears, so that path stays the documented one and this the net under it.
	if (ed->classname && ed->parent && ed->parent->client && ed->parent->client->hook == ed &&
		!strcmp(ed->classname, RP_HOOK_CLASSNAME))
	{
		ed->parent->client->hook = NULL;
		ed->parent->client->ps.pm_flags &= ~PMF_GRAPPLE;
		ed->parent->client->hookHasBeenFired = qfalse;
		ed->parent->client->fireHeld = qfalse;
	}

	// GalaxyRP fix: [Entity System] the same cleanup the chaos portal above has always had, now
	// applied to the other two level fields that hold a bare entity number. Both store the id of a
	// model spawned for a mini-game arena -- the Duel Tournament globe and the Melee Battle catwalk
	// -- and both were only ever cleared by their own *_end() function. Anything else that freed the
	// entity left the id dangling: /entremove accepts it (it only refuses the reserved client and
	// body-queue slots), and G_Spawn() recycles a freed slot after about a second, so by the time
	// duel_tournament_end() or melee_battle_end() ran its G_FreeEntity(&g_entities[id]) that slot
	// could belong to something else entirely -- which then got freed instead. Clearing the id here
	// means whoever frees the entity, by whatever route, the "!= -1" guards downstream see the
	// truth and simply skip the free.
	if (level.duel_tournament_model_id != -1 && level.duel_tournament_model_id == ed->s.number)
	{
		level.duel_tournament_model_id = -1;
	}

	if (level.melee_model_id != -1 && level.melee_model_id == ed->s.number)
	{
		level.melee_model_id = -1;
	}

	// GalaxyRP: [Logical Entities] a logical entity was never linked and never given to ICARUS,
	// and the engine would fatal-error on its number; everything else in this function acts on
	// the struct itself and applies to both regions.
	if ( !ed->isLogical )
	{
		trap->UnlinkEntity ((sharedEntity_t *)ed);		// unlink from world

		trap->ICARUS_FreeEnt( (sharedEntity_t *)ed );	//ICARUS information must be added after this point
	}

	if ( ed->neverFree ) {
		return;
	}

	// GLX_U: [Lua] the slot's handles stop working, and a script's entity is counted out of its
	// plugin -- bookkeeping only, no script runs here (RP_Lua_EntityFreed, rp_lua_world.c). Below the
	// neverFree bail: an entity that is not really freed keeps its handles.
	g_entityGen[ed - g_entities]++;
	RP_Lua_EntityFreed( ed );

	// GalaxyRP fix: [Entity System] this reset used to sit above the neverFree bail, so an entity that
	// is never actually freed -- the body queue is the only one in practice -- lost its Entity System
	// key/value record anyway. /entremove on such an entity left it alive on the map but stripped of
	// its record, after which /entedit showed nothing for it and /entsave silently wrote it out empty,
	// with no error either way. Everything above this point is upstream OpenJK and keeps its order;
	// only this line, which is the mod's own addition, moves below the bail so the record survives
	// exactly as long as the entity does.
	level.zyk_spawn_strings_values_count[ed->s.number] = 0;

	// GalaxyRP fix: [Entity System] and the third level field that holds an entity reference
	// (a fourth, chaos_portal_id, went with the quest engine).
	//
	// The two above -- duel_tournament_model_id, melee_model_id -- are cleared
	// here for exactly this reason, and level.last_spawned_entity was missed in that pass. It is
	// the /entundo target, written by /entadd, /spawnplatform and /spawndummy, and it was only ever
	// cleared by /entundo itself and by G_InitGame. Anything else that freed the entity left a
	// pointer to a dead slot behind.
	//
	// Which is worse than the dangling ids, because it is a POINTER: G_Spawn() recycles a freed
	// slot after about a second, so /entundo did not merely free a blank entity, it freed whatever
	// had moved in. /entload makes that the normal case rather than a corner one -- it frees
	// everything from MAX_CLIENTS + BODY_QUEUE_SIZE upward and immediately respawns the preset into
	// those same slots, so "/entadd X, /entload default, /entundo" deleted one of the preset's
	// entities instead of X.
	//
	// An "is it still in use" test at the /entundo end would not have caught that: a recycled slot
	// is in use, by something else. Clearing the pointer where the entity actually dies is the only
	// place that can tell the difference.
	//
	// Below the neverFree bail on purpose, with the spawn-string reset above it and for the same
	// reason: an entity that is not really being freed should not have its record -- or, here, its
	// undo slot -- thrown away. G_Spawn() never hands out a neverFree entity, so no /entundo target
	// can reach that bail today; this keeps the two lines honest if one ever does.
	if ( level.last_spawned_entity == ed )
	{
		level.last_spawned_entity = NULL;
	}

	//rww - this may seem a bit hackish, but unfortunately we have no access
	//to anything ghoul2-related on the server and thus must send a message
	//to let the client know he needs to clean up all the g2 stuff for this
	//now-removed entity
	// GalaxyRP: [Logical Entities] the two broadcasts below carry the entity NUMBER to every client,
	// which indexes cg_entities[MAX_GENTITIES] with it. No logical class sets either flag, but a
	// number from the upper region must never go out in them, so both are gated on the region.
	if (ed->s.modelGhoul2 && !ed->isLogical)
	{ //force all clients to accept an event to destroy this instance, right now
		/*
		te = G_TempEntity( vec3_origin, EV_DESTROY_GHOUL2_INSTANCE );
		te->r.svFlags |= SVF_BROADCAST;
		te->s.eventParm = ed->s.number;
		*/
		//Or not. Events can be dropped, so that would be a bad thing.
		G_KillG2Queue(ed->s.number);
	}

	//And, free the server instance too, if there is one.
	if (ed->ghoul2)
	{
		trap->G2API_CleanGhoul2Models(&(ed->ghoul2));
	}

	if (ed->s.eType == ET_NPC && ed->m_pVehicle)
	{ //tell the "vehicle pool" that this one is now free
		G_FreeVehicleObject(ed->m_pVehicle);
	}

	if (ed->s.eType == ET_NPC && ed->client)
	{ //this "client" structure is one of our dynamically allocated ones, so free the memory
		int saberEntNum = -1;
		int i = 0;
		if (ed->client->ps.saberEntityNum)
		{
			saberEntNum = ed->client->ps.saberEntityNum;
		}
		else if (ed->client->saberStoredIndex)
		{
			saberEntNum = ed->client->saberStoredIndex;
		}

		if (saberEntNum > 0 && g_entities[saberEntNum].inuse)
		{
			g_entities[saberEntNum].neverFree = qfalse;
			G_FreeEntity(&g_entities[saberEntNum]);
		}

		while (i < MAX_SABERS)
		{
			if (ed->client->weaponGhoul2[i] && trap->G2API_HaveWeGhoul2Models(ed->client->weaponGhoul2[i]))
			{
				trap->G2API_CleanGhoul2Models(&ed->client->weaponGhoul2[i]);
			}
			i++;
		}

		G_FreeFakeClient(&ed->client);
	}

	if ((ed->s.eFlags & EF_SOUNDTRACKER) && !ed->isLogical)
	{
		int i = 0;
		gentity_t *ent;

		while (i < MAX_CLIENTS)
		{
			ent = &g_entities[i];

			if (ent && ent->inuse && ent->client)
			{
				int ch = TRACK_CHANNEL_NONE-50;

				while (ch < NUM_TRACK_CHANNELS-50)
				{
					if (ent->client->ps.fd.killSoundEntIndex[ch] == ed->s.number)
					{
						ent->client->ps.fd.killSoundEntIndex[ch] = 0;
					}

					ch++;
				}
			}

			i++;
		}

		//make sure clientside loop sounds are killed on the tracker and client
		trap->SendServerCommand(-1, va("kls %i %i", ed->s.trickedentindex, ed->s.number));
	}

	// GalaxyRP: [Logical Entities] a free during the spawn pass frees a legacy slot too; no-op
	// at any other time. Before the memset, which clears the field.
	RP_LegacySlotRelease( ed );

	// GalaxyRP fix: [Entity System] out of its team. The links G_FindTeams() builds are raw pointers,
	// and a member freed by /entremove (or any other way) stayed in its master's chain: G_MoverTeam
	// moved whatever G_Spawn() next put in the slot as if it were a door. See RP_TeamDetach().
	RP_TeamDetach( ed );

	// GalaxyRP fix: [Entity System] a misc_weapon_shooter gives its pool client back (g_misc.c);
	// nothing did, so with the pool marked in use a removed shooter kept its slot for the map
	if ( G_IsShooterClient( ed->client ) )
	{
		G_FreeClientForShooter( ed->client );
	}

	// GalaxyRP fix: [Entity System] the sky portal an admin added goes with its entity -- see
	// RP_SkyPortalRelease() in g_misc.c. Before the memset, which clears the number it is known by.
	RP_SkyPortalRelease( ed );

	memset (ed, 0, sizeof(*ed));
	ed->classname = "freed";
	ed->freetime = level.time;
	ed->inuse = qfalse;

	// GalaxyRP: [Logical Entities] if this was the highest logical slot ever opened, give the
	// slots above the last in-use one back, so the region's high-water mark tracks what is really
	// there and G_SpawnLogical's search stays short. Decided from the slot's position rather than
	// from isLogical, because the memset above has just cleared that -- which is exactly the
	// mistake TaystJK's copy makes (it tests the flag after the memset, so its roll-back never
	// runs). The networked region is not rolled back: level.num_entities is what the engine was
	// told in LocateGameData, and stock OpenJK never shrinks it either.
	{
		int entnum = ed - g_entities;

		if ( entnum >= MAX_GENTITIES && entnum == MAX_GENTITIES + level.num_logicalents - 1 )
		{
			int i;

			for ( i = entnum; i >= MAX_GENTITIES; i-- ) {
				if ( g_entities[i].inuse ) {
					break;
				}
			}
			level.num_logicalents = i + 1 - MAX_GENTITIES;
		}
	}
}

/*
=================
G_MusicStringValid / G_SetMusic

GalaxyRP fix: [Music] target_play_music and /playmusic wrote their string straight into CS_MUSIC: up to a
whole argument's worth of bytes into the gamestate every client downloads, outside the budget every other
registration keeps (G_FindConfigstringIndex), so later props were refused for room this had spent -- or
a map near the budget dropped its next joiner. The client reads two tokens (an intro and a loop, each a
MAX_QPATH path); that is all this lets through, and only while the gamestate has room for the change.
=================
*/
qboolean G_MusicStringValid( const char *music, char *normalized, int size, const char **reason )
{
	char copy[MAX_STRING_CHARS];
	char tokens[2][MAX_QPATH];
	const char *p = copy;
	const char *token;
	int count = 0;

	if ( reason )
		*reason = "";
	if ( !VALIDSTRING( music ) )
	{
		if ( reason ) *reason = "no music file given";
		return qfalse;
	}

	Q_strncpyz( copy, music, sizeof( copy ) );
	COM_BeginParseSession( "G_MusicStringValid" );
	while ( 1 )
	{
		token = COM_Parse( &p );
		if ( !token[0] )
			break;
		if ( count >= 2 )
		{
			if ( reason ) *reason = "more than two file names (an intro and a loop are all the client plays)";
			return qfalse;
		}
		if ( strlen( token ) >= MAX_QPATH )
		{
			if ( reason ) *reason = va( "a file name longer than %d characters", MAX_QPATH - 1 );
			return qfalse;
		}
		Q_strncpyz( tokens[count], token, sizeof( tokens[count] ) );
		count++;
	}

	if ( count == 0 )
	{
		if ( reason ) *reason = "no music file given";
		return qfalse;
	}
	if ( normalized && size > 0 )
	{
		if ( count == 2 )
			Com_sprintf( normalized, size, "%s %s", tokens[0], tokens[1] );
		else
			Q_strncpyz( normalized, tokens[0], size );
	}
	return qtrue;
}

qboolean G_SetMusic( const char *music, const char **reason )
{
	char normalized[MAX_QPATH * 2];
	char current[MAX_STRING_CHARS];
	int delta;

	if ( !G_MusicStringValid( music, normalized, sizeof( normalized ), reason ) )
		return qfalse;

	// the gamestate counts what replaces what is there now: each string that is there at all costs
	// its length plus one (zyk_gamestate_bytes_used), an empty slot nothing
	trap->GetConfigstring( CS_MUSIC, current, sizeof( current ) );
	delta = ( (int)strlen( normalized ) + 1 ) - ( current[0] ? (int)strlen( current ) + 1 : 0 );
	if ( delta > 0 && !G_ConfigstringBytesAvailable( delta ) )
	{
		if ( reason ) *reason = "the gamestate is full";
		G_LogPrintf( "music \"%s\" refused: the gamestate is at %d of %d bytes\n", normalized, G_GamestateBytesUsed(), ZYK_GAMESTATE_BUDGET );
		return qfalse;
	}

	trap->SetConfigstring( CS_MUSIC, normalized );
	return qtrue;
}

/*
=================
RP_TeamDetach

GalaxyRP: [Entity System] take an entity out of the team G_FindTeams() (or RP_TeamLinkEntity()) linked it
into, leaving the rest of the team consistent. A slave is unhooked from its master's chain. A master's
team is dissolved: every member goes on alone (no master, no chain, no FL_TEAMSLAVE) -- promoting one
would change which door a targetname reaches, since G_FindTeams() moved the slaves' targetnames onto
the master, and a map with that door deleted is a map of single doors. Every walk is bounded. Called
from G_FreeEntity() before the memset, and before an entity is spawned again in place.
=================
*/
void RP_TeamDetach( gentity_t *ed )
{
	gentity_t *m, *next;
	int steps = 0;

	if ( !ed )
	{
		return;
	}
	if ( !ed->teammaster )
	{
		ed->teamchain = NULL;
		ed->flags &= ~FL_TEAMSLAVE;
		return;
	}

	if ( ed->teammaster == ed )
	{ // the master: the team goes on as single entities -- each a master of one, which is what
	  // G_FindTeams() and RP_TeamLinkEntity() both take a lone teamed entity for, so a member added
	  // to that team later joins one of them rather than standing beside them, whichever of the two
	  // links it (the misc_bsp rebuild runs G_FindTeams() again)
		int members = 0;

		for ( m = ed->teamchain; m && steps < MAX_GENTITIES; m = next, steps++ )
		{
			next = m->teamchain;
			if ( m->teammaster == ed )
			{
				m->teammaster = m;
				m->teamchain = NULL;
				m->flags &= ~FL_TEAMSLAVE;
				members++;
			}
		}
		if ( members > 0 )
		{
			G_LogPrintf( "entity %d (%s), master of team %s, removed: its %d member(s) go on alone\n",
				ed->s.number, ed->classname ? ed->classname : "noclass", ed->team ? ed->team : "", members );
		}
	}
	else
	{ // a slave: out of its master's chain
		gentity_t *master = ed->teammaster;

		if ( master->inuse && master->teammaster == master )
		{
			for ( m = master; m && m->teamchain && steps < MAX_GENTITIES; m = m->teamchain, steps++ )
			{
				if ( m->teamchain == ed )
				{
					m->teamchain = ed->teamchain;
					break;
				}
			}
		}
	}

	ed->teammaster = NULL;
	ed->teamchain = NULL;
	ed->flags &= ~FL_TEAMSLAVE;
}

/*
=================
G_TempEntity

Spawns an event entity that will be auto-removed
The origin will be snapped to save net bandwidth, so care
must be taken if the origin is right on a surface (snap towards start vector first)
=================
*/
gentity_t *G_TempEntity( vec3_t origin, int event ) {
	gentity_t		*e;
	vec3_t		snapped;

	e = G_Spawn();
	e->s.eType = ET_EVENTS + event;

	e->classname = "tempEntity";
	e->eventTime = level.time;
	e->freeAfterEvent = qtrue;

	VectorCopy( origin, snapped );
	SnapVector( snapped );		// save network bandwidth
	G_SetOrigin( e, snapped );
	//WTF?  Why aren't we setting the s.origin? (like below)
	//cg_events.c code checks origin all over the place!!!
	//Trying to save bandwidth...?
	//VectorCopy( snapped, e->s.origin );

	// find cluster for PVS
	trap->LinkEntity( (sharedEntity_t *)e );

	return e;
}


/*
=================
G_SoundTempEntity

Special event entity that keeps sound trackers in mind
=================
*/
gentity_t *G_SoundTempEntity( vec3_t origin, int event, int channel ) {
	gentity_t		*e;
	vec3_t		snapped;

	e = G_Spawn();

	e->s.eType = ET_EVENTS + event;
	e->inuse = qtrue;

	e->classname = "tempEntity";
	e->eventTime = level.time;
	e->freeAfterEvent = qtrue;

	VectorCopy( origin, snapped );
	SnapVector( snapped );		// save network bandwidth
	G_SetOrigin( e, snapped );

	// find cluster for PVS
	trap->LinkEntity( (sharedEntity_t *)e );

	return e;
}


//scale health down below 1024 to fit in health bits
void G_ScaleNetHealth(gentity_t *self)
{
	int maxHealth = self->maxHealth;

    if (maxHealth < 1000)
	{ //it's good then
		self->s.maxhealth = maxHealth;
		self->s.health = self->health;

		if (self->s.health < 0)
		{ //don't let it wrap around
			self->s.health = 0;
		}
		return;
	}

	//otherwise, scale it down
	self->s.maxhealth = (maxHealth/100);
	self->s.health = (self->health/100);

	if (self->s.health < 0)
	{ //don't let it wrap around
		self->s.health = 0;
	}

	if (self->health > 0 &&
		self->s.health <= 0)
	{ //don't let it scale to 0 if the thing is still not "dead"
		self->s.health = 1;
	}
}



/*
==============================================================================

Kill box

==============================================================================
*/

/*
=================
RP_VehicleOf

DAJ_RP: [Vehicles] the vehicle an entity is part of: a vehicle itself, or the one a pilot or passenger (a player or an
NPC) rides; 0 for anyone else. A spectator is never part of one -- one following a rider carries a copy of the rider's
playerState, vehicle number and all. A vehicle's own ps.m_iVehicleNum is something else (its pilot, plus one), so a
vehicle is known by its class and its Vehicle_t.
=================
*/
int RP_VehicleOf( const gentity_t *ent )
{
	if ( !ent || !ent->inuse || !ent->client )
	{
		return 0;
	}
	if ( ent->s.NPC_class == CLASS_VEHICLE && ent->m_pVehicle )
	{
		return ent->s.number;
	}
	if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR || ( ent->client->ps.pm_flags & PMF_FOLLOW ) )
	{
		return 0;
	}
	if ( ent->client->ps.m_iVehicleNum > 0 && ent->client->ps.m_iVehicleNum < ENTITYNUM_WORLD )
	{
		return ent->client->ps.m_iVehicleNum;
	}
	return 0;
}

// DAJ_RP: [Vehicles] a pilot or passenger, a player or an NPC -- not the vehicle itself
qboolean RP_IsVehicleRider( const gentity_t *ent )
{
	const int veh = RP_VehicleOf( ent );

	return ( veh && veh != ent->s.number ) ? qtrue : qfalse;
}

/*
=================
G_KillBox

Kills all entities that would touch the proposed new positioning
of ent.  Ent should be unlinked before calling this!

GalaxyRP fix: [Telefrag] a telefrag must end with nobody standing inside anybody else. Stock JKA got
that for free -- the victim died and a corpse blocks nobody -- but here three things broke it:

  - the Death System downed the victim instead, and a downed player is a solid body, so the
    arrival and the victim stood inside each other and neither could move. G_Damage() kills on
    MOD_TELEFRAG outright now (the certain-death test in g_combat.c).
  - the damage can be turned away before it lands: allies, chat protection, private duels, the arena
    rules, noclip, an NPC a script made invulnerable. The victim survives, still solid. They keep
    their protection; the arrival walks out of them instead (RP_StartOverlapRelease, g_active.c).
  - an NPC coming out of a teleporter onto a player -- NPCs use trigger_teleport like players do, a
    script can fire target_teleporter with an NPC as activator, and trigger_hyperspace moves
    vehicles -- would kill that player outright now. It no longer telefrags players at all; it
    walks out of them the same way. NPCs arriving on NPCs still telefrag them, as stock.
    (Spawning is not a case: NPC_Begin() (NPC_spawn.c) waits while NPC_SpotWouldTelefrag() finds a
    solid body in the spot, and its call here only comes after that check has passed, so a newly
    spawned NPC never lands on a live player. An NPC spawned not-solid, spawnflag 64, skips both.)

And a body already passing through bodies -- /admwalkthrough, /admghost, /admholo, /admhidden, /npc effect,
or the tail of a release -- on either side cannot get stuck, so it is neither telefragged nor released.

DAJ_RP: [Vehicles] nor is anyone aboard the same vehicle (RP_VehicleOf() above): its riders all sit at its one driver
tag, so a vehicle arriving lands on its own pilot and passengers, and they on each other.
=================
*/
extern qboolean duel_tournament_is_duelist(gentity_t *ent);
void G_KillBox (gentity_t *ent) {
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	vec3_t		mins, maxs;
	qboolean	release = qfalse;

	VectorAdd( ent->client->ps.origin, ent->r.mins, mins );
	VectorAdd( ent->client->ps.origin, ent->r.maxs, maxs );
	num = trap->EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	for (i=0 ; i<num ; i++) {
		hit = &g_entities[touch[i]];
		if ( !hit->client ) {
			continue;
		}

		if (hit->s.number == ent->s.number)
		{ //don't telefrag yourself!
			continue;
		}

		if (ent->r.ownerNum == hit->s.number)
		{ //don't telefrag your vehicle!
			continue;
		}

		// DAJ_RP fix: [Vehicles] nor anyone else aboard it: a fellow pilot or passenger, or -- for a vehicle arriving --
		// its own riders. Neither damage nor a walk-out: the vehicle seats its riders itself, every one of them at its
		// driver tag. The skip above only spared a rider's vehicle and a vehicle's pilot, so a teleport could kill a
		// passenger, whoever reached the destination first (RP_VehicleOf)
		if ( RP_VehicleOf( ent ) && RP_VehicleOf( ent ) == RP_VehicleOf( hit ) )
		{
			continue;
		}

		// zyk: if the player is in duel tournament and target is also in it, and they will fight now, do not telefrag
		if (level.duel_tournament_mode > 1 && duel_tournament_is_duelist(ent) == qtrue && duel_tournament_is_duelist(hit) == qtrue)
		{
			continue;
		}

		// GalaxyRP fix: [Telefrag] either side passing through bodies: nothing to resolve
		if ( RP_PhasePassesThrough( ent ) || RP_PhasePassesThrough( hit ) )
		{
			continue;
		}

		// GalaxyRP fix: [Telefrag] an NPC does not telefrag a player; it walks out of them
		if ( ent->s.eType == ET_NPC && hit->s.number < MAX_CLIENTS )
		{
			if ( hit->health > 0 && (hit->r.contents & CONTENTS_BODY) )
			{
				release = qtrue;
			}
			continue;
		}

		// nail it
		G_Damage ( hit, ent, ent, NULL, NULL,
			100000, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);

		// GalaxyRP fix: [Telefrag] still a live, solid body: the damage was turned away (ally, chat
		// protection, duel, arena, noclip, invulnerable NPC...), so walk out of it instead
		if ( hit->inuse && hit->client && hit->health > 0 && (hit->r.contents & CONTENTS_BODY) )
		{
			release = qtrue;
		}
	}

	if ( release )
	{
		RP_StartOverlapRelease( ent );
	}
}

/*
=================
G_KillBoxWouldHit

GLX_U: [Lua] whether G_KillBox() would find a live body to telefrag (or walk out of) with ent standing at
origin, by the same rules: itself, its vehicle and everyone aboard it, two duelists about to fight, and
bodies passing through bodies are left alone. A script's teleport is refused rather than kill anyone
(rp_lua_people.c). A corpse does not count: G_KillBox() only gibs it.
=================
*/
qboolean G_KillBoxWouldHit( gentity_t *ent, const vec3_t origin ) {
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	vec3_t		mins, maxs;

	VectorAdd( origin, ent->r.mins, mins );
	VectorAdd( origin, ent->r.maxs, maxs );
	num = trap->EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	for ( i = 0; i < num; i++ ) {
		hit = &g_entities[touch[i]];
		if ( !hit->inuse || !hit->client || hit->s.number == ent->s.number || ent->r.ownerNum == hit->s.number ) {
			continue;
		}
		if ( RP_VehicleOf( ent ) && RP_VehicleOf( ent ) == RP_VehicleOf( hit ) ) {
			continue;
		}
		if ( level.duel_tournament_mode > 1 && duel_tournament_is_duelist( ent ) == qtrue && duel_tournament_is_duelist( hit ) == qtrue ) {
			continue;
		}
		if ( RP_PhasePassesThrough( ent ) || RP_PhasePassesThrough( hit ) ) {
			continue;
		}
		if ( hit->health <= 0 ) {
			continue;
		}
		return qtrue;
	}
	return qfalse;
}

//==============================================================================

/*
===============
G_AddPredictableEvent

Use for non-pmove events that would also be predicted on the
client side: jumppads and item pickups
Adds an event+parm and twiddles the event counter
===============
*/
void G_AddPredictableEvent( gentity_t *ent, int event, int eventParm ) {
	if ( !ent->client ) {
		return;
	}
	BG_AddPredictableEventToPlayerstate( event, eventParm, &ent->client->ps );
}


/*
===============
G_AddEvent

Adds an event+parm and twiddles the event counter
===============
*/
void G_AddEvent( gentity_t *ent, int event, int eventParm ) {
	int		bits;

	if ( !event ) {
		trap->Print( "G_AddEvent: zero event added for entity %i\n", ent->s.number );
		return;
	}

	// clients need to add the event in playerState_t instead of entityState_t
	if ( ent->client ) {
		bits = ent->client->ps.externalEvent & EV_EVENT_BITS;
		bits = ( bits + EV_EVENT_BIT1 ) & EV_EVENT_BITS;
		ent->client->ps.externalEvent = event | bits;
		ent->client->ps.externalEventParm = eventParm;
		ent->client->ps.externalEventTime = level.time;
	} else {
		bits = ent->s.event & EV_EVENT_BITS;
		bits = ( bits + EV_EVENT_BIT1 ) & EV_EVENT_BITS;
		ent->s.event = event | bits;
		ent->s.eventParm = eventParm;
	}
	ent->eventTime = level.time;
}

/*
=============
G_PlayEffect
=============
*/
gentity_t *G_PlayEffect(int fxID, vec3_t org, vec3_t ang)
{
	gentity_t	*te;

	te = G_TempEntity( org, EV_PLAY_EFFECT );
	VectorCopy(ang, te->s.angles);
	VectorCopy(org, te->s.origin);
	te->s.eventParm = fxID;

	return te;
}

/*
=============
G_PlayEffectID
=============
*/
gentity_t *G_PlayEffectID(const int fxID, vec3_t org, vec3_t ang)
{ //play an effect by the G_EffectIndex'd ID instead of a predefined effect ID
	gentity_t	*te;

	te = G_TempEntity( org, EV_PLAY_EFFECT_ID );
	VectorCopy(ang, te->s.angles);
	VectorCopy(org, te->s.origin);
	te->s.eventParm = fxID;

	if (!te->s.angles[0] &&
		!te->s.angles[1] &&
		!te->s.angles[2])
	{ //play off this dir by default then.
		te->s.angles[1] = 1;
	}

	return te;
}

/*
=============
G_ScreenShake
=============
*/
gentity_t *G_ScreenShake(vec3_t org, gentity_t *target, float intensity, int duration, qboolean global)
{
	gentity_t	*te;

	te = G_TempEntity( org, EV_SCREENSHAKE );
	VectorCopy(org, te->s.origin);
	te->s.angles[0] = intensity;
	te->s.time = duration;

	if (target)
	{
		te->s.modelindex = target->s.number+1;
	}
	else
	{
		te->s.modelindex = 0;
	}

	if (global)
	{
		te->r.svFlags |= SVF_BROADCAST;
	}

	return te;
}

/*
=============
G_MuteSound
=============
*/
void G_MuteSound( int entnum, int channel )
{
	gentity_t	*te, *e;

	te = G_TempEntity( vec3_origin, EV_MUTE_SOUND );
	te->r.svFlags = SVF_BROADCAST;
	te->s.trickedentindex2 = entnum;
	te->s.trickedentindex = channel;

	e = &g_entities[entnum];

	if (e && (e->s.eFlags & EF_SOUNDTRACKER))
	{
		G_FreeEntity(e);
		e->s.eFlags = 0;
	}
}

/*
=============
G_Sound
=============
*/
void G_Sound( gentity_t *ent, int channel, int soundIndex ) {
	gentity_t	*te;

	assert(soundIndex);

	te = G_SoundTempEntity( ent->r.currentOrigin, EV_GENERAL_SOUND, channel );
	te->s.eventParm = soundIndex;
	te->s.saberEntityNum = channel;

	if (ent && ent->client && channel > TRACK_CHANNEL_NONE)
	{ //let the client remember the index of the player entity so he can kill the most recent sound on request
		if (g_entities[ent->client->ps.fd.killSoundEntIndex[channel-50]].inuse &&
			ent->client->ps.fd.killSoundEntIndex[channel-50] > MAX_CLIENTS)
		{
			G_MuteSound(ent->client->ps.fd.killSoundEntIndex[channel-50], CHAN_VOICE);
			if (ent->client->ps.fd.killSoundEntIndex[channel-50] > MAX_CLIENTS && g_entities[ent->client->ps.fd.killSoundEntIndex[channel-50]].inuse)
			{
				G_FreeEntity(&g_entities[ent->client->ps.fd.killSoundEntIndex[channel-50]]);
			}
			ent->client->ps.fd.killSoundEntIndex[channel-50] = 0;
		}

		ent->client->ps.fd.killSoundEntIndex[channel-50] = te->s.number;
		te->s.trickedentindex = ent->s.number;
		te->s.eFlags = EF_SOUNDTRACKER;
		// fix: let other players know about this
		// for case that they will meet this one
		te->r.svFlags |= SVF_BROADCAST;
		//te->freeAfterEvent = qfalse;
	}
}

/*
=============
G_SoundAtLoc
=============
*/
void G_SoundAtLoc( vec3_t loc, int channel, int soundIndex ) {
	gentity_t	*te;

	te = G_TempEntity( loc, EV_GENERAL_SOUND );
	te->s.eventParm = soundIndex;
	te->s.saberEntityNum = channel;
}

/*
=============
G_EntitySound
=============
*/
void G_EntitySound( gentity_t *ent, int channel, int soundIndex ) {
	gentity_t	*te;

	te = G_TempEntity( ent->r.currentOrigin, EV_ENTITY_SOUND );
	te->s.eventParm = soundIndex;
	te->s.clientNum = ent->s.number;
	te->s.trickedentindex = channel;
}

//To make porting from SP easier.
void G_SoundOnEnt( gentity_t *ent, int channel, const char *soundPath )
{
	gentity_t	*te;

	te = G_TempEntity( ent->r.currentOrigin, EV_ENTITY_SOUND );
	te->s.eventParm = G_SoundIndex((char *)soundPath);
	te->s.clientNum = ent->s.number;
	te->s.trickedentindex = channel;

}

//==============================================================================

/*
==============
ValidUseTarget

Returns whether or not the targeted entity is useable
==============
*/
qboolean ValidUseTarget( gentity_t *ent )
{
	if ( !ent->use )
	{
		return qfalse;
	}

	if ( ent->flags & FL_INACTIVE )
	{//set by target_deactivate
		return qfalse;
	}

	if ( !(ent->r.svFlags & SVF_PLAYER_USABLE) )
	{//Check for flag that denotes BUTTON_USE useability
		return qfalse;
	}

	return qtrue;
}

//use an ammo/health dispenser on another client
void G_UseDispenserOn(gentity_t *ent, int dispType, gentity_t *target)
{
	if (dispType == HI_HEALTHDISP)
	{
		target->client->ps.stats[STAT_HEALTH] += 4;

		if (target->client->ps.stats[STAT_HEALTH] > target->client->ps.stats[STAT_MAX_HEALTH])
		{
			target->client->ps.stats[STAT_HEALTH] = target->client->ps.stats[STAT_MAX_HEALTH];
		}

		target->client->isMedHealed = level.time + 500;
		target->health = target->client->ps.stats[STAT_HEALTH];
	}
	else if (dispType == HI_AMMODISP)
	{
		if (ent->client->medSupplyDebounce < level.time)
		{ //do the next increment
			//increment based on the amount of ammo used per normal shot.
			target->client->ps.ammo[weaponData[target->client->ps.weapon].ammoIndex] += weaponData[target->client->ps.weapon].energyPerShot;

			if (target->client->ps.ammo[weaponData[target->client->ps.weapon].ammoIndex] > ammoData[weaponData[target->client->ps.weapon].ammoIndex].max)
			{ //cap it off
				target->client->ps.ammo[weaponData[target->client->ps.weapon].ammoIndex] = ammoData[weaponData[target->client->ps.weapon].ammoIndex].max;
			}

			//base the next supply time on how long the weapon takes to fire. Seems fair enough.
			ent->client->medSupplyDebounce = level.time + weaponData[target->client->ps.weapon].fireTime;
		}
		target->client->isMedSupplied = level.time + 500;
	}
}

//see if this guy needs servicing from a specific type of dispenser
int G_CanUseDispOn(gentity_t *ent, int dispType)
{
	if (!ent->client || !ent->inuse || ent->health < 1 ||
		ent->client->ps.stats[STAT_HEALTH] < 1)
	{ //dead or invalid
		return 0;
	}

	if (dispType == HI_HEALTHDISP)
	{
        if (ent->client->ps.stats[STAT_HEALTH] < ent->client->ps.stats[STAT_MAX_HEALTH])
		{ //he's hurt
			return 1;
		}

		//otherwise no
		return 0;
	}
	else if (dispType == HI_AMMODISP)
	{
		if (ent->client->ps.weapon <= WP_NONE || ent->client->ps.weapon > LAST_USEABLE_WEAPON)
		{ //not a player-useable weapon
			return 0;
		}

		if (ent->client->ps.ammo[weaponData[ent->client->ps.weapon].ammoIndex] < ammoData[weaponData[ent->client->ps.weapon].ammoIndex].max)
		{ //needs more ammo for current weapon
			return 1;
		}

		//needs none
		return 0;
	}

	//invalid type?
	return 0;
}

qboolean TryHeal(gentity_t *ent, gentity_t *target)
{
	if (level.gametype == GT_SIEGE && ent->client->siegeClass != -1 &&
		target && target->inuse && target->maxHealth && target->healingclass &&
		target->healingclass[0] && target->health > 0 && target->health < target->maxHealth)
	{ //it's not dead yet...
		siegeClass_t *scl = &bgSiegeClasses[ent->client->siegeClass];

		if (!Q_stricmp(scl->name, target->healingclass))
		{ //this thing can be healed by the class this player is using
			if (target->healingDebounce < level.time)
			{ //do the actual heal
				target->health += 10;
				if (target->health > target->maxHealth)
				{ //don't go too high
					target->health = target->maxHealth;
				}
				target->healingDebounce = level.time + target->healingrate;
				if (target->healingsound && target->healingsound[0])
				{ //play it
					if (target->s.solid == SOLID_BMODEL)
					{ //ok, well, just play it on the client then.
						G_Sound(ent, CHAN_AUTO, G_SoundIndex(target->healingsound));
					}
					else
					{
						G_Sound(target, CHAN_AUTO, G_SoundIndex(target->healingsound));
					}
				}

				//update net health for bar
				G_ScaleNetHealth(target);
				if (target->target_ent &&
					target->target_ent->maxHealth)
				{
					target->target_ent->health = target->health;
					G_ScaleNetHealth(target->target_ent);
				}
			}

			//keep them in the healing anim even when the healing debounce is not yet expired
			if (ent->client->ps.torsoAnim == BOTH_BUTTON_HOLD ||
				ent->client->ps.torsoAnim == BOTH_CONSOLE1)
			{ //extend the time
				ent->client->ps.torsoTimer = 500;
			}
			else
			{
				G_SetAnim( ent, NULL, SETANIM_TORSO, BOTH_BUTTON_HOLD, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0 );
			}

			return qtrue;
		}
	}

	return qfalse;
}

/*
==============
TryUse

Try and use an entity in the world, directly ahead of us
==============
*/

#define USE_DISTANCE	64.0f

extern void Touch_Button(gentity_t *ent, gentity_t *other, trace_t *trace );
extern qboolean gSiegeRoundBegun;
extern void NPC_BSDefault( void );
static vec3_t	playerMins = {-15, -15, DEFAULT_MINS_2};
static vec3_t	playerMaxs = {15, 15, DEFAULT_MAXS_2};
void TryUse( gentity_t *ent )
{
	gentity_t	*target;
	trace_t		trace;
	vec3_t		src, dest, vf;
	vec3_t		viewspot;

	if (level.gametype == GT_SIEGE &&
		!gSiegeRoundBegun)
	{ //nothing can be used til the round starts.
		return;
	}

	if (!ent || !ent->client || (ent->client->ps.weaponTime > 0 && ent->client->ps.torsoAnim != BOTH_BUTTON_HOLD && ent->client->ps.torsoAnim != BOTH_CONSOLE1) || ent->health < 1 ||
		(ent->client->ps.pm_flags & PMF_FOLLOW) || ent->client->sess.sessionTeam == TEAM_SPECTATOR || ent->client->tempSpectate >= level.time ||
		(ent->client->ps.forceHandExtend != HANDEXTEND_NONE && ent->client->ps.forceHandExtend != HANDEXTEND_DRAGGING))
	{
		return;
	}

	if (ent->client->ps.emplacedIndex)
	{ //on an emplaced gun or using a vehicle, don't do anything when hitting use key
		return;
	}

	if (ent->s.number < MAX_CLIENTS && ent->client && ent->client->ps.m_iVehicleNum)
	{
		gentity_t *currentVeh = &g_entities[ent->client->ps.m_iVehicleNum];
		if (currentVeh->inuse && currentVeh->m_pVehicle)
		{
			Vehicle_t *pVeh = currentVeh->m_pVehicle;
			if (!pVeh->m_iBoarding)
			{
				pVeh->m_pVehicleInfo->Eject( pVeh, (bgEntity_t *)ent, qfalse );
			}
			return;
		}
	}

	if (ent->client->jetPackOn)
	{ //can't use anything else to jp is off
		goto tryJetPack;
	}

	if (ent->client->bodyGrabIndex != ENTITYNUM_NONE)
	{ //then hitting the use key just means let go
		if (ent->client->bodyGrabTime < level.time)
		{
			gentity_t *grabbed = &g_entities[ent->client->bodyGrabIndex];

			if (grabbed->inuse)
			{
				if (grabbed->client)
				{
					grabbed->client->ps.ragAttach = 0;
				}
				else
				{
					grabbed->s.ragAttach = 0;
				}
			}
			ent->client->bodyGrabIndex = ENTITYNUM_NONE;
			ent->client->bodyGrabTime = level.time + 1000;
		}
		return;
	}

	VectorCopy(ent->client->ps.origin, viewspot);
	viewspot[2] += ent->client->ps.viewheight;

	VectorCopy( viewspot, src );
	AngleVectors( ent->client->ps.viewangles, vf, NULL, NULL );

	VectorMA( src, USE_DISTANCE, vf, dest );

	//Trace ahead to find a valid target
	trap->Trace( &trace, src, vec3_origin, vec3_origin, dest, ent->s.number, MASK_OPAQUE|CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_ITEM|CONTENTS_CORPSE, qfalse, 0, 0 );

	if ( trace.fraction == 1.0f || trace.entityNum == ENTITYNUM_NONE )
	{
		goto tryJetPack;
	}

	target = &g_entities[trace.entityNum];

//Enable for corpse dragging
#if 0
	if (target->inuse && target->s.eType == ET_BODY &&
		ent->client->bodyGrabTime < level.time)
	{ //then grab the body
		target->s.eFlags |= EF_RAG; //make sure it's in rag state
		if (!ent->s.number)
		{ //switch cl 0 and entitynum_none, so we can operate on the "if non-0" concept
			target->s.ragAttach = ENTITYNUM_NONE;
		}
		else
		{
			target->s.ragAttach = ent->s.number;
		}
		ent->client->bodyGrabTime = level.time + 1000;
		ent->client->bodyGrabIndex = target->s.number;
		return;
	}
#endif

	if (target && target->m_pVehicle && target->client &&
		target->s.NPC_class == CLASS_VEHICLE &&
		!ent->client->ps.zoomMode)
	{ //if target is a vehicle then perform appropriate checks
		Vehicle_t *pVeh = target->m_pVehicle;

		if (pVeh->m_pVehicleInfo)
		{
			if ( ent->r.ownerNum == target->s.number )
			{ //user is already on this vehicle so eject him
				pVeh->m_pVehicleInfo->Eject( pVeh, (bgEntity_t *)ent, qfalse );
			}
			else
			{ // Otherwise board this vehicle.
				if (level.gametype < GT_TEAM ||
					!target->alliedTeam ||
					(target->alliedTeam == ent->client->sess.sessionTeam))
				{ //not belonging to a team, or client is on same team
					pVeh->m_pVehicleInfo->Board( pVeh, (bgEntity_t *)ent );
				}
			}
			//clear the damn button!
			ent->client->pers.cmd.buttons &= ~BUTTON_USE;
			return;
		}
	}

	// GalaxyRP: [Duel Tournament] the Duel Tournament 2v2 ally-forming block used to sit here -- press
	// Use on another signed-up player during signup (mode 1) to request them as a team-mate, press
	// again to drop them. It was the ONLY writer of level.duel_allies[], and it has been removed along
	// with the zyk_duel_tournament_allow_teams cvar that gated it, so tournaments are now always 1v1.
	//
	// It had been unreachable for logged-in players for years in any case: the condition required
	// sess.amrpgmode < 2 on BOTH players, but amrpgmode only ever holds 0 (not logged in) or 2 (logged
	// in) -- the value 1 is long dead, see the "always 2, kept for backwards compatibility" note in
	// g_cmds.c. So on any server where people log in, the cvar advertised a feature that could never
	// actually be used, and only two logged-OUT players could ever team up.
	//
	// The branches that read it have since been unpicked too: level.duel_allies[], the two
	// duelist_N_ally_id slots and the three duel_leaderboard_ally_* fields are gone from
	// level_locals_t, and with them the ally handling in the tournament state machine (g_main.c)
	// and the " / ally" columns in /dueltable and /duelmatches (g_cmds.c). Removing them changed
	// no behaviour: every one of those branches tested a value that was permanently -1.

	// GalaxyRP fix: [Quests] the jawa_seller Use branch used to be here -- pressing Use on a
	// jawa_seller NPC printed a hint to type /stuff and played a taunt animation. It was the only
	// code anywhere that needed that NPC to exist, and the NPC itself has gone with the three
	// zyk_quest_*.npc asset files. /buy and /stuff are plain command rows with no NPC dependency of
	// any kind, so the shop is unaffected; the two "Jawa Seller:" lines Cmd_Buy_f prints are chat
	// text with no entity behind them and are deliberately kept.

	// GalaxyRP fix: [RPG Class] Bounty Hunter Upgrade sentry gun recovery removed — rpg_class is permanently 0
	// GalaxyRP fix: [RPG Class] Bounty Hunter Upgrade force field recovery removed — rpg_class is permanently 0

	// GalaxyRP fix: [Quests] a large `else if (can_play_quest == 1)` branch used to live here, handling
	// Universe Quest NPC-dialogue/puzzle interactions (quest crystal puzzles, sage dialogue, etc.).
	// can_play_quest can no longer become 1 anywhere in the codebase now that quest_get_new_player,
	// its sole setter, has been deleted as unreachable dead code (see the GalaxyRP fix comment on its
	// old location in g_cmds.c), so this entire branch was dead and has been removed, along with the
	// got_all_amulets/universe_quest_artifacts_checker calls and quest_puzzle_order reads it was the
	// only user of.

	// GalaxyRP fix: [NPC] a dead NPC could be claimed as a follower -- there was no health test
	// here, so pressing Use on a corpse set leader and BS_FOLLOW_LEADER on it. Taken from the
	// upstream Zyk mod, which carries target->health > 0 in this same condition; this fork
	// predates that change.
	// GLX_U: [Lua] a script NPC with an on_use is not taken as a follower here: its on_use, below, decides what the
	// Use key does (rp_lua_world.c)
	if (target->NPC && target->client && target->health > 0 && target->s.NPC_class != CLASS_VEHICLE && OnSameTeam(ent,target)
		&& !RP_Lua_NpcKeepsUse(target))
	{
		// DAJ_RP: [NPC System] a frozen NPC is not claimed (see RP_NpcFreeze(), NPC_spawn.c); its leader can
		// still dismiss it below
		if (!target->client->leader && !RP_NpcIsFrozen(target))
		{ // zyk: setting the npc leader so he follows the player
			RP_Lua_NpcOrderTaken(target);	// GLX_U: [Lua] a script's order on it ends ("replaced", rp_lua_orders.c)
			target->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_NPC_ORDER_GUARD);
			target->client->pers.player_statuses &= ~(1 << PLAYER_STATUS_NPC_ORDER_COVER);
			target->client->leader = ent;
			target->NPC->tempBehavior = BS_FOLLOW_LEADER;
		}
		else if (target->client->leader == ent)
		{ // zyk: npc will stop follow the player, which is the leader
			RP_Lua_NpcOrderTaken(target);	// GLX_U: [Lua] a script's order on it ends ("replaced", rp_lua_orders.c)
			// GalaxyRP fix: [NPC] this used to open-code the release and left NPC->goalEntity still
			// pointing at the player. NPC_BSFollowLeader() parks the leader there while the NPC is
			// following (both its close-in and back-off branches do, and at Use range the NPC is
			// always in one of them), UpdateGoal() only rejects a goal whose entity is not inuse, and
			// ReachedGoal() clears it only on an actual touch rather than on proximity -- so a
			// dismissed NPC kept walking at the player until it bumped into them, and chased them if
			// they moved off. zyk_release_npc_from_leader() clears the goal along with the leader and
			// the order bits; it is otherwise identical here, since the vehicle test it makes on
			// tempBehavior is already guaranteed by the outer condition.
			zyk_release_npc_from_leader(target);
		}
	}

	extern void help_up(gentity_t* ent, gentity_t* target);

	//GalaxyRP (Alex): [Death System] If the target player is downed, help them up.
	if (!target->NPC && target->client) {
		help_up(ent, target);
	}

#if 0 //ye olde method
	if (ent->client->ps.stats[STAT_HOLDABLE_ITEM] > 0 &&
		bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giType == IT_HOLDABLE)
	{
		if (bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giTag == HI_HEALTHDISP ||
			bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giTag == HI_AMMODISP)
		{ //has a dispenser item selected
            if (target && target->client && target->health > 0 && OnSameTeam(ent, target) &&
				G_CanUseDispOn(target, bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giTag))
			{ //a live target that's on my team, we can use him
				G_UseDispenserOn(ent, bg_itemlist[ent->client->ps.stats[STAT_HOLDABLE_ITEM]].giTag, target);

				//for now, we will use the standard use anim
				if (ent->client->ps.torsoAnim == BOTH_BUTTON_HOLD)
				{ //extend the time
					ent->client->ps.torsoTimer = 500;
				}
				else
				{
					G_SetAnim( ent, NULL, SETANIM_TORSO, BOTH_BUTTON_HOLD, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0 );
				}
				ent->client->ps.weaponTime = ent->client->ps.torsoTimer;
				return;
			}
		}
	}
#else
    if ( ((ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_HEALTHDISP)) || (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_AMMODISP))) &&
		target && target->inuse && target->client && target->health > 0 && OnSameTeam(ent, target) &&
		(G_CanUseDispOn(target, HI_HEALTHDISP) || G_CanUseDispOn(target, HI_AMMODISP)) )
	{ //a live target that's on my team, we can use him
		if (G_CanUseDispOn(target, HI_HEALTHDISP))
		{
			G_UseDispenserOn(ent, HI_HEALTHDISP, target);
		}
		if (G_CanUseDispOn(target, HI_AMMODISP))
		{
			G_UseDispenserOn(ent, HI_AMMODISP, target);
		}

		//for now, we will use the standard use anim
		if (ent->client->ps.torsoAnim == BOTH_BUTTON_HOLD)
		{ //extend the time
			ent->client->ps.torsoTimer = 500;
		}
		else
		{
			G_SetAnim( ent, NULL, SETANIM_TORSO, BOTH_BUTTON_HOLD, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0 );
		}
		ent->client->ps.weaponTime = ent->client->ps.torsoTimer;
		return;
	}

#endif

	//Check for a use command
	if ( ValidUseTarget( target )
		&& (level.gametype != GT_SIEGE
			|| !target->alliedTeam
			|| target->alliedTeam != ent->client->sess.sessionTeam
			|| g_ff_objectives.integer) )
	{
		if (ent->client->ps.torsoAnim == BOTH_BUTTON_HOLD ||
			ent->client->ps.torsoAnim == BOTH_CONSOLE1)
		{ //extend the time
			ent->client->ps.torsoTimer = 500;
		}
		else
		{
			G_SetAnim( ent, NULL, SETANIM_TORSO, BOTH_BUTTON_HOLD, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0 );
		}
		ent->client->ps.weaponTime = ent->client->ps.torsoTimer;
		/*
		NPC_SetAnim( ent, SETANIM_TORSO, BOTH_FORCEPUSH, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD );
		if ( !VectorLengthSquared( ent->client->ps.velocity ) )
		{
			NPC_SetAnim( ent, SETANIM_LEGS, BOTH_FORCEPUSH, SETANIM_FLAG_NORMAL|SETANIM_FLAG_HOLD );
		}
		*/
		if ( target->touch == Touch_Button )
		{//pretend we touched it
			target->touch(target, ent, NULL);
		}
		else
		{
			GlobalUse(target, ent, ent);
		}
		return;
	}

	if (TryHeal(ent, target))
	{
		return;
	}

tryJetPack:
	//if we got here, we didn't actually use anything else, so try to toggle jetpack if we are in the air, or if it is already on
	if (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_JETPACK))
	{
		if (ent->client->jetPackOn || ent->client->ps.groundEntityNum == ENTITYNUM_NONE)
		{
			ItemUse_Jetpack(ent);
			return;
		}
	}

	if ( (ent->client->ps.stats[STAT_HOLDABLE_ITEMS] & (1 << HI_AMMODISP)) /*&&
		G_ItemUsable(&ent->client->ps, HI_AMMODISP)*/ )
	{ //if you used nothing, then try spewing out some ammo
		trace_t trToss;
		vec3_t fAng;
		vec3_t fwd;

		VectorSet(fAng, 0.0f, ent->client->ps.viewangles[YAW], 0.0f);
		AngleVectors(fAng, fwd, 0, 0);

        VectorMA(ent->client->ps.origin, 64.0f, fwd, fwd);
		trap->Trace(&trToss, ent->client->ps.origin, playerMins, playerMaxs, fwd, ent->s.number, ent->clipmask, qfalse, 0, 0);
		if (trToss.fraction == 1.0f && !trToss.allsolid && !trToss.startsolid)
		{
			ItemUse_UseDisp(ent, HI_AMMODISP);
			G_AddEvent(ent, EV_USE_ITEM0+HI_AMMODISP, 0);
			return;
		}
	}
}

// GalaxyRP: [Use hint] the trigger half of the answer, and the half originally missed.
//
// A func_door with a targetname gets no automatic touch-trigger (see the spawnflag test in
// g_mover.c's door setup), so it is opened by something that targets it -- very often a
// trigger_multiple or trigger_once carrying the USE_BUTTON spawnflag (4). Touch_Multi (g_trigger.c)
// is what handles those: stand inside the trigger's volume, press Use, and it fires its targets. The
// door itself never needs SVF_PLAYER_USABLE, which is why looking straight at one and testing
// ValidUseTarget() reports "not usable" for a door that visibly opens when you press Use.
//
// Single player covered exactly this case with CanUseInfrontOfPartOfLevel(), a box check run when the
// forward trace found nothing. This is the multiplayer equivalent, built from MP's own two sources of
// truth rather than ported: entity discovery mirrors G_TouchTriggers() (g_active.c) -- the same
// {40,40,52} query box, the same CONTENTS_TRIGGER filter, the same player-bounds EntityContact test --
// and the accept/reject conditions follow Touch_Multi() itself, minus the BUTTON_USE press, which is
// precisely the thing the hint exists to tell you to do.
//
// Not a complete mirror, deliberately: FIRE_BUTTON (spawnflag 8), the post-fire wait window, the
// non-Siege genericValue1 veto and the Siege idealclass list are all left unchecked. Every one of
// those can only ever cause a FALSE POSITIVE -- the hand shows where pressing Use happens to do
// nothing -- never the reverse, so none of them can hide a genuinely usable trigger. Single player's
// own equivalent has the same character. Adding them would mean duplicating four more pieces of
// trigger state here purely to suppress the occasional spurious icon.
//
// DAJ_RP: [Use hint] and a zyk_mini_game_joiner that waits for the Use key (spawnflag 64): its own think
// (zyk_mini_game_joiner_think, g_misc.c) lists what overlaps its box every 100 ms and joins a living player
// holding Use to the Melee Battle (4) or the Duel Tournament (8) -- not through TryUse, nor as a trigger, so
// neither half above saw it and the hand stayed dark inside one. RP_UseJoinerHere() asks what the think asks.
extern void Touch_Multi( gentity_t *self, gentity_t *other, trace_t *trace );
extern void zyk_mini_game_joiner_think( gentity_t *ent );
qboolean G_PointInBounds( vec3_t point, vec3_t mins, vec3_t maxs );

// DAJ_RP: [Use hint] whether hit is a mini-game joiner that pressing Use in would join ent to a mini-game, by the
// joiner's own test: Use needed (64) and a mini-game to join (4 or 8); ent a living player, not an NPC (the think
// takes client numbers under MAX_CLIENTS); its box -- origin plus mins and maxs -- overlapping the box ent is linked
// with, the one the engine's EntitiesInBox() matches the think's query against. Whether that mini-game takes ent is
// left to its command, which says why not, as every other false positive here is.
static qboolean RP_UseJoinerHere( gentity_t *ent, gentity_t *hit )
{
	int k;

	if ( !hit->inuse || hit->think != zyk_mini_game_joiner_think )
		return qfalse;
	if ( !( hit->spawnflags & 64 ) || !( hit->spawnflags & ( 4 | 8 ) ) )
		return qfalse;
	if ( !ent->client || ent->s.number >= MAX_CLIENTS || ent->health <= 0 )
		return qfalse;

	for ( k = 0; k < 3; k++ )
	{
		if ( ent->r.absmin[k] > hit->s.origin[k] + hit->r.maxs[k] || ent->r.absmax[k] < hit->s.origin[k] + hit->r.mins[k] )
			return qfalse;
	}
	return qtrue;
}

static qboolean G_UsableTriggerInRange( gentity_t *ent )
{
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	vec3_t		mins, maxs;
	vec3_t		forward;
	static vec3_t	range = { 40, 40, 52 };

	if ( !ent->client )
	{
		return qfalse;
	}

	VectorSubtract( ent->client->ps.origin, range, mins );
	VectorAdd( ent->client->ps.origin, range, maxs );

	num = trap->EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	// zyk: same comment as G_TouchTriggers -- can't use r.absmin, it carries a one unit pad
	VectorAdd( ent->client->ps.origin, ent->r.mins, mins );
	VectorAdd( ent->client->ps.origin, ent->r.maxs, maxs );

	AngleVectors( ent->client->ps.viewangles, forward, NULL, NULL );

	for ( i = 0; i < num; i++ )
	{
		hit = &g_entities[touch[i]];

		if ( RP_UseJoinerHere( ent, hit ) )
		{ // DAJ_RP: [Use hint] a mini-game joiner waiting for the Use key, and ent inside it
			return qtrue;
		}

		if ( !hit->inuse || hit->touch != Touch_Multi )
		{ // zyk: only trigger_multiple / trigger_once carry the USE_BUTTON behaviour
			continue;
		}

		if ( !(hit->r.contents & CONTENTS_TRIGGER) )
		{
			continue;
		}

		if ( !(hit->spawnflags & 4) )
		{ // zyk: not a USE_BUTTON trigger -- it fires on touch alone, no hint needed
			continue;
		}

		if ( hit->flags & FL_INACTIVE )
		{ // zyk: set by target_deactivate
			continue;
		}

		if ( hit->alliedTeam && ent->client->sess.sessionTeam != hit->alliedTeam )
		{
			continue;
		}

		if ( !(hit->spawnflags & 1) )
		{ // zyk: not CLIENTONLY, so Touch_Multi's NPC-only restrictions apply and we are not an NPC
			if ( hit->spawnflags & 16 )
			{ // NPCONLY
				continue;
			}

			if ( hit->NPC_targetname && hit->NPC_targetname[0] )
			{ // zyk: only a specifically named NPC may fire this one
				continue;
			}
		}

		if ( hit->spawnflags & 2 )
		{ // FACING -- must be within 45 degrees of the trigger's movedir
			if ( DotProduct( hit->movedir, forward ) < 0.5f )
			{
				continue;
			}
		}

		if ( hit->genericValue7 )
		{ // zyk: hold-to-use trigger; Touch_Multi additionally requires the origin itself to be inside
			if ( !G_PointInBounds( ent->client->ps.origin, hit->r.absmin, hit->r.absmax ) )
			{
				continue;
			}
		}

		if ( !trap->EntityContact( mins, maxs, (sharedEntity_t *)hit, qfalse ) )
		{
			continue;
		}

		return qtrue;
	}

	return qfalse;
}

/*
==============
G_CanUseInFrontOf

"Would pressing Use right now activate a usable world entity?" -- the predicate behind the
gfx/hud/useableHint hand icon (/settings 4). Read-only: it never touches any entity or player state.

GalaxyRP: [Use hint] Jedi Academy SINGLE PLAYER has this feature; multiplayer never did. SP's version
(CanUseInfrontOf, code/game/g_utils.cpp) cannot be ported -- its cgame reads server entities directly
through centity_t::gent, starts the trace from client->renderInfo.eyePoint (a server-only struct that
is never networked) and calls gi.trace from client render code. None of that exists across the MP
VM boundary, and nothing about usability appears in entityState_t or playerState_t. So the detection
runs here, on the server, and only its one-bit result is networked (ps.stats[STAT_USE_HINT]).

Scope: world geometry only, by two routes, because the Use key has two ways of reaching a door.
The first is the entity directly ahead being player-usable in its own right (ValidUseTarget, the
TryUse branch). The second is standing inside a USE_BUTTON trigger that fires it --
G_UsableTriggerInRange() above. The second route matters more than it looks: a func_door carrying a
targetname gets no touch-trigger of its own and is opened by whatever targets it, so most map doors
and lifts are reached that way and carry no SVF_PLAYER_USABLE at all. The first version of this
feature had only route one and stayed dark on exactly the doors players use most.

DAJ_RP: [Use hint] the trigger half also answers for a zyk_mini_game_joiner waiting for the Use key
(RP_UseJoinerHere()), which its own think reads the key for, outside TryUse.

Still NOT covered, deliberately: the Use key's other jobs -- vehicles, jetpacks, body dragging,
corpse dragging, the jawa seller, NPC follow orders, dispensers and helping up a downed player. None
of those involve SVF_PLAYER_USABLE or a trigger, and covering them means restructuring TryUse itself
into a query mode, which is a separate job.

This is a separate function rather than a query flag on TryUse precisely so TryUse -- live, heavily
used code -- is not restructured for a HUD hint. The cost of that choice is that the two can drift
apart, and they already did once: this function used to bail on ps.m_iVehicleNum alone while TryUse
only bails when the referenced vehicle actually exists. They are kept adjacent here for that reason.
If you change the guards, the trace or the ValidUseTarget gate in either one, change it in both.
==============
*/
qboolean G_CanUseInFrontOf( gentity_t *ent )
{
	gentity_t	*target;
	trace_t		trace;
	vec3_t		src, dest, vf;
	vec3_t		viewspot;

	if (level.gametype == GT_SIEGE &&
		!gSiegeRoundBegun)
	{ //nothing can be used til the round starts.
		return qfalse;
	}

	// zyk: same eligibility guards TryUse applies before it traces
	if (!ent || !ent->client || (ent->client->ps.weaponTime > 0 && ent->client->ps.torsoAnim != BOTH_BUTTON_HOLD && ent->client->ps.torsoAnim != BOTH_CONSOLE1) || ent->health < 1 ||
		(ent->client->ps.pm_flags & PMF_FOLLOW) || ent->client->sess.sessionTeam == TEAM_SPECTATOR || ent->client->tempSpectate >= level.time ||
		(ent->client->ps.forceHandExtend != HANDEXTEND_NONE && ent->client->ps.forceHandExtend != HANDEXTEND_DRAGGING))
	{
		return qfalse;
	}

	if (ent->client->ps.emplacedIndex)
	{ //on an emplaced gun, Use does nothing else
		return qfalse;
	}

	// GalaxyRP: [Use hint] the three states where TryUse consumes the Use key before it ever reaches
	// the trace below -- riding a vehicle (Use ejects), jetpack on (Use toggles it) and dragging a
	// body (Use lets go). Pressing Use in any of them does something, but never a world entity, so the
	// hand stays dark rather than pointing at scenery the key would not actually activate.
	if (ent->s.number < MAX_CLIENTS && ent->client->ps.m_iVehicleNum)
	{ // zyk: mirror TryUse exactly -- it only consumes the Use key (to eject) when the referenced
	  // vehicle is really there. A stale m_iVehicleNum pointing at a freed slot leaves Use working on
	  // world entities, so returning qfalse on m_iVehicleNum alone would darken the hint in a case
	  // where the key still works.
		gentity_t *currentVeh = &g_entities[ent->client->ps.m_iVehicleNum];

		if (currentVeh->inuse && currentVeh->m_pVehicle)
		{
			return qfalse;
		}
	}

	if (ent->client->jetPackOn)
	{
		return qfalse;
	}

	if (ent->client->bodyGrabIndex != ENTITYNUM_NONE)
	{
		return qfalse;
	}

	VectorCopy(ent->client->ps.origin, viewspot);
	viewspot[2] += ent->client->ps.viewheight;

	VectorCopy( viewspot, src );
	AngleVectors( ent->client->ps.viewangles, vf, NULL, NULL );

	VectorMA( src, USE_DISTANCE, vf, dest );

	//Trace ahead to find a valid target
	trap->Trace( &trace, src, vec3_origin, vec3_origin, dest, ent->s.number, MASK_OPAQUE|CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_ITEM|CONTENTS_CORPSE, qfalse, 0, 0 );

	if ( trace.fraction == 1.0f || trace.entityNum == ENTITYNUM_NONE )
	{ // zyk: nothing ahead -- fall back to the trigger check, exactly as SP does
		return G_UsableTriggerInRange( ent );
	}

	target = &g_entities[trace.entityNum];

	// zyk: no extra !target->inuse test here -- TryUse does not have one, and the whole point of this
	// function is that it decides exactly what TryUse decides.
	// zyk: the same gate TryUse uses to decide it has a usable world entity
	if ( ValidUseTarget( target )
		&& (level.gametype != GT_SIEGE
			|| !target->alliedTeam
			|| target->alliedTeam != ent->client->sess.sessionTeam
			|| g_ff_objectives.integer) )
	{
		return qtrue;
	}

	// zyk: the thing we are looking at is not itself player-usable, but we may still be standing in a
	// USE_BUTTON trigger that opens it -- which is how targetname'd doors and lifts actually work.
	return G_UsableTriggerInRange( ent );
}

qboolean G_PointInBounds( vec3_t point, vec3_t mins, vec3_t maxs )
{
	int i;

	for(i = 0; i < 3; i++ )
	{
		if ( point[i] < mins[i] )
		{
			return qfalse;
		}
		if ( point[i] > maxs[i] )
		{
			return qfalse;
		}
	}

	return qtrue;
}

qboolean G_BoxInBounds( vec3_t point, vec3_t mins, vec3_t maxs, vec3_t boundsMins, vec3_t boundsMaxs )
{
	vec3_t boxMins;
	vec3_t boxMaxs;

	VectorAdd( point, mins, boxMins );
	VectorAdd( point, maxs, boxMaxs );

	if(boxMaxs[0]>boundsMaxs[0])
		return qfalse;

	if(boxMaxs[1]>boundsMaxs[1])
		return qfalse;

	if(boxMaxs[2]>boundsMaxs[2])
		return qfalse;

	if(boxMins[0]<boundsMins[0])
		return qfalse;

	if(boxMins[1]<boundsMins[1])
		return qfalse;

	if(boxMins[2]<boundsMins[2])
		return qfalse;

	//box is completely contained within bounds
	return qtrue;
}


void G_SetAngles( gentity_t *ent, vec3_t angles )
{
	VectorCopy( angles, ent->r.currentAngles );
	VectorCopy( angles, ent->s.angles );
	VectorCopy( angles, ent->s.apos.trBase );
}

qboolean G_ClearTrace( vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int ignore, int clipmask )
{
	static	trace_t	tr;

	trap->Trace( &tr, start, mins, maxs, end, ignore, clipmask, qfalse, 0, 0 );

	if ( tr.allsolid || tr.startsolid || tr.fraction < 1.0 )
	{
		return qfalse;
	}

	return qtrue;
}

/*
================
G_SetOrigin

Sets the pos trajectory for a fixed position
================
*/
void G_SetOrigin( gentity_t *ent, vec3_t origin ) {
	VectorCopy( origin, ent->s.pos.trBase );
	ent->s.pos.trType = TR_STATIONARY;
	ent->s.pos.trTime = 0;
	ent->s.pos.trDuration = 0;
	VectorClear( ent->s.pos.trDelta );

	VectorCopy( origin, ent->r.currentOrigin );

	// GalaxyRP: [NPC] a moved NPC's cached waypoints point at wherever it was (SP does this too)
	if ( ent->client && ent->NPC )
	{
		ent->waypoint = WAYPOINT_NONE;
		ent->lastWaypoint = WAYPOINT_NONE;
	}
}

qboolean G_CheckInSolid (gentity_t *self, qboolean fix)
{
	trace_t	trace;
	vec3_t	end, mins;

	VectorCopy(self->r.currentOrigin, end);
	end[2] += self->r.mins[2];
	VectorCopy(self->r.mins, mins);
	mins[2] = 0;

	trap->Trace(&trace, self->r.currentOrigin, mins, self->r.maxs, end, self->s.number, self->clipmask, qfalse, 0, 0);
	if(trace.allsolid || trace.startsolid)
	{
		return qtrue;
	}

	if(trace.fraction < 1.0)
	{
		if(fix)
		{//Put them at end of trace and check again
			vec3_t	neworg;

			VectorCopy(trace.endpos, neworg);
			neworg[2] -= self->r.mins[2];
			G_SetOrigin(self, neworg);
			// GalaxyRP: [Logical Entities] waypoints and point_combats reach this with "fix" set;
			// a logical one must not be linked (the engine does not know its number), and does
			// not need to be -- the trace below reads only the struct.
			if (!self->isLogical)
			{
				trap->LinkEntity((sharedEntity_t *)self);
			}

			return G_CheckInSolid(self, qfalse);
		}
		else
		{
			return qtrue;
		}
	}

	return qfalse;
}

/*
================
DebugLine

  debug polygons only work when running a local game
  with r_debugSurface set to 2
================
*/
int DebugLine(vec3_t start, vec3_t end, int color) {
	vec3_t points[4], dir, cross, up = {0, 0, 1};
	float dot;

	VectorCopy(start, points[0]);
	VectorCopy(start, points[1]);
	//points[1][2] -= 2;
	VectorCopy(end, points[2]);
	//points[2][2] -= 2;
	VectorCopy(end, points[3]);


	VectorSubtract(end, start, dir);
	VectorNormalize(dir);
	dot = DotProduct(dir, up);
	if (dot > 0.99 || dot < -0.99) VectorSet(cross, 1, 0, 0);
	else CrossProduct(dir, up, cross);

	VectorNormalize(cross);

	VectorMA(points[0], 2, cross, points[0]);
	VectorMA(points[1], -2, cross, points[1]);
	VectorMA(points[2], -2, cross, points[2]);
	VectorMA(points[3], 2, cross, points[3]);

	return trap->DebugPolygonCreate(color, 4, points);
}

void G_ROFF_NotetrackCallback( gentity_t *cent, const char *notetrack)
{
	char type[256];
	int i = 0;
	int addlArg = 0;

	if (!cent || !notetrack)
	{
		return;
	}

	while (notetrack[i] && notetrack[i] != ' ')
	{
		type[i] = notetrack[i];
		i++;
	}

	type[i] = '\0';

	if (!i || !type[0])
	{
		return;
	}

	if (notetrack[i] == ' ')
	{
		addlArg = 1;
	}

	if (strcmp(type, "loop") == 0)
	{
		if (addlArg) //including an additional argument means reset to original position before loop
		{
			VectorCopy(cent->s.origin2, cent->s.pos.trBase);
			VectorCopy(cent->s.origin2, cent->r.currentOrigin);
			VectorCopy(cent->s.angles2, cent->s.apos.trBase);
			VectorCopy(cent->s.angles2, cent->r.currentAngles);
		}

		trap->ROFF_Play(cent->s.number, cent->roffid, qfalse);
	}
}

void G_SpeechEvent( gentity_t *self, int event )
{
	G_AddEvent(self, event, 0);
}

qboolean G_ExpandPointToBBox( vec3_t point, const vec3_t mins, const vec3_t maxs, int ignore, int clipmask )
{
	trace_t	tr;
	vec3_t	start, end;
	int i;

	VectorCopy( point, start );

	for ( i = 0; i < 3; i++ )
	{
		VectorCopy( start, end );
		end[i] += mins[i];
		trap->Trace( &tr, start, vec3_origin, vec3_origin, end, ignore, clipmask, qfalse, 0, 0 );
		if ( tr.allsolid || tr.startsolid )
		{
			return qfalse;
		}
		if ( tr.fraction < 1.0 )
		{
			VectorCopy( start, end );
			end[i] += maxs[i]-(mins[i]*tr.fraction);
			trap->Trace( &tr, start, vec3_origin, vec3_origin, end, ignore, clipmask, qfalse, 0, 0 );
			if ( tr.allsolid || tr.startsolid )
			{
				return qfalse;
			}
			if ( tr.fraction < 1.0 )
			{
				return qfalse;
			}
			VectorCopy( end, start );
		}
	}
	//expanded it, now see if it's all clear
	trap->Trace( &tr, start, mins, maxs, start, ignore, clipmask, qfalse, 0, 0 );
	if ( tr.allsolid || tr.startsolid )
	{
		return qfalse;
	}
	VectorCopy( start, point );
	return qtrue;
}

extern qboolean G_FindClosestPointOnLineSegment( const vec3_t start, const vec3_t end, const vec3_t from, vec3_t result );
float ShortestLineSegBewteen2LineSegs( vec3_t start1, vec3_t end1, vec3_t start2, vec3_t end2, vec3_t close_pnt1, vec3_t close_pnt2 )
{
	float	current_dist, new_dist;
	vec3_t	new_pnt;
	//start1, end1 : the first segment
	//start2, end2 : the second segment

	//output, one point on each segment, the closest two points on the segments.

	//compute some temporaries:
	//vec start_dif = start2 - start1
	vec3_t	start_dif;
	vec3_t	v1;
	vec3_t	v2;
	float v1v1, v2v2, v1v2;
	float denom;

	VectorSubtract( start2, start1, start_dif );
	//vec v1 = end1 - start1
	VectorSubtract( end1, start1, v1 );
	//vec v2 = end2 - start2
	VectorSubtract( end2, start2, v2 );
	//
	v1v1 = DotProduct( v1, v1 );
	v2v2 = DotProduct( v2, v2 );
	v1v2 = DotProduct( v1, v2 );

	//the main computation

	denom = (v1v2 * v1v2) - (v1v1 * v2v2);

	//if denom is small, then skip all this and jump to the section marked below
	if ( fabs(denom) > 0.001f )
	{
		float s = -( (v2v2*DotProduct( v1, start_dif )) - (v1v2*DotProduct( v2, start_dif )) ) / denom;
		float t = ( (v1v1*DotProduct( v2, start_dif )) - (v1v2*DotProduct( v1, start_dif )) ) / denom;
		qboolean done = qtrue;

		if ( s < 0 )
		{
			done = qfalse;
			s = 0;// and see note below
		}

		if ( s > 1 )
		{
			done = qfalse;
			s = 1;// and see note below
		}

		if ( t < 0 )
		{
			done = qfalse;
			t = 0;// and see note below
		}

		if ( t > 1 )
		{
			done = qfalse;
			t = 1;// and see note below
		}

		//vec close_pnt1 = start1 + s * v1
		VectorMA( start1, s, v1, close_pnt1 );
		//vec close_pnt2 = start2 + t * v2
		VectorMA( start2, t, v2, close_pnt2 );

		current_dist = Distance( close_pnt1, close_pnt2 );
		//now, if none of those if's fired, you are done.
		if ( done )
		{
			return current_dist;
		}
		//If they did fire, then we need to do some additional tests.

		//What we are gonna do is see if we can find a shorter distance than the above
		//involving the endpoints.
	}
	else
	{
		//******start here for paralell lines with current_dist = infinity****
		current_dist = Q3_INFINITE;
	}

	//test 2 close_pnts first
	/*
	G_FindClosestPointOnLineSegment( start1, end1, close_pnt2, new_pnt );
	new_dist = Distance( close_pnt2, new_pnt );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( new_pnt, close_pnt1 );
		VectorCopy( close_pnt2, close_pnt2 );
		current_dist = new_dist;
	}

	G_FindClosestPointOnLineSegment( start2, end2, close_pnt1, new_pnt );
	new_dist = Distance( close_pnt1, new_pnt );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( close_pnt1, close_pnt1 );
		VectorCopy( new_pnt, close_pnt2 );
		current_dist = new_dist;
	}
	*/
	//test all the endpoints
	new_dist = Distance( start1, start2 );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( start1, close_pnt1 );
		VectorCopy( start2, close_pnt2 );
		current_dist = new_dist;
	}

	new_dist = Distance( start1, end2 );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( start1, close_pnt1 );
		VectorCopy( end2, close_pnt2 );
		current_dist = new_dist;
	}

	new_dist = Distance( end1, start2 );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( end1, close_pnt1 );
		VectorCopy( start2, close_pnt2 );
		current_dist = new_dist;
	}

	new_dist = Distance( end1, end2 );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( end1, close_pnt1 );
		VectorCopy( end2, close_pnt2 );
		current_dist = new_dist;
	}

	//Then we have 4 more point / segment tests

	G_FindClosestPointOnLineSegment( start2, end2, start1, new_pnt );
	new_dist = Distance( start1, new_pnt );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( start1, close_pnt1 );
		VectorCopy( new_pnt, close_pnt2 );
		current_dist = new_dist;
	}

	G_FindClosestPointOnLineSegment( start2, end2, end1, new_pnt );
	new_dist = Distance( end1, new_pnt );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( end1, close_pnt1 );
		VectorCopy( new_pnt, close_pnt2 );
		current_dist = new_dist;
	}

	G_FindClosestPointOnLineSegment( start1, end1, start2, new_pnt );
	new_dist = Distance( start2, new_pnt );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( new_pnt, close_pnt1 );
		VectorCopy( start2, close_pnt2 );
		current_dist = new_dist;
	}

	G_FindClosestPointOnLineSegment( start1, end1, end2, new_pnt );
	new_dist = Distance( end2, new_pnt );
	if ( new_dist < current_dist )
	{//then update close_pnt1 close_pnt2 and current_dist
		VectorCopy( new_pnt, close_pnt1 );
		VectorCopy( end2, close_pnt2 );
		current_dist = new_dist;
	}

	return current_dist;
}

// GalaxyRP fix: [Death System] is the downed system switched on at all?
//
// rp_downed_timer 0 turns it off: a lethal hit kills outright, the way it did before this system
// existed. G_Damage() (g_combat.c) is the one place that asks, and it asks as
// "RP_DownedSystemEnabled() || G_PlayerIsDowned(targ)" -- the second half is not about leftovers,
// because the cvar is CVAR_LATCH and cannot change mid-map. It is there for ADMIN PARALYSIS, which
// is deliberately independent of this cvar (its own RP_PARALYZE_MIN/MAX_SECONDS, its own release
// path) and can still put a player down while the gameplay system is off. Without that half, a
// lethal hit on a paralysed player would reach targ->die() without RP_ClearDownedState(), and
// player_die()'s early returns would leave the DOWNED and ADMIN_PARALYSIS status bits set on a corpse -- a player whom
// /paralyze afterwards refuses as "already paralyzed" and only /unparalyze can reset.
//
// Nothing else needs a test. With the system off nobody is ever downed, so /getup and /helpup
// refuse on their existing "you are not downed" checks, RP_EnterDownedState()'s invulnerability and
// FL_NOTARGET never run, and all forty G_PlayerIsDowned() call sites simply answer qfalse.
qboolean RP_DownedSystemEnabled( void )
{
	return (rp_downed_timer.integer > 0) ? qtrue : qfalse;
}

// GalaxyRP fix: [Death System] one place that answers "is this player currently downed?", so the
// rule stops being spelled out as a raw pers.player_statuses bit test at every new call site.
// Bit 6 is set in one place, RP_EnterDownedState() (g_cmds.c) -- reached both by paralyze_player()
// for a combat knockdown and by the admin /paralyze command -- and cleared in four:
// RP_ReleaseFromDownedState() (the complete counterpart, used by the countdown's auto-release and by
// /unparalyze), help_up() (/getup and /helpup), player_die(), and G_Damage()'s branch for finishing
// off a player who was already down. While the bit is set the player is lying incapacitated, serving
// pers.downedTime: rp_downed_timer seconds for a knockdown, or whatever /paralyze was given.
//
// This matters because a downed player is NOT dead as far as the rest of the code is concerned:
// paralyze_player() leaves them on RP_DOWNED_HEALTH health, so every "health <= 0", "EF_DEAD" and "PM_DEAD" test
// in the codebase says they are perfectly alive. Nothing about the downed state blocks an action on
// its own -- the only reason a downed player cannot swing a weapon or use Grip/Lightning/Drain/Mind
// Trick/Push is that ClientThink_real() pins forceHandExtend at HANDEXTEND_KNOCKDOWN, and those
// particular actions happen to refuse to run during any hand-extend state. That is inherited vanilla
// knockdown behaviour, not a rule anyone wrote for this system, and everything it does not happen to
// cover (Heal, Speed, Rage, Protect, Absorb, Seeing, the team powers, and every holdable item) was
// reachable while downed. Callers that must not be usable while downed test this explicitly instead
// of relying on that side effect.
//
// Existing bit-6 tests elsewhere were deliberately left as they are; this is for new call sites.
qboolean G_PlayerIsDowned( gentity_t *ent )
{
	if ( !ent || !ent->client )
	{
		return qfalse;
	}

	return (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_DOWNED)) ? qtrue : qfalse;
}

// GalaxyRP fix: [Death System] bit 6 alone cannot tell a combat knockdown from an admin paralysis --
// it was named "Paralyzed by an admin" before the Death System was built on top of it, and both
// features have set it ever since. /getup and /helpup must free the first and refuse the second, so
// /paralyze now also sets the ADMIN_PARALYSIS bit and this answers which state a player is in. The ADMIN_PARALYSIS bit is never set
// without bit 6, so a caller that only wants "can this player act?" should use G_PlayerIsDowned().
qboolean G_PlayerIsAdminParalyzed( gentity_t *ent )
{
	if ( !ent || !ent->client )
	{
		return qfalse;
	}

	return (ent->client->pers.player_statuses & (1 << PLAYER_STATUS_ADMIN_PARALYSIS)) ? qtrue : qfalse;
}
