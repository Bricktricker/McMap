//C++ Header
#include <iostream>
#include <fstream>
#include <array>
#include <cmath> //pow (for g++)
#include <cstring> //memcpy (for g++)
//My-Header
#include "BasicTiledPNGWriter.h"
#include "helper.h"
#include "filesystem.h"

namespace {
	//Function to write png data to disc
	void userWriteData(png_structp pngPtr, png_bytep data, png_size_t length)
	{
		png_voidp a = png_get_io_ptr(pngPtr);
		//Cast the pointer to std::ifstream* and read 'length' bytes into 'data'
		((std::fstream*)a)->write((char*)data, length);
	}
}

namespace image {
	// Tiled output, suitable for google maps
	bool BasicTiledPNGWriter::write(const std::string& path){
		std::cout << "Writing to files...\n";

		if (!Dir::createDir(path)) {
			std::cerr << "Could not create " << path << " folder\n";
			return false;
		}

		// Prepare a temporary buffer to copy the current line to, since we need the width to be a multiple of 4096
		// and adjusting the whole image to that would be a waste of memory
		const size_t tempWidth = ((m_width - 5) / 4096 + 1) * 4096;
		const size_t tempWidthChans = tempWidth * CHANSPERPIXEL;
		const size_t LineWidthChans = m_width * CHANSPERPIXEL;

	#ifdef _DEBUG
		std::cout << "Temp width: " << tempWidthChans << ", original width: " << LineWidthChans << '\n';
	#endif
		std::vector<uint8_t> tempLine(tempWidthChans, 0);
		// Source pointer
		size_t srcLine = 0;
		// Prepare an array of png structs that will output simultaneously to the various tiles
		std::array<size_t, 7> sizeOffset;
		size_t last = 0;
		for (size_t i = 0; i < 7; ++i) {
			sizeOffset[i] = last;
			last += ((tempWidth - 1) / static_cast<size_t>(pow(2, 12 - i))) + 1;
		}
		std::vector<ImageTile> tile(sizeOffset[6]);
		for (size_t y = 0; y < m_height; ++y) {
			if (y % 25 == 0) {
				helper::printProgress(y, m_height);
			}
			std::memcpy(tempLine.data(), &m_buffer[srcLine], LineWidthChans);
			srcLine += LineWidthChans;
			// Handle all png files
			if (y % 128 == 0) {
				size_t start;
				if (y % 4096 == 0) start = 0;
				else if (y % 2048 == 0) start = 1;
				else if (y % 1024 == 0) start = 2;
				else if (y % 512 == 0) start = 3;
				else if (y % 256 == 0) start = 4;
				else start = 5;
				for (size_t tileSize = start; tileSize < 6; ++tileSize) {
					const size_t tileWidth = static_cast<size_t>(pow(2, 12 - tileSize));
					for (size_t tileIndex = sizeOffset[tileSize]; tileIndex < sizeOffset[tileSize + 1]; ++tileIndex) {
						ImageTile& t = tile[tileIndex];
						if (t.fileHandle.is_open()) { // Unload/close first
							png_write_end(t.pngPtr, NULL);
							png_destroy_write_struct(&(t.pngPtr), &(t.pngInfo));
							t.fileHandle.close();
						}
						if (tileWidth * (tileIndex - sizeOffset[tileSize]) < size_t(m_width)) {
							// Open new tile file for a while
							const std::string tmpString = path + "/x" + std::to_string(int(tileIndex - sizeOffset[tileSize])) + 'y' + std::to_string(int((y / pow(2, 12 - tileSize)))) + 'z' + std::to_string(int(tileSize)) + ".png";
	#ifdef _DEBUG
							std::cout << "Starting tile " << tmpString << " of size " << (int)pow(2, 12 - tileSize) << "...\n";
	#endif
							t.fileHandle.open(tmpString, std::ios::out | std::ios::binary);
							if (t.fileHandle.fail()) {
								std::cerr << "Error opening file!\n";
								return false;
							}
							t.pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
							if (t.pngPtr == nullptr) {
								std::cerr << "Error creating png write struct!\n";
								return false;
							}
							if (setjmp(png_jmpbuf(t.pngPtr))) {
								std::cerr << "Something went wrong with pngLib\n";
								return false;
							}
							t.pngInfo = png_create_info_struct(t.pngPtr);
							if (t.pngInfo == nullptr) {
								std::cerr << "Error creating png info struct!\n";
								png_destroy_write_struct(&(t.pngPtr), NULL);
								return false;
							}

							png_set_write_fn(t.pngPtr, (png_voidp)&t.fileHandle, userWriteData, NULL);
							//png_init_io(t.pngPtr, t.fileHandle);
							png_set_IHDR(t.pngPtr, t.pngInfo,
								uint32_t(pow(2, 12 - tileSize)), uint32_t(pow(2, 12 - tileSize)),
								8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
								PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
							png_write_info(t.pngPtr, t.pngInfo);
						}
					}
				}
			} // done preparing tiles
			  // Write data to all current tiles
			for (size_t tileSize = 0; tileSize < 6; ++tileSize) {
				const size_t tileWidth = static_cast<size_t>(pow(2, 12 - tileSize));
				for (size_t tileIndex = sizeOffset[tileSize]; tileIndex < sizeOffset[tileSize + 1]; ++tileIndex) {
					if (tile[tileIndex].fileHandle.fail() || !tile[tileIndex].fileHandle.is_open()) continue;
					png_write_row(tile[tileIndex].pngPtr, png_bytep(&tempLine[tileWidth * (tileIndex - sizeOffset[tileSize]) * CHANSPERPIXEL]));
				}
			} // done writing line
		} // done with whole image

		  // Now the last set of tiles is not finished, so do that manually
		std::fill(tempLine.begin(), tempLine.end(), 0);
		for (size_t tileSize = 0; tileSize < 6; ++tileSize) {
			const size_t tileWidth = static_cast<size_t>(pow(2, 12 - tileSize));
			for (size_t tileIndex = sizeOffset[tileSize]; tileIndex < sizeOffset[tileSize + 1]; ++tileIndex) {
				if (tile[tileIndex].fileHandle.fail() || !tile[tileIndex].fileHandle.is_open()) continue;
				const size_t imgEnd = (((m_height - 1) / tileWidth) + 1) * tileWidth;
				for (size_t i = m_height; i < imgEnd; ++i) {
					png_write_row(tile[tileIndex].pngPtr, png_bytep(tempLine.data())); //writes just 0's
				}
				png_write_end(tile[tileIndex].pngPtr, NULL);
				png_destroy_write_struct(&(tile[tileIndex].pngPtr), &(tile[tileIndex].pngInfo));
			}
		}
		helper::printProgress(10, 10);

		return true;
	}
}