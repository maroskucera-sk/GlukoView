package sk.glukoview.insulinlog.domain

/**
 * Nightscout direction → šípka pre UI.
 * Hodnoty zodpovedajú bežnému NS `direction` poľu.
 */
enum class GlucoseTrend(val arrow: String, val labelSk: String) {
    DOUBLE_UP("↑↑", "rýchlo stúpa"),
    SINGLE_UP("↑", "stúpa"),
    FORTY_FIVE_UP("↗", "mierne stúpa"),
    FLAT("→", "stabilná"),
    FORTY_FIVE_DOWN("↘", "mierne klesá"),
    SINGLE_DOWN("↓", "klesá"),
    DOUBLE_DOWN("↓↓", "rýchlo klesá"),
    NONE("·", "neznámy");

    companion object {
        fun fromNightscout(direction: String?): GlucoseTrend =
            when (direction?.trim()) {
                "DoubleUp" -> DOUBLE_UP
                "SingleUp" -> SINGLE_UP
                "FortyFiveUp" -> FORTY_FIVE_UP
                "Flat" -> FLAT
                "FortyFiveDown" -> FORTY_FIVE_DOWN
                "SingleDown" -> SINGLE_DOWN
                "DoubleDown" -> DOUBLE_DOWN
                else -> NONE
            }
    }
}
