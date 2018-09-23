#pragma once
#include <string>

class PNGWriter
{
public:
	virtual bool open(const size_t width, const size_t height) = 0;
	virtual bool write(const std::string& path) = 0;
	virtual uint8_t* getPixel(const size_t x, const size_t y) = 0;
	virtual ~PNGWriter() = default;
protected:
	const size_t CHANSPERPIXEL{ 4 };
};
