//C++ Header
#include <fstream>
#include <iostream>
#include <map>
//My-Header
#include "colors.h"
#include "pngreader.h"
#include "globals.h"
#include "json.hpp"

using nlohmann::json;

std::map<std::string, Tree<std::string, uint16_t>> blockTree; //Maps blockState to id
RangeMap<uint16_t, Color_t> colorMap; //maps id to color_t
//This beast maps old blockid:meta to new states to save them in terrain array
const std::map<uint16_t, uint16_t> metaToState{ { 0U,0U },{ 1U,1U },{ 4097U,2U },{ 8193U,3U },{ 12289U,4U },{ 16385U,5U },{ 20481U,6U },{ 24577U,7U },{ 2U,9U },{ 3U,10U },{ 4099U,11U },{ 8195U,13U },{ 4U,14U },{ 5U,15U },{ 4101U,16U },{ 8197U,17U },{ 12293U,18U },{ 16389U,19U },{ 20485U,20U },{ 6U,21U },{ 4102U,23U },{ 8198U,25U },{ 12294U,27U },{ 16390U,29U },{ 20486U,31U },{ 7U,33U },{ 8U,34U },{ 9U,34U },{ 10U,50U },{ 11U,50U },{ 12U,66U },{ 4108U,67U },{ 13U,68U },{ 14U,69U },{ 15U,70U },{ 16U,71U },{ 17U,73U },{ 4113U,76U },{ 8209U,79U },{ 12305U,82U },{ 16401U,76U },{ 20497U,79U },{ 24593U,82U },{ 32785U,76U },{ 36881U,79U },{ 40977U,82U },{ 18U,157U },{ 4114U,171U },{ 8210U,185U },{ 12306U,199U },{ 16402U,157U },{ 20498U,171U },{ 24594U,185U },{ 28690U,199U },{ 19U,228U },{ 4115U,229U },{ 20U,230U },{ 21U,231U },{ 22U,232U },{ 23U,234U },{ 24U,245U },{ 4120U,246U },{ 8216U,247U },{ 25U,249U },{ 26U,975U },{ 27U,1010U },{ 28U,1022U },{ 29U,1034U },{ 30U,1040U },{ 31U,1043U },{ 4127U,1041U },{ 8223U,1042U },{ 32U,1043U },{ 33U,1053U },{ 34U,1061U },{ 35U,1083U },{ 4131U,1084U },{ 8227U,1085U },{ 12323U,1086U },{ 16419U,1087U },{ 20515U,1088U },{ 24611U,1089U },{ 28707U,1090U },{ 32803U,1091U },{ 36899U,1092U },{ 40995U,1093U },{ 45091U,1094U },{ 49187U,1095U },{ 53283U,1096U },{ 57379U,1097U },{ 61475U,1098U },{ 37U,1111U },{ 38U,1112U },{ 4134U,1113U },{ 8230U,1114U },{ 12326U,1115U },{ 16422U,1116U },{ 20518U,1117U },{ 24614U,1118U },{ 28710U,1119U },{ 32806U,1120U },{ 39U,1121U },{ 40U,1122U },{ 41U,1123U },{ 42U,1124U },{ 43U,7297U },{ 4139U,7303U },{ 8235U,7261U },{ 12331U,7315U },{ 16427U,7321U },{ 20523U,7327U },{ 24619U,7333U },{ 28715U,7339U },{ 32811U,7354U },{ 44U,7297U },{ 4140U,7303U },{ 8236U,7297U },{ 12332U,7315U },{ 16428U,7321U },{ 20524U,7327U },{ 24620U,7333U },{ 28716U,7339U },{ 45U,1125U },{ 46U,1127U },{ 47U,1128U },{ 48U,1129U },{ 49U,1130U },{ 50U,1131U },{ 51U,1167U },{ 52U,1648U },{ 53U,1660U },{ 54U,1730U },{ 55U,2913U },{ 56U,3049U },{ 57U,3050U },{ 58U,3051U },{ 59U,3052U },{ 60U,3060U },{ 61U,3069U },{ 62U,3069U },{ 63U,3077U },{ 64U,3119U },{ 65U,3173U },{ 66U,3180U },{ 67U,3201U },{ 68U,3271U },{ 69U,3287U },{ 70U,3303U },{ 71U,3315U },{ 72U,3369U },{ 73U,3381U },{ 74U,3381U },{ 75U,3382U },{ 76U,3382U },{ 77U,3401U },{ 78U,3416U },{ 79U,3424U },{ 80U,3425U },{ 81U,3426U },{ 82U,3442U },{ 83U,3443U },{ 84U,3460U },{ 85U,3492U },{ 86U,3493U },{ 87U,3494U },{ 88U,3495U },{ 89U,3496U },{ 90U,3497U },{ 91U,3503U },{ 92U,3507U },{ 93U,3517U },{ 94U,3517U },{ 95U,3578U },{ 4191U,3579U },{ 8287U,3580U },{ 12383U,3581U },{ 16479U,3582U },{ 20575U,3583U },{ 24671U,3584U },{ 28767U,3585U },{ 32863U,3586U },{ 36959U,3587U },{ 41055U,3588U },{ 45151U,3589U },{ 49247U,3590U },{ 53343U,3591U },{ 57439U,3592U },{ 61535U,3593U },{ 96U,3609U },{ 97U,3978U },{ 4193U,3979U },{ 8289U,3980U },{ 12385U,3981U },{ 16481U,3982U },{ 20577U,3983U },{ 98U,3984U },{ 4194U,3985U },{ 8290U,3986U },{ 12386U,3987U },{ 99U,3988U },{ 100U,4052U },{ 101U,4211U },{ 102U,4243U },{ 103U,4244U },{ 104U,4253U },{ 105U,4261U },{ 106U,4300U },{ 107U,4308U },{ 108U,4344U },{ 109U,4424U },{ 110U,4494U },{ 111U,4495U },{ 112U,4496U },{ 113U,4528U },{ 114U,4540U },{ 115U,4540U },{ 116U,4613U },{ 117U,4621U },{ 118U,4622U },{ 119U,4626U },{ 120U,4631U },{ 121U,4635U },{ 122U,4636U },{ 123U,4638U },{ 124U,4638U },{ 125U,7261U },{ 4221U,7267U },{ 8317U,7273U },{ 12413U,7279U },{ 16509U,7285U },{ 20605U,7291U },{ 126U,7261U },{ 4222U,7267U },{ 8318U,7273U },{ 12414U,7279U },{ 16510U,7285U },{ 20606U,7291U },{ 32894U,7261U },{ 36990U,7267U },{ 41086U,7273U },{ 45182U,7279U },{ 49278U,7285U },{ 53374U,7291U },{ 127U,4639U },{ 128U,4662U },{ 129U,4731U },{ 130U,4733U },{ 131U,4749U },{ 132U,4883U },{ 133U,4884U },{ 134U,4896U },{ 135U,4976U },{ 136U,5056U },{ 137U,5131U },{ 138U,5137U },{ 139U,5197U },{ 4235U,5261U },{ 140U,5266U },{ 141U,5288U },{ 142U,5296U },{ 143U,5313U },{ 144U,5452U },{ 145U,5568U },{ 4241U,5572U },{ 8337U,5576U },{ 146U,5581U },{ 147U,5604U },{ 148U,5620U },{ 149U,5637U },{ 150U,5637U },{ 151U,5668U },{ 152U,5684U },{ 153U,5685U },{ 154U,5686U },{ 155U,5696U },{ 4251U,5697U },{ 8347U,5699U },{ 156U,5712U },{ 157U,5787U },{ 158U,5794U },{ 159U,5805U },{ 4255U,5806U },{ 8351U,5807U },{ 12447U,5808U },{ 16543U,5809U },{ 20639U,5810U },{ 24735U,5811U },{ 28831U,5812U },{ 32927U,5813U },{ 37023U,5814U },{ 41119U,5815U },{ 45215U,5816U },{ 49311U,5817U },{ 53407U,5818U },{ 57503U,5819U },{ 61599U,5820U },{ 160U,5852U },{ 4256U,5884U },{ 8352U,5916U },{ 12448U,5948U },{ 16544U,5980U },{ 20640U,6012U },{ 24736U,6044U },{ 28832U,6076U },{ 32928U,6108U },{ 37024U,6140U },{ 41120U,6172U },{ 45216U,6204U },{ 49312U,6236U },{ 53408U,6268U },{ 57504U,6300U },{ 61600U,6332U },{ 161U,213U },{ 4257U,227U },{ 16545U,213U },{ 20641U,227U },{ 162U,85U },{ 4258U,88U },{ 16546U,85U },{ 20642U,88U },{ 32930U,85U },{ 37026U,88U },{ 163U,6344U },{ 164U,6424U },{ 165U,6493U },{ 166U,6494U },{ 167U,6510U },{ 168U,6559U },{ 4264U,6560U },{ 8360U,6561U },{ 169U,6820U },{ 170U,6822U },{ 171U,6824U },{ 4267U,6825U },{ 8363U,6826U },{ 12459U,6827U },{ 16555U,6828U },{ 20651U,6829U },{ 24747U,6830U },{ 28843U,6831U },{ 32939U,6832U },{ 37035U,6833U },{ 41131U,6834U },{ 45227U,6835U },{ 49323U,6836U },{ 53419U,6837U },{ 57515U,6838U },{ 61611U,6839U },{ 172U,6840U },{ 173U,6841U },{ 174U,6842U },{ 175U,6844U },{ 4271U,6846U },{ 8367U,6852U },{ 12463U,6854U },{ 16559U,6848U },{ 20655U,6850U },{ 176U,6855U },{ 177U,6855U },{ 178U,5668U },{ 179U,7175U },{ 4275U,7176U },{ 8371U,7177U },{ 180U,7189U },{ 181U,7345U },{ 182U,7345U },{ 183U,7365U },{ 184U,7397U },{ 185U,7429U },{ 186U,7493U },{ 187U,7461U },{ 188U,7549U },{ 189U,7581U },{ 190U,7613U },{ 191U,7677U },{ 192U,7645U },{ 193U,7689U },{ 194U,7753U },{ 195U,7817U },{ 196U,7881U },{ 197U,7945U },{ 198U,8002U },{ 199U,8067U },{ 200U,8068U },{ 201U,8074U },{ 202U,8076U },{ 203U,8089U },{ 204U,7351U },{ 205U,7351U },{ 206U,8158U },{ 207U,8159U },{ 208U,8163U },{ 209U,8164U },{ 210U,8171U },{ 211U,8183U },{ 212U,8189U },{ 213U,8193U },{ 214U,8194U },{ 215U,8195U },{ 216U,8197U },{ 217U,8199U },{ 218U,8205U },{ 219U,8222U },{ 220U,8228U },{ 221U,8234U },{ 222U,8240U },{ 223U,8246U },{ 224U,8252U },{ 225U,8258U },{ 226U,8264U },{ 227U,8270U },{ 228U,8276U },{ 229U,8282U },{ 230U,8288U },{ 231U,8294U },{ 232U,8300U },{ 233U,8306U },{ 234U,8312U },{ 235U,8314U },{ 236U,8318U },{ 237U,8322U },{ 238U,8326U },{ 239U,8330U },{ 240U,8334U },{ 241U,8338U },{ 242U,8342U },{ 243U,8346U },{ 244U,8350U },{ 245U,8354U },{ 246U,8358U },{ 247U,8362U },{ 248U,8366U },{ 249U,8370U },{ 250U,8374U },{ 251U,8378U },{ 4347U,8379U },{ 8443U,8380U },{ 12539U,8381U },{ 16635U,8382U },{ 20731U,8383U },{ 24827U,8384U },{ 28923U,8385U },{ 33019U,8386U },{ 37115U,8387U },{ 41211U,8388U },{ 45307U,8389U },{ 49403U,8390U },{ 53499U,8391U },{ 57595U,8392U },{ 61691U,8393U },{ 252U,8394U },{ 4348U,8395U },{ 8444U,8396U },{ 12540U,8397U },{ 16636U,8398U },{ 20732U,8399U },{ 24828U,8400U },{ 28924U,8401U },{ 33020U,8402U },{ 37116U,8403U },{ 41212U,8404U },{ 45308U,8405U },{ 49404U,8406U },{ 53500U,8407U },{ 57596U,8408U },{ 61692U,8409U },{ 255U,8595U } };

