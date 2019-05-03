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
 Bot movement AI
**************************************************************************************************************************************/

#include "../qcommon/q_shared.h"
#include "l_memory.h"
#include "l_libvar.h"
#include "l_utils.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_ea.h"
#include "be_ai_goal.h"
#include "be_ai_move.h"

//#define DEBUG_AI_MOVE
//#define DEBUG_ELEVATOR
// movement state
// NOTE: the moveflags MFL_ONGROUND, MFL_WATERJUMP and MFL_TELEPORTED must be set outside the movement code
typedef struct bot_movestate_s {
	// input vars (all set outside the movement code)
	vec3_t origin;								// origin of the bot
	vec3_t velocity;							// velocity of the bot
	vec3_t viewoffset;							// view offset
	int entitynum;								// entity number of the bot
	int client;									// client number of the bot
	float thinktime;							// time the bot thinks
	int presencetype;							// presencetype of the bot
	vec3_t viewangles;							// view angles of the bot
	// state vars
	int areanum;								// area the bot is in
	int lastareanum;							// last area the bot was in
	int lastgoalareanum;						// last goal area number
	int lastreachnum;							// last reachability number
	vec3_t lastorigin;							// origin previous cycle
	int reachareanum;							// area number of the reachabilty
	int moveflags;								// movement flags
	int jumpreach;								// set when jumped
	float reachability_time;					// time to use current reachability
	int avoidreach[MAX_AVOIDREACH];				// reachabilities to avoid
	float avoidreachtimes[MAX_AVOIDREACH];		// times to avoid the reachabilities
	int avoidreachtries[MAX_AVOIDREACH];		// number of tries before avoiding
	bot_avoidspot_t avoidspots[MAX_AVOIDSPOTS];	// spots to avoid
	int numavoidspots;
} bot_movestate_t;
// used to avoid reachability links for some time after being used
#define AVOIDREACH
#define AVOIDREACH_TIME 6 // avoid links for 6 seconds after use
#define AVOIDREACH_TRIES 4
// prediction times
#define PREDICTIONTIME_JUMP 3 // in seconds
#define PREDICTIONTIME_MOVE 2 // in seconds

#define MODELTYPE_FUNC_PLAT		1
#define MODELTYPE_FUNC_BOB		2
#define MODELTYPE_FUNC_DOOR		3
#define MODELTYPE_FUNC_STATIC	4

libvar_t *sv_maxstep;
libvar_t *sv_maxbarrier;
libvar_t *sv_gravity;
libvar_t *weapindex_rocketlauncher;
libvar_t *weapindex_bfg10k;
// type of model, func_plat or func_bobbing
int modeltypes[MAX_SUBMODELS];

bot_movestate_t *botmovestates[MAX_CLIENTS + 1];

/*
=======================================================================================================================================
BotAllocMoveState
=======================================================================================================================================
*/
int BotAllocMoveState(void) {
	int i;

	for (i = 1; i <= MAX_CLIENTS; i++) {
		if (!botmovestates[i]) {
			botmovestates[i] = GetClearedMemory(sizeof(bot_movestate_t));
			return i;
		}
	}

	return 0;
}

/*
=======================================================================================================================================
BotFreeMoveState
=======================================================================================================================================
*/
void BotFreeMoveState(int handle) {

	if (handle <= 0 || handle > MAX_CLIENTS) {
		botimport.Print(PRT_FATAL, "move state handle %d out of range\n", handle);
		return;
	}

	if (!botmovestates[handle]) {
		botimport.Print(PRT_FATAL, "invalid move state %d\n", handle);
		return;
	}

	FreeMemory(botmovestates[handle]);

	botmovestates[handle] = NULL;
}

/*
=======================================================================================================================================
BotMoveStateFromHandle
=======================================================================================================================================
*/
bot_movestate_t *BotMoveStateFromHandle(int handle) {

	if (handle <= 0 || handle > MAX_CLIENTS) {
		botimport.Print(PRT_FATAL, "move state handle %d out of range\n", handle);
		return NULL;
	}

	if (!botmovestates[handle]) {
		botimport.Print(PRT_FATAL, "invalid move state %d\n", handle);
		return NULL;
	}

	return botmovestates[handle];
}

/*
=======================================================================================================================================
BotInitMoveState
=======================================================================================================================================
*/
void BotInitMoveState(int handle, bot_initmove_t *initmove) {
	bot_movestate_t *ms;

	ms = BotMoveStateFromHandle(handle);

	if (!ms) {
		return;
	}

	VectorCopy(initmove->origin, ms->origin);
	VectorCopy(initmove->velocity, ms->velocity);
	VectorCopy(initmove->viewoffset, ms->viewoffset);

	ms->entitynum = initmove->entitynum;
	ms->client = initmove->client;
	ms->thinktime = initmove->thinktime;
	ms->presencetype = initmove->presencetype;

	VectorCopy(initmove->viewangles, ms->viewangles);

	ms->moveflags &= ~MFL_ONGROUND;

	if (initmove->or_moveflags & MFL_ONGROUND) {
		ms->moveflags |= MFL_ONGROUND;
	}

	ms->moveflags &= ~MFL_WATERJUMP;

	if (initmove->or_moveflags & MFL_WATERJUMP) {
		ms->moveflags |= MFL_WATERJUMP;
	}

	ms->moveflags &= ~MFL_SCOUT;

	if (initmove->or_moveflags & MFL_SCOUT) {
		ms->moveflags |= MFL_SCOUT;
	}

	ms->moveflags &= ~MFL_WALK;

	if (initmove->or_moveflags & MFL_WALK) {
		ms->moveflags |= MFL_WALK;
	}

	ms->moveflags &= ~MFL_TELEPORTED;

	if (initmove->or_moveflags & MFL_TELEPORTED) {
		ms->moveflags |= MFL_TELEPORTED;
	}
}

/*
=======================================================================================================================================
AngleDiff
=======================================================================================================================================
*/
float AngleDiff(float ang1, float ang2) {
	float diff;

	diff = ang1 - ang2;

	if (ang1 > ang2) {
		if (diff > 180.0) {
			diff -= 360.0;
		}
	} else {
		if (diff < -180.0) {
			diff += 360.0;
		}
	}

	return diff;
}
#ifndef BASEGAME // Tobias DEBUG
/*
=======================================================================================================================================
BotPrintTravelType
=======================================================================================================================================
*/
void BotPrintTravelType(int traveltype) {
	char *str;

	switch (traveltype & TRAVELTYPE_MASK) {
		case TRAVEL_INVALID:
			str = "TRAVEL_INVALID";
			break;
		case TRAVEL_WALK:
			str = "TRAVEL_WALK";
			break;
		case TRAVEL_CROUCH:
			str = "TRAVEL_CROUCH";
			break;
		case TRAVEL_BARRIERJUMP:
			str = "TRAVEL_BARRIERJUMP";
			break;
		case TRAVEL_JUMP:
			str = "TRAVEL_JUMP";
			break;
		case TRAVEL_LADDER:
			str = "TRAVEL_LADDER";
			break;
		case TRAVEL_WALKOFFLEDGE:
			str = "TRAVEL_WALKOFFLEDGE";
			break;
		case TRAVEL_SWIM:
			str = "TRAVEL_SWIM";
			break;
		case TRAVEL_WATERJUMP:
			str = "TRAVEL_WATERJUMP";
			break;
		case TRAVEL_TELEPORT:
			str = "TRAVEL_TELEPORT";
			break;
		case TRAVEL_ELEVATOR:
			str = "TRAVEL_ELEVATOR";
			break;
		case TRAVEL_ROCKETJUMP:
			str = "TRAVEL_ROCKETJUMP";
			break;
		case TRAVEL_BFGJUMP:
			str = "TRAVEL_BFGJUMP";
			break;
		case TRAVEL_DOUBLEJUMP:
			str = "TRAVEL_DOUBLEJUMP";
			break;
		case TRAVEL_RAMPJUMP:
			str = "TRAVEL_RAMPJUMP";
			break;
		case TRAVEL_STRAFEJUMP:
			str = "TRAVEL_STRAFEJUMP";
			break;
		case TRAVEL_JUMPPAD:
			str = "TRAVEL_JUMPPAD";
			break;
		case TRAVEL_FUNCBOB:
			str = "TRAVEL_FUNCBOB";
			break;
		default:
			botimport.Print(PRT_MESSAGE, S_COLOR_RED "UNKNOWN TRAVEL TYPE (%d)" S_COLOR_WHITE, (traveltype & TRAVELTYPE_MASK));
			return;
	}

	botimport.Print(PRT_MESSAGE, "%s", str);
}
#endif // Tobias END
/*
=======================================================================================================================================
BotFuzzyPointReachabilityArea
=======================================================================================================================================
*/
int BotFuzzyPointReachabilityArea(vec3_t origin) {
	int firstareanum, j, x, y, z;
	int areas[10], numareas, areanum, bestareanum;
	float dist, bestdist;
	vec3_t points[10], v, end;

	firstareanum = 0;
	areanum = AAS_PointAreaNum(origin);

	if (areanum) {
		firstareanum = areanum;

		if (AAS_AreaReachability(areanum)) {
			return areanum;
		}
	}

	VectorCopy(origin, end);

	end[2] += 4;
	numareas = AAS_TraceAreas(origin, end, areas, points, 10);

	for (j = 0; j < numareas; j++) {
		if (AAS_AreaReachability(areas[j])) {
			return areas[j];
		}
	}

	bestdist = 999999;
	bestareanum = 0;

	for (z = 1; z >= -1; z -= 1) {
		for (x = 1; x >= -1; x -= 1) {
			for (y = 1; y >= -1; y -= 1) {
				VectorCopy(origin, end);

				end[0] += x * 8;
				end[1] += y * 8;
				end[2] += z * 12;

				numareas = AAS_TraceAreas(origin, end, areas, points, 10);

				for (j = 0; j < numareas; j++) {
					if (AAS_AreaReachability(areas[j])) {
						VectorSubtract(points[j], origin, v);

						dist = VectorLength(v);

						if (dist < bestdist) {
							bestareanum = areas[j];
							bestdist = dist;
						}
					}

					if (!firstareanum) {
						firstareanum = areas[j];
					}
				}
			}
		}

		if (bestareanum) {
			return bestareanum;
		}
	}

	return firstareanum;
}

/*
=======================================================================================================================================
BotReachabilityArea

Returns the reachability area the bot is in.
=======================================================================================================================================
*/
int BotReachabilityArea(vec3_t origin, int client) {
	int modelnum, modeltype, reachnum, areanum;
	aas_reachability_t reach;
	vec3_t org, end, mins, maxs, up = {0, 0, 1};
	bsp_trace_t bsptrace;
	aas_trace_t trace;

	// check if the bot is standing on something
	AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, mins, maxs);
	VectorMA(origin, -4, up, end);
	bsptrace = AAS_Trace(origin, mins, maxs, end, client, CONTENTS_SOLID|CONTENTS_PLAYERCLIP);

	if (!bsptrace.startsolid && bsptrace.fraction < 1 && bsptrace.entityNum != ENTITYNUM_NONE) {
		// if standing on the world the bot should be in a valid area
		if (bsptrace.entityNum == ENTITYNUM_WORLD) {
			return BotFuzzyPointReachabilityArea(origin);
		}

		modelnum = AAS_EntityModelindex(bsptrace.entityNum);
		modeltype = modeltypes[modelnum];
		// if standing on a func_plat or func_bobbing then the bot is assumed to be in the area the reachability points to
		if (modeltype == MODELTYPE_FUNC_PLAT || modeltype == MODELTYPE_FUNC_BOB) {
			reachnum = AAS_NextModelReachability(0, modelnum);

			if (reachnum) {
				AAS_ReachabilityFromNum(reachnum, &reach);
				return reach.areanum;
			}
		}
		// if the bot is swimming the bot should be in a valid area
		if (AAS_Swimming(origin)) {
			return BotFuzzyPointReachabilityArea(origin);
		}

		areanum = BotFuzzyPointReachabilityArea(origin);
		// if the bot is in an area with reachabilities
		if (areanum && AAS_AreaReachability(areanum)) {
			return areanum;
		}
		// trace down till the ground is hit because the bot is standing on some other entity
		VectorCopy(origin, org);
		VectorCopy(org, end);

		end[2] -= 800;
		trace = AAS_TraceClientBBox(org, end, PRESENCE_CROUCH, -1);

		if (!trace.startsolid) {
			VectorCopy(trace.endpos, org);
		}

		return BotFuzzyPointReachabilityArea(org);
	}

	return BotFuzzyPointReachabilityArea(origin);
}

/*
=======================================================================================================================================
BotReachabilityArea
=======================================================================================================================================
*/
/*
int BotReachabilityArea(vec3_t origin, int testground) {
	int firstareanum, i, j, x, y, z;
	int areas[10], numareas, areanum, bestareanum;
	float dist, bestdist;
	vec3_t org, end, points[10], v;
	aas_trace_t trace;

	firstareanum = 0;

	for (i = 0; i < 2; i++) {
		VectorCopy(origin, org);
		// if test at the ground (used when bot is standing on an entity)
		if (i > 0) {
			VectorCopy(origin, end);
			end[2] -= 800;
			trace = AAS_TraceClientBBox(origin, end, PRESENCE_CROUCH, -1);

			if (!trace.startsolid) {
				VectorCopy(trace.endpos, org);
			}
		}

		firstareanum = 0;
		areanum = AAS_PointAreaNum(org);

		if (areanum) {
			firstareanum = areanum;

			if (AAS_AreaReachability(areanum)) {
				return areanum;
			}
		}

		bestdist = 999999;
		bestareanum = 0;

		for (z = 1; z >= -1; z -= 1) {
			for (x = 1; x >= -1; x -= 1) {
				for (y = 1; y >= -1; y -= 1) {
					VectorCopy(org, end);
					end[0] += x * 8;
					end[1] += y * 8;
					end[2] += z * 12;
					numareas = AAS_TraceAreas(org, end, areas, points, 10);

					for (j = 0; j < numareas; j++) {
						if (AAS_AreaReachability(areas[j])) {
							VectorSubtract(points[j], org, v);

							dist = VectorLength(v);

							if (dist < bestdist) {
								bestareanum = areas[j];
								bestdist = dist;
							}
						}
					}
				}
			}

			if (bestareanum) {
				return bestareanum;
			}
		}

		if (!testground) {
			break;
		}
	}
//#ifndef BASEGAME // Tobias DEBUG
	//botimport.Print(PRT_MESSAGE, "no reachability area\n");
//#endif // Tobias END
	return firstareanum;
}
*/
/*
=======================================================================================================================================
BotOnMover
=======================================================================================================================================
*/
int BotOnMover(vec3_t origin, int entnum, aas_reachability_t *reach) {
	int i, modelnum;
	vec3_t mins, maxs, modelorigin, org, end;
	vec3_t angles = {0, 0, 0};
	vec3_t boxmins = {-16, -16, -8}, boxmaxs = {16, 16, 8};
	bsp_trace_t trace;

	modelnum = reach->facenum & 0x0000FFFF;
	// get some bsp model info
	AAS_BSPModelMinsMaxsOrigin(modelnum, angles, mins, maxs, NULL);

	if (!AAS_OriginOfMoverWithModelNum(modelnum, modelorigin)) {
		botimport.Print(PRT_MESSAGE, "no entity with model %d\n", modelnum);
		return qfalse;
	}

	for (i = 0; i < 2; i++) {
		if (origin[i] > modelorigin[i] + maxs[i] + 16) {
			return qfalse;
		}

		if (origin[i] < modelorigin[i] + mins[i] - 16) {
			return qfalse;
		}
	}

	VectorCopy(origin, org);
	org[2] += 24;
	VectorCopy(origin, end);
	end[2] -= 48;

	trace = AAS_Trace(org, boxmins, boxmaxs, end, entnum, CONTENTS_SOLID|CONTENTS_PLAYERCLIP);

	if (!trace.startsolid && !trace.allsolid) {
		// NOTE: the reachability face number is the model number of the elevator
		if (trace.entityNum != ENTITYNUM_NONE && AAS_EntityModelNum(trace.entityNum) == modelnum) {
			return qtrue;
		}
	}

	return qfalse;
}

