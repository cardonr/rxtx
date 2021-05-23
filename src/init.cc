#include <windows.h> 
/*
extern "C" 
{ int WINAPI dll_entry (HANDLE handke, DWORD dword, void *var); };
int WINAPI dll_entry (HANDLE , DWORD dword, void *)
{ return 1; }
*/

extern "C"
{
  int WINAPI dll_entry (HANDLE h, DWORD reason, void *ptr);
};

int WINAPI dll_entry (HANDLE ,
                     DWORD reason,
                     void *)
{
  switch (reason)
    {
    case DLL_PROCESS_ATTACH:
      break;
    case DLL_PROCESS_DETACH:
      break;
    case DLL_THREAD_ATTACH:
      break;
    case DLL_THREAD_DETACH:
      break;
    }
  return 1;
}

