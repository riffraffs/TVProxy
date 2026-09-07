package com.tvproxy

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.net.VpnService
import android.os.Build
import android.os.ParcelFileDescriptor
import android.util.Log
import java.io.File
import java.net.Inet4Address
import java.util.concurrent.atomic.AtomicBoolean

class TvProxyVpnService : VpnService() {
    private val runningLock = Any()
    private var nativeStarted = false
    private var nativeLoaded = false

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        try {
            startForeground(NOTIF_ID, buildNotification(R.drawable.ic_stat_notify))
        } catch (e: Throwable) {
            Log.e(TAG, "startForeground with app icon failed", e)
            try {
                startForeground(NOTIF_ID, buildNotification(android.R.drawable.stat_sys_warning))
            } catch (e2: Throwable) {
                Log.e(TAG, "startForeground fallback failed", e2)
                CrashLog.write(this, e2)
            }
        }
        return try {
            when (intent?.action) {
                ACTION_STOP -> {
                    teardown()
                    stopSelf()
                    START_NOT_STICKY
                }
                else -> {
                    val ok = startTun()
                    if (!ok) {
                        hideNotification()
                        stopSelf()
                        START_NOT_STICKY
                    } else {
                        ProxyPrefs.clearStartAttempt(this)
                        START_STICKY
                    }
                }
            }
        } catch (e: Throwable) {
            Log.e(TAG, "onStartCommand failed", e)
            CrashLog.write(this, e)
            isRunning.set(false)
            notifyState(e.javaClass.simpleName + ": " + (e.message ?: ""))
            hideNotification()
            stopSelf()
            START_NOT_STICKY
        }
    }

    override fun onDestroy() {
        teardown()
        super.onDestroy()
    }

    override fun onRevoke() {
        teardown()
        stopSelf()
    }

    private fun startTun(): Boolean {
        synchronized(runningLock) {
            teardownLocked()
            val config = ProxyPrefs.load(this)
            val dnsOverTcp = !probeUdpRelay(config.host, config.port)
            Log.i(TAG, "upstream udp relay supported=${!dnsOverTcp} dns-over-tcp=$dnsOverTcp")
            dnsOverTcpRunning.set(dnsOverTcp)
            val dns = dnsServers(dnsOverTcp)
            Log.i(TAG, "establishing tun for ${config.protocol} ${config.host}:${config.port} dns=$dns")
            val builder = Builder()
                .setSession(SESSION)
                .addAddress(TUN_ADDR, TUN_PREFIX)
                .addRoute(TUN_ROUTE, 0)
                .setMtu(TUN_MTU)
            for (server in dns) {
                builder.addDnsServer(server)
            }
            try {
                builder.addDisallowedApplication(packageName)
            } catch (e: Throwable) {
                Log.w(TAG, "addDisallowedApplication skipped", e)
            }
            val pfd: ParcelFileDescriptor? = try {
                builder.establish()
            } catch (e: Throwable) {
                Log.e(TAG, "establish failed", e)
                CrashLog.write(this, e)
                isRunning.set(false)
                notifyState(getString(R.string.vpn_establish_failed, e.message ?: e.javaClass.simpleName))
                return false
            }
            if (pfd == null) {
                Log.e(TAG, "establish returned null")
                isRunning.set(false)
                val needConsent = try {
                    prepare(this) != null
                } catch (_: Throwable) {
                    true
                }
                notifyState(
                    getString(
                        if (needConsent) R.string.vpn_establish_no_consent
                        else R.string.vpn_establish_rejected,
                    ),
                )
                return false
            }
            if (config.protocol != ProxyConfig.PROTOCOL_SOCKS5) {
                Log.w(
                    TAG,
                    "HTTP CONNECT not implemented; using SOCKS5 to ${config.host}:${config.port}",
                )
            }
            val conf = writeNativeConfig(config, dnsOverTcp)
            val fd = pfd.detachFd()
            try {
                if (!nativeLoaded) {
                    try {
                        System.loadLibrary("hev-socks5-tunnel")
                        nativeLoaded = true
                    } catch (le: Throwable) {
                        Log.e(TAG, "loadLibrary failed", le)
                        isRunning.set(false)
                        notifyState(getString(R.string.vpn_native_failed, le.message ?: le.javaClass.simpleName))
                        return false
                    }
                }
                TProxyStartService(conf.absolutePath, fd)
                nativeStarted = true
            } catch (e: Throwable) {
                Log.e(TAG, "TProxyStartService failed", e)
                CrashLog.write(this, e)
                isRunning.set(false)
                notifyState(getString(R.string.vpn_native_failed, e.message ?: e.javaClass.simpleName))
                return false
            }
            isRunning.set(true)
            notifyState()
            return true
        }
    }

    /**
     * DNS servers for the VPN network, in query order.
     *
     * UDP mode (default; the upstream relays UDP, e.g. Clash/mihomo):
     * queries to 8.8.8.8/1.1.1.1 reach the upstream, which answers from its
     * exit node, so foreign domains get real (unpoisoned) addresses; the
     * local ISP/DHCP DNS is kept only as a last fallback.
     *
     * DNS-over-TCP mode (upstream is TCP-only, e.g. iOS Loon LAN sharing):
     * native hev intercepts these UDP port-53 queries and forwards them as
     * DNS-over-TCP over a SOCKS5 CONNECT to the same resolver. Domestic clean
     * resolvers (AliDNS/DNSPod) come first: they answer foreign domains with
     * real addresses and are usually reachable DIRECT by the proxy host;
     * 8.8.8.8/1.1.1.1 follow in case the proxy's rules forward public DNS via
     * its exit node. The local DHCP DNS is omitted to avoid poisoned answers.
     */
    private fun dnsServers(dnsOverTcp: Boolean): List<String> {
        val found = linkedSetOf<String>()
        if (dnsOverTcp) {
            found.add(DNS_TCP_1)
            found.add(DNS_TCP_2)
            found.add(DNS_REMOTE_1)
            found.add(DNS_REMOTE_2)
            return found.toList()
        }
        found.add(DNS_REMOTE_1)
        found.add(DNS_REMOTE_2)
        found.addAll(systemDnsServers())
        return found.toList()
    }

    /**
     * Same as phone Wi-Fi manual proxy: keep the network's DHCP DNS.
     * Read before establish() so we see Wi-Fi, not this VPN.
     */
    private fun systemDnsServers(): List<String> {
        val found = linkedSetOf<String>()
        try {
            val cm = getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
            val networks = cm.allNetworks ?: emptyArray()
            for (network in networks) {
                val caps = cm.getNetworkCapabilities(network) ?: continue
                if (caps.hasTransport(NetworkCapabilities.TRANSPORT_VPN)) continue
                val lp = cm.getLinkProperties(network) ?: continue
                val ifname = lp.interfaceName.orEmpty()
                if (ifname.startsWith("tun") || ifname.startsWith("ppp")) continue
                for (addr in lp.dnsServers) {
                    if (addr is Inet4Address && !addr.isLoopbackAddress) {
                        val ip = addr.hostAddress ?: continue
                        if (ip != TUN_ADDR) found.add(ip)
                    }
                }
            }
        } catch (e: Throwable) {
            Log.w(TAG, "read system DNS failed", e)
        }
        if (found.isEmpty()) {
            Log.w(TAG, "no Wi-Fi DNS; fallback $DNS_REMOTE_1")
            found.add(DNS_REMOTE_1)
        }
        return found.toList()
    }

    /**
     * Run the UDP-relay probe off the main thread (network is banned on the
     * main thread) and wait up to 4s for its result. On timeout assume the
     * upstream supports UDP, keeping the historic default.
     */
    private fun probeUdpRelay(host: String, port: Int): Boolean {
        var result = true
        val thread = Thread {
            result = UpstreamProbe.supportsUdpRelay(host, port)
        }
        thread.start()
        return try {
            thread.join(4000)
            result
        } catch (e: Throwable) {
            Log.w(TAG, "udp probe interrupted", e)
            true
        }
    }

    private fun writeNativeConfig(config: ProxyConfig, dnsOverTcp: Boolean): File {
        val file = File(cacheDir, "tproxy.conf")
        val logPath = File(cacheDir, "hev.log").absolutePath.replace("\\", "/")
        val conf = buildString {
            appendLine("mtu=$TUN_MTU")
            appendLine("socks5-address=${config.host}")
            appendLine("socks5-port=${config.port}")
            appendLine("socks5-udp=udp")
            appendLine("log-file=$logPath")
            appendLine("log-level=info")
            appendLine("dns-over-tcp=$dnsOverTcp")
            appendLine("task-stack-size=262144")
        }
        file.writeText(conf)
        return file
    }

    private fun teardown() {
        synchronized(runningLock) {
            teardownLocked()
        }
        notifyState()
    }

    private fun teardownLocked() {
        dnsOverTcpRunning.set(false)
        if (nativeStarted) {
            try {
                TProxyStopService()
            } catch (e: Exception) {
                Log.e(TAG, "TProxyStopService failed", e)
            }
            nativeStarted = false
        }
        isRunning.set(false)
        hideNotification()
    }

    private fun hideNotification() {
        if (Build.VERSION.SDK_INT >= 24) {
            stopForeground(STOP_FOREGROUND_REMOVE)
        } else {
            @Suppress("DEPRECATION")
            stopForeground(true)
        }
    }

    private fun notifyState(error: String? = null) {
        val intent = Intent(ACTION_STATE).setPackage(packageName)
        if (!error.isNullOrBlank()) {
            intent.putExtra(EXTRA_ERROR, error)
        }
        sendBroadcast(intent)
    }

    private fun buildNotification(icon: Int): Notification {
        ensureChannel()
        val launch = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            pendingFlags(),
        )
        val builder = if (Build.VERSION.SDK_INT >= 26) {
            Notification.Builder(this, CHANNEL_ID)
        } else {
            @Suppress("DEPRECATION")
            Notification.Builder(this)
        }
        return builder
            .setContentTitle(getString(R.string.notif_title))
            .setContentText(getString(R.string.notif_text))
            .setSmallIcon(icon)
            .setContentIntent(launch)
            .setOngoing(true)
            .build()
    }

    private fun ensureChannel() {
        if (Build.VERSION.SDK_INT < 26) return
        val nm = getSystemService(NotificationManager::class.java)
        if (nm.getNotificationChannel(CHANNEL_ID) == null) {
            nm.createNotificationChannel(
                NotificationChannel(
                    CHANNEL_ID,
                    getString(R.string.notif_channel),
                    NotificationManager.IMPORTANCE_LOW,
                ),
            )
        }
    }

    private fun pendingFlags(): Int {
        return if (Build.VERSION.SDK_INT >= 31) {
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        } else {
            PendingIntent.FLAG_UPDATE_CURRENT
        }
    }

    @Suppress("FunctionName")
    private external fun TProxyStartService(configPath: String, fd: Int)

    @Suppress("FunctionName")
    private external fun TProxyStopService()

    companion object {
        const val ACTION_START = "com.tvproxy.action.START"
        const val ACTION_STOP = "com.tvproxy.action.STOP"
        const val ACTION_STATE = "com.tvproxy.action.STATE"
        const val EXTRA_ERROR = "com.tvproxy.extra.ERROR"

        val isRunning = AtomicBoolean(false)

        /** True when the current run is in DNS-over-TCP mode (upstream relays no UDP). */
        val dnsOverTcpRunning = AtomicBoolean(false)

        private const val TAG = "TvProxyVpn"
        private const val SESSION = "TVProxy"
        private const val TUN_ADDR = "10.0.0.2"
        private const val TUN_PREFIX = 24
        private const val TUN_ROUTE = "0.0.0.0"
        private const val TUN_MTU = 1500
        private const val DNS_REMOTE_1 = "8.8.8.8"
        private const val DNS_REMOTE_2 = "1.1.1.1"
        private const val DNS_TCP_1 = "223.5.5.5"
        private const val DNS_TCP_2 = "119.29.29.29"
        private const val NOTIF_ID = 1
        private const val CHANNEL_ID = "tvproxy.vpn"
    }
}
