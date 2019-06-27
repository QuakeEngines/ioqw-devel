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
 Text commands typed in at the local console, or executed by a key binding.
**************************************************************************************************************************************/

#include "cg_local.h"
#include "../ui/ui_shared.h"

extern menuDef_t *menuScoreboard;

/*
=======================================================================================================================================
CG_TargetCommand_f
=======================================================================================================================================
*/
void CG_TargetCommand_f(void) {
	int targetNum;
	char test[4];

	targetNum = CG_CrosshairPlayer();

	if (targetNum == -1) {
		return;
	}

	trap_Argv(1, test, 4);
	trap_SendClientCommand(va("gc %i %i", targetNum, atoi(test)));
}

/*
=======================================================================================================================================
CG_SizeUp_f

Keybinding command.
=======================================================================================================================================
*/
static void CG_SizeUp_f(void) {
	// manually clamp here so cvar range warning isn't shown
	trap_Cvar_SetValue("cg_viewsize", Com_Clamp(30, 100, (int)(cg_viewsize.integer + 10)));
}

/*
=======================================================================================================================================
CG_SizeDown_f

Keybinding command.
=======================================================================================================================================
*/
static void CG_SizeDown_f(void) {
	// manually clamp here so cvar range warning isn't shown
	trap_Cvar_SetValue("cg_viewsize", Com_Clamp(30, 100, (int)(cg_viewsize.integer - 10)));
}

/*
=======================================================================================================================================
CG_Viewpos_f

Debugging command to print the current position.
=======================================================================================================================================
*/
static void CG_Viewpos_f(void) {
	CG_Printf("(%i %i %i) : %i\n", (int)cg.refdef.vieworg[0], (int)cg.refdef.vieworg[1], (int)cg.refdef.vieworg[2], (int)cg.refdefViewAngles[YAW]);
}

/*
=======================================================================================================================================
CG_ScoresDown_f
=======================================================================================================================================
*/
static void CG_ScoresDown_f(void) {

	CG_BuildSpectatorString();

	if (cg.scoresRequestTime + 2000 < cg.time) {
		// the scores are more than two seconds out of data, so request new ones
		cg.scoresRequestTime = cg.time;

		trap_SendClientCommand("score");
		// leave the current scores up if they were already displayed, but if this is the first hit, clear them out
		if (!cg.showScores) {
			cg.showScores = qtrue;
			cg.numScores = 0;
		}
	} else {
		// show the cached contents even if they just pressed if it is within two seconds
		cg.showScores = qtrue;
	}
}

/*
=======================================================================================================================================
CG_ScoresUp_f
=======================================================================================================================================
*/
static void CG_ScoresUp_f(void) {

	if (cg.showScores) {
		cg.showScores = qfalse;
		cg.scoreFadeTime = cg.time;
	}
}

extern menuDef_t *menuScoreboard;
void Menu_Reset(void); // FIXME: add to right include file
/*
=======================================================================================================================================
CG_LoadHud_f
=======================================================================================================================================
*/
static void CG_LoadHud_f(void) {
	char buff[1024];
	const char *hudSet;

	memset(buff, 0, sizeof(buff));

	String_Init();
	Menu_Reset();
	trap_Cvar_VariableStringBuffer("cg_hudFiles", buff, sizeof(buff));

	hudSet = buff;

	if (hudSet[0] == '\0') {
		hudSet = "ui/hud.txt";
	}

	CG_LoadMenus(hudSet);
	CG_HudMenuHacks();

	menuScoreboard = NULL;
}

/*
=======================================================================================================================================
CG_scrollScoresDown_f
=======================================================================================================================================
*/
static void CG_scrollScoresDown_f(void) {

	if (menuScoreboard && cg.scoreBoardShowing) {
		Menu_ScrollFeeder(menuScoreboard, FEEDER_SCOREBOARD, qtrue);
		Menu_ScrollFeeder(menuScoreboard, FEEDER_REDTEAM_LIST, qtrue);
		Menu_ScrollFeeder(menuScoreboard, FEEDER_BLUETEAM_LIST, qtrue);
	}
}

/*
=======================================================================================================================================
CG_scrollScoresUp_f
=======================================================================================================================================
*/
static void CG_scrollScoresUp_f(void) {

	if (menuScoreboard && cg.scoreBoardShowing) {
		Menu_ScrollFeeder(menuScoreboard, FEEDER_SCOREBOARD, qfalse);
		Menu_ScrollFeeder(menuScoreboard, FEEDER_REDTEAM_LIST, qfalse);
		Menu_ScrollFeeder(menuScoreboard, FEEDER_BLUETEAM_LIST, qfalse);
	}
}

