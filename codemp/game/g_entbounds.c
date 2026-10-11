/*
===========================================================================
DAJ_RP: [Entity Bounds] the Entity Bounds display (/settings 6)

For an admin with the Entity System admin power who has the setting ON (player_settings bit 20, clear ==
ON like every /settings toggle), this draws, for that admin only:

  - the box of the entity being aimed at -- solid entities and trigger volumes alike -- colour-coded by
    kind: triggers yellow, brush entities (doors, movers, statics) green, NPCs and items purple, anything
    else (models and the like) blue. A brush entity's box is its own model bounds turned by its angles,
    which is exactly what the game collides against; every other entity collides as an axis-aligned box,
    and that is what is drawn for it. A short centre-print names it: "#id classname targetname", with the
    G mark /entlist uses for an entity the game made;
  - a small white cross on each of the 8 nearest point and logical entities within 384 units -- spawn
    points, targets, NPC spawners, info_ entities -- which have no size or are not in the world at all,
    so they can never be aimed at;
  - DAJ_RP: [Static Models] and the map's misc_model_static, which no client entity stands for: the
    server notes them as the map loads (g_statics.c). The one aimed at, when it is nearer than any entity
    the aim finds, gets its box in red -- its model's bounds, scaled and turned as the client draws it --
    and a centre-print in red, "static model #12 (scale 1.5)", so its number is never read as an
    entity's; and the 8 nearest others within 384 units a red cross each. /entcopystatic (g_entgrab.c)
    takes the one aimed at the same way (RP_EntBoundsAimStatic).

Adapted from Lugormod's /bounds, with three differences: it follows the aim rather than being toggled
per entity, it is sent to the one admin rather than broadcast to every player, and it never borrows the
entity's own think function (Lugormod's replaced it, so an entity with a think of its own was never
redrawn and a mover stopped while displayed). Everything here runs from ClientEndFrame().

The lines are EV_TESTLINE events, which the stock client already draws (CG_TestLine in cg_effects.c):
nothing on the client side is needed. Each line is a temporary entity, and a temporary entity occupies a
real entity slot for EVENT_VALID_MSEC (300 ms) -- and G_Spawn() ends the server when it runs out -- so
every batch asks G_EntitySlotsAvailable() first, and is skipped rather than drawn into the reserve.
===========================================================================
*/

#include "g_local.h"

#define RP_ENTBOUNDS_SETTING_BIT	20		// player_settings bit of /settings 6 (clear == ON)
#define RP_ENTBOUNDS_RANGE			8192.0f	// how far the aim reaches
#define RP_ENTBOUNDS_BOX_MSEC		900		// the aimed box is redrawn this often...
#define RP_ENTBOUNDS_BOX_LIFE		1000	// ...with lines that last a little longer, so it never blinks
#define RP_ENTBOUNDS_MARKER_MSEC	2000	// the nearby markers are redrawn this often...
#define RP_ENTBOUNDS_MARKER_LIFE	2100	// ...likewise
#define RP_ENTBOUNDS_MARKER_RANGE	384.0f
#define RP_ENTBOUNDS_MARKER_COUNT	8
#define RP_ENTBOUNDS_MARKER_SIZE	8.0f	// half the length of each arm of a marker's cross
#define RP_ENTBOUNDS_PRINT_MSEC		300		// the least time between two centre-prints
#define RP_ENTBOUNDS_REPRINT_MSEC	2000	// the centre-print is repeated this often while aimed at

// colours are the saber colours CG_TestLine() maps (CGDEBUG_SaberColor); 0 is white
#define RP_ENTBOUNDS_WHITE			0
#define RP_ENTBOUNDS_STATIC			RP_LINE_RED	// DAJ_RP: [Static Models] used for nothing else here

