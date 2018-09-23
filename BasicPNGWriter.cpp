//C++ Header
#include <iostream>
#include <fstream>
//My-Header
#include <png.h>
#include "BasicPNGWriter.h"
#include "helper.h"

//Function to write png data to disc
void userWriteData(png_structp pngPtr, png_bytep data, png_size_t length)
{
	//Our std::ostream pointer.
	png_voidp a = png_get_io_ptr(pngPtr);
	//Cast the pointer to std::ifstream* and read 'length' bytes into 'data'
	((std::fstream*)a)->write((char*)data, length);
}

BasicPNGWriter::~BasicPNGWriter()
{
	if (m_width != 0 || m_height != 0 || m_buffer.size() != 0)
		std::cerr << "BasicPNGWriter closes without writing image\n";
}

bool BasicPNGWriter::open(const size_t width, const size_t height)
{
	const size_t pixSize = width * height * CHANSPERPIXEL;
	std::cout << "Image dimensions are " << width << 'x' << height << ", 32bpp, " << float(pixSize / float(1024 * 1024)) << "MiB\n";
	m_buffer.resize(pixSize, 0U);

	m_width = width;
	m_height = height;
	return true;
}

bool BasicPNGWriter::write(const std::string& path)
{
	//open file
	std::fstream fileHandle(path, std::fstream::out | std::fstream::binary);
	if (fileHandle.fail()) {
		std::cerr << "Error opening '" << path << "' for writing.\n";
		return false;
	}

	png_structp pngStruct = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (pngStruct == nullptr)
		return false;

	png_infop pngInfo = png_create_info_struct(pngStruct);
	if (pngInfo == nullptr) {
		png_destroy_write_struct(&pngStruct, NULL);
		return false;
	}

	if (setjmp(png_jmpbuf(pngStruct))) { // libpng will issue a longjmp on error, so code flow will end up
		png_destroy_write_struct(&pngStruct, &pngInfo); // here if something goes wrong in the code below
		std::cerr << "Something went wrong with pngLib\n";
		return false;
	}

	png_set_write_fn(pngStruct, (png_voidp)&fileHandle, userWriteData, NULL);

	png_set_IHDR(pngStruct, pngInfo, (uint32_t)m_width, (uint32_t)m_height,
		8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_text title_text;
	title_text.compression = PNG_TEXT_COMPRESSION_NONE;
	title_text.key = (png_charp)"Software";
	title_text.text = (png_charp)"mcmap";
	png_set_text(pngStruct, pngInfo, &title_text, 1);

	png_write_info(pngStruct, pngInfo);

	std::cout << "Writing to file...\n";
	//saving actual image
	{
		size_t srcLine = 0;
		for (int y = 0; y < m_height; ++y) {
			if (y % 25 == 0) {
				printProgress(size_t(y), size_t(m_height));
			}
			png_write_row(pngStruct, (png_bytep)&m_buffer[srcLine]);
			srcLine += m_width * CHANSPERPIXEL;
		}
		printProgress(10, 10);
		png_write_end(pngStruct, NULL);
		png_destroy_write_struct(&pngStruct, &pngInfo);
	}

	m_buffer.clear();
	m_buffer.shrink_to_fit();
	m_width = 0;
	m_height = 0;

	return true;
}

uint8_t* BasicPNGWriter::getPixel(const size_t x, const size_t y)
{
	if (x >= m_width || y >= m_height || x < 0 || y < 0)
		throw std::out_of_range("getPixel out of range\n");

	return &m_buffer.at(x*CHANSPERPIXEL + y * (m_width * CHANSPERPIXEL)); //check index calculation
}
