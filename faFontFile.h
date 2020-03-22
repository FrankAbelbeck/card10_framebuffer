#ifndef _FAFONTFILE_H
#define _FAFONTFILE_H
/**
 * @file
 * @author Frank Abelbeck <frank.abelbeck@googlemail.com>
 * @version 2020-03-05
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
 * Frank Abelbeck Font File (faFF) management library
 */

#include <stdlib.h> // uses: malloc(), free()
#include <stdint.h> // uses: int8_t, uint8_t, int16_t, uint16_t, uint32_t

#include "epicardium.h" // used for file access
#include "faSurfaceBase.h"

//------------------------------------------------------------------------------
// constants
//------------------------------------------------------------------------------
#define RET_FAFF_OK        0 ///< function returned successfully
#define RET_FAFF_OPEN     -1 ///< opening a faFontFile failed
#define RET_FAFF_READ     -2 ///< reading from a faFontFile failed
#define RET_FAFF_MAGIC    -3 ///< faFF signature not detected
#define RET_FAFF_GTAB     -4 ///< creating G table failed
#define RET_FAFF_VTAB     -5 ///< creating V table failed
#define RET_FAFF_REPLCHAR -6 ///< replacement character not found
#define RET_FAFF_ARGS     -7 ///< invalid arguments passed
#define RET_FAFF_BUFFER   -8 ///< failed to set-up bitmap buffer

#define FNV1PRIME  0x01000193 ///< Fowler-Noll-Vo 1 32 bit hash prime
#define FNV1OFFSET 0x811c9dc5 ///< Fowler-Noll-Vo 1 32 bit hash offset
#define FNV1MASK   0x7fffffff ///< hash mask, maximum value of int32_t
#define MAXUCODE   0x10ffff   ///< maximum Unicode code number

#define FAFF_FMT_NONE     0b0000000000000000 ///< format string: invalid
#define FAFF_FMT_FLAGS    0b0000000000000001 ///< format string: flags mode
#define FAFF_FMT_PLUS     0b0000000000000010 ///< format string: plus flag
#define FAFF_FMT_MINUS    0b0000000000000100 ///< format string: minus flag
#define FAFF_FMT_SPACE    0b0000000000001000 ///< format string: space flag
#define FAFF_FMT_PAD0     0b0000000000010000 ///< format string: pad zero flag
#define FAFF_FMT_WIDTH    0b0000000000100000 ///< format string: width mode
#define FAFF_FMT_DECIMAL  0b0000000001000000 ///< format string: decimal type (%i)
#define FAFF_FMT_HEXUPPER 0b0000000010000000 ///< format string: hexadecimal type, lower case (%x)
#define FAFF_FMT_HEXLOWER 0b0000000100000000 ///< format string: hexadecimal type, upper case (%X)
#define FAFF_FMT_OCTAL    0b0000001000000000 ///< format string: octal type (%o)
#define FAFF_FMT_STRING   0b0000010000000000 ///< format string: string type (%s)
#define FAFF_FMT_LITERAL  0b0000100000000000 ///< format string: literal percentage sign

//------------------------------------------------------------------------------
// data structures
//------------------------------------------------------------------------------

// size considerations:
//  * width and height in range 1..256
//  * at most 0x10fff characters
//  * size of an value table entry at size 256x256:
//      + 3 bytes character code
//      + 256 bitmask words, 32 bytes per word
//      = 8195 bytes per entry

/** Data structure of a faFontFile */
typedef struct {
	uint8_t   width,height; ///< width and height of a character symbol 
	uint32_t  nChars; ///< number of characters in the set
	int32_t   *G; ///< intermediate table for minimal perfect hash lookup
	uint8_t   *V; ///< value table with codes+symbol data
	uint8_t   sizeVWord; ///< size of one V entry word; equals ((height-1)/8+1
	uint16_t  sizeVEntry; ///< size of a V entry; equals width*((height-1)/8+1) + 3
	int32_t   iReplChar; ///< V entry index of the replacement character (U+FFFD); please note: this is not the absolute byte position in V!
	uint8_t   distChar; ///< horizontal distance between individual characters in pixels
	uint8_t   distLine; ///< vertical distance between lines in pixels
	uint8_t   tabWidth; ///< number of space characters used per tab character
	uint16_t  colour; ///< character colour
	uint8_t   alpha; ///< character alpha value
	uint16_t  colourBg; ///< character background colour
	uint8_t   alphaBg; ///< character background alpha value
	uint8_t   mode; ///< blend mode
} FontFileData;

//------------------------------------------------------------------------------
// function prototypes
//------------------------------------------------------------------------------

