/***
 * mcmap - create isometric maps of your minecraft alpha/beta world
 * v2.1a, 09-2011 by Zahl
 *
 * Copyright 2011, Zahl. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY ZAHL ''AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL ZAHL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#define NOMINMAX
#include "draw_png.h"
#include "colors.h"
#include "worldloader.h"
#include "globals.h"
#include "filesystem.h"
#include "json.hpp"

//PNGWriter
#include "BasicTiledPNGWriter.h"
#include "CachedTiledPNGWriter.h"

#include <string>
#include <vector>
#include <ctime>
#include <iostream>
#include <fstream>
#include <memory>
#include <algorithm> //std::min, std::max
#ifndef _WIN32
#include <sys/stat.h>
#endif

//determinate 64Bit or 32Bit
#if defined(_WIN32) || defined(_WIN64)
	#if defined(_WIN64)
		#define NUM_BITS 64
	#else
		#define NUM_BITS 32
	#endif
#elif defined __GNUC__
	#if defined(__x86_64__) || defined(__ppc64__)
		#define NUM_BITS 64
	#else
		#define NUM_BITS 32
	#endif
#else
	#error "Missing feature-test macro for 32/64-bit on this compiler."
#endif

namespace
{
	// For bright edge
	bool gAtBottomLeft = true, gAtBottomRight = true;
}

// Macros to make code more readable
#define BLOCK_AT_MAPEDGE(x,z) (((z)+1 == Global::MapsizeZ-CHUNKSIZE_Z && gAtBottomLeft) || ((x)+1 == Global::MapsizeX-CHUNKSIZE_X && gAtBottomRight))

void optimizeTerrain();
size_t optimizeTerrainMulti(const size_t startX, const size_t startZ);
void drawMapMulti(const size_t startX, const size_t endX, const int R1, const int R2, const std::vector<float>& brightnessLookup, PNGWriter* pngWriter);
void undergroundMode(bool explore);
bool prepareNextArea(int splitX, int splitZ, int &bitmapStartX, int &bitmapStartY);
void writeInfoFile(const std::string& file, int xo, int yo, int bitmapx, int bitmapy);
static const inline int floorChunkX(const int val);
static const inline int floorChunkZ(const int val);
void printHelp(const std::string& binary);

int main(int argc, char **argv)
{
	// ########## command line parsing ##########
	if (argc < 2) {
		printHelp(argv[0]);
		return 0;
	}
	bool wholeworld = false;
	std::string filename, outfile, tilePath, colorfile, blockfile, infoFile;
	bool infoOnly = false;
	double scaleImage = 1.0;

#if NUM_BITS == 32
	uint64_t memlimit = 1500 * uint64_t(1024 * 1024);
#else
	uint64_t memlimit = 2000 * uint64_t(1024 * 1024);
#endif

	bool memlimitSet = false;

	{ // -- New command line parsing --
#		define MOREARGS(x) (argpos + (x) < argc)
#		define NEXTARG argv[++argpos]
#		define POLLARG(x) argv[argpos + (x)]
		int argpos = 0;
		while (MOREARGS(1)) {
			const std::string option = NEXTARG;
			if (option == "-from") {
				if (!MOREARGS(2) || !isNumeric(POLLARG(1)) || !isNumeric(POLLARG(2))) {
					std::cerr << "Error: -from needs two integer arguments, ie: -from -10 5\n";
					return 1;
				}
				Global::FromChunkX = std::stoi(NEXTARG);
				Global::FromChunkZ = std::stoi(NEXTARG);
			} else if (option == "-to") {
				if (!MOREARGS(2) || !isNumeric(POLLARG(1)) || !isNumeric(POLLARG(2))) {
					std::cerr << "Error: -to needs two integer arguments, ie: -to -5 20\n";
					return 1;
				}
				Global::ToChunkX = std::stoi(NEXTARG) + 1;
				Global::ToChunkZ = std::stoi(NEXTARG) + 1;
			} else if (option == "-night") {
				Global::settings.nightmode = true;
			} else if ((option == "-cave") || (option == "-caves") || (option == "-underground") ) {
				Global::settings.underground = true;
			} else if ((option == "-blendcave") || (option == "-blendcaves")) {
				Global::settings.blendUnderground = true;
			} else if ((option == "-skylight")) {
				Global::settings.skylight = true;
			} else if ((option == "-nether") || (option == "-hell") || (option == "-slip")) {
				Global::settings.hell = true;
			} else if (option == "-end") {
				Global::settings.end = true;
			}
			else if (option == "-serverhell") {
				Global::settings.serverHell = true;
			}else if(option == "-connGrass") {
				Global::settings.connGrass = true;
			} else if (option == "-biomes") {
				std::cerr << "-biomes no longer supported\n";
			} else if (option == "-biomecolors") {
				std::cerr << "-biomecolors no longer supported\n";
			} else if (option == "-blendall") {
				Global::settings.blendAll = true;
			} else if (option == "-lowmemory") {
				std::cerr << "-lowmemory no longers supported\n";
			} else if ((option == "-noise") || (option == "-dither")) {
				if (!MOREARGS(1) || !isNumeric(POLLARG(1))) {
					std::cerr << "Error: " << option << " needs an integer argument, ie: " << option << " 10\n";
					return 1;
				}
				Global::settings.noise = std::stoi(NEXTARG);
			} else if ((option == "-height") || (option == "-max")) {
				if (!MOREARGS(1) || !isNumeric(POLLARG(1))) {
					std::cerr << "Error: " << option << " needs an integer argument, ie: " << option << " 100\n";
					return 1;
				}
				Global::MapsizeY = std::stoi(NEXTARG);
				if (option == "-max")  Global::MapsizeY++;
			} else if (option == "-min") {
				if (!MOREARGS(1) || !isNumeric(POLLARG(1))) {
					std::cerr << "Error: " << option << " needs an integer argument, ie: " << option << " 50\n";
					return 1;
				}
				Global::MapminY = std::stoi(NEXTARG);
			} else if (option == "-mem") {
				if (!MOREARGS(1) || !isNumeric(POLLARG(1)) || atoi(POLLARG(1)) <= 0) {
					std::cerr << "Error: " << option << " needs a positive integer argument, ie: " << option << " 1000\n";
					return 1;
				}
				memlimitSet = true;
				memlimit = size_t (std::stoi(NEXTARG)) * size_t (1024 * 1024);
			} else if (option == "-file") {
				if (!MOREARGS(1)) {
					std::cerr << "Error: -file needs one argument, ie: -file myworld.png\n";
					return 1;
				}
				outfile = NEXTARG;
			}
			else if (option == "-colors") {
				if (!MOREARGS(1)) {
					std::cerr << "Error: -colors needs one argument, ie: -colors colors.json\n";
					return 1;
				}
				colorfile = NEXTARG;
			}
			else if (option == "-blocks") {
				if (!MOREARGS(1)) {
					std::cerr << "Error: -blocks needs one argument, ie: -blocks BlockIds.json\n";
					return 1;
				}
				blockfile = NEXTARG;
			}else if(option == "-threads") {
				if (!MOREARGS(1) || !isNumeric(POLLARG(1)) || atoi(POLLARG(1)) <= 1) {
					std::cerr << "Error: " << option << " needs a positive integer argument, ie: " << option << " 4\n";
					return 1;
				}
				Global::threadPool = std::make_unique<ThreadPool>(atoi(NEXTARG));
			} else if (option == "-info") {
				if (!MOREARGS(1)) {
					std::cerr << "Error: -info needs one argument, ie: -info data.json\n";
					return 1;
				}
				infoFile = NEXTARG;
			} else if (option == "-infoonly") {
				infoOnly = true;
			} else if (option == "-north") {
				Global::settings.orientation = North;
			} else if (option == "-south") {
				Global::settings.orientation = South;
			} else if (option == "-east") {
				Global::settings.orientation = East;
			} else if (option == "-west") {
				Global::settings.orientation = West;
			} else if (option == "-split") {
				if (!MOREARGS(1)) {
					std::cerr << "Error: -split needs a path argument, ie: -split tiles/\n";
					return 1;
				}
				tilePath = NEXTARG;
			} else if ((option == "-help") || (option == "-h") || (option == "-?")) {
				printHelp(argv[0]);
				return 0;
			} else if (option == "-marker") {
				std::cerr << "Markers currently do not work!\n";
				if (!MOREARGS(3) || !isNumeric(POLLARG(2)) || !isNumeric(POLLARG(3))) {
					std::cerr << "Error: -marker needs a char and two integer arguments, ie: -marker r -15 240\n";
					return 1;
				}
				Marker marker;
				switch (*NEXTARG) {
				case 'r':
					marker.color = 253;
					break;
				case 'g':
					marker.color = 244;
					break;
				case 'b':
					marker.color = 242;
					break;
				default:
					marker.color = 35;
				}
				int x = atoi(NEXTARG);
				int z = atoi(NEXTARG);
				marker.chunkX = floorChunkX(x);
				marker.chunkZ = floorChunkZ(z);
				marker.offsetX = x - (marker.chunkX * CHUNKSIZE_X);
				marker.offsetZ = z - (marker.chunkZ * CHUNKSIZE_Z);
				Global::markers.push_back(marker);
			}
			else if (option == "-mystcraftage") {
				if (!MOREARGS(1)) {
					std::cerr << "Error: -mystcraftage needs an integer age number argument";
					return 1;
				}
				Global::mystCraftAge = atoi(NEXTARG);
			}else if(option == "-scale"){
				if (!MOREARGS(1) || !isNumeric(POLLARG(1))) {
					std::cerr << "Error: -scale needs a scale argument. eg. 50";
					return 1;
				}
				scaleImage = std::stod(NEXTARG) / 100.0;
				if (scaleImage > 1.0) {
					std::cerr << "Warning: you try to upscale the resulting image!\n";
				}
				else if (scaleImage <= 0.0) {
					std::cerr << "Error: -scale needs a postitive scale value > 0. eg. 50";
					return 1;
				}
			} else {
				filename = option;
			}
		}
		wholeworld = (Global::FromChunkX == UNDEFINED || Global::ToChunkX == UNDEFINED);
	}
	// ########## end of command line parsing ##########
	//if (Global::settings.hell || Global::settings.serverHell || Global::settings.end) Global::useBiomes = false;

	std::cout << "mcmap " << VERSION << ' ' << NUM_BITS << "bit by Zahl & mcmap3 by WRIM & 1.13 support by Philipp\n";

#if NUM_BITS == 32
	if (memlimit > 1800 * uint64_t(1024 * 1024)) {
		memlimit = 1800 * uint64_t(1024 * 1024);
	}
#endif

	if (!tilePath.empty() && scaleImage != 1.0) {
		std::cerr << "You can't scale output image, if using -split argument\n";
		scaleImage = 1.0;
	}

	// Load colors
	if (blockfile.empty()) {
		blockfile = "BlockIDs.json";
	}
	if (!loadBlockTree(blockfile)) {
		return 1;
	}

	if (colorfile.empty()) {
		colorfile = "colors.json";
	}
	if (!loadColorMap(colorfile)) {
		return 1;
	}

	if (filename.empty()) {
		std::cerr << "Error: No world given. Please add the path to your world to the command line.\n";
		return 1;
	}
	if (!isAlphaWorld(filename)) {
		std::cerr << "Error: Given path does not contain a Minecraft world.\n";
		return 1;
	}
	if (Global::settings.hell) {
		std::string tmp = filename + "/DIM-1";
		if (!Dir::dirExists(tmp)) {
			std::cerr << "Error: This world does not have a hell world yet. Build a portal first!\n";
			return 1;
		}
		filename = tmp;
	} else if (Global::settings.end) {
		std::string tmp = filename + "/DIM1";
		if (!Dir::dirExists(tmp)) {
			std::cerr << "Error: This world does not have an end-world yet. Find an ender portal first!\n";
			return 1;
		}
		filename = tmp;
	} else if (Global::mystCraftAge) {
		std::string tmp = filename + "/DIM_MYST" + std::to_string(Global::mystCraftAge);
		if (!Dir::dirExists(tmp)) {
			std::cerr << "Error: This world does not have Age " << Global::mystCraftAge  << "!\n";
			return 1;
		}
        filename = tmp;
    }

	// Figure out whether this is the old save format or McRegion or Anvil
	WorldFormat worldFormat = getWorldFormat(filename);
	if (!(worldFormat == ANVIL || worldFormat == ANVIL13)) {
		std::cerr << "World in old format, please convert to new anvil format first\n";
		return 1;
	}

	if (wholeworld && !scanWorldDirectory(filename)) {
		std::cerr << "Error accessing terrain at '" << filename << "'\n";
		return 1;
	}
	if (Global::ToChunkX <= Global::FromChunkX || Global::ToChunkZ <= Global::FromChunkZ) {
		std::cerr << "Nothing to render: -from X Z has to be <= -to X Z\n";
		return 1;
	}
	if (Global::MapsizeY - Global::MapminY < 1) {
		std::cerr << "Nothing to render: -min Y has to be < -max/-height Y\n";
		return 1;
	}
	Global::sectionMin = Global::MapminY >> SECTION_Y_SHIFT;
	Global::sectionMax = (Global::MapsizeY - 1) >> SECTION_Y_SHIFT;
	Global::MapsizeY -= Global::MapminY;
	std::cout << "MinY: " << Global::MapminY << " ... MaxY: " << Global::MapsizeY << " ... MinSecY: " << std::to_string(Global::sectionMin) << " ... MaxSecY: " << std::to_string(Global::sectionMax) << '\n';
	// Whole area to be rendered, in chunks
	// If -mem is omitted or high enough, this won't be needed
	Global::TotalFromChunkX = Global::FromChunkX;
	Global::TotalFromChunkZ = Global::FromChunkZ;
	Global::TotalToChunkX = Global::ToChunkX;
	Global::TotalToChunkZ = Global::ToChunkZ;
	// Don't allow ridiculously small values for big maps
	if (memlimit && memlimit < 200000000 && memlimit < size_t(Global::MapsizeX * Global::MapsizeZ * 150000)) {
		std::cerr << "Need at least " << int(float(Global::MapsizeX) * Global::MapsizeZ * .15f + 1) << " MiB of RAM to render a map of that size.\n";
		return 1;
	}

	// Mem check
	int bitmapX, bitmapY; //number of Pixels in the final image
	uint64_t bitmapBytes = calcImageSize(Global::ToChunkX - Global::FromChunkX, Global::ToChunkZ - Global::FromChunkZ, Global::MapsizeY, bitmapX, bitmapY, false);
	// Cropping
	int cropLeft = 0, cropRight = 0, cropTop = 0, cropBottom = 0;
	if (wholeworld) {
		calcBitmapOverdraw(cropLeft, cropRight, cropTop, cropBottom);
		bitmapX -= (cropLeft + cropRight);
		bitmapY -= (cropTop + cropBottom);
		bitmapBytes = uint64_t(bitmapX) * PNGWriter::BYTESPERPIXEL * uint64_t(bitmapY);
	}

	if (!infoFile.empty()) {
		writeInfoFile(infoFile,
				-cropLeft,
				-cropTop,
				bitmapX, bitmapY);
		infoFile.clear();
		if (infoOnly) return 0;
	}

	bool splitImage = false; //true if we need to split the image in multiple smaller images (memlimit)
	int numSplitsX = 0;
	int numSplitsZ = 0;
	if (memlimit && memlimit < bitmapBytes + calcTerrainSize(Global::ToChunkX - Global::FromChunkX, Global::ToChunkZ - Global::FromChunkZ)) {
		// If we'd need more mem than allowed, we have to render groups of chunks...
		if (memlimit < bitmapBytes + 220 * uint64_t(1024 * 1024)) {
			// Warn about using incremental rendering if user didn't set limit manually
			if (!memlimitSet && sizeof(size_t) > 4) {
				std::cerr << " ***** PLEASE NOTE *****\n"
				       << "mcmap is using disk cached rendering as it has a default memory limit\n"
				       << "of " << int(memlimit / (1024 * 1024)) << " MiB. If you want to use more memory to render (=faster) use\n"
				       << "the -mem switch followed by the amount of memory in MiB to use.\n"
				       << "Start mcmap without any arguments to get more help.\n";
			} else {
				std::cout << "Choosing disk caching strategy...\n";
			}
			// ...or even use disk caching
			splitImage = true;
		}
		// Split up map more and more, until the mem requirements are satisfied
		for (numSplitsX = 1, numSplitsZ = 2;;) {
			int subAreaX = ((Global::TotalToChunkX - Global::TotalFromChunkX) + (numSplitsX - 1)) / numSplitsX;
			int subAreaZ = ((Global::TotalToChunkZ - Global::TotalFromChunkZ) + (numSplitsZ - 1)) / numSplitsZ;
			int subBitmapX, subBitmapY;
			if (splitImage && calcImageSize(subAreaX, subAreaZ, Global::MapsizeY, subBitmapX, subBitmapY, true) + calcTerrainSize(subAreaX, subAreaZ) <= memlimit) {
				break; // Found a suitable partitioning
			} else if (!splitImage && bitmapBytes + calcTerrainSize(subAreaX, subAreaZ) <= memlimit) {
				break; // Found a suitable partitioning
			}
			//
			if (numSplitsZ > numSplitsX) {
				++numSplitsX;
			} else {
				++numSplitsZ;
			}
		}
	}

	// Always same random seed, as this is only used for block noise, which should give the same result for the same input every time
	srand(1337);

	if (outfile.empty()) {
		outfile = "output.png";
	}

	// open output file only if not doing the tiled output
	//std::fstream fileHandle;
	std::unique_ptr<PNGWriter> pngWriter;
	if (tilePath.empty()) {
		if (!splitImage) {
			pngWriter = std::make_unique<PNGWriter>();
			pngWriter->reserve(bitmapX, bitmapY);
		}
		else {
			pngWriter = std::make_unique<CachedPNGWriter>(bitmapX, bitmapY);
		}
	} else {
		if (!splitImage) {
			pngWriter = std::make_unique<BasicTiledPNGWriter>();
			pngWriter->reserve(bitmapX, bitmapY);
		}
		else {
			pngWriter = std::make_unique<CachedTiledPNGWriter>(bitmapX, bitmapY);
		}

		outfile = tilePath;
	}

	// Precompute brightness adjustment factor
	std::vector<float> brightnessLookup(Global::MapsizeY);
	for (size_t y = 0; y < brightnessLookup.size(); ++y) {
		brightnessLookup[y] = ((100.0f / (1.0f + exp(- (1.3f * (float(y) * std::min(Global::MapsizeY, 200U) / Global::MapsizeY) / 16.0f) + 6.0f))) - 91);   // thx Donkey Kong
	}

	// Now here's the loop rendering all the required parts of the image.
	// All the vars previously used to define bounds will be set on each loop,
	// to create something like a virtual window inside the map.
	for (;;) {

		int bitmapStartX = 3, bitmapStartY = 5;
		if (numSplitsX) { // virtual window is set here
			// Set current chunk bounds according to number of splits. returns true if everything has been rendered already
			if (prepareNextArea(numSplitsX, numSplitsZ, bitmapStartX, bitmapStartY)) {
				break;
			}
			// if image is split up, prepare memory block for next part
			if (splitImage) {
				bitmapStartX += 2;
				const int sizex = (Global::ToChunkX - Global::FromChunkX) * CHUNKSIZE_X * 2 + (Global::ToChunkZ - Global::FromChunkZ) * CHUNKSIZE_Z * 2;
				const int sizey = (int)Global::MapsizeY * Global::OffsetY + (Global::ToChunkX - Global::FromChunkX) * CHUNKSIZE_X + (Global::ToChunkZ - Global::FromChunkZ) * CHUNKSIZE_Z + 3;
				if (sizex <= 0 || sizey <= 0) continue; // Don't know if this is right, might also be that the size calulation is plain wrong
				
				CachedPNGWriter* cpngw = dynamic_cast<CachedPNGWriter*>(pngWriter.get());
				const auto ret = cpngw->addPart(bitmapStartX - cropLeft, bitmapStartY - cropTop, sizex, sizey);
				if (ret == -1) {
					std::cerr << "Error creating partial image to render.\n";
					return 1;
				}
				else if (ret == 1) {
					continue;
				}
			}
		}

		// More chunks are needed at the sides to get light and edge detection right at the edges
		// This makes code below a bit messy, as most of the time the surrounding chunks are ignored
		// By starting loops at CHUNKSIZE_X instead of 0.
		++Global::ToChunkX;
		++Global::ToChunkZ;
		--Global::FromChunkX;
		--Global::FromChunkZ;

		// For rotation, X and Z have to be swapped (East and West)
		if (Global::settings.orientation == North || Global::settings.orientation == South) {
			Global::MapsizeZ = (Global::ToChunkZ - Global::FromChunkZ) * CHUNKSIZE_Z;
			Global::MapsizeX = (Global::ToChunkX - Global::FromChunkX) * CHUNKSIZE_X;
		} else {
			Global::MapsizeX = (Global::ToChunkZ - Global::FromChunkZ) * CHUNKSIZE_Z;
			Global::MapsizeZ = (Global::ToChunkX - Global::FromChunkX) * CHUNKSIZE_X;
		}

		// Load world or part of world
		if (numSplitsX == 0 && wholeworld && !loadEntireTerrain()) {
			std::cerr << "Error loading terrain from '" << filename << "'\n";
			return 1;
		} else if (numSplitsX != 0 || !wholeworld) {
			int numberOfChunks;
			const bool result = loadTerrain(filename, numberOfChunks);

			if (splitImage && numberOfChunks == 0) {
				std::cout << "Section is empty, skipping...\n";
				CachedPNGWriter* cpngw = dynamic_cast<CachedPNGWriter*>(pngWriter.get());
				cpngw->discardPart();
				continue;
			}
			else if (numberOfChunks == 0 && numSplitsX != 0) {
				std::cout << "Section is empty, skipping...\n";
				continue;
			}
			else if (!result) {
				std::cerr << "Could not load Section\n";
			}
		}

		if (Global::settings.hell || Global::settings.serverHell) {
			uncoverNether();
		}

		// If underground mode, remove blocks that don't seem to belong to caves
		if (Global::settings.underground) {
			undergroundMode(false);
		}

		optimizeTerrain();

		// Finally, render terrain to file
		std::cout << "Drawing map...\n";
		if (Global::threadPool) {
			const size_t sizePerThread = (Global::MapsizeX - CHUNKSIZE_X - CHUNKSIZE_X) / Global::threadPool->size();
			if (((Global::MapsizeX - CHUNKSIZE_X - CHUNKSIZE_X) % Global::threadPool->size()) == 0) {
				std::cerr << "size does not fit\n";
			}

			std::vector<std::future<void>> results;
			for (size_t i = 0; i < Global::threadPool->size(); ++i) {
				const size_t startX = (i*sizePerThread) + CHUNKSIZE_X;
				const size_t stopX = startX + sizePerThread;
				results.emplace_back(Global::threadPool->enqueue(drawMapMulti, startX, stopX, (splitImage ? -2 : bitmapStartX - cropLeft), (splitImage ? 0 : bitmapStartY - cropTop), brightnessLookup, pngWriter.get()));
			}

			for (const auto& r : results) {
				r.wait();
			}

		}else {
			for (size_t x = CHUNKSIZE_X; x < Global::MapsizeX - CHUNKSIZE_X; ++x) { //iterate over all blocks, ignore outer Chunks
				printProgress(x - CHUNKSIZE_X, Global::MapsizeX);
				for (size_t z = CHUNKSIZE_Z; z < Global::MapsizeZ - CHUNKSIZE_Z; ++z) {
					const int bmpPosX = int((Global::MapsizeZ - z - CHUNKSIZE_Z) * 2 + (x - CHUNKSIZE_X) * 2 + (splitImage ? -2 : bitmapStartX - cropLeft));
					int bmpPosY = int(Global::MapsizeY * Global::OffsetY + z + x - CHUNKSIZE_Z - CHUNKSIZE_X + (splitImage ? 0 : bitmapStartY - cropTop)) + 2 - (HEIGHTAT(x, z) & 0xFF) * Global::OffsetY;
					const unsigned int max = (HEIGHTAT(x, z) & 0xFF00) >> 8;
					for (unsigned int y = int8_t(HEIGHTAT(x, z)); y < max; ++y) {
						bmpPosY -= Global::OffsetY;
						const uint16_t& c = BLOCKAT(x, y, z);
						if (c == AIR) {
							continue;
						}

						//float col = float(y) * .78f - 91;
						float brightnessAdjustment = brightnessLookup[y];
						if (Global::settings.blendUnderground) {
							brightnessAdjustment -= 168;
						}
						// we use light if...
						if (Global::settings.nightmode // nightmode is active, or
							|| (Global::settings.skylight // skylight is used and
								&& (!BLOCK_AT_MAPEDGE(x, z))  // block is not edge of map (or if it is, has non-opaque block above)
								)) {
							int l = GETLIGHTAT(x, y, z);  // find out how much light hits that block
							if (l == 0 && y + 1 == Global::MapsizeY) {
								l = (Global::settings.nightmode ? 3 : 15);   // quickfix: assume maximum strength at highest level
							}
							else {
								const bool up = y + 1 < Global::MapsizeY;
								if (x + 1 < Global::MapsizeX && (!up || BLOCKAT(x + 1, y + 1, z) == 0)) {
									l = std::max(l, GETLIGHTAT(x + 1, y, z));
									if (x + 2 < Global::MapsizeX) l = std::max(l, GETLIGHTAT(x + 2, y, z) - 1);
								}
								if (z + 1 < Global::MapsizeZ && (!up || BLOCKAT(x, y + 1, z + 1) == 0)) {
									l = std::max(l, GETLIGHTAT(x, y, z + 1));
									if (z + 2 < Global::MapsizeZ) l = std::max(l, GETLIGHTAT(x, y, z + 2) - 1);
								}
								if (up) l = std::max(l, GETLIGHTAT(x, y + 1, z));
								//if (y + 2 < Global::MapsizeY) l = MAX(l, GETLIGHTAT(x, y + 2, z) - 1);
							}
							if (!Global::settings.skylight) { // Night
								brightnessAdjustment -= (100 - l * 8);
							}
							else { // Day
								brightnessAdjustment -= (210 - l * 14);
							}
						}

						// Edge detection (this means where terrain goes 'down' and the side of the block is not visible)
						if (y != 0) {
							uint16_t &b = BLOCKAT(x - 1, y - 1, z - 1);
							if ((y + 1 < Global::MapsizeY)  // In bounds?
								&& BLOCKAT(x, y + 1, z) == AIR  // Only if block above is air
								&& BLOCKAT(x - 1, y + 1, z - 1) == AIR  // and block above and behind is air
								&& (b == AIR || b == c)   // block behind (from pov) this one is same type or air
								&& (BLOCKAT(x - 1, y, z) == AIR || BLOCKAT(x, y, z - 1) == AIR)) {   // block TL/TR from this one is air = edge
								brightnessAdjustment += 13;
							}
						}

						setPixel(bmpPosX, bmpPosY, c, brightnessAdjustment, pngWriter.get());
					}
				}
			}
		}

		printProgress(10, 10);
		// Bitmap creation complete
		// unless using....
		// Underground overlay mode
		if (Global::settings.blendUnderground && !Global::settings.underground) {
			// Load map data again, since block culling removed most of the blocks
			if (numSplitsX == 0 && wholeworld && !loadEntireTerrain()) {
				std::cerr << "Error loading terrain from '" << filename <<"'\n";
				return 1;
			} else if (numSplitsX != 0 || !wholeworld) {
				int i;
				if (!loadTerrain(filename, i)) {
					std::cerr << "Error loading terrain from '" << filename << "'\n";
					return 1;
				}
			}

			undergroundMode(true);
			optimizeTerrain();

			std::cout << "Creating cave overlay...\n";
			for (size_t x = CHUNKSIZE_X; x < Global::MapsizeX - CHUNKSIZE_X; ++x) {
				printProgress(x - CHUNKSIZE_X, Global::MapsizeX);
				for (size_t z = CHUNKSIZE_Z; z < Global::MapsizeZ - CHUNKSIZE_Z; ++z) {
					const size_t bmpPosX = (Global::MapsizeZ - z - CHUNKSIZE_Z) * 2 + (x - CHUNKSIZE_X) * 2 + (splitImage ? -2 : bitmapStartX) - cropLeft;
					size_t bmpPosY = Global::MapsizeY * Global::OffsetY + z + x - CHUNKSIZE_Z - CHUNKSIZE_X + (splitImage ? 0 : bitmapStartY) - cropTop;
					for (unsigned int y = 0; y < std::min(Global::MapsizeY, 64U); ++y) {
						const uint16_t &c = BLOCKAT(x, y, z);
						if (c != AIR) { // If block is not air (colors[c][3] != 0)
							blendPixel(bmpPosX, bmpPosY, c, float(y + 30) * .0048f, pngWriter.get());
						}
						bmpPosY -= Global::OffsetY;
					}
				}
			}
			printProgress(10, 10);
		} // End blend-underground
		// If disk caching is used, save part to disk
		if (splitImage) {
			if (tilePath.empty() && scaleImage != 1.0) {
				deallocateTerrain();
				pngWriter->resize(scaleImage);
			}

			if (!pngWriter->write("")) {
				std::cerr << "Error saving partially rendered image.\n";
				return 1;
			}
		}
		// No incremental rendering at all, so quit the loop
		if (numSplitsX == 0) {
			break;
		}
	}
	// Drawing complete, now either just save the image or compose it if disk caching was used
	deallocateTerrain();
	// Saving
	if (!splitImage) {
		if (tilePath.empty() && scaleImage != 1.0) {
			pngWriter->resize(scaleImage);
		}

		if (!pngWriter->write(outfile)) {
			return 1;
		}
	} else {
		CachedPNGWriter* cpngw = dynamic_cast<CachedPNGWriter*>(pngWriter.get());
		if (!cpngw->compose(outfile, scaleImage)) {
			std::cerr << "Aborted.\n";
			return 1;
		}
	}

	std::cout << "Job complete.\n";
	return 0;
}

void optimizeTerrain()
{
	std::cout << "Optimizing terrain...\n";
	const size_t maxX = Global::MapsizeX - CHUNKSIZE_X;
	const size_t maxZ = Global::MapsizeZ - CHUNKSIZE_Z;

	if (Global::threadPool) {
		std::vector<std::future<size_t>> results;
		size_t blocksRemoved = 0;

		for (size_t z = CHUNKSIZE_Z; z < maxZ; ++z) {
			results.emplace_back(Global::threadPool->enqueue(optimizeTerrainMulti, maxX - 1, z));
		}
		results.emplace_back(Global::threadPool->enqueue(optimizeTerrainMulti, maxX - 1, maxZ - 1));

		for (size_t x = CHUNKSIZE_X; x < maxX; ++x) {
			results.emplace_back(Global::threadPool->enqueue(optimizeTerrainMulti, x, maxZ-1));
		}

		printProgress(0, results.size());
		for (size_t i = 0; i < results.size(); ++i) {
			blocksRemoved += results[i].get();
			printProgress(i, results.size());
		}

		printProgress(10, 10);

		std::cout << "Removed " << blocksRemoved << " blocks\n";

		return;
	}

	size_t gBlocksRemoved = 0;
	const size_t modZ = maxZ * Global::MapsizeY;
	std::vector<bool> blocked(modZ, false);
	size_t offsetZ = 0, offsetY = 0, offsetGlobal = 0;
	for (size_t x = maxX - 1; x >= CHUNKSIZE_X; --x) {
		printProgress(maxX - (x + 1), maxX);
		offsetZ = offsetGlobal;
		for (size_t z = CHUNKSIZE_Z; z < maxZ; ++z) {
			size_t highest = 0, lowest = 0xFF; // remember lowest and highest block which are visible to limit the Y-for-loop later
			for (size_t y = 0; y < Global::MapsizeY; ++y) { // Go up
				const uint16_t block = BLOCKAT(x, y, z); // Get the block at that point

				const size_t oldIndex = ((y + offsetY) % Global::MapsizeY) + (offsetZ % modZ);
				//const size_t newIndex = fast_mod<size_t>(y + offsetY, Global::MapsizeY) + fast_mod(offsetZ, modZ);
				//newIndex should be faster, but not sure

				auto&& current = blocked[oldIndex];
				if (current) { // Block is hidden, remove
					if (block != AIR) {
						++gBlocksRemoved;
					}
				} else { // block is not hidden by another block
					const auto col = colorMap[block];
					if (block != AIR && lowest == 0xFF) { // if it's not air, this is the lowest block to draw
						lowest = y;
					}
					if (col.a == 255 && (col.blockType == 0 || col.blockType == 7)) { // Block is not hidden, do not remove, but mark spot as blocked for next iteration (orig. just checking for alpha value, need to remove white spots behind fences)
						current = true;
					}
					if (block != AIR) highest = y; // if it's not air, it's the new highest block encountered so far
				}
			}
			HEIGHTAT(x, z) = (((uint16_t)highest + 1) << 8) | (uint16_t)lowest; // cram them both into a 16bit int
			blocked[(offsetY % Global::MapsizeY) + (offsetZ % modZ)] = false;
			offsetZ += Global::MapsizeY;
		}
		for (size_t y = 0; y < Global::MapsizeY; ++y) {
			blocked[y + (offsetGlobal % modZ)] = false;
		}
		offsetGlobal += Global::MapsizeY;
		++offsetY;
	}
	printProgress(10, 10);
#ifdef _DEBUG
	std::cout << "Removed " << gBlocksRemoved << " blocks\n";
#endif
}

size_t optimizeTerrainMulti(const size_t startX, const size_t startZ) {
	size_t removedBlocks{ 0 };
	size_t numMoves{ 0 };
	std::vector<bool> blocked(Global::MapsizeY, false);
	size_t x = startX;
	size_t z = startZ;

	while (x >= CHUNKSIZE_X && z >= CHUNKSIZE_Z) {
		size_t highest = 0, lowest = 0xFF;
		for (size_t y = 0; y < Global::MapsizeY; ++y) { // Go up
			const uint16_t block = BLOCKAT(x, y, z);
			if (!blocked[(y + numMoves) % Global::MapsizeY]) {
				const auto col = colorMap[block];
				if (block != AIR && lowest == 0xFF) { // if it's not air, this is the lowest block to draw
					lowest = y;
				}
				if (col.a == 255 && (col.blockType == 0 || col.blockType == 7)) { // Block is not hidden, do not remove, but mark spot as blocked for next iteration (orig. just checking for alpha value, need to remove white spots behind fences)
					blocked[(y + numMoves) % Global::MapsizeY] = true;
				}
				if (block != AIR) highest = y;
			}
			else {
				if (block != AIR) {
					++removedBlocks;
				}
			}
		} //y-loop end
		HEIGHTAT(x, z) = (((uint16_t)highest + 1) << 8) | (uint16_t)lowest;
		blocked[numMoves%Global::MapsizeY] = false;
		numMoves += 1;
		x -= 1;
		z -= 1;
	}

	return removedBlocks;

}

void drawMapMulti(const size_t startX, const size_t endX, const int R1, const int R2, const std::vector<float>& brightnessLookup, PNGWriter* pngWriter)
{
	for (size_t x = startX; x < endX; ++x) { //iterate over all blocks, ignore outer Chunks
		printProgress(x - CHUNKSIZE_X, Global::MapsizeX);
		for (size_t z = CHUNKSIZE_Z; z < Global::MapsizeZ - CHUNKSIZE_Z; ++z) {
			const int bmpPosX = int((Global::MapsizeZ - z - CHUNKSIZE_Z) * 2 + (x - CHUNKSIZE_X) * 2 + R1);
			int bmpPosY = int(Global::MapsizeY * Global::OffsetY + z + x - CHUNKSIZE_Z - CHUNKSIZE_X + R2) + 2 - (HEIGHTAT(x, z) & 0xFF) * Global::OffsetY;
			const unsigned int max = (HEIGHTAT(x, z) & 0xFF00) >> 8;
			for (unsigned int y = int8_t(HEIGHTAT(x, z)); y < max; ++y) {
				bmpPosY -= Global::OffsetY;
				const uint16_t& c = BLOCKAT(x, y, z);
				if (c == AIR) {
					continue;
				}

				//float col = float(y) * .78f - 91;
				float brightnessAdjustment = brightnessLookup[y];
				if (Global::settings.blendUnderground) {
					brightnessAdjustment -= 168;
				}
				// we use light if...
				if (Global::settings.nightmode // nightmode is active, or
					|| (Global::settings.skylight // skylight is used and
						&& (!BLOCK_AT_MAPEDGE(x, z))  // block is not edge of map (or if it is, has non-opaque block above)
						)) {
					int l = GETLIGHTAT(x, y, z);  // find out how much light hits that block
					if (l == 0 && y + 1 == Global::MapsizeY) {
						l = (Global::settings.nightmode ? 3 : 15);   // quickfix: assume maximum strength at highest level
					}
					else {
						const bool up = y + 1 < Global::MapsizeY;
						if (x + 1 < Global::MapsizeX && (!up || BLOCKAT(x + 1, y + 1, z) == 0)) {
							l = std::max(l, GETLIGHTAT(x + 1, y, z));
							if (x + 2 < Global::MapsizeX) l = std::max(l, GETLIGHTAT(x + 2, y, z) - 1);
						}
						if (z + 1 < Global::MapsizeZ && (!up || BLOCKAT(x, y + 1, z + 1) == 0)) {
							l = std::max(l, GETLIGHTAT(x, y, z + 1));
							if (z + 2 < Global::MapsizeZ) l = std::max(l, GETLIGHTAT(x, y, z + 2) - 1);
						}
						if (up) l = std::max(l, GETLIGHTAT(x, y + 1, z));
						//if (y + 2 < Global::MapsizeY) l = MAX(l, GETLIGHTAT(x, y + 2, z) - 1);
					}
					if (!Global::settings.skylight) { // Night
						brightnessAdjustment -= (100 - l * 8);
					}
					else { // Day
						brightnessAdjustment -= (210 - l * 14);
					}
				}

				// Edge detection (this means where terrain goes 'down' and the side of the block is not visible)
				if (y != 0) {
					uint16_t &b = BLOCKAT(x - 1, y - 1, z - 1);
					if ((y + 1 < Global::MapsizeY)  // In bounds?
						&& BLOCKAT(x, y + 1, z) == AIR  // Only if block above is air
						&& BLOCKAT(x - 1, y + 1, z - 1) == AIR  // and block above and behind is air
						&& (b == AIR || b == c)   // block behind (from pov) this one is same type or air
						&& (BLOCKAT(x - 1, y, z) == AIR || BLOCKAT(x, y, z - 1) == AIR)) {   // block TL/TR from this one is air = edge
						brightnessAdjustment += 13;
					}
				}

				setPixel(bmpPosX, bmpPosY, c, brightnessAdjustment, pngWriter);
			}
		}
	}
}

void undergroundMode(bool explore)
{
	// This wipes out all blocks that are not caves/tunnels
	//int cnt[256];
	//memset(cnt, 0, sizeof(cnt));
	std::cout << "Exploring underground...\n";
	if (explore) {
		clearLightmap();
		for (size_t x = CHUNKSIZE_X; x < Global::MapsizeX - CHUNKSIZE_X; ++x) {
			printProgress(x - CHUNKSIZE_X, Global::MapsizeX);
			for (size_t z = CHUNKSIZE_Z; z < Global::MapsizeZ - CHUNKSIZE_Z; ++z) {
				for (size_t y = 0; y < std::min(Global::MapsizeY, 64U) - 1; y++) {
					if (isTorch(BLOCKAT(x, y, z))) {
						// Torch
						BLOCKAT(x, y, z) = AIR;
						for (int ty = int(y) - 9; ty < int(y) + 9; ty += 2) { // The trick here is to only take into account
							if (ty < 0) {
								continue;   // areas around torches.
							}
							if (ty >= int(Global::MapsizeY) - 1) {
								break;
							}
							for (int tz = int(z) - 18; tz < int(z) + 18; ++tz) {
								if (tz < CHUNKSIZE_Z) {
									continue;
								}
								if (tz >= int(Global::MapsizeZ) - CHUNKSIZE_Z) {
									break;
								}
								for (int tx = int(x) - 18; tx < int(x) + 18; ++tx) {
									if (tx < CHUNKSIZE_X) {
										continue;
									}
									if (tx >= int(Global::MapsizeX) - CHUNKSIZE_X) {
										break;
									}
									SETLIGHTNORTH(tx, ty, tz) = 0xFF;
								}
							}
						}
						// /
					}
				}
			}
		}
	}
	for (size_t x = 0; x < Global::MapsizeX; ++x) {
		printProgress(x + Global::MapsizeX * (explore ? 1 : 0), Global::MapsizeX * (explore ? 2 : 1));
		for (size_t z = 0; z < Global::MapsizeZ; ++z) {
			size_t ground = 0;
			size_t cave = 0;
			for (int y = Global::MapsizeY - 1; y >= 0; --y) {
				uint16_t &c = BLOCKAT(x, y, z);
				if (c != AIR && cave > 0) { // Found a cave, leave floor
					if (isGrass(c) || isLeave(c) || isSnow(c) || GETLIGHTAT(x, y, z) == 0) {
						c = AIR; // But never count snow or leaves
					} //else cnt[*c]++;
					if (!isWater(c)) {
						--cave;
					}
				} else if (c != AIR) { // Block is not air, count up "ground"
					c = AIR;
					if (/*c != LOG &&*/ !isLeave(c) && !isSnow(c) && /*c != WOOD &&*/ !isWater(c)) {
						++ground;
					}
				} else if (ground < 3) { // Block is air, if there was not enough ground above, don't treat that as a cave
					ground = 0;
				} else { // Thats a cave, draw next two blocks below it
					cave = 2;
				}
			}
		}
	}
	printProgress(10, 10);
}

