package com.tvproxy

import android.app.Application

class TvProxyApp : Application() {
    override fun onCreate() {
        super.onCreate()
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, error ->
            CrashLog.write(this, error)
            previous?.uncaughtException(thread, error)
        }
    }
}
