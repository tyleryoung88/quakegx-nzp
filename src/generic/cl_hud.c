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
// cl_hud.c -- status bar code

#include "quakedef.h"

qpic_t		*sb_round[5];
qpic_t		*sb_round_num[10];
qpic_t		*sb_moneyback;
qpic_t		*instapic;
qpic_t		*x2pic;
qpic_t 		*revivepic;
qpic_t		*jugpic;
qpic_t		*floppic;
qpic_t		*staminpic;
qpic_t		*doublepic;
qpic_t 		*doublepic2;
qpic_t		*speedpic;
qpic_t		*deadpic;
qpic_t 		*mulepic;
qpic_t		*fragpic;
qpic_t		*bettypic;


qpic_t 		*b_abutton;
qpic_t 		*b_bbutton;
qpic_t 		*b_cbutton;
qpic_t 		*b_zbutton;
qpic_t 		*b_left;
qpic_t 		*b_right;
qpic_t 		*b_up;
qpic_t 		*b_down;
qpic_t 		*b_minus;
qpic_t 		*b_plus;
qpic_t 		*b_home;
qpic_t 		*b_one;
qpic_t 		*b_two;

qpic_t      *fx_blood_lu;
qpic_t      *fx_blood_ru;
qpic_t      *fx_blood_ld;
qpic_t      *fx_blood_rd;

qboolean	sb_showscores;
qboolean 	domaxammo;
qboolean 	doubletap_has_damage_buff;

int  x_value, y_value;

void M_DrawPic (int x, int y, qpic_t *pic);
void HUD_Scoreboard_Down (void);
void HUD_Scoreboard_Up (void);

double HUD_Change_time;//hide hud when not chagned
double bettyprompt_time;
double nameprint_time;

char player_name[16];

extern cvar_t waypoint_mode;

int screenflash_color;
double screenflash_duration;
int screenflash_type;
double screenflash_worktime;
double screenflash_starttime;

int old_points;
int current_points;
int point_change_interval;
int point_change_interval_neg;
int alphabling = 0;
float round_center_x;
float round_center_y;

typedef struct
{
	int points;
	int negative;
	float x;
	float y;
	float move_x;
	float move_y;
	double alive_time;
} point_change_t;

point_change_t point_change[10];

/*
===============
HUD_Init
===============
*/
void HUD_Init (void)
{
	int		i;

	for (i=0 ; i<5 ; i++)
	{
		sb_round[i] = Draw_CachePic (va("gfx/hud/r%i",i + 1));
	}

	for (i=0 ; i<10 ; i++)
	{
		sb_round_num[i] = Draw_CachePic (va("gfx/hud/r_num%i",i));
	}

	sb_moneyback = Draw_CachePic ("gfx/hud/moneyback");
	instapic = Draw_CachePic ("gfx/hud/in_kill");
	x2pic = Draw_CachePic ("gfx/hud/2x");

	revivepic = Draw_CachePic ("gfx/hud/revive");
	jugpic = Draw_CachePic ("gfx/hud/jug");
	floppic = Draw_CachePic ("gfx/hud/flopper");
	staminpic = Draw_CachePic ("gfx/hud/stamin");
	doublepic = Draw_CachePic ("gfx/hud/double");
	doublepic2 = Draw_CachePic ("gfx/hud/double2");
	speedpic = Draw_CachePic ("gfx/hud/speed");
	deadpic = Draw_CachePic ("gfx/hud/dead");
	mulepic = Draw_CachePic ("gfx/hud/mule");
	fragpic = Draw_CachePic ("gfx/hud/frag");
	bettypic = Draw_CachePic ("gfx/hud/betty");

	b_abutton = Draw_CachePic ("gfx/butticons/abutton");
	b_bbutton = Draw_CachePic ("gfx/butticons/bbutton");
	b_cbutton = Draw_CachePic ("gfx/butticons/cbutton");
	b_zbutton = Draw_CachePic ("gfx/butticons/zbutton");
	b_left = Draw_CachePic ("gfx/butticons/Dleftbutton");
	b_right = Draw_CachePic ("gfx/butticons/Drightbutton");
	b_up = Draw_CachePic ("gfx/butticons/Dupbutton");
	b_down = Draw_CachePic ("gfx/butticons/Ddownbutton");
	b_minus = Draw_CachePic ("gfx/butticons/-button");
	b_plus = Draw_CachePic ("gfx/butticons/+button");
	b_home = Draw_CachePic ("gfx/butticons/homebutton");
	b_one = Draw_CachePic ("gfx/butticons/1button");
	b_two = Draw_CachePic ("gfx/butticons/2button");

    fx_blood_lu = Draw_CachePic ("gfx/hud/blood");
	
	Cmd_AddCommand ("+showscores", HUD_Scoreboard_Down);
	Cmd_AddCommand ("-showscores", HUD_Scoreboard_Up);

    // naievil -- fixme
	//Achievement_Init();
}

/*
===============
HUD_NewMap
===============
*/
void HUD_NewMap (void)
{
	int i;
	alphabling = 0;

	for (i=0 ; i<10 ; i++)
	{
		point_change[i].points = 0;
		point_change[i].negative = 0;
		point_change[i].x = 0.0;
		point_change[i].y = 0.0;
		point_change[i].move_x = 0.0;
		point_change[i].move_y = 0.0;
		point_change[i].alive_time = 0.0;
	}

	old_points = 500;
	current_points = 500;
	point_change_interval = 0;
	point_change_interval_neg = 0;
	bettyprompt_time = 0;
	nameprint_time = 0;

	round_center_x = (vid.width - sb_round[0]->width) /2;
	round_center_y = (vid.height - sb_round[0]->height) /2;
}


/*
=============
HUD_itoa
=============
*/
int HUD_itoa (int num, char *buf)
{
	char	*str;
	int		pow10;
	int		dig;

	str = buf;

	if (num < 0)
	{
		*str++ = '-';
		num = -num;
	}

	for (pow10 = 10 ; num >= pow10 ; pow10 *= 10)
	;

	do
	{
		pow10 /= 10;
		dig = num/pow10;
		*str++ = '0'+dig;
		num -= dig*pow10;
	} while (pow10 != 1);

	*str = 0;

	return str-buf;
}


//=============================================================================

int		pointsort[MAX_SCOREBOARD];

char	scoreboardtext[MAX_SCOREBOARD][20];
int		scoreboardtop[MAX_SCOREBOARD];
int		scoreboardbottom[MAX_SCOREBOARD];
int		scoreboardcount[MAX_SCOREBOARD];
int		scoreboardlines;

