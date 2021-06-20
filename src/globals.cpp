#include "globals.h"

int Global::TotalFromChunkX, Global::TotalFromChunkZ, Global::TotalToChunkX, Global::TotalToChunkZ;
int Global::FromChunkX = UNDEFINED, Global::FromChunkZ = UNDEFINED, Global::ToChunkX = UNDEFINED, Global::ToChunkZ = UNDEFINED;
size_t Global::MapsizeZ = 0, Global::MapsizeX = 0, Global::Terrainsize = 0;
int Global::MapminY = 0;
size_t Global::MapsizeY = 256;
int Global::OffsetY = 2;
Settings Global::settings = { East, false, false, false, false, 0, false, false, false, false };

std::vector<Marker> Global::markers;
std::vector<StateID_t> Global::terrain;
std::vector<uint8_t> Global::light;
std::vector<uint16_t> Global::heightMap;

std::unordered_map<std::string, Tree<std::string, StateID_t>> Global::blockTree;
std::vector<Model_t> Global::colorMap;

std::array<std::vector<StateID_t>, SpecialBlocks::NUM_SPECIALBLOCKS> Global::specialBlockMap;

int8_t Global::sectionMin, Global::sectionMax;
uint8_t Global::mystCraftAge = 0U;

std::unique_ptr<ThreadPool> Global::threadPool;
