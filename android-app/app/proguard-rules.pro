# GlukoView Insulin Log — keep Retrofit/Gson/Room models if minify is enabled later.
-keepattributes Signature
-keepattributes *Annotation*
-keepclassmembers,allowobfuscation class * {
  @com.google.gson.annotations.SerializedName <fields>;
}