/*
===============
HUD_Sorpoints
===============
*/
void HUD_Sortpoints (void)
{
	int		i, j, k;

// sort by points
	scoreboardlines = 0;
	for (i=0 ; i<cl.maxclients ; i++)
	{
		if (cl.scores[i].name[0])
		{
			pointsort[scoreboardlines] = i;
			scoreboardlines++;
		}
	}

	for (i=0 ; i<scoreboardlines ; i++)
		for (j=0 ; j<scoreboardlines-1-i ; j++)
			if (cl.scores[pointsort[j]].points < cl.scores[pointsort[j+1]].points)
			{
				k = pointsort[j];
				pointsort[j] = pointsort[j+1];
				pointsort[j+1] = k;
			}
}

qboolean showscoreboard = false;
void HUD_Scoreboard_Down (void) 
{
	if (key_dest == key_game)
		showscoreboard = true;
}

void HUD_Scoreboard_Up (void) 
{
	showscoreboard = false;
}

/*
===============
HUD_EndScreen
===============
*/
void HUD_EndScreen (void)
{
	scoreboard_t	*s;
	char			str[80];
	int				i, k, l;
	int				y, x, d;
	
	HUD_Sortpoints ();

	l = scoreboardlines;
	
	if (!showscoreboard) {
		Draw_ColoredString((vid.width - 9*12)/2, 140, "GAME OVER", 255, 0, 0, 255, 1.5);

		sprintf (str,"You survived %3i rounds", cl.stats[STAT_ROUNDS]);
		Draw_String ((vid.width - strlen (str)*12)/2, 165, str);
		
		sprintf (str,"Name           Kills     Points");
		x = (vid.width - strlen (str)*12)/2;

		Draw_String (x, 190, str);
		y = 0;
		for (i=0; i<l ; i++)
		{
			k = pointsort[i];
			s = &cl.scores[k];
			if (!s->name[0])
				continue;

			Draw_String (x, 215 + y, s->name);

			d = strlen (va("%i",s->kills));
			Draw_String (x + (20 - d)*12, 215 + y, va("%i",s->kills));

			d = strlen (va("%i",s->points));
			Draw_String (x + (31 - d)*12, 215 + y, va("%i",s->points));
			y += 25;
		}
	} else {
		sprintf (str,"Name           Kills     Points");
		x = (vid.width - strlen (str)*12)/2;

		Draw_String (x, 190, str);
		y = 0;
		for (i=0; i<l ; i++)
		{
			k = pointsort[i];
			s = &cl.scores[k];
			if (!s->name[0])
				continue;

			Draw_String (x, 215 + y, s->name);

			d = strlen (va("%i",s->kills));
			Draw_String (x + (20 - d)*12, 215 + y, va("%i",s->kills));

			d = strlen (va("%i",s->points));
			Draw_String (x + (31 - d)*12, 215 + y, va("%i",s->points));
			y += 25;
		}
	}
}

//=============================================================================

  //=============================================================================//
 //===============================DRAW FUNCTIONS================================//
//=============================================================================//

/*
==================
HUD_Points

==================
*/


void HUD_Parse_Point_Change (int points, int negative, int x_start, int y_start)
{
	int i, f;
	char str[10];
	i=9;
	while (i>0)
	{
		point_change[i] = point_change[i - 1];
		i--;
	}

	point_change[i].points = points;
	point_change[i].negative = negative;

	f = HUD_itoa (points, str);
	point_change[i].x = x_start + 10.0 + 8.0*f;
	point_change[i].y = y_start;
	point_change[i].move_x = 1.0;
	point_change[i].move_y = ((rand ()&0x7fff) / ((float)0x7fff)) - 0.5;

	point_change[i].alive_time = Sys_FloatTime() + 0.4;
}

void HUD_Points (void)
{
	int				i, k, l;
	int				x, y, f, xplus;
	scoreboard_t	*s;

// scores
	HUD_Sortpoints ();

// draw the text
	l = scoreboardlines;

    x = 15;
    y = 360;
	for (i=0 ; i<l ; i++)
	{
		k = pointsort[i];
		s = &cl.scores[k];
		if (!s->name[0])
			continue;

	// draw background

	// draw number
		f = s->points;
		if (f > current_points)
		{
			point_change_interval_neg = 0;
			if (!point_change_interval)
			{
				point_change_interval = (int)(f - old_points)/55;;
			}
			current_points = old_points + point_change_interval;
			if (f < current_points)
			{
				current_points = f;
				point_change_interval = 0;
			}
		}
		else if (f < current_points)
		{
			point_change_interval = 0;
			if (!point_change_interval_neg)
			{
				point_change_interval_neg = (int)(old_points - f)/55;
			}
			current_points = old_points - point_change_interval_neg;
			if (f > current_points)
			{
				current_points = f;
				point_change_interval_neg = 0;
			}
		}
		Draw_StretchPic (x, y, sb_moneyback, 96, 24);
		xplus = strlen(va("%i", current_points))*12;

		Draw_ColoredString (((64 - xplus)/2) + 15 + x, y + 5, va("%i", current_points), 255, 255, 255, 255, 1.5);

		if (old_points != f)
		{
			if (f > old_points)
				HUD_Parse_Point_Change(f - old_points, 0, ((64 - xplus)/2)+12, y + 3);
			else
				HUD_Parse_Point_Change(old_points - f, 1, ((64 - xplus)/2)+12, y + 3);

			old_points = f;
		}



		y += 10;
	}
}


/*
==================
HUD_Point_Change

==================
*/
void HUD_Point_Change (void)
{

	int	i;

	for (i=0 ; i<10 ; i++)
	{
		if (point_change[i].points)
		{
			if (point_change[i].negative)
				Draw_ColoredString (point_change[i].x, point_change[i].y, va ("-%i", point_change[i].points), 255, 0, 0, 255, 1);
			else
				Draw_ColoredString (point_change[i].x, point_change[i].y, va ("+%i", point_change[i].points), 255, 255, 0, 255, 1);
			point_change[i].y = point_change[i].y + point_change[i].move_y;
			point_change[i].x = point_change[i].x + point_change[i].move_x;
			if (point_change[i].alive_time && point_change[i].alive_time < Sys_FloatTime())
			{
				point_change[i].points = 0;
				point_change[i].negative = 0;
				point_change[i].x = 0.0;
				point_change[i].y = 0.0;
				point_change[i].move_x = 0.0;
				point_change[i].move_y = 0.0;
				point_change[i].alive_time = 0.0;
			}
		}
	}

}

