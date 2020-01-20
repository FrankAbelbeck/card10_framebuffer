#ifndef _FAREADPNG_H
#define _FAREADPNG_H
/**
 * @file
 * @author Frank Abelbeck <frank.abelbeck@googlemail.com>
 * @version 2019-12-14
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
 * Stream-oriented PNG reading library tailored to the card10 badge hardware.
 */

#include <stdint.h> // uses: int8_t, uint8_t, int16_t, uint16_t, uint32_t
#include <stdbool.h> // uses: bool, true, false
#include "faSurface.h"

//------------------------------------------------------------------------------
// constants: error return codes
//------------------------------------------------------------------------------

#define RET_OK                      0 ///< function returned successfully

#define RET_MALLOC_IMAGE           -1 ///< allocation of image data memory failed
#define RET_MALLOC_ALPHA           -2 ///< allocation of alpha channel memory failed
#define RET_MALLOC_PALETTE         -3 ///< allocation of palette memory failed
#define RET_MALLOC_SCANLINE        -4 ///< allocation of scanline buffer memory (previous row) failed
#define RET_MALLOC_CODE            -5 ///< allocation of huffman codes memory failed
#define RET_MALLOC_BUFFER_INFLATE  -6 ///< allocation of Huffman buffer failed

#define RET_OPEN                   -7 ///< opening file failed
#define RET_READ                   -8 ///< reading the file failed
#define RET_SEEK                   -9 ///< seeking in file failed (errno tells why)

#define RET_MAGIC                 -10 ///< magic byte check failed
#define RET_HEADER                -11 ///< header not first chunk or header invalid
#define RET_DIMENSIONS            -12 ///< either width or height zero
#define RET_BIT_DEPTH             -13 ///< invalid bit depth
#define RET_COLOUR_TYPE           -14 ///< invalid colour type
#define RET_COMPRESSION_METHOD    -15 ///< invalid compression method (not 0)
#define RET_FILTER_METHOD         -16 ///< invalid filter method (not 0)
#define RET_FILTER_TYPE           -17 ///< invalid filter type byte
#define RET_INTERLACE_METHOD      -18 ///< invalid interlace method (neither 0 nor 1)
#define RET_PALETTE               -19 ///< invalid palette chunk

#define RET_DEFLATE_COMPRESSION   -20 ///< invalid DEFLATE compression type
#define RET_PRESET_DICT           -21 ///< zlib sports preset dict (not valid in PNG)
#define RET_ZLIB_COMPRESSION      -22 ///< invalid zlib compression type
#define RET_ZLIB_WINSIZE          -23 ///< invalid zlib buffer window size
#define RET_NOCOMP_LEN            -24 ///< length value mismatch in uncompressed DEFLATE block
#define RET_INVALID_CODE_LEN_CODE -25 ///< invalid code length code 16 (at beginning, no code to copy)
#define RET_INVALID_LENGTH_CODE   -26 ///< invalid length code (not in range 0..285)
#define RET_INVALID_DISTANCE_CODE -27 ///< invalid distance code (not in range 0..29)
#define RET_LENGTHS_OVERFLOW      -28 ///< too many lengths while decoding dynamic Huffman alphabet
#define RET_CODE_NOT_FOUND        -29 ///< no code matches bit pattern at current file position

//------------------------------------------------------------------------------
// various constants
//------------------------------------------------------------------------------

#define CHUNK_UNKNOWN 0 ///< unknown PNG chunk type
#define CHUNK_IHDR    1 ///< PNG header chunk type
#define CHUNK_PLTE    2 ///< PNG palette chunk type
#define CHUNK_IDAT    3 ///< PNG image data chunk type
#define CHUNK_IEND    4 ///< PNG end chunk type

#define COLOURTYPE__GREY    0 ///< PNG colourtype greyscale
#define COLOURTYPE__RGB     2 ///< PNG colourtype RGB
#define COLOURTYPE__INDEXED 3 ///< PNG colourtype indexed (palette-based)
#define COLOURTYPE__GREY_A  4 ///< PNG colourtype greyscale + alpha
#define COLOURTYPE__RGB_A   6 ///< PNG colourtype RGB + alpha

#define FILTER_NONE  0 ///< PNG filter type none
#define FILTER_SUB   1 ///< PNG filter type sub
#define FILTER_UP    2 ///< PNG filter type up
#define FILTER_AVG   3 ///< PNG filter type average
#define FILTER_PAETH 4 ///< PNG filter type paeth filter

