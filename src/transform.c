/*
    pygame - Python Game Library
    Copyright (C) 2000-2001  Pete Shinners

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Pete Shinners
    pete@shinners.org
*/

/*
 *  surface transformations for pygame
 */
#include "pygame.h"




static SDL_Surface* newsurf_fromsurf(SDL_Surface* surf, int width, int height)
{
	SDL_Surface* newsurf;

	if(surf->format->BytesPerPixel <= 0 || surf->format->BytesPerPixel > 4)
		return (SDL_Surface*)(RAISE(PyExc_ValueError, "unsupport Surface bit depth for transform"));

	newsurf = SDL_CreateRGBSurface(surf->flags, width, height, surf->format->BitsPerPixel,
				surf->format->Rmask, surf->format->Gmask, surf->format->Bmask, surf->format->Amask);
	if(!newsurf)
		return (SDL_Surface*)(RAISE(PyExc_SDLError, SDL_GetError()));

	/* Copy palette, colorkey, etc info */
	if(surf->format->BytesPerPixel==1 && surf->format->palette)
		SDL_SetColors(newsurf, surf->format->palette->colors, 0, surf->format->palette->ncolors);
	if(surf->flags & SDL_SRCCOLORKEY)
		SDL_SetColorKey(newsurf, (surf->flags&SDL_RLEACCEL)|SDL_SRCCOLORKEY, surf->format->colorkey);

	return newsurf;
}





static void rotate(SDL_Surface *src, SDL_Surface *dst, Uint32 bgcolor, int cx, int cy, int isin, int icos)
{
	int x, y, dx, dy;

	Uint8 *srcpix = (Uint8*)src->pixels;
	Uint8 *dstrow = (Uint8*)dst->pixels;

	int srcpitch = src->pitch;
	int dstpitch = dst->pitch;

	int xd = (src->w - dst->w) << 15;
	int yd = (src->h - dst->h) << 15;

	int ax = (cx << 16) - (icos * cx);
	int ay = (cy << 16) - (isin * cx);

	int minval = (1 << 16) - 1;
	int xmaxval = src->w << 16;
	int ymaxval = src->h << 16;

	switch(src->format->BytesPerPixel)
	{
	case 1:
		for(y = 0; y < dst->h; y++) {
			Uint8 *dstpos = (Uint8*)dstrow;
			dy = cy - y;
			dx = (ax + (isin * dy)) + xd;
			dy = (ay - (icos * dy)) + yd;
			for(x = 0; x < dst->w; x++) {
				if(dx<minval || dy < minval || dx > xmaxval || dy > ymaxval) *dstpos++ = bgcolor;
				else *dstpos++ = *(Uint8*)(srcpix + ((dy>>16) * srcpitch) + (dx>>16));
				dx += icos; dy += isin;
			}
			dstrow += dstpitch;
		}break;
	case 2:
		for(y = 0; y < dst->h; y++) {
			Uint16 *dstpos = (Uint16*)dstrow;
			dy = cy - y;
			dx = (ax + (isin * dy)) + xd;
			dy = (ay - (icos * dy)) + yd;
			for(x = 0; x < dst->w; x++) {
				if(dx<minval || dy < minval || dx > xmaxval || dy > ymaxval) *dstpos++ = bgcolor;
				else *dstpos++ = *(Uint16*)(srcpix + ((dy>>16) * srcpitch) + (dx>>16<<1));
				dx += icos; dy += isin;
			}
			dstrow += dstpitch;
		}break;
	case 4:
		for(y = 0; y < dst->h; y++) {
			Uint32 *dstpos = (Uint32*)dstrow;
			dy = cy - y;
			dx = (ax + (isin * dy)) + xd;
			dy = (ay - (icos * dy)) + yd;
			for(x = 0; x < dst->w; x++) {
				if(dx<minval || dy < minval || dx > xmaxval || dy > ymaxval) *dstpos++ = bgcolor;
				else *dstpos++ = *(Uint32*)(srcpix + ((dy>>16) * srcpitch) + (dx>>16<<2));
				dx += icos; dy += isin;
			}
			dstrow += dstpitch;
		}break;
	default: /*case 3:*/
		for(y = 0; y < dst->h; y++) {
			Uint8 *dstpos = (Uint8*)dstrow;
			dy = cy - y;
			dx = (ax + (isin * dy)) + xd;
			dy = (ay - (icos * dy)) + yd;
			for(x = 0; x < dst->w; x++) {
				if(dx<minval || dy < minval || dx > xmaxval || dy > ymaxval)
				{
					dstpos[0] = ((Uint8*)&bgcolor)[0]; dstpos[1] = ((Uint8*)&bgcolor)[1]; dstpos[2] = ((Uint8*)&bgcolor)[2];
					dstpos += 3;
				}
				else {
					Uint8* srcpos = (Uint8*)(srcpix + ((dy>>16) * srcpitch) + ((dx>>16) * 3));
					dstpos[0] = srcpos[0]; dstpos[1] = srcpos[1]; dstpos[2] = srcpos[2];
					dstpos += 3;
				}
				dx += icos; dy += isin;
			}
			dstrow += dstpitch;
		}break;
	}
}



