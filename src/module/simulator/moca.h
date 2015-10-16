/*
 * Copyright (C) 2015  Beniamine, David <David@Beniamine.net>
 * Author: Beniamine, David <David@Beniamine.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define __NO_VERSION__
#ifndef __MOCA__
#define __MOCA__

#ifdef MOCA_DEBUG
#define MOCA_DEBUG_PRINT(...) printk(KERN_DEBUG __VA_ARGS__)
#else
#define MOCA_DEBUG_PRINT(...)
#endif //MOCA_DEBUG
#define MAX(A,B) ((A)>(B) ? (A) : (B) )







// Panic exit function
void Moca_Panic(const char *s);
//Clocks managment
void Moca_UpdateClock(void);
long Moca_GetClock(void);
int Moca_IsActivated(void);


#endif //__MOCA__

