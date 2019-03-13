#ifndef DRAW_PNG_H_
#define DRAW_PNG_H_

#include <fstream>
#include "defines.h"
#include "PNGWriter.h"

void setPixel(const int x, const int y, const uint16_t stateID, const float fsub, PNGWriter* pngWriter);
void blendPixel(const size_t x, const size_t y, const uint16_t stateID, const float fsub, PNGWriter* pngWriter);
uint64_t calcImageSize(const int mapChunksX, const int mapChunksZ, const size_t mapHeight, int &pixelsX, int &pixelsY, const bool tight = false);
void blend(Channel* const destination, const Channel* const source);

#endif
