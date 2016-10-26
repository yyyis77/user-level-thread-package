//
// Created by yongyi yang on 10/16/16.
//

#include "uthread.h"

int curthread = 0;
int ifinit = 0;
int uthreadIDmax = 0;
int *returnvalue = 0;


vector<queue<uthread_t*>*> ready_queue;
static queue<uthread_t*> *waitqueue;
static queue<uthread_t*> blockqueue;

vector<uthread_tid_t> tid_table;

int uthread_init(){

    if(ifinit == 1)
    {
        return -1;
    }

    ifinit = 1;

    for(int i = 0; i < 10; i++){
        queue<uthread_t *> *temp = new queue<uthread_t *>;
        ready_queue.push_back(temp);
    }

    uthread_t *thread0 = new uthread_t;
    thread0->state = RUNNING;
    thread0->join = -1;
    thread0->tid = 0;
    thread0->priority = 99;
    thread0->context.uc_stack.ss_size = STACK;
    thread0->context.uc_stack.ss_sp = (char*) malloc(STACK);
    ready_queue[9]->push(thread0);
    tid_table.push_back(thread0->tid);

    //set timer and begin schedule
    set_tick();
    signal(SIGVTALRM, scheduler);

    return 0;
}

int uthread_create(void(*func)(int), int val, int pri){
    uthread_t *thread = new uthread_t;
    if (getcontext(&thread->context) == -1) {
        perror("getcontext error\n");
        return -1;
    }
    thread->tid =++curthread;
    uthreadIDmax++;
    tid_table.push_back(thread->tid);
    thread->state = READY;
    thread->priority = pri;
    thread->context.uc_stack.ss_size = STACK;
    thread->context.uc_stack.ss_sp = (char*) malloc(STACK);
    makecontext(&thread->context, (void(*)())(func), 1, val);
    ready_queue[pri/10]->push(thread);
    printf("create one thread\n");

    return curthread;
}

int uthread_yield(){
    block_sig();
    waitqueue = queue_curr();
    waitqueue->front()->state = READY;
    uthread_t *oldthread = waitqueue->front();
    waitqueue->push(oldthread);
    waitqueue->pop();

    waitqueue = queue_highest();
    waitqueue->front()->state = RUNNING;
    unblock_sig();
    swapcontext(&waitqueue->back()->context, &waitqueue->front()->context);

    return 0;
}

void uthread_exit(void *retval){
    retval = returnvalue;
    int len = blockqueue.size();
    for(int i = 0; i < len; i++){
        uthread_t *temp = blockqueue.front();
        if (temp->join == waitqueue->front()->tid){
            temp->state = READY;
            //printf("%d\n",temp->priority);
            ready_queue[temp->priority/10]->push(temp);
            blockqueue.pop();
            break;
        }
    }
    waitqueue = queue_curr();
    //printf("exit one thread\n");
    waitqueue->front()->state = EXIT;
    uthread_tid_del(waitqueue->front()->tid);
    waitqueue->pop();
    waitqueue = queue_highest();
    if(waitqueue->empty()){
        exit(0);
        }
    waitqueue->front()->state = RUNNING;

    setcontext(&waitqueue->front()->context);
    return;
}

int uthread_join(uthread_tid_t tid, void *retval){
    retval = returnvalue;
    int ifexist = uthread_tid_find(tid);
    if (ifexist == -1){  //have terminated
        return 0;
    }

    int ifjoined = uthread_get(tid);
    if ((tid > uthreadIDmax) || (ifjoined == 0))
        return -1;

    block_sig();
    waitqueue = queue_curr();
    uthread_t *jointhread = waitqueue->front();
    jointhread->state =BLOCKED;
    jointhread->join = tid;
    blockqueue.push(jointhread);
    waitqueue->pop();
    waitqueue = queue_highest();
    waitqueue->front()->state = RUNNING;
    unblock_sig();
    swapcontext(&blockqueue.front()->context,&waitqueue->front()->context);
    return 0;
}


/**********************************  multi queue part  ************************************/
queue<uthread_t *>* queue_curr(){
    int cur=9;
    for(int i = 0; i < 10; i++) {
        if (!ready_queue[i]->empty()) {
            if (ready_queue[i]->front()->state == RUNNING) {
                cur = i;
                break;
            }
        }
    }
   // printf("the running thread is in queue %d\n", cur);
    return ready_queue[cur];
}

queue<uthread_t *>* queue_highest(){
    int cur=9;
    for(int i = 0; i < 10; i++) {
        if (!ready_queue[i]->empty()) {
            cur = i;
            break;
            }
        }
    //printf("the higest ready thread is in queue %d\n", cur);
    return ready_queue[cur];
}

/*********************************  multi thread tid part  ************************************/
int uthread_tid_find(uthread_tid_t tid){
    for(int i = 0; i < tid_table.size(); i++){
        if (tid == tid_table[i])
            return i;
    }
    return -1;
}

void uthread_tid_del(uthread_tid_t tid){
    int index = uthread_tid_find(tid);
    if (index != -1){
        tid_table.erase(tid_table.begin()+index);
    }
    return;
}