bool prepareNextArea(int splitX, int splitZ, int &bitmapStartX, int &bitmapStartY)
{
	static int currentAreaX = -1;
	static int currentAreaZ = 0;
	// move on to next part and stop if we're done
	++currentAreaX;
	if (currentAreaX >= splitX) {
		currentAreaX = 0;
		++currentAreaZ;
	}
	if (currentAreaZ >= splitZ) {
		return true;
	}
	// For bright map edges
	if (Global::settings.orientation == West || Global::settings.orientation == East) {
		gAtBottomRight = (currentAreaZ + 1 == splitZ);
		gAtBottomLeft = (currentAreaX + 1 == splitX);
	} else {
		gAtBottomLeft = (currentAreaZ + 1 == splitZ);
		gAtBottomRight = (currentAreaX + 1 == splitX);
	}
	// Calc size of area to be rendered (in chunks)
	const int subAreaX = ((Global::TotalToChunkX - Global::TotalFromChunkX) + (splitX - 1)) / splitX;
	const int subAreaZ = ((Global::TotalToChunkZ - Global::TotalFromChunkZ) + (splitZ - 1)) / splitZ;
	// Adjust values for current frame. order depends on map orientation
	Global::FromChunkX = Global::TotalFromChunkX + subAreaX * (Global::settings.orientation == North || Global::settings.orientation == West ? currentAreaX : splitX - (currentAreaX + 1));
	Global::FromChunkZ = Global::TotalFromChunkZ + subAreaZ * (Global::settings.orientation == North || Global::settings.orientation == East ? currentAreaZ : splitZ - (currentAreaZ + 1));
	Global::ToChunkX = Global::FromChunkX + subAreaX;
	Global::ToChunkZ = Global::FromChunkZ + subAreaZ;
	// Bounds checking
	if (Global::ToChunkX > Global::TotalToChunkX) {
		Global::ToChunkX = Global::TotalToChunkX;
	}
	if (Global::ToChunkZ > Global::TotalToChunkZ) {
		Global::ToChunkZ = Global::TotalToChunkZ;
	}
	std::cout << "Pass " << int(currentAreaX + (currentAreaZ * splitX) + 1) << " of " << int(splitX * splitZ)  << "...\n";
	// Calulate pixel offsets in bitmap. Forgot how this works right after writing it, really.
	if (Global::settings.orientation == North) {
		bitmapStartX = (((Global::TotalToChunkZ - Global::TotalFromChunkZ) * CHUNKSIZE_Z) * 2 + 3)   // Center of image..
		               - ((Global::ToChunkZ - Global::TotalFromChunkZ) * CHUNKSIZE_Z * 2)  // increasing Z pos will move left in bitmap
		               + ((Global::FromChunkX - Global::TotalFromChunkX) * CHUNKSIZE_X * 2);  // increasing X pos will move right in bitmap
		bitmapStartY = 5 + (Global::FromChunkZ - Global::TotalFromChunkZ) * CHUNKSIZE_Z + (Global::FromChunkX - Global::TotalFromChunkX) * CHUNKSIZE_X;
	} else if (Global::settings.orientation == South) {
		const int tox = Global::TotalToChunkX - Global::FromChunkX + Global::TotalFromChunkX;
		const int toz = Global::TotalToChunkZ - Global::FromChunkZ + Global::TotalFromChunkZ;
		const int fromx = tox - (Global::ToChunkX - Global::FromChunkX);
		const int fromz = toz - (Global::ToChunkZ - Global::FromChunkZ);
		bitmapStartX = (((Global::TotalToChunkZ - Global::TotalFromChunkZ) * CHUNKSIZE_Z) * 2 + 3)   // Center of image..
		               - ((toz - Global::TotalFromChunkZ) * CHUNKSIZE_Z * 2)  // increasing Z pos will move left in bitmap
		               + ((fromx - Global::TotalFromChunkX) * CHUNKSIZE_X * 2);  // increasing X pos will move right in bitmap
		bitmapStartY = 5 + (fromz - Global::TotalFromChunkZ) * CHUNKSIZE_Z + (fromx - Global::TotalFromChunkX) * CHUNKSIZE_X;
	} else if (Global::settings.orientation == East) {
		const int tox = Global::TotalToChunkX - Global::FromChunkX + Global::TotalFromChunkX;
		const int fromx = tox - (Global::ToChunkX - Global::FromChunkX);
		bitmapStartX = (((Global::TotalToChunkX - Global::TotalFromChunkX) * CHUNKSIZE_X) * 2 + 3)   // Center of image..
		               - ((tox - Global::TotalFromChunkX) * CHUNKSIZE_X * 2)  // increasing Z pos will move left in bitmap
		               + ((Global::FromChunkZ - Global::TotalFromChunkZ) * CHUNKSIZE_Z * 2);  // increasing X pos will move right in bitmap
		bitmapStartY = 5 + (fromx - Global::TotalFromChunkX) * CHUNKSIZE_X + (Global::FromChunkZ - Global::TotalFromChunkZ) * CHUNKSIZE_Z;
	} else {
		const int toz = Global::TotalToChunkZ - Global::FromChunkZ + Global::TotalFromChunkZ;
		const int fromz = toz - (Global::ToChunkZ - Global::FromChunkZ);
		bitmapStartX = (((Global::TotalToChunkX - Global::TotalFromChunkX) * CHUNKSIZE_X) * 2 + 3)   // Center of image..
		               - ((Global::ToChunkX - Global::TotalFromChunkX) * CHUNKSIZE_X * 2)  // increasing Z pos will move left in bitmap
		               + ((fromz - Global::TotalFromChunkZ) * CHUNKSIZE_Z * 2);  // increasing X pos will move right in bitmap
		bitmapStartY = 5 + (Global::FromChunkX - Global::TotalFromChunkX) * CHUNKSIZE_X + (fromz - Global::TotalFromChunkZ) * CHUNKSIZE_Z;
	}
	return false; // not done yet, return false
}

