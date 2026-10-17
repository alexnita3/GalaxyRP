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
#include "../../galaxyrp/game/rp_lua.h"	// GLX_U: [Lua] a button pressed, touched or pushed reaches scripts


/*
===============================================================================

PUSHMOVE

===============================================================================
*/

void MatchTeam( gentity_t *teamLeader, int moverState, int time );

typedef struct pushed_s {
	gentity_t	*ent;
	vec3_t	origin;
	vec3_t	angles;
	float	deltayaw;
} pushed_t;
pushed_t	pushed[MAX_GENTITIES], *pushed_p;

#define MOVER_START_ON		1
#define MOVER_FORCE_ACTIVATE	2
#define MOVER_CRUSHER		4
#define MOVER_TOGGLE		8
#define MOVER_LOCKED		16
#define MOVER_GOODIE		32
#define MOVER_PLAYER_USE	64
#define MOVER_INACTIVE		128

int	BMS_START = 0;
int	BMS_MID = 1;
int	BMS_END = 2;

/*
-------------------------
G_PlayDoorLoopSound
-------------------------
*/

void G_PlayDoorLoopSound( gentity_t *ent )
{
	if (!ent->soundSet || !ent->soundSet[0])
	{
		return;
	}

	ent->s.soundSetIndex = G_SoundSetIndex(ent->soundSet);
	ent->s.loopIsSoundset = qtrue;
	ent->s.loopSound = BMS_MID;
	/*
	ent->s.soundSetIndex = G_SoundSetIndex(ent->soundSet);
	ent->loopingOnClient = qtrue;
	G_AddEvent(ent, EV_PLAYDOORLOOPSOUND, 0);
	*/
}

/*
-------------------------
G_PlayDoorSound
-------------------------
*/

void G_PlayDoorSound( gentity_t *ent, int type )
{
	if (!ent->soundSet || !ent->soundSet[0])
	{
		return;
	}

	ent->s.soundSetIndex = G_SoundSetIndex(ent->soundSet);

	G_AddEvent(ent, EV_PLAYDOORSOUND, type);
}

/*
============
G_TestEntityPosition

============
*/
gentity_t	*G_TestEntityPosition( gentity_t *ent ) {
	trace_t	tr;
	int		mask;

	if ( ent->clipmask ) {
		mask = ent->clipmask;
	} else {
		mask = MASK_SOLID;
	}
	if ( ent->client ) {
		vec3_t vMax;
		VectorCopy(ent->r.maxs, vMax);
		if (vMax[2] < 1)
		{
			vMax[2] = 1;
		}
		trap->Trace( &tr, ent->client->ps.origin, ent->r.mins, vMax, ent->client->ps.origin, ent->s.number, mask, qfalse, 0, 0 );
	} else {
		trap->Trace( &tr, ent->s.pos.trBase, ent->r.mins, ent->r.maxs, ent->s.pos.trBase, ent->s.number, mask, qfalse, 0, 0 );
	}

	if (tr.startsolid)
		return &g_entities[ tr.entityNum ];

	return NULL;
}

/*
================
G_CreateRotationMatrix
================
*/
void G_CreateRotationMatrix(vec3_t angles, matrix3_t matrix) {
	AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
	VectorInverse(matrix[1]);
}

/*
================
G_TransposeMatrix
================
*/
void G_TransposeMatrix(matrix3_t matrix, matrix3_t transpose) {
	int i, j;
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			transpose[i][j] = matrix[j][i];
		}
	}
}

/*
================
G_RotatePoint
================
*/
void G_RotatePoint(vec3_t point, matrix3_t matrix) {
	vec3_t tvec;

	VectorCopy(point, tvec);
	point[0] = DotProduct(matrix[0], tvec);
	point[1] = DotProduct(matrix[1], tvec);
	point[2] = DotProduct(matrix[2], tvec);
}

/*
==================
G_TryPushingEntity

Returns qfalse if the move is blocked
==================
*/
qboolean	G_TryPushingEntity( gentity_t *check, gentity_t *pusher, vec3_t move, vec3_t amove ) {
	matrix3_t	matrix, transpose;
	vec3_t		org, org2, move2;
	gentity_t	*block;

	//This was only serverside not to mention it was never set.
	/*
	// EF_MOVER_STOP will just stop when contacting another entity
	// instead of pushing it, but entities can still ride on top of it
	if ( ( pusher->s.eFlags & EF_MOVER_STOP ) &&
		check->s.groundEntityNum != pusher->s.number ) {
		return qfalse;
	}
	*/
	if ( pusher->s.apos.trType != TR_STATIONARY//rotating
		&& (pusher->spawnflags&16) //IMPACT
		&& Q_stricmp( "func_rotating", pusher->classname ) == 0 )
	{//just blow the fuck out of them
		G_Damage( check, pusher, pusher, NULL, NULL, pusher->damage, DAMAGE_NO_KNOCKBACK, MOD_CRUSH );
		return qtrue;
	}

	// save off the old position
	if (pushed_p > &pushed[MAX_GENTITIES]) {
		trap->Error( ERR_DROP, "pushed_p > &pushed[MAX_GENTITIES]" );
	}
	pushed_p->ent = check;
	VectorCopy (check->s.pos.trBase, pushed_p->origin);
	VectorCopy (check->s.apos.trBase, pushed_p->angles);
	if ( check->client ) {
		pushed_p->deltayaw = check->client->ps.delta_angles[YAW];
		VectorCopy (check->client->ps.origin, pushed_p->origin);
	}
	pushed_p++;

	// try moving the contacted entity
	// figure movement due to the pusher's amove
	G_CreateRotationMatrix( amove, transpose );
	G_TransposeMatrix( transpose, matrix );
	if ( check->client ) {
		VectorSubtract (check->client->ps.origin, pusher->r.currentOrigin, org);
	}
	else {
		VectorSubtract (check->s.pos.trBase, pusher->r.currentOrigin, org);
	}
	VectorCopy( org, org2 );
	G_RotatePoint( org2, matrix );
	VectorSubtract (org2, org, move2);
	// add movement
	VectorAdd (check->s.pos.trBase, move, check->s.pos.trBase);
	VectorAdd (check->s.pos.trBase, move2, check->s.pos.trBase);
	if ( check->client ) {
		VectorAdd (check->client->ps.origin, move, check->client->ps.origin);
		VectorAdd (check->client->ps.origin, move2, check->client->ps.origin);
		// make sure the client's view rotates when on a rotating mover
		check->client->ps.delta_angles[YAW] += ANGLE2SHORT(amove[YAW]);
	}

	// may have pushed them off an edge
	if ( check->s.groundEntityNum != pusher->s.number ) {
		check->s.groundEntityNum = ENTITYNUM_NONE;
	}

	block = G_TestEntityPosition( check );
	if (!block) {
		// pushed ok
		if ( check->client ) {
			VectorCopy( check->client->ps.origin, check->r.currentOrigin );
		} else {
			VectorCopy( check->s.pos.trBase, check->r.currentOrigin );
		}
		trap->LinkEntity ((sharedEntity_t *)check);
		return qtrue;
	}

	if (check->takedamage && !check->client && check->s.weapon && check->r.ownerNum < MAX_CLIENTS &&
		check->health < 500)
	{
		if (check->health > 0)
		{
			G_Damage(check, pusher, pusher, vec3_origin, check->r.currentOrigin, 999, 0, MOD_UNKNOWN);
		}
	}
	// if it is ok to leave in the old position, do it
	// this is only relevent for riding entities, not pushed
	// Sliding trapdoors can cause this.
	VectorCopy( (pushed_p-1)->origin, check->s.pos.trBase);
	if ( check->client ) {
		VectorCopy( (pushed_p-1)->origin, check->client->ps.origin);
	}
	VectorCopy( (pushed_p-1)->angles, check->s.apos.trBase );
	block = G_TestEntityPosition (check);
	if ( !block ) {
		check->s.groundEntityNum = ENTITYNUM_NONE;
		pushed_p--;
		return qtrue;
	}

	// blocked
	return qfalse;
}


void G_ExplodeMissile( gentity_t *ent );

/*
============
G_MoverPush

Objects need to be moved back on a failed push,
otherwise riders would continue to slide.
If qfalse is returned, *obstacle will be the blocking entity
============
*/
void NPC_RemoveBody( gentity_t *self );
qboolean G_MoverPush( gentity_t *pusher, vec3_t move, vec3_t amove, gentity_t **obstacle ) {
	int			i, e;
	gentity_t	*check;
	vec3_t		mins, maxs;
	pushed_t	*p;
	int			entityList[MAX_GENTITIES];
	int			listedEntities;
	vec3_t		totalMins, totalMaxs;

	*obstacle = NULL;


	// mins/maxs are the bounds at the destination
	// totalMins / totalMaxs are the bounds for the entire move
	if ( pusher->r.currentAngles[0] || pusher->r.currentAngles[1] || pusher->r.currentAngles[2]
		|| amove[0] || amove[1] || amove[2] ) {
		float		radius;

		radius = RadiusFromBounds( pusher->r.mins, pusher->r.maxs );
		for ( i = 0 ; i < 3 ; i++ ) {
			mins[i] = pusher->r.currentOrigin[i] + move[i] - radius;
			maxs[i] = pusher->r.currentOrigin[i] + move[i] + radius;
			totalMins[i] = mins[i] - move[i];
			totalMaxs[i] = maxs[i] - move[i];
		}
	} else {
		for (i=0 ; i<3 ; i++) {
			mins[i] = pusher->r.absmin[i] + move[i];
			maxs[i] = pusher->r.absmax[i] + move[i];
		}

		VectorCopy( pusher->r.absmin, totalMins );
		VectorCopy( pusher->r.absmax, totalMaxs );
		for (i=0 ; i<3 ; i++) {
			if ( move[i] > 0 ) {
				totalMaxs[i] += move[i];
			} else {
				totalMins[i] += move[i];
			}
		}
	}

	// unlink the pusher so we don't get it in the entityList
	trap->UnlinkEntity( (sharedEntity_t *)pusher );

	listedEntities = trap->EntitiesInBox( totalMins, totalMaxs, entityList, MAX_GENTITIES );

	// move the pusher to it's final position
	VectorAdd( pusher->r.currentOrigin, move, pusher->r.currentOrigin );
	VectorAdd( pusher->r.currentAngles, amove, pusher->r.currentAngles );
	trap->LinkEntity( (sharedEntity_t *)pusher );

	// see if any solid entities are inside the final position
	for ( e = 0 ; e < listedEntities ; e++ ) {
		check = &g_entities[ entityList[ e ] ];

		// only push items and players
		if ( /*check->s.eType != ET_ITEM &&*/ check->s.eType != ET_PLAYER && check->s.eType != ET_NPC && !check->physicsObject ) {
			continue;
		}

		// if the entity is standing on the pusher, it will definitely be moved
		if ( check->s.groundEntityNum != pusher->s.number ) {
			// see if the ent needs to be tested
			if ( check->r.absmin[0] >= maxs[0]
			|| check->r.absmin[1] >= maxs[1]
			|| check->r.absmin[2] >= maxs[2]
			|| check->r.absmax[0] <= mins[0]
			|| check->r.absmax[1] <= mins[1]
			|| check->r.absmax[2] <= mins[2] ) {
				continue;
			}
			// see if the ent's bbox is inside the pusher's final position
			// this does allow a fast moving object to pass through a thin entity...
			if (!G_TestEntityPosition (check)) {
				continue;
			}
		}

		// the entity needs to be pushed
		if ( G_TryPushingEntity( check, pusher, move, amove ) ) {
			continue;
		}

		if (pusher->damage && check->client && (pusher->spawnflags & 32))
		{
			G_Damage( check, pusher, pusher, NULL, NULL, pusher->damage, 0, MOD_CRUSH );
			continue;
		}

		if (check->s.eType == ET_BODY ||
			(check->s.eType == ET_PLAYER && check->health < 1))
		{ //whatever, just crush it
			G_Damage( check, pusher, pusher, NULL, NULL, 999, 0, MOD_CRUSH );
			continue;
		}

		if ( (check->r.contents & CONTENTS_TRIGGER) && check->s.weapon == G2_MODEL_PART)
		{//keep limbs from blocking elevators.  Kill the limb and keep moving.
			G_FreeEntity(check);
			continue;
		}

		if( check->s.eFlags & EF_DROPPEDWEAPON )
		{//keep dropped weapons from blocking elevators.  Kill the weapon and keep moving.
			G_FreeEntity(check);
			continue;
		}

		if ( check->s.eType == ET_NPC //an NPC
			&& check->health <= 0 //NPC is dead
			&& !(check->flags & FL_NOTARGET) )  //NPC isn't still spawned or in no target mode.
		{//dead npcs should be removed now!
			NPC_RemoveBody( check );
			continue;
		}

		// the move was blocked an entity

		// bobbing entities are instant-kill and never get blocked
		if ( pusher->s.pos.trType == TR_SINE || pusher->s.apos.trType == TR_SINE ) {
			G_Damage( check, pusher, pusher, NULL, NULL, 99999, 0, MOD_CRUSH );
			continue;
		}


		// save off the obstacle so we can call the block function (crush, etc)
		*obstacle = check;

		// move back any entities we already moved
		// go backwards, so if the same entity was pushed
		// twice, it goes back to the original position
		for ( p=pushed_p-1 ; p>=pushed ; p-- ) {
			VectorCopy (p->origin, p->ent->s.pos.trBase);
			VectorCopy (p->angles, p->ent->s.apos.trBase);
			if ( p->ent->client ) {
				p->ent->client->ps.delta_angles[YAW] = p->deltayaw;
				VectorCopy (p->origin, p->ent->client->ps.origin);
			}
			trap->LinkEntity ((sharedEntity_t *)p->ent);
		}
		return qfalse;
	}

	return qtrue;
}


/*
=================
G_MoverTeam
=================
*/
void G_MoverTeam( gentity_t *ent ) {
	vec3_t		move, amove;
	gentity_t	*part, *obstacle;
	vec3_t		origin, angles;

	obstacle = NULL;

	// GalaxyRP: [Entity System] backstop: a member no longer in use is cut off the chain before anything
	// is moved. G_FreeEntity() unhooks a member itself now (RP_TeamDetach), so this should never find one;
	// if it does, the chain ends there rather than moving whatever has the slot now.
	{
		int steps = 0;

		for ( part = ent; part->teamchain && steps < MAX_GENTITIES; part = part->teamchain, steps++ )
		{
			if ( !part->teamchain->inuse )
			{
				G_LogPrintf( "G_MoverTeam: entity %d (%s) had a freed entity in its team chain; cut off\n",
					ent->s.number, ent->classname ? ent->classname : "noclass" );
				part->teamchain = NULL;
				break;
			}
		}
	}

	// make sure all team slaves can move before commiting
	// any moves or calling any think functions
	// if the move is blocked, all moved objects will be backed out
	pushed_p = pushed;
	for (part = ent ; part ; part=part->teamchain) {
		// get current position
		BG_EvaluateTrajectory( &part->s.pos, level.time, origin );
		BG_EvaluateTrajectory( &part->s.apos, level.time, angles );
		VectorSubtract( origin, part->r.currentOrigin, move );
		VectorSubtract( angles, part->r.currentAngles, amove );
		if ( !VectorCompare( move, vec3_origin )
			|| !VectorCompare( amove, vec3_origin ) )
		{//actually moved
			if ( !G_MoverPush( part, move, amove, &obstacle ) ) {
				break;	// move was blocked
			}
		}
	}

	if (part) {
		// go back to the previous position
		for ( part = ent ; part ; part = part->teamchain ) {
			part->s.pos.trTime += level.time - level.previousTime;
			part->s.apos.trTime += level.time - level.previousTime;
			BG_EvaluateTrajectory( &part->s.pos, level.time, part->r.currentOrigin );
			BG_EvaluateTrajectory( &part->s.apos, level.time, part->r.currentAngles );
			trap->LinkEntity( (sharedEntity_t *)part );
		}

		// if the pusher has a "blocked" function, call it
		// GalaxyRP fix: [SP Maps] only when the master itself is moving: the team is run now while a member
		// moves and the master stands (RP_MoverTeamMoving()), and a door that is not moving must not reverse
		// -- open -- because a rotator of its team was blocked; that member just waits, as a rotator does
		if (ent->blocked && ( ent->s.pos.trType != TR_STATIONARY || ent->s.apos.trType != TR_STATIONARY )) {
			ent->blocked( ent, obstacle );
		}
		return;
	}

	// the move succeeded
	for ( part = ent ; part ; part = part->teamchain ) {
		// call the reached function if time is at or past end point
		if ( part->s.pos.trType == TR_LINEAR_STOP ||
			part->s.pos.trType == TR_NONLINEAR_STOP) {
			if ( level.time >= part->s.pos.trTime + part->s.pos.trDuration ) {
				if ( part->reached ) {
					part->reached( part );
				}
			}
		}
	}
}

