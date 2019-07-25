/*
=======================================================================================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of Spearmint Source Code.

Spearmint Source Code is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.

Spearmint Source Code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with Spearmint Source Code.
If not, see <http://www.gnu.org/licenses/>.

In addition, Spearmint Source Code is also subject to certain additional terms. You should have received a copy of these additional
terms immediately following the terms and conditions of the GNU General Public License. If not, please request a copy in writing from
id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o
ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
=======================================================================================================================================
*/

/**************************************************************************************************************************************
 This file acts on changes in a new playerState_t.
 With normal play, this will be done after local prediction, but when following another player or playing back a demo, it will be
 checked when the snapshot transitions like all the other entities.
**************************************************************************************************************************************/

#include "cg_local.h"

/*
=======================================================================================================================================
CG_CheckAmmo

If the ammo has gone low enough to generate the warning, play a sound.
=======================================================================================================================================
*/
void CG_CheckAmmo(void) {
	int i;
	int total;
	int previous;
	int weapons;

	// see about how many seconds of ammo we have remaining
	weapons = cg.snap->ps.stats[STAT_WEAPONS];
	total = 0;

	for (i = WP_MACHINEGUN; i < WP_NUM_WEAPONS; i++) {
		if (!(weapons & (1 << i))) {
			continue;
		}

		if (cg.snap->ps.ammo[i] < 0) {
			continue;
		}

		switch (i) {
			case WP_SHOTGUN:
			case WP_PROXLAUNCHER:
			case WP_GRENADELAUNCHER:
			case WP_NAPALMLAUNCHER:
			case WP_ROCKETLAUNCHER:
			case WP_RAILGUN:
				total += cg.snap->ps.ammo[i] * 1000;
				break;
			default:
				total += cg.snap->ps.ammo[i] * 200;
				break;
		}

		if (total >= 5000) {
			cg.lowAmmoWarning = 0;
			return;
		}
	}

	previous = cg.lowAmmoWarning;

	if (total == 0) {
		cg.lowAmmoWarning = 2;
	} else {
		cg.lowAmmoWarning = 1;
	}
	// play a sound on transitions
	if (cg.lowAmmoWarning != previous) {
		trap_S_StartLocalSound(cgs.media.noAmmoSound, CHAN_LOCAL_SOUND);
	}
}

/*
=======================================================================================================================================
CG_DamageFeedback
=======================================================================================================================================
*/
void CG_DamageFeedback(int yawByte, int pitchByte, int damage) {
	float left, front, up;
	float kick;
	int health;
	float scale;
	vec3_t dir;
	vec3_t angles;
	float dist;
	float yaw, pitch;

	// show the attacking player's head and name in corner
	cg.attackerTime = cg.time;
	// the lower on health you are, the greater the view kick will be
	health = cg.snap->ps.stats[STAT_HEALTH];

	if (health < 40) {
		scale = 1;
	} else {
		scale = 40.0 / health;
	}

	kick = damage * scale;

	if (kick < 5) {
		kick = 5;
	}

	if (kick > 10) {
		kick = 10;
	}
	// if yaw and pitch are both 255, make the damage always centered (falling, etc.)
	if (yawByte == 255 && pitchByte == 255) {
		cg.damageX = 0;
		cg.damageY = 0;
		cg.v_dmg_roll = 0;
		cg.v_dmg_pitch = -kick;
	} else {
		// positional
		pitch = pitchByte / 255.0 * 360;
		yaw = yawByte / 255.0 * 360;

		angles[PITCH] = pitch;
		angles[YAW] = yaw;
		angles[ROLL] = 0;

		AngleVectorsForward(angles, dir);
		VectorSubtract(vec3_origin, dir, dir);

		front = DotProduct(dir, cg.refdef.viewaxis[0]);
		left = DotProduct(dir, cg.refdef.viewaxis[1]);
		up = DotProduct(dir, cg.refdef.viewaxis[2]);

		dir[0] = front;
		dir[1] = left;
		dir[2] = 0;
		dist = VectorLength(dir);

		if (dist < 0.1) {
			dist = 0.1f;
		}

		cg.v_dmg_roll = kick * left;
		cg.v_dmg_pitch = -kick * front;

		if (front <= 0.1) {
			front = 0.1f;
		}

		cg.damageX = -left / front;
		cg.damageY = up / dist;
	}
	// clamp the position
	if (cg.damageX > 1.0) {
		cg.damageX = 1.0;
	}

	if (cg.damageX < -1.0) {
		cg.damageX = -1.0;
	}

	if (cg.damageY > 1.0) {
		cg.damageY = 1.0;
	}

	if (cg.damageY < -1.0) {
		cg.damageY = -1.0;
	}
	// don't let the screen flashes vary as much
	if (kick > 10) {
		kick = 10;
	}

	cg.damageValue = kick;
	cg.v_dmg_time = cg.time + DAMAGE_TIME;
	cg.damageTime = cg.snap->serverTime;
}

