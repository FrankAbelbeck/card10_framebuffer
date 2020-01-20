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
 * Demo/testbed for the card10 image surface library.
 */
#include <stdio.h> // uses printf()

#include "epicardium.h" // card10 API access

#include "faSurface.h" // custom bitmap surface lib
#include "faSurfacePP.h" // custom bitmap surface lib (perspective projection)
#include "faFramebuffer.h" // custom framebuffer access lib
#include "faReadPng.h" // custom PNG reader lib

void updateLED(uint8_t load) {
	// simple load colouring: red increases, green decreases --> green/yellow/red
	uint8_t r = (uint8_t)((uint16_t)load * 255 / 10);
	uint8_t g = 255 - r;
	uint8_t n;
	// set first load LEDS
	for (n = 0; n < load; n++) epic_leds_prep(n,r,g,0);
	// clear remaining LEDS and write changes 
	for (; n < 11; n++) epic_leds_prep(n,0,0,0);
	epic_leds_update();
}

int main(int argc, char **argv) {
	union disp_framebuffer *framebuffer;
	BoundingBox boundingBox, boundingBoxLogo;
	Matrix matrix;
	MatrixPP matrixPP;
	Surface *background, *frontbuffer, *sprite, *logo; 
	int16_t scale,x,y,alpha,angle;
	int8_t dx,dy,dalpha,dangle,dscale;
	
	printf("starting surfacedemo...\n");
	
	// prepare framebuffer and sprite
	framebuffer = constructFramebuffer(0);
	if (framebuffer == NULL) {
		printf("could not allocate framebuffer\n");
		epic_exit(1);
	}
	
	// set up stars background
	background = loadImage("png/stars.png");
	if (background == NULL) {
		printf("could not set-up background surface\n");
		destructFramebuffer(&framebuffer);
		epic_exit(1);
	}
	
	// set up front buffer surface
	frontbuffer = cloneSurface(background);
	if (frontbuffer == NULL) {
		printf("could not set-up frontbuffer surface\n");
		destructFramebuffer(&framebuffer);
		destructSurface(&background);
		epic_exit(1);
	}
	
	// set up the opening title
	sprite = loadImage("png/title.png");
	if (sprite == NULL) {
		printf("could not set-up title sprite surface\n");
		destructFramebuffer(&framebuffer);
		destructSurface(&background);
		destructSurface(&frontbuffer);
		epic_exit(1);
	}
	printf("loop: star wars titles...\n");
	for (scale = 1024; scale >= 0; scale -= 8) {
		matrix = getMatrixTranslate(-sprite->width/2,-sprite->height/2);
		matrix = mulMatrixMatrix(getMatrixScale(scale,scale),matrix);
		matrix = mulMatrixMatrix(getMatrixTranslate(80,40),matrix);
		
		if (scale > 255)
			boundingBox = compose(background,sprite,frontbuffer,matrix,255,BLEND_OVER,getBoundingBoxSurface(sprite));
		else
			boundingBox = compose(background,sprite,frontbuffer,matrix,scale,BLEND_OVER,getBoundingBoxSurface(sprite));
		
		copySurfaceToFramebuffer(frontbuffer,framebuffer);
		redraw(framebuffer);
		copySurfaceAreaToSurface(background,frontbuffer,boundingBox);
	}
	
	destructSurface(&sprite);
	sprite = loadImage("png/text.png");
	if (sprite == NULL) {
		printf("could not set-up text sprite surface\n");
		destructFramebuffer(&framebuffer);
		destructSurface(&background);
		destructSurface(&frontbuffer);
		epic_exit(1);
	}
	
	for (y = 0; y <= sprite->height+64; y++) {
		matrixPP = getMatrixTranslatePP(-sprite->width/2,-64-y);
		matrixPP = mulMatrixMatrixPP(getMatrixPerspective(0,-4,256),matrixPP);
		matrixPP = mulMatrixMatrixPP(getMatrixTranslatePP(DISP_WIDTH/2,200),matrixPP);
		
		if (y <= sprite->height)
			boundingBox = composePP(background,sprite,frontbuffer,matrixPP,255,BLEND_OVER,createBoundingBox(0,0,sprite->width-1,y));
		else
			boundingBox = composePP(background,sprite,frontbuffer,matrixPP,256 - (y-sprite->height)*4,BLEND_OVER,createBoundingBox(0,0,sprite->width-1,y));
		
		copySurfaceToFramebuffer(frontbuffer,framebuffer);
		redraw(framebuffer);
		copySurfaceAreaToSurface(background,frontbuffer,boundingBox);
	}
	
	destructSurface(&sprite);
	sprite = loadImage("png/sprite.png");
	if (sprite == NULL) {
		printf("could not set-up sprite surface\n");
		destructFramebuffer(&framebuffer);
		destructSurface(&background);
		destructSurface(&frontbuffer);
		epic_exit(1);
	}
	logo = loadImage("png/sprite-logo.png");
	if (logo == NULL) {
		printf("could not set-up logo sprite surface\n");
		destructFramebuffer(&framebuffer);
		destructSurface(&background);
		destructSurface(&frontbuffer);
		destructSurface(&sprite);
		epic_exit(1);
	}
	
	x = 21;
	y = 42;
	angle = 0;
	alpha = 255;
	dx = 2;
	dy = 1;
	dangle = 1;
	dalpha = -1;
	dscale = -8;
	scale = 512;
	
	printf("loop: rotating/moving sprites...\n");
	for (uint16_t counter = 0; counter < 1000; counter++) {
		// update motion variables
		x += dx;
		y += dy;
		angle += dangle;
		alpha += dalpha;
		scale += dscale;
		
		// do image/surface transformations and framebuffer update
		matrix = getMatrixTranslate(-sprite->width/2,-sprite->height/2);
		matrix = mulMatrixMatrix(getMatrixRotate(angle),matrix);
		matrix = mulMatrixMatrix(getMatrixTranslate(x+sprite->width/2,y+sprite->height/2),matrix);
		
		boundingBox = compose(background,sprite,frontbuffer,matrix,alpha,BLEND_OVER,getBoundingBoxSurface(sprite));
		
		matrix = getMatrixTranslate(-logo->width/2,-logo->height/2);
		matrix = mulMatrixMatrix(getMatrixScale(scale,512),matrix);
		matrix = mulMatrixMatrix(getMatrixTranslate(80,40),matrix);
		boundingBoxLogo = compose(frontbuffer,logo,frontbuffer,matrix,255,BLEND_OVER,getBoundingBoxSurface(logo));
		
		copySurfaceToFramebuffer(frontbuffer,framebuffer);
		redraw(framebuffer);
		copySurfaceAreaToSurface(background,frontbuffer,boundingBox);
		copySurfaceAreaToSurface(background,frontbuffer,boundingBoxLogo);
		
		// check clipping
		if ((boundingBox.max.x >= DISP_WIDTH-1 && dx > 0) || (boundingBox.min.x <= 0 && dx < 0)) dx = -dx;
		if ((boundingBox.max.y >= DISP_HEIGHT-1 && dy > 0) || (boundingBox.min.y <= 0 && dy < 0)) dy = -dy;
		// check angle mod 360
		angle = (360 + (angle % 360)) % 360;
		// check alpha mod 255
		if (alpha <= 0 || alpha >= 255) dalpha = -dalpha;
		// check scale
		if (scale >= 512 || scale <= -512) dscale = -dscale;
	}
	
	// clean up and exit
	destructSurface(&background);
	destructSurface(&sprite);
	destructSurface(&logo);
	destructFramebuffer(&framebuffer);
	epic_exit(0);
	return 0;
}