/*
==================
HUD_Blood

==================
*/
void HUD_Blood (void)
{

    float alpha;
	//blubswillrule:
	//this function scales linearly from health = 0 to health = 100
	//alpha = (100.0 - (float)cl.stats[STAT_HEALTH])/100*255;
	//but we want the picture to be fully visible at health = 20, so use this function instead
	alpha = (100.0 - ((1.25 * (float) cl.stats[STAT_HEALTH]) - 25))/100*255;
	
    if (alpha <= 0.0)
        return;
    
    float modifier = (sin(cl.time * 10) * 20) - 20;//always negative

    if(modifier < -35.0)
	modifier = -35.0;
    
    alpha += modifier;
    
    if(alpha < 0.0)
	    return;
    float color = 255.0 + modifier;
    
    Draw_ColoredStretchPic(0, 0, fx_blood_lu, 640, 480, color,color,color,alpha);

}

/*
===============
HUD_GetWorldText
===============
*/

// modified from scatter's worldspawn parser
// FIXME - unoptimized, could probably save a bit of
// memory here in the future.
void HUD_WorldText(int alpha)
{
	// for parser
	char key[128], value[256]; //value was allocated 4096
	char *data;

	// first, parse worldspawn
	data = COM_Parse(cl.worldmodel->entities);

	if (!data)
		return; // err
	if (com_token[0] != '{')
		return; // err

	while(1) {
		data = COM_Parse(data);

		if (!data)
			return; // err
		if (com_token[0] == '}')
			break; // end of worldspawn
		
		if (com_token[0] == '_')
			strcpy(key, com_token + 1);
		else
			strcpy(key, com_token);
		
		while (key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;
		
		data = COM_Parse(data);
		if (!data)
			return; // err

		strcpy(value, com_token);

		if (!strcmp("location", key)) // search for location key
		{
			Draw_ColoredString(17, 300, value, 255, 255, 255, alpha, 1.4);
		}
		if (!strcmp("date", key)) // search for date key
		{
			Draw_ColoredString(17, 320, value, 255, 255, 255, alpha, 1.4);
		}
		if (!strcmp("person", key)) // search for person key
		{
			Draw_ColoredString(17, 340, value, 255, 255, 255, alpha, 1.4);
		}
	}
}

/*
===============
HUD_MaxAmmo
===============
*/
int maxammoy;
int maxammoopac;

void HUD_MaxAmmo(void)
{

	maxammoy -= cl.time * 0.03;
	maxammoopac -= 3;

	Draw_ColoredString(vid.width/2 - strlen("MAX AMMO!")*12/2, maxammoy, "MAX AMMO!", 255, 255, 255, maxammoopac, 1.5);

	if (maxammoopac <= 0) {
		domaxammo = false;
	}

}

/*
===============
HUD_Rounds
===============
*/

float 	color_shift[3];
float 	color_shift_end[3];
float 	color_shift_steps[3];
int		color_shift_init;
int 	blinking;
float 	endroundchange;
int 	textstate;
int 	value, value2;

int 	reallydumbvalue;
int 	f222;
int 	maxammoy;
int 	maxammoopac;

void HUD_Rounds (void)
{
	int i, x_offset, icon_num, savex;
	int num[3];
	x_offset = 0;
	savex = 0;

	// Round and Title text - moto
	// extra creds to scatterbox for some x/y vals
	// ------------------
	// First, fade from white to red, ~3s duration
	if (!textstate) {
		if (!value)
			value = 255;

		Draw_ColoredString(vid.width/2 - (strlen("Round")*24)/2, 80, "Round", 255, value, value, 255, 3);
		
		value -= cl.time * 0.4;

		// prep values for next stage
		if (value <= 0) {
			value = 255;
			value2 = 0;
			textstate = 1;
		}
	} 
	// Now, fade out, and start fading worldtext in
	// ~3s for fade out, 
	else if (textstate == 1) {
		Draw_ColoredString(vid.width/2 - (strlen("Round")*24)/2, 80, "Round", 255, 0, 0, value, 3);

		HUD_WorldText(value2);
		Draw_ColoredString(17, 275, "'Nazi Zombies'", 255, 255, 255, value2, 1.4);
		
		value -= cl.time * 0.4;
		value2 += cl.time * 0.4;

		// prep values for next stage
		if (value <= 0) {
			value2 = 0;
			textstate = 2;
		}
	}
	// Hold world text for a few seconds
	else if (textstate == 2) {
		HUD_WorldText(255);
		Draw_ColoredString(17, 275, "'Nazi Zombies'", 255, 255, 255, 255, 1.4);

		value2 += cl.time * 0.4;

		if (value2 >= 255) {
			value2 = 255;
			textstate = 3;
		}
	}
	// Fade worldtext out, finally.
	else if (textstate == 3) {
		HUD_WorldText(value2);
		Draw_ColoredString(17, 275, "'Nazi Zombies'", 255, 255, 255, value2, 1.4);

		value2 -= cl.time * 0.4;

		// prep values for next stage
		if (value2 <= 0) {
			textstate = -1;
		}
	}
	// ------------------
	// End Round and Title text - moto

	if (cl.stats[STAT_ROUNDCHANGE] == 1)//this is the rounds icon at the middle of the screen
	{
		if (textstate == -1) {
			value = 0;
			value2 = 0;
			textstate = 0;
		}

		Draw_ColoredStretchPic ((vid.width - sb_round[0]->width) /2, (vid.height - sb_round[0]->height) /2, sb_round[0], 18, 78, 107, 1, 0, alphabling); // 11x48

		alphabling = alphabling + 15;

		if (alphabling < 0)
			alphabling = 0;
		else if (alphabling > 255)
			alphabling = 255;
	}
	else if (cl.stats[STAT_ROUNDCHANGE] == 2)//this is the rounds icon moving from middle
	{
		Draw_ColoredStretchPic (round_center_x, round_center_y, sb_round[0], 18, 78, 107, 1, 0, 255);
		round_center_x = round_center_x - (229/108) - 0.2;
		round_center_y = round_center_y + 1.3;
		if (round_center_x <= 5)
			round_center_x = 5;
		if (round_center_y >= 397) // vid.height - sb_round[0]->height
			round_center_y = 397; // vid.height - sb_round[0]->height
	}
	else if (cl.stats[STAT_ROUNDCHANGE] == 3)//shift to white
	{
		if (!color_shift_init)
		{
			color_shift[0] = 107;
			color_shift[1] = 1;
			color_shift[2] = 0;
			for (i = 0; i < 3; i++)
			{
				color_shift_end[i] = 255;
				color_shift_steps[i] = (color_shift_end[i] - color_shift[i])/60;
			}
			color_shift_init = 1;
		}
		for (i = 0; i < 3; i++)
		{
			if (color_shift[i] < color_shift_end[i])
				color_shift[i] = color_shift[i] + color_shift_steps[i];

			if (color_shift[i] >= color_shift_end[i])
				color_shift[i] = color_shift_end[i];
		}
		if (cl.stats[STAT_ROUNDS] > 0 && cl.stats[STAT_ROUNDS] < 11)
		{

			for (i = 0; i < cl.stats[STAT_ROUNDS]; i++)
			{
				if (i == 4)
				{
					Draw_ColoredStretchPic (5, vid.height - (sb_round[4]->height * 1.6363) - 4, sb_round[4], 96, 78, (int)color_shift[0], (int)color_shift[1], (int)color_shift[2], 255);
					savex = x_offset + 10;
					x_offset = x_offset + 20;
					continue;
				}
				if (i == 9)
				{
					Draw_ColoredStretchPic (5 + savex, vid.height - (sb_round[4]->height * 1.6363) - 4, sb_round[4], 96, 78, (int)color_shift[0], (int)color_shift[1], (int)color_shift[2], 255);
					continue;
				}
				if (i > 4)
					icon_num = i - 5;
				else
					icon_num = i;

				Draw_ColoredStretchPic (5 + x_offset, vid.height - (sb_round[icon_num]->height * 1.6363) - 4, sb_round[icon_num], 18, 78, (int)color_shift[0], (int)color_shift[1], (int)color_shift[2], 255);

				x_offset = x_offset + (sb_round[icon_num]->width * 1.6363) + 3;
			}
		}
		else
		{
			if (cl.stats[STAT_ROUNDS] >= 100)
			{
				num[2] = (int)(cl.stats[STAT_ROUNDS]/100);
				Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[2]]->height * 1.6363) - 4, sb_round_num[num[2]], 52, 78, (int)color_shift[0], (int)color_shift[1], (int)color_shift[2], 255); //32x48
				x_offset = x_offset + (sb_round_num[num[2]]->width * 1.6363) - 8;
			}
			else
				num[2] = 0;
			if (cl.stats[STAT_ROUNDS] >= 10)
			{
				num[1] = (int)((cl.stats[STAT_ROUNDS] - num[2]*100)/10);
				Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[1]]->height * 1.6363) - 4, sb_round_num[num[1]], 52, 78, (int)color_shift[0], (int)color_shift[1], (int)color_shift[2], 255);
				x_offset = x_offset + (sb_round_num[num[1]]->width * 1.6363) - 8;
			}
			else
				num[1] = 0;

			num[0] = cl.stats[STAT_ROUNDS] - num[2]*100 - num[1]*10;
			Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[0]]->height * 1.6363) - 4, sb_round_num[num[0]], 52, 78, (int)color_shift[0], (int)color_shift[1], (int)color_shift[2], 255);
			x_offset = x_offset + (sb_round_num[num[0]]->width * 1.6363) - 8;
		}
	}
	else if (cl.stats[STAT_ROUNDCHANGE] == 4)//blink white
	{
		if (endroundchange > cl.time) {
			blinking = (((int)(realtime*475)&510) - 255);
			blinking = abs(blinking);
		} else {
			if (blinking)
				blinking = blinking - 1;
			else
				blinking = 0;
		}

		if (cl.stats[STAT_ROUNDS] > 0 && cl.stats[STAT_ROUNDS] < 11)
		{
			for (i = 0; i < cl.stats[STAT_ROUNDS]; i++)
			{
				if (i == 4)
				{
					Draw_ColoredStretchPic (5, vid.height - (sb_round[4]->height * 1.6363) - 4, sb_round[4], 96, 78, 255, 255, 255, blinking);
					savex = x_offset + 10;
					x_offset = x_offset + 10;
					continue;
				}
				if (i == 9)
				{
					Draw_ColoredStretchPic (5 + savex, vid.height - (sb_round[4]->height * 1.6363) - 4, sb_round[4], 96, 78, 255, 255, 255, blinking);
					continue;
				}
				if (i > 4)
					icon_num = i - 5;
				else
					icon_num = i;

				Draw_ColoredStretchPic (5 + x_offset, vid.height - (sb_round[icon_num]->height * 1.6363) - 4, sb_round[icon_num], 18, 78, 255, 255, 255, blinking);

				x_offset = x_offset + (sb_round[icon_num]->width * 1.6363) + 3;
			}
		}
		else
		{
			if (cl.stats[STAT_ROUNDS] >= 100)
			{
				num[2] = (int)(cl.stats[STAT_ROUNDS]/100);
				Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[2]]->height * 1.6363) - 4, sb_round_num[num[2]], 52, 78, 255, 255, 255, blinking);
				x_offset = x_offset + (sb_round_num[num[2]]->width * 1.6363) - 8;
			}
			else
				num[2] = 0;
			if (cl.stats[STAT_ROUNDS] >= 10)
			{
				num[1] = (int)((cl.stats[STAT_ROUNDS] - num[2]*100)/10);
				Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[1]]->height * 1.6363) - 4, sb_round_num[num[1]], 52, 78, 255, 255, 255, blinking);
				x_offset = x_offset + (sb_round_num[num[1]]->width * 1.6363) - 8;
			}
			else
				num[1] = 0;

			num[0] = cl.stats[STAT_ROUNDS] - num[2]*100 - num[1]*10;
			Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[0]]->height * 1.6363) - 4, sb_round_num[num[0]], 52, 78, 255, 255, 255, blinking);
			x_offset = x_offset + (sb_round_num[num[0]]->width * 1.6363) - 8;
		}

		if (endroundchange == 0) {
			endroundchange = cl.time + 7.5;
			blinking = 0;
		}
	}
	else if (cl.stats[STAT_ROUNDCHANGE] == 5)//blink white
	{
		if (blinking > 0)
			blinking = blinking - 10;
		if (blinking < 0)
			blinking = 0;
		if (cl.stats[STAT_ROUNDS] > 0 && cl.stats[STAT_ROUNDS] < 11)
		{
			for (i = 0; i < cl.stats[STAT_ROUNDS]; i++)
			{
				if (i == 4)
				{
					Draw_ColoredStretchPic (5, vid.height - (sb_round[4]->height * 1.6363) - 4, sb_round[4], 96, 78, 255, 255, 255, blinking);
					savex = x_offset + 10;
					x_offset = x_offset + 10;
					continue;
				}
				if (i == 9)
				{
					Draw_ColoredStretchPic (5 + savex, vid.height - (sb_round[4]->height * 1.6363) - 4, sb_round[4], 96, 78, 255, 255, 255, blinking);
					continue;
				}
				if (i > 4)
					icon_num = i - 5;
				else
					icon_num = i;

				Draw_ColoredStretchPic (5 + x_offset, vid.height - (sb_round[icon_num]->height * 1.6363) - 4, sb_round[icon_num], 18, 78, 255, 255, 255, blinking);

				x_offset = x_offset + (sb_round[icon_num]->width * 1.6363) + 3;
			}
		}
		else
		{
			if (cl.stats[STAT_ROUNDS] >= 100)
			{
				num[2] = (int)(cl.stats[STAT_ROUNDS]/100);
				Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[2]]->height * 1.6363) - 4, sb_round_num[num[2]], 52, 78, 255, 255, 255, blinking);
				x_offset = x_offset + (sb_round_num[num[2]]->width * 1.6363) - 8;
			}
			else
				num[2] = 0;
			if (cl.stats[STAT_ROUNDS] >= 10)
			{
				num[1] = (int)((cl.stats[STAT_ROUNDS] - num[2]*100)/10);
				Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[1]]->height * 1.6363) - 4, sb_round_num[num[1]], 52, 78, 255, 255, 255, blinking);
				x_offset = x_offset + (sb_round_num[num[1]]->width * 1.6363) - 8;
			}
			else
				num[1] = 0;

			num[0] = cl.stats[STAT_ROUNDS] - num[2]*100 - num[1]*10;
			Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[0]]->height * 1.6363) - 4, sb_round_num[num[0]], 52, 78, 255, 255, 255, blinking);
			x_offset = x_offset + (sb_round_num[num[0]]->width * 1.6363) - 8;
		}
	}
	else if (cl.stats[STAT_ROUNDCHANGE] == 6)//blink white while fading back
	{
		if (endroundchange) {
			endroundchange = 0;
			blinking = 0;
		}

		color_shift_init = 0;

		blinking += (int)(host_frametime*475);
		if (blinking > 255) blinking = 255;

		if (cl.stats[STAT_ROUNDS] > 0 && cl.stats[STAT_ROUNDS] < 11)
		{
			for (i = 0; i < cl.stats[STAT_ROUNDS]; i++)
			{
				if (i == 4)
				{
					Draw_ColoredStretchPic (5, vid.height - (sb_round[4]->height * 1.6363) - 4, sb_round[4], 96, 78, 255, 255, 255, blinking);
					savex = x_offset + 10;
					x_offset = x_offset + 10;
					continue;
				}
				if (i == 9)
				{
					Draw_ColoredStretchPic (5 + savex, vid.height - (sb_round[4]->height * 1.6363) - 4, sb_round[4], 96, 78, 255, 255, 255, blinking);
					continue;
				}
				if (i > 4)
					icon_num = i - 5;
				else
					icon_num = i;

				Draw_ColoredStretchPic (5 + x_offset, vid.height - (sb_round[icon_num]->height * 1.6363) - 4, sb_round[icon_num], 18, 78, 255, 255, 255, blinking);

				x_offset = x_offset + (sb_round[icon_num]->width * 1.6363) + 3;
			}
		}
		else
		{
			if (cl.stats[STAT_ROUNDS] >= 100)
			{
				num[2] = (int)(cl.stats[STAT_ROUNDS]/100);
				Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[2]]->height * 1.6363) - 4, sb_round_num[num[2]], 52, 78, 255, 255, 255, blinking);
				x_offset = x_offset + (sb_round_num[num[2]]->width * 1.6363) - 8;
			}
			else
				num[2] = 0;
			if (cl.stats[STAT_ROUNDS] >= 10)
			{
				num[1] = (int)((cl.stats[STAT_ROUNDS] - num[2]*100)/10);
				Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[1]]->height * 1.6363) - 4, sb_round_num[num[1]], 52, 78, 255, 255, 255, blinking);
				x_offset = x_offset + (sb_round_num[num[1]]->width * 1.6363) - 8;
			}
			else
				num[1] = 0;

			num[0] = cl.stats[STAT_ROUNDS] - num[2]*100 - num[1]*10;
			Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[0]]->height * 1.6363) - 4, sb_round_num[num[0]], 52, 78, 255, 255, 255, blinking);
			x_offset = x_offset + (sb_round_num[num[0]]->width * 1.6363) - 8;
		}
	}
	else if (cl.stats[STAT_ROUNDCHANGE] == 7)//blink white while fading back
	{
		if (!color_shift_init)
		{
			color_shift_end[0] = 107;
			color_shift_end[1] = 1;
			color_shift_end[2] = 0;
			for (i = 0; i < 3; i++)
			{
				color_shift[i] = 255;
				color_shift_steps[i] = (color_shift[i] - color_shift_end[i])/60;
			}
			color_shift_init = 1;
		}
		for (i = 0; i < 3; i++)
		{
			if (color_shift[i] > color_shift_end[i])
				color_shift[i] = color_shift[i] - color_shift_steps[i];

			if (color_shift[i] < color_shift_end[i])
				color_shift[i] = color_shift_end[i];
		}
		if (cl.stats[STAT_ROUNDS] > 0 && cl.stats[STAT_ROUNDS] < 11)
		{
			for (i = 0; i < cl.stats[STAT_ROUNDS]; i++)
			{
				if (i == 4)
				{
					Draw_ColoredStretchPic (5, vid.height - (sb_round[4]->height * 1.6363) - 4, sb_round[4], 96, 78, (int)color_shift[0], (int)color_shift[1], (int)color_shift[2], 255);
					savex = x_offset + 10;
					x_offset = x_offset + 10;
					continue;
				}
				if (i == 9)
				{
					Draw_ColoredStretchPic (5 + savex, vid.height - (sb_round[4]->height * 1.6363) - 4, sb_round[4], 96, 78, (int)color_shift[0], (int)color_shift[1], (int)color_shift[2], 255);
					continue;
				}
				if (i > 4)
					icon_num = i - 5;
				else
					icon_num = i;

				Draw_ColoredStretchPic (5 + x_offset, vid.height - (sb_round[icon_num]->height * 1.6363) - 4, sb_round[icon_num], 18, 78, (int)color_shift[0], (int)color_shift[1], (int)color_shift[2], 255);

				x_offset = x_offset + (sb_round[icon_num]->width * 1.6363) + 3;
			}
		}
		else
		{
			if (cl.stats[STAT_ROUNDS] >= 100)
			{
				num[2] = (int)(cl.stats[STAT_ROUNDS]/100);
				Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[2]]->height * 1.6363) - 4, sb_round_num[num[2]], 52, 78, (int)color_shift[0], (int)color_shift[1], (int)color_shift[2], 255);
				x_offset = x_offset + (sb_round_num[num[2]]->width * 1.6363) - 8;
			}
			else
				num[2] = 0;
			if (cl.stats[STAT_ROUNDS] >= 10)
			{
				num[1] = (int)((cl.stats[STAT_ROUNDS] - num[2]*100)/10);
				Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[1]]->height * 1.6363) - 4, sb_round_num[num[1]], 52, 78, (int)color_shift[0], (int)color_shift[1], (int)color_shift[2], 255);
				x_offset = x_offset + (sb_round_num[num[1]]->width * 1.6363) - 8;
			}
			else
				num[1] = 0;

			num[0] = cl.stats[STAT_ROUNDS] - num[2]*100 - num[1]*10;
			Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[0]]->height * 1.6363) - 4, sb_round_num[num[0]], 52, 78, (int)color_shift[0], (int)color_shift[1], (int)color_shift[2], 255);
			x_offset = x_offset + (sb_round_num[num[0]]->width * 1.6363) - 8;
		}
	}
	else
	{
		color_shift[0] = 107;
		color_shift[1] = 1;
		color_shift[2] = 0;
		color_shift_init = 0;
		alphabling = 0;
		if (cl.stats[STAT_ROUNDS] > 0 && cl.stats[STAT_ROUNDS] < 11)
		{
			for (i = 0; i < cl.stats[STAT_ROUNDS]; i++)
			{
				if (i == 4)
				{
					Draw_ColoredStretchPic (5, vid.height - (sb_round[4]->height * 1.6363) - 4, sb_round[4], 96, 78, 107, 1, 0, 255);
					savex = x_offset + 10;
					x_offset = x_offset + 10;
					continue;
				}
				if (i == 9)
				{
					Draw_ColoredStretchPic (5 + savex, vid.height - (sb_round[4]->height * 1.6363) - 4, sb_round[4], 96, 78, 107, 1, 0, 255);
					continue;
				}
				if (i > 4)
					icon_num = i - 5;
				else
					icon_num = i;

				Draw_ColoredStretchPic (5 + x_offset, vid.height - (sb_round[icon_num]->height * 1.6363) - 4, sb_round[icon_num], 18, 78, 107, 1, 0, 255);

				x_offset = x_offset + (sb_round[icon_num]->width * 1.6363) + 3;
			}
		}
		else
		{
			if (cl.stats[STAT_ROUNDS] >= 100)
			{
				num[2] = (int)(cl.stats[STAT_ROUNDS]/100);
				Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[2]]->height * 1.6363) - 4, sb_round_num[num[2]], 52, 78, 107, 1, 0, 255);
				x_offset = x_offset + (sb_round_num[num[2]]->width * 1.6363) - 8;
			}
			else
				num[2] = 0;
			if (cl.stats[STAT_ROUNDS] >= 10)
			{
				num[1] = (int)((cl.stats[STAT_ROUNDS] - num[2]*100)/10);
				Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[1]]->height * 1.6363) - 4, sb_round_num[num[1]], 52, 78, 107, 1, 0, 255);
				x_offset = x_offset + (sb_round_num[num[1]]->width * 1.6363) - 8;
			}
			else
				num[1] = 0;

			num[0] = cl.stats[STAT_ROUNDS] - num[2]*100 - num[1]*10;
			
			if(cl.stats[STAT_ROUNDS] == 0)
				return;
			
			Draw_ColoredStretchPic (2 + x_offset, vid.height - (sb_round_num[num[0]]->height * 1.6363) - 4, sb_round_num[num[0]], 52, 78, 107, 1, 0, 255);
			x_offset = x_offset + (sb_round_num[num[0]]->width * 1.6363) - 8;
		}
	}
}
/*
===============
HUD_Perks
===============
*/
#define 	P_JUG		1
#define 	P_DOUBLE	2
#define 	P_SPEED		4
#define 	P_REVIVE	8
#define 	P_FLOP		16
#define 	P_STAMIN	32
#define 	P_DEAD 		64
#define 	P_MULE 		128

