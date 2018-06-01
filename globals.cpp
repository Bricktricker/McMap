#include "globals.h"

#include "worldloader.h"
/*
int g_TotalFromChunkX, g_TotalFromChunkZ, g_TotalToChunkX, g_TotalToChunkZ;
int g_FromChunkX = UNDEFINED, g_FromChunkZ = UNDEFINED, g_ToChunkX = UNDEFINED, g_ToChunkZ = UNDEFINED;
size_t g_MapsizeZ = 0, g_MapsizeX = 0, g_Terrainsize = 0;
int g_MapminY = 0, g_MapsizeY = 256, g_OffsetY = 2;

int g_WorldFormat = -1;

Orientation g_Orientation = East;
bool g_Nightmode = false;
bool g_Underground = false;
bool g_BlendUnderground = false;
bool g_Skylight = false;
int g_Noise = 0;
bool g_BlendAll = false;
bool g_Hell = false, g_ServerHell = false;
bool g_lowMemory = false;

bool g_UseBiomes = false;
uint64_t g_BiomeMapSize = 0;
uint8_t *g_Grasscolor = NULL, *g_Leafcolor = NULL, *g_TallGrasscolor = NULL;
uint16_t *g_BiomeMap = NULL;
int g_GrasscolorDepth = 0, g_FoliageDepth = 0;

uint8_t *g_Terrain = NULL, *g_Light = NULL;
uint16_t *g_HeightMap = NULL;

int g_MarkerCount = 0;
Marker g_Markers[MAX_MARKERS];

char *g_TilePath = NULL;

int8_t g_SectionMin, g_SectionMax;

uint8_t g_MystCraftAge;
*/



int Global::TotalFromChunkX, Global::TotalFromChunkZ, Global::TotalToChunkX, Global::TotalToChunkZ;
int Global::FromChunkX = UNDEFINED, Global::FromChunkZ = UNDEFINED, Global::ToChunkX = UNDEFINED, Global::ToChunkZ = UNDEFINED;
size_t Global::MapsizeZ, Global::MapsizeX, Global::Terrainsize;
int Global::MapminY, Global::MapsizeY;
int Global::OffsetY;
WorldFormat Global::worldFormat;
Settings Global::settings = { East, false, false, false, false, 0, false, false, false, false, false };

//bool Global::useBiomes;
uint64_t Global::biomeMapSize;
std::vector<uint8_t> Global::grasscolor, Global::leafcolor, Global::tallGrasscolor;
std::vector<uint16_t> Global::biomeMap;
int Global::grasscolorDepth, Global::foliageDepth;

std::vector<Marker> Global::markers;
std::vector<uint8_t> Global::terrain, Global::light;
std::vector<uint16_t> Global::heightMap;
size_t Global::lightsize;

std::string Global::tilePath;
int8_t Global::sectionMin, Global::sectionMax;
uint8_t Global::mystCraftAge = 0U;