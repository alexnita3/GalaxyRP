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
#include "../../galaxyrp/game/rp_lua.h"	// GLX_U: [Lua] target_lua

//==========================================================

/*QUAKED target_give (1 0 0) (-8 -8 -8) (8 8 8)
Gives the activator all the items pointed to.
*/
/*
==================
RP_TargetGiveUnlink

GalaxyRP: [SP Maps] the end of a given item's short stay in the snapshots, see Use_Target_Give()
==================
*/
static void RP_TargetGiveUnlink( gentity_t *item )
{
	item->r.svFlags &= ~SVF_BROADCAST;
	item->think = NULL;
	item->nextthink = 0;
	trap->UnlinkEntity( (sharedEntity_t *)item );
}

/*
==================
Use_Target_Give

GalaxyRP fix: [SP Maps] it gave nothing. Items with a targetname spawn hidden (EF_NODRAW) in MP, and MP's
Touch_Item() -- unlike single player's -- ignores a hidden item, so the pickup never happened. A hidden
item is now shown to Touch_Item() for the pickup and hidden again after it, and, as in single player,
each use gives the items again: they are the stock it gives from, kept off the map (a use of an item's
own name still shows it there, and the next give takes it off again). The usual pickup rules apply --
a player at full health gets no medpak, and an item not taken is left where it was. An item someone picked up
off the map, waiting to respawn, is not given until it is back.

The pickup event the player gets names the item's entity, and the client reads the item from that
entity's last state it received -- one never sent to it gives the pickup sound and name of whatever
last held the slot. So a given item stays in every snapshot, invisible and not touchable, until the event
is over (EVENT_VALID_MSEC), and is then unlinked. A dropped item keeps Touch_Item()'s own removal.
==================
*/
void Use_Target_Give( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	gentity_t	*t;
	trace_t		trace;

	if ( !activator || !activator->client ) {
		return;
	}

	if ( !ent->target ) {
		return;
	}

	G_ActivateBehavior( ent, BSET_USE );

	memset( &trace, 0, sizeof( trace ) );
	t = NULL;
	while ( (t = G_Find (t, FOFS(targetname), ent->target)) != NULL ) {
		qboolean hidden;

		if ( !t->item ) {
			continue;
		}

		// a weapon or powerup a player took off the map, waiting to respawn
		if ( t->s.eFlags & EF_ITEMPLACEHOLDER ) {
			continue;
		}

		// GalaxyRP fix: [SP Maps] and any other item a player took off the map: hidden, but waiting to
		// respawn (Touch_Item()), it read as stock -- it was given again and the give's own think took the
		// place of its respawn, so it never came back on the map
		if ( t->think == RespawnItem && t->nextthink > 0 ) {
			continue;
		}

		hidden = ( t->s.eFlags & EF_NODRAW ) ? qtrue : qfalse;
		t->s.eFlags &= ~EF_NODRAW;
		Touch_Item( t, activator, &trace );

		if ( !t->inuse ) {
			continue;
		}

		// GalaxyRP fix: [SP Maps] taken or not: a pickup Touch_Item() refuses (a player at full health and a
		// medpak) returns before it changes the item, and every pickup it makes hides the item (EF_NODRAW) or
		// leaves its placeholder (EF_ITEMPLACEHOLDER) -- neither was set going in. One not taken is left as it
		// was: on the map when shown, hidden when stock. It used to be taken off the map all the same.
		if ( !( t->s.eFlags & ( EF_NODRAW | EF_ITEMPLACEHOLDER ) ) ) {
			if ( hidden ) {
				t->s.eFlags |= EF_NODRAW;
			}
			continue;
		}

		if ( t->genericValue9 || ( t->flags & FL_DROPPED_ITEM ) ) {
			// a dropped item, picked up: removed by Touch_Item()'s own means
			continue;
		}

		// off the map, without respawning; sent to everyone, unseen, for the pickup event
		t->s.eFlags |= EF_NODRAW;
		t->s.eFlags &= ~EF_ITEMPLACEHOLDER;
		t->r.contents = 0;
		t->r.svFlags &= ~SVF_NOCLIENT;
		t->r.svFlags |= SVF_BROADCAST;
		t->unlinkAfterEvent = qfalse;
		t->think = RP_TargetGiveUnlink;
		t->nextthink = level.time + EVENT_VALID_MSEC;
		trap->LinkEntity( (sharedEntity_t *)t );
	}
}

void SP_target_give( gentity_t *ent ) {
	ent->use = Use_Target_Give;
}


//==========================================================

/*QUAKED target_remove_powerups (1 0 0) (-8 -8 -8) (8 8 8)
takes away all the activators powerups.
Used to drop flight powerups into death puts.
*/
extern void Jedi_Decloak( gentity_t *self );
void Use_target_remove_powerups( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	if( !activator->client ) {
		return;
	}

	if( activator->client->ps.powerups[PW_REDFLAG] ) {
		Team_ReturnFlag( TEAM_RED );
	} else if( activator->client->ps.powerups[PW_BLUEFLAG] ) {
		Team_ReturnFlag( TEAM_BLUE );
	} else if( activator->client->ps.powerups[PW_NEUTRALFLAG] ) {
		Team_ReturnFlag( TEAM_FREE );
	}

	// GalaxyRP fix: [Cloak Item] decloak properly before the blanket clear below. That memset includes
	// PW_CLOAKED, but cloak is half of a pair: Jedi_Cloak() (NPC_AI_Jedi.c) sets FL_NOTARGET alongside
	// the powerup, and only Jedi_Decloak() clears both. Wiping the powerup array directly therefore
	// made a cloaked activator visible again while leaving FL_NOTARGET set -- and nothing clears that
	// flag afterwards, so they stayed permanently ignored by every NPC, turret, seeker drone and
	// vehicle turret (see the FL_NOTARGET tests in NPC_combat.c, g_turret.c, g_turret_G2.c,
	// g_vehicleTurret.c and g_items.c) until their next respawn. Any map can place one of these, so it
	// was reachable in ordinary play, and it also broke the invariant NPC_ValidEnemy() (NPC_utils.c)
	// documents and relies on: PW_CLOAKED and FL_NOTARGET are never out of step.
	// Deliberately routed through Jedi_Decloak() rather than clearing the flag inline here: FL_NOTARGET
	// has other owners (/notarget, the Death System's downed state, ICARUS SET_NOTARGET), so this uses
	// the same rule as every other decloak in the game instead of inventing a second one for this
	// entity. Guarded on the powerup so an activator who holds FL_NOTARGET for one of those other
	// reasons, and is not cloaked, is left exactly as it was.
	if ( activator->client->ps.powerups[PW_CLOAKED] )
	{
		Jedi_Decloak( activator );
	}

	memset( activator->client->ps.powerups, 0, sizeof( activator->client->ps.powerups ) );
}