#define STATE_BEGIN                    0 ///< begin zlib stream
#define STATE_END                      1 ///< end zlib stream
#define STATE_DEFL_BEGIN               2 ///< begin DEFLATE block
#define STATE_DEFL_END                 3 ///< end DEFLATE block
#define STATE_DEFL_NO_COMPRESSION      4 ///< begin uncompressed DEFLATE block
#define STATE_DEFL_NO_COMPRESSION_READ 5 ///< read uncompressed DEFLATE block
#define STATE_DEFL_STAT_HUFFMAN        6 ///< build static Huffman alphabet
#define STATE_DEFL_DYN_HUFFMAN         7 ///< build dynamic Huffman alphabet
#define STATE_DEFL_HUFFMAN_DECODE      8 ///< decode Huffman-coded DEFLATE block
#define STATE_EXIT                     9 ///< exit zlib decoding

#define DEFL_NO_COMPRESSION 0 ///< DEFLATE compression: uncompressed
#define DEFL_STAT_HUFFMAN   1 ///< DEFLATE compression: static Huffman alphabet
#define DEFL_DYN_HUFFMAN    2 ///< DEFLATE compression: dynamic Huffman alphabet

//------------------------------------------------------------------------------
// makro functions
//------------------------------------------------------------------------------

/** Convert integer array x to a 32 bit integer assuming big endianness.
 * 
 * @param x An array of at least four bytes (uint8_t); behaviour undefined for
 * smaller arrays; fifth byte and following will be ignored.
 * @returns An integer (uint32_t).
 */
#define BIGENDIAN32(x)            ( (x)[0] << 24 | (x)[1] << 16 | (x)[2] << 8 | (x)[3] )

/** Convert integer array x to a 16 bit integer assuming big endianness.
 * 
 * @param x An array of at least two bytes (uint8_t); behaviour undefined for
 * smaller arrays; third byte and following will be ignored.
 * @returns An integer (uint16_t).
 */
#define BIGENDIAN16(x)            ( (x)[0] << 8  | (x)[1] )

/** Convert integer array x to an integer assuming little endianness.
 * 
 * @param x An array of at least two bytes (uint8_t); behaviour undefined for
 * smaller arrays; third byte and following will be ignored.
 * @returns An integer (uint16_t).
 */
#define LITTLEENDIAN16(x)         ( (x)[0] + ((x)[1] << 8) )

/** Convert colour values r, g, and b to a 16 bit RGB565 representation.
 * 
 * @param r Integral value for red.
 * @param g Integral value for green.
 * @param b Integral value for blue.
 * @returns A uint16_t integer.
 */
#define RGB565(r,g,b)             ( (uint16_t) ( ((((r) >> 3) & 0xff) << 11) | ((((g) >>2) & 0xff) << 5) | (((b) >>3) & 0xff) ) )

/** Calculate the scanline buffer size in bytes.
 * 
 * @param w Scanline width in pixels.
 * @param spp Number of samples (colour components) per pixel.
 * @param bd Number of bits per colour component (bit depth)
 * @returns An integer, most likely in 16 bit range.
 */
#define SCANLINEBYTES(w,spp,bd)   ( BITS2BYTES((w) * (spp) * (bd)) + 1 )

/** Calculate the amount of bytes needed to hold the given number of bits.
 * 
 * @param x Number of bits.
 * @returns An integer.
 */
#define BITS2BYTES(x)             ( ((x) >> 3) + (((x) & 7) ? 1 : 0) )

/** Calculate the absolute value.
 * 
 * @param x A number value.
 * @returns -x if x is negative; x otherwise.
 */
#define ABS(x)                    ( ((x) < 0) ? -(x) : (x) )

/** Pack information into a code alphabet entry.
 *
 * Internal code alphabet entry decoding: each uint32_t code entry consists of...
 * 
 *     31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
 *     `---------´ `---------´ `-----------------------------------------´ `---------------´
 *        unused     length         code bits in little endian order          symbol value
 *       (4 bits)   (4 bits)                   (15 bits)                        (9 bits)
 * 
 * @param len Number of code bits (clipped to 4 bits).
 * @param bits Code bits as little endian value (clipped to 15 bits).
 * @param symbol Corresponding symbol value (clipped to 9 bits).
 * @returns A uint32_t integer.
 */
