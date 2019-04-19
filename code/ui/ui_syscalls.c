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

#include "ui_local.h"

// this file is only included when building a dll
// syscalls.asm is included instead when building a qvm
#ifdef Q3_VM
#error "Do not use in VM build"
#endif

static intptr_t(QDECL *syscall)(intptr_t arg, ...) = (intptr_t(QDECL *)(intptr_t, ...)) - 1;

/*
=======================================================================================================================================
dllEntry
=======================================================================================================================================
*/
Q_EXPORT void dllEntry(intptr_t(QDECL *syscallptr)(intptr_t arg, ...)) {
	syscall = syscallptr;
}

/*
=======================================================================================================================================
PASSFLOAT
=======================================================================================================================================
*/
int PASSFLOAT(float x) {
	floatint_t fi;

	fi.f = x;
	return fi.i;
}

/*
=======================================================================================================================================
trap_Print
=======================================================================================================================================
*/
void trap_Print(const char *string) {
	syscall(UI_PRINT, string);
}

/*
=======================================================================================================================================
trap_Error
=======================================================================================================================================
*/
void trap_Error(const char *string) {

	syscall(UI_ERROR, string);
	// shut up GCC warning about returning functions, because we know better
	exit(1);
}

/*
=======================================================================================================================================
trap_Milliseconds
=======================================================================================================================================
*/
int trap_Milliseconds(void) {
	return syscall(UI_MILLISECONDS);
}

/*
=======================================================================================================================================
trap_RealTime
=======================================================================================================================================
*/
int trap_RealTime(qtime_t *qtime) {
	return syscall(UI_REAL_TIME, qtime);
}

/*
=======================================================================================================================================
trap_SnapVector
=======================================================================================================================================
*/
void trap_SnapVector(float *v) {
	syscall(UI_SNAPVECTOR, v);
}

/*
=======================================================================================================================================
trap_Argc
=======================================================================================================================================
*/
int trap_Argc(void) {
	return syscall(UI_ARGC);
}

/*
=======================================================================================================================================
trap_Argv
=======================================================================================================================================
*/
void trap_Argv(int n, char *buffer, int bufferLength) {
	syscall(UI_ARGV, n, buffer, bufferLength);
}

/*
=======================================================================================================================================
trap_Args
=======================================================================================================================================
*/
void trap_Args(char *buffer, int bufferLength) {
	syscall(UI_ARGS, buffer, bufferLength);
}

/*
=======================================================================================================================================
trap_AddCommand
=======================================================================================================================================
*/
void trap_AddCommand(const char *cmdName) {
	syscall(UI_ADDCOMMAND, cmdName);
}

/*
=======================================================================================================================================
trap_RemoveCommand
=======================================================================================================================================
*/
void trap_RemoveCommand(const char *cmdName) {
	syscall(UI_REMOVECOMMAND, cmdName);
}

/*
=======================================================================================================================================
trap_Cmd_ExecuteText
=======================================================================================================================================
*/
void trap_Cmd_ExecuteText(int exec_when, const char *text) {
	syscall(UI_CMD_EXECUTETEXT, exec_when, text);
}

/*
=======================================================================================================================================
trap_Cvar_Register
=======================================================================================================================================
*/
void trap_Cvar_Register(vmCvar_t *cvar, const char *var_name, const char *value, int flags) {
	syscall(UI_CVAR_REGISTER, cvar, var_name, value, flags);
}

/*
=======================================================================================================================================
trap_Cvar_Update
=======================================================================================================================================
*/
void trap_Cvar_Update(vmCvar_t *cvar) {
	syscall(UI_CVAR_UPDATE, cvar);
}

/*
=======================================================================================================================================
trap_Cvar_Set
=======================================================================================================================================
*/
void trap_Cvar_Set(const char *var_name, const char *value) {
	syscall(UI_CVAR_SET, var_name, value);
}

/*
=======================================================================================================================================
trap_Cvar_SetValue
=======================================================================================================================================
*/
void trap_Cvar_SetValue(const char *var_name, float value) {
	syscall(UI_CVAR_SET_VALUE, var_name, PASSFLOAT(value));
}

/*
=======================================================================================================================================
trap_Cvar_VariableValue
=======================================================================================================================================
*/
float trap_Cvar_VariableValue(const char *var_name) {
	floatint_t fi;

	fi.i = syscall(UI_CVAR_VARIABLE_VALUE, var_name);
	return fi.f;
}