/*
=======================================================================================================================================
MoverDown
=======================================================================================================================================
*/
int MoverDown(aas_reachability_t *reach) {
	int modelnum;
	vec3_t mins, maxs, origin;
	vec3_t angles = {0, 0, 0};

	modelnum = reach->facenum & 0x0000FFFF;
	// get some bsp model info
	AAS_BSPModelMinsMaxsOrigin(modelnum, angles, mins, maxs, origin);

	if (!AAS_OriginOfMoverWithModelNum(modelnum, origin)) {
		botimport.Print(PRT_MESSAGE, "no entity with model %d\n", modelnum);
		return qfalse;
	}
	// if the top of the plat is below the reachability start point
	if (origin[2] + maxs[2] < reach->start[2]) {
		return qtrue;
	}

	return qfalse;
}

/*
=======================================================================================================================================
BotSetBrushModelTypes
=======================================================================================================================================
*/
void BotSetBrushModelTypes(void) {
	int ent, modelnum;
	char classname[MAX_EPAIRKEY], model[MAX_EPAIRKEY];

	Com_Memset(modeltypes, 0, MAX_SUBMODELS * sizeof(int));

	for (ent = AAS_NextBSPEntity(0); ent; ent = AAS_NextBSPEntity(ent)) {
		if (!AAS_ValueForBSPEpairKey(ent, "classname", classname, MAX_EPAIRKEY)) {
			continue;
		}

		if (!AAS_ValueForBSPEpairKey(ent, "model", model, MAX_EPAIRKEY)) {
			continue;
		}

		if (model[0]) {
			modelnum = atoi(model + 1);
		} else {
			modelnum = 0;
		}

		if (modelnum < 0 || modelnum >= MAX_SUBMODELS) {
			botimport.Print(PRT_MESSAGE, "entity %s model number out of range\n", classname);
			continue;
		}

		if (!Q_stricmp(classname, "func_bobbing")) {
			modeltypes[modelnum] = MODELTYPE_FUNC_BOB;
		} else if (!Q_stricmp(classname, "func_plat")) {
			modeltypes[modelnum] = MODELTYPE_FUNC_PLAT;
		} else if (!Q_stricmp(classname, "func_door")) {
			modeltypes[modelnum] = MODELTYPE_FUNC_DOOR;
		} else if (!Q_stricmp(classname, "func_static")) {
			modeltypes[modelnum] = MODELTYPE_FUNC_STATIC;
		}
	}
}

/*
=======================================================================================================================================
BotOnTopOfEntity
=======================================================================================================================================
*/
int BotOnTopOfEntity(bot_movestate_t *ms) {
	vec3_t mins, maxs, end, up = {0, 0, 1};
	bsp_trace_t trace;

	AAS_PresenceTypeBoundingBox(ms->presencetype, mins, maxs);
	VectorMA(ms->origin, -4, up, end);
	trace = AAS_Trace(ms->origin, mins, maxs, end, ms->entitynum, CONTENTS_SOLID|CONTENTS_PLAYERCLIP);
	// if not started in solid and NOT hitting the world entity
	if (!trace.startsolid && (trace.entityNum != ENTITYNUM_WORLD && trace.entityNum != ENTITYNUM_NONE)) {
		return trace.entityNum;
	}

	return -1;
}

/*
=======================================================================================================================================
BotValidTravel
=======================================================================================================================================
*/
int BotValidTravel(vec3_t origin, aas_reachability_t *reach, int travelflags) {

	// if the reachability uses an unwanted travel type
	if (AAS_TravelFlagForType(reach->traveltype) & ~travelflags) {
		return qfalse;
	}
	// don't go into areas with bad travel types
	if (AAS_AreaContentsTravelFlags(reach->areanum) & ~travelflags) {
		return qfalse;
	}

	return qtrue;
}

/*
=======================================================================================================================================
BotAddToAvoidReach
=======================================================================================================================================
*/
void BotAddToAvoidReach(bot_movestate_t *ms, int number, float avoidtime) {
	int i;

	for (i = 0; i < MAX_AVOIDREACH; i++) {
		if (ms->avoidreach[i] == number) {
			if (ms->avoidreachtimes[i] > AAS_Time()) {
				ms->avoidreachtries[i]++;
			} else {
				ms->avoidreachtries[i] = 1;
			}

			ms->avoidreachtimes[i] = AAS_Time() + avoidtime;
			return;
		}
	}
	// add the reachability to the reachabilities to avoid for a while
	for (i = 0; i < MAX_AVOIDREACH; i++) {
		if (ms->avoidreachtimes[i] < AAS_Time()) {
			ms->avoidreach[i] = number;
			ms->avoidreachtimes[i] = AAS_Time() + avoidtime;
			ms->avoidreachtries[i] = 1;
			return;
		}
	}
}

/*
=======================================================================================================================================
DistanceFromLineSquared
=======================================================================================================================================
*/
float DistanceFromLineSquared(vec3_t p, vec3_t lp1, vec3_t lp2) {
	vec3_t proj, dir;
	int j;

	AAS_ProjectPointOntoVector(p, lp1, lp2, proj);

	for (j = 0; j < 3; j++) {
		if ((proj[j] > lp1[j] && proj[j] > lp2[j]) || (proj[j] < lp1[j] && proj[j] < lp2[j])) {
			break;
		}
	}

	if (j < 3) {
		if (fabs(proj[j] - lp1[j]) < fabs(proj[j] - lp2[j])) {
			VectorSubtract(p, lp1, dir);
		} else {
			VectorSubtract(p, lp2, dir);
		}

		return VectorLengthSquared(dir);
	}

	VectorSubtract(p, proj, dir);
	return VectorLengthSquared(dir);
}

/*
=======================================================================================================================================
VectorDistanceSquared
=======================================================================================================================================
*/
float VectorDistanceSquared(vec3_t p1, vec3_t p2) {
	vec3_t dir;

	VectorSubtract(p2, p1, dir);
	return VectorLengthSquared(dir);
}

/*
=======================================================================================================================================
BotAvoidSpots
=======================================================================================================================================
*/
int BotAvoidSpots(vec3_t origin, aas_reachability_t *reach, bot_avoidspot_t *avoidspots, int numavoidspots) {
	int checkbetween, i, type;
	float squareddist, squaredradius;

	switch (reach->traveltype & TRAVELTYPE_MASK) {
		case TRAVEL_WALK:
			checkbetween = qtrue;
			break;
		case TRAVEL_CROUCH:
			checkbetween = qtrue;
			break;
		case TRAVEL_BARRIERJUMP:
			checkbetween = qtrue;
			break;
		case TRAVEL_LADDER:
			checkbetween = qtrue;
			break;
		case TRAVEL_WALKOFFLEDGE:
			checkbetween = qfalse;
			break;
		case TRAVEL_JUMP:
			checkbetween = qfalse;
			break;
		case TRAVEL_SWIM:
			checkbetween = qtrue;
			break;
		case TRAVEL_WATERJUMP:
			checkbetween = qtrue;
			break;
		case TRAVEL_TELEPORT:
			checkbetween = qfalse;
			break;
		case TRAVEL_ELEVATOR:
			checkbetween = qfalse;
			break;
		case TRAVEL_ROCKETJUMP:
			checkbetween = qfalse;
			break;
		case TRAVEL_BFGJUMP:
			checkbetween = qfalse;
			break;
		case TRAVEL_JUMPPAD:
			checkbetween = qfalse;
			break;
		case TRAVEL_FUNCBOB:
			checkbetween = qfalse;
			break;
		default:
			checkbetween = qtrue;
			break;
	}

	type = AVOID_CLEAR;

	for (i = 0; i < numavoidspots; i++) {
		squaredradius = Square(avoidspots[i].radius);
		squareddist = DistanceFromLineSquared(avoidspots[i].origin, origin, reach->start);
		// if moving towards the avoid spot
		if (squareddist < squaredradius && VectorDistanceSquared(avoidspots[i].origin, origin) > squareddist) {
			type = avoidspots[i].type;
		} else if (checkbetween) {
			squareddist = DistanceFromLineSquared(avoidspots[i].origin, reach->start, reach->end);
			// if moving towards the avoid spot
			if (squareddist < squaredradius && VectorDistanceSquared(avoidspots[i].origin, reach->start) > squareddist) {
				type = avoidspots[i].type;
			}
		} else {
			VectorDistanceSquared(avoidspots[i].origin, reach->end);
			// if the reachability leads closer to the avoid spot
			if (squareddist < squaredradius && VectorDistanceSquared(avoidspots[i].origin, reach->start) > squareddist) {
				type = avoidspots[i].type;
			}
		}

		if (type == AVOID_ALWAYS) {
			return type;
		}
	}

	return type;
}

/*
=======================================================================================================================================
BotAddAvoidSpot
=======================================================================================================================================
*/
void BotAddAvoidSpot(int movestate, vec3_t origin, float radius, int type) {
	bot_movestate_t *ms;

	ms = BotMoveStateFromHandle(movestate);

	if (!ms) {
		return;
	}

	if (type == AVOID_CLEAR) {
		ms->numavoidspots = 0;
		return;
	}

	if (ms->numavoidspots >= MAX_AVOIDSPOTS) {
		return;
	}

	VectorCopy(origin, ms->avoidspots[ms->numavoidspots].origin);

	ms->avoidspots[ms->numavoidspots].radius = radius;
	ms->avoidspots[ms->numavoidspots].type = type;
	ms->numavoidspots++;
}

/*
=======================================================================================================================================
BotGetReachabilityToGoal
=======================================================================================================================================
*/
int BotGetReachabilityToGoal(vec3_t origin, int areanum, int lastgoalareanum, int lastareanum, int *avoidreach, float *avoidreachtimes, int *avoidreachtries, bot_goal_t *goal, int travelflags, struct bot_avoidspot_s *avoidspots, int numavoidspots, int *flags) {
	int i, t, besttime, bestreachnum, reachnum;
	aas_reachability_t reach;

	// if not in a valid area
	if (!areanum) {
		return 0;
	}

	if (AAS_AreaDoNotEnter(areanum) || AAS_AreaDoNotEnter(goal->areanum)) {
		travelflags |= TFL_DONOTENTER;
	}
	// use the routing to find the next area to go to
	besttime = 0;
	bestreachnum = 0;

	for (reachnum = AAS_NextAreaReachability(areanum, 0); reachnum; reachnum = AAS_NextAreaReachability(areanum, reachnum)) {
#ifdef AVOIDREACH
		// check if it isn't a reachability to avoid
		for (i = 0; i < MAX_AVOIDREACH; i++) {
			if (avoidreach[i] == reachnum && avoidreachtimes[i] >= AAS_Time()) {
				break;
			}
		}

		if (i != MAX_AVOIDREACH && avoidreachtries[i] > AVOIDREACH_TRIES) {
#ifndef BASEGAME // Tobias DEBUG
			botimport.Print(PRT_MESSAGE, "avoiding reachability %d\n", avoidreach[i]);
#endif // Tobias END
			continue;
		}
#endif // AVOIDREACH
		// get the reachability from the number
		AAS_ReachabilityFromNum(reachnum, &reach);
		// NOTE: do not go back to the previous area if the goal didn't change
		// NOTE: is this actually avoidance of local routing minima between two areas???
		if (lastgoalareanum == goal->areanum && reach.areanum == lastareanum) {
			continue;
		}
		//if (AAS_AreaContentsTravelFlags(reach.areanum) & ~travelflags) {
		//	continue;
		//}
		// if the travel isn't valid
		if (!BotValidTravel(origin, &reach, travelflags)) {
			continue;
		}
		// get the travel time
		t = AAS_AreaTravelTimeToGoalArea(reach.areanum, reach.end, goal->areanum, travelflags);
		// if the goal area isn't reachable from the reachable area
		if (!t) {
			continue;
		}
		// if the bot should not use this reachability to avoid bad spots
		if (BotAvoidSpots(origin, &reach, avoidspots, numavoidspots)) {
			if (flags) {
				*flags |= MOVERESULT_BLOCKEDBYAVOIDSPOT;
			}

			continue;
		}
		// add the travel time towards the area
		t += reach.traveltime; //+ AAS_AreaTravelTime(areanum, origin, reach.start);
		// if the travel time is better than the ones already found
		if (!besttime || t < besttime) {
			besttime = t;
			bestreachnum = reachnum;
		}
	}

	return bestreachnum;
}

