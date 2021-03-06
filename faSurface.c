/**
 * @file
 * @author Frank Abelbeck <frank.abelbeck@googlemail.com>
 * @version 2020-02-12
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
 * This variant does not support perspective projection.
 * This reduces load of homogeneous coordinate transformation significantly.
 */

#include <stdlib.h> // uses: malloc(), free()
#include <stdint.h> // uses: int8_t, uint8_t, int16_t, uint16_t, uint32_t

#include "faSurface.h"
#include "faSurfaceBase.h"

//------------------------------------------------------------------------------
// matrix manipulation functions
// assuming that zx=zy=0 and zz=1
//------------------------------------------------------------------------------

// p' = M * p
Point mulMatrixPoint(Matrix m, Point p) {
	Point result;
	result.x = ((m.xx * p.x + m.xy * p.y) >> 10) + m.xz;
	result.y = ((m.yx * p.x + m.yy * p.y) >> 10) + m.yz;
	return result;
}

// Mresult = Ma * Mb
Matrix mulMatrixMatrix(Matrix a, Matrix b) {
	Matrix result;
	result.xx = (a.xx * b.xx + a.xy * b.yx) >> 10;
	result.xy = (a.xx * b.xy + a.xy * b.yy) >> 10;
	result.xz = ((a.xx * b.xz + a.xy * b.yz) >> 10) + a.xz;
	result.yx = (a.yx * b.xx + a.yy * b.yx) >> 10;
	result.yy = (a.yx * b.xy + a.yy * b.yy) >> 10;
	result.yz = ((a.yx * b.xz + a.yy * b.yz) >> 10) + a.yz;
	return result;
}

// Mresult = s * M
Matrix mulScalarMatrix(int32_t scalar, Matrix m) {
	m.xx = (scalar * m.xx) >> 10;
	m.xy = (scalar * m.xy) >> 10;
	m.xz = (scalar * m.xz) >> 10;
	m.yx = (scalar * m.xx) >> 10;
	m.yy = (scalar * m.xy) >> 10;
	m.yz = (scalar * m.xz) >> 10;
	return m;
}

// presult = s * p
Point mulScalarPoint(int32_t scalar, Point p) {
	p.x = (scalar * p.x) >> 10;
	p.y = (scalar * p.y) >> 10;
	return p;
}

Matrix invertMatrix(Matrix m) {
	Matrix inverse;
	int32_t det = (m.xx*m.yy - m.xy*m.yx) >> 10;
	if (det != 0) {
		inverse.xx =  (m.yy << 10) / det;
		inverse.xy = -(m.xy << 10) / det;
		inverse.xz =  (m.xy*m.yz - m.xz*m.yy) / det;
		
		inverse.yx = -(m.yx << 10) / det;
		inverse.yy =  (m.xx << 10) / det;
		inverse.yz =  (m.xz*m.yx - m.xx*m.yz) / det;
	} else {
		// matrix not invertable: should not happen, fall back to null matrix
		inverse.xx = 0;
		inverse.xy = 0;
		inverse.xz = 0;
		inverse.yx = 0;
		inverse.yy = 0;
		inverse.yz = 0;
	}
	return inverse;
}


// construct a matrix: rotation about arbitrary pivot point
Matrix getMatrixRotate(int16_t angle) {
	Matrix result;
	angle = (360 + (angle % 360)) % 360;
	int16_t ca = surfaceCosine(angle); 
	int16_t sa = surfaceSine(angle); 
	result.xx = ca;
	result.xy = -sa;
	result.xz = 0;
	result.yx = sa;
	result.yy = ca;
	result.yz = 0;
	return result;
}

// construct a matrix: scale about origin
Matrix getMatrixScale(int16_t factorX, int16_t factorY) {
	Matrix result;
	result.xx = factorX;
	result.xy = 0;
	result.xz = 0;
	result.yx = 0;
	result.yy = factorY;
	result.yz = 0;
	return result;
}

