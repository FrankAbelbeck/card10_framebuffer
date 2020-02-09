/**
 * @file
 * @author Frank Abelbeck <frank.abelbeck@googlemail.com>
 * @version 2020-02-06
 * 
 * @section License
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * @section Description
 * 
 * Graphics surface management routines for the card10 badge: base library
 */

#include <stdlib.h> // uses: malloc(), free()
#include <stdint.h> // uses: int8_t, uint8_t, int16_t, uint16_t, uint32_t
#include <stdio.h>  // uses printf() for printInt() function
#include <stdbool.h> // uses: true, false, bool

#include "faSurfaceBase.h"

//------------------------------------------------------------------------------
// surface/framebuffer constructor and destructor functions
//------------------------------------------------------------------------------

// Surface constructor: create and initialise an Surface structure
Surface *surfaceConstruct() {
	Surface *surface = NULL;
	surface = (Surface*)malloc(sizeof(Surface));
	if (surface != NULL) {
		surface->width = 0;
		surface->height = 0;
		surface->rgb565 = NULL;
		surface->alpha = NULL;
	}
	return surface;
}

// Surface destructor: clear any allocated memory and deallocate structure
void surfaceDestruct(Surface **self) {
	free((*self)->rgb565);
	free((*self)->alpha);
	free(*self);
	*self = NULL;
}

// Surface initialiser: create structure and allocate surface memory
Surface *surfaceSetup(uint8_t width, uint8_t height) {
	Surface *surface = surfaceConstruct();
	if (surface == NULL) return NULL;
	surface->width = width;
	surface->height = height;
	if (width > 0 && height > 0) {
		surface->rgb565 = (uint16_t*)malloc(width*height*2);
		if (surface->rgb565 == NULL) {
			surfaceDestruct(&surface);
			return NULL;
		}
		surface->alpha = (uint8_t*)malloc(width*height);
		if (surface->alpha == NULL) {
			surfaceDestruct(&surface);
			return NULL;
		}
	}
	return surface;
}

// Surface initialiser: create structure and allocate surface memory
Surface *surfaceClone(Surface *surface) {
	if (surface == NULL) return NULL;
	Surface *surfaceClone = surfaceSetup(surface->width,surface->height);
	if (surfaceClone == NULL) return NULL;
	uint16_t i = surface->width*surface->height;
	do {
		i--;
		surfaceClone->rgb565[i] = surface->rgb565[i];
		surfaceClone->alpha[i] = surface->alpha[i];
	} while (i > 0);
	return surfaceClone;
}

void surfaceClear(Surface *surface, uint16_t colour, uint8_t alpha) {
	if (surface == NULL || surface->rgb565 == NULL || surface->alpha == NULL) return;
	uint16_t i = surface->width*surface->height;
	do {
		i--;
		surface->rgb565[i] = colour;
		surface->alpha[i] = alpha;
	} while (i > 0);
}

void surfaceCopy(Surface *source, Surface *destination, SurfaceMod *mask) {
	// sanity check: surfaces should exist and dimensions should match
	if (source == NULL || destination == NULL || mask == NULL || source->width != destination->width || source->height != destination->height || source->height > mask->height) return;
	
	uint32_t bitmask;
	uint8_t x;
	uint16_t i = 0;
	
	for (uint8_t y = 0; y < source->height; y++) {
		bitmask = mask->tile[y >> 3];
		if (bitmask == 0) {
			// empty bitmask: nothing to do for this and the next seven rows
			i += source->width << 3;
			y += 7;
			continue;
		}
		for (x = 0; x < source->width; x++) {
			if ((1 << (x >> 3)) & bitmask) {
				destination->rgb565[i] = source->rgb565[i];
				destination->alpha[i]  = source->alpha[i];
			}
			i++;
		}
	}
}

Point createPoint(int32_t x, int32_t y) {
	Point p;
	p.x = x;
	p.y = y;
	return p;
}


