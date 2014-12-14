// proto.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include <tchar.h>
#include <windows.h>

#include "server.h"
#include "packet_def.h"

#include <json/json.h>
#include <fstream>
#include <map>
#include <string>

#pragma comment(lib, "wsock32.lib")
#define ERR_EXIT(m) { printf(m); printf("\r\n"); return 0; }


std::map<std::string, ServerPacket> s_packets;
std::map<std::string, ClientPacket> c_packets;

SOCKET __sock = 0;
char __packet[0x1000] = {0};
void CFSendPacket(const char* header, const unsigned char* body)
{	
	int size = 2 + *(WORD*)header;
	memcpy(__packet, header, 22);
	memcpy(__packet + 22, body, size - 22);
	send(__sock, __packet, size, 0);
}


void CFSendPacket(const ServerPacket& sp)
{
	int size = 2 + *(WORD*)sp.packet;
	memcpy(__packet, sp.packet, size);
	send(__sock, __packet, size, 0);
}

bool LoadSPacketDef()
{
    std::ifstream ifs;
	Json::Reader reader;
    Json::Value root;
    ifs.open("s_packet.def");
    if (!reader.parse(ifs, root, false)) return false;
	const Json::Value::Members& pns = root.getMemberNames();
	for (auto name : pns) {
		s_packets[name] = ServerPacket();
		s_packets[name].init(root[name]["header"].asCString(), root[name]["body"].asCString());
	}
	return true;
}
bool LoadCPacketDef()
{
    std::ifstream ifs;
	Json::Reader reader;
    Json::Value root;
    ifs.open("c_packet.def");
    if (!reader.parse(ifs, root, false)) return false;
	const Json::Value::Members& pns = root.getMemberNames();
	for (auto name : pns) {
		c_packets[name] = ClientPacket();
		c_packets[name].init(root[name].asCString());
	}
	return true;
}

int _tmain(int argc, _TCHAR* argv[])
{
	WORD ver = MAKEWORD(2, 2);
	WSADATA wsaData;
	WSAStartup(ver, &wsaData);	

	srand(GetTickCount());

	if (!LoadSPacketDef())
		ERR_EXIT("loading s_packet.def failed!");			
	if (!LoadCPacketDef())
		ERR_EXIT("loading c_packet.def failed!");
	if (!LoadBattleDef())
		ERR_EXIT("loading battle.def failed!");

	LoadUnitData();
	LoadUserData();

	// Server Initialize
	int listenfd;
	if ((listenfd = socket(PF_INET, SOCK_STREAM, IPPROTO_IP)) < 0)		
		ERR_EXIT("socket error");
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(21000);
	servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	int on = 1;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)) < 0)
		ERR_EXIT("setsockopt error");
	if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		ERR_EXIT("bind error");
	if (listen(listenfd, SOMAXCONN) < 0)
		ERR_EXIT("listen error");

	printf("waiting for connection ...\n");
	struct sockaddr_in peeraddr;
	int peerlen = sizeof(peeraddr);
	if ((__sock = accept(listenfd, (struct sockaddr *)&peeraddr, &peerlen)) < 0)
		ERR_EXIT("accept error");
	printf("connection accepted! ip=%s port=%d\n", inet_ntoa(peeraddr.sin_addr),
		ntohs(peeraddr.sin_port));

	// Server Main Loop
	static char recvbuf[0x1000];
	char *recvpacket = recvbuf + 4;
	send(__sock, "\x05\x00\xFF\xFF\xF7\x6F\x3E", 7, 0);
	while (1)
	{
		int rd = recv(__sock, recvbuf, sizeof(recvbuf), 0);
		if (rd <= 0 || WSAGetLastError() == WSAECONNRESET) {
			printf("client disconnected!\n");
			break;
		}
		WORD protocol = *(WORD*)recvpacket;
		if (protocol == PROTO_C2S_KEEPALIVE) continue;
		else if (protocol == PROTO_C2S_BATTLE) {
			HandleBattlePacket(recvpacket);
		}
		else if (!HandleSystemPacket(protocol, recvpacket)) {			
			printf("unknown packet protocol %04X occurred!\n", protocol);
		}
	}

	WSACleanup();
	return 0;
}
