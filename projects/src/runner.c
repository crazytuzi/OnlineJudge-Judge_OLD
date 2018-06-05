#define _GNU_SOURCE
#define _POSIX_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>
#include <pthread.h>
#include <wait.h>
#include <errno.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/mman.h>
#include<sys/stat.h>
#include<fcntl.h>

#include "runner.h"
#include "killer.h"
#include "child.h"
#include "logger.h"



int equalStr(const char *s, const char *s2) {
    while (*s && *s2) {
        if (*s++ != *s2++) {
            return 1;
        }
    }

    return 0;
}


#define RETURN(rst) {*result = rst;return 0;}


int checkDiff(struct config *_config,int *result) {
    int rightout_fd,userout_fd;
    /*
    修改fopen函数为open函数
    */
    /*
    FILE * rightout_fp;
    FILE * userout_fp;
    rightout_fp = fopen(_config->answer_path, "r");
    rightout_fd = fileno(rightout_fp);
    userout_fp = fopen(_config->output_path, "r");
    userout_fd = fileno(userout_fp);
    */
    rightout_fd=open(_config->answer_path,O_RDONLY);
    userout_fd=open(_config->output_path,O_RDONLY);
    char *userout, *rightout;
    const char *cuser, *cright, *end_user, *end_right;

    off_t userout_len, rightout_len;
    userout_len = lseek(userout_fd, 0, SEEK_END);
    rightout_len = lseek(rightout_fd, 0, SEEK_END);

    if (userout_len == -1 || rightout_len == -1)
        //RAISE1("lseek failure");

    if (userout_len >= _config->max_output_size)
        RETURN(OUTPUT_LIMIT_EXCEEDED);

    lseek(userout_fd, 0, SEEK_SET);
    lseek(rightout_fd, 0, SEEK_SET);

    if ((userout_len && rightout_len) == 0) {
        if (userout_len || rightout_len)
            RETURN(WRONG_ANSWER)
        else
            RETURN(SUCCESS)
    }

    if ((userout = (char*) mmap(NULL, userout_len, PROT_READ | PROT_WRITE,
            MAP_PRIVATE, userout_fd, 0)) == MAP_FAILED) {
        //RAISE1("mmap userout filure");
    }

    if ((rightout = (char*) mmap(NULL, rightout_len, PROT_READ | PROT_WRITE,
            MAP_PRIVATE, rightout_fd, 0)) == MAP_FAILED) {
        munmap(userout, userout_len);
        //RAISE1("mmap right filure");
    }

    if ((userout_len == rightout_len) && equalStr(userout, rightout) == 0) {
        munmap(userout, userout_len);
        munmap(rightout, rightout_len);
        RETURN(SUCCESS);
    }

    cuser = userout;
    cright = rightout;
    end_user = userout + userout_len;
    end_right = rightout + rightout_len;
    while ((cuser < end_user) && (cright < end_right)) {
        while ((cuser < end_user)
                && (*cuser == ' ' || *cuser == '\n' || *cuser == '\r'
                        || *cuser == '\t'))
            cuser++;
        while ((cright < end_right)
                && (*cright == ' ' || *cright == '\n' || *cright == '\r'
                        || *cright == '\t'))
            cright++;
        if (cuser == end_user || cright == end_right)
            break;
        if (*cuser != *cright)
            break;
        cuser++;
        cright++;
    }
    while ((cuser < end_user)
            && (*cuser == ' ' || *cuser == '\n' || *cuser == '\r'
                    || *cuser == '\t'))
        cuser++;
    while ((cright < end_right)
            && (*cright == ' ' || *cright == '\n' || *cright == '\r'
                    || *cright == '\t'))
        cright++;
    if (cuser == end_user && cright == end_right) {
        munmap(userout, userout_len);
        munmap(rightout, rightout_len);
        RETURN(PRESENTATION_ERROR);
    }

    munmap(userout, userout_len);
    munmap(rightout, rightout_len);
    close(rightout_fd);
    close(userout_fd);
    /*
    fclose(rightout_fp);
    fclose(userout_fp);
    */
    RETURN(WRONG_ANSWER);
}


//初始化结果
void init_result(struct result *_result) {
    _result->result = _result->error = SUCCESS;
    _result->cpu_time = _result->real_time = _result->signal = _result->exit_code = 0;
    _result->memory = 0;
}


