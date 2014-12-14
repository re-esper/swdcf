#include "server.h"

#include <stdio.h>
#include <stdlib.h>

#include <json/json.h>
#include <fstream>
#include <map>
#include <vector>

extern std::map<std::string, ServerPacket> s_packets;
extern std::map<std::string, ClientPacket> c_packets;
std::map<int, std::vector<unsigned char> > boss_packets;
struct random_battle_info {
	random_battle_info() {}
	random_battle_info(const std::vector<unsigned char>& fd, int p, int v, int m) : power(p), variance(v), maxcnt(m) {
		memcpy(field, &(fd[0]), 8);
	}
	unsigned char field[8];
	int power;
	int variance;
	int maxcnt;
};
std::map<int, random_battle_info> random_battle_datas;
int lastidd = 0x14;
std::vector<unsigned char> troop_data[6];

#define SKIP_00(n, var, cntr)  int  var##cntr[n];
#define SKIP_0(n, cntr)  SKIP_00(n, _skip, cntr)
#define SKIP(n)  SKIP_0(n, __COUNTER__)
struct UNITDATA {
	char	name[32];
	int		race;
	int		level;
	SKIP(1)
	int		id;
	int		idx;
	int		hp;
	SKIP(3)
	int		mp;
	SKIP(1)
	int		ep;
	SKIP(2)
	int		atk;
	int		def;
	int		matk;
	int		mdef;
	int		spd;
	int		hit;
	int		eva;
	int		cri;
	int		unk5;
	SKIP(3)
	int		x[7];
	int		unk6;
	SKIP(42)
	int		unk2;
	int		unk1;
	int		unk3;
	SKIP(3)
	int		unk4;
	SKIP(20)
	int		unk7;	
};
UNITDATA* __units = NULL;
int __unit_cnt = 0; // 185

void LoadUnitData()
{
	FILE *f = fopen("unit.dat", "rb");
	fseek(f, 0, SEEK_END);
	int flen = ftell(f);
	__unit_cnt = flen / sizeof(UNITDATA);	
	__units = new UNITDATA[__unit_cnt];
	fseek(f, 0, SEEK_SET);
	fread(__units, sizeof(UNITDATA), __unit_cnt, f);
	fclose(f);
	qsort(__units, __unit_cnt, sizeof(UNITDATA), 
		[](const void* a, const void* b)->int{ return ((UNITDATA*)a)->level - ((UNITDATA*)b)->level; });
	printf("%d units loaded.\r\n", __unit_cnt);
}

void readarray(const char* p, std::vector<unsigned char>& out)
{
#define H2I(c) (c > '9' ? c - 'A' + 10 : c - '0')
	int size = ceil(strlen(p) / 3.0);
	out.resize(size);
	const char* pp = p;
	for (int i = 0; i < size; ++i) {
		out[i] = H2I(*pp) * 0x10; pp++;
		out[i] += H2I(*pp); pp++; pp++;
	}
}
extern void LoadWarField();
bool LoadBattleDef()
{
    std::ifstream ifs;
	Json::Reader reader;
    Json::Value root;
    ifs.open("battle.def");
    if (!reader.parse(ifs, root, false)) return false;

	// troop data
	int tsize = root["troop_data"].size();
	for (int i = 0; i < tsize; ++i) {
		readarray(root["troop_data"][i].asCString(), troop_data[i]);
	}
	// boss battle
	const Json::Value& bbr = root["boss_battle"];
	const Json::Value::Members& pns = bbr.getMemberNames();
	for (auto name : pns) {
		std::vector<unsigned char> data;
		readarray(bbr[name].asCString(), data);			
		int id = strtoul(name.c_str(), NULL, 16);
		boss_packets[id] = data;
	}
	// random battle
	const Json::Value& rbr = root["random_battle"];
	const Json::Value::Members& pns2 = rbr.getMemberNames();
	for (auto name : pns2) {
		std::vector<unsigned char> fd;
		readarray(rbr[name]["field"].asCString(), fd);
		int maxcnt = rbr[name].isMember("max") ? rbr[name]["max"].asInt() : 6;		
		random_battle_info rbi(fd, rbr[name]["power"].asInt(), rbr[name]["variance"].asInt(), maxcnt);
		int id = strtoul(name.c_str(), NULL, 16);
		random_battle_datas[id] = rbi;
	}	
	return true;
}

/// battle packet utils
void clearunits(ServerPacket& sp)
{
	memset(sp.packet + sp.header_size + 12, 0, 108 * 6);
}
void fillunit(int uidx, ServerPacket& sp, int pos)
{
#define FILL(name, p) sp.fill((std::string(name) + suffix).c_str(), (void*)&(p))
#define FILLP(name, p) sp.fill((std::string(name) + suffix).c_str(), (void*)p)

	const UNITDATA& ud = __units[uidx];
	char suffix[] = "_0";
	suffix[1] += pos;

	FILLP("name", ud.name);
	FILL("id", ud.id);
	FILL("idx", ud.idx);
	FILL("lvl", ud.level);
	FILL("race", ud.race);	
	FILL("hp", ud.hp);
	FILL("hpx", ud.hp);
	FILL("mp", ud.mp);
	FILL("mpx", ud.mp);
	FILL("ep", ud.ep);
	FILL("epx", ud.ep);
	FILL("atk", ud.atk);
	FILL("def", ud.def);
	FILL("matk", ud.matk);
	FILL("mdef", ud.mdef);
	FILL("spd", ud.spd);
	FILL("hit", ud.hit);
	FILL("eva", ud.eva);
	FILL("cri", ud.cri);
	FILL("unk1", ud.unk1);
	FILL("unk2", ud.unk2);
	FILL("unk3", ud.unk3);
	FILL("unk4", ud.unk4);
	FILL("unk5", ud.unk5);
	
	unsigned char x[7] = {0};
	for (int i = 0; i < 7; ++i) x[i] = (unsigned char)ud.x[i];
	FILLP("x", x);

	FILL("unk6", ud.unk6);
	FILL("unk7", ud.unk7);
}


