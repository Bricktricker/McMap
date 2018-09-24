#pragma once

#include "TiledPNGWriter.h"

class TiledSplitPNGWriter : public TiledPNGWriter
{
public:
	bool compose(const std::string& path) override;
};

