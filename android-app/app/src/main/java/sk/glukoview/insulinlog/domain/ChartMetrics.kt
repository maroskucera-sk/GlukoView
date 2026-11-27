package sk.glukoview.insulinlog.domain

/**
 * Súhrny pre grafové obdobie (TIR, priemer, dávky).
 * Cieľové pásmo: 3.9–10 mmol/L (70–180 mg/dL) — bežný TIR rozsah.
 */
data class ChartMetrics(
    val readingCount: Int = 0,
    val average: Double? = null,
    val min: Double? = null,
    val max: Double? = null,
    /** Percento času v cieľovom pásme (0–100), podľa počtu CGM bodov. */
    val timeInRangePct: Double? = null,
    val belowPct: Double? = null,
    val abovePct: Double? = null,
    val bolusTotal: Double = 0.0,
    val basalTotal: Double = 0.0,
    val doseCount: Int = 0,
) {
    companion object {
        fun targetLow(unit: GlucoseUnit): Double =
            if (unit == GlucoseUnit.MMOL) 3.9 else 70.0

        fun targetHigh(unit: GlucoseUnit): Double =
            if (unit == GlucoseUnit.MMOL) 10.0 else 180.0

        fun compute(
            glucose: List<NightscoutReading>,
            doses: List<DoseRecord>,
            insulins: Map<Long, InsulinType>,
            unit: GlucoseUnit,
        ): ChartMetrics {
            val values = glucose.map {
                GlucoseUnits.convert(it.sgvMgdl, GlucoseUnit.MGDL, unit)
            }
            val lo = targetLow(unit)
            val hi = targetHigh(unit)
            val inRange = values.count { it in lo..hi }
            val below = values.count { it < lo }
            val above = values.count { it > hi }
            val n = values.size.coerceAtLeast(1)

            var bolus = 0.0
            var basal = 0.0
            var doseCount = 0
            for (d in doses) {
                if (d.deletedAt != null) continue
                doseCount++
                when (insulins[d.insulinId]?.category) {
                    InsulinCategory.BASAL -> basal += d.units
                    else -> bolus += d.units
                }
            }

            return ChartMetrics(
                readingCount = values.size,
                average = values.takeIf { it.isNotEmpty() }?.average(),
                min = values.minOrNull(),
                max = values.maxOrNull(),
                timeInRangePct = if (values.isEmpty()) null else inRange * 100.0 / n,
                belowPct = if (values.isEmpty()) null else below * 100.0 / n,
                abovePct = if (values.isEmpty()) null else above * 100.0 / n,
                bolusTotal = (bolus * 100).toInt() / 100.0,
                basalTotal = (basal * 100).toInt() / 100.0,
                doseCount = doseCount,
            )
        }
    }
}
