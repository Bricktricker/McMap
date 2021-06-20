//C++ Header
#include <iostream>
#include <fstream>
#include <cassert>
#include <array>
#include <cmath> //pow (for g++)
//My-Header
#include "CachedTiledPNGWriter.h"
#include "helper.h"
#include "draw_png.h"
#include "filesystem.h"

namespace {
	//Function to write png data to disc
	void userWriteData(png_structp pngPtr, png_bytep data, png_size_t length)
	{
		png_voidp a = png_get_io_ptr(pngPtr);
		//Cast the pointer to std::ifstream* and write 'length' bytes from 'data'
		static_cast<std::fstream*>(a)->write(reinterpret_cast<char*>(data), static_cast<std::streamsize>(length));
	}

	//Function to read pngt data from disc
	void userReadData(png_structp pngPtr, png_bytep data, png_size_t length)
	{
		png_voidp a = png_get_io_ptr(pngPtr);
		//Cast the pointer to std::ifstream* and read 'length' bytes into 'data'
		static_cast<std::fstream*>(a)->read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(length));
	}
}

namespace image {
	CachedTiledPNGWriter::CachedTiledPNGWriter(const size_t origW, const size_t origH)
		: CachedPNGWriter(origW, origH)
	{
	}

	bool CachedTiledPNGWriter::compose(const std::string & path, [[maybe_unused]] const double scale) {
		// Tiled output, suitable for google maps
		std::cout << "Composing final png files...\n";

		if (!Dir::createDir(path)) {
			std::cerr << "Could not create " << path << " folder\n";
			return false;
		}

		const size_t tempWidth = m_origW * CHANSPERPIXEL;
		const size_t tempWidthChans = tempWidth * CHANSPERPIXEL;

		std::vector<uint8_t> lineWrite(tempWidthChans, 0);
		std::vector<uint8_t> lineRead(tempWidth, 0);

		std::array<size_t, 7> sizeOffset;
		size_t last = 0;
		for (size_t i = 0; i < 7; ++i) {
			sizeOffset[i] = last;
			last += ((tempWidth - 1) / static_cast<size_t>(pow(2, 12 - i))) + 1;
		}
		std::vector<ImageTile> tile(sizeOffset[6]);

		//Init tiles
		for (auto& t : tile) {
			t.pngPtr = nullptr;
			t.pngInfo = nullptr;
		}

		for (size_t y = 0; y < m_origH; ++y) {
			if (y % 100 == 0) {
				helper::printProgress(y, m_origH);
			}
			// paint each image on this one
			std::fill(lineWrite.begin(), lineWrite.end(), 0);
			// the partial images are kept in this list. they're already in the correct order in which they have to me merged and blended
			for (auto it = m_partList.begin(); it != m_partList.end(); ++it) {
				ImagePart& img = *it;
				// do we have to open this image?
				if (img.y != static_cast<int>(y) && img.pngPtr == nullptr)
					continue;

				if (img.pngPtr == nullptr) {
					assert(img.pngInfo == nullptr);
					assert(!img.file.is_open());

					img.file.open(img.filename, std::ios::in | std::ios::binary);
					if (img.file.fail()) {
						std::cerr << "Error opening temporary image " << img.filename << '\n';
						return false;
					}
					img.pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
					if (img.pngPtr == NULL) {
						std::cerr << "Error creating read struct for temporary image " << img.filename << '\n';
						return false; // Not really cleaning up here, but program will terminate anyways, so why bother
					}
					img.pngInfo = png_create_info_struct(img.pngPtr);
					if (img.pngInfo == NULL || setjmp(png_jmpbuf(img.pngPtr))) {
						std::cerr << "Error reading data from temporary image " << img.filename << '\n';
						return false; // Same here
					}

					png_set_read_fn(img.pngPtr, static_cast<png_voidp>(&img.file), userReadData);
					png_read_info(img.pngPtr, img.pngInfo);

					// Check if image dimensions match what is expected
					int type, interlace, comp, filter, bitDepth;
					png_uint_32 width, height;
					png_uint_32 ret = png_get_IHDR(img.pngPtr, img.pngInfo, &width, &height, &bitDepth, &type, &interlace, &comp, &filter);
					if (ret == 0 || width != static_cast<png_uint_32>(img.width) || height != static_cast<png_uint_32>(img.height)) {
						std::cerr << "Temp image " << img.filename << " has wrong dimensions; expected " << std::to_string(img.width) << 'x' << std::to_string(img.height) << ", got " << width << 'x' << height << '\n';
						return false;
					}
				}
				// Here, the image is either open and ready for reading another line, or its not open when it doesn't have to be copied/blended here, or is already finished
				if (img.pngPtr == nullptr) {
					continue;   // Not your turn, image!
				}
				// Read next line from current image chunk
				png_read_row(img.pngPtr, lineRead.data(), NULL);
				// Now this puts all the pixels in the right spot of the current line of the final image
				const size_t end = (static_cast<size_t>(img.x) + img.width) * CHANSPERPIXEL;
				size_t read = 0;
				for (size_t write = static_cast<size_t>(img.x) * CHANSPERPIXEL; write < end; write += CHANSPERPIXEL) {
					draw::blend(&lineWrite[write], &lineRead[read]);
					read += CHANSPERPIXEL;
				}
				// Now check if we're done with this image chunk
				if (--(img.height) == 0) { // if so, close and discard
					png_destroy_read_struct(&(img.pngPtr), &(img.pngInfo), NULL);
					img.file.close();
					img.pngPtr = nullptr;
					remove(img.filename.c_str());
				}
			}
			// Done composing this line, write to final image
			// Tiled output
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
						ImageTile &t = tile[tileIndex];
						if (t.fileHandle.is_open()) { // Unload/close first
							png_write_end(t.pngPtr, NULL);
							png_destroy_write_struct(&(t.pngPtr), &(t.pngInfo));
							t.fileHandle.close();
						}
						if (tileWidth * (tileIndex - sizeOffset[tileSize]) < m_origW) {
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
								//error in pngLib
								std::cerr << "Error in libPng\n";
								return false;
							}
							t.pngInfo = png_create_info_struct(t.pngPtr);
							if (t.pngInfo == nullptr) {
								std::cerr << "Error creating png info struct!\n";
								png_destroy_write_struct(&(t.pngPtr), NULL);
								return false;
							}
							png_set_write_fn(t.pngPtr, static_cast<png_voidp>(&t.fileHandle), userWriteData, NULL);
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
					png_write_row(tile[tileIndex].pngPtr, &lineWrite[tileWidth * (tileIndex - sizeOffset[tileSize]) * CHANSPERPIXEL]);
				}
			} // done writing line

		}
		// Y-Loop

		// Finish all current tiles
		std::fill(lineWrite.begin(), lineWrite.end(), 0);;
		for (size_t tileSize = 0; tileSize < 6; ++tileSize) {
			const size_t tileWidth = static_cast<size_t>(pow(2, 12 - tileSize));
			for (size_t tileIndex = sizeOffset[tileSize]; tileIndex < sizeOffset[tileSize + 1]; ++tileIndex) {
				if (!tile[tileIndex].fileHandle.is_open()) continue;
				const size_t imgEnd = (((m_origH - 1) / tileWidth) + 1) * tileWidth;
				for (size_t i = m_origH; i < imgEnd; ++i) {
					png_write_row(tile[tileIndex].pngPtr, lineWrite.data());
				}
				png_write_end(tile[tileIndex].pngPtr, NULL);
				png_destroy_write_struct(&(tile[tileIndex].pngPtr), &(tile[tileIndex].pngInfo));
				tile[tileIndex].fileHandle.close();
			}
		}

		helper::printProgress(10, 10);
		return true;
	}
}