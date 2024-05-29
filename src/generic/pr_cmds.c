/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.h"
#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>
#include <ctype.h>

#define PR_MAX_TEMPSTRING 2048	// 2001-10-25 Enhanced temp string handling by Maddes
#define	RETURN_EDICT(e) (((int *)pr_globals)[OFS_RETURN] = EDICT_TO_PROG(e))

/*
===============================================================================

						BUILT-IN FUNCTIONS

===============================================================================
*/

extern double time_wpad_off;
extern int rumble_on;
extern cvar_t  rumble;

char	pr_varstring_temp[PR_MAX_TEMPSTRING];	// 2001-10-25 Enhanced temp string handling by Maddes
char *PF_VarString (int	first)
{
	int		i;
// 2001-10-25 Enhanced temp string handling by Maddes  start
	int		maxlen;
	char	*add;

	pr_varstring_temp[0] = 0;
	for (i=first ; i < pr_argc ; i++)
	{
		maxlen = PR_MAX_TEMPSTRING - strlen(pr_varstring_temp) - 1;	// -1 is EndOfString
		add = G_STRING((OFS_PARM0+i*3));
		if (maxlen > strlen(add))
		{
			strcat (pr_varstring_temp, add);
		}
		else
		{
			strncat (pr_varstring_temp, add, maxlen);
			pr_varstring_temp[PR_MAX_TEMPSTRING-1] = 0;
			break;	// can stop here
		}
	}
	return pr_varstring_temp;
// 2001-10-25 Enhanced temp string handling by Maddes  end
}


/*
=================
PF_errror

This is a TERMINAL error, which will kill off the entire server.
Dumps self.

error(value)
=================
*/
void PF_error (void)
{
	char	*s;
	edict_t	*ed;
	
	s = PF_VarString(0);
	Con_Printf ("======SERVER ERROR in %s:\n%s\n"
	,pr_strings + pr_xfunction->s_name,s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print (ed);

	//Host_Error ("Program error");
}

/*
=================
PF_objerror

Dumps out self, then an error message.  The program is aborted and self is
removed, but the level can continue.

objerror(value)
=================
*/
void PF_objerror (void)
{
	char	*s;
	edict_t	*ed;
	
	s = PF_VarString(0);
	Con_Printf ("======OBJECT ERROR in %s:\n%s\n"
	,pr_strings + pr_xfunction->s_name,s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	//ED_Print (ed);
	ED_Free (ed);
	
	//Host_Error ("Program error");
}



/*
==============
PF_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
makevectors(vector)
==============
*/
void PF_makevectors (void)
{
	AngleVectors (G_VECTOR(OFS_PARM0), pr_global_struct->v_forward, pr_global_struct->v_right, pr_global_struct->v_up);
}

/*
=================
PF_setorigin

This is the only valid way to move an object without using the physics of the world (setting velocity and waiting).  Directly changing origin will not set internal links correctly, so clipping would be messed up.  This should be called when an object is spawned, and then only if it is teleported.

setorigin (entity, origin)
=================
*/
void PF_setorigin (void)
{
	edict_t	*e;
	float	*org;
	
	e = G_EDICT(OFS_PARM0);
	org = G_VECTOR(OFS_PARM1);
	VectorCopy (org, e->v.origin);
	SV_LinkEdict (e, FALSE);
}


void SetMinMaxSize (edict_t *e, float *min, float *max, qboolean rotate)
{
	float	*angles;
	vec3_t	rmin, rmax;
	float	bounds[2][3];
	float	xvector[2], yvector[2];
	float	a;
	vec3_t	base, transformed;
	int		i, j, k, l;
	
	for (i=0 ; i<3 ; i++)
		if (min[i] > max[i])
			PR_RunError ("backwards mins/maxs");

	rotate = FALSE;		// FIXME: implement rotation properly again

	if (!rotate)
	{
		VectorCopy (min, rmin);
		VectorCopy (max, rmax);
	}
	else
	{
		float s;
		float c;

	// find min / max for rotations
		angles = e->v.angles;
		
		a = angles[1]/180 * Q_PI;
		s = sinf(a);
		c = cosf(a);
		
		xvector[0] = c;
		xvector[1] = s;
		yvector[0] = -s;
		yvector[1] = c;
		
		VectorCopy (min, bounds[0]);
		VectorCopy (max, bounds[1]);
		
		rmin[0] = rmin[1] = rmin[2] = 9999;
		rmax[0] = rmax[1] = rmax[2] = -9999;
		
		for (i=0 ; i<= 1 ; i++)
		{
			base[0] = bounds[i][0];
			for (j=0 ; j<= 1 ; j++)
			{
				base[1] = bounds[j][1];
				for (k=0 ; k<= 1 ; k++)
				{
					base[2] = bounds[k][2];
					
				// transform the point
					transformed[0] = xvector[0]*base[0] + yvector[0]*base[1];
					transformed[1] = xvector[1]*base[0] + yvector[1]*base[1];
					transformed[2] = base[2];
					
					for (l=0 ; l<3 ; l++)
					{
						if (transformed[l] < rmin[l])
							rmin[l] = transformed[l];
						if (transformed[l] > rmax[l])
							rmax[l] = transformed[l];
					}
				}
			}
		}
	}
	
// set derived values
	VectorCopy (rmin, e->v.mins);
	VectorCopy (rmax, e->v.maxs);
	VectorSubtract (max, min, e->v.size);
	
	SV_LinkEdict (e, FALSE);
}

/*
=================
PF_setsize

the size box is rotated by the current angle

setsize (entity, minvector, maxvector)
=================
*/
void PF_setsize (void)
{
	edict_t	*e;
	float	*min, *max;
	
	e = G_EDICT(OFS_PARM0);
	min = G_VECTOR(OFS_PARM1);
	max = G_VECTOR(OFS_PARM2);
	SetMinMaxSize (e, min, max, FALSE);
}


/*
=================
PF_setmodel

setmodel(entity, model)
=================
*/
void PF_setmodel (void)
{
	edict_t	*e;
	char	*m, **check;
	model_t	*mod;
	int		i;

	e = G_EDICT(OFS_PARM0);
	m = G_STRING(OFS_PARM1);

// check to see if model was properly precached
	for (i=0, check = sv.model_precache ; *check ; i++, check++)
		if (!strcmp(*check, m))
			break;
			
	if (!*check)
		PR_RunError ("no precache: %s\n", m);
		

	e->v.model = m - pr_strings;
	e->v.modelindex = i; //SV_ModelIndex (m);

	mod = sv.models[ (int)e->v.modelindex];  // Mod_ForName (m, TRUE);
	
	if (mod)
		SetMinMaxSize (e, mod->mins, mod->maxs, TRUE);
	else
		SetMinMaxSize (e, vec3_origin, vec3_origin, TRUE);
}

/*
=================
PF_bprint

broadcast print to everyone on server

bprint(style, value)
=================
*/
void PF_bprint (void)
{
	// 
	float style = G_FLOAT(OFS_PARM0);
	char *s = PF_VarString(1);
	SV_BroadcastPrintf ("%s", s);
}

/*
=================
PF_sprint

single print to a specific client

sprint(clientent, value)
=================
*/
void PF_sprint (void)
{
	char		*s;
	client_t	*client;
	int			entnum;
	
	entnum = G_EDICTNUM(OFS_PARM0);
	s = PF_VarString(1);
	
	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}
		
	client = &svs.clients[entnum-1];
		
	MSG_WriteChar (&client->message,svc_print);
	MSG_WriteString (&client->message, s );
}


/*
=================
PF_centerprint

single print to a specific client

centerprint(clientent, value)
=================
*/
void PF_centerprint (void)
{
	char		*s;
	client_t	*client;
	int			entnum;
	
	entnum = G_EDICTNUM(OFS_PARM0);
	s = PF_VarString(1);
	
	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}
		
	client = &svs.clients[entnum-1];
		
	MSG_WriteChar (&client->message,svc_centerprint);
	MSG_WriteString (&client->message, s );
}


/*
=================
PF_normalize

vector normalize(vector)
=================
*/
void PF_normalize (void)
{
	float	*value1;
	vec3_t	newvalue;
	float	new;
	
	value1 = G_VECTOR(OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
	new = sqrtf(new);
	
	if (new == 0)
		newvalue[0] = newvalue[1] = newvalue[2] = 0;
	else
	{
		new = 1/new;
		newvalue[0] = value1[0] * new;
		newvalue[1] = value1[1] * new;
		newvalue[2] = value1[2] * new;
	}
	
	VectorCopy (newvalue, G_VECTOR(OFS_RETURN));	
}

/*
=================
PF_useprint

Print a text depending on what it is fed with

useprint(entity client, float type, float cost, float weapon)
=================
*/
void PF_useprint (void)
{
	client_t	*client;
	int			entnum, type, cost, weapon;

	entnum = G_EDICTNUM(OFS_PARM0);
	type = G_FLOAT(OFS_PARM1);
	cost = G_FLOAT(OFS_PARM2);
	weapon = G_FLOAT(OFS_PARM3);


	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum-1];

	MSG_WriteByte (&client->message,svc_useprint);
	MSG_WriteByte (&client->message,type);
	MSG_WriteShort (&client->message,cost);
	MSG_WriteByte (&client->message,weapon);
	//MSG_WriteString (&client->message, s );
}

/*
=================
PF_vlen

scalar vlen(vector)
=================
*/
void PF_vlen (void)
{
	float	*value1;
	float	new;
	
	value1 = G_VECTOR(OFS_PARM0);

	new = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
	new = sqrtf(new);
	
	G_FLOAT(OFS_RETURN) = new;
}

/*
=================
PF_vectoyaw

float vectoyaw(vector)
=================
*/
void PF_vectoyaw (void)
{
	float	*value1;
	float	yaw;
	
	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
		yaw = 0;
	else
	{
		yaw = (int) (atan2f(value1[1], value1[0]) * 180 / Q_PI);
		if (yaw < 0)
			yaw += 360;
	}

	G_FLOAT(OFS_RETURN) = yaw;
}


/*
=================
PF_vectoangles

vector vectoangles(vector)
=================
*/
void PF_vectoangles (void)
{
	float	*value1;
	float	forward;
	float	yaw, pitch;
	
	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (int) (atan2f(value1[1], value1[0]) * 180 / Q_PI);
		if (yaw < 0)
			yaw += 360;

		forward = sqrtf (value1[0]*value1[0] + value1[1]*value1[1]);
		pitch = (int) (atan2f(value1[2], forward) * 180 / Q_PI);
		if (pitch < 0)
			pitch += 360;
	}

	G_FLOAT(OFS_RETURN+0) = pitch;
	G_FLOAT(OFS_RETURN+1) = yaw;
	G_FLOAT(OFS_RETURN+2) = 0;
}

/*
=================
PF_Random

Returns a number from 0<= num < 1

random()
=================
*/
void PF_random (void)
{
	float		num;
		
	num = (rand ()&0x7fff) / ((float)0x7fff);
	
	G_FLOAT(OFS_RETURN) = num;
}

/*
=================
PF_particle

particle(origin, color, count)
=================
*/
void PF_particle (void)
{
	float		*org, *dir;
	float		color;
	float		count;
			
	org = G_VECTOR(OFS_PARM0);
	dir = G_VECTOR(OFS_PARM1);
	color = G_FLOAT(OFS_PARM2);
	count = G_FLOAT(OFS_PARM3);
	SV_StartParticle (org, dir, color, count);
}


/*
=================
PF_ambientsound

=================
*/
void PF_ambientsound (void)
{
	char		**check;
	char		*samp;
	float		*pos;
	float 		vol, attenuation;
	int			i, soundnum;

	pos = G_VECTOR (OFS_PARM0);			
	samp = G_STRING(OFS_PARM1);
	vol = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);
	
// check to see if samp was properly precached
	for (soundnum=0, check = sv.sound_precache ; *check ; check++, soundnum++)
		if (!strcmp(*check,samp))
			break;
			
	if (!*check)
	{
		Con_Printf ("no precache: %s\n", samp);
		return;
	}