/*
=======================================================================================================================================
CG_Respawn

A respawn happened this snapshot.
=======================================================================================================================================
*/
void CG_Respawn(void) {

	// no error decay on player movement
	cg.thisFrameTeleport = qtrue;
	// display weapons available
	cg.weaponSelectTime = cg.time;
	// select the weapon the server says we are using
	cg.weaponSelect = cg.snap->ps.weapon;
}

extern char *eventnames[];
/*
=======================================================================================================================================
CG_CheckPlayerstateEvents
=======================================================================================================================================
*/
void CG_CheckPlayerstateEvents(playerState_t *ps, playerState_t *ops) {
	int i;
	int event;
	centity_t *cent;

	if (ps->externalEvent && ps->externalEvent != ops->externalEvent) {
		cent = &cg_entities[ps->clientNum];
		cent->currentState.event = ps->externalEvent;
		cent->currentState.eventParm = ps->externalEventParm;
		CG_EntityEvent(cent, cent->lerpOrigin);
	}

	cent = &cg.predictedPlayerEntity; // cg_entities[ps->clientNum];
	// go through the predictable events buffer
	for (i = ps->eventSequence - MAX_PS_EVENTS; i < ps->eventSequence; i++) {
		// if we have a new predictable event or the server told us to play another event instead of a predicted event we already issued
		// or something the server told us changed our prediction causing a different event
		if (i >= ops->eventSequence || (i > ops->eventSequence - MAX_PS_EVENTS && ps->events[i & (MAX_PS_EVENTS - 1)] != ops->events[i & (MAX_PS_EVENTS - 1)])) {
			event = ps->events[i & (MAX_PS_EVENTS - 1)];
			cent->currentState.event = event;
			cent->currentState.eventParm = ps->eventParms[i & (MAX_PS_EVENTS - 1)];

			CG_EntityEvent(cent, cent->lerpOrigin);

			cg.predictableEvents[i & (MAX_PREDICTED_EVENTS - 1)] = event;
			cg.eventSequence++;
		}
	}
}

/*
=======================================================================================================================================
CG_CheckChangedPredictableEvents
=======================================================================================================================================
*/
void CG_CheckChangedPredictableEvents(playerState_t *ps) {
	int i;
	int event;
	centity_t *cent;

	cent = &cg.predictedPlayerEntity;

	for (i = ps->eventSequence - MAX_PS_EVENTS; i < ps->eventSequence; i++) {
		if (i >= cg.eventSequence) {
			continue;
		}
		// if this event is not further back in than the maximum predictable events we remember
		if (i > cg.eventSequence - MAX_PREDICTED_EVENTS) {
			// if the new playerstate event is different from a previously predicted one
			if (ps->events[i & (MAX_PS_EVENTS - 1)] != cg.predictableEvents[i & (MAX_PREDICTED_EVENTS - 1)]) {
				event = ps->events[i & (MAX_PS_EVENTS - 1)];
				cent->currentState.event = event;
				cent->currentState.eventParm = ps->eventParms[i & (MAX_PS_EVENTS - 1)];

				CG_EntityEvent(cent, cent->lerpOrigin);

				cg.predictableEvents[i & (MAX_PREDICTED_EVENTS - 1)] = event;

				if (cg_showmiss.integer) {
					CG_Printf("WARNING: changed predicted event\n");
				}
			}
		}
	}
}