/*
==================
RP_EntBoundsActive

Whether the display is on for this client: a connected player, logged in, with the Entity System admin
power and /settings 6 ON, in the world or free-flying as a spectator -- not following someone, whose
view and whose snapshot (singleClient is matched against the playerState's clientNum) it would be.
==================
*/
static qboolean RP_EntBoundsActive( const gentity_t *ent )
{
	const gclient_t *client = ent->client;

	if ( !client || ent->s.number >= MAX_CLIENTS || client->pers.connected != CON_CONNECTED )
		return qfalse;

	if ( client->sess.loggedin != qtrue )
		return qfalse;

	if ( !(client->pers.bitvalue & (1 << ADM_ENTITYSYSTEM)) )
		return qfalse;

	if ( client->pers.player_settings & (1 << RP_ENTBOUNDS_SETTING_BIT) )
		return qfalse;

	if ( (client->ps.pm_flags & PMF_FOLLOW) || client->sess.spectatorState == SPECTATOR_FOLLOW )
		return qfalse;

	if ( level.intermissiontime )
		return qfalse;

	return qtrue;
}

/*
==================
RP_EntBoundsLine

G_TestLine() (ai_wpnav.c) for one client: the event goes to that client only, and ignores its PVS so a
line whose start is out of sight still arrives. The server tests SVF_SINGLECLIENT before SVF_BROADCAST
(SV_AddEntitiesVisibleFromPoint), so the two together mean exactly that.
==================
*/
void RP_EntBoundsLine( vec3_t start, vec3_t end, int color, int msec, int clientNum )
{
	gentity_t *te = G_TempEntity( start, EV_TESTLINE );

	VectorCopy( start, te->s.origin );
	VectorCopy( end, te->s.origin2 );
	te->s.time2 = msec;
	te->s.weapon = color;
	te->r.svFlags |= SVF_BROADCAST | SVF_SINGLECLIENT;
	te->r.singleClient = clientNum;
}

// zyk: the colour of an entity's box, by what it is
static int RP_EntBoundsColor( const gentity_t *e )
{
	if ( e->r.contents & CONTENTS_TRIGGER )
		return SABER_YELLOW;

	if ( e->s.eType == ET_NPC || e->s.eType == ET_ITEM || RP_EntityIsCharacter( e ) )
		return SABER_PURPLE;

	if ( e->r.bmodel )
		return SABER_GREEN;

	return SABER_BLUE;
}

/*
==================
RP_EntBoundsDrawBoxAt / RP_EntBoundsDrawBox

The 12 edges of a box with these local bounds about origin, turned by angles when angles is given and
not zero. A box with no size along an axis gets 8 units each way along it, so there is something to
see. RP_EntBoundsDrawBox() draws target's collision box: a brush entity collides against its own model
turned by its angles (the engine's transformed box trace), so its bounds are turned by
r.currentAngles about r.currentOrigin; everything else collides as an axis-aligned box and is drawn as
one. /entcopy and /entcut draw where the held entity would land with RP_EntBoundsDrawBoxAt().
==================
*/
void RP_EntBoundsDrawBoxAt( int clientNum, const vec3_t origin, const vec3_t boxMins, const vec3_t boxMaxs, const vec3_t angles, int color, int msec )
{
	vec3_t mins, maxs, corners[8];
	matrix3_t axis;
	qboolean rotate = qfalse;
	int i, k;
	static const int edges[12][2] = {
		{ 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 },		// bottom
		{ 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 },		// top
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }		// sides
	};

	VectorCopy( boxMins, mins );
	VectorCopy( boxMaxs, maxs );

	for ( k = 0; k < 3; k++ )
	{
		if ( maxs[k] - mins[k] < 1.0f )
		{
			mins[k] -= RP_ENTBOUNDS_MARKER_SIZE;
			maxs[k] += RP_ENTBOUNDS_MARKER_SIZE;
		}
	}

	if ( angles && ( angles[0] || angles[1] || angles[2] ) )
	{
		AnglesToAxis( angles, axis );
		rotate = qtrue;
	}

	for ( i = 0; i < 8; i++ )
	{
		vec3_t local;

		local[0] = ( i & 1 ) ? maxs[0] : mins[0];
		local[1] = ( i & 2 ) ? maxs[1] : mins[1];
		local[2] = ( i & 4 ) ? maxs[2] : mins[2];

		if ( rotate )
		{
			for ( k = 0; k < 3; k++ )
			{
				corners[i][k] = origin[k] +
					local[0] * axis[0][k] + local[1] * axis[1][k] + local[2] * axis[2][k];
			}
		}
		else
		{
			VectorAdd( origin, local, corners[i] );
		}
	}

	for ( i = 0; i < 12; i++ )
	{
		RP_EntBoundsLine( corners[edges[i][0]], corners[edges[i][1]], color, msec, clientNum );
	}
}