void SP_target_remove_powerups( gentity_t *ent ) {
	ent->use = Use_target_remove_powerups;
}


//==========================================================

/*QUAKED target_delay (1 0 0) (-8 -8 -8) (8 8 8) NO_RETRIGGER

NO_RETRIGGER - Keeps the delay from resetting the time if it is
activated again while it is counting down to an event.

"wait" seconds to pause before firing targets.
"random" delay variance, total delay = delay +/- random seconds
*/
void Think_Target_Delay( gentity_t *ent ) {
	G_UseTargets( ent, ent->activator );
}

void Use_Target_Delay( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	if (ent->nextthink > level.time && (ent->spawnflags & 1))
	{ //Leave me alone, I am thinking.
		return;
	}
	G_ActivateBehavior(ent,BSET_USE);
	ent->nextthink = level.time + ( ent->wait + ent->random * Q_flrand(-1.0f, 1.0f) ) * 1000;
	ent->think = Think_Target_Delay;
	ent->activator = activator;
}

void SP_target_delay( gentity_t *ent ) {
	// check delay for backwards compatability
	if ( !G_SpawnFloat( "delay", "0", &ent->wait ) ) {
		G_SpawnFloat( "wait", "1", &ent->wait );
	}

	if ( !ent->wait ) {
		ent->wait = 1;
	}
	ent->use = Use_Target_Delay;
}


//==========================================================

/*QUAKED target_score (1 0 0) (-8 -8 -8) (8 8 8)
"count" number of points to add, default 1

The activator is given this many points.
*/
void Use_Target_Score (gentity_t *ent, gentity_t *other, gentity_t *activator) {
	AddScore( activator, ent->r.currentOrigin, ent->count );
}

void SP_target_score( gentity_t *ent ) {
	if ( !ent->count ) {
		ent->count = 1;
	}
	ent->use = Use_Target_Score;
}


//==========================================================

/*QUAKED target_print (1 0 0) (-8 -8 -8) (8 8 8) redteam blueteam private
"message"	text to print
"wait"		don't fire off again if triggered within this many milliseconds ago
If "private", only the activator gets the message.  If no checks, all clients get the message.
*/
void Use_Target_Print (gentity_t *ent, gentity_t *other, gentity_t *activator)
{
	const char *message = NULL;

	if (!ent || !ent->inuse)
	{
		Com_Printf("ERROR: Bad ent in Use_Target_Print");
		return;
	}

	if (ent->wait)
	{
		if (ent->genericValue14 >= level.time)
		{
			return;
		}
		ent->genericValue14 = level.time + ent->wait;
	}

#ifndef FINAL_BUILD
	if (!ent || !ent->inuse)
	{
	//	Com_Error(ERR_DROP, "Bad ent in Use_Target_Print");
		return;
	}
	else if (!activator || !activator->inuse)
	{
	//	Com_Error(ERR_DROP, "Bad activator in Use_Target_Print");
		return;
	}

	if (ent->genericValue15 > level.time)
	{
		Com_Printf("TARGET PRINT ERRORS:\n");
		if (activator && activator->classname && activator->classname[0])
		{
			Com_Printf("activator classname: %s\n", activator->classname);
		}
		if (activator && activator->target && activator->target[0])
		{
			Com_Printf("activator target: %s\n", activator->target);
		}
		if (activator && activator->targetname && activator->targetname[0])
		{
			Com_Printf("activator targetname: %s\n", activator->targetname);
		}
		if (ent->targetname && ent->targetname[0])
		{
			Com_Printf("print targetname: %s\n", ent->targetname);
		}
		Com_Error(ERR_DROP, "target_print used in quick succession, fix it! See the console for details.");
	}
	ent->genericValue15 = level.time + 5000;
#endif

	G_ActivateBehavior(ent,BSET_USE);

	// GalaxyRP fix: [Entity System] every branch below reads ent->message[0], and nothing ever gave a
	// target_print without a "message" key one -- so the first use of such an entity (an /entadd,
	// /entedit or /entload away, and then any player walking into the trigger that fires it) was a
	// NULL dereference that took the server down. Tested after the use script, which may set it.
	// Only NULL: an empty message is left alone, since sending one clears the centre print.
	if ( !ent->message )
	{
		return;
	}

	// GalaxyRP fix: [security] a message the Entity System set is player-written text, so it is shown
	// with any "@@@" run broken up (RP_ShownText, g_utils.c) -- the client expands three '@' as a
	// reference to one of the server's own phrases and the stock client can overflow a buffer doing
	// it, which a trigger anyone walks into would have done to everyone at once. Only the copy that is
	// sent: the stored key, the entity file and /entsave keep what the admin typed. A map's own
	// target_print is left exactly as the map wrote it.
	message = RP_EntitySystemMade( ent ) ? RP_ShownText( ent->message ) : ent->message;

	if ( ( ent->spawnflags & 4 ) )
	{//private, to one client only
		if (!activator || !activator->inuse)
		{
			Com_Printf("ERROR: Bad activator in Use_Target_Print");
		}
		if ( activator && activator->client )
		{//make sure there's a valid client ent to send it to
			if (message[0] == '@' && message[1] != '@')
			{
				trap->SendServerCommand( activator-g_entities, va("cps \"%s\"", message ));
			}
			else
			{
				trap->SendServerCommand( activator-g_entities, va("cp \"%s\"", message ));
			}
		}
		//NOTE: change in functionality - if there *is* no valid client ent, it won't send it to anyone at all
		return;
	}

	if ( ent->spawnflags & 3 ) {
		if ( ent->spawnflags & 1 ) {
			if (message[0] == '@' && message[1] != '@')
			{
				G_TeamCommand( TEAM_RED, va("cps \"%s\"", message) );
			}
			else
			{
				G_TeamCommand( TEAM_RED, va("cp \"%s\"", message) );
			}
		}
		if ( ent->spawnflags & 2 ) {
			if (message[0] == '@' && message[1] != '@')
			{
				G_TeamCommand( TEAM_BLUE, va("cps \"%s\"", message) );
			}
			else
			{
				G_TeamCommand( TEAM_BLUE, va("cp \"%s\"", message) );
			}
		}
		return;
	}

	if (message[0] == '@' && message[1] != '@')
	{
		trap->SendServerCommand( -1, va("cps \"%s\"", message ));
	}
	else
	{
		trap->SendServerCommand( -1, va("cp \"%s\"", message ));
	}
}

