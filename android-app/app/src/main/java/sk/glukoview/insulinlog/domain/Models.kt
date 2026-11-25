package sk.glukoview.insulinlog.domain

import java.time.Instant

enum class InsulinCategory {
    BOLUS,
    BASAL;

    companion object {
        fun fromApi(value: String): InsulinCategory =
            when (value.lowercase()) {
                "basal" -> BASAL
                else -> BOLUS
            }
    }

    fun toApi(): String = name.lowercase()
}

enum class InjectionSite(val apiValue: String) {
    ABDOMEN("abdomen"),
    THIGH("thigh"),
    ARM("arm"),
    BUTTOCK("buttock");

    companion object {
        fun fromApi(value: String?): InjectionSite? =
            entries.firstOrNull { it.apiValue == value }
    }
}

enum class GlucoseUnit {
    MMOL,
    MGDL;

    companion object {
        fun fromApi(value: String?): GlucoseUnit =
            when (value?.lowercase()) {
                "mgdl" -> MGDL
                else -> MMOL
            }
    }

    fun toApi(): String = name.lowercase()
}

enum class ThemeMode {
    SYSTEM,
    LIGHT,
    DARK,
}

data class Profile(
    val id: Long,
    val name: String,
    val colorHex: String,
    val sortOrder: Int,
    val isActive: Boolean,
)

data class InsulinType(
    val id: Long,
    val name: String,
    val category: InsulinCategory,
    val colorHex: String,
    val durationHours: Double,
    val sortOrder: Int,
    val isArchived: Boolean,
)

data class Scene(
    val id: Long,
    val name: String,
    val iconKey: String,
    val sortOrder: Int,
    val isArchived: Boolean,
)

data class FamilySettings(
    val nightscoutUrl: String?,
    val nightscoutToken: String?,
    val basalReminderTime: String?,
    val glucoseUnit: GlucoseUnit,
    val duplicateBolusMinutes: Int,
)

data class DoseRecord(
    val localId: Long = 0,
    val serverId: Long? = null,
    val uuid: String,
    val profileId: Long,
    val insulinId: Long,
    val sceneId: Long?,
    val units: Double,
    val administeredAt: Instant,
    val glucoseValue: Double?,
    val glucoseUnit: GlucoseUnit?,
    val glucoseTrend: String?,
    val carbsSj: Double?,
    val carbsGrams: Double?,
    val injectionSite: InjectionSite?,
    val note: String?,
    val deletedAt: Instant? = null,
    val syncPending: Boolean = false,
)

data class NightscoutReading(
    val sgvMgdl: Double,
    val trend: GlucoseTrend,
    val date: Instant,
    val directionRaw: String?,
)

data class GlucoseDisplay(
    val value: Double,
    val unit: GlucoseUnit,
    val trend: GlucoseTrend,
    val ageLabel: String,
    val deltaLabel: String?,
)
