package com.edgedrop.ui.theme

enum class Vibe(val id: String, val displayName: String) {
    Adaptive("adaptive", "Adaptive"),
    Obsidian("obsidian", "Obsidian"),
    Porcelain("porcelain", "Porcelain"),
    Mist("mist", "Mist");

    companion object {
        fun fromId(id: String): Vibe {
            return entries.firstOrNull { it.id.equals(id, ignoreCase = true) } ?: Adaptive
        }
    }
}
