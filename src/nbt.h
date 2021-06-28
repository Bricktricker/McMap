#pragma once
#include <map>
#include <string>
#include <optional>
#include <vector>
#include <variant>
#include <string_view>

enum TagType
{
	tagUnknown = 0, // Tag_End is not made available, so 0 should never be in any list of available elements
	tagByte = 1,
	tagShort = 2,
	tagInt = 3,
	tagLong = 4,
	tagFloat = 5,
	tagDouble = 6,
	tagByteArray = 7,
	tagString = 8,
	tagList = 9,
	tagCompound = 10,
	tagIntArray = 11,
	tagLongArray = 12
};

//TODO: replace with std::span in C++20
template<typename T>
struct PrimArray
{
	PrimArray()
		: m_data(nullptr), m_len(0)
	{}

	PrimArray(T const * data, const size_t len)
		: m_data(data), m_len(len)
	{}

	T const * m_data;
	size_t m_len; //len in number of T's in _data;
};

class NBTtag
{
	friend class NBT;
public:
	using NBTlist = std::vector<NBTtag>;
	using TagMap = std::map<std::string, NBTtag, std::less<>>;

	using DataHolder = std::variant<
		TagMap,
		NBTlist,
		std::string,
		std::vector<uint8_t>,
		std::vector<int32_t>,
		std::vector<int64_t>,
		int8_t,
		int16_t,
		int32_t,
		int64_t,
		float,
		double>;

	DataHolder m_dataHolder;
	TagType m_type;
	std::string m_name;

	explicit NBTtag();
	explicit NBTtag(const std::vector<uint8_t>& data, size_t& pos);
	explicit NBTtag(const std::vector<uint8_t>& data, size_t& pos, TagType type); //Construct NBTtag from list

private:
	bool parseData(const std::vector<uint8_t>& data, size_t& pos, bool parseHeader = true);

	template<typename T>
	std::optional<T> getValue(const std::string_view name, const TagType type) const
	{
		if (m_type == tagCompound) {
			const auto& compound = std::get<TagMap>(m_dataHolder);
			const auto posVal = compound.find(name);
			if (posVal != compound.end()) {
				if (posVal->second.getType() == type) {
					return std::get<T>(posVal->second.m_dataHolder);
				}
			}
		}
		return std::nullopt;
	}

	template<typename T>
	std::optional<PrimArray<T>> getArray(const std::string_view name, const TagType type) const
	{
		if (m_type == tagCompound) {
			const auto& compound = std::get<TagMap>(m_dataHolder);
			const auto posVal = compound.find(name);
			if (posVal != compound.end()) {
				if (posVal->second.getType() == type) {
					const std::vector<T>& vec = std::get<std::vector<T>>(posVal->second.m_dataHolder);
					return PrimArray(vec.data(), vec.size());
				}
			}
		}
		return std::nullopt;
	}

public:
	void printTags() const;

	std::optional<const NBTtag*> getCompound(const std::string_view name) const;

	std::optional<const NBTlist*> getList(const std::string_view name) const;

	std::optional<int8_t> getByte(const std::string_view name) const
	{
		return getValue<int8_t>(name, tagByte);
	}

	std::optional<int16_t> getShort(const std::string_view name) const
	{
		return getValue<int16_t>(name, tagShort);
	}

	std::optional<int32_t> getInt(const std::string_view name) const
	{
		return getValue<int32_t>(name, tagInt);
	}

	std::optional<int64_t> getLong(const std::string_view name) const
	{
		return getValue<int64_t>(name, tagLong);
	}

	std::optional<PrimArray<uint8_t>> getByteArray(const std::string_view name) const
	{
		return getArray<uint8_t>(name, tagByteArray);
	}

	std::optional<PrimArray<int32_t>> getIntArray(const std::string_view name) const
	{
		return getArray<int32_t>(name, tagIntArray);
	}

	std::optional<PrimArray<int64_t>> getLongArray(const std::string_view name) const
	{
		return getArray<int64_t>(name, tagLongArray);
	}

	std::optional<std::string_view> getString(const std::string_view name) const;

	TagType getType() const noexcept { return m_type; }
	std::string getName() const noexcept { return m_name; };

	virtual ~NBTtag() = default;
};

class NBT : public NBTtag
{
private:
	bool m_good;
public:
	explicit NBT(const std::vector<uint8_t>& _data);

	NBT(const NBT&) = delete;
	NBT& operator=(const NBT&) = delete;
	virtual ~NBT() = default;

	bool good() const noexcept { return m_good; }
};