static void RP_EntBoundsDrawBox( const gentity_t *viewer, const gentity_t *target, int color, int msec )
{
	RP_EntBoundsDrawBoxAt( viewer->s.number, target->r.currentOrigin, target->r.mins, target->r.maxs,
		target->r.bmodel ? target->r.currentAngles : NULL, color, msec );
}

/*
==================
RP_EntBoundsSelectable

What the aim may pick: a live entity in the world that is not a player or a body, not an event, and not
something of the viewer's own (its saber, its missiles).
==================
*/
static qboolean RP_EntBoundsSelectable( const gentity_t *viewer, const gentity_t *e )
{
	int num = e->s.number;

	if ( !e->inuse || !e->r.linked || e->isLogical )
		return qfalse;

	if ( num < MAX_CLIENTS + BODY_QUEUE_SIZE || num >= ENTITYNUM_WORLD )
		return qfalse;

	if ( e->s.eType >= ET_EVENTS || e->freeAfterEvent )
		return qfalse;

	if ( e->r.ownerNum == viewer->s.number || e->isSaberEntity )
		return qfalse;

	return qtrue;
}

/*
==================
RP_EntBoundsSoftVolume

GalaxyRP: [Entity System] a box the aim trace passes straight through, which RP_EntBoundsAim() tests
itself: a trigger; or an entity the map or the entity system made (it has a key/value record) that has a
box but nothing a shot stops at -- an fx_runner (a box only so it reaches the players near it), a
misc_model_breakable without its SOLID flag, an md3 func_ entity that is not solid, a func_usable turned
off. Without this they could be neither aimed at (nothing to hit) nor marked (they have a size).
==================
*/
static qboolean RP_EntBoundsSoftVolume( const gentity_t *e )
{
	if ( e->r.contents & CONTENTS_TRIGGER )
		return qtrue;

	if ( e->r.contents & MASK_SHOT )
		return qfalse;

	if ( VectorCompare( e->r.mins, vec3_origin ) && VectorCompare( e->r.maxs, vec3_origin ) )
		return qfalse;

	return RP_EntityHasSpawnKeys( e );
}

