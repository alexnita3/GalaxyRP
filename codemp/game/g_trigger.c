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
#include "bg_saga.h"

int gTrigFallSound;

void InitTrigger( gentity_t *self ) {
	if (!VectorCompare (self->s.angles, vec3_origin))
		G_SetMovedir (self->s.angles, self->movedir);

	if (self->model) // zyk: added this condition
	{
		// GalaxyRP fix: [Entity System] an unvalidated "*N" here reaches CM_InlineModel() and
		// Com_Error(ERR_DROP)s the server -- see zyk_brush_model_allowed() in g_spawn.c.
		if (zyk_brush_model_allowed(self, self->model) == qtrue)
			trap->SetBrushModel( (sharedEntity_t *)self, self->model );
		else
		{ // DAJ_RP fix: [Entity System] and no brush model left from before it was spawned again in place:
		  // with r.bmodel still set, touching it was tested against modelindex 0, the whole world model --
		  // see zyk_set_brush_model() in g_spawn.c
			self->r.bmodel = qfalse;
			self->s.modelindex = 0;
		}
	}
	else // zyk: if no model is set, show message in server console
		Com_Printf( S_COLOR_RED"ERROR: trigger %s at %s has no brush model specified\n", self->targetname, vtos(self->s.origin) );

	self->r.contents = CONTENTS_TRIGGER;		// replaces the -1 from trap->SetBrushModel
	self->r.svFlags = SVF_NOCLIENT;

	if(self->spawnflags & 128)
	{
		self->flags |= FL_INACTIVE;
	}
}

// the wait time has passed, so set back up for another activation
void multi_wait( gentity_t *ent ) {
	ent->nextthink = 0;
}

void trigger_cleared_fire (gentity_t *self);

// the trigger was just activated
// ent->activator should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
void multi_trigger_run( gentity_t *ent )
{
	ent->think = 0;

	G_ActivateBehavior( ent, BSET_USE );

	if ( ent->soundSet && ent->soundSet[0] )
	{
		trap->SetConfigstring( CS_GLOBAL_AMBIENT_SET, ent->soundSet );
	}

	if (ent->genericValue4)
	{ //we want to activate target3 for team1 or target4 for team2
		if (ent->genericValue4 == SIEGETEAM_TEAM1 &&
			ent->target3 && ent->target3[0])
		{
			G_UseTargets2(ent, ent->activator, ent->target3);
		}
		else if (ent->genericValue4 == SIEGETEAM_TEAM2 &&
			ent->target4 && ent->target4[0])
		{
			G_UseTargets2(ent, ent->activator, ent->target4);
		}

		ent->genericValue4 = 0;
	}

	G_UseTargets (ent, ent->activator);
	if ( ent->noise_index )
	{
		G_Sound( ent->activator, CHAN_AUTO, ent->noise_index );
	}

	if ( ent->target2 && ent->target2[0] && ent->wait >= 0 )
	{
		ent->think = trigger_cleared_fire;
		ent->nextthink = level.time + ent->speed;
	}
	else if ( ent->wait > 0 )
	{
		if ( ent->painDebounceTime != level.time )
		{//first ent to touch it this frame
			//ent->e_ThinkFunc = thinkF_multi_wait;
			ent->nextthink = level.time + ( ent->wait + ent->random * Q_flrand(-1.0f, 1.0f) ) * 1000;
			ent->painDebounceTime = level.time;
		}
	}
	else if ( ent->wait < 0 )
	{
		// we can't just remove (self) here, because this is a touch function
		// called while looping through area links...
		ent->r.contents &= ~CONTENTS_TRIGGER;//so the EntityContact trace doesn't have to be done against me
		ent->think = 0;
		ent->use = 0;
		//Don't remove, Icarus may barf?
		//ent->nextthink = level.time + FRAMETIME;
		//ent->think = G_FreeEntity;
	}
	if( ent->activator && ent->activator->client )
	{	// mark the trigger as being touched by the player
		ent->aimDebounceTime = level.time;
	}
}

//determine if the class given is listed in the string using the | formatting
qboolean G_NameInTriggerClassList(char *list, char *str)
{
	char cmp[MAX_STRING_CHARS];
	int i = 0;
	int j;

	while (list[i])
	{
        j = 0;
        while (list[i] && list[i] != '|')
		{
			cmp[j] = list[i];
			i++;
			j++;
		}
		cmp[j] = 0;

		if (!Q_stricmp(str, cmp))
		{ //found it
			return qtrue;
		}
		if (list[i] != '|')
		{ //reached the end and never found it
			return qfalse;
		}
		i++;
	}

	return qfalse;
}

extern qboolean gSiegeRoundBegun;
void SiegeItemRemoveOwner(gentity_t *ent, gentity_t *carrier);
void multi_trigger( gentity_t *ent, gentity_t *activator )
{
	qboolean haltTrigger = qfalse;

	if ( ent->think == multi_trigger_run )
	{//already triggered, just waiting to run
		return;
	}

	if (level.gametype == GT_SIEGE &&
		!gSiegeRoundBegun)
	{ //nothing can be used til the round starts.
		return;
	}

	if (level.gametype == GT_SIEGE &&
		activator && activator->client &&
		ent->alliedTeam &&
		activator->client->sess.sessionTeam != ent->alliedTeam)
	{ //this team can't activate this trigger.
		return;
	}

	if (level.gametype == GT_SIEGE &&
		ent->idealclass && ent->idealclass[0])
	{ //only certain classes can activate it
		if (!activator ||
			!activator->client ||
			activator->client->siegeClass < 0)
		{ //no class
			return;
		}

		if (!G_NameInTriggerClassList(bgSiegeClasses[activator->client->siegeClass].name, ent->idealclass))
		{ //wasn't in the list
			return;
		}
	}

	if (level.gametype == GT_SIEGE && ent->genericValue1)
	{
		haltTrigger = qtrue;

		if (activator && activator->client &&
			activator->client->holdingObjectiveItem &&
			ent->targetname && ent->targetname[0])
		{
			gentity_t *objItem = &g_entities[activator->client->holdingObjectiveItem];

			if (objItem && objItem->inuse)
			{
				if (objItem->goaltarget && objItem->goaltarget[0] &&
					!Q_stricmp(ent->targetname, objItem->goaltarget))
				{
					if (objItem->genericValue7 != activator->client->sess.sessionTeam)
					{ //The carrier of the item is not on the team which disallows objective scoring for it
						if (objItem->target3 && objItem->target3[0])
						{ //if it has a target3, fire it off instead of using the trigger
							G_UseTargets2(objItem, objItem, objItem->target3);

                            //3-24-03 - want to fire off the target too I guess, if we have one.
							if (ent->targetname && ent->targetname[0])
							{
								haltTrigger = qfalse;
							}
						}
						else
						{
							haltTrigger = qfalse;
						}

						//now that the item has been delivered, it can go away.
						SiegeItemRemoveOwner(objItem, activator);
						objItem->nextthink = 0;
						objItem->neverFree = qfalse;
						G_FreeEntity(objItem);
					}
				}
			}
		}
	}
	else if (ent->genericValue1)
	{ //Never activate in non-siege gametype I guess.
		return;
	}

	if (ent->genericValue2)
	{ //has "teambalance" property
		int i = 0;
		int team1ClNum = 0;
		int team2ClNum = 0;
		int owningTeam = ent->genericValue3;
		int newOwningTeam = 0;
		int numEnts = 0;
		int entityList[MAX_GENTITIES];
		gentity_t *cl;

		if (level.gametype != GT_SIEGE)
		{
			return;
		}

		if (!activator->client ||
			(activator->client->sess.sessionTeam != SIEGETEAM_TEAM1 && activator->client->sess.sessionTeam != SIEGETEAM_TEAM2))
		{ //activator must be a valid client to begin with
			return;
		}

		//Count up the number of clients standing within the bounds of the trigger and the number of them on each team
		numEnts = trap->EntitiesInBox( ent->r.absmin, ent->r.absmax, entityList, MAX_GENTITIES );
		while (i < numEnts)
		{
			if (entityList[i] < MAX_CLIENTS)
			{ //only care about clients
				cl = &g_entities[entityList[i]];

				//the client is valid
				if (cl->inuse && cl->client &&
					(cl->client->sess.sessionTeam == SIEGETEAM_TEAM1 || cl->client->sess.sessionTeam == SIEGETEAM_TEAM2) &&
					cl->health > 0 &&
					!(cl->client->ps.eFlags & EF_DEAD))
				{
					//See which team he's on
					if (cl->client->sess.sessionTeam == SIEGETEAM_TEAM1)
					{
						team1ClNum++;
					}
					else
					{
						team2ClNum++;
					}
				}
			}
			i++;
		}

		if (!team1ClNum && !team2ClNum)
		{ //no one in the box? How did we get activated? Oh well.
			return;
		}

		if (team1ClNum == team2ClNum)
		{ //if equal numbers the ownership will remain the same as it is now
			return;
		}

		//decide who owns it now
		if (team1ClNum > team2ClNum)
		{
			newOwningTeam = SIEGETEAM_TEAM1;
		}
		else
		{
			newOwningTeam = SIEGETEAM_TEAM2;
		}

		if (owningTeam == newOwningTeam)
		{ //it's the same one it already was, don't care then.
			return;
		}

		//Set the new owner and set the variable which will tell us to activate a team-specific target
		ent->genericValue3 = newOwningTeam;
		ent->genericValue4 = newOwningTeam;
	}

	if (haltTrigger)
	{ //This is an objective trigger and the activator is not carrying an objective item that matches the targetname.
		return;
	}

	if ( ent->nextthink > level.time )
	{
		if( ent->spawnflags & 2048 ) // MULTIPLE - allow multiple entities to touch this trigger in a single frame
		{
			if ( ent->painDebounceTime && ent->painDebounceTime != level.time )
			{//this should still allow subsequent ents to fire this trigger in the current frame
				return;		// can't retrigger until the wait is over
			}
		}
		else
		{
			return;
		}

	}

	// if the player has already activated this trigger this frame
	if( activator && activator->s.number < MAX_CLIENTS && ent->aimDebounceTime == level.time )
	{
		return;
	}

	if ( ent->flags & FL_INACTIVE )
	{//Not active at this time
		return;
	}

	ent->activator = activator;

	if(ent->delay && ent->painDebounceTime < (level.time + ent->delay) )
	{//delay before firing trigger
		ent->think = multi_trigger_run;
		ent->nextthink = level.time + ent->delay;
		ent->painDebounceTime = level.time;

	}
	else
	{
		multi_trigger_run (ent);
	}
}

void Use_Multi( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	multi_trigger( ent, activator );
}

qboolean G_PointInBounds( vec3_t point, vec3_t mins, vec3_t maxs );