/*
=======================================================================================================================================
trap_Cvar_VariableIntegerValue
=======================================================================================================================================
*/
int trap_Cvar_VariableIntegerValue(const char *var_name) {
	return syscall(UI_CVAR_VARIABLE_INTEGER_VALUE, var_name);
}

/*
=======================================================================================================================================
trap_Cvar_VariableStringBuffer
=======================================================================================================================================
*/
void trap_Cvar_VariableStringBuffer(const char *var_name, char *buffer, int bufsize) {
	syscall(UI_CVAR_VARIABLE_STRING_BUFFER, var_name, buffer, bufsize);
}

/*
=======================================================================================================================================
trap_Cvar_Reset
=======================================================================================================================================
*/
void trap_Cvar_Reset(const char *name) {
	syscall(UI_CVAR_RESET, name);
}

/*
=======================================================================================================================================
trap_FS_FOpenFile
=======================================================================================================================================
*/
int trap_FS_FOpenFile(const char *qpath, fileHandle_t *f, fsMode_t mode) {
	return syscall(UI_FS_FOPENFILE, qpath, f, mode);
}

/*
=======================================================================================================================================
trap_FS_Read
=======================================================================================================================================
*/
void trap_FS_Read(void *buffer, int len, fileHandle_t f) {
	syscall(UI_FS_READ, buffer, len, f);
}

/*
=======================================================================================================================================
trap_FS_Write
=======================================================================================================================================
*/
void trap_FS_Write(const void *buffer, int len, fileHandle_t f) {
	syscall(UI_FS_WRITE, buffer, len, f);
}

/*
=======================================================================================================================================
trap_FS_Seek
=======================================================================================================================================
*/
int trap_FS_Seek(fileHandle_t f, long offset, int origin) {
	return syscall(UI_FS_SEEK, f, offset, origin);
}

/*
=======================================================================================================================================
trap_FS_FCloseFile
=======================================================================================================================================
*/
void trap_FS_FCloseFile(fileHandle_t f) {
	syscall(UI_FS_FCLOSEFILE, f);
}

/*
=======================================================================================================================================
trap_FS_GetFileList
=======================================================================================================================================
*/
int trap_FS_GetFileList(const char *path, const char *extension, char *listbuf, int bufsize) {
	return syscall(UI_FS_GETFILELIST, path, extension, listbuf, bufsize);
}

/*
=======================================================================================================================================
trap_PC_AddGlobalDefine
=======================================================================================================================================
*/
int trap_PC_AddGlobalDefine(const char *define) {
	return syscall(UI_PC_ADD_GLOBAL_DEFINE, define);
}

/*
=======================================================================================================================================
trap_PC_LoadSource
=======================================================================================================================================
*/
int trap_PC_LoadSource(const char *filename) {
	return syscall(UI_PC_LOAD_SOURCE, filename);
}

/*
=======================================================================================================================================
trap_PC_FreeSource
=======================================================================================================================================
*/
int trap_PC_FreeSource(int handle) {
	return syscall(UI_PC_FREE_SOURCE, handle);
}

/*
=======================================================================================================================================
trap_PC_ReadToken
=======================================================================================================================================
*/
int trap_PC_ReadToken(int handle, pc_token_t *pc_token) {
	return syscall(UI_PC_READ_TOKEN, handle, pc_token);
}

/*
=======================================================================================================================================
trap_PC_SourceFileAndLine
=======================================================================================================================================
*/
int trap_PC_SourceFileAndLine(int handle, char *filename, int *line) {
	return syscall(UI_PC_SOURCE_FILE_AND_LINE, handle, filename, line);
}

/*
=======================================================================================================================================
trap_GetGlconfig
=======================================================================================================================================
*/
void trap_GetGlconfig(glconfig_t *glconfig) {
	syscall(UI_GETGLCONFIG, glconfig);
}

/*
=======================================================================================================================================
trap_MemoryRemaining
=======================================================================================================================================
*/
int trap_MemoryRemaining(void) {
	return syscall(UI_MEMORY_REMAINING);
}

/*
=======================================================================================================================================
trap_UpdateScreen
=======================================================================================================================================
*/
void trap_UpdateScreen(void) {
	syscall(UI_UPDATESCREEN);
}