int perk_order[9];
int current_perk_order;

void HUD_Perks (void)
{

	int x, y, scale;

	x = 48;
	y = 8;
	scale = 36;

	// Double-Tap 2.0 specialty icon
	qpic_t* double_tap_icon;
	if (doubletap_has_damage_buff)
		double_tap_icon = doublepic2;
	else
		double_tap_icon = doublepic;

	// Draw second column first -- these need to be
	// overlayed below the first column.
	for (int i = 4; i < 8; i++) {
		if (perk_order[i]) {
			if (perk_order[i] == P_JUG) {Draw_StretchPic(x, y, jugpic, scale, scale);}
			if (perk_order[i] == P_DOUBLE) {Draw_StretchPic(x, y, double_tap_icon, scale, scale);}
			if (perk_order[i] == P_SPEED) {Draw_StretchPic(x, y, speedpic, scale, scale);}
			if (perk_order[i] == P_REVIVE) {Draw_StretchPic(x, y, revivepic, scale, scale);}
			if (perk_order[i] == P_FLOP) {Draw_StretchPic(x, y, floppic, scale, scale);}
			if (perk_order[i] == P_STAMIN) {Draw_StretchPic(x, y, staminpic, scale, scale);}
			if (perk_order[i] == P_DEAD) {Draw_StretchPic(x, y, deadpic, scale, scale);}
			if (perk_order[i] == P_MULE) {Draw_StretchPic(x, y, mulepic, scale, scale);}
		}
		y += scale;
	}

	x = 12;
	y = 8;

	// Now the first column.
	for (int i = 0; i < 4; i++) {
		if (perk_order[i]) {
			if (perk_order[i] == P_JUG) {Draw_StretchPic(x, y, jugpic, scale, scale);}
			if (perk_order[i] == P_DOUBLE) {Draw_StretchPic(x, y, double_tap_icon, scale, scale);}
			if (perk_order[i] == P_SPEED) {Draw_StretchPic(x, y, speedpic, scale, scale);}
			if (perk_order[i] == P_REVIVE) {Draw_StretchPic(x, y, revivepic, scale, scale);}
			if (perk_order[i] == P_FLOP) {Draw_StretchPic(x, y, floppic, scale, scale);}
			if (perk_order[i] == P_STAMIN) {Draw_StretchPic(x, y, staminpic, scale, scale);}
			if (perk_order[i] == P_DEAD) {Draw_StretchPic(x, y, deadpic, scale, scale);}
			if (perk_order[i] == P_MULE) {Draw_StretchPic(x, y, mulepic, scale, scale);}
		}
		y += scale;
	}

}

