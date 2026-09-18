#include <stdio.h>
#include <stdbool.h>
extern bool joey_execute_job(const char* job_file);
int main(int argc, char** argv) {
    if(argc < 2) { printf("Usage: joey_cli <job.json>\n"); return 1; }
    joey_execute_job(argv[1]);
    return 0;
}
