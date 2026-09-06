package com.tvproxy

data class ProxyConfig(
    val protocol: String = PROTOCOL_SOCKS5,
    val host: String = "10.0.2.2",
    val port: Int = 10800,
) {
    fun validate(): Int? {
        if (host.isBlank()) return R.string.error_host
        if (port !in 1..65535) return R.string.error_port
        return null
    }

    companion object {
        const val PROTOCOL_SOCKS5 = "socks5"
        const val PROTOCOL_HTTP = "http"

        fun fromUi(protocolLabel: String, host: String, portText: String): ProxyConfig {
            val protocol = if (protocolLabel.equals("HTTP", ignoreCase = true)) {
                PROTOCOL_HTTP
            } else {
                PROTOCOL_SOCKS5
            }
            val port = portText.toIntOrNull() ?: -1
            return ProxyConfig(protocol, host.trim(), port)
        }
    }
}
