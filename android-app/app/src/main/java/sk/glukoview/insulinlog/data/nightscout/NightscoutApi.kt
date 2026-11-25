package sk.glukoview.insulinlog.data.nightscout

import retrofit2.http.GET
import retrofit2.http.Query

interface NightscoutApi {
    @GET("api/v1/entries.json")
    suspend fun latestEntries(
        @Query("count") count: Int = 2,
    ): List<NightscoutEntryDto>
}