/*
================
G_RunMover

================
*/
// GalaxyRP: [SP Maps] whether any member of the team ent leads is moving (ent alone when it leads none)
static qboolean RP_MoverTeamMoving( const gentity_t *ent )
{
	const gentity_t *part;
	int steps;

	for ( part = ent, steps = 0; part && steps < MAX_GENTITIES; part = part->teamchain, steps++ )
	{
		if ( !part->inuse )
			break;
		if ( part->s.pos.trType != TR_STATIONARY || part->s.apos.trType != TR_STATIONARY )
			return qtrue;
	}
	return qfalse;
}

void G_RunMover( gentity_t *ent ) {
	// if not a team captain, don't do anything, because
	// the captain will handle everything
	if ( ent->flags & FL_TEAMSLAVE ) {
		return;
	}

	// if stationary at one of the positions, don't move anything
	// GalaxyRP fix: [SP Maps] -- nothing of the team: a member moving while its master stood still (a rotator
	// in a door's team, a script moving one member, a team of rotators toggled apart) was never moved here,
	// so the server held it where it was while every client, evaluating its trajectory, drew it moving
	if ( RP_MoverTeamMoving( ent ) ) {
		G_MoverTeam( ent );
	}

	// check think function
	G_RunThink( ent );
}

/*
============================================================================

GENERAL MOVERS

Doors, plats, and buttons are all binary (two position) movers
Pos1 is "at rest", pos2 is "activated"
============================================================================
*/

/*
CalcTeamDoorCenter

Finds all the doors of a team and returns their center position
*/

void CalcTeamDoorCenter ( gentity_t *ent, vec3_t center )
{
	vec3_t		slavecenter;
	gentity_t	*slave;

	//Start with our center
	VectorAdd(ent->r.mins, ent->r.maxs, center);
	VectorScale(center, 0.5, center);
	for ( slave = ent->teamchain ; slave ; slave = slave->teamchain )
	{
		//Find slave's center
		VectorAdd(slave->r.mins, slave->r.maxs, slavecenter);
		VectorScale(slavecenter, 0.5, slavecenter);
		//Add that to our own, find middle
		VectorAdd(center, slavecenter, center);
		VectorScale(center, 0.5, center);
	}
}

/*
===============
SetMoverState
===============
*/
void SetMoverState( gentity_t *ent, moverState_t moverState, int time ) {
	vec3_t			delta;
	float			f;

	ent->moverState = moverState;

	ent->s.pos.trTime = time;

	if ( ent->s.pos.trDuration <= 0 )
	{//Don't allow divide by zero!
		ent->s.pos.trDuration = 1;
	}

	switch( moverState ) {
	case MOVER_POS1:
		VectorCopy( ent->pos1, ent->s.pos.trBase );
		ent->s.pos.trType = TR_STATIONARY;
		break;
	case MOVER_POS2:
		VectorCopy( ent->pos2, ent->s.pos.trBase );
		ent->s.pos.trType = TR_STATIONARY;
		break;
	case MOVER_1TO2:
		VectorCopy( ent->pos1, ent->s.pos.trBase );
		VectorSubtract( ent->pos2, ent->pos1, delta );
		f = 1000.0 / ent->s.pos.trDuration;
		VectorScale( delta, f, ent->s.pos.trDelta );
		if ( ent->alt_fire )
		{
			ent->s.pos.trType = TR_LINEAR_STOP;
		}
		else
		{
			ent->s.pos.trType = TR_NONLINEAR_STOP;
		}
		//ent->s.eFlags &= ~EF_BLOCKED_MOVER;
		break;
	case MOVER_2TO1:
		VectorCopy( ent->pos2, ent->s.pos.trBase );
		VectorSubtract( ent->pos1, ent->pos2, delta );
		f = 1000.0 / ent->s.pos.trDuration;
		VectorScale( delta, f, ent->s.pos.trDelta );
		if ( ent->alt_fire )
		{
			ent->s.pos.trType = TR_LINEAR_STOP;
		}
		else
		{
			ent->s.pos.trType = TR_NONLINEAR_STOP;
		}
		//ent->s.eFlags &= ~EF_BLOCKED_MOVER;
		break;
	}
	BG_EvaluateTrajectory( &ent->s.pos, level.time, ent->r.currentOrigin );
	trap->LinkEntity( (sharedEntity_t *)ent );
}

/*
================
MatchTeam

All entities in a mover team will move from pos1 to pos2
in the same amount of time
================
*/
void MatchTeam( gentity_t *teamLeader, int moverState, int time ) {
	gentity_t		*slave;

	for ( slave = teamLeader ; slave ; slave = slave->teamchain ) {
		SetMoverState( slave, (moverState_t) moverState, time );
	}
}



/*
================
ReturnToPos1
================
*/
void ReturnToPos1( gentity_t *ent ) {
	ent->think = 0;
	ent->nextthink = 0;
	ent->s.time = level.time;

	MatchTeam( ent, MOVER_2TO1, level.time );

	// starting sound
	G_PlayDoorLoopSound( ent );
	G_PlayDoorSound( ent, BMS_START );	//??
}


/*
================
Reached_BinaryMover
================
*/

void Reached_BinaryMover( gentity_t *ent )
{
	// stop the looping sound
	ent->s.loopSound = 0;
	ent->s.loopIsSoundset = qfalse;

	if ( ent->moverState == MOVER_1TO2 )
	{//reached open
		vec3_t	doorcenter;

		// reached pos2
		SetMoverState( ent, MOVER_POS2, level.time );

		CalcTeamDoorCenter( ent, doorcenter );

		// play sound
		G_PlayDoorSound( ent, BMS_END );

		if ( ent->wait < 0 )
		{//Done for good
			ent->think = 0;
			ent->nextthink = 0;
			ent->use = 0;
		}
		else
		{
			// return to pos1 after a delay
			ent->think = ReturnToPos1;
			if(ent->spawnflags & 8)
			{//Toggle, keep think, wait for next use?
				ent->nextthink = -1;
			}
			else
			{
				ent->nextthink = level.time + ent->wait;
			}
		}

		// fire targets
		if ( !ent->activator )
		{
			ent->activator = ent;
		}
		G_UseTargets2( ent, ent->activator, ent->opentarget );
	}
	else if ( ent->moverState == MOVER_2TO1 )
	{//closed
		vec3_t	doorcenter;

		// reached pos1
		SetMoverState( ent, MOVER_POS1, level.time );

		CalcTeamDoorCenter( ent, doorcenter );

		// play sound
		G_PlayDoorSound( ent, BMS_END );

		// close areaportals
		if ( ent->teammaster == ent || !ent->teammaster )
		{
			trap->AdjustAreaPortalState( (sharedEntity_t *)ent, qfalse );
		}
		G_UseTargets2( ent, ent->activator, ent->closetarget );
	}
	else
	{
		trap->Error( ERR_DROP, "Reached_BinaryMover: bad moverState" );
	}
}


/*
================
Use_BinaryMover_Go
================
*/
void Use_BinaryMover_Go( gentity_t *ent )
{
	int		total;
	int		partial;
//	gentity_t	*other = ent->enemy;
	gentity_t	*activator = ent->activator;

	ent->activator = activator;

	if ( ent->moverState == MOVER_POS1 )
	{
		vec3_t	doorcenter;

		// start moving 50 msec later, becase if this was player
		// triggered, level.time hasn't been advanced yet
		MatchTeam( ent, MOVER_1TO2, level.time + 50 );

		CalcTeamDoorCenter( ent, doorcenter );

		// starting sound
		G_PlayDoorLoopSound( ent );
		G_PlayDoorSound( ent, BMS_START );
		ent->s.time = level.time;

		// open areaportal
		if ( ent->teammaster == ent || !ent->teammaster ) {
			trap->AdjustAreaPortalState( (sharedEntity_t *)ent, qtrue );
		}
		G_UseTargets( ent, ent->activator );
		return;
	}

	// if all the way up, just delay before coming down
	if ( ent->moverState == MOVER_POS2 ) {
		//have to do this because the delay sets our think to Use_BinaryMover_Go
		ent->think = ReturnToPos1;
		if ( ent->spawnflags & 8 )
		{//TOGGLE doors don't use wait!
			ent->nextthink = level.time + FRAMETIME;
		}
		else
		{
			ent->nextthink = level.time + ent->wait;
		}
		G_UseTargets2( ent, ent->activator, ent->target2 );
		return;
	}

	// only partway down before reversing
	if ( ent->moverState == MOVER_2TO1 )
	{
		if ( ent->s.pos.trType == TR_NONLINEAR_STOP )
		{
			vec3_t curDelta;
			float fPartial;
			total = ent->s.pos.trDuration-50;
			VectorSubtract( ent->r.currentOrigin, ent->pos1, curDelta );
			fPartial = VectorLength( curDelta )/VectorLength( ent->s.pos.trDelta );
			VectorScale( ent->s.pos.trDelta, fPartial, curDelta );
			fPartial /= ent->s.pos.trDuration;
			fPartial /= 0.001f;
			fPartial = acos( fPartial );
			fPartial = RAD2DEG( fPartial );
			fPartial = (90.0f - fPartial)/90.0f*ent->s.pos.trDuration;
			partial = total - floor( fPartial );
		}
		else
		{
			total = ent->s.pos.trDuration;
			partial = level.time - ent->s.pos.trTime;
		}

		if ( partial > total ) {
			partial = total;
		}
		ent->s.pos.trTime = level.time - ( total - partial );//ent->s.time;

		MatchTeam( ent, MOVER_1TO2, ent->s.pos.trTime );

		G_PlayDoorSound( ent, BMS_START );

		return;
	}

	// only partway up before reversing
	if ( ent->moverState == MOVER_1TO2 )
	{
		if ( ent->s.pos.trType == TR_NONLINEAR_STOP )
		{
			vec3_t curDelta;
			float fPartial;
			total = ent->s.pos.trDuration-50;
			VectorSubtract( ent->r.currentOrigin, ent->pos2, curDelta );
			fPartial = VectorLength( curDelta )/VectorLength( ent->s.pos.trDelta );
			VectorScale( ent->s.pos.trDelta, fPartial, curDelta );
			fPartial /= ent->s.pos.trDuration;
			fPartial /= 0.001f;
			fPartial = acos( fPartial );
			fPartial = RAD2DEG( fPartial );
			fPartial = (90.0f - fPartial)/90.0f*ent->s.pos.trDuration;
			partial = total - floor( fPartial );
		}
		else
		{
			total = ent->s.pos.trDuration;
			partial = level.time - ent->s.pos.trTime;
		}
		if ( partial > total ) {
			partial = total;
		}

		ent->s.pos.trTime = level.time - ( total - partial );//ent->s.time;
		MatchTeam( ent, MOVER_2TO1, ent->s.pos.trTime );

		G_PlayDoorSound( ent, BMS_START );

		return;
	}
}

void UnLockDoors(gentity_t *const ent)
{
	//noise?
	//go through and unlock the door and all the slaves
	gentity_t	*slave = ent;
	do
	{	// want to allow locked toggle doors, so keep the targetname
		if( !(slave->spawnflags & MOVER_TOGGLE) )
		{
			slave->targetname = NULL;//not usable ever again
		}
		slave->spawnflags &= ~MOVER_LOCKED;
		slave->s.frame = 1;//second stage of anim
		slave = slave->teamchain;
	} while  ( slave );
}
void LockDoors(gentity_t *const ent)
{
	//noise?
	//go through and lock the door and all the slaves
	gentity_t	*slave = ent;
	do
	{
		slave->spawnflags |= MOVER_LOCKED;
		slave->s.frame = 0;//first stage of anim
		slave = slave->teamchain;
	} while  ( slave );
}
/*
================
Use_BinaryMover
================
*/
/*
================
G_MoverValidTeamMaster

GalaxyRP fix: [Entity System] the master a mover team's slave stands for, or NULL. Use_BinaryMover() handed a
slave's use to ent->teammaster unchecked. A NULL one crashed on the "->use", and a master that is itself marked
as a slave recursed there -- forever if the two point at each other, since that call does not pass through
GlobalUse()'s limits. Team links are raw pointers kept from G_FindTeams() while entities are removed and slots
reused under them, so the master counts only when it is a live entity that is not this one and not a slave
itself. And only when it is still a team master, a mover, and of this slave's own team (the "team" key
G_FindTeams() linked them by, which movers keep): after /entremove of the master door, /entadd can hand its
slot to anything -- another team's door included -- which a slave would otherwise run door logic on, setting an
unrelated entity moving. GLX_U: a function of its own so Touch_Button() can press a button team's master too.
================
*/
static gentity_t *G_MoverValidTeamMaster( gentity_t *ent )
{
	gentity_t *master = ent->teammaster;

	if ( !master || master == ent || !master->inuse || ( master->flags & FL_TEAMSLAVE ) ||
		master->teammaster != master || master->s.eType != ET_MOVER ||
		!master->team || !ent->team || strcmp( master->team, ent->team ) )
	{
		return NULL;
	}
	return master;
}

void Use_BinaryMover( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	if ( !ent->use )
	{//I cannot be used anymore, must be a door with a wait of -1 that's opened.
		return;
	}

	// only the master should be used (G_MoverValidTeamMaster: otherwise the slave does nothing)
	if ( ent->flags & FL_TEAMSLAVE )
	{
		gentity_t *master = G_MoverValidTeamMaster( ent );

		if ( !master )
		{
			return;
		}

		Use_BinaryMover( master, other, activator );
		return;
	}

	if ( ent->flags & FL_INACTIVE )
	{
		return;
	}

	// DAJ_RP: [Locks] a player whose account is not on the lock's list cannot use it, by any route: its
	// own trigger, a button, the use key, a chain, a shot (g_locks.c). Not when the door uses itself --
	// Blocked_Door() reversing it off whatever is in the way, which is passed as the activator.
	if ( other != ent && !RP_LockAllows( ent, other, activator ) )
	{
		return;
	}

	if ( ent->spawnflags & MOVER_LOCKED )
	{//a locked door, unlock it
		UnLockDoors(ent);
		return;
	}

	G_ActivateBehavior(ent,BSET_USE);

	ent->enemy = other;
	ent->activator = activator;
	if(ent->delay)
	{
		ent->think = Use_BinaryMover_Go;
		ent->nextthink = level.time + ent->delay;
	}
	else
	{
		Use_BinaryMover_Go(ent);
	}
}

/*
================
RP_StunBatonUseMover

GalaxyRP: [Shop] what a level 5 Stun Baton does to a mover it hits (the shop's Stun Baton Upgrade did, until
DAJ_RP moved it to the skill -- see WP_FireStunBaton()). It used to be GlobalUse(),
which gives up on an inactive mover (spawnflag INACTIVE, a target_deactivate, an ICARUS
SET_INACTIVE) and only unlocks a locked door -- a second hit opened it, and UnLockDoors() also
strips a non-toggle door's name, so that door's own button or script could never open it again.

For the binary movers -- doors, plats and buttons -- the baton now unlocks and activates the whole
team for good and opens it in the same hit: FL_INACTIVE and MOVER_LOCKED are cleared on every
member (with the locked-door shader stepped to its unlocked frame, as UnLockDoors() does), the
name is kept, and the master is used like any button would use it. A locked door already has its
touch trigger from spawn (locked doors always get one), so from then on it opens for everyone;
an inactive one answers its own trigger, button or use key again. The map can still lock or
deactivate it later, and a map load restores it. Every other mover keeps plain GlobalUse().

One kind of door is handled apart: a func_door that STARTS OPEN with a wait of -1. It closes once,
when its trigger or script uses it, and Reached_BinaryMover() then clears its use function for
good ("Done for good") -- so neither GlobalUse() nor anything else could open it again, and on
cairn_assembly one shut everyone behind it in the spawn closet. The baton now reopens it: the
use function is given back to every member, the team is sent back to pos1 (open, for a door
that starts open) the way a door returns after its wait, and it stays there. The map can close it
again, and the baton can reopen it again. While it is open or reopening the baton leaves it
alone -- a plain use would close it, for good -- and while it is closing a plain use reverses it
back open, as it does for any door.

Only when it is shaped like a door, though: at least 64 units tall and taller than it is thick.
The same two settings build things as well as close doorways -- yavin_trial's staircase of 16-unit
stone steps, ns_starpad's hangar ramp, cairn_dock1's covers over a pit -- and "reopening" those
would take the stairs, the ramp or the pit cover away again. They stay GlobalUse() no-ops.
================
*/
static qboolean RP_IsOneShotClosingDoor( const gentity_t *door )
{
	float height, thickness;

	if ( !door->classname || Q_stricmp( door->classname, "func_door" ) ||
		!( door->spawnflags & 1 ) || door->wait >= 0 )	// START_OPEN; wait is in ms by now
	{
		return qfalse;
	}

	height = door->r.maxs[2] - door->r.mins[2];
	thickness = door->r.maxs[0] - door->r.mins[0];
	if ( door->r.maxs[1] - door->r.mins[1] < thickness )
	{
		thickness = door->r.maxs[1] - door->r.mins[1];
	}

	return ( height >= 64.0f && height > thickness ) ? qtrue : qfalse;
}

