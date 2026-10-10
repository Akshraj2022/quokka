#include <windows.h>
#include <stdio.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // Hide console window
    
    char cmd[] = "quokka.exe run GUICHENOTIA.qka";
    
    // Launch quokka run GUICHENOTIA.qka
    if (CreateProcess(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        MessageBox(NULL, "Could not find quokka.exe or GUICHENOTIA.qka. Please reinstall.", "GUICHENOTIA Error", MB_ICONERROR);
    }
    
    return 0;
}
