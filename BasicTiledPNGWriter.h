#pragma once
//C++ Header

//My-Header
#include "png.h"
#include "PNGWriter.h"

class BasicTiledPNGWriter : public PNGWriter
{
public:
	bool write(const std::string& path) override;
private:

	struct ImageTile {
		std::fstream fileHandle;
		png_structp pngPtr;
		png_infop pngInfo;

		ImageTile()
			: pngPtr(nullptr), pngInfo(nullptr) {}

	};

};

