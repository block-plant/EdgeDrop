-keep class com.edgedrop.core.** { *; }
-keepclassmembers class com.edgedrop.core.** {
    native <methods>;
}
-keepclasseswithmembernames class * {
    native <methods>;
}
