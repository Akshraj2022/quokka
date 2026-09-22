#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

// ANSI Escape Codes
#define ANSI_CLEAR "\033[2J\033[H"
#define ANSI_HIDE_CURSOR "\033[?25l"
#define ANSI_SHOW_CURSOR "\033[?25h"
#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define ANSI_CYAN "\033[36m"
#define ANSI_GREEN "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_RED "\033[31m"

const char* SPARK_CHARS[] = {" ", "▂", "▃", "▄", "▅", "▆", "▇", "█"};

void enable_ansi() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
    SetConsoleOutputCP(CP_UTF8);
#endif
}

void cleanup() {
    printf("%s", ANSI_SHOW_CURSOR);
#ifdef _WIN32
    WSACleanup();
#endif
}

int get_vram_mb() {
#ifdef _WIN32
    FILE* fp = _popen("nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits 2>NUL", "r");
#else
    FILE* fp = popen("nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits 2>/dev/null", "r");
#endif
    if(!fp) return 0;
    char buf[128];
    int vram = 0;
    if (fgets(buf, sizeof(buf), fp) != NULL) {
        vram = atoi(buf);
    }
#ifdef _WIN32
    _pclose(fp);
#else
    pclose(fp);
#endif
    return vram;
}

// Very simple JSON string extraction
double get_json_num(const char* json, const char* key) {
    char search[64];
    sprintf(search, "\"%s\":", key);
    const char* p = strstr(json, search);
    if (!p) return 0.0;
    p += strlen(search);
    return atof(p);
}

void save_history(double final_loss, double duration) {
    FILE *f = fopen("joey_history.log", "a");
    if (f) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] Job Finished | Duration: %.1fs | Final Loss: %.4f\n",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec,
            duration, final_loss);
        fclose(f);
    }
}

int joey_watch_main(void) {
    enable_ansi();
    printf("%s", ANSI_HIDE_CURSOR);
    atexit(cleanup);
    
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
#endif

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        printf("Failed to create socket.\n");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(9001);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        printf("Failed to bind socket on port 9001. Is another instance running?\n");
        closesocket(server_fd);
        return 1;
    }

    if (listen(server_fd, 1) == SOCKET_ERROR) {
        printf("Listen failed.\n");
        closesocket(server_fd);
        return 1;
    }

    printf(ANSI_CLEAR);
    printf("Waiting for Joey to connect on port 9001...\n");

    SOCKET client_fd = accept(server_fd, NULL, NULL);
    if (client_fd == INVALID_SOCKET) {
        printf("Accept failed.\n");
        closesocket(server_fd);
        return 1;
    }

    char buffer[4096] = {0};
    int recv_len;
    
    double loss_history[20] = {0};
    int history_idx = 0;
    
    time_t start_time = time(NULL);
    double last_loss = 0.0;
    
    while ((recv_len = recv(client_fd, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[recv_len] = '\0';
        
        char *line = strtok(buffer, "\n");
        while(line) {
            if (strstr(line, "\"step\"") != NULL) {
                int step = (int)get_json_num(line, "step");
                int total_steps = (int)get_json_num(line, "total_steps");
                double loss = get_json_num(line, "loss");
                double throughput = get_json_num(line, "tokens_per_sec");
                
                last_loss = loss;
                
                // Update sparkline
                loss_history[history_idx % 20] = loss;
                history_idx++;
                
                // Clear and draw
                printf(ANSI_CLEAR);
                printf(ANSI_BOLD ANSI_CYAN "🦦 Quokka Joey - LoRA Training Monitor\n" ANSI_RESET);
                printf("=========================================\n\n");
                
                int pct = total_steps > 0 ? (step * 100 / total_steps) : 0;
                // Generate progress bar string
                char bar_str[21];
                int filled = pct / 5;
                for(int i=0; i<20; i++) bar_str[i] = (i < filled) ? '=' : ' ';
                bar_str[20] = '\0';

                printf("Progress: [%s] %d%% (%d/%d)\n", 
                    bar_str, pct, step, total_steps);
                    
                printf("Loss:     %.4f ", loss);
                
                // Draw sparkline
                for(int i=0; i<20; i++) {
                    if (i < history_idx) {
                        int idx = (history_idx <= 20) ? i : ((history_idx + i) % 20);
                        double val = loss_history[idx];
                        int bar = (int)(val * 3); // Naive scaling
                        if (bar < 0) bar = 0;
                        if (bar > 7) bar = 7;
                        printf("%s", SPARK_CHARS[7 - bar]); // Invert so lower loss is lower bar, or whatever visual makes sense
                    }
                }
                printf("\n");
                
                printf("Speed:    %.2f tk/s\n", throughput);
                
                int vram = get_vram_mb();
                if(vram > 0) {
                    printf("VRAM:     %d MB\n", vram);
                } else {
                    printf("VRAM:     N/A (No CUDA / nvidia-smi)\n");
                }
                
                if (step > 0 && total_steps > 0) {
                    time_t now = time(NULL);
                    double elapsed = difftime(now, start_time);
                    double eta = (elapsed / step) * (total_steps - step);
                    printf("ETA:      %.0f seconds\n", eta);
                }
                
                printf("\n=========================================\n");
            } else if (strstr(line, "\"status\": \"completed\"")) {
                printf(ANSI_GREEN "\n[Joey] Training Completed Successfully!\n" ANSI_RESET);
            }
            line = strtok(NULL, "\n");
        }
    }

    time_t end_time = time(NULL);
    save_history(last_loss, difftime(end_time, start_time));

    closesocket(client_fd);
    closesocket(server_fd);
    printf("%s", ANSI_SHOW_CURSOR);
    return 0;
}