void writeInfoFile(const std::string& file, int xo, int yo, int bitmapX, int bitmapY)
{
	nlohmann::json data;

	std::string direction;
	if (Global::settings.orientation == North) {
		xo += (Global::TotalToChunkZ * CHUNKSIZE_Z - Global::FromChunkX * CHUNKSIZE_X) * 2 + 4;
		yo -= (Global::TotalFromChunkX * CHUNKSIZE_X + Global::TotalFromChunkZ * CHUNKSIZE_Z) - Global::MapsizeY * Global::OffsetY;
		direction = "North";
	}
	else if (Global::settings.orientation == South) {
		xo += (Global::TotalToChunkX * CHUNKSIZE_X - Global::TotalFromChunkZ * CHUNKSIZE_Z) * 2 + 4;
		yo += ((Global::TotalToChunkX) * CHUNKSIZE_X + (Global::TotalToChunkZ) * CHUNKSIZE_Z) + Global::MapsizeY * Global::OffsetY;
		direction = "South";
	}
	else if (Global::settings.orientation == East) {
		xo -= (Global::TotalFromChunkX * CHUNKSIZE_X + Global::TotalFromChunkZ * CHUNKSIZE_Z) * Global::OffsetY - 6;
		yo += ((Global::TotalToChunkX) * CHUNKSIZE_X - Global::TotalFromChunkZ * CHUNKSIZE_Z) + Global::MapsizeY * Global::OffsetY;
		direction = "East";
	}
	else {
		xo += (Global::TotalToChunkX * CHUNKSIZE_X + Global::TotalToChunkZ * CHUNKSIZE_Z) * Global::OffsetY + 2;
		yo += ((Global::TotalToChunkZ) * CHUNKSIZE_Z - Global::TotalFromChunkX * CHUNKSIZE_X) + Global::MapsizeY * Global::OffsetY;
		direction = "West";
	}
	yo += 4;

	data["origin"]["x"] = xo;
	data["origin"]["y"] = yo;

	data["geometry"]["scaling"] = Global::OffsetY;
	data["geometry"]["orientation"] = direction;

	data["image"]["x"] = bitmapX;
	data["image"]["y"] = bitmapY;

	data["meta"]["timestamp"] = static_cast<unsigned long>(time(nullptr));

	std::ofstream oStream(file);
	oStream << data;
}

