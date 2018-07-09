/**
 * This file contains functions to create and draw to a png image
 */

#include "draw_png.h"
#include "helper.h"
#include "colors.h"
#include "globals.h"
#include <png.h>
#include <list>

#include <string>
#include <fstream>
#include <iostream>
#include <vector>

#ifndef _WIN32
#include <sys/stat.h>
#endif
#if defined(_WIN32) && !defined(__GNUC__)
#  include <direct.h>
#endif

#ifndef Z_BEST_SPEED
#	define Z_BEST_SPEED 6
#endif

#define PIXEL(x,y) (gImageBuffer[((x) + gOffsetX) * CHANSPERPIXEL + ((y) + gOffsetY) * gPngLocalLineWidthChans])

namespace
{
	struct ImagePart {
		int x, y, width, height;
		std::string filename;
		std::fstream pngFileHandle;
		png_structp pngPtr;
		png_infop pngInfo;
		ImagePart(const std::string& _file, const int _x, const int _y, const int _w, const int _h)
			: x(_x), y(_y), width(_w), height(_h), filename(_file)
		{
			pngPtr = NULL;
			pngInfo = NULL;
		}
		ImagePart(const ImagePart& part)
			: x(part.x), y(part.y), width(part.width), height(part.height), filename(part.filename), pngPtr(part.pngPtr), pngInfo(part.pngInfo)

		{
			pngFileHandle.open(filename, std::ios::binary);
			std::cout << "Copy Constructor\n";
		}

	};
	struct ImageTile {
		std::fstream fileHandle;
		png_structp pngPtr;
		png_infop pngInfo;

		ImageTile()
			: pngPtr(nullptr), pngInfo(nullptr) {}

	};
	typedef std::list<ImagePart> imageList;
	imageList partialImages;

	std::vector<uint8_t> gImageBuffer;
	int gPngLocalLineWidthChans = 0, gPngLocalWidth = 0, gPngLocalHeight = 0;
	int gPngLineWidthChans = 0, gPngWidth = 0, gPngHeight = 0;
	int gOffsetX = 0, gOffsetY = 0;
	uint64_t gPngSize = 0, gPngLocalSize = 0;
	png_structp pngPtrMain = NULL; // Main image
	png_infop pngInfoPtrMain = NULL;
	png_structp pngPtrCurrent = NULL; // This will be either the same as above, or a temp image when using disk caching
	std::fstream gPngPartialFileHandle;

	inline void assignBiome(uint8_t* const color, const uint8_t biome, const uint16_t block);
	inline void blend(uint8_t* const destination, const Color_t& source); //Blend color to pixel
	inline void blend(uint8_t* const destination, const uint8_t* const source); //Blend to pixel
	Color_t modColor(const Color_t& color, const int mod);
	inline void modColor(uint8_t* const pos, const int mod);
	inline void addColor(uint8_t * const color, const int16_t * const add);
	inline void addColorSimple(uint8_t * const color, const int16_t * const add);
	inline void setColor(uint8_t* const pos, const Color_t& color);

	// Split them up so setPixel won't be one hell of a mess
	void setSnow(const size_t x, const size_t y, const Color_t& color);
	void setTorch(const size_t x, const size_t y, const Color_t& color);
	void setFlower(const size_t x, const size_t y, const Color_t& color);
	void setRedwire(const size_t x, const size_t y, const Color_t& color);
	void setFire(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark);
	void setGrass(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, const int sub);
	void setFence(const size_t x, const size_t y, const Color_t& color);
	void setStep(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark);
	void setUpStep(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark);
# define setRailroad setSnowBA

	// Then make duplicate copies so it is one hell of a mess (BlendAll functions)
	// ..but hey, its for speeeeeeeeed!
	void setSnowBA(const size_t x, const size_t y, const Color_t& color);
	void setTorchBA(const size_t x, const size_t y, const Color_t& color);
	void setFlowerBA(const size_t x, const size_t y, const Color_t& color);
	void setGrassBA(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, const int sub);
	void setStepBA(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark);
	void setUpStepBA(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark);

	void userWriteData(png_structp pngPtr, png_bytep data, uint32_t length)
	{
		//Our std::ostream pointer.
		png_voidp a = png_get_io_ptr(pngPtr);
		//Cast the pointer to std::ifstream* and read 'length' bytes into 'data'

		//((std::ifstream*)a)->read((char*)data, length);
		((std::fstream*)a)->write((char*)data, length);
	}

	void userReadData(png_structp pngPtr, png_bytep data, png_size_t length)
	{
		//Here we get our IO pointer back from the read struct.
		//This is the parameter we passed to the png_set_read_fn() function.
		//Our std::istream pointer.
		png_voidp a = png_get_io_ptr(pngPtr);
		//Cast the pointer to std::ifstream* and read 'length' bytes into 'data'

		((std::fstream*)a)->read((char*)data, length);
	}

}

void createImageBuffer(const size_t width, const size_t height, const bool splitUp)
{
	gPngLocalWidth = gPngWidth = (int)width;
	gPngLocalHeight = gPngHeight = (int)height;
	gPngLocalLineWidthChans = gPngLineWidthChans = gPngWidth * CHANSPERPIXEL;
	gPngSize = gPngLocalSize = (uint64_t)gPngLineWidthChans * (uint64_t)gPngHeight;
	std::cout << "Image dimensions are " << gPngWidth << 'x' << gPngHeight << ", 32bpp, " << float(gPngSize / float(1024 * 1024)) << "MiB\n";
	if (!splitUp) {
		gImageBuffer.resize(static_cast<size_t>(gPngSize), 0);
	}
}

