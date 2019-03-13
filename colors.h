#ifndef _COLORS_
#define _COLORS_

#include <string>

#include "RangeMap.h"
#include "Tree.h"
#include "defines.h"

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
