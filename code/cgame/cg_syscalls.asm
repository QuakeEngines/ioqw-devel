code

equ memset									-1
equ memcpy									-2
equ strncpy									-3
equ sin										-4
equ cos										-5
equ atan2									-6
equ sqrt									-7
equ floor									-8
equ ceil									-9
equ acos									-10

equ trap_Print								-21
equ trap_Error								-22
equ trap_Milliseconds						-23
equ trap_RealTime							-24
equ trap_SnapVector							-25
equ trap_Argc								-26
equ trap_Argv								-27
equ trap_Args								-28
equ trap_AddCommand							-29
equ trap_RemoveCommand						-30
equ trap_Cmd_ExecuteText					-31
equ trap_Cvar_Register						-32
equ trap_Cvar_Update						-33
equ trap_Cvar_Set							-34
equ trap_Cvar_SetValue						-35
equ trap_Cvar_VariableValue					-36
equ trap_Cvar_VariableIntegerValue			-37
equ trap_Cvar_VariableStringBuffer			-38
equ trap_FS_FOpenFile						-39
equ trap_FS_Read							-40
equ trap_FS_Write							-41
equ trap_FS_Seek							-42
equ trap_FS_FCloseFile						-43
equ trap_FS_GetFileList						-44
equ trap_PC_AddGlobalDefine					-45
equ trap_PC_LoadSource						-46
equ trap_PC_FreeSource						-47
equ trap_PC_ReadToken						-48
equ trap_PC_SourceFileAndLine				-49

equ trap_GetGlconfig						-101
equ trap_MemoryRemaining					-102
equ trap_UpdateScreen						-103

equ trap_GetGameState						-151
equ trap_GetCurrentSnapshotNumber			-152
equ trap_GetSnapshot						-153
equ trap_GetServerCommand					-154
equ trap_GetCurrentCmdNumber				-155
equ trap_GetUserCmd							-156
equ trap_SetUserCmdValue					-157
equ trap_SendClientCommand					-158

equ trap_CM_LoadMap							-201
equ trap_CM_NumInlineModels					-202
equ trap_CM_InlineModel						-203
equ trap_CM_MarkFragments					-204
equ trap_CM_PointContents					-205
equ trap_CM_TransformedPointContents		-206
equ trap_CM_TempBoxModel					-207
equ trap_CM_BoxTrace						-208
equ trap_CM_TransformedBoxTrace				-209
equ trap_CM_TempCapsuleModel				-210
equ trap_CM_CapsuleTrace					-211
equ trap_CM_TransformedCapsuleTrace			-212

equ trap_R_RegisterModel					-301
equ trap_R_RegisterShaderEx					-302
equ trap_R_RegisterShader					-303
equ trap_R_RegisterShaderNoMip				-304
equ trap_R_RegisterFont						-305
equ trap_R_RenderScene						-306
equ trap_R_ClearScene						-307
equ trap_R_SetColor							-308
equ trap_R_LoadWorldMap						-309
equ trap_R_inPVS							-310
equ trap_GetEntityToken						-311
equ trap_R_AddRefEntityToScene				-312
equ trap_R_AddPolyRefEntityToScene			-313
equ trap_R_AddPolyToScene					-314
equ trap_R_AddPolysToScene					-315
equ trap_R_AddPolyBufferToScene				-316
equ trap_R_LightForPoint					-317
equ trap_R_AddLightToScene					-318
equ trap_R_AddAdditiveLightToScene			-319
equ trap_R_AddVertexLightToScene			-320
equ trap_R_AddJuniorLightToScene			-321
equ trap_R_AddDirectedLightToScene			-322
equ trap_R_AddCoronaToScene					-323
equ trap_R_GetGlobalFog						-324
equ trap_R_GetViewFog						-325
equ trap_R_ModelBounds						-326
equ trap_R_LerpTag							-327
equ trap_R_LerpTagFrameModel				-328
equ trap_R_LerpTagTorso						-329
equ trap_R_AllocSkinSurface					-330
equ trap_R_AddSkinToFrame					-331
equ trap_R_SetClipRegion					-332
equ trap_R_DrawStretchPic					-333
equ trap_R_DrawStretchPicGradient			-334
equ trap_R_DrawRotatedPic					-335
equ trap_R_Add2dPolys						-336
equ trap_R_RemapShader						-337

equ trap_S_RegisterSound					-401
equ trap_S_StartLocalSound					-402
equ trap_S_StartBackgroundTrack				-403
equ trap_S_StopBackgroundTrack				-404
equ trap_S_StartSound						-405
equ trap_S_ClearLoopingSounds				-406
equ trap_S_AddLoopingSound					-407
equ trap_S_AddRealLoopingSound				-408
equ trap_S_StopLoopingSound					-409
equ trap_S_UpdateEntityPosition				-410
equ trap_S_Respatialize						-411

equ trap_Key_GetCatcher						-501
equ trap_Key_SetCatcher						-502
equ trap_Key_IsDown							-503
equ trap_Key_GetKey							-504

equ trap_CIN_PlayCinematic					-601
equ trap_CIN_StopCinematic					-602
equ trap_CIN_RunCinematic					-603
equ trap_CIN_DrawCinematic					-604
equ trap_CIN_SetExtents						-605
