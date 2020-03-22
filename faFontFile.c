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
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#include "faFontFile.h"
#include "faSurfaceBase.h"


FontFileData *fontFileConstruct() {
	FontFileData *data = NULL;
	data = (FontFileData*)malloc(sizeof(FontFileData));
	if (data != NULL) {
		data->width = 0;
		data->height = 0;
		data->G = NULL;
		data->V = NULL;
		data->sizeVWord = 0;
		data->sizeVEntry = 0;
		data->iReplChar = -1;
		data->distChar = 0;
		data->distLine = 0;
		data->tabWidth = 4;
		data->colour = 0xffff;
		data->alpha = 0xff;
		data->colourBg = 0x0000;
		data->alphaBg = 0x00;
		data->mode = BLEND_OVER;
	}
	return data;
}

void fontFileDestruct(FontFileData **self) {
	free((*self)->V);
	free((*self)->G);
	free(*self);
	*self = NULL;
}


// internal function without saveguards! hard-coded value length, doesn't check value validity
// uses the Fowler-Noll-Vo (FNV-1) hash function
// see https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
uint32_t hashFNV1(uint8_t *value, uint32_t seed) {
	if (seed == 0) seed = FNV1OFFSET;
	seed = ((seed * FNV1PRIME) & FNV1MASK) ^ value[0];
	seed = ((seed * FNV1PRIME) & FNV1MASK) ^ value[1];
	seed = ((seed * FNV1PRIME) & FNV1MASK) ^ value[2];
	return seed;
}


int32_t fontFileLookUpIndex(FontFileData *self, uint8_t *code) {
	if (self == NULL || self->G == NULL || self->V == NULL || code == NULL) return self->iReplChar;
	
	// do first hash with default seed
	//int32_t g = self->G[hashFNV1(code,0) % self->nChars];
	uint32_t g = ((FNV1OFFSET * FNV1PRIME) & FNV1MASK) ^ code[0];
	g = ((g * FNV1PRIME) & FNV1MASK) ^ code[1];
	g = ((g * FNV1PRIME) & FNV1MASK) ^ code[2];
	g %= self->nChars;
	
	// decide if direct look-up oder second hash with custom seed is needed
	//int32_t v = (g < 0) ? -g-1 : self->V[hashFNV1(code,g) % self->nChars];
	if (self->G[g] < 0) {
		g = -self->G[g]-1;
	} else {
		g = ((self->G[g] * FNV1PRIME) & FNV1MASK) ^ code[0];
		g = ((g * FNV1PRIME) & FNV1MASK) ^ code[1];
		g = ((g * FNV1PRIME) & FNV1MASK) ^ code[2];
		g %= self->nChars;
	}
	
	uint8_t *v = self->V + g * self->sizeVEntry; // actual byte index in V
	if (*v++ != *code++ || *v++ != *code++ || *v++ != *code++)
		// retrieved value's code and requested code doesn't match: return position of replacement character
		return self->iReplChar;
	else
		// codes match: return retrieved position
		return g;
}


