/*
Copyright (C) 2008 Eluan Costa Miranda

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

#include <ogc/cache.h>
#include <ogc/system.h>
#include <ogc/lwp_heap.h>
#include <ogc/lwp_mutex.h>

#include "../../generic/quakedef.h"

#include <gccore.h>
#include <malloc.h>

// ELUTODO: GL_Upload32 and GL_Update32 could use some optimizations
// ELUTODO: mipmap

cvar_t		gl_max_size = {"gl_max_size", "1024"};
cvar_t 		vid_retromode = {"vid_retromode", "1", false};

gltexture_t	gltextures[MAX_GLTEXTURES];
int			numgltextures;

heap_cntrl texture_heap;
void *texture_heap_ptr;
u32 texture_heap_size;

void R_InitTextureHeap (void)
{
	
	u32 level, size;

	_CPU_ISR_Disable(level);
	texture_heap_ptr = SYS_GetArena2Lo();
	texture_heap_size = 21 * 1024 * 1024;
	if ((u32)texture_heap_ptr + texture_heap_size > (u32)SYS_GetArena2Hi())
	{
		_CPU_ISR_Restore(level);
		Sys_Error("texture_heap + texture_heap_size > (u32)SYS_GetArena2Hi()");
	}	
	else
	{
		SYS_SetArena2Lo(texture_heap_ptr + texture_heap_size);
		_CPU_ISR_Restore(level);
	}

	memset(texture_heap_ptr, 0, texture_heap_size);

	size = __lwp_heap_init(&texture_heap, texture_heap_ptr, texture_heap_size, PPC_CACHE_ALIGNMENT);

	Con_Printf("Allocated %dM texture heap.\n", size / (1024 * 1024));
	
}

/*
==================
R_InitTextures
==================
*/
void	R_InitTextures (void)
{
	/*
	int		x,y, m;
	byte	*dest;
	*/

	R_InitTextureHeap();

	Cvar_RegisterVariable (&gl_max_size);

	numgltextures = 0;
	/*
// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16*16+8*8+4*4+2*2, "notexture");
	
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;
	
	for (m=0 ; m<4 ; m++)
	{
		dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}	
	*/
}

void GL_Bind0 (int texnum)
{
	if (currenttexture0 == texnum)
		return;

	if (!gltextures[texnum].used)
		Sys_Error("Tried to bind a inactive texture0.");

	currenttexture0 = texnum;
	GX_LoadTexObj(&(gltextures[texnum].gx_tex), GX_TEXMAP0);
}

void GL_Bind1 (int texnum)
{
	if (currenttexture1 == texnum)
		return;

	if (!gltextures[texnum].used)
		Sys_Error("Tried to bind a inactive texture1.");

	currenttexture1 = texnum;
	GX_LoadTexObj(&(gltextures[texnum].gx_tex), GX_TEXMAP1);
}

void GX_SetMinMag (int minfilt, int magfilt)
{
	if(gltextures[currenttexture0].data != NULL)
	{
		GX_InitTexObjFilterMode(&gltextures[currenttexture0].gx_tex, minfilt, magfilt);
	};
}

void GX_SetMaxAniso (int aniso)
{
	if(gltextures[currenttexture0].data != NULL)
	{
		GX_InitTexObjMaxAniso(&gltextures[currenttexture0].gx_tex, aniso);
	};
}

void QGX_ZMode(qboolean state)
{
	if (state)
		GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	else
		GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_TRUE);
}

void QGX_Alpha(qboolean state)
{
	if (state)
		GX_SetAlphaCompare(GX_GREATER,0,GX_AOP_AND,GX_ALWAYS,0);
	else
		GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
	
}

void QGX_Blend(qboolean state)
{
	if (state)
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	else
		GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
}

void QGX_BlendMap(qboolean state)
{
	if (state)
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_DSTCLR, GX_BL_SRCCLR, GX_LO_CLEAR);
	else
		GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
}

//====================================================================

