#include "worldloader.h"
#include "filesystem.h"
#include "nbt.h"
#include "colors.h"
#include "globals.h"
#include <string>
#include <zlib.h>
#include <iostream>
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
bool loadChunk(const std::vector<uint8_t>& buffer);
bool loadAnvilChunk(NBT_Tag* const level, const int32_t chunkX, const int32_t chunkZ);
bool load113Chunk(NBT_Tag* const level, const int32_t chunkX, const int32_t chunkZ);
void allocateTerrain();
bool loadAllRegions();
bool loadRegion(const std::string& file, const bool mustExist, int &loadedChunks);
bool loadTerrainRegion(const std::string& fromPath, int &loadedChunks);
inline void assignBlock(const uint8_t &source, uint8_t* &dest, int &x, int &y, int &z, const uint8_t* &justData);
inline void assignBlock(const uint8_t &source, uint8_t* &dest, int &x, int &y, int &z, const uint8_t* &justData, const uint8_t* &addData);
inline void lightCave(const int x, const int y, const int z);

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

/*
 Calc size of Map, if no limit is set
*/
bool scanWorldDirectory(const std::string& fromPath)
{
	// OK go
	world.regions.clear();
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
			//std::cout << "Scan region: " << region.filename << ' ' << std::to_string(offset) + ' ' + std::to_string(i);
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

bool loadChunk(const std::vector<uint8_t>& buffer) //uint8_t* buffer, const size_t streamLen
{
	bool ok = false;
	if (buffer.size() == 0) { // File
		std::cerr << "No data in NBT file.\n";
		return false;
	}
	NBT chunk(buffer);
	if (!chunk.good()) {
		std::cerr << "Error loading chunk.\n";
		__debugbreak();
		return false; // chunk does not exist
	}

	int32_t dataVersion = 0;
	if (!chunk.getInt("DataVersion", dataVersion)) {
		std::cerr << "No DataVersion in Chunk";
		return false;
	}

	NBT_Tag *level = NULL;
	ok = chunk.getCompound("Level", level);
	if (!ok) {
		printf("No level\n");
		__debugbreak();
		return false;
	}
	int32_t chunkX, chunkZ;
	ok = level->getInt("xPos", chunkX);
	ok = ok && level->getInt("zPos", chunkZ);
	if (!ok) {
		printf("No pos\n");
		__debugbreak();
		return false;
	}

	// Check if chunk is in desired bounds (not a chunk where the filename tells a different position)
	if (chunkX < Global::FromChunkX || chunkX >= Global::ToChunkX || chunkZ < Global::FromChunkZ || chunkZ >= Global::ToChunkZ) {
		if (!chunk.good()) printf("Chunk is out of bounds. %d %d\n", chunkX, chunkZ);
		//delete chunk;
		return false; // Nope, its not...
	}

	if (dataVersion > 1343) {
		return load113Chunk(level, chunkX, chunkZ);
	}else{
		std::cerr << "trying to load old Anvil World. Currently not supported\n";
		return false;
		return loadAnvilChunk(level, chunkX, chunkZ);
	}
}

//TODO: rewrite to use new color system
bool loadAnvilChunk(NBT_Tag * const level, const int32_t chunkX, const int32_t chunkZ)
{
#pragma region CHUKLOADING
/*
	PrimArray<uint8_t> *blockdata, *lightdata, *skydata, *justData, *addData = 0;
	int32_t len, yoffset, yoffsetsomething = (Global::MapminY + SECTION_Y * 10000) % SECTION_Y;
	int8_t yo;
	std::list<NBT_Tag*> *sections = NULL;
	bool ok;
	//
	ok = level->getList("Sections", sections);
	if (!ok) {
		std::cerr << "No sections found in region\n";
		return false;
	}
	//
	const int offsetz = (chunkZ - Global::FromChunkZ) * CHUNKSIZE_Z; //Blocks into world, from lowets point
	const int offsetx = (chunkX - Global::FromChunkX) * CHUNKSIZE_X; //Blocks into world, from lowets point
	for (std::list<NBT_Tag *>::iterator it = sections->begin(); it != sections->end(); it++) {
		NBT_Tag *section = *it;
		ok = section->getByte("Y", yo);
		if (!ok) {
			std::cerr << "Y-Offset not found in section\n";
			return false;
		}
		if (yo < Global::sectionMin || yo > Global::sectionMax) continue;
		yoffset = (SECTION_Y * (int)(yo - Global::sectionMin)) - yoffsetsomething; //Blocks into redner zone in Y-Axis
		if (yoffset < 0) yoffset = 0;
		ok = section->getByteArray("Blocks", blockdata);
		if(ok) len = blockdata->_len;
		if (!ok || len < CHUNKSIZE_X * CHUNKSIZE_Z * SECTION_Y) {
			std::cerr << "No blocks\n";
			return false;
		}
		ok = section->getByteArray("Data", justData); //Metadata
		if(ok) len = justData->_len;
		if (!ok || len < (CHUNKSIZE_X * CHUNKSIZE_Z * SECTION_Y) / 2) {
			std::cerr << "No block data\n";
			return false;
		}
		ok = section->getByteArray("Add", addData);
		if(ok) len = addData->_len;
		if (Global::settings.nightmode || Global::settings.skylight) { // If nightmode, we need the light information too
			ok = section->getByteArray("BlockLight", lightdata);
			if (!ok || len < (CHUNKSIZE_X * CHUNKSIZE_Z * SECTION_Y) / 2) {
				std::cerr << "No block light\n";
				return false;
			}
		}
		if (Global::settings.skylight) { // Skylight desired - wish granted
			ok = section->getByteArray("SkyLight", skydata);
			len = skydata->_len;
			if (!ok || len < (CHUNKSIZE_X * CHUNKSIZE_Z * SECTION_Y) / 2) {
				return false;
			}
		}
		// Copy data
		for (int x = 0; x < CHUNKSIZE_X; ++x) {
			for (int z = 0; z < CHUNKSIZE_Z; ++z) {
				uint16_t* targetBlock = nullptr;
				uint8_t *lightByte = nullptr;
				if (Global::settings.orientation == East) {
					targetBlock = &BLOCKEAST(x + offsetx, yoffset, z + offsetz); //BLOCKEAST
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
					const uint8_t &block = blockdata->_data[x + (z + (y * CHUNKSIZE_Z)) * CHUNKSIZE_X]; //Block-ID (0,..,255)
					const uint8_t* tmpAddData = addData ? addData->_data : nullptr;
					assignBlock(block, targetBlock, x, y, z, justData->_data, tmpAddData);
					// Light
					if (Global::settings.underground) {
						if (block == TORCH) {
							if (y + yoffset < Global::MapminY) continue;
							std::cout << "Torch at " << std::to_string(x + offsetx) << ' ' << std::to_string(yoffset + y) << ' ' << std::to_string(z + offsetz) << '\n';
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


	}
	*/
#pragma endregion
	return true;
}

bool load113Chunk(NBT_Tag* const level, const int32_t chunkX, const int32_t chunkZ)
{
	//
	return false;
}

uint64_t calcTerrainSize(const int chunksX, const int chunksZ)
{
	uint64_t size = uint64_t(chunksX+2) * CHUNKSIZE_X * uint64_t(chunksZ+2) * CHUNKSIZE_Z * uint64_t(Global::MapsizeY);

	if (Global::settings.nightmode || Global::settings.underground || Global::settings.blendUnderground || Global::settings.skylight) {
			size += (2*size) / 4;
	}
	/* biomes no longer supported
	if (g_UseBiomes) {
		size += uint64_t(chunksX+2) * CHUNKSIZE_X * uint64_t(chunksZ+2) * CHUNKSIZE_Z * sizeof(uint16_t);
	}*/
	return size;
}
/*
	Berechnet Überschnitt auf allen 4 Seiten
*/
void calcBitmapOverdraw(int &left, int &right, int &top, int &bottom)
{
	top = left = bottom = right = 0x0fffffff;
	int val, x, z;
	pointList::iterator itP;
	itP = world.points.begin();
	
	for (;;) {

		if (itP == world.points.end()) break;
		x = (*itP).x; //x-Coordinate des Chunks
		z = (*itP).z; //z-Coordinate des Chunks

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
			val = ((Global::ToChunkZ - 1) - z) * CHUNKSIZE_Z * 2 + ((Global::ToChunkX - 1) - x) * CHUNKSIZE_X * 2; //Doppelte Distanz zwischen aktuellem chunk und Bis-rendergrenze (X,Z Seiten addiert)
			if (val < right) {
				right = val;
			}
			// Left
			val = ((x - Global::FromChunkX) * CHUNKSIZE_X) * 2 +  + ((z - Global::FromChunkZ) * CHUNKSIZE_Z) * 2;//Doppelte Distanz zwischen aktuellem chunk und Von-rendergrenze (X,Z Seiten addiert)
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
		itP++;
	}
}

void allocateTerrain()
{
	Global::terrain.clear();
	Global::light.clear();
	Global::heightMap.clear();
	Global::biomeMap.clear();

	Global::terrain.shrink_to_fit();
	Global::light.shrink_to_fit();
	Global::heightMap.shrink_to_fit();
	Global::biomeMap.shrink_to_fit();
	/* biomes no longer supported
	if (Global::useBiomes) //&& worldFormat == 2
	{
	    Global::biomeMapSize = Global::MapsizeX * Global::MapsizeZ;
		Global::biomeMap.resize(Global::biomeMapSize, 0);
	}*/
	Global::heightMap.resize(Global::MapsizeX * Global::MapsizeZ, 0);
	//printf("%d -- %d\n", g_MapsizeX, g_MapsizeZ); //dimensions of terrain map (in memory)
	Global::Terrainsize = Global::MapsizeX * Global::MapsizeY * Global::MapsizeZ;

	std::cout << "Terrain takes up " << std::to_string(float(Global::Terrainsize / float(1024 * 1024))) << "MiB";
	Global::terrain.resize(Global::Terrainsize, 0);  // Preset: Air

	if (Global::settings.nightmode || Global::settings.underground || Global::settings.blendUnderground || Global::settings.skylight) {
		Global::lightsize = Global::MapsizeZ * Global::MapsizeX * ((Global::MapsizeY + (Global::MapminY % 2 == 0 ? 1 : 2)) / 2);
		std::cout << ", lightmap " << std::to_string(float(Global::lightsize / float(1024 * 1024))) << "MiB";
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
	std::cout << "\n";
}

void clearLightmap()
{
	std::fill(Global::light.begin(), Global::light.end(), 0x00);
}

/**
 * Round down to the nearest multiple of 8, e.g. floor8(-5) == 8
 */
const inline int floorBiome(const int val)
{
	if (val < 0) {
		return ((val - (CHUNKS_PER_BIOME_FILE - 1)) / CHUNKS_PER_BIOME_FILE) * CHUNKS_PER_BIOME_FILE;
	}
	return (val / CHUNKS_PER_BIOME_FILE) * CHUNKS_PER_BIOME_FILE;
}

/**
 * Round down to the nearest multiple of 32, e.g. floor32(-5) == 32
 */
const inline int floorRegion(const int val)
{
	if (val < 0) {
		return ((val - (REGIONSIZE - 1)) / REGIONSIZE) * REGIONSIZE;
	}
	return (val / REGIONSIZE) * REGIONSIZE;
}

#define REGION_HEADER_SIZE REGIONSIZE * REGIONSIZE * 4
#define DECOMPRESSED_BUFFER 1000 * 1024
#define COMPRESSED_BUFFER 100 * 1024
/**
 * Load all the 32x32-region-files containing chunks information
 */
bool loadEntireTerrain()
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
bool loadTerrain(const std::string& fromPath, int &loadedChunks)
{
	loadedChunks = 0;
	if (fromPath.empty()) {
		return false;
	}
	allocateTerrain();
	std::string path;

	std::cout << "Loading all chunks..\n";
	//
	const int tmpMin = -floorRegion(Global::FromChunkX);
	for (int x = floorRegion(Global::FromChunkX); x <= floorRegion(Global::ToChunkX); x += REGIONSIZE) {
		printProgress(size_t(x + tmpMin), size_t(floorRegion(Global::ToChunkX) + tmpMin));
		for (int z = floorRegion(Global::FromChunkZ); z <= floorRegion(Global::ToChunkZ); z += REGIONSIZE) {
			path = fromPath + "/region/r." + std::to_string(int(x / REGIONSIZE)) + '.' + std::to_string(int(z / REGIONSIZE)) + ".mca";
			loadRegion(path, false, loadedChunks);
		}
	}
	return true;
}

/*
 */
bool loadRegion(const std::string& file, const bool mustExist, int &loadedChunks)
{
	std::vector<uint8_t> buffer(COMPRESSED_BUFFER);
	std::vector<uint8_t> decompressedBuffer(DECOMPRESSED_BUFFER);
	std::ifstream rp(file, std::ios::binary);
	if (rp.fail()) {
		if (mustExist) std::cerr << "Error opening region file " << file << '\n';
		return false;
	}
	rp.read((char*)buffer.data(), REGIONSIZE * REGIONSIZE * 4);
	if (rp.fail()) {
		std::cerr << "Header too short in " << file << '\n';
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
		return false;
	}
	z_stream zlibStream;
	for (chunkMap::iterator ci = localChunks.begin(); ci != localChunks.end(); ci++) {
		uint32_t offset = ci->first;
		//if (offset == 81920) __debugbreak(); //chunk[14, 28] in World at(14, -4) in file r.0.-1.mca
		// Not even needed. duh.
		//uint32_t index = ci->second;
		//int x = (**it).x + (index / 4) % REGIONSIZE;
		//int z = (**it).z + (index / 4) / REGIONSIZE;
		rp.seekg(offset);
		if (rp.fail()) {
			std::cerr << "Error seeking to chunk in region file " << file << '\n';
			continue;
		}
		rp.read((char*)buffer.data(), 5);
		if (rp.fail()) {
			std::cerr << "Error reading chunk size from region file " << file << '\n';
			continue;
		}
		uint32_t len = ntoh<uint32_t>(buffer.data(), 4);
		uint8_t version = buffer[4];
		if (len == 0) continue;
		len--;
		if (len > COMPRESSED_BUFFER) {
			std::cerr << "Chunk too big in " << file << '\n';
			continue;
		}
		rp.read((char*)buffer.data(), len);
		if (rp.fail()) {
			std::cerr << "Not enough input for chunk in " << file << '\n';
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
				std::cerr << "Error decompressing chunk from " << file << '\n';
				continue;
			}

			len = zlibStream.total_out;
			if (len == 0) {
				std::cerr << "cold not decompress region! Error\n";
				__debugbreak();
			}
		} else {
			std::cerr << "Unsupported McRegion version: " << (int)version << '\n';
			continue;
		}
		//__debugbreak(); //check if resize works
		std::vector<uint8_t> buf(decompressedBuffer.begin(), decompressedBuffer.begin() + len);
		if (loadChunk(buf)) {
			loadedChunks++;
		}
	}
	return true;
}

inline void assignBlock(const uint8_t &block, uint8_t* &targetBlock, int &x, int &y, int &z, const uint8_t* &justData, const uint8_t* &addData)
{
	//WorldFormat == 2
	uint8_t add = 0;
	uint8_t col = (justData[(x + (z + (y * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x % 2) * 4)) & 0xF; //Get the 4Bits of MetaData
	if (addData != 0)
	{
		add = ( addData[(x + (z + (y * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x % 2) * 4)) & 0xF;
	}

	//additional data
	*(targetBlock + Global::Terrainsize) = (add) + (col << 4);
	*targetBlock++ = block; //first write, then increment
}

inline void assignBlock(const uint8_t &block, uint8_t* &targetBlock, int &x, int &y, int &z, uint8_t* &justData)
{
	//WorldFormat != 2
	uint8_t col = (justData[(y + (z + (x * CHUNKSIZE_Z)) * CHUNKSIZE_Y) / 2] >> ((y % 2) * 4)) & 0xF;  //Get the 4Bits of MetaData

	//additional data
	*(targetBlock + Global::Terrainsize) = (col << 4);
	*targetBlock++ = block;
}

inline void lightCave(const int x, const int y, const int z)
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
	std::cout << "Uncovering Nether...\n";
	for (size_t x = CHUNKSIZE_X; x < Global::MapsizeX - CHUNKSIZE_X; ++x) {
		printProgress(x - CHUNKSIZE_X, Global::MapsizeX);
		for (size_t z = CHUNKSIZE_Z; z < Global::MapsizeZ - CHUNKSIZE_Z; ++z) {
			// Remove blocks on top, otherwise there is not much to see here
			int massive = 0;
			uint16_t* bp = &Global::terrain[((z + (x * Global::MapsizeZ) + 1) * Global::MapsizeY) - 1];
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
