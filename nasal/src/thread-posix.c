#ifndef _WIN32

#include <pthread.h>
#include "data.h"
#include "thread.h"

#include <stdio.h> // DEBUG

void* naNewLock()
{
    pthread_mutex_t* lock = naAlloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(lock, 0);
    return lock;
}

void naFreeLock(void* lock)
{
    pthread_mutex_destroy((pthread_mutex_t*)lock);
    naFree(lock);
}

void naLock(void* lock)
{
    pthread_mutex_lock((pthread_mutex_t*)lock);
}

void naUnlock(void* lock)
{
    pthread_mutex_unlock((pthread_mutex_t*)lock);
}

struct naSem {
    pthread_mutex_t lock;
    pthread_cond_t cvar;
    int count;
};

void* naNewSem()
{
    struct naSem* sem = naAlloc(sizeof(struct naSem));
    pthread_mutex_init(&sem->lock , 0);
    pthread_cond_init(&sem->cvar, 0);
    sem->count = 0;
    return sem;
}

void naSemDown(void* sh)
{
    struct naSem* sem = (struct naSem*)sh;
    pthread_mutex_lock(&sem->lock);
    while(sem->count <= 0)
        pthread_cond_wait(&sem->cvar, &sem->lock);
    sem->count--;
    pthread_mutex_unlock(&sem->lock);
}

void naSemUp(void* sh)
{
    struct naSem* sem = (struct naSem*)sh;
    pthread_mutex_lock(&sem->lock);
    sem->count++;
    pthread_cond_signal(&sem->cvar);
    pthread_mutex_unlock(&sem->lock);
}

void naSemUpAll(void* sh, int count)
{
    struct naSem* sem = (struct naSem*)sh;
    pthread_mutex_lock(&sem->lock);
    sem->count = count;
    pthread_cond_broadcast(&sem->cvar);
    pthread_mutex_unlock(&sem->lock);
}

void naFreeSem(void* sh)
{
    struct naSem* sem = (struct naSem*)sh;
    pthread_mutex_destroy(&sem->lock);
    pthread_cond_destroy(&sem->cvar);
    naFree(sh);
}

#endif
