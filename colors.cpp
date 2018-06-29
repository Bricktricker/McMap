#include "colors.h"
#include "extractcolors.h"
#include "pngreader.h"
#include "globals.h"
#include <fstream>
#include <iostream>
#include "json.hpp"

using nlohmann::json;

std::map<std::string, Tree<std::string, uint16_t>> blockTree; //Maps blockState to id
RangeMap<uint16_t, Color_t> colorMap; //maps id to color_t

void buildTree(std::vector<std::string>& strVec, const json jState, Tree<std::string, uint16_t>& tree) {
	for (auto itr = jState.begin(); itr != jState.end(); ++itr) {
		if (itr->is_primitive()) {
			uint16_t val = itr.value();
			tree.add(strVec, itr.key(), val);
		}
		else {
			strVec.push_back(itr.key());
			buildTree(strVec, *itr, tree);
			strVec.pop_back();
		}
	}
}

void loadBlockTree(const std::string& path)
{
	json jData;
	try {
		std::ifstream i(path); //res/compressBlock.json
		i >> jData;
		i.close();
	}
	catch (nlohmann::json::parse_error e) {
		std::cerr << e.what() << std::endl;
		return;
	}

	for (auto block = jData.begin(); block != jData.end(); ++block) {
		const std::string name = block.key();
		Tree<std::string, uint16_t> tree;
		const json jOrder = (*block)["order"];
		if (!jOrder.empty()) {
			tree.setOrder(jOrder);

			json jStates = (*block)["states"];
			std::vector<std::string> vec;
			buildTree(vec, jStates, tree);
		}
		else {
			tree.add((*block)["states"][""]);
		}

		blockTree[name] = tree;
	}
}

void loadColorMap(const std::string& path)
{
	json jData;
	try {
		std::ifstream i(path); //res/compressBlock.json
		i >> jData;
		i.close();
	}
	catch (nlohmann::json::parse_error e) {
		std::cerr << e.what() << std::endl;
		return;
	}

	for (const auto& col : jData) {
		uint16_t from = col["from"];
		uint16_t to = col["to"];
		const json color = col["color"];
		uint8_t r, g, b, a, noise, blockType;
		r = color["r"];
		g = color["g"];
		b = color["b"];
		a = color["a"];
		noise = color["n"];
		blockType = col["blockType"];
		uint8_t brightness = (uint8_t)sqrt(double(r) *  double(r) * .236 + double(g) *  double(g) * .601 + double(b) * double(b) * .163);

		Color_t finalCol{ r, g, b, a, noise, brightness, blockType };
		colorMap.insert(from, to, finalCol);
	}

	colorMap.balance();
}