static void stretch(SDL_Surface *src, SDL_Surface *dst)
{
	int looph, loopw;
	
	Uint8* srcrow = (Uint8*)src->pixels;
	Uint8* dstrow = (Uint8*)dst->pixels;

	int srcpitch = src->pitch;
	int dstpitch = dst->pitch;

	int dstwidth = dst->w;
	int dstheight = dst->h;
	int dstwidth2 = dst->w << 1;
	int dstheight2 = dst->h << 1;

	int srcwidth2 = src->w << 1;
	int srcheight2 = src->h << 1;

	int w_err, h_err = srcheight2 - dstheight2;


	switch(src->format->BytesPerPixel)
	{
	case 1:
		for(looph = 0; looph < dstheight; ++looph)
		{
			Uint8 *srcpix = (Uint8*)srcrow, *dstpix = (Uint8*)dstrow;
			w_err = srcwidth2 - dstwidth2;
			for(loopw = 0; loopw < dstwidth; ++ loopw)
			{
				*dstpix++ = *srcpix;
				while(w_err >= 0) {++srcpix; w_err -= dstwidth2;}
				w_err += srcwidth2;
			}
			while(h_err >= 0) {srcrow += srcpitch; h_err -= dstheight2;}
			dstrow += dstpitch;
			h_err += srcheight2;
		}break;
	case 2:
		for(looph = 0; looph < dstheight; ++looph)
		{
			Uint16 *srcpix = (Uint16*)srcrow, *dstpix = (Uint16*)dstrow;
			w_err = srcwidth2 - dstwidth2;
			for(loopw = 0; loopw < dstwidth; ++ loopw)
			{
				*dstpix++ = *srcpix;
				while(w_err >= 0) {++srcpix; w_err -= dstwidth2;}
				w_err += srcwidth2;
			}
			while(h_err >= 0) {srcrow += srcpitch; h_err -= dstheight2;}
			dstrow += dstpitch;
			h_err += srcheight2;
		}break;
	case 3:
		for(looph = 0; looph < dstheight; ++looph)
		{
			Uint8 *srcpix = (Uint8*)srcrow, *dstpix = (Uint8*)dstrow;
			w_err = srcwidth2 - dstwidth2;
			for(loopw = 0; loopw < dstwidth; ++ loopw)
			{
				dstpix[0] = srcpix[0]; dstpix[1] = srcpix[1]; dstpix[2] = srcpix[2];
				dstpix += 3;
				while(w_err >= 0) {srcpix+=3; w_err -= dstwidth2;}
				w_err += srcwidth2;
			}
			while(h_err >= 0) {srcrow += srcpitch; h_err -= dstheight2;}
			dstrow += dstpitch;
			h_err += srcheight2;
		}break;
	default: /*case 4:*/
		for(looph = 0; looph < dstheight; ++looph)
		{
			Uint32 *srcpix = (Uint32*)srcrow, *dstpix = (Uint32*)dstrow;
			w_err = srcwidth2 - dstwidth2;
			for(loopw = 0; loopw < dstwidth; ++ loopw)
			{
				*dstpix++ = *srcpix;
				while(w_err >= 0) {++srcpix; w_err -= dstwidth2;}
				w_err += srcwidth2;
			}
			while(h_err >= 0) {srcrow += srcpitch; h_err -= dstheight2;}
			dstrow += dstpitch;
			h_err += srcheight2;
		}break;
	}
}





    /*DOC*/ static char doc_scale[] =
    /*DOC*/    "pygame.transform.scale(Surface, size) -> Surface\n"
    /*DOC*/    "scale a Surface to an arbitrary size\n"
    /*DOC*/    "\n"
    /*DOC*/    "This will resize a surface to the given resolution.\n"
    /*DOC*/ ;

