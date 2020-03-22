#!/usr/bin/env python3
"""
@file
@author Frank Abelbeck <frank.abelbeck@googlemail.com>
@version 2020-03-11


@section License

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <http://www.gnu.org/licenses/>.

@section Description

Frank Abelbeck Font File (faFF) tool chain: create/modify faFontFiles
"""

import argparse,sys,os.path,PIL.Image,struct,pprint,PIL.ImageDraw

#-------------------------------------------------------------------------------
# constants
#-------------------------------------------------------------------------------

FNV1PRIME     = 0x01000193
FNV1OFFSET    = 0x811c9dc5
FNV1MASK      = 0x7fffffff # = 2**31-1, maximum value of int32_t
MAXUCODE      = 0x10ffff

DEFAULTSUFFIX   = "bin"
DEFAULTDISTANCE = 2
DEFAULTMARGIN   = 32

RANGEMIN = 0
RANGEMAX = 256

#-------------------------------------------------------------------------------
# helper functions
#-------------------------------------------------------------------------------

def hashFNV1(value,seed=FNV1OFFSET):
	# uses the Fowler-Noll-Vo (FNV-1) hash function
	# see https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
	if seed == 0: seed = FNV1OFFSET
	for valueByte in value:
		seed = ((seed * FNV1PRIME) & FNV1MASK) ^ valueByte
	return seed


def createMinimalPerfectHash(source):
	#
	# inspiration: http://stevehanov.ca/blog/?id=119
	#
	# 1) apply hashFNV1(key), put key in buckets
	# 2) sort buckets, criterion: number of keys in bucket
	# 3) process all buckets with more than one key:
	#    start with seed=1, try all keys in bucket with hashFNV1(key,seed) until all keys fit into empty slots
	# 4) process all buckets with just one key: place in remaining slots
	#
	nKeys = len(source) # number of keys to hash
	nCheck = 0 # checksum; should equal nKeys in the end
	nCollisions = 0 # statistics: count seed collisions
	
	buckets = [ [] for i in range(0,nKeys) ]
	G = [0] * nKeys    # intermediate table with the seed for the second hash
	V = [None] * nKeys # value table, initialised to None
	
	# step 1) calculate hashes for all keys, put in buckets
	for key in source.keys():
		buckets[hashFNV1(key) % nKeys].append(key)
	
	# step 2) sort buckets, criterion: number of keys in buckets, largest first
	buckets.sort(key=len, reverse=True)
	
	# step 3) process buckets until only buckets with single key are left
	while len(buckets) > 0 and len(buckets[0]) > 1:
		bucket = buckets.pop(0)
		seed = 1
		k = 0
		slots = []
		
		while k < len(bucket):
			slot = hashFNV1(bucket[k],seed) % nKeys
			if V[slot] is not None or slot in slots:
				# slot in value table already used or marked to be used:
				# start again, with new seed
				if seed >= FNV1MASK: raise ValueError("Hash seed exceeded 32 bit.")
				seed = seed + 1
				k = 0
				slots = []
				nCollisions = nCollisions + 1
			else:
				slots.append(slot)
				k = k + 1
		# found a matching seed: record it in G and spread bucket to V
		G[hashFNV1(bucket[0]) % nKeys] = seed
		for k,slot in enumerate(slots):
			V[slot] = (bucket[k],source[bucket[k]])
			nCheck = nCheck + 1
	
	# step 4) process buckets with single key: place in remaining slots
	#         mark this with a negative value in the intermediate table
	
	k = 0
	while len(buckets) > 0 and len(buckets[0]) > 0:
		bucket = buckets.pop(0)
		# iterate over V to find the next free slot
		while k < len(V):
			if V[k] is None:
				slot = k
				break
			else:
				k = k + 1
		if k >= len(V): raise IndexError("No free slots left.")
		# as this is a direct, one-hash-only mapping, mark the corresponding entry
		# in the intermediate table with the negative target index
		G[hashFNV1(bucket[0]) % nKeys] = -slot-1 # subtract one to ensure a negative value
		V[slot] = (bucket[0],source[bucket[0]])
		nCheck = nCheck + 1
	
	# finally, return lists G and V, defining a perfectly and minimally hashed hash table
	if (nKeys == nCheck):
		return G,V,nCollisions
	else:
		# checksum mismatch!
		raise ValueError("Mismatch between number of keys and number of processed keys.")