/*
================
GL_FindTexture
================
*/
int GL_FindTexture (char *identifier)
{
	int		i;
	gltexture_t	*glt;

	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (gltextures[i].used)
			if (!strcmp (identifier, glt->identifier))
				return gltextures[i].texnum;
	}

	return -1;
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	*inrow;
	unsigned	frac, fracstep;

	fracstep = inwidth*0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j+=4)
		{
			out[j] = inrow[frac>>16];
			frac += fracstep;
			out[j+1] = inrow[frac>>16];
			frac += fracstep;
			out[j+2] = inrow[frac>>16];
			frac += fracstep;
			out[j+3] = inrow[frac>>16];
			frac += fracstep;
		}
	}
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GX_MipMap (byte *in, int width, int height)
{
	int		i, j;
	byte	*out;

	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}

void GX_DeleteTexData(int texnum)
{
	if (gltextures[texnum].data != NULL) {
		__lwp_heap_free(&texture_heap, gltextures[texnum].data);
		gltextures[texnum].data = NULL;
	}
}

void GX_ReallocTextureCache(int texnum, u32 newmem, int width, int height)
{
	GX_DeleteTexData(texnum);
	__lwp_heap_allocate(&texture_heap, newmem);
	if(gltextures[texnum].data == NULL)
	{
		Sys_Error("GX_ReallocTextureCache: allocation failed on %i bytes", newmem);
	};
		
	gltextures[texnum].width = width;
	gltextures[texnum].height = height;
	//GX_InvalidateTexAll();
}

// FIXME, temporary
static	unsigned	scaled[640*480];
static	unsigned	trans[640*480];

