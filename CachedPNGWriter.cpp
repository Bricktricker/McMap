//C++ Header
#include <iostream>
#include <algorithm>
#include <ctime>
#include <fstream>
#include <cassert>
//My-Header
#include <png.h>
#include "CachedPNGWriter.h"
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

CachedPNGWriter::CachedPNGWriter(const size_t origW, const size_t origH)
	: m_currWidth(0), m_currHeight(0), m_origW(origW), m_origH(origH), offsetX(0), offsetY(0)
{
	if (!Dir::createDir("cache")) {
		std::cerr << "Could not create cache directory\n";
	}
}

bool CachedPNGWriter::reserve(const size_t width, const size_t height)
{
	const size_t pixSize = width * height * CHANSPERPIXEL;
	std::cout << "Creating temporary image: " << width << 'x' << height << ", 32bpp, " << float(pixSize / float(1024 * 1024)) << "MiB\n";
	m_buffer.resize(pixSize);
	std::fill(m_buffer.begin(), m_buffer.end(), 0);

	m_currWidth = width;
	m_currHeight = height;
	return true;
}

int CachedPNGWriter::addPart(const int startx, const int starty, const int width, const int height)
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

	if (localX + localWidth > static_cast<int>(m_origW)) {
		localWidth = static_cast<int>(m_origW) - localX;
	}
	if (localY + localHeight > static_cast<int>(m_origH)) {
		localHeight = static_cast<int>(m_origH) - localY;
	}

	if (localWidth < 1 || localHeight < 1) {
		return 1;
	}

	const std::string name = "cache/" + std::to_string(localX) + '.' + std::to_string(localY) + '.' + std::to_string(localWidth) + '.' + std::to_string(localHeight) + '.' + std::to_string((int)time(NULL)) + ".png";
	m_partList.emplace_back(name, localX, localY, localWidth, localHeight);

	if (!this->reserve(localWidth, localHeight))
		return -1;

	return 0;
}

bool CachedPNGWriter::write(const std::string& path)
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
		png_write_row(pngStruct, (png_bytep)&m_buffer.at(line));
		line += m_currWidth * CHANSPERPIXEL;
	}

	png_write_end(pngStruct, NULL);
	png_destroy_write_struct(&pngStruct, &pngInfo);

	m_currHeight = 0;
	m_currWidth = 0;

	return true;
}

uint8_t* CachedPNGWriter::getPixel(const size_t x, const size_t y)
{
	const int newX = static_cast<int>(x) + offsetX;
	const int newY = static_cast<int>(y) + offsetY;
	if (newX >= static_cast<int>(m_currWidth) || newY >= static_cast<int>(m_currHeight))
		throw std::out_of_range("getPixel out of range\n");

	const size_t idx = (x + offsetX) * CHANSPERPIXEL + (y + offsetY) * (m_currWidth * CHANSPERPIXEL);
	return &m_buffer.at(idx); //check index calculation
}

uint8_t* CachedPNGWriter::getPixelClamped(int x, int y)
{
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x > static_cast<int>(m_currWidth - 1)) x = static_cast<int>(m_currWidth - 1);
	if (y > static_cast<int>(m_currHeight - 1)) y = static_cast<int>(m_currHeight - 1);

	return &m_buffer[x*CHANSPERPIXEL + y * (m_currWidth * CHANSPERPIXEL)];
}

void CachedPNGWriter::discardPart()
{
	m_partList.pop_back();
	m_currHeight = 0;
	m_currWidth = 0;
}

