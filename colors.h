#ifndef _COLORS_
#define _COLORS_

#include "helper.h"
#include "RangeMap.h"
#include "Tree.h"
#include <string>

//Data structures for color
struct Color_t {
	uint8_t r, g, b, a, noise, brightness;
	uint8_t blockType;
};

extern std::map<std::string, Tree<std::string, uint16_t>> blockTree; //Maps blockState to id
extern RangeMap<uint16_t, Color_t> colorMap; //maps id to color_t

void loadBlockTree(const std::string& path);
void loadColorMap(const std::string& path);

#define AIR 0

//TODO: check IDs
#define BLOCKSOLID 0
#define BLOCKFLAT 1
#define BLOCKTORCH 2
#define BLOCKFLOWER 3
#define BLOCKFENCE 4
#define BLOCKWIRE 5
#define BLOCKRAILROAD 6
#define BLOCKGRASS 7
#define BLOCKFIRE 8
#define BLOCKSTEP 9
#define BLOCKUPSTEP 10
#define BLOCKBIOME 128

#endif