/*
===============
GL_Upload32
===============
*/
void GL_Upload32 (gltexture_t *destination, unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha, qboolean flipRGBA)
{
	int			i, x, y, s;
	u8			*pos;
	int			scaled_width, scaled_height;

	for (scaled_width = 1 << 5 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 << 5 ; scaled_height < height ; scaled_height<<=1)
		;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	// ELUTODO: gl_max_size should be multiple of 32?
	// ELUTODO: mipmaps

	if (scaled_width * scaled_height > sizeof(scaled)/4)
		Sys_Error ("GL_Upload32: too big");
	
	if (scaled_width != width || scaled_height != height)
	{
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);
	}
	else
	{
		memcpy(scaled, data, scaled_width * scaled_height * 4/*sizeof(data)*/);
	}
	
	destination->data = __lwp_heap_allocate(&texture_heap, scaled_width * scaled_height * 4/*sizeof(data)*/);
	
	if (!destination->data)
		Sys_Error("GL_Upload32: Out of memory.");

	destination->scaled_width = scaled_width;
	destination->scaled_height = scaled_height;

	s = scaled_width * scaled_height;
	if (s & 31)
		Sys_Error ("GL_Upload32: s&31");

	if ((int)destination->data & 31)
		Sys_Error ("GL_Upload32: destination->data&31");

	// sB: barfoo34 kindly pointed out that this appears to be 
	// expecting ARGB, when it actually expects ABGR
	// thus causing my texture profiling confusion.
	// So now I am reordering this to store the textures
	// as RGBA in order to restore compatability.
	
	if (flipRGBA) {
		pos = (u8 *)destination->data;
		for (y = 0; y < scaled_height; y += 4)
		{
			u8* row1 = (u8 *)&(scaled[scaled_width * (y + 0)]);
			u8* row2 = (u8 *)&(scaled[scaled_width * (y + 1)]);
			u8* row3 = (u8 *)&(scaled[scaled_width * (y + 2)]);
			u8* row4 = (u8 *)&(scaled[scaled_width * (y + 3)]);

			for (x = 0; x < scaled_width; x += 4)
			{
				u8 AR[32];
				u8 GB[32];

				for (i = 0; i < 4; i++)
				{
					u8* ptr1 = &(row1[(x + i) * 4]);
					u8* ptr2 = &(row2[(x + i) * 4]);
					u8* ptr3 = &(row3[(x + i) * 4]);
					u8* ptr4 = &(row4[(x + i) * 4]);

					AR[(i * 2) +  0] = ptr1[0];
					AR[(i * 2) +  1] = ptr1[3];
					AR[(i * 2) +  8] = ptr2[0];
					AR[(i * 2) +  9] = ptr2[3];
					AR[(i * 2) + 16] = ptr3[0];
					AR[(i * 2) + 17] = ptr3[3];
					AR[(i * 2) + 24] = ptr4[0];
					AR[(i * 2) + 25] = ptr4[3];

					GB[(i * 2) +  0] = ptr1[2];
					GB[(i * 2) +  1] = ptr1[1];
					GB[(i * 2) +  8] = ptr2[2];
					GB[(i * 2) +  9] = ptr2[1];
					GB[(i * 2) + 16] = ptr3[2];
					GB[(i * 2) + 17] = ptr3[1];
					GB[(i * 2) + 24] = ptr4[2];
					GB[(i * 2) + 25] = ptr4[1];
				}

				memcpy(pos, AR, sizeof(AR));
				pos += sizeof(AR);
				memcpy(pos, GB, sizeof(GB));
				pos += sizeof(GB);
			}
		}
	} else {
		pos = (u8 *)destination->data;
		for (y = 0; y < scaled_height; y += 4)
		{
			u8* row1 = (u8 *)&(scaled[scaled_width * (y + 0)]);
			u8* row2 = (u8 *)&(scaled[scaled_width * (y + 1)]);
			u8* row3 = (u8 *)&(scaled[scaled_width * (y + 2)]);
			u8* row4 = (u8 *)&(scaled[scaled_width * (y + 3)]);

			for (x = 0; x < scaled_width; x += 4)
			{
				u8 RG[32];
				u8 BA[32];

				for (i = 0; i < 4; i++)
				{
					u8* ptr1 = &(row1[(x + i) * 4]);
					u8* ptr2 = &(row2[(x + i) * 4]);
					u8* ptr3 = &(row3[(x + i) * 4]);
					u8* ptr4 = &(row4[(x + i) * 4]);

					RG[(i * 2) +  0] = ptr1[3];
					RG[(i * 2) +  1] = ptr1[0];
					RG[(i * 2) +  8] = ptr2[3];
					RG[(i * 2) +  9] = ptr2[0];
					RG[(i * 2) + 16] = ptr3[3];
					RG[(i * 2) + 17] = ptr3[0];
					RG[(i * 2) + 24] = ptr4[3];
					RG[(i * 2) + 25] = ptr4[0];

					BA[(i * 2) +  0] = ptr1[1];
					BA[(i * 2) +  1] = ptr1[2];
					BA[(i * 2) +  8] = ptr2[1];
					BA[(i * 2) +  9] = ptr2[2];
					BA[(i * 2) + 16] = ptr3[1];
					BA[(i * 2) + 17] = ptr3[2];
					BA[(i * 2) + 24] = ptr4[1];
					BA[(i * 2) + 25] = ptr4[2];
				}

				memcpy(pos, RG, sizeof(RG));
				pos += sizeof(RG);
				memcpy(pos, BA, sizeof(BA));
				pos += sizeof(BA);
			}
		}
	}
	
	GX_InitTexObj(&destination->gx_tex, destination->data, scaled_width, scaled_height, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, /*mipmap ? GX_TRUE :*/ GX_FALSE);
	DCFlushRange(destination->data, scaled_width * scaled_height * 4/*sizeof(data)*/);
	
	// sBTODO finish mipmap implementation 
	// need to verify if the new memory takes up more space
	// if so reallocate tex
	/*
	if (mipmap == true) {
		int mip_level;
		int texnum;
		int sw;
		int sh;
		u32 mip_mem;
		
		mip_level = 0;
		texnum = gltextures[currenttexture0].texnum;
		sw = scaled_width;
		sh = scaled_height;
		while (sw > 4 && sh > 4)
		{
			sw >>= 1;
			sh >>= 1;
			mip_level++;
		};
		
		mip_mem = GX_GetTexBufferSize(scaled_width, scaled_height, GX_TF_RGBA8, GX_TRUE, mip_level);
		GX_ReallocTextureCache(texnum, mip_mem, scaled_width, scaled_height);
		
		while (scaled_width > 4 || scaled_height > 4) {
			GX_MipMap ((byte *)destination->data, scaled_width, scaled_height);
			
			scaled_width >>= 1;
			scaled_height >>= 1;
			
			destination->width = scaled_width;
			destination->height = scaled_height;
			
			GX_InitTexObj(&destination->gx_tex, destination->data, scaled_width, scaled_height, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, GX_TRUE);
			DCFlushRange(destination->data, mip_mem);
		
			if (vid_retromode.value == 1) {
				GX_InitTexObjFilterMode(&destination->gx_tex, GX_NEAR_MIP_NEAR, GX_NEAR_MIP_NEAR);
			} else {
				GX_InitTexObjFilterMode(&destination->gx_tex, GX_LIN_MIP_LIN, GX_LIN_MIP_LIN);
			}
			
		}
	}
	*/
}

