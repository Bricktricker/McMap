#pragma once
#include <map>
#include <vector>
#include "defines.h"

class WorldStorage
{
public:

	WorldStorage();
	~WorldStorage() = default;

	void clear();
	void clearLightMap();

	void setBlock(const int x, const int y, const int z, const StateID_t blockType);
	// returns the blockID at the give block coordinate, or AIR if the block position is not stored
	StateID_t getBlock(const int x, const int y, const int z) const;

	void setLight(const int x, const int y, const int z, const uint8_t value);
	uint8_t getLight(const int x, const int y, const int z) const;

	void setHeight(const int x, const int z, const int16_t low, const int16_t hight);
	// returns the backed (hight << 16) | low value for the given block coordinate, or 0xff00 if the block position is not stored
	uint32_t getHeightPacked(const int x, const int z) const;

	int maxX() const noexcept
	{
		return m_maxX;
	}

	int minX() const noexcept
	{
		return m_minX;
	}

	int maxY() const noexcept
	{
		return m_maxY;
	}

	int minY() const noexcept
	{
		return m_minY;
	}

	int maxZ() const noexcept
	{
		return m_maxZ;
	}

	int minZ() const noexcept
	{
		return m_minZ;
	}

private:

	static uint64_t packBlockPos(const int x, const int z);

	struct SectionStorage
	{
		SectionStorage(const int8_t _sectionY);
		~SectionStorage() = default;

		int8_t sectionY;
		std::vector<StateID_t> terrain;
		std::vector<uint8_t> light;
	};

	struct ChunkStorage
	{
		ChunkStorage();
		~ChunkStorage() = default;

		std::vector<SectionStorage> sections;
		std::vector<uint32_t> heightMap;

		// gets the requested section, creates one if the requested section is not present
		SectionStorage& getSection(const int y);
	};

	std::map<uint64_t, ChunkStorage> m_chunkStorage;
	int m_minX, m_maxX, m_minY, m_maxY, m_minZ, m_maxZ;
};