#define PACKCODE(len,bits,symbol) ( (uint32_t) ( (((len) & 0x0f) << 24) | (((bits) & 0x7fff) << 9) | ((symbol) & 0x1ff) ) )

/** Retrieve length information from a code alphabet entry.
 * 
 * @param x A code entry integer value (uint32_t).
 * @returns An integer in range 0..15.
 */
#define EXTRACTLENGTH(x)          ( (uint8_t)  (((x) & 0x0f000000) >> 24) )

/** Retrieve code bits information from a code alphabet entry.
 * 
 * @param x A code entry integer value (uint32_t).
 * @returns An integer in range 0..32767.
 */
#define EXTRACTBITS(x)            ( (uint16_t) (((x) & 0x00fffe00) >> 9)  )

/** Retrieve symbol information from a code alphabet entry.
 * 
 * @param x A code entry integer value (uint32_t).
 * @returns An integer in range 0..511.
 */
#define EXTRACTSYMBOL(x)          ( (uint16_t) ( (x) & 0x000001ff)        )

//------------------------------------------------------------------------------
// PNG reader data structures ("self", image and pixel data)
//------------------------------------------------------------------------------

/** Data structure of processing state variables. */
typedef struct PngData {
	// palette data
	uint8_t   sizePalette; ///< Number of palette entries minus 1.
	uint16_t  *palette; ///< Palette data (address of a RGB565 colour array). 
	// scanline data: size and bytes, double-buffered (previous and current, needed for de-filtering)
	uint8_t   *scanlineCurrent; ///< Current scanline data (address of a byte array).
	uint8_t   *scanlinePrevious; ///< Previous scanline data (address of a byte array).
	uint8_t   samplesPerPixel; ///< Number of samples per pixel.
	RGBA5658  (*funPixConv)(struct PngData*,uint16_t); ///< Address of a pixel conversion function.
	// chunk and file management
// 	FILE      *file; ///< Address of a file stream.
	int       file; ///< Address of a file stream.
	uint32_t  lenChunk; ///< Length of current chunk
	uint8_t   typeChunk; ///< Type of current chunk
	// zlib management
	uint8_t   state; ///< Number of current zlib/DEFLATE decoding state
	bool      isLastBlock; ///< Boolean indicating if the currently processed block is the last.
	uint32_t  *codesHuffmanLength; ///< Huffman length code alphabet (address of an array of uint32_t).
	uint32_t  *codesHuffmanDistance; ///< Huffman distance code alphabet (address of an array of uint32_t).
	uint16_t  sizeCodesHuffmanLength; ///< Number of entries in Huffman length code alphabet.
	uint16_t  sizeCodesHuffmanDistance; ///< Number of entries in Huffman distance code alphabet.
	uint8_t   bitsRemaining; ///< Number of unprocessed bits left in bit buffer.
	uint8_t   bufferBits; ///< Recently read byte that serves a bit buffer.
	uint32_t  valueBufferBits; ///< Integer value of the bits read the last time readBitsIDAT() was called.
	uint32_t  indexBufferInflate; ///< Index of next free byte in INFLATE buffer.
	uint32_t  indexReading; ///< Index of next byte yet-to-read in INFLATE buffer.
	uint16_t  sizeWindow; ///< Number of bytes in INFLATE buffer.
	uint8_t   *bufferInflate; ///< INFLATE buffer (address of an array of bytes).
	bool      doDecoding; ///< Boolean indicating that zlib/DEFLATE decoding should continue.
} PngData;

//------------------------------------------------------------------------------
// function prototypes
//------------------------------------------------------------------------------

/** Constructor: create and initialise a PngData structure.
 * 
 * This initialises all pointers to NULL, sets sizes to zero and resets states.
 * 
 * @returns An initialised PngData structure.
 */
PngData* constructPngData();

/** Destructor: free any allocated memory in a PngData structure.
 * 
 * This calls free on all pointers (and fclose on file) and sets sizes to zero.
 * 
 * @param self A PngData structure.
 */
void destructPngData(PngData **self);

/** Pixel conversion routine: RGBA5658 from 1 bit greyscale value.
 * 
 * @param self A PngData structure.
 * @param x row position in current scanline.
 * @returns An RGBA5658 pixel information structure; alpha = 0xff.
 */