/*
==================
RP_EntBoundsAim

The entity at the viewer's crosshair, or NULL. The trace finds the first solid thing along the view --
an entity or the world, which is where the view ends. Triggers are not solid, so a trace that stops at
them cannot tell the one the viewer stands in (it would start inside it, and every look would land on
it) from the ones ahead; so they are tested separately, as their world boxes against the ray, and the
nearest one entered in front of the viewer, before the solid hit, wins. So are the other boxes the
trace passes through (RP_EntBoundsSoftVolume), the same way.
==================
*/
// DAJ_RP: [Static Models] the same, with how far along the view it is -- or, with none, how far the view
// reaches (the solid hit, or the range): a static model nearer than that is what is aimed at
static gentity_t *RP_EntBoundsAimDist( const gentity_t *viewer, const vec3_t eye, const vec3_t dir, float *dist )
{
	vec3_t end;
	trace_t tr;
	gentity_t *best = NULL;
	float bestDist;
	int i;

	VectorMA( eye, RP_ENTBOUNDS_RANGE, dir, end );

	trap->Trace( &tr, eye, vec3_origin, vec3_origin, end, viewer->s.number, MASK_SHOT, qfalse, 0, 0 );

	bestDist = tr.fraction * RP_ENTBOUNDS_RANGE;

	if ( !tr.startsolid && tr.fraction < 1.0f && tr.entityNum >= 0 && tr.entityNum < ENTITYNUM_WORLD &&
		RP_EntBoundsSelectable( viewer, &g_entities[tr.entityNum] ) )
	{
		best = &g_entities[tr.entityNum];
	}

	for ( i = MAX_CLIENTS + BODY_QUEUE_SIZE; i < level.num_entities && i < ENTITYNUM_WORLD; i++ )
	{
		gentity_t *e = &g_entities[i];
		float tEnter = -1.0e9f, tExit = 1.0e9f;
		int k;
		qboolean miss = qfalse;

		if ( !RP_EntBoundsSelectable( viewer, e ) || !RP_EntBoundsSoftVolume( e ) )
			continue;

		// zyk: the slab test, against the box one unit wider each way than the entity's world box
		for ( k = 0; k < 3; k++ )
		{
			float lo = e->r.absmin[k] - 1.0f, hi = e->r.absmax[k] + 1.0f;

			if ( dir[k] > -0.0001f && dir[k] < 0.0001f )
			{
				if ( eye[k] < lo || eye[k] > hi )
				{
					miss = qtrue;
					break;
				}
			}
			else
			{
				float t1 = ( lo - eye[k] ) / dir[k];
				float t2 = ( hi - eye[k] ) / dir[k];

				if ( t1 > t2 )
				{
					float t = t1; t1 = t2; t2 = t;
				}
				if ( t1 > tEnter ) tEnter = t1;
				if ( t2 < tExit ) tExit = t2;
			}
		}

		// zyk: missed, behind the viewer, or the viewer is inside it
		if ( miss || tEnter > tExit || tEnter <= 0.0f )
			continue;

		if ( tEnter < bestDist )
		{
			bestDist = tEnter;
			best = e;
		}
	}

	if ( dist )
		*dist = bestDist;

	return best;
}

static void RP_EntBoundsEye( const gentity_t *viewer, vec3_t eye, vec3_t dir )
{
	VectorCopy( viewer->client->ps.origin, eye );
	eye[2] += viewer->client->ps.viewheight;
	AngleVectors( viewer->client->ps.viewangles, dir, NULL, NULL );
}

gentity_t *RP_EntBoundsAim( const gentity_t *viewer )
{
	vec3_t eye, dir;

	RP_EntBoundsEye( viewer, eye, dir );
	return RP_EntBoundsAimDist( viewer, eye, dir, NULL );
}

/*
==================
RP_EntBoundsAimStatic

DAJ_RP: [Static Models] the static model at the viewer's crosshair, or -1: the nearest one the view
enters before the entity RP_EntBoundsAim() would pick, or before what stops the view when it picks none
-- the one Entity Bounds draws in red, and the one /entcopystatic copies.
==================
*/
static int RP_EntBoundsAimBoth( const gentity_t *viewer, gentity_t **entity )
{
	vec3_t eye, dir;
	float dist = RP_ENTBOUNDS_RANGE;
	gentity_t *e;
	int n;

	RP_EntBoundsEye( viewer, eye, dir );
	e = RP_EntBoundsAimDist( viewer, eye, dir, &dist );
	n = RP_StaticAim( eye, dir, dist, NULL );

	if ( entity )
		*entity = ( n >= 0 ) ? NULL : e;

	return n;
}

int RP_EntBoundsAimStatic( const gentity_t *viewer )
{
	return RP_EntBoundsAimBoth( viewer, NULL );
}