/*
=======================================================================================================================================
trap_GetClientState
=======================================================================================================================================
*/
void trap_GetClientState(uiClientState_t *state) {
	syscall(UI_GETCLIENTSTATE, state);
}

/*
=======================================================================================================================================
trap_GetConfigString
=======================================================================================================================================
*/
int trap_GetConfigString(int index, char *buff, int buffsize) {
	return syscall(UI_GETCONFIGSTRING, index, buff, buffsize);
}

/*
=======================================================================================================================================
trap_LAN_GetServerCount
=======================================================================================================================================
*/
int trap_LAN_GetServerCount(int source) {
	return syscall(UI_LAN_GETSERVERCOUNT, source);
}

/*
=======================================================================================================================================
trap_LAN_GetServerAddressString
=======================================================================================================================================
*/
void trap_LAN_GetServerAddressString(int source, int n, char *buf, int buflen) {
	syscall(UI_LAN_GETSERVERADDRESSSTRING, source, n, buf, buflen);
}

/*
=======================================================================================================================================
trap_LAN_GetServerInfo
=======================================================================================================================================
*/
void trap_LAN_GetServerInfo(int source, int n, char *buf, int buflen) {
	syscall(UI_LAN_GETSERVERINFO, source, n, buf, buflen);
}

/*
=======================================================================================================================================
trap_LAN_MarkServerVisible
=======================================================================================================================================
*/
void trap_LAN_MarkServerVisible(int source, int n, qboolean visible) {
	syscall(UI_LAN_MARKSERVERVISIBLE, source, n, visible);
}

/*
=======================================================================================================================================
trap_LAN_UpdateVisiblePings
=======================================================================================================================================
*/
qboolean trap_LAN_UpdateVisiblePings(int source) {
	return syscall(UI_LAN_UPDATEVISIBLEPINGS, source);
}

/*
=======================================================================================================================================
trap_LAN_ResetPings
=======================================================================================================================================
*/
void trap_LAN_ResetPings(int n) {
	syscall(UI_LAN_RESETPINGS, n);
}

/*
=======================================================================================================================================
trap_LAN_GetPingQueueCount
=======================================================================================================================================
*/
int trap_LAN_GetPingQueueCount(void) {
	return syscall(UI_LAN_GETPINGQUEUECOUNT);
}

/*
=======================================================================================================================================
trap_LAN_ClearPing
=======================================================================================================================================
*/
void trap_LAN_ClearPing(int n) {
	syscall(UI_LAN_CLEARPING, n);
}

/*
=======================================================================================================================================
trap_LAN_GetPing
=======================================================================================================================================
*/
void trap_LAN_GetPing(int n, char *buf, int buflen, int *pingtime) {
	syscall(UI_LAN_GETPING, n, buf, buflen, pingtime);
}

/*
=======================================================================================================================================
trap_LAN_GetPingInfo
=======================================================================================================================================
*/
void trap_LAN_GetPingInfo(int n, char *buf, int buflen) {
	syscall(UI_LAN_GETPINGINFO, n, buf, buflen);
}

/*
=======================================================================================================================================
trap_LAN_LoadCachedServers
=======================================================================================================================================
*/
void trap_LAN_LoadCachedServers(void) {
	syscall(UI_LAN_LOADCACHEDSERVERS);
}

/*
=======================================================================================================================================
trap_LAN_SaveCachedServers
=======================================================================================================================================
*/
void trap_LAN_SaveCachedServers(void) {
	syscall(UI_LAN_SAVECACHEDSERVERS);
}

/*
=======================================================================================================================================
trap_LAN_AddServer
=======================================================================================================================================
*/
int trap_LAN_AddServer(int source, const char *name, const char *addr) {
	return syscall(UI_LAN_ADDSERVER, source, name, addr);
}

/*
=======================================================================================================================================
trap_LAN_RemoveServer
=======================================================================================================================================
*/
void trap_LAN_RemoveServer(int source, const char *addr) {
	syscall(UI_LAN_REMOVESERVER, source, addr);
}

/*
=======================================================================================================================================
trap_LAN_ServerStatus
=======================================================================================================================================
*/
int trap_LAN_ServerStatus(const char *serverAddress, char *serverStatus, int maxLen) {
	return syscall(UI_LAN_SERVERSTATUS, serverAddress, serverStatus, maxLen);
}

/*
=======================================================================================================================================
trap_LAN_GetServerPing
=======================================================================================================================================
*/
int trap_LAN_GetServerPing(int source, int n) {
	return syscall(UI_LAN_GETSERVERPING, source, n);
}