void SP_target_print( gentity_t *ent ) {
	ent->use = Use_Target_Print;
}


//==========================================================


/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) looped-on looped-off global activator
"noise"		wav file to play

A global sound will play full volume throughout the level.
Activator sounds will play on the player that activated the target.
Global and activator sounds can't be combined with looping.
Normal sounds play each time the target is used.
Looped sounds will be toggled by use functions.
Multiple identical looping sounds will just increase volume without any speed cost.
"wait" : Seconds between auto triggerings, 0 = don't auto trigger
"random"	wait variance, default is 0
*/
void Use_Target_Speaker (gentity_t *ent, gentity_t *other, gentity_t *activator) {
	G_ActivateBehavior(ent,BSET_USE);

	if (ent->spawnflags & 3) {	// looping sound toggles
		if (ent->s.loopSound)
		{
			ent->s.loopSound = 0;	// turn it off
			ent->s.loopIsSoundset = qfalse;
			ent->s.trickedentindex = 1;
		}
		else
		{
			ent->s.loopSound = ent->noise_index;	// start it
			ent->s.loopIsSoundset = qfalse;
			ent->s.trickedentindex = 0;
		}
	}else {	// normal sound
		if ( ent->spawnflags & 8 ) {
			G_AddEvent( activator, EV_GENERAL_SOUND, ent->noise_index );
		} else if (ent->spawnflags & 4) {
			G_AddEvent( ent, EV_GLOBAL_SOUND, ent->noise_index );
		} else {
			G_AddEvent( ent, EV_GENERAL_SOUND, ent->noise_index );
		}
	}
}

void SP_target_speaker( gentity_t *ent ) {
	char	buffer[MAX_QPATH];
	char	*s;

	G_SpawnFloat( "wait", "0", &ent->wait );
	G_SpawnFloat( "random", "0", &ent->random );

	// GalaxyRP fix: [Entity System] spawned again in place (/entedit) without its soundSet, it kept the
	// set's index and played the set as well as its noise
	ent->rpAudibleRange = 0.0f;
	ent->s.soundSetIndex = 0;

	if ( G_SpawnString ( "soundSet", "", &s ) )
	{	// this is a sound set
		int radius;

		ent->s.soundSetIndex = G_SoundSetIndex(s);
		// GalaxyRP fix: [Entity System] not EF_PERMANENT, which the engine never sends after the baseline
		// taken at map start: one added later, or respawned by /entload or the default.txt preset (which
		// frees and respawns the map's own), was silent, and one freed after the baseline went on playing
		// for everyone. Sent instead to the clients close enough to hear it -- within its set's radius,
		// where the client's volume for it reaches 0, plus RP_SPEAKER_AUDIBLE_MARGIN for the movement
		// between snapshots: RP_SpeakerAudienceFrame() (g_main.c) sets them every frame. Wherever it
		// stands, a wall included, as a permanent one was heard. (Sending it to everyone, as it was for a
		// while, filled every client's snapshot with every speaker on the map: bespin_undercity has 94.)
		// When the server cannot read sound/sound.txt it does not know the radius: then to everyone.
		ent->s.eFlags = 0;
		if ( RP_SoundSetRadius( s, &radius ) )
		{
			ent->rpAudibleRange = (float)( radius > 0 ? radius : 0 ) + RP_SPEAKER_AUDIBLE_MARGIN;
			ent->r.svFlags |= SVF_BROADCASTCLIENTS;
			ent->r.broadcastClients[0] = ent->r.broadcastClients[1] = 0u;
		}
		else
		{
			ent->r.svFlags |= SVF_BROADCAST;
		}
		VectorCopy( ent->s.origin, ent->s.pos.trBase );
		trap->LinkEntity ((sharedEntity_t *)ent);
		return;
	}

	if ( !G_SpawnString( "noise", "NOSOUND", &s ) ) {
		// GalaxyRP fix: [Entity System] was trap->Error(ERR_DROP) -- a fatal error and process exit
		// on a dedicated server, reachable at runtime through /entadd, /entedit and /entload. Log it
		// and free the entity instead, and return, since the old code relied on never coming back.
		G_LogPrintf( "target_speaker at %s has no \"noise\" key; not spawned.\n", vtos( ent->s.origin ) );
		G_FreeEntity( ent );
		return;
	}

	// force all client reletive sounds to be "activator" speakers that
	// play on the entity that activates it
	if ( s[0] == '*' ) {
		ent->spawnflags |= 8;
	}

	Q_strncpyz( buffer, s, sizeof(buffer) );

	ent->noise_index = G_SoundIndex(buffer);

	// a repeating speaker can be done completely client side
	ent->s.eType = ET_SPEAKER;
	ent->s.eventParm = ent->noise_index;
	ent->s.frame = ent->wait * 10;
	ent->s.clientNum = ent->random * 10;


	// check for prestarted looping sound
	if ( ent->spawnflags & 1 ) {
		ent->s.loopSound = ent->noise_index;
		ent->s.loopIsSoundset = qfalse;
	}

	ent->use = Use_Target_Speaker;

	if (ent->spawnflags & 4) {
		ent->r.svFlags |= SVF_BROADCAST;
	}

	VectorCopy( ent->s.origin, ent->s.pos.trBase );

	// must link the entity so we get areas and clusters so
	// the server can determine who to send updates to
	trap->LinkEntity( (sharedEntity_t *)ent );
}



