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


Installation of the surfacedemo l0dable
=======================================

1) Get the card10 repo and set-up the toolchain (depends on your OS).
   Let's assume the local firmware repo lies at "$FIRMWARE"
2) Create a directory "surfacedemo" below "$FIRMWARE/l0dables".
3) Copy the following files to this directory "$FIRMWARE/l0dables/surfacedemo/":

      surfacedemo.c, faFramebuffer.c, faFramebuffer.h, faReadPng.c, faReadPng.h
      faSurfaceBase.c, faSurfaceBase.h, faSurface.c, faSurface.h,
      faSurfacePP.c, faSurfacePP.h, meson.build

4) Edit "meson.build" in the directory "$FIRMWARE/l0dables" and add the
   following line:

      subdir('surfacedemo/')

5) Compile the firmware; this will now build surfacedemo, too:

      cd $FIRMWARE
      ninja -C build/

6) Mount the card10 (assuming mountpoint "$MEDIACARD10").
6.1) Create a directory "$MEDIACARD10/png/".
6.2) Copy "$FIRMWARE/build/l0dables/surfacedemo/surfacedemo.elf" to 
     ""$MEDIACARD10/apps".
6.3) Copy the images to "$MEDIACARD10/png/":

      sprite-logo.png, sprite.png, stars.png, text.png, title.png

6.4) "surfacedemo.elf" should now be available in the card10 menu.


Changelog
=========

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
