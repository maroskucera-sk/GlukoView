package sk.glukoview.insulinlog.domain

import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import java.time.Instant

class DuplicateBolusCheckerTest {
    private val bolus = InsulinType(
        id = 1, name = "NR", category = InsulinCategory.BOLUS,
        colorHex = "#E53935", durationHours = 4.0, sortOrder = 1, isArchived = false,
    )

    @Test
    fun warnsWithin20Minutes() {
        val recent = DoseRecord(
            uuid = "old",
            profileId = 1,
            insulinId = 1,
            sceneId = null,
            units = 2.0,
            administeredAt = Instant.parse("2026-11-25T12:00:00Z"),
            glucoseValue = null, glucoseUnit = null, glucoseTrend = null,
            carbsSj = null, carbsGrams = null, injectionSite = null, note = null,
        )
        val warning = DuplicateBolusChecker.findRecentBolus(
            candidateInsulin = bolus,
            candidateAt = Instant.parse("2026-11-25T12:10:00Z"),
            candidateUuid = "new",
            existingDoses = listOf(recent),
            insulins = mapOf(1L to bolus),
            minutesThreshold = 20,
        )
        assertNotNull(warning)
    }

    @Test
    fun noWarnAfterThreshold() {
        val recent = DoseRecord(
            uuid = "old",
            profileId = 1,
            insulinId = 1,
            sceneId = null,
            units = 2.0,
            administeredAt = Instant.parse("2026-11-25T12:00:00Z"),
            glucoseValue = null, glucoseUnit = null, glucoseTrend = null,
            carbsSj = null, carbsGrams = null, injectionSite = null, note = null,
        )
        val warning = DuplicateBolusChecker.findRecentBolus(
            candidateInsulin = bolus,
            candidateAt = Instant.parse("2026-11-25T12:25:00Z"),
            candidateUuid = "new",
            existingDoses = listOf(recent),
            insulins = mapOf(1L to bolus),
            minutesThreshold = 20,
        )
        assertNull(warning)
    }
}