def lookUp(nKeys,G,V,key):
	g = G[hashFNV1(key) % nKeys]
	if g < 0:
		# direct look-up
		v = -g-1
	else:
		v = hashFNV1(key,g) % nKeys
	if (V[v][0] == key):
		return V[v][1]
	else:
		raise KeyError


def loadFile(args):
	# read the input file, build the font dictionary
	print("Reading input file {}...".format(args.infile.name))
	if args.infile.read(2) != b"\xfa\xff":
		raise ValueError("File signature not found.")
	
	# signature bytes fit, check dimensions
	width,height,nChars = struct.unpack(">BBI",args.infile.read(6))
	
	if width == 0: raise IndexError("Zero width detected.")
	if height == 0: raise IndexError("Zero height detected.")
	if nChars == 0: raise IndexError("Empty table detected.")
	
	print("symbol dimensions: {}x{} pixels".format(width,height))
	print("number of characters: {}".format(nChars))
	
	# read intermediate table G
	G = [ struct.unpack(">i",args.infile.read(4))[0] for i in range(0,nChars)]
	
	# read value table V
	sizeEntryV = width * ((height-1) // 8 + 1)
	strFormatV = ">3s{}s".format(sizeEntryV)
	sizeEntryV = sizeEntryV + 3
	V = [ struct.unpack(strFormatV,args.infile.read(sizeEntryV)) for i in range(0,nChars)]
	
	return width,height,nChars,G,V


def loadAndCheckFile(args):
	try:
		width,height,nChars,G,V = loadFile(args)
	except ValueError as e:
		print("File signature not found.")
		raise e
	except IndexError as e:
		print("Invalid header data: {}".format(e))
		raise e
	except struct.error as e:
		print("Invalid data encountered.")
		print("Reason: {}".format(e))
		raise e
	except Exception as e:
		print(e)
	
	if args.verbose:
		print("\nIntermediate table G:")
		pprint.pprint(G)
		print("\nValue table V:")
		pprint.pprint(V)
		print("")
	
	# check that the Unicode replacement character U+FFFD is defined
	try:
		lookUp(nChars,G,V,b"\x00\xff\xfd")
	except KeyError:
		print("Unicode replacement character at U+FFFD is not defined! Font definition is incomplete!")
		raise
	print("Unicode replacement character at U+FFFD is defined.")
	
	return width,height,nChars,G,V


def rangePixels(arg):
	try:
		value = int(arg)
	except (TypeError,ValueError):
		raise argparse.ArgumentTypeError("pixel argument is not an integer")
	if value < RANGEMIN or value > RANGEMAX:
		raise argparse.ArgumentTypeError("pixel value out of range [{},{}]".format(RANGEMIN,RANGEMAX))
	return value


#-------------------------------------------------------------------------------
# mode of operation: "create" --> calls createFile(), returns either 0 or -1
#-------------------------------------------------------------------------------

def createFile(args):
	# read the input file, build the font dictionary
	print("Reading input file {}...".format(args.infile.name))
	try:
		signature,dimensions = args.infile.readline().split(maxsplit=1)
		if signature != "faFF": raise ValueError("Wrong signature.")
		width,height = [int(i) for i in dimensions.split("x",1)]
		if width <= 0 or width >= 256: raise ValueError("Width not in range 1..255.")
		if height <= 0 or height >= 256: raise ValueError("Height not in range 1..255.")
	except (ValueError,TypeError) as e:
		print("Invalid input file: no info on width or height.")
		print("Reason: {}".format(e))
		print("No output written. Bye.")
		args.infile.close()
		return 1
	
	print("detected width {} and height {}.".format(width,height))
	
	sizeColumnWord = (height-1)//8+1
	fileroot = os.path.dirname(os.path.realpath(os.path.expanduser(args.infile.name)))
	dictFont = {}
	iCode = 0
	filename = None
	img = None
	for line in args.infile:
		line = line.strip() # remove any whitespace/line delimiter
		
		if filename is None and len(line) > 0:
			# start a new file block: get filename, process image file
			# get image filename: concat to fileroot, make absolute, prepare bytes list
			filename = os.path.realpath(os.path.join(fileroot,os.path.expanduser(line.strip())))
			iCode = 0
			try:
				img = PIL.Image.open(filename).convert("L")
				if img.width % width != 0 or img.height % height != 0:
					raise IndexError("Image dimensions not a multiple of the symbol dimensions.")
				widthTiles = img.width // width
				numTiles = widthTiles * img.height // height
			except (IndexError,OSError) as e:
				print("Invalid image file encountered.")
				print("Reason: {}".format(e))
				print("No output written. Bye.")
				try:
					img.close()
				except:
					pass
				args.infile.close()
				return 1
			
		elif len(line) > 0:
			# filename is set: inside a file block, process line
			# split along commas, convert to int; if not a number string: ignore (set 0)
			for strCode in line.split(","):
				try:
					uCode = int(strCode)
				except (TypeError,ValueError,OverflowError) as e:
					uCode = 0
				if uCode > 0:
					try:
						# valid code found; process it; iCode stores the current code index
						uCodeBytes = uCode.to_bytes(3,"big")
						dictFont[uCodeBytes] = b""
						tx = iCode % widthTiles
						ty = iCode // widthTiles
						for x in range(tx*width, (tx+1)*width):
							bitmask = 0
							for y in range(ty*height, (ty+1)*height):
								bitmask = bitmask | (bool(img.getpixel((x,y))) << (y % height))
							dictFont[uCodeBytes] = dictFont[uCodeBytes] + bitmask.to_bytes(sizeColumnWord,"little")
					except IndexError:
						print("{}: read beyond image (more character codes than tiles).".format(filename))
						print("No output written. Bye.")
						img.close()
						args.infile.close()
						return 1
				iCode = iCode + 1
				
		else:
			# empty line: terminate file block; try to close image file 
			filename = None
			try:    img.close()
			except: pass
	
	# loop exit: close file; close image since it might be still open when file block ends with EOF
	args.infile.close()
	try:    img.close()
	except: pass
	
	# all tiles mapped to character codes? issue warning 
	if (iCode != numTiles):
		print("Warning, there are tiles left that were not mapped to a code.")
	
	if (len(dictFont) > FNV1MASK):
		print("Sorry, this program only supports up to {} character definitions.".format(FNV1MASK))
		print("No output written. Bye.")
		return 1
	
	print("Creating minimal perfect hashing...")
	try:
		G,V,nCollisions = createMinimalPerfectHash(dictFont)
	except (IndexError,ValueError) as e:
		print("Sorry, could not find a minimal perfect hash function for that character set.")
		print("Reason: {}".format(e))
		print("No output written. Bye.")
		return 1
	print("Minimal perfect hash function found with {} re-seedings.".format(nCollisions))
	
	print("Checking that replacement character is defined...")
	try:
		lookUp(len(G),G,V,b"\x00\xff\xfd")
	except KeyError:
		print("Unicode replacement character at U+FFFD is not defined! Font definition is incomplete!")
		return 1
	print("Unicode replacement character at U+FFFD is defined.")
	
	# prepare output bytes
	output = bytearray()
	# first four bytes: signature (fa FF for faFontFile in hex) + char width + char height
	output.extend(b"\xfa\xff")
	output.extend((width).to_bytes(1,"big"))
	output.extend((height).to_bytes(1,"big"))
	# next four bytes: number of entries, uint32_t, big-endian
	output.extend((len(G)).to_bytes(4,"big"))
	# append G table as array of int32_t, big-endian
	
	for g in G:
		# in case of negative values: calculate 32 bit twos complement = 2*32 + value
		if g < 0: g = 0x100000000 + g
		output.extend(g.to_bytes(4,"big"))
	
	# append V table as array of uint8_t[3] + bitmap format depending on char dimensions
	for v in V:
		output.extend(v[0])
		output.extend(v[1])
	
	if args.outfile is None:
		args.outfile = args.infile.name.rsplit(".",1)[0] + "." + args.suffix
	
	try:
		with open(args.outfile,"wb") as outfile:
			print("Writing output file {}...".format(outfile.name))
			try:
				outfile.write(output)
			except OSError as e:
				print("Error opening file for writing.")
				raise e
				
	except OSError as e:
		print("Error opening file {} for writing.".format(args.outfile))
		print("Reason: {}".format(e))
		print("Output is invalid. Bye.")
		return 1
	
	print("File successfully written. Bye")
	return 0


#-------------------------------------------------------------------------------
# mode of operation: "check" --> calls checkFile(), returns either 0 or -1
#-------------------------------------------------------------------------------

def checkFile(args):
	try:
		width,height,nChars,G,V = loadAndCheckFile(args)
	except Exception as e:
		print("File check failed.")
		print(e)
		args.infile.close()
		return 1
	
	print("File check passed.")
	args.infile.close()
	return 0


#-------------------------------------------------------------------------------
# mode of operation: "render" --> calls renderString(), returns either 0 or -1
#-------------------------------------------------------------------------------

def renderString(args):
	# step 1: check input file
	try:
		width,height,nChars,G,V = loadAndCheckFile(args)
	except Exception as e:
		print("File check failed.")
		args.infile.close()
		return 1
	
	print("File check passed.")
	args.infile.close()
	
	# prepare image surface
	widthImage = width * len(args.text) + args.distance * (len(args.text)-1) + 2 * args.margin
	heightImage = height + 2 * args.margin
	xCursor = args.margin
	yCursor = args.margin
	sizeWord = (height-1)//8 + 1
	formatBits = ">{}s".format(sizeWord)
	
	print("Dimensions of rendered text (without margin): {}x{}".format(widthImage-2*args.margin,heightImage-2*args.margin))
	
	if args.blackOnWhite:
		img = PIL.Image.new("L",(widthImage,heightImage),color=255)
		colourFg = 0
	else:
		img = PIL.Image.new("L",(widthImage,heightImage),color=0)
		colourFg = 255
	
	# iterate over string, paint symbols
	for character in args.text:
		try:
			bitmap = lookUp(nChars,G,V,ord(character).to_bytes(3,"big"))
		except KeyError:
			bitmap = lookUp(nChars,G,V,b"\x00\xff\xfd")
		for x in range(0,width):
			for y in range(0,height):
				if (bitmap[x*sizeWord + (y >> 3)] & (1 << (y & 7))):
					img.putpixel((xCursor+x,yCursor+y),colourFg)
		
		xCursor = xCursor + width + args.distance
	
	img.show()
	return 0


#-------------------------------------------------------------------------------
# mode of operation: "info" --> calls printFileInfo(), returns 0
#-------------------------------------------------------------------------------

def printFileInfo(args):
	print("""
Input File Format
-----------------

When using the mode "create", the input file is assumed to be a text file
describing the character definitions in one or more image files.

File layout:
 - First line states "faFF WxH" with W = width and H = height in pixels.
 - What follows are one or more file blocks with the following layout:
    + First file block line: image filename
      the image defines m*n tiles of W*H pixels each.
    + one or more lines with comma-separated decimal character code values;
      all in all there must be m*n character code values.
    + An empty lines closes the file block.

Per default, the output file is created using the input file's name with the
last suffix being replaced by the value provided by the -s argument (or it's
default value {!r}). The user can provide a custom output filename via the
-o argument (which in turn overrides the -s argument).

faFontFile (faFF) Format
------------------------

The faFF defines access to bitmap font definitions. It supports character bitmap
definitions for the entire UTF-8 Unicode range of 0..0x10ffff. Each character
can be encoded in a bit matrix of dimensions up to 255x255 pixels.

The bitmap pattern for a given character code is retrieved via hash table.
The definitions for the minimal perfect hash function are part of the faFF.

The faFF uses big-endian format.

Structure of a faFF:
   byte 0..1: values 0xfa 0xff
   byte 2:    character symbol width, range [1,255] (uint8_t)
   byte 3:    character symbol height, range [1,255] (uint8_t)
   byte 4..7: number nChars of char definitions, range [1,0x10ffff] (uint32_t)
   bytes 8ff: data, consisting of intermediate table G and value table V

If width, height or nChars are outside the specified ranges, the file is
considered invalid. If the Unicode replacement character (U+FFFD) is not
included in the character table, the file is considered invalid, too.

Hashing is based on the Fowler-Noll-Vo hash function (example in Python3):

   FNV1PRIME  = 0x01000193
   FNV1OFFSET = 0x811c9dc5
   FNV1MASK   = 0x7fffffff # = 2**31-1, maximum value of int32_t
   
   def hashFNV1(value,seed=FNV1OFFSET):
      if seed == 0: seed = FNV1OFFSET
      for valueByte in value:
         seed = ((seed * FNV1PRIME) & FNV1MASK) ^ valueByte
      return seed

When looking up a character, the character code is hashed with the standard seed
of 0x811c9dc5 (32 bit FNV offset) modulo nChars. The result points to a position
in the intermediate table G.

If G[hashResult] is positive, the  entry defines a new seed for another FNV hash
operation. The resulting hash modulo nChars then points to the correct value
table entry. This resembles cuckoo hashing, but with a variable second function.

If G[hashResult] is negative, the entry points directly to a value table entry.
Just calculate -G[hashResult]-1 to get the correct position for value lookup.

Hashes can be ambiguous and can resolve codes not included in the
character table. To allow key matching after hash resolution, the code is stored
alongside the bitmap in the value table.

Each entry of the value table V is an array of uint8_t, starting with a
three-byte character code, followed by a byte sequence describing the bitmap.

The bitmap consists of a sequence of words, one for each column -- thus width
defines the number of words. Since each bit of a colum word represents one
pixel (least significant bit = row 0, most significant bit = row height-1),
height defines the number of bits. Since bits have to be rounded up to the next
octet, the symbol height defines the column word size as follows:

 - number of column words = width
 - size of column word in byte = (height-1) / 8 + 1

Unused bits should be set to zero. The bytes of a column word are ordered from
least to most significant byte, i.e. little-endian (allowing a simpler mapping
of y coordinate to bit position).

""".format(DEFAULTSUFFIX))
	return 0;


#-------------------------------------------------------------------------------
# main program
#-------------------------------------------------------------------------------

if __name__ == "__main__":
	# command line parsing: use subparser schema
	parser = argparse.ArgumentParser(
		description="Create or check faFontFiles."
	)
	subparsers = parser.add_subparsers(
		help="mode of operation; one of the following:",
		metavar="MODE"
	)
	
	# subparser create
	parser_create = subparsers.add_parser("create",
		description="Create a new faFontFile from a given text file (cf. 'info' for file details).",
		help="create a new faFontFile"
	)
	parser_create.add_argument("infile",
		metavar="FILE",
		type=argparse.FileType("r"),
		help="name of the input file"
	)
	parser_create.add_argument("-s",
		dest="suffix",
		metavar="SUFFIX",
		default=DEFAULTSUFFIX,
		help="suffix of the new file (default: {!r})".format(DEFAULTSUFFIX)
	)
	parser_create.add_argument("-o",
		dest="outfile",
		metavar="OUTFILE",
		help="name of the output file (default: use input name with new suffix); overrides -s argument"
	)
	parser_create.set_defaults(function=createFile)
	
	# subparser check
	parser_check  = subparsers.add_parser("check",
		description="Check if a given faFontFile is valid.",
		help="Check a given faFontFile."
	)
	parser_check.add_argument("infile",
		metavar="FILE",
		type=argparse.FileType("rb"),
		help="name of the input file"
	)
	parser_check.add_argument("-v",
		dest="verbose",
		action="store_true",
		help="print tables G and V"
	)
	parser_check.set_defaults(function=checkFile)
	
	# subparser render
	parser_render = subparsers.add_parser("render",
		description="Render a given string using the given faFontFile. "
"On Unix, the image is then opened using the 'display', 'eog' or 'xv' utility, "
"depending on which one can be found (cf. Pillow Documentation). Rendering implies file checking.",
		help="Render a string using a given faFontFile.")
	parser_render.add_argument("infile",
		metavar="FILE",
		type=argparse.FileType("rb"),
		help="name of the input file"
	)
	parser_render.add_argument("text",
		metavar="STRING",
		help="string to be rendered"
	)
	parser_render.add_argument("-v",
		dest="verbose",
		action="store_true",
		help="print tables G and V"
	)
	parser_render.add_argument("-m",
		dest="margin",
		type=rangePixels,
		default=DEFAULTMARGIN,
		help="margin of the text in pixels (default {}, range [{},{}])".format(DEFAULTMARGIN,RANGEMIN,RANGEMAX)
	)
	parser_render.add_argument("-d",
		dest="distance",
		type=rangePixels,
		default=DEFAULTDISTANCE,
		help="distance in pixels between characters (default {}, range [{},{}])".format(DEFAULTDISTANCE,RANGEMIN,RANGEMAX)
	)
	parser_render.add_argument("-b",
		dest="blackOnWhite",
		action="store_true",
		help="render black on white (default: white on black)"
	)
	parser_render.set_defaults(function=renderString)
	
	# subparser info
	parser_info   = subparsers.add_parser("info",
		description="Print out file format specification for both input and output formats.",
		help="Provide file format information."
	)
	parser_info.set_defaults(function=printFileInfo)
	
	# parse arguments and call the defined function
	args = parser.parse_args()
	try:
		sys.exit(args.function(args))
	except AttributeError as e:
		parser.print_help()
		sys.exit(0)