// zyk: the centre-print naming the aimed entity. A value from the entity file can hold a quote, which
// would end the command's own quoted argument early, so quotes are turned into apostrophes.
static void RP_EntBoundsPrint( const gentity_t *viewer, const gentity_t *target )
{
	char text[256];
	char *p;

	Com_sprintf( text, sizeof( text ), "^3#%d%s ^7%s%s%s", target->s.number,
		RP_EntityHasSpawnKeys( target ) ? "" : "G",
		target->classname ? target->classname : "noclass",
		( target->targetname && target->targetname[0] ) ? " ^5" : "",
		( target->targetname && target->targetname[0] ) ? target->targetname : "" );

	for ( p = text; *p; p++ )
	{
		if ( *p == '"' )
			*p = '\'';
	}

	// GalaxyRP fix: [security] and no "@@@" either -- see RP_BreakStringEdRefs() (g_utils.c). The
	// names come from whoever placed the entity, and the client would expand a run of three '@' as a
	// reference to a server phrase, which on the stock client can overflow a buffer and crash the
	// admin who merely looked at it. Only this line is changed, never the entity.
	RP_BreakStringEdRefs( text );

	trap->SendServerCommand( viewer->s.number, va( "cp \"%s\n\"", text ) );
}

/*
==================
RP_EntBoundsMarkers

A white three-armed cross on each of the nearest point and logical entities: in use, not a player, body,
event, missile, NPC, item or saber, and either in the logical region (never in the world at all) or
networked with no size and a key/value record. The one the viewer is aiming at is skipped -- it has its
box. Three lines rather than a 12-edge cube: eight markers every two seconds is then 24 short-lived
event entities, not 96.
==================
*/
static void RP_EntBoundsMarkers( const gentity_t *viewer, int aimed )
{
	gentity_t *e;
	gentity_t *pick[RP_ENTBOUNDS_MARKER_COUNT];
	float pickDist[RP_ENTBOUNDS_MARKER_COUNT];
	vec3_t eye;
	int count = 0;
	int i;

	VectorCopy( viewer->client->ps.origin, eye );
	eye[2] += viewer->client->ps.viewheight;

	RP_FOR_EACH_ENTITY( e )
	{
		int num = e->s.number;
		const float *origin;
		float dist;

		if ( !e->inuse || num < MAX_CLIENTS + BODY_QUEUE_SIZE || num == ENTITYNUM_WORLD || num == ENTITYNUM_NONE || num == aimed )
			continue;
		// zyk: one an admin is holding with /entcopy or /entcut has a preview of its own (g_entgrab.c)
		if ( e->rpHeldBy )
			continue;
		// DAJ_RP: [Entity Bounds] a client of the weapon shooters' pool is no player or NPC: a misc_weapon_shooter
		// borrows one only so the weapon code can fire for it (G_ClientForShooter(), g_misc.c), so it is let
		// through, and gets the cross shooter_blaster gets. It has no size and is never linked, so without the
		// cross it showed nowhere at all. Every other client still means a player or an NPC
		// (RP_EntityIsCharacter, g_misc.c).
		if ( e->s.eType >= ET_EVENTS || e->freeAfterEvent || e->s.eType == ET_MISSILE || e->s.eType == ET_NPC ||
			e->s.eType == ET_ITEM || RP_EntityIsCharacter( e ) || e->isSaberEntity )
			continue;
		if ( !e->isLogical && ( !VectorCompare( e->r.mins, vec3_origin ) || !VectorCompare( e->r.maxs, vec3_origin ) ) )
			continue;
		// zyk: a networked one only if the map or the entity system made it -- the game keeps plenty of
		// its own bookkeeping as sizeless networked entities (every NPC's goal entity, for one), and
		// those are not what a builder is looking for. Logical ones are all shown, spawn points the
		// per-map fixes add included.
		if ( !e->isLogical && RP_EntityHasSpawnKeys( e ) == qfalse )
			continue;

		// zyk: most set both; a logical entity is never linked and some only ever set s.origin
		origin = VectorCompare( e->r.currentOrigin, vec3_origin ) ? e->s.origin : e->r.currentOrigin;
		dist = Distance( eye, origin );

		if ( dist > RP_ENTBOUNDS_MARKER_RANGE )
			continue;

		// zyk: keep the nearest few, nearest first
		for ( i = count; i > 0 && pickDist[i - 1] > dist; i-- )
		{
			if ( i < RP_ENTBOUNDS_MARKER_COUNT )
			{
				pick[i] = pick[i - 1];
				pickDist[i] = pickDist[i - 1];
			}
		}
		if ( i < RP_ENTBOUNDS_MARKER_COUNT )
		{
			pick[i] = e;
			pickDist[i] = dist;
			if ( count < RP_ENTBOUNDS_MARKER_COUNT )
				count++;
		}
	}

	if ( count == 0 || G_EntitySlotsAvailable( count * 3 ) == qfalse )
		return;

	for ( i = 0; i < count; i++ )
	{
		const float *origin = VectorCompare( pick[i]->r.currentOrigin, vec3_origin ) ? pick[i]->s.origin : pick[i]->r.currentOrigin;
		int k;

		for ( k = 0; k < 3; k++ )
		{
			vec3_t a, b;

			VectorCopy( origin, a );
			VectorCopy( origin, b );
			a[k] -= RP_ENTBOUNDS_MARKER_SIZE;
			b[k] += RP_ENTBOUNDS_MARKER_SIZE;
			RP_EntBoundsLine( a, b, RP_ENTBOUNDS_WHITE, RP_ENTBOUNDS_MARKER_LIFE, viewer->s.number );
		}
	}
}

