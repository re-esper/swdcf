#include "packet_def.h"
#include <assert.h>
#include <windows.h>

#define HDR_MAGIC "\x34\x12\x23\xAC\x3C\x1A\x22\x34"

unsigned char readhex(const char* &pp)
{
#define H2I(c) (c > '9' ? c - 'A' + 10 : c - '0')
	unsigned char rlst = 0;
	rlst = H2I(*pp) * 0x10; pp++;
	rlst += H2I(*pp); pp++;
	return rlst;
}
void readspace(const char* &pp)
{
	while (isspace(*pp)) pp++;
}
int readdef(const char* &pp, DefType& defs, int idx)
{
	if (*pp != '<') return 0;
	pp++;
	const char *p2 = strchr(pp, ':');
	if (p2 == 0) return 0;
	std::string name(pp, p2 - pp);

	pp = p2 + 1;
	const char *p3 = strchr(pp, '>');
	if (p3 == 0) return 0;

	std::string sizestr(pp, p3 - pp);
	int len = atoi(sizestr.c_str());
	defs[name] = std::pair<int, int>(idx, len);
	pp = p3 + 1;
	return len;
}
int calcdef(const char* &pp)
{
	if (*pp != '<') return 0;
	pp++;
	const char *p2 = strchr(pp, ':');
	if (p2 == 0) return 0;	

	pp = p2 + 1;
	const char *p3 = strchr(pp, '>');
	if (p3 == 0) return 0;

	std::string sizestr(pp, p3 - pp);
	pp = p3 + 1;
	return atoi(sizestr.c_str());
}

// 读取defs, 并返回最终长度
int parse_packet_def(const char* p, DefType& defs)
{
	const char *pp = p;
	int n = 0;
	while (*pp) {
		if (*pp == '<') { // is a define
			int ldef = readdef(pp, defs, n);
			assert(ldef > 0);
			n += ldef;
			readspace(pp);
		}
		else { // hex
			readhex(pp);
			n++;
			readspace(pp);
		}
	}
	return n;
}
// 读取包数据
void read_packet_data(const char* p, unsigned char* &d)
{
	const char *pp = p;
	int n = 0;
	while (*pp) {
		if (*pp == '<') { // is a define
			int ldef = calcdef(pp);
			assert(ldef > 0);
			n += ldef;
			readspace(pp);
		}
		else { // hex
			d[n] = readhex(pp);
			n++;
			readspace(pp);
		}
	}
}



ServerPacket::~ServerPacket()
{
	if (packet) delete [] packet;
}
void ServerPacket::init(const char* hdr, const char* b)
{
	int hdr_size = (int)ceil(strlen(hdr) / 3.0);
	unsigned char *header = new unsigned char[hdr_size];	
	read_packet_data(hdr, header);

	int b_size = parse_packet_def(b, defs);
	unsigned char *body = new unsigned char[b_size];
	read_packet_data(b, body);
	
	header_size = 2 + 8 + hdr_size; // should be 22
	size = header_size + b_size;
	packet = new unsigned char[size];
	*(WORD*)packet = *(WORD*)header + 1;
	memcpy(packet + 2, HDR_MAGIC, 8);
	memcpy(packet + 10, header, hdr_size);	
	memcpy(packet + header_size, body, b_size);

	delete [] header;
	delete [] body;
}
void ServerPacket::fill(const char *name, void* data)
{
	auto itr = defs.find(name);
	assert(itr != defs.end());
	memcpy(packet + header_size + itr->second.first, data, itr->second.second);			
}


void ClientPacket::init(const char* body)
{
	parse_packet_def(body, defs);
}
bool ClientPacket::get(const char *name, void* out)
{
	auto itr = defs.find(name);
	if (itr != defs.end()) {
		memcpy(out, packet + itr->second.first, itr->second.second);
		return true;
	}
	return false;
}