#pragma once

#include <vector>
#include "PNGWriter.h"

class BasicPNGWriter : public PNGWriter
{
public:
	~BasicPNGWriter();
	bool open(const size_t width, const size_t height);
	virtual bool write(const std::string& path);
	uint8_t* getPixel(const size_t x, const size_t y);
private:
	std::vector<uint8_t> m_buffer;
	size_t m_width;
	size_t m_height;
};

