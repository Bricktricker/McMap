#ifndef _WORLDLOADER_H_
#define _WORLDLOADER_H_

#include <cstdlib>
#include <stdint.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "globals.h"

WorldFormat getWorldFormat(const std::string& worldPath);
bool scanWorldDirectory(const std::string& fromPath);
bool loadTerrain(const std::string& fromPath, int &loadedChunks);
bool loadEntireTerrain();
uint64_t calcTerrainSize(const int chunksX, const int chunksZ);
void clearLightmap();
void calcBitmapOverdraw(int &left, int &right, int &top, int &bottom); //Berechnet ueberschnitt auf allen 4 Seiten
//void loadBiomeMap(const std::string& path); //no longer supported
void uncoverNether();

// This will hold all chunks (<1.3) or region files (>=1.3) discovered while scanning world dir
struct Region {
	int x; //x-val in filename
	int z; //z-val in filename
	std::string filename;
	Region(const std::string& source, const int sx, const int sz)
		: x(sx), z(sz), filename(source) {}
};
struct Point {
	int x;
	int z;
	Point(const int sx, const int sz) {
		x = sx;
		z = sz;
	}
};

typedef std::list<char *> charList; // List that holds C-Strings
//typedef std::list<Chunk *> chunkList; // List that holds Chunk structs
typedef std::list<Region> regionList; //List that hold all regions (see above)
typedef std::list<Point> pointList; // List that holds Point structs (see above)
typedef std::map<uint32_t, uint32_t> chunkMap;

struct World
{
	regionList regions; // list of all chunks/regions of a world
	pointList points; // all existing chunk X|Z found in region files
};


#endif