/*
==================
RP_EntBoundsStaticMarkers

DAJ_RP: [Static Models] a red cross, the same as the white ones, on each of the 8 nearest static models
within the same range -- at the model's origin -- but the one aimed at, which has its box.
==================
*/
static void RP_EntBoundsStaticMarkers( const gentity_t *viewer, int aimedStatic )
{
	int pick[RP_ENTBOUNDS_MARKER_COUNT];
	float pickDist[RP_ENTBOUNDS_MARKER_COUNT];
	vec3_t eye, origin;
	int count = 0, total = RP_StaticsCount();
	int n, i;

	VectorCopy( viewer->client->ps.origin, eye );
	eye[2] += viewer->client->ps.viewheight;

	for ( n = 0; n < total; n++ )
	{
		float dist;

		if ( n == aimedStatic || !RP_StaticGet( n, NULL, origin, NULL, NULL ) )
			continue;

		dist = Distance( eye, origin );
		if ( dist > RP_ENTBOUNDS_MARKER_RANGE )
			continue;

		for ( i = count; i > 0 && pickDist[i - 1] > dist; i-- )
		{
			if ( i < RP_ENTBOUNDS_MARKER_COUNT )
			{
				pick[i] = pick[i - 1];
				pickDist[i] = pickDist[i - 1];
			}
		}
		if ( i < RP_ENTBOUNDS_MARKER_COUNT )
		{
			pick[i] = n;
			pickDist[i] = dist;
			if ( count < RP_ENTBOUNDS_MARKER_COUNT )
				count++;
		}
	}

	if ( count == 0 || G_EntitySlotsAvailable( count * 3 ) == qfalse )
		return;

	for ( i = 0; i < count; i++ )
	{
		int k;

		RP_StaticGet( pick[i], NULL, origin, NULL, NULL );

		for ( k = 0; k < 3; k++ )
		{
			vec3_t a, b;

			VectorCopy( origin, a );
			VectorCopy( origin, b );
			a[k] -= RP_ENTBOUNDS_MARKER_SIZE;
			b[k] += RP_ENTBOUNDS_MARKER_SIZE;
			RP_EntBoundsLine( a, b, RP_ENTBOUNDS_STATIC, RP_ENTBOUNDS_MARKER_LIFE, viewer->s.number );
		}
	}
}

// DAJ_RP: [Static Models] the aimed static model's box: its bounds in its own axes, turned and at its origin
static void RP_EntBoundsDrawStatic( const gentity_t *viewer, int n, int msec )
{
	vec3_t mins, maxs, origin, angles;

	if ( !RP_StaticGet( n, NULL, origin, angles, NULL ) )
		return;

	RP_StaticBox( n, mins, maxs );
	RP_EntBoundsDrawBoxAt( viewer->s.number, origin, mins, maxs, angles, RP_ENTBOUNDS_STATIC, msec );
}

