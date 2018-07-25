#ifndef _NBT_H_
#define _NBT_H_
#include <map>
#include <list>
#include <string>
#include "helper.h"

using std::string;

class NBT_Tag;
typedef std::map<std::string, NBT_Tag*> tagmap;
typedef std::list<NBT_Tag*> NBTlist;

enum TagType {
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

template<typename T>
struct PrimArray {
	T const * _data;
	size_t _len; //len in number of T's in _data;
};

class NBT_Tag
{
	friend class NBT;
private:

	union PrimDataType
	{
		int8_t _byte;
		int16_t _short;
		int32_t _int;
		int64_t _long;
		float _float;
		double _double;
	};

	union DataHolder
	{
		DataHolder() {};
		~DataHolder() {};

		tagmap* _compound;
		std::list<NBT_Tag*>* _list;
		PrimDataType _primDataType;
		PrimArray<char> _string;
		PrimArray<uint8_t> _byteArray;
		PrimArray<int32_t> _intArray;
		PrimArray<int64_t> _longArray;
	};

	DataHolder dataHolder;
	TagType _type;
	std::string _name;

	/*
	tagmap *_elems;
	list<NBT_Tag *> *_list;
	uint8_t *_data;
	uint32_t _len;
	*/

	//explicit NBT_Tag(uint8_t* &position, const uint8_t *end, string &name);
	//explicit NBT_Tag(uint8_t* &position, const uint8_t *end, TagType type);
	explicit NBT_Tag();
	explicit NBT_Tag(const std::vector<uint8_t>& data, size_t& pos);
	explicit NBT_Tag(const std::vector<uint8_t>& data, size_t& pos, TagType type); //Construct NBT_Tag from list
	//void parseData(uint8_t* &position, const uint8_t *end, string *name = NULL);
	bool parseData(const std::vector<uint8_t>& data, size_t& pos, bool parseHeader = true);

public:
	void printTags() const;
	bool getCompound(const string& name, NBT_Tag* &compound);
	bool getList(const string& name, std::list<NBT_Tag*>& lst);
	bool getByte(const string& name, int8_t& value);
	bool getShort(const string& name, int16_t& value);
	bool getInt(const string& name, int32_t& value);
	bool getLong(const string& name, int64_t& value);
	bool getByteArray(const string& name, PrimArray<uint8_t>& data);
	bool getIntArray(const string& name, PrimArray<int32_t>& data);
	bool getLongArray(const string& name, PrimArray<int64_t>& data);
	bool getString(const string& name, std::string& data);
	TagType getType() const noexcept { return _type; }
	std::string getName() const noexcept { return _name; };

	virtual ~NBT_Tag();
};

class NBT : public NBT_Tag
{
private:
	const std::vector<uint8_t>& data;
	bool _good;
public:
	explicit NBT(const std::vector<uint8_t>& _data);
	NBT(const NBT&) = delete;
	NBT& operator=(const NBT&) = delete;
	virtual ~NBT() = default;

	bool good() const noexcept { return _good; }

};

#endif