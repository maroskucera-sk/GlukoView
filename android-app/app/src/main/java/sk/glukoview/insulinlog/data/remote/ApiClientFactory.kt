package sk.glukoview.insulinlog.data.remote

import com.google.gson.GsonBuilder
import okhttp3.Interceptor
import okhttp3.OkHttpClient
import okhttp3.logging.HttpLoggingInterceptor
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import sk.glukoview.insulinlog.BuildConfig
import java.util.concurrent.TimeUnit

object ApiClientFactory {
    fun create(baseUrl: String, accessKey: String): InsulinLogApi {
        val normalized = baseUrl.trim().trimEnd('/') + "/"
        // serializeNulls: inak Gson vynechá null a server nerozpozná zrušenie current_injection_site
        val gson = GsonBuilder().serializeNulls().create()

        val auth = Interceptor { chain ->
            val req = chain.request().newBuilder()
                .header("Authorization", "Bearer $accessKey")
                .header("Accept", "application/json")
                .build()
            chain.proceed(req)
        }

        val logging = HttpLoggingInterceptor().apply {
            level = if (BuildConfig.DEBUG) {
                HttpLoggingInterceptor.Level.BASIC
            } else {
                HttpLoggingInterceptor.Level.NONE
            }
        }

        val client = OkHttpClient.Builder()
            .connectTimeout(20, TimeUnit.SECONDS)
            .readTimeout(30, TimeUnit.SECONDS)
            .addInterceptor(auth)
            .addInterceptor(logging)
            .build()

        return Retrofit.Builder()
            .baseUrl(normalized)
            .client(client)
            .addConverterFactory(GsonConverterFactory.create(gson))
            .build()
            .create(InsulinLogApi::class.java)
    }
}
