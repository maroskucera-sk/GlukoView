package sk.glukoview.insulinlog.data.nightscout

import com.google.gson.annotations.SerializedName

/**
 * Nightscout `/api/v1/entries.json?count=1` položka.
 * `sgv` je typicky mg/dL; `date` je epoch ms.
 */
data class NightscoutEntryDto(
    val sgv: Double?,
    val date: Long?,
    @SerializedName("dateString") val dateString: String?,
    val direction: String?,
    val trend: Int? = null,
    val type: String? = null,
)
