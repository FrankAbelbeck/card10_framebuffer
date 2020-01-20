#ifndef _FASURFACEBASE_H
#define _FASURFACEBASE_H
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

#include <stdint.h> // uses: int8_t, uint8_t, int16_t, uint16_t, uint32_t

//------------------------------------------------------------------------------
// constants
//------------------------------------------------------------------------------

#define BLEND_UNKNOWN 0 ///< unknown blend mode (does nothing)
#define BLEND_OVER    1 ///< blend operation "over"
#define BLEND_IN      2 ///< blend operation "in"
#define BLEND_OUT     3 ///< blend operation "out"
#define BLEND_ATOP    4 ///< blend operation "atop"
#define BLEND_XOR     5 ///< blend operation "xor"
#define BLEND_PLUS    6 ///< blend operation "plus"

//------------------------------------------------------------------------------
// macro functions
//------------------------------------------------------------------------------

#define GETRED(x)       ( (uint8_t)( ( (x) >> 11 ) & 0x1f ) )
#define GETGREEN(x)     ( (uint8_t)( ( (x) >> 5 )  & 0x3f ) )
#define GETBLUE(x)      ( (uint8_t)(   (x)         & 0x1f ) )
#define MKRGB565(r,g,b) ( (uint16_t)( ((r) & 0x1f) << 11 | ((g) & 0x3f) << 5 | ((b) & 0x1f) ) )

//------------------------------------------------------------------------------
// data structures
//------------------------------------------------------------------------------

/** Data structure of a card10 display pixel. */
typedef struct {
	uint16_t rgb565; ///< RGB information (5 bits red, 6 bits green, 5 bits blue).
	uint8_t  alpha;  ///< Transparency information (0=transparent..255=opaque).
} RGBA5658;

/** Data structure of an image. */
typedef struct {
	uint16_t width;   ///< Width in pixels.
	uint16_t height;  ///< Height in pixels.
	uint16_t *rgb565; ///< Image data (address of a RGB565 pixel array).
	uint8_t  *alpha;  ///< Alpha values (address of a byte array; might be NULL).
} Surface;

/** Data structure of a 2D point.
 * Components are assumed to be pixel-based.
 */
typedef struct {
	int32_t x;
	int32_t y;
} Point;

/** Data structure of a bounding box, consisting of two points.
 */
typedef struct {
	Point min,max;
} BoundingBox;

//------------------------------------------------------------------------------
// function prototypes
//------------------------------------------------------------------------------

/** Constructor: create and initialise an Image data structure.
 * 
 * @returns An initialised Image data structure.
 */
Surface* constructSurface();

/** Destructor: free any allocated memory in an Image data structure.
 * 
 * @param self Pointer to a pointer to a Surface structure to be modified.
 */
void destructSurface(Surface **self);

/** Create a surface structure and allocate memory for given dimensions.
 * 
 * @param width Number of pixels in horizontal direction.
 * @param height Number of pixels in vertical direction.
 * @returns A pointer to a Surface structure or NULL if something went wrong.
 */
Surface *setupSurface(uint16_t width, uint16_t height);

/** Clear a surface by setting all pixels to a given colour and alpha value.
 * 
 * @param surface Pointer to a Surface structure to be modified.
 * @param colour A 16-bit colour value (RGB565).
 * @param alpha An 8-bit alpha value.
 */
void clearSurface(Surface *surface, uint16_t colour, uint8_t alpha);

/** Clone an existing surface by creating a new surface of same size and copying
 * all pixels.
 * 
 * @param surface Pointer to a Surface structure to be modified.
 * @returns A pointer to a Surface structure.
 */
Surface *cloneSurface(Surface *surface);

/** Copy part of one surface onto another.
 * 
 * @param source Pointer to a Surface structure.
 * @param destination Pointer to a Surface structure.
 * @param boundingBox A BoundingBox structure; area to copy.
 */
void copySurfaceAreaToSurface(Surface *source, Surface *destination, BoundingBox boundingBox);

/** Create a new BoundingBox with given coordinates.
 * 
 * Minimum point = upper left point when worn on the left, lower right otherwise.
 * Maximum point = lower right point when worn on the left, upper left otherwise.
 * 
 * @param xMin Coordinate component in x direction of minimum point.
 * @param yMin Coordinate component in y direction of minimum point.
 * @param xMax Coordinate component in x direction of maximum point.
 * @param yMax Coordinate component in y direction of maximum point.
 * @returns A BoundingBox structure.
 */
BoundingBox createBoundingBox(int32_t xMin, int32_t yMin, int32_t xMax, int32_t yMax);

/** Create a new BoundingBox that encloses given surface.
 * 
 * @param surface A pointer to a Surface.
 * @returns A BoundingBox structure.
 */
BoundingBox getBoundingBoxSurface(Surface *surface);


void drawPoint(Surface *surface, int16_t x, int16_t y, uint16_t colour, uint8_t alpha);
void drawLine(Surface *surface, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t colour, uint8_t alpha);
void drawCircle(Surface *surface, int16_t x, int16_t y, uint16_t radius, uint16_t colour, uint8_t alpha);
void drawTriangle(Surface *surface, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t colour, uint8_t alpha);
void drawRectangle(Surface *surface, int16_t x, int16_t y, int16_t width, int16_t height, uint16_t colour, uint8_t alpha); 

/** Calculate the tangent for x in range [-45,45].
 * 
 * @param x Integer value in degrees in range [-45,45].
 * @returns Integer result of tan(x) normalised to range [-1024,+1024]; -1024 if x < -45; +1024 if x > 45.
 */
int16_t faTan45(int16_t x) ;

/** Calculate the sine for x in range [0..360].
 * 
 * @param x Integer value as degrees in range [0..360].
 * @returns Integer result of sin(x) normalised to range [-1024,+1024]; 0 if x outside range.
 */
int16_t faSin(int16_t x) ;

/** Calculate the cosine for x in range [0..360].
 * 
 * @param x Integer value as degrees in range [0..360].
 * @returns Integer result of cos(x) normalised to range [-1024,+1024]; 0 if x outside range.
 */
int16_t faCos(int16_t x) ;

/** Image composition function for alpha blending a pixel with another pixel.
 * 
 * This function calculates "A op B", with "op" depending of the given mode.
 * Details can be found in:
 *    Thomas Porter, Tom Duff: "Compositing Digital Images."
 *    SIGGRAPH '84 Proceedings of the 11th annual conference on Computer graphics and interactive techniques;
 *    ACM New York, NY, USA, 1984; pp. 253-259. 
 *    https://doi.org/10.1145%2F800031.808606
 * 
 * @param colourA colour component of pixel A
 * @param alphaA alpha value of pixel A
 * @param colourB colour component of pixel B
 * @param alphaB alpha value of pixel B
 * @param colourResult resulting colour component
 * @param alphaResult resulting alpha value
 * @param mode a mode as defined by BLEND_*
 */
void blend(uint16_t colourA, uint8_t alphaA, uint16_t colourB, uint8_t alphaB, uint16_t *colourResult, uint8_t *alphaResult, uint8_t mode);


void printInt(int32_t value);

#endif // _FASURFACEBASE_H
