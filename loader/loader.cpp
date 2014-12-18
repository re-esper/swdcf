#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

int CreateProcessWithDll(LPWSTR pwzCmdline, LPCWSTR pwzDir, LPCSTR szDll)
{
	STARTUPINFOW si = {0};     
	PROCESS_INFORMATION pi = {0};  
	si.cb = sizeof(STARTUPINFOW);	
	if (CreateProcessW(pwzCmdline, pwzCmdline, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, pwzDir, &si, &pi))
	{
		void *remote_str = NULL;
		HANDLE thrd;
		LPTHREAD_START_ROUTINE pLoadLibrary;
		if (NULL == (remote_str = VirtualAllocEx(pi.hProcess, NULL, 0x1000, MEM_COMMIT, PAGE_READWRITE)))
			return 1;
		if (FALSE == WriteProcessMemory(pi.hProcess, remote_str, szDll, strlen(szDll)+1, NULL)) {
			VirtualFreeEx(pi.hProcess, remote_str, 0x1000, MEM_RELEASE);
			return 2;
		}	
		if (NULL == (pLoadLibrary = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "LoadLibraryA"))) {
			VirtualFreeEx(pi.hProcess, remote_str, 0x1000, MEM_RELEASE);
			return 3;
		}	
		if (NULL == (thrd = CreateRemoteThread(pi.hProcess, NULL, 0, pLoadLibrary, remote_str, 0, NULL))) {
			VirtualFreeEx(pi.hProcess, remote_str, 0x1000, MEM_RELEASE);
			return 4;
		}
		WaitForSingleObject(thrd, INFINITE);
		CloseHandle(thrd);
		VirtualFreeEx(pi.hProcess, remote_str, 0x1000, MEM_RELEASE);
		ResumeThread(pi.hThread);
	}
	return 0;
}
 
#define DEBUG_PACKET
#ifdef DEBUG_PACKET
const char injected[] = "packet.dll";
const wchar_t host[] = L"d:\\SwdCF\\CH\\SwdCF.exe";
#else
const char injected[] = "hook.dll";
//const wchar_t host[] = L"C:\\SwdCF\\CH\\USwdCF.exe";
const wchar_t host[] = L"USwdCF.exe";
#endif


int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{	
	char szDll[MAX_PATH] = {0};
	
	GetModuleFileNameA(0, szDll, MAX_PATH);
	(strrchr(szDll, '\\'))[1] = 0;
	strcat(szDll, injected);	

	int err = CreateProcessWithDll((LPWSTR)host, NULL, szDll);
	if (err != 0)
	{
		TCHAR err_text[0x20] = {0};
		wsprintf(err_text, _T("Loader failed! Error = %d"), err);
		MessageBox(0, _T("Error"), err_text, MB_OK);		
	}
	return 0;
}