int8_t fontFileRead(FontFileData *self, char *filename) {
	// check that data structure exists
	if (self == NULL || filename == NULL) return RET_FAFF_ARGS;
	
	// open file
	int file = epic_file_open(filename,"rb");
	if (file < 0) return RET_FAFF_OPEN;
	
	// define general purpose read buffer
	uint8_t readBuffer[8];
	uint32_t i;
	
	// read and check header:
	//   bytes 0..1 should be 0xfa 0xff (signature)
	//   byte  2    is width-1
	//   byte  3    is height-1
	//   bytes 4..7 is number of chars, nChars, as uint32_t, big-endian
 	if (epic_file_read(file,readBuffer,8) != 8) {
		epic_file_close(file);
		return RET_FAFF_READ;
	}
	if (readBuffer[0] != 0xfa || readBuffer[1] != 0xff) {
		epic_file_close(file);
		return RET_FAFF_MAGIC;
	}
	
	// reading succeeded: free and reset data structure
	free(self->G);
	free(self->V);
// 	surfaceDestruct(&self->buffer);
	
	self->width = 0;
	self->height = 0;
	self->G = NULL;
	self->V = NULL;
	self->sizeVWord = 0;
	self->sizeVEntry = 0;
	self->iReplChar = -1;
	
	// signature found, process header vars
	self->width  = readBuffer[2];
	self->height = readBuffer[3];
	self->nChars = (readBuffer[4] << 24) + (readBuffer[5] << 16) + (readBuffer[6] << 8) + readBuffer[7];
	self->sizeVWord = (((self->height-1) >> 3) + 1);
	self->sizeVEntry = 3 + self->width * self->sizeVWord;
	
	// read G table
	self->G = (int32_t*)malloc(self->nChars*sizeof(int32_t));
	if (self->G == NULL) {
		epic_file_close(file);
		return RET_FAFF_GTAB;
	}
	for (i = 0; i < self->nChars; i++) {
		if (epic_file_read(file,readBuffer,4) != 4) {
			free(self->G);
			epic_file_close(file);
			return RET_FAFF_READ;
		}
		// convert 4 byte sequence to integer, big-endian, and calculate two's complement
		self->G[i] = (int32_t)(((readBuffer[0] << 24) + (readBuffer[1] << 16) + (readBuffer[2] << 8) + readBuffer[3]) - 0x100000000);
	}
	
	// read V table: no data processing needed, as this table is a directly-copied sequence of bytes
	uint64_t sizeV = self->nChars*self->sizeVEntry;
	self->V = (uint8_t*)malloc(sizeV);
	if (self->V == NULL) {
		free(self->G);
		epic_file_close(file);
		return RET_FAFF_VTAB;
	}
	if ((uint64_t)epic_file_read(file,self->V,sizeV) != sizeV) {
		free(self->G);
		free(self->V);
		epic_file_close(file);
		return RET_FAFF_READ;
	}
	
	// look-up Unicode replacement character U+FFFD to check integrity of tables
	// save the replacement char's index in the data structure to speed up retrieval in case of look-up errors 
	uint8_t replChar[3] = {0x00,0xff,0xfd};
	self->iReplChar = fontFileLookUpIndex(self,replChar);
	if (self->iReplChar < 0) {
		free(self->G);
		free(self->V);
		epic_file_close(file);
		return RET_FAFF_REPLCHAR;
	}
	
	return RET_FAFF_OK;
}


FontFileData *fontFileLoad(char *filename) {
	FontFileData *data = fontFileConstruct();
	if (data == NULL) return NULL;
	int8_t retval = fontFileRead(data,filename);
	if (retval == RET_FAFF_OK) {
		return data;
	} else {
		fontFileDestruct(&data);
		return NULL;
	}
}


//------------------------------------------------------------------------------
// helper functions for INTERNAL USE (without input argument saveguards!)
//------------------------------------------------------------------------------

// get byte at current text pointer position, process UTF-8 (possibly consuming
// more bytes) and move the pointer to the next byte
// returns the Unicode character code
int32_t fontFileGetNextUTF8(char **text) {
	int32_t uCode = -1;
	
	if (**text < 128) {
		// no encoding, i.e. ASCII value
		uCode = **text;
		
	} else if (**text > 193 && **text < 224) {
		// first byte of a sequence of two
		// extract lower five bits (=bits 6..10)
		uCode = (**text & 0b00011111) << 6;
		
		// move on; stop if end-of-string reached or next character is not a follow-up byte
		(*text)++;
		if (!(**text) || **text < 128 || **text > 191) return -1;
		uCode |= (**text & 0b00111111);
		
	} else if (**text > 223 && **text < 240) {
		// first byte of a sequence of three
		// extract lower four bits (=bits 11..15)
		uCode = (**text & 0b00001111) << 12;
		
		// move on; stop if end-of-string reached or next character is not a follow-up byte
		(*text)++;
		if (!(**text) || **text < 128 || **text > 191) return -1;
		uCode |= (**text & 0b00111111) << 6;
		
		// move on; stop if end-of-string reached or next character is not a follow-up byte
		(*text)++;
		if (!(**text) || **text < 128 || **text > 191) return -1;
		uCode |= (*((*text)++) & 0b00111111);
		
	} else if (**text > 239 && **text < 245) {
		// first byte of a sequence of four
		// extract lower three bits (=bits 11..15)
		uCode = (**text & 0b00000111) << 18;
		
		// move on; stop if end-of-string reached or next character is not a follow-up byte
		(*text)++;
		if (!(**text) || **text < 128 || **text > 191) return -1;
		uCode |= (**text & 0b00111111) << 12;
		
		// move on; stop if end-of-string reached or next character is not a follow-up byte
		(*text)++;
		if (!(**text) || **text < 128 || **text > 191) return -1;
		uCode |= (**text & 0b00111111) << 6;
		
		// move on; stop if end-of-string reached or next character is not a follow-up byte
		(*text)++;
		if (!(**text) || **text < 128 || **text > 191) return -1;
		uCode |= (**text & 0b00111111);
	}
	
	(*text)++;
	return uCode;
}