void buildTree(std::vector<std::string>& strVec, const json jState, Tree<std::string, uint16_t>& tree) {
	for (auto itr = jState.begin(); itr != jState.end(); ++itr) {
		if (itr->is_primitive()) {
			const uint16_t val = itr.value();
			tree.add(strVec, itr.key(), val);
		}
		else {
			strVec.push_back(itr.key());
			buildTree(strVec, *itr, tree);
			strVec.pop_back();
		}
	}
}

bool loadBlockTree(const std::string& path)
{
	json jData;
	try {
		std::ifstream i(path);
		if (i.fail()) {
			std::cerr << "Could not open " << path << '\n';
			return false;
		}

		i >> jData;
		i.close();
	}
	catch (nlohmann::json::parse_error e) {
		std::cerr << e.what() << std::endl;
		return false;
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

	return true;
}

bool loadColorMap(const std::string& path)
{
	json jData;
	try {
		std::ifstream i(path);
		if (i.fail()) {
			std::cerr << "Could not open " << path << '\n';
			return false;
		}

		i >> jData;
		i.close();
	}
	catch (nlohmann::json::parse_error e) {
		std::cerr << e.what() << std::endl;
		return false;
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
	Color_t finalCol{ 0, 0, 0, 0, 0, 0, 0 };
	colorMap.insert(0, 0, finalCol);

	colorMap.balance();

	//cachecontent: Air, Stone, Grass, Dirt, Water 
	colorMap.addToCache(0, 0);
	colorMap.addToCache(1, 1);
	colorMap.addToCache(2, 9);
	colorMap.addToCache(3, 10);
	colorMap.addToCache(4, 34);

	return true;
}
