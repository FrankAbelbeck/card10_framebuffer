/**
 * @file
 * @author Frank Abelbeck <frank.abelbeck@googlemail.com>
 * @version 2020-01-20
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

#include "faSurfaceBase.h"

//------------------------------------------------------------------------------
// surface/framebuffer constructor and destructor functions
//------------------------------------------------------------------------------

// Surface constructor: create and initialise an Surface structure
Surface *constructSurface() {
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
void destructSurface(Surface **self) {
	free((*self)->rgb565);
	free((*self)->alpha);
	free(*self);
	*self = NULL;
}

// Surface initialiser: create structure and allocate surface memory
Surface *setupSurface(uint16_t width, uint16_t height) {
	Surface *surface = constructSurface();
	if (surface == NULL) return NULL;
	surface->width = width;
	surface->height = height;
	if (width > 0 && height > 0) {
		surface->rgb565 = (uint16_t*)malloc(width*height*2);
		if (surface->rgb565 == NULL) {
			destructSurface(&surface);
			return NULL;
		}
		surface->alpha = (uint8_t*)malloc(width*height);
		if (surface->alpha == NULL) {
			destructSurface(&surface);
			return NULL;
		}
	}
	return surface;
}

// Surface initialiser: create structure and allocate surface memory
Surface *cloneSurface(Surface *surface) {
	if (surface == NULL) return NULL;
	Surface *surfaceClone = setupSurface(surface->width,surface->height);
	if (surfaceClone == NULL) return NULL;
	for (uint32_t i = 0; i < surface->width*surface->height; i++) {
		surfaceClone->rgb565[i] = surface->rgb565[i];
		surfaceClone->alpha[i] = surface->alpha[i];
	}
	return surfaceClone;
}

void clearSurface(Surface *surface, uint16_t colour, uint8_t alpha) {
	if (surface != NULL && surface->rgb565 != NULL && surface->alpha != NULL) {
		for (uint32_t i = 0; i < surface->width*surface->height; i++) {
			surface->rgb565[i] = colour;
			surface->alpha[i] = alpha;
		}
	}
}

void copySurfaceAreaToSurface(Surface *source, Surface *destination, BoundingBox boundingBox) {
	// sanity check: surfaces should exist and bounding box has to be clipped to a valid area
	if (source == NULL || destination == NULL || source->width != destination->width || source->height != destination->height) return;
#ifdef FASURFACE_FULL
	boundingBox.min = divPerspective(boundingBox.min);
	boundingBox.max = divPerspective(boundingBox.max);
#endif
	if (boundingBox.min.x < 0) boundingBox.min.x = 0;
	if (boundingBox.min.y < 0) boundingBox.min.y = 0;
	if (boundingBox.max.x >= source->width) boundingBox.max.x = source->width - 1;
	if (boundingBox.max.y >= source->height) boundingBox.max.y = source->height - 1;
	
	// iterate over bounding box and adjust surface indices accordingly.
	uint16_t x;
	uint32_t i = boundingBox.min.y * source->width + boundingBox.min.x;
	uint16_t di = source->width - 1 - boundingBox.max.x + boundingBox.min.x;
	for (uint16_t y = boundingBox.min.y; y <= boundingBox.max.y; y++) {
		for (x = boundingBox.min.x; x <= boundingBox.max.x; x++) {
			destination->rgb565[i] = source->rgb565[i];
			destination->alpha[i] = source->alpha[i];
			i++;
		}
		i += di;
	}
}

BoundingBox createBoundingBox(int32_t xMin, int32_t yMin, int32_t xMax, int32_t yMax) {
	BoundingBox bb;
	bb.min.x = xMin;
	bb.min.y = yMin;
	bb.max.x = xMax;
	bb.max.y = yMax;
	return bb;
}

BoundingBox getBoundingBoxSurface(Surface *surface) {
	BoundingBox bb;
	bb.min.x = 0;
	bb.min.y = 0;
	bb.max.x = surface->width-1;
	bb.max.y = surface->height-1;
	return bb;
}

//------------------------------------------------------------------------------
// drawing routines for geometric primitives
//------------------------------------------------------------------------------

void drawPoint(Surface *surface, int16_t x, int16_t y, uint16_t colour, uint8_t alpha) {
	// TODO
}

void drawLine(Surface *surface, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t colour, uint8_t alpha) {
	// TODO
}

void drawCircle(Surface *surface, int16_t x, int16_t y, uint16_t radius, uint16_t colour, uint8_t alpha) {
	// TODO
}

void drawTriangle(Surface *surface, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t colour, uint8_t alpha) {
	// TODO
}

void drawRectangle(Surface *surface, int16_t x, int16_t y, int16_t width, int16_t height, uint16_t colour, uint8_t alpha) {
	// TODO
}

//------------------------------------------------------------------------------
// integer mathematics helper functions
//------------------------------------------------------------------------------

// return tan(x) for x in -45..+45, mapping [-1.0..+1.0] to -1024..+1024
int16_t faTan45(int16_t x) {
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
int16_t faSin90(int16_t x) {
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
int16_t faSin(int16_t x) {
	if (x < 0) {
		return 0;
	} else if (x < 90) {
		// 1st sector: use faSin90
		return faSin90(x);
	} else if (x < 180) {
		// 2nd sector: use faSin90, mirrored about y axis
		return faSin90(180-x);
	} else if (x < 270) {
		// 3rd sector: use faSin90, mirrored about x axis
		return -faSin90(x-180);
	} else if (x < 360) {
		// 4th sector: use faSin90, mirrored about x and y axis
		return -faSin90(360-x);
	} else {
		return 0;
	}
}

// return cos(x) for x in 0..360, mapping [-1.0..+1.0] to -1024..+1024
int16_t faCos(int16_t x) {
	// cos = sin, phase-shifted by 90 degrees
	return faSin((x + 90) % 360);
}

//------------------------------------------------------------------------------
// surface composition functions
//------------------------------------------------------------------------------

// Image composition function for alpha blending a colour component of a pixel
// of surface A with a colour component of a pixel of surface B.
void blend(uint16_t colourA, uint8_t alphaA, uint16_t colourB, uint8_t alphaB, uint16_t *colourResult, uint8_t *alphaResult, uint8_t mode) {
	uint8_t F_A,F_B;
	uint16_t alpha;
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
			return;
	}
	// calculate compositing formula (cf. (Porter Duff, 1984), p. 256, adopted to alpha in [0,255])
	alpha = (alphaA * F_A + alphaB * F_B) >> 8;
	if (alpha == 0) {
		// catch fully transparent composition
		*alphaResult = 0;
		*colourResult = 0;
	} else {
		// alpha and colour need to be limited to [0,255] due to the plus operator simply adding both
		*alphaResult = (alpha > 255) ? 255 : (uint8_t)alpha;
		// compose red channel
		colour = ( alphaA * F_A * ((colourA >> 11) & 0x1f) + alphaB * F_B * ((colourB >> 11) & 0x1f) ) >> 16;
		if (colour > 31) colour = 31;
		*colourResult = (uint16_t)(colour << 11); 
		// compose green channel
		colour = ( alphaA * F_A * ((colourA >> 5) & 0x3f) + alphaB * F_B * ((colourB >> 5) & 0x3f) ) >> 16;
		if (colour > 63) colour = 63;
		*colourResult |= (uint16_t)(colour << 5); 
		// compose blue channel
		colour = ( alphaA * F_A * (colourA & 0x1f) + alphaB * F_B * (colourB & 0x1f) ) >> 16;
		if (colour > 31) colour = 31;
		*colourResult |= (uint16_t)colour;
	}
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