void Touch_Multi( gentity_t *self, gentity_t *other, trace_t *trace )
{
	if( !other->client )
	{
		return;
	}

	if ( self->flags & FL_INACTIVE )
	{//set by target_deactivate
		return;
	}

	if( self->alliedTeam )
	{
		if ( other->client->sess.sessionTeam != self->alliedTeam )
		{
			return;
		}
	}

// moved to just above multi_trigger because up here it just checks if the trigger is not being touched
// we want it to check any conditions set on the trigger, if one of those isn't met, the trigger is considered to be "cleared"
//	if ( self->e_ThinkFunc == thinkF_trigger_cleared_fire )
//	{//We're waiting to fire our target2 first
//		self->nextthink = level.time + self->speed;
//		return;
//	}

	if ( self->spawnflags & 1 )
	{
		if ( other->s.eType == ET_NPC )
		{
			return;
		}
	}
	else
	{
		if ( self->spawnflags & 16 )
		{//NPCONLY
			if ( other->NPC == NULL )
			{
				return;
			}
		}

		if ( self->NPC_targetname && self->NPC_targetname[0] )
		{
			if ( other->script_targetname && other->script_targetname[0] )
			{
				if ( Q_stricmp( self->NPC_targetname, other->script_targetname ) != 0 )
				{//not the right guy to fire me off
					return;
				}
			}
			else
			{
				return;
			}
		}
	}

	if ( self->spawnflags & 2 )
	{//FACING
		vec3_t	forward;

		AngleVectors( other->client->ps.viewangles, forward, NULL, NULL );

		if ( DotProduct( self->movedir, forward ) < 0.5 )
		{//Not Within 45 degrees
			return;
		}
	}

	if ( self->spawnflags & 4 )
	{//USE_BUTTON
		if( !( other->client->pers.cmd.buttons & BUTTON_USE ) )
		{//not pressing use button
			return;
		}

		if ((other->client->ps.weaponTime > 0 && other->client->ps.torsoAnim != BOTH_BUTTON_HOLD && other->client->ps.torsoAnim != BOTH_CONSOLE1) || other->health < 1 ||
			(other->client->ps.pm_flags & PMF_FOLLOW) || other->client->sess.sessionTeam == TEAM_SPECTATOR ||
			other->client->ps.forceHandExtend != HANDEXTEND_NONE)
		{ //player has to be free of other things to use.
			return;
		}

		// DAJ_RP: [Locks] a locked use trigger refuses when Use is pressed -- not on walking in, which
		// G_TouchTriggers() leaves to this check -- and before a hold-to-use hack starts, so nobody waits
		// through the bar to be refused. A hack under way when the lock started refusing is called off.
		if ( !RP_LockAllows( self, other, other ) )
		{
			if ( other->s.number < MAX_CLIENTS && other->client->isHacking == self->s.number )
			{
				other->client->isHacking = 0;
				other->client->ps.hackingTime = 0;
			}
			return;
		}

		if (self->genericValue7)
		{ //we have to be holding the use key in this trigger for x milliseconds before firing
			if (level.gametype == GT_SIEGE &&
				self->idealclass && self->idealclass[0])
			{ //only certain classes can activate it
				if (!other ||
					!other->client ||
					other->client->siegeClass < 0)
				{ //no class
					return;
				}

				if (!G_NameInTriggerClassList(bgSiegeClasses[other->client->siegeClass].name, self->idealclass))
				{ //wasn't in the list
					return;
				}
			}

			if (!G_PointInBounds( other->client->ps.origin, self->r.absmin, self->r.absmax ))
			{
				return;
			}
			else if (other->client->isHacking != self->s.number && other->s.number < MAX_CLIENTS )
			{ //start the hack
				other->client->isHacking = self->s.number;
				VectorCopy(other->client->ps.viewangles, other->client->hackingAngles);
				other->client->ps.hackingTime = level.time + self->genericValue7;
				other->client->ps.hackingBaseTime = self->genericValue7;
				if (other->client->ps.hackingBaseTime > 60000)
				{ //don't allow a bit overflow
					other->client->ps.hackingTime = level.time + 60000;
					other->client->ps.hackingBaseTime = 60000;
				}
				return;
			}
			else if (other->client->ps.hackingTime < level.time)
			{ //finished with the hack, reset the hacking values and let it fall through
				other->client->isHacking = 0; //can't hack a client
				other->client->ps.hackingTime = 0;
			}
			else
			{ //hack in progress
				return;
			}
		}
	}

	if ( self->spawnflags & 8 )
	{//FIRE_BUTTON
		if( !( other->client->pers.cmd.buttons & BUTTON_ATTACK ) &&
			!( other->client->pers.cmd.buttons & BUTTON_ALT_ATTACK ) )
		{//not pressing fire button or altfire button
			return;
		}

		// DAJ_RP: [Locks] a locked fire trigger refuses when fire is pressed, as a use trigger does above
		if ( !( self->spawnflags & 4 ) && !RP_LockAllows( self, other, other ) )
		{
			return;
		}
	}

	if ( self->radius )
	{
		vec3_t	eyeSpot;

		//Only works if your head is in it, but we allow leaning out
		//NOTE: We don't use CalcEntitySpot SPOT_HEAD because we don't want this
		//to be reliant on the physical model the player uses.
		VectorCopy(other->client->ps.origin, eyeSpot);
		eyeSpot[2] += other->client->ps.viewheight;

		if ( G_PointInBounds( eyeSpot, self->r.absmin, self->r.absmax ) )
		{
			if( !( other->client->pers.cmd.buttons & BUTTON_ATTACK ) &&
				!( other->client->pers.cmd.buttons & BUTTON_ALT_ATTACK ) )
			{//not attacking, so hiding bonus
				/*
				//FIXME:  should really have sound events clear the hiddenDist
				other->client->hiddenDist = self->radius;
				//NOTE: movedir HAS to be normalized!
				if ( VectorLength( self->movedir ) )
				{//They can only be hidden from enemies looking in this direction
					VectorCopy( self->movedir, other->client->hiddenDir );
				}
				else
				{
					VectorClear( other->client->hiddenDir );
				}
				*/
				//Not using this, at least not yet.
			}
		}
	}

	if ( self->spawnflags & 4 )
	{//USE_BUTTON
		if (other->client->ps.torsoAnim != BOTH_BUTTON_HOLD &&
			other->client->ps.torsoAnim != BOTH_CONSOLE1)
		{
			G_SetAnim( other, NULL, SETANIM_TORSO, BOTH_BUTTON_HOLD, SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD, 0 );
		}
		else
		{
			other->client->ps.torsoTimer = 500;
		}
		other->client->ps.weaponTime = other->client->ps.torsoTimer;
	}

	if ( self->think == trigger_cleared_fire )
	{//We're waiting to fire our target2 first
		self->nextthink = level.time + self->speed;
		return;
	}

	multi_trigger( self, other );
}

void trigger_cleared_fire (gentity_t *self)
{
	G_UseTargets2( self, self->activator, self->target2 );
	self->think = 0;
	// should start the wait timer now, because the trigger's just been cleared, so we must "wait" from this point
	if ( self->wait > 0 )
	{
		self->nextthink = level.time + ( self->wait + self->random * Q_flrand(-1.0f, 1.0f) ) * 1000;
	}
}

/*QUAKED trigger_multiple (.1 .5 .1) ? CLIENTONLY FACING USE_BUTTON FIRE_BUTTON NPCONLY x x INACTIVE MULTIPLE
CLIENTONLY - only a player can trigger this by touch
FACING - Won't fire unless triggering ent's view angles are within 45 degrees of trigger's angles (in addition to any other conditions)
USE_BUTTON - Won't fire unless player is in it and pressing use button (in addition to any other conditions)
FIRE_BUTTON - Won't fire unless player/NPC is in it and pressing fire button (in addition to any other conditions)
NPCONLY - only non-player NPCs can trigger this by touch
INACTIVE - Start off, has to be activated to be touchable/usable
MULTIPLE - multiple entities can touch this trigger in a single frame *and* if needed, the trigger can have a wait of > 0

"wait"		Seconds between triggerings, 0 default, number < 0 means one time only.
"random"	wait variance, default is 0
"delay"		how many seconds to wait to fire targets after tripped
"hiderange" As long as NPC's head is in this trigger, NPCs out of this hiderange cannot see him.  If you set an angle on the trigger, they're only hidden from enemies looking in that direction.  the player's crouch viewheight is 36, his standing viewheight is 54.  So a trigger thast should hide you when crouched but not standing should be 48 tall.
"target2"	The trigger will fire this only when the trigger has been activated and subsequently 'cleared'( once any of the conditions on the trigger have not been satisfied).  This will not fire the "target" more than once until the "target2" is fired (trigger field is 'cleared')
"speed"		How many seconds to wait to fire the target2, default is 1
"noise"		Sound to play when the trigger fires (plays at activator's origin)
"NPC_targetname"  Only the NPC with this NPC_targetname fires this trigger

Variable sized repeatable trigger.  Must be targeted at one or more entities.
so, the basic time between firing is a random time between
(wait - random) and (wait + random)

"team" - If set, only this team can trip this trigger
	0 - any
	1 - red
	2 - blue

"soundSet"	Ambient sound set to play when this trigger is activated

usetime		-	If specified (in milliseconds) along with the USE_BUTTON flag, will
				require a client to hold the use key for x amount of ms before firing.

Applicable only during Siege gametype:
teamuser	-	if 1, team 2 can't use this. If 2, team 1 can't use this.
siegetrig	-	if non-0, can only be activated by players carrying a misc_siege_item
				which is associated with this trigger by the item's goaltarget value.
teambalance	-	if non-0, is "owned" by the last team that activated. Can only be activated
				by the other team if the number of players on the other team inside	the
				trigger outnumber the number of players on the owning team inside the
				trigger.
target3		-	fire when activated by team1
target4		-	fire when activated by team2

idealclass	-	Can only be used by this class/these classes. You can specify use by
				multiple classes with the use of |, e.g.:
				"Imperial Medic|Imperial Assassin|Imperial Demolitionist"
*/

/*
==================
RP_TriggerRegisterSoundSet

GalaxyRP fix: [Ambient Sets] a trigger's "soundSet" becomes the map-wide ambience when it fires
(multi_trigger_run sets CS_GLOBAL_AMBIENT_SET), but a client only loads a set whose name is in a
CS_AMBIENT_SET slot -- or that was the global one when it connected -- and the engine plays nothing for
a set it has not loaded (S_UpdateAmbientSet). The map's own triggers get their slot from
G_PrecacheSoundsets() at the end of the map's spawn; one spawned after that (/entadd, /entaddaim,
/entedit, an entity preset) never did, so it was silent for every player already in the game until
they reconnected. Its name goes into a slot now, as it spawns: cgame loads a slot that arrives mid-game
(CG_ConfigStringModified), so the set is ready before the trigger fires and plays at once, and players
who join later load it with the rest. The name has been checked against sound/sound.txt already
(RP_EntitySystemSpawnRefused() refuses the entity before its spawn function runs), and a full table
or gamestate gives slot 0 -- the trigger stays silent, as it was. During the map's spawn there is
nothing to do here: G_PrecacheSoundsets() does it.
==================
*/
static void RP_TriggerRegisterSoundSet( gentity_t *ent )
{
	if ( level.spawning || !ent->soundSet || !ent->soundSet[0] )
		return;

	G_SoundSetIndex( ent->soundSet );
}

void SP_trigger_multiple( gentity_t *ent )
{
	char	*s;
	if ( G_SpawnString( "noise", "", &s ) )
	{
		if (s && s[0])
		{
			ent->noise_index = G_SoundIndex(s);
		}
		else
		{
			ent->noise_index = 0;
		}
	}

	G_SpawnInt("usetime", "0", &ent->genericValue7);

	//For siege gametype
	G_SpawnInt("siegetrig", "0", &ent->genericValue1);
    G_SpawnInt("teambalance", "0", &ent->genericValue2);

	G_SpawnInt("delay", "0", &ent->delay);

	if ( (ent->wait > 0) && (ent->random >= ent->wait) ) {
		// GalaxyRP fix: [Entity System] both are in seconds; this took off FRAMETIME (100 ms as a number)
		// and left random at -99 s, which the think turned into a nextthink in the past about half the
		// time -- the trigger re-fired on consecutive frames whatever wait said. As SP_func_timer does.
		ent->random = ent->wait - 0.1f;
		Com_Printf(S_COLOR_YELLOW"trigger_multiple has random >= wait\n");
	}

	ent->delay *= 1000;//1 = 1 msec, 1000 = 1 sec
	if ( !ent->speed && ent->target2 && ent->target2[0] )
	{
		ent->speed = 1000;
	}
	else
	{
		ent->speed *= 1000;
	}

	ent->touch = Touch_Multi;
	ent->use   = Use_Multi;

	if ( ent->team && ent->team[0] )
	{
		ent->alliedTeam = atoi(ent->team);
		ent->team = NULL;
	}

	InitTrigger( ent );
	RP_TriggerRegisterSoundSet( ent );
	trap->LinkEntity ((sharedEntity_t *)ent);
}