// add an svc_spawnambient command to the level signon packet

	MSG_WriteByte (&sv.signon,svc_spawnstaticsound);
	for (i=0 ; i<3 ; i++)
		MSG_WriteCoord(&sv.signon, pos[i]);

	MSG_WriteByte (&sv.signon, soundnum);

	MSG_WriteByte (&sv.signon, vol*255);
	MSG_WriteByte (&sv.signon, attenuation*64);

}

/*
=================
PF_sound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
allready running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.

=================
*/
void PF_sound (void)
{
	char		*sample;
	int			channel;
	edict_t		*entity;
	int 		volume;
	float attenuation;
		
	entity = G_EDICT(OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = G_STRING(OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3) * 255;
	attenuation = G_FLOAT(OFS_PARM4);
	
	if (volume < 0 || volume > 255)
		Sys_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Sys_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 7)
		Sys_Error ("SV_StartSound: channel = %i", channel);

	SV_StartSound (entity, channel, sample, volume, attenuation);
}

/*
=================
PF_break

break()
=================
*/
void PF_break (void)
{
Con_Printf ("break statement\n");
*(int *)-4 = 0;	// dump to debugger
//	PR_RunError ("break statement");
}

/*
=================
PF_traceline

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

traceline (vector1, vector2, tryents)
=================
*/
void PF_traceline (void)
{
	float	*v1, *v2;
	trace_t	trace;
	int		nomonsters;
	edict_t	*ent;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	nomonsters = G_FLOAT(OFS_PARM2);
	ent = G_EDICT(OFS_PARM3);

	trace = SV_Move (v1, vec3_origin, vec3_origin, v2, nomonsters, ent);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist =  trace.plane.dist;	
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}


#ifdef QUAKE2
extern trace_t SV_Trace_Toss (edict_t *ent, edict_t *ignore);

void PF_TraceToss (void)
{
	trace_t	trace;
	edict_t	*ent;
	edict_t	*ignore;

	ent = G_EDICT(OFS_PARM0);
	ignore = G_EDICT(OFS_PARM1);

	trace = SV_Trace_Toss (ent, ignore);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist =  trace.plane.dist;	
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}
#endif

int TraceMove(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *ent)//engine-sides
{
	if(start[0] == end[0] && start[1] == end[1] && start[2] == end[2])
	{
		return 1;
	}
	vec3_t forward, up;
	float HorDist;
	vec3_t HorGoal;
	vec3_t tempHorGoal;

	up[0] = 0; up[1] = 0; up[2] = 1;
	HorGoal[0] = end[0]; HorGoal[1] = end[1]; HorGoal[2] = start[2];

	VectorSubtract(HorGoal,start,forward);
	HorDist = VectorLength(forward);
	VectorNormalize(forward);

	vec3_t CurrentPos;

	VectorCopy(start,CurrentPos);
	VectorCopy(HorGoal,tempHorGoal);

	float CurrentDist = 0;//2d distance from initial 3d positionvector

	trace_t trace1, trace2;
	float tempDist;
	vec3_t tempVec;
	vec3_t tempVec2;
	float i;
	int STEPSIZEB = 18;//other declaration isn't declared yet
	float SLOPELEN = 10.4;//18/tan(60) = 10.4, the the length of the triangle formed by the max walkable slope of 60 degrees.
 	int skip = 0;
	int LoopBreak = 0;

	while(CurrentDist < HorDist)
	{
		if(LoopBreak > 20)//was 50, decreased this quite a bit. now it's 260 meters
		{
			//Con_Printf("AI Warning: There is a ledge that is greater than 650 meters.\n");
			return -1;
		}

		trace1 = SV_Move(CurrentPos, mins, maxs, tempHorGoal, MOVE_NOMONSTERS, ent);

		VectorSubtract(tempHorGoal,CurrentPos,tempVec);
		tempDist = trace1.fraction * VectorLength(tempVec);
		//Check if we fell along the path
		for(i = (maxs[0] * 1); i < tempDist; i += (maxs[0] * 1))
		{
			VectorScale(forward,i,tempVec);
			VectorAdd(tempVec,CurrentPos,tempVec);
			VectorScale(up,-500,tempVec2);//500 inches is about 13 meters
			VectorAdd(tempVec,tempVec2,tempVec2);
			trace2 = SV_Move(tempVec, mins, maxs, tempVec2, MOVE_NOMONSTERS, ent);
			if(trace2.fraction > 0)
			{
				VectorScale(up,trace2.fraction * -100,tempVec2);
				VectorAdd(tempVec,tempVec2,CurrentPos);
				VectorAdd(tempHorGoal,tempVec2,tempHorGoal);
				skip = 1;
				CurrentDist += i;
				if(trace2.fraction == 1)
				{
					//We fell the full 13 meters!, we need to be careful here,
					//because if we're checking over the void, then we could be stuck in an infinite loop and crash the game
					//So we're going to keep track of how many times we fall 13 meters
					LoopBreak++;
				}
				else
				{
					LoopBreak = 0;
				}
				break;
			}
		}
		//If we fell at any location along path, then we don't try to step up
		if(skip == 1)
		{
			trace2.fraction = 0;
			skip = 0;
			continue;
		}
		//We need to advance it as much as possible along path before step up
		if(trace1.fraction > 0 && trace1.fraction < 1)
		{
			VectorCopy(trace1.endpos,CurrentPos);
			trace1.fraction = 0;
		}
		//Check step up
		if(trace1.fraction < 1)
		{
			VectorScale(up,STEPSIZEB,tempVec2);
			VectorAdd(CurrentPos,tempVec2,tempVec);
			VectorAdd(tempHorGoal,tempVec2,tempVec2);
			trace2 = SV_Move(tempVec, mins, maxs, tempVec2, MOVE_NOMONSTERS, ent);
			//10.4 is minimum length for a slope of 60 degrees, we need to at least advance this much to know the surface is walkable
			VectorSubtract(tempVec2,tempVec,tempVec2);
			if(trace2.fraction > (trace1.fraction + (SLOPELEN/VectorLength(tempVec2))) || trace2.fraction == 1)
			{
				VectorCopy(tempVec,CurrentPos);
				tempHorGoal[2] = CurrentPos[2];
				continue;
			}
			else
			{
				return 0;//stepping up didn't advance so we've hit a wall, we failed
			}
		}
		if(trace1.fraction == 1)//we've made it horizontally to our goal... so check if we've made it vertically...
		{
			if((end[2] - tempHorGoal[2] < STEPSIZEB) && (end[2] - tempHorGoal[2]) > -1 * STEPSIZEB)
				return 1;
			else return 0;
		}
	}
	return 0;
}

void PF_tracemove(void)//progs side
{
	float   *start, *end, *mins, *maxs;
	int      nomonsters;
	edict_t   *ent;

	start = G_VECTOR(OFS_PARM0);
	mins = G_VECTOR(OFS_PARM1);
	maxs = G_VECTOR(OFS_PARM2);
	end = G_VECTOR(OFS_PARM3);
	nomonsters = G_FLOAT(OFS_PARM4);
	ent = G_EDICT(OFS_PARM5);

	Con_DPrintf ("TraceMove start, ");
	G_INT(OFS_RETURN) = TraceMove(start, mins, maxs, end,nomonsters,ent);
	Con_DPrintf ("TM end\n");
	return;
}

void PF_tracebox (void)
{
   float   *v1, *v2, *mins, *maxs;
   trace_t   trace;
   int      nomonsters;
   edict_t   *ent;

   v1 = G_VECTOR(OFS_PARM0);
   mins = G_VECTOR(OFS_PARM1);
   maxs = G_VECTOR(OFS_PARM2);
   v2 = G_VECTOR(OFS_PARM3);
   nomonsters = G_FLOAT(OFS_PARM4);
   ent = G_EDICT(OFS_PARM5);

   trace = SV_Move (v1, mins, maxs, v2, nomonsters, ent);

   pr_global_struct->trace_allsolid = trace.allsolid;
   pr_global_struct->trace_startsolid = trace.startsolid;
   pr_global_struct->trace_fraction = trace.fraction;
   pr_global_struct->trace_inwater = trace.inwater;
   pr_global_struct->trace_inopen = trace.inopen;
   VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
   VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
   pr_global_struct->trace_plane_dist =  trace.plane.dist;
   if (trace.ent)
      pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
   else
      pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}

/*
=================
PF_checkpos

Returns TRUE if the given entity can move to the given position from it's
current position by walking or rolling.
FIXME: make work...
scalar checkpos (entity, vector)
=================
*/
void PF_checkpos (void)
{
}

//============================================================================

byte	checkpvs[MAX_MAP_LEAFS/8];

int PF_newcheckclient (int check)
{
	int		i;
	byte	*pvs;
	edict_t	*ent;
	mleaf_t	*leaf;
	vec3_t	org;

// cycle to the next one

	if (check < 1)
		check = 1;
	if (check > svs.maxclients)
		check = svs.maxclients;

	if (check == svs.maxclients)
		i = 1;
	else
		i = check + 1;

	for ( ;  ; i++)
	{
		if (i == svs.maxclients+1)
			i = 1;

		ent = EDICT_NUM(i);

		if (i == check)
			break;	// didn't find anything else

		if (ent->free)
			continue;
		if (ent->v.health <= 0)
			continue;
		if ((int)ent->v.flags & FL_NOTARGET)
			continue;

	// anything that is a client, or has a client as an enemy
		break;
	}

// get the PVS for the entity
	VectorAdd (ent->v.origin, ent->v.view_ofs, org);
	leaf = Mod_PointInLeaf (org, sv.worldmodel);
	pvs = Mod_LeafPVS (leaf, sv.worldmodel);
	memcpy (checkpvs, pvs, (sv.worldmodel->numleafs+7)>>3 );

	return i;
}

/*
=================
PF_checkclient

Returns a client (or object that has a client enemy) that would be a
valid target.

If there are more than one valid options, they are cycled each frame

If (self.origin + self.viewofs) is not in the PVS of the current target,
it is not returned at all.

name checkclient ()
=================
*/
#define	MAX_CHECK	16
int c_invis, c_notvis;
void PF_checkclient (void)
{
	edict_t	*ent, *self;
	mleaf_t	*leaf;
	int		l;
	vec3_t	view;
	
// find a new check if on a new frame
	if (sv.time - sv.lastchecktime >= 0.1f)
	{
		sv.lastcheck = PF_newcheckclient (sv.lastcheck);
		sv.lastchecktime = sv.time;
	}

// return check if it might be visible	
	ent = EDICT_NUM(sv.lastcheck);
	if (ent->free || ent->v.health <= 0)
	{
		RETURN_EDICT(sv.edicts);
		return;
	}

// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT(pr_global_struct->self);
	VectorAdd (self->v.origin, self->v.view_ofs, view);
	leaf = Mod_PointInLeaf (view, sv.worldmodel);
	l = (leaf - sv.worldmodel->leafs) - 1;
	if ( (l<0) || !(checkpvs[l>>3] & (1<<(l&7)) ) )
	{
c_notvis++;
		RETURN_EDICT(sv.edicts);
		return;
	}

// might be able to see it
c_invis++;
	RETURN_EDICT(ent);
}

//============================================================================


/*
=================
PF_stuffcmd

Sends text over to the client's execution buffer

stuffcmd (clientent, value)
=================
*/
void PF_stuffcmd (void)
{
	int		entnum;
	char	*str;
	client_t	*old;
	
	entnum = G_EDICTNUM(OFS_PARM0);
	if (entnum < 1 || entnum > svs.maxclients)
		PR_RunError ("Parm 0 not a client");
	str = G_STRING(OFS_PARM1);	
	
	old = host_client;
	host_client = &svs.clients[entnum-1];
	Host_ClientCommands ("%s", str);
	host_client = old;
}

/*
=================
PF_localcmd

Sends text over to the client's execution buffer

localcmd (string)
=================
*/
void PF_localcmd (void)
{
	char	*str;
	
	str = G_STRING(OFS_PARM0);	
	Cbuf_AddText (str);
}

/*
=================
PF_cvar

float cvar (string)
=================
*/
void PF_cvar (void)
{
	char	*str;
	
	str = G_STRING(OFS_PARM0);
	
	G_FLOAT(OFS_RETURN) = Cvar_VariableValue (str);
}

