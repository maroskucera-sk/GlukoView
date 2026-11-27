package sk.glukoview.insulinlog.domain

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.time.Instant

class NightscoutGlucosePickerTest {
    @Test
    fun isNearNow_withinThreeMinutes() {
        val now = Instant.parse("2026-11-27T10:00:00Z")
        assertTrue(NightscoutGlucosePicker.isNearNow(now.minusSeconds(60), now))
        assertTrue(NightscoutGlucosePicker.isNearNow(now.plusSeconds(90), now))
        assertFalse(NightscoutGlucosePicker.isNearNow(now.minusSeconds(5 * 60), now))
    }

    @Test
    fun closest_picksNearestReading() {
        val at = Instant.parse("2026-11-27T12:00:00Z")
        val readings = listOf(
            reading(at.minusSeconds(600), 100.0),
            reading(at.minusSeconds(120), 120.0),
            reading(at.plusSeconds(300), 140.0),
        )
        val closest = NightscoutGlucosePicker.closest(readings, at)!!
        assertEquals(120.0, closest.sgvMgdl, 0.01)
    }

    private fun reading(at: Instant, sgv: Double) = NightscoutReading(
        sgvMgdl = sgv,
        trend = GlucoseTrend.FLAT,
        date = at,
        directionRaw = "Flat",
    )
}