/*
=======================================================================================================================================
CG_spWin_f
=======================================================================================================================================
*/
static void CG_spWin_f(void) {

	trap_Cvar_SetValue("cg_cameraOrbit", 30);
	trap_Cvar_SetValue("cg_thirdPerson", 1);
	trap_Cvar_SetValue("cg_thirdPersonAngle", 0);
	trap_Cvar_SetValue("cg_thirdPersonRange", 100);

	CG_AddBufferedAnnouncerSound(cgs.media.winnerSound);
	CG_CenterPrint("YOU WIN!", SCREEN_HEIGHT * .30, 0);
}

/*
=======================================================================================================================================
CG_spLose_f
=======================================================================================================================================
*/
static void CG_spLose_f(void) {

	trap_Cvar_SetValue("cg_cameraOrbit", 30);
	trap_Cvar_SetValue("cg_thirdPerson", 1);
	trap_Cvar_SetValue("cg_thirdPersonAngle", 0);
	trap_Cvar_SetValue("cg_thirdPersonRange", 100);

	CG_AddBufferedAnnouncerSound(cgs.media.loserSound);
	CG_CenterPrint("YOU LOSE...", SCREEN_HEIGHT * .30, 0);
}

/*
=======================================================================================================================================
CG_TellTarget_f
=======================================================================================================================================
*/
static void CG_TellTarget_f(void) {
	int clientNum;
	char command[128];
	char message[128];

	clientNum = CG_CrosshairPlayer();

	if (clientNum == -1) {
		return;
	}

	trap_Args(message, 128);
	Com_sprintf(command, 128, "tell %i %s", clientNum, message);
	trap_SendClientCommand(command);
}

/*
=======================================================================================================================================
CG_TellAttacker_f
=======================================================================================================================================
*/
static void CG_TellAttacker_f(void) {
	int clientNum;
	char command[128];
	char message[128];

	clientNum = CG_LastAttacker();

	if (clientNum == -1) {
		return;
	}

	trap_Args(message, 128);
	Com_sprintf(command, 128, "tell %i %s", clientNum, message);
	trap_SendClientCommand(command);
}

/*
=======================================================================================================================================
CG_VoiceTellTarget_f
=======================================================================================================================================
*/
static void CG_VoiceTellTarget_f(void) {
	int clientNum;
	char command[128];
	char message[128];

	clientNum = CG_CrosshairPlayer();

	if (clientNum == -1) {
		return;
	}

	trap_Args(message, 128);
	Com_sprintf(command, 128, "vtell %i %s", clientNum, message);
	trap_SendClientCommand(command);
}

/*
=======================================================================================================================================
CG_VoiceTellAttacker_f
=======================================================================================================================================
*/
static void CG_VoiceTellAttacker_f(void) {
	int clientNum;
	char command[128];
	char message[128];

	clientNum = CG_LastAttacker();

	if (clientNum == -1) {
		return;
	}

	trap_Args(message, 128);
	Com_sprintf(command, 128, "vtell %i %s", clientNum, message);
	trap_SendClientCommand(command);
}

/*
=======================================================================================================================================
CG_NextTeamMember_f
=======================================================================================================================================
*/
static void CG_NextTeamMember_f(void) {
	CG_SelectNextPlayer();
}

/*
=======================================================================================================================================
CG_PrevTeamMember_f
=======================================================================================================================================
*/
static void CG_PrevTeamMember_f(void) {
	CG_SelectPrevPlayer();
}

/*
=======================================================================================================================================
CG_NextOrder_f

ASS U ME's enumeration order as far as task specific orders, OFFENSE is zero, PATROL is last.
=======================================================================================================================================
*/
static void CG_NextOrder_f(void) {
	clientInfo_t *ci = cgs.clientinfo + cg.snap->ps.clientNum;

	if (ci) {
		if (!ci->teamLeader && sortedTeamPlayers[cg_currentSelectedPlayer.integer] != cg.snap->ps.clientNum) {
			return;
		}
	}

	if (cgs.currentOrder < TEAMTASK_PATROL) {
		cgs.currentOrder++;

		if (cgs.currentOrder == TEAMTASK_RETRIEVE) {
			if (!CG_OtherTeamHasFlag()) {
				cgs.currentOrder++;
			}
		}

		if (cgs.currentOrder == TEAMTASK_ESCORT) {
			if (!CG_YourTeamHasFlag()) {
				cgs.currentOrder++;
			}
		}
	} else {
		cgs.currentOrder = TEAMTASK_OFFENSE;
	}

	cgs.orderPending = qtrue;
	cgs.orderTime = cg.time + 3000;
}

