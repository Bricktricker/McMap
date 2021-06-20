#pragma once

#include <vector>
#include <string>
#include <memory>
#include <array>
#include <utility>
#include <unordered_map>
#include "defines.h"
#include "ThreadPool.h"
#include "Tree.h"

enum Orientation {
	North,
	East,
	South,
	West
};

struct Marker {
	int chunkX, chunkZ, offsetX, offsetZ;
	uint8_t color;
};

enum WorldFormat {
	ALPHA = 0, //old 0
	REGION = 1, //old 1
	ANVIL = 2, //old 2
	ANVIL13 = 3 //New Anvil format in Minecraft 1.13
};

enum SpecialBlocks {
	LEAVES,
	WATER,
	LAVA,
	TORCH,
	SNOW,
	GRASS_BLOCK,
	NUM_SPECIALBLOCKS
};

struct Settings {
	Orientation orientation; // North, West, South, East
	bool nightmode;
	bool underground;
	bool blendUnderground;
	bool skylight;
	int noise;
	bool blendAll; // If set, do not assume certain blocks (like grass) are always opaque
	bool hell, serverHell; // rendering the nether
	bool end; //rendering the End
};

class Global
{
public:
	static int TotalFromChunkX, TotalFromChunkZ, TotalToChunkX, TotalToChunkZ; //global area from - to
	static int FromChunkX, FromChunkZ, ToChunkX, ToChunkZ; // Current area of world being rendered
	static size_t MapsizeZ, MapsizeX, Terrainsize; // size of that area in blocks (no offset)
	static int MapminY; //minimum height to render from
	static size_t MapsizeY; //maximum height to render (max CHUNKSIZE_Y (256))
	static int OffsetY; // y pixel offset in the final image for one y step in 3d array (2)
	static Settings settings; //Used settings

	static std::vector<Marker> markers;
	static std::vector<StateID_t> terrain;
	static std::vector<uint8_t>	light; // 3D arrays holding terrain/lightmap
	static std::vector<uint16_t> heightMap; // 2D array to store min and max block height per X/Z - it's 2 bytes per index, upper for highest, lower for lowest (don't ask!)

	static std::unordered_map<std::string, Tree<std::string, StateID_t>> blockTree; //Maps blockState to id
	static std::vector<Model_t> colorMap; //maps id to color_t

	static std::array<std::vector<StateID_t>, SpecialBlocks::NUM_SPECIALBLOCKS> specialBlockMap; //BlockType -> list of stateIds of that BlockType

	static int8_t sectionMin, sectionMax; //Section borders, 1 Section are 16x16x16 Blocks, values for block height
	static uint8_t mystCraftAge;

	static std::unique_ptr<ThreadPool> threadPool;
};