//==========================================================

/*QUAKED target_laser (0 .5 .8) (-8 -8 -8) (8 8 8) START_ON
When triggered, fires a laser.  You can either set a target or a direction.
*/
void target_laser_think (gentity_t *self) {
	vec3_t	end;
	trace_t	tr;
	vec3_t	point;

	// DAJ_RP fix: [Entity System] its target is looked for again while it is missing (placed after the laser, or
	// removed): at most once a second, the beam pointing along its angle meanwhile. And one that is gone, or whose slot
	// now holds something else, is let go of -- the beam used to follow whatever took the freed slot.
	if ( self->target && self->target[0] )
	{
		if ( self->enemy && ( !self->enemy->inuse || !self->enemy->targetname || Q_stricmp( self->enemy->targetname, self->target ) ) )
		{
			self->enemy = NULL;
		}
		if ( !self->enemy && level.time >= self->genericValue1 )
		{
			self->enemy = G_Find( NULL, FOFS(targetname), self->target );
			self->genericValue1 = level.time + 1000;
		}
		if ( !self->enemy )
		{
			VectorCopy( self->pos2, self->movedir );	// its angle (target_laser_start)
		}
	}

	// if pointed at another entity, set movedir to point at it
	if ( self->enemy ) {
		VectorMA (self->enemy->s.origin, 0.5, self->enemy->r.mins, point);
		VectorMA (point, 0.5, self->enemy->r.maxs, point);
		VectorSubtract (point, self->s.origin, self->movedir);
		VectorNormalize (self->movedir);
	}

	// fire forward and see what we hit
	VectorMA (self->s.origin, 2048, self->movedir, end);

	trap->Trace( &tr, self->s.origin, NULL, NULL, end, self->s.number, CONTENTS_SOLID|CONTENTS_BODY|CONTENTS_CORPSE, qfalse, 0, 0);

	// GalaxyRP fix: [SP Maps] this tested "tr.entityNum" alone, Quake 3's code: the player in client slot 0
	// (entity 0) was never hurt, while everyone else was, and a beam that hit nothing (ENTITYNUM_NONE)
	// still damaged entity 1023. It hurts whatever it hits now, the world aside.
	if ( tr.fraction < 1.0f && tr.entityNum < ENTITYNUM_WORLD ) {
		// hurt it if we can
		G_Damage ( &g_entities[tr.entityNum], self, self->activator, self->movedir,
			tr.endpos, self->damage, DAMAGE_NO_KNOCKBACK, MOD_TARGET_LASER);
	}

	VectorCopy (tr.endpos, self->s.origin2);

	trap->LinkEntity( (sharedEntity_t *)self );
	self->nextthink = level.time + FRAMETIME;
}

void target_laser_on (gentity_t *self)
{
	if (!self->activator)
		self->activator = self;
	target_laser_think (self);
}

void target_laser_off (gentity_t *self)
{
	trap->UnlinkEntity( (sharedEntity_t *)self );
	self->nextthink = 0;
}

void target_laser_use (gentity_t *self, gentity_t *other, gentity_t *activator)
{
	self->activator = activator;
	if ( self->nextthink > 0 )
		target_laser_off (self);
	else
		target_laser_on (self);
}

void target_laser_start (gentity_t *self)
{
	gentity_t *ent;

	self->s.eType = ET_BEAM;

	// DAJ_RP fix: [Entity System] its angle always gives a direction, kept in pos2: with a target that matches nothing
	// (yet) the beam points that way instead of having no length (Quake 3's code read the angle only without a target)
	G_SetMovedir (self->s.angles, self->movedir);
	VectorCopy( self->movedir, self->pos2 );
	self->genericValue1 = 0;

	if (self->target) {
		ent = G_Find (NULL, FOFS(targetname), self->target);
		if (!ent) {
			trap->Print ("%s at %s: %s is a bad target\n", self->classname, vtos(self->s.origin), self->target);
			self->genericValue1 = level.time + 1000;
		}
		self->enemy = ent;
	}

	self->use = target_laser_use;
	self->think = target_laser_think;

	if ( !self->damage ) {
		self->damage = 1;
	}

	if (self->spawnflags & 1)
		target_laser_on (self);
	else
		target_laser_off (self);
}

void SP_target_laser (gentity_t *self)
{
	// let everything else get spawned before we start firing
	self->think = target_laser_start;
	self->nextthink = level.time + FRAMETIME;
}


//==========================================================

void target_teleporter_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	gentity_t	*dest;

	if (!activator->client)
		return;

	G_ActivateBehavior(self,BSET_USE);

	dest = 	G_PickTarget( self->target );
	if (!dest) {
		trap->Print ("Couldn't find teleporter destination\n");
		return;
	}

	TeleportPlayer( activator, dest->s.origin, dest->s.angles );
}