/*
=================
PF_cvar_set

float cvar (string)
=================
*/
void PF_cvar_set (void)
{
	char	*var, *val;
	
	var = G_STRING(OFS_PARM0);
	val = G_STRING(OFS_PARM1);
	
	Cvar_Set (var, val);
}

/*
=================
PF_findradius

Returns a chain of entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
void PF_findradius (void)
{
	edict_t	*ent, *chain;
	float	rad;
	float	*org;
	vec3_t	eorg;
	int		i, j;

	chain = (edict_t *)sv.edicts;
	
	org = G_VECTOR(OFS_PARM0);
	rad = G_FLOAT(OFS_PARM1);

	ent = NEXT_EDICT(sv.edicts);
	for (i=1 ; i<sv.num_edicts ; i++, ent = NEXT_EDICT(ent))
	{
		if (ent->free)
			continue;
		if (ent->v.solid == SOLID_NOT)
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (ent->v.origin[j] + (ent->v.mins[j] + ent->v.maxs[j])*0.5f);			
		if (Length(eorg) > rad)
			continue;
			
		ent->v.chain = EDICT_TO_PROG(chain);
		chain = ent;
	}

	RETURN_EDICT(chain);
}


/*
=========
PF_dprint
=========
*/
void PF_dprint (void)
{
	Con_DPrintf ("%s",PF_VarString(0));
}

char	pr_string_temp[128];