BoundingBox boundingBoxCreate(int32_t xMin, int32_t yMin, int32_t xMax, int32_t yMax) {
	BoundingBox bb;
	bb.min.x = xMin;
	bb.min.y = yMin;
	bb.max.x = xMax;
	bb.max.y = yMax;
	return bb;
}

BoundingBox boundingBoxGet(Surface *surface) {
	BoundingBox bb;
	bb.min.x = 0;
	bb.min.y = 0;
	bb.max.x = surface->width-1;
	bb.max.y = surface->height-1;
	return bb;
}

SurfaceMod *surfaceModConstruct(uint8_t height) {
	SurfaceMod *mask = (SurfaceMod*)malloc(sizeof(SurfaceMod));
	mask->height = 0;
	mask->tile = NULL;
	if (height > 0) {
		mask->tile = (uint32_t*)malloc((height >> 1) + 4); // size = 4*(height/8 + 1);
		if (mask->tile != NULL) {
			mask->height = height;
			surfaceModClear(mask);
		} else {
			surfaceModDestruct(&mask);
		}
	}
	return mask;
}

void surfaceModDestruct(SurfaceMod **mask) {
	if (mask == NULL) return;
	free((*mask)->tile);
	free((*mask));
	*mask = NULL;
}

/* clear mask by setting used to zero */
void surfaceModClear(SurfaceMod *mask) {
	if (mask == NULL) return;
	uint8_t iMax = mask->height >> 3;
	for (uint8_t i = 0; i < iMax; i++) mask->tile[i] = 0;
}

void surfaceModSetSeq(SurfaceMod *mask, uint8_t x, uint8_t y, uint8_t len) {
	if (mask == NULL || y >= mask->height || len == 0) return;
	// new bitmask = all bits below start bit  XOR  all bits up to and including stop bit
	mask->tile[y >> 3] |= ( (1 << (x >> 3))-1 )  ^  ( (1 << (((x+len-1) >> 3)+1))-1 );
}

void surfaceModSetRow(SurfaceMod *mask, uint8_t y, uint32_t bitmask) {
	if (mask == NULL || y >= mask->height) return;
	mask->tile[y >> 3] |= bitmask;
}

void surfaceModSetPixel(SurfaceMod *mask, uint8_t x, uint8_t y) {
	if (mask == NULL || y >= mask->height) return;
	mask->tile[y >> 3] |= 1 << (x >> 3);
}

//------------------------------------------------------------------------------
// drawing routines for geometric primitives
//------------------------------------------------------------------------------

BoundingBox surfaceDrawPoint(Surface *surface, Point p, uint16_t colour, uint8_t alpha, uint8_t mode, SurfaceMod *mask) {
	BoundingBox bb = boundingBoxCreate(0,0,0,0);
	if (surface == NULL || mask == NULL) return bb;
	
	if (p.x >= 0 && p.x < surface->width && p.y >= 0 && p.y < surface->height) {
		// pixel visible, draw it
		uint32_t i = p.y * surface->width + p.x;
		if (surfaceBlend(
			colour,alpha,
			surface->rgb565[i],surface->alpha[i],
			&surface->rgb565[i],&surface->alpha[i],
			mode)) {
			// blending changed the surface pixel; add single pixel entry
			surfaceModSetPixel(mask,p.x,p.y);
		}
		bb.min = p;
		bb.max = p;
	}
	return bb;
}


