#ifndef DRAW_PNG_H_
#define DRAW_PNG_H_

#include <fstream>
#include "defines.h"
#include "PNGWriter.h"

namespace draw {
	void setPixel(const int x, const int y, const StateID_t stateID, const float fsub, image::PNGWriter* pngWriter);
	void blendPixel(const size_t x, const size_t y, const StateID_t stateID, const float fsub, image::PNGWriter* pngWriter);
	uint64_t calcImageSize(const int mapChunksX, const int mapChunksZ, const size_t mapHeight, int &pixelsX, int &pixelsY, const bool tight = false);
	void blend(Channel* const destination, const Channel* const source);
}
#endif