/*QUAKED trigger_once (.5 1 .5) ? CLIENTONLY FACING USE_BUTTON FIRE_BUTTON x x x INACTIVE MULTIPLE
CLIENTONLY - only a player can trigger this by touch
FACING - Won't fire unless triggering ent's view angles are within 45 degrees of trigger's angles (in addition to any other conditions)
USE_BUTTON - Won't fire unless player is in it and pressing use button (in addition to any other conditions)
FIRE_BUTTON - Won't fire unless player/NPC is in it and pressing fire button (in addition to any other conditions)
INACTIVE - Start off, has to be activated to be touchable/usable
MULTIPLE - multiple entities can touch this trigger in a single frame *and* if needed, the trigger can have a wait of > 0

"random"	wait variance, default is 0
"delay"		how many seconds to wait to fire targets after tripped
Variable sized repeatable trigger.  Must be targeted at one or more entities.
so, the basic time between firing is a random time between
(wait - random) and (wait + random)
"noise"		Sound to play when the trigger fires (plays at activator's origin)
"NPC_targetname"  Only the NPC with this NPC_targetname fires this trigger

"team" - If set, only this team can trip this trigger
	0 - any
	1 - red
	2 - blue

"soundSet"	Ambient sound set to play when this trigger is activated

usetime		-	If specified (in milliseconds) along with the USE_BUTTON flag, will
				require a client to hold the use key for x amount of ms before firing.

Applicable only during Siege gametype:
teamuser - if 1, team 2 can't use this. If 2, team 1 can't use this.
siegetrig - if non-0, can only be activated by players carrying a misc_siege_item
			which is associated with this trigger by the item's goaltarget value.

idealclass	-	Can only be used by this class/these classes. You can specify use by
				multiple classes with the use of |, e.g.:
				"Imperial Medic|Imperial Assassin|Imperial Demolitionist"
*/
void SP_trigger_once( gentity_t *ent )
{
	char	*s;
	if ( G_SpawnString( "noise", "", &s ) )
	{
		if (s && s[0])
		{
			ent->noise_index = G_SoundIndex(s);
		}
		else
		{
			ent->noise_index = 0;
		}
	}

	G_SpawnInt("usetime", "0", &ent->genericValue7);

	//For siege gametype
	G_SpawnInt("siegetrig", "0", &ent->genericValue1);

	G_SpawnInt("delay", "0", &ent->delay);

	ent->wait = -1;

	ent->touch = Touch_Multi;
	ent->use   = Use_Multi;

	if ( ent->team && ent->team[0] )
	{
		ent->alliedTeam = atoi(ent->team);
		ent->team = NULL;
	}

	ent->delay *= 1000;//1 = 1 msec, 1000 = 1 sec

	InitTrigger( ent );
	RP_TriggerRegisterSoundSet( ent );
	trap->LinkEntity ((sharedEntity_t *)ent);
}

/*
======================================================================
trigger_lightningstrike -rww
======================================================================
*/
//lightning strike trigger lightning strike event
void Do_Strike(gentity_t *ent)
{
	trace_t localTrace;
	vec3_t strikeFrom;
	vec3_t strikePoint;
	vec3_t fxAng;

	//maybe allow custom fx direction at some point?
	VectorSet(fxAng, 90.0f, 0.0f, 0.0f);

	//choose a random point to strike within the bounds of the trigger
	strikePoint[0] = flrand(ent->r.absmin[0], ent->r.absmax[0]);
	strikePoint[1] = flrand(ent->r.absmin[1], ent->r.absmax[1]);

	//consider the bottom mins the ground level
	strikePoint[2] = ent->r.absmin[2];

	//set the from point
	strikeFrom[0] = strikePoint[0];
	strikeFrom[1] = strikePoint[1];
	strikeFrom[2] = ent->r.absmax[2]-4.0f;

	//now trace for damaging stuff, and do the effect
	trap->Trace(&localTrace, strikeFrom, NULL, NULL, strikePoint, ent->s.number, MASK_PLAYERSOLID, qfalse, 0, 0);
	VectorCopy(localTrace.endpos, strikePoint);

	if (localTrace.startsolid || localTrace.allsolid)
	{ //got a bad spot, think again next frame to try another strike
		// GalaxyRP fix: [Entity System] "next frame" was every frame for the rest of the map for a
		// trigger whose top is inside a brush (any brushless one placed with /entadd); its own wait
		ent->nextthink = level.time + ent->wait;
		return;
	}

	if (ent->radius)
	{ //do a radius damage at the end pos
		G_RadiusDamage(strikePoint, ent, ent->damage, ent->radius, ent, NULL, MOD_SUICIDE);
	}
	else
	{ //only damage individuals
		gentity_t *trHit = &g_entities[localTrace.entityNum];

		if (trHit->inuse && trHit->takedamage)
		{ //damage it then
			G_Damage(trHit, ent, ent, NULL, trHit->r.currentOrigin, ent->damage, 0, MOD_SUICIDE);
		}
	}

	G_PlayEffectID(ent->genericValue2, strikeFrom, fxAng);
}

//lightning strike trigger think loop
void Think_Strike(gentity_t *ent)
{
	if (ent->genericValue1)
	{ //turned off currently
		return;
	}

	ent->nextthink = level.time + ent->wait + Q_irand(0, ent->random);

	// DAJ_RP fix: [Entity System] inactive (spawnflag 128, target_deactivate): no strike, as every other trigger
	// does nothing while inactive. The loop keeps going, so it strikes again on its own once a target_activate
	// (which only clears the flag) switches it back on. Stock multiplayer struck on regardless.
	if (ent->flags & FL_INACTIVE)
	{
		return;
	}

	Do_Strike(ent);
}

//lightning strike trigger use event function
void Use_Strike( gentity_t *ent, gentity_t *other, gentity_t *activator )
{
	ent->genericValue1 = !ent->genericValue1;

	if (!ent->genericValue1)
	{ //turn it back on
		ent->nextthink = level.time;
	}
}

/*QUAKED trigger_lightningstrike (.1 .5 .1) ? START_OFF
START_OFF - start trigger disabled

"lightningfx"	effect to use for lightning, MUST be specified
"wait"			Seconds between strikes, 1000 default
"random"		wait variance, default is 2000
"dmg"			damage on strike (default 50)
"radius"		if non-0, does a radius damage at the lightning strike
				impact point (using this value as the radius). otherwise
				will only do line trace damage. default 0.

use to toggle on and off
*/
void SP_trigger_lightningstrike( gentity_t *ent )
{
	char *s;

	ent->use = Use_Strike;
	ent->think = Think_Strike;
	ent->nextthink = level.time + 500;

	G_SpawnString("lightningfx", "", &s);
	s = RP_EffectKeyValue(ent, "lightningfx", s);	// DAJ_RP: [Weather] a weather command counts as none
	if (!s || !s[0])
	{
		// GalaxyRP fix: [Entity System] was Com_Error(ERR_DROP) -- a fatal error and process exit
		// on a dedicated server, reachable at runtime through the entity commands. Log and free,
		// and return, since the old code relied on Com_Error never coming back. The use and think
		// set above go with the entity.
		G_LogPrintf( "trigger_lightningstrike at %s has no \"lightningfx\"; not spawned.\n", vtos( ent->s.origin ) );
		G_FreeEntity( ent );
		return;
	}

	//get a configstring index for it
	ent->genericValue2 = G_EffectIndex(s);

	if (ent->spawnflags & 1)
	{ //START_OFF
		ent->genericValue1 = 1;
	}

	if (!ent->wait)
	{ //default 1000
		ent->wait = 1000;
	}
	if (!ent->random)
	{ //default 2000
		ent->random = 2000;
	}
	if (!ent->damage)
	{ //default 50
		ent->damage = 50;
	}

	// GalaxyRP fix: [Entity System] the defaults above only replace zero; a negative wait struck every
	// frame, a negative random put nextthink in the past, dmg and radius came straight off the keys
	// ("radius 10000" was map-wide radius damage). Floored and capped, logged.
	if ( ent->wait < 100 || ent->random < 0 || ent->damage < 0 || ent->radius < 0 || ent->radius > 4096 )
	{
		G_LogPrintf( "trigger_lightningstrike at %s: wait %g, random %g, dmg %d, radius %g clamped (wait >= 100, random >= 0, dmg >= 0, radius 0..4096)\n",
			vtos( ent->s.origin ), ent->wait, ent->random, ent->damage, ent->radius );
		if ( ent->wait < 100 )
			ent->wait = 100;
		if ( ent->random < 0 )
			ent->random = 0;
		if ( ent->damage < 0 )
			ent->damage = 0;
		if ( ent->radius < 0 )
			ent->radius = 0;
		if ( ent->radius > 4096 )
			ent->radius = 4096;
	}

	InitTrigger( ent );
	trap->LinkEntity ((sharedEntity_t *)ent);
}


/*
==============================================================================

trigger_always

==============================================================================
*/

void trigger_always_think( gentity_t *ent ) {
	G_UseTargets(ent, ent);
	G_FreeEntity( ent );
}

/*QUAKED trigger_always (.5 .5 .5) (-8 -8 -8) (8 8 8)
This trigger will always fire.  It is activated by the world.
*/
void SP_trigger_always (gentity_t *ent) {
	// we must have some delay to make sure our use targets are present

	// zyk: added a wait time so it will be possible to save it in entity file (before it gets removed)
	// GalaxyRP fix: [Entity System] the 300ms below is not cosmetic -- the comment above says why it
	// exists, and trigger_always_think() fires G_UseTargets and then frees itself, so a trigger that
	// goes off before the rest of the map has spawned finds no targets and is gone. spawnflag 1024
	// takes the delay from "count" instead, to hold the trigger alive long enough for /entsave to
	// capture it, but nothing stopped count being 0 or negative, which fired it a frame after spawn
	// -- sooner than vanilla, not later. Treat 300 as the floor it already was.
	if (ent->spawnflags & 1024 && ent->count > 300)
		ent->nextthink = level.time + ent->count;
	else
		ent->nextthink = level.time + 300;

	ent->think = trigger_always_think;
}


/*
==============================================================================

trigger_push

==============================================================================
*/
//trigger_push
#define PUSH_PLAYERONLY	1	// GalaxyRP: [SP Maps] single player's flags 1, 8 and 32, see trigger_push_touch()
#define PUSH_NO_TOUCH	2
#define PUSH_LINEAR		4
#define PUSH_NPCONLY	8
#define PUSH_RELATIVE	16
#define PUSH_CONVEYOR	32
#define PUSH_MULTIPLE	2048
//target_push
#define PUSH_CONSTANT	2

/*
==================
RP_PushTriggerNetSync

GalaxyRP fix: [SP Maps] the client predicts every trigger_push it is sent as an arc jump pad: on touching
it, the player's velocity becomes origin2. That is right only for an arc push that pushes now. A LINEAR
push's origin2 is a unit direction (the server multiplies it by speed), a RELATIVE one's is the target's
position, and an inactive one, one without its touch (spawnflag 2), an NPCONLY or a CONVEYOR one does not
throw the player at all -- in every such case the player's own screen launched him and the server pulled
him back. Those are now not sent to clients (SVF_NOCLIENT); the server pushes as it always did, and the
entity type stays ET_PUSH_TRIGGER so ClientThink's sweep for fast players (g_active.c) still finds them.

Called on spawn, once its push is aimed (AimAtTarget), whenever target_activate, target_deactivate or a
script switches it, and every frame from its think while it is one that can be switched.
==================
*/
void trigger_push_touch (gentity_t *self, gentity_t *other, trace_t *trace );

