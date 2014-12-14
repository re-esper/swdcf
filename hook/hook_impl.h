#ifndef __HOOK_IMPL_H_INCLUDED__
#define __HOOK_IMPL_H_INCLUDED__

BOOL WINAPI HookHotPatch(void **pTarget, const void *pDetour);
BOOL WINAPI HookTrampoline(void **pTarget, const void *pDetour);
LONG WINAPI CallAPI(FARPROC func, UINT paramCnt, ...);

#endif // __HOOK_IMPL_H_INCLUDED__

