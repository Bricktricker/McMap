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
#include "helper.h"
#include "draw_png.h"

#ifndef Z_BEST_SPEED
#	define Z_BEST_SPEED 6
#endif

namespace {
//Function to write png data to disc
void userWriteData(png_structp pngPtr, png_bytep data, png_size_t length)
{
	//Our std::ostream pointer.
	png_voidp a = png_get_io_ptr(pngPtr);
	//Cast the pointer to std::ifstream* and read 'length' bytes into 'data'
	((std::fstream*)a)->write((char*)data, length);
}

//Function to read pngt data from disc
void userReadData(png_structp pngPtr, png_bytep data, png_size_t length)
{
	png_voidp a = png_get_io_ptr(pngPtr);
	//Cast the pointer to std::ifstream* and read 'length' bytes into 'data'
	((std::fstream*)a)->read((char*)data, length);
}
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

bool TiledPNGWriter::addPart(const int startx, const int starty, const int width, const int height)
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

bool TiledPNGWriter::compose(const std::string& path)
{
	std::fstream outHandle(path, std::ios::out | std::ios::binary);
	if (outHandle.fail()) {
		std::cerr << "Error opening '" << path << "' for writing.\n";
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

	const size_t tempWidth = m_origW * CHANSPERPIXEL;
	const size_t tempWidthChans = tempWidth * CHANSPERPIXEL;

	std::vector<uint8_t> lineWrite(tempWidthChans, 0);
	std::vector<uint8_t> lineRead(tempWidth, 0);

	// Prepare an array of png structs that will output simultaneously to the various tiles
	//size_t sizeOffset[7]; //TODO: change to std::array
	//ImageTile *tile = NULL;

	for (int y = 0; y < m_origH; ++y) {
		if (y % 100 == 0) {
			printProgress(size_t(y), size_t(m_origH));
		}
		// paint each image on this one
		std::fill(lineWrite.begin(), lineWrite.end(), 0);
		// the partial images are kept in this list. they're already in the correct order in which they have to me merged and blended
		for (auto it = m_partList.begin(); it != m_partList.end(); ++it) {
			ImagePart& img = *it;
			// do we have to open this image?
			if (img.y != y) {
				continue;   // Not your turn, image!
			}

			std::fstream pngFileHandle(img.filename, std::ios::in | std::ios::binary);
			if (pngFileHandle.fail()) {
				std::cerr << "Error opening temporary image " << img.filename << '\n';
				return false;
			}

			png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
			if (pngPtr == NULL) {
				std::cerr << "Error creating read struct for temporary image " << img.filename << '\n';
				return false; // Not really cleaning up here, but program will terminate anyways, so why bother
			}
			png_infop pngInfo = png_create_info_struct(pngPtr);
			if (pngInfo == NULL || setjmp(png_jmpbuf(pngPtr))) {
				std::cerr << "Error reading data from temporary image " << img.filename << '\n';
				return false; // Same here
			}

			png_set_read_fn(pngPtr, (png_voidp)&pngFileHandle, userReadData);
			png_read_info(pngPtr, pngInfo);
			// Check if image dimensions match what is expected
			int type, interlace, comp, filter, bitDepth;
			png_uint_32 width, height;
			png_uint_32 ret = png_get_IHDR(pngPtr, pngInfo, &width, &height, &bitDepth, &type, &interlace, &comp, &filter);
			if (ret == 0 || width != (png_uint_32)img.width || height != (png_uint_32)img.height) {
				std::cerr << "Temp image " << img.filename << " has wrong dimensions; expected " << img.width << 'x' << img.height << ", got " << width << 'x' << height << '\n';
				return false;
			}
			// Here, the image is either open and ready for reading another line, or its not open when it doesn't have to be copied/blended here, or is already finished

			// Read next line from current image chunk
			png_read_row(pngPtr, (png_bytep)lineRead.data(), NULL);
			// Now this puts all the pixels in the right spot of the current line of the final image
			const size_t end = (img.x + img.width) * CHANSPERPIXEL;
			size_t read = 0;
			for (size_t write = (img.x * CHANSPERPIXEL); write < end; write += CHANSPERPIXEL) {
				blend(&lineWrite[write], &lineRead[read]);
				read += CHANSPERPIXEL;
			}
			// Now check if we're done with this image chunk
			if (--(img.height) == 0) { // if so, close and discard
				png_destroy_read_struct(&(pngPtr), &(pngInfo), NULL);
				pngFileHandle.close();
				pngPtr = NULL;
				remove(img.filename.c_str());
			}
		}

		// Done composing this line, write to final image
		png_write_row(pngStruct, (png_bytep)lineWrite.data());

	}// Y-Loop

	png_write_end(pngStruct, NULL);
	png_destroy_write_struct(&pngStruct, &pngInfo);
	printProgress(10, 10);
	return true;
}
