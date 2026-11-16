// threadsnap: prints what every thread of a running Boat Crew is doing, without restarting the game.
// For each thread: id, name, start address, CPU time, and a call stack found by scanning the stack for
// return addresses (values that point just after a call instruction in a loaded module). Each thread is
// suspended only while its registers and stack are copied. Map GameAssembly.dll+RVA lines to method names
// with symbolize.py.
//   threadsnap.exe [process name, default "Boat Crew.exe"] [frames per thread, default 40]
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <stdio.h>
#include <string>
#include <vector>

struct Mod { ULONG_PTR base, size; std::string name; };
static std::vector<Mod> g_mods;

static const Mod* ModOf(ULONG_PTR a)
{
    for (const Mod& m : g_mods) if (a >= m.base && a < m.base + m.size) return &m;
    return nullptr;
}

static std::string AddrName(ULONG_PTR a)
{
    char buf[200];
    const Mod* m = ModOf(a);
    if (m) sprintf_s(buf, "%s+0x%llX", m->name.c_str(), (unsigned long long)(a - m->base));
    else sprintf_s(buf, "0x%llX", (unsigned long long)a);
    return buf;
}

static bool Executable(HANDLE proc, ULONG_PTR a)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQueryEx(proc, (LPCVOID)a, &mbi, sizeof mbi)) return false;
    return (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

// True when the bytes before a look like a call that returns to a.
static bool AfterCall(HANDLE proc, ULONG_PTR a)
{
    unsigned char b[8];
    SIZE_T got = 0;
    if (!ReadProcessMemory(proc, (LPCVOID)(a - 8), b, 8, &got) || got != 8) return false;
    if (b[3] == 0xE8) return true;                                          // call rel32
    if (b[2] == 0xFF && (b[3] & 0x38) == 0x10 && (b[3] & 0xC7) == 0x05) return true; // call [rip+disp32]
    if (b[6] == 0xFF && (b[7] & 0xF8) == 0xD0) return true;                 // call reg
    if (b[5] == 0x41 && b[6] == 0xFF && (b[7] & 0xF8) == 0xD0) return true; // call r8-r15
    if (b[5] == 0xFF && (b[6] & 0x38) == 0x10 && (b[6] & 0xC0) == 0x40) return true; // call [reg+disp8]
    if (b[2] == 0xFF && (b[3] & 0x38) == 0x10 && (b[3] & 0xC0) == 0x80) return true; // call [reg+disp32]
    if (b[6] == 0xFF && (b[7] & 0x38) == 0x10 && (b[7] & 0xC0) == 0x00) return true; // call [reg]
    if (b[4] == 0xFF && (b[5] & 0x38) == 0x10 && b[5] == 0x54) return true; // call [rsp+..]
    return false;
}

int main(int argc, char** argv)
{
    const char* target = argc > 1 ? argv[1] : "Boat Crew.exe";
    int maxFrames = argc > 2 ? atoi(argv[2]) : 40;

    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe = { sizeof pe };
    for (BOOL ok = Process32First(snap, &pe); ok; ok = Process32Next(snap, &pe))
        if (_stricmp(pe.szExeFile, target) == 0) { pid = pe.th32ProcessID; break; }
    CloseHandle(snap);
    if (!pid) { printf("%s is not running\n", target); return 1; }

    HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!proc) { printf("OpenProcess failed (%lu)\n", GetLastError()); return 1; }

    HMODULE mods[1024];
    DWORD need = 0;
    EnumProcessModulesEx(proc, mods, sizeof mods, &need, LIST_MODULES_64BIT);
    for (DWORD i = 0; i < need / sizeof(HMODULE) && i < 1024; i++) {
        MODULEINFO mi;
        char name[MAX_PATH];
        if (!GetModuleInformation(proc, mods[i], &mi, sizeof mi)) continue;
        if (!GetModuleBaseNameA(proc, mods[i], name, MAX_PATH)) strcpy_s(name, "?");
        g_mods.push_back({ (ULONG_PTR)mi.lpBaseOfDll, mi.SizeOfImage, name });
    }

    typedef LONG (WINAPI *NtQIT)(HANDLE, int, PVOID, ULONG, PULONG);
    NtQIT ntqit = (NtQIT)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread");
    typedef HRESULT (WINAPI *GTD)(HANDLE, PWSTR*);
    GTD gtd = (GTD)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetThreadDescription");

    SYSTEMTIME st;
    GetLocalTime(&st);
    printf("threadsnap %02d:%02d:%02d.%03d pid %lu, %zu modules\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, pid, g_mods.size());

    std::vector<DWORD> tids;
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te = { sizeof te };
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te))
        if (te.th32OwnerProcessID == pid) tids.push_back(te.th32ThreadID);
    CloseHandle(snap);

    static unsigned char stack[64 * 1024];
    for (DWORD tid : tids) {
        HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid);
        if (!th) { printf("\n== thread %lu: OpenThread failed (%lu)\n", tid, GetLastError()); continue; }

        std::string tname;
        if (gtd) {
            PWSTR w = nullptr;
            if (SUCCEEDED(gtd(th, &w)) && w) {
                char n[256] = "";
                WideCharToMultiByte(CP_UTF8, 0, w, -1, n, sizeof n, nullptr, nullptr);
                tname = n;
                LocalFree(w);
            }
        }
        ULONG_PTR start = 0;
        if (ntqit) ntqit(th, 9, &start, sizeof start, nullptr); // ThreadQuerySetWin32StartAddress
        FILETIME c, e, k, u;
        double cpu = 0;
        if (GetThreadTimes(th, &c, &e, &k, &u))
            cpu = ((((ULONGLONG)k.dwHighDateTime << 32) | k.dwLowDateTime) + (((ULONGLONG)u.dwHighDateTime << 32) | u.dwLowDateTime)) / 1e7;

        CONTEXT ctx = {};
        ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        SIZE_T got = 0;
        bool haveCtx = false;
        if (SuspendThread(th) != (DWORD)-1) {
            haveCtx = GetThreadContext(th, &ctx) != 0;
            if (haveCtx) {
                MEMORY_BASIC_INFORMATION mbi;
                SIZE_T len = sizeof stack;
                if (VirtualQueryEx(proc, (LPCVOID)ctx.Rsp, &mbi, sizeof mbi)) {
                    ULONG_PTR end = (ULONG_PTR)mbi.BaseAddress + mbi.RegionSize;
                    if (end - ctx.Rsp < len) len = end - ctx.Rsp;
                }
                ReadProcessMemory(proc, (LPCVOID)ctx.Rsp, stack, len, &got);
            }
            ResumeThread(th);
        }

        printf("\n== thread %lu \"%s\" start %s cpu %.1f s\n", tid, tname.c_str(), AddrName(start).c_str(), cpu);
        if (!haveCtx) { printf("   (no context)\n"); CloseHandle(th); continue; }
        printf("   at %s\n", AddrName(ctx.Rip).c_str());
        int frames = 0;
        for (SIZE_T off = 0; off + 8 <= got && frames < maxFrames; off += 8) {
            ULONG_PTR v = *(ULONG_PTR*)(stack + off);
            if (!ModOf(v) || !Executable(proc, v) || !AfterCall(proc, v)) continue;
            printf("   %s\n", AddrName(v).c_str());
            frames++;
        }
        CloseHandle(th);
    }
    CloseHandle(proc);
    return 0;
}
