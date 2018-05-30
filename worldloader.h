#ifndef _WORLDLOADER_H_
#define _WORLDLOADER_H_

#include <cstdlib>
#include <stdint.h>
#include <string>

WorldFormat getWorldFormat(const std::string& worldPath);
bool scanWorldDirectory(const std::string& fromPath);
bool loadTerrain(const std::string& fromPath, int &loadedChunks);
bool loadEntireTerrain();
uint64_t calcTerrainSize(const int chunksX, const int chunksZ);
void clearLightmap();
void calcBitmapOverdraw(int &left, int &right, int &top, int &bottom);
void loadBiomeMap(const std::string& path);
void uncoverNether();

#endif