static void RP_StunBatonUseMoverUnlocked( gentity_t *mover, gentity_t *user );

// DAJ_RP: [Locks] the baton opens everything: account locks do not stop it, nor what it sets off
void RP_StunBatonUseMover( gentity_t *mover, gentity_t *user )
{
	RP_LockBypass( qtrue );
	RP_StunBatonUseMoverUnlocked( mover, user );
	RP_LockBypass( qfalse );
}

static void RP_StunBatonUseMoverUnlocked( gentity_t *mover, gentity_t *user )
{
	gentity_t *master, *member;

	if ( !mover || !mover->inuse )
	{
		return;
	}

	master = mover;
	if ( ( master->flags & FL_TEAMSLAVE ) && master->teammaster && master->teammaster->inuse )
	{
		master = master->teammaster;
	}

	if ( master->s.eType == ET_MOVER && RP_IsOneShotClosingDoor( master ) )
	{
		for ( member = master; member; member = member->teamchain )
		{
			member->flags &= ~FL_INACTIVE;
			if ( member->spawnflags & MOVER_LOCKED )
			{
				member->spawnflags &= ~MOVER_LOCKED;
				member->s.frame = 1;
			}
		}

		if ( master->moverState == MOVER_POS2 )
		{ // closed, for good: give the whole team its use back and send it back open
			for ( member = master; member; member = member->teamchain )
			{
				if ( !member->use )
				{
					member->use = Use_BinaryMover;
				}
			}
			master->activator = user;
			RP_LockMarkPass( master, user );	// DAJ_RP: [Locks] what it fires on the way passes too (g_locks.c)
			ReturnToPos1( master );
		}
		else if ( master->moverState == MOVER_1TO2 && master->use )
		{ // on its way shut: a use reverses it, back open
			Use_BinaryMover( master, user, user );
		}
		// open, or reopening: nothing to do
		return;
	}

	if ( mover->use != Use_BinaryMover || !mover->classname ||
		( Q_stricmp( mover->classname, "func_door" ) && Q_stricmp( mover->classname, "func_plat" ) &&
		  Q_stricmp( mover->classname, "func_button" ) ) )
	{
		GlobalUse( mover, user, user );
		return;
	}

	for ( member = master; member; member = member->teamchain )
	{
		member->flags &= ~FL_INACTIVE;
		if ( member->spawnflags & MOVER_LOCKED )
		{
			member->spawnflags &= ~MOVER_LOCKED;
			member->s.frame = 1;	// second stage of the locked-door shader anim, as UnLockDoors()
		}
	}

	Use_BinaryMover( master, user, user );
}



/*
================
InitMover

"pos1", "pos2", and "speed" should be set before calling,
so the movement delta can be calculated
================
*/
void InitMoverTrData( gentity_t *ent )
{
	vec3_t		move;
	float		distance;

	ent->s.pos.trType = TR_STATIONARY;
	VectorCopy( ent->pos1, ent->s.pos.trBase );

	// calculate time to reach second position from speed
	VectorSubtract( ent->pos2, ent->pos1, move );
	distance = VectorLength( move );
	if ( ! ent->speed )
	{
		ent->speed = 100;
	}
	VectorScale( move, ent->speed, ent->s.pos.trDelta );
	ent->s.pos.trDuration = distance * 1000 / ent->speed;
	if ( ent->s.pos.trDuration <= 0 )
	{
		ent->s.pos.trDuration = 1;
	}
}

void InitMover( gentity_t *ent )
{
	float		light;
	vec3_t		color;
	qboolean	lightSet, colorSet;

	// if the "model2" key is set, use a seperate model
	// for drawing, but clip against the brushes
	if ( ent->model2 )
	{
		if ( strstr( ent->model2, ".glm" ))
		{ //for now, not supported in MP.
			ent->s.modelindex2 = 0;
		}
		else
		{
			ent->s.modelindex2 = RP_EntityModelIndex( ent, ent->model2 );	// GalaxyRP: [Slot Reuse]
		}
	}

	// if the "color" or "light" keys are set, setup constantLight
	lightSet = G_SpawnFloat( "light", "100", &light );
	colorSet = G_SpawnVector( "color", "1 1 1", color );
	if ( lightSet || colorSet ) {
		int		r, g, b, i;

		r = color[0] * 255;
		if ( r > 255 ) {
			r = 255;
		}
		g = color[1] * 255;
		if ( g > 255 ) {
			g = 255;
		}
		b = color[2] * 255;
		if ( b > 255 ) {
			b = 255;
		}
		i = light / 4;
		if ( i > 255 ) {
			i = 255;
		}
		ent->s.constantLight = r | ( g << 8 ) | ( b << 16 ) | ( i << 24 );
	}

	ent->use = Use_BinaryMover;
	ent->reached = Reached_BinaryMover;

	ent->moverState = MOVER_POS1;
	ent->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	if ( ent->spawnflags & MOVER_INACTIVE )
	{// Make it inactive
		ent->flags |= FL_INACTIVE;
	}
	if(ent->spawnflags & MOVER_PLAYER_USE)
	{//Can be used by the player's BUTTON_USE
		ent->r.svFlags |= SVF_PLAYER_USABLE;
	}
	ent->s.eType = ET_MOVER;
	VectorCopy( ent->pos1, ent->r.currentOrigin );
	trap->LinkEntity( (sharedEntity_t *)ent );

	InitMoverTrData( ent );
}


/*
===============================================================================

DOOR

A use can be triggered either by a touch function, by being shot, or by being
targeted by another entity.

===============================================================================
*/

/*
================
Blocked_Door
================
*/
void Blocked_Door( gentity_t *ent, gentity_t *other )
{
	//determines if we need to relock after moving or not.
	qboolean relock = (ent->spawnflags & MOVER_LOCKED) ? qtrue : qfalse;
	if ( ent->damage ) {
		G_Damage( other, ent, ent, NULL, NULL, ent->damage, 0, MOD_CRUSH );
	}
	if ( ent->spawnflags & MOVER_CRUSHER ) {
		return;		// crushers don't reverse
	}

	// reverse direction
	// DAJ_RP: [Locks] as the same use going on: the one who set the door moving stays its activator, rather
	// than whoever is in the way becoming it. A player who blocked a door took over what it fires next -- with
	// a pass mark of his own on it (g_locks.c), past the locks of its opentarget, however long ago he got it --
	// and anyone blocking a door an admin had just opened with /entuse took the admin's pass away from it.
	Use_BinaryMover( ent, ent, ent->activator ? ent->activator : other );
	if(relock)
	{//door was locked before reverse move, relock door.
		LockDoors(ent);
	}
}


/*
================
Touch_DoorTriggerSpectator
================
*/
static vec3_t doorangles = { 10000000.0, 0, 0 };
static void Touch_DoorTriggerSpectator( gentity_t *ent, gentity_t *other, trace_t *trace ) {
	int axis;
	float doorMin, doorMax;
	vec3_t origin, pMins, pMaxs;
	trace_t tr;

	axis = ent->count;
	// the constants below relate to constants in Think_SpawnNewDoorTrigger()
	doorMin = ent->r.absmin[axis] + 100;
	doorMax = ent->r.absmax[axis] - 100;

	VectorCopy(other->client->ps.origin, origin);

	if (origin[axis] < doorMin || origin[axis] > doorMax) return;

	if (fabs(origin[axis] - doorMax) < fabs(origin[axis] - doorMin)) {
		origin[axis] = doorMin - 25; // 10
	} else {
		origin[axis] = doorMax + 25; // 10
	}

	VectorSet(pMins, -15.0f, -15.0f, DEFAULT_MINS_2);
	VectorSet(pMaxs, 15.0f, 15.0f, DEFAULT_MAXS_2);
	trap->Trace(&tr, origin, pMins, pMaxs, origin, other->s.number, other->clipmask, qfalse, 0, 0);
	if (!tr.startsolid &&
		!tr.allsolid &&
		tr.fraction == 1.0f &&
		tr.entityNum == ENTITYNUM_NONE)
	{
		TeleportPlayer( other, origin, doorangles );
	}
}

/*
================
Touch_DoorTrigger
================
*/
void Touch_DoorTrigger( gentity_t *ent, gentity_t *other, trace_t *trace )
{
	gentity_t *relockEnt = NULL;

	if ( other->client && other->client->sess.sessionTeam == TEAM_SPECTATOR )
	{
		// if the door is not open and not opening
		if ( ent->parent->moverState != MOVER_1TO2 &&
			ent->parent->moverState != MOVER_POS2 )
		{
			Touch_DoorTriggerSpectator( ent, other, trace );
		}
		return;
	}

	if (!ent->genericValue14 &&
		(!ent->parent || !ent->parent->genericValue14))
	{
		if (other->client && other->s.number >= MAX_CLIENTS &&
			other->s.eType == ET_NPC && other->s.NPC_class == CLASS_VEHICLE)
		{ //doors don't open for vehicles
			return;
		}

		if (other->client && other->s.number < MAX_CLIENTS &&
			other->client->ps.m_iVehicleNum)
		{ //can't open a door while on a vehicle
			return;
		}
	}

	if ( ent->flags & FL_INACTIVE )
	{
		return;
	}

	if ( ent->parent->spawnflags & MOVER_LOCKED )
	{//don't even try to use the door if it's locked
		if ( !ent->parent->alliedTeam //we don't have a "teamallow" team
			|| !other->client //we do have a "teamallow" team, but this isn't a client
			|| other->client->sess.sessionTeam != ent->parent->alliedTeam )//it is a client, but it's not on the right team
		{
			return;
		}
		else
		{//temporarily unlock us while we call Use_BinaryMover (so it doesn't unlock all the doors in this team)
			if ( ent->parent->flags & FL_TEAMSLAVE )
			{
				relockEnt = ent->parent->teammaster;
			}
			else
			{
				relockEnt = ent->parent;
			}
			if ( relockEnt != NULL )
			{
				relockEnt->spawnflags &= ~MOVER_LOCKED;
			}
		}
	}

	if ( ent->parent->moverState != MOVER_1TO2 )
	{//Door is not already opening
		//if ( ent->parent->moverState == MOVER_POS1 || ent->parent->moverState == MOVER_2TO1 )
		//{//only check these if closed or closing

		//If door is closed, opening or open, check this
		Use_BinaryMover( ent->parent, ent, other );
	}
	if ( relockEnt != NULL )
	{//re-lock us
		relockEnt->spawnflags |= MOVER_LOCKED;
	}

	/*
	//Old style
	if ( ent->parent->moverState != MOVER_1TO2 ) {
		Use_BinaryMover( ent->parent, ent, other );
	}
	*/
}

/*
======================
Think_SpawnNewDoorTrigger

All of the parts of a door have been spawned, so create
a trigger that encloses all of them
======================
*/
void Think_SpawnNewDoorTrigger( gentity_t *ent )
{
	gentity_t		*other;
	vec3_t		mins, maxs;
	int			i, best;

	// set all of the slaves as shootable
	if ( ent->takedamage )
	{
		for ( other = ent ; other ; other = other->teamchain )
		{
			other->takedamage = qtrue;
		}
	}

	// find the bounds of everything on the team
	VectorCopy (ent->r.absmin, mins);
	VectorCopy (ent->r.absmax, maxs);

	for (other = ent->teamchain ; other ; other=other->teamchain) {
		AddPointToBounds (other->r.absmin, mins, maxs);
		AddPointToBounds (other->r.absmax, mins, maxs);
	}

	// find the thinnest axis, which will be the one we expand
	best = 0;
	for ( i = 1 ; i < 3 ; i++ ) {
		if ( maxs[i] - mins[i] < maxs[best] - mins[best] ) {
			best = i;
		}
	}
	maxs[best] += 120;
	mins[best] -= 120;

	// create a trigger with this size
	other = G_Spawn ();
	VectorCopy (mins, other->r.mins);
	VectorCopy (maxs, other->r.maxs);
	other->parent = ent;
	other->r.contents = CONTENTS_TRIGGER;
	other->touch = Touch_DoorTrigger;
	trap->LinkEntity ((sharedEntity_t *)other);
	other->classname = "trigger_door";
	// remember the thinnest axis
	other->count = best;

	MatchTeam( ent, ent->moverState, level.time );
}

void Think_MatchTeam( gentity_t *ent )
{
	MatchTeam( ent, ent->moverState, level.time );
}

qboolean G_EntIsDoor( int entityNum )
{
	gentity_t *ent;

	if ( entityNum < 0 || entityNum >= ENTITYNUM_WORLD )
	{
		return qfalse;
	}

	ent = &g_entities[entityNum];
	if ( ent && !Q_stricmp( "func_door", ent->classname ) )
	{//blocked by a door
		return qtrue;
	}
	return qfalse;
}

gentity_t *G_FindDoorTrigger( gentity_t *ent )
{
	gentity_t *owner = NULL;
	gentity_t *door = ent;
	if ( door->flags & FL_TEAMSLAVE )
	{//not the master door, get the master door
		while ( door->teammaster && (door->flags&FL_TEAMSLAVE))
		{
			door = door->teammaster;
		}
	}
	if ( door->targetname )
	{//find out what is targeting it
		//FIXME: if ent->targetname, check what kind of trigger/ent is targetting it?  If a normal trigger (active, etc), then it's okay?
		while ( (owner = G_Find( owner, FOFS( target ), door->targetname )) != NULL )
		{
			if ( owner && (owner->r.contents&CONTENTS_TRIGGER) )
			{
				return owner;
			}
		}
		owner = NULL;
		while ( (owner = G_Find( owner, FOFS( target2 ), door->targetname )) != NULL )
		{
			if ( owner && (owner->r.contents&CONTENTS_TRIGGER) )
			{
				return owner;
			}
		}
	}

	owner = NULL;
	while ( (owner = G_Find( owner, FOFS( classname ), "trigger_door" )) != NULL )
	{
		if ( owner->parent == door )
		{
			return owner;
		}
	}

	return NULL;
}

qboolean G_EntIsUnlockedDoor( int entityNum )
{
	if ( entityNum < 0 || entityNum >= ENTITYNUM_WORLD )
	{
		return qfalse;
	}

	if ( G_EntIsDoor( entityNum ) )
	{
		gentity_t *ent = &g_entities[entityNum];
		gentity_t *owner = NULL;
		if ( ent->flags & FL_TEAMSLAVE )
		{//not the master door, get the master door
			while ( ent->teammaster && (ent->flags&FL_TEAMSLAVE))
			{
				ent = ent->teammaster;
			}
		}
		if ( ent->targetname )
		{//find out what is targetting it
			owner = NULL;
			//FIXME: if ent->targetname, check what kind of trigger/ent is targetting it?  If a normal trigger (active, etc), then it's okay?
			while ( (owner = G_Find( owner, FOFS( target ), ent->targetname )) != NULL )
			{
				if ( !Q_stricmp( "trigger_multiple", owner->classname ) )//FIXME: other triggers okay too?
				{
					if ( !(owner->flags & FL_INACTIVE) )
					{
						return qtrue;
					}
				}
			}
			owner = NULL;
			while ( (owner = G_Find( owner, FOFS( target2 ), ent->targetname )) != NULL )
			{
				if ( !Q_stricmp( "trigger_multiple", owner->classname ) )//FIXME: other triggers okay too?
				{
					if ( !(owner->flags & FL_INACTIVE) )
					{
						return qtrue;
					}
				}
			}
			return qfalse;
		}
		else
		{//check the door's auto-created trigger instead
			owner = G_FindDoorTrigger( ent );
			if ( owner && (owner->flags&FL_INACTIVE) )
			{//owning auto-created trigger is inactive
				return qfalse;
			}
		}
		if ( !(ent->flags & FL_INACTIVE) && //assumes that the reactivate trigger isn't right next to the door!
			!ent->health &&
			!(ent->spawnflags & MOVER_PLAYER_USE) &&
			!(ent->spawnflags & MOVER_FORCE_ACTIVATE) &&
			!(ent->spawnflags & MOVER_LOCKED))
			//FIXME: what about MOVER_GOODIE?
		{
			return qtrue;
		}
	}
	return qfalse;
}


