package sk.glukoview.insulinlog.data.repository

import sk.glukoview.insulinlog.data.local.AppDatabase
import sk.glukoview.insulinlog.data.local.SyncMetaEntity
import sk.glukoview.insulinlog.data.local.UserPreferences
import sk.glukoview.insulinlog.data.remote.ApiClientFactory
import sk.glukoview.insulinlog.data.remote.DtoMappers.toEntity
import sk.glukoview.insulinlog.domain.Profile

class AuthRepository(
    private val db: AppDatabase,
    private val prefs: UserPreferences,
) {
    suspend fun verifyAndStore(serverUrl: String, accessKey: String): Result<List<Profile>> =
        runCatching {
            val api = ApiClientFactory.create(serverUrl, accessKey)
            val response = api.verify()
            db.profiles().upsertAll(response.profiles.map { it.toEntity() })
            response.settings?.let { db.settings().upsert(it.toEntity()) }
            db.syncMeta().upsert(
                SyncMetaEntity(
                    familyId = response.family.id,
                    familyName = response.family.name,
                    lastSyncEpochMs = null,
                ),
            )
            prefs.saveServer(serverUrl, accessKey)
            response.profiles.filter { it.isActive }.map {
                Profile(it.id, it.name, it.colorHex, it.sortOrder, it.isActive)
            }
        }

    suspend fun selectProfile(profileId: Long) {
        prefs.saveProfile(profileId)
    }

    suspend fun logout() {
        prefs.clearSession()
    }
}
