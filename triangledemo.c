/**
 * @file
 * @author Frank Abelbeck <frank.abelbeck@googlemail.com>
 * @version 2020-02-17
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
 * Demo/testbed for the card10 triangle drawing and image surface library.
 */
#include <stdio.h> // uses printf()

#include "epicardium.h" // card10 API access

#include "faSurface.h" // custom bitmap surface lib
#include "faFramebuffer.h" // custom framebuffer access lib
#include "faReadPng.h" // custom PNG reader lib


#define CAM_DX DISP_WIDTH/2
#define CAM_DY DISP_HEIGHT/2
#define CAM_DZ 65536

#define CAM_SX 1024
#define CAM_SY 1024


typedef struct {
	int32_t x,y,z;
} Point3D;

typedef struct {
	uint8_t  p0,p1,p2,p3;
	uint16_t colour;
	uint8_t  alpha;
} Triangle;


// apply rotation according to the yaw-pitch-roll convention:
//  1st rotate around z by yaw
//  2nd rotate around y' by pitch
//  3rd rotate around x'' by roll
Point3D rotate3D(Point3D p, int16_t roll, int16_t pitch, int16_t yaw) {
	int16_t cosRoll   = surfaceCosine(roll);
	int16_t cosPitch = surfaceCosine(pitch);
	int16_t cosYaw   = surfaceCosine(yaw);
	int16_t sinRoll   = surfaceSine(roll);
	int16_t sinPitch = surfaceSine(pitch);
	int16_t sinYaw   = surfaceSine(yaw);
	Point3D result;
	result.x = p.x * (cosPitch*cosYaw) + p.y * (-cosRoll*sinYaw + ((sinRoll*sinPitch*cosYaw)>>10)) + p.z * (sinRoll*sinYaw + ((cosRoll*sinPitch*cosYaw)>>10));
	result.y = p.x * (cosPitch*sinYaw) + p.y * (cosRoll*cosYaw + ((sinRoll*sinPitch*sinYaw)>>10))  + p.z * (-sinRoll*cosYaw + ((cosRoll*sinPitch*sinYaw)>>10));
	result.z = p.x * (-sinPitch << 10) + p.y * (sinRoll*cosPitch)                                 + p.z * (cosRoll*cosPitch);
	result.x >>= 20;
	result.y >>= 20;
	result.z >>= 20;
	return result;
}

Point3D normaliseVector(Point3D p) {
	int64_t norm2 = p.x*p.x + p.y*p.y + p.z*p.z;
	int32_t norm = 0;
	int32_t bit = 1 << 30;
	
	// calculate integer sqrt (see wikipedia for details)
	while (bit > norm2) bit >>= 2;
    while (bit != 0) {
        if (norm2 >= norm + bit) {
            norm2 -= norm + bit;
            norm = (norm >> 1) + bit;
        }
        else
            norm >>= 1;
        bit >>= 2;
    }
    
    if (norm != 0) {
		Point3D result;
		result.x = (p.x << 10) / norm;
		result.y = (p.y << 10) / norm;
		result.z = (p.z << 10) / norm;
		return result;
	} else {
		return p;
	}
}

Point3D vectorDiff(Point3D a, Point3D b) {
	Point3D result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	result.z = a.z - b.z;
	return result;
}

Point3D crossProduct(Point3D a, Point3D b) {
	Point3D result;
	result.x = (a.y*b.z - a.z*b.y) >> 10;
	result.y = (a.z*b.x - a.x*b.z) >> 10;
	result.z = (a.x*b.y - a.y*b.x) >> 10;
	return result;
}


void doCleanExit(char *reason, int numError, union disp_framebuffer **framebuffer, Surface **background, Surface **frontbuffer, SurfaceMod **mask) {
	printf("%s\n",reason);
	framebufferDestruct(framebuffer);
	surfaceDestruct(background);
	surfaceDestruct(frontbuffer);
	surfaceModDestruct(mask);
	epic_exit(numError);
}