/*
===============
HUD_Powerups
===============
*/
void HUD_Powerups (void)
{

	int count = 0;

	// horrible way to offset check :)))))))))))))))))) :DDDDDDDD XOXO

	if (cl.stats[STAT_X2])
		count++;

	if (cl.stats[STAT_INSTA])
		count++;

	// both are avail draw fixed order
	if (count == 2) {
		Draw_StretchPic((vid.width/2) - 40, 435, x2pic, 36, 36);
		Draw_StretchPic(vid.width/2, 435, instapic, 36, 36);
	} else {
		if (cl.stats[STAT_X2])
			Draw_StretchPic((vid.width/2) - 40, 435, x2pic, 36, 36);
		if(cl.stats[STAT_INSTA])
			Draw_StretchPic ((vid.width/2), 435, instapic, 36, 36);
	}

}

/*
===============
HUD_ProgressBar
===============
*/
void HUD_ProgressBar (void)
{

	float progressbar;

	if (cl.progress_bar)
	{
		progressbar = 100 - ((cl.progress_bar-sv.time)*10);
		if (progressbar >= 100)
			progressbar = 100;

 		Draw_FillByColor  (((vid.width) >> 1) - 51, (int)(vid.height*0.75 - 1), 102, 5, 0, 0, 0,100);
 		Draw_FillByColor (((vid.width) >> 1) - 50, (int)((vid.height*3) >> 2), (int)progressbar, 3, 255, 255, 255,100);

		Draw_String ((vid.width - (88))/2, vid.height*0.75 + 10, "Reviving...");
	}

}

