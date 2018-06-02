#include "worldloader.h"
#include "filesystem.h"
#include "nbt.h"
#include "colors.h"
#include "globals.h"
#include <cstring>
#include <string>
#include <cstdio>
#include <zlib.h>
#include <iostream>
#include <climits>
#include <array>
#include <cassert>
#include <fstream>

/*
Callstack:
getWorldFormat()

scanWorldDirectory()
	scanWorldDirectoryRegion() -> finds all regon files and point data

calcBitmapOverdraw() -> iterates over all points
calcTerrainSize() -> calcultes terrain size in bytes?

loadTerrain()
	loadTerrainRegion() -> iterates over all chunks
		allocateTerrain()
		loadChunk() -> iterates over all blocks
			loadAnvilChunk() -> loads chunk in anvil format
				assignBlock() -> sets block in array
*/

#define CHUNKS_PER_BIOME_FILE 32
#define REGIONSIZE 32

namespace {
	static World world;
}

#include "helper.h"

using std::string;

template <typename T>
T ntoh(T u)
{
	static_assert (CHAR_BIT == 8, "CHAR_BIT != 8");

	{ //Check for big endiness
		union {
			uint32_t i;
			char c[4];
		} b = { 0x01020304 };

		if (b.c[0] == 1) return u;
	}

	union
	{
		T u;
		unsigned char u8[sizeof(T)];
	} source, dest;

	source.u = u;

	for (size_t k = 0; k < sizeof(T); k++)
		dest.u8[k] = source.u8[sizeof(T) - k - 1];

	return dest.u;
}

template <typename T>
T ntoh(void* u, size_t size)
{
	static_assert (CHAR_BIT == 8, "CHAR_BIT != 8");
	assert(size <= sizeof(T));

	{ //Check for big endiness
		union {
			uint32_t i;
			char c[4];
		} b = { 0x01020304 };

		if (b.c[0] == 1) {
			T t{};
			memcpy(&t, u, size);
			__debugbreak(); //check, may not working
			return t;
		}
	}

	union
	{
		T u;
		unsigned char u8[sizeof(T)];
	} source{}, dest{};

	memcpy(&source.u, u, size);

	for (size_t k = 0; k < sizeof(T); k++)
		dest.u8[k] = source.u8[sizeof(T) - k - 1];

	return dest.u;
}

//static bool loadChunk(uint8_t* buffer, const size_t len);
static bool loadChunk(const std::vector<uint8_t>& buffer);
static bool loadAnvilChunk(NBT_Tag * const level, const int32_t chunkX, const int32_t chunkZ);
static void allocateTerrain();
static void loadBiomeChunk(const std::string& path, const int chunkX, const int chunkZ);
static bool loadAllRegions();
static bool loadRegion(const std::string& file, const bool mustExist, int &loadedChunks);
static bool loadTerrainRegion(const std::string& fromPath, int &loadedChunks);
static bool scanWorldDirectoryRegion(const std::string& fromPath);
static inline void assignBlock(const uint16_t &source, uint8_t* &dest, int &x, int &y, int &z, const uint8_t* &justData);
static inline void assignBlock(const uint16_t &source, uint8_t* &dest, int &x, int &y, int &z, const uint8_t* &justData, const uint8_t* &addData);
static inline void lightCave(const int x, const int y, const int z);

WorldFormat getWorldFormat(const std::string& worldPath)
{
	WorldFormat format = ALPHA; // alpha (single chunk files)
	std::string path = worldPath;
	path.append("/region");

	myFile file;
	DIRHANDLE sd = Dir::open(path.c_str(), file);
	if (sd != NULL) {
		do { // Here we finally arrived at the region files
			if (strEndsWith(file.name, ".mca")) {
				format = ANVIL;
				break;
			} else if (format != REGION && strEndsWith(file.name, ".mcr")) {
				format = REGION;
			}
		} while (Dir::next(sd, path.c_str(), file));
		Dir::close(sd);
	}
	return format;
}

bool scanWorldDirectory(const std::string& fromPath)
{
	//if (Global::worldFormat != ALPHA)
		return scanWorldDirectoryRegion(fromPath);

#pragma region loadAlphaWorld
	/*
	//Alpha format
	charList subdirs;
	myFile file;
	DIRHANDLE d = Dir::open((char*)fromPath.c_str(), file);
	if (d == NULL) {
		return false;
	}
	do {
		if (file.isdir && strcmp(file.name + strlen(file.name) - 3, "/..") != 0 && strcmp(file.name + strlen(file.name) - 2, "/.") != 0) {
			char *s = strdup(file.name);
			subdirs.push_back(s);
		}
	} while (Dir::next(d, (char *)fromPath.c_str(), file));
	Dir::close(d);
	if (subdirs.empty()) {
		return false;
	}
	// OK go
	for (chunkList::iterator it = chunks.begin(); it != chunks.end(); it++) {
		delete *it;
	}
	chunks.clear();
	g_FromChunkX = g_FromChunkZ = 10000000;
	g_ToChunkX   = g_ToChunkZ  = -10000000;
	// Read subdirs now
	string base(fromPath);
	base.append("/");
	const size_t max = subdirs.size();
	size_t count = 0;
	printf("Scanning world...\n");
	for (charList::iterator it = subdirs.begin(); it != subdirs.end(); it++) {
		string base2 = base + *it;
		printProgress(count++, max);
		d = Dir::open((char *)base2.c_str(), file);
		if (d == NULL) {
			continue;
		}
		do {
			if (file.isdir) {
				// Scan inside scan
				myFile chunk;
				string path = base2 + "/" + file.name;
				DIRHANDLE sd = Dir::open((char *)path.c_str(), chunk);
				if (sd != NULL) {
					do { // Here we finally arrived at the chunk files
						if (!chunk.isdir && chunk.name[0] == 'c' && chunk.name[1] == '.') { // Make sure filename is a chunk
							char *s = chunk.name;
							// Extract x coordinate from chunk filename
							s += 2;
							int valX = base10(s);
							// Extract z coordinate from chunk filename
							while (*s != '.' && *s != '\0') {
								++s;
							}
							int valZ = base10(s+1);
							if (valX > -4000 && valX < 4000 && valZ > -4000 && valZ < 4000) {
								// Update bounds
								if (valX < g_FromChunkX) {
									g_FromChunkX = valX;
								}
								if (valX > g_ToChunkX) {
									g_ToChunkX = valX;
								}
								if (valZ < g_FromChunkZ) {
									g_FromChunkZ = valZ;
								}
								if (valZ > g_ToChunkZ) {
									g_ToChunkZ = valZ;
								}
								string full = path + "/" + chunk.name;
								chunks.push_back(new Chunk(full.c_str(), valX, valZ));
							} else {
								printf("Ignoring bad chunk at %d %d\n", valX, valZ);
							}
						}
					} while (Dir::next(sd, (char *)path.c_str(), chunk));
					Dir::close(sd);
				}
			}
		} while (Dir::next(d, (char *)base2.c_str(), file));
		Dir::close(d);
	}
	printProgress(10, 10);
	g_ToChunkX++;
	g_ToChunkZ++;
	//
	for (charList::iterator it = subdirs.begin(); it != subdirs.end(); it++) {
		free(*it);
	}
	printf("Min: (%d|%d) Max: (%d|%d)\n", g_FromChunkX, g_FromChunkZ, g_ToChunkX, g_ToChunkZ);
	return true;
	*/
#pragma endregion
}

