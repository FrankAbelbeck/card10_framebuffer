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
 * Graphics surface management routines for the card10 badge.
 * This variant supports perspective projection, i.e. it features full-blown
 * (CPU-hungry) homogeneous coordinate transformation routines.
 */

#include <stdlib.h> // uses: malloc(), free()
#include <stdint.h> // uses: int8_t, uint8_t, int16_t, uint16_t, uint32_t

#include "faSurfacePP.h"
#include "faSurfaceBase.h"

//------------------------------------------------------------------------------
// matrix manipulation functions (full version, 3x3 matrix, 3x1 point)
//------------------------------------------------------------------------------

PointPP divPerspective(PointPP p, int32_t zExpected) {
	if (p.z == zExpected) return p;
	if (p.z != 0) {
		p.x = p.x * zExpected / p.z;
		p.y = p.y * zExpected / p.z;
		p.z = zExpected;
	} else {
		p.x = (p.x > 0) ? INT32_MAX : INT32_MIN;
		p.y = (p.y > 0) ? INT32_MAX : INT32_MIN;
		p.z = zExpected;
	}
	return p;
}

// p' = M * p
PointPP mulMatrixPointPP(MatrixPP m, PointPP p) {
	PointPP result;
	result.x = (m.xx * p.x + m.xy * p.y + m.xz * p.z) / 1024;
	result.y = (m.yx * p.x + m.yy * p.y + m.yz * p.z) / 1024;
	result.z = (m.zx * p.y + m.zy * p.y + m.zz * p.z) / 1024;
	return result;
}

// Mresult = Ma * Mb
MatrixPP mulMatrixMatrixPP(MatrixPP a, MatrixPP b) {
	MatrixPP result;
	result.xx = (a.xx * b.xx + a.xy * b.yx + a.xz * b.zx) / 1024;
	result.xy = (a.xx * b.xy + a.xy * b.yy + a.xz * b.zy) / 1024;
	result.xz = (a.xx * b.xz + a.xy * b.yz + a.xz * b.zz) / 1024;
	result.yx = (a.yx * b.xx + a.yy * b.yx + a.yz * b.zx) / 1024;
	result.yy = (a.yx * b.xy + a.yy * b.yy + a.yz * b.zy) / 1024;
	result.yz = (a.yx * b.xz + a.yy * b.yz + a.yz * b.zz) / 1024;
	result.zx = (a.zx * b.xx + a.zy * b.yx + a.zz * b.zx) / 1024;
	result.zy = (a.zx * b.xy + a.zy * b.yy + a.zz * b.zy) / 1024;
	result.zz = (a.zx * b.xz + a.zy * b.yz + a.zz * b.zz) / 1024;
	return result;
}

// Mresult = s * M
MatrixPP mulScalarMatrixPP(int32_t scalar, MatrixPP m) {
	m.xx = (scalar * m.xx / 1024);
	m.xy = (scalar * m.xy / 1024);
	m.xz = (scalar * m.xz / 1024);
	m.yx = (scalar * m.xx / 1024);
	m.yy = (scalar * m.xy / 1024);
	m.yz = (scalar * m.xz / 1024);
	m.zx = (scalar * m.zx / 1024);
	m.zy = (scalar * m.zy / 1024);
	m.zz = (scalar * m.zz / 1024);
	return m;
}

// presult = s * p
PointPP mulScalarPointPP(int32_t scalar, PointPP p) {
	p.x = (scalar * p.x / 1024);
	p.y = (scalar * p.y / 1024);
	p.z = (scalar * p.z / 1024);
	return p;
}

