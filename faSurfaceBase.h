#ifndef _FASURFACEBASE_H
#define _FASURFACEBASE_H
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

#include <stdint.h> // uses: int8_t, uint8_t, int16_t, uint16_t, uint32_t
#include <stdbool.h> // uses: true, false, bool;

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

#define MASK_MEMORY_STEPUP   32 ///< number of cells to add to the mask arrays if enlargement is necessary

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
	uint8_t width;   ///< Width in pixels.
	uint8_t height;  ///< Height in pixels.
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

/** Data structure of an update mask, tile-based (8x8 pixels large).
 * 
 * Each tile represents the state of an 8x8 pixels large area of a surface.
 * If any of the included pixels was modified, the entire area is regarded as modified.
 * 
 * Since the maximum size of an image can be 255x255 pixels, there are at most 32x32 tiles. 
 * 
 * The tiles are encoded as bit masks with length 32. There are n bit masks with n = surface height / 8.
 */
typedef struct {
	uint8_t  height;///< Overall number of lines; as image height is limited to 255 and each tile is 8 pixels high, this is a number in range 0..31.
	uint32_t  *tile; ///< Array of tiles; bitmasks, each covering one eight of the surface height and the entire surface width.
} SurfaceMod;


//------------------------------------------------------------------------------
// function prototypes
//------------------------------------------------------------------------------

/** Constructor: create and initialise a Surface structure.
 * 
 * @returns An initialised Surface structure.
 */
Surface* surfaceConstruct();

/** Destructor: free any allocated memory in a Surface structure.
 * 
 * This calls free on all pointers and on the Surface structure itself.
 * 
 * @param self Pointer to a pointer to a Surface structure.
 */
void surfaceDestruct(Surface **self);

/** Create a surface structure and allocate memory for given dimensions.
 * 
 * @param width Number of pixels in horizontal direction.
 * @param height Number of pixels in vertical direction.
 * @returns A pointer to a Surface structure or NULL if something went wrong.
 */
Surface *surfaceSetup(uint8_t width, uint8_t height);

/** Clear a surface by setting all pixels to a given colour and alpha value.
 * 
 * @param surface Pointer to a Surface structure to be modified.
 * @param colour A 16-bit colour value (RGB565).
 * @param alpha An 8-bit alpha value.
 */
void surfaceClear(Surface *surface, uint16_t colour, uint8_t alpha);

/** Clone an existing surface by creating a new surface of same size and copying
 * all pixels.
 * 
 * @param surface Pointer to a Surface structure to be modified.
 * @returns A pointer to a Surface structure.
 */
Surface *surfaceClone(Surface *surface);

/** Copy part of one surface onto another.
 * 
 * @param source Pointer to a Surface structure.
 * @param destination Pointer to a Surface structure.
 * @param boundingBox A BoundingBox structure; area to copy.
 */
void surfaceCopy(Surface *source, Surface *destination, SurfaceMod *mask);

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
BoundingBox boundingBoxCreate(int32_t xMin, int32_t yMin, int32_t xMax, int32_t yMax);

/** Create a new BoundingBox that encloses given surface.
 * 
 * @param surface A pointer to a Surface.
 * @returns A BoundingBox structure.
 */
BoundingBox boundingBoxGet(Surface *surface);

/** Constructor: create a pointer to a SurfaceMod structure.
 * 
 * @param size Initial size of the mask (number of entries).
 * @param allowResize Boolean value, specifying if structure may be resized later.
 * @returns Pointer to a SurfaceMod structure.
 */
SurfaceMod *surfaceModConstruct(uint8_t height);

/** Destructor: free any allocated memory in a SurfaceMod structure.
 * 
 * @param mask Pointer to a pointer to a SurfaceMod structure.
 */
void surfaceModDestruct(SurfaceMod **mask);

/** Clear a SurfaceMod structure.
 * 
 * @param mask Pointer to a SurfaceMod structure.
 */
void surfaceModClear(SurfaceMod *mask);

/** Set a sequence of updated pixel in the given SurfaceMod structure.
 * 
 * @param mask Pointer to a SurfaceMod structure.
 * @param x Number of the coloumn of the starting point.
 * @param y Number of the row of the starting point.
 * @param len Number of updated pixels, including the starting point.
 */
void surfaceModSetSeq(SurfaceMod *mask, uint8_t x, uint8_t y, uint8_t len);