/*
=======================================================================================================================================
CG_CheckLocalSounds
=======================================================================================================================================
*/
void CG_CheckLocalSounds(playerState_t *ps, playerState_t *ops) {
	int highScore;
	int health, armor;

	// don't play the sounds if the player just changed teams
	if (ps->persistant[PERS_TEAM] != ops->persistant[PERS_TEAM]) {
		return;
	}
	// if we are going into the intermission, don't start any voices
	if (cg.intermissionStarted) {
		return;
	}
	// health changes of more than -1 should make pain sounds
	if (ps->stats[STAT_HEALTH] < ops->stats[STAT_HEALTH] - 1) {
		if (ps->stats[STAT_HEALTH] > 0) {
			CG_PainEvent(&cg.predictedPlayerEntity, ps->stats[STAT_HEALTH]);
		}
	}
	// never start any voices during warmup
	if (cg.warmup) {
		return;
	}
	// don't play the sounds in mission mode
	if (cgs.gametype > GT_HARVESTER) {
		return;
	}
	// hit changes
	if (cg_hitSounds.integer) {
		if (ps->persistant[PERS_HITS] > ops->persistant[PERS_HITS]) {
			armor = ps->persistant[PERS_ATTACKEE_ARMOR] & 0xff;
			health = ps->persistant[PERS_ATTACKEE_ARMOR] >> 8;

			if (armor > 50) {
				trap_S_StartLocalSound(cgs.media.hitSoundHighArmor, CHAN_LOCAL_SOUND);
			} else if (armor || health > 100) {
				trap_S_StartLocalSound(cgs.media.hitSoundLowArmor, CHAN_LOCAL_SOUND);
			} else {
				if (ps->persistant[PERS_SCORE] > ops->persistant[PERS_SCORE]) {
					trap_S_StartLocalSound(cgs.media.killSound, CHAN_LOCAL_SOUND);
				} else {
					trap_S_StartLocalSound(cgs.media.hitSound, CHAN_LOCAL_SOUND);
				}
			}
		} else if (ps->persistant[PERS_HITS] < ops->persistant[PERS_HITS]) {
			trap_S_StartLocalSound(cgs.media.hitTeamSound, CHAN_LOCAL_SOUND);
		}
	}
	// check for flag pickup
	if (cgs.gametype > GT_TEAM) {
		if ((ps->powerups[PW_REDFLAG] != ops->powerups[PW_REDFLAG] && ps->powerups[PW_REDFLAG]) || (ps->powerups[PW_BLUEFLAG] != ops->powerups[PW_BLUEFLAG] && ps->powerups[PW_BLUEFLAG]) || (ps->powerups[PW_NEUTRALFLAG] != ops->powerups[PW_NEUTRALFLAG] && ps->powerups[PW_NEUTRALFLAG])) {
			CG_AddBufferedAnnouncerSound(cgs.media.youHaveFlagSound);
		}
	}
	// lead changes
	if (ps->persistant[PERS_RANK] != ops->persistant[PERS_RANK]) {
		if (cgs.gametype < GT_TEAM) {
			if (ps->persistant[PERS_RANK] == 0) {
				CG_AddBufferedAnnouncerSound(cgs.media.takenLeadSound);
			} else if (ps->persistant[PERS_RANK] == RANK_TIED_FLAG) {
				CG_AddBufferedAnnouncerSound(cgs.media.tiedLeadSound);
			} else if ((ops->persistant[PERS_RANK] & ~RANK_TIED_FLAG) == 0) {
				CG_AddBufferedAnnouncerSound(cgs.media.lostLeadSound);
			}
		}
	}
	// fraglimit warnings
	if (cgs.fraglimit > 0 && cgs.gametype < GT_CTF) {
		highScore = cgs.scores1;

		if (cgs.gametype == GT_TEAM && cgs.scores2 > highScore) {
			highScore = cgs.scores2;
		}

		if (!(cg.fraglimitWarnings & 4) && highScore == (cgs.fraglimit - 1)) {
			cg.fraglimitWarnings |= 1|2|4;
			CG_AddBufferedAnnouncerSound(cgs.media.oneFragSound);
		} else if (cgs.fraglimit > 2 && !(cg.fraglimitWarnings & 2) && highScore == (cgs.fraglimit - 2)) {
			cg.fraglimitWarnings |= 1|2;
			CG_AddBufferedAnnouncerSound(cgs.media.twoFragSound);
		} else if (cgs.fraglimit > 3 && !(cg.fraglimitWarnings & 1) && highScore == (cgs.fraglimit - 3)) {
			cg.fraglimitWarnings |= 1;
			CG_AddBufferedAnnouncerSound(cgs.media.threeFragSound);
		}
	}
	// timelimit warnings
	if (cgs.timelimit > 0) {
		int msec;

		msec = cg.time - cgs.levelStartTime;

		if (!(cg.timelimitWarnings & 4) && msec > (cgs.timelimit * 60 + 2) * 1000) {
			cg.timelimitWarnings |= 1|2|4;
			CG_AddBufferedAnnouncerSound(cgs.media.suddenDeathSound);
		} else if (!(cg.timelimitWarnings & 2) && msec > (cgs.timelimit - 1) * 60 * 1000) {
			cg.timelimitWarnings |= 1|2;
			CG_AddBufferedAnnouncerSound(cgs.media.oneMinuteSound);
		} else if (cgs.timelimit > 5 && !(cg.timelimitWarnings & 1) && msec > (cgs.timelimit - 5) * 60 * 1000) {
			cg.timelimitWarnings |= 1;
			CG_AddBufferedAnnouncerSound(cgs.media.fiveMinuteSound);
		}
	}
	// reward sounds will only play if no other announcer sounds will be played
	if (CG_HasBufferedAnnouncerSound()) {
		return;
	}
	// if any of the player event bits changed
	if (ps->persistant[PERS_PLAYEREVENTS] != ops->persistant[PERS_PLAYEREVENTS]) {
		if ((ps->persistant[PERS_PLAYEREVENTS] & PLAYEREVENT_DENIEDREWARD) != (ops->persistant[PERS_PLAYEREVENTS] & PLAYEREVENT_DENIEDREWARD)) {
			trap_S_StartLocalSound(cgs.media.deniedSound, CHAN_ANNOUNCER);
		} else if ((ps->persistant[PERS_PLAYEREVENTS] & PLAYEREVENT_GAUNTLETREWARD) != (ops->persistant[PERS_PLAYEREVENTS] & PLAYEREVENT_GAUNTLETREWARD)) {
			trap_S_StartLocalSound(cgs.media.humiliationSound, CHAN_ANNOUNCER);
		} else if ((ps->persistant[PERS_PLAYEREVENTS] & PLAYEREVENT_HOLYSHIT) != (ops->persistant[PERS_PLAYEREVENTS] & PLAYEREVENT_HOLYSHIT)) {
			trap_S_StartLocalSound(cgs.media.holyShitSound, CHAN_ANNOUNCER);
		}
	}
	// reward excellent sound
	if (ps->persistant[PERS_EXCELLENT_COUNT] != ops->persistant[PERS_EXCELLENT_COUNT]) {
		if (ps->persistant[PERS_EXCELLENT_COUNT] == 1) {
			CG_AddBufferedAnnouncerSound(cgs.media.firstExcellentSound);
		}
		
		CG_AddBufferedAnnouncerSound(cgs.media.excellentSound);
	}
	// reward impressive sound
	if (ps->persistant[PERS_IMPRESSIVE_COUNT] != ops->persistant[PERS_IMPRESSIVE_COUNT]) {
		if (ps->persistant[PERS_IMPRESSIVE_COUNT] == 1) {
			CG_AddBufferedAnnouncerSound(cgs.media.firstImpressiveSound);
		} else {
			CG_AddBufferedAnnouncerSound(cgs.media.impressiveSound);
		}
	}
	// reward gauntlet sound
	if (ps->persistant[PERS_GAUNTLET_FRAG_COUNT] != ops->persistant[PERS_GAUNTLET_FRAG_COUNT]) {
		if (ps->persistant[PERS_GAUNTLET_FRAG_COUNT] == 1) {
			CG_AddBufferedAnnouncerSound(cgs.media.firstHumiliationSound);
		} else {
			CG_AddBufferedAnnouncerSound(cgs.media.humiliationSound);
		}
	}
	// reward defend sound
	if (ps->persistant[PERS_DEFEND_COUNT] != ops->persistant[PERS_DEFEND_COUNT]) {
		CG_AddBufferedAnnouncerSound(cgs.media.defendSound);
	}
	// reward assist sound
	if (ps->persistant[PERS_ASSIST_COUNT] != ops->persistant[PERS_ASSIST_COUNT]) {
		CG_AddBufferedAnnouncerSound(cgs.media.assistSound);
	}
}

