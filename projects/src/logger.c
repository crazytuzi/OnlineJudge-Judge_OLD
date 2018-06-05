#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/file.h>

#include "logger.h"

#define log_buffer_size 8192


FILE *log_open(const char *filename) {
    //追加
    FILE *log_fp = fopen(filename, "a");
    if (log_fp == NULL) {
        fprintf(stderr, "can not open log file %s", filename);
    }
    return log_fp;
}


void log_close(FILE *log_fp) {
    if (log_fp != NULL) {
        fclose(log_fp);
    }
}


//在C中，当我们无法列出传递函数的所有实参的类型和数目时,可以用省略号指定参数表
void log_write(int level, const char *source_filename, const int line, const FILE *log_fp, const char *fmt, ...) {
    char LOG_LEVEL_NOTE[][10] = {"FATAL", "WARNING", "INFO", "DEBUG"};
    if (log_fp == NULL) {
        fprintf(stderr, "can not open log file");
        return;
    }
    static char buffer[log_buffer_size];
    static char log_buffer[log_buffer_size];
    static char datetime[100];
    static char line_str[20];
    static time_t now;
    now = time(NULL);
    //第二个参数 sizeof(str)
    strftime(datetime, 99, "%Y-%m-%d %H:%M:%S", localtime(&now));
    snprintf(line_str, 19, "%d", line);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(log_buffer, log_buffer_size, fmt, ap);
    va_end(ap);

    int count = snprintf(buffer, log_buffer_size,
                         "%s [%s] [%s:%s]%s\n",
                         LOG_LEVEL_NOTE[level], datetime, source_filename, line_str, log_buffer);
    // fprintf(stderr, "%s", buffer);
    //fileno()用来取得参数stream指定的文件流所使用的文件描述符
    int log_fd = fileno((FILE *) log_fp);
    /*
    flock()会依参数operation所指定的方式对参数fd所指的文件做各种锁定或解除锁定的动作。
    此函数只能锁定整个文件，无法锁定文件的某一区域。
    */
    //LOCK_EX 建立互斥锁定。一个文件同时只有一个互斥锁定。
    //LOCK_UN 解除文件锁定状态。
    //返回值 返回0表示成功，若有错误则返回-1，错误代码存于errno。
    if (flock(log_fd, LOCK_EX) == 0) {
        if (write(log_fd, buffer, (size_t) count) < 0) {
            fprintf(stderr, "write error");
            return;
        }
        flock(log_fd, LOCK_UN);
    }
    else {
        fprintf(stderr, "flock error");
        return;
    }
}