/**
 * Round down to the nearest multiple of 16
 */
static const inline int floorChunkX(const int val)
{
	return val & ~(CHUNKSIZE_X - 1);
}

/**
 * Round down to the nearest multiple of 16
 */
static const inline int floorChunkZ(const int val)
{
	return val & ~(CHUNKSIZE_Z - 1);
}

void printHelp(const std::string& binary)
{
	const std::string numBits = sizeof(size_t) == 8 ? "64" : "32";
	std::cout
		////////////////////////////////////////////////////////////////////////////////
		<< "\nmcmap by Zahl - an isometric minecraft map rendering tool.\n"
		<< "Version " << VERSION << ' ' << numBits << "bit\n\n"
		<< "Usage: " << binary << " [-from X Z -to X Z] [-night] [-cave] [-noise VAL] [...] WORLDPATH\n\n"
		<< "  -from X Z     sets the coordinate of the chunk to start rendering at\n"
		<< "  -to X Z       sets the coordinate of the chunk to end rendering at\n"
		<< "                Note: Currently you need both -from and -to to define\n"
		<< "                bounds, otherwise the entire world will be rendered.\n"
		<< "  -cave         renders a map of all caves that have been explored by players\n"
		<< "  -blendcave    overlay caves over normal map; doesn't work with incremental\n"
		<< "                rendering (some parts will be hidden)\n"
		<< "  -night        renders the world at night using blocklight (torches)\n"
		<< "  -skylight     use skylight when rendering map (shadows below trees etc.)\n"
		<< "                hint: using this with -night makes a difference\n"
		<< "  -noise VAL    adds some noise to certain blocks, reasonable values are 0-20\n"
		<< "  -height VAL   maximum height at which blocks will be rendered\n"
		<< "  -min/max VAL  minimum/maximum Y index (height) of blocks to render\n"
		<< "  -file NAME    sets the output filename to 'NAME'; default is output.png\n"
		<< "  -mem VAL      sets the amount of memory (in MiB) used for rendering. mcmap\n"
		<< "                will use incremental rendering or disk caching to stick to\n"
		<< "                this limit. Default is 1800.\n"
		<< "  -colors NAME  loads user defined colors from file 'NAME'\n"
		<< "  -blocks NAME  loads user defined block ids from file 'NAME'\n"
		<< "  -threads VAL  uses VAL number of threads to load and optimize the world\n"
		<< "                uses this with a high mem limit for best performance\n"
		<< "  -north -east -south -west\n"
		<< "                controls which direction will point to the *top left* corner\n"
		<< "                it only makes sense to pass one of them; East is default\n"
		<< "  -blendall     always use blending mode for blocks\n"
		<< "  -hell         render the hell/nether dimension of the given world\n"
		<< "  -end          render the end dimension of the given world\n"
		<< "  -serverhell   force cropping of blocks at the top (use for nether servers)\n"
		<< "  -connGrass    uses connected textures to render grass\n"
		<< "  -info NAME    Write information about map to file 'NAME' in JSON format\n"
		<< "                use -infoonly to not render the world\n"
		<< "  -split PATH   create tiled output (128x128 to 4096x4096) in given PATH\n"
		<< "  -scale VAL    scales the resulting image by VAL. VAL in range 1-100\n"
		<< "  -marker c x z currently not working\n"
		<< "\n    WORLDPATH is the path of the desired world.\n\n"
		////////////////////////////////////////////////////////////////////////////////
		<< "Examples:\n\n"
#ifdef _WIN32
		<< binary << " %%APPDATA%%\\.minecraft\\saves\\World1\n"
		<< "  - This would render your entire singleplayer world in slot 1\n"
		<< binary << " -night -from -10 -10 -to 10 10 %%APPDATA%%\\.minecraft\\saves\\World1\n"
		<< "  - This would render the same world but at night, and only\n"
		<< "    from chunk (-10 -10) to chunk (10 10)\n";
#else
		<< binary << " ~/.minecraft/saves/World1\n"
		<< "  - This would render your entire singleplayer world in slot 1\n"
		<< binary << " -night -from -10 -10 -to 10 10 ~/.minecraft/saves/World1\n"
		<< "  - This would render the same world but at night, and only\n"
		<< "    from chunk (-10 -10) to chunk (10 10)\n";
#endif
}