/*QUAKED target_teleporter (1 0 0) (-8 -8 -8) (8 8 8)
The activator will be teleported away.
*/
void SP_target_teleporter( gentity_t *self ) {
	if (!self->targetname)
		trap->Print("untargeted %s at %s\n", self->classname, vtos(self->s.origin));

	self->use = target_teleporter_use;
}

//==========================================================


/*QUAKED target_relay (.5 .5 .5) (-8 -8 -8) (8 8 8) RED_ONLY BLUE_ONLY RANDOM x x x x INACTIVE
This doesn't perform any actions except fire its targets.
The activator can be forced to be from a certain team.
if RANDOM is checked, only one of the targets will be fired, not all of them

  INACTIVE  Can't be used until activated

wait - set to -1 to use it only once
*/
void target_relay_use (gentity_t *self, gentity_t *other, gentity_t *activator) {
	qboolean ranscript = qfalse;
	if ( ( self->spawnflags & 1 ) && activator->client
		&& activator->client->sess.sessionTeam != TEAM_RED ) {
		return;
	}
	if ( ( self->spawnflags & 2 ) && activator->client
		&& activator->client->sess.sessionTeam != TEAM_BLUE ) {
		return;
	}

	if ( self->flags & FL_INACTIVE )
	{//set by target_deactivate
		return;
	}

	ranscript = G_ActivateBehavior( self, BSET_USE );
	if ( self->wait == -1 )
	{//never use again
		if ( ranscript )
		{//crap, can't remove!
			self->use = NULL;
		}
		else
		{//remove
			self->think = G_FreeEntity;
			self->nextthink = level.time + FRAMETIME;
		}
	}
	if ( self->spawnflags & 4 ) {
		gentity_t	*ent;

		ent = G_PickTarget( self->target );
		if ( ent && ent->use ) {
			GlobalUse( ent, self, activator );
		}
		return;
	}
	G_UseTargets (self, activator);
}

void SP_target_relay (gentity_t *self) {
	self->use = target_relay_use;
	if ( self->spawnflags&128 )
	{
		self->flags |= FL_INACTIVE;
	}
}

//==========================================================

/*QUAKED target_lua (0 .5 .8) (-8 -8 -8) (8 8 8)
GLX_U: [Lua] when used -- by a button, a trigger, a relay, ICARUS or /entuse -- raises the "map_event"
event for the server's Lua scripts, with this entity's "event" name (lower-case letters, digits and _,
up to 32) and whoever set it off. Scripts listen with Events.on("map_event", fn, { name = "..." }).
With Lua off, or nothing listening, it does nothing. Logical: it is never sent to clients.

"event"		the name the scripts see (required)
"targetname"	what uses it
*/
void target_lua_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	(void)other;
	if ( self->message && self->message[0] )
	{
		RP_Lua_MapEvent( self->message, activator, self );
	}
}

void SP_target_lua( gentity_t *self ) {
	char *event = NULL;

	self->use = target_lua_use;
	self->message = NULL;	// the event lives here; a "message" key is not one of its keys
	G_SpawnString( "event", "", &event );
	if ( !RP_Lua_ValidEventName( event ) )
	{
		G_LogPrintf( "target_lua %d: its \"event\" key (\"%s\") must be 1 to 32 lower-case letters, digits or _; it does nothing\n",
			self->s.number, event ? event : "" );
		return;
	}
	self->message = G_NewString( event );
}


//==========================================================

/*QUAKED target_kill (.5 .5 .5) (-8 -8 -8) (8 8 8)
Kills the activator.
*/
void target_kill_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	G_ActivateBehavior(self,BSET_USE);
	G_Damage ( activator, NULL, NULL, NULL, NULL, 100000, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);
}

void SP_target_kill( gentity_t *self ) {
	self->use = target_kill_use;
}

/*QUAKED target_position (0 0.5 0) (-4 -4 -4) (4 4 4)
Used as a positional target for in-game calculation, like jumppad targets.
*/
void SP_target_position( gentity_t *self ){
	G_SetOrigin( self, self->s.origin );
	/*
	G_SetAngles( self, self->s.angles );
	self->s.eType = ET_INVISIBLE;
	*/
}

/*QUAKED target_location (0 0.5 0) (-8 -8 -8) (8 8 8)
Set "message" to the name of this location.
Set "count" to 0-7 for color.
0:white 1:red 2:green 3:yellow 4:blue 5:cyan 6:magenta 7:white

Closest target_location in sight used for the location, if none
in site, closest in distance
*/
void SP_target_location( gentity_t *self ) {
	if ( self->targetname && self->targetname[0] ) {
		SP_target_position( self );
		return;
	}
	else {
		static qboolean didwarn = qfalse;
		if ( !self->message ) {
			trap->Print( "target_location with no message at %s\n", vtos( self->s.origin ) );
			RP_SpawnSaysWhy( self, "a target_location needs a message (the location's name), or a targetname to be a position" );
			G_FreeEntity( self );
			return;
		}

		if ( level.locations.num >= MAX_LOCATIONS ) {
			if ( !didwarn ) {
				trap->Print( "Maximum target_locations hit (%d)! Remaining locations will be removed.\n", MAX_LOCATIONS );
				didwarn = qtrue;
			}
			G_FreeEntity( self );
			return;
		}

		VectorCopy( self->s.origin, level.locations.data[level.locations.num].origin );
		Q_strncpyz( level.locations.data[level.locations.num].message, self->message, sizeof( level.locations.data[level.locations.num].message ) );
		level.locations.data[level.locations.num].count = Com_Clampi( 0, 7, self->count );

		level.locations.num++;

		G_FreeEntity( self );
	}
}