// look-up a character code in the given font data structure and paint the
// retrieved symbol at the given position of the given surface
// Note: cursor is moved on automatically afterwards (x += width + distChar)
void fontFileLookUpAndDraw(Surface *surface, SurfaceMod *mask, FontFileData *font, Point *cursor, int32_t uCode) {
	uint8_t uCodeBytes[3];
	int16_t xStart,yStart,xStartFont,yStartFont,width,height;
	uint16_t iSurface,iSurfaceStart;
	uint8_t *value,yMask,x,y;
	uint32_t bitmask;
	
	uCodeBytes[0] = (uCode & 0xff0000) >> 16;
	uCodeBytes[1] = (uCode & 0x00ff00) >> 8;
	uCodeBytes[2] = (uCode & 0x0000ff);
	value = font->V + 3 + fontFileLookUpIndex(font,uCodeBytes)*font->sizeVEntry;
	
	// edit starting point and dimensions so that rendering starts
	// and ends inside the visible surface area
	width = font->width;
	if (cursor->x < 0) {
		xStartFont = -cursor->x;
		value += -cursor->x*font->sizeVWord;
		xStart = 0;
	} else { 
		xStartFont = 0;
		xStart = cursor->x;
	}
	if (xStart + width > surface->width) {
		width = surface->width - xStart;
	}
	
	height = font->height;
	if (cursor->y < 0) {
		yStartFont = -cursor->y;
		yStart = 0;
	} else { 
		yStartFont = 0;
		yStart = cursor->y;
	}
	if (yStart + height > surface->height) {
		height = surface->height - yStart;
	}
	
	if (width > 0 && height > 0) {
		// character at least partial visible: draw it
		iSurfaceStart = yStart * surface->width + xStart;
		for (x = xStartFont; x < width; x++) {
			yMask = yStart;
			iSurface = iSurfaceStart;
			bitmask = 0;
			for (y = yStartFont; y < height; y++) {
				if (*(value+(y>>3)) & (1 << (y & 7))) {
					// blend foreground colour
					if (surfacePixelBlend(
							font->colour,font->alpha,
							surface->rgb565[iSurface],surface->alpha[iSurface],
							&surface->rgb565[iSurface],&surface->alpha[iSurface],
							font->mode)) bitmask |= 1 << (yMask >> 3); // pixel changed: mark in bitmask
				} else {
					// blend background colour
					if (surfacePixelBlend(
							font->colourBg,font->alphaBg,
							surface->rgb565[iSurface],surface->alpha[iSurface],
							&surface->rgb565[iSurface],&surface->alpha[iSurface],
							font->mode)) bitmask |= 1 << (yMask >> 3); // pixel changed: mark in bitmask
				}
				iSurface += surface->width;
				yMask++;
			}
			surfaceModSetCol(mask,xStart,bitmask);
			iSurfaceStart++;
			xStart++;
			value += font->sizeVWord;
		}
	}
	cursor->x += font->width + font->distChar;
}

