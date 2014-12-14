#include <windows.h>

#include "hook_impl.h"
#include "x86il.h"

#define JMP_SIZE		5
#define JMP_INSTR		0xE9
#define HOTPATCH_HEAD	0xFF8B	// MOV EDI,EDI

BOOL WINAPI HookHotPatch(void **pTarget, const void *pDetour)
{
#pragma pack(push, 1)
	typedef struct
	{
		unsigned char	nJumpInst;
		unsigned long	nRelativeAddr;
		unsigned short	nHotPatch;
	} HOT_PATCH;
#pragma pack(pop)

	HOT_PATCH *pPatch;
	DWORD op = 0;
	const unsigned char *pHook;
	const unsigned char *pDestination;

	static const unsigned char nJumpBack[] = { 0xEB, 0xF9 };

	pHook = (const unsigned char *)(*pTarget);

	if (HOTPATCH_HEAD != *((const unsigned short *)pHook))
		return FALSE;

	pDestination	= (const unsigned char *)(pDetour);
	pPatch			= (HOT_PATCH *)(pHook - JMP_SIZE);

	if (FALSE == VirtualProtect(pPatch, sizeof(HOT_PATCH), PAGE_EXECUTE_READWRITE, &op))
		return FALSE;

	pPatch->nJumpInst		= JMP_INSTR;
	pPatch->nRelativeAddr	= PtrToLong(pDestination - pHook);
	pPatch->nHotPatch		= *(const unsigned short *)nJumpBack;

	(*pTarget) = (void *)(pHook + 2);

	VirtualProtect(pPatch, sizeof(HOT_PATCH), op, &op);

	return TRUE;
}


#pragma pack(push, 1)
typedef struct {
	unsigned char savebytes[16];
	unsigned char jmptoapi[8];
	PVOID	OrgApiAddr;
	DWORD	SizeOfReplaceCode;
} HOOK_TRAMPOLINE, *PHOOK_TRAMPOLINE;
#pragma pack(pop)


BYTE jmp_gate[] = {
	JMP_INSTR, 0x00, 0x00, 0x00, 0x00   // JMP XXXXXXXX
};

#pragma comment(linker, "/SECTION:stub,RW")
#pragma code_seg("stub")
#pragma optimize("",off)
__declspec(allocate("stub")) HOOK_TRAMPOLINE tramp = {0};
__declspec(naked) DWORD get_delta()
{
	__asm
	{
		call __NEXT
__NEXT:
		pop eax
		sub eax, offset __NEXT
		ret
	}
}
__declspec(naked) void stub()
{
	__asm
	{	
		jmp __NEXT
__BACK:
		__emit JMP_INSTR
		nop
		nop
		nop
		nop
__NEXT:
		call get_delta
		jmp __BACK
	}
}
__declspec(naked) DWORD get_end_addr()
{
	__asm
	{
		call __NEXT
__NEXT:
		pop eax
		sub eax, JMP_SIZE
		ret
	}
}
#pragma optimize("",off)
#pragma code_seg()

BOOL WINAPI HookTrampoline(void **pTarget, const void *pDetour)
{
	unsigned char *pHook = (unsigned char *)(*pTarget);
	DWORD op;
	DWORD nReplaceCodeSize, nStubSize;
	DWORD delta;

	PHOOK_TRAMPOLINE pTrampoline;
	if (pHook == NULL || pDetour == NULL) return FALSE;

	nReplaceCodeSize = X86IL((BYTE*)pHook);

	while (nReplaceCodeSize < JMP_SIZE)
		nReplaceCodeSize += X86IL((BYTE*)((DWORD)pHook + (DWORD)nReplaceCodeSize));

	if (nReplaceCodeSize > 16) return NULL;

	nStubSize = get_end_addr() - (DWORD)&tramp;

	pTrampoline = (PHOOK_TRAMPOLINE)VirtualAlloc(NULL, nStubSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	memset((void*)&tramp, 0x90, sizeof(tramp));
	memcpy(pTrampoline,(PVOID)&tramp, nStubSize);
	memcpy((void*)pTrampoline, (void*)&tramp, sizeof(tramp.savebytes));
	memcpy(pTrampoline->savebytes, pHook, nReplaceCodeSize);

	pTrampoline->OrgApiAddr = pHook;
	pTrampoline->SizeOfReplaceCode = nReplaceCodeSize;

	pTrampoline->jmptoapi[0] = JMP_INSTR;
	*(DWORD*)(&pTrampoline->jmptoapi[1]) = (DWORD)pHook + nReplaceCodeSize - ((DWORD)pTrampoline->jmptoapi + JMP_SIZE);

	if (!VirtualProtect(pHook, nReplaceCodeSize, PAGE_EXECUTE_READWRITE, &op))
		return FALSE;

	delta = (DWORD)pTrampoline - (DWORD)&tramp;

	*(DWORD*)(&jmp_gate[1]) = ((DWORD)stub + delta) - ((DWORD)pHook + JMP_SIZE);

	memcpy(pHook, jmp_gate, sizeof(jmp_gate));

	if (!VirtualProtect(pHook, nReplaceCodeSize, op, &op))
		return FALSE;

	*(DWORD*)((DWORD)stub + delta + 3) = (DWORD)pDetour - ((DWORD)stub + delta + 3 + 4);

	*pTarget = (void *)pTrampoline;
	return TRUE;
}


LONG WINAPI CallAPI(FARPROC proc, UINT paramCnt, ...)
{	
	va_list argp;
	UINT* EspPoint;
	UINT paramsize = paramCnt * sizeof(UINT);
	__asm
	{
		SUB		ESP, paramsize
		MOV		EspPoint, ESP
	}
	va_start(argp, paramCnt); 
	for (unsigned int i=0; i<paramCnt; ++i)		
		EspPoint[i] = va_arg(argp, UINT);	
	va_end(argp);
	__asm
	{
		MOV		EAX, proc
		CALL	EAX
		MOV		ESP, EspPoint
		ADD		ESP, paramsize
	}
}