/*
=======================================================================================================================================
CG_TransitionPlayerState
=======================================================================================================================================
*/
void CG_TransitionPlayerState(playerState_t *ps, playerState_t *ops) {

	// check for changing follow mode
	if (ps->clientNum != ops->clientNum) {
		cg.thisFrameTeleport = qtrue;
		// make sure we don't get any unwanted transition effects
		*ops = *ps;
	}
	// damage events (player is getting wounded)
	if (ps->damageEvent != ops->damageEvent && ps->damageCount) {
		CG_DamageFeedback(ps->damageYaw, ps->damagePitch, ps->damageCount);
	}
	// respawning
	if (ps->persistant[PERS_SPAWN_COUNT] != ops->persistant[PERS_SPAWN_COUNT]) {
		CG_Respawn();
	}

	if (cg.mapRestart) {
		CG_Respawn();
		cg.mapRestart = qfalse;
	}

	if (cg.snap->ps.pm_type != PM_INTERMISSION && ps->persistant[PERS_TEAM] != TEAM_SPECTATOR) {
		CG_CheckLocalSounds(ps, ops);
	}
	// check for going low on ammo
	CG_CheckAmmo();
	// run events
	CG_CheckPlayerstateEvents(ps, ops);
	// smooth the ducking viewheight change
	if (ps->viewheight != ops->viewheight) {
		cg.duckChange = ps->viewheight - ops->viewheight;
		cg.duckTime = cg.time;
	}
}