static PyObject* surf_scale(PyObject* self, PyObject* arg)
{
	PyObject *surfobj;
	SDL_Surface* surf, *newsurf;
	int width, height;

	/*get all the arguments*/
	if(!PyArg_ParseTuple(arg, "O!(ii)", &PySurface_Type, &surfobj, &width, &height))
		return NULL;
	surf = PySurface_AsSurface(surfobj);

	newsurf = newsurf_fromsurf(surf, width, height);
	if(!newsurf) return NULL;

	SDL_LockSurface(newsurf);
	PySurface_Lock(surfobj);

	stretch(surf, newsurf);

	PySurface_Unlock(surfobj);
	SDL_UnlockSurface(newsurf);

	return PySurface_New(newsurf);
}




    /*DOC*/ static char doc_rotate[] =
    /*DOC*/    "pygame.transform.rotate(Surface, angle) -> Surface\n"
    /*DOC*/    "rotate a Surface\n"
    /*DOC*/    "\n"
    /*DOC*/    "Rotates the image clockwise with the given angle (in degrees).\n"
    /*DOC*/    "The image can be any floating point value, and negative\n"
    /*DOC*/    "rotation amounts will do counter-clockwise rotations.\n"
    /*DOC*/    "\n"
    /*DOC*/    "Unless rotating by 90 degree increments, the resulting image\n"
    /*DOC*/    "size will be larger than the original. There will be newly\n"
    /*DOC*/    "uncovered areas in the image. These will filled with either\n"
    /*DOC*/    "the current colorkey for the Surface, or the topleft pixel value.\n"
    /*DOC*/    "(with the alpha channel zeroed out if available)\n"
    /*DOC*/ ;

static PyObject* surf_rotate(PyObject* self, PyObject* arg)
{
	PyObject *surfobj;
	SDL_Surface* surf, *newsurf;
	float angle;

	double radangle, sangle, cangle;
	int dstwidthhalf, dstheighthalf;
	double x, y, cx, cy, sx, sy;
	int nxmax,nymax;
	Uint32 bgcolor;

	/*get all the arguments*/
	if(!PyArg_ParseTuple(arg, "O!f", &PySurface_Type, &surfobj, &angle))
		return NULL;
	surf = PySurface_AsSurface(surfobj);


	if(surf->format->BytesPerPixel <= 0 || surf->format->BytesPerPixel > 4)
		return RAISE(PyExc_ValueError, "unsupport Surface bit depth for transform");

	radangle = angle*.01745329251994329;
	sangle = sin(radangle);
	cangle = cos(radangle);
	
	x = surf->w/2;
	y = surf->h/2;
	cx = cangle*x;
	cy = cangle*y;
	sx = sangle*x;
	sy = sangle*y;
	nxmax = (int)ceil(max(max(max(fabs(cx+sy), fabs(cx-sy)), fabs(-cx+sy)), fabs(-cx-sy)));
	nymax = (int)ceil(max(max(max(fabs(sx+cy), fabs(sx-cy)), fabs(-sx+cy)), fabs(-sx-cy)));
	dstwidthhalf = nxmax ? nxmax : 1;
	dstheighthalf = nymax ? nymax : 1;

	newsurf = newsurf_fromsurf(surf, dstwidthhalf*2, dstheighthalf*2);
	if(!newsurf) return NULL;

	/* get the background color */
	if(surf->flags & SDL_SRCCOLORKEY)
	{
		bgcolor = surf->format->colorkey;
	}
	else
	{
		switch(surf->format->BytesPerPixel)
		{
		case 1: bgcolor = *(Uint8*)surf->pixels; break;
		case 2: bgcolor = *(Uint16*)surf->pixels; break;
		case 4: bgcolor = *(Uint32*)surf->pixels; break;
		default: /*case 3:*/
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
			bgcolor = (((Uint8*)surf->pixels)[0]) + (((Uint8*)surf->pixels)[1]<<8) + (((Uint8*)surf->pixels)[2]<<16);
#else
			bgcolor = (((Uint8*)surf->pixels)[2]) + (((Uint8*)surf->pixels)[1]<<8) + (((Uint8*)surf->pixels)[0]<<16);
#endif
			bgcolor &= ~surf->format->Amask;
		}
	}

	SDL_LockSurface(newsurf);
	PySurface_Lock(surfobj);

	rotate(surf, newsurf, bgcolor, dstwidthhalf, dstheighthalf, (int)(sangle*65536), (int)(cangle*65536));

	PySurface_Unlock(surfobj);
	SDL_UnlockSurface(newsurf);

	return PySurface_New(newsurf);
}




    /*DOC*/ static char doc_flip[] =
    /*DOC*/    "pygame.transform.flip(Surface, xaxis, yaxis) -> Surface\n"
    /*DOC*/    "flips a surface on either axis\n"
    /*DOC*/    "\n"
    /*DOC*/    "Flips the image on the x-axis or the y-axis if the argument\n"
    /*DOC*/    "for that axis is true.\n"
    /*DOC*/    "\n"
    /*DOC*/    "The flip operation is nondestructive, you may flip the image\n"
    /*DOC*/    "as many times as you like, and always be able to recreate the\n"
    /*DOC*/    "exact original image.\n"
    /*DOC*/ ;

