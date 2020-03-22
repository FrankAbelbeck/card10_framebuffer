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
 * Stream-oriented PNG reading library tailored to the card10 badge hardware.
 */

#include "epicardium.h" // uses epic_file* functions

#include <stdint.h> // uses: int8_t, uint8_t, int16_t, uint16_t, uint32_t
#include <stdlib.h> // uses: malloc(), free()
#include <stdbool.h> // uses: bool, true, false
#include <stdio.h> // uses: SEEK_CUR
#include "faReadPng.h"

//------------------------------------------------------------------------------
// methods for PngData structures
//------------------------------------------------------------------------------

// PngData constructor: create and initialise a PngData structure
PngData* pngDataConstruct() {
	PngData *pngdata = NULL;
	pngdata = (PngData*)malloc(sizeof(PngData));
	if (pngdata != NULL) {
		pngdata->sizePalette = 0;
		pngdata->palette = NULL;
		pngdata->scanlineCurrent = NULL;
		pngdata->scanlinePrevious = NULL;
		pngdata->funPixConv = NULL;
 		pngdata->file = -1;
		pngdata->lenChunk = 0;
		pngdata->typeChunk = CHUNK_UNKNOWN;
		pngdata->state = STATE_BEGIN;
		pngdata->isLastBlock = false;
		pngdata->codesHuffmanLength = NULL;
		pngdata->sizeCodesHuffmanLength = 0;
		pngdata->codesHuffmanDistance = NULL;
		pngdata->sizeCodesHuffmanDistance = 0;
		pngdata->bitsRemaining = 0;
		pngdata->bufferBits = 0;
		pngdata->valueBufferBits = 0;
		pngdata->bufferInflate = NULL;
		pngdata->doDecoding = true;
	}
	return pngdata;
}

// PngData destructor: clear any allocated memory, close the file and clear the structure
void pngDataDestruct(PngData **self) {
	if ((*self)->file >= 0)
		if (epic_file_close((*self)->file) >= 0)
			(*self)->file = -1;
	free((*self)->palette);
	free((*self)->scanlineCurrent);
	free((*self)->scanlinePrevious);
	free((*self)->codesHuffmanLength);
	free((*self)->codesHuffmanDistance);
	free((*self)->bufferInflate);
	free(*self);
	*self = NULL;
}

//------------------------------------------------------------------------------
// pixel conversion routines
// rgb565 = ((r>>3) << 11) | ((r>>2) << 5) | (r>>3)
//------------------------------------------------------------------------------

RGBA5658 convertPixelGrey1(PngData *self, uint8_t x) {
	RGBA5658 colour = {0,0};
	uint16_t i = (x >> 3) + 1;
	if (((self->scanlineCurrent[i] >> (x & 7)) & 1) == 1)
		colour.rgb565 = 0xffff;
	else
		colour.rgb565 = 0x0000;
	colour.alpha = 0xff;
	return colour;
}

RGBA5658 convertPixelGrey2(PngData *self, uint8_t x) {
	RGBA5658 colour = {0,0};
	uint16_t i = (x >> 2) + 1;
	uint8_t grey = 85 * ( (self->scanlineCurrent[i] >> (x & 3)) & 3 );
	colour.rgb565 = ((grey >> 3) << 11) | ((grey >> 2) << 5) | (grey >> 3) ;
	colour.alpha = 0xff;
	return colour;
}

RGBA5658 convertPixelGrey4(PngData *self, uint8_t x) {
	uint16_t i = (x >> 1) + 1;
	uint8_t grey = 17 * ( (self->scanlineCurrent[i] >> (x & 1)) & 15 );
	RGBA5658 colour;
	colour.rgb565 = ((grey >> 3) << 11) | ((grey >> 2) << 5) | (grey >> 3) ;
	colour.alpha = 0xff;
	return colour;
}

RGBA5658 convertPixelGrey8(PngData *self, uint8_t x) {
	uint16_t i = x + 1;
	RGBA5658 colour;
	colour.rgb565 = ((self->scanlineCurrent[i] >> 3) << 11) | ((self->scanlineCurrent[i] >> 2) << 5) | (self->scanlineCurrent[i] >> 3) ;
	colour.alpha = 0xff;
	return colour;
}

RGBA5658 convertPixelGrey16(PngData *self, uint8_t x) {
	uint16_t i = x*2 + 1;
	uint16_t grey = (self->scanlineCurrent[i] << 8) | self->scanlineCurrent[i+1];
	RGBA5658 colour;
	colour.rgb565 = ((grey >> 11) << 11) | ((grey >> 10) << 5) | (grey >> 11) ;
	colour.alpha = 0xff;
	return colour;
}

RGBA5658 convertPixelIndexed1(PngData *self, uint8_t x) {
	uint16_t i = (x >> 3) + 1;
	uint8_t indexPalette = (self->scanlineCurrent[i] >> (x & 7)) & 1;
	RGBA5658 colour = {0,0};
	if (indexPalette <= self->sizePalette) {
		colour.rgb565 = self->palette[indexPalette];
		colour.alpha = 0xff;
	}
	return colour;
}

RGBA5658 convertPixelIndexed2(PngData *self, uint8_t x) {
	uint16_t i = (x >> 2) + 1;
	uint8_t indexPalette = (self->scanlineCurrent[i] >> (x & 3)) & 3;
	RGBA5658 colour = {0,0};
	if (indexPalette <= self->sizePalette) {
		colour.rgb565 = self->palette[indexPalette];
		colour.alpha = 0xff;
	}
	return colour;
}

RGBA5658 convertPixelIndexed4(PngData *self, uint8_t x) {
	uint16_t i = (x >> 1) + 1;
	uint8_t indexPalette = (self->scanlineCurrent[i] >> (x & 1)) & 15;
	RGBA5658 colour = {0,0};
	if (indexPalette <= self->sizePalette) {
		colour.rgb565 = self->palette[indexPalette];
		colour.alpha = 0xff;
	}
	return colour;
}

RGBA5658 convertPixelIndexed8(PngData *self, uint8_t x) {
	uint16_t i = x + 1;
	uint8_t indexPalette = self->scanlineCurrent[i];
	RGBA5658 colour = {0,0};
	if (indexPalette <= self->sizePalette) {
		colour.rgb565 = self->palette[indexPalette];
		colour.alpha = 0xff;
	}
	return colour;
}

RGBA5658 convertPixelRGB8(PngData *self, uint8_t x) {
	uint16_t i = x*3 + 1;
	RGBA5658 colour;
	colour.rgb565 = ((self->scanlineCurrent[i] >> 3) << 11) | ((self->scanlineCurrent[i+1] >> 2) << 5) | (self->scanlineCurrent[i+2] >> 3) ;
	colour.alpha = 0xff;
	return colour;
}