// parse a printf-like format string, possibly consuming more characters
// (text pointer gets modified!)
// flags gets set-up according to the format string properties (flags, type),
// width is set to the specified field width
// flags is set to FAFF_FMT_NONE if string is invalid
void fontFileParseFormatstring(char **text,uint16_t *fmtFlags, uint8_t *fmtWidth) {
	// situation: print routine encountered a '%'
	// so a format string has to be parsed
	//
	// general syntax: [Flags][Width]Type
	uint16_t flags = FAFF_FMT_FLAGS;
	uint16_t width = 0;
	
	if (**text == 0x25) {
		// percent sign just after initial percent sign: literal percent sign
		*fmtFlags = FAFF_FMT_LITERAL;
		*fmtWidth = 0;
		(*text)++;
		return;
	}
	
	while (**text) {
		// as format strings are ASCII-only, no UTF-8 decoding is needed
		switch (**text) {
			case 0x2d: // character '-'
				if (flags & FAFF_FMT_FLAGS) {
					// in flags state: set minus flag
					flags |= FAFF_FMT_MINUS;
				} else {
					// otherwise: invalid format string, exit
					*fmtFlags = FAFF_FMT_NONE;
					*fmtWidth = 0;
					(*text)++;
					return;
				}
				break;
				
			case 0x2b: // character '+'
				if (flags & FAFF_FMT_FLAGS) {
					// in flags state: set minus flag
					flags |= FAFF_FMT_PLUS;
				} else {
					// otherwise: invalid format string, exit
					*fmtFlags = FAFF_FMT_NONE;
					*fmtWidth = 0;
					(*text)++;
					return;
				}
				break;
				
			case 0x20: // character <space>
				if (flags & FAFF_FMT_FLAGS) {
					// in flags state: set minus flag
					flags |= FAFF_FMT_SPACE;
				} else {
					// otherwise: invalid format string, exit
					*fmtFlags = FAFF_FMT_NONE;
					*fmtWidth = 0;
					(*text)++;
					return;
				}
				break;
				
			case 0x30: // character "0"
				if (flags & FAFF_FMT_FLAGS) {
					// in flags state: switch to width state, set zero padding flag
					flags &= ~FAFF_FMT_FLAGS;
					flags |= FAFF_FMT_PAD0 | FAFF_FMT_WIDTH;
					break;
				} // otherwise: in width mode, fall through and calculate width
			case 0x31: // character "1"
			case 0x32: // character "2"
			case 0x33: // character "3"
			case 0x34: // character "4"
			case 0x35: // character "5"
			case 0x36: // character "6"
			case 0x37: // character "7"
			case 0x38: // character "8"
			case 0x39: // character "9"
				flags &= ~FAFF_FMT_FLAGS; // just in case: remove flags flag
				flags |= FAFF_FMT_WIDTH; // ..and set width flag 
				width = width * 10 + (**text - 0x30);
				if (width > 255) width = 255;
				break;
				
			case 0x69: // character 'i'; convert integer to decimal representation
				flags |= FAFF_FMT_DECIMAL;
				*fmtFlags = flags;
				*fmtWidth = (uint8_t)width;
				(*text)++;
				return;
				
			case 0x78: // character 'x': convert integer to hexadecimal representation, lower case
				flags |= FAFF_FMT_HEXLOWER;
				*fmtFlags = flags;
				*fmtWidth = (uint8_t)width;
				(*text)++;
				return;
				
			case 0x58: // character 'X': convert integer to hexadecimal representation, upper case
				flags |= FAFF_FMT_HEXUPPER;
				*fmtFlags = flags;
				*fmtWidth = (uint8_t)width;
				(*text)++;
				return;
				
			case 0x6f: // character 'o': convert integer to octal representation
				flags |= FAFF_FMT_OCTAL;
				*fmtFlags = flags;
				*fmtWidth = (uint8_t)width;
				(*text)++;
				return;
			
			case 0x73: // character 's': insert a string
				flags |= FAFF_FMT_STRING;
				*fmtFlags = flags;
				*fmtWidth = (uint8_t)width;
				(*text)++;
				return;
			
			default: // any other character: invalid format string
				flags = FAFF_FMT_NONE;
				*fmtFlags = flags;
				*fmtWidth = (uint8_t)width;
				(*text)++;
				return;
		}
		(*text)++;
	}
	*fmtFlags = flags;
	*fmtWidth = (uint8_t)width;
}

