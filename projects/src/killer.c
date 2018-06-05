#define _POSIX_SOURCE
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include "killer.h"


int kill_pid(pid_t pid) {
    return kill(pid, SIGKILL);
}


void *timeout_killer(void *timeout_killer_args) {
    // this is a new thread, kill the process if timeout
    pid_t pid = ((struct timeout_killer_args *)timeout_killer_args)->pid;
    int timeout = ((struct timeout_killer_args *)timeout_killer_args)->timeout;
    // On success, pthread_detach() returns 0; on error, it returns an error number.
    /*
    创建一个线程默认的状态是joinable, 如果一个线程结束运行但没有被join,
    则它的状态类似于进程中的Zombie Process,即还有一部分资源没有被回收（退出状态码），
    所以创建线程者应该pthread_join来等待线程运行结束，
    并可得到线程的退出代码，回收其资源（类似于wait,waitpid)
    但是调用pthread_join(pthread_id)后，
    如果该线程没有运行结束，调用者会被阻塞，
    在有些情况下我们并不希望如此，
    比如在Web服务器中当主线程为每个新来的链接创建一个子线程进行处理的时候，
    主线程并不希望因为调用pthread_join而阻塞（因为还要继续处理之后到来的链接），
    这时可以在子线程中加入代码
    pthread_detach(pthread_self())
    或者父线程调用
    pthread_detach(thread_id)（非阻塞，可立即返回）
    这将该子线程的状态设置为detached,则该线程运行结束后会自动释放所有资源。
    */
    if (pthread_detach(pthread_self()) != 0) {
        kill_pid(pid);
        return NULL;
    }
    // usleep can't be used, for time args must < 1000ms
    // this may sleep longer that expected, but we will have a check at the end
    /*
    一般情况下，延迟时间数量级是秒的时候，尽可能使用sleep()函数。
    如果延迟时间为几十毫秒（1ms = 1000us），或者更小，
    尽可能使用usleep()函数。这样才能最佳的利用CPU时间
    */
    if (sleep((unsigned int)((timeout + 1000) / 1000)) != 0) {
        kill_pid(pid);
        return NULL;
    }
    if (kill_pid(pid) != 0) {
        return NULL;
    }
    return NULL;
}