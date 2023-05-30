#ifndef CTHREADS_H
#define CTHREADS_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "cthreads.h"

#if _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

struct cthreads_thread {
    #ifdef _WIN32
        HANDLE wThread;
    #else
        pthread_t pThread;
    #endif
};

struct cthreads_thread_attr {
    int detachstate;
    size_t guardsize;
    int inheritsched;
    int schedpolicy;
    int scope;
    size_t stack;
    void *stackaddr;
    size_t stacksize;
    int dwCreationFlags;
};

struct cthreads_mutex {
    #ifdef _WIN32
        HANDLE wMutex;
    #else
        pthread_mutex_t pMutex;
    #endif
};

struct cthreads_mutex_attr {
    int pshared;
    int type;
    int protocol;
    int robust;
    int prioceiling;
    int bInitialOwner;
    char *lpName;
};

struct cthreads_cond {
    #ifdef _WIN32
        HANDLE wCond;
    #else
        pthread_cond_t pCond;
    #endif
};

struct cthreads_cond_attr {
    int pshared;
    int clock;
    int bManualReset;
    int bInitialState;
    char *lpName;
};

struct cthreads_args {
  void *(*func)(void *data);
  void *data;
};

#if _WIN32
DWORD WINAPI __cthreads_winthreads_function_wrapper(void *data) {
    struct cthreads_args *args = data;
    args->func(args->data);

    free(data);

    return TRUE;
}
#else
void *__cthreads_pthread_function_wrapper(void *data) {
    struct cthreads_args *args = data;
    args->func(args->data);

    free(data);

    return NULL;
}
#endif

int cthreads_thread_create(struct cthreads_thread *thread, struct cthreads_thread_attr *attr, void *(*func)(void *data), void *data) {
    #ifdef _WIN32
        struct cthreads_args *args = malloc(sizeof(struct cthreads_args));
        args->func = func;
        args->data = data;

        if (attr) thread->wThread = CreateThread(NULL, attr->stacksize ? attr->stacksize : 0,
                                                 __cthreads_winthreads_function_wrapper, args, 
                                                 attr->dwCreationFlags ? (DWORD)attr->dwCreationFlags : 0, NULL);
        else thread->wThread = CreateThread(NULL, 0, __cthreads_winthreads_function_wrapper, args, 0, NULL);

        return 0;
    #else
        pthread_t pthread;
        pthread_attr_t pAttr;
        int res;

        struct cthreads_args *args = malloc(sizeof(struct cthreads_args));
        args->func = func;
        args->data = data;


        if (attr) {
            if (attr->detachstate) pthread_attr_setdetachstate(&pAttr, attr->detachstate);
            if (attr->guardsize) pthread_attr_setguardsize(&pAttr, attr->guardsize);
            if (attr->inheritsched) pthread_attr_setinheritsched(&pAttr, attr->inheritsched);
            if (attr->schedpolicy) pthread_attr_setschedpolicy(&pAttr, attr->schedpolicy);
            if (attr->scope) pthread_attr_setscope(&pAttr, attr->scope);
            if (attr->stack) pthread_attr_setstack(&pAttr, attr->stackaddr, attr->stack);
            if (attr->stacksize) pthread_attr_setstacksize(&pAttr, attr->stacksize);
        }

        res = pthread_create(&pthread, NULL, __cthreads_pthread_function_wrapper, args);

        thread->pThread = pthread;

        return res;
    #endif
}

int cthreads_thread_detach(struct cthreads_thread *thread) {
    #ifdef _WIN32
        return CloseHandle(thread->wThread);
    #else
        return pthread_detach(thread->pThread);
    #endif
}

void cthreads_thread_close(void *code) {
    #ifdef _WIN32
        ExitThread((DWORD)code);
    #else
        pthread_exit(code);
    #endif
}

int cthreads_mutex_init(struct cthreads_mutex *mutex, struct cthreads_mutex_attr *attr) {
    #ifdef _WIN32
        HANDLE wMutex;
        if (attr) wMutex= CreateMutex(NULL, attr->bInitialOwner ? TRUE : FALSE, 
                                      attr->lpName ? (LPCSTR)attr->lpName : NULL);
        else wMutex= CreateMutex(NULL, FALSE, NULL);

        if (wMutex == NULL) return 1;
        else {
          mutex->wMutex = wMutex;
          return 0;
        }
    #else
        pthread_mutexattr_t pAttr;
        if (attr) {
            if (attr->pshared) pthread_mutexattr_setpshared(&pAttr, attr->pshared);
            if (attr->type) pthread_mutexattr_settype(&pAttr, attr->type);
            if (attr->protocol) pthread_mutexattr_setprotocol(&pAttr, attr->protocol);
            #ifdef __linux__
                if (attr->robust) pthread_mutexattr_setrobust(&pAttr, attr->robust);
            #elif __FreeBSD__
                if (attr->robust) pthread_mutexattr_setrobust(&pAttr, attr->robust);
            #endif
            if (attr->prioceiling) pthread_mutexattr_setprioceiling(&pAttr, attr->prioceiling);
        }
        return pthread_mutex_init(&mutex->pMutex, attr ? &pAttr : NULL);
    #endif
}

