package sk.glukoview.insulinlog.data.repository

import sk.glukoview.insulinlog.data.local.AppDatabase
import sk.glukoview.insulinlog.data.local.SyncMetaEntity
import sk.glukoview.insulinlog.data.local.UserPreferences
import sk.glukoview.insulinlog.data.remote.ApiClientFactory
import sk.glukoview.insulinlog.data.remote.DtoMappers
import sk.glukoview.insulinlog.data.remote.DtoMappers.toEntity
import sk.glukoview.insulinlog.data.remote.DtoMappers.toUpsertRequest
import kotlinx.coroutines.flow.first
import java.time.Instant

class SyncRepository(
    private val db: AppDatabase,
    private val prefs: UserPreferences,
) {
    suspend fun syncNow(): Result<Unit> = runCatching {
        val session = prefs.session.first()
        val url = session.serverUrl ?: error("Chýba URL servera.")
        val key = session.accessKey ?: error("Chýba prístupový kľúč.")
        val api = ApiClientFactory.create(url, key)

        // 1) Push offline fronty
        val pending = db.doses().getPending()
        for (dose in pending) {
            if (dose.deletedAtEpochMs != null) {
                runCatching { api.deleteDose(dose.uuid) }
                db.doses().markSynced(dose.uuid, dose.serverId, System.currentTimeMillis())
            } else {
                val response = api.upsertDose(dose.toDomainUpsert())
                val existing = db.doses().getByUuid(dose.uuid)
                db.doses().upsert(
                    response.dose.toEntity(
                        existingLocalId = existing?.localId ?: dose.localId,
                        syncPending = false,
                    ),
                )
            }
        }

        // 2) Pull delta
        val meta = db.syncMeta().get()
        val since = meta?.lastSyncEpochMs?.let { Instant.ofEpochMilli(it).toString() }
        val remote = api.sync(since)

        if (remote.profiles.isNotEmpty()) {
            db.profiles().upsertAll(remote.profiles.map { it.toEntity() })
        }
        if (remote.insulins.isNotEmpty()) {
            db.insulins().upsertAll(remote.insulins.map { it.toEntity() })
        }
        if (remote.scenes.isNotEmpty()) {
            db.scenes().upsertAll(remote.scenes.map { it.toEntity() })
        }
        remote.settings?.let { db.settings().upsert(it.toEntity()) }

        if (remote.doses.isNotEmpty()) {
            val mapped = remote.doses.map { dto ->
                val existing = db.doses().getByUuid(dto.uuid)
                // Neprepíš lokálny pending záznam starším serverovým stavom
                if (existing?.syncPending == true) {
                    existing
                } else {
                    dto.toEntity(existingLocalId = existing?.localId ?: 0, syncPending = false)
                }
            }
            db.doses().upsertAll(mapped)
        }

        val serverMs = DtoMappers.parseInstant(remote.serverTime)?.toEpochMilli()
            ?: System.currentTimeMillis()
        db.syncMeta().upsert(
            SyncMetaEntity(
                lastSyncEpochMs = serverMs,
                familyId = meta?.familyId,
                familyName = meta?.familyName,
            ),
        )
    }

    private fun sk.glukoview.insulinlog.data.local.DoseEntity.toDomainUpsert() =
        DtoMappers.run { toDomain().toUpsertRequest() }
}