/*QUAKED target_counter (1.0 0 0) (-4 -4 -4) (4 4 4) x x x x x x x INACTIVE
Acts as an intermediary for an action that takes multiple inputs.

INACTIVE cannot be used until used by a target_activate

target2 - what the counter should fire each time it's incremented and does NOT reach it's count

After the counter has been triggered "count" times (default 2), it will fire all of it's targets and remove itself.

bounceCount - number of times the counter should reset to it's full count when it's done
*/
extern void G_DebugPrint( int level, const char *format, ... );
void target_counter_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	if ( self->count == 0 )
	{
		return;
	}

	//trap->Printf("target_counter %s used by %s, entnum %d\n", self->targetname, activator->targetname, activator->s.number );
	self->count--;

	if ( activator )
	{
		G_DebugPrint( WL_VERBOSE, "target_counter %s used by %s (%d/%d)\n", self->targetname, activator->targetname, (self->genericValue1-self->count), self->genericValue1 );
	}

	if ( self->count )
	{
		if ( self->target2 )
		{
			//trap->Printf("target_counter %s firing target2 from %s, entnum %d\n", self->targetname, activator->targetname, activator->s.number );
			G_UseTargets2( self, activator, self->target2 );
		}
		return;
	}

	G_ActivateBehavior( self,BSET_USE );

	if ( self->spawnflags & 128 )
	{
		self->flags |= FL_INACTIVE;
	}

	self->activator = activator;
	G_UseTargets( self, activator );

	if ( self->count == 0 )
	{
		if ( self->bounceCount == 0 )
		{
			return;
		}
		self->count = self->genericValue1;
		if ( self->bounceCount > 0 )
		{//-1 means bounce back forever
			self->bounceCount--;
		}
	}
}

void SP_target_counter (gentity_t *self)
{
	self->wait = -1;
	if (!self->count)
	{
		self->count = 2;
	}
	//if ( self->bounceCount > 0 )//let's always set this anyway
	{//we will reset when we use up our count, remember our initial count
		self->genericValue1 = self->count;
	}

	self->use = target_counter_use;
}

/*QUAKED target_random (.5 .5 .5) (-4 -4 -4) (4 4 4) USEONCE
Randomly fires off only one of it's targets each time used

USEONCE	set to never fire again
*/

void target_random_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	int			t_count = 0, pick;
	gentity_t	*t = NULL;

	//trap->Printf("target_random %s used by %s (entnum %d)\n", self->targetname, activator->targetname, activator->s.number );
	G_ActivateBehavior(self,BSET_USE);

	if(self->spawnflags & 1)
	{
		self->use = 0;
	}

	while ( (t = G_Find (t, FOFS(targetname), self->target)) != NULL )
	{
		if (t != self)
		{
			t_count++;
		}
	}

	if(!t_count)
	{
		return;
	}

	if(t_count == 1)
	{
		G_UseTargets (self, activator);
		return;
	}

	//FIXME: need a seed
	pick = Q_irand(1, t_count);
	t_count = 0;
	while ( (t = G_Find (t, FOFS(targetname), self->target)) != NULL )
	{
		if (t != self)
		{
			t_count++;
		}
		else
		{
			continue;
		}

		if (t == self)
		{
//				trap->Printf ("WARNING: Entity used itself.\n");
		}
		else if(t_count == pick)
		{
			if (t->use != NULL)	// check can be omitted
			{
				GlobalUse(t, self, activator);
				return;
			}
		}

		if (!self->inuse)
		{
			Com_Printf("entity was removed while using targets\n");
			return;
		}
	}
}

void SP_target_random (gentity_t *self)
{
	self->use = target_random_use;
}

int	numNewICARUSEnts = 0;
void scriptrunner_run (gentity_t *self)
{
	/*
	if (self->behaviorSet[BSET_USE])
	{
		char	newname[MAX_FILENAME_LENGTH];

		sprintf((char *) &newname, "%s/%s", Q3_SCRIPT_DIR, self->behaviorSet[BSET_USE] );

		ICARUS_RunScript( self, newname );
	}
	*/

	if ( self->count != -1 )
	{
		if ( self->count <= 0 )
		{
			self->use = 0;
			self->behaviorSet[BSET_USE] = NULL;
			return;
		}
		else
		{
			--self->count;
		}
	}

	if (self->behaviorSet[BSET_USE])
	{
		if ( self->spawnflags & 1 )
		{
			if ( !self->activator )
			{
				if (developer.integer)
				{
					Com_Printf("target_scriptrunner tried to run on invalid entity!\n");
				}
				return;
			}

			// DAJ_RP fix: [Entity System] never a logical entity (ICARUS keeps its state by entity number, and the engine
			// does not know those numbers -- see G_SpawnGEntityFromSpawnVars()) nor the world
			if ( self->activator->isLogical || self->activator->s.number >= ENTITYNUM_WORLD )
			{
				if (developer.integer)
				{
					Com_Printf("target_scriptrunner tried to run on invalid ICARUS activator!\n");
				}
				return;
			}

			//if ( !self->activator->sequencer || !self->activator->taskManager )
			// DAJ_RP fix: [Entity System] the ACTIVATOR is the one to set up, as single player tests it
			// (activator->m_iIcarusID). Stock multiplayer tested the scriptrunner itself -- always set up, since it has a
			// usescript -- so an activator that was not a player or an NPC never was, and the script did nothing on it.
			if (!trap->ICARUS_IsInitialized(self->activator->s.number))
			{//Need to be initialized through ICARUS
				if ( !self->activator->script_targetname || !self->activator->script_targetname[0] )
				{
					//We don't have a script_targetname, so create a new one
					// GalaxyRP fix: [Entity System] va() hands out a rotating buffer; stored here, the name was
					// overwritten by the next few va() calls anywhere in the frame -- and that is what every
					// later G_Find(script_targetname) compared against. A copy of its own.
					self->activator->script_targetname = G_NewString( va( "newICARUSEnt%d", numNewICARUSEnts++ ) );
				}

				if ( trap->ICARUS_ValidEnt( (sharedEntity_t *)self->activator ) )
				{
					trap->ICARUS_InitEnt( (sharedEntity_t *)self->activator );
				}
				else
				{
					if (developer.integer)
					{
						Com_Printf("target_scriptrunner tried to run on invalid ICARUS activator!\n");
					}
					return;
				}
			}

			if (developer.integer)
			{
				Com_Printf( "target_scriptrunner running %s on activator %s\n", self->behaviorSet[BSET_USE], self->activator->targetname );
			}
			trap->ICARUS_RunScript( (sharedEntity_t *)self->activator, RP_ScriptPath( self->behaviorSet[BSET_USE] ) );
		}
		else
		{
			if ( developer.integer && self->activator )
			{
				Com_Printf( "target_scriptrunner %s used by %s\n", self->targetname, self->activator->targetname );
			}
			G_ActivateBehavior( self, BSET_USE );
		}
	}

	if ( self->wait )
	{
		self->nextthink = level.time + self->wait;
	}
}

