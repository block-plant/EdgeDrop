package com.edgedrop.ui

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import com.edgedrop.R
import com.edgedrop.service.OverlayNotchService
import com.edgedrop.ui.components.TintedIcon
import com.edgedrop.ui.theme.EdgeDropTheme
import com.edgedrop.ui.theme.Vibe
import com.edgedrop.util.FileUtils
import com.edgedrop.util.PreferencesManager
import java.io.File

class MainActivity : ComponentActivity() {

    private var incomingFile by mutableStateOf<File?>(null)
    private var hasOverlayPermission by mutableStateOf(false)
    private var isOverlayActive by mutableStateOf(false)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        handleIntent(intent)

        setContent {
            val prefs = remember { PreferencesManager(this) }
            val currentVibe = remember { Vibe.fromId(prefs.vibe) }

            // Notification permission launcher for Android 13+
            val notificationPermissionLauncher = rememberLauncherForActivityResult(
                ActivityResultContracts.RequestPermission()
            ) { isGranted ->
                if (isGranted && prefs.isOverlayEnabled && hasOverlayPermission) {
                    OverlayNotchService.start(this)
                    isOverlayActive = true
                }
            }

            EdgeDropTheme(vibe = currentVibe) {
                val theme = EdgeDropTheme.tokens
                val scrollState = rememberScrollState()

                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .background(theme.surface)
                        .padding(horizontal = 16.dp, vertical = 12.dp),
                    contentAlignment = Alignment.TopCenter
                ) {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .widthIn(max = 420.dp)
                            .verticalScroll(scrollState),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(14.dp)
                    ) {
                        // Floating Notch Overlay Status & Quick Controls
                        FloatingNotchControlCard(
                            hasOverlayPermission = hasOverlayPermission,
                            isOverlayActive = isOverlayActive,
                            onToggleOverlay = { enable ->
                                prefs.isOverlayEnabled = enable
                                if (enable) {
                                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
                                        ContextCompat.checkSelfPermission(this@MainActivity, Manifest.permission.POST_NOTIFICATIONS)
                                        != PackageManager.PERMISSION_GRANTED
                                    ) {
                                        notificationPermissionLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
                                    }
                                    OverlayNotchService.start(this@MainActivity)
                                    isOverlayActive = true
                                } else {
                                    OverlayNotchService.stop(this@MainActivity)
                                    isOverlayActive = false
                                }
                            },
                            onRequestPermission = {
                                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                                    val intent = Intent(
                                        Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                                        Uri.parse("package:$packageName")
                                    )
                                    startActivity(intent)
                                }
                            },
                            onRecenterNotch = {
                                OverlayNotchService.recenterNotch(this@MainActivity)
                            },
                            onOpenAppSettings = {
                                val intent = Intent(
                                    Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                                    Uri.parse("package:$packageName")
                                )
                                startActivity(intent)
                            }
                        )

                        // Full In-App Compression Card (Always working offline directly in-app)
                        EdgeDropCard(
                            initialFile = incomingFile,
                            onClose = { finish() },
                            modifier = Modifier.fillMaxWidth()
                        )

                        Spacer(modifier = Modifier.height(8.dp))
                    }
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        val prefs = PreferencesManager(this)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            hasOverlayPermission = Settings.canDrawOverlays(this)
        } else {
            hasOverlayPermission = true
        }

        if (hasOverlayPermission && prefs.isOverlayEnabled) {
            OverlayNotchService.start(this)
            isOverlayActive = true
        } else {
            isOverlayActive = OverlayNotchService.isRunning()
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleIntent(intent)
    }

    private fun handleIntent(intent: Intent?) {
        if (intent == null) return
        val action = intent.action
        val type = intent.type

        if (Intent.ACTION_SEND == action && type != null) {
            val uri = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                intent.getParcelableExtra(Intent.EXTRA_STREAM, Uri::class.java)
            } else {
                @Suppress("DEPRECATION")
                intent.getParcelableExtra(Intent.EXTRA_STREAM)
            }
            if (uri != null) {
                val cached = FileUtils.copyUriToCache(this, uri)
                if (cached != null) {
                    incomingFile = cached
                }
            }
        } else if (Intent.ACTION_SEND_MULTIPLE == action && type != null) {
            val uris = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM, Uri::class.java)
            } else {
                @Suppress("DEPRECATION")
                intent.getParcelableArrayListExtra(Intent.EXTRA_STREAM)
            }
            val firstUri = uris?.firstOrNull()
            if (firstUri != null) {
                val cached = FileUtils.copyUriToCache(this, firstUri)
                if (cached != null) {
                    incomingFile = cached
                }
            }
        }
    }
}

