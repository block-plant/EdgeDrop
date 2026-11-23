package com.edgedrop.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import com.edgedrop.R
import com.edgedrop.ui.MainActivity

class EdgeNotchService : Service() {

    private var overlayManager: OverlayViewManager? = null

    companion object {
        const val CHANNEL_ID = "edgedrop_notch_service_channel"
        const val NOTIFICATION_ID = 5001

        const val ACTION_START = "com.edgedrop.action.START"
        const val ACTION_TOGGLE_NOTCH = "com.edgedrop.action.TOGGLE_NOTCH"
        const val ACTION_STOP = "com.edgedrop.action.STOP"

        fun start(context: Context) {
            val intent = Intent(context, EdgeNotchService::class.java).apply {
                action = ACTION_START
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent)
            } else {
                context.startService(intent)
            }
        }

        fun stop(context: Context) {
            val intent = Intent(context, EdgeNotchService::class.java).apply {
                action = ACTION_STOP
            }
            context.startService(intent)
        }
    }

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        overlayManager = OverlayViewManager(this)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val action = intent?.action ?: ACTION_START

        when (action) {
            ACTION_START -> {
                startForegroundWithNotification()
                overlayManager?.show()
            }
            ACTION_TOGGLE_NOTCH -> {
                overlayManager?.toggleVisibility()
            }
            ACTION_STOP -> {
                stopSelf()
            }
        }

        return START_STICKY
    }

    private fun startForegroundWithNotification() {
        val openAppIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE
        )

        val toggleIntent = PendingIntent.getService(
            this,
            1,
            Intent(this, EdgeNotchService::class.java).apply { action = ACTION_TOGGLE_NOTCH },
            PendingIntent.FLAG_IMMUTABLE
        )

        val stopIntent = PendingIntent.getService(
            this,
            2,
            Intent(this, EdgeNotchService::class.java).apply { action = ACTION_STOP },
            PendingIntent.FLAG_IMMUTABLE
        )

        val notification: Notification = NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("EdgeDrop is active")
            .setContentText("Tap the screen edge to shrink any file offline")
            .setSmallIcon(R.drawable.ic_drop)
            .setContentIntent(openAppIntent)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .addAction(R.drawable.ic_close, "Toggle Notch", toggleIntent)
            .addAction(R.drawable.ic_quit, "Stop", stopIntent)
            .build()

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
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "EdgeDrop Floating Notch",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Keeps the EdgeDrop edge notch available on your screen"
                setShowBadge(false)
            }
            val manager = getSystemService(NotificationManager::class.java)
            manager?.createNotificationChannel(channel)
        }
    }

    override fun onDestroy() {
        overlayManager?.onDestroy()
        overlayManager = null
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null
}