/// random battle
int randint(int l, int h) { return l + int(rand() / double(RAND_MAX + 1) * (h - l + 1)); }
int pickunit(int level)
{	
	// get unit range of the level
	int startidx = -1, endidx = -1;
	for (int i = 0; i < __unit_cnt; ++i) {
		if (__units[i].level == level) {
			if (startidx == -1) {
				startidx = i;
				continue;
			}
		}
		else if (__units[i].level > level) {
			if (startidx == -1) return -1; // not found
			endidx = i;
			break;
		}		
	}
	if (startidx == -1) return -1;
	return randint(startidx, endidx);
}
int pickunitbylevel(int l)
{
	int level = l > 0 ? l : 1;
	int idx = pickunit(level);
	if (idx > 0) return idx;
	int offset = 1;
	while (1) {
		idx = pickunit(level + offset);
		if (idx > 0) return idx;
		idx = pickunit(level - offset);
		if (idx > 0) return idx;
		offset++;
	}
}
struct {
	int unit_cnt;
	int unit_type[6];
	double weight;
} __battle_patterns[] = {
	{ 1, { 1, 0, 0, 0, 0, 0 }, 1 },
	{ 2, { 1, 1, 0, 0, 0, 0 }, 0.5 },
	{ 2, { 1, 2, 0, 0, 0, 0 }, 0.5 },
	{ 3, { 1, 1, 1, 0, 0, 0 }, 0.35 },
	{ 3, { 1, 2, 2, 0, 0, 0 }, 0.35 },
	{ 3, { 1, 2, 3, 0, 0, 0 }, 0.35 },
	{ 4, { 1, 1, 1, 1, 0, 0 }, 0.35 },
	{ 4, { 1, 1, 2, 2, 0, 0 }, 0.35 },
	{ 4, { 1, 2, 3, 4, 0, 0 }, 0.35 },
	{ 5, { 1, 1, 1, 1, 1, 0 }, 0.35 },
	{ 5, { 1, 1, 1, 1, 2, 0 }, 0.35 },
	{ 5, { 1, 1, 2, 2, 3, 0 }, 0.35 },
	{ 6, { 1, 1, 1, 1, 2, 2 }, 0.5 },
	{ 6, { 1, 1, 2, 2, 3, 3 }, 0.5 },
};
int fillbattleunit(ServerPacket& sp, int power, int variance, int maxcnt)
{
	int pattern_cnt = sizeof(__battle_patterns) / sizeof(__battle_patterns[0]);
	double total_weight = 0;
	for (int i = 0; i < pattern_cnt; ++i) {
		if (__battle_patterns[i].unit_cnt > maxcnt) break;
		total_weight += __battle_patterns[i].weight;
	}

	double ran = rand() * total_weight / RAND_MAX;	
	for (int i = 0; i < pattern_cnt; ++i) {
		ran -= __battle_patterns[i].weight;
		if (ran < 0) {			
			int utype[6] = { 0 };
			for (int j = 0; j < 6; ++j) {
				int ord = __battle_patterns[i].unit_type[j];
				if (ord == 0) break;
				if (utype[ord] == 0) {
					int lvlmod = 0;
					if (__battle_patterns[i].unit_cnt == 1 && maxcnt == 6) lvlmod = 3;
					else if (__battle_patterns[i].unit_cnt == 6) lvlmod = -1;
					utype[ord] = pickunitbylevel(randint(power - variance, power + variance) + lvlmod);
				}
				fillunit(utype[ord], sp, j + 1);
			}
			return __battle_patterns[i].unit_cnt;
		}
	}
	return -1;
}





#define B_HDR "\x1B\x03\x34\x12\x23\xAC\x3C\x1A\x22\x34\x1A\x03\x00\x00\x01\x00\x00\x00\x06\x03\x01\x00"
void HandleBattlePacket(const char* in_packet)
{
	ClientPacket& cp = c_packets["battle_event"];
	cp.parse(in_packet);
	int id2 = 0;
	cp.get("id2", &id2);
	auto itr = boss_packets.find(id2);
	if (itr != boss_packets.end()) {
		CFSendPacket(B_HDR, &(itr->second[0]));
	}
	else {
		int id3 = 0;
		cp.get("id3", &id3);
		ServerPacket& sp = s_packets["battle"];
		clearunits(sp);

		auto bditr = random_battle_datas.find(id3);		
		if (bditr == random_battle_datas.end()) {
			bditr = random_battle_datas.find(lastidd);
		}
		else {
			lastidd = id3;
		}
		const random_battle_info& rbi = bditr->second;
		sp.fill("field", (void*)&(rbi.field[0]));		
		int ucnt = fillbattleunit(sp, rbi.power, rbi.variance, rbi.maxcnt);
		if (ucnt > 0) {
			sp.fill("troopdata", &(troop_data[ucnt-1][0]));
		}
		CFSendPacket(sp);
	}
}