/*
===============
HUD_Achievement

Achievements based on code by Arkage
===============
*/


int		achievement; // the background image
int		achievement_unlocked;
char		achievement_text[MAX_QPATH];
double		achievement_time;
float smallsec;
int ach_pic;

void HUD_Achievement (void)
{
/*
	if (achievement_unlocked == 1)
	{
		smallsec = smallsec + 0.7;
		if (smallsec >= 55)
			smallsec = 55;
		//Background image
		//Sbar_DrawPic (176, 5, achievement);
		// The achievement text
		Draw_AlphaPic (30, smallsec - 50, achievement_list[ach_pic].img, 0.7f);
	}

	// Reset the achievement
	if (Sys_FloatTime() >= achievement_time)
	{
		achievement_unlocked = 0;
	}
*/
	return;
}

void HUD_Parse_Achievement (int ach)
{
/*
    if (achievement_list[ach].unlocked)
        return;

	achievement_unlocked = 1;
	smallsec = 0;
	achievement_time = Sys_FloatTime() + 10;
	ach_pic = ach;
	achievement_list[ach].unlocked = 1;
	//Save_Achivements();
*/
	return;
}

/*
===============
HUD_Ammo
===============
*/

int GetLowAmmo(int weapon, int type)
{
	switch (weapon)
	{
		case W_COLT: if (type) return 2; else return 16;
		case W_KAR: if (type) return 1; else return 10;
		case W_KAR_SCOPE: if (type) return 1; else return 10;
		case W_M1A1: if (type) return 4; else return 24;
		case W_SAWNOFF: if (type) return 1; else return 12;
		case W_DB: if (type) return 1; else return 12;
		case W_THOMPSON: if (type) return 6; else return 40;
		case W_BAR: if (type) return 6; else return 28;
		default: return 0;
	}
}

