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

#include "g_local.h"

#define POOLSIZE (256 * 1024)

static char memoryPool[POOLSIZE];
static int allocPoint;

/*
=======================================================================================================================================
G_Alloc
=======================================================================================================================================
*/
void *G_Alloc(int size) {
	char *p;

	if (g_debugAlloc.integer) {
		G_Printf("G_Alloc of %i bytes (%i left)\n", size, POOLSIZE - allocPoint - ((size + 31) & ~31));
	}

	if (allocPoint + size > POOLSIZE) {
		G_Error("G_Alloc: failed on allocation of %i bytes", size);
		return NULL;
	}

	p = &memoryPool[allocPoint];
	allocPoint += (size + 31) & ~31;
	return p;
}

/*
=======================================================================================================================================
G_InitMemory
=======================================================================================================================================
*/
void G_InitMemory(void) {
	allocPoint = 0;
}

/*
=======================================================================================================================================
Svcmd_GameMem_f
=======================================================================================================================================
*/
void Svcmd_GameMem_f(void) {
	G_Printf("Game memory status: %i out of %i bytes allocated\n", allocPoint, POOLSIZE);
}