static PyObject* surf_flip(PyObject* self, PyObject* arg)
{
	PyObject *surfobj;
	SDL_Surface* surf, *newsurf;
	int xaxis, yaxis;
	int loopx, loopy;
	int pixsize, srcpitch, dstpitch;
	Uint8 *srcpix, *dstpix;

	/*get all the arguments*/
	if(!PyArg_ParseTuple(arg, "O!ii", &PySurface_Type, &surfobj, &xaxis, &yaxis))
		return NULL;
	surf = PySurface_AsSurface(surfobj);

	newsurf = newsurf_fromsurf(surf, surf->w, surf->h);
	if(!newsurf) return NULL;

	pixsize = surf->format->BytesPerPixel;
	srcpitch = surf->pitch;
	dstpitch = newsurf->pitch;

	SDL_LockSurface(newsurf);
	PySurface_Lock(surfobj);

	srcpix = (Uint8*)surf->pixels;
	dstpix = (Uint8*)newsurf->pixels;

	if(!xaxis)
	{
		if(!yaxis)
		{
			for(loopy = 0; loopy < surf->h; ++loopy)
				memcpy(dstpix+loopy*dstpitch, srcpix+loopy*srcpitch, surf->w*surf->format->BytesPerPixel);
		}
		else
		{
			for(loopy = 0; loopy < surf->h; ++loopy)
				memcpy(dstpix+loopy*dstpitch, srcpix+(surf->h-1-loopy)*srcpitch, surf->w*surf->format->BytesPerPixel);
		}
	}
	else /*if (xaxis)*/
	{
		if(yaxis)
		{
			switch(surf->format->BytesPerPixel)
			{
			case 1:
				for(loopy = 0; loopy < surf->h; ++loopy) {
					Uint8* dst = (Uint8*)(dstpix+loopy*dstpitch);
					Uint8* src = ((Uint8*)(srcpix+(surf->h-1-loopy)*srcpitch)) + surf->w;
					for(loopx = 0; loopx < surf->w; ++loopx)
						*dst++ = *src--;
				}break;
			case 2:
				for(loopy = 0; loopy < surf->h; ++loopy) {
					Uint16* dst = (Uint16*)(dstpix+loopy*dstpitch);
					Uint16* src = ((Uint16*)(srcpix+(surf->h-1-loopy)*srcpitch)) + surf->w;
					for(loopx = 0; loopx < surf->w; ++loopx)
						*dst++ = *src--;
				}break;
			case 4:
				for(loopy = 0; loopy < surf->h; ++loopy) {
					Uint32* dst = (Uint32*)(dstpix+loopy*dstpitch);
					Uint32* src = ((Uint32*)(srcpix+(surf->h-1-loopy)*srcpitch)) + surf->w;
					for(loopx = 0; loopx < surf->w; ++loopx)
						*dst++ = *src--;
				}break;
			case 3:
				for(loopy = 0; loopy < surf->h; ++loopy) {
					Uint8* dst = (Uint8*)(dstpix+loopy*dstpitch);
					Uint8* src = ((Uint8*)(srcpix+(surf->h-1-loopy)*srcpitch)) + surf->w*3;
					for(loopx = 0; loopx < surf->w; ++loopx)
					{
						dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
						dst += 3;
						src -= 3;
					}
				}break;
			}
		}
		else
		{
			switch(surf->format->BytesPerPixel)
			{
			case 1:
				for(loopy = 0; loopy < surf->h; ++loopy) {
					Uint8* dst = (Uint8*)(dstpix+loopy*dstpitch);
					Uint8* src = ((Uint8*)(srcpix+loopy*srcpitch)) + surf->w;
					for(loopx = 0; loopx < surf->w; ++loopx)
						*dst++ = *src--;
				}break;
			case 2:
				for(loopy = 0; loopy < surf->h; ++loopy) {
					Uint16* dst = (Uint16*)(dstpix+loopy*dstpitch);
					Uint16* src = ((Uint16*)(srcpix+loopy*srcpitch)) + surf->w;
					for(loopx = 0; loopx < surf->w; ++loopx)
						*dst++ = *src--;
				}break;
			case 4:
				for(loopy = 0; loopy < surf->h; ++loopy) {
					Uint32* dst = (Uint32*)(dstpix+loopy*dstpitch);
					Uint32* src = ((Uint32*)(srcpix+loopy*srcpitch)) + surf->w;
					for(loopx = 0; loopx < surf->w; ++loopx)
						*dst++ = *src--;
				}break;
			case 3:
				for(loopy = 0; loopy < surf->h; ++loopy) {
					Uint8* dst = (Uint8*)(dstpix+loopy*dstpitch);
					Uint8* src = ((Uint8*)(srcpix+loopy*srcpitch)) + surf->w*3;
					for(loopx = 0; loopx < surf->w; ++loopx)
					{
						dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
						dst += 3;
						src -= 3;
					}
				}break;
			}
		}
	}

	PySurface_Unlock(surfobj);
	SDL_UnlockSurface(newsurf);

	return PySurface_New(newsurf);
}