int IsDualWeapon(int weapon)
{
	switch(weapon) {
		case W_BIATCH:
		case W_SNUFF:
			return 1;
		default:
			return 0;
	}

	return 0;
}

void HUD_Ammo (void)
{
	char* magstring;
	int reslen;

	reslen = strlen(va("/%i", cl.stats[STAT_AMMO]));

	//
	// Magazine
	//
	magstring = va("%i", cl.stats[STAT_CURRENTMAG]);
	if (GetLowAmmo(cl.stats[STAT_ACTIVEWEAPON], 1) >= cl.stats[STAT_CURRENTMAG]) {
		Draw_ColoredString((565-(reslen*12)) - strlen(magstring)*12, 452, magstring, 255, 0, 0, 255, 1.5);
	} else {
		Draw_ColoredString((565-(reslen*12)) - strlen(magstring)*12, 452, magstring, 255, 255, 255, 255, 1.5);
	}

	//
	// Reserve Ammo
	//
	magstring = va("/%i", cl.stats[STAT_AMMO]);
	if (GetLowAmmo(cl.stats[STAT_ACTIVEWEAPON], 0) >= cl.stats[STAT_AMMO]) {
		Draw_ColoredString(565 - strlen(magstring)*12, 452, magstring, 255, 0, 0, 255, 1.5);
	} else {
		Draw_ColoredString(565 - strlen(magstring)*12, 452, magstring, 255, 255, 255, 255, 1.5);
	}
}

/*
===============
HUD_AmmoString
===============
*/

void HUD_AmmoString (void)
{

	if (GetLowAmmo(cl.stats[STAT_ACTIVEWEAPON], 1) >= cl.stats[STAT_CURRENTMAG])
	{
		int x;

		if (0 < cl.stats[STAT_AMMO] && cl.stats[STAT_CURRENTMAG] >= 0) {
			x = (vid.width - strlen("Reload")*12)/2;
			Draw_ColoredString (x, 340, "Reload", 255, 255, 255, 255, 1.6);
		} else if (0 < cl.stats[STAT_CURRENTMAG]) {
			x = (vid.width - strlen("LOW AMMO")*12)/2;
			Draw_ColoredString (x, 340, "LOW AMMO", 255, 255, 0, 255, 1.6);
		} else {
			x = (vid.width - strlen("NO AMMO")*12)/2;
			Draw_ColoredString (x, 340, "NO AMMO", 255, 0, 0, 255, 1.6);
		}
	}

}

/*
===============
HUD_Grenades
===============
*/
#define 	UI_FRAG		1
#define 	UI_BETTY	2

