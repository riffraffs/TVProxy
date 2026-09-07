/*
 ============================================================================
 Name        : hev-jni.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2019 - 2023 hev
 Description : Jave Native Interface
 ============================================================================
 */

#ifdef ANDROID

#include <jni.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "hev-main.h"

#include "hev-jni.h"

/* clang-format off */
#ifndef PKGNAME
#define PKGNAME hev/htproxy
#endif
#ifndef CLSNAME
#define CLSNAME TProxyService
#endif
/* clang-format on */

#define STR(s) STR_ARG (s)
#define STR_ARG(c) #c
#define N_ELEMENTS(arr) (sizeof (arr) / sizeof ((arr)[0]))

typedef struct _ThreadData ThreadData;

struct _ThreadData
{
    char *path;
    int fd;
};

static int is_working;
static pthread_t work_thread;
static pthread_mutex_t mutex;

static void native_start_service (JNIEnv *env, jobject thiz, jstring conig_path,
                                  jint fd);
static void native_stop_service (JNIEnv *env, jobject thiz);

static JNINativeMethod native_methods[] = {
    { "TProxyStartService", "(Ljava/lang/String;I)V",
      (void *)native_start_service },
    { "TProxyStopService", "()V", (void *)native_stop_service },
};

jint
JNI_OnLoad (JavaVM *vm, void *reserved)
{
    JNIEnv *env = NULL;
    jclass klass;

    if (JNI_OK != (*vm)->GetEnv (vm, (void **)&env, JNI_VERSION_1_4)) {
        return 0;
    }

    klass = (*env)->FindClass (env, STR (PKGNAME) "/" STR (CLSNAME));
    (*env)->RegisterNatives (env, klass, native_methods,
                             N_ELEMENTS (native_methods));
    (*env)->DeleteLocalRef (env, klass);

    pthread_mutex_init (&mutex, NULL);

    return JNI_VERSION_1_4;
}

static void *
thread_handler (void *data)
{
    ThreadData *tdata = data;

    hev_socks5_tunnel_main (tdata->path, tdata->fd);

    free (tdata->path);
    free (tdata);

    return NULL;
}

static void
native_start_service (JNIEnv *env, jobject thiz, jstring config_path, jint fd)
{
    const jbyte *bytes;
    ThreadData *tdata;
    int res;

    pthread_mutex_lock (&mutex);

    if (is_working)
        goto exit;

    tdata = malloc (sizeof (ThreadData));
    tdata->fd = fd;

    bytes = (const jbyte *)(*env)->GetStringUTFChars (env, config_path, NULL);
    tdata->path = strdup ((const char *)bytes);
    (*env)->ReleaseStringUTFChars (env, config_path, (const char *)bytes);

    res = pthread_create (&work_thread, NULL, thread_handler, tdata);
    if (res < 0) {
        free (tdata->path);
        free (tdata);
        goto exit;
    }

    is_working = 1;
exit:
    pthread_mutex_unlock (&mutex);
}

static void
native_stop_service (JNIEnv *env, jobject thiz)
{
    pthread_mutex_lock (&mutex);

    if (!is_working)
        goto exit;

    hev_socks5_tunnel_quit ();
    pthread_join (work_thread, NULL);

    is_working = 0;
exit:
    pthread_mutex_unlock (&mutex);
}

#endif /* ANDROID */
