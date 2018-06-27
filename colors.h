#ifndef _COLORS_
#define _COLORS_

#include "helper.h"
#include "RangeMap.h"
#include "Tree.h"
#include <string>

//Data structures for color
struct color_t {
	uint8_t r, g, b, a, noise, brightness;
	uint8_t blockType;
};

extern std::map<std::string, Tree<std::string, uint16_t>> blockTree; //Maps blockState to id
extern RangeMap<uint16_t, color_t> colorMap; //maps id to color_t

void loadBlockTree(const std::string& path);
void loadColorMap(const std::string& path);

#define AIR 0

#endif