/*
=======================================================================================================================================
CG_ConfirmOrder_f
=======================================================================================================================================
*/
static void CG_ConfirmOrder_f(void) {

	trap_Cmd_ExecuteText(EXEC_NOW, va("cmd vtell %d %s\n", cgs.acceptLeader, VOICECHAT_YES));
	trap_Cmd_ExecuteText(EXEC_NOW, "+button5; wait; -button5");

	if (cg.time < cgs.acceptOrderTime) {
		trap_SendClientCommand(va("teamtask %d\n", cgs.acceptTask));
		cgs.acceptOrderTime = 0;
	}
}

/*
=======================================================================================================================================
CG_DenyOrder_f
=======================================================================================================================================
*/
static void CG_DenyOrder_f(void) {

	trap_Cmd_ExecuteText(EXEC_NOW, va("cmd vtell %d %s\n", cgs.acceptLeader, VOICECHAT_NO));
	trap_Cmd_ExecuteText(EXEC_NOW, "+button6; wait; -button6");

	if (cg.time < cgs.acceptOrderTime) {
		cgs.acceptOrderTime = 0;
	}
}

/*
=======================================================================================================================================
CG_TaskOffense_f
=======================================================================================================================================
*/
static void CG_TaskOffense_f(void) {

	if (cgs.gametype == GT_CTF || cgs.gametype == GT_1FCTF) {
		trap_Cmd_ExecuteText(EXEC_NOW, va("cmd vsay_team %s\n", VOICECHAT_ONGETFLAG));
	} else {
		trap_Cmd_ExecuteText(EXEC_NOW, va("cmd vsay_team %s\n", VOICECHAT_ONOFFENSE));
	}

	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_OFFENSE));
}

/*
=======================================================================================================================================
CG_TaskDefense_f
=======================================================================================================================================
*/
static void CG_TaskDefense_f(void) {

	trap_Cmd_ExecuteText(EXEC_NOW, va("cmd vsay_team %s\n", VOICECHAT_ONDEFENSE));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_DEFENSE));
}

/*
=======================================================================================================================================
CG_TaskPatrol_f
=======================================================================================================================================
*/
static void CG_TaskPatrol_f(void) {

	trap_Cmd_ExecuteText(EXEC_NOW, va("cmd vsay_team %s\n", VOICECHAT_ONPATROL));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_PATROL));
}

/*
=======================================================================================================================================
CG_TaskCamp_f
=======================================================================================================================================
*/
static void CG_TaskCamp_f(void) {

	trap_Cmd_ExecuteText(EXEC_NOW, va("cmd vsay_team %s\n", VOICECHAT_ONCAMPING));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_CAMP));
}

/*
=======================================================================================================================================
CG_TaskFollow_f
=======================================================================================================================================
*/
static void CG_TaskFollow_f(void) {

	trap_Cmd_ExecuteText(EXEC_NOW, va("cmd vsay_team %s\n", VOICECHAT_ONFOLLOW));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_FOLLOW));
}

/*
=======================================================================================================================================
CG_TaskRetrieve_f
=======================================================================================================================================
*/
static void CG_TaskRetrieve_f(void) {

	trap_Cmd_ExecuteText(EXEC_NOW, va("cmd vsay_team %s\n", VOICECHAT_ONRETURNFLAG));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_RETRIEVE));
}

/*
=======================================================================================================================================
CG_TaskEscort_f
=======================================================================================================================================
*/
static void CG_TaskEscort_f(void) {

	trap_Cmd_ExecuteText(EXEC_NOW, va("cmd vsay_team %s\n", VOICECHAT_ONFOLLOWCARRIER));
	trap_SendClientCommand(va("teamtask %d\n", TEAMTASK_ESCORT));
}

/*
=======================================================================================================================================
CG_TaskOwnFlag_f
=======================================================================================================================================
*/
static void CG_TaskOwnFlag_f(void) {
	trap_Cmd_ExecuteText(EXEC_NOW, va("cmd vsay_team %s\n", VOICECHAT_IHAVEFLAG));
}

/*
=======================================================================================================================================
CG_TauntKillInsult_f
=======================================================================================================================================
*/
static void CG_TauntKillInsult_f(void) {
	trap_Cmd_ExecuteText(EXEC_NOW, "cmd vsay kill_insult\n");
}

