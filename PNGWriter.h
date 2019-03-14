#pragma once
#include <string>
#include <vector>
#include <array>
#include "defines.h"

namespace image {
	class PNGWriter
	{
	public:
		PNGWriter();

		virtual bool reserve(const size_t width, const size_t height);
		virtual bool write(const std::string& path);
		virtual Channel* getPixel(const size_t x, const size_t y);
		virtual ~PNGWriter() = default;

		virtual void resize(const double scaleFac);
		virtual void resize(const size_t newWidth, const size_t newHeight);

		virtual size_t getWidth() const noexcept { return m_width; };
		virtual size_t getHeight() const noexcept { return m_height; };

		// Separate them in case I ever implement 16bit rendering
		static constexpr size_t CHANSPERPIXEL{ 4 };
		static constexpr size_t BYTESPERCHAN{ 1 }; //Not used
		static constexpr size_t BYTESPERPIXEL{ 4 };

	protected:
		Channel* getPixelClamped(int x, int y);
		std::array<Channel, CHANSPERPIXEL> SampleBicubic(const float u, const float v);

		std::vector<Channel> m_buffer; //Change to Pixel
		size_t m_width;
		size_t m_height;
	};
}