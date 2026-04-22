# Burning Sun
> The sun doesn't ask for permission to rise. Neither do we.

Windows Defender Privilege Escalation PoC — Local Privilege Escalation to `NT AUTHORITY\SYSTEM` via race condition in the Defender real-time protection signature update pipeline.

> **DISCLAIMER:** This project is strictly for educational and authorized security research purposes only. Unauthorized use of this code against systems you do not own or have explicit permission to test is illegal and unethical. The author is not responsible for any misuse.

> **Zero-Day Notice:** As of the time of writing (22 April 2026), this vulnerability is unpatched and works against the latest Windows 10/11 builds with current Defender definitions. This is a 0-day — Microsoft can patch it at any time via Windows Update or a Defender definition push. If the exploit breaks after an update, the race window was likely closed or the Tiering Engine service loading was hardened. No guarantee of longevity. CREDITS - https://github.com/Nightmare-Eclipse/RedSun

---

## Overview

The exploit abuses a race condition during Windows Defender's real-time protection response to achieve arbitrary file write to `C:\Windows\System32`, then leverages the Windows Storage Tiering Engine COM service to execute the planted binary as `NT AUTHORITY\SYSTEM`.

The attack chain combines:

- **Cloud Files API** (`cldapi.dll`) — placeholder file creation via sync provider registration
- **NTFS Opportunistic Locks** — stalling Defender's file I/O at critical moments
- **NTFS Junction Points** — redirecting a temp directory to `System32` mid-operation
- **Volume Shadow Copy detection** — monitoring the Object Manager's `\Device` directory for AV-triggered shadow copies
- **Storage Tiering Engine Service** — COM activation (`CLSCTX_LOCAL_SERVER`) for SYSTEM-level code execution

---

## Files

| File | Codename | Description |
|------|----------|-------------|
| `Red-Sun.cpp` | **Red Sun** | The original PoC that inspired this work. Contains the core exploit logic but has multiple compilation errors and spawns a visible `conhost.exe` shell on SYSTEM elevation. Does not compile with GCC/MinGW. |
| `Burning-Sun.cpp` | **Burning Sun** | Our improved version. Compiles cleanly on both MinGW and MSVC, removes the visible shell entirely, and replaces it with silent automated registry hive extraction. Exfiltration logic is left for the user to implement. |

---

## Attack Flow

```
 1. Create named pipe (\\.\pipe\REDSUN) as instance lock
 2. Create temp working directory (%TEMP%\RS-{GUID})
 3. Write reversed EICAR test string to bait file
    (TieringEngineService.exe in temp dir)
 4. Trigger Defender RTP scan via FILE_EXECUTE access
 5. Shadow Copy Finder thread detects new VSS volume
 6. Acquire Batch OpLock on VSS copy of bait file
 7. Delete original bait, replace with CF API placeholder
 8. Rename + delete-on-close the working directory
 9. Re-create directory as NTFS junction -> System32
10. Race Defender's file restoration into System32
11. Overwrite TieringEngineService.exe in System32
12. Copy self to System32 & trigger COM activation
13. Service starts as NT AUTHORITY\SYSTEM
14. On SYSTEM context: dump SAM / SYSTEM / SECURITY hives
```

### Key Techniques

**EICAR Bait** — The EICAR test string is stored reversed in the binary and flipped at runtime, then the file is opened with `FILE_EXECUTE` to force Defender's real-time scan.

**VSS Race Detection** — A background thread polls the NT Object Manager `\Device` directory for new `HarddiskVolumeShadowCopy*` entries that appear when Defender triggers a Volume Shadow Copy during remediation.

**OpLock Stalling** — Batch OpLocks (`FSCTL_REQUEST_BATCH_OPLOCK`) are placed on both the shadow copy and the recreated placeholder to stall Defender's file operations, widening the race window.

**Junction Point Redirect** — The working directory is torn down and recreated as an NTFS mount point targeting `C:\Windows\System32`. When Defender restores the file, it writes into System32.

**COM Service Hijack** — The Storage Tiering Management Engine (`{50d185b9-fff3-4656-92c7-e4018da4361d}`) is triggered via `CoCreateInstance` with `CLSCTX_LOCAL_SERVER`, loading the planted `TieringEngineService.exe` under SYSTEM context.

---

## What's Wrong With Red Sun (Original)

