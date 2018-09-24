#pragma once

#include "PNGWriter.h"
#include <vector>

class TiledPNGWriter : public PNGWriter
{
public:
	TiledPNGWriter(const size_t origW, const size_t origH);
	virtual ~TiledPNGWriter();
	bool open(const size_t width, const size_t height) override; //creats imageBuffer
	bool addPart(const int startx, const int starty, const int width, const int height);
	virtual bool write(const std::string& path) override;
	uint8_t* getPixel(const size_t x, const size_t y) override;
	bool compose(const std::string& path );
private:
	struct ImagePart {
		std::string filename;
		int x, y, width, height;
	};

	struct ImageTile {
		std::fstream fileHandle;
		png_structp pngPtr;
		png_infop pngInfo;

		ImageTile()
			: pngPtr(nullptr), pngInfo(nullptr) {}

	};

	size_t m_currWidth;
	size_t m_currHeight;
	size_t m_origW;
	size_t m_origH;
	int offsetX;
	int offsetY;

	std::vector<ImagePart> m_partList;
	std::vector<uint8_t> m_buffer;
};