/** Set an entire row bitmask in the given SurfaceMod structure.
 * 
 * @param mask Pointer to a SurfaceMod structure.
 * @param y Number of the row.
 * @param bitmask A 32 bit bitmask.
 */
void surfaceModSetRow(SurfaceMod *mask, uint8_t y, uint32_t bitmask);

/** Set a pixel in the given SurfaceMod structure.
 * 
 * @param mask Pointer to a SurfaceMod structure.
 * @param x Number of the coloumn of the pixel.
 * @param y Number of the row of the pixel.
 */
void surfaceModSetPixel(SurfaceMod *mask, uint8_t x, uint8_t y);


/** Create a Point data structure.
 * 
 * @param x Component of the point's coordinates on x axis in pixels.
 * @param y Component of the point's coordinates on y axis in pixels.
 * @returns A Point structure.
 */
Point createPoint(int32_t x, int32_t y);


/** Draw a point onto the given surface, compositing pixel values with the given blend mode. 
 * 
 * @param surface Pointer to a Surface structure to be modified.
 * @param p A Point structure, starting point.
 * @param colour Colour of the contour, RGB565 format.
 * @param alpha Transparency value of the contour, in range 0..255.
 * @param mode A mode as defined by BLEND_*
 * @param mask Pointer to a SurfaceMod structure where changes to the surface are recorded.
 * @returns A BoundingBox structure describing the smalles box enclosing this point.
 */
BoundingBox surfaceDrawPoint(Surface *surface, Point p, uint16_t colour, uint8_t alpha, uint8_t mode, SurfaceMod *mask);

/** Draw a line onto the given surface, compositing pixel values with the given blend mode. 
 * 
 * @param surface Pointer to a Surface structure to be modified.
 * @param p0 A Point structure, starting point.
 * @param p1 A Point structure, end point.
 * @param colour Colour of the line, RGB565 format.
 * @param alpha Transparency value of the line, in range 0..255.
 * @param mode A mode as defined by BLEND_*
 * @param mask Pointer to a SurfaceMod structure where changes to the surface are recorded.
 * @returns A BoundingBox structure describing the smalles box enclosing this line.
 */
BoundingBox surfaceDrawLine(Surface *surface, Point p0, Point p1, uint16_t colour, uint8_t alpha, uint8_t mode, SurfaceMod *mask);

/** Draw a circle onto the given surface, compositing pixel values with the given blend mode. 
 * 
 * @param surface Pointer to a Surface structure to be modified.
 * @param pm A Point structure describing the centre of the circle.
 * @param radius Radius of the circle in pixels.
 * @param colour Colour of the circle, RGB565 format.
 * @param alpha Transparency value of the circle, in range 0..255.
 * @param mode A mode as defined by BLEND_*
 * @param mask Pointer to a SurfaceMod structure where changes to the surface are recorded.
 * @returns A BoundingBox structure describing the smalles box enclosing this circle.
 */
BoundingBox surfaceDrawCircle(Surface *surface, Point pm, uint16_t radius, uint16_t colour, uint8_t alpha, uint8_t mode, SurfaceMod *mask);

/** Draw an arc onto the given surface, compositing pixel values with the given blend mode. 
 * 
 * @param surface Pointer to a Surface structure to be modified.
 * @param pm A Point structure describing the centre of the arc.
 * @param radius Radius of the arc in pixels.
 * @param angleStart Start angle of the arc in degrees.
 * @param angleStop Stop angle of the arc in degrees.
 * @param colour Colour of the arc, RGB565 format.
 * @param alpha Transparency value of the arc, in range 0..255.
 * @param mode A mode as defined by BLEND_*
 * @param mask Pointer to a SurfaceMod structure where changes to the surface are recorded.
 * @returns A BoundingBox structure describing the smalles box enclosing this arc.
 */
BoundingBox surfaceDrawArc(Surface *surface, Point pm, uint16_t radius, int16_t angleStart, int16_t angleStop, uint16_t colour, uint8_t alpha, uint8_t mode, SurfaceMod *mask);