void PF_ftos (void)
{
	float	v;
	v = G_FLOAT(OFS_PARM0);
	
	if (v == (int)v)
		sprintf (pr_string_temp, "%d",(int)v);
	else
		sprintf (pr_string_temp, "%5.1f",v);
	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

void PF_fabs (void)
{
	float	v;
	v = G_FLOAT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = fabsf(v);
}

void PF_vtos (void)
{
	sprintf (pr_string_temp, "'%5.1f %5.1f %5.1f'", G_VECTOR(OFS_PARM0)[0], G_VECTOR(OFS_PARM0)[1], G_VECTOR(OFS_PARM0)[2]);
	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

void PF_etos (void)
{
	sprintf (pr_string_temp, "entity %i", G_EDICTNUM(OFS_PARM0));
	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

/*
=================
PF_stof

float stof (string)
=================
*/
// thanks Zoid, taken from QuakeWorld
void PF_stof (void)
{
	char	*s;

	s = G_STRING(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = atof(s);
}

/*
=================
PF_stov

vector stov (string)
=================
*/
void PF_stov (void)
{
	char *v;
	int i;
	vec3_t d;

	v = G_STRING(OFS_PARM0);

	for (i=0; i<3; i++)
	{
		while(v && (v[0] == ' ' || v[0] == '\'')) //skip unneeded data
			v++;
		d[i] = atof(v);
		while (v && v[0] != ' ') // skip to next space
			v++;
	}
	VectorCopy (d, G_VECTOR(OFS_RETURN));
}

// 2001-09-20 QuakeC string manipulation by FrikaC/Maddes  start
/*
=================
PF_strzone

string strzone (string)
=================
*/
void PF_strzone (void)
{
	char *m, *p;
	m = G_STRING(OFS_PARM0);
	p = Z_Malloc(strlen(m) + 1);
	strcpy(p, m);

	G_INT(OFS_RETURN) = p - pr_strings;
}

/*
=================
PF_strunzone

string strunzone (string)
=================
*/
void PF_strunzone (void)
{
	Z_Free(G_STRING(OFS_PARM0));
	G_INT(OFS_PARM0) = OFS_NULL; // empty the def
};

/*
=================
PF_strtrim

string strtrim (string)
=================
*/
void PF_strtrim (void)
{
		int		offset, length;
	int		maxoffset;		// 2001-10-25 Enhanced temp string handling by Maddes
	char	*str;
	char 	*end;

	str = G_STRING(OFS_PARM0);

	// figure out the new start
	while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
		offset++;
		str++;
	}

	// figure out the new end.
	end = str + strlen (str);
	while (end > str && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r'))
		end--;

	length = end - str;

	if (offset < 0)
		offset = 0;
// 2001-10-25 Enhanced temp string handling by Maddes  start
	if (length >= PR_MAX_TEMPSTRING)
		length = PR_MAX_TEMPSTRING-1;
// 2001-10-25 Enhanced temp string handling by Maddes  end
	if (length < 0)
		length = 0;

	//str += offset;
	strncpy(pr_string_temp, str, length);
	pr_string_temp[length] = 0;

	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
};

/*
=================
PF_strlen

float strlen (string)
=================
*/
void PF_strlen (void)
{
	char *p = G_STRING(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = strlen(p);
}

/*
=================
PF_strcat

string strcat (string, string)
=================
*/

void PF_strcat (void)
{
	char *s1, *s2;
	int		maxlen;	// 2001-10-25 Enhanced temp string handling by Maddes

	s1 = G_STRING(OFS_PARM0);
	s2 = PF_VarString(1);

// 2001-10-25 Enhanced temp string handling by Maddes  start
	pr_string_temp[0] = 0;
	if (strlen(s1) < PR_MAX_TEMPSTRING)
	{
		strcpy(pr_string_temp, s1);
	}
	else
	{
		strncpy(pr_string_temp, s1, PR_MAX_TEMPSTRING);
		pr_string_temp[PR_MAX_TEMPSTRING-1] = 0;
	}

	maxlen = PR_MAX_TEMPSTRING - strlen(pr_string_temp) - 1;	// -1 is EndOfString
	if (maxlen > 0)
	{
		if (maxlen > strlen(s2))
		{
			strcat (pr_string_temp, s2);
		}
		else
		{
			strncat (pr_string_temp, s2, maxlen);
			pr_string_temp[PR_MAX_TEMPSTRING-1] = 0;
		}
	}
// 2001-10-25 Enhanced temp string handling by Maddes  end

	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

/*
=================
PF_strtolower

string strtolower (string)
=================
*/
void PF_strtolower(void)
{
	char *s;

	s = G_STRING(OFS_PARM0);

	pr_string_temp[0] = 0;
	if (strlen(s) < PR_MAX_TEMPSTRING)
	{
		strcpy(pr_string_temp, s);
	}
	else
	{
		strncpy(pr_string_temp, s, PR_MAX_TEMPSTRING);
		pr_string_temp[PR_MAX_TEMPSTRING-1] = 0;
	}

	for(int i = 0; i < strlen(s); i++)
  		pr_string_temp[i] = tolower(pr_string_temp[i]);

	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

/*
=================
PF_crc16

float crc16 (float, string)
=================
*/
void PF_crc16(void)
{
	int insens = G_FLOAT(OFS_PARM0);
	char *s = G_STRING(OFS_PARM1);

	G_FLOAT(OFS_RETURN) = (unsigned short) ((insens ? CRC_Block_CaseInsensitive : CRC_Block) ((unsigned char *) s, strlen(s)));
}

/*
=================
PF_substring

string substring (string, float, float)
=================
*/
void PF_substring (void)
{
	int		offset, length;
	int		maxoffset;		// 2001-10-25 Enhanced temp string handling by Maddes
	char	*p;

	p = G_STRING(OFS_PARM0);
	offset = (int)G_FLOAT(OFS_PARM1); // for some reason, Quake doesn't like G_INT
	length = (int)G_FLOAT(OFS_PARM2);

	// cap values
	maxoffset = strlen(p);
	if (offset > maxoffset)
	{
		offset = maxoffset;
	}
	if (offset < 0)
		offset = 0;
// 2001-10-25 Enhanced temp string handling by Maddes  start
	if (length >= PR_MAX_TEMPSTRING)
		length = PR_MAX_TEMPSTRING-1;
// 2001-10-25 Enhanced temp string handling by Maddes  end
	if (length < 0)
		length = 0;

	p += offset;
	strncpy(pr_string_temp, p, length);
	pr_string_temp[length]=0;

	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

void PF_Spawn (void)
{
	edict_t	*ed;
	ed = ED_Alloc();
	RETURN_EDICT(ed);
}

void PF_Remove (void)
{
	edict_t	*ed;
	
	ed = G_EDICT(OFS_PARM0);
	ED_Free (ed);
}

// 2001-09-20 QuakeC file access by FrikaC/Maddes  start
/*
=================
PF_fopen

float fopen (string,float)
=================
*/
void PF_fopen (void)
{
	char *p = G_STRING(OFS_PARM0);
	char *ftemp;
	int fmode = G_FLOAT(OFS_PARM1);
	int h = 0, fsize = 0;

	switch (fmode)
	{
		case 0: // read
			Sys_FileOpenRead (va("%s/%s",com_gamedir, p), &h);
			G_FLOAT(OFS_RETURN) = (float) h;
			return;
		case 1: // append -- this is nasty
			// copy whole file into the zone
			fsize = Sys_FileOpenRead(va("%s/%s",com_gamedir, p), &h);
			if (h == -1)
			{
				h = Sys_FileOpenWrite(va("%s/%s",com_gamedir, p));
				G_FLOAT(OFS_RETURN) = (float) h;
				return;
			}
			ftemp = Z_Malloc(fsize + 1);
			Sys_FileRead(h, ftemp, fsize);
			Sys_FileClose(h);
			// spit it back out
			h = Sys_FileOpenWrite(va("%s/%s",com_gamedir, p));
			Sys_FileWrite(h, ftemp, fsize);
			Z_Free(ftemp); // free it from memory
			G_FLOAT(OFS_RETURN) = (float) h;  // return still open handle
			return;
		default: // write
			h = Sys_FileOpenWrite (va("%s/%s", com_gamedir, p));
			G_FLOAT(OFS_RETURN) = (float) h;
			return;
	}
}

/*
=================
PF_fclose

void fclose (float)
=================
*/
void PF_fclose (void)
{
	int h = (int)G_FLOAT(OFS_PARM0);
	Sys_FileClose(h);
}

/*
=================
PF_fgets

string fgets (float)
=================
*/
void PF_fgets (void)
{
	// reads one line (up to a \n) into a string
	int		h;
	int		i;
	int		count;
	char	buffer;

	h = (int)G_FLOAT(OFS_PARM0);

	count = Sys_FileRead(h, &buffer, 1);
	if (count && buffer == '\r')	// carriage return
	{
		count = Sys_FileRead(h, &buffer, 1);	// skip
	}
	if (!count)	// EndOfFile
	{
		G_INT(OFS_RETURN) = OFS_NULL;	// void string
		return;
	}

	i = 0;
	while (count && buffer != '\n')
	{
		if (i < PR_MAX_TEMPSTRING-1)	// no place for character in temp string
		{
			pr_string_temp[i++] = buffer;
		}

		// read next character
		count = Sys_FileRead(h, &buffer, 1);
		if (count && buffer == '\r')	// carriage return
		{
			count = Sys_FileRead(h, &buffer, 1);	// skip
		}
	};
	pr_string_temp[i] = 0;

	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
}

/*
=================
PF_fputs

void fputs (float,string)
=================
*/
void PF_fputs (void)
{
	// writes to file, like bprint
	float handle = G_FLOAT(OFS_PARM0);
	char *str = PF_VarString(1);
	Sys_FileWrite (handle, str, strlen(str));
}
// 2001-09-20 QuakeC file access by FrikaC/Maddes  end

// entity (entity start, .string field, string match) find = #5;
void PF_Find (void)
#ifdef QUAKE2
{
	int		e;	
	int		f;
	char	*s, *t;
	edict_t	*ed;
	edict_t	*first;
	edict_t	*second;
	edict_t	*last;

	first = second = last = (edict_t *)sv.edicts;
	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = G_STRING(OFS_PARM2);
	if (!s)
		PR_RunError ("PF_Find: bad search string");
		
	for (e++ ; e < sv.num_edicts ; e++)
	{
		ed = EDICT_NUM(e);
		if (ed->free)
			continue;
		t = E_STRING(ed,f);
		if (!t)
			continue;
		if (!strcmp(t,s))
		{
			if (first == (edict_t *)sv.edicts)
				first = ed;
			else if (second == (edict_t *)sv.edicts)
				second = ed;
			ed->v.chain = EDICT_TO_PROG(last);
			last = ed;
		}
	}

	if (first != last)
	{
		if (last != second)
			first->v.chain = last->v.chain;
		else
			first->v.chain = EDICT_TO_PROG(last);
		last->v.chain = EDICT_TO_PROG((edict_t *)sv.edicts);
		if (second && second != last)
			second->v.chain = EDICT_TO_PROG(last);
	}
	RETURN_EDICT(first);
}
#else
{
	int		e;	
	int		f;
	char	*s, *t;
	edict_t	*ed;

	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = G_STRING(OFS_PARM2);
	if (!s)
		PR_RunError ("PF_Find: bad search string");
		
	for (e++ ; e < sv.num_edicts ; e++)
	{
		ed = EDICT_NUM(e);
		if (ed->free)
			continue;
		t = E_STRING(ed,f);
		if (!t)
			continue;
		if (!strcmp(t,s))
		{
			RETURN_EDICT(ed);
			return;
		}
	}

	RETURN_EDICT(sv.edicts);
}
#endif

void PR_CheckEmptyString (char *s)
{
	if (s[0] <= ' ')
		PR_RunError ("Bad string");
}

void PF_precache_file (void)
{	// precache_file is only used to copy files with qcc, it does nothing
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

void PF_precache_sound (void)
{
	char	*s;
	int		i;
	
	if (sv.state != ss_loading)
		PR_RunError ("PF_Precache_*: Precache can only be done in spawn functions");
		
	s = G_STRING(OFS_PARM0);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	PR_CheckEmptyString (s);
	
	for (i=0 ; i<MAX_SOUNDS ; i++)
	{
		if (!sv.sound_precache[i])
		{
			sv.sound_precache[i] = s;
			return;
		}
		if (!strcmp(sv.sound_precache[i], s))
			return;
	}
	PR_RunError ("PF_precache_sound: overflow");
}

void PF_precache_model (void)
{
	char	*s;
	int		i;
	
	if (sv.state != ss_loading)
		PR_RunError ("PF_Precache_*: Precache can only be done in spawn functions");
		
	s = G_STRING(OFS_PARM0);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	PR_CheckEmptyString (s);

	for (i=0 ; i<MAX_MODELS ; i++)
	{
		if (!sv.model_precache[i])
		{
			sv.model_precache[i] = s;
			sv.models[i] = Mod_ForName (s, TRUE);
			return;
		}
		if (!strcmp(sv.model_precache[i], s))
			return;
	}
	PR_RunError ("PF_precache_model: overflow");
}


void PF_coredump (void)
{
	ED_PrintEdicts ();
}

void PF_traceon (void)
{
	pr_trace = TRUE;
}

void PF_traceoff (void)
{
	pr_trace = FALSE;
}

void PF_eprint (void)
{
	ED_PrintNum (G_EDICTNUM(OFS_PARM0));
}

/*
===============
PF_walkmove

float(float yaw, float dist) walkmove
===============
*/
void PF_walkmove (void)
{
	edict_t	*ent;
	float	yaw, dist;
	vec3_t	move;
	dfunction_t	*oldf;
	int 	oldself;
	
	ent = PROG_TO_EDICT(pr_global_struct->self);
	yaw = G_FLOAT(OFS_PARM0);
	dist = G_FLOAT(OFS_PARM1);
	
	if ( !( (int)ent->v.flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	yaw = yaw*Q_PI*2 / 360;
	
	move[0] = cosf(yaw)*dist;
	move[1] = sinf(yaw)*dist;
	move[2] = 0;

// save program state, because SV_movestep may call other progs
	oldf = pr_xfunction;
	oldself = pr_global_struct->self;
	
	G_FLOAT(OFS_RETURN) = SV_movestep(ent, move, TRUE);
	
	
// restore program state
	pr_xfunction = oldf;
	pr_global_struct->self = oldself;
}

/*
===============
PF_droptofloor

void() droptofloor
===============
*/
void PF_droptofloor (void)
{
	edict_t		*ent;
	vec3_t		end;
	trace_t		trace;
	
	ent = PROG_TO_EDICT(pr_global_struct->self);

	VectorCopy (ent->v.origin, end);
	end[2] -= 256;
	
	trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, FALSE, ent);

	if (trace.fraction == 1 || trace.allsolid)
		G_FLOAT(OFS_RETURN) = 0;
	else
	{
		VectorCopy (trace.endpos, ent->v.origin);
		SV_LinkEdict (ent, FALSE);
		ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
		ent->v.groundentity = EDICT_TO_PROG(trace.ent);
		G_FLOAT(OFS_RETURN) = 1;
	}
}

/*
===============
PF_lightstyle

void(float style, string value) lightstyle
===============
*/
void PF_lightstyle (void)
{
	int		style;
	char	*val;
	client_t	*client;
	int			j;
	
	style = G_FLOAT(OFS_PARM0);
	val = G_STRING(OFS_PARM1);

// change the string in sv
	sv.lightstyles[style] = val;
	
// send message to all clients on this server
	if (sv.state != ss_active)
		return;
	
	for (j=0, client = svs.clients ; j<svs.maxclients ; j++, client++)
		if (client->active || client->spawned)
		{
			MSG_WriteChar (&client->message, svc_lightstyle);
			MSG_WriteChar (&client->message,style);
			MSG_WriteString (&client->message, val);
		}
}

void PF_rint (void)
{
	float	f;
	f = G_FLOAT(OFS_PARM0);
	if (f > 0)
		G_FLOAT(OFS_RETURN) = (int)(f + 0.5f);
	else
		G_FLOAT(OFS_RETURN) = (int)(f - 0.5f);
}
void PF_floor (void)
{
	G_FLOAT(OFS_RETURN) = floorf(G_FLOAT(OFS_PARM0));
}
void PF_ceil (void)
{
	G_FLOAT(OFS_RETURN) = ceilf(G_FLOAT(OFS_PARM0));
}


/*
=============
PF_checkbottom
=============
*/
void PF_checkbottom (void)
{
	edict_t	*ent;
	
	ent = G_EDICT(OFS_PARM0);

	G_FLOAT(OFS_RETURN) = SV_CheckBottom (ent);
}

/*
=============
PF_pointcontents
=============
*/
void PF_pointcontents (void)
{
	float	*v;
	
	v = G_VECTOR(OFS_PARM0);

	G_FLOAT(OFS_RETURN) = SV_PointContents (v);	
}

/*
=============
PF_nextent

entity nextent(entity)
=============
*/
void PF_nextent (void)
{
	int		i;
	edict_t	*ent;
	
	i = G_EDICTNUM(OFS_PARM0);
	while (1)
	{
		i++;
		if (i == sv.num_edicts)
		{
			RETURN_EDICT(sv.edicts);
			return;
		}
		ent = EDICT_NUM(i);
		if (!ent->free)
		{
			RETURN_EDICT(ent);
			return;
		}
	}
}

/*
=============
PF_aim

Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
cvar_t	sv_aim = {"sv_aim", "0.93"};
void PF_aim (void)
{
	edict_t	*ent, *check, *bestent;
	vec3_t	start, dir, end, bestdir;
	int		i, j;
	trace_t	tr;
	float	dist, bestdist;
	float	speed;
	
	ent = G_EDICT(OFS_PARM0);
	speed = G_FLOAT(OFS_PARM1);

	VectorCopy (ent->v.origin, start);
	start[2] += 20;

// try sending a trace straight
	VectorCopy (pr_global_struct->v_forward, dir);
	VectorMA (start, 2048, dir, end);
	tr = SV_Move (start, vec3_origin, vec3_origin, end, FALSE, ent);
	if (tr.ent && tr.ent->v.takedamage == DAMAGE_AIM
	&& (!teamplay.value || ent->v.team <=0 || ent->v.team != tr.ent->v.team) )
	{
		VectorCopy (pr_global_struct->v_forward, G_VECTOR(OFS_RETURN));
		return;
	}


// try all possible entities
	VectorCopy (dir, bestdir);
	bestdist = sv_aim.value;
	bestent = NULL;
	
	check = NEXT_EDICT(sv.edicts);
	for (i=1 ; i<sv.num_edicts ; i++, check = NEXT_EDICT(check) )
	{
		if (check->v.takedamage != DAMAGE_AIM)
			continue;
		if (check == ent)
			continue;
		if (teamplay.value && ent->v.team > 0 && ent->v.team == check->v.team)
			continue;	// don't aim at teammate
		for (j=0 ; j<3 ; j++)
			end[j] = check->v.origin[j]
			+ 0.5f*(check->v.mins[j] + check->v.maxs[j]);
		VectorSubtract (end, start, dir);
		VectorNormalize (dir);
		dist = DotProduct (dir, pr_global_struct->v_forward);
		if (dist < bestdist)
			continue;	// to far to turn
		tr = SV_Move (start, vec3_origin, vec3_origin, end, FALSE, ent);
		if (tr.ent == check)
		{	// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}
	
	if (bestent)
	{
		VectorSubtract (bestent->v.origin, ent->v.origin, dir);
		dist = DotProduct (dir, pr_global_struct->v_forward);
		VectorScale (pr_global_struct->v_forward, dist, end);
		end[2] = dir[2];
		VectorNormalize (end);
		VectorCopy (end, G_VECTOR(OFS_RETURN));	
	}
	else
	{
		VectorCopy (bestdir, G_VECTOR(OFS_RETURN));
	}
}

// entity (entity start, .float field, float match) findfloat = #98;
void PF_FindFloat (void)
{
	int		e;
	int		f;
	float	s, t;
	edict_t	*ed;

	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = G_FLOAT(OFS_PARM2);
	if (!s)
		PR_RunError ("PF_FindFloat: bad search float");

	for (e++ ; e < sv.num_edicts ; e++)
	{
		ed = EDICT_NUM(e);
		if (ed->free)
			continue;
		t = E_FLOAT(ed,f);
		if (!t)
			continue;
		if (t == s)
		{
			RETURN_EDICT(ed);
			return;
		}
	}

	RETURN_EDICT(sv.edicts);
}

/*
==============
PF_changeyaw

This was a major timewaster in progs, so it was converted to C
==============
*/
void PF_changeyaw (void)
{
	edict_t		*ent;
	float		ideal, current, move, speed;
	
	ent = PROG_TO_EDICT(pr_global_struct->self);
	current = anglemod( ent->v.angles[1] );
	ideal = ent->v.ideal_yaw;
	speed = ent->v.yaw_speed;
	
	if (current == ideal)
		return;
	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}
	
	ent->v.angles[1] = anglemod (current + move);
}

/*
==============
PF_GetSoundLen

Get the lenght of the sound (useful for things like radio)
==============
*/
void PF_GetSoundLen (void)
{

	char	*name;

	name = G_STRING(OFS_PARM0);

    char	namebuffer[256];
	byte	*data;
	wavinfo_t	info;
	byte	stackbuf[1*1024];		// avoid dirtying the cache heap

//Con_Printf ("S_LoadSound: %x\n", (int)stackbuf);
// load it in
    Q_strcpy(namebuffer, "");
    Q_strcat(namebuffer, name);

	data = COM_LoadStackFile(namebuffer, stackbuf, sizeof(stackbuf));

	if (!data)
	{
		Con_Printf ("Couldn't load %s\n", namebuffer);
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	info = GetWavinfo (name, data, com_filesize);
	if (info.channels != 1)
	{
		Con_Printf ("%s is a stereo sample\n",name);
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}

	G_FLOAT(OFS_RETURN) = (float)info.samples/(float)info.rate;
}

#ifdef QUAKE2
/*
==============
PF_changepitch
==============
*/
void PF_changepitch (void)
{
	edict_t		*ent;
	float		ideal, current, move, speed;
	
	ent = G_EDICT(OFS_PARM0);
	current = anglemod( ent->v.angles[0] );
	ideal = ent->v.idealpitch;
	speed = ent->v.pitch_speed;
	
	if (current == ideal)
		return;
	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}
	
	ent->v.angles[0] = anglemod (current + move);
}
#endif

/*
===============================================================================

MESSAGE WRITING

===============================================================================
*/

#define	MSG_BROADCAST	0		// unreliable to all
#define	MSG_ONE			1		// reliable to one (msg_entity)
#define	MSG_ALL			2		// reliable to all
#define	MSG_INIT		3		// write to the init string

sizebuf_t *WriteDest (void)
{
	int		entnum;
	int		dest;
	edict_t	*ent;

	dest = G_FLOAT(OFS_PARM0);
	switch (dest)
	{
	case MSG_BROADCAST:
		return &sv.datagram;
	
	case MSG_ONE:
		ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
		entnum = NUM_FOR_EDICT(ent);
		if (entnum < 1 || entnum > svs.maxclients)
			PR_RunError ("WriteDest: not a client");
		return &svs.clients[entnum-1].message;
		
	case MSG_ALL:
		return &sv.reliable_datagram;
	
	case MSG_INIT:
		return &sv.signon;

	default:
		PR_RunError ("WriteDest: bad destination");
		break;
	}
	
	return NULL;
}

void PF_WriteByte (void)
{
	MSG_WriteByte (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteChar (void)
{
	MSG_WriteChar (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteShort (void)
{
	MSG_WriteShort (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteLong (void)
{
	MSG_WriteLong (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteAngle (void)
{
	MSG_WriteAngle (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteCoord (void)
{
	MSG_WriteCoord (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteString (void)
{
	MSG_WriteString (WriteDest(), G_STRING(OFS_PARM1));
}


void PF_WriteEntity (void)
{
	MSG_WriteShort (WriteDest(), G_EDICTNUM(OFS_PARM1));
}

//=============================================================================

int SV_ModelIndex (char *name);

void PF_makestatic (void)
{
	edict_t	*ent;
	int		i;
	
	ent = G_EDICT(OFS_PARM0);

	MSG_WriteByte (&sv.signon,svc_spawnstatic);

	MSG_WriteByte (&sv.signon, SV_ModelIndex(pr_strings + ent->v.model));

	MSG_WriteByte (&sv.signon, ent->v.frame);
	MSG_WriteByte (&sv.signon, ent->v.colormap);
	MSG_WriteByte (&sv.signon, ent->v.skin);
	for (i=0 ; i<3 ; i++)
	{
		MSG_WriteCoord(&sv.signon, ent->v.origin[i]);
		MSG_WriteAngle(&sv.signon, ent->v.angles[i]);
	}

// throw the entity away now
	ED_Free (ent);
}

//=============================================================================

/*
==============
PF_setspawnparms
==============
*/
void PF_setspawnparms (void)
{
	edict_t	*ent;
	int		i;
	client_t	*client;

	ent = G_EDICT(OFS_PARM0);
	i = NUM_FOR_EDICT(ent);
	if (i < 1 || i > svs.maxclients)
		PR_RunError ("Entity is not a client");

	// copy spawn parms out of the client_t
	client = svs.clients + (i-1);

	for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
		(&pr_global_struct->parm1)[i] = client->spawn_parms[i];
}

/*
==============
PF_changelevel
==============
*/
void PF_changelevel (void)
{
#ifdef QUAKE2
	char	*s1, *s2;

	if (svs.changelevel_issued)
		return;
	svs.changelevel_issued = TRUE;

	s1 = G_STRING(OFS_PARM0);
	s2 = G_STRING(OFS_PARM1);

	if ((int)pr_global_struct->serverflags & (SFL_NEW_UNIT | SFL_NEW_EPISODE))
		Cbuf_AddText (va("changelevel %s %s\n",s1, s2));
	else
		Cbuf_AddText (va("changelevel2 %s %s\n",s1, s2));
#else
	char	*s;

// make sure we don't issue two changelevels
	if (svs.changelevel_issued)
		return;
	svs.changelevel_issued = TRUE;
	
	s = G_STRING(OFS_PARM0);
	Cbuf_AddText (va("changelevel %s\n",s));
#endif
}
#ifdef QUAKE2
#define	CONTENT_WATER	-3
#define CONTENT_SLIME	-4
#define CONTENT_LAVA	-5

#define FL_IMMUNE_WATER	131072
#define	FL_IMMUNE_SLIME	262144
#define FL_IMMUNE_LAVA	524288

#define	CHAN_VOICE	2
#define	CHAN_BODY	4

#define	ATTN_NORM	1

void PF_WaterMove (void)
{
	edict_t		*self;
	int			flags;
	int			waterlevel;
	int			watertype;
	float		drownlevel;
	float		damage = 0.0f;

	self = PROG_TO_EDICT(pr_global_struct->self);

	if (self->v.movetype == MOVETYPE_NOCLIP)
	{
		self->v.air_finished = sv.time + 12;
		G_FLOAT(OFS_RETURN) = damage;
		return;
	}

	if (self->v.health < 0)
	{
		G_FLOAT(OFS_RETURN) = damage;
		return;
	}

	if (self->v.deadflag == DEAD_NO)
		drownlevel = 3;
	else
		drownlevel = 1;

	flags = (int)self->v.flags;
	waterlevel = (int)self->v.waterlevel;
	watertype = (int)self->v.watertype;

	if (!(flags & (FL_IMMUNE_WATER + FL_GODMODE)))
		if (((flags & FL_SWIM) && (waterlevel < drownlevel)) || (waterlevel >= drownlevel))
		{
			if (self->v.air_finished < sv.time)
				if (self->v.pain_finished < sv.time)
				{
					self->v.dmg = self->v.dmg + 2;
					if (self->v.dmg > 15)
						self->v.dmg = 10;
//					T_Damage (self, world, world, self.dmg, 0, FALSE);
					damage = self->v.dmg;
					self->v.pain_finished = sv.time + 1.0f;
				}
		}
		else
		{
			if (self->v.air_finished < sv.time)
//				sound (self, CHAN_VOICE, "player/gasp2.wav", 1, ATTN_NORM);
				SV_StartSound (self, CHAN_VOICE, "player/gasp2.wav", 255, ATTN_NORM);
			else if (self->v.air_finished < sv.time + 9)
//				sound (self, CHAN_VOICE, "player/gasp1.wav", 1, ATTN_NORM);
				SV_StartSound (self, CHAN_VOICE, "player/gasp1.wav", 255, ATTN_NORM);
			self->v.air_finished = sv.time + 12.0;
			self->v.dmg = 2;
		}
	
	if (!waterlevel)
	{
		if (flags & FL_INWATER)
		{	
			// play leave water sound
//			sound (self, CHAN_BODY, "misc/outwater.wav", 1, ATTN_NORM);
			SV_StartSound (self, CHAN_BODY, "misc/outwater.wav", 255, ATTN_NORM);
			self->v.flags = (float)(flags &~FL_INWATER);
		}
		self->v.air_finished = sv.time + 12.0;
		G_FLOAT(OFS_RETURN) = damage;
		return;
	}

	if (watertype == CONTENT_LAVA)
	{	// do damage
		if (!(flags & (FL_IMMUNE_LAVA + FL_GODMODE)))
			if (self->v.dmgtime < sv.time)
			{
				if (self->v.radsuit_finished < sv.time)
					self->v.dmgtime = sv.time + 0.2f;
				else
					self->v.dmgtime = sv.time + 1.0f;
//				T_Damage (self, world, world, 10*self.waterlevel, 0, TRUE);
				damage = (float)(10*waterlevel);
			}
	}
	else if (watertype == CONTENT_SLIME)
	{	// do damage
		if (!(flags & (FL_IMMUNE_SLIME + FL_GODMODE)))
			if (self->v.dmgtime < sv.time && self->v.radsuit_finished < sv.time)
			{
				self->v.dmgtime = sv.time + 1.0f;
//				T_Damage (self, world, world, 4*self.waterlevel, 0, TRUE);
				damage = (float)(4*waterlevel);
			}
	}
	
	if ( !(flags & FL_INWATER) )
	{	

// player enter water sound
		if (watertype == CONTENT_LAVA)
//			sound (self, CHAN_BODY, "player/inlava.wav", 1, ATTN_NORM);
			SV_StartSound (self, CHAN_BODY, "player/inlava.wav", 255, ATTN_NORM);
		if (watertype == CONTENT_WATER)
//			sound (self, CHAN_BODY, "player/inh2o.wav", 1, ATTN_NORM);
			SV_StartSound (self, CHAN_BODY, "player/inh2o.wav", 255, ATTN_NORM);
		if (watertype == CONTENT_SLIME)
//			sound (self, CHAN_BODY, "player/slimbrn2.wav", 1, ATTN_NORM);
			SV_StartSound (self, CHAN_BODY, "player/slimbrn2.wav", 255, ATTN_NORM);

		self->v.flags = (float)(flags | FL_INWATER);
		self->v.dmgtime = 0;
	}
	
	if (! (flags & FL_WATERJUMP) )
	{
//		self.velocity = self.velocity - 0.8f*self.waterlevel*frametime*self.velocity;
		VectorMA (self->v.velocity, -0.8f * self->v.waterlevel * host_frametime, self->v.velocity, self->v.velocity);
	}

	G_FLOAT(OFS_RETURN) = damage;
}
#endif
/*
=================
PF_achievement

unlocks the achievement number for entity

achievement(clientent, value)
=================
*/
void PF_achievement (void)
{
	int		ach;
	client_t	*client;
	int			entnum;

	entnum = G_EDICTNUM(OFS_PARM0);
	ach = G_FLOAT(OFS_PARM1);

	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_DPrintf ("tried to unlock ach to a non-client\n");
		return;
	}

	//Con_Printf (va("Achievement? %i\n", ach));	// JPG
	client = &svs.clients[entnum-1];

	MSG_WriteByte (&client->message,svc_achievement);
	MSG_WriteByte (&client->message, ach);
}

/*
=================
PF_SetPlayerName

sends the name string to
the client, avoids making
a protocol extension and
spamming strings.

nzp_setplayername()
=================
*/
void PF_SetPlayerName(void)
{
	client_t	*client;
	int			entnum;
	char* 		s;

	entnum = G_EDICTNUM(OFS_PARM0);
	s = G_STRING(OFS_PARM1);

	if (entnum < 1 || entnum > svs.maxclients)
		return;

	client = &svs.clients[entnum-1];
	MSG_WriteByte (&client->message, svc_playername);
	MSG_WriteString (&client->message, s);
}

/*
=================
PF_ScreenFlash

Server tells client to flash on screen
for a short (but specified) moment.

nzp_screenflash(target, color, duration, type)
=================
*/
void PF_ScreenFlash(void)
{
	client_t	*client;
	int			entnum;
	int 		color, duration, type;

	entnum = G_EDICTNUM(OFS_PARM0);
	color = G_FLOAT(OFS_PARM1);
	duration = G_FLOAT(OFS_PARM2);
	type = G_FLOAT(OFS_PARM3);

	// Specified world, or something. Send to everyone.
	if (entnum < 1 || entnum > svs.maxclients) {
		MSG_WriteByte(&sv.reliable_datagram, svc_screenflash);
		MSG_WriteByte(&sv.reliable_datagram, color);
		MSG_WriteByte(&sv.reliable_datagram, duration);
		MSG_WriteByte(&sv.reliable_datagram, type);
	} 
	// Send to specific user
	else {
		client = &svs.clients[entnum-1];
		MSG_WriteByte (&client->message, svc_screenflash);
		MSG_WriteByte (&client->message, color);
		MSG_WriteByte (&client->message, duration);
		MSG_WriteByte (&client->message, type);
	}
}

/*
=================
PF_updateLimb

updates zombies limb

PF_updateLimb(zombieent, value. limbent)
=================
*/
void PF_updateLimb (void)
{
	int		limb;
	int		zombieent, limbent;

	zombieent = G_EDICTNUM(OFS_PARM0);
	limb = G_FLOAT(OFS_PARM1);
	limbent = G_EDICTNUM(OFS_PARM2);
	MSG_WriteByte (&sv.reliable_datagram,   svc_limbupdate);
	MSG_WriteByte (&sv.reliable_datagram,  limb);
	MSG_WriteShort (&sv.reliable_datagram,  zombieent);
	MSG_WriteShort (&sv.reliable_datagram,  limbent);
}


void PF_sin (void)
{
	G_FLOAT(OFS_RETURN) = sinf(G_FLOAT(OFS_PARM0));
}

void PF_cos (void)
{
	G_FLOAT(OFS_RETURN) = cosf(G_FLOAT(OFS_PARM0));
}

void PF_sqrt (void)
{
	G_FLOAT(OFS_RETURN) = sqrtf(G_FLOAT(OFS_PARM0));
}

void PF_Fixme (void)
{
	PR_RunError ("unimplemented bulitin");
}

/*
=================
PF_SongEgg

plays designated easter egg track

songegg(trackname)
=================
*/
void PF_SongEgg (void)
{
	char trackname;
	
	trackname = G_STRING(OFS_PARM0);

	//MSG_WriteByte (&sv.reliable_datagram,   svc_songegg);
	//MSG_WriteString (&sv.reliable_datagram, trackname);
}

/*
=================
PF_MaxAmmo

activates max ammo text in HUD

nzp_maxammo()
=================
*/
void PF_MaxAmmo(void)
{
	MSG_WriteByte(&sv.reliable_datagram, svc_maxammo);
}

/*
=================
PF_GrenadePulse

pulses crosshair for grenades

grenade_pulse()
=================
*/
void PF_GrenadePulse(void)
{
	client_t	*client;
	int			entnum;

	entnum = G_EDICTNUM(OFS_PARM0);

	if (entnum < 1 || entnum > svs.maxclients)
		return;

	client = &svs.clients[entnum-1];
	MSG_WriteByte (&client->message,svc_pulse);
}

/*
=================
PF_SetDoubleTapVersion

Server tells client which HUD icon
to draw for Double-Tap (damage buff
v.s. just rate of fire enhancement).

nzp_setdoubletapver()
=================
*/
void PF_SetDoubleTapVersion(void)
{
	client_t	*client;
	int			entnum;
	int 		state;

	entnum = G_EDICTNUM(OFS_PARM0);
	state = G_FLOAT(OFS_PARM1);

	if (entnum < 1 || entnum > svs.maxclients)
		return;

	client = &svs.clients[entnum-1];
	MSG_WriteByte (&client->message, svc_doubletap);
	MSG_WriteByte (&client->message, state);
}

/*
=================
PF_MaxZombies

Returns the total number of zombies
the platform can have out at once.

nzp_maxai()
=================
*/
void PF_MaxZombies(void)
{
	G_FLOAT(OFS_RETURN) = 16;
}

/*
=================
PF_BettyPrompt

draws status on hud on
how to use bouncing
betty.

nzp_bettyprompt()
=================
*/
void PF_BettyPrompt(void)
{
	client_t	*client;
	int			entnum;

	entnum = G_EDICTNUM(OFS_PARM0);

	if (entnum < 1 || entnum > svs.maxclients)
		return;

	client = &svs.clients[entnum-1];
	MSG_WriteByte (&client->message, svc_bettyprompt);
}

/*
=================
Main_Waypoint functin

This is where the magic happens
=================
*/

int closedset[MAX_WAYPOINTS]; // The set of nodes already evaluated.
int openset[MAX_WAYPOINTS];//Actual sorted open list
int opensetRef[MAX_WAYPOINTS];//Reference values of open list
int opensetLength;//equivalent of javaScript's array[].length;
#define MaxZombies 16

zombie_ai zombie_list[MaxZombies];
//Debug//
void printSortedOpenSet()
{
	Con_Printf("Sorted!: ");
	int qr;
	for(qr = 0; qr < opensetLength; qr++)
	{
		Con_Printf("%i, ",(int)waypoints[openset[qr]].f_score);
	}
	Con_Printf("\n");
}
//------//


void RemoveWayFromList (int listnumber, int waynum)
{
	if(listnumber == 1)
	{
		//Con_DPrintf ("RemoveWayFromList: closedset[%i] = %i\n", waynum, 0);
		closedset[waynum] = 0;
		return;
	}

	int i;
	int s;
	if(listnumber == 2)
	{
		for(i = 0; i < opensetLength; i++)
		{
			if(openset[i] == waynum)
			{
				openset[i] = 0;
				opensetRef[waynum] = 0;

				for(s = i; s < opensetLength; s++)
				{
					openset[s] = openset[s+1];
				}
				opensetLength -= 1;
				return;
			}
		}
	}
}

void CompareOpenLists()
{
	int refCount, count;
	refCount = 0;
	count = 0;
	int i;
	for(i = 0; i < MAX_WAYPOINTS; i++)
	{
		if(openset[i])
			count++;
		if(opensetRef[i])
			refCount++;
	}
	if(count != refCount || count != opensetLength || refCount != opensetLength)
		Con_Printf("%i%i%i\n",count, refCount,opensetLength);
}

int AddWayToList (int listnumber, int waynum)//blubs binary sorting
{
	if(listnumber == 1)//closed list
	{
		//Con_DPrintf ("AddWayToList: closedset[%i] = %i\n", waynum, 1);
		closedset[waynum] = 1;
		return 1;
	}

	if(listnumber == 2)//openlist
	{
		int min, max, test;
		min = -1;
		max = opensetLength;
		float wayVal = waypoints[waynum].f_score;

		while(max > min)
		{
			if(max - min == 1)
			{
				int i;
				for(i = opensetLength; i > max ; i--)
				{
					openset[i] = openset[i-1];
				}
				openset[max] = waynum;
				opensetLength += 1;
				opensetRef[waynum] = 1;
				//printSortedOpenSet(); for debug only
				return max;
			}
			test = (int)((min + max)/2);
			if(wayVal > waypoints[openset[test]].f_score)
			{
				min = test;
			}
			else if(wayVal < waypoints[openset[test]].f_score)
			{
				max = test;
			}
			if(wayVal == waypoints[openset[test]].f_score)
			{
				max = test;
				min = test - 1;
			}
		}
	}
	return -1;
}

int GetLowestFromOpenSet()
{
	return openset[0];
}

int CheckIfEmptyList (int listnumber)
{
	int i;

	for (i = 0; i < MAX_WAYPOINTS; i++)
	{
		if (listnumber == 1)
		{
			if (closedset[i])
			{
				//Con_DPrintf ("CheckIfEmptyList: closedset[%i]\n", i);
				return 0;
			}
		}
		else if (listnumber == 2)
		{
			if (openset[i])
			{
				//Con_DPrintf ("CheckIfEmptyList: openset[%i]\n", i);
				return 0;
			}
		}
	}
	return 1;
}

int CheckIfWayInList (int listnumber, int waynum)
{
	if(listnumber == 1)
	{
		if(closedset[waynum])
		{
			//Con_DPrintf ("CheckIfWayInList: closedset[%i] = %i\n", waynum, 1);
			return 1;
		}
	}
	if(listnumber == 2)
	{
		if(opensetRef[waynum])
		{
			//Con_DPrintf ("CheckIfWayInList: openset[%i] = %i\n", waynum, 1);
			return 1;
		}
	}
	return 0;
}

float heuristic_cost_estimate (int start_way, int end_way)
{
	//for now we will just look the distance between.
	return VectorDistanceSquared(waypoints[start_way].origin, waypoints[end_way].origin);
}

int proces_list[MAX_WAYPOINTS];
void reconstruct_path(int start_node, int current_node)
{
	int i, s, current;
	current = current_node;
	s = 0;
/*
    if (current_node == waypoints[current_node].came_from)
        p := reconstruct_path(came_from, came_from[current_node])
        return (p + current_node)
    else
        return current_node*/
	Con_DPrintf ("\n");
	Con_DPrintf ("reconstruct_path: start_node = %i, current_node = %i\n\n", start_node, current_node);
	for (i = 0;i < MAX_WAYPOINTS; i++)
	{
		//if (closedset[i])
		//	Con_DPrintf ("reconstruct_path: closedset[%i] = %i\n", i, closedset[i]);
		proces_list[i] = 0;
	}
	proces_list[s] = -1;//-1 means the enemy is the last waypoint
	s = 1;
	while (1)
	{
		//Con_DPrintf("\nreconstruct_path: current = %i, waypoints[current].came_from = %i\n", current, waypoints[current].came_from);
		proces_list[s] = current;//blubs, we now add the first waypoint to the path list
		if (current == start_node)
		{
			Con_DPrintf("reconstruct_path: path done!\n");
			break;
		}
		if (CheckIfWayInList (1, waypoints[current].came_from))
		{
			//Con_DPrintf("reconstruct_path: waypoints[current].came_from %i is in list!\n", waypoints[current].came_from);
			for (i = 0;i < 8; i++)
			{
				if (waypoints[waypoints[current].came_from].target_id[i] < 0) break;

				//Con_DPrintf("reconstruct_path for loop: waypoints[waypoints[current].came_from].target_id[i] =  %i, current = %i\n", waypoints[waypoints[current].came_from].target_id[i], current)
				if (waypoints[waypoints[current].came_from].target_id[i] == current)
				{
					//Con_DPrintf("reconstruct_path: current %i is viable target!\n", current);
					current = waypoints[current].came_from;//woohoo, this waypoint is viable. So set it now as the current one
					break;
				}
			}
		}
		else
		{
			//Con_DPrintf("reconstruct_path: skipped waypoint %i\n", waypoints[current].came_from);
			break;
		}
		s++;
	}
	Con_DPrintf("\nreconstruct_path: dumping the final list\n");
	/*
	for (s = MAX_WAYPOINTS - 1; s > -1; s--)
	{
		//if (proces_list[s])
			//Con_DPrintf("reconstruct_path final: s = %i, proces_list[s] = %i\n", s, proces_list[s]);
	}
	*/
}

int Pathfind (int start_way, int end_way)//note thease 2 are ARRAY locations. Not the waypoints names.
{
	int current, last_way;//current is for the waypoint array, last_way is a way that was used last
	float tentative_g_score, tentative_f_score;
	int i;
	last_way = 0;
	for (i = 0; i < MAX_WAYPOINTS;i++)// clear all the waypoints
	{
		openset[i] = 0;
		opensetRef[i] = 0;
		closedset[i] = 0;
		waypoints[i].f_score = 0;
		waypoints[i].g_score = 0;
		waypoints[i].came_from = 0;
	}
	opensetLength = 0;

	waypoints[start_way].g_score = 0; // Cost from start along best known path.
	// Estimated total cost from start to goal through y.
	waypoints[start_way].f_score = waypoints[start_way].g_score + heuristic_cost_estimate(start_way, end_way);

	AddWayToList (2, start_way);// The set of tentative nodes to be evaluated, initially containing the start node

	while (!CheckIfEmptyList (2))
	{
		//Con_DPrintf("\n");
		current = GetLowestFromOpenSet();
		//Con_DPrintf("Pathfind current: %i, f_score: %f, g_score: %f\n", current, waypoints[current].f_score, waypoints[current].g_score);
		if (current == end_way)
		{
			Con_DPrintf("Pathfind goal reached\n");
			reconstruct_path(start_way, end_way);
			return 1;
		}
		AddWayToList (1, current);
		RemoveWayFromList (2, current);

		for (i = 0;i < 8; i++)
		{
			//Con_DPrintf("Pathfind for start\n");
			if (waypoints[current].target_id[i] < 0) break;

			if (!waypoints[waypoints[current].target_id[i]].open)
			{
				//if (waypoints[current].target_id[i])
					//Con_DPrintf("Pathfind for: %i, waypoints[waypoints[current].target_id[i]].open = %i, current = %i\n", waypoints[current].target_id[i], waypoints[waypoints[current].target_id[i]].open, current);
				continue;
			}

			tentative_g_score = waypoints[current].g_score + waypoints[current].dist[i];
			tentative_f_score = tentative_g_score + heuristic_cost_estimate(waypoints[current].target_id[i], end_way);
			//Con_DPrintf("Pathfind for: %i, t_f_score: %f, t_g_score: %f\n", waypoints[current].target_id[i], tentative_f_score, tentative_g_score);
			
			//if (CheckIfWayInList (1, waypoints[current].target_id[i]) && tentative_f_score >= waypoints[waypoints[current].target_id[i]].f_score)
			if (CheckIfWayInList (1, waypoints[current].target_id[i]))//it was the above, but why do we care about this waypoint if it's already in the closed list? we never check 2 waypoints twice m8, the first iteration that we reach this waypoint is also the fastest way, so lets not EVER check it again.
			{
				//if (CheckIfWayInList (1, waypoints[current].target_id[i]))
				//Con_DPrintf("Pathfind: waypoint %i in closed list\n", waypoints[current].target_id[i]);
				continue;
			}

			if(tentative_f_score < waypoints[waypoints[current].target_id[i]].f_score)
			{
				//Con_DPrintf("Pathfind waypoint is better\n");
				waypoints[waypoints[current].target_id[i]].g_score = tentative_g_score;
				waypoints[waypoints[current].target_id[i]].f_score = tentative_f_score;
			}

			if (!CheckIfWayInList (2, waypoints[current].target_id[i]))
			{
				//Con_DPrintf("Pathfind waypoint not in list\n");
				waypoints[waypoints[current].target_id[i]].g_score = tentative_g_score;
				waypoints[waypoints[current].target_id[i]].f_score = tentative_f_score;

				waypoints[waypoints[current].target_id[i]].came_from = current;
				AddWayToList (2, waypoints[current].target_id[i]);
				//Con_DPrintf("Pathfind: %i added to the openset with waypoints[current].came_from = %i, current = %i\n", waypoints[current].target_id[i], waypoints[current].came_from, current);
			}
		}
		last_way = current;
	}
	return 0;
}
/*
=================
Get_Waypoint_Near

vector Get_Waypoint_Near (entity)
=================
*/

void Get_Waypoint_Near (void)
{
	float best_dist;
	float dist;
	int i, best;
	trace_t   trace;
	edict_t   *ent;

	best = 0;
	Con_DPrintf("Starting Get_Waypoint_Near\n");
	ent = G_EDICT(OFS_PARM0);
	best_dist = 1000000000;
	dist = 0;

	for (i = 0; i < MAX_WAYPOINTS; i++)
	{
		if (waypoints[i].open)
		{
			dist = VecLength2(waypoints[i].origin, ent->v.origin);
			if(dist < best_dist)
			{
				trace = SV_Move (ent->v.origin, vec3_origin, vec3_origin, waypoints[i].origin, 1, ent);

				//Con_DPrintf("Waypoint: %i, distance: %f, fraction: %f\n", i, dist, trace.fraction);
				if (trace.fraction >= 1)
				{
					best_dist = dist;
					best = i;
				}
			}
		}
	}
	Con_DPrintf("'%5.1f %5.1f %5.1f', %f is %f, (%i, %i)\n", waypoints[best].origin[0],waypoints[best].origin[1], waypoints[best].origin[2], best_dist, dist, i, best);
	VectorCopy (waypoints[best].origin, G_VECTOR(OFS_RETURN));
}

/*
=================
Open_Waypoint

void Open_Waypoint (string, string, string, string, string, string, string, string)
=================
*/
void Open_Waypoint (void)
{
	int i, t;
	char *p = G_STRING(OFS_PARM0);

	//Con_DPrintf("Open_Waypoint\n");
	for (i = 1; i < MAX_WAYPOINTS; i++)
	{
		if (waypoints[i].special[0])//no need to open without tag
		{
			if (!strcmp(p, waypoints[i].special))
			{
				waypoints[i].open = 1;
				//Con_DPrintf("Open_Waypoint: %i, opened\n", i);
				t = 1;
			}
			else
				continue;
		}
	}
	//if (t == 0)
	//{
		//Con_DPrintf("Open_Waypoint: no waypoints opened\n");
	//}
}

/*
=================
Close_Waypoint

void Close_Waypoint (string, string, string, string, string, string, string, string)

moto - basically a carbon copy of open_waypoint lol
=================
*/
void Close_Waypoint (void)
{
	int i, t;
	char *p = G_STRING(OFS_PARM0);

	for (i = 1; i < MAX_WAYPOINTS; i++)
	{
		if (waypoints[i].special[0])//no need to open without tag
		{
			if (!strcmp(p, waypoints[i].special))
			{
				waypoints[i].open = 0;
				t = 1;
			}
			else
				continue;
		}
	}
}

/*
=================
Do_Pathfind

float Do_Pathfind (entity zombie, entity target)
=================
*/
float max_waypoint_distance = 750;
short closest_waypoints[MAX_EDICTS]; 

void Do_Pathfind (void)
{
	int i, s;
	trace_t   trace;

	Con_DPrintf("Starting Do_Pathfind\n"); //we first need to look for closest point for both zombie and the player

	int zombie_entnum = G_EDICTNUM(OFS_PARM0);
	int target_entnum = G_EDICTNUM(OFS_PARM1);
	edict_t * zombie = G_EDICT(OFS_PARM0);
	edict_t * ent = G_EDICT(OFS_PARM1);

	float best_dist_z = max_waypoint_distance * max_waypoint_distance;
	float dist_z = 0;
	int best_z = -1;
	float best_dist_e = max_waypoint_distance * max_waypoint_distance;
	float dist_e = 0;
	int best_e = -1;

	int prevclosest = closest_waypoints[zombie_entnum];
	if (prevclosest >= 0) {
		trace = SV_Move (zombie->v.origin, vec3_origin, vec3_origin, waypoints[prevclosest].origin, 1, zombie);
		if (trace.fraction >= 1) {
			dist_z = VectorDistanceSquared(waypoints[prevclosest].origin, zombie->v.origin);
			best_dist_z = dist_z;
			best_z = prevclosest;
		} else {
			for (s = 0; s < 8; s++) {
				int neighbor = waypoints[prevclosest].target_id[s];
				if (neighbor < 0) break;

				dist_z = VectorDistanceSquared(waypoints[neighbor].origin, zombie->v.origin);
				if (dist_z < best_dist_z) {
					trace = SV_Move (zombie->v.origin, vec3_origin, vec3_origin, waypoints[neighbor].origin, 1, zombie);
					if (trace.fraction >= 1) {
						best_dist_z = dist_z;
						best_z = neighbor;
						break;
					}
				}
			}
		}
	}

	// copypasta, forgive me
	prevclosest = closest_waypoints[target_entnum];
	if (prevclosest >= 0) {
		trace = SV_Move (ent->v.origin, vec3_origin, vec3_origin, waypoints[prevclosest].origin, 1, ent);
		if (trace.fraction >= 1) {
			dist_e = VectorDistanceSquared(waypoints[prevclosest].origin, ent->v.origin);
			best_dist_e = dist_e;
			best_e = prevclosest;
		} else {
			for (s = 0; s < 8; s++) {
				int neighbor = waypoints[prevclosest].target_id[s];
				if (neighbor < 0) break;

				dist_e = VectorDistanceSquared(waypoints[neighbor].origin, ent->v.origin);
				if (dist_e < best_dist_e) {
					trace = SV_Move (ent->v.origin, vec3_origin, vec3_origin, waypoints[neighbor].origin, 1, ent);
					if (trace.fraction >= 1) {
						best_dist_e = dist_e;
						best_e = neighbor;
						break;
					}
				}
			}
		}
	}

	for (i = 0; i < MAX_WAYPOINTS; i++) {
		if (!waypoints[i].used || !waypoints[i].open)
			continue;
		
		dist_z = VectorDistanceSquared(waypoints[i].origin, zombie->v.origin);
		if (dist_z < best_dist_z) {
			trace = SV_Move (zombie->v.origin, vec3_origin, vec3_origin, waypoints[i].origin, 1, zombie);
			if (trace.fraction >= 1) {
				best_dist_z = dist_z;
				best_z = i;
			}
		}

		dist_e = VectorDistanceSquared(waypoints[i].origin, ent->v.origin);
		if (dist_e < best_dist_e) {
			trace = SV_Move (ent->v.origin, vec3_origin, vec3_origin, waypoints[i].origin, 1, ent);
			if (trace.fraction >= 1) {
				best_dist_e = dist_e;
				best_e = i;
			}
		}
	}

	closest_waypoints[zombie_entnum] = best_z;
	closest_waypoints[target_entnum] = best_e;

	Con_DPrintf("Starting waypoint: %i, Ending waypoint: %i\n", best_z, best_e);
	if (Pathfind(best_z, best_e))
	{
		for (i = 0; i < MaxZombies; i++)
		{
			if (zombie_entnum == zombie_list[i].zombienum)
			{
				for (s = 0; s < MAX_WAYPOINTS; s++)
				{
					zombie_list[i].pathlist[s] = proces_list[s];
				}
				break;
			}
			if (i == MaxZombies - 1)//zombie was not in list
			{
				for (i = 0; i < MaxZombies; i++)
				{
					if (!zombie_list[i].zombienum)
					{
						zombie_list[i].zombienum = zombie_entnum;
						for (s = 0; s < MAX_WAYPOINTS; s++)
						{
							zombie_list[i].pathlist[s] = proces_list[s];
						}
						break;
					}
				}
				break;
			}
		}

		if(zombie_list[i].pathlist[2] == 0 && zombie_list[i].pathlist[1] != 0)//then we are at player's waypoint!
		{
			Con_DPrintf("We are at player's waypoint already!\n");
			G_FLOAT(OFS_RETURN) = -1;
			return;
		}

		Con_DPrintf("Path found!\n");
		G_FLOAT(OFS_RETURN) = 1;
	}
	else
	{
		Con_DPrintf("Path not found!\n");
		G_FLOAT(OFS_RETURN) = 0;
	}
}


/*
=================
Get_Next_Waypoint This function will return the next waypoint in zombies path and then remove it from the list

vector Get_Next_Waypoint (entity)
=================
*/
void Get_Next_Waypoint (void)
{
	int i, s;
	s = 0;//useless initialize, because compiler likes to yell at me
	int			entnum;
	edict_t   *ent;//blubs added
	vec3_t	move;
	float *start,*mins, *maxs;
	int currentWay = 0;
	//int zomb = 0;
	int skippedWays = 0;

	move [0] = 0;
	move [1] = 0;
	move [2] = 0;

	entnum = G_EDICTNUM(OFS_PARM0);
	ent = G_EDICT(OFS_PARM0);//blubsadded
	start = G_VECTOR(OFS_PARM1);
	mins = G_VECTOR(OFS_PARM2);
	maxs = G_VECTOR(OFS_PARM3);

	mins[0] -= 2;
	mins[1] -= 2;

	maxs[0] += 2;
	maxs[1] += 2;


	for (i = 0; i < MaxZombies; i++)
	{
		if (entnum == zombie_list[i].zombienum)
		{
			for (s = MAX_WAYPOINTS - 1; s > -1; s--)
			{
				if (zombie_list[i].pathlist[s])
				{
					zombie_list[i].pathlist[s] = 0;//This is get_next, so remove our current waypoint from the list.

					if(s == 1)
					{
						VectorCopy (move, G_VECTOR(OFS_RETURN));//we are at our last waypoint, so just return 0,0,0, this should never happen anyways, because we'll make pathfind return something else
						Con_Printf("Warning, only one waypoint in path!\n");
						return;
					}
					s-= 1;
					currentWay = s;//We want the next waypoint
					break;
				}
			}
			break;
		}
	}
	//s is the index in our path, so if s == 1

	if(s == -1 || s == 0)
	{
		//-1?
		//then that means only player was in path, this is just in case...
		//we are at our last waypoint, so just return 0,0,0, this should never happen anyways, because we'll make pathfind return something else
		//0?
		//only 1 waypoint left in path, we can't possibly smooth the path in this scenario.
		//next waypoint in any case is going to be player, so....
		VectorCopy (move, G_VECTOR(OFS_RETURN));
		return;
	}

	int iterations = 5;//that's how many segments per waypoint, pretty important number
	float Scale = 0.5;
	float curScale = 1;
	float Scalar = Scale;
	float TraceResult;
	vec3_t toAdd;
	vec3_t curStart;
	vec3_t temp;
	int q;
	VectorCopy(waypoints[zombie_list[i].pathlist[currentWay]].origin,temp);
	VectorCopy(temp,move);

	while(1)
	{
		//Con_Printf("Main Vector Start: %f, %f, %f Vector End: %f, %f, %f\n",start[0],start[1],start[2],waypoints[zombie_list[i].pathlist[currentWay]].origin[0],waypoints[zombie_list[i].pathlist[currentWay]].origin[1],waypoints[zombie_list[i].pathlist[currentWay]].origin[2]);
		TraceResult = TraceMove(start,mins,maxs,waypoints[zombie_list[i].pathlist[currentWay]].origin,MOVE_NOMONSTERS,ent);
		if(TraceResult == 1)
		{
			VectorCopy(waypoints[zombie_list[i].pathlist[currentWay]].origin,move);
			if(currentWay == 1)//we're at the end of the list, we better not go out of bounds, was 0, now 1, since 0 is for player index
			{
				break;
			}
			currentWay -= 1;
			skippedWays += 1;
		}
		else
		{
			if(skippedWays > 0)
			{
				VectorCopy(waypoints[zombie_list[i].pathlist[currentWay + 1]].origin,temp);
				VectorCopy(temp,curStart);
				VectorSubtract(waypoints[zombie_list[i].pathlist[currentWay]].origin,curStart,toAdd);
				for(q = 0;q < iterations; q++)
				{
					curScale *= Scalar;
					VectorScale(toAdd,curScale,temp);
					VectorAdd(temp,curStart,temp);
					TraceResult = TraceMove(start,mins,maxs,temp,MOVE_NOMONSTERS,ent);
					if(TraceResult ==1)
					{
						Scalar = Scale + 1;
						VectorCopy(temp,move);
					}
					else
					{
						Scalar = Scale;
					}
				}
			}
			break;
		}
	}

	Con_DPrintf("Get Next Way returns: list[%i], waypoint:%i\n",s,waypoints[zombie_list[i].pathlist[s]]);

	//VectorCopy(waypoints[zombie_list[i].pathlist[s]].origin,move); //for old get_next_way
	zombie_list[i].pathlist[s] = 0;

	//Con_Printf("Skipped %i waypoints, we're moving to the %f percentage in between 2 waypoints\n",skippedWays,curScale);
	//Con_DPrintf("'%5.1f %5.1f %5.1f'\n", move[0], move[1], move[2]);
	VectorCopy (move, G_VECTOR(OFS_RETURN));
}
/*
=================
Get_First_Waypoint This function will return the waypoint waypoint in zombies path and then remove it from the list

vector Get_First_Waypoint (entity)
=================
*/
void Get_First_Waypoint (void)
{
	int i, s;
	s = 0;//useless initialize, because compiler likes to yell at me
	int			entnum;
	edict_t   *ent;//blubs added
	vec3_t	move;
	float *start,*mins, *maxs;
	int currentWay = 0;
	//int zomb = 0;
	int skippedWays = 0;

	move [0] = 0;
	move [1] = 0;
	move [2] = 0;

	entnum = G_EDICTNUM(OFS_PARM0);
	ent = G_EDICT(OFS_PARM0);//blubsadded
	start = G_VECTOR(OFS_PARM1);
	mins = G_VECTOR(OFS_PARM2);
	maxs = G_VECTOR(OFS_PARM3);

	mins[0] -= 2;
	mins[1] -= 2;

	maxs[0] += 2;
	maxs[1] += 2;


	for (i = 0; i < MaxZombies; i++)
	{
		if (entnum == zombie_list[i].zombienum)
		{
			for (s = MAX_WAYPOINTS - 1; s > -1; s--)
			{
				if (zombie_list[i].pathlist[s])
				{
					currentWay = s;
					break;
				}
			}
			break;
		}
	}

	if(s == 0)
	{
		//0?
		//currentway is player, just return world
		VectorCopy (move, G_VECTOR(OFS_RETURN));
		return;
	}
	//1? only one way in list, we can't possibly smooth list when we only have one...

	int iterations = 5;//that's how many segments per waypoint, pretty important number
	float Scale = 0.5;
	float curScale = 1;
	float Scalar = Scale;
	float TraceResult;
	vec3_t toAdd;
	vec3_t curStart;
	vec3_t temp;
	int q;
	VectorCopy(waypoints[zombie_list[i].pathlist[currentWay]].origin,temp);
	VectorCopy(temp,move);

	while(1)
	{
		//Con_Printf("Main Vector Start: %f, %f, %f Vector End: %f, %f, %f\n",start[0],start[1],start[2],waypoints[zombie_list[i].pathlist[currentWay]].origin[0],waypoints[zombie_list[i].pathlist[currentWay]].origin[1],waypoints[zombie_list[i].pathlist[currentWay]].origin[2]);
		TraceResult = TraceMove(start,mins,maxs,waypoints[zombie_list[i].pathlist[currentWay]].origin,MOVE_NOMONSTERS,ent);
		if(TraceResult == 1)
		{
			VectorCopy(waypoints[zombie_list[i].pathlist[currentWay]].origin,move);
			if(currentWay == 1)//we're at the end of the list, we better not go out of bounds//was 0, now 1 since 0 is for enemy
			{
				break;
			}
			currentWay -= 1;
			skippedWays += 1;
		}
		else
		{
			if(skippedWays > 0)
			{
				VectorCopy(waypoints[zombie_list[i].pathlist[currentWay + 1]].origin,temp);
				VectorCopy(temp,curStart);
				VectorSubtract(waypoints[zombie_list[i].pathlist[currentWay]].origin,curStart,toAdd);
				for(q = 0;q < iterations; q++)
				{
					curScale *= Scalar;
					VectorScale(toAdd,curScale,temp);
					VectorAdd(temp,curStart,temp);
					//Con_Printf("subVector Start: %f, %f, %f Vector End: %f, %f, %f\n",start[0],start[1],start[2],temp[0],temp[1],temp[2]);
					TraceResult = TraceMove(start,mins,maxs,temp,MOVE_NOMONSTERS,ent);
					if(TraceResult ==1)
					{
						Scalar = Scale + 1;
						VectorCopy(temp,move);
					}
					else
					{
						Scalar = Scale;
					}//we need a way to go back to the other value if it doesn't work!, so lets work with temp, but RETURN a different value other than temp!
				}
			}
			break;
		}
	}

	Con_DPrintf("Get First Way returns: %i\n",s);
	//VectorCopy(waypoints[zombie_list[i].pathlist[s]].origin,move);//for old get_first_way
	zombie_list[i].pathlist[s] = 0;
	//Con_Printf("Skipped %i waypoints, we're moving to the %f percentage in between 2 waypoints\n",skippedWays,curScale);
	//Con_DPrintf("'%5.1f %5.1f %5.1f'\n", move[0], move[1], move[2]);
	VectorCopy (move, G_VECTOR(OFS_RETURN));
}

/*
===============
PF_rumble

void(float time) rumble

added for wii rumble
===============
*/
void PF_rumble (void)
{
	if (!rumble.value) return;
	double rumble_time;
	rumble_time = G_FLOAT(OFS_PARM0) / 1000.0;
	
	//it switches rumble on for rumble_time milliseconds  
	WPAD_Rumble(0, TRUE);
	rumble_on=1;
	time_wpad_off = Sys_FloatTime() + rumble_time;
	
}

/*
=================
PF_tokenize

float tokenize (string) = #441
=================
*/
//KRIMZON_SV_PARSECLIENTCOMMAND added both of these
// refined to work on psp on 2017-DEC-09
void PF_tokenize (void)
{
	char *m = G_STRING(OFS_PARM0);
	Cmd_TokenizeString(m);
	G_FLOAT(OFS_RETURN) = Cmd_Argc();
};

/*
=================
PF_argv

string argv (float num) = #442
=================
*/
void PF_ArgV  (void)
{
	char tempc[256];
	strcpy(tempc, Cmd_Argv(G_FLOAT(OFS_PARM0)));
	G_INT(OFS_RETURN) = tempc - pr_strings;
}


builtin_t pr_builtin[] =
{
PF_Fixme,
PF_makevectors,	// void(entity e)	makevectors 		= #1;
PF_setorigin,	// void(entity e, vector o) setorigin	= #2;
PF_setmodel,	// void(entity e, string m) setmodel	= #3;
PF_setsize,	// void(entity e, vector min, vector max) setsize = #4;
PF_Fixme,	// void(entity e, vector min, vector max) setabssize = #5;
PF_break,	// void() break						= #6;
PF_random,	// float() random						= #7;
PF_sound,	// void(entity e, float chan, string samp) sound = #8;
PF_normalize,	// vector(vector v) normalize			= #9;
PF_error,	// void(string e) error				= #10;
PF_objerror,	// void(string e) objerror				= #11;
PF_vlen,	// float(vector v) vlen				= #12;
PF_vectoyaw,	// float(vector v) vectoyaw		= #13;
PF_Spawn,	// entity() spawn						= #14;
PF_Remove,	// void(entity e) remove				= #15;
PF_traceline,	// float(vector v1, vector v2, float tryents) traceline = #16;
PF_checkclient,	// entity() clientlist					= #17;
PF_Find,	// entity(entity start, .string fld, string match) find = #18;
PF_precache_sound,	// void(string s) precache_sound		= #19;
PF_precache_model,	// void(string s) precache_model		= #20;
PF_stuffcmd,	// void(entity client, string s)stuffcmd = #21;
PF_findradius,	// entity(vector org, float rad) findradius = #22;
PF_bprint,	// void(string s) bprint				= #23;
PF_sprint,	// void(entity client, string s) sprint = #24;
PF_dprint,	// void(string s) dprint				= #25;
PF_ftos,	// void(string s) ftos				= #26;
PF_vtos,	// void(string s) vtos				= #27;
PF_coredump,
PF_traceon,
PF_traceoff,
PF_eprint,	// void(entity e) debug print an entire entity
PF_walkmove, // float(float yaw, float dist) walkmove
PF_updateLimb, // #33
PF_droptofloor,
PF_lightstyle,
PF_rint,
PF_floor,
PF_ceil,
PF_Fixme,
PF_checkbottom, // #40
PF_pointcontents,
PF_Fixme,
PF_fabs,
PF_aim,
PF_cvar,
PF_localcmd,
PF_nextent,
PF_particle,
PF_changeyaw,
PF_GetSoundLen,
PF_vectoangles,

PF_WriteByte,
PF_WriteChar,
PF_WriteShort,
PF_WriteLong,
PF_WriteCoord,
PF_WriteAngle,
PF_WriteString,
PF_WriteEntity,

PF_sin,
PF_cos,
PF_sqrt,
PF_Fixme,//PF_changepitch,
PF_Fixme,//PF_TraceToss,
PF_etos,
PF_Fixme,//PF_WaterMove,

SV_MoveToGoal,
PF_precache_file,
PF_makestatic,

PF_changelevel,
SV_MoveToOrigin, // #71

PF_cvar_set,
PF_centerprint,

PF_ambientsound,

PF_precache_model,
PF_precache_sound,		// precache_sound2 is different only for qcc
PF_precache_file,

PF_setspawnparms,
PF_achievement, // #79
PF_Fixme, // #80
PF_stof, // #81
PF_Fixme, // #82
Get_Waypoint_Near, // #83
Do_Pathfind, // #84
Open_Waypoint, // #85
Get_Next_Waypoint, // #86
PF_useprint, // #87
Get_First_Waypoint, // #88
Close_Waypoint, // #89
PF_tracebox, // #90
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_FindFloat, // #98
PF_tracemove, // #99
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #109
PF_fopen, // #110
PF_fclose, // #111
PF_fgets, // #112
PF_fputs, // #113
PF_strlen, // #114
PF_strcat, // #115
PF_substring, // #116
PF_stov, // #117
PF_strzone, // #118
PF_strunzone, // #119
PF_strtrim, // #120
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #129
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #139
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #149
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #159
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #169
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #179
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #189
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #199
PF_Fixme, // #200
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #210
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #220
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #230
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #240
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #250
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #260
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #270
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #280
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #290
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #300
PF_Fixme, // #301
PF_Fixme, // #302
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #312
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #322
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #332
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #342
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #352
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #362
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #372
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #382
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #392
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #402
PF_Fixme, // #403
PF_Fixme, //#404
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #413
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #423
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #433
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_tokenize, // #441
PF_ArgV, // #442 
PF_Fixme, // #443
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #453
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #463
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #473
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_strtolower, // #480 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #483
PF_Fixme, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, // #493
PF_crc16, 
PF_Fixme,
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_Fixme, 
PF_SongEgg, // #500
PF_MaxAmmo, // #501
PF_GrenadePulse, // #502 
PF_MaxZombies, // #503 
PF_BettyPrompt, // #504
PF_SetPlayerName, // #505
PF_SetDoubleTapVersion, // #506
PF_ScreenFlash, // #507
PF_Fixme
//PF_rumble
};

builtin_t *pr_builtins = pr_builtin;
int pr_numbuiltins = sizeof(pr_builtin)/sizeof(pr_builtin[0]);

