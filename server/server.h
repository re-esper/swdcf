#pragma once

#include <windows.h>
#include "packet_def.h"

#define	PROTO_C2S_KEEPALIVE		0x0032
#define	PROTO_C2S_BATTLE		0x020A


void CFSendPacket(const ServerPacket& sp);
void CFSendPacket(const char* header, const unsigned char* body);

void LoadUnitData();
bool LoadBattleDef();
void LoadUserData();

bool HandleSystemPacket(WORD protocol, const char* inpacket);
void HandleBattlePacket(const char* packet);

