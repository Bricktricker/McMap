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

	Color_t()
		: r(0), g(0), b(0), a(0), noise(0), brightness(0), blockType(0)
	{}

	Color_t(const uint8_t _r, const uint8_t _g, const uint8_t _b, const uint8_t _a, const uint8_t _n, const uint8_t _bright, const uint8_t _bType)
		: r(_r), g(_g), b(_b), a(_a), noise(_n), brightness(_bright), blockType(_bType)
	{}

};

extern std::map<std::string, Tree<std::string, uint16_t>> blockTree; //Maps blockState to id
extern RangeMap<uint16_t, Color_t> colorMap; //maps id to color_t
extern const std::map<uint16_t, uint16_t> metaToState; //maps old blockid:meta to new states

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
