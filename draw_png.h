#ifndef DRAW_PNG_H_
#define DRAW_PNG_H_

#include <fstream>

// Separate them in case I ever implement 16bit rendering
#define CHANSPERPIXEL 4
#define BYTESPERCHAN 1
#define BYTESPERPIXEL 4

void createImageBuffer(const size_t width, const size_t height, const bool splitUp);
bool createImage(std::fstream& fh, const size_t width, const size_t height, const bool splitUp);
bool saveImage();
int loadImagePart(const int startx, const int starty, const int width, const int height);
void setPixel(const size_t x, const size_t y, const uint16_t stateID, const float fsub);
void blendPixel(const size_t x, const size_t y, const uint16_t stateID, const float fsub);
bool saveImagePart();
bool discardImagePart();
bool composeFinalImage();
uint64_t calcImageSize(const int mapChunksX, const int mapChunksZ, const size_t mapHeight, int &pixelsX, int &pixelsY, const bool tight = false);

void blend(uint8_t* const destination, const uint8_t* const source);
#endif