/*QUAKED func_door (0 .5 .8) ? START_OPEN FORCE_ACTIVATE CRUSHER TOGGLE LOCKED x PLAYER_USE INACTIVE
START_OPEN	the door to moves to its destination when spawned, and operate in reverse.  It is used to temporarily or permanently close off an area when triggered (not useful for touch or takedamage doors).
FORCE_ACTIVATE	Can only be activated by a force push or pull
CRUSHER		?
TOGGLE		wait in both the start and end states for a trigger event - does NOT work on Trek doors.
LOCKED		Starts locked, with the shader animmap at the first frame and inactive.  Once used, the animmap changes to the second frame and the door operates normally.  Note that you cannot use the door again after this.
PLAYER_USE	Player can use it with the use button
INACTIVE	must be used by a target_activate before it can be used

"target"     Door fires this when it starts moving from it's closed position to its open position.
"opentarget" Door fires this after reaching its "open" position
"target2"    Door fires this when it starts moving from it's open position to its closed position.
"closetarget" Door fires this after reaching its "closed" position
"model2"	.md3 model to also draw
"angle"		determines the opening direction
"targetname" if set, no touch field will be spawned and a remote button or trigger field activates the door.
"speed"		movement speed (100 default)
"wait"		wait before returning (3 default, -1 = never return)
"delay"		when used, how many seconds to wait before moving - default is none
"lip"		lip remaining at end of move (8 default)
"dmg"		damage to inflict when blocked (2 default, set to negative for no damage)
"color"		constantLight color
"light"		constantLight radius
"health"	if set, the door must be shot open
"linear"	set to 1 and it will move linearly rather than with acceleration (default is 0)
"teamallow" even if locked, this team can always open and close it just by walking up to it
	0 - none (locked to everyone)
	1 - red
	2 - blue
"vehopen"	if non-0, vehicles/players riding vehicles can open
*/
void SP_func_door (gentity_t *ent)
{
	vec3_t	abs_movedir;
	float	distance;
	vec3_t	size;
	float	lip;
	vec3_t old_angles; // zyk: used to have the angles value, so after G_SetMovedir clears it, we can set it back

	G_SpawnInt("vehopen", "0", &ent->genericValue14);

	ent->blocked = Blocked_Door;

	// default speed of 400
	if (!ent->speed)
		ent->speed = 400;

	// default wait of 2 seconds
	if (!ent->wait)
		ent->wait = 2;
	ent->wait *= 1000;

	ent->delay *= 1000;

	// default lip of 8 units
	G_SpawnFloat( "lip", "8", &lip );

	// default damage of 2 points
	G_SpawnInt( "dmg", "2", &ent->damage );
	if ( ent->damage < 0 )
	{
		ent->damage = 0;
	}

	G_SpawnInt( "teamallow", "0", &ent->alliedTeam );

	// first position at start
	VectorCopy( ent->s.origin, ent->pos1 );

	// calculate second position
	zyk_set_brush_model( ent );

	// zyk: saving the angles value so we can recover it later
	VectorCopy(ent->s.angles, old_angles);

	G_SetMovedir( ent->s.angles, ent->movedir );
	abs_movedir[0] = fabs( ent->movedir[0] );
	abs_movedir[1] = fabs( ent->movedir[1] );
	abs_movedir[2] = fabs( ent->movedir[2] );
	VectorSubtract( ent->r.maxs, ent->r.mins, size );
	distance = DotProduct( abs_movedir, size ) - lip;
	VectorMA( ent->pos1, distance, ent->movedir, ent->pos2 );

	// if "start_open", reverse position 1 and 2
	if ( ent->spawnflags & 1 )
	{
		vec3_t	temp;

		VectorCopy( ent->pos2, temp );
		VectorCopy( ent->s.origin, ent->pos2 );
		VectorCopy( temp, ent->pos1 );
	}

	if ( ent->spawnflags & MOVER_LOCKED )
	{//a locked door, set up as locked until used directly
		ent->s.eFlags |= EF_SHADER_ANIM;//use frame-controlled shader anim
		ent->s.frame = 0;//first stage of anim
	}
	InitMover( ent );

	ent->nextthink = level.time + FRAMETIME;

	// DAJ_RP: [Force Powers] cleared first: one spawned again in place (/entedit) keeps its struct, and a mark left from
	// before an edit took MOVER_FORCE_ACTIVATE away would still show the crosshair's Force hint on it
	ent->s.bolt1 = 0;

	if ( !(ent->flags&FL_TEAMSLAVE) )
	{
		int health;

		G_SpawnInt( "health", "0", &health );

		if ( health )
		{
			ent->takedamage = qtrue;
		}

		if ( !(ent->spawnflags&MOVER_LOCKED) && (ent->targetname || health || ent->spawnflags & MOVER_PLAYER_USE || ent->spawnflags & MOVER_FORCE_ACTIVATE) )
		{
			// non touch/shoot doors
			ent->think = Think_MatchTeam;

			if (ent->spawnflags & MOVER_FORCE_ACTIVATE)
			{ //so we know it's push/pullable on the client
				// DAJ_RP: [Force Powers] 3: Push and Pull both use it (1 Push, 2 Pull -- CG_RpForceHintTarget, cg_draw.c)
				ent->s.bolt1 = 3;
			}
		}
		else
		{//locked doors still spawn a trigger
			ent->think = Think_SpawnNewDoorTrigger;
		}
	}

	// zyk: recovering the angles value, so we can save it into the entity file
	VectorCopy(old_angles, ent->s.angles);
}

/*
===============================================================================

PLAT

===============================================================================
*/

/*
==============
Touch_Plat

Don't allow descent if a living player is on it
===============
*/
void Touch_Plat( gentity_t *ent, gentity_t *other, trace_t *trace ) {
	if ( !other->client || other->client->ps.stats[STAT_HEALTH] <= 0 ) {
		return;
	}

	// delay return-to-pos1 by one second
	if ( ent->moverState == MOVER_POS2 ) {
		ent->nextthink = level.time + 1000;
	}
}

/*
==============
Touch_PlatCenterTrigger

If the plat is at the bottom position, start it going up
===============
*/
void Touch_PlatCenterTrigger(gentity_t *ent, gentity_t *other, trace_t *trace ) {
	if ( !other->client ) {
		return;
	}

	if ( ent->parent->moverState == MOVER_POS1 ) {
		Use_BinaryMover( ent->parent, ent, other );
	}
}


/*
================
SpawnPlatTrigger

Spawn a trigger in the middle of the plat's low position
Elevator cars require that the trigger extend through the entire low position,
not just sit on top of it.
================
*/
void SpawnPlatTrigger( gentity_t *ent ) {
	gentity_t	*trigger;
	vec3_t	tmin, tmax;

	// the middle trigger will be a thin trigger just
	// above the starting position
	trigger = G_Spawn();
	trigger->touch = Touch_PlatCenterTrigger;
	trigger->r.contents = CONTENTS_TRIGGER;
	trigger->parent = ent;

	tmin[0] = ent->pos1[0] + ent->r.mins[0] + 33;
	tmin[1] = ent->pos1[1] + ent->r.mins[1] + 33;
	tmin[2] = ent->pos1[2] + ent->r.mins[2];

	tmax[0] = ent->pos1[0] + ent->r.maxs[0] - 33;
	tmax[1] = ent->pos1[1] + ent->r.maxs[1] - 33;
	tmax[2] = ent->pos1[2] + ent->r.maxs[2] + 8;

	if ( tmax[0] <= tmin[0] ) {
		tmin[0] = ent->pos1[0] + (ent->r.mins[0] + ent->r.maxs[0]) *0.5;
		tmax[0] = tmin[0] + 1;
	}
	if ( tmax[1] <= tmin[1] ) {
		tmin[1] = ent->pos1[1] + (ent->r.mins[1] + ent->r.maxs[1]) *0.5;
		tmax[1] = tmin[1] + 1;
	}

	VectorCopy (tmin, trigger->r.mins);
	VectorCopy (tmax, trigger->r.maxs);

	trap->LinkEntity ((sharedEntity_t *)trigger);
}


/*QUAKED func_plat (0 .5 .8) ? x x x x x x PLAYER_USE INACTIVE
PLAYER_USE	Player can use it with the use button
INACTIVE	must be used by a target_activate before it can be used

Plats are always drawn in the extended position so they will light correctly.

"lip"		default 8, protrusion above rest position
"height"	total height of movement, defaults to model height
"speed"		overrides default 200.
"dmg"		overrides default 2
"model2"	.md3 model to also draw
"color"		constantLight color
"light"		constantLight radius
*/
void SP_func_plat (gentity_t *ent) {
	float		lip, height;

//	ent->sound1to2 = ent->sound2to1 = G_SoundIndex("sound/movers/plats/pt1_strt.wav");
//	ent->soundPos1 = ent->soundPos2 = G_SoundIndex("sound/movers/plats/pt1_end.wav");

	VectorClear (ent->s.angles);

	G_SpawnFloat( "speed", "200", &ent->speed );
	G_SpawnInt( "dmg", "2", &ent->damage );
	G_SpawnFloat( "wait", "1", &ent->wait );
	G_SpawnFloat( "lip", "8", &lip );

	ent->wait = 1000;

	// create second position
	zyk_set_brush_model( ent );

	if ( !G_SpawnFloat( "height", "0", &height ) ) {
		height = (ent->r.maxs[2] - ent->r.mins[2]) - lip;
	}

	// pos1 is the rest (bottom) position, pos2 is the top
	VectorCopy( ent->s.origin, ent->pos2 );
	VectorCopy( ent->pos2, ent->pos1 );
	ent->pos1[2] -= height;

	InitMover( ent );

	// touch function keeps the plat from returning while
	// a live player is standing on it
	if (!(ent->spawnflags & 4096)) // zyk: dont test for living player if this spawnflag is set
		ent->touch = Touch_Plat;

	ent->blocked = Blocked_Door;

	ent->parent = ent;	// so it can be treated as a door

	// spawn the trigger if one hasn't been custom made
	if ( !ent->targetname ) {
		SpawnPlatTrigger(ent);
	}
}

/*
===============================================================================

BUTTON

===============================================================================
*/

/*
==============
Touch_Button

===============
*/
void Touch_Button(gentity_t *ent, gentity_t *other, trace_t *trace ) {
	gentity_t *button = ent, *prev;

	if ( !other->client ) {
		return;
	}
	if ( !ent->use ) {
		return;		// as Use_BinaryMover(), which this was
	}
	// GLX_U: a button team's slave brush presses the team's master, as Use_BinaryMover() hands it on -- and GlobalUse()
	// below must see the master: its FL_INACTIVE (target_activate reaches only the master, so a slave of an INACTIVE
	// team kept the flag for good and stopped working), its lock and its targetname (the "use" event)
	if ( ent->flags & FL_TEAMSLAVE ) {
		button = G_MoverValidTeamMaster( ent );
		if ( !button ) {
			return;
		}
	}

	if ( button->moverState == MOVER_POS1 ) {
		// GLX_U: [Lua] through GlobalUse(), the way every other use goes: the lock, the loop limits, then a
		// script's on_use and "use" event, which can stop it. This is the Use key (TryUse), walking into the
		// button and a Force push; before, they went straight to Use_BinaryMover() and no script heard of
		// them. A button a script just stopped is left alone for a moment (RP_Lua_ButtonQuiet), or a player
		// leaning on it would run the script every frame: the scripts are told which entity is being pressed.
		if ( RP_Lua_ButtonQuiet( button ) ) {
			return;
		}
		prev = RP_Lua_ButtonPressing( button );
		GlobalUse( button, other, other );
		RP_Lua_ButtonPressing( prev );
	}
}


/*QUAKED func_button (0 .5 .8) ? x x x x x x PLAYER_USE INACTIVE
PLAYER_USE	Player can use it with the use button
INACTIVE	must be used by a target_activate before it can be used

When a button is touched, it moves some distance in the direction of it's angle, triggers all of it's targets, waits some time, then returns to it's original position where it can be triggered again.

"model2"	.md3 model to also draw
"angle"		determines the opening direction
"target"	all entities with a matching targetname will be used
"speed"		override the default 40 speed
"wait"		override the default 1 second wait (-1 = never return)
"lip"		override the default 4 pixel lip remaining at end of move
"health"	if set, the button must be killed instead of touched
"color"		constantLight color
"light"		constantLight radius
*/
void SP_func_button( gentity_t *ent ) {
	vec3_t		abs_movedir;
	float		distance;
	vec3_t		size;
	float		lip;
	vec3_t old_angles; // zyk: used to have the angles value, so after G_SetMovedir clears it, we can set it back

//	ent->sound1to2 = G_SoundIndex("sound/movers/switches/butn2.wav");

	if ( !ent->speed ) {
		ent->speed = 40;
	}

	if ( !ent->wait ) {
		ent->wait = 1;
	}
	ent->wait *= 1000;

	// first position
	VectorCopy( ent->s.origin, ent->pos1 );

	// calculate second position
	zyk_set_brush_model( ent );

	G_SpawnFloat( "lip", "4", &lip );

	VectorCopy(ent->s.angles, old_angles);
	G_SetMovedir( ent->s.angles, ent->movedir );
	abs_movedir[0] = fabs(ent->movedir[0]);
	abs_movedir[1] = fabs(ent->movedir[1]);
	abs_movedir[2] = fabs(ent->movedir[2]);
	VectorSubtract( ent->r.maxs, ent->r.mins, size );
	distance = abs_movedir[0] * size[0] + abs_movedir[1] * size[1] + abs_movedir[2] * size[2] - lip;
	VectorMA (ent->pos1, distance, ent->movedir, ent->pos2);

	if (ent->health) {
		// shootable button
		ent->takedamage = qtrue;
	} else {
		// touchable button
		ent->touch = Touch_Button;
	}

	InitMover( ent );

	VectorCopy(old_angles, ent->s.angles);
}




/*
===============================================================================

TRAIN

===============================================================================
*/


#define TRAIN_START_ON		1
#define TRAIN_TOGGLE		2
#define TRAIN_BLOCK_STOPS	4

/*
===============
Think_BeginMoving

The wait time at a corner has completed, so start moving again
===============
*/
void Think_BeginMoving( gentity_t *ent ) {
	G_PlayDoorSound( ent, BMS_START );
	G_PlayDoorLoopSound( ent );
	ent->s.pos.trTime = level.time;
	ent->s.pos.trType = TR_LINEAR_STOP;
}

/*
===============
Reached_Train
===============
*/
void Reached_Train( gentity_t *ent ) {
	gentity_t		*next;
	float			speed;
	vec3_t			move;
	float			length;

	// copy the apropriate values
	next = ent->nextTrain;
	if ( !next || !next->nextTrain ) {
		return;		// just stop
	}

	// fire all other targets
	// GalaxyRP fix: [Entity System] the activator was NULL, and several use functions dereference it
	// without a test: target_relay's red/blue-only flags, target_teleporter and target_give read
	// activator->client, target_kill damages it. A path_corner targeting any of them crashed the
	// server the moment a train reached it. The train is the activator now: it has no client, so
	// those checks pass it by as they would any non-player, and it takes no damage.
	G_UseTargets( next, ent );

	// set the new trajectory
	ent->nextTrain = next->nextTrain;
	VectorCopy( next->s.origin, ent->pos1 );
	VectorCopy( next->nextTrain->s.origin, ent->pos2 );

	// if the path_corner has a speed, use that
	if ( next->speed ) {
		speed = next->speed;
	} else {
		// otherwise use the train's speed
		speed = ent->speed;
	}
	if ( speed < 1 ) {
		speed = 1;
	}

	// calculate duration
	VectorSubtract( ent->pos2, ent->pos1, move );
	length = VectorLength( move );

	ent->s.pos.trDuration = length * 1000 / speed;

	// start it going
	SetMoverState( ent, MOVER_1TO2, level.time );

	G_PlayDoorSound( ent, BMS_END );

	// if there is a "wait" value on the target, don't start moving yet
	if ( next->wait ) {
		ent->s.loopSound = 0;
		ent->s.loopIsSoundset = qfalse;
		ent->nextthink = level.time + next->wait * 1000;
		ent->think = Think_BeginMoving;
		ent->s.pos.trType = TR_STATIONARY;
	}
	else
	{
		G_PlayDoorLoopSound( ent );
	}
}

/*
===============
Think_SetupTrainTargets

Link all the corners together
===============
*/
static qboolean rp_train_path_seen[MAX_ENTITIESTOTAL];	// GalaxyRP: see Think_SetupTrainTargets()

