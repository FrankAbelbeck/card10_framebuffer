/**
 * @file
 * @author Frank Abelbeck <frank.abelbeck@googlemail.com>
 * @version 2020-02-14
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

#include "faSurface.h" // custom bitmap surface lib
#include "faSurfacePP.h" // custom bitmap surface lib (perspective projection)
#include "faFramebuffer.h" // custom framebuffer access lib
#include "faReadPng.h" // custom PNG reader lib

#include "epicardium.h" // card10 API access
#include "FreeRTOS.h" // needed for access to vTaskDelay() and pdMS_TO_TICKS() in FreeRTOS SDK

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
	SurfaceMod *mask;
	Matrix matrix;
	MatrixPP matrixPP;
	Point p;
	Surface *background, *frontbuffer, *sprite, *logo; 
	BoundingBox bbSprite;
	int16_t scale,x,y,alpha,angle;
	int8_t dx,dy,dalpha,dangle,dscale;
	
	printf("starting surfacedemo...\n");
	
	// prepare surface update mask
	printf("creating update mask\n");
	mask = surfaceModConstruct(DISP_HEIGHT);
	if (mask == NULL) {
		printf("could not allocate update mask\n");
		epic_exit(1);
	}
	
	// prepare framebuffer and sprite
	printf("creating framebuffer\n");
	framebuffer = framebufferConstruct(0);
	if (framebuffer == NULL) {
		printf("could not allocate framebuffer\n");
		surfaceModDestruct(&mask);
		epic_exit(1);
	}
	
	// set up stars background
	printf("creating background surface\n");
	background = pngDataLoad("png/stars.png");
	if (background == NULL) {
		printf("could not set-up background surface\n");
		surfaceModDestruct(&mask);
		framebufferDestruct(&framebuffer);
		epic_exit(1);
	}
	
	// set up front buffer surface
	printf("creating frontbuffer surface\n");
	frontbuffer = surfaceClone(background);
	if (frontbuffer == NULL) {
		printf("could not set-up frontbuffer surface\n");
		surfaceModDestruct(&mask);
		framebufferDestruct(&framebuffer);
		surfaceDestruct(&background);
		epic_exit(1);
	}
	
	// set up the opening title
	printf("loading title image\n");
	sprite = pngDataLoad("png/title.png");
	if (sprite == NULL) {
		printf("could not set-up title sprite surface\n");
		surfaceModDestruct(&mask);
		framebufferDestruct(&framebuffer);
		surfaceDestruct(&background);
		surfaceDestruct(&frontbuffer);
		epic_exit(1);
	}
	printf("loop: star wars titles...\n");
	for (scale = 1024; scale >= 0; scale -= 8) {
		matrix = getMatrixTranslate(-sprite->width/2,-sprite->height/2);
		matrix = mulMatrixMatrix(getMatrixScale(scale,scale),matrix);
		matrix = mulMatrixMatrix(getMatrixTranslate(80,40),matrix);
		
		if (scale > 255)
			compose(background,sprite,frontbuffer,matrix,255,BLEND_OVER,boundingBoxGet(sprite),mask);
		else
			compose(background,sprite,frontbuffer,matrix,scale,BLEND_OVER,boundingBoxGet(sprite),mask);
		
		framebufferCopySurface(framebuffer,frontbuffer);
		framebufferRedraw(framebuffer);
		surfaceCopyMask(background,frontbuffer,mask);
		surfaceModClear(mask);
	}
	
	surfaceDestruct(&sprite);
	printf("loading text image\n");
	sprite = pngDataLoad("png/text.png");
	if (sprite == NULL) {
		printf("could not set-up text sprite surface\n");
		surfaceModDestruct(&mask);
		framebufferDestruct(&framebuffer);
		surfaceDestruct(&background);
		surfaceDestruct(&frontbuffer);
		epic_exit(1);
	}
	
	for (y = 0; y <= sprite->height+64; y++) {
		matrixPP = getMatrixTranslatePP(-sprite->width/2,-64-y);
		matrixPP = mulMatrixMatrixPP(getMatrixPerspective(0,-4,256),matrixPP);
		matrixPP = mulMatrixMatrixPP(getMatrixTranslatePP(DISP_WIDTH/2,200),matrixPP);
		
		if (y <= sprite->height)
			composePP(background,sprite,frontbuffer,matrixPP,255,BLEND_OVER,boundingBoxCreate(0,0,sprite->width-1,y),mask);
		else
			composePP(background,sprite,frontbuffer,matrixPP,256 - (y-sprite->height)*4,BLEND_OVER,boundingBoxCreate(0,0,sprite->width-1,y),mask);
		
		framebufferCopySurface(framebuffer,frontbuffer);
		framebufferRedraw(framebuffer);
		surfaceCopyMask(background,frontbuffer,mask);
		surfaceModClear(mask);
	}
	
	surfaceDestruct(&sprite);
	printf("loading sprite image\n");
	sprite = pngDataLoad("png/sprite.png");
	if (sprite == NULL) {
		printf("could not set-up sprite surface\n");
		surfaceModDestruct(&mask);
		framebufferDestruct(&framebuffer);
		surfaceDestruct(&background);
		surfaceDestruct(&frontbuffer);
		epic_exit(1);
	}
	printf("loading logo image\n");
	logo = pngDataLoad("png/sprite-logo.png");
	if (logo == NULL) {
		printf("could not set-up logo sprite surface\n");
		surfaceModDestruct(&mask);
		framebufferDestruct(&framebuffer);
		surfaceDestruct(&background);
		surfaceDestruct(&frontbuffer);
		surfaceDestruct(&sprite);
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
	
	p.x = 80;
	p.y = 40;
	
	printf("loop: rotating/moving sprites...\n");
	for (uint16_t counter = 0; counter < 768; counter++) {
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
		
		bbSprite = compose(background,sprite,frontbuffer,matrix,alpha,BLEND_OVER,boundingBoxGet(sprite),mask);
		
		matrix = getMatrixTranslate(-logo->width/2,-logo->height/2);
		matrix = mulMatrixMatrix(getMatrixScale(scale,512),matrix);
		matrix = mulMatrixMatrix(getMatrixTranslate(80,40),matrix);
		compose(frontbuffer,logo,frontbuffer,matrix,255,BLEND_OVER,boundingBoxGet(logo),mask);
		
		surfaceDrawLine(frontbuffer,p,createPoint(80+surfaceCosine(angle)/32,40+surfaceSine(angle)/32),0xfff,0xff,BLEND_OVER,mask);
		surfaceDrawArc(frontbuffer,p,32,angle,angle+120,0xffff,0xff,BLEND_OVER,mask);
		surfaceDrawArc(frontbuffer,p,28,angle-60,angle+60,0xffff,0xff,BLEND_OVER,mask);
		surfaceDrawArc(frontbuffer,p,24,angle-120,angle-60,0xffff,0xff,BLEND_OVER,mask);
		
// 		framebufferUpdateFromSurface(framebuffer,frontbuffer,mask);
// 		framebufferRedraw(framebuffer);
// 		framebufferUpdateFromSurface(framebuffer,background,mask);
		
		framebufferCopySurface(framebuffer,frontbuffer);
		framebufferRedraw(framebuffer);
		surfaceCopyMask(background,frontbuffer,mask);
		surfaceModClear(mask);
		
		// check clipping
		if ((bbSprite.max.x >= DISP_WIDTH-1  && dx > 0) || (bbSprite.min.x <= 0 && dx < 0)) dx = -dx;
		if ((bbSprite.max.y >= DISP_HEIGHT-1 && dy > 0) || (bbSprite.min.y <= 0 && dy < 0)) dy = -dy;
		// check angle mod 360
		angle = (360 + (angle % 360)) % 360;
		// check alpha mod 255
		if (alpha <= 0 || alpha >= 255) dalpha = -dalpha;
		// check scale
		if (scale >= 512 || scale <= -512) dscale = -dscale;
	}
	
	// clean up and exit
	surfaceModDestruct(&mask);
	surfaceDestruct(&background);
	surfaceDestruct(&sprite);
	surfaceDestruct(&logo);
	framebufferDestruct(&framebuffer);
	epic_exit(0);
	return 0;
}
