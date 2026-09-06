/*
 ============================================================================
 Name        : hev-jni.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2019 - 2023 hev
 Description : Java Native Interface
 ============================================================================
 */

#ifndef __HEV_JNI_H__
#define __HEV_JNI_H__

#ifdef ANDROID
/* Exclude SOCKS sockets from the VpnService tun (VpnService.protect). */
int hev_net_protect (int fd);
#endif

#endif /* __HEV_JNI_H__ */