The original PoC (`Red-Sun.cpp`) doesn't compile under GCC/MinGW and has several bugs. Here's what we found and fixed:

### 1. Duplicated Functions

`ShadowCopyFinderThread` and the `gevent` global are literally copy-pasted twice in the file (lines 414-583 and 586-755). Any compiler will throw `multiple definition` errors.

### 2. Undefined Symbols

The thread code references `_GetOverlappedResult` and `_WaitForSingleObject` — function pointers that are never declared, never initialized, and never loaded via `GetProcAddress`. Instant compilation failure.

### 3. MSVC-Only Pragmas

```cpp
#pragma comment(lib,"synchronization.lib")
#pragma comment(lib,"sas.lib")
#pragma comment(lib,"ntdll.lib")
#pragma comment(lib,"CldApi.lib")
```

GCC ignores these entirely. If you try to compile with MinGW, you get unresolved externals with no indication of what libraries are needed.

### 4. Missing UNICODE Defines

No `#define UNICODE` or `#define _UNICODE` at the top. Under MinGW, this causes mismatched function signatures — `CreateFile` resolves to the ANSI variant while the code passes wide strings everywhere.

### 5. Struct Redefinition

`FILE_RENAME_INFORMATION` is manually redefined, but MinGW already pulls it in from `<winternl.h>`. Redefinition error.

### 6. NULL Where 0 Is Expected

`NULL` is passed to Win32 functions expecting integer parameters (`FileAttributes`, `Reserved`, `HeapFree` flags, etc.). GCC treats these as pointer-to-integer truncation warnings, and with `-Werror` they become hard errors.

### 7. Memory Leak

The `FILE_RENAME_INFORMATION` allocation (`pfri`) inside the rename block is never freed.

### 8. Fire-and-Forget CopyFile

The original calls `CopyFileW` to deploy itself to System32 but never checks the return value. If the copy fails, it still triggers the COM service — which then loads whatever was already there (or nothing).

---

## What Burning Sun Does Differently

### No More Shell Popup

The biggest change. Red Sun spawns a **visible `conhost.exe` window** on the user's desktop when it gets SYSTEM:

```cpp
// Red-Sun.cpp — the original approach
void LaunchConsoleInSessionId()
{
    // Gets session ID from the named pipe
    GetNamedPipeServerSessionId(hpipe, &sessionid);
    
    // Duplicates SYSTEM token into user's session
    DuplicateTokenEx(htoken, TOKEN_ALL_ACCESS, ..., &hnewtoken);
    SetTokenInformation(hnewtoken, TokenSessionId, &sessionid, ...);
    
    // Pops a visible console as SYSTEM
    CreateProcessAsUser(hnewtoken, L"C:\\Windows\\System32\\conhost.exe", ...);
}
```

This is loud. Anyone watching the screen sees a new console window. EDR logs an interactive SYSTEM process. And you still have to manually type commands in it.

Burning Sun removes all of that. Instead, the moment SYSTEM context is confirmed, it silently:

1. Enables `SeBackupPrivilege`
2. Dumps SAM, SYSTEM, and SECURITY registry hives to disk
3. Exits cleanly — no window, no shell, no noise

```cpp
// Burning-Sun.cpp — silent, automated
if (ret) {
    EnablePrivilege(L"SeBackupPrivilege");
    
    SaveHive(HKEY_LOCAL_MACHINE, L"SAM",      L"C:\\Windows\\System32\\sam.hiv");
    SaveHive(HKEY_LOCAL_MACHINE, L"SYSTEM",   L"C:\\Windows\\System32\\system.hiv");
    SaveHive(HKEY_LOCAL_MACHINE, L"SECURITY", L"C:\\Windows\\System32\\security.hiv");
    
    ExitProcess(0);
}
```

### Exfiltration — Your Call