void run(struct config *_config, struct result *_result) {
    // init log fp
    FILE *log_fp = log_open(_config->log_path);

    // init result
    init_result(_result);


    //能不能不用root权限
    //如果docker root还行,物理机的话root权限,第一麻烦,第二安全性
    // check whether current user is root
    uid_t uid = getuid();
    if (uid != 0) {
        ERROR_EXIT(ROOT_REQUIRED);
    }

    //不满足的条件可否添加或者删除
    // check args
    if ((_config->max_cpu_time < 1 && _config->max_cpu_time != UNLIMITED) ||
        (_config->max_real_time < 1 && _config->max_real_time != UNLIMITED) ||
        (_config->max_stack < 1) ||
        (_config->max_memory < 1 && _config->max_memory != UNLIMITED) ||
        (_config->max_process_number < 1 && _config->max_process_number != UNLIMITED) ||
        (_config->max_output_size < 1 && _config->max_output_size != UNLIMITED)) {
        ERROR_EXIT(INVALID_CONFIG);
    }

    //记录当前时间,计算real_time
    // record current time
    struct timeval start, end;
    gettimeofday(&start, NULL);

    //fork和vfork
    /*
    返回一个大于0的值给父进程
    返回0给子进程
    返回其他值说明fork失败了
    */
    pid_t child_pid = fork();

    // pid < 0 shows clone failed
    if (child_pid < 0) {
        ERROR_EXIT(FORK_FAILED);
    }
    else if (child_pid == 0) {
        child_process(log_fp, _config);
    }
    else if (child_pid > 0){
        // create new thread to monitor process running time
        pthread_t tid = 0;
        //时间设置限制
        if (_config->max_real_time != UNLIMITED) {
            struct timeout_killer_args killer_args;

            killer_args.timeout = _config->max_real_time;
            killer_args.pid = child_pid;
            /*
            pthread_create是类Unix操作系统（Unix、Linux、Mac OS X等）的创建线程的函数。
            它的功能是创建线程（实际上就是确定调用该线程函数的入口点），在线程创建以后，就开始运行相关的线程函数
            第一个参数为指向线程标识符的指针。
            第二个参数用来设置线程属性。
            第三个参数是线程运行函数的起始地址。
            最后一个参数是运行函数的参数。
            */
            if (pthread_create(&tid, NULL, timeout_killer, (void *) (&killer_args)) != 0) {
                kill_pid(child_pid);
                ERROR_EXIT(PTHREAD_FAILED);
            }
        }

        int status;
        struct rusage resource_usage;

        // wait for child process to terminate
        // on success, returns the process ID of the child whose state has changed;
        // On error, -1 is returned.
        if (wait4(child_pid, &status, WSTOPPED, &resource_usage) == -1) {
            kill_pid(child_pid);
            ERROR_EXIT(WAIT_FAILED);
        }
        // get end time
        gettimeofday(&end, NULL);
        _result->real_time = (int) (end.tv_sec * 1000 + end.tv_usec / 1000 - start.tv_sec * 1000 - start.tv_usec / 1000);



        //这里为什么是先检查max_real_time是不是没有限制
        //直接取消不行吗
        // process exited, we may need to cancel timeout killer thread
        if (_config->max_real_time != UNLIMITED) {
            if (pthread_cancel(tid) != 0) {
                // todo logging
            };
        }


        //这部分同lorun那个
        if (WIFSIGNALED(status) != 0) {
            _result->signal = WTERMSIG(status);
        }

        if(_result->signal == SIGUSR1) {
            _result->result = SYSTEM_ERROR;
        }
        else {
            _result->exit_code = WEXITSTATUS(status);
            //计算时间的方式是否要改变一下
            _result->cpu_time = (int) (resource_usage.ru_utime.tv_sec * 1000 +
                                       resource_usage.ru_utime.tv_usec / 1000);
            _result->memory = resource_usage.ru_maxrss * 1024;

            if (_result->exit_code != 0) {
                _result->result = RUNTIME_ERROR;
            }

            if (_result->signal == SIGSEGV) {
                // !=  UNLIMITED这个很重要 特殊情况下
                if (_config->max_memory != UNLIMITED && _result->memory > _config->max_memory) {
                    _result->result = MEMORY_LIMIT_EXCEEDED;
                }
                else {
                    _result->result = RUNTIME_ERROR;
                }
            }
            else {
                if (_result->signal != 0) {
                    _result->result = RUNTIME_ERROR;
                }
                if (_config->max_memory != UNLIMITED && _result->memory > _config->max_memory) {
                    _result->result = MEMORY_LIMIT_EXCEEDED;
                }
                if (_config->max_real_time != UNLIMITED && _result->real_time > _config->max_real_time) {
                    _result->result = REAL_TIME_LIMIT_EXCEEDED;
                }
                if (_config->max_cpu_time != UNLIMITED && _result->cpu_time > _config->max_cpu_time) {
                    _result->result = CPU_TIME_LIMIT_EXCEEDED;
                }
            }
        }
        //先判断是否目前为AC
        //判断WA、,OLE,PE的情况
        if (_result->result == SUCCESS){
            int result;
            checkDiff(_config, &result);
            _result->result = result;
        }
        log_close(log_fp);
    }
}
