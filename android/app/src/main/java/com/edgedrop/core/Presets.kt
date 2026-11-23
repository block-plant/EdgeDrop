package com.edgedrop.core

data class Preset(
    val id: String,
    val name: String,
    val category: String,
    val format: String,
    val targetKb: Int,
    val minKb: Int = 0,
    val maxKb: Int = 0,
    val width: Int = 0,
    val height: Int = 0,
    val dpi: Int = 300,
    val specSummary: String
)

object PresetRegistry {
    val presets = listOf(
        Preset(
            id = "gate_photo",
            name = "GATE Photo",
            category = "Photo",
            format = "JPG",
            targetKb = 250,
            minKb = 5,
            maxKb = 600,
            width = 480,
            height = 640,
            dpi = 300,
            specSummary = "JPG · 480×640 px · 300 DPI · 5–600 KB"
        ),
        Preset(
            id = "gate_sig",
            name = "GATE Signature",
            category = "Signature",
            format = "JPG",
            targetKb = 150,
            minKb = 3,
            maxKb = 300,
            width = 480,
            height = 150,
            dpi = 300,
            specSummary = "JPG · 480×150 px · 300 DPI · 3–300 KB"
        ),
        Preset(
            id = "gate_pdf",
            name = "GATE PDF",
            category = "Document",
            format = "PDF",
            targetKb = 500,
            minKb = 10,
            maxKb = 600,
            dpi = 150,
            specSummary = "PDF · Document · 150 DPI · < 600 KB"
        ),
        Preset(
            id = "passport_photo",
            name = "Passport Seva",
            category = "Photo",
            format = "JPG",
            targetKb = 200,
            minKb = 10,
            maxKb = 250,
            width = 630,
            height = 810,
            dpi = 300,
            specSummary = "JPG · 630×810 px · 300 DPI · 10–250 KB"
        ),
        Preset(
            id = "pan_photo",
            name = "PAN Photo",
            category = "Photo",
            format = "JPG",
            targetKb = 45,
            minKb = 5,
            maxKb = 50,
            width = 213,
            height = 213,
            dpi = 200,
            specSummary = "JPG · 213×213 px · 200 DPI · < 50 KB"
        ),
        Preset(
            id = "pan_sig",
            name = "PAN Signature",
            category = "Signature",
            format = "JPG",
            targetKb = 25,
            minKb = 2,
            maxKb = 30,
            width = 400,
            height = 200,
            dpi = 200,
            specSummary = "JPG · 400×200 px · 200 DPI · < 30 KB"
        )
    )
}