// calculate and return the length of a UTF-8 string
int64_t fontFileGetUTF8Length(char *string) {
	int64_t length = 0;
	while (*string) {
		// *string < 128: ASCII range
		if (*string > 193 && *string < 224) {
			// first byte of a sequence of two
			// move on; stop if end-of-string reached or next character is not a follow-up byte
			length++;
			string++;
			if (!(*string) || *string < 128 || *string > 191) return -1;
		} else if (*string > 223 && *string < 240) {
			// first byte of a sequence of three
			// move on; stop if end-of-string reached or next character is not a follow-up byte
			length++;
			string++;
			if (!(*string) || *string < 128 || *string > 191) return -1;
			// move on; stop if end-of-string reached or next character is not a follow-up byte
			length++;
			string++;
			if (!(*string) || *string < 128 || *string > 191) return -1;
		} else if (*string > 239 && *string < 245) {
			// first byte of a sequence of four
			// move on; stop if end-of-string reached or next character is not a follow-up byte
			length++;
			string++;
			if (!(*string) || *string < 128 || *string > 191) return -1;
			// move on; stop if end-of-string reached or next character is not a follow-up byte
			length++;
			string++;
			if (!(*string) || *string < 128 || *string > 191) return -1;
			// move on; stop if end-of-string reached or next character is not a follow-up byte
			length++;
			string++;
			if (!(*string) || *string < 128 || *string > 191) return -1;
		}
		length++;
		string++;
	}
	return length;
}

//------------------------------------------------------------------------------
// text rendering functions
//------------------------------------------------------------------------------