int cthreads_mutex_lock(struct cthreads_mutex *mutex) {
    #ifdef _WIN32
        DWORD ret = WaitForSingleObject(mutex->wMutex, INFINITE);

        if (ret == WAIT_OBJECT_0) return 0;
        return 1;
    #else
        return pthread_mutex_lock(&mutex->pMutex);
    #endif
}

int cthreads_mutex_trylock(struct cthreads_mutex *mutex) {
    #ifdef _WIN32
        DWORD ret = WaitForSingleObject(mutex->wMutex, 0);

        if (ret == WAIT_OBJECT_0) return 0;
        return 1;
    #else
        return pthread_mutex_trylock(&mutex->pMutex);
    #endif
}

int cthreads_mutex_unlock(struct cthreads_mutex *mutex) {
    #ifdef _WIN32
        return ReleaseMutex(mutex->wMutex) == 0 ? 1 : 0;
    #else
        return pthread_mutex_unlock(&mutex->pMutex);
    #endif
}

int cthreads_mutex_destroy(struct cthreads_mutex *mutex) {
    #ifdef _WIN32
        return CloseHandle(mutex->wMutex) == 0 ? 1 : 0;
    #else
        return pthread_mutex_destroy(&mutex->pMutex);
    #endif
}

int cthreads_cond_init(struct cthreads_cond *cond, struct cthreads_cond_attr *attr) {
    #ifdef _WIN32
        if (attr) cond->wCond = CreateEvent(NULL, attr->bManualReset ? TRUE : FALSE, 
                                            attr->bInitialState ? TRUE : FALSE,
                                            attr->lpName ? (LPTSTR)attr->lpName : NULL);
        else cond->wCond = CreateEvent(NULL, FALSE, FALSE, NULL);

        if (cond->wCond == NULL) return 1;
        return 0;
    #else
        pthread_condattr_t pAttr;
        if (attr) {
            if (attr->pshared) pthread_condattr_setpshared(&pAttr, attr->pshared);
            if (attr->clock) pthread_condattr_setclock(&pAttr, attr->clock);
        }
        return pthread_cond_init(&cond->pCond, attr ? &pAttr : NULL);
    #endif
}

int cthreads_cond_signal(struct cthreads_cond *cond) {
    #ifdef _WIN32
        return SetEvent(cond->wCond) == 0 ? 1 : 0;
    #else
        return pthread_cond_signal(&cond->pCond);
    #endif
}

int cthreads_cond_broadcast(struct cthreads_cond *cond) {
    #ifdef _WIN32
        return SetEvent(cond->wCond) == 0 ? 1 : 0;
    #else
        return pthread_cond_broadcast(&cond->pCond);
    #endif
}

int cthreads_cond_destroy(struct cthreads_cond *cond) {
    #ifdef _WIN32
        return CloseHandle(cond->wCond) == 0 ? 1 : 0;
    #else
        return pthread_cond_destroy(&cond->pCond);
    #endif
}

int cthreads_cond_wait(struct cthreads_cond *cond, struct cthreads_mutex *mutex) {
    #ifdef _WIN32
        if (cthreads_mutex_unlock(mutex) == 1) return 1;
        if (WaitForSingleObject(cond->wCond, INFINITE) == WAIT_FAILED) return 1;
        return cthreads_mutex_lock(mutex) == 1 ? 1 : 0;
    #else
        return pthread_cond_wait(&cond->pCond, &mutex->pMutex);
    #endif
}

int cthreads_join(struct cthreads_thread *thread, void *code) {
    #ifdef _WIN32
        if (WaitForSingleObject(thread->wThread, INFINITE) == WAIT_FAILED) return 0;
        return GetExitCodeThread(thread->wThread, (LPDWORD)&code) == 0 ? 1 : 0;
    #else
        return pthread_join(thread->pThread, code ? &code : NULL);
    #endif
}

int cthreads_equal(struct cthreads_thread thread1, struct cthreads_thread thread2) {
    #ifdef _WIN32
        return thread1.wThread == thread2.wThread;
    #else
        return pthread_equal(thread1.pThread, thread2.pThread);
    #endif
}

struct cthreads_thread cthreads_self(void) {
    struct cthreads_thread t;

    #ifdef _WIN32
        t.wThread = GetCurrentThread();
    #else
        t.pThread = pthread_self();
    #endif

    return t;
}

#ifdef __cplusplus
}
#endif

#endif
