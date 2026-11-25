package sk.glukoview.insulinlog.data.remote

import okhttp3.ResponseBody
import retrofit2.http.Body
import retrofit2.http.DELETE
import retrofit2.http.GET
import retrofit2.http.PATCH
import retrofit2.http.POST
import retrofit2.http.PUT
import retrofit2.http.Path
import retrofit2.http.Query
import retrofit2.http.Streaming

interface InsulinLogApi {
    @GET("api/v1/health")
    suspend fun health(): HealthDto

    @POST("api/v1/auth/verify")
    suspend fun verify(): AuthVerifyResponse

    @GET("api/v1/sync")
    suspend fun sync(@Query("since") since: String? = null): SyncResponse

    @GET("api/v1/doses")
    suspend fun listDoses(
        @Query("from") from: String? = null,
        @Query("to") to: String? = null,
        @Query("limit") limit: Int? = null,
    ): Map<String, List<DoseDto>>

    @POST("api/v1/doses")
    suspend fun upsertDose(@Body body: DoseUpsertRequest): DoseResponse

    @PATCH("api/v1/doses/{uuid}")
    suspend fun updateDose(@Path("uuid") uuid: String, @Body body: Map<String, @JvmSuppressWildcards Any?>): DoseResponse

    @DELETE("api/v1/doses/{uuid}")
    suspend fun deleteDose(@Path("uuid") uuid: String)

    @GET("api/v1/insulins")
    suspend fun listInsulins(@Query("include_archived") includeArchived: Boolean = false): InsulinsResponse

    @POST("api/v1/insulins")
    suspend fun createInsulin(@Body body: Map<String, @JvmSuppressWildcards Any?>): Map<String, InsulinDto>

    @PATCH("api/v1/insulins/{id}")
    suspend fun updateInsulin(@Path("id") id: Long, @Body body: Map<String, @JvmSuppressWildcards Any?>): Map<String, InsulinDto>

    @GET("api/v1/scenes")
    suspend fun listScenes(@Query("include_archived") includeArchived: Boolean = false): ScenesResponse

    @POST("api/v1/scenes")
    suspend fun createScene(@Body body: Map<String, @JvmSuppressWildcards Any?>): Map<String, SceneDto>

    @PATCH("api/v1/scenes/{id}")
    suspend fun updateScene(@Path("id") id: Long, @Body body: Map<String, @JvmSuppressWildcards Any?>): Map<String, SceneDto>

    @GET("api/v1/profiles")
    suspend fun listProfiles(): ProfilesResponse

    @GET("api/v1/settings")
    suspend fun getSettings(): SettingsResponse

    @PUT("api/v1/settings")
    suspend fun putSettings(@Body body: Map<String, @JvmSuppressWildcards Any?>): SettingsResponse

    @Streaming
    @GET("api/v1/export")
    suspend fun exportCsv(
        @Query("from") from: String,
        @Query("to") to: String,
        @Query("format") format: String = "csv",
    ): ResponseBody
}