/*
===============
GL_Upload8
===============
*/
void GL_Upload8 (gltexture_t *destination, byte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, s;
	qboolean	noalpha;
	int			p;

	s = width*height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	
	if (alpha)
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			if (p == 255)
				noalpha = false;
			trans[i] = d_8to24table[p];
		}

		if (alpha && noalpha)
			alpha = false;
	}
	else
	{
		if (s&3)
			Sys_Error ("GL_Upload8: s&3");
		for (i=0 ; i<s ; i+=4)
		{
			trans[i] = d_8to24table[data[i]];
			trans[i+1] = d_8to24table[data[i+1]];
			trans[i+2] = d_8to24table[data[i+2]];
			trans[i+3] = d_8to24table[data[i+3]];
		}
	}

	GL_Upload32 (destination, trans, width, height, mipmap, alpha, true);
}

byte		vid_gamma_table[256];
void Build_Gamma_Table (void) {
	int		i;
	float		inf;
	float   in_gamma;

	if ((i = COM_CheckParm("-gamma")) != 0 && i+1 < com_argc) {
		in_gamma = Q_atof(com_argv[i+1]);
		if (in_gamma < 0.3) in_gamma = 0.3;
		if (in_gamma > 1) in_gamma = 1.0;
	} else {
		in_gamma = 1;
	}

	if (in_gamma != 1) {
		for (i=0 ; i<256 ; i++) {
			inf = min(255 * pow((i + 0.5) / 255.5, in_gamma) + 0.5, 255);
			vid_gamma_table[i] = inf;
		}
	} else {
		for (i=0 ; i<256 ; i++)
			vid_gamma_table[i] = i;
	}

}

/*
================
GL_LoadTexture
================
*/

//Diabolickal TGA Begin

int lhcsumtable[256];
int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, qboolean keep, int bytesperpixel)
{
	int			i, s, lhcsum;
	gltexture_t	*glt;
	// occurances. well this isn't exactly a checksum, it's better than that but
	// not following any standards.
	lhcsum = 0;
	s = width*height*bytesperpixel;
	
	for (i = 0;i < 256;i++) lhcsumtable[i] = i + 1;
	for (i = 0;i < s;i++) lhcsum += (lhcsumtable[data[i] & 255]++);

	// see if the texture is allready present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			if (glt->used)
			{
				// ELUTODO: causes problems if we compare to a texture with NO name?
				// sBTODO we definitely have issues with identifier strings. will investigate later..
				if (!strcmp (identifier, glt->identifier))
				{
					if (width != glt->width || height != glt->height)
					{
						Con_Printf ("GL_LoadTexture: cache mismatch, reloading");
						if (!__lwp_heap_free(&texture_heap, glt->data))
							Sys_Error("GL_ClearTextureCache: Error freeing data.");
						goto reload; // best way to do it
					}
					return glt->texnum;
				}
			}
		}
	}

	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if (!glt->used)
			break;
	}

	if (i == MAX_GLTEXTURES)
		Sys_Error ("GL_LoadTexture: numgltextures == MAX_GLTEXTURES\n");