int uthread_get(uthread_tid_t tid) {
    if (blockqueue.front() == NULL)
        return -1;
    long len = blockqueue.size();
    for (int i = 0; i < len; i++) {
        uthread_t *temp = blockqueue.front();
        if (temp->tid == tid)
            return 0; //joined
        blockqueue.push(temp);
        blockqueue.pop();
    }
    return 1;
}


/**********************************  timer part  ************************************/
void set_tick() {
    struct itimerval tick;
    tick.it_value.tv_sec = 0;
    tick.it_value.tv_usec = 1000;
    tick.it_interval.tv_sec = 0;
    tick.it_interval.tv_usec = 1000;
    setitimer(ITIMER_VIRTUAL, &tick, NULL);
}

void scheduler(int u){
    uthread_yield();
}

void block_sig(){
    sigprocmask(SIG_BLOCK, &ut_sigset, NULL);
}

void unblock_sig(){
    sigprocmask(SIG_UNBLOCK, &ut_sigset, NULL);
}


/************************************  umutex part  ************************************/
int uthread_mutex_init(uthread_mutex_t *mutex){
    if (mutex->ifinit == 1){
        //printf("init fail: mutex has initialized");
        return -1;
    }

    priority_queue<uthread_t*> waitmutexqueue;
    mutex->wait_mutex_queue = waitmutexqueue;
    mutex->ifinit = 1; //init
    mutex->status = 1;  //unlocked

    return 0;
}

int uthread_mutex_lock(uthread_mutex_t *mutex){
    if(mutex->ifinit != 1){
        //printf("mutex lock fail: mutex didn't initialized");
        return -1;
    }

    block_sig();
    if(mutex->status == 0){
        waitqueue = queue_curr();
        waitqueue->front()->state = SLEEP;
        uthread_t *waitformutex = waitqueue->front();
        mutex->wait_mutex_queue.push(waitformutex);
        waitqueue->pop();
        waitqueue = queue_curr();
        unblock_sig();
        swapcontext(&waitformutex->context, &waitqueue->front()->context);
    } else
    {
        mutex->status = 0;
        unblock_sig();
    }

    return 0;
}

int uthread_mutex_unlock(uthread_mutex_t *mutex) {
    if(mutex->ifinit != 1 || mutex->status == 1)
        return -1;

    block_sig();
    if(mutex->wait_mutex_queue.empty())
    {
        mutex->status = 1;
        unblock_sig();
    } else
    {
        waitqueue = queue_curr();
        mutex->wait_mutex_queue.top()->state = READY;
        uthread_t *getmutex = mutex->wait_mutex_queue.top();
        waitqueue->push(getmutex);
        mutex->wait_mutex_queue.pop();
        unblock_sig();
    }

    return 0;

}


/************************************  usem part  ***********************************/
int usem_init(usem_t *sem, int pshared, unsigned value){
    if(value > SEM_VALUE_MAX || sem->ifinit == 1)
        return -1;

    sem->val = value;
    sem->initial = value;
    sem->sem_state = pshared; //0:shared
    sem->ifinit = 1;
    queue<uthread_t*> waitsemqueue;
    sem->wait_sem_queue = waitsemqueue;
    //printf("usem init successfully\n");
    return 0;
}

int usem_destroy(usem_t *sem){
    if((sem->ifinit == 0) || (sem->initial != sem->val) || sem->sem_state == -1)
        return -1;

    sem->val = 0;
    sem->initial = 0;
    sem->ifinit = 0;
    sem->sem_state = -1; //destroy
    sem->wait_sem_queue.front() = NULL;
    //printf("usem destroy succeessfully\n");
}

int usem_wait(usem_t *sem){
    if((sem->ifinit == 0) || sem->sem_state == -1){
        //printf("usem has destroyed or hasn't init");
        return -1;
    }

    block_sig();
    if(sem->val > 0){
        sem->val--;
       // printf("enter critical section,val now is: %d\n", sem->val);
        unblock_sig();

    } else{
        //call block
        waitqueue = queue_curr();
        uthread_t *waitforsem = waitqueue->front();
        waitforsem->state = SLEEP;
        sem->wait_sem_queue.push(waitforsem);
        waitqueue->pop();
      //  printf("one thread waiting usem\n");
        waitqueue = queue_highest();
        if(waitqueue->front() == NULL){
            unblock_sig();
            return 0;
        }
        waitqueue->front()->state = RUNNING;
        unblock_sig();
        swapcontext(&sem->wait_sem_queue.back()->context, &waitqueue->front()->context);
    }
    return 0;
}

int usem_post(usem_t *sem){
    if((sem->ifinit == 0) || sem->sem_state == -1)
        return -1;

    block_sig();
    if(sem->wait_sem_queue.front() != NULL){
        uthread_t *getsem = sem->wait_sem_queue.front();
        getsem->state = READY;
        ready_queue[getsem->priority/10]->push(getsem);
        sem->wait_sem_queue.pop();
      //  printf("one thread get usem\n");
        unblock_sig();
        return 0;
    } else{
        sem->val++;
        unblock_sig();
        return 0;
    }
}