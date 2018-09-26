#pragma once

#include "TiledPNGWriter.h"

class TiledSplitPNGWriter : public TiledPNGWriter
{
public:
	TiledSplitPNGWriter(const size_t origW, const size_t origH);
	bool compose(const std::string& path) override;
private:

	struct ImageTile {
		std::fstream fileHandle;
		png_structp pngPtr;
		png_infop pngInfo;

		ImageTile()
			: pngPtr(nullptr), pngInfo(nullptr) {}

	};

};