/*
=======================================================================================================================================
trap_LAN_ServerIsVisible
=======================================================================================================================================
*/
int trap_LAN_ServerIsVisible(int source, int n) {
	return syscall(UI_LAN_SERVERISVISIBLE, source, n);
}

/*
=======================================================================================================================================
trap_LAN_CompareServers
=======================================================================================================================================
*/
int trap_LAN_CompareServers(int source, int sortKey, int sortDir, int s1, int s2) {
	return syscall(UI_LAN_COMPARESERVERS, source, sortKey, sortDir, s1, s2);
}

/*
=======================================================================================================================================
trap_R_RegisterModel
=======================================================================================================================================
*/
qhandle_t trap_R_RegisterModel(const char *name) {
	return syscall(UI_R_REGISTERMODEL, name);
}

/*
=======================================================================================================================================
trap_R_RegisterShaderEx
=======================================================================================================================================
*/
qhandle_t trap_R_RegisterShaderEx(const char *name, int lightmapIndex, qboolean mipRawImage) {
	return syscall(UI_R_REGISTERSHADEREX, name, lightmapIndex, mipRawImage);
}

/*
=======================================================================================================================================
trap_R_RegisterShader
=======================================================================================================================================
*/
qhandle_t trap_R_RegisterShader(const char *name) {
	return syscall(UI_R_REGISTERSHADER, name);
}

/*
=======================================================================================================================================
trap_R_RegisterShaderNoMip
=======================================================================================================================================
*/
qhandle_t trap_R_RegisterShaderNoMip(const char *name) {
	return syscall(UI_R_REGISTERSHADERNOMIP, name);
}

/*
=======================================================================================================================================
trap_R_RegisterFont
=======================================================================================================================================
*/
void trap_R_RegisterFont(const char *fontName, int pointSize, float borderWidth, qboolean forceAutoHint, fontInfo_t *font) {
	syscall(UI_R_REGISTERFONT, fontName, pointSize, PASSFLOAT(borderWidth), forceAutoHint, font);
}

/*
=======================================================================================================================================
trap_R_RenderScene
=======================================================================================================================================
*/
void trap_R_RenderScene(const refdef_t *fd) {
	syscall(UI_R_RENDERSCENE, fd);
}

/*
=======================================================================================================================================
trap_R_ClearScene
=======================================================================================================================================
*/
void trap_R_ClearScene(void) {
	syscall(UI_R_CLEARSCENE);
}

/*
=======================================================================================================================================
trap_R_SetColor
=======================================================================================================================================
*/
void trap_R_SetColor(const float *rgba) {
	syscall(UI_R_SETCOLOR, rgba);
}

/*
=======================================================================================================================================
trap_R_AddRefEntityToScene
=======================================================================================================================================
*/
void trap_R_AddRefEntityToScene(const refEntity_t *re) {
	syscall(UI_R_ADDREFENTITYTOSCENE, re, sizeof(refEntity_t));
}

/*
=======================================================================================================================================
trap_R_AddPolyRefEntityToScene
=======================================================================================================================================
*/
void trap_R_AddPolyRefEntityToScene(const refEntity_t *re, int numVerts, const polyVert_t *verts, int numPolys) {
	syscall(UI_R_ADDPOLYREFENTITYTOSCENE, re, sizeof(refEntity_t), numVerts, verts, numPolys);
}

/*
=======================================================================================================================================
trap_R_AddPolyToScene
=======================================================================================================================================
*/
void trap_R_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts, int bmodelNum, int sortLevel) {
	syscall(UI_R_ADDPOLYTOSCENE, hShader, numVerts, verts, bmodelNum, sortLevel);
}

/*
=======================================================================================================================================
trap_R_AddPolysToScene
=======================================================================================================================================
*/
void trap_R_AddPolysToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys, int bmodelNum, int sortLevel) {
	syscall(UI_R_ADDPOLYSTOSCENE, hShader, numVerts, verts, numPolys, bmodelNum, sortLevel);
}

/*
=======================================================================================================================================
trap_R_AddPolyBufferToScene
=======================================================================================================================================
*/
void trap_R_AddPolyBufferToScene(polyBuffer_t *pPolyBuffer) {
	syscall(UI_R_ADDPOLYBUFFERTOSCENE, pPolyBuffer);
}