RGBA5658 convertPixelRGB16(PngData *self, uint8_t x) {
	uint16_t i = x*6 + 1;
	RGBA5658 colour;
	colour.rgb565 = ((( (self->scanlineCurrent[i] << 8) | self->scanlineCurrent[i+1] ) >> 11) << 11) | \
		((( (self->scanlineCurrent[i+2] << 8) | self->scanlineCurrent[i+3] ) >> 10) << 5) | \
		(( (self->scanlineCurrent[i+4] << 8) | self->scanlineCurrent[i+5] ) >> 11) ;
	colour.alpha = 0xff;
	return colour;
}

RGBA5658 convertPixelGreyA8(PngData *self, uint8_t x) {
	uint16_t i = x*2 + 1;
	RGBA5658 colour;
	colour.rgb565 = ((self->scanlineCurrent[i] >> 3) << 11) | ((self->scanlineCurrent[i] >> 2) << 5) | (self->scanlineCurrent[i] >> 3) ;
	colour.alpha = self->scanlineCurrent[i+1];
	return colour;
}

RGBA5658 convertPixelGreyA16(PngData *self, uint8_t x) {
	uint16_t i = x*4 + 1;
	RGBA5658 colour;
	uint16_t grey = (self->scanlineCurrent[i] << 8) | (self->scanlineCurrent[i+1]);
	colour.rgb565 = ((grey >> 3) << 11) | ((grey >> 2) << 5) | (grey >> 3);
	colour.alpha = ((self->scanlineCurrent[i+2] << 8) | self->scanlineCurrent[i+3]) >> 8;
	return colour;
}

RGBA5658 convertPixelRGBA8(PngData *self, uint8_t x) {
	uint16_t i = x*4 + 1;
	RGBA5658 colour;
	colour.rgb565 = ((self->scanlineCurrent[i] >> 3) << 11) | ((self->scanlineCurrent[i+1] >> 2) << 5) | (self->scanlineCurrent[i+2] >> 3) ;
	colour.alpha = self->scanlineCurrent[i+3];
	return colour;
}

RGBA5658 convertPixelRGBA16(PngData *self, uint8_t x) {
	uint16_t i = x*8 + 1;
	RGBA5658 colour;
	colour.rgb565 = ((( (self->scanlineCurrent[i] << 8) | self->scanlineCurrent[i+1] ) >> 11) << 11) | \
		((( (self->scanlineCurrent[i+2] << 8) | self->scanlineCurrent[i+3] ) >> 10) << 5) | \
		(( (self->scanlineCurrent[i+4] << 8) | self->scanlineCurrent[i+5] ) >> 11) ;
	colour.alpha = ( (self->scanlineCurrent[i+6] << 8) | self->scanlineCurrent[i+7] ) >> 8;
	return colour;
}

//------------------------------------------------------------------------------
// chunk handling
//------------------------------------------------------------------------------

// read and decode chunk type and chunk length
int8_t readChunkHeader(PngData *self) {
	// read length (32 Bit Big Endian)
	uint8_t buffer32[4];
	if (epic_file_read(self->file,&buffer32,4) != 4) return RET_FAPNG_READ;
	self->lenChunk = (uint32_t)BIGENDIAN32(buffer32);
	
	// read and parse type string (4 chars)
	if (epic_file_read(self->file,&buffer32,4) != 4) return RET_FAPNG_READ;
	// primitive prefix tree instead of using strncmp()
	self->typeChunk = CHUNK_UNKNOWN;
	switch (buffer32[0]) {
		case 73:
			// I___
			switch (buffer32[1]) {
				case 72:
					// IH__
					if (buffer32[2] == 68 && buffer32[3] == 82) {
						// IHDR
						self->typeChunk = CHUNK_IHDR;
						if (self->lenChunk != 13) return RET_FAPNG_HEADER;
					}
					break;
				case 68:
					// ID__
					if (buffer32[2] == 65 && buffer32[3] == 84) {
						// IDAT
						self->typeChunk = CHUNK_IDAT;
					}
					break;
				case 69:
					// IE__
					if (buffer32[2] == 78 && buffer32[3] == 68) {
						// IEND
						self->typeChunk = CHUNK_IEND;
					}
					break;
			}
			break;
		case 80:
			// P____
			if (buffer32[1] == 76 && buffer32[2] == 84 && buffer32[3] == 69) {
				// PLTE: contains 1..256 palette entries, each 3 bytes (RGB) wide
				self->typeChunk = CHUNK_PLTE;
				if (self->lenChunk < 3 || self->lenChunk > 768 || self->lenChunk % 3 != 0) return RET_FAPNG_PALETTE;
			}
			break;
	};
	return RET_FAPNG_OK;
}

// skip bytes until the given chunk type is encountered
int8_t seekChunk(PngData *self, uint8_t typeChunkRequested) {
	int8_t retval;
	do {
		if (epic_file_seek(self->file,self->lenChunk+4,SEEK_CUR) != 0) return RET_FAPNG_SEEK;
		retval = readChunkHeader(self);
		if (retval != RET_FAPNG_OK) return retval;
	} while (self->typeChunk != typeChunkRequested);
	return RET_FAPNG_OK;
}

//------------------------------------------------------------------------------
// IDAT chunk reading functions; if reaching end of chunk: skip to next IDAT 
//------------------------------------------------------------------------------

// skip to next byte boundary
void skipRemainingBits(PngData *self) {
	self->bitsRemaining = 0;
	self->valueBufferBits = 0;
}

// read numBytes byte from self->file
int8_t readBytesIDAT(PngData *self, uint8_t *buffer, uint16_t numBytes) {
	uint16_t numBytesRead = 0;
	uint16_t numBytesRemaining = numBytes;
	int8_t retval;
	do {
		if (self->lenChunk < numBytesRemaining) {
			// chunk holds fewer bytes than needed:
			// read only lenChunk bytes, update bytes counters and seek new IDAT chunk
			if ((uint32_t)epic_file_read(self->file,&buffer[numBytesRead],self->lenChunk) != self->lenChunk) return RET_FAPNG_READ;
			numBytesRemaining -= self->lenChunk;
			numBytesRead += self->lenChunk;
			self->lenChunk = 0;
			retval = seekChunk(self,CHUNK_IDAT);
			if (retval != RET_FAPNG_OK) return retval;
		} else {
			// chunk holds enough bytes to satisfy request:
			// read request number of bytes and 
			if ((uint32_t)epic_file_read(self->file,&buffer[numBytesRead],numBytesRemaining) != numBytesRemaining) return RET_FAPNG_READ;
			numBytesRead += numBytesRemaining;
			self->lenChunk -= numBytesRemaining;
			numBytesRemaining = 0;
		}
	} while (numBytesRead < numBytes);
	skipRemainingBits(self);
	return RET_FAPNG_OK;
}