/*
=======================================================================================================================================
BotAddToTarget
=======================================================================================================================================
*/
int BotAddToTarget(vec3_t start, vec3_t end, float maxdist, float *dist, vec3_t target) {
	vec3_t dir;
	float curdist;

	VectorSubtract(end, start, dir);

	curdist = VectorNormalize(dir);

	if (*dist + curdist < maxdist) {
		VectorCopy(end, target);
		*dist += curdist;
		return qfalse;
	} else {
		VectorMA(start, maxdist - *dist, dir, target);
		*dist = maxdist;
		return qtrue;
	}
}

/*
=======================================================================================================================================
BotMovementViewTarget
=======================================================================================================================================
*/
int BotMovementViewTarget(int movestate, bot_goal_t *goal, int travelflags, float lookahead, vec3_t target) {
	aas_reachability_t reach;
	int reachnum, lastareanum;
	bot_movestate_t *ms;
	vec3_t end;
	float dist;

	ms = BotMoveStateFromHandle(movestate);

	if (!ms) {
		return qfalse;
	}
	// if the bot has no goal or no last reachability
	if (!ms->lastreachnum || !goal) {
		return qfalse;
	}

	reachnum = ms->lastreachnum;

	VectorCopy(ms->origin, end);

	lastareanum = ms->lastareanum;
	dist = 0;

	while (reachnum && dist < lookahead) {
		AAS_ReachabilityFromNum(reachnum, &reach);

		if (BotAddToTarget(end, reach.start, lookahead, &dist, target)) {
			return qtrue;
		}
		// never look beyond teleporters
		if ((reach.traveltype & TRAVELTYPE_MASK) == TRAVEL_TELEPORT) {
			return qtrue;
		}
		// never look beyond the weapon jump point
		if ((reach.traveltype & TRAVELTYPE_MASK) == TRAVEL_ROCKETJUMP) {
			return qtrue;
		}

		if ((reach.traveltype & TRAVELTYPE_MASK) == TRAVEL_BFGJUMP) {
			return qtrue;
		}
		// don't add jump pad distances
		if ((reach.traveltype & TRAVELTYPE_MASK) != TRAVEL_JUMPPAD && (reach.traveltype & TRAVELTYPE_MASK) != TRAVEL_ELEVATOR && (reach.traveltype & TRAVELTYPE_MASK) != TRAVEL_FUNCBOB) {
			if (BotAddToTarget(reach.start, reach.end, lookahead, &dist, target)) {
				return qtrue;
			}
		}

		reachnum = BotGetReachabilityToGoal(reach.end, reach.areanum, ms->lastgoalareanum, lastareanum, ms->avoidreach, ms->avoidreachtimes, ms->avoidreachtries, goal, travelflags, NULL, 0, NULL);

		VectorCopy(reach.end, end);

		lastareanum = reach.areanum;

		if (lastareanum == goal->areanum) {
			BotAddToTarget(reach.end, goal->origin, lookahead, &dist, target);
			return qtrue;
		}
	}

	return qfalse;
}

/*
=======================================================================================================================================
BotVisible
=======================================================================================================================================
*/
int BotVisible(int ent, vec3_t eye, vec3_t target) {
	bsp_trace_t trace;

	trace = AAS_Trace(eye, NULL, NULL, target, ent, CONTENTS_SOLID|CONTENTS_PLAYERCLIP);

	if (trace.fraction >= 1) {
		return qtrue;
	}

	return qfalse;
}

/*
=======================================================================================================================================
BotPredictVisiblePosition
=======================================================================================================================================
*/
int BotPredictVisiblePosition(vec3_t origin, int areanum, bot_goal_t *goal, int travelflags, vec3_t target) {
	aas_reachability_t reach;
	int reachnum, lastgoalareanum, lastareanum, i;
	int avoidreach[MAX_AVOIDREACH];
	float avoidreachtimes[MAX_AVOIDREACH];
	int avoidreachtries[MAX_AVOIDREACH];
	vec3_t end;

	// if the bot has no goal or no last reachability
	if (!goal) {
		return qfalse;
	}
	// if the areanum is not valid
	if (!areanum) {
		return qfalse;
	}
	// if the goal areanum is not valid
	if (!goal->areanum) {
		return qfalse;
	}

	Com_Memset(avoidreach, 0, MAX_AVOIDREACH * sizeof(int));

	lastgoalareanum = goal->areanum;
	lastareanum = areanum;

	VectorCopy(origin, end);
	// only do 20 hops
	for (i = 0; i < 20 && (areanum != goal->areanum); i++) {
		reachnum = BotGetReachabilityToGoal(end, areanum, lastgoalareanum, lastareanum, avoidreach, avoidreachtimes, avoidreachtries, goal, travelflags, NULL, 0, NULL);

		if (!reachnum) {
			return qfalse;
		}

		AAS_ReachabilityFromNum(reachnum, &reach);

		if (BotVisible(goal->entitynum, goal->origin, reach.start)) {
			VectorCopy(reach.start, target);
			return qtrue;
		}

		if (BotVisible(goal->entitynum, goal->origin, reach.end)) {
			VectorCopy(reach.end, target);
			return qtrue;
		}

		if (reach.areanum == goal->areanum) {
			VectorCopy(reach.end, target);
			return qtrue;
		}

		lastareanum = areanum;
		areanum = reach.areanum;

		VectorCopy(reach.end, end);
	}

	return qfalse;
}

/*
=======================================================================================================================================
MoverBottomCenter
=======================================================================================================================================
*/
void MoverBottomCenter(aas_reachability_t *reach, vec3_t bottomcenter) {
	int modelnum;
	vec3_t mins, maxs, origin, mids;
	vec3_t angles = {0, 0, 0};

	modelnum = reach->facenum & 0x0000FFFF;
	// get some bsp model info
	AAS_BSPModelMinsMaxsOrigin(modelnum, angles, mins, maxs, origin);

	if (!AAS_OriginOfMoverWithModelNum(modelnum, origin)) {
		botimport.Print(PRT_MESSAGE, "no entity with model %d\n", modelnum);
	}
	// get a point just above the plat in the bottom position
	VectorAdd(mins, maxs, mids);
	VectorMA(origin, 0.5, mids, bottomcenter);

	bottomcenter[2] = reach->start[2];
}

/*
=======================================================================================================================================
BotGapDistance
=======================================================================================================================================
*/
float BotGapDistance(vec3_t origin, vec3_t hordir, int entnum) {
	int dist;
	float startz;
	vec3_t start, end;
	aas_trace_t trace;

	// do gap checking
	//startz = origin[2];
	// this enables walking down stairs more fluidly
	{
		VectorCopy(origin, start);
		VectorCopy(origin, end);
		end[2] -= 60;
		trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, entnum);

		if (trace.fraction >= 1) {
			return 1;
		}

		startz = trace.endpos[2] + 1;
	}

	for (dist = 8; dist <= 100; dist += 8) {
		VectorMA(origin, dist, hordir, start);

		start[2] = startz + 24;

		VectorCopy(start, end);

		end[2] -= 48 + sv_maxbarrier->value;

		trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, entnum);
		// if solid is found the bot can't walk any further and fall into a gap
		if (!trace.startsolid) {
			// if it is a gap
			if (trace.endpos[2] < startz - sv_maxstep->value - 8) {
				VectorCopy(trace.endpos, end);

				end[2] -= 20;

				if (AAS_PointContents(end) & CONTENTS_WATER) {
					break;
				}
				// if a gap is found slow down
				//botimport.Print(PRT_MESSAGE, S_COLOR_YELLOW "BotGapDistance: found a gap at %i.\n", dist);
				return dist;
			}

			startz = trace.endpos[2];
		}
	}

	return 0;
}

/*
=======================================================================================================================================
BotCheckBarrierJump
=======================================================================================================================================
*/
int BotCheckBarrierJump(bot_movestate_t *ms, vec3_t dir, float speed) {
	vec3_t start, hordir, end;
	aas_trace_t trace;

	VectorCopy(ms->origin, end);

	end[2] += sv_maxbarrier->value;
	// trace right up
	trace = AAS_TraceClientBBox(ms->origin, end, PRESENCE_NORMAL, ms->entitynum);
	// this shouldn't happen... but we check anyway
	if (trace.startsolid) {
		return qfalse;
	}
	// if very low ceiling it isn't possible to jump up to a barrier
	if (trace.endpos[2] - ms->origin[2] < sv_maxstep->value) {
		return qfalse;
	}

	hordir[0] = dir[0];
	hordir[1] = dir[1];
	hordir[2] = 0;

	VectorNormalize(hordir);
	VectorMA(ms->origin, ms->thinktime * speed * 0.5, hordir, end);
	VectorCopy(trace.endpos, start);

	end[2] = trace.endpos[2];
	// trace from previous trace end pos horizontally in the move direction
	trace = AAS_TraceClientBBox(start, end, PRESENCE_NORMAL, ms->entitynum);
	// again this shouldn't happen
	if (trace.startsolid) {
		return qfalse;
	}

	VectorCopy(trace.endpos, start);
	VectorCopy(trace.endpos, end);

	end[2] = ms->origin[2];
	// trace down from the previous trace end pos
	trace = AAS_TraceClientBBox(start, end, PRESENCE_NORMAL, ms->entitynum);
	// if solid
	if (trace.startsolid) {
		return qfalse;
	}
	// if no obstacle at all
	if (trace.fraction >= 1.0) {
		return qfalse;
	}
	// if less than the maximum step height
	if (trace.endpos[2] - ms->origin[2] < sv_maxstep->value) {
		return qfalse;
	}
	// elementary actions
	EA_Jump(ms->client);
	EA_Move(ms->client, hordir, speed);

	ms->moveflags |= MFL_BARRIERJUMP;
	// there is a barrier
	return qtrue;
}

/*
=======================================================================================================================================
BotSwimInDirection
=======================================================================================================================================
*/
int BotSwimInDirection(bot_movestate_t *ms, vec3_t dir, float speed, int type) {
	vec3_t normdir;

	VectorCopy(dir, normdir);
	VectorNormalize(normdir);
	EA_Move(ms->client, normdir, speed);
	return qtrue;
}

/*
=======================================================================================================================================
BotWalkInDirection
=======================================================================================================================================
*/
int BotWalkInDirection(bot_movestate_t *ms, vec3_t dir, float speed, int type) {
	vec3_t hordir, cmdmove, velocity, tmpdir, origin;
	int presencetype, maxframes, cmdframes, stopevent;
	aas_clientmove_t move;
	float dist;

	if (AAS_OnGround(ms->origin, ms->presencetype, ms->entitynum)) {
		ms->moveflags |= MFL_ONGROUND;
	}
	// if the bot is on the ground
	if (ms->moveflags & MFL_ONGROUND) {
		// if there is a barrier the bot can jump on
		if (BotCheckBarrierJump(ms, dir, speed)) {
			return qtrue;
		}
		// remove barrier jump flag
		ms->moveflags &= ~MFL_BARRIERJUMP;
		// get the presence type for the movement
		if ((type & MOVE_CROUCH) && !(type & MOVE_JUMP)) {
			presencetype = PRESENCE_CROUCH;
		} else {
			presencetype = PRESENCE_NORMAL;
		}
		// horizontal direction
		hordir[0] = dir[0];
		hordir[1] = dir[1];
		hordir[2] = 0;

		VectorNormalize(hordir);
		// if the bot is not supposed to jump
		if (!(type & MOVE_JUMP)) {
			// if there is a gap, try to jump over it
			if (BotGapDistance(ms->origin, hordir, ms->entitynum) > 0) {
				type |= MOVE_JUMP;
			}
		}
		// get command movement
		VectorScale(hordir, speed, cmdmove);
		VectorCopy(ms->velocity, velocity);

		if (type & MOVE_JUMP) {
			//botimport.Print(PRT_MESSAGE, "trying jump\n");
			cmdmove[2] = 400;
			maxframes = PREDICTIONTIME_JUMP / 0.1;
			cmdframes = 1;
			stopevent = SE_HITGROUND|SE_HITGROUNDDAMAGE|SE_ENTERWATER|SE_ENTERSLIME|SE_ENTERLAVA;
		} else {
			maxframes = 2;
			cmdframes = 2;
			stopevent = SE_HITGROUNDDAMAGE|SE_ENTERWATER|SE_ENTERSLIME|SE_ENTERLAVA;
		}

		//AAS_ClearShownDebugLines();

		VectorCopy(ms->origin, origin);

		origin[2] += 0.5;

		AAS_PredictClientMovement(&move, ms->entitynum, origin, presencetype, qtrue, velocity, cmdmove, cmdframes, maxframes, 0.1f, stopevent, 0, qfalse); // qtrue
		// if prediction time wasn't enough to fully predict the movement
		if (move.frames >= maxframes && (type & MOVE_JUMP)) {
			//botimport.Print(PRT_MESSAGE, "client %d: max prediction frames\n", ms->client);
			return qfalse;
		}
		// don't enter slime or lava and don't fall from too high
		if (move.stopevent & (SE_ENTERSLIME|SE_ENTERLAVA|SE_HITGROUNDDAMAGE)) {
			//botimport.Print(PRT_MESSAGE, "client %d: would be hurt ", ms->client);
			//if (move.stopevent & SE_ENTERSLIME) botimport.Print(PRT_MESSAGE, "slime\n");
			//if (move.stopevent & SE_ENTERLAVA) botimport.Print(PRT_MESSAGE, "lava\n");
			//if (move.stopevent & SE_HITGROUNDDAMAGE) botimport.Print(PRT_MESSAGE, "hitground\n");
			return qfalse;
		}
		// if ground was hit
		if (move.stopevent & SE_HITGROUND) {
			// check for nearby gap
			VectorNormalize2(move.velocity, tmpdir);

			dist = BotGapDistance(move.endpos, tmpdir, ms->entitynum);

			if (dist > 0) {
				return qfalse;
			}

			dist = BotGapDistance(move.endpos, hordir, ms->entitynum);

			if (dist > 0) {
				return qfalse;
			}
		}
		// get horizontal movement
		tmpdir[0] = move.endpos[0] - ms->origin[0];
		tmpdir[1] = move.endpos[1] - ms->origin[1];
		tmpdir[2] = 0;

		//AAS_DrawCross(move.endpos, 4, LINECOLOR_BLUE);
		// the bot is blocked by something
		if (VectorLength(tmpdir) < speed * ms->thinktime * 0.5) {
			return qfalse;
		}
		// perform the movement
		if (type & MOVE_JUMP) {
			EA_Jump(ms->client);
		}

		if (type & MOVE_CROUCH) {
			EA_Crouch(ms->client);
		}

		EA_Move(ms->client, hordir, speed);
		// movement was succesfull
		return qtrue;
	} else {
		if (ms->moveflags & MFL_BARRIERJUMP) {
			// if near the top or going down
			if (ms->velocity[2] < 50) {
				EA_Move(ms->client, dir, speed);
			}
		}
		// FIXME: do air control to avoid hazards
		return qtrue;
	}
}