reload:
	strcpy (glt->identifier, identifier);
	
	gltextures[glt->texnum].checksum = lhcsum;
	gltextures[glt->texnum].lhcsum = lhcsum;
	
	glt->texnum = i;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;
	glt->type = 0;
	glt->keep = keep;
	glt->used = true;
	
	if (bytesperpixel == 1) {
			GL_Upload8 (glt, data, width, height, mipmap, alpha);
		}
		else if (bytesperpixel == 4) {
			GL_Upload32 (glt, (unsigned*)data, width, height, mipmap, alpha, false);
		}
		else {
			Sys_Error("GL_LoadTexture: unknown bytesperpixel\n");
		}
		
	//GL_Upload8 (glt, data, width, height, mipmap, alpha);

	if (glt->texnum == numgltextures)
		numgltextures++;

	return glt->texnum;
}

/*
======================
GL_LoadLightmapTexture
======================
*/
int GL_LoadLightmapTexture (char *identifier, int width, int height, byte *data)
{
	gltexture_t	*glt;

	// They need to be allocated sequentially
	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error ("GL_LoadLightmapTexture: numgltextures == MAX_GLTEXTURES\n");

	glt = &gltextures[numgltextures];
	//Con_Printf("gltexnum: %i", numgltextures);
	strcpy (glt->identifier, identifier);
	//Con_Printf("Identifier: %s", identifier);
	glt->texnum = numgltextures;
	glt->width = width;
	glt->height = height;
	glt->mipmap = false; // ELUTODO
	glt->type = 1;
	glt->keep = false;
	glt->used = true;

	GL_Upload32 (glt, (unsigned *)data, width, height, false /*mipmap?*/, false, false);

	if (width != glt->scaled_width || height != glt->scaled_height)
		Sys_Error("GL_LoadLightmapTexture: Tried to scale lightmap\n");

	numgltextures++;

	return glt->texnum;
}

const int lightblock_datamap[128*128*4] =
{
#include "128_128_datamap.h"
};

/*
================================
GL_UpdateLightmapTextureRegion32
================================
*/
void GL_UpdateLightmapTextureRegion32 (gltexture_t *destination, unsigned *data, int width, int height, int xoffset, int yoffset, qboolean mipmap, qboolean alpha)
{
	int			x, y, pos;
	int			realwidth = width + xoffset;
	int			realheight = height + yoffset;
	u8			*dest = (u8 *)destination->data, *src = (u8 *)data;

	// ELUTODO: mipmaps
	// ELUTODO samples = alpha ? GX_TF_RGBA8 : GX_TF_RGBA8;

	if ((int)destination->data & 31)
		Sys_Error ("GL_Update32: destination->data&31");
	
	for (y = yoffset; y < realheight; y++)
	{
		for (x = xoffset; x < realwidth; x++)
		{
			pos = (x + y * realwidth) * 4;
			dest[lightblock_datamap[pos + 0]] = src[pos + 3];
			dest[lightblock_datamap[pos + 1]] = src[pos + 2];
			dest[lightblock_datamap[pos + 2]] = src[pos + 1];
			dest[lightblock_datamap[pos + 3]] = src[pos + 0];
		}
	}
	
	DCFlushRange(destination->data, destination->scaled_width * destination->scaled_height * 4/*sizeof(data)*/);
	//GX_InvalidateTexAll();
}
extern int lightmap_textures;
/*
==============================
GL_UpdateLightmapTextureRegion
==============================
*/
// ELUTODO: doesn't work if the texture doesn't follow the default quake format. Needs improvements.
void GL_UpdateLightmapTextureRegion (int pic_id, int width, int height, int xoffset, int yoffset, byte *data)
{
	gltexture_t	*destination;

	// see if the texture is allready present
	destination = &gltextures[pic_id];
	
	GL_UpdateLightmapTextureRegion32 (destination, (unsigned *)data, width, height, xoffset, yoffset, false, true);
}

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture (qpic_t *pic)
{
	// ELUTODO: loading too much with "" fills the memory with repeated data? Hope not... Check later.
	return GL_LoadTexture ("", pic->width, pic->height, pic->data, false, true, true, 1);
}