// read numBits bits from self->file
int8_t readBitsIDAT(PngData *self, uint8_t numBits) {
	// preparation: limit numBits to range 0..32
	numBits = (numBits > 32) ? 32 : numBits;
	uint8_t bitsRead = 0;
	int8_t retval;
	
	// consume remaining bits and/or initialise bit value
	if (self->bitsRemaining > 0) {
		if (self->bitsRemaining >= numBits) {
			// buffer holds enough bits
			self->valueBufferBits = self->bufferBits & ((1 << numBits)-1);
			self->bufferBits >>= numBits;
			self->bitsRemaining -= numBits;
			return RET_FAPNG_OK;
		}
		else {
			// consume all remaining bits
			self->valueBufferBits = self->bufferBits;
			numBits -= self->bitsRemaining;
			bitsRead = self->bitsRemaining;
			self->bitsRemaining = 0;
		}
	} else {
		self->valueBufferBits = 0;
	}
	
	// read as many whole bytes as necessary
	while (numBits > 8) {
		if (self->lenChunk > 0) {
			if (epic_file_read(self->file,&self->bufferBits,1) != 1) return RET_FAPNG_READ;
			self->lenChunk--;
			self->valueBufferBits |= self->bufferBits << bitsRead;
			bitsRead += 8;
			numBits -= 8;
		} else {
			// seek next chunk for more bytes
			retval = seekChunk(self,CHUNK_IDAT);
			if (retval != RET_FAPNG_OK) return retval;
		}
	}
	
	// read remaining bits from next byte
	if (numBits > 0) {
		if (self->lenChunk == 0) {
			// seek next chunk for more bytes
			retval = seekChunk(self,CHUNK_IDAT);
			if (retval != RET_FAPNG_OK) return retval;
		}
		if (epic_file_read(self->file,&self->bufferBits,1) != 1) return RET_FAPNG_READ;
		self->lenChunk--;
		self->bitsRemaining = 8 - numBits;
		self->valueBufferBits |= (self->bufferBits & ((1 << numBits)-1)) << bitsRead;
		self->bufferBits >>= numBits;
	}
	return RET_FAPNG_OK;
}

int8_t checkCode(PngData *self, uint16_t lenCodes, uint32_t *codes) {
	// codes are ordered with ascending bit length
	// iterate over all codes, read bits as needed, check value match, write code into valueBufferBits
	int8_t retval;
	uint8_t lenCode;
	uint8_t bitsRead = 0;
	uint8_t lenCodeCurrent = 0;
	uint16_t valueCode = 0;
	for (uint16_t i=0; i<lenCodes; i++) {
		// check current code's bit lengths; if greater, get additional bits from file
		lenCode = EXTRACTLENGTH(codes[i]);
		if (lenCode > lenCodeCurrent) {
			retval = readBitsIDAT(self,lenCode - lenCodeCurrent);
			if (retval != RET_FAPNG_OK) return retval;
			valueCode |= self->valueBufferBits << bitsRead;
			bitsRead += lenCode - lenCodeCurrent;
			lenCodeCurrent = lenCode;
		}
		// check if extracted bit value matches code value; if so, set symbol and exit
		if (EXTRACTBITS(codes[i]) == valueCode) {
			self->valueBufferBits = EXTRACTSYMBOL(codes[i]);
			return RET_FAPNG_OK;
		}
	}
	// fell through: no code matches bit pattern at current file position: error
	return RET_FAPNG_CODE_NOT_FOUND;
}


//------------------------------------------------------------------------------
// zlib/deflate algorithms: inflate IDAT data
//------------------------------------------------------------------------------

// generate a Huffman code alphabet from given lenths
int8_t generateHuffmanCodes(uint16_t numLengths, uint8_t *lengths, uint32_t **codeStruct, uint16_t *sizeCodeStruct) {
	// rationale: longest bit lengths for...
	// ...length codes: 9 bits (static huffman), 7 bits (dynamic huffman)
	// ...distance codes: 15 bits
	// => 15 bits maximum
	uint8_t bl_count[] = {0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0};
	uint16_t next_code[] = {0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0};
	uint16_t i,k,l,length,codebits;
	uint32_t entry;
	uint32_t *codes;
	
	codes = (uint32_t*)malloc(numLengths*sizeof(uint32_t));
	if (codes == NULL) return RET_FAPNG_MALLOC_CODE;
	for (i=0;i<numLengths;i++) codes[i] = 0xffffffff; // pre-set unused codes with highest value (sorting shifts them down)
	
	// algorithm: ref. to RFC 1951, 3.2.2 
	// step 1: count number of codes for each code length
	for (i=0; i < numLengths; i++)
		bl_count[lengths[i]]++;
	
	// step 2: find numerical value of smallest code for each code length
	uint16_t code = 0;
	bl_count[0] = 0;
	for (i=1; i <= 15; i++) {
		code = (code + bl_count[i-1]) << 1;
		next_code[i] = code;
	}
	
	// step 3: assign numerical values to all codes;
	*sizeCodeStruct = 0;
	for (i=0; i < numLengths; i++) {
		if (lengths[i] != 0) {
			length = lengths[i];
			codebits = next_code[length];
			next_code[length]++;
			
			// convert length, codebits and symbol i to a 32 bit uint
			entry = PACKCODE(length,codebits,i);
			
			// copy into codes, sorted (length bits are msb, thus sorted from small to large lengths)
			// kind of insertion sorting, online during list building
			
			k = 0;
			while (k < i && entry > codes[k]) k++; // look for insertion place
			if (k < i) {
				// shift remaining entries up to free kth place: swap places
				// (an empty entry is shifted down to position k)
				for (l=i; l>k; l--) {
					codes[l]   ^= codes[l-1];
					codes[l-1] ^= codes[l];
					codes[l]   ^= codes[l-1];
				}
			}
			codes[k] = entry; // insert at k
			*sizeCodeStruct += 1;
		}
	}
	// reverse code bits in order to use the normal little endian read routine
	for (i=0; i < numLengths; i++) {
		// extract length and codebits from the entry
		length = EXTRACTLENGTH(codes[i]);
		codebits = EXTRACTBITS(codes[i]);
		
		// algorithm: swap odd and even bits, swap bit pairs, swap nibbles, swap bytes
		codebits = ((codebits >> 1) & 0x5555) | ((codebits << 1) & 0xaaaa);
		codebits = ((codebits >> 2) & 0x3333) | ((codebits << 2) & 0xcccc);
		codebits = ((codebits >> 4) & 0x0f0f) | ((codebits << 4) & 0xf0f0);
		codebits = ((codebits >> 8) & 0x00ff) | ((codebits << 8) & 0xff00);
		
		// shift codebits down and trim them to the required bit length
 		codebits >>= 16-length;
 		codebits &= (1 << length) - 1;
		
		// set new entry: clear bits 8..23 and OR the new codebits to them
 		codes[i] &= 0xff0001ff;
  		codes[i] |= ((uint32_t)codebits & 0x7fff) << 9;
	}
	// create codeStruct, copy defined codes and adjust size var (free old one if not NULL)
	if (*codeStruct != NULL) free(*codeStruct);
	*codeStruct = (uint32_t*)malloc(*sizeCodeStruct*sizeof(uint32_t));
	if (*codeStruct == NULL) return RET_FAPNG_MALLOC_CODE;
	for (i=0;i<*sizeCodeStruct;i++) (*codeStruct)[i] = codes[i];
	free(codes);
	return RET_FAPNG_OK;
}

