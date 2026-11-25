package sk.glukoview.insulinlog.domain

import java.time.Duration
import java.time.Instant

data class DuplicateBolusWarning(
    val recent: DoseRecord,
    val minutesThreshold: Int,
)

object DuplicateBolusChecker {
    fun findRecentBolus(
        candidateInsulin: InsulinType,
        candidateAt: Instant,
        candidateUuid: String?,
        existingDoses: List<DoseRecord>,
        insulins: Map<Long, InsulinType>,
        minutesThreshold: Int = 20,
    ): DuplicateBolusWarning? {
        if (candidateInsulin.category != InsulinCategory.BOLUS) return null
        if (minutesThreshold <= 0) return null

        val threshold = Duration.ofMinutes(minutesThreshold.toLong())
        return existingDoses
            .asSequence()
            .filter { it.deletedAt == null }
            .filter { candidateUuid == null || it.uuid != candidateUuid }
            .filter { insulins[it.insulinId]?.category == InsulinCategory.BOLUS }
            .mapNotNull { dose ->
                val delta = Duration.between(dose.administeredAt, candidateAt).abs()
                if (delta < threshold) dose to delta else null
            }
            .minByOrNull { it.second }
            ?.let { DuplicateBolusWarning(it.first, minutesThreshold) }
    }
}
