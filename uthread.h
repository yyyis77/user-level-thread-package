//
// Created by yongyi yang on 10/16/16.
//

#ifndef P3YY4AV_UTHREAD_H
#define P3YY4AV_UTHREAD_H

#include <queue>
using namespace std;

#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define STACK (1024 * 1024)

typedef int uthread_tid_t;

struct uthread_t {
    uthread_tid_t tid;
    int state;
    int priority;
    int join;
    char *stack;
    ucontext_t context ;

};

enum STATE {RUNNING, READY, SLEEP, BLOCKED, EXIT};

int uthread_init();
int uthread_create(void(*func)(int), int val, int pri);
int uthread_yield();
void uthread_exit(void *retval);
int uthread_join(uthread_tid_t tid, void *retval);

/**********************************  timer part  ************************************/
void set_tick();
void scheduler(int u);

static sigset_t ut_sigset;
void block_sig();
void unblock_sig();


/**********************************  multi queue part  ************************************/
queue<uthread_t *>* queue_curr();
queue<uthread_t *>* queue_highest();


/**********************************  multi thread tid part  ************************************/
int uthread_tid_find(uthread_tid_t tid);
void uthread_tid_del(uthread_tid_t tid);
int uthread_get(uthread_tid_t tid);


/**********************************  umutex part  ************************************/
struct uthread_mutex_t {
    int ifinit;
    int status;
    priority_queue<uthread_t*> wait_mutex_queue;
};

int uthread_mutex_init(uthread_mutex_t *mutex);
int uthread_mutex_lock(uthread_mutex_t *mutex);
int uthread_mutex_unlock(uthread_mutex_t *mutex);


/************************************  usem part  ***********************************/
#define SEM_VALUE_MAX 65536

struct usem_t{
    int val;
    int initial;
    int sem_state;
    int ifinit;
    queue<uthread_t*> wait_sem_queue;
};

int usem_init(usem_t *sem, int pshared, unsigned value);
int usem_destroy(usem_t *sem);
int usem_wait(usem_t *sem);
int usem_post(usem_t *sem);

#endif //P3YY4AV_UTHREAD_H