void Think_SetupTrainTargets( gentity_t *ent ) {
	gentity_t		*path, *next, *start;

	ent->nextTrain = G_Find( NULL, FOFS(targetname), ent->target );
	if ( !ent->nextTrain ) {
		Com_Printf( "func_train at %s with an unfound target\n",
			vtos(ent->r.absmin) );
		//Free me?`
		return;
	}

	// GalaxyRP fix: [Entity System] the FIXME that stood here was right: a path that loops back to
	// any corner but the first one never came back to start, so this never ended -- 100% CPU, every
	// client timing out, and three /entadd lines were enough to cause it:
	// t1---->t2---->t3
	//         ^      |
	//          \_____|
	// Each corner is marked as it is walked, and the walk stops once the corner it just linked has
	// been walked already. Every corner is still linked exactly as before, so a train running along
	// a tail into a loop keeps circling the loop, which is what such a path means.
	memset( rp_train_path_seen, 0, sizeof( rp_train_path_seen ) );
	start = NULL;
	for ( path = ent->nextTrain ; path != start ; path = next ) {
		if ( !start ) {
			start = path;
		}
		rp_train_path_seen[path->s.number] = qtrue;

		if ( !path->target ) {
//			trap->Printf( "Train corner at %s without a target\n",
//				vtos(path->s.origin) );
			//end of path
			break;
		}

		// find a path_corner among the targets
		// there may also be other targets that get fired when the corner
		// is reached
		next = NULL;
		do {
			next = G_Find( next, FOFS(targetname), path->target );
			if ( !next ) {
//				trap->Printf( "Train corner at %s without a target path_corner\n",
//					vtos(path->s.origin) );
				//end of path
				break;
			}
		} while ( strcmp( next->classname, "path_corner" ) );

		if ( next )
		{
			path->nextTrain = next;
			if ( rp_train_path_seen[next->s.number] )
			{ // zyk: walked already, so everything from here on is linked
				break;
			}
		}
		else
		{
			break;
		}
	}

	if ( !ent->targetname || (ent->spawnflags&1) /*start on*/)
	{
		// start the train moving from the first corner
		Reached_Train( ent );
	}
	else
	{
		G_SetOrigin( ent, ent->s.origin );
	}
}


/*QUAKED path_corner (.5 .3 0) (-8 -8 -8) (8 8 8)
Train path corners.
Target: next path corner and other targets to fire
"speed" speed to move to the next corner
"wait" seconds to wait before behining move to next corner
*/
void SP_path_corner( gentity_t *self ) {
	if ( !self->targetname ) {
		trap->Print ("path_corner with no targetname at %s\n", vtos(self->s.origin));
		RP_SpawnSaysWhy( self, "a path_corner needs a targetname: a train reaches it by name" );
		G_FreeEntity( self );
		return;
	}
	// path corners don't need to be linked in
}



/*QUAKED func_train (0 .5 .8) ? START_ON TOGGLE BLOCK_STOPS ? ? CRUSH_THROUGH PLAYER_USE INACTIVE
A train is a mover that moves between path_corner target points.
Trains MUST HAVE AN ORIGIN BRUSH.
The train spawns at the first target it is pointing at.
CRUSH_THROUGH spawnflag combined with a dmg value will make the train pass through
entities and damage them on contact as well.
"model2"	.md3 model to also draw
"speed"		default 100
"dmg"		default	2
"target"	next path corner
"color"		constantLight color
"light"		constantLight radius
*/
void SP_func_train (gentity_t *self) {
	VectorClear (self->s.angles);

	if (self->spawnflags & TRAIN_BLOCK_STOPS) {
		self->damage = 0;
	} else {
		if (!self->damage) {
			self->damage = 2;
		}
	}

	if ( !self->speed ) {
		self->speed = 100;
	}

	if ( !self->target ) {
		trap->Print ("func_train without a target at %s\n", vtos(self->r.absmin));
		RP_SpawnSaysWhy( self, "a func_train needs a target: the path_corner it starts from" );
		G_FreeEntity( self );
		return;
	}

	zyk_set_brush_model( self );

	InitMover( self );

	self->reached = Reached_Train;

	// start trains on the second frame, to make sure their targets have had
	// a chance to spawn
	self->nextthink = level.time + FRAMETIME;
	self->think = Think_SetupTrainTargets;
}

/*
===============================================================================

STATIC

===============================================================================
*/

void func_static_use ( gentity_t *self, gentity_t *other, gentity_t *activator );
/*QUAKED func_static (0 .5 .8) ? F_PUSH F_PULL SWITCH_SHADER CRUSHER IMPACT x PLAYER_USE INACTIVE BROADCAST
F_PUSH		Will be used when you Force-Push it
F_PULL		Will be used when you Force-Pull it
SWITCH_SHADER	Toggle the shader anim frame between 1 and 2 when used
CRUSHER		Make it do damage when it's blocked
IMPACT		Make it do damage when it hits any entity
PLAYER_USE	Player can use it with the use button
INACTIVE	must be used by a target_activate before it can be used
BROADCAST   don't ever use this, it's evil

A bmodel that just sits there, doing nothing.  Can be used for conditional walls and models.
"model2"	.md3 model to also draw
"model2scale"	percent of normal scale (on all x y and z axii) to scale the model2 if there is one. 100 is normal scale, min is 1 (100 times smaller than normal), max is 1000 (ten times normal).
"color"		constantLight color
"light"		constantLight radius
"dmg"		how much damage to do when it crushes (use with CRUSHER spawnflag)
"linear" set to 1 and it will move linearly rather than with acceleration (default is 0)
*/
void SP_func_static( gentity_t *ent )
{
	int		test;

	zyk_set_brush_model( ent );

	VectorCopy( ent->s.origin, ent->pos1 );
	VectorCopy( ent->s.origin, ent->pos2 );

	InitMover( ent );

	ent->use = func_static_use;
	ent->reached = 0;

	G_SetOrigin( ent, ent->s.origin );
	G_SetAngles( ent, ent->s.angles );

	if( ent->spawnflags & 2048 )
	{								   // yes this is very very evil, but for now (pre-alpha) it's a solution
		ent->r.svFlags |= SVF_BROADCAST; // I need to rotate something that is huge and it's touching too many area portals...
	}

	if ( ent->spawnflags & 4/*SWITCH_SHADER*/ )
	{
		ent->s.eFlags |= EF_SHADER_ANIM;//use frame-controlled shader anim
		ent->s.frame = 0;//first stage of anim
	}

	//so we know it's push/pullable on the client
	// DAJ_RP: [Force Powers] which power uses it, for the crosshair's Force hint (CG_RpForceHintTarget, cg_draw.c):
	// 1 Push (F_PUSH), 2 Pull (F_PULL), 3 both, 0 neither. It was 1 for either; a client that only tests it for non-zero
	// is unaffected. Set either way: one spawned again in place (/entedit) keeps its struct, and its old mark with it.
	ent->s.bolt1 = ent->spawnflags & 3;

	G_SpawnInt("model2scale", "0", &ent->s.iModelScale);
	if (ent->s.iModelScale < 0)
	{
		ent->s.iModelScale = 0;
	}
	else if (ent->s.iModelScale > 1023)
	{
		ent->s.iModelScale = 1023;
	}

	G_SpawnInt( "hyperspace", "0", &test );
	if ( test )
	{
		ent->r.svFlags |= SVF_BROADCAST; // I need to rotate something that is huge and it's touching too many area portals...
		ent->s.eFlags2 |= EF2_HYPERSPACE;
	}

	trap->LinkEntity( (sharedEntity_t *)ent );

	if (level.mBSPInstanceDepth)
	{	// GalaxyRP fix: [Entity System] part of a misc_bsp's sub-BSP: sent to every client like its misc_bsp
		// (see SP_misc_bsp(), g_misc.c) rather than made EF_PERMANENT, which the engine never sends after the
		// baseline -- a misc_bsp rebuilding its sub-BSP after map load brought its brushes back invisible.
		// Only the permanent bit goes: this used to overwrite every flag, SWITCH_SHADER's EF_SHADER_ANIM too.
		ent->s.eFlags &= ~EF_PERMANENT;
		ent->r.svFlags |= SVF_BROADCAST;
	}
}

void func_static_use ( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	G_ActivateBehavior( self, BSET_USE );

	if ( self->spawnflags & 4/*SWITCH_SHADER*/ )
	{
		self->s.frame = self->s.frame ? 0 : 1;//toggle frame
	}
	G_UseTargets( self, activator );
}

/*
===============================================================================

ROTATING

===============================================================================
*/

/*
================
RP_MoverHoldAngles / RP_MoverHoldOrigin

GalaxyRP fix: [SP Maps] stopping a mover in place. Its trajectory base becomes where the server has it
now -- r.currentAngles / r.currentOrigin, what it collides as -- not where its trajectory would put it at
level.time: a stationary mover is not run by G_RunMover(), so a base the collision never reached would
leave players walking into a blade the screen shows elsewhere. A rotator's angles are folded into
0..360 on both sides (fold), so the drawn and the colliding angles stay equal and its numbers do not
grow with every turn; a pendulum's are not, since it restarts from its own angles.
================
*/
static void RP_MoverHoldAngles( gentity_t *ent, qboolean fold )
{
	int i;

	for ( i = 0; fold && i < 3; i++ )
	{
		ent->r.currentAngles[i] = AngleNormalize360( ent->r.currentAngles[i] );
	}
	VectorCopy( ent->r.currentAngles, ent->s.apos.trBase );
	ent->s.apos.trType = TR_STATIONARY;
	ent->s.apos.trTime = level.time;
	trap->LinkEntity( (sharedEntity_t *)ent );
}

static void RP_MoverHoldOrigin( gentity_t *ent )
{
	VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	ent->s.pos.trType = TR_STATIONARY;
	ent->s.pos.trTime = level.time;
	trap->LinkEntity( (sharedEntity_t *)ent );
}

// GalaxyRP: [SP Maps] where a sine mover (TR_SINE: bobbing, pendulum) is in its cycle at level.time, 0..1
static float RP_MoverSinePhase( const trajectory_t *tr )
{
	float phase;

	if ( tr->trDuration <= 0 )
	{
		return 0.0f;
	}

	phase = fmodf( (float)( level.time - tr->trTime ) / (float)tr->trDuration, 1.0f );
	if ( phase < 0.0f )
	{
		phase += 1.0f;
	}
	return phase;
}

/*
================
RP_ToggleMover*

GalaxyRP fix: [SP Maps] starting and stopping the three movers a use toggles -- func_rotating (its spin),
func_bobbing (its bob), func_pendulum (its swing) -- for a whole team at once. A team ("team" key) moves
as one body: G_RunMover() runs it from its master, and a use of any member reaches the master (its name
is the team's: G_FindTeams() moves a member's targetname onto it). The per-entity toggles they had let a
master stop while a member went on, or the other way round: the server then held the member still while
every client, evaluating its trajectory, drew it moving.

A use goes to the master, as Use_BinaryMover() takes it there, and every rotator, bobber and pendulum of
the team is set to the opposite of what the master is doing; other members (a door) are left to their
own use. When the master is of another class, the used entity toggles alone, as before. A member of a
team whose master is gone or not its own does nothing, as Use_BinaryMover() does.

A team starts the way its master spawned (RP_ToggleMoverTeamMatch): after G_FindTeams() at map load and
after a preset, and whenever the Entity System links an entity into a team.
================
*/
typedef enum
{
	RP_TOGGLE_NONE,
	RP_TOGGLE_ROTATING,
	RP_TOGGLE_BOBBING,
	RP_TOGGLE_PENDULUM
} rpToggleKind_t;

static rpToggleKind_t RP_ToggleMoverKind( const gentity_t *e )
{
	if ( !e || !e->inuse || !e->classname || e->s.eType != ET_MOVER )
		return RP_TOGGLE_NONE;
	if ( !Q_stricmp( e->classname, "func_rotating" ) )
		return RP_TOGGLE_ROTATING;
	if ( !Q_stricmp( e->classname, "func_bobbing" ) )
		return RP_TOGGLE_BOBBING;
	if ( !Q_stricmp( e->classname, "func_pendulum" ) )
		return RP_TOGGLE_PENDULUM;
	return RP_TOGGLE_NONE;
}

static qboolean RP_ToggleMoverRunning( const gentity_t *e )
{
	switch ( RP_ToggleMoverKind( e ) )
	{
	case RP_TOGGLE_ROTATING:
		return ( e->s.apos.trType != TR_STATIONARY ) ? qtrue : qfalse;
	case RP_TOGGLE_BOBBING:
		return ( e->s.pos.trType == TR_SINE ) ? qtrue : qfalse;
	case RP_TOGGLE_PENDULUM:
		return ( e->s.apos.trType == TR_SINE ) ? qtrue : qfalse;
	default:
		return qfalse;
	}
}

// the time a sine mover restarts from so that it carries on at the place in its cycle it stopped at (radius)
static int RP_ToggleMoverResumeTime( const trajectory_t *tr, float phase )
{
	return level.time - (int)floorf( 0.5f + tr->trDuration * phase );
}

/*
Starts (run) or stops one of them. Stopping holds it where the server has it (RP_MoverHoldAngles /
RP_MoverHoldOrigin) and keeps its place in the cycle in radius, as single player keeps it; starting
carries on from there. atSpawn: it has only just spawned, so it has never moved -- a bobber or a pendulum
is then held where it stands with its cycle at the start (radius 0, which is where it stands), and a
rotator makes no start or stop sound, only its loop.
*/
static void RP_ToggleMoverSet( gentity_t *e, qboolean run, qboolean atSpawn )
{
	const rpToggleKind_t kind = RP_ToggleMoverKind( e );

	if ( kind == RP_TOGGLE_NONE || RP_ToggleMoverRunning( e ) == run )
		return;

	switch ( kind )
	{
	case RP_TOGGLE_ROTATING:
		if ( run )
		{
			if ( e->soundSet && e->soundSet[0] )
			{
				e->s.soundSetIndex = G_SoundSetIndex( e->soundSet );
				if ( !atSpawn )
					G_AddEvent( e, EV_BMODEL_SOUND, BMS_START );
				e->s.loopSound = BMS_MID;
				e->s.loopIsSoundset = qtrue;
			}
			// from the angles it stopped at, starting now
			VectorCopy( e->r.currentAngles, e->s.apos.trBase );
			e->s.apos.trTime = level.time;
			e->s.apos.trType = TR_LINEAR;
		}
		else
		{
			RP_MoverHoldAngles( e, qtrue );
			// stop the sound if it stops moving
			e->s.loopSound = 0;
			e->s.loopIsSoundset = qfalse;
			if ( !atSpawn && e->soundSet && e->soundSet[0] )
			{
				e->s.soundSetIndex = G_SoundSetIndex( e->soundSet );
				G_AddEvent( e, EV_BMODEL_SOUND, BMS_END );
			}
		}
		break;

	case RP_TOGGLE_BOBBING:
		if ( run )
		{
			VectorCopy( e->s.origin, e->s.pos.trBase );
			e->s.pos.trTime = RP_ToggleMoverResumeTime( &e->s.pos, e->radius );
			e->s.pos.trType = TR_SINE;
		}
		else if ( atSpawn )
		{
			e->radius = 0.0f;
			VectorCopy( e->s.origin, e->r.currentOrigin );
			RP_MoverHoldOrigin( e );
		}
		else
		{
			e->radius = RP_MoverSinePhase( &e->s.pos );
			RP_MoverHoldOrigin( e );
		}
		break;

	case RP_TOGGLE_PENDULUM:
		if ( run )
		{
			VectorCopy( e->s.angles, e->s.apos.trBase );
			e->s.apos.trTime = RP_ToggleMoverResumeTime( &e->s.apos, e->radius );
			e->s.apos.trType = TR_SINE;
		}
		else if ( atSpawn )
		{
			e->radius = 0.0f;
			VectorCopy( e->s.angles, e->r.currentAngles );
			RP_MoverHoldAngles( e, qfalse );
		}
		else
		{
			e->radius = RP_MoverSinePhase( &e->s.apos );
			RP_MoverHoldAngles( e, qfalse );
		}
		break;

	default:
		break;
	}
}

// the master a member's use goes to: itself when it is in no team, NULL when its team is broken
static gentity_t *RP_ToggleMoverMaster( gentity_t *self )
{
	gentity_t *master;

	if ( !( self->flags & FL_TEAMSLAVE ) )
		return self;

	master = self->teammaster;
	if ( !master || master == self || !master->inuse || ( master->flags & FL_TEAMSLAVE ) ||
		master->teammaster != master || !master->team || !self->team || strcmp( master->team, self->team ) )
	{
		return NULL;
	}
	return master;
}

static void RP_ToggleMoverUse( gentity_t *self )
{
	gentity_t *master = RP_ToggleMoverMaster( self );
	gentity_t *part;
	qboolean run;
	int steps;

	if ( !master )
		return;

	if ( RP_ToggleMoverKind( master ) == RP_TOGGLE_NONE )
	{	// a door or another mover leads the team: this one toggles alone
		G_ActivateBehavior( self, BSET_USE );
		RP_ToggleMoverSet( self, RP_ToggleMoverRunning( self ) ? qfalse : qtrue, qfalse );
		return;
	}

	if ( master != self && ( master->flags & FL_INACTIVE ) )
		return;

	G_ActivateBehavior( master, BSET_USE );

	run = RP_ToggleMoverRunning( master ) ? qfalse : qtrue;
	for ( part = master, steps = 0; part && steps < MAX_GENTITIES; part = part->teamchain, steps++ )
	{
		if ( !part->inuse )
			break;
		RP_ToggleMoverSet( part, run, qfalse );
	}
}