The hive dump is local-only by design. Files land in `C:\Windows\System32\` as `.hiv` files. What you do with them after is up to you.

Hook in your own logic right after the `SaveHive()` calls:

```cpp
// ── YOUR EXFILTRATION LOGIC HERE ──
// HTTP POST via WinHTTP / WinINet
// SMB copy to a network share
// Encrypt + stage to cloud blob storage
// Compress with RtlCompressBuffer and drop to USB
// ───────────────────────────────────
ExitProcess(0);
```

### What the Hives Contain

| Hive | What's Inside |
|------|---------------|
| **SAM** | NTLM password hashes for all local accounts. Crack with `hashcat`/`john` or pass-the-hash. |
| **SYSTEM** | Boot key needed to decrypt SAM hashes. Also contains LSA secrets and service account credentials. |
| **SECURITY** | Cached domain logon credentials (DCC2), LSA policies, security audit configuration. |

You need both SAM + SYSTEM together to extract usable hashes. Tools like `secretsdump.py` (Impacket), `mimikatz`, or `samdump2` work offline.

---

## Full Comparison

| | Red Sun (Original) | Burning Sun (Ours) |
|---|---|---|
| **Compiles with MinGW** | No — multiple errors | Yes — clean build with `-municode` |
| **Duplicated functions** | `ShadowCopyFinderThread` + `gevent` defined twice | Single implementation |
| **Undefined symbols** | `_GetOverlappedResult`, `_WaitForSingleObject` never declared | Uses standard Win32 APIs directly |
| **MSVC-only directives** | `#pragma comment(lib, ...)` | Removed; pass libs via compiler flags |
| **UNICODE defines** | Missing | Properly defined |
| **Struct conflicts** | Redefines `FILE_RENAME_INFORMATION` | Uses SDK definition |
| **NULL vs 0** | NULL for integer params (GCC warnings) | Correct integer literals |
| **SYSTEM payload** | Visible `conhost.exe` shell popup | Silent hive dump, no GUI |
| **Synchronization** | `WaitOnAddress`/`WakeByAddressAll` with race | `SetEvent`/`WaitForSingleObject`, proper 2-stage signaling |
| **Retry logic** | Infinite retry, can hang forever | Bounded `RETRY_MAX_COUNT` with sleep |
| **Resource cleanup** | Leaks handles, memory, named pipe | Proper cleanup on all exit paths |
| **Config values** | Magic numbers hardcoded | Named constants (`AV_TIMEOUT_MS`, etc.) |
| **VSS buffer safety** | No bounds check before `memmove` | `MAX_PATH` validation |
| **Thread safety** | Bare global event | `CRITICAL_SECTION` + managed init/cleanup |
| **Self-deploy check** | Always re-runs full chain | Detects prior deployment, skips to trigger |
| **CopyFile handling** | Fire-and-forget | Checks return, triggers service only on success |
| **Logging** | `printf` only | File-based logging to `%TEMP%\poc_research.log` |

---

## Build

### Prerequisites

- MinGW-w64 or MSVC (Visual Studio)
- Windows SDK headers (`winternl.h`, `ntstatus.h`)
- Target: Windows 10/11 with Defender Real-Time Protection enabled

### MinGW

```bash
x86_64-w64-mingw32-g++ -o Burning-Sun.exe Burning-Sun.cpp \
    -lole32 -luuid -lntdll -ladvapi32 \
    -static -municode -O2
```

### MSVC

```powershell
cl /EHsc /O2 Burning-Sun.cpp ole32.lib uuid.lib ntdll.lib advapi32.lib
```

---

## Usage

Requires administrator privileges. Defender Real-Time Protection must be **enabled** (that's the whole point).

```powershell
.\Burning-Sun.exe
```

### Expected Output

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

### Output Files

| File | Contents |
|------|----------|
| `C:\Windows\System32\sam.hiv` | SAM database (password hashes) |
| `C:\Windows\System32\system.hiv` | SYSTEM hive (boot key, LSA secrets) |
| `C:\Windows\System32\security.hiv` | SECURITY hive (cached creds, policies) |

---

## Testing

- Use a **virtual machine** (VMware, Hyper-V, VirtualBox)
- Take a **snapshot** before running — revert after
- Defender definitions should be reasonably current
- Monitor with Process Monitor / DebugView for diagnostics

---

## References

- [EICAR Anti-Virus Test File](https://www.eicar.org/download-anti-malware-testfile/)
- [Windows Cloud Files API](https://learn.microsoft.com/en-us/windows/win32/cfapi/)
- [Opportunistic Locks](https://learn.microsoft.com/en-us/windows/win32/fileio/opportunistic-locks)
- [NTFS Reparse Points](https://learn.microsoft.com/en-us/windows/win32/fileio/reparse-points)

---

## Legal

This tool is provided as-is for educational purposes and authorized penetration testing only. Usage against targets without prior mutual consent is illegal. The end user is responsible for complying with all applicable laws. The developer assumes no liability for any misuse or damage caused by this program.