/*
=======================================================================================================================================
BotMoveInDirection
=======================================================================================================================================
*/
int BotMoveInDirection(int movestate, vec3_t dir, float speed, int type) {
	bot_movestate_t *ms;

	ms = BotMoveStateFromHandle(movestate);

	if (!ms) {
		return qfalse;
	}
	// if swimming
	if (AAS_Swimming(ms->origin)) {
		return BotSwimInDirection(ms, dir, speed, type);
	} else {
		return BotWalkInDirection(ms, dir, speed, type);
	}
}

/*
=======================================================================================================================================
Intersection
=======================================================================================================================================
*/
int Intersection(vec2_t p1, vec2_t p2, vec2_t p3, vec2_t p4, vec2_t out) {
	float x1, dx1, dy1, x2, dx2, dy2, d;

	dx1 = p2[0] - p1[0];
	dy1 = p2[1] - p1[1];
	dx2 = p4[0] - p3[0];
	dy2 = p4[1] - p3[1];
	d = dy1 * dx2 - dx1 * dy2;

	if (d != 0) {
		x1 = p1[1] * dx1 - p1[0] * dy1;
		x2 = p3[1] * dx2 - p3[0] * dy2;
		out[0] = (int)((dx1 * x2 - dx2 * x1) / d);
		out[1] = (int)((dy1 * x2 - dy2 * x1) / d);
		return qtrue;
	} else {
		return qfalse;
	}
}

/*
=======================================================================================================================================
BotCheckBlocked
=======================================================================================================================================
*/
void BotCheckBlocked(bot_movestate_t *ms, vec3_t dir, int checkbottom, bot_moveresult_t *result) {
	vec3_t mins, maxs, end, up = {0, 0, 1};
	bsp_trace_t trace;

	// test for entities obstructing the bot's path
	AAS_PresenceTypeBoundingBox(ms->presencetype, mins, maxs);
	// if the bot can step on
	if (fabs(DotProduct(dir, up)) < 0.7) {
		mins[2] += sv_maxstep->value;
		maxs[2] -= 10; // a little lower to avoid low ceiling
	}

	VectorMA(ms->origin, 3, dir, end);
	trace = AAS_Trace(ms->origin, mins, maxs, end, ms->entitynum, CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_BODY);
	// if not started in solid and not hitting the world entity
	if (!trace.startsolid && (trace.entityNum != ENTITYNUM_WORLD && trace.entityNum != ENTITYNUM_NONE)) {
		result->blocked = qtrue;
		result->blockentity = trace.entityNum;
#ifndef BASEGAME // Tobias DEBUG
		botimport.Print(PRT_MESSAGE, "%d: BotCheckBlocked: I'm blocked\n", ms->client);
#endif // Tobias END
	// if not in an area with reachability
	} else if (checkbottom && !AAS_AreaReachability(ms->areanum)) {
		// check if the bot is standing on something
		AAS_PresenceTypeBoundingBox(ms->presencetype, mins, maxs);
		VectorMA(ms->origin, -4, up, end);
		trace = AAS_Trace(ms->origin, mins, maxs, end, ms->entitynum, CONTENTS_SOLID|CONTENTS_PLAYERCLIP);

		if (!trace.startsolid && (trace.entityNum != ENTITYNUM_WORLD && trace.entityNum != ENTITYNUM_NONE)) {
			result->blocked = qtrue;
			result->blockentity = trace.entityNum;
			result->flags |= MOVERESULT_ONTOPOF_OBSTACLE;
#ifndef BASEGAME // Tobias DEBUG
			botimport.Print(PRT_MESSAGE, "%d: BotCheckBlocked: I'm blocked\n", ms->client);
#endif // Tobias END
		}
	}
}

/*
=======================================================================================================================================
BotTravel_Walk
=======================================================================================================================================
*/
bot_moveresult_t BotTravel_Walk(bot_movestate_t *ms, aas_reachability_t *reach) {
	float dist, speed;
	vec3_t hordir;
	bot_moveresult_t_cleared(result);

	// first walk straight to the reachability start
	hordir[0] = reach->start[0] - ms->origin[0];
	hordir[1] = reach->start[1] - ms->origin[1];
	hordir[2] = 0;
	dist = VectorNormalize(hordir);

	BotCheckBlocked(ms, hordir, qtrue, &result);

	if (dist < 10) {
		// walk straight to the reachability end
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;
		dist = VectorNormalize(hordir);
	}
	// if going towards a crouch area
	if (!(AAS_AreaPresenceType(reach->areanum) & PRESENCE_NORMAL)) {
		// if pretty close to the reachable area
		if (dist < 20) {
			EA_Crouch(ms->client);
		}
	}

	dist = BotGapDistance(ms->origin, hordir, ms->entitynum); // Tobias NOTE: in a perfect world we would not need this! Try to get rid of gap checking here(it works fine, but it looks ugly)

	if (ms->moveflags & MFL_WALK) {
		if (dist > 0) {
			speed = 200 - (180 - dist);
		} else {
			speed = 200;
		}

		EA_Walk(ms->client);
	} else {
		if (dist > 0) {
			speed = 400 - (360 - 2 * dist);
		} else {
			speed = 400;
		}
	}
	// elementary action move in direction
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);

	return result;
}

/*
=======================================================================================================================================
BotFinishTravel_Walk
=======================================================================================================================================
*/
bot_moveresult_t BotFinishTravel_Walk(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t hordir;
	float dist, speed;
	bot_moveresult_t_cleared(result);

	// if not on the ground and changed areas... don't walk back!!
	// (doesn't seem to help)
	/*
	ms->areanum = BotFuzzyPointReachabilityArea(ms->origin);

	if (ms->areanum == reach->areanum) {
#ifndef BASEGAME // Tobias DEBUG
		botimport.Print(PRT_MESSAGE, "BotFinishTravel_Walk: already in reach area\n");
#endif // Tobias END
		return result;
	}
	*/
	// go straight to the reachability end
	hordir[0] = reach->end[0] - ms->origin[0];
	hordir[1] = reach->end[1] - ms->origin[1];
	hordir[2] = 0;
	dist = VectorNormalize(hordir);

	if (dist > 100) {
		dist = 100;
	}

	speed = 400 - (400 - 3 * dist);
	// elementary action move in direction
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);

	return result;
}

/*
=======================================================================================================================================
BotTravel_Crouch
=======================================================================================================================================
*/
bot_moveresult_t BotTravel_Crouch(bot_movestate_t *ms, aas_reachability_t *reach) {
	float speed;
	vec3_t hordir;
	bot_moveresult_t_cleared(result);

	speed = 400;
	// walk straight to reachability end
	hordir[0] = reach->end[0] - ms->origin[0];
	hordir[1] = reach->end[1] - ms->origin[1];
	hordir[2] = 0;

	VectorNormalize(hordir);
	BotCheckBlocked(ms, hordir, qtrue, &result);
	// elementary actions
	EA_Crouch(ms->client);
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);

	return result;
}

/*
=======================================================================================================================================
BotTravel_BarrierJump
=======================================================================================================================================
*/
bot_moveresult_t BotTravel_BarrierJump(bot_movestate_t *ms, aas_reachability_t *reach) {
	float dist, speed;
	vec3_t hordir;
	bot_moveresult_t_cleared(result);

	// walk straight to reachability start
	hordir[0] = reach->start[0] - ms->origin[0];
	hordir[1] = reach->start[1] - ms->origin[1];
	hordir[2] = 0;
	dist = VectorNormalize(hordir);

	BotCheckBlocked(ms, hordir, qtrue, &result);
	// if pretty close to the barrier
	if (dist < 9) {
		EA_Jump(ms->client);
	} else {
		if (dist > 60) {
			dist = 60;
		}

		speed = 360 - (360 - 6 * dist);

		EA_Move(ms->client, hordir, speed);
	}

	VectorCopy(hordir, result.movedir);

	return result;
}

/*
=======================================================================================================================================
BotFinishTravel_BarrierJump
=======================================================================================================================================
*/
bot_moveresult_t BotFinishTravel_BarrierJump(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t hordir;
	bot_moveresult_t_cleared(result);

	// if near the top or going down
	if (ms->velocity[2] < 250) {
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;

		BotCheckBlocked(ms, hordir, qtrue, &result);
		EA_Move(ms->client, hordir, 400);
		VectorCopy(hordir, result.movedir);
	}

	return result;
}

/*
=======================================================================================================================================
BotTravel_Swim
=======================================================================================================================================
*/
bot_moveresult_t BotTravel_Swim(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t dir;
	bot_moveresult_t_cleared(result);

	// swim straight to reachability end
	VectorSubtract(reach->start, ms->origin, dir);
	VectorNormalize(dir);
	BotCheckBlocked(ms, dir, qtrue, &result);
	// elementary actions
	EA_Move(ms->client, dir, 400);
	// set the ideal view angles
	VectorToAngles(dir, result.ideal_viewangles);

	result.flags |= MOVERESULT_SWIMVIEW;

	VectorCopy(dir, result.movedir);

	return result;
}

/*
=======================================================================================================================================
BotTravel_WaterJump
=======================================================================================================================================
*/
bot_moveresult_t BotTravel_WaterJump(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t dir, hordir;
	float dist;
	bot_moveresult_t_cleared(result);

	// swim straight to reachability end
	VectorSubtract(reach->end, ms->origin, dir);
	VectorCopy(dir, hordir);

	hordir[2] = 0;
	dir[2] += 15 + crandom() * 40;
	//botimport.Print(PRT_MESSAGE, "BotTravel_WaterJump: dir[2] = %f\n", dir[2]);
	VectorNormalize(dir);

	dist = VectorNormalize(hordir);
	// elementary actions
	//EA_Move(ms->client, dir, 400);
	EA_MoveForward(ms->client);
	// move up if close to the actual out of water jump spot
	if (dist < 40) {
		EA_MoveUp(ms->client);
	}
	// set the ideal view angles
	VectorToAngles(dir, result.ideal_viewangles);

	result.flags |= MOVERESULT_MOVEMENTVIEW;

	VectorCopy(dir, result.movedir);

	return result;
}

/*
=======================================================================================================================================
BotFinishTravel_WaterJump
=======================================================================================================================================
*/
bot_moveresult_t BotFinishTravel_WaterJump(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t dir, pnt;
	bot_moveresult_t_cleared(result);

	//botimport.Print(PRT_MESSAGE, "BotFinishTravel_WaterJump\n");
	// if waterjumping there's nothing to do
	if (ms->moveflags & MFL_WATERJUMP) {
		return result;
	}
	// if not touching any water anymore don't do anything, otherwise the bot sometimes keeps jumping?
	VectorCopy(ms->origin, pnt);

	pnt[2] -= 32; // extra for q2dm4 near red armor/mega health

	if (!(AAS_PointContents(pnt) & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER))) {
		return result;
	}
	// swim straight to reachability end
	VectorSubtract(reach->end, ms->origin, dir);
	dir[0] += crandom() * 10;
	dir[1] += crandom() * 10;
	dir[2] += 70 + crandom() * 10;
	// elementary actions
	EA_Move(ms->client, dir, 400);
	// set the ideal view angles
	VectorToAngles(dir, result.ideal_viewangles);

	result.flags |= MOVERESULT_MOVEMENTVIEW;

	VectorCopy(dir, result.movedir);

	return result;
}

/*
=======================================================================================================================================
BotTravel_WalkOffLedge
=======================================================================================================================================
*/
bot_moveresult_t BotTravel_WalkOffLedge(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t hordir, dir;
	float dist, speed, reachhordist;
	bot_moveresult_t_cleared(result);

	// check if the bot is blocked by anything
	VectorSubtract(reach->start, ms->origin, dir);
	VectorNormalize(dir);
	BotCheckBlocked(ms, dir, qtrue, &result);
	// if the reachability start and end are practically above each other
	VectorSubtract(reach->end, reach->start, dir);

	dir[2] = 0;
	reachhordist = VectorLength(dir);
	// walk straight to the reachability start
	hordir[0] = reach->start[0] - ms->origin[0];
	hordir[1] = reach->start[1] - ms->origin[1];
	hordir[2] = 0;
	dist = VectorNormalize(hordir);
	// if pretty close to the start focus on the reachability end
	if (dist < 48) {
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;

		VectorNormalize(hordir);

		if (reachhordist < 20) {
			speed = 100;
		} else if (!AAS_HorizontalVelocityForJump(0, reach->start, reach->end, &speed)) {
			speed = 400;
		}
	} else {
		if (reachhordist < 20) {
			if (dist > 64) {
				dist = 64;
			}

			speed = 400 - (256 - 4 * dist);
		} else {
			speed = 400;
		}
	}

	BotCheckBlocked(ms, hordir, qtrue, &result);
	// elementary action move in direction
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);

	return result;
}

/*
=======================================================================================================================================
BotAirControl
=======================================================================================================================================
*/
int BotAirControl(vec3_t origin, vec3_t velocity, vec3_t goal, vec3_t dir, float *speed) {
	vec3_t org, vel;
	float dist;
	int i;

	VectorCopy(origin, org);
	VectorScale(velocity, 0.1, vel);

	for (i = 0; i < 50; i++) {
		vel[2] -= sv_gravity->value * 0.01;
		// if going down and next position would be below the goal
		if (vel[2] < 0 && org[2] + vel[2] < goal[2]) {
			VectorScale(vel, (goal[2] - org[2]) / vel[2], vel);
			VectorAdd(org, vel, org);
			VectorSubtract(goal, org, dir);

			dist = VectorNormalize(dir);

			if (dist > 32) {
				dist = 32;
			}

			*speed = 400 - (400 - 13 * dist);
			return qtrue;
		} else {
			VectorAdd(org, vel, org);
		}
	}

	VectorSet(dir, 0, 0, 0);

	*speed = 400;
	return qfalse;
}