void target_scriptrunner_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	if ( self->nextthink > level.time )
	{
		return;
	}

	self->activator = activator;
	self->enemy = other;
	if ( self->delay )
	{//delay before firing scriptrunner
		self->think = scriptrunner_run;
		self->nextthink = level.time + self->delay;
	}
	else
	{
		scriptrunner_run (self);
	}
}

/*QUAKED target_scriptrunner (1 0 0) (-4 -4 -4) (4 4 4) runonactivator x x x x x x INACTIVE
--- SPAWNFLAGS ---
runonactivator - Will run the script on the entity that used this or tripped the trigger that used this
INACTIVE - start off

----- KEYS ------
Usescript - Script to run when used
count - how many times to run, -1 = infinite.  Default is once
wait - can't be used again in this amount of seconds (Default is 1 second if it's multiple-use)
delay - how long to wait after use to run script

*/
void SP_target_scriptrunner( gentity_t *self )
{
	float v;
	if ( self->spawnflags & 128 )
	{
		self->flags |= FL_INACTIVE;
	}

	if ( !self->count )
	{
		self->count = 1;//default 1 use only
	}
	/*
	else if ( !self->wait )
	{
		self->wait = 1;//default wait of 1 sec
	}
	*/
	// FIXME: this is a hack... because delay is read in as an int, so I'm bypassing that because it's too late in the project to change it and I want to be able to set less than a second delays
	// no one should be setting a radius on a scriptrunner, if they are this would be bad, take this out for the next project
	v = 0.0f;
	G_SpawnFloat( "delay", "0", &v );
	self->delay = v * 1000;//sec to ms
	self->wait *= 1000;//sec to ms

	G_SetOrigin( self, self->s.origin );
	self->use = target_scriptrunner_use;
}

void G_SetActiveState(char *targetstring, qboolean actState)
{
	gentity_t	*target = NULL;
	while( NULL != (target = G_Find(target, FOFS(targetname), targetstring)) )
	{
		target->flags = actState ? (target->flags&~FL_INACTIVE) : (target->flags|FL_INACTIVE);
		RP_PushTriggerNetSync( target );	// GalaxyRP: [SP Maps] a trigger_push's prediction follows it
	}
}

#define ACT_ACTIVE		qtrue
#define ACT_INACTIVE	qfalse

void target_activate_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	G_ActivateBehavior(self,BSET_USE);

	G_SetActiveState(self->target, ACT_ACTIVE);
}

void target_deactivate_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	G_ActivateBehavior(self,BSET_USE);

	G_SetActiveState(self->target, ACT_INACTIVE);
}

//FIXME: make these apply to doors, etc too?
/*QUAKED target_activate (1 0 0) (-4 -4 -4) (4 4 4)
Will set the target(s) to be usable/triggerable
*/
void SP_target_activate( gentity_t *self )
{
	G_SetOrigin( self, self->s.origin );
	self->use = target_activate_use;
}

/*QUAKED target_deactivate (1 0 0) (-4 -4 -4) (4 4 4)
Will set the target(s) to be non-usable/triggerable
*/
void SP_target_deactivate( gentity_t *self )
{
	G_SetOrigin( self, self->s.origin );
	self->use = target_deactivate_use;
}

void target_level_change_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	G_ActivateBehavior(self,BSET_USE);

	// GalaxyRP fix: [security] the mapname below is spliced into a console command line. EXEC_APPEND
	// is correct and must stay -- it is what stops the map change from unloading the game module
	// underneath this very function, which is why TaystJK made the same change -- but it means the
	// text reaches Cbuf_Execute, which splits on ';' and on a newline outside quotes and runs each
	// piece as its own command at full server privilege. "mapname" is an ordinary spawn key, so a
	// map's own entity string, an /entload preset or an /entadd can set it, and G_NewString turns a
	// typed backslash-n into a real linefeed; the first ordinary player to trip the trigger would
	// then run whatever followed the separator. Refuse those characters here, the same way
	// Cmd_AdmMap_f and Cmd_CallVote_f already do for the map names they pass along.
	if (!self->message || Q_strchrs(self->message, ";\r\n"))
	{
		G_LogPrintf("target_level_change at %s refused: mapname contains a command separator\n", vtos(self->s.origin));

		return;
	}

	// GalaxyRP: [Entity System] who tripped it goes to the log before the map goes
	G_LogPrintf( "target_level_change at %s used by %s: changing map to %s\n", vtos( self->s.origin ),
		( activator && activator->client ) ? activator->client->pers.netname : "the map", self->message );

	if (sv_cheats.integer)
		trap->SendConsoleCommand(EXEC_APPEND, va("devmap %s\n", self->message));
	else
		trap->SendConsoleCommand(EXEC_APPEND, va("map %s\n", self->message));
}

// GalaxyRP: [Entity System] a map name as the "map" command takes it: letters, digits, '_', '-', '/'
// and '.', not starting with '/', no "..", no separators. Bounded by MAX_QPATH, as the file system is.
static qboolean RP_MapNameSane( const char *name )
{
	int i;

	if ( !name || !name[0] || name[0] == '/' || strlen( name ) >= MAX_QPATH || strstr( name, ".." ) )
		return qfalse;

	for ( i = 0; name[i]; i++ )
	{
		char c = name[i];

		if ( !( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '_' || c == '-' || c == '/' || c == '.' ) )
			return qfalse;
	}

	return qtrue;
}

