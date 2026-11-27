package sk.glukoview.insulinlog.data.remote

import com.google.gson.annotations.SerializedName

data class HealthDto(
    val ok: Boolean,
    val name: String?,
    val version: String?,
)

data class AuthVerifyResponse(
    val family: FamilyDto,
    val profiles: List<ProfileDto>,
    val settings: SettingsDto?,
    @SerializedName("server_time") val serverTime: String?,
)

data class FamilyDto(
    val id: Long,
    val name: String,
)

data class ProfileDto(
    val id: Long,
    val name: String,
    @SerializedName("color_hex") val colorHex: String,
    @SerializedName("sort_order") val sortOrder: Int,
    @SerializedName("is_active") val isActive: Boolean,
    @SerializedName("updated_at") val updatedAt: String?,
)

data class InsulinDto(
    val id: Long,
    val name: String,
    val category: String,
    @SerializedName("color_hex") val colorHex: String,
    @SerializedName("duration_hours") val durationHours: Double,
    @SerializedName("sort_order") val sortOrder: Int,
    @SerializedName("is_archived") val isArchived: Boolean,
    @SerializedName("updated_at") val updatedAt: String?,
)

data class SceneDto(
    val id: Long,
    val name: String,
    @SerializedName("icon_key") val iconKey: String,
    @SerializedName("sort_order") val sortOrder: Int,
    @SerializedName("is_archived") val isArchived: Boolean,
    @SerializedName("updated_at") val updatedAt: String?,
)

data class SettingsDto(
    @SerializedName("nightscout_url") val nightscoutUrl: String?,
    @SerializedName("nightscout_token") val nightscoutToken: String?,
    @SerializedName("basal_reminder_time") val basalReminderTime: String?,
    @SerializedName("glucose_unit") val glucoseUnit: String?,
    @SerializedName("duplicate_bolus_minutes") val duplicateBolusMinutes: Int?,
    @SerializedName("current_injection_site") val currentInjectionSite: String? = null,
    @SerializedName("updated_at") val updatedAt: String?,
)

data class InjectionSiteDto(
    val id: Long,
    val name: String,
    @SerializedName("sort_order") val sortOrder: Int,
    @SerializedName("is_archived") val isArchived: Boolean,
    @SerializedName("updated_at") val updatedAt: String?,
)

data class DoseDto(
    val id: Long?,
    val uuid: String,
    @SerializedName("profile_id") val profileId: Long,
    @SerializedName("insulin_id") val insulinId: Long,
    @SerializedName("scene_id") val sceneId: Long?,
    val units: Double,
    @SerializedName("administered_at") val administeredAt: String,
    @SerializedName("glucose_value") val glucoseValue: Double?,
    @SerializedName("glucose_unit") val glucoseUnit: String?,
    @SerializedName("glucose_trend") val glucoseTrend: String?,
    @SerializedName("carbs_sj") val carbsSj: Double?,
    @SerializedName("carbs_grams") val carbsGrams: Double?,
    @SerializedName("injection_site") val injectionSite: String?,
    val note: String?,
    @SerializedName("updated_at") val updatedAt: String?,
    @SerializedName("deleted_at") val deletedAt: String?,
)

data class SyncResponse(
    @SerializedName("server_time") val serverTime: String?,
    val profiles: List<ProfileDto> = emptyList(),
    val insulins: List<InsulinDto> = emptyList(),
    val scenes: List<SceneDto> = emptyList(),
    @SerializedName("injection_sites") val injectionSites: List<InjectionSiteDto> = emptyList(),
    val doses: List<DoseDto> = emptyList(),
    val settings: SettingsDto?,
)

data class DoseUpsertRequest(
    val uuid: String,
    @SerializedName("profile_id") val profileId: Long,
    @SerializedName("insulin_id") val insulinId: Long,
    @SerializedName("scene_id") val sceneId: Long?,
    val units: Double,
    @SerializedName("administered_at") val administeredAt: String,
    @SerializedName("glucose_value") val glucoseValue: Double?,
    @SerializedName("glucose_unit") val glucoseUnit: String?,
    @SerializedName("glucose_trend") val glucoseTrend: String?,
    @SerializedName("carbs_sj") val carbsSj: Double?,
    @SerializedName("carbs_grams") val carbsGrams: Double?,
    @SerializedName("injection_site") val injectionSite: String?,
    val note: String?,
)

data class DoseResponse(
    val dose: DoseDto,
    val warnings: List<ApiWarningDto> = emptyList(),
)

data class ApiWarningDto(
    val code: String?,
    val message: String?,
)

data class SettingsResponse(val settings: SettingsDto)
data class InsulinsResponse(val insulins: List<InsulinDto>)
data class ScenesResponse(val scenes: List<SceneDto>)
data class InjectionSitesResponse(@SerializedName("injection_sites") val injectionSites: List<InjectionSiteDto>)
data class ProfilesResponse(val profiles: List<ProfileDto>)
