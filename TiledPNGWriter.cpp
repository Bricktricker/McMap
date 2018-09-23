//C++ Header
#include <iostream>
#include <algorithm>
#include <ctime>
#include <fstream>
//My-Header
#include <png.h>
#include "TiledPNGWriter.h"
#define NOMINMAX
#include "filesystem.h"

#ifndef Z_BEST_SPEED
#	define Z_BEST_SPEED 6
#endif

//Function to write png data to disc
void userWriteData(png_structp pngPtr, png_bytep data, png_size_t length)
{
	//Our std::ostream pointer.
	png_voidp a = png_get_io_ptr(pngPtr);
	//Cast the pointer to std::ifstream* and read 'length' bytes into 'data'
	((std::fstream*)a)->write((char*)data, length);
}

TiledPNGWriter::TiledPNGWriter(const size_t origW, const size_t origH)
	: m_origW(origW), m_origH(origH)
{
	if (!Dir::createDir("cache")) {
		std::cerr << "Could not create cache directory\n";
	}
}

TiledPNGWriter::~TiledPNGWriter()
{
}

bool TiledPNGWriter::open(const size_t width, const size_t height)
{
	const size_t pixSize = width * height * CHANSPERPIXEL;
	std::cout << "Creating temporary image: " << width << 'x' << height << ", 32bpp, " << float(pixSize / float(1024 * 1024)) << "MiB\n";
	m_buffer.resize(pixSize, 0U);

	m_currWidth = width;
	m_currHeight = height;
	return true;
}

bool TiledPNGWriter::addPart(const int startx, const int starty, const size_t width, const size_t height)
{
	offsetX = std::min(startx, 0);
	offsetY = std::min(starty, 0);
	int localX = startx;
	int localY = starty;
	int localWidth = width;
	int localHeight = height;

	if (localX < 0) {
		localWidth += localX;
		localX = 0;
	}
	if (localY < 0) {
		localHeight += localY;
		localY = 0;
	}

	if (localX + localWidth > width) {
		localWidth = width - localX;
	}
	if (localY + localHeight > height) {
		localHeight = height - localY;
	}
	if (localWidth < 1 || localHeight < 1) return false;

	const std::string name = "cache/" + std::to_string(localX) + '.' + std::to_string(localY) + '.' + std::to_string(localWidth) + '.' + std::to_string(localHeight) + '.' + std::to_string((int)time(NULL)) + ".png";
	ImagePart img{ name, localX, localY, localWidth, localHeight };
	m_partList.push_back(img);

	if (!open(localWidth, localHeight))
		return false;

	return true;
}

bool TiledPNGWriter::write(const std::string& path)
{
	const auto& part = m_partList.back();
	std::fstream fileHandle(part.filename, std::ios::out | std::ios::binary);
	if (fileHandle.fail()) {
		std::cerr << "Could not create temporary image at " << part.filename << "; check permissions in current dir.\n";
		return false;
	}

	png_structp pngStruct = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (pngStruct == nullptr) {
		return false;
	}

	png_infop pngInfo = png_create_info_struct(pngStruct);

	if (pngInfo == NULL) {
		png_destroy_write_struct(&pngStruct, NULL);
		return false;
	}

	if (setjmp(png_jmpbuf(pngStruct))) { // libpng will issue a longjmp on error, so code flow will end up
		png_destroy_write_struct(&pngStruct, &pngInfo); // here if something goes wrong in the code below
		return false;
	}

	png_set_write_fn(pngStruct, (png_voidp)&fileHandle, userWriteData, NULL);
	png_set_compression_level(pngStruct, Z_BEST_SPEED);
	png_set_IHDR(pngStruct, pngInfo, (uint32_t)part.width, (uint32_t)part.height,
		8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(pngStruct, pngInfo);

	size_t line = 0;
	for (int y = 0; y < part.height; ++y) {
		png_write_row(pngStruct, (png_bytep)&m_buffer.at(line)); //out of bounds??
		line += part.width * CHANSPERPIXEL * part.height;
	}

	png_write_end(pngStruct, NULL);
	png_destroy_write_struct(&pngStruct, &pngInfo);

	return true;
}

uint8_t* TiledPNGWriter::getPixel(const size_t x, const size_t y)
{
	if (x >= m_currWidth || y >= m_currHeight || x < 0 || y < 0)
		throw std::out_of_range("getPixel out of range\n");

	return &m_buffer.at((x+offsetX) * CHANSPERPIXEL + (y + offsetY) * (m_origW * CHANSPERPIXEL)); //check index calculation
}

bool TiledPNGWriter::compose(const std::string & path)
{
	std::fstream outHandle(path, std::ios::out | std::ios::binary);
	if (outHandle.fail()) {
		std::cerr << "Error opening '" << path << "' for writing.\n";
		return false;
	}

	return false;
}
