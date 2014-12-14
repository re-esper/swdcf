#include "server.h"

extern std::map<std::string, ServerPacket> s_packets;
extern std::map<std::string, ClientPacket> c_packets;
struct {
	BYTE fabaodata[108];
	BYTE monsterdata[1300];
} __userdata = {0};
void LoadUserData()
{
	FILE *f = fopen("user.dat", "rb");	
	fread(&__userdata, 1, sizeof(__userdata), f);
	fclose(f);
}
void SaveUserData()
{
	FILE *f = fopen("user.dat", "wb");	
	fwrite(&__userdata, 1, sizeof(__userdata), f);
	fclose(f);
}

BYTE __flagdata[1024] = {0};
void set_bit(WORD bit, BOOL on)
{
	int pos = bit / 8;
	DWORD op = 1 << (bit & 7);
	if (on) {		
		__flagdata[pos] |= op;
	}
	else {
		__flagdata[pos] &= ~op;		
	}
}

DWORD IdDecode(const char* strid)
{
	DWORD id = 0;
	DWORD a = *(BYTE*)strid;
	DWORD b = *(BYTE*)(strid + 1);
	DWORD c = *(BYTE*)(strid + 2);
	if (b % 5 <= 3) {
		if (a < 0x30 || a > 0x39)
			a = a - 0x41;
		else
			a = 0x53 - a;
	}
	else {
		if (a < 0x30 || a > 0x39)
			a = a - 0x61;
		else
			a = a - 0x16;
	}
	id = c & 0x80000001;
	if ((c & 0x80000001) < 0)
		id = ((c & 0x80000001) - 1) | 0xFFFFFFFE;
	if (0 == id) {
		if (b < 0x30 || b > 0x39)
			b = b - 0x41;
		else
			b = 0x53 - b;
	}
	else {
		if (b < 0x30 || b > 0x39)
			b = b - 0x61;
		else
			b = b - 0x16;
	}
	if (c < 0x61 || c > 0x7a) {
		if (c < 0x41 || c > 0x5a) {
		}
		else {			
			c -= 0x41;
		}
	}
	else {		
		c = 0x7A - c;
	}
	id = a + 0x24 * (b + 0x24 * c);
	return id;
}

bool HandleSystemPacket(WORD protocol, const char* in_packet)
{	
	bool is_unknown = false;
	switch (protocol) {
	case 0x001: // 登录
		CFSendPacket(s_packets["login"]);
		CFSendPacket(s_packets["login_1"]);
		CFSendPacket(s_packets["login_2"]);
		break;
	case 0x01FE: // 动画欣赏
		CFSendPacket(s_packets["gallery"]);
		break;
	case 0x1770: // 地城系统
		CFSendPacket(s_packets["1770"]);
		break;
	case 0x13BA: // 地城系统
		CFSendPacket(s_packets["13BA"]);
		break;
	case 0x0212: { // 开始游戏
		ServerPacket& sp = s_packets["game_start"];
		sp.fill("monsterdata", __userdata.monsterdata);
		sp.fill("fabaodata", __userdata.fabaodata);
		CFSendPacket(sp);				 
		break;
	}
	case 0x0202: { // 更新辨识数据
		ClientPacket& cp = c_packets["update_mdata"];
		cp.parse(in_packet);
		cp.get("monsterdata", __userdata.monsterdata);
		cp.get("fabaodata", __userdata.fabaodata);
		SaveUserData();
		break;
	}
	case 0x01F4: { // 读档
		ClientPacket& cp = c_packets["load_cmd"];
		cp.parse(in_packet);		
		cp.get("flagdata", __flagdata);
		int load_idx = 0;
		cp.get("index", &load_idx);
		ServerPacket& sp = s_packets["load"];
		sp.fill("index", &load_idx);
		CFSendPacket(sp);
		break;
	}
	case 0x01F5: { // 存档
		ClientPacket& cp = c_packets["save_cmd"];
		cp.parse(in_packet);
		int load_idx = 0;
		cp.get("index", &load_idx);
		ServerPacket& sp = s_packets["save"];
		sp.fill("index", &load_idx);		
		sp.fill("flagdata", __flagdata);
		CFSendPacket(sp);
		break;
	}
	case 0x01F6: { // 设置标志加密
		ClientPacket& cp = c_packets["set_flag_encode"];
		cp.parse(in_packet);
		WORD flag = 0;
		char eid[4] = {0};
		cp.get("flag", &flag);
		cp.get("eid", eid);
		set_bit(LOWORD(IdDecode(eid)), flag);
		break;
	}
	case 0x01F7: { // 设置标志
		ClientPacket& cp = c_packets["set_flag"];
		cp.parse(in_packet);
		WORD flag = 0, id = 0;
		cp.get("flag", &flag);
		cp.get("id", &id);
		set_bit(id, flag);
		break;
	}
	case 0x0206: // 境界
		CFSendPacket(s_packets["mirror"]);
		break;
	default:
		is_unknown = true;
	}

	return !is_unknown;
}