/*
=======================================================================================================================================
BotFinishTravel_WalkOffLedge
=======================================================================================================================================
*/
bot_moveresult_t BotFinishTravel_WalkOffLedge(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t dir, hordir, end, v;
	float dist, speed;
	bot_moveresult_t_cleared(result);

	VectorSubtract(reach->end, ms->origin, dir);
	BotCheckBlocked(ms, dir, qtrue, &result);
	VectorSubtract(reach->end, ms->origin, v);

	v[2] = 0;
	dist = VectorNormalize(v);

	if (dist > 16) {
		VectorMA(reach->end, 16, v, end);
	} else {
		VectorCopy(reach->end, end);
	}

	if (!BotAirControl(ms->origin, ms->velocity, end, hordir, &speed)) {
		// go straight to the reachability end
		VectorCopy(dir, hordir);
		hordir[2] = 0;
		speed = 400;
	}
	// elementary action move in direction
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);

	return result;
}

/*
=======================================================================================================================================
BotTravel_Jump
=======================================================================================================================================
*/
/*
bot_moveresult_t BotTravel_Jump(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t hordir;
	float dist, gapdist, speed, horspeed, sv_jumpvel;
	bot_moveresult_t_cleared(result);

	sv_jumpvel = botlibglobals.sv_jumpvel->value;
	// walk straight to the reachability start
	hordir[0] = reach->start[0] - ms->origin[0];
	hordir[1] = reach->start[1] - ms->origin[1];
	hordir[2] = 0;
	dist = VectorNormalize(hordir);
	speed = 350;
	gapdist = BotGapDistance(ms, hordir, ms->entitynum);
	// if pretty close to the start focus on the reachability end
	if (dist < 50 || (gapdist && gapdist < 50)) {
		// NOTE: using max speed (400) works best
		//if (AAS_HorizontalVelocityForJump(sv_jumpvel, ms->origin, reach->end, &horspeed)) {
		//	speed = horspeed * 400 / botlibglobals.sv_maxwalkvelocity->value;
		//}

		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		VectorNormalize(hordir);
		// elementary action jump
		EA_Jump(ms->client);

		ms->jumpreach = ms->lastreachnum;
		speed = 600;
	} else {
		if (AAS_HorizontalVelocityForJump(sv_jumpvel, reach->start, reach->end, &horspeed)) {
			speed = horspeed * 400 / botlibglobals.sv_maxwalkvelocity->value;
		}
	}
	// elementary action
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);

	return result;
}
*/
/*
bot_moveresult_t BotTravel_Jump(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t hordir, dir1, dir2, mins, maxs, start, end;
	int gapdist;
	float dist1, dist2, speed;
	bot_moveresult_t_cleared(result);
	bsp_trace_t trace;

	hordir[0] = reach->start[0] - reach->end[0];
	hordir[1] = reach->start[1] - reach->end[1];
	hordir[2] = 0;

	VectorNormalize(hordir);
	VectorCopy(reach->start, start);
	start[2] += 1;
	// minus back the bouding box size plus 16
	VectorMA(reach->start, 80, hordir, end);

	AAS_PresenceTypeBoundingBox(PRESENCE_NORMAL, mins, maxs);
	// check for solids
	trace = AAS_Trace(start, mins, maxs, end, ms->entitynum, MASK_PLAYERSOLID);

	if (trace.startsolid) {
		VectorCopy(start, trace.endpos);
	}
	// check for a gap
	for (gapdist = 0; gapdist < 80; gapdist += 10) {
		VectorMA(start, gapdist + 10, hordir, end);
		end[2] += 1;

		if (AAS_PointAreaNum(end) != ms->reachareanum) {
			break;
		}
	}

	if (gapdist < 80) {
		VectorMA(reach->start, gapdist, hordir, trace.endpos);
	}

//	dist1 = BotGapDistance(start, hordir, ms->entitynum);

//	if (dist1 && dist1 <= trace.fraction * 80) {
//		VectorMA(reach->start, dist1 - 20, hordir, trace.endpos);
//	}

	VectorSubtract(ms->origin, reach->start, dir1);
	dir1[2] = 0;
	dist1 = VectorNormalize(dir1);
	VectorSubtract(ms->origin, trace.endpos, dir2);
	dir2[2] = 0;
	dist2 = VectorNormalize(dir2);
	// if just before the reachability start
	if (DotProduct(dir1, dir2) < -0.8 || dist2 < 5) {
		//botimport.Print(PRT_MESSAGE, "between jump start and run to point\n");
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;

		VectorNormalize(hordir);
		// elementary action jump
		if (dist1 < 24) {
			EA_Jump(ms->client);
		} else if (dist1 < 32) {
			EA_DelayedJump(ms->client);
		}

		EA_Move(ms->client, hordir, 600);

		ms->jumpreach = ms->lastreachnum;
	} else {
		//botimport.Print(PRT_MESSAGE, "going towards run to point\n");
		hordir[0] = trace.endpos[0] - ms->origin[0];
		hordir[1] = trace.endpos[1] - ms->origin[1];
		hordir[2] = 0;
		VectorNormalize(hordir);

		if (dist2 > 80) {
			dist2 = 80;
		}

		speed = 400 - (400 - 5 * dist2);

		EA_Move(ms->client, hordir, speed);
	}

	VectorCopy(hordir, result.movedir);

	return result;
}
*/
/*
=======================================================================================================================================
BotTravel_Jump
=======================================================================================================================================
*/
bot_moveresult_t BotTravel_Jump(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t hordir, dir1, dir2, start, end, runstart;
	//vec3_t runstart, dir1, dir2, hordir;
	int gapdist;
	float dist1, dist2, speed;
	bot_moveresult_t_cleared(result);

	AAS_JumpReachRunStart(reach, runstart);

	hordir[0] = runstart[0] - reach->start[0];
	hordir[1] = runstart[1] - reach->start[1];
	hordir[2] = 0;

	VectorNormalize(hordir);
	VectorCopy(reach->start, start);

	start[2] += 1;

	VectorMA(reach->start, 80, hordir, runstart);
	// check for a gap
	for (gapdist = 0; gapdist < 80; gapdist += 10) {
		VectorMA(start, gapdist + 10, hordir, end);

		end[2] += 1;

		if (AAS_PointAreaNum(end) != ms->reachareanum) {
			break;
		}
	}

	if (gapdist < 80) {
		VectorMA(reach->start, gapdist, hordir, runstart);
	}

	VectorSubtract(ms->origin, reach->start, dir1);

	dir1[2] = 0;
	dist1 = VectorNormalize(dir1);

	VectorSubtract(ms->origin, runstart, dir2);

	dir2[2] = 0;
	dist2 = VectorNormalize(dir2);
	// if just before the reachability start
	if (DotProduct(dir1, dir2) < -0.8 || dist2 < 5) {
		//botimport.Print(PRT_MESSAGE, "between jump start and run start point\n");
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;

		VectorNormalize(hordir);
		// elementary action jump
		if (dist1 < 24) {
			EA_Jump(ms->client);
		} else if (dist1 < 32) {
			EA_DelayedJump(ms->client);
		}

		EA_Move(ms->client, hordir, 600);

		ms->jumpreach = ms->lastreachnum;
	} else {
		//botimport.Print(PRT_MESSAGE, "going towards run start point\n");
		hordir[0] = runstart[0] - ms->origin[0];
		hordir[1] = runstart[1] - ms->origin[1];
		hordir[2] = 0;

		VectorNormalize(hordir);

		if (dist2 > 80) {
			dist2 = 80;
		}

		speed = 400 - (400 - 5 * dist2);

		EA_Move(ms->client, hordir, speed);
	}

	VectorCopy(hordir, result.movedir);

	return result;
}

/*
=======================================================================================================================================
BotFinishTravel_Jump
=======================================================================================================================================
*/
bot_moveresult_t BotFinishTravel_Jump(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t hordir, hordir2;
	float speed, dist;
	bot_moveresult_t_cleared(result);

	// if not jumped yet
	if (!ms->jumpreach) {
		return result;
	}
	// go straight to the reachability end
	hordir[0] = reach->end[0] - ms->origin[0];
	hordir[1] = reach->end[1] - ms->origin[1];
	hordir[2] = 0;
	dist = VectorNormalize(hordir);

	hordir2[0] = reach->end[0] - reach->start[0];
	hordir2[1] = reach->end[1] - reach->start[1];
	hordir2[2] = 0;

	VectorNormalize(hordir2);

	if (DotProduct(hordir, hordir2) < -0.5 && dist < 24) {
		return result;
	}
	// always use max speed when traveling through the air
	speed = 800;
	// elementary action move in direction
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);

	return result;
}

/*
=======================================================================================================================================
BotTravel_Ladder
=======================================================================================================================================
*/
bot_moveresult_t BotTravel_Ladder(bot_movestate_t *ms, aas_reachability_t *reach) {
	//float dist, speed;
	vec3_t dir, viewdir; //hordir
	vec3_t origin = {0, 0, 0};
	//vec3_t up = {0, 0, 1};
	bot_moveresult_t_cleared(result);

	//if ((ms->moveflags & MFL_AGAINSTLADDER))
		// NOTE: not a good idea for ladders starting in water
		// || !(ms->moveflags & MFL_ONGROUND))
	{
		//botimport.Print(PRT_MESSAGE, "against ladder or not on ground\n");
		VectorSubtract(reach->end, ms->origin, dir);
		VectorNormalize(dir);
		// set the ideal view angles, facing the ladder up or down
		viewdir[0] = dir[0];
		viewdir[1] = dir[1];
		viewdir[2] = 3 * dir[2];

		VectorToAngles(viewdir, result.ideal_viewangles);
		// elementary action
		EA_Move(ms->client, origin, 0);
		EA_MoveForward(ms->client);
		// set movement view flag so the AI can see the view is focussed
		result.flags |= MOVERESULT_MOVEMENTVIEW;
	}
/*	else
	{
		//botimport.Print(PRT_MESSAGE, "moving towards ladder\n");
		VectorSubtract(reach->end, ms->origin, dir);
		// make sure the horizontal movement is large enough
		VectorCopy(dir, hordir);

		hordir[2] = 0;
		dist = VectorNormalize(hordir);
		dir[0] = hordir[0];
		dir[1] = hordir[1];

		if (dir[2] > 0) {
			dir[2] = 1;
		} else {
			dir[2] = -1;
		}

		if (dist > 50) {
			dist = 50;
		}

		speed = 400 - (200 - 4 * dist);

		EA_Move(ms->client, dir, speed);
	}
*/
	// save the movement direction
	VectorCopy(dir, result.movedir);

	return result;
}

/*
=======================================================================================================================================
BotTravel_Teleport
=======================================================================================================================================
*/
bot_moveresult_t BotTravel_Teleport(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t hordir;
	float dist;
	bot_moveresult_t_cleared(result);

	// if the bot is being teleported
	if (ms->moveflags & MFL_TELEPORTED) {
		return result;
	}
	// walk straight to center of the teleporter
	VectorSubtract(reach->start, ms->origin, hordir);

	if (!(ms->moveflags & MFL_SWIMMING)) {
		hordir[2] = 0;
	}

	dist = VectorNormalize(hordir);

	BotCheckBlocked(ms, hordir, qtrue, &result);

	if (dist < 30) {
		EA_Move(ms->client, hordir, 200);
	} else {
		EA_Move(ms->client, hordir, 400);
	}

	if (ms->moveflags & MFL_SWIMMING) {
		result.flags |= MOVERESULT_SWIMVIEW;
	}

	VectorCopy(hordir, result.movedir);
	return result;
}

/*
=======================================================================================================================================
BotTravel_Elevator
=======================================================================================================================================
*/
bot_moveresult_t BotTravel_Elevator(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t dir, dir1, dir2, hordir, bottomcenter;
	float dist, dist1, dist2, speed;
	bot_moveresult_t_cleared(result);

	// if standing on the plat
	if (BotOnMover(ms->origin, ms->entitynum, reach)) {
#ifndef BASEGAME // Tobias DEBUG
		botimport.Print(PRT_MESSAGE, "bot on elevator\n");
#endif // Tobias END
		// if vertically not too far from the end point
		if (fabsf(ms->origin[2] - reach->end[2]) < sv_maxbarrier->value) {
#ifndef BASEGAME // Tobias DEBUG
			botimport.Print(PRT_MESSAGE, "bot moving to end\n");
#endif // Tobias END
			// move to the end point
			VectorSubtract(reach->end, ms->origin, hordir);

			hordir[2] = 0;

			VectorNormalize(hordir);

			if (!BotCheckBarrierJump(ms, hordir, 100)) {
				EA_Move(ms->client, hordir, 400);
			}

			VectorCopy(hordir, result.movedir);
		// if not really close to the center of the elevator
		} else {
			MoverBottomCenter(reach, bottomcenter);
			VectorSubtract(bottomcenter, ms->origin, hordir);

			hordir[2] = 0;
			dist = VectorNormalize(hordir);

			if (dist > 10) {
#ifndef BASEGAME // Tobias DEBUG
				botimport.Print(PRT_MESSAGE, "bot moving to center\n");
#endif // Tobias END
				// move to the center of the plat
				if (dist > 100) {
					dist = 100;
				}

				speed = 400 - (400 - 4 * dist);

				EA_Move(ms->client, hordir, speed);
				VectorCopy(hordir, result.movedir);
			}
		}
	} else {
#ifndef BASEGAME // Tobias DEBUG
		botimport.Print(PRT_MESSAGE, "bot not on elevator\n");
#endif // Tobias END
		// if very near the reachability end
		VectorSubtract(reach->end, ms->origin, dir);

		dist = VectorLength(dir);

		if (dist < 64) {
			if (dist > 60) {
				dist = 60;
			}

			speed = 360 - (360 - 6 * dist);

			if ((ms->moveflags & MFL_SWIMMING) || !BotCheckBarrierJump(ms, dir, 50)) {
				if (speed > 5) {
					EA_Move(ms->client, dir, speed);
				}
			}

			VectorCopy(dir, result.movedir);

			if (ms->moveflags & MFL_SWIMMING) {
				result.flags |= MOVERESULT_SWIMVIEW;
			}
			// stop using this reachability
			ms->reachability_time = 0;
			return result;
		}
		// get direction and distance to reachability start
		VectorSubtract(reach->start, ms->origin, dir1);

		if (!(ms->moveflags & MFL_SWIMMING)) {
			dir1[2] = 0;
		}

		dist1 = VectorNormalize(dir1);
		// if the elevator isn't down
		if (!MoverDown(reach)) {
#ifndef BASEGAME // Tobias DEBUG
			botimport.Print(PRT_MESSAGE, "elevator not down\n");
#endif // Tobias END
			dist = dist1;

			VectorCopy(dir1, dir);
			BotCheckBlocked(ms, dir, qfalse, &result);

			if (dist > 60) {
				dist = 60;
			}

			speed = 360 - (360 - 6 * dist);

			if (!(ms->moveflags & MFL_SWIMMING) && !BotCheckBarrierJump(ms, dir, 50)) {
				if (speed > 5) {
					EA_Move(ms->client, dir, speed);
				}
			}

			VectorCopy(dir, result.movedir);

			if (ms->moveflags & MFL_SWIMMING) {
				result.flags |= MOVERESULT_SWIMVIEW;
			}
			// this isn't a failure... just wait till the elevator comes down
			result.type = RESULTTYPE_ELEVATORUP;
			result.flags |= MOVERESULT_WAITING;
			return result;
		}
		// get direction and distance to elevator bottom center
		MoverBottomCenter(reach, bottomcenter);
		VectorSubtract(bottomcenter, ms->origin, dir2);

		if (!(ms->moveflags & MFL_SWIMMING)) {
			dir2[2] = 0;
		}

		dist2 = VectorNormalize(dir2);
		// if very close to the reachability start or closer to the elevator center or between reachability start and elevator center
		if (dist1 < 20 || dist2 < dist1 || DotProduct(dir1, dir2) < 0) {
#ifndef BASEGAME // Tobias DEBUG
			botimport.Print(PRT_MESSAGE, "bot moving to center\n");
#endif // Tobias END
			dist = dist2;
			VectorCopy(dir2, dir);
		} else { // closer to the reachability start
#ifndef BASEGAME // Tobias DEBUG
			botimport.Print(PRT_MESSAGE, "bot moving to start\n");
#endif // Tobias END
			dist = dist1;

			VectorCopy(dir1, dir);
		}

		BotCheckBlocked(ms, dir, qfalse, &result);

		if (dist > 60) {
			dist = 60;
		}

		speed = 400 - (400 - 6 * dist);

		if (!(ms->moveflags & MFL_SWIMMING) && !BotCheckBarrierJump(ms, dir, 50)) {
			EA_Move(ms->client, dir, speed);
		}

		VectorCopy(dir, result.movedir);

		if (ms->moveflags & MFL_SWIMMING) {
			result.flags |= MOVERESULT_SWIMVIEW;
		}
	}

	return result;
}

