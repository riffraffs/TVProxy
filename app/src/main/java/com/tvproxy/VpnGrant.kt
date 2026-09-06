package com.tvproxy

import android.app.AppOpsManager
import android.content.Context
import android.net.VpnService
import android.os.IBinder
import android.os.Process
import android.util.Log

/**
 * HarmonyOS 2.0 TV ships no com.android.vpndialogs. The only way to become
 * "prepared" without that activity is AppOps OP_ACTIVATE_VPN = allow, which a
 * normal app cannot set. We still try hidden APIs in case the OEM is loose.
 */
object VpnGrant {
    private const val TAG = "TvProxyGrant"

    fun isPrepared(context: Context): Boolean = try {
        VpnService.prepare(context) == null
    } catch (_: Throwable) {
        false
    }

    fun tryActivate(context: Context): Boolean {
        if (isPrepared(context)) return true
        tryAppOpsSetMode(context)
        if (isPrepared(context)) return true
        tryPrepareVpnBinder(context)
        return isPrepared(context)
    }

    private fun tryAppOpsSetMode(context: Context) {
        try {
            val appOps = context.getSystemService(Context.APP_OPS_SERVICE) as AppOpsManager
            val op = AppOpsManager::class.java.getField("OP_ACTIVATE_VPN").getInt(null)
            val setMode = AppOpsManager::class.java.getMethod(
                "setMode",
                Int::class.javaPrimitiveType,
                Int::class.javaPrimitiveType,
                String::class.java,
                Int::class.javaPrimitiveType,
            )
            setMode.invoke(appOps, op, Process.myUid(), context.packageName, AppOpsManager.MODE_ALLOWED)
            Log.i(TAG, "AppOps setMode OP_ACTIVATE_VPN attempted")
        } catch (e: Throwable) {
            Log.w(TAG, "AppOps setMode failed: ${e.javaClass.simpleName}: ${e.message}")
        }
    }

    private fun tryPrepareVpnBinder(context: Context) {
        val userId = Process.myUid() / 100000
        val pkg = context.packageName
        for (svc in arrayOf("connectivity", "vpn_management")) {
            try {
                val sm = Class.forName("android.os.ServiceManager")
                val raw = sm.getMethod("getService", String::class.java).invoke(null, svc) as? IBinder
                    ?: continue
                val stubName = if (svc == "vpn_management") {
                    "android.net.IVpnManager\$Stub"
                } else {
                    "android.net.IConnectivityManager\$Stub"
                }
                val iface = Class.forName(stubName)
                    .getMethod("asInterface", IBinder::class.java)
                    .invoke(null, raw) ?: continue
                val method = iface.javaClass.methods.firstOrNull { it.name == "prepareVpn" } ?: continue
                val args = when (method.parameterTypes.size) {
                    2 -> arrayOf(null, pkg)
                    3 -> arrayOf(null, pkg, userId)
                    else -> continue
                }
                method.invoke(iface, *args)
                Log.i(TAG, "prepareVpn via $svc attempted")
            } catch (e: Throwable) {
                Log.w(TAG, "prepareVpn $svc failed: ${e.javaClass.simpleName}: ${e.message}")
            }
            if (isPrepared(context)) return
        }
    }
}
