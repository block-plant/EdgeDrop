package com.edgedrop.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.content.res.Configuration
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import com.edgedrop.R
import com.edgedrop.ui.MainActivity
import java.io.File

/**
 * Crash-proof foreground service hosting EdgeDrop's persistent floating Notch / Edge Pill overlay.
 * Fully compliant with Android 14/15/16 foreground service requirements (FOREGROUND_SERVICE_TYPE_SPECIAL_USE).
 */
open class OverlayNotchService : Service() {

    private var overlayManager: OverlayViewManager? = null

    companion object {
        const val CHANNEL_ID = "edgedrop_notch_service_channel"
        const val NOTIFICATION_ID = 5001

        const val ACTION_START = "com.edgedrop.action.START"
        const val ACTION_SHOW_NOTCH = "com.edgedrop.action.SHOW_NOTCH"
        const val ACTION_TOGGLE_NOTCH = "com.edgedrop.action.TOGGLE_NOTCH"
        const val ACTION_STOP = "com.edgedrop.action.STOP"
        const val ACTION_SET_FILE = "com.edgedrop.action.SET_FILE"
        const val ACTION_RECENTER = "com.edgedrop.action.RECENTER"
        const val EXTRA_FILE_PATH = "extra_file_path"

        @Volatile
        private var instance: OverlayNotchService? = null

        fun isRunning(): Boolean = instance != null

        fun start(context: Context) {
            val intent = Intent(context, OverlayNotchService::class.java).apply {
                action = ACTION_START
            }
            try {
                ContextCompat.startForegroundService(context, intent)
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }

        fun stop(context: Context) {
            val intent = Intent(context, OverlayNotchService::class.java).apply {
                action = ACTION_STOP
            }
            context.startService(intent)
        }

        fun showNotch(context: Context) {
            val intent = Intent(context, OverlayNotchService::class.java).apply {
                action = ACTION_SHOW_NOTCH
            }
            context.startService(intent)
        }

        fun recenterNotch(context: Context) {
            val intent = Intent(context, OverlayNotchService::class.java).apply {
                action = ACTION_RECENTER
            }
            context.startService(intent)
        }

        fun onFilePicked(context: Context, filePath: String) {
            instance?.overlayManager?.setIncomingFileAndExpand(File(filePath)) ?: run {
                val intent = Intent(context, OverlayNotchService::class.java).apply {
                    action = ACTION_SET_FILE
                    putExtra(EXTRA_FILE_PATH, filePath)
                }
                context.startService(intent)
            }
        }

        fun onPickerCancelled(context: Context) {
            instance?.overlayManager?.collapse()
        }
    }

    override fun onCreate() {
        super.onCreate()
        instance = this
        createNotificationChannel()
        overlayManager = OverlayViewManager(this)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        startForegroundWithNotification()

        when (intent?.action) {
            ACTION_START, ACTION_SHOW_NOTCH -> {
                overlayManager?.show()
            }
            ACTION_TOGGLE_NOTCH -> {
                overlayManager?.toggleVisibility()
            }
            ACTION_RECENTER -> {
                overlayManager?.show()
                overlayManager?.recenter()
            }
            ACTION_SET_FILE -> {
                val path = intent.getStringExtra(EXTRA_FILE_PATH)
                if (path != null) {
                    val file = File(path)
                    if (file.exists()) {
                        overlayManager?.setIncomingFileAndExpand(file)
                    }
                }
            }
            ACTION_STOP -> {
                stopSelf()
                return START_NOT_STICKY
            }
            else -> {
                overlayManager?.show()
            }
        }

        return START_STICKY
    }

    override fun onConfigurationChanged(newConfig: Configuration) {
        super.onConfigurationChanged(newConfig)
        overlayManager?.onConfigurationChanged(newConfig)
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onDestroy() {
        instance = null
        overlayManager?.onDestroy()
        overlayManager = null
        super.onDestroy()
    }

    private fun startForegroundWithNotification() {
        val openAppIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE
        )

        val showNotchIntent = PendingIntent.getService(
            this,
            1,
            Intent(this, OverlayNotchService::class.java).apply { action = ACTION_SHOW_NOTCH },
            PendingIntent.FLAG_IMMUTABLE
        )

        val stopIntent = PendingIntent.getService(
            this,
            2,
            Intent(this, OverlayNotchService::class.java).apply { action = ACTION_STOP },
            PendingIntent.FLAG_IMMUTABLE
        )

        val notification: Notification = NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("EdgeDrop is active")
            .setContentText("Tap the screen edge to shrink any file offline")
            .setSmallIcon(R.drawable.ic_drop)
            .setContentIntent(openAppIntent)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .addAction(R.drawable.ic_drop, "Show Notch", showNotchIntent)
            .addAction(R.drawable.ic_quit, "Turn Off", stopIntent)
            .build()

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                startForeground(
                    NOTIFICATION_ID,
                    notification,
                    ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE
                )
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                startForeground(
                    NOTIFICATION_ID,
                    notification,
                    ServiceInfo.FOREGROUND_SERVICE_TYPE_MANIFEST
                )
            } else {
                startForeground(NOTIFICATION_ID, notification)
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "EdgeDrop Floating Overlay",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Keeps EdgeDrop edge notch available above apps"
                setShowBadge(false)
            }

            val manager = getSystemService(NotificationManager::class.java)
            manager?.createNotificationChannel(channel)
        }
    }
}