/*
================
RP_ToggleMoverTeamMatch

GalaxyRP fix: [SP Maps] the rotators, bobbers and pendulums of the master's team take the state the master
is in -- spinning or still -- so the team starts as one body the way its master spawned: the first of the
team in the map, whose own name and START_ON / START_OFF decide. Its members' were decided by each one
alone when it spawned, before G_FindTeams() moved a member's name onto the master: a named member started
still and, its name gone, nothing could start it. Called after G_FindTeams() and when the Entity System
links an entity into a team (zyk_main_spawn_entity()), so on members that have only just spawned.
================
*/
void RP_ToggleMoverTeamMatch( gentity_t *master )
{
	gentity_t *part;
	qboolean run;
	int steps;

	if ( !master || !master->inuse || master->teammaster != master || !master->teamchain ||
		RP_ToggleMoverKind( master ) == RP_TOGGLE_NONE )
	{
		return;
	}

	run = RP_ToggleMoverRunning( master );
	for ( part = master->teamchain, steps = 0; part && steps < MAX_GENTITIES; part = part->teamchain, steps++ )
	{
		if ( !part->inuse )
			break;
		RP_ToggleMoverSet( part, run, qtrue );
	}
}

// every team in the map, after G_FindTeams()
void RP_ToggleMoverTeamsMatch( void )
{
	int i;

	for ( i = MAX_CLIENTS; i < level.num_entities; i++ )
	{
		gentity_t *e = &g_entities[i];

		if ( e->inuse && e->teammaster == e && e->teamchain )
			RP_ToggleMoverTeamMatch( e );
	}
}

/*
================
func_rotating_use

GalaxyRP fix: [SP Maps] this existed but nothing assigned it: InitMover() left every func_rotating with
Use_BinaryMover(), and a use sent it to pos1 -- the world origin 0 0 0 -- in the middle of the map. Now
SP_func_rotating() gives it to every rotator without health (single player gives it to the named ones;
here the Stun Baton and PLAYER_USE reach unnamed ones too). It stops the spin where it is
(RP_MoverHoldAngles) and starts it again from there -- for its whole team: RP_ToggleMoverUse().
================
*/
void func_rotating_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	RP_ToggleMoverUse( self );
}

/*QUAKED func_rotating (0 .5 .8) ? START_ON RADAR X_AXIS Y_AXIS IMPACT x PLAYER_USE INACTIVE
RADAR - shows up on Radar in Siege mode (good for asteroids when you're in a ship)
IMPACT - Kills anything on impact when rotating (10000 points of damage, unless specified otherwise)

You need to have an origin brush as part of this entity.  The center of that brush will be
the point around which it is rotated. It will rotate around the Z axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"model2"		.md3 model to also draw
"model2scale"	percent of normal scale (on all x y and z axii) to scale the model2 if there is one. 100 is normal scale, min is 1 (100 times smaller than normal), max is 1000 (ten times normal).
"speed"			determines how fast it moves; default value is 100.
"dmg"			damage to inflict when blocked (2 default)
"color"			constantLight color
"light"			constantLight radius
"spinangles"	instead of using "speed", you can set use this to set rotation on all 3 axes (pitch yaw and roll)

==================================================
"health"	default is 0

If health is set, the following key/values are available:

"numchunks" Multiplies the number of chunks spawned.  Chunk code tries to pick a good volume of chunks, but you can alter this to scale the number of spawned chunks. (default 1)  (.5) is half as many chunks, (2) is twice as many chunks
"chunksize"	scales up/down the chunk size by this number (default is 1)

"showhealth" if non-0, will display the health bar on the hud when the crosshair is over this ent (in siege)
"teamowner" in siege this will specify which team this thing is "owned" by. To that team the crosshair will
highlight green, to the other team it will highlight red.

Damage: default is none
"splashDamage" - damage to do
"splashRadius" - radius for above damage

"team" - If set, only this team can trip this trigger
	0 - any
	1 - red
	2 - blue

Don't know if these work:
"color"		constantLight color
"light"		constantLight radius

"material" - default is "0 - MAT_METAL" - choose from this list:
0 = MAT_METAL		(basic blue-grey scorched-DEFAULT)
1 = MAT_GLASS
2 = MAT_ELECTRICAL	(sparks only)
3 = MAT_ELEC_METAL	(METAL2 chunks and sparks)
4 =	MAT_DRK_STONE	(brown stone chunks)
5 =	MAT_LT_STONE	(tan stone chunks)
6 =	MAT_GLASS_METAL (glass and METAL2 chunks)
7 = MAT_METAL2		(electronic type of metal)
8 = MAT_NONE		(no chunks)
9 = MAT_GREY_STONE	(grey colored stone)
10 = MAT_METAL3		(METAL and METAL2 chunk combo)
11 = MAT_CRATE1		(yellow multi-colored crate chunks)
12 = MAT_GRATE1		(grate chunks--looks horrible right now)
13 = MAT_ROPE		(for yavin_trial, no chunks, just wispy bits )
14 = MAT_CRATE2		(red multi-colored crate chunks)
15 = MAT_WHITE_METAL (white angular chunks for Stu, NS_hideout )
16 = MAT_SNOWY_ROCK	 (mix of gray and brown rocks)

Applicable only during Siege gametype:
teamnodmg - if 1, team 1 can't damage this. If 2, team 2 can't damage this.

*/
void SP_func_breakable( gentity_t *self );
void SP_func_rotating (gentity_t *ent) {
	vec3_t spinangles;
	const qboolean breakable = ent->health ? qtrue : qfalse;	// GalaxyRP: [SP Maps] see the use below
	if ( breakable )
	{
		int sav_spawnflags = ent->spawnflags;
		// DAJ_RP fix: [Entity System] all but SOLID (1024): func_rotating's flags mean other things to a breakable
		// (16 IMPACT would be SABERONLY), which is why stock clears them here -- but SOLID is what makes an .md3
		// model collide at all (zyk_set_brush_model()), and the same on both: without it an .md3 func_rotating with
		// health could not be shot, so it could never break
		ent->spawnflags = sav_spawnflags & 1024;
		SP_func_breakable( ent );
		// GalaxyRP fix: [Entity System] SP_func_breakable() frees an entity with no brush model
		// instead of ending the server; everything below would then be written into a freed slot.
		if ( !ent->inuse )
		{
			return;
		}
		ent->spawnflags = sav_spawnflags;
	}
	else
	{
		zyk_set_brush_model( ent );

		InitMover( ent );

		VectorCopy( ent->s.origin, ent->s.pos.trBase );
		VectorCopy( ent->s.pos.trBase, ent->r.currentOrigin );
		VectorCopy( ent->s.apos.trBase, ent->r.currentAngles );

		trap->LinkEntity( (sharedEntity_t *)ent );
	}

	G_SpawnInt("model2scale", "0", &ent->s.iModelScale);
	if (ent->s.iModelScale < 0)
	{
		ent->s.iModelScale = 0;
	}
	else if (ent->s.iModelScale > 1023)
	{
		ent->s.iModelScale = 1023;
	}

	if ( G_SpawnVector( "spinangles", "0 0 0", spinangles ) )
	{
		ent->speed = VectorLength( spinangles );
		// set the axis of rotation
		VectorCopy( spinangles, ent->s.apos.trDelta );
	}
	else
	{
		// zyk: in this case, it is possible to use trdelta as an argument of entadd or entedit

		if ( !ent->speed ) {
			ent->speed = 100;
		}
		// set the axis of rotation
		if ( ent->spawnflags & 4 ) {
			ent->s.apos.trDelta[2] = ent->speed;
		} else if ( ent->spawnflags & 8 ) {
			ent->s.apos.trDelta[0] = ent->speed;
		} else {
			ent->s.apos.trDelta[1] = ent->speed;
		}
	}
	ent->s.apos.trType = TR_LINEAR;
	// GalaxyRP fix: [SP Maps] spinning from its own angles now: trTime was left 0, so one added after map load
	// started at the angle it would have reached spinning since the map began, and its first frame on the
	// server turned it -- and pushed whatever stood by it -- through all of that at once
	ent->s.apos.trTime = level.time;

	// GalaxyRP fix: [SP Maps] using a func_rotating ran Use_BinaryMover() (InitMover's), which sent it to
	// the world origin 0 0 0; one with health keeps func_breakable's use (it breaks). Every other one now
	// starts and stops with a use. As in single player, a named one starts still unless START_ON (1) --
	// MP used to ignore the flag and spin them all from spawn. An unnamed one has nothing that could
	// start it, so it spins from spawn as before.
	if ( !breakable )
	{
		ent->use = func_rotating_use;

		if ( ent->targetname && ent->targetname[0] && !( ent->spawnflags & 1 ) )
		{
			ent->s.apos.trType = TR_STATIONARY;
		}
		else
		{
			G_PlayDoorLoopSound( ent );	// a spinning one hums its soundSet's loop from the start
		}
	}

	if (!ent->damage) {
		if ( (ent->spawnflags&16) )//IMPACT
		{
			ent->damage = 10000;
		}
		else
		{
			ent->damage = 2;
		}
	}
	if ( (ent->spawnflags&2) )//RADAR
	{//show up on Radar at close range and play impact sound when close...?  Range based on my size
		ent->s.speed = Distance( ent->r.absmin, ent->r.absmax )*0.5f;
		ent->s.eFlags |= EF_RADAROBJECT;
	}
}


/*
===============================================================================

BOBBING

===============================================================================
*/


/*
================
func_bobbing_use

GalaxyRP fix: [SP Maps] single player's toggle. Without it a func_bobbing kept InitMover()'s
Use_BinaryMover(), and any use -- a target, PLAYER_USE, the Stun Baton -- sent it to the world origin
0 0 0, where it sat still. It now stops where it is, keeping its place in the cycle (radius, as single
player keeps it), and the next use carries on from that place.
================
*/
void func_bobbing_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	RP_ToggleMoverUse( self );	// for its whole team, see RP_ToggleMover*
}

/*QUAKED func_bobbing (0 .5 .8) ? X_AXIS Y_AXIS START_OFF x x x PLAYER_USE INACTIVE
Normally bobs on the Z axis
"model2"	.md3 model to also draw
"height"	amplitude of bob (32 default)
"speed"		seconds to complete a bob cycle (4 default)
"phase"		the 0.0 to 1.0 offset in the cycle to start at
"dmg"		damage to inflict when blocked (2 default)
"color"		constantLight color
"light"		constantLight radius
*/
void SP_func_bobbing (gentity_t *ent) {
	float		height;
	float		phase;

	G_SpawnFloat( "speed", "4", &ent->speed );
	G_SpawnFloat( "height", "32", &height );
	G_SpawnInt( "dmg", "2", &ent->damage );
	G_SpawnFloat( "phase", "0", &phase );

	zyk_set_brush_model( ent );

	InitMover( ent );

	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );

	ent->s.pos.trDuration = ent->speed * 1000;
	ent->s.pos.trTime = ent->s.pos.trDuration * phase;
	ent->s.pos.trType = TR_SINE;

	// set the axis of bobbing
	if ( ent->spawnflags & 1 ) {
		ent->s.pos.trDelta[0] = height;
	} else if ( ent->spawnflags & 2 ) {
		ent->s.pos.trDelta[1] = height;
	} else {
		ent->s.pos.trDelta[2] = height;
	}

	// GalaxyRP fix: [SP Maps] using it starts and stops it (func_bobbing_use), and START_OFF (4) holds it
	// still from spawn at the place in the cycle its phase gives -- both as single player has them
	ent->use = func_bobbing_use;
	if ( ent->spawnflags & 4 )
	{
		ent->radius = phase;
		VectorMA( ent->s.pos.trBase, sin( phase * M_PI * 2 ), ent->s.pos.trDelta, ent->s.pos.trBase );
		VectorCopy( ent->s.pos.trBase, ent->r.currentOrigin );
		ent->s.pos.trType = TR_STATIONARY;
		trap->LinkEntity( (sharedEntity_t *)ent );
	}
}

/*
===============================================================================

PENDULUM

===============================================================================
*/


/*
================
func_pendulum_use

GalaxyRP fix: [SP Maps] the swing's counterpart of func_bobbing_use. Single player gives a pendulum no use
at all, so it kept InitMover()'s Use_BinaryMover() -- a use sent it to the world origin 0 0 0. It now
stops mid-swing where it is and carries on from the same place in its swing on the next use.
================
*/
void func_pendulum_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	RP_ToggleMoverUse( self );	// for its whole team, see RP_ToggleMover*
}

/*QUAKED func_pendulum (0 .5 .8) ? x x START_OFF x x x PLAYER_USE INACTIVE
You need to have an origin brush as part of this entity.
Pendulums always swing north / south on unrotated models.  Add an angles field to the model to allow rotation in other directions.
Pendulum frequency is a physical constant based on the length of the beam and gravity.
"model2"	.md3 model to also draw
"speed"		the number of degrees each way the pendulum swings, (30 default)
"phase"		the 0.0 to 1.0 offset in the cycle to start at
"dmg"		damage to inflict when blocked (2 default)
"color"		constantLight color
"light"		constantLight radius
*/
void SP_func_pendulum(gentity_t *ent) {
	float		freq;
	float		length;
	float		phase;
	float		speed;

	G_SpawnFloat( "speed", "30", &speed );
	G_SpawnInt( "dmg", "2", &ent->damage );
	G_SpawnFloat( "phase", "0", &phase );

	zyk_set_brush_model( ent );

	// find pendulum length
	length = fabs( ent->r.mins[2] );
	if ( length < 8 ) {
		length = 8;
	}

	freq = 1 / ( M_PI * 2 ) * sqrt( g_gravity.value / ( 3 * length ) );

	ent->s.pos.trDuration = ( 1000 / freq );

	InitMover( ent );

	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );

	VectorCopy( ent->s.angles, ent->s.apos.trBase );

	ent->s.apos.trDuration = 1000 / freq;
	ent->s.apos.trTime = ent->s.apos.trDuration * phase;
	ent->s.apos.trType = TR_SINE;
	ent->s.apos.trDelta[2] = speed;

	// GalaxyRP fix: [SP Maps] using it stops and restarts the swing (func_pendulum_use); START_OFF (4)
	// holds it still from spawn at the point of the swing its phase gives
	ent->use = func_pendulum_use;
	if ( ent->spawnflags & 4 )
	{
		ent->radius = phase;
		VectorMA( ent->s.apos.trBase, sin( phase * M_PI * 2 ), ent->s.apos.trDelta, ent->s.apos.trBase );
		VectorCopy( ent->s.apos.trBase, ent->r.currentAngles );
		ent->s.apos.trType = TR_STATIONARY;
		trap->LinkEntity( (sharedEntity_t *)ent );
	}
}

/*
===============================================================================

BREAKABLE BRUSH

===============================================================================
*/
//---------------------------------------------------
static void CacheChunkEffects( material_t material )
{
	switch( material )
	{
	case MAT_GLASS:
		G_EffectIndex( "chunks/glassbreak" );
		break;
	case MAT_GLASS_METAL:
		G_EffectIndex( "chunks/glassbreak" );
		G_EffectIndex( "chunks/metalexplode" );
		break;
	case MAT_ELECTRICAL:
	case MAT_ELEC_METAL:
		G_EffectIndex( "chunks/sparkexplode" );
		break;
	case MAT_METAL:
	case MAT_METAL2:
	case MAT_METAL3:
	case MAT_CRATE1:
	case MAT_CRATE2:
		G_EffectIndex( "chunks/metalexplode" );
		break;
	case MAT_GRATE1:
		G_EffectIndex( "chunks/grateexplode" );
		break;
	case MAT_DRK_STONE:
	case MAT_LT_STONE:
	case MAT_GREY_STONE:
	case MAT_WHITE_METAL: // what is this crap really supposed to be??
	case MAT_SNOWY_ROCK:
		G_EffectIndex( "chunks/rockbreaklg" );
		G_EffectIndex( "chunks/rockbreakmed" );
		break;
	case MAT_ROPE:
		G_EffectIndex( "chunks/ropebreak" );
//		G_SoundIndex(); // FIXME: give it a sound
		break;
	default:
		break;
	}
}

void G_MiscModelExplosion( vec3_t mins, vec3_t maxs, int size, material_t chunkType )
{
	gentity_t *te;
	vec3_t mid;

	VectorAdd( mins, maxs, mid );
	VectorScale( mid, 0.5f, mid );

	te = G_TempEntity( mid, EV_MISC_MODEL_EXP );

	VectorCopy(maxs, te->s.origin2);
	VectorCopy(mins, te->s.angles2);
	te->s.time = size;
	te->s.eventParm = chunkType;
}