void HUD_Grenades (void)
{

	Draw_StretchPic (565, 430, fragpic, 36, 36);
	if (cl.stats[STAT_GRENADES] & UI_FRAG)
	{
		if (cl.stats[STAT_PRIGRENADES] <= 0)
			Draw_ColoredString (585, 452, va ("%i",cl.stats[STAT_PRIGRENADES]), 255, 0, 0, 255, 1.5);
		else
			Draw_String (585, 452, va ("%i",cl.stats[STAT_PRIGRENADES]));
	}
	if (cl.stats[STAT_GRENADES] & UI_BETTY)
	{
		Draw_StretchPic (600, 430, bettypic, 36, 36);
		if (cl.stats[STAT_PRIGRENADES] <= 0)
			Draw_ColoredString (620, 452, va ("%i",cl.stats[STAT_SECGRENADES]), 255, 0, 0, 255, 1.5);
		else
			Draw_String (620, 452, va ("%i",cl.stats[STAT_SECGRENADES]));
	}

}

/*
===============
HUD_Weapon
===============
*/
void HUD_Weapon (void)
{
	char str[32];
	float l;
	x_value = vid.width;
	y_value = 430;

	strcpy(str, pr_strings+sv_player->v.Weapon_Name);
	l = strlen(str);

	x_value = 565 - l*12;
	Draw_ColoredString (x_value, y_value, str, 255, 255, 255, 255, 1.5);
}

/*
===============
HUD_BettyPrompt
===============
*/
extern qpic_t *b_minus;
void HUD_BettyPrompt (void)
{

	char str[32];
	char str2[32];

	strcpy(str, "Tap   to\n");
	strcpy(str2, "place a Bouncing Betty\n");

	int x, x2;
	x = (vid.width - strlen(str)*12)/2;
	x2 = (vid.width - strlen(str2)*12)/2;

	Draw_String(x, 70, str);
	Draw_String(x2, 90, str2);
	Draw_Pic (x + 45, 68, b_minus);

}

/*
===============
HUD_PlayerName
===============
*/
void HUD_PlayerName (void)
{
	int alpha = 255;

	if (nameprint_time - sv.time < 1)
		alpha = (int)((nameprint_time - sv.time)*255);

	Draw_ColoredString(118, 365, player_name, 255, 255, 255, alpha, 1.4);
}

/*
===============
HUD_Screenflash
===============
*/

//
// Types of screen-flashes.
//

// Colors
#define SCREENFLASH_COLOR_WHITE			0
#define SCREENFLASH_COLOR_BLACK			1

// Types
#define SCREENFLASH_FADE_INANDOUT		0
#define SCREENFLASH_FADE_IN 			1
#define SCREENFLASH_FADE_OUT 			2

//
// invert float takes in float value between 0 and 1, inverts position
// eg: 0.1 returns 0.9, 0.34 returns 0.66
float invertfloat(float input) {
    if (input < 0)
        return 0; // adjust to lower boundary
    else if (input > 1)
        return 1; // adjust to upper boundary
    else
        return (1 - input);
}

void HUD_Screenflash (void)
{

	int r, g, b, a;
	float flash_alpha;

	double percentage_complete = screenflash_worktime / (screenflash_duration - screenflash_starttime);

	// Fade Out
	if (screenflash_type == SCREENFLASH_FADE_OUT) {
		flash_alpha = invertfloat((float)percentage_complete);
	}
	// Fade In
	else if (screenflash_type == SCREENFLASH_FADE_IN) {
		flash_alpha = (float)percentage_complete;
	}
	// Fade In + Fade Out
	else {
		// Fade In
		if (percentage_complete < 0.5) {
			flash_alpha = (float)percentage_complete;// was *2 but this was far too quick
		} 
		// Fade Out
		else {
			flash_alpha = invertfloat((float)percentage_complete);//*2
		}
	}

	// Obtain the flash color
	switch(screenflash_color) {
		case SCREENFLASH_COLOR_BLACK: r = 0; g = 0; b = 0; a = (int)(flash_alpha * 255); break;
		case SCREENFLASH_COLOR_WHITE: r = 255; g = 255; b = 255; a = (int)(flash_alpha * 255); break;
		default: r = 255; g = 0; b = 0; a = 255; break;
	}

	screenflash_worktime += host_frametime;
	Draw_FillByColor(0, 0, vid.width, vid.height, r, g, b, a);

}

/*
===============
HUD_Draw
===============
*/
void HUD_Draw (void)
{
	if (scr_con_current == vid.height)
		return;		// console is full screen

	if (key_dest == key_menu_pause) {
		//Make sure we still draw the screen flash.
		if (screenflash_duration > sv.time)
			HUD_Screenflash();
		return;
	}

	scr_copyeverything = 1;

	if (waypoint_mode.value)
	{
		Draw_String (vid.width - 112, 8, "WAYPOINTMODE");
		Draw_String (vid.width - 240, 20, "Press fire to create waypoint");
		Draw_String (vid.width - 232, 32, "Press use to select waypoint");
		Draw_String (vid.width - 216, 44, "Press aim to link waypoint");
		Draw_String (vid.width - 248, 56, "Press knife to remove waypoint");
		Draw_String (vid.width - 272, 68, "Press switch to move waypoint here");
		Draw_String (vid.width - 304, 80, "Press reload to make special waypoint");
		return;
	}

	if (cl.stats[STAT_HEALTH] <= 0 || showscoreboard == true)
	{
		HUD_EndScreen ();

		// Make sure we still draw the screen flash.
		if (screenflash_duration > sv.time)
			HUD_Screenflash();
		
		if (cl.stats[STAT_HEALTH] <= 0)
			return;
	}

	if (bettyprompt_time > sv.time)
		HUD_BettyPrompt();

	if (nameprint_time > sv.time)
		HUD_PlayerName();

	HUD_Blood();
	HUD_Rounds();
	HUD_Perks();
	HUD_Powerups();
	HUD_ProgressBar();
	if ((HUD_Change_time > Sys_FloatTime() || GetLowAmmo(cl.stats[STAT_ACTIVEWEAPON], 1) >= cl.stats[STAT_CURRENTMAG] || GetLowAmmo(cl.stats[STAT_ACTIVEWEAPON], 0) >= cl.stats[STAT_AMMO]) && cl.stats[STAT_HEALTH] >= 20)
	{ //these elements are only drawn when relevant for few seconds
		HUD_Ammo();
		HUD_Grenades();
		HUD_Weapon();
		HUD_AmmoString();
	}
	HUD_Points();
	HUD_Point_Change();
	HUD_Achievement();

	if (domaxammo == true) {
		if (maxammoopac <= 0) {
			maxammoopac = 255;
		}
		maxammoy = 100;
		HUD_MaxAmmo();
	}

	// This should always come last!
	if (screenflash_duration > sv.time)
		HUD_Screenflash();
}
