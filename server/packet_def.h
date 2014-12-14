#pragma once

#include <map>
#include <string>
#include <vector>

typedef std::map<std::string, std::pair<int, int>> DefType;

class ServerPacket {
public:
	ServerPacket() : packet(0), size(0) {}	
	~ServerPacket();
public:	
	void init(const char* header, const char* body);
	void fill(const char *name, void* data);	
	unsigned char*	packet;	
	unsigned int	size;
	unsigned int	header_size;
private:
	DefType	defs;

};

class ClientPacket {
public:
	ClientPacket() : packet(0) {}	
	~ClientPacket() {}
public:	
	void init(const char* body);
	void parse(const void* p) { packet = (const unsigned char*)p; }
	bool get(const char *name, void* out);
private:
	DefType	defs;
	const unsigned char*	packet;	
};