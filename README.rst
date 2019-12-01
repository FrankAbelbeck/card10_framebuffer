==========================
Card10 Framebuffer Library
==========================

Preamble
========

Copyright (C) 2019 Frank Abelbeck <frank.abelbeck@googlemail.com>

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

Changelog
=========

2019-12-01
    made pngdata and image dynamically allocated;
    checked heap memory usage with valgrind;
    git repo init;
    decision to develop a l0dable instead of a MicroPython module
    (use GPL3 license, avoid dealing with the MicroPython C layer)

2019-11-19
    readPng.c ready for release.
