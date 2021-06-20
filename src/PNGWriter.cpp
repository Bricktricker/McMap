//C++ Header
#include <iostream>
#include <fstream>
#include <cmath> //g++ floor
//Own Header
#include "png.h"
#include "PNGWriter.h"
#include "helper.h"

namespace
{
	//Function to write png data to disc
	void userWriteData(png_structp pngPtr, png_bytep data, png_size_t length)
	{
		png_voidp a = png_get_io_ptr(pngPtr);
		//Cast the pointer to std::ifstream* and write 'length' bytes from 'data'
		static_cast<std::fstream*>(a)->write(reinterpret_cast<char*>(data), static_cast<std::streamsize>(length));
	}
}

namespace image
{
	PNGWriter::PNGWriter()
		: m_width(0), m_height(0)
	{}

	bool PNGWriter::reserve(const size_t width, const size_t height)
	{
		const size_t pixSize = width * height * CHANSPERPIXEL;
		std::cout << "Image dimensions are " << width << 'x' << height << ", 32bpp, " << static_cast<float>(pixSize) / (1024.0f * 1024.0f) << "MiB\n";
		m_buffer.resize(pixSize, 0U);

		m_width = width;
		m_height = height;
		return true;
	}

	bool PNGWriter::write(const std::string& path)
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

		png_set_write_fn(pngStruct, static_cast<png_voidp>(&fileHandle), userWriteData, NULL);

		png_set_IHDR(pngStruct, pngInfo, static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height),
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
			for (size_t y = 0; y < m_height; ++y) {
				if (y % 25 == 0) {
					helper::printProgress(y, m_height);
				}
				png_write_row(pngStruct, &m_buffer[srcLine]);
				srcLine += m_width * CHANSPERPIXEL;
			}
			helper::printProgress(10, 10);
			png_write_end(pngStruct, NULL);
			png_destroy_write_struct(&pngStruct, &pngInfo);
		}

		m_buffer.clear();
		m_buffer.shrink_to_fit();
		m_width = 0;
		m_height = 0;

		return true;
	}

	Channel* PNGWriter::getPixel(const size_t x, const size_t y)
	{
		if (x >= m_width || y >= m_height)
			throw std::out_of_range("getPixel out of range\n");

		return &m_buffer[x*CHANSPERPIXEL + y * (m_width * CHANSPERPIXEL)];
	}

	void PNGWriter::resize(const double scaleFac)
	{
		resize(static_cast<size_t>(static_cast<double>(m_width) * scaleFac), static_cast<size_t>(static_cast<double>(m_height) * scaleFac));
	}

	//https://blog.demofox.org/2015/08/15/resizing-images-with-bicubic-interpolation/
	void PNGWriter::resize(const size_t newWidth, const size_t newHeight)
	{
		std::cout << "Resizing image...\n";
		std::vector<Channel> out(newWidth * newHeight * CHANSPERPIXEL);

		for (size_t y = 0; y < newHeight; ++y) {
			if (y % 25 == 0) {
				helper::printProgress(y, newHeight);
			}

			const float v = float(y) / float(newHeight - 1);
			for (size_t x = 0; x < newWidth; ++x) {
				Channel* destPixel = &out.at(x*CHANSPERPIXEL + y * (newWidth * CHANSPERPIXEL));
				const float u = float(x) / float(newWidth - 1);
				const auto sample = SampleBicubic(u, v);

				destPixel[0] = sample[0];
				destPixel[1] = sample[1];
				destPixel[2] = sample[2];
				destPixel[3] = sample[3];
			}
		}
		helper::printProgress(10, 10);

		m_buffer = out;
		m_width = newWidth;
		m_height = newHeight;
	}

	Channel* PNGWriter::getPixelClamped(int x, int y)
	{
		if (x < 0) x = 0;
		if (y < 0) y = 0;
		if (x > static_cast<int>(m_width - 1)) x = static_cast<int>(m_width - 1);
		if (y > static_cast<int>(m_height - 1)) y = static_cast<int>(m_height - 1);

		return &m_buffer[static_cast<size_t>(x) * CHANSPERPIXEL + static_cast<size_t>(y) * (m_width * CHANSPERPIXEL)];
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

		return a * t*t*t + b * t*t + c * t + d;
	}

	inline Channel saturate(const float x)
	{
		return x > 255.0f ? 255
			: x < 0.0f ? 0
			: Channel(x);
	}

	std::array<uint8_t, PNGWriter::CHANSPERPIXEL> PNGWriter::SampleBicubic(const float u, const float v)
	{
		// calculate coordinates -> also need to offset by half a pixel to keep image from shifting down and left half a pixel
		const float x = (u * static_cast<float>(m_width)) - 0.5f;
		const int xint = int(x);
		const float xfract = x - floorf(x);

		const float y = (v * static_cast<float>(m_height)) - 0.5f;
		const int yint = int(y);
		const float yfract = y - floorf(y);

		// 1st row
		const auto p00 = getPixelClamped(xint - 1, yint - 1);
		const auto p10 = getPixelClamped(xint + 0, yint - 1);
		const auto p20 = getPixelClamped(xint + 1, yint - 1);
		const auto p30 = getPixelClamped(xint + 2, yint - 1);

		// 2nd row
		const auto p01 = getPixelClamped(xint - 1, yint + 0);
		const auto p11 = getPixelClamped(xint + 0, yint + 0);
		const auto p21 = getPixelClamped(xint + 1, yint + 0);
		const auto p31 = getPixelClamped(xint + 2, yint + 0);

		// 3rd row
		const auto p02 = getPixelClamped(xint - 1, yint + 1);
		const auto p12 = getPixelClamped(xint + 0, yint + 1);
		const auto p22 = getPixelClamped(xint + 1, yint + 1);
		const auto p32 = getPixelClamped(xint + 2, yint + 1);

		// 4th row
		const auto p03 = getPixelClamped(xint - 1, yint + 2);
		const auto p13 = getPixelClamped(xint + 0, yint + 2);
		const auto p23 = getPixelClamped(xint + 1, yint + 2);
		const auto p33 = getPixelClamped(xint + 2, yint + 2);

		// interpolate bi-cubically!
		// Clamp the values since the curve can put the value below 0 or above 255
		std::array<Channel, PNGWriter::CHANSPERPIXEL> ret;
		for (size_t i = 0; i < PNGWriter::CHANSPERPIXEL; ++i) {
			const float col0 = CubicHermite(p00[i], p10[i], p20[i], p30[i], xfract);
			const float col1 = CubicHermite(p01[i], p11[i], p21[i], p31[i], xfract);
			const float col2 = CubicHermite(p02[i], p12[i], p22[i], p32[i], xfract);
			const float col3 = CubicHermite(p03[i], p13[i], p23[i], p33[i], xfract);
			const float value = CubicHermite(col0, col1, col2, col3, yfract);
			ret[i] = saturate(value);
		}

		return ret;
	}
}