// ELUTODO: clean the disable/enable multitexture calls around the engine

void GL_DisableMultitexture(void)
{
	// ELUTODO: we shouldn't need the color atributes for the vertices...

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
}

void GL_EnableMultitexture(void)
{
	// ELUTODO: we shouldn't need the color atributes for the vertices...

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX1, GX_DIRECT);

	GX_SetNumTexGens(2);
	GX_SetNumTevStages(2);
}

void GL_ClearTextureCache(void)
{
	int i;
	int oldnumgltextures = numgltextures;
	void *newdata;

	numgltextures = 0;

	for (i = 0; i < oldnumgltextures; i++)
	{
		if (gltextures[i].used)
		{
			if (gltextures[i].keep)
			{
				numgltextures = i + 1;

				newdata = __lwp_heap_allocate(&texture_heap, gltextures[i].scaled_width * gltextures[i].scaled_height * 4/*sizeof(data)*/);
				if (!newdata)
					Sys_Error("GL_ClearTextureCache: Out of memory.");

				// ELUTODO Pseudo-defragmentation that helps a bit :)
				memcpy(newdata, gltextures[i].data, gltextures[i].scaled_width * gltextures[i].scaled_height * 4/*sizeof(data)*/);

				if (!__lwp_heap_free(&texture_heap, gltextures[i].data))
					Sys_Error("GL_ClearTextureCache: Error freeing data.");

				gltextures[i].data = newdata;
				GX_InitTexObj(&gltextures[i].gx_tex, gltextures[i].data, gltextures[i].scaled_width, gltextures[i].scaled_height, GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, /*mipmap ? GX_TRUE :*/ GX_FALSE);

				DCFlushRange(gltextures[i].data, gltextures[i].scaled_width * gltextures[i].scaled_height * 4/*sizeof(data)*/);
			}
			else
			{
				gltextures[i].used = false;
				if (!__lwp_heap_free(&texture_heap, gltextures[i].data))
					Sys_Error("GL_ClearTextureCache: Error freeing data.");
			}
		}
	}

	GX_InvalidateTexAll();
}
/*
//
//
//
//
//
//
*/
int		image_width;
int		image_height;