void G_Chunks( int owner, vec3_t origin, const vec3_t normal, const vec3_t mins, const vec3_t maxs,
						float speed, int numChunks, material_t chunkType, int customChunk, float baseScale )
{
	gentity_t *te = G_TempEntity( origin, EV_DEBRIS );

	//Now it's time to cram everything horribly into the entitystate of an event entity.
	te->s.owner = owner;
	VectorCopy(origin, te->s.origin);
	VectorCopy(normal, te->s.angles);
	VectorCopy(maxs, te->s.origin2);
	VectorCopy(mins, te->s.angles2);
	te->s.speed = speed;
	te->s.eventParm = numChunks;
	te->s.trickedentindex = chunkType;
	te->s.modelindex = customChunk;
	te->s.apos.trBase[0] = baseScale;
}

//--------------------------------------
void funcBBrushDieGo (gentity_t *self)
{
	vec3_t		org, dir, up;
	gentity_t	*attacker = self->enemy;
	float		scale;
	int			i, numChunks, size = 0;
	material_t	chunkType = self->material;

	// if a missile is stuck to us, blow it up so we don't look dumb
	for ( i = 0; i < MAX_GENTITIES; i++ )
	{
		if ( g_entities[i].s.groundEntityNum == self->s.number && ( g_entities[i].s.eFlags & EF_MISSILE_STICK ))
		{
			G_Damage( &g_entities[i], self, self, NULL, NULL, 99999, 0, MOD_CRUSH ); //?? MOD?
		}
	}

	//So chunks don't get stuck inside me
	self->s.solid = 0;
	self->r.contents = 0;
	self->clipmask = 0;
	trap->LinkEntity((sharedEntity_t *)self);

	VectorSet(up, 0, 0, 1);

	if ( self->target && attacker != NULL )
	{
		G_UseTargets(self, attacker);
	}

	VectorSubtract( self->r.absmax, self->r.absmin, org );// size

	numChunks = Q_flrand(0.0f, 1.0f) * 6 + 18;

	// This formula really has no logical basis other than the fact that it seemed to be the closest to yielding the results that I wanted.
	// Volume is length * width * height...then break that volume down based on how many chunks we have
	scale = sqrt( sqrt( org[0] * org[1] * org[2] )) * 1.75f;

	if ( scale > 48 )
	{
		size = 2;
	}
	else if ( scale > 24 )
	{
		size = 1;
	}

	scale = scale / numChunks;

	if ( self->radius > 0.0f )
	{
		// designer wants to scale number of chunks, helpful because the above scale code is far from perfect
		//	I do this after the scale calculation because it seems that the chunk size generally seems to be very close, it's just the number of chunks is a bit weak
		numChunks *= self->radius;
	}

	VectorMA( self->r.absmin, 0.5, org, org );
	VectorAdd( self->r.absmin,self->r.absmax, org );
	VectorScale( org, 0.5f, org );

	if ( attacker != NULL && attacker->client )
	{
		VectorSubtract( org, attacker->r.currentOrigin, dir );
		VectorNormalize( dir );
	}
	else
	{
		VectorCopy(up, dir);
	}

	if ( !(self->spawnflags & 2048) ) // NO_EXPLOSION
	{
		// we are allowed to explode
		G_MiscModelExplosion( self->r.absmin, self->r.absmax, size, chunkType );
	}

	if (self->genericValue15)
	{ //a custom effect to play
		vec3_t ang;
		VectorSet(ang, 0.0f, 1.0f, 0.0f);
		G_PlayEffectID(self->genericValue15, org, ang);
	}

	if ( self->splashDamage > 0 && self->splashRadius > 0 )
	{
		gentity_t *te;
		//explode
		G_RadiusDamage( org, self, self->splashDamage, self->splashRadius, self, NULL, MOD_UNKNOWN );

		te = G_TempEntity( org, EV_GENERAL_SOUND );
		te->s.eventParm = G_SoundIndex("sound/weapons/explosions/cargoexplode.wav");
	}

	//FIXME: base numChunks off size?
	G_Chunks( self->s.number, org, dir, self->r.absmin, self->r.absmax, 300, numChunks, chunkType, 0, (scale*self->mass) );

	trap->AdjustAreaPortalState( (sharedEntity_t *)self, qtrue );
	self->think = G_FreeEntity;
	self->nextthink = level.time + 50;
	//G_FreeEntity( self );
}