/** Constructor: create and initialise a fontFileData structure.
 * 
 * Default values unaffected by font file reading:
 *  - distChar = 0
 *  - distLine = 0
 *  - tabWidth = 4
 *  - colour = 0xffff (white)
 *  - alpha = 0xff (opaque)
 *  - colourBg = 0x0000 (black)
 *  - alphaBg = 0x00 (transparent)
 *  - mode = BLEND_OVER
 * 
 * For font layout customisation, you have to edit these values.
 * 
 * @returns An initialised fontFileData structure.
 */
FontFileData *fontFileConstruct();

/** Destructor: free any allocated memory in a fontFileData structure.
 * 
 * This calls free on all pointers and on the fontFileData structure itself.
 * 
 * @param self Pointer to a pointer to a fontFileData structure.
 */
void fontFileDestruct(FontFileData **self);

/** Look up a given Unicode character code in the given FontFileData structure.
 * Return the value table index of the matching symbol definition. Please note:
 * This index is entry-oriented, not byte-oriented (cf. self->sizeVEntry)!
 * If code is not defined, the replacement character's entry index is returned.
 * 
 * @param self Pointer to a fontFileData structure.
 * @param code Pointer to a byte sequence with a Unicode character code (big-endian).
 * @returns a positive value table index.
 */
int32_t fontFileLookUpIndex(FontFileData *self, uint8_t *code);

/** Read given file and populate the fiven FontFileData structure.
 * 
 * @param self Pointer to a fontFileData structure.
 * @param filename Address of a filename string (char array).
 * @returns A signed byte (int8_t) with one of the following return codes:
 *     - RET_FAFF_OK: file successfully parsed.
 *     - RET_FAFF_OPEN: opening the file failed; structure invalid.
 *     - RET_FAFF_READ: reading the file failed; structure invalid.
 *     - RET_FAFF_MAGIC: faFF signature not detected; structure invalid.
 *     - RET_FAFF_GTAB: creating G table failed; structure invalid.
 *     - RET_FAFF_VTAB: creating V table failed; structure invalid.
 *     - RET_FAFF_REPLCHAR: replacement character not found; structure invalid.
 *     - RET_FAFF_ARGS: invalid arguments passed; structure invalid.
 */
int8_t fontFileRead(FontFileData *self, char *filename);

/** Font file reading wrapper function. Load the faFontFile with given filename.
 * 
 * Manages FontFileData automatically.
 * 
 * @param filename Address of a filename string (char array).
 * @returns A pointer to a FontFileData structure. Might be NULL if something went wrong.
 */
FontFileData *fontFileLoad(char *filename);


/** Render a character string on a surface using a given bitmap font.
 * 
 * The string is parsed assuming UTF-8 encoding. In addition, very basic printf
 * format placeholders are recognised. Each format placeholder has the
 * following form:
 * 
 *    %[flags][width]type
 * 
 * Flags can be zero or more of the following characters:
 * 
 *  - '-': Left-align the formatted output (default: right-align).
 *  - '+': Prepend a plus sign to positive numbers (default: minus sign only).
 *  - ' ': Prepend a space to positive numbers (default: minus sign only).
 *  - '0': If a width is specified: prepend zeros to fill up any unused space.
 * 
 * Width is a positive integer number denoting the minimum number of characters
 * that should be put out. Width is clipped to range (0,255).
 * 
 * Type can be one of the following characters:
 * 
 *  - 'i': Take an integer argument from the list of optional arguments, convert
 *         it to a decimal digit string, and replace this %i with the result.
 *  - 'x': Take an integer argument from the list of optional arguments, convert
 *         it to a heaxdecimal digit string (lower case letters), and replace
 *         this %x with the result.
 *  - 'X': Take an integer argument from the list of optional arguments, convert
 *         it to a heaxdecimal digit string (upper case letters), and replace
 *         this %X with the result.
 *  - 'o': Take an integer argument from the list of optional arguments, convert
 *         it to a octal digit string, and replace this %o with the result.
 *  - 's': Take a null-terminated character string argument from the list of
 *         optional arguments and replace this %s with this string.
 *  - '%': Print a single '%' character.
 * 
 * @param surface Pointer to a Surface structure.
 * @param mask Pointer to a SurfaceMod structure where changes to the surface are recorded.
 * @param font Pointer to a FontFileData structure.
 * @param p A Point structure; upper left starting point on the surface.
 * @param text Pointer to a char array; text to print. A variable number of
 *             arguments might follow, which are inserted into text following a
 *             very basic printf style.
 * @returns A BoundingBox structure describing the smalles box enclosing the rendered text.
 */
BoundingBox fontFilePrint(Surface *surface, SurfaceMod *mask, FontFileData *font, Point p, char *text, ...) ;

#endif // _FAFONTFILE_H
