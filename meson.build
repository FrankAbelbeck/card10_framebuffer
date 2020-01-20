# Copyright (C) 2020 Frank Abelbeck <frank.abelbeck@googlemail.com>
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

name = 'surfacedemo'

elf = executable(
   name + '.elf',
   'surfacedemo.c',
   'faSurfaceBase.h',
   'faSurfaceBase.c',
   'faSurface.h',
   'faSurface.c',
   'faSurfacePP.h',
   'faSurfacePP.c',
   'faFramebuffer.h',
   'faFramebuffer.c',
   'faReadPng.h',
   'faReadPng.c',
   build_by_default: true,
   dependencies: [l0dable_startup, api_caller],
   link_whole: [l0dable_startup_lib],
   link_args: [
      '-Wl,-Map=' + meson.current_build_dir() + '/' + name + '.map',
      '-lgcc'
   ],
   pie: true,
)
