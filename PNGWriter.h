#pragma once
#include <string>

class PNGWriter
{
public:
	virtual bool reserve(const size_t width, const size_t height) = 0;
	virtual bool write(const std::string& path) = 0;
	virtual uint8_t* getPixel(const size_t x, const size_t y) = 0;
	virtual ~PNGWriter() = default;

	// Separate them in case I ever implement 16bit rendering
	static const size_t CHANSPERPIXEL{ 4 };
	static const size_t BYTESPERCHAN{ 1 }; //Not used
	static const size_t BYTESPERPIXEL{ 4 };
};