static bool scanWorldDirectoryRegion(const std::string& fromPath)
{
	// OK go
	world.regions.clear();
	//reset, why??
	Global::FromChunkX = Global::FromChunkZ = 10000000;
	Global::ToChunkX   = Global::ToChunkZ = -10000000;

	// Read subdirs now
	string path(fromPath);
	path.append("/region");
	std::cout << "Scanning world...\n";
	myFile region;
	DIRHANDLE sd = Dir::open(path, region);
	if (sd != NULL) {
		do { // Here we finally arrived at the region files
			if (!region.isdir && region.name[0] == 'r' && region.name[1] == '.') { // Make sure filename is a region
				std::string s = region.name;
				if (strEndsWith(s, ".mca")) {
					// Extract x coordinate from region filename
					s = s.substr(2);
					const auto values = strSplit(s, '.');
					if (values.size() != 3) __debugbreak();

					const int valX = std::stoi(values.at(0)) * REGIONSIZE;
					// Extract z coordinate from region filename
					const int valZ = std::stoi(values.at(1)) * REGIONSIZE;
					if (valX > -4000 && valX < 4000 && valZ > -4000 && valZ < 4000) {
						string full = path + "/" + region.name;
						world.regions.push_back(Region(full, valX, valZ));
						//printf("Good region at %d %d\n", valX, valZ);
					}
					else {
						std::cerr << std::string("Ignoring bad region at ") << valX << ' ' + valZ << '\n';
					}
				}
			}
		} while (Dir::next(sd, path, region));
		Dir::close(sd);
	}

	// Read all region files' headers to figure out which chunks actually exist
	// It would be sufficient to just do this on those which form the edge
	// Have yet to find out how slow this is on big maps to see if it's worth the effort
	world.points.clear();

	for (regionList::iterator it = world.regions.begin(); it != world.regions.end(); it++) {
		Region& region = (*it);
		std::ifstream fh(region.filename, std::ios::in | std::ios::binary);
		//FILE *fh = fopen(region.filename.c_str(), "rb");
		if (!fh.good()) {
			std::cerr << "Cannot scan region " << region.filename << '\n';
			region.filename.clear();
			continue;
		}
		//std::array<uint8_t, REGIONSIZE * REGIONSIZE * 4> buffer;
		std::vector<uint8_t> buffer(REGIONSIZE * REGIONSIZE * 4, 0);
		fh.read((char*)buffer.data(), 4 * REGIONSIZE * REGIONSIZE);
		if (fh.fail()) {
			std::cerr << "Could not read header from " << region.filename << '\n';
			region.filename.clear();
			continue;
		}
		// Check for existing chunks in region and update bounds
		for (int i = 0; i < REGIONSIZE * REGIONSIZE; ++i) {
			const uint32_t offset = (ntoh<uint32_t>(&buffer[i * 4], 3) >> 8) * 4096;
			if (offset == 0) continue;
			std::cout << "Scan region: " << region.filename << ' ' << std::to_string(offset) + ' ' + std::to_string(i);
			//printf("Scan region %s, offset %d (%d)\n", region.filename,offset,i);

			const int valX = region.x + i % REGIONSIZE;
			const int valZ = region.z + i / REGIONSIZE;
			world.points.push_back(Point(valX, valZ));
			if (valX < Global::FromChunkX) {
				Global::FromChunkX = valX;
			}
			if (valX > Global::ToChunkX) {
				Global::ToChunkX = valX;
			}
			if (valZ < Global::FromChunkZ) {
				Global::FromChunkZ = valZ;
			}
			if (valZ > Global::ToChunkZ) {
				Global::ToChunkZ = valZ;
			}
		}
	}
	Global::ToChunkX++;
	Global::ToChunkZ++;
	//
	std::cout << "Min: (" << Global::FromChunkX << '|' << Global::FromChunkZ << ") Max: (" << Global::ToChunkX << '|' << Global::ToChunkZ << ")\n";
	//printf("Min: (%d|%d) Max: (%d|%d)\n", Global::FromChunkX, Global::FromChunkZ, Global::ToChunkX, Global::ToChunkZ);
	return true;
}

bool loadEntireTerrain()
{
	if (Global::worldFormat != ALPHA) return loadAllRegions();
	
	__debugbreak(); //schould not arrive here
	return false;
	//Load Alpha world
	/*
	if (chunks.empty()) {
		return false;
	}
	allocateTerrain();
	const size_t max = chunks.size();
	size_t count = 0;
	printf("Loading all chunks..\n");
	for (chunkList::iterator it = chunks.begin(); it != chunks.end(); it++) {
		printProgress(count++, max);
		loadChunk((**it).filename);
		delete *it;
	}
	chunks.clear();
	printProgress(10, 10);
	return true;
	*/
}

bool loadTerrain(const std::string& fromPath, int &loadedChunks)
{
	loadedChunks = 0;
	if (Global::worldFormat != ALPHA) return loadTerrainRegion(fromPath, loadedChunks);

	//load Alpha format, support dropped
	__debugbreak();
	return false;
	/*
	if (fromPath.empty()) {
		return false;
	}
	allocateTerrain();
	string path(fromPath);
	if (path.at(path.size()-1) != '/') {
		path.append("/");
	}

	printf("Loading all chunks..\n");
	for (int chunkZ = g_FromChunkZ; chunkZ < g_ToChunkZ; ++chunkZ) {
		printProgress(chunkZ - g_FromChunkZ, g_ToChunkZ - g_FromChunkZ);
		for (int chunkX = g_FromChunkX; chunkX < g_ToChunkX; ++chunkX) {
			string thispath = path + base36((chunkX + 640000) % 64) + "/" + base36((chunkZ + 640000) % 64) + "/c." + base36(chunkX) + "." + base36(chunkZ) + ".dat";
			if (loadChunk(thispath.c_str())) {
				++loadedChunks;
			}
		}
	}
	// Done loading all chunks
	printProgress(10, 10);
	return true;
	*/
}