bool CachedPNGWriter::compose(const std::string& path, const double scale)
{
	std::cout << "Composing final png file...\n";

	m_origH = static_cast<size_t>(m_origH * scale);
	m_origW = static_cast<size_t>(m_origW * scale);

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
	if (pngInfo == nullptr) {
		png_destroy_write_struct(&pngStruct, NULL);
		return false;
	}

	if (setjmp(png_jmpbuf(pngStruct))) { // libpng will issue a longjmp on error, so code flow will end up
		png_destroy_write_struct(&pngStruct, &pngInfo); // here if something goes wrong in the code below
		return false;
	}

	png_set_write_fn(pngStruct, (png_voidp)&outHandle, userWriteData, NULL);

	png_set_IHDR(pngStruct, pngInfo, (uint32_t)m_origW, (uint32_t)m_origH,
		8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_text title_text;
	title_text.compression = PNG_TEXT_COMPRESSION_NONE;
	title_text.key = (png_charp)"Software";
	title_text.text = (png_charp)"mcmap";
	png_set_text(pngStruct, pngInfo, &title_text, 1);

	png_write_info(pngStruct, pngInfo);

	const size_t tempWidth = (m_origW * CHANSPERPIXEL)+1;
	const size_t tempWidthChans = tempWidth * CHANSPERPIXEL;

	std::vector<uint8_t> lineWrite(tempWidthChans, 0);
	std::vector<uint8_t> lineRead(tempWidth, 0);

	// Prepare an array of png structs that will output simultaneously to the various tiles
	for (size_t y = 0; y < m_origH; ++y) {
		if (y % 100 == 0) {
			printProgress(size_t(y), size_t(m_origH));
		}
		// paint each image on this one
		std::fill(lineWrite.begin(), lineWrite.end(), 0);
		// the partial images are kept in this list. they're already in the correct order in which they have to me merged and blended
		for (auto it = m_partList.begin(); it != m_partList.end(); ++it) {
			ImagePart& img = *it;
			// do we have to open this image?
			if (img.y != y && img.pngPtr == nullptr) {
				continue;   // Not your turn, image!
			}

			if (img.pngPtr == nullptr) {
				assert(img.pngInfo == nullptr);
				assert(!img.file.is_open());

				img.file.open(img.filename, std::ios::in | std::ios::binary);
				if (img.file.fail()) {
					std::cerr << "Error opening temporary image " << img.filename << '\n';
					return false;
				}

				img.pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
				if (img.pngPtr == nullptr) {
					std::cerr << "Error creating read struct for temporary image " << img.filename << '\n';
					return false; // Not really cleaning up here, but program will terminate anyways, so why bother
				}
				 img.pngInfo = png_create_info_struct(img.pngPtr);
				if (img.pngInfo == nullptr || setjmp(png_jmpbuf(img.pngPtr))) {
					std::cerr << "Error reading data from temporary image " << img.filename << '\n';
					return false; // Same here
				}

				png_set_read_fn(img.pngPtr, (png_voidp)&img.file, userReadData);
				png_read_info(img.pngPtr, img.pngInfo);
				// Check if image dimensions match what is expected
				int type, interlace, comp, filter, bitDepth;
				png_uint_32 width, height;
				png_uint_32 ret = png_get_IHDR(img.pngPtr, img.pngInfo, &width, &height, &bitDepth, &type, &interlace, &comp, &filter);
				if (ret == 0 || width != (png_uint_32)img.width || height != (png_uint_32)img.height) {
					std::cerr << "Temp image " << img.filename << " has wrong dimensions; expected " << img.width << 'x' << img.height << ", got " << width << 'x' << height << '\n';
					return false;
				}

			}

			// Read next line from current image chunk
			png_read_row(img.pngPtr, (png_bytep)lineRead.data(), nullptr);
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
				img.file.close();
				img.pngPtr = nullptr;
				remove(img.filename.c_str());
			}
		}

		// Done composing this line, write to final image
		png_write_row(pngStruct, (png_bytep)lineWrite.data());

	}// Y-Loop

	png_write_end(pngStruct, nullptr);
	png_destroy_write_struct(&pngStruct, &pngInfo);
	printProgress(10, 10);
	return true;
}

void CachedPNGWriter::resize(const double scaleFac)
{
	resize(static_cast<size_t>(m_currWidth*scaleFac), static_cast<size_t>(m_currHeight*scaleFac));
}

inline uint8_t saturate(const float x)
{
	return x > 255.0f ? 255
		: x < 0.0f ? 0
		: uint8_t(x);
}

// t is a value that goes from 0 to 1 to interpolate in a C1 continuous way across uniformly sampled data points.
// when t is 0, this will return B.  When t is 1, this will return C.  Inbetween values will return an interpolation
// between B and C.  A and B are used to calculate slopes at the edges.
inline float CubicHermite(const float A, const float B, const float C, const float D, const float t)
{
	const float a = -A / 2.0f + (3.0f*B) / 2.0f - (3.0f*C) / 2.0f + D / 2.0f;
	const float b = A - (5.0f*B) / 2.0f + 2.0f*C - D / 2.0f;
	const float c = -A / 2.0f + C / 2.0f;
	const float d = B;

	return a*t*t*t + b*t*t + c*t + d;
}