/*
==================
RP_EntBoundsFrame

Once per client per server frame, from ClientEndFrame().
==================
*/
void RP_EntBoundsFrame( gentity_t *ent )
{
	gclient_t *client = ent->client;
	gentity_t *aimed;
	int aimedNum, aimedStatic;

	if ( !client )
		return;

	if ( !RP_EntBoundsActive( ent ) )
	{
		// zyk: switched off (or logged out, or lost the power): take our centre-print down and forget
		if ( client->entBoundsPrinted && ent->s.number < MAX_CLIENTS && client->pers.connected == CON_CONNECTED )
			trap->SendServerCommand( ent->s.number, "cp \"\"" );

		client->entBoundsAimed = 0;
		client->entBoundsAimedStatic = 0;
		client->entBoundsNextBox = 0;
		client->entBoundsNextMarkers = 0;
		client->entBoundsNextPrint = 0;
		client->entBoundsPrintPending = qfalse;
		client->entBoundsPrinted = qfalse;
		return;
	}

	// zyk: a stamp far in the future can only have come from an older, longer-running level.time
	if ( client->entBoundsNextBox > level.time + 10000 ) client->entBoundsNextBox = 0;
	if ( client->entBoundsNextMarkers > level.time + 10000 ) client->entBoundsNextMarkers = 0;
	if ( client->entBoundsNextPrint > level.time + 10000 ) client->entBoundsNextPrint = 0;

	// DAJ_RP: [Static Models] a static model nearer than the entity aimed at is what is aimed at
	aimedStatic = RP_EntBoundsAimBoth( ent, &aimed );
	aimedNum = aimed ? aimed->s.number : 0;

	if ( aimedNum != client->entBoundsAimed || aimedStatic + 1 != client->entBoundsAimedStatic )
	{
		client->entBoundsAimed = aimedNum;
		client->entBoundsAimedStatic = aimedStatic + 1;
		client->entBoundsNextBox = 0;	// draw the new one now
		client->entBoundsPrintPending = qtrue;
	}

	if ( ( aimed || aimedStatic >= 0 ) && level.time >= client->entBoundsNextBox && G_EntitySlotsAvailable( 12 ) )
	{
		if ( aimed )
			RP_EntBoundsDrawBox( ent, aimed, RP_EntBoundsColor( aimed ), RP_ENTBOUNDS_BOX_LIFE );
		else
			RP_EntBoundsDrawStatic( ent, aimedStatic, RP_ENTBOUNDS_BOX_LIFE );
		client->entBoundsNextBox = level.time + RP_ENTBOUNDS_BOX_MSEC;
	}

	// zyk: the name, when the aim settles on something new, and again now and then while it stays
	if ( ( aimed || aimedStatic >= 0 ) && !client->entBoundsPrintPending && level.time >= client->entBoundsNextPrint + RP_ENTBOUNDS_REPRINT_MSEC - RP_ENTBOUNDS_PRINT_MSEC )
		client->entBoundsPrintPending = qtrue;

	if ( client->entBoundsPrintPending && level.time >= client->entBoundsNextPrint )
	{
		if ( aimed )
		{
			RP_EntBoundsPrint( ent, aimed );
			client->entBoundsPrinted = qtrue;
		}
		else if ( aimedStatic >= 0 )
		{
			trap->SendServerCommand( ent->s.number, va( "cp \"%s\n\"", RP_StaticLabel( aimedStatic ) ) );
			client->entBoundsPrinted = qtrue;
		}
		else if ( client->entBoundsPrinted )
		{
			trap->SendServerCommand( ent->s.number, "cp \"\"" );
			client->entBoundsPrinted = qfalse;
		}

		client->entBoundsPrintPending = qfalse;
		client->entBoundsNextPrint = level.time + RP_ENTBOUNDS_PRINT_MSEC;
	}

	if ( level.time >= client->entBoundsNextMarkers )
	{
		RP_EntBoundsMarkers( ent, aimedNum );
		RP_EntBoundsStaticMarkers( ent, aimedStatic );
		client->entBoundsNextMarkers = level.time + RP_ENTBOUNDS_MARKER_MSEC;
	}
}
