#pragma once
#include <fstream>
#include "defines.h"
#include "PNGWriter.h"

namespace draw {
	void setPixel(const int x, const int y, const StateID_t stateID, const float fsub, image::PNGWriter* pngWriter);
	void blendPixel(const int x, const int y, const StateID_t stateID, const float fsub, image::PNGWriter* pngWriter);
	uint64_t calcImageSize(const size_t mapChunksX, const size_t mapChunksZ, const size_t mapHeight, size_t &pixelsX, size_t &pixelsY, const bool tight = false);
	void blend(Channel* const destination, const Channel* const source);
}

