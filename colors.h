#ifndef _COLORS_
#define _COLORS_

#include <string>

#include "helper.h"
#include "RangeMap.h"
#include "Tree.h"
#include "PNGWriter.h"

//Data structures for color
struct Color_t {
	Channel r, g, b, a;
	uint8_t noise, brightness, blockType;

	Color_t()
		: r(0), g(0), b(0), a(0), noise(0), brightness(0), blockType(0)
	{}

	Color_t(const Channel _r, const Channel _g, const Channel _b, const Channel _a, const Channel _n, const Channel _bright, const Channel _bType)
		: r(_r), g(_g), b(_b), a(_a), noise(_n), brightness(_bright), blockType(_bType)
	{}

};

using StateID_t = uint16_t;

extern std::map<std::string, Tree<std::string, StateID_t>> blockTree; //Maps blockState to id
extern RangeMap<StateID_t, Color_t> colorMap; //maps id to color_t
extern const std::map<uint16_t, StateID_t> metaToState; //maps old blockid:meta to new states

bool loadBlockTree(const std::string& path);
bool loadColorMap(const std::string& path);

#define AIR 0

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
