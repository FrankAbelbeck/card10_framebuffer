#ifndef _FAFRAMEBUFFER_H
#define _FAFRAMEBUFFER_H
/**
 * @file
 * @author Frank Abelbeck <frank.abelbeck@googlemail.com>
 * @version 2020-01-16
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

#include "epicardium.h" // access to disp_framebuffer
#include "faSurface.h" // access to surface structures

/** Constructor: create a new framebuffer structure.
 * 
 * @param colour A 16-bit colour value (RGB565) with which to initialise the framebuffer area.
 * @returns A pointer to a framebuffer structure or NULL if something went wrong.
 */
union disp_framebuffer *constructFramebuffer(uint16_t colour);

/** Destructor: free any allocated memory of a framebuffer structure.
 * 
 * @param fb Pointer to a framebuffer structure.
 */
void destructFramebuffer(union disp_framebuffer **fb);

/** Clear framebuffer by setting all pixels to the given colour.
 * 
 * @param framebuffer Pointer to a framebuffer structure.
 * @param colour A 16-bit colour value (RGB565).
 */
void clearFramebuffer(union disp_framebuffer *framebuffer, uint16_t colour);

/** Copy a surface to the framebuffer, replacing the framebuffer contents.
 * Does nothing if surface dimensions don't match framebuffer dimensions.
 * 
 * Works in place, modifies given framebuffer structure!
 * 
 * @param framebuffer pointer to a framebuffer structure.
 * @param surface Pointer to a Surface.
 */
void copySurfaceToFramebuffer(Surface *surface, union disp_framebuffer *framebuffer);

/** Send the contents of a given framebuffer to the display.
 *
 * @param fb pointer to a framebuffer.
 * @returns either 0 (success) or EBUSY (display already locked).
 */
int redraw(union disp_framebuffer *fb);

#endif // _FAFRAMEBUFFER_H