/*
=======================================================================================================================================
BotFinishTravel_Elevator
=======================================================================================================================================
*/
bot_moveresult_t BotFinishTravel_Elevator(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t bottomcenter, bottomdir, topdir;
	bot_moveresult_t_cleared(result);

	MoverBottomCenter(reach, bottomcenter);
	VectorSubtract(bottomcenter, ms->origin, bottomdir);
	VectorSubtract(reach->end, ms->origin, topdir);

	if (fabs(bottomdir[2]) < fabs(topdir[2])) {
		VectorNormalize(bottomdir);
		EA_Move(ms->client, bottomdir, 300);
	} else {
		VectorNormalize(topdir);
		EA_Move(ms->client, topdir, 300);
	}

	return result;
}

/*
=======================================================================================================================================
BotFuncBobStartEnd
=======================================================================================================================================
*/
void BotFuncBobStartEnd(aas_reachability_t *reach, vec3_t start, vec3_t end, vec3_t origin) {
	int spawnflags, modelnum;
	vec3_t mins, maxs, mid, angles = {0, 0, 0};
	int num0, num1;

	modelnum = reach->facenum & 0x0000FFFF;

	if (!AAS_OriginOfMoverWithModelNum(modelnum, origin)) {
		botimport.Print(PRT_MESSAGE, "BotFuncBobStartEnd: no entity with model %d\n", modelnum);
		VectorSet(start, 0, 0, 0);
		VectorSet(end, 0, 0, 0);
		return;
	}

	AAS_BSPModelMinsMaxsOrigin(modelnum, angles, mins, maxs, NULL);
	VectorAdd(mins, maxs, mid);
	VectorScale(mid, 0.5, mid);
	VectorCopy(mid, start);
	VectorCopy(mid, end);

	spawnflags = reach->facenum >> 16;
	num0 = reach->edgenum >> 16;

	if (num0 > 0x00007FFF) {
		num0 |= 0xFFFF0000;
	}

	num1 = reach->edgenum & 0x0000FFFF;

	if (num1 > 0x00007FFF) {
		num1 |= 0xFFFF0000;
	}

	if (spawnflags & 1) {
		start[0] = num0;
		end[0] = num1;

		origin[0] += mid[0];
		origin[1] = mid[1];
		origin[2] = mid[2];
	} else if (spawnflags & 2) {
		start[1] = num0;
		end[1] = num1;

		origin[0] = mid[0];
		origin[1] += mid[1];
		origin[2] = mid[2];
	} else {
		start[2] = num0;
		end[2] = num1;

		origin[0] = mid[0];
		origin[1] = mid[1];
		origin[2] += mid[2];
	}
}

/*
=======================================================================================================================================
BotTravel_FuncBobbing
=======================================================================================================================================
*/
bot_moveresult_t BotTravel_FuncBobbing(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t dir, dir1, dir2, hordir, bottomcenter, bob_start, bob_end, bob_origin;
	float dist, dist1, dist2, speed;
	bot_moveresult_t_cleared(result);

	BotFuncBobStartEnd(reach, bob_start, bob_end, bob_origin);
	// if standing ontop of the func_bobbing
	if (BotOnMover(ms->origin, ms->entitynum, reach)) {
#ifndef BASEGAME // Tobias DEBUG
		botimport.Print(PRT_MESSAGE, "bot on func_bobbing\n");
#endif // Tobias END
		// if near end point of reachability
		VectorSubtract(bob_origin, bob_end, dir);

		if (VectorLength(dir) < 24) {
#ifndef BASEGAME // Tobias DEBUG
			botimport.Print(PRT_MESSAGE, "bot moving to reachability end\n");
#endif // Tobias END
			// move to the end point
			VectorSubtract(reach->end, ms->origin, hordir);

			hordir[2] = 0;

			VectorNormalize(hordir);

			if (!BotCheckBarrierJump(ms, hordir, 100)) {
				EA_Move(ms->client, hordir, 400);
			}

			VectorCopy(hordir, result.movedir);
		// if not really close to the center of the func_bobbing
		} else {
			MoverBottomCenter(reach, bottomcenter);
			VectorSubtract(bottomcenter, ms->origin, hordir);

			hordir[2] = 0;
			dist = VectorNormalize(hordir);

			if (dist > 10) {
#ifndef BASEGAME // Tobias DEBUG
				botimport.Print(PRT_MESSAGE, "bot moving to func_bobbing center\n");
#endif // Tobias END
				// move to the center of the func_bobbing
				if (dist > 100) {
					dist = 100;
				}

				speed = 400 - (400 - 4 * dist);

				EA_Move(ms->client, hordir, speed);
				VectorCopy(hordir, result.movedir);
			}
		}
	} else {
#ifndef BASEGAME // Tobias DEBUG
		botimport.Print(PRT_MESSAGE, "bot not ontop of func_bobbing\n");
#endif // Tobias END
		// if very near the reachability end
		VectorSubtract(reach->end, ms->origin, dir);

		dist = VectorLength(dir);

		if (dist < 64) {
#ifndef BASEGAME // Tobias DEBUG
			botimport.Print(PRT_MESSAGE, "bot moving to end\n");
#endif // Tobias END
			if (dist > 60) {
				dist = 60;
			}

			speed = 360 - (360 - 6 * dist);
			// if swimming or no barrier jump
			if ((ms->moveflags & MFL_SWIMMING) || !BotCheckBarrierJump(ms, dir, 50)) {
				if (speed > 5) {
					EA_Move(ms->client, dir, speed);
				}
			}

			VectorCopy(dir, result.movedir);

			if (ms->moveflags & MFL_SWIMMING) {
				result.flags |= MOVERESULT_SWIMVIEW;
			}
			// stop using this reachability
			ms->reachability_time = 0;
			return result;
		}
		// get direction and distance to reachability start
		VectorSubtract(reach->start, ms->origin, dir1);

		if (!(ms->moveflags & MFL_SWIMMING)) {
			dir1[2] = 0;
		}

		dist1 = VectorNormalize(dir1);
		// if the func_bobbing is NOT in its start position
		VectorSubtract(bob_origin, bob_start, dir);

		if (VectorLength(dir) > 16) {
#ifndef BASEGAME // Tobias DEBUG
			botimport.Print(PRT_MESSAGE, "func_bobbing not at start\n");
#endif // Tobias END
			dist = dist1;

			VectorCopy(dir1, dir);
			BotCheckBlocked(ms, dir, qfalse, &result);

			if (dist > 60) {
				dist = 60;
			}

			speed = 360 - (360 - 6 * dist);

			if (!(ms->moveflags & MFL_SWIMMING) && !BotCheckBarrierJump(ms, dir, 50)) {
				if (speed > 5) {
					EA_Move(ms->client, dir, speed);
				}
			}

			VectorCopy(dir, result.movedir);

			if (ms->moveflags & MFL_SWIMMING) {
				result.flags |= MOVERESULT_SWIMVIEW;
			}
			// this isn't a failure... just wait till the func_bobbing arrives
			result.type = RESULTTYPE_WAITFORFUNCBOBBING;
			result.flags |= MOVERESULT_WAITING;
			return result;
		}
		// get direction and distance to func_bobbing bottom center
		MoverBottomCenter(reach, bottomcenter);
		VectorSubtract(bottomcenter, ms->origin, dir2);

		if (!(ms->moveflags & MFL_SWIMMING)) {
			dir2[2] = 0;
		}

		dist2 = VectorNormalize(dir2);
		// if very close to the reachability start or closer to the func_bobbing center or between reachability start and func_bobbing center
		if (dist1 < 20 || dist2 < dist1 || DotProduct(dir1, dir2) < 0) {
#ifndef BASEGAME // Tobias DEBUG
			botimport.Print(PRT_MESSAGE, "bot moving to func_bobbing center\n");
#endif // Tobias END
			dist = dist2;

			VectorCopy(dir2, dir);
		} else { // closer to the reachability start
#ifndef BASEGAME // Tobias DEBUG
			botimport.Print(PRT_MESSAGE, "bot moving to reachability start\n");
#endif // Tobias END
			dist = dist1;

			VectorCopy(dir1, dir);
		}

		BotCheckBlocked(ms, dir, qfalse, &result);

		if (dist > 60) {
			dist = 60;
		}

		speed = 400 - (400 - 6 * dist);

		if (!(ms->moveflags & MFL_SWIMMING) && !BotCheckBarrierJump(ms, dir, 50)) {
			EA_Move(ms->client, dir, speed);
		}

		VectorCopy(dir, result.movedir);

		if (ms->moveflags & MFL_SWIMMING) {
			result.flags |= MOVERESULT_SWIMVIEW;
		}
	}

	return result;
}

/*
=======================================================================================================================================
BotFinishTravel_FuncBobbing
=======================================================================================================================================
*/
bot_moveresult_t BotFinishTravel_FuncBobbing(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t bob_origin, bob_start, bob_end, dir, hordir, bottomcenter;
	bot_moveresult_t_cleared(result);
	float dist, speed;

	BotFuncBobStartEnd(reach, bob_start, bob_end, bob_origin);
	VectorSubtract(bob_origin, bob_end, dir);

	dist = VectorLength(dir);
	// if the func_bobbing is near the end
	if (dist < 16) {
		VectorSubtract(reach->end, ms->origin, hordir);

		if (!(ms->moveflags & MFL_SWIMMING)) {
			hordir[2] = 0;
		}

		dist = VectorNormalize(hordir);

		if (dist > 60) {
			dist = 60;
		}

		speed = 360 - (360 - 6 * dist);

		if (speed > 5) {
			EA_Move(ms->client, dir, speed);
		}

		VectorCopy(dir, result.movedir);

		if (ms->moveflags & MFL_SWIMMING) {
			result.flags |= MOVERESULT_SWIMVIEW;
		}
	} else {
		MoverBottomCenter(reach, bottomcenter);
		VectorSubtract(bottomcenter, ms->origin, hordir);

		if (!(ms->moveflags & MFL_SWIMMING)) {
			hordir[2] = 0;
		}

		dist = VectorNormalize(hordir);

		if (dist > 5) {
			// move to the center of the func_bobbing
			if (dist > 100) {
				dist = 100;
			}

			speed = 400 - (400 - 4 * dist);

			EA_Move(ms->client, hordir, speed);
			VectorCopy(hordir, result.movedir);
		}
	}

	return result;
}

/*
=======================================================================================================================================
BotTravel_RocketJump
=======================================================================================================================================
*/
bot_moveresult_t BotTravel_RocketJump(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t hordir;
	float dist, speed;
	bot_moveresult_t_cleared(result);

	//botimport.Print(PRT_MESSAGE, "BotTravel_RocketJump: bah\n");

	hordir[0] = reach->start[0] - ms->origin[0];
	hordir[1] = reach->start[1] - ms->origin[1];
	hordir[2] = 0;

	dist = VectorNormalize(hordir);
	// look in the movement direction
	VectorToAngles(hordir, result.ideal_viewangles);
	// look straight down
	result.ideal_viewangles[PITCH] = 90;

	if (dist < 5 && fabs(AngleDiff(result.ideal_viewangles[0], ms->viewangles[0])) < 5 && fabs(AngleDiff(result.ideal_viewangles[1], ms->viewangles[1])) < 5) {
		//botimport.Print(PRT_MESSAGE, "between jump start and run start point\n");
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;

		VectorNormalize(hordir);
		// elementary action jump
		EA_Jump(ms->client);
		EA_Attack(ms->client);
		EA_Move(ms->client, hordir, 800);

		ms->jumpreach = ms->lastreachnum;
	} else {
		if (dist > 80) {
			dist = 80;
		}

		speed = 400 - (400 - 5 * dist);

		EA_Move(ms->client, hordir, speed);
	}
	// look in the movement direction
	VectorToAngles(hordir, result.ideal_viewangles);
	// look straight down
	result.ideal_viewangles[PITCH] = 90;
	// set the view angles directly
	EA_View(ms->client, result.ideal_viewangles);
	// view is important for the movement
	result.flags |= MOVERESULT_MOVEMENTVIEWSET;
	// select the rocket launcher
	EA_SelectWeapon(ms->client, (int)weapindex_rocketlauncher->value);
	// weapon is used for movement
	result.weapon = (int)weapindex_rocketlauncher->value;
	result.flags |= MOVERESULT_MOVEMENTWEAPON;

	VectorCopy(hordir, result.movedir);
	return result;
}

