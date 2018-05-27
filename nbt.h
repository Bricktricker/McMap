#ifndef _NBT_H_
#define _NBT_H_
#include <cstring>
#include <map>
#include <list>
#include <string>
#include "helper.h"

using std::string;
using std::map;
using std::list;

class NBT_Tag;
typedef map<string, NBT_Tag *> tagmap;

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
	tagIntArray = 11
};

class NBT_Tag
{
	friend class NBT;
private:
	tagmap *_elems;
	list<NBT_Tag *> *_list;
	TagType _type;
	uint8_t *_data;
	uint32_t _len;

	explicit NBT_Tag(uint8_t* &position, const uint8_t *end, string &name);
	explicit NBT_Tag(uint8_t* &position, const uint8_t *end, TagType type);
	explicit NBT_Tag();
	void parseData(uint8_t* &position, const uint8_t *end, string *name = NULL);

public:
	void printTags();
	bool getCompound(const string name, NBT_Tag* &compound);
	bool getList(const string name, list<NBT_Tag *>* &lst);
	bool getByte(const string name, int8_t &value);
	bool getShort(const string name, int16_t &value);
	bool getInt(const string name, int32_t &value);
	bool getLong(const string name, int64_t &value);
	bool getByteArray(const string name, uint8_t* &data, int &len);
	TagType getType() {
		return _type;
	}
	virtual ~NBT_Tag();
};

class NBT : public NBT_Tag
{
private:
	uint8_t *_blob;
	char *_filename;
	size_t _bloblen;
public:
	explicit NBT(const char *file, bool &success);
	explicit NBT(uint8_t * const file, const size_t len, const bool shared, bool &success);
	explicit NBT();
	virtual ~NBT();
	//bool save();
};

#endif