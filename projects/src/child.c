#define _DEFAULT_SOURCE
#define _POSIX_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
/*
stdarg.h是C语言中C标准函数库的头文件，
stdarg是由standard（标准） arguments（参数）简化而来，
主要目的为让函数能够接收可变参数。
*/
#include <stdarg.h>
#include <signal.h>
/*
unistd.h 是 C 和 C++ 程序设计语言中提供对 POSIX 操作系统 API 的访问功能的头文件的名称。
*/
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
/*
setgroups()在<grp.h>
*/
#include <grp.h>
/*
dlfcn.h : Linux动态库的显式调用
*/
#include <dlfcn.h>
#include <errno.h>
/*
任务调度相关函数的头文件
*/
#include <sched.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
/*
挂载文件系统
*/
#include <sys/mount.h>

#include "runner.h"
#include "child.h"
#include "logger.h"
#include "rules/seccomp_rules.h"

#include "killer.h"


//关闭文件
void close_file(FILE *fp, ...) {
    va_list args;
    /*
    va_start，函数名称，读取可变参数的过程其实就是在堆栈中，
    使用指针,遍历堆栈段中的参数列表,从低地址到高地址一个一个地把参数内容读出来的过程·
    */
    va_start(args, fp);

    if (fp != NULL) {
        fclose(fp);
    }
    /*
    由于在C语言中没有函数重载,
    解决不定数目函数参数问题变得比较麻烦；
    即使采用C++,如果参数个数不能确定，
    也很难采用函数重载.
    对这种情况，有些人采用指针参数来解决问题。

    “C语言”可变数目变元
    宏va_arg()、va_start()和va_end()一起使用，
    便可以完成向函数传入数目可变的变元操作。
    取可变数目变元的典型例子是函数printf()。
    类型va_list是在<stdarg.h>中定义的。
    */
    va_end(args);
}