/*QUAKED target_level_change (1 0 0) (-4 -4 -4) (4 4 4)
"mapname" - Name of map to change to
*/
void SP_target_level_change( gentity_t *self )
{
	char *s;

	G_SpawnString( "mapname", "", &s );
	self->message = G_NewString(s);

	if ( !self->message || !self->message[0] )
	{
		// GalaxyRP fix: [Entity System] was trap->Error(ERR_DROP) -- a fatal error and process exit
		// on a dedicated server, reachable at runtime through the entity commands. Log and free.
		G_LogPrintf( "target_level_change at %s has no \"mapname\"; not spawned.\n", vtos( self->s.origin ) );
		RP_SpawnSaysWhy( self, "a target_level_change needs a mapname" );
		G_FreeEntity( self );
		return;
	}

	// GalaxyRP fix: [Entity System] one the entity commands place changes the map for everyone the
	// moment a player trips it, so its mapname is checked now: a plain map name, and a map this server
	// has (maps/<name>.bsp, the pk3s included). A typo would otherwise end the map with "map not found";
	// the map's own are left as they are.
	if ( RP_EntitySystemMade( self ) )
	{
		if ( !RP_MapNameSane( self->message ) )
		{
			if ( RP_RefuseAtRuntime( self, va( "\"%s\" is not a plain map name", self->message ) ) )
				return;
		}
		else if ( !RP_FileExists( va( "maps/%s.bsp", self->message ) ) )
		{
			if ( RP_RefuseAtRuntime( self, va( "this server has no map \"%s\" (maps/%s.bsp)", self->message, self->message ) ) )
				return;
		}
	}

	G_SetOrigin( self, self->s.origin );
	self->use = target_level_change_use;
}

void target_play_music_use(gentity_t *self, gentity_t *other, gentity_t *activator)
{
	const char *reason = "";

	G_ActivateBehavior(self,BSET_USE);
	// GalaxyRP fix: [Music] through G_SetMusic(): two file names at most, within the gamestate budget
	if ( !G_SetMusic( self->message, &reason ) )
	{
		G_LogPrintf( "target_play_music at %s: music not set (%s)\n", vtos( self->s.origin ), reason );
	}
}

/*QUAKED target_play_music (1 0 0) (-4 -4 -4) (4 4 4)
target_play_music
Plays the requested music files when this target is used.

"targetname"
"music"		music WAV or MP3 file ( music/introfile.mp3 [optional]  music/loopfile.mp3 )

If an intro file and loop file are specified, the intro plays first, then the looping
portion will start and loop indefinetly.  If no introfile is entered, only the loopfile
will play.
*/
void SP_target_play_music( gentity_t *self )
{
	char *s;

	G_SetOrigin( self, self->s.origin );
	if (!G_SpawnString( "music", "", &s ))
	{
		// GalaxyRP fix: [Entity System] was trap->Error(ERR_DROP) -- a fatal error and process exit
		// on a dedicated server, reachable at runtime through the entity commands. Log and free,
		// and return, since the old code relied on trap->Error never coming back.
		G_LogPrintf( "target_play_music at %s has no \"music\" key; not spawned.\n", vtos( self->s.origin ) );
		G_FreeEntity( self );
		return;
	}

	// GalaxyRP fix: [Music] the string is checked once here (two file names at most, each short of
	// MAX_QPATH) and stored in that shape, so a key that would never play is refused at spawn
	{
		char normalized[MAX_QPATH * 2];
		const char *reason = "";

		if ( !G_MusicStringValid( s, normalized, sizeof( normalized ), &reason ) )
		{
			Q_strncpyz( level.rp_spawn_refusal, va( "music: %s", reason ), sizeof( level.rp_spawn_refusal ) );
			G_LogPrintf( "target_play_music at %s: %s; not spawned.\n", vtos( self->s.origin ), reason );
			G_FreeEntity( self );
			return;
		}
		self->message = G_NewString( normalized );
	}

	self->use = target_play_music_use;
}

/*
==================================================================================================
GalaxyRP: [SP Maps] single player's target_secret and target_autosave.
==================================================================================================
*/

/*QUAKED target_secret (1 0 1) (-4 -4 -4) (4 4 4)
Marks a secret area. Using it tells everyone who found it and plays the secret-area sound.

  "count" - number of secret areas on the map (for the "x / y found" message)
*/
static void target_secret_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	if ( !activator || !activator->client || activator->s.number >= MAX_CLIENTS )
	{
		return;
	}

	self->genericValue1++;

	G_Sound( activator, CHAN_AUTO, G_SoundIndex( "sound/interface/secret_area" ) );
	if ( self->count > 0 )
	{
		trap->SendServerCommand( -1, va( "cp \"%s^7 found a secret area (%d / %d)\n\"", activator->client->pers.netname, self->genericValue1, self->count ) );
	}
	else
	{
		trap->SendServerCommand( -1, va( "cp \"%s^7 found a secret area\n\"", activator->client->pers.netname ) );
	}

	//found, done
	self->use = NULL;
}

void SP_target_secret( gentity_t *self )
{
	G_SoundIndex( "sound/interface/secret_area" );
	self->use = target_secret_use;
}

/*QUAKED target_autosave (1 0 0) (-4 -4 -4) (4 4 4)
Single player saved the game when this was used. Multiplayer has nothing to save: the entity
is accepted so single-player maps stop reporting it as unknown, and does nothing.
*/
void SP_target_autosave( gentity_t *self )
{
	RP_SpawnSaysWhy( self, "target_autosave is single player only: multiplayer has nothing to save" );
	G_FreeEntity( self );
}