RGBA5658 convertPixelGrey1(PngData *self, uint16_t x);

/** Pixel conversion routine: RGBA5658 from 2 bit greyscale value.
 * 
 * @param self A PngData structure.
 * @param x row position in current scanline.
 * @returns An RGBA5658 pixel information structure; alpha = 0xff.
 */
RGBA5658 convertPixelGrey2(PngData *self, uint16_t x);

/** Pixel conversion routine: RGBA5658 from 4 bit greyscale value.
 * 
 * @param self A PngData structure.
 * @param x row position in current scanline.
 * @returns An RGBA5658 pixel information structure; alpha = 0xff.
 */
RGBA5658 convertPixelGrey4(PngData *self, uint16_t x);

/** Pixel conversion routine: RGBA5658 from 8 bit greyscale value.
 * 
 * @param self A PngData structure.
 * @param x row position in current scanline.
 * @returns An RGBA5658 pixel information structure; alpha = 0xff.
 */
RGBA5658 convertPixelGrey8(PngData *self, uint16_t x);

/** Pixel conversion routine: RGBA5658 from 16 bit greyscale value.
 * 
 * @param self A PngData structure.
 * @param x row position in current scanline.
 * @returns An RGBA5658 pixel information structure; alpha = 0xff.
 */
RGBA5658 convertPixelGrey16(PngData *self, uint16_t x);

/** Pixel conversion routine: RGBA5658 from 1 bit indexed value.
 * 
 * @param self A PngData structure.
 * @param x row position in current scanline.
 * @returns An RGBA5658 pixel information structure; alpha = 0xff.
 */
RGBA5658 convertPixelIndexed1(PngData *self, uint16_t x);

/** Pixel conversion routine: RGBA5658 from 2 bit indexed value.
 * 
 * @param self A PngData structure.
 * @param x row position in current scanline.
 * @returns An RGBA5658 pixel information structure; alpha = 0xff.
 */
RGBA5658 convertPixelIndexed2(PngData *self, uint16_t x);

/** Pixel conversion routine: RGBA5658 from 4 bit indexed value.
 * 
 * @param self A PngData structure.
 * @param x row position in current scanline.
 * @returns An RGBA5658 pixel information structure; alpha = 0xff.
 */
RGBA5658 convertPixelIndexed4(PngData *self, uint16_t x);

/** Pixel conversion routine: RGBA5658 from 8 bit indexed value.
 * 
 * @param self A PngData structure.
 * @param x row position in current scanline.
 * @returns An RGBA5658 pixel information structure; alpha = 0xff.
 */
RGBA5658 convertPixelIndexed8(PngData *self, uint16_t x);

/** Pixel conversion routine: RGBA5658 from 8 bit RGB value.
 * 
 * @param self A PngData structure.
 * @param x row position in current scanline.
 * @returns An RGBA5658 pixel information structure; alpha = 0xff.
 */
RGBA5658 convertPixelRGB8(PngData *self, uint16_t x);

/** Pixel conversion routine: RGBA5658 from 16 bit RGB value.
 * 
 * @param self A PngData structure.
 * @param x row position in current scanline.
 * @returns An RGBA5658 pixel information structure; alpha = 0xff.
 */
RGBA5658 convertPixelRGB16(PngData *self, uint16_t x);

/** Pixel conversion routine: RGBA5658 from 8 bit greyscale+alpha value.
 * 
 * @param self A PngData structure.
 * @param x row position in current scanline.
 * @returns An RGBA5658 pixel information structure.
 */
RGBA5658 convertPixelGreyA8(PngData *self, uint16_t x);

/** Pixel conversion routine: RGBA5658 from 16 bit greyscale+alpha value.
 * 
 * @param self A PngData structure.
 * @param x row position in current scanline.
 * @returns An RGBA5658 pixel information structure.
 */
RGBA5658 convertPixelGreyA16(PngData *self, uint16_t x);

/** Pixel conversion routine: RGBA5658 from 8 bit RGB+alpha value.
 * 
 * @param self A PngData structure.
 * @param x row position in current scanline.
 * @returns An RGBA5658 pixel information structure.
 */
RGBA5658 convertPixelRGBA8(PngData *self, uint16_t x);

/** Pixel conversion routine: RGBA5658 from 16 bit RGB+alpha value.
 * 
 * @param self A PngData structure.
 * @param x row position in current scanline.
 * @returns An RGBA5658 pixel information structure.
 */