/*
=======================================================================================================================================
trap_R_LightForPoint
=======================================================================================================================================
*/
int trap_R_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir) {
	return syscall(UI_R_LIGHTFORPOINT, point, ambientLight, directedLight, lightDir);
}

/*
=======================================================================================================================================
trap_R_AddLightToScene
=======================================================================================================================================
*/
void trap_R_AddLightToScene(const vec3_t org, float radius, float intensity, float r, float g, float b, qhandle_t hShader) {
	syscall(UI_R_ADDLIGHTTOSCENE, org, PASSFLOAT(radius), PASSFLOAT(intensity), PASSFLOAT(r), PASSFLOAT(g), PASSFLOAT(b), hShader);
}

/*
=======================================================================================================================================
trap_R_AddAdditiveLightToScene
=======================================================================================================================================
*/
void trap_R_AddAdditiveLightToScene(const vec3_t org, float radius, float intensity, float r, float g, float b) {
	syscall(UI_R_ADDADDITIVELIGHTTOSCENE, org, PASSFLOAT(radius), PASSFLOAT(intensity), PASSFLOAT(r), PASSFLOAT(g), PASSFLOAT(b));
}

/*
=======================================================================================================================================
trap_R_AddVertexLightToScene
=======================================================================================================================================
*/
void trap_R_AddVertexLightToScene(const vec3_t org, float radius, float intensity, float r, float g, float b) {
	syscall(UI_R_ADDVERTEXLIGHTTOSCENE, org, PASSFLOAT(radius), PASSFLOAT(intensity), PASSFLOAT(r), PASSFLOAT(g), PASSFLOAT(b));
}

/*
=======================================================================================================================================
trap_R_AddJuniorLightToScene
=======================================================================================================================================
*/
void trap_R_AddJuniorLightToScene(const vec3_t org, float radius, float intensity, float r, float g, float b) {
	syscall(UI_R_ADDJUNIORLIGHTTOSCENE, org, PASSFLOAT(radius), PASSFLOAT(intensity), PASSFLOAT(r), PASSFLOAT(g), PASSFLOAT(b));
}

/*
=======================================================================================================================================
trap_R_AddDirectedLightToScene
=======================================================================================================================================
*/
void trap_R_AddDirectedLightToScene(const vec3_t normal, float intensity, float r, float g, float b) {
	syscall(UI_R_ADDDIRECTEDLIGHTTOSCENE, normal, PASSFLOAT(intensity), PASSFLOAT(r), PASSFLOAT(g), PASSFLOAT(b));
}

/*
=======================================================================================================================================
trap_R_AddCoronaToScene
=======================================================================================================================================
*/
void trap_R_AddCoronaToScene(const vec3_t org, float r, float g, float b, float scale, int id, qboolean visible, qhandle_t hShader) {
	syscall(UI_R_ADDCORONATOSCENE, org, PASSFLOAT(r), PASSFLOAT(g), PASSFLOAT(b), PASSFLOAT(scale), id, visible, hShader);
}

/*
=======================================================================================================================================
trap_R_ModelBounds
=======================================================================================================================================
*/
int trap_R_ModelBounds(clipHandle_t model, vec3_t mins, vec3_t maxs, int startFrame, int endFrame, float frac) {
	return syscall(UI_R_MODELBOUNDS, model, mins, maxs, startFrame, endFrame, PASSFLOAT(frac));
}

/*
=======================================================================================================================================
trap_R_LerpTag
=======================================================================================================================================
*/
int trap_R_LerpTag(orientation_t *tag, clipHandle_t mod, int startFrame, int endFrame, float frac, const char *tagName) {
	return syscall(UI_R_LERPTAG, tag, mod, startFrame, endFrame, PASSFLOAT(frac), tagName);
}

/*
=======================================================================================================================================
trap_R_LerpTagFrameModel
=======================================================================================================================================
*/
int trap_R_LerpTagFrameModel(orientation_t *tag, clipHandle_t mod, clipHandle_t frameModel, int startFrame, clipHandle_t endFrameModel, int endFrame, float frac, const char *tagName, int *tagIndex) {
	return syscall(UI_R_LERPTAG_FRAMEMODEL, tag, mod, frameModel, startFrame, endFrameModel, endFrame, PASSFLOAT(frac), tagName, tagIndex);
}