// read numBytes from self->file into self->scanlineCurrent, decoding zlib/DEFLATE data on the fly
int8_t readScanline(PngData *self, uint16_t numBytes) {
	int8_t retval;
	// before we begin: seek next IDAT chunk if not yet inside an IDAT chunk
	if (self->typeChunk != CHUNK_IDAT) {
		retval = seekChunk(self,CHUNK_IDAT);
		if (retval != RET_FAPNG_OK) return retval;
	}
	// read n_bytes into self->scanlineCurrent
	// if chunk end is reached, seek next IDAT chunk or fail
	//
	// buffer management for huffman decoding:
	//  - size according to sizeWindow (max 32k)
	//  - indexDecoding: buffer position for next decoded byte
	//  - indexReading: buffer position for next requested byte
	//  - ring buffer: index increment modulo size, eg.
	//       indexDecoding = (indexDecoding+1 < size) ? indexDecoding + 1 : 0
	//
	// huffman decoding: every decoding operation must be finished;
	// thus it might happen that more bytes are decoded than read
	uint8_t buffer32[4];
	uint8_t *lengths;
	uint16_t length,distance;
	uint8_t extraBits;
	uint32_t indexCopy;
	
	uint16_t numBytesToRead = numBytes;
	uint16_t i;
	while (self->state != STATE_EXIT) {
		switch (self->state) {
			case STATE_BEGIN:
				// start of first IDAT chunk = start of zlib stream (RFC 1950, 2.2)
				// read two bytes CMF and FLG
				retval = readBytesIDAT(self,buffer32,2);
				if (retval != RET_FAPNG_OK) return retval;
				// check CM and CINFO
				if ((buffer32[0] & 0x0f) != 8) return RET_FAPNG_ZLIB_COMPRESSION;
				self->sizeWindow = buffer32[0] >> 4;
				if (self->sizeWindow > 7) return RET_FAPNG_ZLIB_WINSIZE;
				self->sizeWindow = 1 << (self->sizeWindow + 8);
				
				// check FLG; bit 5 should not be set (no preset dictionary!)
				if ((buffer32[1] & 0x20) == 0x20) return RET_FAPNG_PRESET_DICT;
				
				// what follows is compressed data according to RFC 1951
				self->state = STATE_DEFL_BEGIN;
				break;
			
			case STATE_END:
				// inflating data finished
				// signal error if not enough data could be read
				// otherwise skip zlib's ADLER32 checksum and exit
				retval = readBytesIDAT(self,buffer32,4);
				if (retval != RET_FAPNG_OK) return retval;
				free(self->bufferInflate);
				self->state = STATE_EXIT;
				return RET_FAPNG_OK;
				
			case STATE_DEFL_BEGIN:
				retval = readBitsIDAT(self,3);
				if (retval != RET_FAPNG_OK) return retval;
				if ((self->valueBufferBits & 0x01) == 0x01) self->isLastBlock = true;
				switch ((self->valueBufferBits & 0x06) >> 1) {
					case DEFL_NO_COMPRESSION:
						skipRemainingBits(self);
						self->state = STATE_DEFL_NO_COMPRESSION;
						break;
					case DEFL_STAT_HUFFMAN:
						self->state = STATE_DEFL_STAT_HUFFMAN;
						break;
					case DEFL_DYN_HUFFMAN:
						self->state = STATE_DEFL_DYN_HUFFMAN;
						break;
					default:
						return RET_FAPNG_DEFLATE_COMPRESSION;
				}
				break;
			
			case STATE_DEFL_END:
				// end of DEFLATE block: check if it's the last block
				if (self->isLastBlock)
					self->state = STATE_END;
				else
					self->state = STATE_DEFL_BEGIN;
				break;
			
			case STATE_DEFL_STAT_HUFFMAN:
				// allocate zlib buffer
				if (self->sizeWindow == 0) return RET_FAPNG_ZLIB_WINSIZE;
				self->bufferInflate = (uint8_t*)malloc(self->sizeWindow);
				if (self->bufferInflate == NULL) return RET_FAPNG_MALLOC_BUFFER_INFLATE;
				self->indexBufferInflate = 0;
				self->indexReading = 0;
				// calculate static huffman code alphabets
				// create huffman code structure using the bit lengths defined in RFC 1951, 3.2.6
				lengths = (uint8_t*)malloc(288);
				if (lengths == NULL) return RET_FAPNG_MALLOC_CODE;
				for (i=0; i<=143; i++) lengths[i] = 8;
				for (i=144; i<=255; i++) lengths[i] = 9;
				for (i=256; i<=279; i++) lengths[i] = 7;
				for (i=280; i<=287; i++) lengths[i] = 8;
				
				retval = generateHuffmanCodes(288,lengths,&self->codesHuffmanLength,&self->sizeCodesHuffmanLength);
				if (retval != RET_FAPNG_OK) return retval;
				free(lengths);
				
				lengths = (uint8_t*)malloc(32);
				if (lengths == NULL) return RET_FAPNG_MALLOC_CODE;
				for (i=0; i<32; i++) lengths[i] = 5;
				retval = generateHuffmanCodes(32,lengths,&self->codesHuffmanDistance,&self->sizeCodesHuffmanDistance);
				if (retval != RET_FAPNG_OK) return retval;
				free(lengths);
				
				// start decoding
				self->indexBufferInflate = 0;
				self->indexReading = 0;
				self->doDecoding = true;
				self->state = STATE_DEFL_HUFFMAN_DECODE;
				break;
			
			case STATE_DEFL_DYN_HUFFMAN:
				// allocate zlib buffer
				if (self->sizeWindow == 0) return RET_FAPNG_ZLIB_WINSIZE;
				self->bufferInflate = (uint8_t*)malloc(self->sizeWindow);
				if (self->bufferInflate == NULL) return RET_FAPNG_MALLOC_BUFFER_INFLATE;
				self->indexBufferInflate = 0;
				self->indexReading = 0;
				// extract huffman code alphabets from block
				// cf. RFC 1951:
				//  - read 5 bits HLIT = number of lit/len codes - 257
				//  - read 5 bits HDIST = number of dist codes - 1
				//  - read 4 bits HCLEN = number of code length codes - 4
				retval = readBitsIDAT(self,5);
				if (retval != RET_FAPNG_OK) return retval;
				uint16_t numLit = self->valueBufferBits + 257;
				retval = readBitsIDAT(self,5);
				if (retval != RET_FAPNG_OK) return retval;
				uint8_t numDist = self->valueBufferBits + 1;
				retval = readBitsIDAT(self,4);
				if (retval != RET_FAPNG_OK) return retval;
				uint8_t numCodeLen = self->valueBufferBits + 4;
				// read code length code lengths
				lengths = (uint8_t*)calloc(19,1);
				if (lengths == NULL) return RET_FAPNG_MALLOC_CODE;
				for (i=0; i< numCodeLen; i++) {
					retval = readBitsIDAT(self,3);
					if (retval != RET_FAPNG_OK) return retval;
					switch (i) {
						case 0:
							lengths[16] = self->valueBufferBits;
							break;
						case 1:
							lengths[17] = self->valueBufferBits;
							break;
						case 2:
							lengths[18] = self->valueBufferBits;
							break;
						case 3:
							lengths[0] = self->valueBufferBits;
							break;
						case 4:
							lengths[8] = self->valueBufferBits;
							break;
						case 5:
							lengths[7] = self->valueBufferBits;
							break;
						case 6:
							lengths[9] = self->valueBufferBits;
							break;
						case 7:
							lengths[6] = self->valueBufferBits;
							break;
						case 8:
							lengths[10] = self->valueBufferBits;
							break;
						case 9:
							lengths[5] = self->valueBufferBits;
							break;
						case 10:
							lengths[11] = self->valueBufferBits;
							break;
						case 11:
							lengths[4] = self->valueBufferBits;
							break;
						case 12:
							lengths[12] = self->valueBufferBits;
							break;
						case 13:
							lengths[3] = self->valueBufferBits;
							break;
						case 14:
							lengths[13] = self->valueBufferBits;
							break;
						case 15:
							lengths[2] = self->valueBufferBits;
							break;
						case 16:
							lengths[14] = self->valueBufferBits;
							break;
						case 17:
							lengths[1] = self->valueBufferBits;
							break;
						case 18:
							lengths[15] = self->valueBufferBits;
							break;
					}
					if (retval != RET_FAPNG_OK) return retval;
				}
				// build alphabet from code length code lengths
				retval = generateHuffmanCodes(19,lengths,&self->codesHuffmanLength,&self->sizeCodesHuffmanLength);
				if (retval != RET_FAPNG_OK) return retval;
				free(lengths);
				// decode combined lengths for lit/len and distance alphabets
				// using the codes in codesHuffmanLength
				lengths = (uint8_t*)calloc(numLit+numDist,1);
				if (lengths == NULL) return RET_FAPNG_MALLOC_CODE;
				i = 0;
				while (i < numLit+numDist) {
					retval = checkCode(self,19,self->codesHuffmanLength);
					if (retval != RET_FAPNG_OK) return retval;
					if (self->valueBufferBits < 16) {
						// code 0..15: just add this value as length
						lengths[i] = (uint8_t)self->valueBufferBits;
						i++;
					} else if (self->valueBufferBits == 16) {
						// code 16: copy previous lengths x times; x = next 2 bits + 3
						if (i == 0) return RET_FAPNG_INVALID_CODE_LEN_CODE;
						retval = readBitsIDAT(self,2);
						if (retval != RET_FAPNG_OK) return retval;
						for (self->valueBufferBits += 3; self->valueBufferBits > 0; self->valueBufferBits--) {
							lengths[i] = lengths[i-1];
							i++;
							if (i > numLit+numDist) return RET_FAPNG_LENGTHS_OVERFLOW;
						}
					} else if (self->valueBufferBits == 17) {
						// code 17: repeat length 0, x times; x = next 3 bits + 3
						// since lengths is initialised with zeros, just skip the next x lengths
						retval = readBitsIDAT(self,3);
						if (retval != RET_FAPNG_OK) return retval;
						i += self->valueBufferBits + 3;
						if (i > numLit+numDist) return RET_FAPNG_LENGTHS_OVERFLOW;
					} else if (self->valueBufferBits == 18) {
						// code 18: repeat length 0, x times; x = next 7 bits + 11
						// since lengths is initialised with zeros, just skip the next x lengths
						retval = readBitsIDAT(self,7);
						if (retval != RET_FAPNG_OK) return retval;
						i += self->valueBufferBits + 11;
						if (i > numLit+numDist) return RET_FAPNG_LENGTHS_OVERFLOW;
					}
				}
				// all lengths fields are now set up: build code alphabets, de-allocate lengths memory
				retval = generateHuffmanCodes(numLit,lengths,&self->codesHuffmanLength,&self->sizeCodesHuffmanLength);
				if (retval != RET_FAPNG_OK) return retval;
				retval = generateHuffmanCodes(numDist,&lengths[numLit],&self->codesHuffmanDistance,&self->sizeCodesHuffmanDistance);
				if (retval != RET_FAPNG_OK) return retval;
				free(lengths);
				
				// head on to huffman decoding
				self->indexBufferInflate = 0;
				self->indexReading = 0;
				self->doDecoding = true;
				self->state = STATE_DEFL_HUFFMAN_DECODE;
				break;
			
			case STATE_DEFL_HUFFMAN_DECODE:
				while (self->doDecoding) {
 					// prior to decoding: try to read from buffer
					// decoding might yield more bytes than needed, thus read
					// until buffer is exhausted (reading index reached front index)
					// or the requested number of bytes was read
					// note: since reading index is always behind buffer front, checking for equality suffices
					while (self->indexReading != self->indexBufferInflate) {
						self->scanlineCurrent[numBytes-numBytesToRead] = self->bufferInflate[self->indexReading];
						self->indexReading = (self->indexReading + 1) & (self->sizeWindow-1);
						numBytesToRead--;
						if (numBytesToRead == 0) return RET_FAPNG_OK;
					}
					
					// retrieve length symbol
					retval = checkCode(self,self->sizeCodesHuffmanLength,self->codesHuffmanLength);
					if (retval != RET_FAPNG_OK) return retval;
					if  (self->valueBufferBits < 256) {
						// literal symbol found: just append symbol to buffer
						// wrap around index if reaching end of buffer (ring buffer)
						self->bufferInflate[self->indexBufferInflate] = self->valueBufferBits;
						self->indexBufferInflate = (self->indexBufferInflate + 1) & (self->sizeWindow-1);
					}
					else if (self->valueBufferBits == 256) {
						// stop symbol found: break from loop
						self->doDecoding = false;
					}
					else {
						// length symbol found
						// step 1: decode length
						length = self->valueBufferBits;
						if (length < 265) {
							// length symbol 257..264 represent length 3..10 without extra distance bits
							length -= 254;
						} else if (length < 285) {
							// code      bits    length
							// -----------------------------
							// 265-268   1       11,13,15,17
							// 269-272   2       19,23,27,31
							// 273-276   3       35,43,51,59
							// 277-280   4       67,83,99,115
							// 281-284   5       131,163,195,227
							//
							// => extraBits = (symbol-261) / 4
							// => length = (1<<(extraBits+2)) + 3 + ((symbol-261) & 3)*(1<<(extraBits))
							length -= 261;
							extraBits = length / 4;
							length = (1<<(extraBits+2)) + 3 + (length & 3)*(1<<(extraBits));
							retval = readBitsIDAT(self,extraBits);
							if (retval != RET_FAPNG_OK) return retval;
							length += self->valueBufferBits;
						} else if (length == 285) {
							length = 258;
						} else return RET_FAPNG_INVALID_LENGTH_CODE;
						
						// step 2: decode distance
						retval = checkCode(self,self->sizeCodesHuffmanDistance,self->codesHuffmanDistance);
						if (retval != RET_FAPNG_OK) return retval;
						distance = self->valueBufferBits;
						if (distance < 4) {
							// first four codes + 1 => distance
							distance++;
						} else if (distance < 30) {
							// code   bits  distance
							// ------------------------
							// 4,5    1     5,7
							// 6,7    2     9,13
							// 8,9    3     17,25
							// 10,11  4     33,49
							// 12,13  5     65,97
							// 14,15  6     129,193
							// 16,17  7     257,385
							// 18,19  8     513,769
							// 20,21  9     1025,1537
							// 22,23  10    2049,3073
							// 24,25  11    4097,6145
							// 26,27  12    8193,12289
							// 28,29  13    16385,24577
							//
							// => extraBits = (symbol-2) / 2
							// => distance = (1<<(extraBits+1))+1 + ((symbol-2) & 1)*(1<<extraBits)
							distance -= 2;
							extraBits = distance / 2;
							distance = (1<<(extraBits+1)) + 1 + (distance & 1)*(1<<extraBits);
							retval = readBitsIDAT(self,extraBits);
							if (retval != RET_FAPNG_OK) return retval;
							distance += self->valueBufferBits;
						} else return RET_FAPNG_INVALID_DISTANCE_CODE;
						
						// step 3: go back distance and copy length bytes to buffer front
						// since it's a ring buffer, check crossing lower boundary (index==0)
						if (self->indexBufferInflate >= distance) {
							// subtraction would not violate lower boundary, proceed
							indexCopy = self->indexBufferInflate - distance;
						} else {
							// subtract from end and add current index
							indexCopy = self->sizeWindow - distance + self->indexBufferInflate;
						}
						while (length > 0) {
							self->bufferInflate[self->indexBufferInflate] = self->bufferInflate[indexCopy];
							self->indexBufferInflate = (self->indexBufferInflate + 1) & (self->sizeWindow-1);
							indexCopy = (indexCopy + 1) & (self->sizeWindow-1);
							length--;
						}
					}
				}
				self->state = STATE_DEFL_END;
				break;
			
			case STATE_DEFL_NO_COMPRESSION:
				retval = readBytesIDAT(self,buffer32,4);
				if (retval != RET_FAPNG_OK) return retval;
				
				// buffer holds LEN and NLEN (1s complement of LEN) in little endian order
				// calculate values and compare 1s complement
				// if lengths fit, head on to reading uncompressed bytes
				self->sizeWindow = LITTLEENDIAN16(buffer32);
				if (self->sizeWindow != (uint16_t)~LITTLEENDIAN16(&buffer32[2])) return RET_FAPNG_NOCOMP_LEN;
				self->state = STATE_DEFL_NO_COMPRESSION_READ;
				break;
			
			case STATE_DEFL_NO_COMPRESSION_READ:
				// read uncompressed bytes
				if (self->sizeWindow >= numBytesToRead) {
					// requested number of bytes fits into
					// currently available number of uncompressed bytes 
					retval = readBytesIDAT(self,&self->scanlineCurrent[numBytes - numBytesToRead],numBytesToRead);
					if (retval != RET_FAPNG_OK) return retval;
					self->sizeWindow -= numBytesToRead;
					numBytesToRead = 0;
					return RET_FAPNG_OK;
				} else {
					// read as many bytes as possible and terminate block
					// it should follow another block with the remaining bytes
					// see state STATE_DEFL_END --> STATE_DEFL_BEGIN
					retval = readBytesIDAT(self,&self->scanlineCurrent[numBytes - numBytesToRead],self->sizeWindow);
					if (retval != RET_FAPNG_OK) return retval;
					numBytesToRead -= self->sizeWindow;
					self->sizeWindow = 0;
					self->state = STATE_DEFL_END;
				}
				break;
		}
	}
	return RET_FAPNG_OK;
}

