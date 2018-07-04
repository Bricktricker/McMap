#include "globals.h"
#include "worldloader.h"

int Global::TotalFromChunkX, Global::TotalFromChunkZ, Global::TotalToChunkX, Global::TotalToChunkZ;
int Global::FromChunkX = UNDEFINED, Global::FromChunkZ = UNDEFINED, Global::ToChunkX = UNDEFINED, Global::ToChunkZ = UNDEFINED;
size_t Global::MapsizeZ = 0, Global::MapsizeX = 0, Global::Terrainsize = 0;
int Global::MapminY = 0;
unsigned int Global::MapsizeY = 256;
int Global::OffsetY = 2;
WorldFormat Global::worldFormat;
Settings Global::settings = { East, false, false, false, false, 0, false, false, false, false};

//bool Global::useBiomes = false;
uint64_t Global::biomeMapSize = 0;
std::vector<uint8_t> Global::grasscolor, Global::leafcolor, Global::tallGrasscolor;
std::vector<uint16_t> Global::biomeMap;
int Global::grasscolorDepth = 0, Global::foliageDepth = 0;

std::vector<Marker> Global::markers;
std::vector<uint16_t> Global::terrain;
/*
addData: "add" => for blocks with blockIDs > 255
Double size of number of blocks
1. Half: Block ID (0,...,255)
2: Half: addData (without shiftet by 8) + (MetaData << 4) so: First 4LSBits: addData, Last 4MSBits: MetaData
*/
std::vector<uint8_t> Global::light;
std::vector<uint16_t> Global::heightMap;
size_t Global::lightsize;

std::string Global::tilePath;
int8_t Global::sectionMin, Global::sectionMax;
uint8_t Global::mystCraftAge = 0U;