#pragma once
#include <string>
#include <vector>
#include <array>

class PNGWriter
{
public:
	PNGWriter();

	virtual bool reserve(const size_t width, const size_t height);
	virtual bool write(const std::string& path);
	virtual uint8_t* getPixel(const size_t x, const size_t y);
	virtual ~PNGWriter() = default;

	virtual void resize(const double scaleFac);
	virtual void resize(const size_t newWidth, const size_t newHeight);

	// Separate them in case I ever implement 16bit rendering
	static const size_t CHANSPERPIXEL{ 4 };
	static const size_t BYTESPERCHAN{ 1 }; //Not used
	static const size_t BYTESPERPIXEL{ 4 };

protected:
	uint8_t* getPixelClamped(int x, int y);
	std::array<uint8_t, CHANSPERPIXEL> SampleBicubic(const float u, const float v);

	std::vector<uint8_t> m_buffer;
	size_t m_width;
	size_t m_height;
};