void funcBBrushDie (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
{
	self->takedamage = qfalse;//stop chain reaction runaway loops

	self->enemy = attacker;

	if(self->delay)
	{
		self->think = funcBBrushDieGo;
		self->nextthink = level.time + floor(self->delay * 1000.0f);
		return;
	}

	funcBBrushDieGo(self);
}

void funcBBrushUse (gentity_t *self, gentity_t *other, gentity_t *activator)
{
	G_ActivateBehavior( self, BSET_USE );
	if(self->spawnflags & 64)
	{//Using it doesn't break it, makes it use it's targets
		if(self->target && self->target[0])
		{
			G_UseTargets(self, activator);
		}
	}
	else
	{
		funcBBrushDie(self, other, activator, self->health, MOD_UNKNOWN);
	}
}

void funcBBrushPain(gentity_t *self, gentity_t *attacker, int damage)
{
	if ( self->painDebounceTime > level.time )
	{
		return;
	}

	if ( self->paintarget && self->paintarget[0] )
	{
		if (!self->activator)
		{
			if (attacker && attacker->inuse && attacker->client)
			{
				G_UseTargets2 (self, attacker, self->paintarget);
			}
		}
		else
		{
			G_UseTargets2 (self, self->activator, self->paintarget);
		}
	}

	G_ActivateBehavior( self, BSET_PAIN );

	if ( self->material == MAT_DRK_STONE
		|| self->material == MAT_LT_STONE
		|| self->material == MAT_GREY_STONE
		|| self->material == MAT_SNOWY_ROCK )
	{
		vec3_t	org, dir;
		float	scale;
		int		numChunks;
		VectorSubtract( self->r.absmax, self->r.absmin, org );// size
		// This formula really has no logical basis other than the fact that it seemed to be the closest to yielding the results that I wanted.
		// Volume is length * width * height...then break that volume down based on how many chunks we have
		scale = VectorLength( org ) / 100.0f;
		VectorMA( self->r.absmin, 0.5, org, org );
		VectorAdd( self->r.absmin,self->r.absmax, org );
		VectorScale( org, 0.5f, org );
		if ( attacker != NULL && attacker->client )
		{
			VectorSubtract( attacker->r.currentOrigin, org, dir );
			VectorNormalize( dir );
		}
		else
		{
			VectorSet( dir, 0, 0, 1 );
		}
		numChunks = Q_irand( 1, 3 );
		if ( self->radius > 0.0f )
		{
			// designer wants to scale number of chunks, helpful because the above scale code is far from perfect
			//	I do this after the scale calculation because it seems that the chunk size generally seems to be very close, it's just the number of chunks is a bit weak
			numChunks = ceil(numChunks*self->radius);
		}
		G_Chunks( self->s.number, org, dir, self->r.absmin, self->r.absmax, 300, numChunks, self->material, 0, (scale*self->mass) );
	}

	if ( self->wait == -1 )
	{
		self->pain = 0;
		return;
	}

	self->painDebounceTime = level.time + self->wait;
}

static void InitBBrush ( gentity_t *ent )
{
	float		light;
	vec3_t		color;
	qboolean	lightSet, colorSet;

	VectorCopy( ent->s.origin, ent->pos1 );

	zyk_set_brush_model( ent );

	ent->die = funcBBrushDie;

	ent->flags |= FL_BBRUSH;

	//This doesn't have to be an svFlag, can just be a flag.
	//And it might not be needed anyway.
	//ent->r.svFlags |= SVF_BBRUSH;


	// if the "model2" key is set, use a seperate model
	// for drawing, but clip against the brushes
	if ( ent->model2 && ent->model2[0] )
	{
		ent->s.modelindex2 = RP_EntityModelIndex( ent, ent->model2 );	// GalaxyRP: [Slot Reuse]
	}

	// if the "color" or "light" keys are set, setup constantLight
	lightSet = G_SpawnFloat( "light", "100", &light );
	colorSet = G_SpawnVector( "color", "1 1 1", color );
	if ( lightSet || colorSet )
	{
		int		r, g, b, i;

		r = color[0] * 255;
		if ( r > 255 ) {
			r = 255;
		}
		g = color[1] * 255;
		if ( g > 255 ) {
			g = 255;
		}
		b = color[2] * 255;
		if ( b > 255 ) {
			b = 255;
		}
		i = light / 4;
		if ( i > 255 ) {
			i = 255;
		}
		ent->s.constantLight = r | ( g << 8 ) | ( b << 16 ) | ( i << 24 );
	}

	if(ent->spawnflags & 128)
	{//Can be used by the player's BUTTON_USE
		ent->r.svFlags |= SVF_PLAYER_USABLE;
	}

	ent->s.eType = ET_MOVER;
	trap->LinkEntity((sharedEntity_t *)ent);

	ent->s.pos.trType = TR_STATIONARY;
	VectorCopy( ent->pos1, ent->s.pos.trBase );
}

void funcBBrushTouch( gentity_t *ent, gentity_t *other, trace_t *trace )
{
}

/*QUAKED func_breakable (0 .8 .5) ? INVINCIBLE IMPACT CRUSHER THIN SABERONLY HEAVY_WEAP USE_NOT_BREAK PLAYER_USE NO_EXPLOSION
INVINCIBLE - can only be broken by being used
IMPACT - does damage on impact
CRUSHER - won't reverse movement when hit an obstacle
THIN - can be broken by impact damage, like glass
SABERONLY - only takes damage from sabers
HEAVY_WEAP - only takes damage by a heavy weapon, like an emplaced gun or AT-ST gun.
USE_NOT_BREAK - Using it doesn't make it break, still can be destroyed by damage
PLAYER_USE - Player can use it with the use button
NO_EXPLOSION - Does not play an explosion effect, though will still create chunks if specified

When destroyed, fires it's trigger and chunks and plays sound "noise" or sound for type if no noise specified

"targetname" entities with matching target will fire it
"paintarget" target to fire when hit (but not destroyed)
"wait"		how long minimum to wait between firing paintarget each time hit
"delay"		When killed or used, how long (in seconds) to wait before blowing up (none by default)
"model2"	.md3 model to also draw
"target"	all entities with a matching targetname will be used when this is destoryed
"health"	default is 10
"numchunks" Multiplies the number of chunks spawned.  Chunk code tries to pick a good volume of chunks, but you can alter this to scale the number of spawned chunks. (default 1)  (.5) is half as many chunks, (2) is twice as many chunks
"chunksize"	scales up/down the chunk size by this number (default is 1)
"playfx"	path of effect to play on death

"showhealth" if non-0, will display the health bar on the hud when the crosshair is over this ent (in siege)
"teamowner" in siege this will specify which team this thing is "owned" by. To that team the crosshair will
highlight green, to the other team it will highlight red.

Damage: default is none
"splashDamage" - damage to do
"splashRadius" - radius for above damage

"team" - If set, only this team can trip this trigger
	0 - any
	1 - red
	2 - blue

Don't know if these work:
"color"		constantLight color
"light"		constantLight radius

"material" - default is "0 - MAT_METAL" - choose from this list:
0 = MAT_METAL		(basic blue-grey scorched-DEFAULT)
1 = MAT_GLASS
2 = MAT_ELECTRICAL	(sparks only)
3 = MAT_ELEC_METAL	(METAL2 chunks and sparks)
4 =	MAT_DRK_STONE	(brown stone chunks)
5 =	MAT_LT_STONE	(tan stone chunks)
6 =	MAT_GLASS_METAL (glass and METAL2 chunks)
7 = MAT_METAL2		(electronic type of metal)
8 = MAT_NONE		(no chunks)
9 = MAT_GREY_STONE	(grey colored stone)
10 = MAT_METAL3		(METAL and METAL2 chunk combo)
11 = MAT_CRATE1		(yellow multi-colored crate chunks)
12 = MAT_GRATE1		(grate chunks--looks horrible right now)
13 = MAT_ROPE		(for yavin_trial, no chunks, just wispy bits )
14 = MAT_CRATE2		(red multi-colored crate chunks)
15 = MAT_WHITE_METAL (white angular chunks for Stu, NS_hideout )
16 = MAT_SNOWY_ROCK	 (mix of gray and brown rocks)

Applicable only during Siege gametype:
teamnodmg - if 1, team 1 can't damage this. If 2, team 2 can't damage this.

*/
void SP_func_breakable( gentity_t *self )
{
	int t;
	char *s = NULL;

	G_SpawnString("playfx", "", &s);
	s = RP_EffectKeyValue(self, "playfx", s);	// DAJ_RP: [Weather] not a weather command

	if (s && s[0])
	{ //should we play a special death effect?
		self->genericValue15 = G_EffectIndex(s);
	}
	else
	{
		self->genericValue15 = 0;
	}

	if(!(self->spawnflags & 1))
	{
		if(!self->health)
		{
			self->health = 10;
		}
	}

	G_SpawnInt( "showhealth", "0", &t );

	if (t)
	{ //a non-0 maxhealth value will mean we want to show the health on the hud
		self->maxHealth = self->health;
		G_ScaleNetHealth(self);
	}

	//NOTE: g_spawn.c does this automatically now
	//G_SpawnInt( "teamowner", "0", &t );
	//self->s.teamowner = t;

	if ( self->spawnflags & 16 ) // saber only
	{
		self->flags |= FL_DMG_BY_SABER_ONLY;
	}
	else if ( self->spawnflags & 32 ) // heavy weap
	{
		self->flags |= FL_DMG_BY_HEAVY_WEAP_ONLY;
	}

	if (self->health)
	{
		self->takedamage = qtrue;
	}

	G_SoundIndex("sound/weapons/explosions/cargoexplode.wav");//precaching
	G_SpawnFloat( "radius", "1", &self->radius ); // used to scale chunk code if desired by a designer
	G_SpawnInt( "material", "0", (int*)&self->material );

	G_SpawnInt( "splashDamage", "0", &self->splashDamage );
	G_SpawnInt( "splashRadius", "0", &self->splashRadius );

	CacheChunkEffects( self->material );

	self->use = funcBBrushUse;

	//if ( self->paintarget )
	{
		self->pain = funcBBrushPain;
	}

	self->touch = funcBBrushTouch;

	/*
	if ( self->team && self->team[0] )
	{
		self->alliedTeam = TranslateTeamName( self->team );
		if(self->alliedTeam == TEAM_FREE)
		{
			trap->Error( ERR_DROP, "team name %s not recognized\n", self->team );
		}
	}
	*/
	if ( self->team && self->team[0] && level.gametype == GT_SIEGE &&
		!self->teamnodmg)
	{
		self->teamnodmg = atoi(self->team);
	}
	self->team = NULL;
	if (!self->model) {
		// GalaxyRP fix: [Entity System] was trap->Error(ERR_DROP), which a dedicated server promotes
		// to a fatal error and process exit. A func_breakable (or a func_rotating with health, which
		// comes through here) without a brush model is reachable at runtime from /entadd, /entedit
		// and /entload, so one missing key took the server down. Log it and free the entity instead;
		// the return matters, because the old code relied on trap->Error never coming back.
		G_LogPrintf( "%s at %s has no model; not spawned.\n", self->classname ? self->classname : "func_breakable", vtos( self->s.origin ) );
		G_FreeEntity( self );
		return;
	}
	InitBBrush( self );

	if ( !self->radius )
	{//numchunks multiplier
		self->radius = 1.0f;
	}
	if ( !self->mass )
	{//chunksize multiplier
		self->mass = 1.0f;
	}
	self->genericValue4 = 1; //so damage sys knows it's a bbrush
}

qboolean G_EntIsBreakable( int entityNum )
{
	gentity_t *ent;

	if ( entityNum < 0 || entityNum >= ENTITYNUM_WORLD )
	{
		return qfalse;
	}

	ent = &g_entities[entityNum];
	if ( (ent->r.svFlags & SVF_GLASS_BRUSH) )
	{
		return qtrue;
	}
	/*
	if ( (ent->svFlags&SVF_BBRUSH) )
	{
		return qtrue;
	}
	*/
	if ( !Q_stricmp( "func_breakable", ent->classname ) )
	{
		return qtrue;
	}

	if ( !Q_stricmp( "misc_model_breakable", ent->classname ) )
	{
		return qtrue;
	}
	if ( !Q_stricmp( "misc_maglock", ent->classname ) )
	{
		return qtrue;
	}

	return qfalse;
}

/*
===============================================================================

GLASS

===============================================================================
*/
void GlassDie(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
{
	gentity_t *te;
	vec3_t dif;

	if (self->genericValue5)
	{ //was already destroyed, do not retrigger it
		return;
	}

	self->genericValue5 = 1;

	dif[0] = (self->r.absmax[0]+self->r.absmin[0])/2;
	dif[1] = (self->r.absmax[1]+self->r.absmin[1])/2;
	dif[2] = (self->r.absmax[2]+self->r.absmin[2])/2;

	G_UseTargets(self, attacker);

	self->splashRadius = 40; // ?? some random number, maybe it's ok?

	te = G_TempEntity( dif, EV_GLASS_SHATTER );
	te->s.genericenemyindex = self->s.number;
	VectorCopy(self->pos1, te->s.origin);
	VectorCopy(self->pos2, te->s.angles);
	te->s.trickedentindex = (int)self->splashRadius;
	te->s.pos.trTime = (int)self->genericValue3;

	G_FreeEntity(self);
}

void GlassDie_Old(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
{
	gentity_t *te;
	vec3_t dif;

	dif[0] = (self->r.absmax[0]+self->r.absmin[0])/2;
	dif[1] = (self->r.absmax[1]+self->r.absmin[1])/2;
	dif[2] = (self->r.absmax[2]+self->r.absmin[2])/2;

	G_UseTargets(self, attacker);

	te = G_TempEntity( dif, EV_GLASS_SHATTER );
	te->s.genericenemyindex = self->s.number;
	VectorCopy(self->r.maxs, te->s.origin);
	VectorCopy(self->r.mins, te->s.angles);

	G_FreeEntity(self);
}

void GlassPain(gentity_t *self, gentity_t *attacker, int damage)
{
	//trap->Print("Mr. Glass says: PLZ NO IT HURTS\n");
	//Make "cracking" sound?
}

void GlassUse(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	vec3_t temp1, temp2;

	//no direct object to blame for the break, so fill the values with whatever
	VectorAdd( self->r.mins, self->r.maxs, temp1 );
	VectorScale( temp1, 0.5f, temp1 );

	VectorAdd( other->r.mins, other->r.maxs, temp2 );
	VectorScale( temp2, 0.5f, temp2 );

	VectorSubtract( temp1, temp2, self->pos2 );
	VectorCopy( temp1, self->pos1 );

	VectorNormalize( self->pos2 );
	VectorScale( self->pos2, 390, self->pos2 );

	GlassDie(self, other, activator, 100, MOD_UNKNOWN);
}

/*QUAKED func_glass (0 .5 .8) ? x x x x x x PLAYER_USE INACTIVE
Breakable glass
"model2"	.md3 model to also draw
"color"		constantLight color
"light"		constantLight radius
"maxshards"	Max number of shards to spawn on glass break
*/
void SP_func_glass( gentity_t *ent ) {
	zyk_set_brush_model( ent );

	InitMover( ent );

	ent->r.svFlags = SVF_GLASS_BRUSH;

	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );
	if (!ent->health)
	{
		ent->health = 1;
	}

	G_SpawnInt("maxshards", "0", &ent->genericValue3);

	ent->genericValue1 = 0;

	ent->genericValue4 = 1;

	ent->moverState = MOVER_POS1;

	if (ent->spawnflags & 1)
	{
		ent->takedamage = qfalse;
	}
	else
	{
		ent->takedamage = qtrue;
	}

	ent->die = GlassDie;
	ent->use = GlassUse;
	ent->pain = GlassPain;
}

void func_usable_use (gentity_t *self, gentity_t *other, gentity_t *activator);

extern gentity_t	*G_TestEntityPosition( gentity_t *ent );
void func_wait_return_solid( gentity_t *self )
{
	//once a frame, see if it's clear.
	self->clipmask = CONTENTS_BODY;
	if ( !(self->spawnflags&16) || G_TestEntityPosition( self ) == NULL )
	{
		// DAJ_RP fix: [Entity System] InitMover() sets FL_INACTIVE again from spawnflag 128: one target_activate'd
		// and then switched off and on was inactive again (stock multiplayer is the same). It keeps the state it had.
		const int inactive = self->flags & FL_INACTIVE;

		zyk_set_brush_model( self );

		InitMover( self );
		self->flags = ( self->flags & ~FL_INACTIVE ) | inactive;
		VectorCopy( self->s.origin, self->s.pos.trBase );
		VectorCopy( self->s.origin, self->r.currentOrigin );
		self->r.svFlags &= ~SVF_NOCLIENT;
		self->s.eFlags &= ~EF_NODRAW;
		self->use = func_usable_use;
		self->clipmask = 0;
		if ( self->target2 && self->target2[0] )
		{
			G_UseTargets2( self, self->activator, self->target2 );
		}
		//FIXME: Animations?
		/*if ( self->s.eFlags & EF_ANIM_ONCE )
		{//Start our anim
			self->s.frame = 0;
		}*/
	}
	else
	{
		self->clipmask = 0;
		self->think = func_wait_return_solid;
		self->nextthink = level.time + FRAMETIME;
	}
}

void func_usable_think( gentity_t *self )
{
	if ( self->spawnflags & 8 )
	{
		self->r.svFlags |= SVF_PLAYER_USABLE;	//Replace the usable flag
		self->use = func_usable_use;
		self->think = 0;
	}
}

qboolean G_EntIsRemovableUsable( int entNum )
{
	gentity_t *ent = &g_entities[entNum];
	if ( ent->classname && !Q_stricmp( "func_usable", ent->classname ) )
	{
		if ( !(ent->s.eFlags&EF_SHADER_ANIM) && !(ent->spawnflags&8) && ent->targetname )
		{//not just a shader-animator and not ALWAYS_ON, so it must be removable somehow
			return qtrue;
		}
	}
	return qfalse;
}

void func_usable_use (gentity_t *self, gentity_t *other, gentity_t *activator)
{//Toggle on and off
	G_ActivateBehavior( self, BSET_USE );
	if ( self->s.eFlags & EF_SHADER_ANIM )
	{//animate shader when used
		self->s.frame++;//inc frame
		if ( self->s.frame > self->genericValue5 )
		{//wrap around
			self->s.frame = 0;
		}
		if ( self->target && self->target[0] )
		{
			G_UseTargets( self, activator );
		}
	}
	else if ( self->spawnflags & 8 )
	{//ALWAYS_ON
		//Remove the ability to use the entity directly
		self->r.svFlags &= ~SVF_PLAYER_USABLE;
		//also remove ability to call any use func at all!
		self->use = 0;

		if(self->target && self->target[0])
		{
			G_UseTargets(self, activator);
		}

		if ( self->wait )
		{
			self->think = func_usable_think;
			self->nextthink = level.time + ( self->wait * 1000 );
		}

		return;
	}
	else if ( !self->count )
	{//become solid again
		self->count = 1;
		func_wait_return_solid( self );
	}
	else
	{
		self->s.solid = 0;
		self->r.contents = 0;
		self->clipmask = 0;
		self->r.svFlags |= SVF_NOCLIENT;
		self->s.eFlags |= EF_NODRAW;
		self->count = 0;

		if(self->target && self->target[0])
		{
			G_UseTargets(self, activator);
		}
		self->think = 0;
		self->nextthink = -1;
	}
}

void func_usable_pain(gentity_t *self, gentity_t *attacker, int damage)
{
	GlobalUse(self, attacker, attacker);
}

void func_usable_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod)
{
	self->takedamage = qfalse;
	GlobalUse(self, inflictor, attacker);
}

/*QUAKED func_usable (0 .5 .8) ? STARTOFF x x ALWAYS_ON x x PLAYER_USE INACTIVE
START_OFF - the wall will not be there
ALWAYS_ON - Doesn't toggle on and off when used, just runs usescript and fires target

A bmodel that just sits there, doing nothing.  Can be used for conditional walls and models.
"targetname" - When used, will toggle on and off
"target"	Will fire this target every time it is toggled OFF
"model2"	.md3 model to also draw
"color"		constantLight color
"light"		constantLight radius
"usescript" script to run when turned on
"deathscript"  script to run when turned off
"wait"		amount of time before the object is usable again (only valid with ALWAYS_ON flag)
"health"	if it has health, it will be used whenever shot at/killed - if you want it to only be used once this way, set health to 1
"endframe"	Will make it animate to next shader frame when used, not turn on/off... set this to number of frames in the shader, minus 1

Applicable only during Siege gametype:
teamuser - if 1, team 2 can't use this. If 2, team 1 can't use this.

*/

void SP_func_usable( gentity_t *self )
{
	zyk_set_brush_model( self );

	InitMover( self );
	VectorCopy( self->s.origin, self->s.pos.trBase );
	VectorCopy( self->s.origin, self->r.currentOrigin );
	VectorCopy( self->s.origin, self->pos1 );

	G_SpawnInt("endframe", "0", &self->genericValue5);

	if ( self->model2 && self->model2[0] )
	{
		if ( strstr( self->model2, ".glm" ))
		{ //for now, not supported in MP.
			self->s.modelindex2 = 0;
		}
		else
		{
			self->s.modelindex2 = RP_EntityModelIndex( self, self->model2 );	// GalaxyRP: [Slot Reuse]
		}
	}

	self->count = 1;
	if (self->spawnflags & 1)
	{
		self->s.solid = 0;
		self->r.contents = 0;
		self->clipmask = 0;
		self->r.svFlags |= SVF_NOCLIENT;
		self->s.eFlags |= EF_NODRAW;
		self->count = 0;
	}

	//FIXME: Animation?
	/*
	if (self->spawnflags & 2)
	{
		self->s.eFlags |= EF_ANIM_ALLFAST;
	}

	if (self->spawnflags & 4)
	{//FIXME: need to be able to do change to something when it's done?  Or not be usable until it's done?
		self->s.eFlags |= EF_ANIM_ONCE;
	}
	*/

	self->use = func_usable_use;

	// DAJ_RP fix: [Entity System] "if it has health, it will be used whenever shot at/killed - if you want it to only
	// be used once this way, set health to 1" (its QUAKED), and func_usable_pain/func_usable_die below do that. But
	// G_Damage() treats every mover as a shootable door -- uses it and returns before any health is taken -- unless
	// genericValue4 is 1, which only func_breakable and func_glass set: so every shot used it, forever, and its health
	// did nothing (stock multiplayer is the same). 0 without health: an entity respawned in place keeps the field.
	self->genericValue4 = self->health ? 1 : 0;

	if ( self->health )
	{
		self->takedamage = qtrue;
		self->die = func_usable_die;
		self->pain = func_usable_pain;
	}

	if ( self->genericValue5 > 0 )
	{
		self->s.frame = 0;
		self->s.eFlags |= EF_SHADER_ANIM;
		self->s.time = self->genericValue5 + 1;
	}

	trap->LinkEntity ((sharedEntity_t *)self);
}


/*
===============================================================================

WALL

===============================================================================
*/

//static -slc
void use_wall( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	G_ActivateBehavior(ent,BSET_USE);

	// Not there so make it there
	if (!(ent->r.contents & CONTENTS_SOLID))
	{
		ent->r.svFlags &= ~SVF_NOCLIENT;
		ent->s.eFlags &= ~EF_NODRAW;
		ent->r.contents = CONTENTS_SOLID;
		if ( !(ent->spawnflags&1) )
		{//START_OFF doesn't effect area portals
			trap->AdjustAreaPortalState( (sharedEntity_t *)ent, qfalse );
		}
	}
	// Make it go away
	else
	{
		ent->r.contents = 0;
		ent->r.svFlags |= SVF_NOCLIENT;
		ent->s.eFlags |= EF_NODRAW;
		if ( !(ent->spawnflags&1) )
		{//START_OFF doesn't effect area portals
			trap->AdjustAreaPortalState( (sharedEntity_t *)ent, qtrue );
		}
	}
}

#define FUNC_WALL_OFF	1

/*QUAKED func_wall (0 .5 .8) ? START_OFF x x x x x PLAYER_USE INACTIVE
PLAYER_USE	Player can use it with the use button
INACTIVE	must be used by a target_activate before it can be used

A bmodel that just sits there, doing nothing.  Can be used for conditional walls and models.
"model2"	.md3 model to also draw
"color"		constantLight color
"light"		constantLight radius

START_OFF - the wall will not be there
*/
void SP_func_wall( gentity_t *ent )
{
	zyk_set_brush_model( ent );

	VectorCopy( ent->s.origin, ent->pos1 );
	VectorCopy( ent->s.origin, ent->pos2 );

	InitMover( ent );
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );

	// it must be START_OFF
	if (ent->spawnflags & FUNC_WALL_OFF)
	{
		ent->r.contents = 0;
		ent->r.svFlags |= SVF_NOCLIENT;
		ent->s.eFlags |= EF_NODRAW;
	}

	ent->use = use_wall;

	trap->LinkEntity ((sharedEntity_t *)ent);

}

/*
==================================================================================================
GalaxyRP: [SP Maps] Jedi Outcast's brush-model key panels. Both are used with the use button;
the keys are per client (RP_*Key in g_misc.c).
==================================================================================================
*/

/*QUAKED func_security_panel (0 .5 .8) ? x x x x x x x INACTIVE
A brush panel that opens when a player uses it while carrying the right security key.

  INACTIVE - Start off, has to be activated to be usable

  "message" - name of the key the player must have
  "target" - what to use when successfully opened
  "target2" - what to use when the panel is used without the key
*/
static void func_security_panel_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	if ( !activator || !activator->client || activator->s.number >= MAX_CLIENTS )
	{
		return;
	}

	if ( self->message && self->message[0] && !RP_HasSecurityKey( activator, self->message ) )
	{//don't have the key
		G_Sound( self, CHAN_AUTO, G_SoundIndex( "sound/movers/sec_panel_fail.mp3" ) );
		G_UseTargets2( self, activator, self->target2 );
		return;
	}

	RP_TakeSecurityKey( activator );
	G_Sound( self, CHAN_AUTO, G_SoundIndex( "sound/movers/sec_panel_pass.mp3" ) );
	G_UseTargets2( self, activator, self->target );

	//spent
	self->use = NULL;
	self->r.svFlags &= ~SVF_PLAYER_USABLE;
}

void SP_func_security_panel( gentity_t *ent )
{
	zyk_set_brush_model( ent );

	VectorCopy( ent->s.origin, ent->pos1 );
	VectorCopy( ent->s.origin, ent->pos2 );

	InitMover( ent );
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );

	G_SoundIndex( "sound/movers/sec_panel_pass.mp3" );
	G_SoundIndex( "sound/movers/sec_panel_fail.mp3" );

	if ( ent->spawnflags & 128 )
	{
		ent->flags |= FL_INACTIVE;
	}

	ent->r.svFlags |= SVF_PLAYER_USABLE;
	ent->use = func_security_panel_use;

	trap->LinkEntity( (sharedEntity_t *)ent );
}

/*QUAKED func_goodie_panel (0 .5 .8) ? x x x x x x x INACTIVE
A brush panel that opens when a player uses it while carrying a goodie key; the key is used up.

  INACTIVE - Start off, has to be activated to be usable

  "target" - what to use when successfully opened
  "target2" - what to use when the panel is used without a key
*/
static void func_goodie_panel_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	if ( !activator || !activator->client || activator->s.number >= MAX_CLIENTS )
	{
		return;
	}

	if ( !RP_TakeGoodieKey( activator ) )
	{//don't have one
		G_Sound( self, CHAN_AUTO, G_SoundIndex( "sound/movers/sec_panel_fail.mp3" ) );
		G_UseTargets2( self, activator, self->target2 );
		return;
	}

	G_Sound( self, CHAN_AUTO, G_SoundIndex( "sound/movers/sec_panel_pass.mp3" ) );
	G_UseTargets2( self, activator, self->target );

	//spent
	self->use = NULL;
	self->r.svFlags &= ~SVF_PLAYER_USABLE;
}

void SP_func_goodie_panel( gentity_t *ent )
{
	zyk_set_brush_model( ent );

	VectorCopy( ent->s.origin, ent->pos1 );
	VectorCopy( ent->s.origin, ent->pos2 );

	InitMover( ent );
	VectorCopy( ent->s.origin, ent->s.pos.trBase );
	VectorCopy( ent->s.origin, ent->r.currentOrigin );

	G_SoundIndex( "sound/movers/sec_panel_pass.mp3" );
	G_SoundIndex( "sound/movers/sec_panel_fail.mp3" );

	if ( ent->spawnflags & 128 )
	{
		ent->flags |= FL_INACTIVE;
	}

	ent->r.svFlags |= SVF_PLAYER_USABLE;
	ent->use = func_goodie_panel_use;

	trap->LinkEntity( (sharedEntity_t *)ent );
}