int main(int argc, char **argv) {
	union disp_framebuffer *framebuffer = NULL;
	SurfaceMod *mask = NULL;
	Surface *background = NULL;
	Surface *frontbuffer = NULL; 
	
	printf("starting triangledemo...\n");
	
	// prepare surface update mask
	printf("creating update mask\n");
	mask = surfaceModConstruct(DISP_HEIGHT);
	if (mask == NULL) doCleanExit("could not set up update mask",-1,&framebuffer,&background,&frontbuffer,&mask);
	
	// prepare framebuffer and sprite
	printf("creating framebuffer\n");
	framebuffer = framebufferConstruct(0);
	if (framebuffer == NULL) doCleanExit("could not set up framebuffer",-1,&framebuffer,&background,&frontbuffer,&mask);
	
	// set up stars background
	printf("creating background surface\n");
	background = pngDataLoad("png/stars.png");
	if (background == NULL) doCleanExit("could not set up background surface",-1,&framebuffer,&background,&frontbuffer,&mask);
	
	// set up front buffer surface
	printf("creating frontbuffer surface\n");
	frontbuffer = surfaceClone(background);
	if (frontbuffer == NULL) doCleanExit("could not set up frontbuffer surface",-1,&framebuffer,&background,&frontbuffer,&mask);
	
	// prepare vertices of the cube
	Point3D vertices[8] = {
		{ 1024, 1024, 1024}, // 0
		{-1024, 1024, 1024}, // 1
		{ 1024,-1024, 1024}, // 2
		{-1024,-1024, 1024}, // 3
		{ 1024, 1024,-1024}, // 4
		{-1024, 1024,-1024}, // 5
		{ 1024,-1024,-1024}, // 6
		{-1024,-1024,-1024}  // 7
	};
	
	// prepare cube faces: assign vertices to faces
	// note: order of vertices determines face normal direction since crossproduct of p0p1 and p0p2 is used
	// applying the right-hand rule, those normals should point outward
	Triangle triangles[6] = {
		{0,2,4,6,MKRGB565(255,  0,  0),255},
		{1,5,3,7,MKRGB565(255,255,  0),255},
		{0,4,1,5,MKRGB565(  0,255,  0),255},
		{2,3,6,7,MKRGB565(  0,255,255),255},
		{0,1,2,3,MKRGB565(  0,  0,255),255},
		{4,6,5,7,MKRGB565(255,  0,255),255}
	};
	
	// light source: assume one vector, valid for all surfaces, i.e. like the sun
	// note: shading is done with the dot product; the angle between surface normals
	// and the light vector determines the amount of shading
	//    angle = 0 ----> full colour
	//    angle = 45 ---> colour shaded with black at alpha=127
	//    angle = 90 ---> colour shaded with black at alpha=255 (surface is black)
	// thus the light source vector points towards the sun
	Point3D lightSource = {0,0,-1024};
	
	// pre-calculate face normals (for shading)
	uint8_t k;
	Point3D normals[6];
	for (k=0; k<6; k++) {
		normals[k] = normaliseVector(
			crossProduct(
				vectorDiff(vertices[triangles[k].p0],vertices[triangles[k].p1]),
				vectorDiff(vertices[triangles[k].p0],vertices[triangles[k].p2])
			)
		);
	}
	
	Point3D p,pLight;
	Point p0,p1,p2,p3;
	uint16_t colour;
	uint8_t alpha;
	int16_t shading;
	
	int16_t yaw   = 0; // angle around z axis
	int16_t pitch = 0; // angle around y' axis
	int16_t roll  = 0; // angle around x'' axis
	int8_t dYaw   = 1;
	int8_t dPitch = 1;
	int8_t dRoll  = 1;
	
	int16_t pitchLight = 0;
	int8_t dPitchLight = 1;
	
	uint8_t buttons,buttonsPressed,buttonsReleased;
	uint8_t buttonsOld = 0;
	
	bool doRotateCube = true;
	bool doRotateLight = false;
	bool doShading = true;
	
	while (1) {
		for (k=0; k<6; k++) {
			// first, check visibility of the cube face:
			// rotate face normal and calculate dot product to estimate
			// the cosine of the angle between face normal and camera normal
			// since we have a fixed camera looking into z direction, the normals
			// should look out of the z-plane, i.e. be parallel with (0,0,-1024),
			// this is dot product is reduced to -p.z * 1024 / 1024 = -p.z = cos angle;
			// visibile = cos angle = -p.z > 0 (i.e. -90 < angle < 90)
			// => p.z < 0 (*(-1) on both sides)
			// => continue with next iteration if p.x >= 0
			p = rotate3D(normals[k],yaw,pitch,roll);
			if (p.z >= 0) continue; // break since angle outside (-90,90)
			
			// calculate shading value: dot product of normal and light vector,
			// scaled to 0..255, because it is used as an alpha value to map
			// the colour black onto the cube face's colour
			if (doShading) {
				pLight = rotate3D(lightSource,0,pitchLight,0);
				shading = surfaceArcusCosine((p.x * pLight.x + p.y * pLight.y + p.z * pLight.z) >> 10);
				if (shading >= 90) shading = 255; else shading = 255*shading/90;
				surfacePixelBlend(0,shading,triangles[k].colour,triangles[k].alpha,&colour,&alpha,BLEND_OVER);
			} else {
				colour = triangles[k].colour;
				alpha = triangles[k].alpha;
			}
			
			// get transformed coordinates of this cube face
			// naive projection: divide x and y by z, scale and shift output (see CAM_* defs)
			p = rotate3D(vertices[triangles[k].p0],roll,pitch,yaw);
			p.z += CAM_DZ;
			if (p.z == 0) continue;
			p0.x = CAM_DX + CAM_SX * p.x / p.z;
			p0.y = CAM_DY + CAM_SY * p.y / p.z;
			
			p = rotate3D(vertices[triangles[k].p1],roll,pitch,yaw);
			p.z += CAM_DZ;
			if (p.z == 0) continue;
			p1.x = CAM_DX + CAM_SX * p.x / p.z;
			p1.y = CAM_DY + CAM_SY * p.y / p.z;
			
			p = rotate3D(vertices[triangles[k].p2],roll,pitch,yaw);
			p.z += CAM_DZ;
			if (p.z == 0) continue;
			p2.x = CAM_DX + CAM_SX * p.x / p.z;
			p2.y = CAM_DY + CAM_SY * p.y / p.z;
			
			p = rotate3D(vertices[triangles[k].p3],roll,pitch,yaw);
			p.z += CAM_DZ;
			if (p.z == 0) continue;
			p3.x = CAM_DX + CAM_SX * p.x / p.z;
			p3.y = CAM_DY + CAM_SY * p.y / p.z;
			
			// draw triangles (2 per face)
			surfaceDrawTriangle(frontbuffer,p0,p1,p2,colour,alpha,BLEND_OVER,mask);
			surfaceDrawTriangle(frontbuffer,p1,p2,p3,colour,alpha,BLEND_OVER,mask);
		}
		
		// update framebuffer
		framebufferCopySurface(framebuffer,frontbuffer);
		framebufferRedraw(framebuffer);
		surfaceCopyMask(background,frontbuffer,mask);
		surfaceModClear(mask);
		
		buttons = epic_buttons_read(BUTTON_LEFT_BOTTOM | BUTTON_RIGHT_BOTTOM | BUTTON_LEFT_TOP | BUTTON_RIGHT_TOP);
		buttonsPressed = (buttonsOld ^ buttons) & buttons;
		buttonsReleased = (buttonsOld ^ buttons) & buttonsOld;
		if (buttonsReleased & BUTTON_RIGHT_TOP) {
			// select button released
			// switch on/off shading
			doShading = !doShading;
		} else if (buttonsReleased & BUTTON_LEFT_BOTTOM) {
			// left menu button released
			doRotateLight = !doRotateLight;
		} else if (buttonsReleased & BUTTON_RIGHT_BOTTOM) {
			// right menu button released
			doRotateCube = !doRotateCube;
		} else if (buttonsReleased & BUTTON_LEFT_TOP) {
			// reset button released; try to clean up before app terminates
			break;
		}
		buttonsOld = buttons;
		
		if (doRotateCube) {
			// update angles
			yaw = (360 + ((yaw + dYaw) % 360)) % 360;
			pitch = (360 + ((pitch + dPitch) % 360)) % 360;
			roll = (360 + ((roll + dRoll) % 360)) % 360;
		};
		if (doRotateLight) {
			// rotate light source
			pitchLight = (360 + ((pitchLight + dPitchLight) % 360)) % 360;
		}
	}
	
	// clean up and exit
	doCleanExit("exiting triangledemo",0,&framebuffer,&background,&frontbuffer,&mask);
	return 0;
}