bool createImage(FILE *fh, const size_t width, const size_t height, const bool splitUp)
{
	gPngLocalWidth = gPngWidth = (int)width;
	gPngLocalHeight = gPngHeight = (int)height;
	gPngLocalLineWidthChans = gPngLineWidthChans = gPngWidth * 4;
	gPngSize = gPngLocalSize = (uint64_t)gPngLineWidthChans * (uint64_t)gPngHeight;
	std::cout << "Image dimensions are " << gPngWidth << 'x' << gPngHeight << ", 32bpp, " << float(gPngSize / float(1024 * 1024)) << "MiB\n";
	if (!splitUp) {
		gImageBuffer.resize(static_cast<size_t>(gPngSize), 0);
	}
	fseek64(fh, 0, SEEK_SET);
	// Write header
	pngPtrMain = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (pngPtrMain == NULL) {
		return false;
	}

	pngInfoPtrMain = png_create_info_struct(pngPtrMain);

	if (pngInfoPtrMain == NULL) {
		png_destroy_write_struct(&pngPtrMain, NULL);
		return false;
	}

	if (setjmp(png_jmpbuf(pngPtrMain))) { // libpng will issue a longjmp on error, so code flow will end up
		png_destroy_write_struct(&pngPtrMain, &pngInfoPtrMain); // here if something goes wrong in the code below
		return false;
	}

	png_init_io(pngPtrMain, fh);

	png_set_IHDR(pngPtrMain, pngInfoPtrMain, (uint32_t)width, (uint32_t)height,
	             8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
	             PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_text title_text;
	title_text.compression = PNG_TEXT_COMPRESSION_NONE;
	title_text.key = (png_charp)"Software";
	title_text.text = (png_charp)"mcmap";
	png_set_text(pngPtrMain, pngInfoPtrMain, &title_text, 1);

	png_write_info(pngPtrMain, pngInfoPtrMain);
	if (!splitUp) {
		pngPtrCurrent = pngPtrMain;
	}
	return true;
}

bool saveImage()
{
	if (Global::tilePath.empty()) {
		// Normal single-file output
		if (setjmp(png_jmpbuf(pngPtrMain))) { // libpng will issue a longjmp on error, so code flow will end up
			png_destroy_write_struct(&pngPtrMain, &pngInfoPtrMain); // here if something goes wrong in the code below
			std::cerr << "Something went wrong with pngLib\n";
			return false;
		}

		size_t srcLine = 0;
		//uint8_t *srcLine = gImageBuffer;
		std::cout << "Writing to file...\n";
		for (int y = 0; y < gPngHeight; ++y) {
			if (y % 25 == 0) {
				printProgress(size_t(y), size_t(gPngHeight));
			}
			png_write_row(pngPtrMain, (png_bytep)&gImageBuffer[srcLine]);
			srcLine += gPngLineWidthChans;
		}
		printProgress(10, 10);
		png_write_end(pngPtrMain, NULL);
		png_destroy_write_struct(&pngPtrMain, &pngInfoPtrMain);
		//
	} else {
		// Tiled output, suitable for google maps
		std::cout << "Writing to files...\n";
		std::string tmpString;
		// Prepare a temporary buffer to copy the current line to, since we need the width to be a multiple of 4096
		// and adjusting the whole image to that would be a waste of memory
		const size_t tempWidth = ((gPngWidth - 5) / 4096 + 1) * 4096;
		const size_t tempWidthChans = tempWidth * CHANSPERPIXEL;
#ifdef _DEBUG
		std::cout << "Temp width: " << tempWidthChans << ", original width: " << gPngLineWidthChans << '\n';
#endif
		std::vector<uint8_t> tempLine(tempWidthChans, 0);
		// Source pointer
		size_t srcLine = 0;
		//uint8_t *srcLine = gImageBuffer;
		// Prepare an array of png structs that will output simultaneously to the various tiles
		size_t sizeOffset[7], last = 0;
		for (size_t i = 0; i < 7; ++i) {
			sizeOffset[i] = last;
			last += ((tempWidth - 1) / static_cast<size_t>(pow(2, 12 - i))) + 1;
		}
		std::vector<ImageTile> tile(sizeOffset[6]);
		for (int y = 0; y < gPngHeight; ++y) {
			if (y % 25 == 0) {
				printProgress(size_t(y), size_t(gPngHeight));
			}
			memcpy(tempLine.data(), &gImageBuffer[srcLine], gPngLineWidthChans);
			srcLine += gPngLineWidthChans;
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
					for (size_t tileIndex = sizeOffset[tileSize]; tileIndex < sizeOffset[tileSize+1]; ++tileIndex) {
						ImageTile& t = tile[tileIndex];
						if (t.fileHandle.is_open()) { // Unload/close first
							//printf("Calling end with ptr == %p, y == %d, start == %d, tileSize == %d, tileIndex == %d, to == %d, numpng == %d\n",
									//t.pngPtr, y, (int)start, (int)tileSize, (int)tileIndex, (int)sizeOffset[tileSize+1], (int)numpng);
							png_write_end(t.pngPtr, NULL);
							png_destroy_write_struct(&(t.pngPtr), &(t.pngInfo));
							t.fileHandle.close();
						}
						if (tileWidth * (tileIndex - sizeOffset[tileSize]) < size_t(gPngWidth)) {
							// Open new tile file for a while
							tmpString = Global::tilePath + "/x" + std::to_string(int(tileIndex - sizeOffset[tileSize])) + 'y' + std::to_string(int((y / pow(2, 12 - tileSize)))) + 'z' + std::to_string(int(tileSize)) + ".png";
							//snprintf(tmpString, tmpLen, "%s/x%dy%dz%d.png", g_TilePath,
									//int(tileIndex - sizeOffset[tileSize]), int((y / pow(2, 12 - tileSize))), int(tileSize));
#ifdef _DEBUG
							std::cout << "Starting tile " << tmpString << " of size " << (int)pow(2, 12 - tileSize) << "...\n";
#endif
							t.fileHandle.open(tmpString, std::ios::out | std::ios::binary);
							if (t.fileHandle.fail()) {
								std::cerr << "Error opening file!\n";
								return false;
							}
							t.pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
							if (t.pngPtr == NULL) {
								std::cerr << "Error creating png write struct!\n";
								return false;
							}
							if (setjmp(png_jmpbuf(t.pngPtr))) {
								std::cerr << "Something went wrong with pngLib\n";
								return false;
							}
							t.pngInfo = png_create_info_struct(t.pngPtr);
							if (t.pngInfo == NULL) {
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
				for (size_t tileIndex = sizeOffset[tileSize]; tileIndex < sizeOffset[tileSize+1]; ++tileIndex) {
					if (tile[tileIndex].fileHandle.fail() || !tile[tileIndex].fileHandle.is_open()) continue;
					png_write_row(tile[tileIndex].pngPtr, png_bytep(&tempLine[tileWidth * (tileIndex - sizeOffset[tileSize]) * CHANSPERPIXEL]));
				}
			} // done writing line
		} // done with whole image
		// Now the last set of tiles is not finished, so do that manually

		std::fill(tempLine.begin(), tempLine.end(), 0);
		for (size_t tileSize = 0; tileSize < 6; ++tileSize) {
			const size_t tileWidth = static_cast<size_t>(pow(2, 12 - tileSize));
			for (size_t tileIndex = sizeOffset[tileSize]; tileIndex < sizeOffset[tileSize+1]; ++tileIndex) {
				if (tile[tileIndex].fileHandle.fail() || !tile[tileIndex].fileHandle.is_open()) continue; //.fileHandle == NULL
				const int imgEnd = (((gPngHeight - 1) / tileWidth) + 1) * tileWidth;
				for (int i = gPngHeight; i < imgEnd; ++i) {
					png_write_row(tile[tileIndex].pngPtr, png_bytep(tempLine.data())); //writes just 0's
				}
				png_write_end(tile[tileIndex].pngPtr, NULL);
				png_destroy_write_struct(&(tile[tileIndex].pngPtr), &(tile[tileIndex].pngInfo)); 
			}
		}
		printProgress(10, 10);
	}
	return true;
}

/**
 * @return 0 = OK, -1 = Error, 1 = Zero/Negative size
 */
int loadImagePart(const int startx, const int starty, const int width, const int height)
{
	// These are set to NULL in saveImagePartPng to make sure the two functions are called in turn
	if (pngPtrCurrent != NULL || gPngPartialFileHandle.is_open()) {
		std::cerr << "Something wrong with disk caching.\n";
		__debugbreak(); //Check
		return -1;
	}
	// In case the image needs to be cropped the offsets will be negative
	gOffsetX = MIN(startx, 0);
	gOffsetY = MIN(starty, 0);
	gPngLocalWidth = width;
	gPngLocalHeight = height;
	int localX = startx;
	int localY = starty;
	// Also modify gPngLocalWidth and gPngLocalHeight in these cases
	if (localX < 0) {
		gPngLocalWidth += localX;
		localX = 0;
	}
	if (localY < 0) {
		gPngLocalHeight += localY;
		localY = 0;
	}
	if (localX + gPngLocalWidth > gPngWidth) {
		gPngLocalWidth = gPngWidth - localX;
	}
	if (localY + gPngLocalHeight > gPngHeight) {
		gPngLocalHeight = gPngHeight - localY;
	}
	if (gPngLocalWidth < 1 || gPngLocalHeight < 1) return 1;
	std::string name = "cache/" + std::to_string(localX) + '.' + std::to_string(localY) + '.' + std::to_string(gPngLocalWidth) + '.' + std::to_string(gPngLocalHeight) + '.' + std::to_string((int)time(NULL)) + ".png";
	ImagePart img(name, localX, localY, gPngLocalWidth, gPngLocalHeight);
	partialImages.push_back(img);
	// alloc mem for image and open tempfile
	gPngLocalLineWidthChans = gPngLocalWidth * CHANSPERPIXEL;
	uint64_t size = (uint64_t)gPngLocalLineWidthChans * (uint64_t)gPngLocalHeight;
	std::cout << "Creating temporary image: " << gPngLocalWidth <<  'x' << gPngLocalHeight << ", 32bpp, " << float(size / float(1024 * 1024)) << "MiB\n";
	if (gImageBuffer.empty()) {
		gImageBuffer.resize(static_cast<size_t>(size));
		gPngLocalSize = size;
	} else if (size > gPngLocalSize) {
		gImageBuffer.resize(static_cast<size_t>(size));
		gPngLocalSize = size;
	}
	std::fill(gImageBuffer.begin(), gImageBuffer.end(), 0);
	// Create temp image
	// This is done here to detect early if the target is not writable
#ifdef _WIN32
	mkdir("cache");
#else
	mkdir("cache", 0755);
#endif
	gPngPartialFileHandle = std::fstream(name, std::ios::out | std::ios::binary);
	if (gPngPartialFileHandle.fail()) {
		std::cerr << "Could not create temporary image at "<< name <<"; check permissions in current dir.\n";
		return -1;
	}
	return 0;
}

bool saveImagePart()
{
	if (gPngPartialFileHandle.fail() || !gPngPartialFileHandle.is_open() || pngPtrCurrent != NULL) {
		std::cerr << "saveImagePart() called in bad state.\n";
		return false;
	}
	// Write header
	png_infop info_ptr = NULL;
	pngPtrCurrent = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (pngPtrCurrent == NULL) {
		return false;
	}

	info_ptr = png_create_info_struct(pngPtrCurrent);

	if (info_ptr == NULL) {
		png_destroy_write_struct(&pngPtrCurrent, NULL);
		return false;
	}

	if (setjmp(png_jmpbuf(pngPtrCurrent))) { // libpng will issue a longjmp on error, so code flow will end up
		png_destroy_write_struct(&pngPtrCurrent, &info_ptr); // here if something goes wrong in the code below
		return false;
	}

	png_set_write_fn(pngPtrCurrent, (png_voidp)&gPngPartialFileHandle, userWriteData, NULL);
	//png_init_io(pngPtrCurrent, gPngPartialFileHandle);
	png_set_compression_level(pngPtrCurrent, Z_BEST_SPEED);

	png_set_IHDR(pngPtrCurrent, info_ptr, (uint32_t)gPngLocalWidth, (uint32_t)gPngLocalHeight,
	             8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
	             PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(pngPtrCurrent, info_ptr);
	//
	size_t line = 0;
	for (int y = 0; y < gPngLocalHeight; ++y) {
		png_write_row(pngPtrCurrent, (png_bytep)&gImageBuffer[line]);
		line += gPngLocalLineWidthChans;
	}
	png_write_end(pngPtrCurrent, NULL);
	png_destroy_write_struct(&pngPtrCurrent, &info_ptr);
	pngPtrCurrent = NULL;
	gPngPartialFileHandle.close();
	return true;
}

bool discardImagePart()
{
	if (gPngPartialFileHandle.fail() || !gPngPartialFileHandle.is_open() || pngPtrCurrent != NULL) {
		std::cerr << "discardImagePart() called in bad state.\n";
		return false;
	}
	gPngPartialFileHandle.close();
	ImagePart& img = partialImages.back();
	remove(img.filename.c_str());
	partialImages.pop_back();
	return true;
}

bool composeFinalImage()
{
	std::string tmpString;
	size_t tmpLen = 0;
	if (Global::tilePath.empty()) {
		std::cout << "Composing final png file...\n";
		if (setjmp(png_jmpbuf(pngPtrMain))) {
			png_destroy_write_struct(&pngPtrMain, NULL);
			return false;
		}
	} else {
		// Tiled output, suitable for google maps
		std::cout << "Composing final png files...\n";
		// Prepare a temporary buffer to copy the current line to, since we need the width to be a multiple of 4096
		// and adjusting the whole image to that would be a waste of memory
	}
	const size_t tempWidth = (Global::tilePath.empty() ? gPngLineWidthChans : ((gPngWidth - 5) / 4096 + 1) * 4096);
	const size_t tempWidthChans = tempWidth * CHANSPERPIXEL;

	std::vector<uint8_t> lineWrite(tempWidthChans);
	std::vector<uint8_t> lineRead(gPngLineWidthChans);

	// Prepare an array of png structs that will output simultaneously to the various tiles
	size_t sizeOffset[7], last = 0;
	ImageTile *tile = NULL;
	if (!Global::tilePath.empty()) {
		for (size_t i = 0; i < 7; ++i) {
			sizeOffset[i] = last;
			last += ((tempWidth - 1) / static_cast<size_t>(pow(2, 12 - i))) + 1;
		}
		tile = new ImageTile[sizeOffset[6]];
		memset(tile, 0, sizeOffset[6] * sizeof(ImageTile));
	}

	for (int y = 0; y < gPngHeight; ++y) {
		if (y % 100 == 0) {
			printProgress(size_t(y), size_t(gPngHeight));
		}
		// paint each image on this one
		std::fill(lineWrite.begin(), lineWrite.end(), 0);
		// the partial images are kept in this list. they're already in the correct order in which they have to me merged and blended
		for (imageList::iterator it = partialImages.begin(); it != partialImages.end(); it++) {
			ImagePart& img = *it;
			// do we have to open this image?
			if (img.y == y && img.pngPtr == NULL) {
				img.pngFileHandle = std::fstream(img.filename, std::ios::in | std::ios::binary);
				if (img.pngFileHandle.fail()) {
					std::cerr << "Error opening temporary image " << img.filename << '\n';
					return false;
				}
				img.pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
				if (img.pngPtr == NULL) {
					std::cerr << "Error creating read struct for temporary image " << img.filename <<'\n';
					return false; // Not really cleaning up here, but program will terminate anyways, so why bother
				}
				img.pngInfo = png_create_info_struct(img.pngPtr);
				if (img.pngInfo == NULL || setjmp(png_jmpbuf(img.pngPtr))) {
					std::cerr << "Error reading data from temporary image " << img.filename << '\n';
					return false; // Same here
				}

				png_set_read_fn(img.pngPtr, (png_voidp)&img.pngFileHandle, userReadData);
				//png_init_io(img->pngPtr, img->pngFileHandle);
				png_read_info(img.pngPtr, img.pngInfo);
				// Check if image dimensions match what is expected
				int type, interlace, comp, filter, bitDepth;
				png_uint_32 width, height;
				png_uint_32 ret = png_get_IHDR(img.pngPtr, img.pngInfo, &width, &height, &bitDepth, &type, &interlace, &comp, &filter);
				if (ret == 0 || width != (png_uint_32)img.width || height != (png_uint_32)img.height) {
					std::cerr << "Temp image " << img.filename << " has wrong dimensions; expected " << std::to_string(img.width) << 'x' << std::to_string(img.height) << ", got " << width << 'x' << height << '\n';
					return false;
				}
			}
			// Here, the image is either open and ready for reading another line, or its not open when it doesn't have to be copied/blended here, or is already finished
			if (img.pngPtr == NULL) {
				continue;   // Not your turn, image!
			}
			// Read next line from current image chunk
			png_read_row(img.pngPtr, (png_bytep)lineRead.data(), NULL);
			// Now this puts all the pixels in the right spot of the current line of the final image
			const size_t end = (img.x + img.width) * CHANSPERPIXEL;
			size_t read = 0;
			for (size_t write = (img.x * CHANSPERPIXEL); write < end; write += CHANSPERPIXEL) {
				blend(&lineWrite[write], &lineRead[read]);
				read += CHANSPERPIXEL;
			}
			// Now check if we're done with this image chunk
			if (--(img.height) == 0) { // if so, close and discard
				png_destroy_read_struct(&(img.pngPtr), &(img.pngInfo), NULL);
				img.pngFileHandle.close();
				img.pngPtr = NULL;
				remove(img.filename.c_str());
			}
		}
		// Done composing this line, write to final image
		if (Global::tilePath.empty()) {
			// Single file
			png_write_row(pngPtrMain, (png_bytep)lineWrite.data());
		} else {
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
					for (size_t tileIndex = sizeOffset[tileSize]; tileIndex < sizeOffset[tileSize+1]; ++tileIndex) {
						ImageTile &t = tile[tileIndex];
						if (t.fileHandle.is_open()) { // Unload/close first
							//printf("Calling end with ptr == %p, y == %d, start == %d, tileSize == %d, tileIndex == %d, to == %d, numpng == %d\n",
									//t.pngPtr, y, (int)start, (int)tileSize, (int)tileIndex, (int)sizeOffset[tileSize+1], (int)numpng);
							png_write_end(t.pngPtr, NULL);
							png_destroy_write_struct(&(t.pngPtr), &(t.pngInfo));
							t.fileHandle.close();
						}
						if (tileWidth * (tileIndex - sizeOffset[tileSize]) < size_t(gPngWidth)) {
							// Open new tile file for a while
							tmpString = Global::tilePath + "/x" + std::to_string(int(tileIndex - sizeOffset[tileSize])) + 'y' + std::to_string(int((y / pow(2, 12 - tileSize)))) + 'z' + std::to_string(int(tileSize)) + ".png";
							//snprintf(tmpString, tmpLen, "%s/x%dy%dz%d.png", g_TilePath,
									//int(tileIndex - sizeOffset[tileSize]), int((y / pow(2, 12 - tileSize))), int(tileSize));
#ifdef _DEBUG
							std::cout << "Starting tile " << tmpString << " of size " << (int)pow(2, 12 - tileSize) << "...\n";
#endif
							t.fileHandle.open(tmpString, std::ios::out | std::ios::binary);
							if (t.fileHandle.fail()) {
								std::cerr << "Error opening file!\n";
								return false;
							}
							t.pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
							if (t.pngPtr == NULL) {
								std::cerr << "Error creating png write struct!\n";
								return false;
							}
							if (setjmp(png_jmpbuf(t.pngPtr))) {
								__debugbreak(); //error in pngLib
								return false;
							}
							t.pngInfo = png_create_info_struct(t.pngPtr);
							if (t.pngInfo == NULL) {
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
				for (size_t tileIndex = sizeOffset[tileSize]; tileIndex < sizeOffset[tileSize+1]; ++tileIndex) {
					if (tile[tileIndex].fileHandle.fail() || !tile[tileIndex].fileHandle.is_open()) continue;
					png_write_row(tile[tileIndex].pngPtr, png_bytep(&lineWrite[tileWidth * (tileIndex - sizeOffset[tileSize]) * CHANSPERPIXEL]));
				}
			} // done writing line
			//
		}
		// Y-Loop
	}
	if (Global::tilePath.empty()) {
		png_write_end(pngPtrMain, NULL);
		png_destroy_write_struct(&pngPtrMain, &pngInfoPtrMain);
	} else {
		// Finish all current tiles
		std::fill(lineWrite.begin(), lineWrite.end(), 0);;
		for (size_t tileSize = 0; tileSize < 6; ++tileSize) {
			const size_t tileWidth = static_cast<size_t>(pow(2, 12 - tileSize));
			for (size_t tileIndex = sizeOffset[tileSize]; tileIndex < sizeOffset[tileSize+1]; ++tileIndex) {
				if (!tile[tileIndex].fileHandle.is_open()) continue;
				const int imgEnd = (((gPngHeight - 1) / tileWidth) + 1) * tileWidth;
				for (int i = gPngHeight; i < imgEnd; ++i) {
					png_write_row(tile[tileIndex].pngPtr, png_bytep(lineWrite.data()));
				}
				png_write_end(tile[tileIndex].pngPtr, NULL);
				png_destroy_write_struct(&(tile[tileIndex].pngPtr), &(tile[tileIndex].pngInfo));
				tile[tileIndex].fileHandle.close();
			}
		}
	}
	printProgress(10, 10);
	return true;
}

uint64_t calcImageSize(const int mapChunksX, const int mapChunksZ, const size_t mapHeight, int &pixelsX, int &pixelsY, const bool tight)
{
	pixelsX = (mapChunksX * CHUNKSIZE_X + mapChunksZ * CHUNKSIZE_Z) * 2 + (tight ? 3 : 10);
	pixelsY = (mapChunksX * CHUNKSIZE_X + mapChunksZ * CHUNKSIZE_Z + int(mapHeight) * Global::OffsetY) + (tight ? 3 : 10);
	return uint64_t(pixelsX) * BYTESPERPIXEL * uint64_t(pixelsY);
}

/*
color: First 8 LSBits: BlockID, First 8MSBits: extraData (see globals.cpp:23)
fsub: brightnessAdjustment
biom parameter not used
TODO: Rewrite to use new color system
*/
void setPixel(const size_t x, const size_t y, const uint16_t stateID, const float fsub, const uint16_t biome)
{
	//if (isLeave(stateID)) __debugbreak();
	// Sets pixels around x,y where A is the anchor
	// T = given color, D = darker, L = lighter
	// A T T T
	// D D L L
	// D D L L
	//	  D L
	// First determine how much the color has to be lightened up or darkened
	int sub = static_cast<int>(fsub * (static_cast<float>(colorMap[stateID].brightness) / 323.0f + 0.21f));  // The brighter the color, the stronger the impact
	//uint8_t L[CHANSPERPIXEL], D[CHANSPERPIXEL], c[CHANSPERPIXEL];
	Color_t L, D;
	Color_t currentColor = colorMap[stateID];
	// Now make a local copy of the color that we can modify just for this one block
	currentColor = modColor(currentColor, sub); //set brightness

	//if (g_UseBiomes && g_WorldFormat == 2) assignBiome(c, biome, color); //dropped biom support
	uint8_t colortype = currentColor.blockType % BLOCKBIOME;

	if (Global::settings.blendAll) {
		// Then check the block type, as some types will be drawn differently
		switch (colortype)
		{
		case BLOCKFLAT:
			setSnowBA(x, y, currentColor);
			return;
		case BLOCKTORCH:
			setTorchBA(x, y, currentColor);
			return;
		case BLOCKFLOWER:
			setFlowerBA(x, y, currentColor);
			return;
		case BLOCKFENCE:
			setFence(x, y, currentColor);
			return;
		case BLOCKWIRE:
			setRedwire(x, y, currentColor);
			return;
		case BLOCKRAILROAD:
			setRailroad(x, y, currentColor);
			return;
		}
		// All the above blocks didn't need the shaded down versions of the color, so we only calc them here
		// They are for the sides of blocks
		L = modColor(currentColor, -17);
		D = modColor(currentColor, -27);

		// A few more blocks with special handling... Those need the two colors we just mixed
		switch (colortype)
		{
		case BLOCKGRASS:
			setGrassBA(x, y, currentColor, L, D, sub);
			return;
		case BLOCKFIRE:
			setFire(x, y, currentColor, L, D);
			return;
		case BLOCKSTEP:
			setStepBA(x, y, currentColor, L, D);
			return;
		case BLOCKUPSTEP:
			setUpStepBA(x, y, currentColor, L, D);
			return;
		}
	} else {
		// Then check the block type, as some types will be drawn differently
		switch (colortype)
		{
		case BLOCKFLAT:
			setSnow(x, y, currentColor);
			return;
		case BLOCKTORCH:
			setTorch(x, y, currentColor);
			return;
		case BLOCKFLOWER:
			setFlower(x, y, currentColor);
			return;
		case BLOCKFENCE:
			setFence(x, y, currentColor);
			return;
		case BLOCKWIRE:
			setRedwire(x, y, currentColor);
			return;
		case BLOCKRAILROAD:
			setRailroad(x, y, currentColor);
			return;
		}
		// All the above blocks didn't need the shaded down versions of the color, so we only calc them here
		// They are for the sides of blocks
		L = modColor(currentColor, -17);
		D = modColor(currentColor, -27);
		// A few more blocks with special handling... Those need the two colors we just mixed
		switch (colortype)
		{
		case BLOCKGRASS:
			setGrass(x, y, currentColor, L, D, sub);
			return;
		case BLOCKFIRE:
			setFire(x, y, currentColor, L, D);
			return;
		case BLOCKSTEP:
			setStep(x, y, currentColor, L, D);
			return;
		case BLOCKUPSTEP:
			setUpStep(x, y, currentColor, L, D);
			return;
		}
	}
	// In case the user wants noise, calc the strength now, depending on the desired intensity and the block's brightness
	int noise = 0;
	if (Global::settings.noise && currentColor.noise) {
		noise = static_cast<int>(static_cast<float>(Global::settings.noise * currentColor.noise) * (static_cast<float>(currentColor.brightness + 10) / 2650.0f));
		//noise = int(float(Global::settings.noise * colors[color][NOISE]) * (float(GETBRIGHTNESS(c) + 10) / 2650.0f));
	}
	// Ordinary blocks are all rendered the same way
	if (currentColor.a == 255) { // Fully opaque - faster
		// Top row
		uint8_t *pos = &PIXEL(x, y);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, currentColor);
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise);
			}
		}
		// Second row (going down)
		pos = &PIXEL(x, y+1);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, (i < 2 ? D : L));
			// The weird check here is to get the pattern right, as the noise should be stronger
			// every other row, but take into account the isometric perspective
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise * (i == 0 || i == 3 ? 1 : 2));
			}
		}
		// Third row (going down)
		pos = &PIXEL(x, y+2);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, (i < 2 ? D : L));
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise * (i == 0 || i == 3 ? 2 : 1));
			}
		}
		// Last row (going down)
		pos = &PIXEL(x, y+3);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, (i < 2 ? D : L));
			// The weird check here is to get the pattern right, as the noise should be stronger
			// every other row, but take into account the isometric perspective
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise * (i == 0 || i == 3 ? 1 : 2));
			}
		}
	} else { // Not opaque, use slower blending code
		// Top row
		uint8_t *pos = &PIXEL(x, y);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			blend(pos, currentColor);
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise);
			}
		}
		// Second row
		pos = &PIXEL(x, y+1);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			blend(pos, (i < 2 ? D : L));
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise * (i == 0 || i == 3 ? 1 : 2));
			}
		}
		// Third row
		pos = &PIXEL(x, y+2);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			blend(pos, (i < 2 ? D : L));
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise * (i == 0 || i == 3 ? 2 : 1));
			}
		}
		// Last row
		pos = &PIXEL(x, y+3);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			blend(pos, (i < 2 ? D : L));
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise * (i == 0 || i == 3 ? 1 : 2));
			}
		}
	}
	// The above two branches are almost the same, maybe one could just create a function pointer and...
}