/** Draw a sector onto the given surface, compositing pixel values with the given blend mode. 
 * 
 * @param surface Pointer to a Surface structure to be modified.
 * @param pm A Point structure describing the centre of the arc.
 * @param radiusOuter Outer radius of the sector in pixels.
 * @param radiusInner Inner radius of the sector in pixels.
 * @param angleStart Start angle of the arc in degrees.
 * @param angleStop Stop angle of the arc in degrees.
 * @param colour Colour of the sector, RGB565 format.
 * @param alpha Transparency value of the sector, in range 0..255.
 * @param mode A mode as defined by BLEND_*
 * @param mask Pointer to a SurfaceMod structure where changes to the surface are recorded.
 * @returns A BoundingBox structure describing the smalles box enclosing this sector.
 */
BoundingBox surfaceDrawSector(Surface *surface, Point pm, uint16_t radiusOuter, uint16_t radiusInner, int16_t angleStart, int16_t angleStop, uint16_t colour, uint8_t alpha, uint8_t mode, SurfaceMod *mask);

/** Draw a sector onto the given surface, compositing pixel values with the given blend mode. 
 * 
 * @param surface Pointer to a Surface structure to be modified.
 * @param p0 First point of the triangle.
 * @param p1 Second point of the triangle.
 * @param p2 Third point of the triangle.
 * @param colour Colour of the triangle, RGB565 format.
 * @param alpha Transparency value of the triangle, in range 0..255.
 * @param mode A mode as defined by BLEND_*
 * @param mask Pointer to a SurfaceMod structure where changes to the surface are recorded.
 * @returns A BoundingBox structure describing the smalles box enclosing this triangle.
 */
BoundingBox surfaceDrawTriangle(Surface *surface, Point p0, Point p1, Point p2, uint16_t colour, uint8_t alpha, uint8_t mode, SurfaceMod *mask);

/** Draw a rectangle onto the given surface, compositing pixel values with the given blend mode. 
 * 
 * @param surface Pointer to a Surface structure to be modified.
 * @param p0 First point of the rectangle.
 * @param p1 Second point of the rectangle.
 * @param colour Colour of the rectangle, RGB565 format.
 * @param alpha Transparency value of the rectangle, in range 0..255.
 * @param mode A mode as defined by BLEND_*
 * @param mask Pointer to a SurfaceMod structure where changes to the surface are recorded.
 * @returns A BoundingBox structure describing the smalles box enclosing this rectangle.
 */
BoundingBox surfaceDrawRectangle(Surface *surface, Point p0, Point p1, uint16_t colour, uint8_t alpha, uint8_t mode, SurfaceMod *mask); 


/** Calculate the tangent for x in range [-45,45].
 * 
 * @param x Integer value in degrees in range [-45,45].
 * @returns Integer result of tan(x) normalised to range [-1024,+1024]; -1024 if x < -45; +1024 if x > 45.
 */
int16_t surfaceTangent45(int16_t x) ;

/** Calculate the sine for x in range [0..360].
 * 
 * @param x Integer value as degrees in range [0..360].
 * @returns Integer result of sin(x) normalised to range [-1024,+1024]; 0 if x outside range.
 */
int16_t surfaceSine(int16_t x) ;

/** Calculate the cosine for x in range [0..360].
 * 
 * @param x Integer value as degrees in range [0..360].
 * @returns Integer result of cos(x) normalised to range [-1024,+1024]; 0 if x outside range.
 */
int16_t surfaceCosine(int16_t x) ;

/** Image composition function for alpha blending a pixel with another pixel.
 * 
 * This function calculates "A op B", with "op" depending of the given mode.
 * Details can be found in:
 *    Thomas Porter, Tom Duff: "Compositing Digital Images."
 *    SIGGRAPH '84 Proceedings of the 11th annual conference on Computer graphics and interactive techniques;
 *    ACM New York, NY, USA, 1984; pp. 253-259. 
 *    https://doi.org/10.1145%2F800031.808606
 * 
 * @param colourA Colour component of pixel A.
 * @param alphaA Alpha value of pixel A.
 * @param colourB Colour component of pixel B.
 * @param alphaB Alpha value of pixel B.
 * @param colourResult Resulting colour component.
 * @param alphaResult Resulting alpha value.
 * @param mode A mode as defined by BLEND_*.
 * @returns Boolean value, true if result differs from B.
 */
bool surfaceBlend(uint16_t colourA, uint8_t alphaA, uint16_t colourB, uint8_t alphaB, uint16_t *colourResult, uint8_t *alphaResult, uint8_t mode);


void printInt(int32_t value);

#endif // _FASURFACEBASE_H
