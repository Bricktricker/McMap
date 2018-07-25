/*
 * PngReader.cpp
 *
 *  Created on: 04.11.2010
 *      Author: Zahl, Philipp
 */

#include "pngreader.h"
#include <png.h>
#include <fstream>

void userReadData(png_structp pngPtr, png_bytep data, png_size_t length)
{
	//Here we get our IO pointer back from the read struct.
	//This is the parameter we passed to the png_set_read_fn() function.
	//Our std::istream pointer.
	png_voidp a = png_get_io_ptr(pngPtr);
	//Cast the pointer to std::ifstream* and read 'length' bytes into 'data'

	((std::ifstream*)a)->read((char*)data, length);
}

PngReader::PngReader(const std::string& filename)
	: _width(0), _height(0), _chans(Unknown), _bitDepth(0), _bytesPerPixel(0), _status(0)
{
	open(filename);
}

PngReader::PngReader()
	: _width(0), _height(0), _chans(Unknown), _bitDepth(0), _bytesPerPixel(0), _status(0)
{
}

void PngReader::open(const std::string & filename)
{
	uint8_t **rows = NULL;
	// Open PNG file
	std::ifstream source(filename, std::ios::in | std::ios::binary);
	if (!source.good()) {
		_status |= 1;
		return;
	}

	constexpr size_t PNGSIGSIZE = 8;
	png_byte header[8]; // Check header
	source.read((char*)header, PNGSIGSIZE);
	if (!source.good()) {
		_status |= (1 << 0);
		return;
	}

	if (png_sig_cmp(header, 0, PNGSIGSIZE) != 0) {
		_status |= (1 << 1);
		return;
	}//Not a PNG file

	// Set up libpng to read file
	png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (pngPtr == NULL) {
		_status |= (1 << 2);
		return;
	}
	png_infop pngInfo = png_create_info_struct(pngPtr);
	if (pngInfo == nullptr) {
		png_destroy_read_struct(&pngPtr, NULL, NULL);
		_status |= (1 << 2);
		return;
	}

	if (setjmp(png_jmpbuf(pngPtr))) {
		//An error occured, so clean up what we have allocated so far...
		png_destroy_read_struct(&pngPtr, &pngInfo, NULL);
		if (rows != NULL) delete[] rows;
		_status |= (1 << 2);

		//Make sure you return here. libPNG will jump to here if something
		//goes wrong, and if you continue with your normal code, you might
		//End up with an infinite loop.
		return;
	}

	png_set_read_fn(pngPtr, (png_voidp)&source, userReadData);

	png_set_sig_bytes(pngPtr, PNGSIGSIZE);
	png_read_info(pngPtr, pngInfo);

	png_read_info(pngPtr, pngInfo);
	png_uint_32 width, height;
	int type, interlace, comp, filter; // Check image format (square, RGBA)
	png_uint_32 ret = png_get_IHDR(pngPtr, pngInfo, &width, &height, &_bitDepth, &type, &interlace, &comp, &filter);
	if (ret == 0) {
		png_destroy_read_struct(&pngPtr, &pngInfo, NULL);
		_status |= (1 << 2);
		return;
	}
	// Assign properties
	_width = width;
	_height = height;
	switch (type) {
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		_chans = GrayScaleAlpha;
		_bytesPerPixel = 2;
		break;
	case PNG_COLOR_TYPE_GRAY:
		_chans = GrayScale;
		_bytesPerPixel = 1;
		break;
	case PNG_COLOR_TYPE_PALETTE:
		_chans = Palette;
		_bytesPerPixel = 1;
		break;
	case PNG_COLOR_TYPE_RGB:
		_chans = RGB;
		_bytesPerPixel = 3;
		break;
	case PNG_COLOR_TYPE_RGBA:
		_chans = RGBA;
		_bytesPerPixel = 4;
		break;
	default:
		_chans = Unknown;
		_bytesPerPixel = 0;
	}
	_bytesPerPixel = (_bytesPerPixel * _bitDepth) / 8;
	// Alloc mem
	_imageData.resize(width * height * _bytesPerPixel);
	rows = new uint8_t*[height];
	for (png_uint_32 i = 0; i < height; ++i) {
		rows[i] = &_imageData[i * width * _bytesPerPixel]; // _imageData +i * width * _bytesPerPixel;
	}
	png_read_image(pngPtr, rows);
	png_destroy_read_struct(&pngPtr, &pngInfo, NULL);
	delete[] rows;

	if (!source.good()) _status |= 1;
}