MatrixPP invertMatrixPP(MatrixPP m) {
	MatrixPP inverse;
	int32_t det = (m.xx*m.yy*m.zz + m.xy*m.yz*m.zx + m.xz*m.yx*m.zy - m.xz*m.yy*m.zx - m.xx*m.yz*m.zy - m.xy*m.yx*m.zz) / 1048576;
	if (det != 0) {
		inverse.xx = (m.yy*m.zz - m.yz*m.zy) / det;
		inverse.xy = (m.xz*m.zy - m.xy*m.zz) / det;
		inverse.xz = (m.xy*m.yz - m.xz*m.yy) / det;
		
		inverse.yx = (m.yz*m.zx - m.yx*m.zz) / det;
		inverse.yy = (m.xx*m.zz - m.xz*m.zx) / det;
		inverse.yz = (m.xz*m.yx - m.xx*m.yz) / det;
		
		inverse.zx = (m.yx*m.zy - m.yy*m.zx) / det;
		inverse.zy = (m.xy*m.zx - m.xx*m.zy) / det;
		inverse.zz = (m.xx*m.yy - m.xy*m.yx) / det;
	} else {
		// matrix not invertable: should not happen, fall back to null matrix
		inverse.xx = 0;
		inverse.xy = 0;
		inverse.xz = 0;
		inverse.yx = 0;
		inverse.yy = 0;
		inverse.yz = 0;
		inverse.zx = 0;
		inverse.zy = 0;
		inverse.zz = 0;
	}
	return inverse;
}

// construct a matrix: rotation about origin
MatrixPP getMatrixRotatePP(int16_t angle) {
	MatrixPP result;
	angle = (360 + (angle % 360)) % 360;
	int16_t ca = faCos(angle); 
	int16_t sa = faSin(angle); 
	result.xx = ca;
	result.xy = -sa;
	result.xz = 0;
	result.yx = sa;
	result.yy = ca;
	result.yz = 0;
	result.zx = 0;
	result.zy = 0;
	result.zz = 1024;
	return result;
}

// construct a matrix: scale about origin
MatrixPP getMatrixScalePP(int16_t factorX, int16_t factorY) {
	MatrixPP result;
	result.xx = factorX;
	result.xy = 0;
	result.xz = 0;
	result.yx = 0;
	result.yy = factorY;
	result.yz = 0;
	result.zx = 0;
	result.zy = 0;
	result.zz = 1024;
	return result;
}

// construct a matrix: translate by offset
MatrixPP getMatrixTranslatePP(int16_t x, int16_t y) {
	MatrixPP result;
	result.xx = 1024;
	result.xy = 0;
	result.xz = x*1024;
	result.yx = 0;
	result.yy = 1024;
	result.yz = y*1024;
	result.zx = 0;
	result.zy = 0;
	result.zz = 1024;
	return result;
}

// construct a matrix: shear along x axis
MatrixPP getMatrixShearXPP(int16_t factor) {
	MatrixPP result;
	result.xx = 1024;
	result.xy = factor;
	result.xz = 0;
	result.yx = 0;
	result.yy = 1024;
	result.yz = 0;
	result.zx = 0;
	result.zy = 0;
	result.zz = 1024;
	return result;
}

// construct a matrix: shear along y axis
MatrixPP getMatrixShearYPP(int16_t factor) {
	MatrixPP result;
	result.xx = 1024;
	result.xy = 0;
	result.xz = 0;
	result.yx = factor;
	result.yy = 1024;
	result.yz = 0;
	result.zx = 0;
	result.zy = 0;
	result.zz = 1024;
	return result;
}

// construct a matrix: perspective transformation
MatrixPP getMatrixPerspective(int16_t factorX, int16_t factorY, int16_t factorZ) {
	MatrixPP result;
	result.xx = 1024;
	result.xy = 0;
	result.xz = 0;
	result.yx = 0;
	result.yy = 1024;
	result.yz = 0;
	result.zx = factorX;
	result.zy = factorY;
	result.zz = factorZ;
	return result;
}

//------------------------------------------------------------------------------
// surface composition functions
//------------------------------------------------------------------------------