void RP_PushTriggerNetSync( gentity_t *self )
{
	qboolean predicted;

	if ( !self || !self->inuse || !self->classname || Q_stricmp( self->classname, "trigger_push" ) )
	{
		return;
	}

	predicted = ( self->touch == trigger_push_touch
		&& !( self->flags & FL_INACTIVE )
		&& !( self->spawnflags & ( PUSH_LINEAR | PUSH_RELATIVE | PUSH_NPCONLY | PUSH_CONVEYOR ) ) ) ? qtrue : qfalse;

	if ( predicted )
	{
		self->r.svFlags &= ~SVF_NOCLIENT;
	}
	else
	{
		self->r.svFlags |= SVF_NOCLIENT;
	}
}

// GalaxyRP: [SP Maps] keeps a switchable push's SVF_NOCLIENT in step with it, see RP_PushTriggerNetSync()
static void RP_PushTriggerThink( gentity_t *self )
{
	RP_PushTriggerNetSync( self );
	self->nextthink = level.time + 1;
}

void trigger_push_touch (gentity_t *self, gentity_t *other, trace_t *trace ) {
	if ( self->flags & FL_INACTIVE )
	{//set by target_deactivate
		return;
	}

	// GalaxyRP fix: [SP Maps] single player's PLAYERONLY (1), NPCONLY (8) and CONVEYOR (32), which MP had
	// commented out: the SP maps' conveyor belts (taspir2, t3_stamp, cairn_bay, ns_hideout) pushed anyone
	// in the air over them too, and their players-only and NPC-only pushes everyone. Players are client
	// slots here, not only slot 0.
	if ( self->spawnflags & PUSH_PLAYERONLY )
	{
		if ( other->s.number >= MAX_CLIENTS )
		{
			return;
		}
	}
	else if ( self->spawnflags & PUSH_NPCONLY )
	{
		if ( !other->NPC )
		{
			return;
		}
	}

	if ( self->spawnflags & PUSH_CONVEYOR )
	{// only pushes what stands on it
		const int groundEntityNum = other->client ? other->client->ps.groundEntityNum : other->s.groundEntityNum;

		if ( groundEntityNum == ENTITYNUM_NONE )
		{
			return;
		}
	}

	// GalaxyRP fix: [SP Maps] a RELATIVE push without LINEAR came here too, and was thrown as a jump pad
	// whose velocity is origin2 -- for RELATIVE the target's position, so a player was flung at
	// "the target's coordinates" units per second. It now takes the RELATIVE branch below, as in single
	// player (the SP maps' conveyors are mostly RELATIVE alone).
	if ( !(self->spawnflags&(PUSH_LINEAR|PUSH_RELATIVE)) )
	{//normal throw
		if ( !other->client ) {
			return;
		}
		BG_TouchJumpPad( &other->client->ps, &self->s );
		return;
	}

	//linear
	if( level.time < self->painDebounceTime + self->wait  ) // normal 'wait' check
	{
		if( self->spawnflags & PUSH_MULTIPLE ) // MULTIPLE - allow multiple entities to touch this trigger in one frame
		{
			if ( self->painDebounceTime && level.time > self->painDebounceTime ) // if we haven't reached the next frame continue to let ents touch the trigger
			{
				return;
			}
		}
		else // only allowing one ent per frame to touch trigger
		{
			return;
		}
	}

	/*
	//???
	// if the player has already activated this trigger this frame
	if( other && !other->s.number && self->aimDebounceTime == level.time )
	{
		return;
	}
	*/

	/*
	if( self->spawnflags & PUSH_CONVEYOR )
	{   // only push player if he's on the ground
		if( other->s.groundEntityNum == ENTITYNUM_NONE )
		{
			return;
		}
	}
	*/

	/*
	if ( self->spawnflags & 1 )
	{//PLAYERONLY
		if ( other->s.number >= MAX_CLIENTS )
		{
			return;
		}
	}
	else
	{
		if ( self->spawnflags & 8 )
		{//NPCONLY
			if ( other->NPC == NULL )
			{
				return;
			}
		}
	}
	*/

	if ( !other->client ) {
		if ( other->s.pos.trType != TR_STATIONARY && other->s.pos.trType != TR_LINEAR_STOP && other->s.pos.trType != TR_NONLINEAR_STOP && VectorLengthSquared( other->s.pos.trDelta ) )
		{//already moving
			VectorCopy( other->r.currentOrigin, other->s.pos.trBase );
			VectorCopy( self->s.origin2, other->s.pos.trDelta );
			other->s.pos.trTime = level.time;
		}
		return;
	}

	if ( other->client->ps.pm_type != PM_NORMAL
		&& other->client->ps.pm_type != PM_DEAD
		&& other->client->ps.pm_type != PM_FREEZE )
	{
		return;
	}

	if ( (self->spawnflags&PUSH_RELATIVE) )
	{//relative, dir to it * speed
		vec3_t dir;
		VectorSubtract( self->s.origin2, other->r.currentOrigin, dir );
		if ( self->speed )
		{
			VectorNormalize( dir );
			VectorScale( dir, self->speed, dir );
		}
		VectorCopy( dir, other->client->ps.velocity );
	}
	else if ( (self->spawnflags&PUSH_LINEAR) )
	{//linear dir * speed
		VectorScale( self->s.origin2, self->speed, other->client->ps.velocity );
	}
	else
	{
		VectorCopy( self->s.origin2, other->client->ps.velocity );
	}
	//so we don't take damage unless we land lower than we start here...
	/*
	other->client->ps.forceJumpZStart = 0;
	other->client->ps.pm_flags |= PMF_TRIGGER_PUSHED;//pushed by a trigger
	other->client->ps.jumpZStart = other->client->ps.origin[2];
	*/

	if ( self->wait == -1 )
	{
		self->touch = NULL;
	}
	else if ( self->wait > 0 )
	{
		self->painDebounceTime = level.time;

	}
	/*
	if( other && !other->s.number )
	{	// mark that the player has activated this trigger this frame
		self->aimDebounceTime =level.time;
	}
	*/
}


/*
=================
AimAtTarget

Calculate origin2 so the target apogee will be hit
=================
*/
void AimAtTarget( gentity_t *self ) {
	gentity_t	*ent;
	vec3_t		origin;
	float		height, gravity, time, forward;
	float		dist;

	VectorAdd( self->r.absmin, self->r.absmax, origin );
	VectorScale ( origin, 0.5f, origin );

	ent = G_PickTarget( self->target );
	if ( !ent ) {
		G_FreeEntity( self );
		return;
	}

	if ( self->classname && !Q_stricmp( "trigger_push", self->classname ) )
	{
		// GalaxyRP: [SP Maps] from here on a trigger_push is aimed: its think keeps whether clients may
		// predict it in step (RP_PushTriggerNetSync) when it can be switched on or off, and stops otherwise
		if ( self->targetname || self->script_targetname || ( self->spawnflags & 128 ) )
		{
			self->think = RP_PushTriggerThink;
			self->nextthink = level.time + 1;
		}
		else
		{
			self->think = NULL;
			self->nextthink = 0;
		}
		RP_PushTriggerNetSync( self );

		if ( (self->spawnflags&PUSH_RELATIVE) )
		{//relative, not an arc or linear
			VectorCopy( ent->r.currentOrigin, self->s.origin2 );
			return;
		}
		else if ( (self->spawnflags&PUSH_LINEAR) )
		{//linear, not an arc
			VectorSubtract( ent->r.currentOrigin, origin, self->s.origin2 );
			VectorNormalize( self->s.origin2 );
			return;
		}
	}

	if ( self->classname && !Q_stricmp( "target_push", self->classname ) )
	{
		if( self->spawnflags & PUSH_CONSTANT )
		{
			VectorSubtract ( ent->s.origin, self->s.origin, self->s.origin2 );
			VectorNormalize( self->s.origin2);
			VectorScale (self->s.origin2, self->speed, self->s.origin2);
			return;
		}
	}

	height = ent->s.origin[2] - origin[2];
	gravity = g_gravity.value;
	// GalaxyRP fix: [Entity System] a target below the pusher (or no gravity) put sqrt() of a negative --
	// NaN -- into every pushed player's velocity, and "!time" does not catch NaN. No push, logged.
	if ( height <= 0 || gravity <= 0 ) {
		G_LogPrintf( "%s at %s: its target is not above it (height %g, gravity %g); no push\n",
			self->classname ? self->classname : "push", vtos( self->s.origin ), height, gravity );
		G_FreeEntity( self );
		return;
	}
	time = sqrt( height / ( .5 * gravity ) );
	if ( !time ) {
		G_FreeEntity( self );
		return;
	}

	// set s.origin2 to the push velocity
	VectorSubtract ( ent->s.origin, origin, self->s.origin2 );
	self->s.origin2[2] = 0;
	dist = VectorNormalize( self->s.origin2);

	forward = dist / time;
	VectorScale( self->s.origin2, forward, self->s.origin2 );

	self->s.origin2[2] = time * gravity;
}


/*QUAKED trigger_push (.5 .5 .5) ? x x LINEAR x RELATIVE x x INACTIVE MULTIPLE
Must point at a target_position, which will be the apex of the leap.
This will be client side predicted, unlike target_push

LINEAR - Instead of tossing the client at the target_position, it will push them towards it.  Must set a "speed" (see below)
RELATIVE - instead of pushing you in a direction that is always from the center of the trigger to the target_position, it pushes *you* toward the target position, relative to your current location (can use with "speed"... if don't set a speed, it will use the distance from you to the target_position)
INACTIVE - not active until targeted by a target_activate
MULTIPLE - multiple entities can touch this trigger in a single frame *and* if needed, the trigger can have a wait of > 0

wait - how long to wait between pushes: -1 = push only once
speed - when used with the LINEAR spawnflag, pushes the client toward the position at a constant speed (default is 1000)
*/
void SP_trigger_push( gentity_t *self ) {
	InitTrigger (self);

	// unlike other triggers, we need to send this one to the client
	self->r.svFlags &= ~SVF_NOCLIENT;

	// make sure the client precaches this sound
	G_SoundIndex("sound/weapons/force/jump.wav");

	self->s.eType = ET_PUSH_TRIGGER;

	if ( !(self->spawnflags&PUSH_NO_TOUCH) )
	{//start on
		self->touch = trigger_push_touch;
	}

	if ( self->spawnflags & 4 )
	{//linear
		self->speed = 1000;
	}

	self->think = AimAtTarget;
	self->nextthink = level.time + FRAMETIME;
	RP_PushTriggerNetSync( self );	// GalaxyRP: [SP Maps] not sent to clients unless they may predict it
	trap->LinkEntity ((sharedEntity_t *)self);
}

void Use_target_push( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	if ( !activator->client ) {
		return;
	}

	if ( activator->client->ps.pm_type != PM_NORMAL && activator->client->ps.pm_type != PM_FLOAT ) {
		return;
	}

	G_ActivateBehavior(self,BSET_USE);

	VectorCopy (self->s.origin2, activator->client->ps.velocity);

	// play fly sound every 1.5 seconds
	if ( activator->fly_sound_debounce_time < level.time ) {
		activator->fly_sound_debounce_time = level.time + 1500;
		if (self->noise_index)
		{
			G_Sound( activator, CHAN_AUTO, self->noise_index );
		}
	}
}

