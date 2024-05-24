/*

Fogging system based on FitzQuake's implementation
Now with Quakespasm bits thrown into it!
Modified for Wii :)

*/

#include "../../generic/quakedef.h"

//==============================================================================
//
//  GLOBAL FOG
//
//==============================================================================
#define DEFAULT_GRAY 0.3

float fog_start;
float fog_end;
float fog_red;
float fog_green;
float fog_blue;

float old_start;
float old_end;
float old_red;
float old_green;
float old_blue;

float fade_time; //duration of fade
float fade_done; //time when fade will be done

GXColor BLACK = {0, 0, 0, 0};

/*
=============
Fog_Update

update internal variables
=============
*/
void Fog_Update (float start, float end, float red, float green, float blue, float time)
{
	//save previous settings for fade
	if (time > 0)
	{
		//check for a fade in progress
		if (fade_done > cl.time)
		{
			float f;

			f = (fade_done - cl.time) / fade_time;
			old_start = f * old_start + (1.0 - f) * fog_start;
			old_end = f * old_end + (1.0 - f) * fog_end;
			old_red = f * old_red + (1.0 - f) * fog_red;
			old_green = f * old_green + (1.0 - f) * fog_green;
			old_blue = f * old_blue + (1.0 - f) * fog_blue;
		}
		else
		{
			old_start = fog_start;
			old_end = fog_end;
			old_red = fog_red;
			old_green = fog_green;
			old_blue = fog_blue;
		}
	}

	fog_start = start;
	fog_end = end;
	fog_red = red;
	fog_green = green;
	fog_blue = blue;
	fade_time = time;
	fade_done = cl.time + time;
}

/*
=============
Fog_ParseServerMessage

handle an SVC_FOG message from server
=============
*/
void Fog_ParseServerMessage (void)
{
	float start, end, red, green, blue, time;

	start = MSG_ReadByte() / 255.0;
	end = MSG_ReadByte() / 255.0;
	red = MSG_ReadByte() / 255.0;
	green = MSG_ReadByte() / 255.0;
	blue = MSG_ReadByte() / 255.0;
	time = MSG_ReadShort() / 100.0;

	Fog_Update (start, end, red, green, blue, time);
}

/*
=============
Fog_FogCommand_f

handle the 'fog' console command
=============
*/
void Fog_FogCommand_f (void)
{
	switch (Cmd_Argc())
	{
	default:
	case 1:
		Con_Printf("usage:\n");
		Con_Printf("   fog <fade>\n");
		Con_Printf("   fog <start> <end>\n");
		Con_Printf("   fog <red> <green> <blue>\n");
		Con_Printf("   fog <fade> <red> <green> <blue>\n");
		Con_Printf("   fog <start> <end> <red> <green> <blue>\n");
		Con_Printf("   fog <start> <end> <red> <green> <blue> <fade>\n");
		Con_Printf("current values:\n");
		Con_Printf("   \"start\" is \"%f\"\n", fog_start);
		Con_Printf("   \"end\" is \"%f\"\n", fog_end);
		Con_Printf("   \"red\" is \"%f\"\n", fog_red);
		Con_Printf("   \"green\" is \"%f\"\n", fog_green);
		Con_Printf("   \"blue\" is \"%f\"\n", fog_blue);
		Con_Printf("   \"fade\" is \"%f\"\n", fade_time);
		break;
	case 2: //TEST
		Fog_Update(fog_start,
				   fog_end,
				   fog_red,
				   fog_green,
				   fog_blue,
				   atof(Cmd_Argv(1)));
		break;
	case 3:
		Fog_Update(atof(Cmd_Argv(1)),
				   atof(Cmd_Argv(2)),
				   fog_red,
				   fog_green,
				   fog_blue,
				   0.0);
		break;
	case 4:
		Fog_Update(fog_start,
				   fog_end,
				   CLAMP(0.0, atof(Cmd_Argv(1)), 100.0),
				   CLAMP(0.0, atof(Cmd_Argv(2)), 100.0),
				   CLAMP(0.0, atof(Cmd_Argv(3)), 100.0),
				   0.0);
		break;
	case 5: //TEST
		Fog_Update(fog_start,
				   fog_end,
				   CLAMP(0.0, atof(Cmd_Argv(1)), 100.0),
				   CLAMP(0.0, atof(Cmd_Argv(2)), 100.0),
				   CLAMP(0.0, atof(Cmd_Argv(3)), 100.0),
				   atof(Cmd_Argv(4)));
		break;
	case 6:
		Fog_Update(atof(Cmd_Argv(1)),
				   atof(Cmd_Argv(2)),
				   CLAMP(0.0, atof(Cmd_Argv(3)), 100.0),
				   CLAMP(0.0, atof(Cmd_Argv(4)), 100.0),
				   CLAMP(0.0, atof(Cmd_Argv(5)), 100.0),
				   0.0);
		break;
	case 7:
		Fog_Update(atof(Cmd_Argv(1)),
				   atof(Cmd_Argv(2)),
				   CLAMP(0.0, atof(Cmd_Argv(3)), 100.0),
				   CLAMP(0.0, atof(Cmd_Argv(4)), 100.0),
				   CLAMP(0.0, atof(Cmd_Argv(5)), 100.0),
				   atof(Cmd_Argv(6)));
		break;
	}
}

