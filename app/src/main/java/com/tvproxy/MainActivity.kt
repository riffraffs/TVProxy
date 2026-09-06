package com.tvproxy

import android.app.Activity
import android.app.Dialog
import android.content.ActivityNotFoundException
import android.content.BroadcastReceiver
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.VpnService
import android.os.Build
import android.os.Bundle
import android.os.UserManager
import android.text.method.ScrollingMovementMethod
import android.util.Log
import android.view.View
import android.view.inputmethod.InputMethodManager
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.EditText
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.Spinner
import android.widget.TextView

class MainActivity : Activity() {
    private lateinit var statusView: TextView
    private lateinit var pill: LinearLayout
    private lateinit var pillLed: ImageView
    private lateinit var lanIp: TextView
    private lateinit var protocol: Spinner
    private lateinit var host: EditText
    private lateinit var port: EditText
    private lateinit var errorView: TextView
    private lateinit var saveBtn: Button
    private lateinit var stopBtn: Button

    private val stateReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            refreshStatus()
            val err = intent?.getStringExtra(TvProxyVpnService.EXTRA_ERROR)
            if (!err.isNullOrBlank()) showError(err)
        }
    }

    /** Waiting for the system VPN consent activity (HarmonyOS/TV often returns CANCELED early). */
    private var awaitingVpnConsent = false
    private var consentReturned = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        statusView = findViewById(R.id.status)
        pill = findViewById(R.id.pill)
        pillLed = findViewById(R.id.pill_led)
        lanIp = findViewById(R.id.lan_ip)
        protocol = findViewById(R.id.protocol)
        host = findViewById(R.id.host)
        port = findViewById(R.id.port)
        errorView = findViewById(R.id.error)
        errorView.movementMethod = ScrollingMovementMethod.getInstance()
        saveBtn = findViewById(R.id.btn_save)
        stopBtn = findViewById(R.id.btn_stop)

        val adapter = ArrayAdapter.createFromResource(
            this,
            R.array.protocols,
            R.layout.item_protocol,
        )
        adapter.setDropDownViewResource(R.layout.item_protocol_dropdown)
        protocol.adapter = adapter

        bindConfig(ProxyPrefs.load(this))
        lanIp.text = LanAddress.ipv4() ?: getString(R.string.lan_unknown)
        saveBtn.setOnClickListener { onSave() }
        stopBtn.setOnClickListener { stopVpn() }
        wireEditor(host)
        wireEditor(port)
        protocol.requestFocus()
        refreshStatus()
        CrashLog.consume(this)?.let { showError(getString(R.string.crash_last, it)) }
        if (errorView.visibility != View.VISIBLE && ProxyPrefs.hadUnfinishedStart(this)) {
            ProxyPrefs.clearStartAttempt(this)
            showError(getString(R.string.crash_native_hint))
        }
    }

    override fun onStart() {
        super.onStart()
        val filter = IntentFilter(TvProxyVpnService.ACTION_STATE)
        if (Build.VERSION.SDK_INT >= 33) {
            registerReceiver(stateReceiver, filter, RECEIVER_NOT_EXPORTED)
        } else {
            @Suppress("UnspecifiedRegisterReceiverFlag")
            registerReceiver(stateReceiver, filter)
        }
        refreshStatus()
        lanIp.text = LanAddress.ipv4() ?: getString(R.string.lan_unknown)
    }

    override fun onResume() {
        super.onResume()
        completeVpnConsentIfReady()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) completeVpnConsentIfReady()
    }

    override fun onStop() {
        unregisterReceiver(stateReceiver)
        super.onStop()
    }

    @Deprecated("Deprecated in Java")
    override fun onBackPressed() {
        val focused = currentFocus
        if (focused is EditText) {
            hideIme()
            focused.clearFocus()
            saveBtn.requestFocus()
            return
        }
        super.onBackPressed()
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != REQ_PREPARE) return
        Log.i(TAG, "VPN prepare result=$resultCode prepared=${isVpnPrepared()}")
        consentReturned = true
        if (resultCode == RESULT_OK || isVpnPrepared()) {
            awaitingVpnConsent = false
            startVpn()
            return
        }
        completeVpnConsentIfReady()
    }

    private fun onSave() {
        hideIme()
        val config = ProxyConfig.fromUi(
            protocol.selectedItem?.toString().orEmpty(),
            host.text.toString(),
            port.text.toString(),
        )
        val err = config.validate()
        if (err != null) {
            showError(getString(err))
            return
        }
        errorView.visibility = View.GONE
        ProxyPrefs.save(this, config)
        if (!ProxyPrefs.wasVpnExplained(this)) {
            showExplainDialog()
        } else {
            requestVpn()
        }
    }

    private fun showExplainDialog() {
        try {
            val dialog = Dialog(this)
            dialog.setContentView(R.layout.dialog_vpn_explain)
            dialog.findViewById<Button>(R.id.dlg_cancel).setOnClickListener { dialog.dismiss() }
            dialog.findViewById<Button>(R.id.dlg_allow).setOnClickListener {
                ProxyPrefs.setVpnExplained(this)
                dialog.dismiss()
                saveBtn.post { requestVpn() }
            }
            dialog.show()
            dialog.findViewById<Button>(R.id.dlg_allow).requestFocus()
        } catch (e: Throwable) {
            CrashLog.write(this, e)
            showError(getString(R.string.vpn_start_failed, e.message ?: e.javaClass.simpleName))
        }
    }

    private fun requestVpn() {
        try {
            if (isVpnPrepared()) {
                startVpn()
                return
            }
            if (vpnConfigRestricted()) {
                showError(getString(R.string.vpn_denied_restricted))
                return
            }
            val prepare = VpnService.prepare(this) ?: run {
                startVpn()
                return
            }
            if (launchSystemVpnConfirm(prepare)) {
                return
            }
            // HarmonyOS TV has no com.android.vpndialogs. Try AppOps / hidden
            // prepareVpn first; if still not prepared, establish() will fail
            // and the service broadcasts the ADB grant instructions.
            Log.w(TAG, "no VPN confirm activity ${prepare.component}")
            if (VpnGrant.tryActivate(this)) {
                Log.i(TAG, "VPN pre-consented via hidden grant")
            }
            startVpn()
        } catch (e: Throwable) {
            awaitingVpnConsent = false
            CrashLog.write(this, e)
            showError(
                getString(
                    R.string.vpn_prepare_failed,
                    e.javaClass.simpleName + ": " + (e.message ?: ""),
                ),
            )
        }
    }

    /**
     * @return true if a system confirm activity was launched
     */
    private fun launchSystemVpnConfirm(prepare: Intent): Boolean {
        prepare.flags = prepare.flags and Intent.FLAG_ACTIVITY_NEW_TASK.inv()
        val pm = packageManager
        if (prepare.resolveActivity(pm) == null) {
            val alts = arrayOf(
                ComponentName("com.huawei.vpndialogs", "com.huawei.vpndialogs.ConfirmDialog"),
                ComponentName(
                    "com.huawei.android.vpndialogs",
                    "com.huawei.android.vpndialogs.ConfirmDialog",
                ),
            )
            val hit = alts.firstOrNull { cn ->
                try {
                    pm.getActivityInfo(cn, 0)
                    true
                } catch (_: Exception) {
                    false
                }
            }
            if (hit != null) {
                prepare.component = hit
            } else {
                prepare.component = null
            }
        }
        if (prepare.resolveActivity(pm) == null) {
            return false
        }
        Log.i(TAG, "launch VPN confirm component=${prepare.component}")
        awaitingVpnConsent = true
        consentReturned = false
        return try {
            startActivityForResult(prepare, REQ_PREPARE)
            true
        } catch (e: ActivityNotFoundException) {
            Log.w(TAG, "VPN confirm not found", e)
            awaitingVpnConsent = false
            false
        }
    }

    private fun completeVpnConsentIfReady() {
        if (!awaitingVpnConsent) return
        if (isVpnPrepared()) {
            awaitingVpnConsent = false
            consentReturned = false
            startVpn()
            return
        }
        if (consentReturned && hasWindowFocus()) {
            awaitingVpnConsent = false
            showError(
                if (vpnConfigRestricted()) {
                    getString(R.string.vpn_denied_restricted)
                } else {
                    getString(R.string.vpn_denied_help)
                },
            )
        }
    }

    private fun isVpnPrepared(): Boolean = VpnService.prepare(this) == null

    private fun vpnConfigRestricted(): Boolean {
        return try {
            val um = getSystemService(USER_SERVICE) as? UserManager ?: return false
            um.hasUserRestriction(UserManager.DISALLOW_CONFIG_VPN)
        } catch (_: Throwable) {
            false
        }
    }

    private fun startVpn() {
        ProxyPrefs.markStartAttempt(this)
        val intent = Intent(this, TvProxyVpnService::class.java)
            .setAction(TvProxyVpnService.ACTION_START)
        try {
            if (Build.VERSION.SDK_INT >= 26) {
                startForegroundService(intent)
            } else {
                @Suppress("DEPRECATION")
                startService(intent)
            }
        } catch (e: Throwable) {
            ProxyPrefs.clearStartAttempt(this)
            CrashLog.write(this, e)
            showError(getString(R.string.vpn_start_failed, e.message ?: e.javaClass.simpleName))
        }
    }

    private fun stopVpn() {
        startService(Intent(this, TvProxyVpnService::class.java).setAction(TvProxyVpnService.ACTION_STOP))
    }

    private fun bindConfig(config: ProxyConfig) {
        protocol.setSelection(if (config.protocol == ProxyConfig.PROTOCOL_HTTP) 1 else 0)
        host.setText(config.host)
        port.setText(config.port.toString())
    }

    private fun refreshStatus() {
        val running = TvProxyVpnService.isRunning.get()
        if (running) {
            ProxyPrefs.clearStartAttempt(this)
            errorView.visibility = View.GONE
        }
        statusView.setText(if (running) R.string.status_running else R.string.status_idle)
        @Suppress("DEPRECATION")
        statusView.setTextColor(resources.getColor(if (running) R.color.ok else R.color.muted))
        pill.setBackgroundResource(if (running) R.drawable.pill_on else R.drawable.pill_idle)
        pillLed.setImageResource(if (running) R.drawable.led_dot_on else R.drawable.led_dot)
        saveBtn.setText(if (running) R.string.btn_save_restart else R.string.btn_save_start)
        stopBtn.isEnabled = running
        stopBtn.alpha = if (running) 1f else 0.38f
        maybeShowKeepAliveHint()
    }

    /**
     * First successful start only: guide the user to allow background running
     * in HarmonyOS 2.0 "应用启动管理", so the proxy keeps working after being
     * switched away or idle. Shows once; flag set before showing to avoid loops.
     */
    private fun maybeShowKeepAliveHint() {
        if (!TvProxyVpnService.isRunning.get()) return
        if (ProxyPrefs.wasKeepAliveHintShown(this)) return
        if (isFinishing || isDestroyed) return
        ProxyPrefs.setKeepAliveHintShown(this)
        val dialog = Dialog(this)
        dialog.setContentView(R.layout.dialog_keepalive)
        dialog.findViewById<Button>(R.id.dlg_keepalive_ok).setOnClickListener {
            dialog.dismiss()
        }
        dialog.setOnDismissListener { saveBtn.requestFocus() }
        dialog.show()
        dialog.findViewById<Button>(R.id.dlg_keepalive_ok).requestFocus()
    }

    private fun showError(msg: String) {
        errorView.text = msg
        errorView.visibility = View.VISIBLE
        errorView.isFocusable = true
        errorView.isFocusableInTouchMode = true
    }

    private fun wireEditor(edit: EditText) {
        edit.setSelectAllOnFocus(true)
        val selectAll = Runnable {
            if (edit.hasFocus() && edit.text.isNotEmpty()) {
                edit.selectAll()
            }
        }
        edit.setOnFocusChangeListener { _, hasFocus ->
            if (hasFocus) {
                edit.post(selectAll)
                edit.postDelayed(selectAll, 80)
            } else {
                edit.removeCallbacks(selectAll)
            }
        }
        edit.setOnClickListener {
            showIme(edit)
            edit.post(selectAll)
            edit.postDelayed(selectAll, 80)
        }
    }

    private fun showIme(view: View) {
        view.post {
            view.requestFocus()
            val imm = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
            imm.showSoftInput(view, InputMethodManager.SHOW_IMPLICIT)
        }
    }

    private fun hideIme() {
        val imm = getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
        val token = currentFocus?.windowToken ?: saveBtn.windowToken
        imm.hideSoftInputFromWindow(token, 0)
    }

    companion object {
        private const val TAG = "TvProxy"
        private const val REQ_PREPARE = 1001
    }
}
