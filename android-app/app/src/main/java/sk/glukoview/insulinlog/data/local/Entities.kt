package sk.glukoview.insulinlog.data.local

import androidx.room.Entity
import androidx.room.Index
import androidx.room.PrimaryKey

@Entity(tableName = "profiles")
data class ProfileEntity(
    @PrimaryKey val id: Long,
    val name: String,
    val colorHex: String,
    val sortOrder: Int,
    val isActive: Boolean,
    val updatedAtEpochMs: Long,
)

@Entity(tableName = "insulins")
data class InsulinEntity(
    @PrimaryKey val id: Long,
    val name: String,
    val category: String,
    val colorHex: String,
    val durationHours: Double,
    val sortOrder: Int,
    val isArchived: Boolean,
    val updatedAtEpochMs: Long,
)

@Entity(tableName = "scenes")
data class SceneEntity(
    @PrimaryKey val id: Long,
    val name: String,
    val iconKey: String,
    val sortOrder: Int,
    val isArchived: Boolean,
    val updatedAtEpochMs: Long,
)

@Entity(tableName = "injection_sites")
data class InjectionSiteEntity(
    @PrimaryKey val id: Long,
    val name: String,
    val sortOrder: Int,
    val isArchived: Boolean,
    val updatedAtEpochMs: Long,
)

@Entity(
    tableName = "doses",
    indices = [
        Index(value = ["uuid"], unique = true),
        Index(value = ["administeredAtEpochMs"]),
        Index(value = ["syncPending"]),
    ],
)
data class DoseEntity(
    @PrimaryKey(autoGenerate = true) val localId: Long = 0,
    val serverId: Long? = null,
    val uuid: String,
    val profileId: Long,
    val insulinId: Long,
    val sceneId: Long?,
    val units: Double,
    val administeredAtEpochMs: Long,
    val glucoseValue: Double?,
    val glucoseUnit: String?,
    val glucoseTrend: String?,
    val carbsSj: Double?,
    val carbsGrams: Double?,
    val injectionSite: String?,
    val note: String?,
    val deletedAtEpochMs: Long? = null,
    val updatedAtEpochMs: Long,
    val syncPending: Boolean = false,
)

@Entity(tableName = "family_settings", primaryKeys = ["id"])
data class SettingsEntity(
    val id: Int = 1,
    val nightscoutUrl: String?,
    val nightscoutToken: String?,
    val basalReminderTime: String?,
    val glucoseUnit: String,
    val duplicateBolusMinutes: Int,
    val currentInjectionSite: String? = null,
    val updatedAtEpochMs: Long,
)

@Entity(tableName = "sync_meta", primaryKeys = ["id"])
data class SyncMetaEntity(
    val id: Int = 1,
    val lastSyncEpochMs: Long?,
    val familyId: Long?,
    val familyName: String?,
)