int COM_OpenFile (char *filename, int *hndl);
/*
=================================================================

PCX LOADING

=================================================================
*/
typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned char	data;			// unbounded
} pcx_t;
/*
============
LoadPCX
============
*/
byte* LoadPCX (char* filename, int matchwidth, int matchheight)
{
	pcx_t	*pcx;
	byte	*palette;
	byte	*out;
	byte	*pix, *image_rgba;
	int		x, y;
	int		dataByte, runLength;
	byte	*pcx_data;
	byte	*pic;

//
// parse the PCX file
//

	// Load the raw data into memory, then store it
    pcx_data = COM_LoadFile(filename, 5);

    pcx = (pcx_t *)pcx_data;
	
	pcx->xmin = LittleShort(pcx->xmin);
    pcx->ymin = LittleShort(pcx->ymin);
    pcx->xmax = LittleShort(pcx->xmax);
    pcx->ymax = LittleShort(pcx->ymax);
    pcx->hres = LittleShort(pcx->hres);
    pcx->vres = LittleShort(pcx->vres);
    pcx->bytes_per_line = LittleShort(pcx->bytes_per_line);
    pcx->palette_type = LittleShort(pcx->palette_type);
	
	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 640
		|| pcx->ymax >= 480)
	{
		Con_Printf ("Bad pcx file %i\n");
		return 0;
	}
	
	image_rgba = &pcx->data;

	out = malloc ((pcx->ymax+1)*(pcx->xmax+1)*4);
	
	pic = out;

	pix = out;

	if (matchwidth && (pcx->xmax+1) != matchwidth)
		return 0;
	if (matchheight && (pcx->ymax+1) != matchheight)
		return 0;
	
	palette = malloc(768);
	memcpy (palette, (byte *)pcx + com_filesize - 768, 768);
	
	for (y=0 ; y<=pcx->ymax ; y++)
	{
		pix = out + 4*y*(pcx->xmax+1);
		
		for (x=0 ; x<=pcx->xmax ; )
		{
			dataByte = *image_rgba++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *image_rgba++;
			}
			else
				runLength = 1;

			while(runLength-- > 0) {
				pix[0] = palette[dataByte*3+0];
				pix[1] = palette[dataByte*3+1];
				pix[2] = palette[dataByte*3+2];
				pix[3] = 255;
				pix += 4;
				x++;
				
			}
		}

	}
	

	image_width = pcx->xmax+1;
	image_height = pcx->ymax+1;
	
	free (palette);
	free (pcx_data);
	return pic;
}

/*small function to read files with stb_image - single-file image loader library.
** downloaded from: https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
** only use jpeg+png formats, because tbh there's not much need for the others.
** */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#define STBI_ONLY_PIC
#include "stb_image.h"
byte* loadimagepixels (char* filename, qboolean complain, int matchwidth, int matchheight, int reverseRGBA)
{
	int bpp;
	int width, height;
	
	byte* rgba_data;
	
	// Load the raw data into memory, then store it
    rgba_data = COM_LoadFile(filename, 5);

	if (rgba_data == NULL) {
		Con_Printf("NULL: %s", filename);
		return 0;
	}

    byte *image = stbi_load_from_memory(rgba_data, com_filesize, &width, &height, &bpp, 4);
	
	if(image == NULL) {
		Con_Printf("%s\n", stbi_failure_reason());
		return 0;
	}
	
	//set image width/height for texture uploads
	image_width = width;
	image_height = height;

	free(rgba_data);

	return image;
}

