package com.tvproxy

import android.content.Context
import android.util.Log
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter

object CrashLog {
    private const val TAG = "TvProxyCrash"
    private const val FILE = "last_crash.txt"
    private const val MAX = 1200

    fun write(context: Context, error: Throwable) {
        try {
            val sw = StringWriter()
            error.printStackTrace(PrintWriter(sw))
            val text = (error.javaClass.name + ": " + (error.message ?: "") + "\n" + sw.toString())
                .take(MAX)
            File(context.filesDir, FILE).writeText(text)
            Log.e(TAG, text, error)
        } catch (_: Throwable) {
        }
    }

    fun consume(context: Context): String? {
        val file = File(context.filesDir, FILE)
        if (!file.exists()) return null
        val text = try {
            file.readText()
        } catch (_: Throwable) {
            file.name
        }
        file.delete()
        return text.ifBlank { null }
    }
}
