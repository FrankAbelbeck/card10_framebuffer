==========================
Card10 Framebuffer Library
==========================

Preamble
========

Copyright (C) 2020 Frank Abelbeck <frank.abelbeck@googlemail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.


Description
===========

Collection of routines for manipulation of a card10 badge framebuffer.


General l0dable Installation Routine
====================================

1) Get the card10 repo and set-up the toolchain (depends on your OS).
   Let's assume the local firmware repo lies at "$FIRMWARE"
2) Create a new directory PROGNAME below "$FIRMWARE/l0dables".
3) Copy the specified files to said directory "$FIRMWARE/l0dables/PROGNAME/".
   Copy "meson.PROGNAME.build" as "meson.build" to this directory.

4) Edit "meson.build" in the directory "$FIRMWARE/l0dables" and add the
   following line:

      subdir('PROGNAME/')

5) Compile the firmware; this will now build the newly included l0dable, too:

      cd $FIRMWARE
      ninja -C build/

6) Mount card10 badge (assuming mountpoint "$MEDIACARD10").

7) If not yet done: Create file "card10.cfg" in card10's root directory and add
   the following line to it:
      
      execute_elf = true

8) Copy "$FIRMWARE/build/l0dables/DIRNAME/PROGNAME.elf" to
   "$MEDIACARD10/apps". The App "PROGNAME.elf" should now be available
   in the card10 menu.


Installation of the surfacedemo l0dable
=======================================

Follow general installation instructions as stated above. Modify and extend them
as follows.

2) Use PROGNAME=surfacedemo

3) Files to copy:

   surfacedemo.c, faFramebuffer.c, faFramebuffer.h, faReadPng.c, faReadPng.h
   faSurfaceBase.c, faSurfaceBase.h, faSurface.c, faSurface.h,
   faSurfacePP.c, faSurfacePP.h
      
9) Create a directory "$MEDIACARD10/png/" (if not yet existent) and copy the
   following images to it:

      sprite-logo.png, sprite.png, stars.png, text.png, title.png


Installation of the triangledemo l0dable
========================================

Follow general installation instructions as stated above. Modify and extend them
as follows.

2) Use PROGNAME=triangledemo

3) Files to copy:

      triangledemo.c, faFramebuffer.c, faFramebuffer.h, faReadPng.c, faReadPng.h
      faSurfaceBase.c, faSurfaceBase.h, faSurface.c, faSurface.h
      
9) Create a directory "$MEDIACARD10/png/" (if not yet existent) and copy the
   following image to it:

      stars.png


Installation of the fontdemo l0dable
====================================

Follow general installation instructions as stated above. Modify and extend them
as follows.

2) Use PROGNAME=fontdemo

3) Files to copy:

      fontdemo.c, faFramebuffer.c, faFramebuffer.h,
      faSurfaceBase.c, faSurfaceBase.h, faSurface.c, faSurface.h,
      faReadPng.h, faReadPng.c, faFontFile.h, faFontFile.c

9) Create a directory "$MEDIACARD10/png/" (if not yet existent) and copy the
   following files to it:

      faTinyFont.bin, earthrise.png


Changelog
=========

2020-03-22
    release of fontdemo

2020-03-11
    revised toolchain for faFontFile handling: merged into one program faFFTool.py
    faFontFile format creation now done via "faFFTool.py create" (image-based symbol definition + text file code mapping)
    faFontFile format checking now done via "faFFTool.py check"
    faFontFile format rendering now possible via "faFFTool.py render"

2020-03-07
    bitmap font library reaches beta state
    
2020-03-06
    bitmap font library reaches alpha state

2020-02-17
    created triangledemo

2020-02-14
    implemented disc (untested)

2020-02-12
    implemented triangle and rectangle
    changed meaning of return value bounding box: now not clipped to surface (mask tracks modifications)
    simplified normalisation/denormalisation in integer math: shifts instead of divs and muls

2020-02-06
    changed SurfaceMod to an even simpler format because processing mask pixels took longer than simply redrawing the display :(

2020-02-05
    changed SurfaceMod to a simpler format
    reduced size of width/height to uint8_t (limiting images to 255*255, ok for card10's screen)

2020-02-03
    introduced SurfaceMod: a surface modification data structure for directed surface updates
    introduced first drawing functions for primitives (point, line, circle, arc)
    code refactoring: function naming now follows convention ObjectAction()
    
2020-01-20
    introduced boundingBoxSprite parameter; finally the crawling text works;
    preparing faSurface lib for release

2020-01-16
	solution works, without perspective projection (assuming last row of matrix is 0,0,1);
	starting work on general surface library (faSurfacePP)

2020-01-13
    try other way 'round: parameter "matrix" and "matrixInverse"
     - calculate surface bounding box with matrix
     - iteratore over surface coordinates of the bounding box
     - calculate p = A^-1 * p', i.e. get sprite coordinates and paint surface coordinates accordingly

2020-01-12:
    solution works, but shows non-interpolation pattern

2020-01-09
    move from "3 shears" to "general affine transformation", i.e. p' = A*p
    problem: interpolation; anti-aliasing/interpolation via blendFractional() does not work

2019-12-25
    surface library supports stretching and scaling (nearest-neighbour interpol)

2019-12-23
    surface library supports shearing, rotating, mirroring

2019-12-15
    moved image and pixel structures into separate faSurface library
    PNG images now always have an alpha channel
    created Surface management library faSurface
    created framebuffer management library faFramebuffer
    dug-out 80s papers from Porter, Duff, Paeth (image composition + rotation)
    
2019-12-01
    made pngdata and image dynamically allocated;
    checked heap memory usage with valgrind;
    git repo init;
    decision to develop a l0dable instead of a MicroPython module
    (use GPL3 license, avoid dealing with the MicroPython C layer)

2019-11-19
    readPng.c ready for release.
