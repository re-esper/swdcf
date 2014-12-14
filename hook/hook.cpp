
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "hook_impl.h"

#define HOOK_SEND_ADDR		0x00623F87
#define HOOK_RECV_ADDR		0x00624015
#define HOOK_ENCRYPT_ADDR	0x006187F7
#define HOOK_DECRYPT_ADDR	0x006189b6

DWORD __hook_send_jmp = HOOK_SEND_ADDR + 6;
void __declspec(naked) HookSend()
{
	__asm {
		pop edi
		jmp __hook_send_jmp
	}
}
DWORD __hook_encrypt_jmp = 0x0061894C;
void __declspec(naked) HookEncrypt()
{
	__asm {
		add eax, 2
		jmp __hook_encrypt_jmp
	}
}
DWORD __hook_recv_jmp = HOOK_RECV_ADDR + 9;
void __declspec(naked) HookRecv()
{
	__asm jmp __hook_recv_jmp	
}
DWORD __hook_decrypt_jmp = 0x00618AD4;
void __declspec(naked) HookDecrypt()
{
	__asm jmp __hook_decrypt_jmp	
}

void MakeJmp(DWORD addr, DWORD jumpto)
{
	DWORD op = 0;
	VirtualProtect((LPVOID)addr, 6, PAGE_EXECUTE_READWRITE, &op);
	*(BYTE*)addr = 0x68;
	*(DWORD*)(addr + 1) = jumpto;
	*(BYTE*)(addr + 5) = 0xC3;
	VirtualProtect((LPVOID)addr, 6, op, &op);
}

struct {
	LONG	height;
	LONG	mapped;
	char*	font;
} __ModifiedFontMap[] = {
	{ 12,	12,		"PMingLiU" }, 
	{ 16,	-12,	"Î¢ÈíÑÅºÚ" }, // 9ºÅ
	{ 18,	-14,	"Î¢ÈíÑÅºÚ" }, // 11ºÅ
	{ 20,	-16,	"Î¢ÈíÑÅºÚ" }, // 12ºÅ
	{ 22,	-18,	"Î¢ÈíÑÅºÚ" }, // 14ºÅ
	{ 0,	0,		0 },
};
FARPROC __CreateFontIndirectA = 0;
HFONT WINAPI HookCreateFontIndirectA(LOGFONTA* lplf)
{	
	for (int i = 0; __ModifiedFontMap[i].height != 0; ++i) {
		if (lplf->lfHeight == __ModifiedFontMap[i].height) {
			strcpy(lplf->lfFaceName, __ModifiedFontMap[i].font);	
			lplf->lfHeight = __ModifiedFontMap[i].mapped;
			break;
		}
	}	
	return (HFONT)CallAPI(__CreateFontIndirectA, 1, lplf);
}


void WINAPI thread_Hook(LPVOID lp)
{
	// ÐÞ¸Ä×ÖÌå
	__CreateFontIndirectA = GetProcAddress(GetModuleHandle("gdi32.dll"), "CreateFontIndirectA");
	HookHotPatch((void**)&__CreateFontIndirectA, HookCreateFontIndirectA);

	Sleep(2000);	
	while (*(DWORD*)0x0062389D != 0x5102C183) { Sleep(100); }

	MakeJmp(HOOK_SEND_ADDR, (DWORD)HookSend);
	MakeJmp(HOOK_ENCRYPT_ADDR, (DWORD)HookEncrypt);
	MakeJmp(HOOK_RECV_ADDR, (DWORD)HookRecv);
	MakeJmp(HOOK_DECRYPT_ADDR, (DWORD)HookDecrypt);
}


BOOL APIENTRY DllMain( HMODULE hModule,
					  DWORD  ul_reason_for_call,
					  LPVOID lpReserved
					  )
{
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		CloseHandle(CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_Hook, NULL, 0, NULL));
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