/*
=======================================================================================================================================
CG_TauntPraise_f
=======================================================================================================================================
*/
static void CG_TauntPraise_f(void) {
	trap_Cmd_ExecuteText(EXEC_NOW, "cmd vsay praise\n");
}

/*
=======================================================================================================================================
CG_TauntTaunt_f
=======================================================================================================================================
*/
static void CG_TauntTaunt_f(void) {
	trap_Cmd_ExecuteText(EXEC_NOW, "cmd vtaunt\n");
}

/*
=======================================================================================================================================
CG_TauntDeathInsult_f
=======================================================================================================================================
*/
static void CG_TauntDeathInsult_f(void) {
	trap_Cmd_ExecuteText(EXEC_NOW, "cmd vsay death_insult\n");
}

/*
=======================================================================================================================================
CG_TauntGauntlet_f
=======================================================================================================================================
*/
static void CG_TauntGauntlet_f(void) {
	trap_Cmd_ExecuteText(EXEC_NOW, "cmd vsay kill_gauntlet\n");
}

/*
=======================================================================================================================================
CG_TaskSuicide_f
=======================================================================================================================================
*/
static void CG_TaskSuicide_f(void) {
	int clientNum;
	char command[128];

	clientNum = CG_CrosshairPlayer();

	if (clientNum == -1) {
		return;
	}

	Com_sprintf(command, 128, "tell %i suicide", clientNum);
	trap_SendClientCommand(command);
}

/*
=======================================================================================================================================
CG_TeamMenu_f
=======================================================================================================================================
*/
/*
static void CG_TeamMenu_f(void) {

	if (trap_Key_GetCatcher() & KEYCATCH_CGAME) {
		CG_EventHandling(CGAME_EVENT_NONE);
		trap_Key_SetCatcher(0);
	} else {
		CG_EventHandling(CGAME_EVENT_TEAMMENU);
		//trap_Key_SetCatcher(KEYCATCH_CGAME);
	}
}
*/
/*
=======================================================================================================================================
CG_EditHud_f
=======================================================================================================================================
*/
/*
static void CG_EditHud_f(void) {

	//cls.keyCatchers ^= KEYCATCH_CGAME;

	//VM_Call(cgvm, CG_EVENT_HANDLING, (cls.keyCatchers & KEYCATCH_CGAME) ? CGAME_EVENT_EDITHUD : CGAME_EVENT_NONE);
}
*/
/*
=======================================================================================================================================
CG_StartOrbit_f
=======================================================================================================================================
*/
static void CG_StartOrbit_f(void) {
	char var[MAX_TOKEN_CHARS];

	trap_Cvar_VariableStringBuffer("developer", var, sizeof(var));

	if (!atoi(var)) {
		return;
	}

	if (cg_cameraOrbit.value != 0) {
		trap_Cvar_SetValue("cg_cameraOrbit", 0);
		trap_Cvar_SetValue("cg_thirdPerson", 0);
	} else {
		trap_Cvar_SetValue("cg_cameraOrbit", 30);
		trap_Cvar_SetValue("cg_thirdPerson", 1);
		trap_Cvar_SetValue("cg_thirdPersonAngle", 0);
		trap_Cvar_SetValue("cg_thirdPersonRange", 100);
	}
}

/*
=======================================================================================================================================
CG_Camera_f
=======================================================================================================================================
*/
/*
static void CG_Camera_f(void) {
	char name[1024];

	trap_Argv(1, name, sizeof(name));

	if (trap_loadCamera(name)) {
		cg.cameraMode = qtrue;
		trap_startCamera(cg.time);
	} else {
		CG_Printf("Unable to load camera %s\n", name);
	}
}
*/
/*
=======================================================================================================================================
CG_GenerateTracemap
=======================================================================================================================================
*/
void CG_GenerateTracemap(void) {
	bgGenTracemap_t gen;

	if (!cg.mapcoordsValid) {
		CG_Printf("Need valid mapcoords in the worldspawn to be able to generate a tracemap.\n");
		return;
	}

	gen.trace = CG_Trace;
	gen.pointcontents = CG_PointContents;

	BG_GenerateTracemap(cgs.mapname, cg.mapcoordsMins, cg.mapcoordsMaxs, &gen);
}

typedef struct {
	char *cmd;
	void (*function)(void);
} consoleCommand_t;