/**
 * Clean, compact status card for the persistent Floating Edge Notch.
 * Shows overlay state, permission request button, recenter trigger, and OEM guidance.
 */
@Composable
fun FloatingNotchControlCard(
    hasOverlayPermission: Boolean,
    isOverlayActive: Boolean,
    onToggleOverlay: (Boolean) -> Unit,
    onRequestPermission: () -> Unit,
    onRecenterNotch: () -> Unit,
    onOpenAppSettings: () -> Unit,
    modifier: Modifier = Modifier
) {
    val theme = EdgeDropTheme.tokens
    val manufacturer = remember { Build.MANUFACTURER.lowercase() }
    val isOemSkin = remember {
        manufacturer.contains("xiaomi") || manufacturer.contains("redmi") ||
        manufacturer.contains("poco") || manufacturer.contains("oppo") ||
        manufacturer.contains("realme") || manufacturer.contains("oneplus") ||
        manufacturer.contains("vivo") || manufacturer.contains("iqoo") ||
        manufacturer.contains("samsung")
    }

    Box(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(theme.cardRadius))
            .background(theme.surfaceElevated)
            .border(1.dp, theme.borderHairline, RoundedCornerShape(theme.cardRadius))
            .padding(14.dp)
    ) {
        Column(
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            // Header Row: Icon + Title + Switch / Permission Button
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    Box(
                        modifier = Modifier
                            .size(32.dp)
                            .clip(CircleShape)
                            .background(theme.pillSurface)
                            .border(1.dp, theme.borderHairline, CircleShape),
                        contentAlignment = Alignment.Center
                    ) {
                        TintedIcon(
                            resId = R.drawable.ic_drop,
                            color = theme.accent,
                            size = 18.dp
                        )
                    }

                    Column {
                        Text(
                            text = "Floating Edge Notch",
                            fontSize = 14.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = theme.textPrimary
                        )
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(4.dp)
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(6.dp)
                                    .clip(CircleShape)
                                    .background(
                                        if (!hasOverlayPermission) Color(0xFFF59E0B)
                                        else if (isOverlayActive) Color(0xFF10B981)
                                        else theme.textMuted
                                    )
                            )
                            Text(
                                text = if (!hasOverlayPermission) "Permission Needed"
                                else if (isOverlayActive) "Active on Screen Edge"
                                else "Paused",
                                fontSize = 11.sp,
                                color = theme.textSecondary
                            )
                        }
                    }
                }

                if (hasOverlayPermission) {
                    Switch(
                        checked = isOverlayActive,
                        onCheckedChange = { onToggleOverlay(it) },
                        colors = SwitchDefaults.colors(
                            checkedThumbColor = Color.White,
                            checkedTrackColor = theme.accent,
                            uncheckedThumbColor = theme.textMuted,
                            uncheckedTrackColor = theme.pillSurface
                        )
                    )
                } else {
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(theme.buttonRadius))
                            .background(theme.accent)
                            .clickable { onRequestPermission() }
                            .padding(horizontal = 12.dp, vertical = 6.dp)
                    ) {
                        Text(
                            text = "Enable",
                            fontSize = 12.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = Color.White
                        )
                    }
                }
            }

            // Quick Actions: Recenter & Overlay Guidance
            if (hasOverlayPermission && isOverlayActive) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.End,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = "Re-center Notch",
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Medium,
                        color = theme.accent,
                        modifier = Modifier
                            .clickable { onRecenterNotch() }
                            .padding(vertical = 4.dp, horizontal = 6.dp)
                    )
                }
            }

            // OEM-specific notice (Xiaomi, Samsung, OnePlus, Oppo, Vivo)
            if (isOemSkin) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(8.dp))
                        .background(theme.pillSurface.copy(alpha = 0.5f))
                        .border(0.5.dp, theme.borderHairline, RoundedCornerShape(8.dp))
                        .padding(horizontal = 10.dp, vertical = 8.dp)
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Text(
                            text = "Tip: Allow \"Display pop-up windows\" & Autostart in Settings to keep notch persistent.",
                            fontSize = 10.sp,
                            color = theme.textSecondary,
                            lineHeight = 14.sp,
                            modifier = Modifier.weight(1f)
                        )
                        Spacer(modifier = Modifier.width(6.dp))
                        Text(
                            text = "App Settings",
                            fontSize = 11.sp,
                            fontWeight = FontWeight.Medium,
                            color = theme.accent,
                            modifier = Modifier
                                .clickable { onOpenAppSettings() }
                                .padding(4.dp)
                        )
                    }
                }
            }
        }
    }
}
