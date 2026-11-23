package com.edgedrop.ui

import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
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
import com.edgedrop.R
import com.edgedrop.service.EdgeNotchService
import com.edgedrop.ui.components.TintedIcon
import com.edgedrop.ui.theme.EdgeDropTheme
import com.edgedrop.ui.theme.Vibe
import com.edgedrop.util.FileUtils
import com.edgedrop.util.PreferencesManager
import java.io.File

class MainActivity : ComponentActivity() {

    private var incomingFile by mutableStateOf<File?>(null)
    private var hasOverlayPermission by mutableStateOf(false)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        handleIntent(intent)

        setContent {
            val prefs = remember { PreferencesManager(this) }
            val currentVibe = remember { Vibe.fromId(prefs.vibe) }

            EdgeDropTheme(vibe = currentVibe) {
                val theme = EdgeDropTheme.tokens

                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .background(theme.surface)
                        .padding(16.dp),
                    contentAlignment = Alignment.Center
                ) {
                    if (!hasOverlayPermission && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && !Settings.canDrawOverlays(this@MainActivity)) {
                        OverlayPermissionCard(
                            onRequestPermission = {
                                val intent = Intent(
                                    Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                                    Uri.parse("package:$packageName")
                                )
                                startActivity(intent)
                            },
                            onContinueWithoutOverlay = {
                                hasOverlayPermission = true
                            }
                        )
                    } else {
                        EdgeDropCard(
                            initialFile = incomingFile,
                            onClose = { finish() }
                        )
                    }
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            hasOverlayPermission = Settings.canDrawOverlays(this)
            if (hasOverlayPermission) {
                EdgeNotchService.start(this)
            }
        } else {
            hasOverlayPermission = true
            EdgeNotchService.start(this)
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

@Composable
fun OverlayPermissionCard(
    onRequestPermission: () -> Unit,
    onContinueWithoutOverlay: () -> Unit,
    modifier: Modifier = Modifier
) {
    val theme = EdgeDropTheme.tokens

    Box(
        modifier = modifier
            .fillMaxWidth()
            .widthIn(max = 380.dp)
            .clip(RoundedCornerShape(theme.cardRadius))
            .background(theme.surfaceElevated)
            .border(1.dp, theme.borderHairline, RoundedCornerShape(theme.cardRadius))
            .padding(24.dp),
        contentAlignment = Alignment.Center
    ) {
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(14.dp)
        ) {
            Box(
                modifier = Modifier
                    .size(48.dp)
                    .clip(CircleShape)
                    .background(theme.pillSurface)
                    .border(1.dp, theme.borderHairline, CircleShape),
                contentAlignment = Alignment.Center
            ) {
                TintedIcon(
                    resId = R.drawable.ic_drop,
                    color = theme.accent,
                    size = 24.dp
                )
            }

            Text(
                text = "Enable Floating Edge Notch",
                fontSize = 17.sp,
                fontWeight = FontWeight.Bold,
                color = theme.textPrimary,
                textAlign = TextAlign.Center
            )

            Text(
                text = "EdgeDrop lives as a calm, unobtrusive pill on the edge of your screen. Shrink photos, PDFs, and documents instantly from any app without switching windows.",
                fontSize = 12.sp,
                color = theme.textSecondary,
                textAlign = TextAlign.Center,
                lineHeight = 18.sp
            )

            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(theme.buttonRadius))
                    .background(theme.accent)
                    .clickable { onRequestPermission() }
                    .padding(vertical = 11.dp),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = "Grant Overlay Permission",
                    fontSize = 13.sp,
                    fontWeight = FontWeight.SemiBold,
                    color = Color.White
                )
            }

            Text(
                text = "Or use directly in app",
                fontSize = 12.sp,
                color = theme.textMuted,
                modifier = Modifier.clickable { onContinueWithoutOverlay() }
            )
        }
    }
}