/*
=======================================================================================================================================
BotTravel_BFGJump
=======================================================================================================================================
*/
bot_moveresult_t BotTravel_BFGJump(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t hordir;
	float dist, speed;
	bot_moveresult_t_cleared(result);

	//botimport.Print(PRT_MESSAGE, "BotTravel_BFGJump: bah\n");

	hordir[0] = reach->start[0] - ms->origin[0];
	hordir[1] = reach->start[1] - ms->origin[1];
	hordir[2] = 0;

	dist = VectorNormalize(hordir);
	// look in the movement direction
	VectorToAngles(hordir, result.ideal_viewangles);
	// look straight down
	result.ideal_viewangles[PITCH] = 90;

	if (dist < 5 && fabs(AngleDiff(result.ideal_viewangles[0], ms->viewangles[0])) < 5 && fabs(AngleDiff(result.ideal_viewangles[1], ms->viewangles[1])) < 5) {
		//botimport.Print(PRT_MESSAGE, "between jump start and run start point\n");
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;

		VectorNormalize(hordir);
		// elementary action jump
		EA_Jump(ms->client);
		EA_Attack(ms->client);
		EA_Move(ms->client, hordir, 800);

		ms->jumpreach = ms->lastreachnum;
	} else {
		if (dist > 80) {
			dist = 80;
		}

		speed = 400 - (400 - 5 * dist);

		EA_Move(ms->client, hordir, speed);
	}
	// look in the movement direction
	VectorToAngles(hordir, result.ideal_viewangles);
	// look straight down
	result.ideal_viewangles[PITCH] = 90;
	// set the view angles directly
	EA_View(ms->client, result.ideal_viewangles);
	// view is important for the movement
	result.flags |= MOVERESULT_MOVEMENTVIEWSET;
	// select the rocket launcher
	EA_SelectWeapon(ms->client, (int)weapindex_bfg10k->value);
	// weapon is used for movement
	result.weapon = (int)weapindex_bfg10k->value;
	result.flags |= MOVERESULT_MOVEMENTWEAPON;

	VectorCopy(hordir, result.movedir);
	return result;
}

/*
=======================================================================================================================================
BotFinishTravel_WeaponJump
=======================================================================================================================================
*/
bot_moveresult_t BotFinishTravel_WeaponJump(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t hordir;
	float speed;
	bot_moveresult_t_cleared(result);

	// if not jumped yet
	if (!ms->jumpreach) {
		return result;
	}
	/*
	// go straight to the reachability end
	hordir[0] = reach->end[0] - ms->origin[0];
	hordir[1] = reach->end[1] - ms->origin[1];
	hordir[2] = 0;
	VectorNormalize(hordir);
	// always use max speed when traveling through the air
	EA_Move(ms->client, hordir, 800);
	VectorCopy(hordir, result.movedir);
	*/
	if (!BotAirControl(ms->origin, ms->velocity, reach->end, hordir, &speed)) {
		// go straight to the reachability end
		VectorSubtract(reach->end, ms->origin, hordir);
		hordir[2] = 0;
		VectorNormalize(hordir);
		speed = 400;
	}
	// elementary action move in direction
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);

	return result;
}

/*
=======================================================================================================================================
BotTravel_JumpPad
=======================================================================================================================================
*/
bot_moveresult_t BotTravel_JumpPad(bot_movestate_t *ms, aas_reachability_t *reach) {
	vec3_t hordir;
	bot_moveresult_t_cleared(result);

	// first walk straight to the reachability start
	hordir[0] = reach->start[0] - ms->origin[0];
	hordir[1] = reach->start[1] - ms->origin[1];
	hordir[2] = 0;

	BotCheckBlocked(ms, hordir, qtrue, &result);
	// elementary action move in direction
	EA_Move(ms->client, hordir, 400);
	VectorCopy(hordir, result.movedir);

	return result;
}

/*
=======================================================================================================================================
BotFinishTravel_JumpPad
=======================================================================================================================================
*/
bot_moveresult_t BotFinishTravel_JumpPad(bot_movestate_t *ms, aas_reachability_t *reach) {
	float speed;
	vec3_t hordir;
	bot_moveresult_t_cleared(result);

	if (!BotAirControl(ms->origin, ms->velocity, reach->end, hordir, &speed)) {
		hordir[0] = reach->end[0] - ms->origin[0];
		hordir[1] = reach->end[1] - ms->origin[1];
		hordir[2] = 0;

		VectorNormalize(hordir);

		speed = 400;
	}

	BotCheckBlocked(ms, hordir, qtrue, &result);
	// elementary action move in direction
	EA_Move(ms->client, hordir, speed);
	VectorCopy(hordir, result.movedir);

	return result;
}

/*
=======================================================================================================================================
BotReachabilityTime

Time before the reachability times out.
=======================================================================================================================================
*/
int BotReachabilityTime(aas_reachability_t *reach) {

	switch (reach->traveltype & TRAVELTYPE_MASK) {
		case TRAVEL_WALK:
			return 5;
		case TRAVEL_CROUCH:
			return 5;
		case TRAVEL_BARRIERJUMP:
			return 5;
		case TRAVEL_LADDER:
			return 6;
		case TRAVEL_WALKOFFLEDGE:
			return 5;
		case TRAVEL_JUMP:
			return 5;
		case TRAVEL_SWIM:
			return 5;
		case TRAVEL_WATERJUMP:
			return 5;
		case TRAVEL_TELEPORT:
			return 5;
		case TRAVEL_ELEVATOR:
			return 10;
		case TRAVEL_ROCKETJUMP:
			return 6;
		case TRAVEL_BFGJUMP:
			return 6;
		case TRAVEL_JUMPPAD:
			return 10;
		case TRAVEL_FUNCBOB:
			return 10;
		default:
		{
			botimport.Print(PRT_ERROR, "travel type %d not implemented yet\n", reach->traveltype);
			return 8;
		}
	}
}

/*
=======================================================================================================================================
BotMoveInGoalArea
=======================================================================================================================================
*/
bot_moveresult_t BotMoveInGoalArea(bot_movestate_t *ms, bot_goal_t *goal) {
	bot_moveresult_t_cleared(result);
	vec3_t dir;
	float dist, speed;
#ifndef BASEGAME // Tobias DEBUG
	if (botDeveloper) {
		botimport.Print(PRT_MESSAGE, "%s: moving straight to goal\n", ClientName(ms->entitynum-1));
		AAS_ClearShownDebugLines();
		AAS_DebugLine(ms->origin, goal->origin, LINECOLOR_RED);
	}
#endif // Tobias END
	// walk straight to the goal origin
	dir[0] = goal->origin[0] - ms->origin[0];
	dir[1] = goal->origin[1] - ms->origin[1];

	if (ms->moveflags & MFL_SWIMMING) {
		dir[2] = goal->origin[2] - ms->origin[2];
		result.traveltype = TRAVEL_SWIM;
	} else {
		dir[2] = 0;
		result.traveltype = TRAVEL_WALK;
	}

	dist = VectorNormalize(dir);

	if (dist > 100) {
		dist = 100;
	}

	speed = 400 - (400 - 4 * dist);

	if (speed < 10) {
		speed = 0;
	}

	BotCheckBlocked(ms, dir, qtrue, &result);
	// elementary action move in direction
	EA_Move(ms->client, dir, speed);
	VectorCopy(dir, result.movedir);

	if (ms->moveflags & MFL_SWIMMING) {
		VectorToAngles(dir, result.ideal_viewangles);
		result.flags |= MOVERESULT_SWIMVIEW;
	}
	//if (!debugline) debugline = botimport.DebugLineCreate();
	//botimport.DebugLineShow(debugline, ms->origin, goal->origin, LINECOLOR_BLUE);

	ms->lastreachnum = 0;
	ms->lastareanum = 0;
	ms->lastgoalareanum = goal->areanum;

	VectorCopy(ms->origin, ms->lastorigin);

	return result;
}