RGBA5658 convertPixelRGBA16(PngData *self, uint16_t x);
/** Read the chunk header at current file position.
 * 
 * This function sets self->lenChunk and self->typeChunk and does some
 * sanity checking:
 *  - IHDR chunk: lenChunk has to be 13 bytes.
 *  - PLTE chunk: lenChunk has to be >= 3 bytes (one entry),
 *                                   <= 768 (256 entries),
 *                                   mod 3 == 0 (divisible by three).
 * 
 * @param self Address of a PngData structure.
 * @returns A signed byte (int8_t) with one of the following return codes:
 *     - RET_OK: successfully found next chunk of given type.
 *     - RET_READ: reading from file failed.
 *     - RET_HEADER: invalid IHDR length.
 *     - RET_PALETTE: invalid PLTE length.
 */
int8_t readChunkHeader(PngData *self);

/** Seek next chunk of the given typeChunkRequested type.
 * 
 * @param self Address of a PngData structure.
 * @param typeChunkRequested Number of chunk type, one of the CHUNK_* consts.
 * @returns A signed byte (int8_t) with one of the following return codes:
 *     - RET_OK: successfully found next chunk of given type.
 *     - RET_SEEK: seeking next chunk failed (EOF?).
 *     - any error reported by readChunkHeader().
 */
int8_t seekChunk(PngData *self, uint8_t typeChunkRequested);

/** Skip remaining bits in order to continue at a byte boundary.
 * If chunk is exhausted, this function skips to next IDAT chunk.
 * 
 * @param self Address of a PngData structure.
 */
void skipRemainingBits(PngData *self);

/** Read numBytes bytes from the current chunk into the given byte buffer.
 * If chunk is exhausted, this function skips to next IDAT chunk.
 * 
 * The caller is responsible for having allocated enough buffer memory.
 * 
 * @param self Address of a PngData structure.
 * @param buffer Address of a byte (uint8_t) array.
 * @param numBytes Number of bytes to read; uint16_t, range 0..0xffff.
 * @returns A signed byte (int8_t) with one of the following return codes:
 *     - RET_OK: all bytes successfully read.
 *     - RET_READ: reading the file failed.
 *     - any error reported by seekChunk().
 */
int8_t readBytesIDAT(PngData *self, uint8_t *buffer, uint16_t numBytes);

/** Read numBits bits from the current chunk into the internal value buffer.
 * If chunk is exhausted, this function skips to next IDAT chunk.
 * 
 * The (little endian) value of the bits is stored in self->valueBufferBits.
 * 
 * @param self Address of a PngData structure.
 * @param numBits Number of bits to read; uint8_t, range 0..0xff.
 * @returns A signed byte (int8_t) with one of the following return codes:
 *     - RET_OK: all bytes successfully read.
 *     - RET_READ: reading the file failed.
 *     - any error reported by seekChunk().
 */
int8_t readBitsIDAT(PngData *self, uint8_t numBits);

/** Look for a known bit code sequence at current file position.
 * If chunk is exhausted, this function skips to next IDAT chunk.
 * 
 * @param self Address of a PngData structure.
 * @param lenCodes Number of codes in given alphabet.
 * @param codes Address of an array of codes (packed into uint32_ts).
 * @returns A signed byte (int8_t) with one of the following return codes:
 *     - RET_OK: all bytes successfully read.
 *     - RET_CODE_NOT_FOUND: no code matches the current bit sequence.
 *     - any error reported by readBitsIDAT().
 */
int8_t checkCode(PngData *self, uint16_t lenCodes, uint32_t *codes);

/** Generate a Huffman code alphabet from a set of bit lengths.
 * 
 * @param numLengths Number of lengths.
 * @param lengths Address of a uint8_t array of lengths.
 * @param codeStruct Pointer to the address of a code array. If codeStruct is not Null, free() is called and a new struct is allocated.
 * @param sizeCodeStruct Address of a uint16_t var to hold the number of generated codes (i.e. the size of codeStruct).
 * @returns A signed byte (int8_t) with one of the following return codes:
 *     - RET_OK: all bytes successfully read.
 *     - RET_MALLOC_CODE: failed to allocate code alphabet memory.
 */