/*QUAKED target_push (.5 .5 .5) (-8 -8 -8) (8 8 8) bouncepad CONSTANT
CONSTANT will push activator in direction of 'target' at constant 'speed'

Pushes the activator in the direction.of angle, or towards a target apex.
"speed"		defaults to 1000
if "bouncepad", play bounce noise instead of none
*/
void SP_target_push( gentity_t *self ) {
	if (!self->speed) {
		self->speed = 1000;
	}
	G_SetMovedir (self->s.angles, self->s.origin2);
	VectorScale (self->s.origin2, self->speed, self->s.origin2);

	if ( self->spawnflags & 1 ) {
		self->noise_index = G_SoundIndex("sound/weapons/force/jump.wav");
	} else {
		self->noise_index = 0;	//G_SoundIndex("sound/misc/windfly.wav");
	}
	if ( self->target ) {
		VectorCopy( self->s.origin, self->r.absmin );
		VectorCopy( self->s.origin, self->r.absmax );
		self->think = AimAtTarget;
		self->nextthink = level.time + FRAMETIME;
	}
	self->use = Use_target_push;
}

/*
==============================================================================

trigger_teleport

==============================================================================
*/

void trigger_teleporter_touch (gentity_t *self, gentity_t *other, trace_t *trace ) {
	gentity_t	*dest;

	if ( self->flags & FL_INACTIVE )
	{//set by target_deactivate
		return;
	}

	if ( !other->client ) {
		return;
	}
	if ( other->client->ps.pm_type == PM_DEAD ) {
		return;
	}
	// Spectators only?
	if ( ( self->spawnflags & 1 ) &&
		other->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		return;
	}


	dest = 	G_PickTarget( self->target );
	if (!dest) {
		trap->Print ("Couldn't find teleporter destination\n");
		return;
	}

	TeleportPlayer( other, dest->s.origin, dest->s.angles );
}


/*QUAKED trigger_teleport (.5 .5 .5) ? SPECTATOR
Allows client side prediction of teleportation events.
Must point at a target_position, which will be the teleport destination.

If spectator is set, only spectators can use this teleport
Spectator teleporters are not normally placed in the editor, but are created
automatically near doors to allow spectators to move through them
*/
void SP_trigger_teleport( gentity_t *self ) {
	InitTrigger (self);

	// unlike other triggers, we need to send this one to the client
	// unless is a spectator trigger
	if ( self->spawnflags & 1 ) {
		self->r.svFlags |= SVF_NOCLIENT;
	} else {
		self->r.svFlags &= ~SVF_NOCLIENT;
	}

	// make sure the client precaches this sound
	G_SoundIndex("sound/weapons/force/speed.wav");

	self->s.eType = ET_TELEPORT_TRIGGER;
	self->touch = trigger_teleporter_touch;

	trap->LinkEntity ((sharedEntity_t *)self);
}


/*
==============================================================================

trigger_hurt

==============================================================================
*/

/*QUAKED trigger_hurt (.5 .5 .5) ? START_OFF CAN_TARGET SILENT NO_PROTECTION SLOW
Any entity that touches this will be hurt.
It does dmg points of damage each server frame
Targeting the trigger will toggle its on / off state.

CAN_TARGET		if you target it, it will toggle on and off
SILENT			supresses playing the sound
SLOW			changes the damage rate to once per second
NO_PROTECTION	*nothing* stops the damage

"team"			team (1 or 2) to allow hurting (if none then hurt anyone) only applicable for siege
"dmg"			default 5 (whole numbers only)
If dmg is set to -1 this brush will use the fade-kill method

*/
void hurt_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	if (activator && activator->inuse && activator->client)
	{
		self->activator = activator;
	}
	else
	{
		self->activator = NULL;
	}

	G_ActivateBehavior(self,BSET_USE);

	if ( self->r.linked ) {
		trap->UnlinkEntity( (sharedEntity_t *)self );
	} else {
		trap->LinkEntity( (sharedEntity_t *)self );
	}
}

void hurt_touch( gentity_t *self, gentity_t *other, trace_t *trace ) {
	int		dflags;

	if (level.gametype == GT_SIEGE && self->team && self->team[0])
	{
		int team = atoi(self->team);

		if (other->inuse && other->s.number < MAX_CLIENTS && other->client &&
			other->client->sess.sessionTeam != team)
		{ //real client don't hurt
			return;
		}
		else if (other->inuse && other->client && other->s.eType == ET_NPC &&
			other->s.NPC_class == CLASS_VEHICLE && other->s.teamowner != team)
		{ //vehicle owned by team don't hurt
			return;
		}
	}

	if ( self->flags & FL_INACTIVE )
	{//set by target_deactivate
		return;
	}

	if ( !other->takedamage ) {
		return;
	}

	if ( self->timestamp > level.time ) {
		return;
	}

	if (self->damage == -1 && other && other->client && other->health < 1)
	{
		other->client->ps.fallingToDeath = 0;
		ClientRespawn(other);
		return;
	}

	if (self->damage == -1 && other && other->client && other->client->ps.fallingToDeath)
	{
		return;
	}

	if ( self->spawnflags & 16 ) {
		self->timestamp = level.time + 1000;
	} else {
		self->timestamp = level.time + FRAMETIME;
	}

	// play sound
	/*
	if ( !(self->spawnflags & 4) && self->damage != -1 ) {
		G_Sound( other, CHAN_AUTO, self->noise_index );
	}
	*/

	if (self->spawnflags & 8)
		dflags = DAMAGE_NO_PROTECTION;
	else
		dflags = 0;

	if (self->damage == -1 && other && other->client)
	{
		if (other->client->ps.otherKillerTime > level.time)
		{ //we're as good as dead, so if someone pushed us into this then remember them
			other->client->ps.otherKillerTime = level.time + 20000;
			other->client->ps.otherKillerDebounceTime = level.time + 10000;
		}
		other->client->ps.fallingToDeath = level.time;

		//rag on the way down, this flag will automatically be cleared for us on respawn
		other->client->ps.eFlags |= EF_RAG;

		//make sure his jetpack is off
		Jetpack_Off(other);

		if (other->NPC)
		{ //kill it now
			vec3_t vDir;

			VectorSet(vDir, 0, 1, 0);
			G_Damage(other, other, other, vDir, other->client->ps.origin, Q3_INFINITE, 0, MOD_FALLING);
		}
		else
		{
			G_EntitySound(other, CHAN_VOICE, G_SoundIndex("*falling1.wav"));
		}

		self->timestamp = 0; //do not ignore others
	}
	else
	{
		int dmg = self->damage;

		if (dmg == -1)
		{ //so fall-to-blackness triggers destroy evertyhing
			dmg = 99999;
			self->timestamp = 0;
		}
		// GalaxyRP fix: [Combat] both G_Damage calls below used to unconditionally OR in
		// DAMAGE_NO_PROTECTION, regardless of the NO_PROTECTION spawnflag dflags was just computed
		// from a few lines up (self->spawnflags & 8) -- that made every trigger_hurt (lava, acid,
		// any hazard brush a mapper places) always bypass FL_GODMODE, armor, shields and the
		// spawn-invulnerability shield, even on brushes the mapper never marked NO_PROTECTION, and
		// made the spawnflag itself dead weight since it could never actually change anything.
		// Passing dflags through as-is restores the documented per-brush behavior: only a
		// trigger_hurt explicitly flagged NO_PROTECTION ignores protection, same as every other
		// damage source in the game already respects.
		if (self->activator && self->activator->inuse && self->activator->client)
		{
			G_Damage (other, self->activator, self->activator, NULL, NULL, dmg, dflags, MOD_TRIGGER_HURT);
		}
		else
		{
			G_Damage (other, self, self, NULL, NULL, dmg, dflags, MOD_TRIGGER_HURT);
		}
	}
}

void SP_trigger_hurt( gentity_t *self ) {
	InitTrigger (self);

	gTrigFallSound = G_SoundIndex("*falling1.wav");

	self->noise_index = G_SoundIndex( "sound/weapons/force/speed.wav" );
	self->touch = hurt_touch;

	if ( !self->damage ) {
		self->damage = 5;
	}

	self->r.contents = CONTENTS_TRIGGER;

	if ( self->spawnflags & 2 ) {
		self->use = hurt_use;
	}

	// link in to the world if starting active
	if ( ! (self->spawnflags & 1) ) {
		trap->LinkEntity ((sharedEntity_t *)self);
	}
	else if (self->r.linked)
	{
		trap->UnlinkEntity((sharedEntity_t *)self);
	}
}

#define	INITIAL_SUFFOCATION_DELAY	500 //.5 seconds

/*
==================
RP_SpaceRiderProtected

GalaxyRP: [Space] whether this client rides inside a vehicle that hides its rider -- a fighter's
cockpit -- which protects it from space. This was the player-only test in space_touch(); it now
covers NPC pilots too, which until now could never suffocate and so never needed it.

A vehicle is never a rider here, whatever its m_iVehicleNum holds: a vehicle keeps its PILOT'S
number plus one in that field (g_vehicles.c), and with an NPC pilot that is MAX_CLIENTS or more --
which would read as "riding a vehicle" and take the fighter itself out of space.
==================
*/
static qboolean RP_SpaceRiderProtected( const gentity_t *ent )
{
	const gentity_t *veh;

	if ( !ent->client || ent->client->NPC_class == CLASS_VEHICLE )
	{
		return qfalse;
	}

	if ( ent->client->ps.m_iVehicleNum < MAX_CLIENTS || ent->client->ps.m_iVehicleNum >= ENTITYNUM_WORLD )
	{
		return qfalse;
	}

	veh = &g_entities[ent->client->ps.m_iVehicleNum];

	return ( veh->inuse && veh->client && veh->m_pVehicle && veh->m_pVehicle->m_pVehicleInfo &&
		veh->m_pVehicle->m_pVehicleInfo->hideRider ) ? qtrue : qfalse;
}

/*
==================
RP_SpaceSafeClass

GalaxyRP: [Space] the NPC classes that do not breathe, and so are not hurt by space: vehicles --
which have to fly there -- and droids. Every other class suffocates, creatures included, and so
does any class added later.
==================
*/
static qboolean RP_SpaceSafeClass( int npcClass )
{
	switch ( npcClass )
	{
	case CLASS_VEHICLE:
	case CLASS_ATST:
	case CLASS_GALAKMECH:
	case CLASS_MARK1:
	case CLASS_MARK2:
	case CLASS_PROBE:
	case CLASS_SEEKER:
	case CLASS_REMOTE:
	case CLASS_SENTRY:
	case CLASS_INTERROGATOR:
	case CLASS_R2D2:
	case CLASS_R5D2:
	case CLASS_GONK:
	case CLASS_MOUSE:
	case CLASS_PROTOCOL:
		return qtrue;
	default:
		return qfalse;
	}
}

/*
==================
RP_NpcSpaceFrame

GalaxyRP: [Space] the NPC half of what G_RunFrame() does for a player in a trigger_space, called
once a frame for every NPC, vehicles included, from its NPC branch.

Stock JKA only ever did this for players. space_touch() flags NPCs as well (vehicles need it to fly
in space), but nothing took the flag off again or hurt them, so:

- an NPC or vehicle that touched space once stayed "in space" until it died: no gravity wherever it
  went, a fighter kept its drifting-when-damaged behaviour indoors, and its eventual death -- anywhere
  -- played the choke animation and left no corpse. The exit check below is the player one.
- no NPC ever suffocated. Now every NPC whose class breathes (RP_SpaceSafeClass) does, on the
  player timing and damage, unless it sits inside an enclosed vehicle (RP_SpaceRiderProtected).
==================
*/
void RP_NpcSpaceFrame( gentity_t *ent )
{
	gentity_t *spacetrigger;

	if ( !ent || !ent->client )
	{
		return;
	}

	if ( !ent->client->inSpaceIndex || ent->client->inSpaceIndex == ENTITYNUM_NONE )
	{
		return;
	}

	spacetrigger = &g_entities[ent->client->inSpaceIndex];

	if ( !spacetrigger->inuse ||
		!G_PointInBounds( ent->client->ps.origin, spacetrigger->r.absmin, spacetrigger->r.absmax ) )
	{ // no longer in space
		ent->client->inSpaceIndex = 0;
		return;
	}

	if ( RP_SpaceSafeClass( ent->client->NPC_class ) )
	{
		return;
	}

	if ( RP_SpaceRiderProtected( ent ) )
	{ // as space_touch() does for a rider: the cockpit keeps space out
		ent->client->inSpaceSuffocation = 0;
		ent->client->inSpaceIndex = ENTITYNUM_NONE;
		return;
	}

	if ( ent->client->inSpaceSuffocation < level.time )
	{ // suffocate -- the player code in G_RunFrame(), for an NPC
		if ( ent->health > 0 && ent->takedamage )
		{
			G_Damage( ent, spacetrigger, spacetrigger, NULL, ent->client->ps.origin, Q_irand( 50, 70 ), DAMAGE_NO_ARMOR, MOD_SUICIDE );

			if ( ent->health > 0 )
			{ // still alive: choke
				G_EntitySound( ent, CHAN_VOICE, G_SoundIndex( va( "*choke%d.wav", Q_irand( 1, 3 ) ) ) );

				ent->client->ps.forceHandExtend = HANDEXTEND_CHOKE;
				ent->client->ps.forceHandExtendTime = level.time + 2000;
			}
		}

		ent->client->inSpaceSuffocation = level.time + Q_irand( 100, 200 );
	}
}