/*
=======================================================================================================================================
BotMoveToGoal
=======================================================================================================================================
*/
void BotMoveToGoal(bot_moveresult_t *result, int movestate, bot_goal_t *goal, int travelflags) {
	int reachnum, lastreachnum, foundjumppad, ent, resultflags;
	aas_reachability_t reach, lastreach;
	bot_movestate_t *ms;
	//vec3_t mins, maxs, up = {0, 0, 1};
	//bsp_trace_t trace;
	//static int debugline;

	result->failure = qfalse;
	result->type = 0;
	result->blocked = qfalse;
	result->blockentity = 0;
	result->traveltype = 0;
	result->flags = 0;

	ms = BotMoveStateFromHandle(movestate);

	if (!ms) {
		return;
	}

	if (!goal) {
#ifndef BASEGAME // Tobias DEBUG
		botimport.Print(PRT_MESSAGE, "client %d: movetogoal -> no goal\n", ms->client);
#endif // Tobias END
		result->failure = qtrue;
		return;
	}
	//botimport.Print(PRT_MESSAGE, "numavoidreach = %d\n", ms->numavoidreach);
	// remove some of the move flags
	ms->moveflags &= ~(MFL_SWIMMING|MFL_AGAINSTLADDER);
	// set some of the move flags
	// NOTE: the MFL_ONGROUND flag is also set in the higher AI
	if (AAS_OnGround(ms->origin, ms->presencetype, ms->entitynum)) {
		ms->moveflags |= MFL_ONGROUND;
	}

	if (ms->moveflags & MFL_ONGROUND) {
		int modeltype, modelnum;

		ent = BotOnTopOfEntity(ms);

		if (ent != -1) {
			modelnum = AAS_EntityModelindex(ent);

			if (modelnum >= 0 && modelnum < MAX_SUBMODELS) {
				modeltype = modeltypes[modelnum];

				if (modeltype == MODELTYPE_FUNC_PLAT) {
					AAS_ReachabilityFromNum(ms->lastreachnum, &reach);
					// if the bot is NOT using the elevator. NOTE: the face number is the plat model number
					if ((reach.traveltype & TRAVELTYPE_MASK) != TRAVEL_ELEVATOR || (reach.facenum & 0x0000FFFF) != modelnum) {
						reachnum = AAS_NextModelReachability(0, modelnum);

						if (reachnum) {
							//botimport.Print(PRT_MESSAGE, "client %d: accidentally ended up on func_plat\n", ms->client);
							AAS_ReachabilityFromNum(reachnum, &reach);
							ms->lastreachnum = reachnum;
							ms->reachability_time = AAS_Time() + BotReachabilityTime(&reach);
						} else {
#ifndef BASEGAME // Tobias DEBUG
							botimport.Print(PRT_MESSAGE, "client %d: on func_plat without reachability\n", ms->client);
#endif // Tobias END
							result->blocked = qtrue;
							result->blockentity = ent;
							result->flags |= MOVERESULT_ONTOPOF_OBSTACLE;
							return;
						}
					}

					result->flags |= MOVERESULT_ONTOPOF_ELEVATOR;
				} else if (modeltype == MODELTYPE_FUNC_BOB) {
					AAS_ReachabilityFromNum(ms->lastreachnum, &reach);
					// if the bot is NOT using the func_bobbing. NOTE: the face number is the func_bobbing model number
					if ((reach.traveltype & TRAVELTYPE_MASK) != TRAVEL_FUNCBOB || (reach.facenum & 0x0000FFFF) != modelnum) {
						reachnum = AAS_NextModelReachability(0, modelnum);

						if (reachnum) {
							//botimport.Print(PRT_MESSAGE, "client %d: accidentally ended up on func_bobbing\n", ms->client);
							AAS_ReachabilityFromNum(reachnum, &reach);
							ms->lastreachnum = reachnum;
							ms->reachability_time = AAS_Time() + BotReachabilityTime(&reach);
						} else {
#ifndef BASEGAME // Tobias DEBUG
							botimport.Print(PRT_MESSAGE, "client %d: on func_bobbing without reachability\n", ms->client);
#endif // Tobias END
							result->blocked = qtrue;
							result->blockentity = ent;
							result->flags |= MOVERESULT_ONTOPOF_OBSTACLE;
							return;
						}
					}

					result->flags |= MOVERESULT_ONTOPOF_FUNCBOB;
				} else if (modeltype == MODELTYPE_FUNC_STATIC || modeltype == MODELTYPE_FUNC_DOOR) {
					// check if ontop of a door bridge ?
					ms->areanum = BotFuzzyPointReachabilityArea(ms->origin);
					// if not in a reachability area
					if (!AAS_AreaReachability(ms->areanum)) {
						result->blocked = qtrue;
						result->blockentity = ent;
						result->flags |= MOVERESULT_ONTOPOF_OBSTACLE;
						return;
					}
				} else {
					result->blocked = qtrue;
					result->blockentity = ent;
					result->flags |= MOVERESULT_ONTOPOF_OBSTACLE;
					return;
				}
			}
		}
	}
	// if swimming
	if (AAS_Swimming(ms->origin)) {
		ms->moveflags |= MFL_SWIMMING;
	}
	// if against a ladder
	if (AAS_AgainstLadder(ms->origin)) {
		ms->moveflags |= MFL_AGAINSTLADDER;
	}
	// if the bot is on the ground, swimming or against a ladder
	if (ms->moveflags & (MFL_ONGROUND|MFL_SWIMMING|MFL_AGAINSTLADDER)) {
		//botimport.Print(PRT_MESSAGE, "%s: onground, swimming or against ladder\n", ClientName(ms->entitynum - 1));
		AAS_ReachabilityFromNum(ms->lastreachnum, &lastreach);
		// reachability area the bot is in
		//ms->areanum = BotReachabilityArea(ms->origin, ((lastreach.traveltype & TRAVELTYPE_MASK) != TRAVEL_ELEVATOR));
		ms->areanum = BotFuzzyPointReachabilityArea(ms->origin);

		if (!ms->areanum) {
			result->failure = qtrue;
			result->blocked = qtrue;
			result->blockentity = 0;
			result->type = RESULTTYPE_INSOLIDAREA;
			return;
		}
		// if the bot is in the goal area
		if (ms->areanum == goal->areanum) {
			*result = BotMoveInGoalArea(ms, goal);
			return;
		}
		// assume we can use the reachability from the last frame
		reachnum = ms->lastreachnum;
		// if there is a last reachability
		if (reachnum) {
			AAS_ReachabilityFromNum(reachnum, &reach);
			// check if the reachability is still valid
			if (!(AAS_TravelFlagForType(reach.traveltype) & travelflags)) {
				reachnum = 0;
			// special elevator case
			} else if ((reach.traveltype & TRAVELTYPE_MASK) == TRAVEL_ELEVATOR || (reach.traveltype & TRAVELTYPE_MASK) == TRAVEL_FUNCBOB) {
				if ((result->flags & MOVERESULT_ONTOPOF_ELEVATOR) || (result->flags & MOVERESULT_ONTOPOF_FUNCBOB)) {
					ms->reachability_time = AAS_Time() + 5;
				}
				// if the bot was going for an elevator and reached the reachability area
				if (ms->areanum == reach.areanum || ms->reachability_time < AAS_Time()) {
					reachnum = 0;
				}
			} else {
#ifndef BASEGAME // Tobias DEBUG
				if (ms->reachability_time < AAS_Time()) {
					botimport.Print(PRT_MESSAGE, "client %d: reachability timeout in ", ms->client);
					BotPrintTravelType(reach.traveltype & TRAVELTYPE_MASK);
					botimport.Print(PRT_MESSAGE, "\n");
				}
				/*
				if (ms->lastareanum != ms->areanum) {
					botimport.Print(PRT_MESSAGE, "changed from area %d to %d\n", ms->lastareanum, ms->areanum);
				}
				*/
#endif // Tobias END
				// if the goal area changed or the reachability timed out or the area changed
				if (ms->lastgoalareanum != goal->areanum || ms->reachability_time < AAS_Time() || ms->lastareanum != ms->areanum) {
					reachnum = 0;
					//botimport.Print(PRT_MESSAGE, "area change or timeout\n");
				}
			}
		}

		resultflags = 0;
		// if the bot needs a new reachability
		if (!reachnum) {
			// if the area has no reachability links
			if (!AAS_AreaReachability(ms->areanum)) {
#ifndef BASEGAME // Tobias DEBUG
				botimport.Print(PRT_MESSAGE, "area %d no reachability\n", ms->areanum);
#endif // Tobias END
			}
			// get a new reachability leading towards the goal
			reachnum = BotGetReachabilityToGoal(ms->origin, ms->areanum, ms->lastgoalareanum, ms->lastareanum, ms->avoidreach, ms->avoidreachtimes, ms->avoidreachtries, goal, travelflags, ms->avoidspots, ms->numavoidspots, &resultflags);
			// the area number the reachability starts in
			ms->reachareanum = ms->areanum;
			// reset some state variables
			ms->jumpreach = 0; // for TRAVEL_JUMP
			// if there is a reachability to the goal
			if (reachnum) {
				AAS_ReachabilityFromNum(reachnum, &reach);
				// set a timeout for this reachability
				ms->reachability_time = AAS_Time() + BotReachabilityTime(&reach);
#ifdef AVOIDREACH
				// add the reachability to the reachabilities to avoid for a while
				BotAddToAvoidReach(ms, reachnum, AVOIDREACH_TIME);
#endif // AVOIDREACH
			}
#ifndef BASEGAME // Tobias DEBUG
			else {
				botimport.Print(PRT_MESSAGE, "goal not reachable\n");
				//Com_Memset(&reach, 0, sizeof(aas_reachability_t)); // make compiler happy
			}
			// if still going for the same goal
			if (ms->lastgoalareanum == goal->areanum) {
				if (ms->lastareanum == reach.areanum) {
					botimport.Print(PRT_MESSAGE, "same goal, going back to previous area\n");
				}
			}
#endif // Tobias END
		}

		ms->lastreachnum = reachnum;
		ms->lastgoalareanum = goal->areanum;
		ms->lastareanum = ms->areanum;
		// if the bot has a reachability
		if (reachnum) {
			// get the reachability from the number
			AAS_ReachabilityFromNum(reachnum, &reach);
			result->traveltype = reach.traveltype;
#ifndef BASEGAME // Tobias DEBUG
			AAS_ClearShownDebugLines();
			BotPrintTravelType(reach.traveltype & TRAVELTYPE_MASK);
			AAS_ShowReachability(&reach);
#endif // Tobias END
#ifndef BASEGAME // Tobias DEBUG
			//botimport.Print(PRT_MESSAGE, "client %d: ", ms->client);
			//BotPrintTravelType(reach.traveltype);
			//botimport.Print(PRT_MESSAGE, "\n");
#endif // Tobias END
			switch (reach.traveltype & TRAVELTYPE_MASK) {
				case TRAVEL_WALK:
					*result = BotTravel_Walk(ms, &reach);
					break;
				case TRAVEL_CROUCH:
					*result = BotTravel_Crouch(ms, &reach);
					break;
				case TRAVEL_BARRIERJUMP:
					*result = BotTravel_BarrierJump(ms, &reach);
					break;
				case TRAVEL_LADDER:
					*result = BotTravel_Ladder(ms, &reach);
					break;
				case TRAVEL_WALKOFFLEDGE:
					*result = BotTravel_WalkOffLedge(ms, &reach);
					break;
				case TRAVEL_JUMP:
					*result = BotTravel_Jump(ms, &reach);
					break;
				case TRAVEL_SWIM:
					*result = BotTravel_Swim(ms, &reach);
					break;
				case TRAVEL_WATERJUMP:
					*result = BotTravel_WaterJump(ms, &reach);
					break;
				case TRAVEL_TELEPORT:
					*result = BotTravel_Teleport(ms, &reach);
					break;
				case TRAVEL_ELEVATOR:
					*result = BotTravel_Elevator(ms, &reach);
					break;
				case TRAVEL_ROCKETJUMP:
					*result = BotTravel_RocketJump(ms, &reach);
					break;
				case TRAVEL_BFGJUMP:
					*result = BotTravel_BFGJump(ms, &reach);
					break;
				case TRAVEL_JUMPPAD:
					*result = BotTravel_JumpPad(ms, &reach);
					break;
				case TRAVEL_FUNCBOB:
					*result = BotTravel_FuncBobbing(ms, &reach);
					break;
				default:
				{
					botimport.Print(PRT_FATAL, "travel type %d not implemented yet\n", (reach.traveltype & TRAVELTYPE_MASK));
					break;
				}
			}

			result->traveltype = reach.traveltype;
			result->flags |= resultflags;
		} else {
			result->failure = qtrue;
			result->flags |= resultflags;

			Com_Memset(&reach, 0, sizeof(aas_reachability_t));
		}
#ifndef BASEGAME // Tobias DEBUG
		if (result->failure) {
			botimport.Print(PRT_MESSAGE, "client %d: movement failure in ", ms->client);
			BotPrintTravelType(reach.traveltype & TRAVELTYPE_MASK);
			botimport.Print(PRT_MESSAGE, "\n");
		}
#endif // Tobias END
	} else {
		int i, numareas, areas[16];
		vec3_t end;

		// special handling of jump pads when the bot uses a jump pad without knowing it
		foundjumppad = qfalse;

		VectorMA(ms->origin, -2 * ms->thinktime, ms->velocity, end);

		numareas = AAS_TraceAreas(ms->origin, end, areas, NULL, 16);

		for (i = numareas - 1; i >= 0; i--) {
			if (AAS_AreaJumpPad(areas[i])) {
				//botimport.Print(PRT_MESSAGE, "client %d used a jumppad without knowing, area %d\n", ms->client, areas[i]);
				foundjumppad = qtrue;
				lastreachnum = BotGetReachabilityToGoal(end, areas[i], ms->lastgoalareanum, ms->lastareanum, ms->avoidreach, ms->avoidreachtimes, ms->avoidreachtries, goal, TFL_JUMPPAD, ms->avoidspots, ms->numavoidspots, NULL);

				if (lastreachnum) {
					ms->lastreachnum = lastreachnum;
					ms->lastareanum = areas[i];
					//botimport.Print(PRT_MESSAGE, "found jumppad reachability\n");
					break;
				} else {
					for (lastreachnum = AAS_NextAreaReachability(areas[i], 0); lastreachnum; lastreachnum = AAS_NextAreaReachability(areas[i], lastreachnum)) {
						// get the reachability from the number
						AAS_ReachabilityFromNum(lastreachnum, &reach);

						if ((reach.traveltype & TRAVELTYPE_MASK) == TRAVEL_JUMPPAD) {
							ms->lastreachnum = lastreachnum;
							ms->lastareanum = areas[i];
							//botimport.Print(PRT_MESSAGE, "found jumppad reachability hard!!\n");
							break;
						}
					}

					if (lastreachnum) {
						break;
					}
				}
			}
		}

#ifndef BASEGAME // Tobias DEBUG
		// if a jumppad is found with the trace but no reachability is found
		if (foundjumppad && !ms->lastreachnum) {
			botimport.Print(PRT_MESSAGE, "client %d didn't find jumppad reachability\n", ms->client);
		}
#endif // Tobias END
		if (ms->lastreachnum) {
			//botimport.Print(PRT_MESSAGE, "%s: NOT onground, swimming or against ladder\n", ClientName(ms->entitynum - 1));
			AAS_ReachabilityFromNum(ms->lastreachnum, &reach);
			result->traveltype = reach.traveltype;
#ifndef BASEGAME // Tobias DEBUG
			//botimport.Print(PRT_MESSAGE, "client %d finish: ", ms->client);
			//BotPrintTravelType(reach.traveltype & TRAVELTYPE_MASK);
			//botimport.Print(PRT_MESSAGE, "\n");
#endif // Tobias END
			switch (reach.traveltype & TRAVELTYPE_MASK) {
				case TRAVEL_WALK:
					*result = BotTravel_Walk(ms, &reach);
					break;
				case TRAVEL_CROUCH:
					/*do nothing*/
					break;
				case TRAVEL_BARRIERJUMP:
					*result = BotFinishTravel_BarrierJump(ms, &reach);
					break;
				case TRAVEL_LADDER:
					*result = BotTravel_Ladder(ms, &reach);
					break;
				case TRAVEL_WALKOFFLEDGE:
					*result = BotFinishTravel_WalkOffLedge(ms, &reach);
					break;
				case TRAVEL_JUMP:
					*result = BotFinishTravel_Jump(ms, &reach);
					break;
				case TRAVEL_SWIM:
					*result = BotTravel_Swim(ms, &reach);
					break;
				case TRAVEL_WATERJUMP:
					*result = BotFinishTravel_WaterJump(ms, &reach);
					break;
				case TRAVEL_TELEPORT:
					/*do nothing*/
					break;
				case TRAVEL_ELEVATOR:
					*result = BotFinishTravel_Elevator(ms, &reach);
					break;
				case TRAVEL_ROCKETJUMP:
				case TRAVEL_BFGJUMP:
					*result = BotFinishTravel_WeaponJump(ms, &reach);
					break;
				case TRAVEL_JUMPPAD:
					*result = BotFinishTravel_JumpPad(ms, &reach);
					break;
				case TRAVEL_FUNCBOB:
					*result = BotFinishTravel_FuncBobbing(ms, &reach);
					break;
				default:
				{
					botimport.Print(PRT_FATAL, "(last) travel type %d not implemented yet\n", (reach.traveltype & TRAVELTYPE_MASK));
					break;
				}
			}

			result->traveltype = reach.traveltype;
#ifndef BASEGAME // Tobias DEBUG
			if (result->failure) {
				botimport.Print(PRT_MESSAGE, "client %d: movement failure in finish ", ms->client);
				BotPrintTravelType(reach.traveltype & TRAVELTYPE_MASK);
				botimport.Print(PRT_MESSAGE, "\n");
			}
#endif // Tobias END
		}
	}
	// FIXME: is it right to do this here?
	if (result->blocked) {
		ms->reachability_time -= 10 * ms->thinktime;
	}
	// copy the last origin
	VectorCopy(ms->origin, ms->lastorigin);
}

/*
=======================================================================================================================================
BotResetAvoidReach
=======================================================================================================================================
*/
void BotResetAvoidReach(int movestate) {
	bot_movestate_t *ms;

	ms = BotMoveStateFromHandle(movestate);

	if (!ms) {
		return;
	}

	Com_Memset(ms->avoidreach, 0, MAX_AVOIDREACH * sizeof(int));
	Com_Memset(ms->avoidreachtimes, 0, MAX_AVOIDREACH * sizeof(float));
	Com_Memset(ms->avoidreachtries, 0, MAX_AVOIDREACH * sizeof(int));
}

/*
=======================================================================================================================================
BotResetLastAvoidReach
=======================================================================================================================================
*/
void BotResetLastAvoidReach(int movestate) {
	int i, latest;
	float latesttime;
	bot_movestate_t *ms;

	ms = BotMoveStateFromHandle(movestate);

	if (!ms) {
		return;
	}

	latesttime = 0;
	latest = 0;

	for (i = 0; i < MAX_AVOIDREACH; i++) {
		if (ms->avoidreachtimes[i] > latesttime) {
			latesttime = ms->avoidreachtimes[i];
			latest = i;
		}
	}

	if (latesttime) {
		ms->avoidreachtimes[latest] = 0;

		if (ms->avoidreachtries[latest] > 0) {
			ms->avoidreachtries[latest]--;
		}
	}
}

/*
=======================================================================================================================================
BotResetMoveState
=======================================================================================================================================
*/
void BotResetMoveState(int movestate) {
	bot_movestate_t *ms;

	ms = BotMoveStateFromHandle(movestate);

	if (!ms) {
		return;
	}

	Com_Memset(ms, 0, sizeof(bot_movestate_t));
}

/*
=======================================================================================================================================
BotSetupMoveAI
=======================================================================================================================================
*/
int BotSetupMoveAI(void) {

	BotSetBrushModelTypes();

	sv_gravity = LibVar("sv_gravity", "800");
	sv_maxstep = LibVar("sv_step", "18");
	sv_maxbarrier = LibVar("sv_maxbarrier", "32");

	weapindex_rocketlauncher = LibVar("weapindex_rocketlauncher", "9");
	weapindex_bfg10k = LibVar("weapindex_bfg10k", "13");
	return BLERR_NOERROR;
}

/*
=======================================================================================================================================
BotShutdownMoveAI
=======================================================================================================================================
*/
void BotShutdownMoveAI(void) {
	int i;

	for (i = 1; i <= MAX_CLIENTS; i++) {
		if (botmovestates[i]) {
			FreeMemory(botmovestates[i]);
			botmovestates[i] = NULL;
		}
	}
}