int8_t generateHuffmanCodes(uint16_t numLengths, uint8_t *lengths, uint32_t **codeStruct, uint16_t *sizeCodeStruct);

/** Read a new scanline from the file by trying to decode zlib/DEFLATE data
 *  from IDAT chunks. Seeks next IDAT chunk if current chunk gets exhausted.
 * 
 * @param self Address of a PngData structure.
 * @param numBytes Number of bytes to read/decode.
 * @returns A signed byte (int8_t) with one of the following return codes:
 *     - RET_OK: all bytes successfully read.
 *     - RET_ZLIB_COMPRESSION: invalid zlib compression method.
 *     - RET_ZLIB_WINSIZE: invalid zlib decode window size.
 *     - RET_PRESET_DICT: zlib stream features preset dictionary (shall not appear in PNG).
 *     - RET_DEFLATE_COMPRESSION: invalid DEFLATE compression type.
 *     - RET_MALLOC_BUFFER_INFLATE: failed to allocate decode window memory.
 *     - RET_MALLOC_CODE: failed to allocate code alphabet memory.
 *     - RET_INVALID_CODE_LEN_CODE: encountered code length code 16 at beginning of block.
 *     - RET_LENGTHS_OVERFLOW: created more lengths from codes length codes than announced.
 *     - RET_INVALID_LENGTH_CODE: encountered invalid length code (most unlikely!).
 *     - RET_INVALID_DISTANCE_CODE: encountered invalid length code (most unlikely!).
 *     - RET_NOCOMP_LEN: LEN and NLEN data mismatch in uncompressed DEFLATE block.
 *     - any error reported by seekChunk().
 *     - any error reported by readBytesIDAT().
 *     - any error reported by readBitsIDAT().
 *     - any error reported by generateHuffmanCodes().
 *     - any error reported by checkCode().
 */
int8_t readScanline(PngData *self, uint16_t numBytes);

/** Paeth predictor function used when applying FILTER_PAETH to a scanline.
 * 
 * Works with 16 bit integer precision as requested in the PNG specs.
 * 
 * @param a Value of corresponding pixel byte to the left.
 * @param b Value of corresponding pixel byte in previous scanline.
 * @param c Value of Corresponding pixel byte in previous scanline, to the left.
 * @returns Value of the closest byte value.
 */
uint16_t PaethPredictor(uint16_t a, uint16_t b, uint16_t c);

/** Central PNG reading function. Reads file with given filename and creates
 *  an Image structure. Uses given PngData structure for state information
 *  and tracking of allocated memory.
 * 
 * @param self Address of a PngData structure; 
 * @param filename Address of a filename string (char array).
 * @param image Address of an Image structure. Any allocated image memory will be freed and rewritten.
 * @returns A signed byte (int8_t) with one of the following return codes:
 *     - RET_OK: all bytes successfully read.
 *     - RET_OPEN: failed opening given file.
 *     - RET_READ: failed reading from file.
 *     - RET_MAGIC: first eight bytes don't match the PNG magic bytes.
 *     - RET_RET_HEADER: first chunk in file is not the IHDR chunk.
 *     - RET_DIMENSIONS: invalid image dimensions.
 *     - RET_COMPRESSION_METHOD: invalid PNG compression method.
 *     - RET_FILTER_METHOD: invalid PNG filter method.
 *     - RET_INTERLACE_METHOD: invalid PNG interlace method.
 *     - RET_BIT_DEPTH: invalid bit depth value or invalid combination of bit depth and colour type.
 *     - RET_MALLOC_PALETTE: failed to allocate palette memory.
 *     - RET_COLOUR_TYPE: invalid colour type value.
 *     - RET_MALLOC_IMAGE: failed allocating image memory.
 *     - RET_MALLOC_SCANLINE: failed allocating scanline buffer memory.
 *     - RET_FILTER_TYPE: invalid filter type value.
 *     - any error reported by readChunkHeader().
 *     - any error reported by seekChunk().
 *     - any error reported by readScanline().
 */
int8_t readPNG(PngData *self, char *filename, Surface *image);

/** PNG reading wrapper function. Load the PNG file with given filename.
 * 
 * Manages PngData automatically.
 * 
 * @param filename Address of a filename string (char array).
 * @returns A pointer to a surface with the image bitmap data. Might be NULL if something went wrong.
 */
Surface *loadImage(char *filename);

#endif // _FAREADPNG_H
