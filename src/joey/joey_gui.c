#define TokenType QkTokenType
#include "../bootstrap/quokka.h"
#undef TokenType

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

static HWND hEditor, hConsole, hBtnRun, hBtnOpen, hBtnSave;
static WNDPROC OldEditorProc;
static char currentFile[MAX_PATH] = "";

LRESULT CALLBACK EditorProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_TAB) {
        SendMessage(hwnd, EM_REPLACESEL, TRUE, (LPARAM)"    ");
        return 0;
    }
    return CallWindowProc(OldEditorProc, hwnd, msg, wParam, lParam);
}

void GuiRunScript() {
    int len = GetWindowTextLength(hEditor);
    char *src = malloc(len + 1);
    GetWindowText(hEditor, src, len + 1);
    
    FILE *f = fopen("GUICHENOTIA_temp.qk", "wb");
    if (f) {
        fwrite(src, 1, len, f);
        fclose(f);
    }
    free(src);
    
    SetWindowText(hConsole, "> Running script...\r\n");
    
    char cmdLine[256];
    sprintf(cmdLine, "quokka.exe run GUICHENOTIA_temp.qk");
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    HANDLE hRead, hWrite;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        return;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hWrite;
    si.hStdOutput = hWrite;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    
    if (CreateProcess(NULL, cmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(hWrite);
        
        char buf[1024];
        DWORD bytesRead;
        while (ReadFile(hRead, buf, sizeof(buf)-1, &bytesRead, NULL) && bytesRead > 0) {
            buf[bytesRead] = '\0';
            SendMessage(hConsole, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
            SendMessage(hConsole, EM_REPLACESEL, FALSE, (LPARAM)buf);
        }
        
        CloseHandle(hRead);
        WaitForSingleObject(pi.hProcess, INFINITE);
        
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        char exitMsg[128];
        sprintf(exitMsg, "\r\n> Process exited with code %lu\r\n", exitCode);
        SendMessage(hConsole, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
        SendMessage(hConsole, EM_REPLACESEL, FALSE, (LPARAM)exitMsg);
    } else {
        CloseHandle(hWrite);
        CloseHandle(hRead);
        SendMessage(hConsole, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
        SendMessage(hConsole, EM_REPLACESEL, FALSE, (LPARAM)"\r\n[Execution Error] Could not start quokka.exe (make sure it's in PATH or same folder)\r\n");
    }
}

void GuiOpenFile(HWND hwnd) {
    OPENFILENAME ofn;
    char szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Quokka Files\0*.qk;*.qka\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE) {
        strcpy(currentFile, szFile);
        FILE *f = fopen(szFile, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *buf = malloc(len + 1);
            fread(buf, 1, len, f);
            buf[len] = '\0';
            fclose(f);
            
            SetWindowText(hEditor, buf);
            free(buf);
            
            char title[300];
            sprintf(title, "GUICHENOTIA - %s", szFile);
            SetWindowText(hwnd, title);
        }
    }
}

void GuiSaveFile(HWND hwnd) {
    if (strlen(currentFile) == 0) {
        OPENFILENAME ofn;
        char szFile[260] = {0};
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "Quokka Files\0*.qk;*.qka\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrDefExt = "qk";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        
        if (GetSaveFileName(&ofn) == TRUE) {
            strcpy(currentFile, szFile);
            char title[300];
            sprintf(title, "GUICHENOTIA - %s", szFile);
            SetWindowText(hwnd, title);
        } else {
            return;
        }
    }
    
    int len = GetWindowTextLength(hEditor);
    char *src = malloc(len + 1);
    GetWindowText(hEditor, src, len + 1);
    
    FILE *f = fopen(currentFile, "wb");
    if (f) {
        fwrite(src, 1, len, f);
        fclose(f);
    }
    free(src);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            hBtnOpen = CreateWindowEx(0, "BUTTON", "Open",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                10, 10, 80, 30, hwnd, (HMENU)1, NULL, NULL);
            hBtnSave = CreateWindowEx(0, "BUTTON", "Save",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                100, 10, 80, 30, hwnd, (HMENU)2, NULL, NULL);
            hBtnRun = CreateWindowEx(0, "BUTTON", "Run Script",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                190, 10, 120, 30, hwnd, (HMENU)3, NULL, NULL);
                
            hEditor = CreateWindowEx(0, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                10, 50, 860, 400, hwnd, (HMENU)4, NULL, NULL);
            hConsole = CreateWindowEx(0, "EDIT", "Output / REPL\r\n",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                10, 460, 860, 200, hwnd, (HMENU)5, NULL, NULL);
            
            HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, 
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_MODERN, "Consolas");
            SendMessage(hEditor, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hConsole, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hBtnRun, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hBtnOpen, WM_SETFONT, (WPARAM)hFont, TRUE);
            SendMessage(hBtnSave, WM_SETFONT, (WPARAM)hFont, TRUE);

            OldEditorProc = (WNDPROC)SetWindowLongPtr(hEditor, GWLP_WNDPROC, (LONG_PTR)EditorProc);
            break;
            
        case WM_COMMAND:
            if (LOWORD(wParam) == 1) GuiOpenFile(hwnd);
            else if (LOWORD(wParam) == 2) GuiSaveFile(hwnd);
            else if (LOWORD(wParam) == 3) GuiRunScript();
            break;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void joey_start_gui() {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    const char CLASS_NAME[] = "QuokkaGUIClass";
    
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = CreateSolidBrush(RGB(250, 246, 240)); // BG_COLOR
    
    RegisterClass(&wc);
    
    HWND hwnd = CreateWindowEx(0, CLASS_NAME, "GUICHENOTIA (Native)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 900, 720,
        NULL, NULL, hInstance, NULL);
        
    if (hwnd == NULL) return;
    
    HWND hwndConsole = GetConsoleWindow();
    if (hwndConsole) ShowWindow(hwndConsole, SW_HIDE);
    
    ShowWindow(hwnd, SW_SHOW);
    
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    if (hwndConsole) ShowWindow(hwndConsole, SW_SHOW);
}
#else
void joey_start_gui() {
    printf("Native GUI is only supported on Windows in this version.\n");
}
#endif

Value builtin_joey_start_gui(Value* args, int argc, int line) {
    (void)args; (void)argc; (void)line;
    joey_start_gui();
    return val_none();
}
