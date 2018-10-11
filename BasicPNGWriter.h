#pragma once
#include <array>
#include <vector>
#include "PNGWriter.h"

class BasicPNGWriter : public PNGWriter
{
public:
	virtual ~BasicPNGWriter();
	bool reserve(const size_t width, const size_t height) override;
	virtual bool write(const std::string& path) override;
	uint8_t* getPixel(const size_t x, const size_t y) override;
	uint8_t* getPixelClamped(int x, int y);

	void resize(const double scaleFac);
	void resize(const size_t newWidth, const size_t newHeight);
protected:
	std::vector<uint8_t> m_buffer;
	size_t m_width;
	size_t m_height;
private:
	std::array<uint8_t, CHANSPERPIXEL> SampleBicubic(const float u, const float v);
};

