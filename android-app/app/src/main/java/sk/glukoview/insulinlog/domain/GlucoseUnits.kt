package sk.glukoview.insulinlog.domain

object GlucoseUnits {
    private const val MGDL_PER_MMOL = 18.0182

    fun mgdlToMmol(mgdl: Double): Double =
        ((mgdl / MGDL_PER_MMOL) * 10).toInt() / 10.0

    fun mmolToMgdl(mmol: Double): Double =
        (mmol * MGDL_PER_MMOL).toInt().toDouble()

    fun format(value: Double, unit: GlucoseUnit): String =
        when (unit) {
            GlucoseUnit.MMOL -> String.format("%.1f", value)
            GlucoseUnit.MGDL -> String.format("%.0f", value)
        }

    fun convert(value: Double, from: GlucoseUnit, to: GlucoseUnit): Double {
        if (from == to) return value
        return if (from == GlucoseUnit.MGDL) mgdlToMmol(value) else mmolToMgdl(value)
    }
}