void space_touch( gentity_t *self, gentity_t *other, trace_t *trace )
{
	if (!other || !other->inuse || !other->client )
		//NOTE: we need vehicles to know this, too...
		//|| other->s.number >= MAX_CLIENTS)
	{
		return;
	}

	// GalaxyRP: [Space] any rider, not only a player one -- NPC pilots can suffocate now (see
	// RP_NpcSpaceFrame) -- and never a vehicle itself; see RP_SpaceRiderProtected.
	if ( RP_SpaceRiderProtected( other ) )
	{ //if they are "inside" a vehicle, then let that protect them from THE HORRORS OF SPACE.
		other->client->inSpaceSuffocation = 0;
		other->client->inSpaceIndex = ENTITYNUM_NONE;
		return;
	}

	if (!G_PointInBounds(other->client->ps.origin, self->r.absmin, self->r.absmax))
	{ //his origin must be inside the trigger
		return;
	}

	if (!other->client->inSpaceIndex ||
		other->client->inSpaceIndex == ENTITYNUM_NONE)
	{ //freshly entering space
		other->client->inSpaceSuffocation = level.time + INITIAL_SUFFOCATION_DELAY;
	}

	other->client->inSpaceIndex = self->s.number;
}

/*QUAKED trigger_space (.5 .5 .5) ?
causes human clients to suffocate and have no gravity.

*/
void SP_trigger_space(gentity_t *self)
{
	InitTrigger(self);
	self->r.contents = CONTENTS_TRIGGER;

	self->touch = space_touch;

    trap->LinkEntity((sharedEntity_t *)self);
}