static bool loadChunk(const std::vector<uint8_t>& buffer) //uint8_t* buffer, const size_t streamLen
{
	bool ok = false;
	if (buffer.size() == 0) { // File
		__debugbreak(); //give file to loadChunk, should not happen
	}
	NBT chunk(buffer);
	if (!chunk.good()) {
		//printf("Error loading chunk.\n");
		std::cerr << "Error loading chunk.\n";
		return false; // chunk does not exist
	}
	NBT_Tag *level = NULL;
	ok = chunk.getCompound("Level", level);
	if (!ok) {
		printf("No level\n");
		//delete chunk;
		return false;
	}
	int32_t chunkX, chunkZ;
	ok = level->getInt("xPos", chunkX);
	ok = ok && level->getInt("zPos", chunkZ);
	if (!ok) {
		printf("No pos\n");
		//delete chunk;
		return false;
	}
	// Check if chunk is in desired bounds (not a chunk where the filename tells a different position)
	if (chunkX < Global::FromChunkX || chunkX >= Global::ToChunkX || chunkZ < Global::FromChunkZ || chunkZ >= Global::ToChunkZ) {
		if (!chunk.good()) printf("Chunk is out of bounds. %d %d\n", chunkX, chunkZ);
		//delete chunk;
		return false; // Nope, its not...
	}
	if (Global::worldFormat == ANVIL) {
		bool ret = loadAnvilChunk(level, chunkX, chunkZ);
		//delete chunk;
		return ret;
	}
	//loading old world format, support dropped
	__debugbreak();
	return false;

	/*
	const int offsetz = (chunkZ - Global::FromChunkZ) * CHUNKSIZE_Z;
	const int offsetx = (chunkX - Global::FromChunkX) * CHUNKSIZE_X;
	uint8_t *blockdata, *lightdata, *skydata, *justData;
	int32_t len;
	ok = level->getByteArray("Blocks", blockdata, len);
	if (!ok || len < CHUNKSIZE_X * CHUNKSIZE_Z * CHUNKSIZE_Y) {
		printf("No blocks\n");
		delete chunk;
		return false;
	}
	ok = level->getByteArray("Data", justData, len);
	if (!ok || len < (CHUNKSIZE_X * CHUNKSIZE_Z * CHUNKSIZE_Y) / 2) {
		printf("No block data\n");
		delete chunk;
		return false;
	}
	if (g_Nightmode || g_Skylight) { // If nightmode, we need the light information too
		ok = level->getByteArray("BlockLight", lightdata, len);
		if (!ok || len < (CHUNKSIZE_X * CHUNKSIZE_Z * CHUNKSIZE_Y) / 2) {
			printf("No block light\n");
			delete chunk;
			return false;
		}
	}
	if (g_Skylight) { // Skylight desired - wish granted
		ok = level->getByteArray("SkyLight", skydata, len);
		if (!ok || len < (CHUNKSIZE_X * CHUNKSIZE_Z * CHUNKSIZE_Y) / 2) {
			delete chunk;
			return false;
		}
	}
	// Markers
	if (g_MarkerCount != 0) for (int i = 0; i < g_MarkerCount; ++i) {
		Marker &m = g_Markers[i];
		if (m.chunkX == chunkX && m.chunkZ == chunkZ) {
			memset(blockdata + ((m.offsetZ + (m.offsetX * CHUNKSIZE_Z)) * CHUNKSIZE_Y), m.color, CHUNKSIZE_Y);
		}
	}
	// Now read all blocks from this chunk and copy them to the world array
	// Rotation introduces lots of if-else blocks here :-(
	// Maybe make the macros functions and then use pointers.... Probably not faster
	for (int x = 0; x < CHUNKSIZE_X; ++x) {
		for (int z = 0; z < CHUNKSIZE_Z; ++z) {
			if (g_Hell || g_ServerHell) {
				// Remove blocks on top, otherwise there is not much to see here
				int massive = 0;
				uint8_t *bp = blockdata + ((z + (x * CHUNKSIZE_Z) + 1) * CHUNKSIZE_Y) - 1;
				int i;
				for (i = 0; i < 74; ++i) { // Go down 74 blocks from the ceiling to see if there is anything except solid
					if (massive && (*bp == AIR || *bp == LAVA || *bp == STAT_LAVA)) {
						if (--massive == 0) {
							break;   // Ignore caves that are only 2 blocks high
						}
					}
					if (*bp != AIR && *bp != LAVA && *bp != STAT_LAVA) {
						massive = 3;
					}
					--bp;
				}
				// So there was some cave or anything before going down 70 blocks, everything above will get removed
				// If not, only 45 blocks starting at the ceiling will be removed
				if (i > 70) {
					i = 45;   // TODO: Make this configurable
				}
				bp = blockdata + ((z + (x * CHUNKSIZE_Z) + 1) * CHUNKSIZE_Y) - 1;
				for (int j = 0; j < i; ++j) {
					*bp-- = AIR;
				}
			}
			uint8_t *targetBlock;
			if (g_Orientation == East) {
				targetBlock = &BLOCKEAST(x + offsetx, 0, z + offsetz);
			} else if (g_Orientation == North) {
				targetBlock = &BLOCKNORTH(x + offsetx, 0, z + offsetz);
			} else if (g_Orientation == South) {
				targetBlock = &BLOCKSOUTH(x + offsetx, 0, z + offsetz);
			} else {
				targetBlock = &BLOCKWEST(x + offsetx, 0, z + offsetz);
			}
			// Following code applies only to modes (ab)using the light map, and for block remapping (wool color, trees, steps)
			const int toY = g_MapsizeY + g_MapminY;
			for (int y = (g_MapminY / 2) * 2; y < toY; ++y) {
				const int oy = y - g_MapminY;
				uint8_t &block = blockdata[y + (z + (x * CHUNKSIZE_Z)) * CHUNKSIZE_Y];
				// Wool/wood/leaves block hack: Additional block data determines type of this block, here those get remapped to other block ids
				// Ignore leaves for now if biomes are used, since I have no clue how the color shifting works then
				assignBlock(block, targetBlock, x, y, z, justData);
				if (g_Underground) {
					if (y < g_MapminY) continue; // As we start at even numbers there might be no block data here
					if (block == TORCH) {
						// In underground mode, the lightmap is also used, but the values are calculated manually, to only show
						// caves the players have discovered yet. It's not perfect of course, but works ok.
						lightCave(x + offsetx, y, z + offsetz);
					}
				} else if (g_Skylight && y % 2 == 0 && y >= g_MapminY) { // copy light info too. Only every other time, since light info is 4 bits
					const uint8_t &light = lightdata[(y / 2) + (z + (x * CHUNKSIZE_Z)) * (CHUNKSIZE_Y / 2)];
					const uint8_t highlight = (light >> 4) & 0x0F;
					const uint8_t lowlight =  (light & 0x0F);
					const uint8_t &sky = skydata[(y / 2) + (z + (x * CHUNKSIZE_Z)) * (CHUNKSIZE_Y / 2)];
					uint8_t highsky = ((sky >> 4) & 0x0F);
					uint8_t lowsky =  (sky & 0x0F);
					if (g_Nightmode) {
						highsky = clamp(highsky / 3 - 2);
						lowsky = clamp(lowsky / 3 - 2);
					}
					if (g_Orientation == East) {
						SETLIGHTEAST(x + offsetx, oy, z + offsetz) = (MAX(highlight, highsky) << 4) | (MAX(lowlight, lowsky) & 0x0F);
					} else if (g_Orientation == North) {
						SETLIGHTNORTH(x + offsetx, oy, z + offsetz) = (MAX(highlight, highsky) << 4) | (MAX(lowlight, lowsky) & 0x0F);
					} else if (g_Orientation == South) {
						SETLIGHTSOUTH(x + offsetx, oy, z + offsetz) = (MAX(highlight, highsky) << 4) | (MAX(lowlight, lowsky) & 0x0F);
					} else {
						SETLIGHTWEST(x + offsetx, oy, z + offsetz) = (MAX(highlight, highsky) << 4) | (MAX(lowlight, lowsky) & 0x0F);
					}
				} else if (g_Nightmode && y % 2 == 0 && y >= g_MapminY) {
					if (g_Orientation == East) {
						SETLIGHTEAST(x + offsetx, oy, z + offsetz) = lightdata[(y / 2) + (z + (x * CHUNKSIZE_Z)) * (CHUNKSIZE_Y / 2)];
					} else if (g_Orientation == North) {
						SETLIGHTNORTH(x + offsetx, oy, z + offsetz) = lightdata[(y / 2) + (z + (x * CHUNKSIZE_Z)) * (CHUNKSIZE_Y / 2)];
					} else if (g_Orientation == South) {
						SETLIGHTSOUTH(x + offsetx, oy, z + offsetz) = lightdata[(y / 2) + (z + (x * CHUNKSIZE_Z)) * (CHUNKSIZE_Y / 2)];
					} else {
						SETLIGHTWEST(x + offsetx, oy, z + offsetz) = lightdata[(y / 2) + (z + (x * CHUNKSIZE_Z)) * (CHUNKSIZE_Y / 2)];
					}
				}
			}
		} // z
	} // x
	delete chunk;
	return true;
	*/
}