/*
=============
Fog_ParseWorldspawn

called at map load
=============
*/
void Fog_ParseWorldspawn (void)
{
	char key[128], value[4096];
	char *data;

	//initially no fog
	fog_start = 0;
	old_start = 0;

	fog_end = 4000;
	old_end = 4000;

	fog_red = 0.0;
	old_red = 0.0;

	fog_green = 0.0;
	old_green = 0.0;

	fog_blue = 0.0;
	old_blue = 0.0;

	fade_time = 0.0;
	fade_done = 0.0;

	data = COM_Parse(cl.worldmodel->entities);
	if (!data)
		return; // error
	if (com_token[0] != '{')
		return; // error
	while (1)
	{
		data = COM_Parse(data);
		if (!data)
			return; // error
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
			return; // error
		strcpy(value, com_token);

		if (!strcmp("fog", key))
		{
			sscanf(value, "%f %f %f %f %f", &fog_start, &fog_end, &fog_red, &fog_green, &fog_blue);
		}
	}
}

/*
=============
Fog_GetColor

calculates fog color for this frame, taking into account fade times
=============
*/
GXColor FogColor = {108, 122, 137, 255};

/*
=============
Fog_SetupFrame

called at the beginning of each frame
=============
*/
void Fog_SetupFrame (void)
{
	/*
	glFogfv(GL_FOG_COLOR, Fog_GetColor());
	glFogf(GL_FOG_DENSITY, 0.2f);
	glFogf(GL_FOG_START, fog_start);
	glFogf(GL_FOG_END, fog_end);
	*/
	
	//Wii documentation sucks hard.
	
	//GX_SetFog(GX_FOG_LIN, fog_start, fog_end, GX_PERSPECTIVE, GX_PERSPECTIVE, FogColor);	
	GX_SetFog(GX_FOG_LIN, 50, 300, GX_PERSPECTIVE, GX_PERSPECTIVE, FogColor);		
}

/*
=============
Fog_GetStart
returns current start of fog
=============
*/
float Fog_GetStart (void)
{
	float f;

	if (fade_done > cl.time)
	{
		f = (fade_done - cl.time) / fade_time;
		return f * old_start + (1.0 - f) * fog_start;
	}
	else
		return fog_start;
}

/*
=============
Fog_GetEnd
returns current end of fog
=============
*/
float Fog_GetEnd (void)
{
	float f;

	if (fade_done > cl.time)
	{
		f = (fade_done - cl.time) / fade_time;
		return f * old_start + (1.0 - f) * fog_end;
	}
	else
		return fog_end;
}

/*
=============
Fog_EnableGFog

called before drawing stuff that should be fogged
=============
*/
void Fog_EnableGFog (void)
{
	if (!Fog_GetStart() == 0 || !Fog_GetEnd() <= 0) {
		//glEnable(GL_FOG);
		//GX_SetFog(GX_FOG_LIN, fog_start, fog_end, GX_PERSPECTIVE, GX_PERSPECTIVE, FogColor);	
		GX_SetFog(GX_FOG_LIN, 50, 300, GX_PERSPECTIVE, GX_PERSPECTIVE, FogColor);	
	}	
}

/*
=============
Fog_DisableGFog

called after drawing stuff that should be fogged
=============
*/
void Fog_DisableGFog (void)
{
	if (!Fog_GetStart() == 0 || !Fog_GetEnd() <= 0)
		//glDisable(GL_FOG);
		GX_SetFog(GX_FOG_NONE, 0.0F, 1.0F, 0.1F, 1.0F, BLACK);
}

//==============================================================================
//
//  VOLUMETRIC FOG
//
//==============================================================================

//==============================================================================
//
//  INIT
//
//==============================================================================

/*
=============
Fog_NewMap

called whenever a map is loaded
=============
*/

void Fog_NewMap (void)
{
	Fog_ParseWorldspawn (); //for global fog
}

/*
=============
Fog_Init

called when quake initializes
=============
*/
void Fog_Init (void)
{
	Cmd_AddCommand ("fog",Fog_FogCommand_f);

	//set up global fog
	fog_start = 300;
	fog_end = 4000;
	fog_red = 0.5;
	fog_green = 0.5;
	fog_blue = 0.5;
	fade_time = 1;
	fade_time = 1;

	//glFogi(GL_FOG_MODE, GL_LINEAR);
	GX_SetFog(GX_FOG_LIN, 0.0F, 1.0F, 0.1F, 1.0F, BLACK);	
}