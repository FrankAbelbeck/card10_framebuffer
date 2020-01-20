#ifndef _FASURFACEPP_H
#define _FASURFACEPP_H
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

#include <stdint.h> // uses: int8_t, uint8_t, int16_t, uint16_t, uint32_t
#include "faSurfaceBase.h" // uses BoundingBox

//------------------------------------------------------------------------------
// data structures
//------------------------------------------------------------------------------

/** Data structure of a 2D point, given as homogeneous coordinates;
 * Components are assumed to be pixel-based.
 */
typedef struct {
	int32_t x;
	int32_t y;
	int32_t z;
} PointPP;

/** Data structure of an affine transformation matrix (3x3).
 * Components are assumed to be normalised to 1024 (i.e. -0.5 would be -512).
 */
typedef struct {
	int32_t xx,xy,xz;
	int32_t yx,yy,yz;
	int32_t zx,zy,zz;
} MatrixPP;

//------------------------------------------------------------------------------
// function prototypes
//------------------------------------------------------------------------------

/** Perform a perspective divide, i.e. map homogeneous coordinates to the real plane.
 * 
 * This calculates p.x /= p.z, p.y /= p.z; it sets values to INT32_MAX/INT32_MIN if p.z==0.
 * 
 * @param p A PointPP.
 * @returns A PointPP.
 */
PointPP divPerspective(PointPP p, int32_t zExpected);

/** Multiply a MatrixPP with a PointPP, resulting in a modified PointPP.
 * 
 * @param m A MatrixPP.
 * @param p A PointPP.
 * @returns A PointPP, result of m*p.
 */
PointPP mulMatrixPointPP(MatrixPP m, PointPP p);

/** Multiply a MatrixPP with a MatrixPP, resulting in a modified MatrixPP.
 * 
 * @param m A MatrixPP.
 * @param p A MatrixPP.
 * @returns A MatrixPP, result of a*b.
 */
MatrixPP mulMatrixMatrixPP(MatrixPP a, MatrixPP b);

/** Multiply a MatrixPP with a scalar, resulting in a modified MatrixPP.
 * 
 * @param scalar An integer.
 * @param m A MatrixPP.
 * @returns A MatrixPP, result of scalar*m.
 */
MatrixPP mulScalarMatrixPP(int32_t scalar, MatrixPP m);

/** Multiply a PointPP with a scalar, resulting in a modified PointPP.
 * 
 * @param scalar An integer.
 * @param p A PointPP.
 * @returns A PointPP, result of scalar*p.
 */
PointPP mulScalarPointPP(int32_t scalar, PointPP p);

/** Invert a MatrixPP.
 * 
 * @param m A MatrixPP.
 * @param result A pointer to a MatrixPP.
 */
MatrixPP invertMatrixPP(MatrixPP m);

/** Calculate a MatrixPP for the following operation:
 * Rotation by angle about arbitrary point.
 * 
 * @param angle An angle in degrees.
 * @returns A transformation MatrixPP.
 */
MatrixPP getMatrixRotatePP(int16_t angle);

/** Calculate a MatrixPP for the following operation:
 * Scale about origin by given factors in both x and y direction.
 * 
 * @param factorX Factor for width scaling, normalised to 1024 (i.e. -0.5 would be -512).
 * @param factorY Factor for height scaling, normalised to 1024 (i.e. -0.5 would be -512).
 * @returns A transformation MatrixPP.
 */
MatrixPP getMatrixScalePP(int16_t factorX, int16_t factorY);

/** Calculate a MatrixPP for the following operation:
 * Translation by given offset.
 * 
 * @param x x component of offset.
 * @param y y component of offset.
 * @returns A transformation MatrixPP.
 */
MatrixPP getMatrixTranslatePP(int16_t x, int16_t y);

/** Calculate a MatrixPP for the following operation:
 * Shear in x direction.
 * 
 * @param factor Factor for x shearing, normalised to 1024 (i.e. -0.5 would be -512).
 * @returns A transformation MatrixPP.
 */
MatrixPP getMatrixShearXPP(int16_t factor);

/** Calculate a MatrixPP for the following operation:
 * Shear in y direction.
 * 
 * @param factor Factor for y shearing, normalised to 1024 (i.e. -0.5 would be -512).
 * @returns A transformation MatrixPP.
 */
MatrixPP getMatrixShearYPP(int16_t factor);

/** Calculate a MatrixPP for the following operation:
 * Perspective projection in x and/or y direction.
 * 
 * Note: factorX=0 factorY=0 factorZ=1024 = unit matrix
 *       factorX=0 factorY=0 factorZ=2048 = scale by 0.5 (1024/2048)
 *       factorX=0 factorY=0 factorZ=512  = scale by 2 (1024/512)
 * 
 * @param factorX Factor for x axis perspective projection, normalised to 1024 (i.e. -0.5 would be -512).
 * @param factorY Factor for y axis perspective projection, normalised to 1024 (i.e. -0.5 would be -512).
 * @param factorZ Factor for general coordinate mangling, normalised to 1024 (i.e. -0.5 would be -512).
 * @returns A transformation MatrixPP.
 */
MatrixPP getMatrixPerspective(int16_t factorX, int16_t factorY, int16_t factorZ);

/** Compose a surface (A, the sprite) with another surface (B) and write the result to a destination surface. 
 * 
 * Dimensions of surfaces B and C must match.
 * 
 * This function applies "C = A op B", with "op" depending of the given mode.
 * Details can be found in:
 *    Thomas Porter, Tom Duff: "Compositing Digital Images."
 *    SIGGRAPH '84 Proceedings of the 11th annual conference on Computer graphics and interactive techniques;
 *    ACM New York, NY, USA, 1984; pp. 253-259.
 *    https://doi.org/10.1145%2F800031.808606
 * 
 * @param surface Pointer to a Surface.
 * @param sprite Pointer to a Surface.
 * @param destination Pointer to a Surface.
 * @param matrix 3-by-3 Transformation MatrixPP.
 * @param alpha Transparency of the sprite during composition (multiplied with the sprite's own transparency).
 * @param mode A mode as defined by BLEND_*
 * @returns A BoundingBox, receiving information on the transformed sprite's bounding box. If an error occurs, this is set to (0,0,0,0).
 */
BoundingBox composePP(Surface *surface, Surface *sprite, Surface *destination, MatrixPP matrix, uint8_t alpha, uint8_t mode, BoundingBox boundingBoxSprite);

#endif // _FASURFACEPP_H
