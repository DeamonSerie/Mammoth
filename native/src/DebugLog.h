#pragma once
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <cstdlib>
#include <unistd.h>

class DebugLog {
    static FILE* s_file;
public:
    static void init(const char* path = nullptr) {
        if (s_file) return;
        char buf[512];
        if (path) {
            snprintf(buf, sizeof(buf), "%s", path);
        } else {
            time_t t = time(nullptr);
            struct tm* tm = localtime(&t);
            snprintf(buf, sizeof(buf),
                "/home/knw102/Mammoth/DebugLogs/mammoth_%04d%02d%02d_%02d%02d%02d_%d.log",
                tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                tm->tm_hour, tm->tm_min, tm->tm_sec, (int)getpid());
        }
        s_file = fopen(buf, "w");
        if (s_file) printf("[DebugLog] Writing to %s\n", buf);
    }
    static void shutdown() {
        if (s_file) { fclose(s_file); s_file = nullptr; }
    }
    static void flush() { if (s_file) fflush(s_file); }
    static void log(const char* fmt, ...) {
        init();
        if (!s_file) return;
        va_list args;
        va_start(args, fmt);
        vfprintf(s_file, fmt, args);
        va_end(args);
        fprintf(s_file, "\n");
        fflush(s_file);
    }
};
