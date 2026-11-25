package sk.glukoview.insulinlog.data.repository

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.map
import sk.glukoview.insulinlog.data.local.AppDatabase
import sk.glukoview.insulinlog.data.local.UserPreferences
import sk.glukoview.insulinlog.data.remote.ApiClientFactory
import sk.glukoview.insulinlog.data.remote.DtoMappers.toDomain
import sk.glukoview.insulinlog.data.remote.DtoMappers.toEntity
import sk.glukoview.insulinlog.domain.FamilySettings
import sk.glukoview.insulinlog.domain.GlucoseUnit
import sk.glukoview.insulinlog.domain.InsulinCategory
import sk.glukoview.insulinlog.domain.InsulinType
import sk.glukoview.insulinlog.domain.Profile
import sk.glukoview.insulinlog.domain.Scene

class CatalogRepository(
    private val db: AppDatabase,
    private val prefs: UserPreferences,
) {
    fun observeProfiles(): Flow<List<Profile>> =
        db.profiles().observeActive().map { list -> list.map { it.toDomain() } }

    fun observeInsulins(): Flow<List<InsulinType>> =
        db.insulins().observeActive().map { list -> list.map { it.toDomain() } }

    fun observeAllInsulins(): Flow<List<InsulinType>> =
        db.insulins().observeAll().map { list -> list.map { it.toDomain() } }

    suspend fun getAllInsulins(): List<InsulinType> =
        db.insulins().getAll().map { it.toDomain() }

    fun observeScenes(): Flow<List<Scene>> =
        db.scenes().observeActive().map { list -> list.map { it.toDomain() } }

    fun observeAllScenes(): Flow<List<Scene>> =
        db.scenes().observeAll().map { list -> list.map { it.toDomain() } }

    fun observeSettings(): Flow<FamilySettings?> =
        db.settings().observe().map { it?.toDomain() }

    suspend fun getSettings(): FamilySettings? =
        db.settings().get()?.toDomain()

    private suspend fun api() = run {
        val session = prefs.session.first()
        val url = session.serverUrl ?: error("Chýba URL servera.")
        val key = session.accessKey ?: error("Chýba prístupový kľúč.")
        ApiClientFactory.create(url, key)
    }

    suspend fun saveSettings(
        nightscoutUrl: String?,
        nightscoutToken: String?,
        basalReminderTime: String?,
        glucoseUnit: GlucoseUnit,
        duplicateBolusMinutes: Int,
    ): Result<FamilySettings> = runCatching {
        val body = mapOf(
            "nightscout_url" to nightscoutUrl,
            "nightscout_token" to nightscoutToken,
            "basal_reminder_time" to basalReminderTime,
            "glucose_unit" to glucoseUnit.toApi(),
            "duplicate_bolus_minutes" to duplicateBolusMinutes,
        )
        val response = api().putSettings(body)
        val entity = response.settings.toEntity()
        db.settings().upsert(entity)
        entity.toDomain()
    }

    suspend fun createInsulin(
        name: String,
        category: InsulinCategory,
        colorHex: String,
        durationHours: Double,
    ): Result<InsulinType> = runCatching {
        val response = api().createInsulin(
            mapOf(
                "name" to name,
                "category" to category.toApi(),
                "color_hex" to colorHex,
                "duration_hours" to durationHours,
            ),
        )
        val dto = response["insulin"] ?: error("Server nevrátil inzulín.")
        db.insulins().upsertAll(listOf(dto.toEntity()))
        dto.toEntity().toDomain()
    }

    suspend fun updateInsulin(
        id: Long,
        name: String,
        category: InsulinCategory,
        colorHex: String,
        durationHours: Double,
        isArchived: Boolean,
    ): Result<InsulinType> = runCatching {
        val response = api().updateInsulin(
            id,
            mapOf(
                "name" to name,
                "category" to category.toApi(),
                "color_hex" to colorHex,
                "duration_hours" to durationHours,
                "is_archived" to isArchived,
            ),
        )
        val dto = response["insulin"] ?: error("Server nevrátil inzulín.")
        db.insulins().upsertAll(listOf(dto.toEntity()))
        dto.toEntity().toDomain()
    }

    suspend fun createScene(name: String, iconKey: String): Result<Scene> = runCatching {
        val response = api().createScene(
            mapOf("name" to name, "icon_key" to iconKey),
        )
        val dto = response["scene"] ?: error("Server nevrátil scénu.")
        db.scenes().upsertAll(listOf(dto.toEntity()))
        dto.toEntity().toDomain()
    }

    suspend fun updateScene(
        id: Long,
        name: String,
        iconKey: String,
        isArchived: Boolean,
    ): Result<Scene> = runCatching {
        val response = api().updateScene(
            id,
            mapOf(
                "name" to name,
                "icon_key" to iconKey,
                "is_archived" to isArchived,
            ),
        )
        val dto = response["scene"] ?: error("Server nevrátil scénu.")
        db.scenes().upsertAll(listOf(dto.toEntity()))
        dto.toEntity().toDomain()
    }
}
