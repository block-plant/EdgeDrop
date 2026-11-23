package com.edgedrop.util

import android.content.Context
import android.content.SharedPreferences

class PreferencesManager(context: Context) {
    private val prefs: SharedPreferences = context.getSharedPreferences("edgedrop_prefs", Context.MODE_PRIVATE)

    var vibe: String
        get() = prefs.getString("vibe", "adaptive") ?: "adaptive"
        set(value) = prefs.edit().putString("vibe", value).apply()

    var edgeSide: String
        get() = prefs.getString("edge_side", "right") ?: "right"
        set(value) = prefs.edit().putString("edge_side", value).apply()

    var edgeYRatio: Float
        get() = prefs.getFloat("edge_y_ratio", 0.35f).coerceIn(0.18f, 0.65f)
        set(value) = prefs.edit().putFloat("edge_y_ratio", value.coerceIn(0.18f, 0.65f)).apply()

    var isPinned: Boolean
        get() = prefs.getBoolean("is_pinned", false)
        set(value) = prefs.edit().putBoolean("is_pinned", value).apply()

    var isOverlayEnabled: Boolean
        get() = prefs.getBoolean("is_overlay_enabled", true)
        set(value) = prefs.edit().putBoolean("is_overlay_enabled", value).apply()

    var recentFiles: List<String>
        get() {
            val set = prefs.getStringSet("recent_files", emptySet()) ?: emptySet()
            return set.toList()
        }
        set(value) {
            val limited = value.take(5).toSet()
            prefs.edit().putStringSet("recent_files", limited).apply()
        }

    fun addRecentFile(path: String) {
        val current = recentFiles.toMutableList()
        current.remove(path)
        current.add(0, path)
        recentFiles = current
    }
}
