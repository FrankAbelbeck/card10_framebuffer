#ifndef _FASURFACE_H
#define _FASURFACE_H
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
 * This variant does not support perspective projection.
 * This reduces load of homogeneous coordinate transformation significantly.
 */

#include <stdint.h> // uses: int8_t, uint8_t, int16_t, uint16_t, uint32_t
#include "faSurfaceBase.h"

//------------------------------------------------------------------------------
// data structures
//------------------------------------------------------------------------------

/** Data structure of an affine transformation matrix, with implicit definition of zx=zy=0 and zz=1.
 * Define FASURFACEFULL to activate explicit definition of zx, zy and zz (needed for perspective transformation).
 * Components are assumed to be normalised to 1024 (i.e. -0.5 would be -512).
 */
typedef struct {
	int32_t xx,xy,xz;
	int32_t yx,yy,yz;
} Matrix;

//------------------------------------------------------------------------------
// function prototypes
//------------------------------------------------------------------------------

/** Multiply a Matrix with a Point, resulting in a modified Point.
 * 
 * @param m A Matrix.
 * @param p A Point, values normalised to 1024 (i.e. -0.5 would be -512).
 * @returns A Point, result of m*p; normalised to 1024, too.
 */
Point mulMatrixPoint(Matrix m, Point p);

/** Multiply a Matrix with a Matrix, resulting in a modified Matrix.
 * 
 * @param m A Matrix.
 * @param p A Matrix.
 * @returns A Matrix, result of a*b.
 */
Matrix mulMatrixMatrix(Matrix a, Matrix b);

/** Multiply a Matrix with a scalar, resulting in a modified Matrix.
 * 
 * @param scalar An integer.
 * @param m A Matrix.
 * @returns A Matrix, result of scalar*m.
 */
Matrix mulScalarMatrix(int32_t scalar, Matrix m);

/** Multiply a Point with a scalar, resulting in a modified Point.
 * 
 * @param scalar An integer.
 * @param p A Point, values normalised to 1024 (i.e. -0.5 would be -512).
 * @returns A Point, result of scalar*p; normalised to 1024, too.
 */
Point mulScalarPoint(int32_t scalar, Point p);

/** Invert a Matrix.
 * 
 * @param m A Matrix.
 * @param result A pointer to a Matrix.
 */
Matrix invertMatrix(Matrix m);

/** Calculate a Matrix structure for the following operation:
 * Rotation by angle about arbitrary point.
 * 
 * @param angle An angle in degrees.
 * @returns A transformation Matrix.
 */
Matrix getMatrixRotate(int16_t angle);

/** Calculate a Matrix structure for the following operation:
 * Scale about origin by given factors in both x and y direction.
 * 
 * @param factorX Factor for width scaling, normalised to 1024 (i.e. -0.5 would be -512).
 * @param factorY Factor for height scaling, normalised to 1024 (i.e. -0.5 would be -512).
 * @returns A transformation Matrix.
 */
Matrix getMatrixScale(int16_t factorX, int16_t factorY);

/** Calculate a Matrix structure for the following operation:
 * Translation by given offset.
 * 
 * @param x x component of offset, pixel-based.
 * @param y y component of offset, pixel-based.
 * @returns A transformation Matrix.
 */
Matrix getMatrixTranslate(int16_t x, int16_t y);

/** Calculate a Matrix structure for the following operation:
 * Shear in x direction.
 * 
 * @param factor Factor for x shearing, normalised to 1024 (i.e. -0.5 would be -512).
 * @returns A transformation Matrix.
 */
Matrix getMatrixShearX(int16_t factor);

/** Calculate a Matrix structure for the following operation:
 * Shear in y direction.
 * 
 * @param factor Factor for y shearing, normalised to 1024 (i.e. -0.5 would be -512).
 * @returns A transformation Matrix.
 */
Matrix getMatrixShearY(int16_t factor);

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
 * @param matrix 3-by-3 Transformation Matrix.
 * @param alpha Transparency of the sprite during composition (multiplied with the sprite's own transparency).
 * @param mode A mode as defined by BLEND_*
 * @param boundingBoxSprite BoundingBox of the sprite are that should be displayed; use getBoundingBoxSurface(sprite) to display the entire sprite.
 * @returns A BoundingBox, receiving information on the transformed sprite's bounding box. If an error occurs, this is set to (0,0,0,0).
 */
BoundingBox compose(Surface *surface, Surface *sprite, Surface *destination, Matrix matrix, uint8_t alpha, uint8_t mode, BoundingBox boundingBoxSprite);

#endif // _FASURFACE_H