BoundingBox surfaceDrawLine(Surface *surface, Point p0, Point p1, uint16_t colour, uint8_t alpha, uint8_t mode, SurfaceMod *mask)  {
	BoundingBox bb = boundingBoxCreate(0,0,0,0);
	// simple implementation of the Bresenham line algorithm; first check validity of surface
	if (surface == NULL || mask == NULL) return bb;
	
	int16_t xDiff,yDiff;
	int8_t  xStep,yStep;
	
	// calculate the x direction of the line and stepping parameters
	if (p1.x > p0.x) {
		xDiff = p1.x - p0.x;
		xStep = 1;
		bb.min.x = p0.x;
		bb.max.x = p1.x;
	} else {
		xDiff = p0.x - p1.x;
		xStep = -1;
		bb.min.x = p1.x;
		bb.max.x = p0.x;
	}
	
	// since p1.y is always >= p0.y (see above), there is only one stepping calculation case
	if (p1.y > p0.y) {
		yDiff = p0.y - p1.y;
		yStep = 1;
		bb.min.y = p0.y;
		bb.max.y = p1.y;
	} else {
		yDiff = p1.y - p0.y;
		yStep = -1;
		bb.min.y = p1.y;
		bb.max.y = p0.y;
	}
	
	// setup error variables
	int16_t error  = xDiff+yDiff;
	int16_t error2 = 0;
	int16_t i;
	uint32_t bitmask = 0;
	
	// use x0,y0 as running coordinate pair, calculate new coordinates as long as p1.x,p1.y is not reached
	while (1) {
		if (p0.x >= 0 && p0.x < surface->width && p0.y >= 0 && p0.y < surface->height) {
			// pixel visible: calculate new index and record in mask if blending modified the surface
			i = p0.y * surface->width + p0.x;	
			if (surfaceBlend(
				colour,alpha,
				surface->rgb565[i],surface->alpha[i],
				&surface->rgb565[i],&surface->alpha[i],
				mode)
			) bitmask |= 1 << (p0.x >> 3);
		}
		
		if (p0.x == p1.x && p0.y == p1.y) {
			surfaceModSetRow(mask,p0.y,bitmask); // prior to exit, set last bitmask
			break; // end is reached, let loop terminate
		}
		// update error term
		error2 = error + error;
		if (error2 > yDiff) {
			// error inside yDiff bounds: step in x direction, update error term
			error += yDiff;
			p0.x += xStep;
		}
		if (error2 < xDiff) {
			surfaceModSetRow(mask,p0.y,bitmask);
			bitmask = 0;
			// error inside yDiff bounds: step in x direction, update error term
			error += xDiff;
			p0.y += yStep;
		}
	}
	
	return bb;
}