void child_process(FILE *log_fp, struct config *_config) {
    FILE *input_file = NULL, *output_file = NULL, *error_file = NULL;

    /*
    RLIMIT_STACK 最大的进程堆栈，以字节为单位
    */
    if (_config->max_stack != UNLIMITED) {
        struct rlimit max_stack;
        max_stack.rlim_cur = max_stack.rlim_max = (rlim_t) (_config->max_stack);
        if (setrlimit(RLIMIT_STACK, &max_stack) != 0) {
            CHILD_ERROR_EXIT(SETRLIMIT_FAILED);
        }
    }

    /*
    RLIMIT_AS 进程的最大虚内存空间，字节为单位
    */
    // set memory limit
    if (_config->max_memory != UNLIMITED) {
        struct rlimit max_memory;
        max_memory.rlim_cur = max_memory.rlim_max = (rlim_t) (_config->max_memory) * 2;
        if (setrlimit(RLIMIT_AS, &max_memory) != 0) {
            CHILD_ERROR_EXIT(SETRLIMIT_FAILED);
        }
    }


    /*
    RLIMIT_CPU 最大允许的CPU使用时间，秒为单位。
    当进程达到软限制，内核将给其发送SIGXCPU信号，
    这一信号的默认行为是终止进程的执行。
    然而，可以捕捉信号，处理句柄可将控制返回给主程序。
    如果进程继续耗费CPU时间，核心会以每秒一次的频率给其发送SIGXCPU信号，
    直到达到硬限制，那时将给进程发送 SIGKILL信号终止其执行。
    */
    // set cpu time limit (in seconds)
    if (_config->max_cpu_time != UNLIMITED) {
        struct rlimit max_cpu_time;
        max_cpu_time.rlim_cur = max_cpu_time.rlim_max = (rlim_t) ((_config->max_cpu_time + 1000) / 1000);
        if (setrlimit(RLIMIT_CPU, &max_cpu_time) != 0) {
            CHILD_ERROR_EXIT(SETRLIMIT_FAILED);
        }
    }

    /*
    RLIMIT_NPROC 用户可拥有的最大进程数
    */
    // set max process number limit
    if (_config->max_process_number != UNLIMITED) {
        struct rlimit max_process_number;
        max_process_number.rlim_cur = max_process_number.rlim_max = (rlim_t) _config->max_process_number;
        if (setrlimit(RLIMIT_NPROC, &max_process_number) != 0) {
            CHILD_ERROR_EXIT(SETRLIMIT_FAILED);
        }
    }


    /*
    RLIMIT_FSIZE 进程可建立的文件的最大长度。
    如果进程试图超出这一限制时，核心会给其发送SIGXFSZ信号，
    默认情况下将终止进程的执行
    */
    // set max output size limit
    if (_config->max_output_size != UNLIMITED) {
        struct rlimit max_output_size;
        max_output_size.rlim_cur = max_output_size.rlim_max = (rlim_t ) _config->max_output_size;
        if (setrlimit(RLIMIT_FSIZE, &max_output_size) != 0) {
            CHILD_ERROR_EXIT(SETRLIMIT_FAILED);
        }
    }

    if (_config->input_path != NULL) {
        input_file = fopen(_config->input_path, "r");
        if (input_file == NULL) {
            CHILD_ERROR_EXIT(DUP2_FAILED);
        }
        // redirect file -> stdin
        // On success, these system calls return the new descriptor.
        // On error, -1 is returned, and errno is set appropriately.
        if (dup2(fileno(input_file), fileno(stdin)) == -1) {
            // todo log
            CHILD_ERROR_EXIT(DUP2_FAILED);
        }
    }

    if (_config->output_path != NULL) {
        output_file = fopen(_config->output_path, "w");
        if (output_file == NULL) {
            CHILD_ERROR_EXIT(DUP2_FAILED);
        }
        // redirect stdout -> file
        if (dup2(fileno(output_file), fileno(stdout)) == -1) {
            CHILD_ERROR_EXIT(DUP2_FAILED);
        }
    }


    if (_config->error_path != NULL) {
        // if outfile and error_file is the same path, we use the same file pointer
        if (_config->output_path != NULL && strcmp(_config->output_path, _config->error_path) == 0) {
            error_file = output_file;
        }
        else {
            error_file = fopen(_config->error_path, "w");
            if (error_file == NULL) {
                // todo log
                CHILD_ERROR_EXIT(DUP2_FAILED);
            }
        }
        // redirect stderr -> file
        if (dup2(fileno(error_file), fileno(stderr)) == -1) {
            // todo log
            CHILD_ERROR_EXIT(DUP2_FAILED);
        }
    }

    // set gid
    /*
    setuid 和setgid位是让普通用户可以以root用户的角色运行只有root帐号才能运行的程序或命令。
    */
    /*
    头文件：#include <grp.h>
    定义函数：int setgroups(size_t size, const gid_t * list);
    函数说明：setgroups()用来将list 数组中所标明的组加入到目前进程的组设置中. 参数size 为list()的gid_t 数目, 最大值为NGROUP(32)。
    返回值：设置成功则返回0, 如有错误则返回-1.
    错误代码：
    EFAULT：参数list 数组地址不合法.
    EPERM：权限不足, 必须是root 权限
    EINVAL：参数size 值大于NGROUP(32).
    */
    gid_t group_list[] = {_config->gid};
    if (_config->gid != -1 && (setgid(_config->gid) == -1 || setgroups(sizeof(group_list) / sizeof(gid_t), group_list) == -1)) {
        CHILD_ERROR_EXIT(SETUID_FAILED);
    }

    // set uid
    if (_config->uid != -1 && setuid(_config->uid) == -1) {
        CHILD_ERROR_EXIT(SETUID_FAILED);
    }

    // load seccomp
    if (_config->seccomp_rule_name != NULL) {
        if (strcmp("c_cpp", _config->seccomp_rule_name) == 0) {
            if (c_cpp_seccomp_rules(_config) != SUCCESS) {
                CHILD_ERROR_EXIT(LOAD_SECCOMP_FAILED);
            }
        }
        else if (strcmp("general", _config->seccomp_rule_name) == 0) {
            if (general_seccomp_rules(_config) != SUCCESS ) {
                CHILD_ERROR_EXIT(LOAD_SECCOMP_FAILED);
            }
        }
        // other rules
        else {
            // rule does not exist
            CHILD_ERROR_EXIT(LOAD_SECCOMP_FAILED);
        }
    }
    /*
    execve（执行文件）在父进程中fork一个子进程，
    在子进程中调用exec函数启动新的程序。exec函数一共有六个，
    其中execve为内核级系统调用，其他（execl，execle，execlp，execv，execvp）都是调用execve的库函数。
    */
    execve(_config->exe_path, _config->args, _config->env);
    CHILD_ERROR_EXIT(EXECVE_FAILED);
}