void blendPixel(const size_t x, const size_t y, const uint16_t stateID, const float fsub)
{
	// This one is used for cave overlay
	// Sets pixels around x,y where A is the anchor
	// T = given color, D = darker, L = lighter
	// A T T T
	// D D L L
	// D D L L
	//	  D L
	//uint8_t L[CHANSPERPIXEL], D[CHANSPERPIXEL], c[CHANSPERPIXEL];
	Color_t L, D;
	Color_t currentColor = colorMap[stateID];
	// Now make a local copy of the color that we can modify just for this one block
	//c[PALPHA] = clamp(int(float(c[PALPHA]) * fsub)); // The brighter the color, the stronger the impact
	currentColor.a = clamp(static_cast<int>(static_cast<float>(currentColor.a) * fsub));
	// They are for the sides of blocks
	L = modColor(currentColor, -17);
	D = modColor(currentColor, -27);
	// In case the user wants noise, calc the strength now, depending on the desired intensity and the block's brightness
	int noise = 0;
	if (Global::settings.noise && currentColor.noise) {
		//noise = int(float(Global::settings.noise * colors[color][NOISE]) * (float(GETBRIGHTNESS(c) + 10) / 2650.0f));
		noise = static_cast<int>(static_cast<float>(currentColor.noise * Global::settings.noise) * (static_cast<float>(currentColor.brightness + 10) / 2650.0f));
	}
	// Top row
	uint8_t *pos = &PIXEL(x, y);
	for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
		blend(pos, currentColor);
		if (noise) {
			modColor(pos, rand() % (noise * 2) - noise);
		}
	}
	// Second row
	pos = &PIXEL(x, y+1);
	for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
		blend(pos, (i < 2 ? D : L));
		if (noise) {
			modColor(pos, rand() % (noise * 2) - noise * (i == 0 || i == 3 ? 1 : 2));
		}
	}
}

