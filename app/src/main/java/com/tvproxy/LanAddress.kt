package com.tvproxy

import java.net.Inet4Address
import java.net.NetworkInterface

object LanAddress {
    private const val TUN = "10.0.0.2"

    fun ipv4(): String? {
        val found = ArrayList<String>()
        val ifaces = NetworkInterface.getNetworkInterfaces() ?: return null
        for (nic in ifaces) {
            if (!nic.isUp || nic.isLoopback) continue
            val addrs = nic.inetAddresses
            while (addrs.hasMoreElements()) {
                val addr = addrs.nextElement()
                if (addr !is Inet4Address || addr.isLoopbackAddress) continue
                val ip = addr.hostAddress ?: continue
                if (ip == TUN) continue
                found.add(ip)
            }
        }
        return found.firstOrNull { it.startsWith("192.168.") }
            ?: found.firstOrNull { isPrivate10(it) }
            ?: found.firstOrNull { isPrivate172(it) }
            ?: found.firstOrNull()
    }

    private fun isPrivate10(ip: String) = ip.startsWith("10.") && ip != TUN

    private fun isPrivate172(ip: String): Boolean {
        if (!ip.startsWith("172.")) return false
        val second = ip.split('.').getOrNull(1)?.toIntOrNull() ?: return false
        return second in 16..31
    }
}
