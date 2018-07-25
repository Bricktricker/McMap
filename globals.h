#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#define VERSION "beta 3.0.4"

#include <vector>
#include <string>

#define UNDEFINED 0x7FFFFFFF
#define MAX_MARKERS 200

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
	//bool lowMemory;
};

class Global
{
public:
	static int TotalFromChunkX, TotalFromChunkZ, TotalToChunkX, TotalToChunkZ; //global area from - to
	static int FromChunkX, FromChunkZ, ToChunkX, ToChunkZ; // Current area of world being rendered
	static size_t MapsizeZ, MapsizeX, Terrainsize; // size of that area in blocks (no offset)
	static int MapminY; //minimum height to render from
	static unsigned int MapsizeY; //maximum height to rendrer (max CHUNKSIZE_Y (256))
	static int OffsetY; // y pixel offset in the final image for one y step in 3d array (2 or 3)
	static WorldFormat worldFormat; //Format of the world
	static Settings settings; //Used settings

	// For rendering biome colors properly, external png files are used
	//static bool useBiomes;
	static uint64_t biomeMapSize;
	static std::vector<uint8_t> grasscolor, leafcolor, tallGrasscolor;
	static std::vector<uint16_t> biomeMap;
	static int grasscolorDepth, foliageDepth;

	static std::vector<Marker> markers;
	static std::vector<uint16_t> terrain;
	static std::vector<uint8_t>	light; // 3D arrays holding terrain/lightmap
	static std::vector<uint16_t> heightMap; // 2D array to store min and max block height per X/Z - it's 2 bytes per index, upper for highest, lower for lowest (don't ask!)
	static size_t lightsize; // Size of lightmap

	static std::string tilePath; // If output is to be split up (for google maps etc) this contains the path to output to, "" otherwise
	static int8_t sectionMin, sectionMax; //No idea
	static uint8_t mystCraftAge;

};

#endif