namespace
{

	inline void assignBiome(uint8_t* const color, const uint8_t biome, const uint16_t block)
	{
		//do there what you want, this code response for changing single pixel depending on its biome
		//note this works only for anvli format. old one still requires donkey kong biome extractor

		//if (colorMap[block].blockType / BLOCKBIOME)
			//addColor(color, biomes[biome]);
		__debugbreak(); //dropped biom support
	}

	inline void blend(uint8_t* const destination, const uint8_t* const source)
	{
#define PALPHA 3
		if (destination[PALPHA] == 0 || source[PALPHA] == 255) { //compare alpha
			memcpy(destination, source, BYTESPERPIXEL);
			return;
		}
#define BLEND(ca,aa,cb) uint8_t(((size_t(ca) * size_t(aa)) + (size_t(255 - aa) * size_t(cb))) / 255)
		destination[0] = BLEND(source[0], source[PALPHA], destination[0]);
		destination[1] = BLEND(source[1], source[PALPHA], destination[1]);
		destination[2] = BLEND(source[2], source[PALPHA], destination[2]);
		destination[PALPHA] += (size_t(source[PALPHA]) * size_t(255 - destination[PALPHA])) / 255;
	}

	inline void blend(uint8_t* const destination, const Color_t& source)
	{
		if (destination[PALPHA] == 0 || source.a == 255) { //compare alpha
			destination[0] = source.r;
			destination[1] = source.g;
			destination[2] = source.b;
			destination[3] = source.a;
			return;
		}
#define BLEND(ca,aa,cb) uint8_t(((size_t(ca) * size_t(aa)) + (size_t(255 - aa) * size_t(cb))) / 255)
		destination[0] = BLEND(source.r, source.a, destination[0]);
		destination[1] = BLEND(source.g, source.a, destination[1]);
		destination[2] = BLEND(source.b, source.a, destination[2]);
		destination[PALPHA] += (size_t(source.a) * size_t(255 - destination[PALPHA])) / 255;
	}