BoundingBox surfaceDrawCircle(Surface *surface, Point pm, uint16_t radius, uint16_t colour, uint8_t alpha, uint8_t mode, SurfaceMod *mask) {
	BoundingBox bb = boundingBoxCreate(0,0,0,0);
	// Bresenham-like algorithm, running from (x,y+radius) along the circle until (x==y)
	// this is the octant 1 in the following ascii art (screen coordinates),
	// running counter-clockwise from 90° to 45°
	//
	//   .  |  .
	//    .5|6.
	//   4 .|. 7
	//  ----+---->
	//   3 .|. 0
	//    .2|1.
	//   .  |  .
	//      V
	// every other octant can be reached using the circle's symmetry
	if (surface == NULL || radius == 0) return bb;
	
	int16_t error = 1 - radius;
	int16_t ddE_x = 0;
	int16_t ddE_y = -2 * radius;
	uint32_t i;
	Point p, pTemp;
	
	bb.min.x = pm.x - radius;
	bb.max.x = pm.x + radius;
	bb.min.y = pm.y - radius;
	bb.max.y = pm.y + radius;
	
	if (bb.min.x < 0) bb.min.x = 0;
	if (bb.min.y < 0) bb.min.y = 0;
	if (bb.max.x >= surface->width) bb.max.x = surface->width - 1;
	if (bb.max.y >= surface->height) bb.max.y = surface->height - 1;
	
	p.x = 0;
	p.y = radius;
	do {
		pTemp.y = pm.y + p.y;
		if (pTemp.y >= 0 && pTemp.y < surface->height) {
			pTemp.x = pm.x + p.x;
			if (pTemp.x >= 0 && pTemp.x < surface->width) {
				// set pixel in octant 1 (x+xMod,y+yMod)
				// drawing direction: from 90° to 45°
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
			pTemp.x = pm.x - p.x;
			if (pTemp.x >= 0 && pTemp.x < surface->width) {
				// set pixel in octant 2 (x-xMod,y+yMod)
				// drawing direction: from 90° to 135°
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
		}
		
		pTemp.y = pm.y - p.y;
		if (pTemp.y >= 0 && pTemp.y < surface->height) {
			pTemp.x = pm.x + p.x;
			if (pTemp.y >= 0 && pTemp.y < surface->height) {
				// set pixel in octant 6 (x+xMod,y-yMod)
				// drawing direction: from 270° to 315°
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
			pTemp.x = pm.x - p.x;
			if (pTemp.y >= 0 && pTemp.y < surface->height) {
				// set pixel in octant 5 (x-xMod,y-yMod)
				// drawing direction: from 270° to 225°
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
		}
		
		pTemp.y = pm.y + p.x;
		if (pTemp.y >= 0 && pTemp.y < surface->height) {
			pTemp.x = pm.x + p.y;
			if (pTemp.y >= 0 && pTemp.y < surface->height) {
				// set pixel in octant 0 (x+yMod,y+xMod)
				// drawing direction: from 0° to 45°
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
			pTemp.x = pm.x - p.y;
			if (pTemp.y >= 0 && pTemp.y < surface->height) {
				// set pixel in octant 3 (x-yMod,y+xMod)
				// drawing direction: from 180° to 135°
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
		}
		
		pTemp.y = pm.y - p.x;
		if (pTemp.y >= 0 && pTemp.y < surface->height) {
			pTemp.x = pm.x + p.y;
			if (pTemp.y >= 0 && pTemp.y < surface->height) {
				// set pixel in octant 7 (x+yMod,y-xMod)
				// drawing direction: from 360° to 315°
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
			pTemp.x = pm.x - p.y;
			if (pTemp.y >= 0 && pTemp.y < surface->height) {
				// set pixel in octant 4 (x-yMod,y-xMod)
				// drawing direction: from 180° to 225°
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
		}
		
		// update error term to check which axis needs stepping
		if (error >= 0) {
			// error term positive:
			// also reduce running y var (going von radius to 0) and updat error term
			p.y--;
			ddE_y += 2;
			error += ddE_y;
		}
		// increase x var and update error term
		p.x++;
		ddE_x += 2;
		error += ddE_x + 1;
	} while (p.x < p.y);
	
	return bb;
}


BoundingBox surfaceDrawArc(Surface *surface, Point pm, uint16_t radius, int16_t angleStart, int16_t angleStop, uint16_t colour, uint8_t alpha, uint8_t mode, SurfaceMod *mask) {
	BoundingBox bb = boundingBoxCreate(0,0,0,0);
	// Bresenham-like algorithm, running from (x,y+radius) along the circle until (x==y)
	// this is the octant 1 in the following ascii art (screen coordinates),
	// running counter-clockwise from 90° to 45°
	//
	//   .  |  .
	//    .5|6.
	//   4 .|. 7
	//  ----+---->
	//   3 .|. 0
	//    .2|1.
	//   .  |  .
	//      V
	// every other octant can be reached using the circle's symmetry
	if (surface == NULL || radius == 0) return bb;
	
	int16_t error = 1 - radius;
	int16_t ddE_x = 0;
	int16_t ddE_y = -2 * radius;
	uint32_t i;
	int16_t xStart,xStop,yStart,yStop;
	Point p, pTemp;
	
	p.x = 0;
	p.y = radius;
	
	// Since this function should draw partial circles, we have to pre-calculate
	// the start and stop coordinates
	// limit angles to 0..359
	angleStart = (360 + (angleStart % 360)) % 360;
	angleStop = (360 + (angleStop % 360)) % 360;
	
	xStart = radius * surfaceCosine(angleStart) / 1024;
	yStart = radius * surfaceSine(angleStart) / 1024;
	xStop  = radius * surfaceCosine(angleStop)  / 1024;
	yStop  = radius * surfaceSine(angleStop)  / 1024;
	
	
	// --- TODO: modify circle bounding box to arc ---
	
	bb.min.x = pm.x - radius;
	bb.max.x = pm.x + radius;
	bb.min.y = pm.y - radius;
	bb.max.y = pm.y + radius;
	
	if (bb.min.x < 0) bb.min.x = 0;
	if (bb.min.y < 0) bb.min.y = 0;
	if (bb.max.x >= surface->width) bb.max.x = surface->width - 1;
	if (bb.max.y >= surface->height) bb.max.y = surface->height - 1;
	
	// --- TODO: modify circle bounding box to arc ---
	
	
	// setup a bitmask for all octants and -- depending on angles -- set bits for octants that need processing
	uint8_t octantStart = angleStart/45;
	uint8_t octantStop = angleStop/45;
	uint8_t octants = 0;
	if (octantStop >= octantStart) {
		octants = ((1 << (octantStop-octantStart+1))-1) << octantStart;
	} else {
		octants = ~(((1 << (octantStart-octantStop-1))-1) << (octantStop+1));
	}
	
	do {
		// paint pixel for all octants (use symmetry)
		
		pTemp.x = pm.x + p.x;
		if (pTemp.x >= 0 && pTemp.x < surface->width) {
			pTemp.y = pm.y + p.y;
			// set pixel in octant 1 (x+xMod,y+yMod)
			// drawing direction: from 90° to 45°
			if (octants & 2 && pTemp.y >= 0 && pTemp.y < surface->height && \
				(octantStart != 1 || p.x <= xStart) && (octantStop != 1 || p.x >= xStop) ) {
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
			pTemp.y = pm.y - p.y;
			// set pixel in octant 6 (x+xMod,y-yMod)
			// drawing direction: from 270° to 315°
			if (octants & 64 && pTemp.y >= 0 && pTemp.y < surface->height && \
				(octantStart != 6 || p.x >= xStart) && (octantStop != 6 || p.x <= xStop) ) {
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
		}
		
		pTemp.x = pm.x - p.x;
		if (pTemp.x >= 0 && pTemp.x < surface->width) {
			pTemp.y = pm.y + p.y;
			// set pixel in octant 2 (x-xMod,y+yMod)
			// drawing direction: from 90° to 135°
			if (octants & 4 && pTemp.y >= 0 && pTemp.y < surface->height && \
				(octantStart != 2 || -p.x <= xStart) && (octantStop != 2 || -p.x >= xStop) ) {
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
			pTemp.y = pm.y - p.y;
			// set pixel in octant 5 (x-xMod,y-yMod)
			// drawing direction: from 270° to 225°
			if (octants & 32 && pTemp.y >= 0 && pTemp.y < surface->height && \
				(octantStart != 5 || -p.x >= xStart) && (octantStop != 5 || -p.x <= xStop) ) {
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
		}
		
		pTemp.x = pm.x + p.y;
		if (pTemp.x >= 0 && pTemp.x < surface->width) {
			pTemp.y = pm.y + p.x;
			// set pixel in octant 0 (x+yMod,y+xMod)
			// drawing direction: from 0° to 45°
			if (octants & 1 && pTemp.y >= 0 && pTemp.y < surface->height && \
				(octantStart != 0 || p.x >= yStart) && (octantStop != 0 || p.x <= yStop) ) {
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
			pTemp.y = pm.y - p.x;
			// set pixel in octant 7 (x+yMod,y-xMod)
			// drawing direction: from 360° to 315°
			if (octants & 128 && pTemp.y >= 0 && pTemp.y < surface->height && \
				(octantStart != 7 || -p.x >= yStart) && (octantStop != 7 || -p.x <= yStop) ) {
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
		}
		pTemp.x = pm.x - p.y;
		if (pTemp.x >= 0 && pTemp.x < surface->width) {
			pTemp.y = pm.y + p.x;
			// set pixel in octant 3 (x-yMod,y+xMod)
			// drawing direction: from 180° to 135°
			if (octants & 8 && pTemp.y >= 0 && pTemp.y < surface->height && \
				(octantStart != 3 || p.x <= yStart) && (octantStop != 3 || p.x >= yStop) ) {
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
			pTemp.y = pm.y - p.x;
			// set pixel in octant 4 (x-yMod,y-xMod)
			// drawing direction: from 180° to 225°
			if (octants & 16 && pTemp.y >= 0 && pTemp.y < surface->height && \
				(octantStart != 4 || -p.x <= yStart) && (octantStop != 4 || -p.x >= yStop) ) {
				i = pTemp.y * surface->width + pTemp.x;
				if (surfaceBlend(
					colour,alpha,
					surface->rgb565[i],surface->alpha[i],
					&surface->rgb565[i],&surface->alpha[i],
					mode)) surfaceModSetPixel(mask,pTemp.x,pTemp.y);
			}
		}
		
		// update error term to check which axis needs stepping
		if (error >= 0) {
			p.y--;
			ddE_y += 2;
			error += ddE_y;
		}
		p.x++;
		ddE_x += 2;
		error += ddE_x + 1;
	} while (p.x < p.y);
	
	return bb;
}

BoundingBox surfaceDrawTriangle(Surface *surface, Point p0, Point p1, Point p2, uint16_t colour, uint8_t alpha, uint8_t mode, SurfaceMod *mask) {
	BoundingBox bb = boundingBoxCreate(0,0,0,0);
	return bb;
}

BoundingBox surfaceDrawRectangle(Surface *surface, Point p0, Point p1, uint16_t colour, uint8_t alpha, uint8_t mode, SurfaceMod *mask) {
	BoundingBox bb = boundingBoxCreate(0,0,0,0);
	return bb;
}

//------------------------------------------------------------------------------
// integer mathematics helper functions
//------------------------------------------------------------------------------

// return tan(x) for x in -45..+45, mapping [-1.0..+1.0] to -1024..+1024
int16_t surfaceTangent45(int16_t x) {
	// limit to -44..+44
	if (x <= -45) return -1024;
// 	>>> for x in range(-44,45): print("case {}: return {};".format(x,round(math.tan(math.pi*x/180)*1024)))
	switch (x) {
		case -44: return -989;
		case -43: return -955;
		case -42: return -922;
		case -41: return -890;
		case -40: return -859;
		case -39: return -829;
		case -38: return -800;
		case -37: return -772;
		case -36: return -744;
		case -35: return -717;
		case -34: return -691;
		case -33: return -665;
		case -32: return -640;
		case -31: return -615;
		case -30: return -591;
		case -29: return -568;
		case -28: return -544;
		case -27: return -522;
		case -26: return -499;
		case -25: return -477;
		case -24: return -456;
		case -23: return -435;
		case -22: return -414;
		case -21: return -393;
		case -20: return -373;
		case -19: return -353;
		case -18: return -333;
		case -17: return -313;
		case -16: return -294;
		case -15: return -274;
		case -14: return -255;
		case -13: return -236;
		case -12: return -218;
		case -11: return -199;
		case -10: return -181;
		case -9: return -162;
		case -8: return -144;
		case -7: return -126;
		case -6: return -108;
		case -5: return -90;
		case -4: return -72;
		case -3: return -54;
		case -2: return -36;
		case -1: return -18;
		case 0: return 0;
		case 1: return 18;
		case 2: return 36;
		case 3: return 54;
		case 4: return 72;
		case 5: return 90;
		case 6: return 108;
		case 7: return 126;
		case 8: return 144;
		case 9: return 162;
		case 10: return 181;
		case 11: return 199;
		case 12: return 218;
		case 13: return 236;
		case 14: return 255;
		case 15: return 274;
		case 16: return 294;
		case 17: return 313;
		case 18: return 333;
		case 19: return 353;
		case 20: return 373;
		case 21: return 393;
		case 22: return 414;
		case 23: return 435;
		case 24: return 456;
		case 25: return 477;
		case 26: return 499;
		case 27: return 522;
		case 28: return 544;
		case 29: return 568;
		case 30: return 591;
		case 31: return 615;
		case 32: return 640;
		case 33: return 665;
		case 34: return 691;
		case 35: return 717;
		case 36: return 744;
		case 37: return 772;
		case 38: return 800;
		case 39: return 829;
		case 40: return 859;
		case 41: return 890;
		case 42: return 922;
		case 43: return 955;
		case 44: return 989;
		default: return 1024;
	}
}

// return sin(x) for x in 0..90, mapping [0..+1.0] to 0..+1024
int16_t surfaceSine90(int16_t x) {
	if (x <= 0) return 0;
// 	>>> for x in range(0,91): print("case {}: return {};".format(x,round(math.sin(math.pi*x/180)*1024)))
	switch (x) {
		case 0: return 0;
		case 1: return 18;
		case 2: return 36;
		case 3: return 54;
		case 4: return 71;
		case 5: return 89;
		case 6: return 107;
		case 7: return 125;
		case 8: return 143;
		case 9: return 160;
		case 10: return 178;
		case 11: return 195;
		case 12: return 213;
		case 13: return 230;
		case 14: return 248;
		case 15: return 265;
		case 16: return 282;
		case 17: return 299;
		case 18: return 316;
		case 19: return 333;
		case 20: return 350;
		case 21: return 367;
		case 22: return 384;
		case 23: return 400;
		case 24: return 416;
		case 25: return 433;
		case 26: return 449;
		case 27: return 465;
		case 28: return 481;
		case 29: return 496;
		case 30: return 512;
		case 31: return 527;
		case 32: return 543;
		case 33: return 558;
		case 34: return 573;
		case 35: return 587;
		case 36: return 602;
		case 37: return 616;
		case 38: return 630;
		case 39: return 644;
		case 40: return 658;
		case 41: return 672;
		case 42: return 685;
		case 43: return 698;
		case 44: return 711;
		case 45: return 724;
		case 46: return 737;
		case 47: return 749;
		case 48: return 761;
		case 49: return 773;
		case 50: return 784;
		case 51: return 796;
		case 52: return 807;
		case 53: return 818;
		case 54: return 828;
		case 55: return 839;
		case 56: return 849;
		case 57: return 859;
		case 58: return 868;
		case 59: return 878;
		case 60: return 887;
		case 61: return 896;
		case 62: return 904;
		case 63: return 912;
		case 64: return 920;
		case 65: return 928;
		case 66: return 935;
		case 67: return 943;
		case 68: return 949;
		case 69: return 956;
		case 70: return 962;
		case 71: return 968;
		case 72: return 974;
		case 73: return 979;
		case 74: return 984;
		case 75: return 989;
		case 76: return 994;
		case 77: return 998;
		case 78: return 1002;
		case 79: return 1005;
		case 80: return 1008;
		case 81: return 1011;
		case 82: return 1014;
		case 83: return 1016;
		case 84: return 1018;
		case 85: return 1020;
		case 86: return 1022;
		case 87: return 1023;
		case 88: return 1023;
		default: return 1024;
	}
}

// return sin(x) for x in 0..360, mapping [-1.0..+1.0] to -1024..+1024
int16_t surfaceSine(int16_t x) {
	x = ((x % 360) + 360) % 360;
	if (x < 90) {
		// 1st sector: use surfaceSine90
		return surfaceSine90(x);
	} else if (x < 180) {
		// 2nd sector: use surfaceSine90, mirrored about y axis
		return surfaceSine90(180-x);
	} else if (x < 270) {
		// 3rd sector: use surfaceSine90, mirrored about x axis
		return -surfaceSine90(x-180);
	} else if (x < 360) {
		// 4th sector: use surfaceSine90, mirrored about x and y axis
		return -surfaceSine90(360-x);
	} else {
		return 0;
	}
}

// return cos(x) for x in 0..360, mapping [-1.0..+1.0] to -1024..+1024
int16_t surfaceCosine(int16_t x) {
	// cos = sin, phase-shifted by 90 degrees
	return surfaceSine(x + 90);
}

//------------------------------------------------------------------------------
// surface composition functions
//------------------------------------------------------------------------------

// Image composition function for alpha blending a colour component of a pixel
// of surface A with a colour component of a pixel of surface B.
// operation: result = a op b (mode defines op)
bool surfaceBlend(uint16_t colourA, uint8_t alphaA, uint16_t colourB, uint8_t alphaB, uint16_t *colourResult, uint8_t *alphaResult, uint8_t mode) {
	uint8_t F_A,F_B, alphaC;
	uint16_t alpha,colourC;
	uint32_t colour;
	// depending on mode, select the right fractions (indicating the extent of contributions of A and B)
	// see (Porter Duff, 1984) (https://doi.org/10.1145%2F800031.808606), p. 255
	// adapted to alpha in [0,255]
	switch (mode) {
		case BLEND_OVER:
			F_A = 255;
			F_B = 255 - alphaA;
			break;
		case BLEND_IN:
			F_A = alphaB;
			F_B = 0;
			break;
		case BLEND_OUT:
			F_A = 255 - alphaB;
			F_B = 0;
			break;
		case BLEND_ATOP:
			F_A = alphaB;
			F_B = 255 - alphaA;
			break;
		case BLEND_XOR:
			F_A = 255 - alphaB;
			F_B = 255 - alphaA;
			break;
		case BLEND_PLUS:
			F_A = 255;
			F_B = 255;
			break;
		default:
			return false;
	}
	// calculate compositing formula (cf. (Porter Duff, 1984), p. 256, adopted to alpha in [0,255])
	alpha = (alphaA * F_A + alphaB * F_B) >> 8;
	alphaC = (alpha > 255) ? 255 : (uint8_t)alpha;
	
	// compose red channel
	colour = ( alphaA * F_A * ((colourA >> 11) & 0x1f) + alphaB * F_B * ((colourB >> 11) & 0x1f) ) >> 16;
	if (colour > 31) colour = 31;
	colourC = (uint16_t)(colour << 11); 
	// compose green channel
	colour = ( alphaA * F_A * ((colourA >> 5) & 0x3f) + alphaB * F_B * ((colourB >> 5) & 0x3f) ) >> 16;
	if (colour > 63) colour = 63;
	colourC |= (uint16_t)(colour << 5); 
	// compose blue channel
	colour = ( alphaA * F_A * (colourA & 0x1f) + alphaB * F_B * (colourB & 0x1f) ) >> 16;
	if (colour > 31) colour = 31;
	colourC |= (uint16_t)colour;
	
	*alphaResult = alphaC;
	*colourResult = colourC;
	
	// if A has modified B (B!=C), signal this by returning true
	return (colourC != colourB || alphaC != alphaB);
}


void printInt(int32_t value) {
 	char result[12] = "           \0";
	const char digits[10] = "0123456789";
	if (value < 0) {
		result[0] = '-';
		value = -value;
	} else {
		result[0] = '+';
		value = value;
	}
	uint8_t i = 10;
	do {
		result[i] = digits[value % 10];
		value /= 10;
		i--;
	} while (value != 0 && i > 0);
	printf("%s",result);
}