// construct a matrix: translate by offset
Matrix getMatrixTranslate(int16_t x, int16_t y) {
	Matrix result;
	result.xx = 1024;
	result.xy = 0;
	result.xz = x << 10;
	result.yx = 0;
	result.yy = 1024;
	result.yz = y << 10;
	return result;
}

// construct a matrix: shear along x axis
Matrix getMatrixShearX(int16_t factor) {
	Matrix result;
	result.xx = 1024;
	result.xy = factor;
	result.xz = 0;
	result.yx = 0;
	result.yy = 1024;
	result.yz = 0;
	return result;
}

// construct a matrix: shear along y axis
Matrix getMatrixShearY(int16_t factor) {
	Matrix result;
	result.xx = 1024;
	result.xy = 0;
	result.xz = 0;
	result.yx = factor;
	result.yy = 1024;
	result.yz = 0;
	return result;
}

//------------------------------------------------------------------------------
// surface composition functions
//------------------------------------------------------------------------------

// paint sprite transformed by given matrix on surface, using given transparency value and blend mode
BoundingBox compose(Surface *surface, Surface *sprite, Surface *destination, Matrix matrix, uint8_t alpha, uint8_t mode, BoundingBox boundingBoxSprite, SurfaceMod *mask) {
	// 2020-01-09: move from "3 shears" to "general affine transformation", i.e. p' = A*p
	//             problem: interpolation
	//             anti-aliasing/interpolation via blendFractional() does not work
	// 2020-01-12: solution works, but shows non-interpolation pattern
	// 2020-01-13: try other way 'round: parameter "matrix" and "matrixInverse"
	//              - calculate surface bounding box with matrix
	//              - iteratore over surface coordinates of the bounding box
	//              - calculate p = A^-1 * p', i.e. get sprite coordinates and paint surface coordinates accordingly
	// 2020-01-20: introduced boundingBoxSprite parameter
	// 2020-02-03: changed to SurfaceMod update mask handling
	// 2020-02-05: removed boundingBox return value, using mask parameter instead
	// 2020-02-06: re-introduced boundingBox return value
	// 2020-02-11: new meaning of return value: bounding box of _unclipped_ sprite; removing explicit coordinate rounding
	
	// sanity check: bail out if invalid parameters were given
	if (surface == NULL || sprite == NULL || destination == NULL || mask == NULL || \
		surface->width != destination->width || surface->height != destination->height || 
		mask->height != surface->height)
		return boundingBoxCreate(0,0,0,0);
	
	Point pMin,pMax,pMod;
	BoundingBox bb;
	
	// new bounding box = bounding box sprite transformed by matrix
	if (boundingBoxSprite.min.x < 0) boundingBoxSprite.min.x = 0;
	if (boundingBoxSprite.min.y < 0) boundingBoxSprite.min.y = 0;
	if (boundingBoxSprite.max.x >= sprite->width) boundingBoxSprite.max.x = sprite->width-1;
	if (boundingBoxSprite.max.y >= sprite->height) boundingBoxSprite.max.y = sprite->height-1;
	
	// bounding box: min x, min y
	pMin.x = boundingBoxSprite.min.x << 10;
	pMin.y = boundingBoxSprite.min.y << 10;
	pMin = mulMatrixPoint(matrix,pMin);
	pMax = pMin;
	
	// bounding box: max x, min y
	pMod.x = boundingBoxSprite.max.x << 10;
	pMod.y = boundingBoxSprite.min.y << 10;
	pMod = mulMatrixPoint(matrix,pMod);
	if (pMod.x < pMin.x) pMin.x = pMod.x; else if (pMod.x > pMax.x) pMax.x = pMod.x;
	if (pMod.y < pMin.y) pMin.y = pMod.y; else if (pMod.y > pMax.y) pMax.y = pMod.y;
	
	// bounding box: min x, max y
	pMod.x = boundingBoxSprite.min.x << 10;
	pMod.y = boundingBoxSprite.max.y << 10;
	pMod = mulMatrixPoint(matrix,pMod);
	if (pMod.x < pMin.x) pMin.x = pMod.x; else if (pMod.x > pMax.x) pMax.x = pMod.x;
	if (pMod.y < pMin.y) pMin.y = pMod.y; else if (pMod.y > pMax.y) pMax.y = pMod.y;
	
	// bounding box: max x, max y
	pMod.x = boundingBoxSprite.max.x << 10;
	pMod.y = boundingBoxSprite.max.y << 10;
	pMod = mulMatrixPoint(matrix,pMod);
	if (pMod.x < pMin.x) pMin.x = pMod.x; else if (pMod.x > pMax.x) pMax.x = pMod.x;
	if (pMod.y < pMin.y) pMin.y = pMod.y; else if (pMod.y > pMax.y) pMax.y = pMod.y;
	
	// divide by 1024 and round if necessary
	// check that at least part of the bounding box overlaps with the surface
	bb.min.x = (pMin.x >> 10) + (((pMin.x & 1023) >= 512) ? 1 : 0);
	bb.max.x = (pMax.x >> 10) + (((pMax.x & 1023) >= 512) ? 1 : 0);
	bb.min.y = (pMin.y >> 10) + (((pMin.y & 1023) >= 512) ? 1 : 0);
	bb.max.y = (pMax.y >> 10) + (((pMax.y & 1023) >= 512) ? 1 : 0);
	if (bb.min.x >= surface->width || bb.max.x < 0 || bb.min.y >= surface->height || bb.max.y < 0) return bb;
	
	// clip bounding box to surface area
	uint8_t xMin = (bb.min.x < 0) ? 0 : bb.min.x;
	uint8_t yMin = (bb.min.y < 0) ? 0 : bb.min.y;
	uint8_t xMax = (bb.max.x > surface->width) ? surface->width - 1 : bb.max.x;
	uint8_t yMax = (bb.max.y > surface->height) ? surface->height - 1 : bb.max.y;
	
	// calculate inverse of transformation matrix
	Matrix inverse = invertMatrix(matrix);
	
	// iterate over boundingBox area of surface
	uint8_t  x,y;
	uint16_t iSurface = yMin * surface->width + xMin;
	uint16_t diSurface = surface->width - (xMax - xMin + 1);
	uint16_t iSprite;
	uint32_t bitmask;
	for (y = yMin; y <= yMax; y++) {
		bitmask = 0;
		for (x = xMin; x <= xMax; x++) {
			// calculate sprite coordinates:
			// 1) apply inverse matrix
			pMod.x = x << 10;
			pMod.y = y << 10;
			pMod = mulMatrixPoint(inverse,pMod);
			// 2) de-normalise and round
			pMod.x = (pMod.x >> 10) + (((pMod.x & 1023) >= 512) ? 1 : 0);
			pMod.y = (pMod.y >> 10) + (((pMod.y & 1023) >= 512) ? 1 : 0);
			// 3) check that the sprite coordinates are inside the sprite's bounding box
			if (pMod.x >= boundingBoxSprite.min.x && pMod.y >= boundingBoxSprite.min.y && \
				pMod.x <= boundingBoxSprite.max.x && pMod.y <= boundingBoxSprite.max.y) {
				// 4) calculate pixel index
				iSprite = pMod.y * sprite->width + pMod.x;
				// 5) blend pixel (destination = sprite op surface) and add pixel to mask if destination was changed
				if (surfacePixelBlend(
					sprite->rgb565[iSprite],(alpha * sprite->alpha[iSprite]) >> 8,
					surface->rgb565[iSurface],surface->alpha[iSurface],
					&destination->rgb565[iSurface],&destination->alpha[iSurface],
					mode)) bitmask |= 1 << (x >> 3); // pixel changed: mark in bitmask
			}
			// next pixel
			iSurface++;
		}
		// skip remainder of line and beginning of next line (xMax --> width, xMin)
		surfaceModSetRow(mask,y,bitmask);
		iSurface += diSurface;
	}
	
	return bb;
}