BoundingBox fontFilePrint(Surface *surface, SurfaceMod *mask, FontFileData *font, Point p, char *text, ...) {
	// sanity check
	if (surface == NULL || mask == NULL || font == NULL) return boundingBoxCreate(0,0,0,0);
	
	// initialise argument list
	va_list args;
	va_start(args,text);
	
	// prepare bounding box
	BoundingBox bb;
	bb.min = p;
	bb.max = p;
	Point cursor = p;
	
	// parse text byte by byte, assuming it's UTF-8 encoded
	int32_t uCode;
	int valueInt,valueIntTmp,factor;
	uint16_t flags;
	int16_t width;
	uint8_t base,digit,widthTmp;
	char *valueString;
	
	while (*text) {
		// take next UTF-8 sequence from text; afterwards text points to next character
		uCode = fontFileGetNextUTF8(&text);
		
		if (uCode > 0) {
			// valid unicode character code detected
			switch (uCode) {
				case 0x0008:
					// backspace character: move p one symbol back
					cursor.x -= font->width + font->distChar;
					if (cursor.x < bb.min.x) bb.min.x = cursor.x;
					continue;
					
				case 0x0009:
					// horizontal tabulator: move cursor font->tabWidth steps forth
					cursor.x += font->tabWidth * (font->width + font->distChar);
					if (cursor.x > bb.max.x) bb.max.x = cursor.x;
					continue;
					
				case 0x000a:
					// line feed: UNIX newline, move cursor to the beginning of the next line
					cursor.x = p.x;
					cursor.y += font->height + font->distLine;
					if (cursor.y > bb.max.y) bb.max.y = cursor.y;
					continue;
					
				case 0x000b:
					// vertical tabulator
					cursor.y += font->tabWidth * (font->height + font->distLine);
					if (cursor.y > bb.max.y) bb.max.y = cursor.y;
					continue;
					
				case 0x000d:
					// carriage return: move cursor the the beginning of this line
					cursor.x = p.x;
					continue;
					
				case 0x0025:
					// percentage sign: parse a format string
					fontFileParseFormatstring(&text,&flags,&widthTmp);
					width = widthTmp;
					// check type
					if (flags & FAFF_FMT_DECIMAL) {
						base = 10;
					} else if (flags & FAFF_FMT_HEXLOWER) {
						base = 16;
					} else if (flags & FAFF_FMT_HEXUPPER) {
						base = 16;
					} else if (flags & FAFF_FMT_OCTAL) {
						base = 8;
					} else if (flags & FAFF_FMT_STRING) {
						// just copy a null-terminated string argument to the current cursor position
						valueString = va_arg(args,char*);
						factor = fontFileGetUTF8Length(valueString);
						if (factor < 0) continue; // valueString could not be parsed as UTF-8
						widthTmp -= factor; // subtract string length from width
						if (!(flags & FAFF_FMT_MINUS)) {
							// minus flag not set: right-align, thus prepend remaining width as spaces
							while (width > 0) {
								fontFileLookUpAndDraw(surface,mask,font,&cursor,0x20);
								width--;
							}
						}
						while (*valueString) {
							uCode = fontFileGetNextUTF8(&valueString);
							fontFileLookUpAndDraw(surface,mask,font,&cursor,uCode);
						}
						if (flags & FAFF_FMT_MINUS) {
							// minus flag set: left-align, thus fill remaining width with spaces
							while (width> 0) {
								fontFileLookUpAndDraw(surface,mask,font,&cursor,0x20);
								width--;
							}
						}
						// adjust bounding box if necessary and move on
						if (cursor.x > bb.max.x) bb.max.x = cursor.x;
						continue;
					} else if (flags & FAFF_FMT_LITERAL) {
						// literal percentage sign: break and draw it
						break;
					} else {
						// invalid format string: head on
						continue;
					}
					//
					// convert an integer to a string
					//
					// 1. get next input argument
					valueInt = va_arg(args,int);
					
					// 2. check sign and draw it if needed
					if (valueInt < 0) {
						// negative value: always paint sign, make value absolute for following algorithm
						fontFileLookUpAndDraw(surface,mask,font,&cursor,0x2d);
						valueInt = -valueInt;
						width--; // sign is part of the field width
					} else if (flags & FAFF_FMT_PLUS) {
						// positive value and plus specified: paint plus
						fontFileLookUpAndDraw(surface,mask,font,&cursor,0x2b);
						width--; // sign is part of the field width
					} else if (flags & FAFF_FMT_SPACE) {
						// positive value and SPACE specified: paint SPACE
						fontFileLookUpAndDraw(surface,mask,font,&cursor,0x20);
						width--; // sign is part of the field width
					}
					// 3. calculate digits; adjust parsed field width
					factor = 1;
					valueIntTmp = valueInt;
					while (valueIntTmp >= base) {
						valueIntTmp /= base;
						factor *= base;
						width--;
					}
					width--; // at least one digit will be drawn
					
					// 4. if any width remains, and no left-alignment is requested (flag 'minus'):
					//    pad value (either with space or zeros)
					if (!(flags & FAFF_FMT_MINUS)) {
						digit = (flags & FAFF_FMT_PAD0) ? 0x30 : 0x20;
						while (width > 0) {
							// as long as width is left: draw pad digit, move cursor one character right
							fontFileLookUpAndDraw(surface,mask,font,&cursor,digit);
							width--;
						}
					}
					// 5. print digits, from largest (left) to smallest factor (1, right)
					while (factor > 0) {
						digit = 0x30 + (valueInt / factor);
						// if digit is outside decimal range, shift it so that the upper/lower case letters a-f are reached
						if (digit > 0x39) digit += (flags & FAFF_FMT_HEXLOWER) ? 0x27 : 0x07 ; 
						valueInt %= factor;
						factor /= base;
						fontFileLookUpAndDraw(surface,mask,font,&cursor,digit);
					}
					// 6. if any width remains, and left-alignment is requested (flag 'minus'):
					//    pad value after printed digits (only with space, ignore any PAD0)
					if (flags & FAFF_FMT_MINUS) {
						while (width > 0) {
							// as long as width is left: draw pad digit, move cursor one character right
							fontFileLookUpAndDraw(surface,mask,font,&cursor,0x20);
							width--;
						}
					}
					// formated string printed, head on
					if (cursor.x > bb.max.x) bb.max.x = cursor.x;
					continue;
			}
			// pre-processing the code fell through: look-up and draw character symbol
			fontFileLookUpAndDraw(surface,mask,font,&cursor,uCode);
		}
	}
	
	// free argument list
	va_end(args);
	
	// printed entire string; bounding box describes the envelope of the cursor (upper left character pixel)
	// thus add width and height to get the real maximum coordinates of the text
	bb.max.x += font->width;
	bb.max.y += font->height;
	
	return bb;
}