void shipboundary_touch( gentity_t *self, gentity_t *other, trace_t *trace )
{
	gentity_t *ent;

	if (!other || !other->inuse || !other->client ||
		other->s.number < MAX_CLIENTS ||
		!other->m_pVehicle)
	{ //only let vehicles touch
		return;
	}

	if ( other->client->ps.hyperSpaceTime && level.time - other->client->ps.hyperSpaceTime < HYPERSPACE_TIME )
	{//don't interfere with hyperspacing ships
		return;
	}

	ent = G_Find (NULL, FOFS(targetname), self->target);
	if (!ent || !ent->inuse)
	{
		// GalaxyRP fix: [Logical Entities] this was trap->Error(ERR_DROP), and common.cpp promotes
		// every ERR_DROP to ERR_FATAL on a dedicated server -- so one boundary whose marker had
		// gone missing ended the server process the first time any fighter reached the map edge,
		// with nothing in the log to say why (Com_Shutdown closes it before Sys_Error prints).
		//
		// A marker that is not there is an ordinary misconfiguration, not a reason to take everyone
		// offline: an Entity System preset that was saved without it, an /entremove, a hand-edited
		// file. Same fallback as the logical-target guard below, and as the "no pilot or missing
		// parts" branch further down -- say so once, and destroy the ship that cannot be turned
		// around. "other" was validated at the top of this function, so its origin is safe to read.
		if ( !level.rp_shipboundary_target_warned )
		{
			level.rp_shipboundary_target_warned = qtrue;
			G_LogPrintf( "%s: trigger_shipboundary target '%s' does not exist; cannot turn the "
				"ship around, destroying it instead.\n",
				level.mapname, self->target ? self->target : "" );
		}

		G_Damage(other, other, other, NULL, other->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
		return;
	}

	if (!other->client->ps.m_iVehicleNum || other->m_pVehicle->m_iRemovedSurfaces)
	{ //if a vehicle touches a boundary without a pilot in it or with parts missing, just blow the thing up
		G_Damage(other, other, other, NULL, other->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
		return;
	}

	// GalaxyRP fix: [Logical Entities] belt-and-braces behind RP_PromoteShipboundaryTargets() in
	// g_spawn.c, which moves these markers into the networked region at the end of the spawn pass.
	// If one is still logical here -- it was not inert, or something spawned a boundary after the
	// pass -- then both of the lines below would hand the engine an entity number it refuses:
	// SV_SvEntityForGentity() Com_Error(ERR_DROP)s on s.number >= MAX_GENTITIES, which a dedicated
	// server turns into ERR_FATAL and a process exit, and vehTurnaroundIndex is only GENTITYNUM_BITS
	// wide on the wire, so the client would read a truncated number pointing at the wrong entity.
	//
	// Do neither. Blow the ship up instead, with the same call the "no pilot or missing parts"
	// branch above uses: the fighter is at the map edge with no turnaround coming, so letting it
	// fly on is not an option, and this is a fate the code already knows how to produce there.
	if ( ent->isLogical )
	{
		if ( !level.rp_shipboundary_logical_warned )
		{
			level.rp_shipboundary_logical_warned = qtrue;
			G_LogPrintf( "%s: trigger_shipboundary target '%s' (classname %s) is in the logical "
				"region; cannot turn the ship around, destroying it instead.\n",
				level.mapname, self->target ? self->target : "",
				ent->classname ? ent->classname : "?" );
		}

		G_Damage(other, other, other, NULL, other->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
		return;
	}

	//make sure this sucker is linked so the prediction knows where to go
	trap->LinkEntity((sharedEntity_t *)ent);

	other->client->ps.vehTurnaroundIndex = ent->s.number;
	other->client->ps.vehTurnaroundTime = level.time + (self->genericValue1*2);

	//keep up the detailed checks for another 2 seconds
	self->genericValue7 = level.time + 2000;
}

void shipboundary_think(gentity_t *ent)
{
	int			iEntityList[MAX_GENTITIES];
	int			numListedEntities;
	int			i = 0;
	gentity_t	*listedEnt;

	ent->nextthink = level.time + 100;

	if (ent->genericValue7 < level.time)
	{ //don't need to be doing this check, no one has touched recently
		return;
	}

	numListedEntities = trap->EntitiesInBox( ent->r.absmin, ent->r.absmax, iEntityList, MAX_GENTITIES );
	while (i < numListedEntities)
	{
		listedEnt = &g_entities[iEntityList[i]];
		if (listedEnt->inuse && listedEnt->client && listedEnt->client->ps.m_iVehicleNum)
		{
            if (listedEnt->s.eType == ET_NPC &&
				listedEnt->s.NPC_class == CLASS_VEHICLE)
			{
				Vehicle_t *pVeh = listedEnt->m_pVehicle;
				if (pVeh && pVeh->m_pVehicleInfo->type == VH_FIGHTER)
				{
                    shipboundary_touch(ent, listedEnt, NULL);
				}
			}
		}
		i++;
	}
}

/*QUAKED trigger_shipboundary (.5 .5 .5) ?
causes vehicle to turn toward target and travel in that direction for a set time when hit.

"target"		name of entity to turn toward (can be info_notnull, or whatever).
"traveltime"	time to travel in this direction

*/
void SP_trigger_shipboundary(gentity_t *self)
{
	InitTrigger(self);
	self->r.contents = CONTENTS_TRIGGER;

	if (!self->target || !self->target[0])
	{
		// GalaxyRP fix: [Logical Entities] was trap->Error(ERR_DROP), which ends the server process
		// on a dedicated build. A trigger this malformed cannot work whatever we do, but it is also
		// reachable at RUNTIME, not just at map load: zyk_main_spawn_entity() calls G_CallSpawn()
		// for every line of an Entity System preset, so one truncated /entload record -- or an
		// /entadd of a trigger_shipboundary with a key missing -- took the whole server down. Free it
		// instead: the trigger simply does not exist, the log says which one and where, and
		// everyone stays connected. The return is NOT optional -- the old code relied on
		// trap->Error never coming back, and without it execution would carry on below.
		G_LogPrintf( "trigger_shipboundary at %s has no \"target\"; not spawned.\n", vtos( self->s.origin ) );
		G_FreeEntity( self );
		return;
	}
	G_SpawnInt("traveltime", "0", &self->genericValue1);

	if (!self->genericValue1)
	{
		// GalaxyRP fix: [Logical Entities] as above -- free it rather than ending the server, and
		// return, because the old code relied on trap->Error never coming back.
		G_LogPrintf( "trigger_shipboundary at %s has no \"traveltime\"; not spawned.\n", vtos( self->s.origin ) );
		G_FreeEntity( self );
		return;
	}

	self->think = shipboundary_think;
	self->nextthink = level.time + 500;
	self->touch = shipboundary_touch;

    trap->LinkEntity((sharedEntity_t *)self);
}

void hyperspace_touch( gentity_t *self, gentity_t *other, trace_t *trace )
{
	gentity_t *ent;

	if (!other || !other->inuse || !other->client ||
		other->s.number < MAX_CLIENTS ||
		!other->m_pVehicle)
	{ //only let vehicles touch
		return;
	}

	if ( other->client->ps.hyperSpaceTime && level.time - other->client->ps.hyperSpaceTime < HYPERSPACE_TIME )
	{//already hyperspacing, just keep us moving
		if ( (other->client->ps.eFlags2&EF2_HYPERSPACE) )
		{//they've started the hyperspace but haven't been teleported yet
			float timeFrac = ((float)(level.time-other->client->ps.hyperSpaceTime))/HYPERSPACE_TIME;
			if ( timeFrac >= HYPERSPACE_TELEPORT_FRAC )
			{//half-way, now teleport them!
				vec3_t	diff, fwd, right, up, newOrg;
				float	fDiff, rDiff, uDiff;
				//take off the flag so we only do this once
				other->client->ps.eFlags2 &= ~EF2_HYPERSPACE;
				//Get the offset from the local position
				ent = G_Find (NULL, FOFS(targetname), self->target);
				if (!ent || !ent->inuse)
				{
					// GalaxyRP fix: [Logical Entities] was trap->Error(ERR_DROP), a process exit
					// on a dedicated server -- see shipboundary_touch above for why that is the
					// wrong answer to a missing marker. A hyperspace that cannot resolve its
					// destination is not the same situation as a boundary that cannot turn a ship
					// around, though: nothing is leaving the map, so the jump simply fails and the
					// ship carries on flying. State is left as it is; hyperSpaceTime expires.
					if ( !level.rp_hyperspace_target_warned )
					{
						level.rp_hyperspace_target_warned = qtrue;
						G_LogPrintf( "%s: trigger_hyperspace target '%s' does not exist; the jump "
							"cannot be completed.\n",
							level.mapname, self->target ? self->target : "" );
					}
					return;
				}
				VectorSubtract( other->client->ps.origin, ent->s.origin, diff );
				AngleVectors( ent->s.angles, fwd, right, up );
				fDiff = DotProduct( fwd, diff );
				rDiff = DotProduct( right, diff );
				uDiff = DotProduct( up, diff );
				//Now get the base position of the destination
				ent = G_Find (NULL, FOFS(targetname), self->target2);
				if (!ent || !ent->inuse)
				{
					// GalaxyRP fix: [Logical Entities] was trap->Error(ERR_DROP), a process exit
					// on a dedicated server -- see shipboundary_touch above for why that is the
					// wrong answer to a missing marker. A hyperspace that cannot resolve its
					// destination is not the same situation as a boundary that cannot turn a ship
					// around, though: nothing is leaving the map, so the jump simply fails and the
					// ship carries on flying. State is left as it is; hyperSpaceTime expires.
					if ( !level.rp_hyperspace_target_warned )
					{
						level.rp_hyperspace_target_warned = qtrue;
						G_LogPrintf( "%s: trigger_hyperspace target2 '%s' does not exist; the jump "
							"cannot be completed.\n",
							level.mapname, self->target2 ? self->target2 : "" );
					}
					return;
				}
				VectorCopy( ent->s.origin, newOrg );
				//finally, add the offset into the new origin
				AngleVectors( ent->s.angles, fwd, right, up );
				VectorMA( newOrg, fDiff, fwd, newOrg );
				VectorMA( newOrg, rDiff, right, newOrg );
				VectorMA( newOrg, uDiff, up, newOrg );
				//trap->Print("hyperspace from %s to %s\n", vtos(other->client->ps.origin), vtos(newOrg) );
				//now put them in the offset position, facing the angles that position wants them to be facing
				TeleportPlayer( other, newOrg, ent->s.angles );
				if ( other->m_pVehicle && other->m_pVehicle->m_pPilot )
				{//teleport the pilot, too
					TeleportPlayer( (gentity_t*)other->m_pVehicle->m_pPilot, newOrg, ent->s.angles );
					//FIXME: and the passengers?
				}
				//make them face the new angle
				//other->client->ps.hyperSpaceIndex = ent->s.number;
				VectorCopy( ent->s.angles, other->client->ps.hyperSpaceAngles );
				//sound
				G_Sound( other, CHAN_LOCAL, G_SoundIndex( "sound/vehicles/common/hyperend.wav" ) );
			}
		}
		return;
	}
	else
	{
		ent = G_Find (NULL, FOFS(targetname), self->target);
		if (!ent || !ent->inuse)
		{
			// GalaxyRP fix: [Logical Entities] as the two sites above -- a missing marker fails the
			// jump rather than ending the server. Here the jump has not started yet, so returning
			// simply means it never does.
			if ( !level.rp_hyperspace_target_warned )
			{
				level.rp_hyperspace_target_warned = qtrue;
				G_LogPrintf( "%s: trigger_hyperspace target '%s' does not exist; the jump cannot "
					"be started.\n",
					level.mapname, self->target ? self->target : "" );
			}
			return;
		}

		if (!other->client->ps.m_iVehicleNum || other->m_pVehicle->m_iRemovedSurfaces)
		{ //if a vehicle touches a boundary without a pilot in it or with parts missing, just blow the thing up
			G_Damage(other, other, other, NULL, other->client->ps.origin, 99999, DAMAGE_NO_PROTECTION, MOD_SUICIDE);
			return;
		}
		//other->client->ps.hyperSpaceIndex = ent->s.number;
		VectorCopy( ent->s.angles, other->client->ps.hyperSpaceAngles );
		other->client->ps.hyperSpaceTime = level.time;
	}
}

/*
void trigger_hyperspace_find_targets( gentity_t *self )
{
	gentity_t *targEnt = NULL;
	targEnt = G_Find (NULL, FOFS(targetname), self->target);
	if (!targEnt || !targEnt->inuse)
	{ //this is bad
		trap->Error( ERR_DROP, "trigger_hyperspace has invalid target '%s'\n", self->target );
		return;
	}
	targEnt->r.svFlags |= SVF_BROADCAST;//crap, need to tell the cgame about the target_position
	targEnt = G_Find (NULL, FOFS(targetname), self->target2);
	if (!targEnt || !targEnt->inuse)
	{ //this is bad
		trap->Error( ERR_DROP, "trigger_hyperspace has invalid target2 '%s'\n", self->target2 );
		return;
	}
	targEnt->r.svFlags |= SVF_BROADCAST;//crap, need to tell the cgame about the target_position
}
*/
/*QUAKED trigger_hyperspace (.5 .5 .5) ?
Ship will turn to face the angles of the first target_position then fly forward, playing the hyperspace effect, then pop out at a relative point around the target

"target"		whatever position the ship teleports from in relation to the target_position specified here, that's the relative position the ship will spawn at around the target2 target_position
"target2"		name of target_position to teleport the ship to (will be relative to it's origin)
*/
void SP_trigger_hyperspace(gentity_t *self)
{
	//register the hyperspace end sound (start sounds are customized)
	G_SoundIndex( "sound/vehicles/common/hyperend.wav" );

	InitTrigger(self);
	self->r.contents = CONTENTS_TRIGGER;

	if (!self->target || !self->target[0])
	{
		// GalaxyRP fix: [Logical Entities] as above -- free it rather than ending the server, and
		// return, because the old code relied on trap->Error never coming back.
		G_LogPrintf( "trigger_hyperspace at %s has no \"target\"; not spawned.\n", vtos( self->s.origin ) );
		G_FreeEntity( self );
		return;
	}
	if (!self->target2 || !self->target2[0])
	{
		// GalaxyRP fix: [Logical Entities] as above -- free it rather than ending the server, and
		// return, because the old code relied on trap->Error never coming back.
		G_LogPrintf( "trigger_hyperspace at %s has no \"target2\"; not spawned.\n", vtos( self->s.origin ) );
		G_FreeEntity( self );
		return;
	}

	self->delay = Distance( self->r.absmax, self->r.absmin );//my size

	self->touch = hyperspace_touch;

    trap->LinkEntity((sharedEntity_t *)self);

	//self->think = trigger_hyperspace_find_targets;
	//self->nextthink = level.time + FRAMETIME;
}
/*
==============================================================================

timer

==============================================================================
*/


/*QUAKED func_timer (0.3 0.1 0.6) (-8 -8 -8) (8 8 8) START_ON
This should be renamed trigger_timer...
Repeatedly fires its targets.
Can be turned on or off by using.

"wait"			base time between triggering all targets, default is 1
"random"		wait variance, default is 0
so, the basic time between firing is a random time between
(wait - random) and (wait + random)

*/
void func_timer_think( gentity_t *self ) {
	G_UseTargets (self, self->activator);
	// set time before next firing
	self->nextthink = level.time + 1000 * ( self->wait + Q_flrand(-1.0f, 1.0f) * self->random );
}

void func_timer_use( gentity_t *self, gentity_t *other, gentity_t *activator ) {
	self->activator = activator;

	G_ActivateBehavior(self,BSET_USE);

	// if on, turn it off
	if ( self->nextthink ) {
		self->nextthink = 0;
		return;
	}

	// turn it on
	func_timer_think (self);
}

void SP_func_timer( gentity_t *self ) {
	G_SpawnFloat( "random", "1", &self->random);
	G_SpawnFloat( "wait", "1", &self->wait );

	// GalaxyRP fix: [Entity System] a wait of 0 or less fired the timer's targets every frame for the
	// rest of the map (nextthink never ahead of level.time); floored at 0.1 s, the rate the mod's other
	// timed entities use
	if ( self->wait < 0.1f ) {
		G_LogPrintf( "func_timer at %s: wait %g raised to 0.1\n", vtos( self->s.origin ), self->wait );
		self->wait = 0.1f;
	}

	self->use = func_timer_use;
	self->think = func_timer_think;

	if ( self->random >= self->wait ) {
		// GalaxyRP fix: [Entity System] was wait - 1 (a whole second, these are seconds): for any wait
		// under 1 s -- the floored one above among them -- that left random negative, and the think's
		// wait + crandom * random then landed in the past about half the time, so the timer went on
		// firing its targets on consecutive frames. A tenth under wait, and never below zero.
		self->random = self->wait - 0.1f;
		trap->Print( "func_timer at %s has random >= wait\n", vtos( self->s.origin ) );
	}
	// GalaxyRP fix: [Entity System] and random is never negative, whichever way it got there (a
	// negative key, or the clamp above on a wait of exactly 0.1)
	if ( self->random < 0 ) {
		self->random = 0;
	}

	if ( self->spawnflags & 1 ) {
		self->nextthink = level.time + FRAMETIME;
		self->activator = self;
	}

	self->r.svFlags = SVF_NOCLIENT;
}

gentity_t *asteroid_pick_random_asteroid( gentity_t *self )
{
	int			t_count = 0, pick;
	gentity_t	*t = NULL;
	gentity_t	*only = NULL;	// DAJ_RP: [Entities] the match, for when there is just the one

	while ( (t = G_Find (t, FOFS(targetname), self->target)) != NULL )
	{
		if (t != self)
		{
			t_count++;
			only = t;
		}
	}

	if(!t_count)
	{
		return NULL;
	}

	// DAJ_RP fix: [Entities] this returned t, which the loop above has already run to NULL -- so a field
	// with exactly one template (the usual way to set one up) never spawned an asteroid, and logged that
	// nothing had its target's name. Raven's code, unchanged in OpenJK and TaystJK. With several the pick
	// below walks the list again and returns inside the loop, which was always right.
	if(t_count == 1)
	{
		return only;
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

		if(t_count == pick)
		{
			return t;
		}
	}
	return NULL;
}

int asteroid_count_num_asteroids( gentity_t *self )
{
	int	i, count = 0;

	for ( i = MAX_CLIENTS; i < ENTITYNUM_WORLD; i++ )
	{
		if ( !g_entities[i].inuse )
		{
			continue;
		}
		if ( g_entities[i].r.ownerNum == self->s.number )
		{
			count++;
		}
	}
	return count;
}

extern void SP_func_rotating (gentity_t *ent);
extern void Q3_Lerp2Origin( int taskID, int entID, vec3_t origin, float duration );
void asteroid_field_think(gentity_t *self)
{
	int numAsteroids = asteroid_count_num_asteroids( self );

	self->nextthink = level.time + 500;

	// GalaxyRP fix: [Entity System] an asteroid field spawns one asteroid every think until it has
	// self->count of them, and count comes straight off a spawn key -- "/entadd
	// trigger_asteroid_field ... count 900" used to be a slow, patient way to walk the entity table
	// off its end and take the server process down with it. SP_trigger_asteroid_field now clamps
	// count, and this refuses
	// the individual spawn as well, because the field is not the only thing competing for slots.
	if ( numAsteroids < self->count && G_EntitySlotsAvailable( 1 ) == qfalse )
	{
		return;
	}

	if ( numAsteroids < self->count )
	{
		// GalaxyRP fix: [Entity System] the asteroid to copy is picked before a slot is taken. This
		// took the slot first and, with no entity carrying the field's target, left it in use with
		// nothing in it -- not counted (ownerNum never set), not thinking, never freed: one slot
		// leaked every 500 ms for the rest of the map, from a target typo on /entadd.
		gentity_t *copyAsteroid = asteroid_pick_random_asteroid( self );
		gentity_t *newAsteroid;

		if ( !copyAsteroid )
		{
			// GalaxyRP fix: [Entity System] genericValue3, not alt_fire, which is the "linear" spawn key
			if ( !self->genericValue3 )
			{
				self->genericValue3 = 1;	// logged once per field
				G_LogPrintf( "trigger_asteroid_field at %s: nothing has the targetname %s to copy, no asteroids spawned\n",
					vtos( self->s.origin ), self->target ? self->target : "" );
			}
			self->nextthink = level.time + 10000;	// a template may still be added; look again later
			return;
		}
		self->genericValue3 = 0;

		//need to spawn a new asteroid
		newAsteroid = G_Spawn();
		if ( newAsteroid )
		{
			vec3_t startSpot, endSpot, startAngles;
			float dist, speed = flrand( self->speed * 0.25f, self->speed * 2.0f );
			int	capAxis, axis, time = 0;
			if ( copyAsteroid )
			{
				newAsteroid->model = copyAsteroid->model;
				newAsteroid->model2 = copyAsteroid->model2;
				newAsteroid->health = copyAsteroid->health;
				newAsteroid->spawnflags = copyAsteroid->spawnflags;
				newAsteroid->mass = copyAsteroid->mass;
				newAsteroid->damage = copyAsteroid->damage;
				newAsteroid->speed = copyAsteroid->speed;

				G_SetOrigin( newAsteroid, copyAsteroid->s.origin );
				G_SetAngles( newAsteroid, copyAsteroid->s.angles );
				newAsteroid->classname = "func_rotating";

				SP_func_rotating( newAsteroid );

				newAsteroid->genericValue15 = copyAsteroid->genericValue15;
				newAsteroid->s.iModelScale = copyAsteroid->s.iModelScale;
				newAsteroid->maxHealth = newAsteroid->health;
				G_ScaleNetHealth(newAsteroid);
				newAsteroid->radius = copyAsteroid->radius;
				newAsteroid->material = copyAsteroid->material;
				//CacheChunkEffects( self->material );

				//keep track of it
				newAsteroid->r.ownerNum = self->s.number;

				//move it
				capAxis = Q_irand( 0, 2 );
				for ( axis = 0; axis < 3; axis++ )
				{
					if ( axis == capAxis )
					{
						if ( Q_irand( 0, 1 ) )
						{
							startSpot[axis] = self->r.mins[axis];
							endSpot[axis] = self->r.maxs[axis];
						}
						else
						{
							startSpot[axis] = self->r.maxs[axis];
							endSpot[axis] = self->r.mins[axis];
						}
					}
					else
					{
						startSpot[axis] = self->r.mins[axis]+(flrand(0,1.0f)*(self->r.maxs[axis]-self->r.mins[axis]));
						endSpot[axis] = self->r.mins[axis]+(flrand(0,1.0f)*(self->r.maxs[axis]-self->r.mins[axis]));
					}
				}
				//FIXME: maybe trace from start to end to make sure nothing is in the way?  How big of a trace?

				G_SetOrigin( newAsteroid, startSpot );
				dist = Distance( endSpot, startSpot );
				time = ceil(dist/speed)*1000;
				Q3_Lerp2Origin( -1, newAsteroid->s.number, endSpot, time );

				//spin it
				startAngles[0] = flrand( -360, 360 );
				startAngles[1] = flrand( -360, 360 );
				startAngles[2] = flrand( -360, 360 );
				G_SetAngles( newAsteroid, startAngles );
				newAsteroid->s.apos.trDelta[0] = flrand( -100, 100 );
				newAsteroid->s.apos.trDelta[1] = flrand( -100, 100 );
				newAsteroid->s.apos.trDelta[2] = flrand( -100, 100 );
				newAsteroid->s.apos.trTime = level.time;
				newAsteroid->s.apos.trType = TR_LINEAR;

				//remove itself when done
				newAsteroid->think = G_FreeEntity;
				newAsteroid->nextthink = level.time+time;

				//think again sooner if need even more
				if ( numAsteroids+1 < self->count )
				{//still need at least one more
					//spawn it in 100ms
					self->nextthink = level.time + 100;
				}
			}
		}
	}
}

/*QUAKED trigger_asteroid_field (.5 .5 .5) ?
speed - how fast, on average, the asteroid moves
count - how many asteroids, max, to have at one time
target - target this at func_rotating asteroids
*/
void SP_trigger_asteroid_field(gentity_t *self)
{
	// GalaxyRP fix: [Entity System] this was the one SetBrushModel call with no model test at all,
	// so "/entadd trigger_asteroid_field" with no model reached SV_SetBrushModel's own
	// Com_Error(ERR_DROP, "SV_SetBrushModel: NULL"), which on a dedicated server ends the process
	// (see ZYK_ENTITY_RESERVE in g_local.h). It now refuses the same
	// two ways every other site does -- see zyk_brush_model_allowed() in g_spawn.c.
	if (!self->model || zyk_brush_model_allowed(self, self->model) == qfalse)
	{
		if (!self->model)
			Com_Printf( S_COLOR_RED"ERROR: trigger_asteroid_field at %s has no brush model specified\n", vtos(self->s.origin) );

		self->think = G_FreeEntity;
		self->nextthink = level.time + FRAMETIME;

		return;
	}

	trap->SetBrushModel( (sharedEntity_t *)self, self->model );
	self->r.contents = 0;

	// GalaxyRP fix: [Entity System] this read "self->health = 20", which is in OpenJK and TaystJK
	// unchanged and reads as a Raven slip for "self->count = 20": the test is on count, the QUAKED
	// comment above documents count as "how many asteroids, max, to have at one time", and
	// asteroid_field_think only ever spawns while numAsteroids < count. As written the field was
	// inert -- a map that did not set count spawned nothing at all, and the health it assigned was
	// never read by anything.
	//
	// Corrected, but to 1 rather than Raven's 20. The default is the only part of this that can
	// change a shipped map: a map that sets count explicitly is untouched either way, while a map
	// that omits it goes from spawning nothing to spawning something. One asteroid is the smallest
	// that revival can be -- and it is a live cycle, not a static entity, since each asteroid is
	// lerped across the volume and then frees itself on arrival, so the field holds one drifting
	// rock at a time rather than accumulating.
	//
	// DAJ_RP fix: [Entities] a negative count passed this test and left the field spawning nothing, saying
	// nothing (asteroid_field_think spawns while numAsteroids < count): it means 1 as well now, with a
	// warning, as the clamp below warns.
	if ( self->count <= 0 )
	{
		if ( self->count < 0 )
		{
			Com_Printf( S_COLOR_YELLOW"WARNING: trigger_asteroid_field at %s asked for %d asteroids, using 1\n",
				vtos(self->s.origin), self->count );
		}
		self->count = 1;
	}

	// GalaxyRP fix: [Entity System] count is unbounded and comes from a spawn key, so /entadd could
	// ask for thousands of asteroids -- one per think, forever, until G_Spawn() ERR_DROPs the
	// server. Clamped to something no real map needs to exceed; the field still refuses individual
	// spawns when the table is tight (see asteroid_field_think).
	if ( self->count > ZYK_MAX_ASTEROIDS )
	{
		Com_Printf( S_COLOR_YELLOW"WARNING: trigger_asteroid_field at %s asked for %d asteroids, clamped to %d\n",
			vtos(self->s.origin), self->count, ZYK_MAX_ASTEROIDS );
		self->count = ZYK_MAX_ASTEROIDS;
	}

	if ( !self->speed )
	{
		self->speed = 10000;
	}

	self->think = asteroid_field_think;
	self->nextthink = level.time + 100;

    trap->LinkEntity((sharedEntity_t *)self);
}

/*
==================================================================================================
GalaxyRP: [SP Maps] single player's trigger_visible and trigger_location.
==================================================================================================
*/

/*QUAKED trigger_visible (.1 .5 .1) (-8 -8 -8) (8 8 8) NOTRACE FORCESIGHT x x x x x INACTIVE
Only fires when a player is looking at it, fires only once then removes itself.

  NOTRACE - Doesn't check that the line of sight is clear (sees through walls, forcefields, etc)
  FORCESIGHT - Only fires for a player with Force Sight active
  INACTIVE - won't check for player visibility until activated

  "radius" - how far this ent can be from the player's eyes, max, and still be considered "seen" (0 = any)
  "FOV" - how far off the centre of the player's view this can be, max, and still be considered "seen" (0 = anywhere in front)
  "target" - what to use when it fires
*/
extern qboolean InFOV3( vec3_t spot, vec3_t from, vec3_t fromAngles, int hFOV, int vFOV );

static void trigger_visible_think( gentity_t *self )
{
	int i;

	self->nextthink = level.time + 500;

	if ( self->flags & FL_INACTIVE )
	{
		return;
	}

	for ( i = 0; i < MAX_CLIENTS; i++ )
	{
		gentity_t	*player = &g_entities[i];
		vec3_t		eyes;

		if ( !player->inuse || !player->client || player->health <= 0
			|| player->client->pers.connected != CON_CONNECTED
			|| player->client->sess.sessionTeam == TEAM_SPECTATOR
			|| player->client->tempSpectate >= level.time )
		{
			continue;
		}

		if ( ( self->spawnflags & 2 ) && !( player->client->ps.fd.forcePowersActive & ( 1 << FP_SEE ) ) )
		{//force sight only
			continue;
		}

		VectorCopy( player->client->ps.origin, eyes );
		eyes[2] += player->client->ps.viewheight;

		if ( self->radius > 0 && Distance( self->s.origin, eyes ) > self->radius )
		{
			continue;
		}

		if ( self->genericValue1 > 0 )
		{//FOV check
			if ( !InFOV3( self->s.origin, eyes, player->client->ps.viewangles, self->genericValue1, self->genericValue1 ) )
			{
				continue;
			}
		}
		else
		{//in front of the player at all?
			vec3_t fwd, dir;

			AngleVectors( player->client->ps.viewangles, fwd, NULL, NULL );
			VectorSubtract( self->s.origin, eyes, dir );
			if ( DotProduct( fwd, dir ) <= 0 )
			{
				continue;
			}
		}

		if ( !( self->spawnflags & 1 ) )
		{//line of sight
			trace_t tr;

			trap->Trace( &tr, eyes, NULL, NULL, self->s.origin, player->s.number, MASK_OPAQUE, qfalse, 0, 0 );
			if ( tr.fraction < 1.0f )
			{
				continue;
			}
		}

		//seen: fire once and go away
		self->think = G_FreeEntity;
		self->nextthink = level.time + 100;
		G_ActivateBehavior( self, BSET_USE );
		G_UseTargets( self, player );
		return;
	}
}

static void trigger_visible_use( gentity_t *self, gentity_t *other, gentity_t *activator )
{
	self->flags ^= FL_INACTIVE;
}

void SP_trigger_visible( gentity_t *self )
{
	G_SpawnInt( "FOV", "0", &self->genericValue1 );

	if ( self->spawnflags & 128 )
	{
		self->flags |= FL_INACTIVE;
	}

	G_SetOrigin( self, self->s.origin );
	self->use = trigger_visible_use;
	self->think = trigger_visible_think;
	self->nextthink = level.time + 500;
}

/*QUAKED trigger_location (.1 .5 .1) ?
When an ent is asked for its location (a script's get(SET_LOCATION), team chat's location), it
returns this ent's "message" field if it is in it.

  "message" - location name

  NOTE: always rectangular
*/
void SP_trigger_location( gentity_t *self )
{
	InitTrigger( self );
	trap->LinkEntity( (sharedEntity_t *)self );
}

// the "message" of the trigger_location the entity stands in, or NULL when it is in none
const char *RP_TriggerLocationName( gentity_t *ent )
{
	int			touch[MAX_GENTITIES];
	int			i, num;
	vec3_t		mins, maxs;

	if ( !ent )
	{
		return NULL;
	}

	VectorAdd( ent->r.currentOrigin, ent->r.mins, mins );
	VectorAdd( ent->r.currentOrigin, ent->r.maxs, maxs );
	num = trap->EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	for ( i = 0; i < num; i++ )
	{
		gentity_t *hit = &g_entities[touch[i]];

		if ( hit->inuse && hit->classname && !strcmp( hit->classname, "trigger_location" ) && hit->message && hit->message[0] )
		{
			return hit->message;
		}
	}

	return NULL;
}
