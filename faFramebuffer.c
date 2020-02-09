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
 * Framebuffer management routines for the card10 badge.
 */

#include <stdlib.h> // uses: malloc(), free()
#include "epicardium.h" // access to disp_framebuffer
#include "faFramebuffer.h"
#include "faSurface.h" // access to surface structures

union disp_framebuffer *framebufferConstruct(uint16_t colour) {
	// allocate framebuffer memory
	union disp_framebuffer *fb = (union disp_framebuffer*)malloc(sizeof(union disp_framebuffer));
	framebufferClear(fb,colour);
	return fb;
}

void framebufferDestruct(union disp_framebuffer **fb) {
	free(*fb);
	*fb = NULL;
}

void framebufferClear(union disp_framebuffer *framebuffer, uint16_t colour) {
	if (framebuffer != NULL) {
		// fill framebuffer with given colour
		// note: framebuffer index addressing is reversed!
		uint32_t iFramebuffer = 2*DISP_WIDTH*DISP_HEIGHT-1;
		do {
			framebuffer->raw[iFramebuffer--] = colour & 0xff; // swap bytes
			framebuffer->raw[iFramebuffer--] = colour >> 8;   // (according to epicardium doc)
		} while (iFramebuffer > 1);
	}
}

void framebufferCopySurface(union disp_framebuffer *framebuffer, Surface *surface) {
	if (framebuffer == NULL || surface == NULL) return;
	uint32_t iSurface = 0;
	uint32_t iFramebuffer = ((DISP_HEIGHT * DISP_WIDTH) << 1) - 1;
	do {
		framebuffer->raw[iFramebuffer--] = surface->rgb565[iSurface] & 0xff;
		framebuffer->raw[iFramebuffer--] = surface->rgb565[iSurface] >> 8;
		iSurface++;
	} while (iFramebuffer > 1);
}

void framebufferUpdateFromSurface(union disp_framebuffer *framebuffer, Surface *surface, SurfaceMod *mask) {
	if (framebuffer == NULL || surface == NULL || mask == NULL || surface->width != DISP_WIDTH || surface->height != DISP_HEIGHT || DISP_HEIGHT > mask->height) return;
	
	uint32_t bitmask;
	uint8_t x;
	uint32_t iFramebuffer = ((DISP_WIDTH * DISP_HEIGHT) << 1) - 1;
	uint16_t iSurface = 0;
	for (uint8_t y = 0; y < surface->height; y++) {
		bitmask = mask->tile[y >> 3];
		if (bitmask == 0) {
			// empty bitmask: nothing to do for this and the next seven rows
			iSurface += surface->width << 3;
			iFramebuffer -= surface->width << 4;
			y += 7;
			continue;
		}
		for (x = 0; x < surface->width; x++) {
			if ((1 << (x >> 3)) & bitmask) {
				framebuffer->raw[iFramebuffer--] = surface->rgb565[iSurface] & 0xff;
				framebuffer->raw[iFramebuffer--] = surface->rgb565[iSurface] >> 8;
			} else {
				iFramebuffer -= 2;
			}
			iSurface++;
		}
	}
}

/* Send the contents of given framebuffer to the display and
 * returns either 0 on success or EBUSY (display already locked).
 */
int framebufferRedraw(union disp_framebuffer *fb) {
	// lock display
	int retval = epic_disp_open();
	if (retval != 0) return retval;
	
	// send framebuffer data to display
	retval = epic_disp_framebuffer(fb);
	
	// unlock display and return 
	epic_disp_close();
	return retval;
}