	Color_t modColor(const Color_t& color, const int mod)
	{
		Color_t retCol = color;
		retCol.r = clamp(color.r + mod);
		retCol.g = clamp(color.g + mod);
		retCol.b = clamp(color.b + mod);
		return retCol;
	}

	void modColor(uint8_t* const pos, const int mod)
	{
		pos[0] = clamp(pos[0] + mod);
		pos[1] = clamp(pos[1] + mod);
		pos[2] = clamp(pos[2] + mod);
	}

	inline void addColor(uint8_t* const color, const int16_t * const add)
	{
		const float v2 = (float(add[PALPHA]) / 255.0f);
		const float v1 = (1.0f - (v2 * .2f));
		color[0] = clamp(int16_t(float(color[0]) * v1 + float(add[0]) * v2));
		color[1] = clamp(int16_t(float(color[1]) * v1 + float(add[1]) * v2));
		color[2] = clamp(int16_t(float(color[2]) * v1 + float(add[2]) * v2));
	}

	inline void addColorSimple(uint8_t * const color, const int16_t * const add)
	{
		color[0] = clamp(color[0] + add[0]);
		color[1] = clamp(color[1] + add[1]);
		color[2] = clamp(color[2] + add[2]);
	}

	void setColor(uint8_t* const pos, const Color_t & color)
	{
		pos[0] = color.r;
		pos[1] = color.g;
		pos[2] = color.b;
		pos[3] = color.a;
	}