//https://blog.demofox.org/2015/08/15/resizing-images-with-bicubic-interpolation/
void CachedPNGWriter::resize(const size_t newWidth, const size_t newHeight)
{
	std::cout << "Resizing image...\n";
	std::vector<uint8_t> out(newWidth * newHeight * CHANSPERPIXEL);

	for (size_t y = 0; y < newHeight; ++y) {
		if (y % 25 == 0) {
			printProgress(y, newHeight);
		}

		const float v = float(y) / float(newHeight - 1);
		for (size_t x = 0; x < newWidth; ++x) {
			uint8_t* destPixel = &out.at(x*CHANSPERPIXEL + y * (newWidth * CHANSPERPIXEL));
			const float u = float(x) / float(newWidth - 1);
			const auto sample = SampleBicubic(u, v);

			destPixel[0] = sample[0];
			destPixel[1] = sample[1];
			destPixel[2] = sample[2];
			destPixel[3] = sample[3];
		}
	}
	printProgress(10, 10);

	m_buffer = out;
	ImagePart& part = m_partList.back();
	part.width = static_cast<int>(newWidth);
	part.height = static_cast<int>(newHeight);
	part.x = static_cast<int>((newWidth / (double)m_currWidth) * part.x);
	part.y = static_cast<int>((newHeight / (double)m_currHeight) * part.y);
	m_currWidth = newWidth;
	m_currHeight = newHeight;
}

std::array<uint8_t, PNGWriter::CHANSPERPIXEL> CachedPNGWriter::SampleBicubic(const float u, const float v)
{
	// calculate coordinates -> also need to offset by half a pixel to keep image from shifting down and left half a pixel
	const float x = (u * m_currWidth) - 0.5f;
	const int xint = int(x);
	const float xfract = x - floor(x);

	const float y = (v * m_currHeight) - 0.5f;
	const int yint = int(y);
	const float yfract = y - floor(y);

	// 1st row
	auto p00 = getPixelClamped(xint - 1, yint - 1);
	auto p10 = getPixelClamped(xint + 0, yint - 1);
	auto p20 = getPixelClamped(xint + 1, yint - 1);
	auto p30 = getPixelClamped(xint + 2, yint - 1);

	// 2nd row
	auto p01 = getPixelClamped(xint - 1, yint + 0);
	auto p11 = getPixelClamped(xint + 0, yint + 0);
	auto p21 = getPixelClamped(xint + 1, yint + 0);
	auto p31 = getPixelClamped(xint + 2, yint + 0);

	// 3rd row
	auto p02 = getPixelClamped(xint - 1, yint + 1);
	auto p12 = getPixelClamped(xint + 0, yint + 1);
	auto p22 = getPixelClamped(xint + 1, yint + 1);
	auto p32 = getPixelClamped(xint + 2, yint + 1);

	// 4th row
	auto p03 = getPixelClamped(xint - 1, yint + 2);
	auto p13 = getPixelClamped(xint + 0, yint + 2);
	auto p23 = getPixelClamped(xint + 1, yint + 2);
	auto p33 = getPixelClamped(xint + 2, yint + 2);

	// interpolate bi-cubically!
	// Clamp the values since the curve can put the value below 0 or above 255
	std::array<uint8_t, PNGWriter::CHANSPERPIXEL> ret;
	for (int i = 0; i < PNGWriter::CHANSPERPIXEL; ++i)
	{
		const float col0 = CubicHermite(p00[i], p10[i], p20[i], p30[i], xfract);
		const float col1 = CubicHermite(p01[i], p11[i], p21[i], p31[i], xfract);
		const float col2 = CubicHermite(p02[i], p12[i], p22[i], p32[i], xfract);
		const float col3 = CubicHermite(p03[i], p13[i], p23[i], p33[i], xfract);
		const float value = CubicHermite(col0, col1, col2, col3, yfract);
		ret[i] = saturate(value);
	}

	return ret;
}