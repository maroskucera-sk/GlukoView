package sk.glukoview.insulinlog.data.nightscout

import retrofit2.http.GET
import retrofit2.http.Query

interface NightscoutApi {
    @GET("api/v1/entries.json")
    suspend fun latestEntries(
        @Query("count") count: Int = 2,
    ): List<NightscoutEntryDto>

    /**
     * Mongo-style filter (väčšina Nightscout inštancií).
     * Fallback pri zlyhaní: [latestEntries] s vysokým count + filter na klientovi.
     */
    @GET("api/v1/entries.json")
    suspend fun entriesByDate(
        @Query("find[date][\$gte]") gteMs: Long,
        @Query("find[date][\$lte]") lteMs: Long,
        @Query("count") count: Int = 50000,
    ): List<NightscoutEntryDto>
}