static consoleCommand_t commands[] = {
	{"testgun", CG_TestGun_f},
	{"testmodel", CG_TestModel_f},
	{"nextframe", CG_TestModelNextFrame_f},
	{"prevframe", CG_TestModelPrevFrame_f},
	{"nextskin", CG_TestModelNextSkin_f},
	{"prevskin", CG_TestModelPrevSkin_f},
	{"viewpos", CG_Viewpos_f},
	{"+scores", CG_ScoresDown_f},
	{"-scores", CG_ScoresUp_f},
	{"+zoom", CG_ZoomDown_f},
	{"-zoom", CG_ZoomUp_f},
	{"sizeup", CG_SizeUp_f},
	{"sizedown", CG_SizeDown_f},
	{"weapnext", CG_NextWeapon_f},
	{"weapprev", CG_PrevWeapon_f},
	{"weapon", CG_Weapon_f},
	{"tcmd", CG_TargetCommand_f},
	{"tell_target", CG_TellTarget_f},
	{"tell_attacker", CG_TellAttacker_f},
	{"vtell_target", CG_VoiceTellTarget_f},
	{"vtell_attacker", CG_VoiceTellAttacker_f},
	{"loadhud", CG_LoadHud_f},
	{"nextTeamMember", CG_NextTeamMember_f},
	{"prevTeamMember", CG_PrevTeamMember_f},
	{"nextOrder", CG_NextOrder_f},
	{"confirmOrder", CG_ConfirmOrder_f},
	{"denyOrder", CG_DenyOrder_f},
	{"taskOffense", CG_TaskOffense_f},
	{"taskDefense", CG_TaskDefense_f},
	{"taskPatrol", CG_TaskPatrol_f},
	{"taskCamp", CG_TaskCamp_f},
	{"taskFollow", CG_TaskFollow_f},
	{"taskRetrieve", CG_TaskRetrieve_f},
	{"taskEscort", CG_TaskEscort_f},
	{"taskSuicide", CG_TaskSuicide_f},
	{"taskOwnFlag", CG_TaskOwnFlag_f},
	{"tauntKillInsult", CG_TauntKillInsult_f},
	{"tauntPraise", CG_TauntPraise_f},
	{"tauntTaunt", CG_TauntTaunt_f},
	{"tauntDeathInsult", CG_TauntDeathInsult_f},
	{"tauntGauntlet", CG_TauntGauntlet_f},
	{"spWin", CG_spWin_f},
	{"spLose", CG_spLose_f},
	{"scoresDown", CG_scrollScoresDown_f},
	{"scoresUp", CG_scrollScoresUp_f},
	{"startOrbit", CG_StartOrbit_f},
	//{"camera", CG_Camera_f},
	{"loaddeferred", CG_LoadDeferredPlayers},
	{"generateTracemap", CG_GenerateTracemap}
};

/*
=======================================================================================================================================
CG_ConsoleCommand

The string has been tokenized and can be retrieved with Cmd_Argc()/Cmd_Argv().
=======================================================================================================================================
*/
qboolean CG_ConsoleCommand(void) {
	const char *cmd;
	int i;

	cmd = CG_Argv(0);

	for (i = 0; i < ARRAY_LEN(commands); i++) {
		if (!Q_stricmp(cmd, commands[i].cmd)) {
			commands[i].function();
			return qtrue;
		}
	}

	return qfalse;
}

/*
=======================================================================================================================================
CG_InitConsoleCommands

Let the client system know about all of our commands so it can perform tab completion.
=======================================================================================================================================
*/
void CG_InitConsoleCommands(void) {
	int i;

	for (i = 0; i < ARRAY_LEN(commands); i++) {
		trap_AddCommand(commands[i].cmd);
	}
	// the game server will interpret these commands, which will be automatically forwarded to the server after they are not recognized locally
	trap_AddCommand("kill");
	trap_AddCommand("say");
	trap_AddCommand("say_team");
	trap_AddCommand("tell");
	trap_AddCommand("vsay");
	trap_AddCommand("vsay_team");
	trap_AddCommand("vtell");
	trap_AddCommand("vtaunt");
	trap_AddCommand("vosay");
	trap_AddCommand("vosay_team");
	trap_AddCommand("votell");
	trap_AddCommand("give");
	trap_AddCommand("god");
	trap_AddCommand("notarget");
	trap_AddCommand("noclip");
	trap_AddCommand("where");
	trap_AddCommand("team");
	trap_AddCommand("follow");
	trap_AddCommand("follownext");
	trap_AddCommand("followprev");
	trap_AddCommand("levelshot");
	trap_AddCommand("addbot");
	trap_AddCommand("setviewpos");
	trap_AddCommand("callvote");
	trap_AddCommand("vote");
	trap_AddCommand("callteamvote");
	trap_AddCommand("teamvote");
	trap_AddCommand("stats");
	trap_AddCommand("teamtask");
}
