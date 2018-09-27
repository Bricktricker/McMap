#pragma once

#include "CachedPNGWriter.h"

class CachedTiledPNGWriter : public CachedPNGWriter
{
public:
	CachedTiledPNGWriter(const size_t origW, const size_t origH);
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

