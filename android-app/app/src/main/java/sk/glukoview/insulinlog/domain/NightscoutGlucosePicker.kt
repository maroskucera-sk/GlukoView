package sk.glukoview.insulinlog.domain

import java.time.Duration
import java.time.Instant
import kotlin.math.abs

/**
 * Výber CGM bodu k času dávky a rozhodnutie „teraz vs. spätne“.
 */
object NightscoutGlucosePicker {
    /** Ak je čas dávky v tomto okne okolo teraz → berieme aktuálne NS. */
    const val NEAR_NOW_MINUTES: Long = 3

    fun isNearNow(at: Instant, now: Instant = Instant.now(), minutes: Long = NEAR_NOW_MINUTES): Boolean =
        Duration.between(at, now).abs().toMinutes() <= minutes

    fun closest(
        readings: List<NightscoutReading>,
        at: Instant,
    ): NightscoutReading? {
        if (readings.isEmpty()) return null
        return readings.minByOrNull { abs(Duration.between(it.date, at).toMillis()) }
    }
}
