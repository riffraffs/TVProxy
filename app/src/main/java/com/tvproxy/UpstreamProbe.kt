package com.tvproxy

import android.util.Log
import java.io.InputStream
import java.net.InetSocketAddress
import java.net.Socket

/**
 * Quick probe of the upstream SOCKS5 server: does it provide a usable UDP
 * relay (RFC 1928 UDP ASSOCIATE)?
 *
 * Some LAN-shared SOCKS5 servers are TCP-only (e.g. iOS Loon "网络共享"):
 * they answer the ASSOCIATE request with rep=0 but a zeroed BND.ADDR/PORT
 * (0.0.0.0:0), so relayed datagrams can never reach them. In that case
 * TVProxy falls back to forwarding DNS over TCP instead.
 *
 * Return-value policy:
 * - Connect / greeting failures -> true (assume UDP works). This preserves
 *   today's behaviour for every upstream that was reachable before; if the
 *   server is simply down the mode does not matter.
 * - Once the SOCKS5 greeting succeeded, "no usable UDP relay" (zeroed or
 *   missing BND, or command rejected) -> false, so the DNS-over-TCP mode is
 *   entered. That mode only needs TCP CONNECT, which every SOCKS5 server
 *   supports, so this is safe even for a slow or unusual UDP-capable server.
 */
object UpstreamProbe {
    private const val TAG = "TvProxyProbe"
    private const val CONNECT_TIMEOUT_MS = 1500
    private const val IO_TIMEOUT_MS = 3000

    fun supportsUdpRelay(host: String, port: Int): Boolean {
        val socket = Socket()
        try {
            socket.tcpNoDelay = true
            socket.connect(InetSocketAddress(host, port), CONNECT_TIMEOUT_MS)
            socket.soTimeout = IO_TIMEOUT_MS
            val out = socket.getOutputStream()
            val input = socket.getInputStream()

            // Greeting: only offer no-auth.
            out.write(byteArrayOf(0x05, 0x01, 0x00))
            out.flush()
            val sel = ByteArray(2)
            if (!readExact(input, sel, 2)) {
                Log.w(TAG, "probe: greeting read failed -> assume udp")
                return true
            }
            Log.i(TAG, "probe: greeting=${sel.joinToString(" ") { "%02x".format(it) }}")
            if (sel[0] != 0x05.toByte() || sel[1] != 0x00.toByte()) {
                Log.w(TAG, "probe: no no-auth method -> assume udp")
                return true
            }

            // UDP ASSOCIATE with 0.0.0.0:0.
            out.write(byteArrayOf(0x05, 0x03, 0x00, 0x01, 0, 0, 0, 0, 0, 0))
            out.flush()
            val head = ByteArray(4)
            if (!readExact(input, head, 4)) {
                Log.w(TAG, "probe: associate head read failed -> no udp")
                return false
            }
            Log.i(TAG, "probe: associate head=${head.joinToString(" ") { "%02x".format(it) }}")
            if (head[0] != 0x05.toByte()) {
                Log.w(TAG, "probe: bad ver -> no udp")
                return false
            }
            if (head[1] != 0x00.toByte()) {
                Log.w(TAG, "probe: associate rejected rep=${head[1]} -> no udp")
                return false // e.g. command not supported
            }

            return when (head[3].toInt() and 0xff) {
                1 -> {
                    val bnd = ByteArray(6)
                    if (!readExact(input, bnd, 6)) {
                        Log.w(TAG, "probe: v4 bnd read failed -> no udp")
                        return false
                    }
                    Log.i(TAG, "probe: v4 bnd=${bnd.joinToString(" ") { "%02x".format(it) }}")
                    val addrZero = (bnd[0].toInt() and 0xff) == 0 &&
                        (bnd[1].toInt() and 0xff) == 0 &&
                        (bnd[2].toInt() and 0xff) == 0 &&
                        (bnd[3].toInt() and 0xff) == 0
                    val port = ((bnd[4].toInt() and 0xff) shl 8) or
                        (bnd[5].toInt() and 0xff)
                    !(addrZero && port == 0)
                }
                4 -> {
                    val bnd = ByteArray(18)
                    if (!readExact(input, bnd, 18)) return false
                    val addrZero = bnd.all { (it.toInt() and 0xff) == 0 }
                    val port = ((bnd[16].toInt() and 0xff) shl 8) or
                        (bnd[17].toInt() and 0xff)
                    !(addrZero && port == 0)
                }
                3 -> {
                    val len = ByteArray(1)
                    if (!readExact(input, len, 1)) return false
                    val skip = ByteArray(len[0].toInt() and 0xff)
                    if (!readExact(input, skip, skip.size)) return false
                    false
                }
                else -> false
            }
        } catch (e: Throwable) {
            Log.w(TAG, "probe: exception ${e.javaClass.simpleName}: ${e.message} -> assume udp")
            return true
        } finally {
            runCatching { socket.close() }
        }
    }

    private fun readExact(input: InputStream, buf: ByteArray, n: Int): Boolean {
        var off = 0
        while (off < n) {
            val r = input.read(buf, off, n - off)
            if (r < 0) return false
            off += r
        }
        return true
    }
}
