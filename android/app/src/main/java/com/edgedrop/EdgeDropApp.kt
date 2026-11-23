package com.edgedrop

import android.app.Application
import com.edgedrop.util.FileUtils

class EdgeDropApp : Application() {

    override fun onCreate() {
        super.onCreate()
        // Clean temporary caches on app initialization
        FileUtils.clearCache(this)
    }
}
