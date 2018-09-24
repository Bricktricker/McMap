#pragma once
#include "BasicPNGWriter.h"

class BasicSplitPNGWriter : public BasicPNGWriter
{
public:
	bool write(const std::string& path) override;
private:

};

