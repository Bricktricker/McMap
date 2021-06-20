#define NOMINMAX
#include <string>
#include <zlib.h>
#include <iostream>
#include <cassert>
#include <fstream>
#include <iomanip>
#include <limits>
#include <cstring>
#include <algorithm>
#include <filesystem>

#include "ThreadPool.h"
#include "worldloader.h"
#include "filesystem.h"
#include "nbt.h"
#include "colors.h"
#include "helper.h"

namespace
{
	static terrain::World world;
}

namespace terrain
{
	size_t getPalletIndex(const std::vector<uint64_t>& arr, const size_t index, const bool denselyPacked);
	bool loadChunk(const std::vector<uint8_t>& buffer);
	bool load113Chunk(const NBTtag* level, const int32_t chunkX, const int32_t chunkZ, const size_t dataVersion);
	void allocateTerrain();
	bool loadRegion(const std::string& file, const bool mustExist, int &loadedChunks);
	inline void lightCave(const int x, const int y, const int z);

	WorldFormat getWorldFormat(const std::string& worldPath)
	{
		WorldFormat format = ALPHA; // alpha (single chunk files)
		std::string path = worldPath;
		path.append("/region");

		for (const auto& itr : std::filesystem::directory_iterator(path)) {
			const std::string fileNameStr = itr.path().filename().generic_string();
			if (helper::strEndsWith(fileNameStr, ".mca")) {
				format = ANVIL;
				break;
			} else if (format != REGION && helper::strEndsWith(fileNameStr, ".mcr")) {
				format = REGION;
			}
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
		Global::ToChunkX = Global::ToChunkZ = -10000000;

		// Read subdirs now
		std::string path(fromPath);
		path.append("/region");
		std::cout << "Scanning world...\n";
		for (const auto& itr : std::filesystem::directory_iterator(path)) {
			if (itr.is_directory()) {
				continue;
			}

			std::string regionStr = itr.path().filename().generic_string();
			if (regionStr[0] == 'r' && regionStr[1] == '.') { // Make sure filename is a region
				if (helper::strEndsWith(regionStr, ".mca")) {
					// Extract x coordinate from region filename
					const auto s = regionStr.substr(2);
					const auto values = helper::strSplit(s, '.');
					assert(values.size() == 3);

					const int valX = std::stoi(values.at(0)) * REGIONSIZE;
					// Extract z coordinate from region filename
					const int valZ = std::stoi(values.at(1)) * REGIONSIZE;
					if (valX > -4000 && valX < 4000 && valZ > -4000 && valZ < 4000) {
						std::string full = path + "/" + regionStr;
						world.regions.push_back(Region(full, valX, valZ));
					} else {
						std::cerr << "Ignoring bad region at " << valX << ' ' + valZ << '\n';
					}
				}
			}
		}

		// Read all region files' headers to figure out which chunks actually exist
		// It would be sufficient to just do this on those which form the edge
		// Have yet to find out how slow this is on big maps to see if it's worth the effort
		world.points.clear();

		for (regionList::iterator it = world.regions.begin(); it != world.regions.end(); ++it) {
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
			fh.read(reinterpret_cast<char*>(buffer.data()), 4 * REGIONSIZE * REGIONSIZE);
			if (fh.fail()) {
				std::cerr << "Could not read header from " << region.filename << '\n';
				region.filename.clear();
				continue;
			}
			// Check for existing chunks in region and update bounds
			for (size_t i = 0; i < REGIONSIZE * REGIONSIZE; ++i) {
				const uint32_t offset = (helper::swap_endian<uint32_t>(buffer[i] + (buffer[i + 1] << 8) + (buffer[i + 2] << 16)) >> 8) * 4096;
				if (offset == 0) continue;

				const int valX = region.x + static_cast<int>(i % REGIONSIZE);
				const int valZ = region.z + static_cast<int>(i / REGIONSIZE);
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
		return true;
	}

	bool loadChunk(const std::vector<uint8_t>& buffer)
	{
		if (buffer.size() == 0) { // File
			std::cerr << "No data in NBT file.\n";
			return false;
		}
		NBT chunk(buffer);
		if (!chunk.good()) {
			std::cerr << "Error loading chunk.\n";
			return false; // chunk does not exist
		}

		const auto dataVersionOpt = chunk.getInt("DataVersion");
		if (!dataVersionOpt.has_value()) {
			std::cerr << "No DataVersion in Chunk\n";
			return false;
		}
		const size_t dataVersion = static_cast<size_t>(dataVersionOpt.value());

		const auto levelOpt = chunk.getCompound("Level");
		if (!levelOpt.has_value()) {
			std::cerr << "No level\n";
			return false;
		}
		const auto level = levelOpt.value();

		const auto chunkXOpt = level->getInt("xPos");
		const auto chunkZOpt = level->getInt("zPos");
		if (!chunkXOpt.has_value() || !chunkZOpt.has_value()) {
			std::cerr << "No pos\n";
			return false;
		}
		const int32_t chunkX = chunkXOpt.value();
		const int32_t chunkZ = chunkZOpt.value();

		// Check if chunk is in desired bounds (not a chunk where the filename tells a different position)
		if (chunkX < Global::FromChunkX || chunkX >= Global::ToChunkX || chunkZ < Global::FromChunkZ || chunkZ >= Global::ToChunkZ) {
			return false; // Nope, its not...
		}

		//Check for version > 1.12.2
		if (dataVersion > 1343) {
			/*
			1.13.x status types: empty, base, carved, liquid_carved, decorated, lighted, mobs_spawned, finalized, fullchunk, postprocessed
			1.14.x status types: empty, structure_starts, structure_references, biomes, noise, surface, carvers, liquid_carvers, features, light, spawn, heightmaps, full
			*/

			const auto statusOpt = level->getString("Status");
			if (!statusOpt.has_value()) {
				std::cerr << "could not find Status in Chunk\n";
				return false;
			}
			const std::string status = statusOpt.value();
			//Check if we use light
			if (Global::light.empty()) {
				if (status != "empty") {
					return load113Chunk(level, chunkX, chunkZ, dataVersion);
				}
			} else {
				if (dataVersion > 1631) { //1.13.2
					return load113Chunk(level, chunkX, chunkZ, dataVersion); //try to load them in 1.14.x 
				} else {
					if (status >= "finalized" && status != "liquid_carved") {
						return load113Chunk(level, chunkX, chunkZ, dataVersion);
					}
				}
			}
			return false;
		} else {
			static bool showedWarning = false;
			if (!showedWarning) {
				std::cerr << "found chunk in 1.12.2 or older format, this is no longer supported. Update to 1.13.2+\n";
				showedWarning = true;
			}

			return false;
		}
	}

	//Loads 1.13.2+ chunks
	bool load113Chunk(const NBTtag* level, const int32_t chunkX, const int32_t chunkZ, const size_t dataVersion)
	{
		const auto sectionsOpt = level->getList("Sections");
		if (!sectionsOpt.has_value()) {
			std::cerr << "No sections found in region\n";
			return false;
		}
		const auto sections = sectionsOpt.value();
		if (sections->empty())
			return false;

		const int offsetz = (chunkZ - Global::FromChunkZ) * CHUNKSIZE_Z; //Blocks into world, from lowest point
		const int offsetx = (chunkX - Global::FromChunkX) * CHUNKSIZE_X; //Blocks into world, from lowest point
		const size_t yoffsetsomething = (Global::MapminY + SECTION_Y * 10000) % SECTION_Y;
		assert(yoffsetsomething == 0); //I don't now what this variable does. Always 0

		for (const auto sec : *sections) {
			const auto yOffsetOpt = sec.getByte("Y");
			if (!yOffsetOpt.has_value()) {
				std::cerr << "Y-Offset not found in section\n";
				return false;
			}
			const int32_t yo = yOffsetOpt.value();

			if (yo < Global::sectionMin || yo > Global::sectionMax) continue; //sub-Chunk out of bounds, continue
			int32_t yoffset = (SECTION_Y * (yo - Global::sectionMin)) - static_cast<int32_t>(yoffsetsomething); //Blocks into render zone in Y-Axis
			if (yoffset < 0) yoffset = 0;

			const auto blockStatesOpt = sec.getLongArray("BlockStates");
			if (!blockStatesOpt.has_value()) {
				continue;
			}
			const uint64_t* beginPtr = reinterpret_cast<const uint64_t*>(blockStatesOpt.value().m_data);
			std::vector<uint64_t> blockStates(beginPtr, beginPtr + blockStatesOpt.value().m_len);

			PrimArray<uint8_t> lightdata;
			if (Global::settings.nightmode || Global::settings.skylight) { // If nightmode, we need the light information too
				// If there is no light in this section the byte array ist not stored
				const auto lightdataOpt = sec.getByteArray("BlockLight");
				if (lightdataOpt.has_value()) {
					lightdata = lightdataOpt.value();
				}
			}

			PrimArray<uint8_t> skydata;
			if (Global::settings.skylight) {
				// If there is no light in this section the byte array ist not stored
				const auto skydataOpt = sec.getByteArray("SkyLight");
				if (skydataOpt.has_value()) {
					skydata = skydataOpt.value();
				}
			}

			const auto paletteOpt = sec.getList("Palette");
			if (!paletteOpt.has_value()) {
				continue;
			}
			const auto palette = paletteOpt.value();

			std::vector<StateID_t> idList;
			for (const auto state : *palette) {
				const auto blockNameOpt = state.getString("Name");
				if (!blockNameOpt.has_value()) {
					std::cerr << "State has no name\n";
					continue;
				}
				const std::string blockName = blockNameOpt.value();

				if (Global::blockTree.find(blockName) == Global::blockTree.end()) {
					std::cerr << blockName << " is missing in your colors file!\n";
					idList.push_back(AIR);
					continue;
				}

				const auto propertiesOpt = state.getCompound("Properties");
				StateID_t blockID = 0;
				if (propertiesOpt.has_value()) {
					//has complex properties
					const auto properties = propertiesOpt.value();

					const auto& tree = Global::blockTree.at(blockName);
					const auto& order = tree.getOrder();
					std::vector<std::string> stateValues;

					for (const auto& propName : order) {
						const auto propValue = properties->getString(propName);
						if (!propValue.has_value()) {
							std::cerr << "blockstate " << propName << " does not exist for block " << blockName << '\n';
							stateValues.clear();
							blockID = 0;
							break;
						}
						stateValues.push_back(propValue.value());
					}

					try {
						blockID = tree.get(stateValues);
					}
					catch (std::out_of_range&) {
						std::cerr << "Loaded blockstates for " << blockName << " differ from defined blockstates in your colors file\n";
					}

				} else {
					//Simple Block, no extra properties
					const auto& tree = Global::blockTree.at(blockName);
					blockID = tree.get();
				}
				idList.push_back(blockID);
			}

			//Now IDList is build up, no run through all block in sub-Chunk
			for (int x = 0; x < CHUNKSIZE_X; ++x) {
				for (int z = 0; z < CHUNKSIZE_Z; ++z) {
					StateID_t* targetBlock = nullptr;
					uint8_t* lightByte = nullptr;

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
					//set targetBlock
					for (size_t y = 0; y < SECTION_Y; ++y) {
						// In bounds check
						if (Global::sectionMin == yo && y < yoffsetsomething) continue;
						if (Global::sectionMax == yo && y + yoffset >= Global::MapsizeY) break;

						const size_t block1D = x + (z + (y * CHUNKSIZE_Z)) * CHUNKSIZE_X;
						const size_t IDLIstIndex = getPalletIndex(blockStates, block1D, dataVersion < 2529); //snapshot 20w17a = data version 2529
						const StateID_t block = idList[IDLIstIndex];
						*targetBlock = block;
						targetBlock++;
						// Light
						if (Global::settings.underground) {
							if (helper::isTorch(block)) {
								if (y + yoffset < static_cast<size_t>(Global::MapminY)) continue;
								//std::cout << "Torch at " << std::to_string(x + offsetx) << ' ' << std::to_string(yoffset + y) << ' ' << std::to_string(z + offsetz) << '\n';
								lightCave(x + offsetx, yoffset + static_cast<int>(y), z + offsetz);
							}
						} else if (Global::settings.skylight && (y & 1) == 0) {
							const uint8_t highlight = lightdata.m_len > 0 ? ((lightdata.m_data[(x + (z + ((y + 1) * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x & 1) * 4)) & 0x0F) : 0;
							const uint8_t lowlight = lightdata.m_len > 0 ? ((lightdata.m_data[(x + (z + (y * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x & 1) * 4)) & 0x0F) : 0;
							uint8_t highsky = skydata.m_len > 0 ? ((skydata.m_data[(x + (z + ((y + 1) * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x & 1) * 4)) & 0x0F) : 0;
							uint8_t lowsky = skydata.m_len > 0 ? ((skydata.m_data[(x + (z + (y * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x & 1) * 4)) & 0x0F) : 0;
							if (Global::settings.nightmode) {
								highsky = helper::clamp(highsky / 3 - 2);
								lowsky = helper::clamp(lowsky / 3 - 2);
							}
							*lightByte++ = ((std::max(highlight, highsky) & 0x0F) << 4) | (std::max(lowlight, lowsky) & 0x0F);
						} else if (Global::settings.nightmode && (y & 1) == 0) {
							if (lightdata.m_len > 0) {
								*lightByte++ = ((lightdata.m_data[(x + (z + (y * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x & 1) * 4)) & 0x0F)
									| ((lightdata.m_data[(x + (z + ((y + 1) * CHUNKSIZE_Z)) * CHUNKSIZE_X) / 2] >> ((x & 1) * 4)) << 4);
							} else {
								*lightByte++ = 0;
							}
						}
					} //for y
				} //for z
			} //for x

		}

		return true;
	}

	uint64_t calcTerrainSize(const size_t chunksX, const size_t chunksZ)
	{
		uint64_t size = sizeof(StateID_t) * (chunksX + 2) * CHUNKSIZE_X * (chunksZ + 2) * CHUNKSIZE_Z * (Global::MapsizeY);

		if (Global::settings.nightmode || Global::settings.underground || Global::settings.blendUnderground || Global::settings.skylight) {
			size += size / 4;
		}

		return size;
	}

	/*
		Calculates overdraw on all 4 sites
	*/
	void calcBitmapOverdraw(int &left, int &right, int &top, int &bottom)
	{
		top = left = bottom = right = 0x0fffffff;
		int val;

		for (pointList::iterator itP = world.points.begin(); itP != world.points.end(); ++itP) {

			int x = (*itP).x; //x-Coordinate
			int z = (*itP).z; //z-Coordinate

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
				val = ((x - Global::FromChunkX) * CHUNKSIZE_X) * 2 + +((z - Global::FromChunkZ) * CHUNKSIZE_Z) * 2;//Doppelte Distanz zwischen aktuellem chunk und Von-rendergrenze (X,Z Seiten addiert)
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
				val = ((x - Global::FromChunkX) * CHUNKSIZE_X) * 2 + +((z - Global::FromChunkZ) * CHUNKSIZE_Z) * 2;
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
		}
	}

	void allocateTerrain()
	{
		const size_t heightMapSize = Global::MapsizeX * Global::MapsizeZ;
		Global::Terrainsize = Global::MapsizeX * Global::MapsizeY * Global::MapsizeZ;

		if (Global::heightMap.size() < heightMapSize) {
			Global::heightMap.clear();
			Global::heightMap.shrink_to_fit();
			Global::heightMap.resize(heightMapSize);
		}
		std::fill_n(Global::heightMap.begin(), heightMapSize, static_cast<uint16_t>(0xff00));

		std::cout << "Terrain takes up " << std::setprecision(5) << float(Global::Terrainsize * sizeof(StateID_t) / float(1024 * 1024)) << "MiB";
		if (Global::terrain.size() < Global::Terrainsize) {
			Global::terrain.clear();
			Global::terrain.shrink_to_fit();
			Global::terrain.resize(Global::Terrainsize);
		}

		std::fill_n(Global::terrain.begin(), Global::Terrainsize, static_cast<StateID_t>(0U));// Preset: Air

		if (Global::settings.nightmode || Global::settings.underground || Global::settings.blendUnderground || Global::settings.skylight) {
			const size_t lightsize = Global::MapsizeZ * Global::MapsizeX * ((Global::MapsizeY + (Global::MapminY % 2 == 0 ? 1 : 2)) / 2);
			std::cout << ", lightmap " << std::setprecision(5) << float(lightsize / float(1024 * 1024)) << "MiB";
			if (Global::light.size() < lightsize || Global::light.size() * 0.9f > lightsize) {
				Global::light.clear();
				Global::light.shrink_to_fit();
				Global::light.resize(lightsize);
			}

			// Preset: all bright / dark depending on night or day
			if (Global::settings.nightmode) {
				std::fill_n(Global::light.begin(), lightsize, static_cast<uint8_t>(0x11));
			} else if (Global::settings.underground) {
				std::fill_n(Global::light.begin(), lightsize, static_cast<uint8_t>(0x00));
			} else {
				std::fill_n(Global::light.begin(), lightsize, static_cast<uint8_t>(0xFF));
			}
		}
		std::cout << '\n';
	}

	void deallocateTerrain()
	{
		Global::heightMap.clear();
		Global::heightMap.shrink_to_fit();
		Global::terrain.clear();
		Global::terrain.shrink_to_fit();
		Global::light.clear();
		Global::light.shrink_to_fit();
	}

	void clearLightmap()
	{
		std::fill(Global::light.begin(), Global::light.end(), static_cast<uint8_t>(0x00));
	}

	/**
	 * Round down to the nearest multiple of 8, e.g. floor8(-5) == 8
	 */
	inline int floorBiome(const int val)
	{
		if (val < 0) {
			return ((val - (CHUNKS_PER_BIOME_FILE - 1)) / CHUNKS_PER_BIOME_FILE) * CHUNKS_PER_BIOME_FILE;
		}
		return (val / CHUNKS_PER_BIOME_FILE) * CHUNKS_PER_BIOME_FILE;
	}

	/**
	 * Round down to the nearest multiple of 32, e.g. floor32(-5) == 32
	 */
	inline int floorRegion(const int val)
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
		if (world.regions.empty()) {
			//no regions loaded
			return false;
		}
		allocateTerrain();
		const size_t max = world.regions.size();
		std::cout << "Loading all chunks..\n";

		if (Global::threadPool) {
			std::vector<std::future<bool>> results;
			//multi-thread
			for (regionList::iterator it = world.regions.begin(); it != world.regions.end(); ++it) {
				Region& region = (*it);
				results.emplace_back(Global::threadPool->enqueue([](Region reg) {
					int i = 0;
					return loadRegion(reg.filename, true, i);
				}, region));

			}
			bool result = false;
			size_t count = 0;

			for (auto& r : results) {
				result |= r.get();
				helper::printProgress(count++, max);
			}

			helper::printProgress(10, 10);
			return result;
		} else {
			size_t count = 0;
			bool result = false;

			for (regionList::iterator it = world.regions.begin(); it != world.regions.end(); ++it) {
				Region& region = (*it);
				helper::printProgress(count++, max);
				int i;
				result |= loadRegion(region.filename, true, i);
			}
			helper::printProgress(10, 10);
			return result;
		}

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

		std::cout << "Loading all chunks..\n";
		bool result = false;
		const int tmpMin = -floorRegion(Global::FromChunkX);

		if (Global::threadPool) {
			std::vector<std::future<bool>> results;
			std::atomic_int atomicLoadedChunks{ 0 };

			for (int x = floorRegion(Global::FromChunkX); x <= floorRegion(Global::ToChunkX); x += REGIONSIZE) {
				for (int z = floorRegion(Global::FromChunkZ); z <= floorRegion(Global::ToChunkZ); z += REGIONSIZE) {
					const std::string path = fromPath + "/region/r." + std::to_string(int(x / REGIONSIZE)) + '.' + std::to_string(int(z / REGIONSIZE)) + ".mca";

					results.emplace_back(Global::threadPool->enqueue([&atomicLoadedChunks](const std::string _path) {
						int load = 0;
						const bool r = loadRegion(_path, false, load);
						atomicLoadedChunks += load;
						return r;
					}, path));
				}
			}

			helper::printProgress(0, results.size());
			for (size_t i = 0; i < results.size(); ++i) {
				const bool b = results[i].get();
				result |= b;
				helper::printProgress(i, results.size());
			}

			loadedChunks = atomicLoadedChunks;
			helper::printProgress(10, 10);

		} else {
			helper::printProgress(0, size_t(floorRegion(Global::ToChunkX) + tmpMin));
			const int maxX = floorRegion(Global::ToChunkX);
			for (int x = floorRegion(Global::FromChunkX); x <= maxX; x += REGIONSIZE) {
				const int maxZ = floorRegion(Global::ToChunkZ);
				for (int z = floorRegion(Global::FromChunkZ); z <= maxZ; z += REGIONSIZE) {
					const std::string path = fromPath + "/region/r." + std::to_string(x / REGIONSIZE) + '.' + std::to_string(z / REGIONSIZE) + ".mca";
					const bool b = loadRegion(path, false, loadedChunks);
					result |= b;
				}
				helper::printProgress(size_t(x + tmpMin), size_t(floorRegion(Global::ToChunkX) + tmpMin));
			}
		}

		return result;
	}

	bool loadRegion(const std::string& file, const bool mustExist, int &loadedChunks)
	{
		using chunkMap = std::map<uint32_t, uint32_t>;
		std::vector<uint8_t> buffer(COMPRESSED_BUFFER);
		std::vector<uint8_t> decompressedBuffer(DECOMPRESSED_BUFFER);
		std::ifstream rp(file, std::ios::binary);
		if (rp.fail()) {
			if (mustExist) std::cerr << "Error opening region file " << file << '\n';
			return false;
		}
		rp.read(reinterpret_cast<char*>(buffer.data()), REGIONSIZE * REGIONSIZE * 4);
		if (rp.fail()) {
			std::cerr << "Header too short in " << file << '\n';
			return false;
		}
		// Sort chunks using a map, so we access the file as sequential as possible
		chunkMap localChunks;
		for (uint32_t i = 0; i < REGION_HEADER_SIZE; i += 4) {
			const uint32_t offset = (helper::swap_endian<uint32_t>(buffer[i] + (buffer[i + 1] << 8) + (buffer[i + 2] << 16)) >> 8) * 4096;
			if (offset == 0) continue;
			localChunks[offset] = i;
		}
		if (localChunks.size() == 0) {
			return false;
		}
		z_stream zlibStream;
		for (chunkMap::iterator ci = localChunks.begin(); ci != localChunks.end(); ++ci) {
			uint32_t offset = ci->first;

			rp.seekg(offset);
			if (rp.fail()) {
				std::cerr << "Error seeking to chunk in region file " << file << '\n';
				continue;
			}
			rp.read(reinterpret_cast<char*>(buffer.data()), 5);
			if (rp.fail()) {
				std::cerr << "Error reading chunk size from region file " << file << '\n';
				continue;
			}
			size_t len = helper::swap_endian<uint32_t>(*reinterpret_cast<uint32_t*>(buffer.data()));
			uint8_t version = buffer[4];
			if (len == 0) continue;
			len--;
			if (len > COMPRESSED_BUFFER) {
				std::cerr << "Chunk too big in " << file << '\n';
				continue;
			}
			rp.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(len));
			if (rp.fail()) {
				std::cerr << "Not enough input for chunk in " << file << '\n';
				continue;
			}
			if (version == 1 || version == 2) { // zlib/gzip deflate
				std::memset(&zlibStream, 0, sizeof(z_stream));
				zlibStream.next_out = decompressedBuffer.data();
				zlibStream.avail_out = DECOMPRESSED_BUFFER;
				zlibStream.avail_in = static_cast<uInt>(len);
				zlibStream.next_in = buffer.data();

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
				}
			} else {
				std::cerr << "Unsupported Region version: " << (int) version << '\n';
				continue;
			}
			std::vector<uint8_t> buf(decompressedBuffer.begin(), decompressedBuffer.begin() + len);
			if (loadChunk(buf)) {
				loadedChunks++;
			}
		}
		return true;
	}

	//denselyPacked: if set to false uses the new block storage format added in 20w17a
	//if set to true it uses the old format
	size_t getPalletIndex(const std::vector<uint64_t>& arr, const size_t index, const bool denselyPacked)
	{
		const size_t lengthOfOne = std::max<size_t>((arr.size() * 64) / 4096, 4);
#ifdef _DEBUG
		const size_t maxObj = (arr.size() * helper::numBits<uint64_t>()) / lengthOfOne;
		if (maxObj <= index)
			throw std::out_of_range("out of range");
#endif

		if (!denselyPacked) {
			//use the new block storage format
			const size_t entriesPerWord = 64 / lengthOfOne;
			const size_t arrIdx = index / entriesPerWord;

			uint64_t val = helper::swap_endian(arr[arrIdx]);
			const size_t startBit = (index % entriesPerWord) * lengthOfOne;

			const auto m = startBit & (~0x3F);

			val >>= (startBit - m);
			const uint64_t mask = ~((~static_cast<uint64_t>(0)) << lengthOfOne);
			val &= mask;
			return static_cast<size_t>(val);
		}

		size_t startBit = index * lengthOfOne;
		size_t endBit = (startBit + lengthOfOne) - 1;
		if ((startBit / helper::numBits<uint64_t>()) != (endBit / helper::numBits<uint64_t>())) {
			uint64_t lowByte = helper::swap_endian(arr[startBit / helper::numBits<uint64_t>()]);
			uint64_t upByte = helper::swap_endian(arr[endBit / helper::numBits<uint64_t>()]);

			size_t bitsLow = helper::numBits<uint64_t>() - (startBit % helper::numBits<uint64_t>()); //((index + 1) * lengthOfOne) - startBit - 1;
			size_t bitsUp = lengthOfOne - bitsLow;

			const uint64_t mask = ~(~static_cast<uint64_t>(0) << bitsUp);

			upByte &= mask;
			upByte = upByte << bitsLow;

			lowByte = lowByte >> (helper::numBits<uint64_t>() - bitsLow);

			return static_cast<size_t>(upByte | lowByte);
		} else {
			//on same index in arr
			const uint64_t norm = arr[startBit / helper::numBits<uint64_t>()];
			uint64_t val = helper::swap_endian(norm);
			const auto m = startBit & (~0x3F); //((startBit / numBits<uint64_t>()) * numBits<uint64_t>());

			val >>= (startBit - m);
			const uint64_t mask = ~(~static_cast<uint64_t>(0) << lengthOfOne);
			val &= mask;
			return static_cast<size_t>(val);
		}

	}

	inline void lightCave(const int x, const int y, const int z)
	{
		for (int ty = y - 9; ty < y + 9; ty += 2) { // The trick here is to only take into account
			const int oty = ty - Global::MapminY;
			if (oty < 0) {
				continue;   // areas around torches.
			}
			if (oty >= static_cast<int>(Global::MapsizeY)) {
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
						if (tx >= int(Global::MapsizeZ) - CHUNKSIZE_Z) {
							break;
						}
						if (tz >= int(Global::MapsizeX) - CHUNKSIZE_X) {
							break;
						}
						SETLIGHTEAST(tx, oty, tz) = 0xFF;
					} else if (Global::settings.orientation == North) {
						if (tx >= int(Global::MapsizeX) - CHUNKSIZE_X) {
							break;
						}
						if (tz >= int(Global::MapsizeZ) - CHUNKSIZE_Z) {
							break;
						}
						SETLIGHTNORTH(tx, oty, tz) = 0xFF;
					} else if (Global::settings.orientation == South) {
						if (tx >= int(Global::MapsizeX) - CHUNKSIZE_X) {
							break;
						}
						if (tz >= int(Global::MapsizeZ) - CHUNKSIZE_Z) {
							break;
						}
						SETLIGHTSOUTH(tx, oty, tz) = 0xFF;
					} else {
						if (tx >= int(Global::MapsizeZ) - CHUNKSIZE_Z) {
							break;
						}
						if (tz >= int(Global::MapsizeX) - CHUNKSIZE_X) {
							break;
						}
						SETLIGHTWEST(tx, oty, tz) = 0xFF;
					}
				}
			}
		}
	}

	void uncoverNether()
	{
		const int cap = (static_cast<int>(Global::MapsizeY) - Global::MapminY) - 57;
		const int to = (static_cast<int>(Global::MapsizeY) - Global::MapminY) - 52;
		std::cout << "Uncovering Nether...\n";
		for (size_t x = CHUNKSIZE_X; x < Global::MapsizeX - CHUNKSIZE_X; ++x) {
			helper::printProgress(x - CHUNKSIZE_X, Global::MapsizeX);
			for (size_t z = CHUNKSIZE_Z; z < Global::MapsizeZ - CHUNKSIZE_Z; ++z) {
				// Remove blocks on top, otherwise there is not much to see here
				int massive = 0;
				StateID_t* bp = &Global::terrain[((z + (x * Global::MapsizeZ) + 1) * Global::MapsizeY) - 1];
				int i;
				for (i = 0; i < to; ++i) { // Go down 74 blocks from the ceiling to see if there is anything except solid
					if (massive && (*bp == AIR || helper::isLava(*bp))) {
						if (--massive == 0) {
							break;   // Ignore caves that are only 2 blocks high
						}
					}
					if (*bp != AIR && !helper::isLava(*bp)) {
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
		helper::printProgress(10, 10);
	}
}