int loadtextureimage (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean keep)
{
	int	f = 0;
	int texnum;
	char basename[128], name[128]/*, texname[32]*/;
	byte* data;
	byte *c;
	
	if (complain == false)
		COM_StripExtension(filename, basename); // strip the extension to allow TGA
	else
		strcpy(basename, filename);

	c = (byte*)basename;
	while (*c)
	{
		if (*c == '*')
			*c = '+';
		c++;
	}
	
	// sBTODO
	// leaking memory somewhere
	// first I need to find a better way to pass texture names 
	// so they don't overflow. (increased identifier[to 128] temporarily)
	
	//Try PCX	
	sprintf (name, "%s.pcx", basename);
	COM_FOpenFile (name, &f);
	if (f > 0) {
		COM_CloseFile (f);
		data = LoadPCX (name, matchwidth, matchheight);
		if (data == 0) {
			Con_Printf("PCX: can't load %s\n", name);	
			return 0; //Sys_Error ("PCX: can't load %s", name);
		}
		// sBTODO find a better way to store texture names :/
		// don't overflow texture names
		// hack
		//COM_StripExtension (name + (strlen(basename) - 16), texname);

		//Con_Printf("pcx %s\n", basename);
		//Con_Printf("p name:%s\n", texname);
		texnum = GL_LoadTexture (basename, image_width, image_height, data, true, true, true, 4);		
		free(data);
		return texnum;
	}	
	//Try TGA
	sprintf (name, "%s.tga", basename);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 4);	
		if (data == 0) {
			Con_Printf("TGA: can't load %s\n", name);	
			return 0;
		}
		//Con_Printf("tga: %s\n", basename);
		//Con_Printf("t name:%s\n", texname);
		texnum = GL_LoadTexture (basename, image_width, image_height, data, mipmap, true, keep, 4);
		free(data);
		return texnum;
	}
	//Try PNG
	sprintf (name, "%s.png", basename);
	//Con_Printf("png: %s\n", name);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 1);
		if (data == 0) {
			Con_Printf("PNG: can't load %s\n", name);	
			return 0;
		}
		texnum = GL_LoadTexture (basename, image_width, image_height, data, mipmap, true, keep, 4);	
		free(data);
		return texnum;
	}
	//Try JPEG
	sprintf (name, "%s.jpeg", basename);
	//Con_Printf("jpeg %s\n", name);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 1);
		if (data == 0) {
			Con_Printf("JPEG: can't load %s\n", name);	
			return 0;
		}
		texnum = GL_LoadTexture (basename, image_width, image_height, data, mipmap, true, keep, 4);	
		free(data);
		return texnum;
	}
	sprintf (name, "%s.jpg", basename);
	//Con_Printf("jpg %s\n", name);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 1);
		if (data == 0) {
			Con_Printf("JPG: can't load %s\n", name);	
			return 0;
		}
		texnum = GL_LoadTexture (basename, image_width, image_height, data, mipmap, true, keep, 4);
		free(data);
		return texnum;
	}

	//Con_Printf("Cannot load image %s\n", filename);
	return 0;
}

extern char	skybox_name[32];
extern char skytexname[32];
int loadskyboximage (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap)
{
	int	f = 0;
	int texnum;
	char basename[128], name[132];
	
	int image_size = 128 * 128;
	
	byte* data = (byte*)malloc(image_size * 4);
	byte *c;
	
	if (complain == false)
		COM_StripExtension(filename, basename); // strip the extension to allow TGA
	else
		strcpy(basename, filename);

	c = (byte*)basename;
	while (*c)
	{
		if (*c == '*')
			*c = '+';
		c++;
	}
	
	if (strcmp(skybox_name, ""))
		return 0;
//Try PCX
	
	sprintf (name, "%s.pcx", basename);
	COM_FOpenFile (name, &f);
	if (f > 0) {
		COM_CloseFile (f);
		//Con_Printf("Trying to load: %s", name);	
		data = LoadPCX (name, matchwidth, matchheight);
		if (!data)
			return 0; //Sys_Error ("PCX: can't load %s", name);
		
		texnum = GL_LoadTexture (basename, image_width, image_height, data, false, true, true, 4);
		
		free(data);
		return texnum;
	}
	//Try TGA
	sprintf (name, "%s.tga", basename);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 4);	
		//Con_Printf("Trying to load: %s", name);
		texnum = GL_LoadTexture (basename, image_width, image_height, data, mipmap, false, true, 4);
		
		free(data);
		return texnum;
	}
	//Try PNG
	sprintf (name, "%s.png", basename);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 1);
		//Con_Printf("Trying to load: %s", name);
		texnum = GL_LoadTexture (basename, image_width, image_height, data, mipmap, false, true, 4);
		
		free(data);
		return texnum;
	}
	//Try JPEG
	sprintf (name, "%s.jpeg", basename);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 1);
		Con_Printf("Trying to load: %s", name);
		texnum = GL_LoadTexture (basename, image_width, image_height, data, mipmap, false, true, 4);
		
		free(data);
		return texnum;
	}
	sprintf (name, "%s.jpg", basename);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 1);
		//Con_Printf("Trying to load: %s", name);
		texnum = GL_LoadTexture (basename, image_width, image_height, data, mipmap, false, true, 4);
		
		free(data);
		return texnum;
	}
	
	if (data == NULL) { 
		Con_Printf("Cannot load image %s\n", filename);
		return 0;
	}
	
	return 0;
}
