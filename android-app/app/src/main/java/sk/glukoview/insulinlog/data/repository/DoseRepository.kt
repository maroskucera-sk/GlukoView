package sk.glukoview.insulinlog.data.repository

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import sk.glukoview.insulinlog.data.local.AppDatabase
import sk.glukoview.insulinlog.data.local.DoseEntity
import sk.glukoview.insulinlog.data.remote.DtoMappers.toDomain
import sk.glukoview.insulinlog.domain.DoseRecord
import java.time.Instant
import java.util.UUID

class DoseRepository(
    private val db: AppDatabase,
) {
    fun observeRecent(limit: Int = 200): Flow<List<DoseRecord>> =
        db.doses().observeRecent(limit).map { list -> list.map { it.toDomain() } }

    fun observeLatest(): Flow<DoseRecord?> =
        db.doses().observeLatest().map { it?.toDomain() }

    fun observeBetween(from: Instant, to: Instant): Flow<List<DoseRecord>> =
        db.doses().observeBetween(from.toEpochMilli(), to.toEpochMilli())
            .map { list -> list.map { it.toDomain() } }

    suspend fun saveLocal(dose: DoseRecord): DoseRecord {
        val now = System.currentTimeMillis()
        val existing = db.doses().getByUuid(dose.uuid)
        val entity = DoseEntity(
            localId = existing?.localId ?: 0,
            serverId = existing?.serverId,
            uuid = dose.uuid.ifBlank { UUID.randomUUID().toString() },
            profileId = dose.profileId,
            insulinId = dose.insulinId,
            sceneId = dose.sceneId,
            units = dose.units,
            administeredAtEpochMs = dose.administeredAt.toEpochMilli(),
            glucoseValue = dose.glucoseValue,
            glucoseUnit = dose.glucoseUnit?.toApi(),
            glucoseTrend = dose.glucoseTrend,
            carbsSj = dose.carbsSj,
            carbsGrams = dose.carbsGrams,
            injectionSite = dose.injectionSite?.apiValue,
            note = dose.note,
            deletedAtEpochMs = null,
            updatedAtEpochMs = now,
            syncPending = true,
        )
        val id = db.doses().upsert(entity)
        return entity.copy(localId = if (entity.localId == 0L) id else entity.localId).toDomain()
    }

    suspend fun softDeleteLocal(uuid: String) {
        val existing = db.doses().getByUuid(uuid) ?: return
        db.doses().upsert(
            existing.copy(
                deletedAtEpochMs = System.currentTimeMillis(),
                updatedAtEpochMs = System.currentTimeMillis(),
                syncPending = true,
            ),
        )
    }
}
