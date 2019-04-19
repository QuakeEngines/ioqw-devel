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
 Genetic selection.
**************************************************************************************************************************************/

#include "../qcommon/q_shared.h"
#include "l_memory.h"
#include "l_log.h"
#include "l_utils.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_ai_gen.h"

/*
=======================================================================================================================================
GeneticSelection
=======================================================================================================================================
*/
int GeneticSelection(int numranks, float *rankings) {
	float sum;
	int i, index;

	sum = 0;

	for (i = 0; i < numranks; i++) {
		if (rankings[i] < 0) {
			continue;
		}

		sum += rankings[i];
	}

	if (sum > 0) {
		// select a bot where the ones with the highest rankings have the highest chance of being selected
		//sum *= random();

		for (i = 0; i < numranks; i++) {
			if (rankings[i] < 0) {
				continue;
			}

			sum -= rankings[i];

			if (sum <= 0) {
				return i;
			}
		}
	}
	// select a bot randomly
	index = random() * numranks;

	for (i = 0; i < numranks; i++) {
		if (rankings[index] >= 0) {
			return index;
		}

		index = (index + 1) % numranks;
	}

	return 0;
}

/*
=======================================================================================================================================
GeneticParentsAndChildSelection
=======================================================================================================================================
*/
int GeneticParentsAndChildSelection(int numranks, float *ranks, int *parent1, int *parent2, int *child) {
	float rankings[256], max;
	int i;

	if (numranks > 256) {
		botimport.Print(PRT_WARNING, "GeneticParentsAndChildSelection: too many bots\n");
		*parent1 = *parent2 = *child = 0;
		return qfalse;
	}

	for (max = 0, i = 0; i < numranks; i++) {
		if (ranks[i] < 0) {
			continue;
		}

		max++;
	}

	if (max < 3) {
		botimport.Print(PRT_WARNING, "GeneticParentsAndChildSelection: too few valid bots\n");
		*parent1 = *parent2 = *child = 0;
		return qfalse;
	}

	Com_Memcpy(rankings, ranks, sizeof(float) * numranks);
	// select first parent
	*parent1 = GeneticSelection(numranks, rankings);
	rankings[*parent1] = -1;
	// select second parent
	*parent2 = GeneticSelection(numranks, rankings);
	rankings[*parent2] = -1;
	// reverse the rankings
	max = 0;

	for (i = 0; i < numranks; i++) {
		if (rankings[i] < 0) {
			continue;
		}

		if (rankings[i] > max) {
			max = rankings[i];
		}
	}

	for (i = 0; i < numranks; i++) {
		if (rankings[i] < 0) {
			continue;
		}

		rankings[i] = max - rankings[i];
	}
	// select child
	*child = GeneticSelection(numranks, rankings);
	return qtrue;
}
