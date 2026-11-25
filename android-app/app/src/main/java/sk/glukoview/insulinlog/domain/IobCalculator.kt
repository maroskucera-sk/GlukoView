package sk.glukoview.insulinlog.domain

import java.time.Duration
import java.time.Instant
import kotlin.math.max

/**
 * Orientačný lineárny IOB (nie medicínsky model).
 *
 * zostávajúce = units * max(0, 1 - elapsed / duration)
 */
object IobCalculator {
    fun activeBolusIob(
        doses: List<DoseRecord>,
        insulins: Map<Long, InsulinType>,
        now: Instant = Instant.now(),
    ): Double {
        var total = 0.0
        for (dose in doses) {
            if (dose.deletedAt != null) continue
            val insulin = insulins[dose.insulinId] ?: continue
            if (insulin.category != InsulinCategory.BOLUS) continue
            val durationH = insulin.durationHours.coerceAtLeast(0.1)
            val elapsedH = Duration.between(dose.administeredAt, now).toMillis() / 3_600_000.0
            if (elapsedH < 0) continue
            val remaining = dose.units * max(0.0, 1.0 - elapsedH / durationH)
            total += remaining
        }
        return (total * 100).toInt() / 100.0
    }
}