static PyMethodDef transform_builtins[] =
{
	{ "scale", surf_scale, 1, doc_scale },
	{ "rotate", surf_rotate, 1, doc_rotate },
	{ "flip", surf_flip, 1, doc_flip },

	{ NULL, NULL }
};



    /*DOC*/ static char doc_pygame_transform_MODULE[] =
    /*DOC*/    "Contains routines to transform a Surface image.\n"
    /*DOC*/    "\n"
    /*DOC*/    "All transformation functions take a source Surface and\n"
    /*DOC*/    "return a new copy of that surface in the same format as\n"
    /*DOC*/    "the original.\n"
    /*DOC*/    "\n"
    /*DOC*/    "These transform routines are not filtered or smoothed.\n"
    /*DOC*/    "\n"
    /*DOC*/    "Some of the\n"
    /*DOC*/    "filters are 'destructive', which means if you transform\n"
    /*DOC*/    "the image one way, you can't transform the image back to\n"
    /*DOC*/    "the exact same way as it was before. If you plan on doing\n"
    /*DOC*/    "many transforms, it is good practice to keep the original\n"
    /*DOC*/    "untransformed image, and only translate that image.\n"
    /*DOC*/ ;

PYGAME_EXPORT
void inittransform(void)
{
	PyObject *module;
	module = Py_InitModule3("transform", transform_builtins, doc_pygame_transform_MODULE);

	/*imported needed apis*/
	import_pygame_base();
	import_pygame_rect();
	import_pygame_surface();
}

