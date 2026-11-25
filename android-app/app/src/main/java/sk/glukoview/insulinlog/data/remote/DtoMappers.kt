package sk.glukoview.insulinlog.data.remote

import sk.glukoview.insulinlog.data.local.DoseEntity
import sk.glukoview.insulinlog.data.local.InsulinEntity
import sk.glukoview.insulinlog.data.local.ProfileEntity
import sk.glukoview.insulinlog.data.local.SceneEntity
import sk.glukoview.insulinlog.data.local.SettingsEntity
import sk.glukoview.insulinlog.domain.DoseRecord
import sk.glukoview.insulinlog.domain.FamilySettings
import sk.glukoview.insulinlog.domain.GlucoseUnit
import sk.glukoview.insulinlog.domain.InjectionSite
import sk.glukoview.insulinlog.domain.InsulinCategory
import sk.glukoview.insulinlog.domain.InsulinType
import sk.glukoview.insulinlog.domain.Profile
import sk.glukoview.insulinlog.domain.Scene
import java.time.Instant

object DtoMappers {
    fun parseInstant(value: String?): Instant? {
        if (value.isNullOrBlank()) return null
        return runCatching { Instant.parse(value) }.getOrNull()
            ?: runCatching {
                // MySQL-ish fallback
                Instant.parse(value.replace(' ', 'T') + "Z")
            }.getOrNull()
    }

    fun instantToApi(instant: Instant): String = instant.toString()

    fun ProfileDto.toEntity(): ProfileEntity = ProfileEntity(
        id = id,
        name = name,
        colorHex = colorHex,
        sortOrder = sortOrder,
        isActive = isActive,
        updatedAtEpochMs = parseInstant(updatedAt)?.toEpochMilli() ?: System.currentTimeMillis(),
    )

    fun InsulinDto.toEntity(): InsulinEntity = InsulinEntity(
        id = id,
        name = name,
        category = category,
        colorHex = colorHex,
        durationHours = durationHours,
        sortOrder = sortOrder,
        isArchived = isArchived,
        updatedAtEpochMs = parseInstant(updatedAt)?.toEpochMilli() ?: System.currentTimeMillis(),
    )

    fun SceneDto.toEntity(): SceneEntity = SceneEntity(
        id = id,
        name = name,
        iconKey = iconKey,
        sortOrder = sortOrder,
        isArchived = isArchived,
        updatedAtEpochMs = parseInstant(updatedAt)?.toEpochMilli() ?: System.currentTimeMillis(),
    )

    fun SettingsDto.toEntity(): SettingsEntity = SettingsEntity(
        nightscoutUrl = nightscoutUrl,
        nightscoutToken = nightscoutToken,
        basalReminderTime = basalReminderTime,
        glucoseUnit = glucoseUnit ?: "mmol",
        duplicateBolusMinutes = duplicateBolusMinutes ?: 20,
        updatedAtEpochMs = parseInstant(updatedAt)?.toEpochMilli() ?: System.currentTimeMillis(),
    )

    fun DoseDto.toEntity(existingLocalId: Long = 0, syncPending: Boolean = false): DoseEntity = DoseEntity(
        localId = existingLocalId,
        serverId = id,
        uuid = uuid,
        profileId = profileId,
        insulinId = insulinId,
        sceneId = sceneId,
        units = units,
        administeredAtEpochMs = parseInstant(administeredAt)?.toEpochMilli() ?: 0L,
        glucoseValue = glucoseValue,
        glucoseUnit = glucoseUnit,
        glucoseTrend = glucoseTrend,
        carbsSj = carbsSj,
        carbsGrams = carbsGrams,
        injectionSite = injectionSite,
        note = note,
        deletedAtEpochMs = parseInstant(deletedAt)?.toEpochMilli(),
        updatedAtEpochMs = parseInstant(updatedAt)?.toEpochMilli() ?: System.currentTimeMillis(),
        syncPending = syncPending,
    )

    fun ProfileEntity.toDomain() = Profile(id, name, colorHex, sortOrder, isActive)

    fun InsulinEntity.toDomain() = InsulinType(
        id, name, InsulinCategory.fromApi(category), colorHex, durationHours, sortOrder, isArchived,
    )

    fun SceneEntity.toDomain() = Scene(id, name, iconKey, sortOrder, isArchived)

    fun SettingsEntity.toDomain() = FamilySettings(
        nightscoutUrl = nightscoutUrl,
        nightscoutToken = nightscoutToken,
        basalReminderTime = basalReminderTime,
        glucoseUnit = GlucoseUnit.fromApi(glucoseUnit),
        duplicateBolusMinutes = duplicateBolusMinutes,
    )

    fun DoseEntity.toDomain() = DoseRecord(
        localId = localId,
        serverId = serverId,
        uuid = uuid,
        profileId = profileId,
        insulinId = insulinId,
        sceneId = sceneId,
        units = units,
        administeredAt = Instant.ofEpochMilli(administeredAtEpochMs),
        glucoseValue = glucoseValue,
        glucoseUnit = glucoseUnit?.let { GlucoseUnit.fromApi(it) },
        glucoseTrend = glucoseTrend,
        carbsSj = carbsSj,
        carbsGrams = carbsGrams,
        injectionSite = InjectionSite.fromApi(injectionSite),
        note = note,
        deletedAt = deletedAtEpochMs?.let(Instant::ofEpochMilli),
        syncPending = syncPending,
    )

    fun DoseRecord.toUpsertRequest(): DoseUpsertRequest = DoseUpsertRequest(
        uuid = uuid,
        profileId = profileId,
        insulinId = insulinId,
        sceneId = sceneId,
        units = units,
        administeredAt = instantToApi(administeredAt),
        glucoseValue = glucoseValue,
        glucoseUnit = glucoseUnit?.toApi(),
        glucoseTrend = glucoseTrend,
        carbsSj = carbsSj,
        carbsGrams = carbsGrams,
        injectionSite = injectionSite?.apiValue,
        note = note,
    )
}