	//setzt color an pos + 3 weitere
	void setSnow(const size_t x, const size_t y, const Color_t& color)
	{
		// Top row (second row)
		uint8_t *pos = &PIXEL(x, y+1);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			pos[0] = color.r;
			pos[1] = color.g;
			pos[2] = color.b;
			pos[3] = color.a;
		}
	}

	void setTorch(const size_t x, const size_t y, const Color_t& color)
	{
		// Maybe the orientation should be considered when drawing, but it probably isn't worth the efford
		uint8_t *pos = &PIXEL(x+2, y+1);
		pos[0] = color.r;
		pos[1] = color.g;
		pos[2] = color.b;
		pos[3] = color.a;

		pos = &PIXEL(x + 2, y + 2);
		pos[0] = color.r;
		pos[1] = color.g;
		pos[2] = color.b;
		pos[3] = color.a;
	}

	void setFlower(const size_t x, const size_t y, const Color_t& color)
	{
		uint8_t *pos = &PIXEL(x, y+1);
		setColor(pos + (CHANSPERPIXEL), color);
		setColor(pos + (CHANSPERPIXEL*3), color);

		pos = &PIXEL(x + 2, y + 2);
		setColor(pos, color);

		pos = &PIXEL(x + 1, y + 3);
		setColor(pos, color);
	}

	void setFire(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark)
	{
		// This basically just leaves out a few pixels
		// Top row
		uint8_t *pos = &PIXEL(x, y);
		blend(pos, light);
		blend(pos + CHANSPERPIXEL*2, dark);
		// Second and third row
		for (size_t i = 1; i < 3; ++i) {
			pos = &PIXEL(x, y+i);
			blend(pos, dark);
			blend(pos+(CHANSPERPIXEL*i), color);
			blend(pos+(CHANSPERPIXEL*3), light);
		}
		// Last row
		pos = &PIXEL(x, y+3);
		blend(pos+(CHANSPERPIXEL*2), light);
	}

	void setGrass(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, const int sub)
	{
		// this will make grass look like dirt from the side
		__debugbreak(); //are the two lower lines working?
		Color_t L = colorMap[blockTree["minecraft:dirt"].get()];
		Color_t D = L;
		modColor(L, sub - 15);
		modColor(D, sub - 25);
		// consider noise
		int noise = 0;
		if (Global::settings.noise && color.noise) {
			//noise = int(float(Global::settings.noise * colors[GRASS][NOISE]) * (float(GETBRIGHTNESS(color) + 10) / 2650.0f));
			noise = static_cast<int>(static_cast<float>(Global::settings.noise * color.noise) * (static_cast<float>(color.brightness + 10) / 2650.0f));
		}
		// Top row
		uint8_t *pos = &PIXEL(x, y);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, color);
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise);
			}
		}
		// Second row
		pos = &PIXEL(x, y+1);
		setColor(pos, dark);
		setColor(pos+CHANSPERPIXEL, dark);
		setColor(pos+CHANSPERPIXEL*2, light);
		setColor(pos+CHANSPERPIXEL*3, light);

		// Third row
		pos = &PIXEL(x, y+2);
		setColor(pos, D);
		setColor(pos+CHANSPERPIXEL, D);
		setColor(pos+CHANSPERPIXEL*2, L);
		setColor(pos+CHANSPERPIXEL*3, L);

		// Last row
		pos = &PIXEL(x, y+3);
		setColor(pos, D);
		setColor(pos + CHANSPERPIXEL, D);
		setColor(pos + CHANSPERPIXEL * 2, L);
		setColor(pos + CHANSPERPIXEL * 3, L);
	}

	void setFence(const size_t x, const size_t y, const Color_t& color)
	{
		// First row
		uint8_t *pos = &PIXEL(x, y);
		blend(pos+CHANSPERPIXEL, color);
		blend(pos+CHANSPERPIXEL*2, color);
		// Second row
		pos = &PIXEL(x+1, y+1);
		blend(pos, color);
		// Third row
		pos = &PIXEL(x, y+2);
		blend(pos+CHANSPERPIXEL, color);
		blend(pos+CHANSPERPIXEL*2, color);
	}

	void setStep(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark)
	{
		uint8_t *pos = &PIXEL(x, y+1);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, color);
		}
		pos = &PIXEL(x, y+2);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, color);
		}
	}

	void setUpStep(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark)
	{
		uint8_t *pos = &PIXEL(x, y);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, color);
		}
		pos = &PIXEL(x, y+1);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, color);
		}
	}

	void setRedwire(const size_t x, const size_t y, const Color_t& color)
	{
		uint8_t *pos = &PIXEL(x+1, y+2);
		blend(pos, color);
		blend(pos+CHANSPERPIXEL, color);
	}

	// The g_BlendAll versions of the block set functions
	//
	void setSnowBA(const size_t x, const size_t y, const Color_t& color)
	{
		// Top row (second row)
		uint8_t *pos = &PIXEL(x, y+1);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			blend(pos, color);
		}
	}

	void setTorchBA(const size_t x, const size_t y, const Color_t& color)
	{
		// Maybe the orientation should be considered when drawing, but it probably isn't worth the effort
		uint8_t *pos = &PIXEL(x+2, y+1);
		blend(pos, color);
		pos = &PIXEL(x+2, y+2);
		blend(pos, color);
	}

	void setFlowerBA(const size_t x, const size_t y, const Color_t& color)
	{
		uint8_t *pos = &PIXEL(x, y+1);
		blend(pos+CHANSPERPIXEL, color);
		blend(pos+CHANSPERPIXEL*3, color);
		pos = &PIXEL(x+2, y+2);
		blend(pos, color);
		pos = &PIXEL(x+1, y+3);
		blend(pos, color);
	}

	void setGrassBA(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, const int sub)
	{
		// this will make grass look like dirt from the side
		__debugbreak(); //are the two lower lines working?
		Color_t L = colorMap[blockTree["minecraft:dirt"].get()];
		Color_t D = L;
		modColor(L, sub - 15);
		modColor(D, sub - 25);
		// consider noise
		int noise = 0;
		if (Global::settings.noise && color.noise) {
			//noise = int(float(Global::settings.noise * colors[GRASS][NOISE]) * (float(GETBRIGHTNESS(color) + 10) / 2650.0f));
			noise = static_cast<int>(static_cast<float>(Global::settings.noise * color.noise) * (static_cast<float>(color.brightness + 10) / 2650.0f));
		}
		// Top row
		uint8_t *pos = &PIXEL(x, y);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			blend(pos, color);
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise);
			}
		}
		// Second row
		pos = &PIXEL(x, y+1);
		blend(pos, dark);
		blend(pos+CHANSPERPIXEL, dark);
		blend(pos+CHANSPERPIXEL*2, light);
		blend(pos+CHANSPERPIXEL*3, light);
		// Third row
		pos = &PIXEL(x, y+2);
		blend(pos, D);
		blend(pos+CHANSPERPIXEL, D);
		blend(pos+CHANSPERPIXEL*2, L);
		blend(pos+CHANSPERPIXEL*3, L);
		// Last row
		pos = &PIXEL(x, y+3);
		blend(pos, D);
		blend(pos+CHANSPERPIXEL, D);
		blend(pos+CHANSPERPIXEL*2, L);
		blend(pos+CHANSPERPIXEL*3, L);
	}

	void setStepBA(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark)
	{
		uint8_t *pos = &PIXEL(x, y+1);
		for (size_t i = 0; i < 3; ++i, pos += CHANSPERPIXEL) {
			blend(pos, color);
		}
		pos = &PIXEL(x, y+2);
		for (size_t i = 0; i < 10; ++i, pos += CHANSPERPIXEL) {
			blend(pos, color);
		}
	}

	void setUpStepBA(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark)
	{
		uint8_t *pos = &PIXEL(x, y);
		for (size_t i = 0; i < 3; ++i, pos += CHANSPERPIXEL) {
			blend(pos, color);
		}
		pos = &PIXEL(x, y+1);
		for (size_t i = 0; i < 10; ++i, pos += CHANSPERPIXEL) {
			blend(pos, color);
		}
	}
}