//------------------------------------------------------------------------------
// PNG reading function
//------------------------------------------------------------------------------

// paeth predictor function, used by filter type PAETH
uint16_t PaethPredictor(uint16_t a, uint16_t b, uint16_t c) {
	int16_t p = a + b - c;
	uint16_t pa = ABS(p - a);
	uint16_t pb = ABS(p - b);
	uint16_t pc = ABS(p - c);
	if (pa <= pb && pa <= pc) return a;
	if (pb <= pc) return b;
	return c;
}

// central PNG reading function
int8_t pngDataRead(PngData *self, char *filename, Surface *image) {
	if (image == NULL) return RET_FAPNG_MALLOC_IMAGE;
	
	// local vars
	int8_t   retval;
	uint8_t  magicBytes[8] = {0};
	
	// open file
	self->file = epic_file_open(filename,"rb");
	if (self->file < 0) return RET_FAPNG_OPEN;
	
	// read and check first eight magic bytes
	// should be 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a
 	if (epic_file_read(self->file,magicBytes,8) != 8) return RET_FAPNG_READ;
	if (magicBytes[0] != 0x89 || magicBytes[1] != 0x50 || \
		magicBytes[2] != 0x4e || magicBytes[3] != 0x47 || \
		magicBytes[4] != 0x0d || magicBytes[5] != 0x0a || \
		magicBytes[6] != 0x1a || magicBytes[7] != 0x0a) return RET_FAPNG_MAGIC;
	
	// read first chunk; has to be IHDR, otherwise PNG file is flawed
	retval = readChunkHeader(self);
	if (retval != RET_FAPNG_OK) return retval;
	if (self->typeChunk != CHUNK_IHDR) return RET_FAPNG_HEADER;
	
	// parse header:
	//  - first reading pass: 4 byte width, 4 byte height,
	//  - second reading pass: 1 byte each for bit depth, colour type, compression method, filter method, interlace method
	if (epic_file_read(self->file,magicBytes,8) != 8) return RET_FAPNG_READ;
	self->lenChunk -= 8;
	uint32_t tmp = (uint32_t)BIGENDIAN32(&magicBytes[0]);
	if (tmp == 0 || tmp > 255) return RET_FAPNG_DIMENSIONS;
	image->width = (uint8_t)tmp;
	tmp = (uint32_t)BIGENDIAN32(&magicBytes[4]);
	if (tmp == 0 || tmp > 255) return RET_FAPNG_DIMENSIONS;
	image->height = (uint8_t)tmp;
	
	if (epic_file_read(self->file,magicBytes,5) != 5) return RET_FAPNG_READ;
	self->lenChunk -= 5;
	
	// magicBytes[0] = bit depth (1,2,4,8,16)
	// magicBytes[1] = colour type (0,2,3,4,6)
	// magicBytes[2] = compression method, must be 0
	// magicBytes[3] = filter method, must be 0
	// magicBytes[4] = interlace method, either 0=none or 1=Adam7
	if (magicBytes[2]) return RET_FAPNG_COMPRESSION_METHOD;
	if (magicBytes[3]) return RET_FAPNG_FILTER_METHOD;
	if (magicBytes[4] > 1) return RET_FAPNG_INTERLACE_METHOD;
	uint8_t bitDepth = magicBytes[0];
	uint8_t pass = magicBytes[4];
	uint8_t bytesPerPixel;
	uint8_t samplesPerPixel;
	uint16_t k;
	
	switch (magicBytes[1]) {
		// process colour type + bit depth combinations
		case COLOURTYPE__GREY:
			samplesPerPixel = 1;
			bytesPerPixel = 1;
			switch (bitDepth) {
				case 1:
					self->funPixConv = convertPixelGrey1;
					break;
				case 2:
					self->funPixConv = convertPixelGrey2;
					break;
				case 4:
					self->funPixConv = convertPixelGrey4;
					break;
				case 8:
					self->funPixConv = convertPixelGrey8;
					break;
				case 16:
					self->funPixConv = convertPixelGrey16;
					break;
				default:
					return RET_FAPNG_BIT_DEPTH;
			}
			break;
		case COLOURTYPE__RGB:
			samplesPerPixel = 3;
			switch (bitDepth) {
				case 8:
					self->funPixConv = convertPixelRGB8;
					bytesPerPixel = 3;
					break;
				case 16:
					self->funPixConv = convertPixelRGB16;
					bytesPerPixel = 6;
					break;
				default:
					return RET_FAPNG_BIT_DEPTH;
			}
			break;
		case COLOURTYPE__INDEXED:
			samplesPerPixel = 1;
			bytesPerPixel = 1;
			switch (bitDepth) {
				case 1:
					self->funPixConv = convertPixelIndexed1;
					break;
				case 2:
					self->funPixConv = convertPixelIndexed2;
					break;
				case 4:
					self->funPixConv = convertPixelIndexed4;
					break;
				case 8:
					self->funPixConv = convertPixelIndexed8;
					break;
				default:
					return RET_FAPNG_BIT_DEPTH;
			}
			// look for PLTE chunk; has to appear before first IDAT chunk
			retval = seekChunk(self,CHUNK_PLTE);
			if (retval != RET_FAPNG_OK) return retval;
			// note: seekChunk/readChunkHeader have already ensured correct PLTE length
			self->sizePalette = (uint8_t)((self->lenChunk / 3) - 1); // min 1, max 256, fitting into one byte (0..255)
			self->palette = (uint16_t*)malloc(sizeof(uint16_t) * self->sizePalette + 2); // +2: account for normalised size
			if (self->palette == NULL) return RET_FAPNG_MALLOC_PALETTE;
			for (k=0; k <= self->sizePalette; k++) {
				// read palette entries from chunk; the checks above ensure
				// that the chunk holds enough bytes --> no lenChunk checking
				if (epic_file_read(self->file,magicBytes,3) != 3) return RET_FAPNG_READ;
				self->lenChunk -= 3;
				self->palette[k] = RGB565(magicBytes[0],magicBytes[1],magicBytes[2]);
			}
			break;
		case COLOURTYPE__GREY_A:
			samplesPerPixel = 2;
			switch (bitDepth) {
				case 8:
					self->funPixConv = convertPixelGreyA8;
					bytesPerPixel = 2;
					break;
				case 16:
					self->funPixConv = convertPixelGreyA16;
					bytesPerPixel = 4;
					break;
				default:
					return RET_FAPNG_BIT_DEPTH;
			}
			break;
		case COLOURTYPE__RGB_A:
			samplesPerPixel = 4;
			switch (bitDepth) {
				case 8:
					self->funPixConv = convertPixelRGBA8;
					bytesPerPixel = 4;
					break;
				case 16:
					self->funPixConv = convertPixelRGBA16;
					bytesPerPixel = 8;
					break;
				default:
					return RET_FAPNG_BIT_DEPTH;
			}
			break;
		default:
			return RET_FAPNG_COLOUR_TYPE;
	}
	// memorise bit depth and calculate worst-case scanline buffer size
	uint16_t sizeScanline = SCANLINEBYTES(image->width,samplesPerPixel,bitDepth);
	
	// look for first IDAT chunk
	retval = seekChunk(self,CHUNK_IDAT);
	if (retval != RET_FAPNG_OK) return retval;
	
	// no image data yet: allocate memory for the image, with 16-bit pixels and one 8-bit alpha channel
	if (image->rgb565 != NULL) free(image->rgb565);
	image->rgb565 = (uint16_t*)malloc((image->width * image->height) << 1);
	if (image->rgb565 == NULL) return RET_FAPNG_MALLOC_IMAGE;
	
	if (image->alpha != NULL) free(image->alpha);
	image->alpha = (uint8_t*)malloc(image->width * image->height);
	if (image->alpha == NULL) return RET_FAPNG_MALLOC_IMAGE;
	
	// allocate scanline buffers (largest dimension, kind of memory pool)
	// w*spp*bpp --> bits --> bytes + 1 filter type byte
	if (self->scanlineCurrent != NULL) free(self->scanlineCurrent);
	self->scanlineCurrent = NULL;
	if (self->scanlinePrevious != NULL) free(self->scanlinePrevious);
	self->scanlinePrevious = NULL;
	
	self->scanlineCurrent = (uint8_t*)malloc(sizeScanline);
	if (self->scanlineCurrent == NULL) return RET_FAPNG_MALLOC_SCANLINE;
	
	self->scanlinePrevious = (uint8_t*)malloc(sizeScanline);
	if (self->scanlinePrevious == NULL) {
		free(self->scanlineCurrent);
		self->scanlineCurrent = NULL;
		return RET_FAPNG_MALLOC_SCANLINE;
	}
	
	// deal with interlacing (stored in magicBytes[4])
	// interlace method = 0: no interlacing, thus pass=0, startNewPass sets current dimensions to image dimensions
	// interlace method = 1: ADAM7 interlacing, thus pass=1, startNewPass sets current dimensions to first pass dimensions
	// current dimensions (of subimage, if interlaced) determine processed scanline width
	uint8_t x = 0;
	uint8_t x0 = 0;
	uint8_t y = 0;
	uint8_t dx = 1;
	uint8_t dy = 1;
	uint8_t widthCurrent = image->width;
	uint16_t pixX,pixA,pixB,pixC;
	uint16_t sizeScanlineCurrent;
	uint16_t indexImage;
	uint8_t *tmpPtr;
	RGBA5658 colour;
	
	do {
		// for every pass...
		// calculate new dimensions, clear scanlinePrevious
		switch (pass) {
			case 0:
				x0 = 0;
				y  = 0;
				dx = 1;
				dy = 1;
				widthCurrent = image->width;
				break;
			case 1:
				x0 = 0;
				y  = 0;
				dx = 8;
				dy = 8;
				widthCurrent = ((image->width - 1) >> 3) + 1;
				break;
			case 2:
				x0 = 4;
				y  = 0;
				dx = 8;
				dy = 8;
				widthCurrent = ((image->width - 5) >> 3) + 1;
				break;
			case 3:
				x0 = 0;
				y  = 4;
				dx = 4;
				dy = 8;
				widthCurrent = ((image->width - 1) >> 2) + 1;
				break;
			case 4:
				x0 = 2;
				y  = 0;
				dx = 4;
				dy = 4;
				widthCurrent = ((image->width - 3) >> 2) + 1;
				break;
			case 5:
				x0 = 0;
				y  = 2;
				dx = 2;
				dy = 4;
				widthCurrent = ((image->width - 1) >> 1) + 1;
				break;
			case 6:
				x0 = 1;
				y  = 0;
				dx = 2;
				dy = 2;
				widthCurrent = ((image->width - 2) >> 1) + 1;
				break;
			case 7:
				x0 = 0;
				y  = 1;
				dx = 1;
				dy = 2;
				widthCurrent = image->width;
				break;
		}
// 		widthCurrent = (image->width - 1 - x0) / dx +1;
		
		// calculate scanline buffer length for given dimensions and clear previous scanline
		sizeScanlineCurrent = SCANLINEBYTES(widthCurrent,samplesPerPixel,bitDepth);
		for (k = 0; k < sizeScanlineCurrent; k++) self->scanlinePrevious[x] = 0;
		
		// (y was already initialised)
		for (; y < image->height; y += dy) {
			// for every image row in the current subimage...
			// 1) request bytes needed for the current pass and fill scanlineCurrent
			retval = readScanline(self,sizeScanlineCurrent);
			if (retval != RET_FAPNG_OK) return retval;
			
			// 2) apply filter type (byte0) to all bytes in scanlineCurrent
			switch (self->scanlineCurrent[0]) {
				case FILTER_SUB:
					for (k = 1; k < sizeScanlineCurrent; k++) {
						pixX = (uint16_t)self->scanlineCurrent[k];
						pixA = (k - bytesPerPixel > 0)? (uint16_t)self->scanlineCurrent[k - bytesPerPixel] : 0;
						self->scanlineCurrent[k] = (uint8_t)(pixX + pixA);
					}
					break;
				case FILTER_UP:
					for (k = 1; k < sizeScanlineCurrent; k++) {
						pixX = (uint16_t)self->scanlineCurrent[k];
						pixB = (uint16_t)self->scanlinePrevious[k];
						self->scanlineCurrent[k] = (uint8_t)((pixX + pixB) & 0xff);
					}
					break;
				case FILTER_AVG:
					for (k = 1; k < sizeScanlineCurrent; k++){
						pixX = (uint16_t)self->scanlineCurrent[k];
						pixA = (k - bytesPerPixel > 0)? (uint16_t)self->scanlineCurrent[k - bytesPerPixel] : 0;
						pixB = (uint16_t)self->scanlinePrevious[k];
						self->scanlineCurrent[k] = (uint8_t)((pixX + ((pixA+pixB)>>1)) & 0xff);
					}
					break;
				case FILTER_PAETH:
					for (k = 1; k < sizeScanlineCurrent; k++) {
						pixX = (uint16_t)self->scanlineCurrent[k];
						if (k - bytesPerPixel > 0) {
							pixA = (uint16_t)self->scanlineCurrent[k - bytesPerPixel];
							pixC = (uint16_t)self->scanlinePrevious[k - bytesPerPixel];
						} else {
							pixA = 0;
							pixC = 0;
						}
						pixB = (uint16_t)self->scanlinePrevious[k];
						self->scanlineCurrent[k] = (uint8_t)((pixX + PaethPredictor(pixA,pixB,pixC)) & 0xff);
					}
					break;
				case FILTER_NONE:
					break;
				default:
					return RET_FAPNG_FILTER_TYPE;
			}
			
			// 3) convert scanline bytes to pixels
			indexImage = y * image->width + x0;
			for (x = 0; x < widthCurrent; x++) {
				colour = self->funPixConv(self,x);
				image->rgb565[indexImage] = colour.rgb565;
				image->alpha[indexImage] = colour.alpha; 
				indexImage += dx;
			}
			// 4) swap scanline buffers
			tmpPtr = self->scanlinePrevious;
			self->scanlinePrevious = self->scanlineCurrent;
			self->scanlineCurrent = tmpPtr;
		}
		pass = (pass > 0) ? pass + 1 : 8;
	} while (pass < 8);
	
	return RET_FAPNG_OK;
}

//------------------------------------------------------------------------------
// Surface construction via image loading
//------------------------------------------------------------------------------

Surface *pngDataLoad(char *filename) {
	Surface *image = surfaceConstruct();
	if (image == NULL) return NULL;
	
	PngData *data = pngDataConstruct();
	if (data == NULL) return NULL;
	
	int retval = pngDataRead(data,filename,image);
	pngDataDestruct(&data);
	if (retval == RET_FAPNG_OK) {
		return image;
	} else {
		surfaceDestruct(&image);
		return NULL;
	}
}

