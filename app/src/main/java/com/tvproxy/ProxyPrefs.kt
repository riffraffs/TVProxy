package com.tvproxy

import android.content.Context

object ProxyPrefs {
    private const val NAME = "tvproxy"
    private const val KEY_PROTOCOL = "protocol"
    private const val KEY_HOST = "host"
    private const val KEY_PORT = "port"
    private const val KEY_VPN_EXPLAINED = "vpn_explained"
    private const val KEY_KEEPALIVE_HINT = "keepalive_hint_shown"
    private const val KEY_START_ATTEMPT = "start_attempt"
    private const val KEY_DROP_QUIC = "drop_quic"

    fun load(context: Context): ProxyConfig {
        val p = prefs(context)
        val defaults = ProxyConfig()
        return ProxyConfig(
            protocol = p.getString(KEY_PROTOCOL, defaults.protocol) ?: defaults.protocol,
            host = p.getString(KEY_HOST, defaults.host) ?: defaults.host,
            port = p.getInt(KEY_PORT, defaults.port),
        )
    }

    fun save(context: Context, config: ProxyConfig) {
        prefs(context).edit()
            .putString(KEY_PROTOCOL, config.protocol)
            .putString(KEY_HOST, config.host)
            .putInt(KEY_PORT, config.port)
            .apply()
    }

    fun wasVpnExplained(context: Context): Boolean =
        prefs(context).getBoolean(KEY_VPN_EXPLAINED, false)

    fun setVpnExplained(context: Context) {
        prefs(context).edit().putBoolean(KEY_VPN_EXPLAINED, true).apply()
    }

    fun wasKeepAliveHintShown(context: Context): Boolean =
        prefs(context).getBoolean(KEY_KEEPALIVE_HINT, false)

    fun setKeepAliveHintShown(context: Context) {
        prefs(context).edit().putBoolean(KEY_KEEPALIVE_HINT, true).apply()
    }

    fun markStartAttempt(context: Context) {
        prefs(context).edit().putBoolean(KEY_START_ATTEMPT, true).apply()
    }

    fun clearStartAttempt(context: Context) {
        prefs(context).edit().putBoolean(KEY_START_ATTEMPT, false).apply()
    }

    fun hadUnfinishedStart(context: Context): Boolean =
        prefs(context).getBoolean(KEY_START_ATTEMPT, false)

    /**
     * Drop QUIC (UDP/443) in the tunnel and answer ICMP port unreachable, so
     * apps fall back to TCP. On for upstream exits whose UDP relay is
     * unreliable (TCP is fast but QUIC crawls); persisted so a UI toggle can
     * be added later.
     */
    fun dropQuic(context: Context): Boolean =
        prefs(context).getBoolean(KEY_DROP_QUIC, true)

    private fun prefs(context: Context) =
        context.getSharedPreferences(NAME, Context.MODE_PRIVATE)
}
