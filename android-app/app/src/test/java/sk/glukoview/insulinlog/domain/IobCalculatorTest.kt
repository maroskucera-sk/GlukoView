package sk.glukoview.insulinlog.domain

import org.junit.Assert.assertEquals
import org.junit.Test
import java.time.Instant

class IobCalculatorTest {
    @Test
    fun linearIobHalvesAtMidDuration() {
        val insulin = InsulinType(
            id = 1,
            name = "NovoRapid",
            category = InsulinCategory.BOLUS,
            colorHex = "#E53935",
            durationHours = 4.0,
            sortOrder = 1,
            isArchived = false,
        )
        val now = Instant.parse("2026-11-25T12:00:00Z")
        val dose = DoseRecord(
            uuid = "a",
            profileId = 1,
            insulinId = 1,
            sceneId = null,
            units = 4.0,
            administeredAt = Instant.parse("2026-11-25T10:00:00Z"),
            glucoseValue = null,
            glucoseUnit = null,
            glucoseTrend = null,
            carbsSj = null,
            carbsGrams = null,
            injectionSite = null,
            note = null,
        )
        val iob = IobCalculator.activeBolusIob(
            doses = listOf(dose),
            insulins = mapOf(1L to insulin),
            now = now,
        )
        assertEquals(2.0, iob, 0.01)
    }

    @Test
    fun basalIgnored() {
        val insulin = InsulinType(
            id = 2,
            name = "Tresiba",
            category = InsulinCategory.BASAL,
            colorHex = "#1E88E5",
            durationHours = 24.0,
            sortOrder = 1,
            isArchived = false,
        )
        val dose = DoseRecord(
            uuid = "b",
            profileId = 1,
            insulinId = 2,
            sceneId = null,
            units = 10.0,
            administeredAt = Instant.parse("2026-11-25T11:00:00Z"),
            glucoseValue = null,
            glucoseUnit = null,
            glucoseTrend = null,
            carbsSj = null,
            carbsGrams = null,
            injectionSite = null,
            note = null,
        )
        val iob = IobCalculator.activeBolusIob(
            doses = listOf(dose),
            insulins = mapOf(2L to insulin),
            now = Instant.parse("2026-11-25T12:00:00Z"),
        )
        assertEquals(0.0, iob, 0.001)
    }
}