// paint sprite transformed by given matrix on surface, using given transparency value and blend mode
BoundingBox composePP(Surface *surface, Surface *sprite, Surface *destination, MatrixPP matrix, uint8_t alpha, uint8_t mode, BoundingBox boundingBoxSprite) {
	// 2020-01-09: move from "3 shears" to "general affine transformation", i.e. p' = A*p
	//             problem: interpolation
	//             anti-aliasing/interpolation via blendFractional() does not work
	// 2020-01-12: solution works, but shows non-interpolation pattern
	// 2020-01-13: try other way 'round: parameter "matrix" and "matrixInverse"
	//              - calculate surface bounding box with matrix
	//              - iteratore over surface coordinates of the bounding box
	//              - calculate p = A^-1 * p', i.e. get sprite coordinates and paint surface coordinates accordingly
	// 2020-01-20: introduced boundingBoxSprite parameter
	BoundingBox boundingBox = createBoundingBox(0,0,0,0);
	
	// sanity check: bail out if invalid parameters were given
	if (surface == NULL || sprite == NULL || destination == NULL || \
		surface->width != destination->width || surface->height != destination->height || \
		boundingBoxSprite.min.x >= sprite->width || boundingBoxSprite.min.y >= sprite->height || \
		boundingBoxSprite.max.x < 0 || boundingBoxSprite.max.y < 0)
		return boundingBox;
	
	PointPP pMin,pMax,pMod;
	
	// step one: calculate bounding box of transformed sprite in surface space
	// new bounding box = bounding box sprite transformed by matrix
	if (boundingBoxSprite.min.x < 0) boundingBoxSprite.min.x = 0;
	if (boundingBoxSprite.min.y < 0) boundingBoxSprite.min.y = 0;
	if (boundingBoxSprite.max.x >= sprite->width) boundingBoxSprite.max.x = sprite->width-1;
	if (boundingBoxSprite.max.y >= sprite->height) boundingBoxSprite.max.y = sprite->height-1;
	
	// bounding box: min x, min y
	pMin.x = 1024*boundingBoxSprite.min.x;
	pMin.y = 1024*boundingBoxSprite.min.y;
	pMin.z = 1024;
	pMin = divPerspective(mulMatrixPointPP(matrix,pMin),1024);
	pMax = pMin;
	
	// bounding box: max x, min y
	pMod.x = 1024*boundingBoxSprite.max.x;
	pMod.y = 1024*boundingBoxSprite.min.y;
	pMod.z = 1024;
	pMod = divPerspective(mulMatrixPointPP(matrix,pMod),1024);
	if (pMod.x < pMin.x) pMin.x = pMod.x;
	if (pMod.x > pMax.x) pMax.x = pMod.x;
	if (pMod.y < pMin.y) pMin.y = pMod.y;
	if (pMod.y > pMax.y) pMax.y = pMod.y;
	
	// bounding box: min x, max y
	pMod.x = 1024*boundingBoxSprite.min.x;
	pMod.y = 1024*boundingBoxSprite.max.y;
	pMod.z = 1024;
	pMod = divPerspective(mulMatrixPointPP(matrix,pMod),1024);
	if (pMod.x < pMin.x) pMin.x = pMod.x;
	if (pMod.x > pMax.x) pMax.x = pMod.x;
	if (pMod.y < pMin.y) pMin.y = pMod.y;
	if (pMod.y > pMax.y) pMax.y = pMod.y;
	
	// bounding box: max x, max y
	pMod.x = 1024*boundingBoxSprite.max.x;
	pMod.y = 1024*boundingBoxSprite.max.y;
	pMod.z = 1024;
	pMod = divPerspective(mulMatrixPointPP(matrix,pMod),1024);
	if (pMod.x < pMin.x) pMin.x = pMod.x;
	if (pMod.x > pMax.x) pMax.x = pMod.x;
	if (pMod.y < pMin.y) pMin.y = pMod.y;
	if (pMod.y > pMax.y) pMax.y = pMod.y;
		
	// divide by 1024; if remainder is greater than 0.5, round to next pixel in that direction
	if (pMax.x < 0) {
		// negative maximum x value: not visible, exit function
		return boundingBox;
	} else {
		boundingBox.max.x = pMax.x >> 10;
		if ((pMax.x & 1023) >= 512) boundingBox.max.x++;
		if (boundingBox.max.x >= surface->width) boundingBox.max.x = surface->width - 1;
	}
	
	if (pMax.y < 0) {
		// negative maximum y value: not visible, exit function
		return boundingBox;
	} else {
		boundingBox.max.y = pMax.y >> 10;
		if ((pMax.y & 1023) >= 512) boundingBox.max.y++;
		if (boundingBox.max.y >= surface->height) boundingBox.max.y = surface->height - 1;
	}
	
	if (pMin.x < 0) {
		boundingBox.min.x = 0;
	} else {
		boundingBox.min.x = pMin.x >> 10;
		if ((pMin.x & 1023) >= 512) boundingBox.min.x++;
		if (boundingBox.min.x >= surface->width) return boundingBox; // minimum x value greater than width: not visible, exit function
	}
	
	if (pMin.y < 0) {
		boundingBox.min.y = 0;
	} else {
		boundingBox.min.y = pMin.y >> 10;
		if ((pMin.y & 1023) >= 512) boundingBox.min.y++;
		if (boundingBox.min.y >= surface->height) return boundingBox; // minimum y value greater than height: not visible, exit function
	}
	
	// bounding box and surface overlap, continue
	// calculate inverse of transformation matrix
	MatrixPP inverse = invertMatrixPP(matrix);
	
	// iterate over boundingBox area of surface
	int16_t x,y,xSprite,ySprite;
	uint32_t iSurface = boundingBox.min.y * surface->width + boundingBox.min.x;
	uint32_t diSurface = surface->width - (boundingBox.max.x - boundingBox.min.x) - 1;
	uint32_t iSprite;
	for (y = boundingBox.min.y; y <= boundingBox.max.y; y++) {
		for (x = boundingBox.min.x; x <= boundingBox.max.x; x++) {
			// calculate sprite coordinates:
			// 1) apply inverse matrix
			pMod.x = x << 10;
			pMod.y = y << 10;
			pMod.z = 1024;
			pMod = mulMatrixPointPP(inverse,pMod);
			
			// 2) perform perspective divide, i.e. map coordinates to z=1 (view plane)
			if (pMod.z == 0) {
				// z is zero; would result in division by zero error; coordinates outside screen area, skip
				iSurface++;
				continue;
			} else {
				pMod.x = 1024 * pMod.x / pMod.z;
				pMod.y = 1024 * pMod.y / pMod.z;
			}
			
			// 3) calculate pixel coordinates, round up if fraction is >0.5
			//    and check if inside area (otherwise skip iteration)
			if (pMod.x < boundingBoxSprite.min.x) { iSurface++; continue; }
			xSprite = pMod.x >> 10;
			if ((pMod.x & 1023) >= 512) xSprite++;
			if (xSprite > boundingBoxSprite.max.x) { iSurface++; continue; }
			
			if (pMod.y < boundingBoxSprite.min.y) { iSurface++; continue; }
			ySprite = pMod.y >> 10;
			if ((pMod.y & 1023) >= 512) ySprite++;
			if (ySprite > boundingBoxSprite.max.y) { iSurface++; continue; }
			
			// 4) calculate sprite pixel index
			iSprite = ySprite * sprite->width + xSprite;
			
			// 5) blend pixel (destination = sprite op surface)
			blend(
				sprite->rgb565[iSprite],(alpha * sprite->alpha[iSprite]) >> 8,
				surface->rgb565[iSurface],surface->alpha[iSurface],
				&destination->rgb565[iSurface],&destination->alpha[iSurface],
				mode
			);
			
			// next pixel
			iSurface++;
		} // for x
		// skip remainder of line and beginning of next line (xMax --> width, xMin)
		iSurface += diSurface;
	} // for y
	
	return boundingBox;
}