/*
=======================================================================================================================================
trap_R_LerpTagTorso
=======================================================================================================================================
*/
int trap_R_LerpTagTorso(orientation_t *tag, clipHandle_t mod, clipHandle_t frameModel, int startFrame, clipHandle_t endFrameModel, int endFrame, float frac, const char *tagName, int *tagIndex, const vec3_t *torsoAxis, qhandle_t torsoFrameModel, int torsoFrame, qhandle_t oldTorsoFrameModel, int oldTorsoFrame, float torsoFrac) {
	return syscall(UI_R_LERPTAG_TORSO, tag, mod, frameModel, startFrame, endFrameModel, endFrame, PASSFLOAT(frac), tagName, tagIndex, torsoAxis, torsoFrameModel, torsoFrame, oldTorsoFrameModel, oldTorsoFrame, PASSFLOAT(torsoFrac));
}

/*
=======================================================================================================================================
trap_R_AllocSkinSurface
=======================================================================================================================================
*/
qhandle_t trap_R_AllocSkinSurface(const char *surface, qhandle_t hShader) {
	return syscall(UI_R_ALLOCSKINSURFACE, surface, hShader);
}

/*
=======================================================================================================================================
trap_R_AddSkinToFrame
=======================================================================================================================================
*/
qhandle_t trap_R_AddSkinToFrame(int numSurfaces, const qhandle_t *surfaces) {
	return syscall(UI_R_ADDSKINTOFRAME, numSurfaces, surfaces);
}

/*
=======================================================================================================================================
trap_R_SetClipRegion
=======================================================================================================================================
*/
void trap_R_SetClipRegion(const float *region) {
	syscall(UI_R_SETCLIPREGION, region);
}

/*
=======================================================================================================================================
trap_R_DrawStretchPic
=======================================================================================================================================
*/
void trap_R_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader) {
	syscall(UI_R_DRAWSTRETCHPIC, PASSFLOAT(x), PASSFLOAT(y), PASSFLOAT(w), PASSFLOAT(h), PASSFLOAT(s1), PASSFLOAT(t1), PASSFLOAT(s2), PASSFLOAT(t2), hShader);
}

/*
=======================================================================================================================================
trap_R_DrawStretchPicGradient
=======================================================================================================================================
*/
void trap_R_DrawStretchPicGradient(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, const float *gradientColor) {
	syscall(UI_R_DRAWSTRETCHPIC_GRADIENT, PASSFLOAT(x), PASSFLOAT(y), PASSFLOAT(w), PASSFLOAT(h), PASSFLOAT(s1), PASSFLOAT(t1), PASSFLOAT(s2), PASSFLOAT(t2), hShader, gradientColor);
}

/*
=======================================================================================================================================
trap_R_DrawRotatedPic
=======================================================================================================================================
*/
void trap_R_DrawRotatedPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, float angle) {
	syscall(UI_R_DRAWROTATEDPIC, PASSFLOAT(x), PASSFLOAT(y), PASSFLOAT(w), PASSFLOAT(h), PASSFLOAT(s1), PASSFLOAT(t1), PASSFLOAT(s2), PASSFLOAT(t2), hShader, PASSFLOAT(angle));
}

/*
=======================================================================================================================================
trap_R_Add2dPolys
=======================================================================================================================================
*/
void trap_R_Add2dPolys(polyVert_t *verts, int numverts, qhandle_t hShader) {
	syscall(UI_R_DRAW2DPOLYS, verts, numverts, hShader);
}

/*
=======================================================================================================================================
trap_R_RemapShader
=======================================================================================================================================
*/
void trap_R_RemapShader(const char *oldShader, const char *newShader, const char *timeOffset) {
	syscall(UI_R_REMAP_SHADER, oldShader, newShader, timeOffset);
}

/*
=======================================================================================================================================
trap_S_RegisterSound
=======================================================================================================================================
*/
sfxHandle_t trap_S_RegisterSound(const char *sample, qboolean compressed) {
	return syscall(UI_S_REGISTERSOUND, sample, compressed);
}

/*
=======================================================================================================================================
trap_S_StartLocalSound
=======================================================================================================================================
*/
void trap_S_StartLocalSound(sfxHandle_t sfx, int channelNum) {
	syscall(UI_S_STARTLOCALSOUND, sfx, channelNum);
}

