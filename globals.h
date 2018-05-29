#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#define VERSION "beta 3.0.3"

#include <stdint.h>
#include <cstdlib>
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
	FORMAT1,
	FORMAT2,
	FORMAT3,
	NUM_FORMATS
};

// Global area of world being rendered
extern int g_TotalFromChunkX, g_TotalFromChunkZ, g_TotalToChunkX, g_TotalToChunkZ;
// Current area of world being rendered
extern int g_FromChunkX, g_FromChunkZ, g_ToChunkX, g_ToChunkZ;
// size of that area in blocks (no offset)
extern size_t g_MapsizeZ, g_MapsizeX, g_Terrainsize;
extern int g_MapminY, g_MapsizeY;

extern int g_OffsetY; // y pixel offset in the final image for one y step in 3d array (2 or 3)

extern int g_WorldFormat;
extern Orientation g_Orientation; // North, West, South, East
extern bool g_Nightmode;
extern bool g_Underground;
extern bool g_BlendUnderground;
extern bool g_Skylight;
extern int g_Noise;
extern bool g_BlendAll; // If set, do not assume certain blocks (like grass) are always opaque
extern bool g_Hell, g_ServerHell; // rendering the nether
extern bool g_lowMemory;

// For rendering biome colors properly, external png files are used
extern bool g_UseBiomes;
extern uint64_t g_BiomeMapSize;
extern uint8_t *g_Grasscolor, *g_Leafcolor, *g_TallGrasscolor;
extern uint16_t *g_BiomeMap;
extern int g_GrasscolorDepth, g_FoliageDepth;

extern int g_MarkerCount;
extern Marker g_Markers[MAX_MARKERS];

// 3D arrays holding terrain/lightmap
extern uint8_t *g_Terrain, *g_Light;
// 2D array to store min and max block height per X/Z - it's 2 bytes per index, upper for highest, lower for lowest (don't ask!)
extern uint16_t *g_HeightMap;

// If output is to be split up (for google maps etc) this contains the path to output to, NULL otherwise
extern char *g_TilePath;

extern int8_t g_SectionMin, g_SectionMax;

extern uint8_t g_MystCraftAge;

struct Settings {
	Orientation orientation; // North, West, South, East
	bool nightmode;
	bool underground;
	bool blendUnderground;
	bool skylight;
	int noise;
	bool blendAll; // If set, do not assume certain blocks (like grass) are always opaque
	bool hell, serverHell; // rendering the nether
	bool lowMemory;
};

class Global
{
public:
	static int TotalFromChunkX, TotalFromChunkZ, TotalToChunkX, TotalToChunkZ; //global area from - to
	static int FromChunkX, FromChunkZ, ToChunkX, ToChunkZ; // Current area of world being rendered
	static size_t MapsizeZ, MapsizeX, Terrainsize; // size of that area in blocks (no offset)
	static int MapminY, MapsizeY; //no idea
	static int OffsetY; // y pixel offset in the final image for one y step in 3d array (2 or 3)
	static WorldFormat wolrdFormat;
	static Settings settings; //Used settings

	// For rendering biome colors properly, external png files are used
	static bool useBiomes;
	static uint64_t biomeMapSize;
	static std::vector<uint8_t> grasscolor, leafcolor, tallGrasscolor;
	static std::vector<uint16_t> biomeMap;
	static int grasscolorDepth, foliageDepth;

	static std::vector<Marker> markers;
	static std::vector<uint8_t> terrain, light; // 3D arrays holding terrain/lightmap
	static std::vector<uint16_t> heightMap; // 2D array to store min and max block height per X/Z - it's 2 bytes per index, upper for highest, lower for lowest (don't ask!)

	static std::string tilePath; // If output is to be split up (for google maps etc) this contains the path to output to, "" otherwise
	static int8_t g_SectionMin, g_SectionMax;
	uint8_t g_MystCraftAge;
};

#endif
