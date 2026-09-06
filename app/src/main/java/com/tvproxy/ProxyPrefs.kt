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

    private fun prefs(context: Context) =
        context.getSharedPreferences(NAME, Context.MODE_PRIVATE)
}