static bool loadAnvilChunk(NBT_Tag * const level, const int32_t chunkX, const int32_t chunkZ)
{
	PrimArray<uint8_t> *blockdata, *lightdata, *skydata, *justData, *addData = 0, *biomesdata;
	int32_t len, yoffset, yoffsetsomething = (Global::MapminY + SECTION_Y * 10000) % SECTION_Y;
	int8_t yo;
	std::list<NBT_Tag*> *sections = NULL;
	bool ok;
	/* Dropped support for biomes
	if (g_UseBiomes) {
		ok = level->getByteArray("Biomes", biomesdata, len);
		if (!ok) {
			printf("No biomes found in region\n");	
			return false;
		}
	}*/
	//
	ok = level->getList("Sections", sections);
	if (!ok) {
		printf("No sections found in region\n");
		return false;
	}
	//
	const int offsetz = (chunkZ - Global::FromChunkZ) * CHUNKSIZE_Z;
	const int offsetx = (chunkX - Global::FromChunkX) * CHUNKSIZE_X;
	for (std::list<NBT_Tag *>::iterator it = sections->begin(); it != sections->end(); it++) {
		NBT_Tag *section = *it;
		ok = section->getByte("Y", yo);
		if (!ok) {
			printf("Y-Offset not found in section\n");
			return false;
		}
		if (yo < Global::sectionMin || yo > Global::sectionMax) continue;
		yoffset = (SECTION_Y * (int)(yo - Global::sectionMin)) - yoffsetsomething;
		if (yoffset < 0) yoffset = 0;
		PrimArray<uint8_t> b;
		ok = section->getByteArray("Blocks", blockdata);
		if (!ok || len < CHUNKSIZE_X * CHUNKSIZE_Z * SECTION_Y) {
			printf("No blocks\n");
			return false;
		}
		ok = section->getByteArray("Data", justData);
		if (!ok || len < (CHUNKSIZE_X * CHUNKSIZE_Z * SECTION_Y) / 2) {
			printf("No block data\n");
			return false;
		}
		ok = section->getByteArray("Add", addData);
		if (Global::settings.nightmode || Global::settings.skylight) { // If nightmode, we need the light information too
			ok = section->getByteArray("BlockLight", lightdata);
			if (!ok || len < (CHUNKSIZE_X * CHUNKSIZE_Z * SECTION_Y) / 2) {
				printf("No block light\n");
				return false;
			}
		}
		if (Global::settings.skylight) { // Skylight desired - wish granted
			ok = section->getByteArray("SkyLight", skydata);
			if (!ok || len < (CHUNKSIZE_X * CHUNKSIZE_Z * SECTION_Y) / 2) {
				return false;
			}
		}
		// Copy data
		for (int x = 0; x < CHUNKSIZE_X; ++x) {
			for (int z = 0; z < CHUNKSIZE_Z; ++z) {
				uint8_t *targetBlock, *lightByte;
				if (Global::settings.orientation == East) {
					targetBlock = &BLOCKEAST(x + offsetx, yoffset, z + offsetz);
					if (Global::settings.skylight || Global::settings.nightmode) lightByte = &SETLIGHTEAST(x + offsetx, yoffset, z + offsetz);
					//if (g_UseBiomes) BIOMEEAST(x + offsetx, z + offsetz) = biomesdata[x + (z * CHUNKSIZE_X)];
				} else if (Global::settings.orientation == North) {
					targetBlock = &BLOCKNORTH(x + offsetx, yoffset, z + offsetz);
					if (Global::settings.skylight || Global::settings.nightmode) lightByte = &SETLIGHTNORTH(x + offsetx, yoffset, z + offsetz);
					//if (g_UseBiomes) BIOMENORTH(x + offsetx, z + offsetz) = biomesdata[x + (z * CHUNKSIZE_X)];
				} else if (Global::settings.orientation == South) {
					targetBlock = &BLOCKSOUTH(x + offsetx, yoffset, z + offsetz);
					if (Global::settings.skylight || Global::settings.nightmode) lightByte = &SETLIGHTSOUTH(x + offsetx, yoffset, z + offsetz);
					//if (g_UseBiomes) BIOMESOUTH(x + offsetx, z + offsetz) = biomesdata[x + (z * CHUNKSIZE_X)];
				} else {
					targetBlock = &BLOCKWEST(x + offsetx, yoffset, z + offsetz);
					if (Global::settings.skylight || Global::settings.nightmode) lightByte = &SETLIGHTWEST(x + offsetx, yoffset, z + offsetz);
					//if (g_UseBiomes) BIOMEWEST(x + offsetx, z + offsetz) = biomesdata[x + (z * CHUNKSIZE_X)];
				}
				//const int toY = g_MapsizeY + g_MapminY;
				for (int y = 0; y < SECTION_Y; ++y) {
					// In bounds check
					if (Global::sectionMin == yo && y < yoffsetsomething) continue;
					if (Global::sectionMax == yo && y + yoffset >= Global::MapsizeY) break;
					// Block data
					const uint8_t &block = blockdata->_data[x + (z + (y * CHUNKSIZE_Z)) * CHUNKSIZE_X];
					assignBlock(block, targetBlock, x, y, z, justData->_data, addData->_data);
					// Light
					if (Global::settings.underground) {
						if (block == TORCH) {
							if (y + yoffset < Global::MapminY) continue;
							printf("Torch at %d %d %d\n", x + offsetx, yoffset + y, z + offsetz);
							lightCave(x + offsetx, yoffset + y, z + offsetz);
						}
					} else if (Global::settings.skylight && (y & 1) == 0) {
						const uint8_t highlight = ((lightdata->_data[(x + (z + ((y + 1) * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x & 1) * 4)) & 0x0F);
						const uint8_t lowlight =  ((lightdata->_data[(x + (z + (y * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x & 1) * 4)) & 0x0F);
						uint8_t highsky = ((skydata->_data[(x + (z + ((y + 1) * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x & 1) * 4)) & 0x0F);
						uint8_t lowsky =  ((skydata->_data[(x + (z + (y * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x & 1) * 4)) & 0x0F);
						if (Global::settings.nightmode) {
							highsky = clamp(highsky / 3 - 2);
							lowsky = clamp(lowsky / 3 - 2);
						}
						*lightByte++ = ((MAX(highlight, highsky) & 0x0F) << 4) | (MAX(lowlight, lowsky) & 0x0F);
					} else if (Global::settings.nightmode && (y & 1) == 0) {
						*lightByte++ = ((lightdata->_data[(x + (z + (y * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x & 1) * 4)) & 0x0F)
							| ((lightdata->_data[(x + (z + ((y + 1) * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x & 1) * 4)) << 4);
					}
				} // for y
			} // for z
		} // for x

/*
		// Markers Anvil - TODO
		if (g_MarkerCount != 0) for (int i = 0; i < g_MarkerCount; ++i) {
			Marker &m = g_Markers[i];
			if (m.chunkX == chunkX && m.chunkZ == chunkZ) {
				memset(blockdata + ((m.offsetZ + (m.offsetX * CHUNKSIZE_Z)) * SECTION_Y), m.color, SECTION_Y - 1);
		printf("marker-%d-%d---%d-%d---%d---\n\n",chunkX,chunkZ,m.offsetX,m.offsetZ,m.color);
			}
		}
*/

	}
	return true;
}

uint64_t calcTerrainSize(const int chunksX, const int chunksZ)
{
	uint64_t size;
	if (Global::settings.lowMemory)
		size =     uint64_t(chunksX+2) * CHUNKSIZE_X * uint64_t(chunksZ+2) * CHUNKSIZE_Z * uint64_t(Global::MapsizeY);
	else
		size = 2 * uint64_t(chunksX+2) * CHUNKSIZE_X * uint64_t(chunksZ+2) * CHUNKSIZE_Z * uint64_t(Global::MapsizeY);

	if (Global::settings.nightmode || Global::settings.underground || Global::settings.blendUnderground || Global::settings.skylight) {
		if (Global::settings.lowMemory)
			size += size / 2;
		else
			size += size / 4;
	}
	/* biomes no longer supported
	if (g_UseBiomes) {
		size += uint64_t(chunksX+2) * CHUNKSIZE_X * uint64_t(chunksZ+2) * CHUNKSIZE_Z * sizeof(uint16_t);
	}*/
	return size;
}

void calcBitmapOverdraw(int &left, int &right, int &top, int &bottom)
{
	top = left = bottom = right = 0x0fffffff;
	int val, x, z;
	regionList::iterator itR;
	pointList::iterator itP;
	if (Global::worldFormat != ALPHA) {
		itP = world.points.begin();
	} else {
		__debugbreak(), //Loading alpha world
		itR = world.regions.begin();
	}
	for (;;) {
		if (Global::worldFormat != ALPHA) {
			if (itP == world.points.end()) break;
			x = (*itP).x;
			z = (*itP).z;
		} else {
			__debugbreak(); //loading alpha world
			if (itR == world.regions.end()) break;
			x = (*itR).x;
			z = (*itR).z;
		}
		if (Global::settings.orientation == North) {
			// Right
			val = (((Global::ToChunkX - 1) - x) * CHUNKSIZE_X * 2)
			      + ((z - Global::FromChunkZ) * CHUNKSIZE_Z * 2);
			if (val < right) {
				right = val;
			}
			// Left
			val = (((Global::ToChunkZ - 1) - z) * CHUNKSIZE_Z * 2)
			      + ((x - Global::FromChunkX) * CHUNKSIZE_X * 2);
			if (val < left) {
				left = val;
			}
			// Top
			val = (z - Global::FromChunkZ) * CHUNKSIZE_Z + (x - Global::FromChunkX) * CHUNKSIZE_X;
			if (val < top) {
				top = val;
			}
			// Bottom
			val = (((Global::ToChunkX - 1) - x) * CHUNKSIZE_X) + (((Global::ToChunkZ - 1) - z) * CHUNKSIZE_Z);
			if (val < bottom) {
				bottom = val;
			}
		} else if (Global::settings.orientation == South) {
			// Right
			val = (((Global::ToChunkZ - 1) - z) * CHUNKSIZE_Z * 2)
			      + ((x - Global::FromChunkX) * CHUNKSIZE_X * 2);
			if (val < right) {
				right = val;
			}
			// Left
			val = (((Global::ToChunkX - 1) - x) * CHUNKSIZE_X * 2)
			      + ((z - Global::FromChunkZ) * CHUNKSIZE_Z * 2);
			if (val < left) {
				left = val;
			}
			// Top
			val = ((Global::ToChunkZ - 1) - z) * CHUNKSIZE_Z + ((Global::ToChunkX - 1) - x) * CHUNKSIZE_X;
			if (val < top) {
				top = val;
			}
			// Bottom
			val = ((x - Global::FromChunkX) * CHUNKSIZE_X) + ((z - Global::FromChunkZ) * CHUNKSIZE_Z);
			if (val < bottom) {
				bottom = val;
			}
		} else if (Global::settings.orientation == East) {
			// Right
			val = ((Global::ToChunkZ - 1) - z) * CHUNKSIZE_Z * 2 + ((Global::ToChunkX - 1) - x) * CHUNKSIZE_X * 2;
			if (val < right) {
				right = val;
			}
			// Left
			val = ((x - Global::FromChunkX) * CHUNKSIZE_X) * 2 +  + ((z - Global::FromChunkZ) * CHUNKSIZE_Z) * 2;
			if (val < left) {
				left = val;
			}
			// Top
			val = ((Global::ToChunkX - 1) - x) * CHUNKSIZE_X
			      + (z - Global::FromChunkZ) * CHUNKSIZE_Z;
			if (val < top) {
				top = val;
			}
			// Bottom
			val = ((Global::ToChunkZ - 1) - z) * CHUNKSIZE_Z
			      + (x - Global::FromChunkX) * CHUNKSIZE_X;
			if (val < bottom) {
				bottom = val;
			}
		} else {
			// Right
			val = ((x - Global::FromChunkX) * CHUNKSIZE_X) * 2 +  + ((z - Global::FromChunkZ) * CHUNKSIZE_Z) * 2;
			if (val < right) {
				right = val;
			}
			// Left
			val = ((Global::ToChunkZ - 1) - z) * CHUNKSIZE_Z * 2 + ((Global::ToChunkX - 1) - x) * CHUNKSIZE_X * 2;
			if (val < left) {
				left = val;
			}
			// Top
			val = ((Global::ToChunkZ - 1) - z) * CHUNKSIZE_Z
			      + (x - Global::FromChunkX) * CHUNKSIZE_X;
			if (val < top) {
				top = val;
			}
			// Bottom
			val = ((Global::ToChunkX - 1) - x) * CHUNKSIZE_X
			      + (z - Global::FromChunkZ) * CHUNKSIZE_Z;
			if (val < bottom) {
				bottom = val;
			}
		}
		//
		if (Global::worldFormat != ALPHA) {
			itP++;
		} else {
			itR++;
		}
	}
}

static void allocateTerrain()
{
	Global::terrain.clear();
	Global::light.clear();
	Global::heightMap.clear();
	Global::biomeMap.clear();
	/* biomes no longer supported
	if (Global::useBiomes) //&& worldFormat == 2
	{
	    Global::biomeMapSize = Global::MapsizeX * Global::MapsizeZ;
		Global::biomeMap.resize(Global::biomeMapSize, 0);
	}*/
	Global::heightMap.resize(Global::MapsizeX * Global::MapsizeZ, 0);
	//printf("%d -- %d\n", g_MapsizeX, g_MapsizeZ); //dimensions of terrain map (in memory)
	Global::Terrainsize = Global::MapsizeX * Global::MapsizeY * Global::MapsizeZ;
	if (Global::settings.lowMemory) {
		printf("Terrain takes up %.2fMiB", float(Global::Terrainsize / float(1024 * 1024)));
		Global::terrain.resize(Global::Terrainsize, 0);  // Preset: Air
	} else {
		printf("Terrain takes up %.2fMiB", float(Global::Terrainsize * 2 / float(1024 * 1024)));
		Global::terrain.resize(Global::Terrainsize*2, 0);  // Preset: Air
	}
	if (Global::settings.nightmode || Global::settings.underground || Global::settings.blendUnderground || Global::settings.skylight) {
		Global::lightsize = Global::MapsizeZ * Global::MapsizeX * ((Global::MapsizeY + (Global::MapminY % 2 == 0 ? 1 : 2)) / 2);
		printf(", lightmap %.2fMiB", float(Global::lightsize / float(1024 * 1024)));
		Global::light.resize(Global::lightsize);
		// Preset: all bright / dark depending on night or day
		if (Global::settings.nightmode) {
			std::fill(Global::light.begin(), Global::light.end(), 0x11);
		} else if (Global::settings.underground) {
			std::fill(Global::light.begin(), Global::light.end(), 0x00);
		} else {
			std::fill(Global::light.begin(), Global::light.end(), 0xFF);
		}
	}
	printf("\n");
}

void clearLightmap()
{
	std::fill(Global::light.begin(), Global::light.end(), 0x00);
}

/**
 * Round down to the nearest multiple of 8, e.g. floor8(-5) == 8
 */
static const inline int floorBiome(const int val)
{
	if (val < 0) {
		return ((val - (CHUNKS_PER_BIOME_FILE - 1)) / CHUNKS_PER_BIOME_FILE) * CHUNKS_PER_BIOME_FILE;
	}
	return (val / CHUNKS_PER_BIOME_FILE) * CHUNKS_PER_BIOME_FILE;
}

/**
 * Round down to the nearest multiple of 32, e.g. floor32(-5) == 32
 */
static const inline int floorRegion(const int val)
{
	if (val < 0) {
		return ((val - (REGIONSIZE - 1)) / REGIONSIZE) * REGIONSIZE;
	}
	return (val / REGIONSIZE) * REGIONSIZE;
}

/**
 * Load all the 8x8-chunks-files containing biome information
  No longer support biomes
void loadBiomeMap(const std::string& path)
{
	printf("Loading biome data...\n");
	const uint64_t size = g_MapsizeX * g_MapsizeZ;
	if (g_BiomeMapSize == 0 || size > g_BiomeMapSize) {
		if (g_BiomeMap != NULL) delete[] g_BiomeMap;
		g_BiomeMapSize = size;
		g_BiomeMap = new uint16_t[size];
	}
	memset(g_BiomeMap, 0, size * sizeof(uint16_t));
	//
	const int tmpMin = -floorBiome(g_FromChunkX);
	for (int x = floorBiome(g_FromChunkX); x <= floorBiome(g_ToChunkX); x += CHUNKS_PER_BIOME_FILE) {
		printProgress(size_t(x + tmpMin), size_t(floorBiome(g_ToChunkX) + tmpMin));
		for (int z = floorBiome(g_FromChunkZ); z <= floorBiome(g_ToChunkZ); z += CHUNKS_PER_BIOME_FILE) {
			loadBiomeChunk(path.c_str(), x, z);
		}
	}
	printProgress(10, 10);
}
*/

#define REGION_HEADER_SIZE REGIONSIZE * REGIONSIZE * 4
#define DECOMPRESSED_BUFFER 1000 * 1024
#define COMPRESSED_BUFFER 100 * 1024
/**
 * Load all the 32x32-region-files containing chunks information
 */
static bool loadAllRegions()
{
	if(world.regions.empty()) {
		__debugbreak(); //no regions loaded
		return false;
	}
	allocateTerrain();
	const size_t max = world.regions.size();
	size_t count = 0;
	std::cout << "Loading all chunks..\n";
	for (regionList::iterator it = world.regions.begin(); it != world.regions.end(); it++) {
		Region& region = (*it);
		printProgress(count++, max);
		int i;
		loadRegion(region.filename, true, i);
	}
	printProgress(10, 10);
	return true;
}

/**
 * Load all the 32x32 region files withing the specified bounds
 */
static bool loadTerrainRegion(const std::string& fromPath, int &loadedChunks)
{
	loadedChunks = 0;
	if (fromPath.empty()) {
		return false;
	}
	allocateTerrain();
	size_t maxlen = fromPath.length() + 40;
	std::string path;

	printf("Loading all chunks..\n");
	//
	const int tmpMin = -floorRegion(Global::FromChunkX);
	for (int x = floorRegion(Global::FromChunkX); x <= floorRegion(Global::ToChunkX); x += REGIONSIZE) {
		printProgress(size_t(x + tmpMin), size_t(floorRegion(Global::ToChunkX) + tmpMin));
		for (int z = floorRegion(Global::FromChunkZ); z <= floorRegion(Global::ToChunkZ); z += REGIONSIZE) {
			if (Global::worldFormat == ANVIL) {
				path = fromPath + "/region/r." + std::to_string(int(x / REGIONSIZE)) + '.' + std::to_string(int(z / REGIONSIZE)) + ".mca";
				loadRegion(path, false, loadedChunks);
			} else {
				__debugbreak(); //Loading non anvil world
				path = fromPath + "/region/r." + std::to_string(int(x / REGIONSIZE)) + '.' + std::to_string(int(z / REGIONSIZE)) + ".mcr";
				if (!loadRegion(path, false, loadedChunks)) {
					path = fromPath + "/region/r." + std::to_string(int(x / REGIONSIZE)) + '.' + std::to_string(int(z / REGIONSIZE)) + ".data";
					loadRegion(path, false, loadedChunks);
				}
			}
		}
	}
	return true;
}

/*
  TODO: Funktion umschreiben um fstream zu nutzen
 */
static bool loadRegion(const std::string& file, const bool mustExist, int &loadedChunks)
{
	//uint8_t buffer[COMPRESSED_BUFFER], decompressedBuffer[DECOMPRESSED_BUFFER];
	std::vector<uint8_t> buffer(COMPRESSED_BUFFER);
	std::vector<uint8_t> decompressedBuffer(DECOMPRESSED_BUFFER);
	FILE *rp = fopen(file.c_str(), "rb");
	if (rp == NULL) {
		if (mustExist) std::cerr << "Error opening region file " << file << '\n';
		return false;
	}
	if (fread(buffer.data(), 4, REGIONSIZE * REGIONSIZE, rp) != REGIONSIZE * REGIONSIZE) {
		std::cerr << "Header too short in " << file << '\n';
		fclose(rp);
		return false;
	}
	// Sort chunks using a map, so we access the file as sequential as possible
	chunkMap localChunks;
	for (uint32_t i = 0; i < REGION_HEADER_SIZE; i += 4) {
		const uint32_t offset = (ntoh<uint32_t>(&buffer[i], 3) >> 8) * 4096;
		if (offset == 0) continue;
		localChunks[offset] = i;
	}
	if (localChunks.size() == 0) {
		fclose(rp);
		return false;
	}
	z_stream zlibStream;
	for (chunkMap::iterator ci = localChunks.begin(); ci != localChunks.end(); ci++) {
		uint32_t offset = ci->first;
		// Not even needed. duh.
		//uint32_t index = ci->second;
		//int x = (**it).x + (index / 4) % REGIONSIZE;
		//int z = (**it).z + (index / 4) / REGIONSIZE;
		if (0 != fseek(rp, offset, SEEK_SET)) {
			printf("Error seeking to chunk in region file %s\n", file);
			continue;
		}
		if (1 != fread(buffer.data(), 5, 1, rp)) {
			printf("Error reading chunk size from region file %s\n", file);
			continue;
		}
		uint32_t len = ntoh<uint32_t>(buffer.data(), 4);
		uint8_t version = buffer[4];
		if (len == 0) continue;
		len--;
		if (len > COMPRESSED_BUFFER) {
			printf("Chunk too big in %s\n", file);
			continue;
		}
		if (fread(buffer.data(), 1, len, rp) != len) {
			printf("Not enough input for chunk in %s\n", file);
			continue;
		}
		if (version == 1 || version == 2) { // zlib/gzip deflate
			memset(&zlibStream, 0, sizeof(z_stream));
			zlibStream.next_out = (Bytef*)decompressedBuffer.data();
			zlibStream.avail_out = DECOMPRESSED_BUFFER;
			zlibStream.avail_in = len;
			zlibStream.next_in = (Bytef*)buffer.data();

			inflateInit2(&zlibStream, 32 + MAX_WBITS);
			int status = inflate(&zlibStream, Z_FINISH); // decompress in one step
			inflateEnd(&zlibStream);

			if (status != Z_STREAM_END) {
				printf("Error decompressing chunk from %s\n", file);
				continue;
			}

			len = zlibStream.total_out;
			if (len == 0) {
				std::cerr << "cold not decompress region! Error\n";
				__debugbreak();
			}
		} else {
			printf("Unsupported McRegion version: %d\n", (int)version);
			continue;
		}
		//__debugbreak(); //check if resize works
		decompressedBuffer.resize(len);
		if (loadChunk(decompressedBuffer)) {
			loadedChunks++;
		}
	}
	fclose(rp);
	return true;
}

/*
TODO: Funktion umschreiben um fstream zu nutzen
*/
static void loadBiomeChunk(const std::string& path, const int chunkX, const int chunkZ)
{
#	define BIOME_ENTRIES CHUNKS_PER_BIOME_FILE * CHUNKS_PER_BIOME_FILE * CHUNKSIZE_X * CHUNKSIZE_Z
#	define RECORDS_PER_LINE CHUNKSIZE_X * CHUNKS_PER_BIOME_FILE

	const size_t size = path.length() + 50;
	std::string file;
	file += path + "/b." + std::to_string(chunkX / CHUNKS_PER_BIOME_FILE) + "." + std::to_string(chunkZ / CHUNKS_PER_BIOME_FILE) + ".biome";
	//snprintf(file, size, "%s/b.%d.%d.biome", path, chunkX / CHUNKS_PER_BIOME_FILE, chunkZ / CHUNKS_PER_BIOME_FILE);
	if (!fileExists(file)) {
		std::cerr << file << " doesn't exist. Please update biome cache.\n";
		return;
	}
	FILE *fh = fopen(file.c_str(), "rb");
	std::vector<uint16_t> data(BIOME_ENTRIES);
	const bool success = (fread(data.data(), sizeof(uint16_t), BIOME_ENTRIES, fh) == BIOME_ENTRIES);
	fclose(fh);
	if (!success) {
		std::cerr << file << " seems to be truncated. Try rebuilding biome cache.\n";
	} else {
		const int fromX = Global::FromChunkX * CHUNKSIZE_X;
		const int toX   = Global::ToChunkX * CHUNKSIZE_X;
		const int fromZ = Global::FromChunkZ * CHUNKSIZE_Z;
		const int toZ   = Global::ToChunkZ * CHUNKSIZE_Z;
		const int offX  = chunkX * CHUNKSIZE_X;
		const int offZ  = chunkZ * CHUNKSIZE_Z;
		for (int z = 0; z < CHUNKSIZE_Z * CHUNKS_PER_BIOME_FILE; ++z) {
			if (z + offZ < fromZ || z + offZ >= toZ) continue;
			for (int x = 0; x < CHUNKSIZE_X * CHUNKS_PER_BIOME_FILE; ++x) {
				if (x + offX < fromX || x + offX >= toX) continue;
				if (Global::settings.orientation == North) {
					BIOMENORTH(x + offX - fromX, z + offZ - fromZ) = ntoh<uint16_t>(data[RECORDS_PER_LINE * z + x]);
				} else if (Global::settings.orientation == East) {
					BIOMEEAST(x + offX - fromX, z + offZ - fromZ) = ntoh<uint16_t>(data[RECORDS_PER_LINE * z + x]);
				} else if (Global::settings.orientation == South) {
					BIOMESOUTH(x + offX - fromX, z + offZ - fromZ) = ntoh<uint16_t>(data[RECORDS_PER_LINE * z + x]);
				} else {
					BIOMEWEST(x + offX - fromX, z + offZ - fromZ) = ntoh<uint16_t>(data[RECORDS_PER_LINE * z + x]);
				}
			}
		}
	}
}

static inline void assignBlock(const uint16_t &block, uint8_t* &targetBlock, int &x, int &y, int &z, const uint8_t* &justData, const uint8_t* &addData)
{
	//WorldFormat == 2
	uint8_t add = 0;
	uint8_t col = (justData[(x + (z + (y * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x % 2) * 4)) & 0xF;
	if (addData != 0)
	{
		add = ( addData[(x + (z + (y * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x % 2) * 4)) & 0xF;
	}
	if (!Global::settings.lowMemory) //additional data
	{
		*(targetBlock + Global::Terrainsize) = (add) + (col << 4);
		*targetBlock++ = block;
	}
	else //convert to 256-colors format
	{
		if (colorsToMap[block + (add << 8) + (col << 12)] == 0)
			*targetBlock++ = colorsToMap[block + (add << 8)];
		else
			*targetBlock++ = colorsToMap[block + (add << 8) + (col << 12)];
	}
}

static inline void assignBlock(const uint16_t &block, uint8_t* &targetBlock, int &x, int &y, int &z, uint8_t* &justData)
{
	//WorldFormat != 2
	uint8_t col = (justData[(y + (z + (x * CHUNKSIZE_Z)) * CHUNKSIZE_Y) / 2] >> ((y % 2) * 4)) & 0xF;
	if (!Global::settings.lowMemory) //additional data
	{
		*(targetBlock + Global::Terrainsize) = (col << 4);
		*targetBlock++ = block;
	}
	else //convert to 256-colors format
	{
		if (colorsToMap[block + (col << 12)] == 0)
			*targetBlock++ = colorsToMap[block];
		else
			*targetBlock++ = colorsToMap[block + (col << 12)];
	}
}

static inline void lightCave(const int x, const int y, const int z)
{
	for (int ty = y - 9; ty < y + 9; ty+=2) { // The trick here is to only take into account
		const int oty = ty - Global::MapminY;
		if (oty < 0) {
			continue;   // areas around torches.
		}
		if (oty >= Global::MapsizeY) {
			break;
		}
		for (int tz = z - 18; tz < z + 18; ++tz) {
			if (tz < CHUNKSIZE_Z) {
				continue;
			}
			for (int tx = x - 18; tx < x + 18; ++tx) {
				if (tx < CHUNKSIZE_X) {
					continue;
				}
				if (Global::settings.orientation == East) {
					if (tx >= int(Global::MapsizeZ)-CHUNKSIZE_Z) {
						break;
					}
					if (tz >= int(Global::MapsizeX)-CHUNKSIZE_X) {
						break;
					}
					SETLIGHTEAST(tx, oty, tz) = 0xFF;
				} else if (Global::settings.orientation == North) {
					if (tx >= int(Global::MapsizeX)-CHUNKSIZE_X) {
						break;
					}
					if (tz >= int(Global::MapsizeZ)-CHUNKSIZE_Z) {
						break;
					}
					SETLIGHTNORTH(tx, oty, tz) = 0xFF;
				} else if (Global::settings.orientation == South) {
					if (tx >= int(Global::MapsizeX)-CHUNKSIZE_X) {
						break;
					}
					if (tz >= int(Global::MapsizeZ)-CHUNKSIZE_Z) {
						break;
					}
					SETLIGHTSOUTH(tx, oty, tz) = 0xFF;
				} else {
					if (tx >= int(Global::MapsizeZ)-CHUNKSIZE_Z) {
						break;
					}
					if (tz >= int(Global::MapsizeX)-CHUNKSIZE_X) {
						break;
					}
					SETLIGHTWEST(tx , oty, tz) = 0xFF;
				}
			}
		}
	}
}

void uncoverNether()
{
	const int cap = (Global::MapsizeY - Global::MapminY) - 57;
	const int to = (Global::MapsizeY - Global::MapminY) - 52;
	printf("Uncovering Nether...\n");
	for (size_t x = CHUNKSIZE_X; x < Global::MapsizeX - CHUNKSIZE_X; ++x) {
		printProgress(x - CHUNKSIZE_X, Global::MapsizeX);
		for (size_t z = CHUNKSIZE_Z; z < Global::MapsizeZ - CHUNKSIZE_Z; ++z) {
			// Remove blocks on top, otherwise there is not much to see here
			int massive = 0;
			uint8_t *bp = &Global::terrain[((z + (x * Global::MapsizeZ) + 1) * Global::MapsizeY) - 1];
			int i;
			for (i = 0; i < to; ++i) { // Go down 74 blocks from the ceiling to see if there is anything except solid
				if (massive && (*bp == AIR || *bp == LAVA || *bp == STAT_LAVA)) {
					if (--massive == 0) {
						break;   // Ignore caves that are only 2 blocks high
					}
				}
				if (*bp != AIR && *bp != LAVA && *bp != STAT_LAVA) {
					massive = 3;
				}
				--bp;
			}
			// So there was some cave or anything before going down 70 blocks, everything above will get removed
			// If not, only 45 blocks starting at the ceiling will be removed
			if (i > cap) {
				i = cap - 25;   // TODO: Make this configurable
			}
			bp = &Global::terrain[((z + (x * Global::MapsizeZ) + 1) * Global::MapsizeY) - 1];
			for (int j = 0; j < i; ++j) {
				*bp-- = AIR;
			}
		}
	}
	printProgress(10, 10);
}
