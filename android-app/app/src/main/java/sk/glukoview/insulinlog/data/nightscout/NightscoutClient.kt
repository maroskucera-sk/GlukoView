package sk.glukoview.insulinlog.data.nightscout

import okhttp3.Interceptor
import okhttp3.OkHttpClient
import okhttp3.logging.HttpLoggingInterceptor
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory
import sk.glukoview.insulinlog.BuildConfig
import sk.glukoview.insulinlog.domain.GlucoseTrend
import sk.glukoview.insulinlog.domain.GlucoseUnit
import sk.glukoview.insulinlog.domain.GlucoseUnits
import sk.glukoview.insulinlog.domain.NightscoutGlucosePicker
import sk.glukoview.insulinlog.domain.NightscoutReading
import java.time.Duration
import java.time.Instant
import java.time.temporal.ChronoUnit
import java.util.concurrent.TimeUnit
import kotlin.math.ceil

/**
 * Nightscout stratégia:
 * - Bežná práca appky: stiahne len aktuálne 1–2 merania (dashboard, predvyplnenie).
 * - CGM históriu neukladáme do Room — zdroj pravdy ostáva Nightscout.
 * - Export pre lekára: pri exporte on-demand stiahne entries za zvolené obdobie a zlúči s inzulínom.
 */
class NightscoutClient {
    suspend fun fetchLatest(
        baseUrl: String,
        tokenOrSecret: String?,
    ): Result<Pair<NightscoutReading, NightscoutReading?>> = runCatching {
        val api = createApi(baseUrl, tokenOrSecret)
        val entries = api.latestEntries(count = 2)
            .mapNotNull { it.toReading() }
            .sortedByDescending { it.date }

        val latest = entries.firstOrNull()
            ?: error("Nightscout nevrátil žiadne meranie.")
        val previous = entries.getOrNull(1)
        latest to previous
    }

    /**
     * Stiahne glykémie za [from, to]. Najprv skúsi date-filter API, pri chybe fallback na count.
     */
    suspend fun fetchRange(
        baseUrl: String,
        tokenOrSecret: String?,
        from: Instant,
        to: Instant,
    ): Result<List<NightscoutReading>> = runCatching {
        val api = createApi(baseUrl, tokenOrSecret)
        val fromMs = from.toEpochMilli()
        val toMs = to.toEpochMilli()
        val days = ceil(Duration.between(from, to).toMinutes().coerceAtLeast(1) / 1440.0).toInt()
        // ~288 CGM bodov / deň (5 min) + rezerva
        val estimated = (days * 320).coerceIn(500, 50_000)

        val raw = runCatching {
            api.entriesByDate(gteMs = fromMs, lteMs = toMs, count = estimated)
        }.getOrElse {
            api.latestEntries(count = estimated)
        }

        raw.mapNotNull { it.toReading() }
            .filter { !it.date.isBefore(from) && !it.date.isAfter(to) }
            .sortedBy { it.date }
    }

    /**
     * Najbližšie CGM meranie k [at] (okno ±[windowMinutes], pri prázdne rozšírenie na ±60).
     * Pre „teraz“ radšej použite [fetchLatest].
     */
    suspend fun fetchClosestTo(
        baseUrl: String,
        tokenOrSecret: String?,
        at: Instant,
        windowMinutes: Long = 20,
    ): Result<NightscoutReading> {
        return try {
            suspend fun load(minutes: Long): List<NightscoutReading> {
                val from = at.minus(minutes, ChronoUnit.MINUTES)
                val to = at.plus(minutes, ChronoUnit.MINUTES)
                return fetchRange(baseUrl, tokenOrSecret, from, to).getOrElse { emptyList() }
            }
            val near = load(windowMinutes).ifEmpty { load(60) }
            val reading = NightscoutGlucosePicker.closest(near, at)
                ?: error("Nightscout nemá meranie okolo zvoleného času.")
            Result.success(reading)
        } catch (e: Throwable) {
            Result.failure(e)
        }
    }

    fun formatAge(reading: NightscoutReading, now: Instant = Instant.now()): String {
        val minutes = Duration.between(reading.date, now).toMinutes().coerceAtLeast(0)
        return when {
            minutes < 1 -> "pred chvíľou"
            minutes == 1L -> "pred 1 min"
            minutes < 60 -> "pred $minutes min"
            else -> {
                val h = minutes / 60
                val m = minutes % 60
                if (m == 0L) "pred ${h}h" else "pred ${h}h ${m}m"
            }
        }
    }

    fun formatDelta(
        latest: NightscoutReading,
        previous: NightscoutReading?,
        unit: GlucoseUnit,
    ): String? {
        if (previous == null) return null
        val a = GlucoseUnits.convert(latest.sgvMgdl, GlucoseUnit.MGDL, unit)
        val b = GlucoseUnits.convert(previous.sgvMgdl, GlucoseUnit.MGDL, unit)
        val delta = a - b
        val formatted = when (unit) {
            GlucoseUnit.MMOL -> String.format("%+.1f", delta)
            GlucoseUnit.MGDL -> String.format("%+.0f", delta)
        }
        return "$formatted ${if (unit == GlucoseUnit.MMOL) "mmol/L" else "mg/dL"}"
    }

    private fun NightscoutEntryDto.toReading(): NightscoutReading? {
        val sgv = sgv ?: return null
        val epoch = date
            ?: dateString?.let { runCatching { Instant.parse(it).toEpochMilli() }.getOrNull() }
            ?: return null
        return NightscoutReading(
            sgvMgdl = sgv,
            trend = GlucoseTrend.fromNightscout(direction),
            date = Instant.ofEpochMilli(epoch),
            directionRaw = direction,
        )
    }

    private fun createApi(baseUrl: String, tokenOrSecret: String?): NightscoutApi {
        var url = baseUrl.trim().trimEnd('/')
        if (url.endsWith("/api/v1", ignoreCase = true)) {
            url = url.dropLast("/api/v1".length)
        }
        url += "/"

        val auth = Interceptor { chain ->
            val original = chain.request()
            val builder = original.newBuilder()
            val token = tokenOrSecret?.trim().orEmpty()
            if (token.isNotEmpty()) {
                builder.header("api-secret", token)
                val withToken = original.url.newBuilder()
                    .addQueryParameter("token", token)
                    .build()
                builder.url(withToken)
            }
            chain.proceed(builder.build())
        }

        val logging = HttpLoggingInterceptor().apply {
            level = if (BuildConfig.DEBUG) HttpLoggingInterceptor.Level.BASIC
            else HttpLoggingInterceptor.Level.NONE
        }

        val client = OkHttpClient.Builder()
            .connectTimeout(20, TimeUnit.SECONDS)
            .readTimeout(60, TimeUnit.SECONDS)
            .addInterceptor(auth)
            .addInterceptor(logging)
            .build()

        return Retrofit.Builder()
            .baseUrl(url)
            .client(client)
            .addConverterFactory(GsonConverterFactory.create())
            .build()
            .create(NightscoutApi::class.java)
    }
}
