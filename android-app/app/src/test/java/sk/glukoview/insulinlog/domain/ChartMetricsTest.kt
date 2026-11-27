package sk.glukoview.insulinlog.domain

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import java.time.Instant

class ChartMetricsTest {
    @Test
    fun tir_computesFromReadings() {
        val now = Instant.parse("2026-11-27T12:00:00Z")
        val glucose = listOf(
            // 5.0 mmol ≈ 90 mg/dl — in range
            NightscoutReading(90.0, GlucoseTrend.FLAT, now, "Flat"),
            // 3.0 mmol ≈ 54 — below
            NightscoutReading(54.0, GlucoseTrend.FLAT, now.plusSeconds(300), "Flat"),
            // 12 mmol ≈ 216 — above
            NightscoutReading(216.0, GlucoseTrend.FLAT, now.plusSeconds(600), "Flat"),
            NightscoutReading(100.0, GlucoseTrend.FLAT, now.plusSeconds(900), "Flat"),
        )
        val m = ChartMetrics.compute(glucose, emptyList(), emptyMap(), GlucoseUnit.MMOL)
        assertEquals(4, m.readingCount)
        assertNotNull(m.timeInRangePct)
        // 2 of 4 in range → 50%
        assertEquals(50.0, m.timeInRangePct!!, 0.1)
        assertEquals(25.0, m.belowPct!!, 0.1)
        assertEquals(25.0, m.abovePct!!, 0.1)
    }

    @Test
    fun doses_splitBolusBasal() {
        val bolus = InsulinType(1, "NR", InsulinCategory.BOLUS, "#f00", 5.0, 1, false)
        val basal = InsulinType(2, "Lantus", InsulinCategory.BASAL, "#00f", 24.0, 2, false)
        val t = Instant.parse("2026-11-27T10:00:00Z")
        val doses = listOf(
            DoseRecord(uuid = "a", profileId = 1, insulinId = 1, sceneId = null, units = 3.0, administeredAt = t, glucoseValue = null, glucoseUnit = null, glucoseTrend = null, carbsSj = null, carbsGrams = null, injectionSite = null, note = null),
            DoseRecord(uuid = "b", profileId = 1, insulinId = 2, sceneId = null, units = 12.0, administeredAt = t, glucoseValue = null, glucoseUnit = null, glucoseTrend = null, carbsSj = null, carbsGrams = null, injectionSite = null, note = null),
        )
        val m = ChartMetrics.compute(emptyList(), doses, mapOf(1L to bolus, 2L to basal), GlucoseUnit.MMOL)
        assertEquals(3.0, m.bolusTotal, 0.01)
        assertEquals(12.0, m.basalTotal, 0.01)
        assertEquals(2, m.doseCount)
    }
}