/*
=======================================================================================================================================
trap_S_StartBackgroundTrack
=======================================================================================================================================
*/
void trap_S_StartBackgroundTrack(const char *intro, const char *loop) {
	syscall(UI_S_STARTBACKGROUNDTRACK, intro, loop);
}

/*
=======================================================================================================================================
trap_S_StopBackgroundTrack
=======================================================================================================================================
*/
void trap_S_StopBackgroundTrack(void) {
	syscall(UI_S_STOPBACKGROUNDTRACK);
}

/*
=======================================================================================================================================
trap_Key_GetCatcher
=======================================================================================================================================
*/
int trap_Key_GetCatcher(void) {
	return syscall(UI_KEY_GETCATCHER);
}

/*
=======================================================================================================================================
trap_Key_SetCatcher
=======================================================================================================================================
*/
void trap_Key_SetCatcher(int catcher) {
	syscall(UI_KEY_SETCATCHER, catcher);
}

/*
=======================================================================================================================================
trap_Key_IsDown
=======================================================================================================================================
*/
qboolean trap_Key_IsDown(int keynum) {
	return syscall(UI_KEY_ISDOWN, keynum);
}

/*
=======================================================================================================================================
trap_Key_KeynumToStringBuf
=======================================================================================================================================
*/
void trap_Key_KeynumToStringBuf(int keynum, char *buf, int buflen) {
	syscall(UI_KEY_KEYNUMTOSTRINGBUF, keynum, buf, buflen);
}

/*
=======================================================================================================================================
trap_Key_GetBindingBuf
=======================================================================================================================================
*/
void trap_Key_GetBindingBuf(int keynum, char *buf, int buflen) {
	syscall(UI_KEY_GETBINDINGBUF, keynum, buf, buflen);
}

/*
=======================================================================================================================================
trap_Key_SetBinding
=======================================================================================================================================
*/
void trap_Key_SetBinding(int keynum, const char *binding) {
	syscall(UI_KEY_SETBINDING, keynum, binding);
}

/*
=======================================================================================================================================
trap_Key_GetOverstrikeMode
=======================================================================================================================================
*/
qboolean trap_Key_GetOverstrikeMode(void) {
	return syscall(UI_KEY_GETOVERSTRIKEMODE);
}

/*
=======================================================================================================================================
trap_Key_SetOverstrikeMode
=======================================================================================================================================
*/
void trap_Key_SetOverstrikeMode(qboolean state) {
	syscall(UI_KEY_SETOVERSTRIKEMODE, state);
}

/*
=======================================================================================================================================
trap_Key_ClearStates
=======================================================================================================================================
*/
void trap_Key_ClearStates(void) {
	syscall(UI_KEY_CLEARSTATES);
}

/*
=======================================================================================================================================
trap_CIN_PlayCinematic

This returns a handle. arg0 is the name in the format "idlogo.roq", set arg1 to NULL, alteredstates to qfalse(do not alter gamestate).
=======================================================================================================================================
*/
int trap_CIN_PlayCinematic(const char *arg0, int xpos, int ypos, int width, int height, int bits) {
	return syscall(UI_CIN_PLAYCINEMATIC, arg0, xpos, ypos, width, height, bits);
}

/*
=======================================================================================================================================
trap_CIN_StopCinematic

Stops playing the cinematic and ends it.
Should always return FMV_EOF cinematics must be stopped in reverse order of when they are started.
=======================================================================================================================================
*/
e_status trap_CIN_StopCinematic(int handle) {
	return syscall(UI_CIN_STOPCINEMATIC, handle);
}

/*
=======================================================================================================================================
trap_CIN_RunCinematic

Will run a frame of the cinematic but will not draw it. Will return FMV_EOF if the end of the cinematic has been reached.
=======================================================================================================================================
*/
e_status trap_CIN_RunCinematic(int handle) {
	return syscall(UI_CIN_RUNCINEMATIC, handle);
}

/*
=======================================================================================================================================
trap_CIN_DrawCinematic

Draws the current frame.
=======================================================================================================================================
*/
void trap_CIN_DrawCinematic(int handle) {
	syscall(UI_CIN_DRAWCINEMATIC, handle);
}

/*
=======================================================================================================================================
trap_CIN_SetExtents

Allows you to resize the animation dynamically.
=======================================================================================================================================
*/
void trap_CIN_SetExtents(int handle, int x, int y, int w, int h) {
	syscall(UI_CIN_SETEXTENTS, handle, x, y, w, h);
}
