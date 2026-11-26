package sk.glukoview.insulinlog.domain

/**
 * Vstavaný katalóg bežných inzulínov s orientačnou dĺžkou pôsobenia (DIA)
 * a časom vrcholu pre budúci/lepší IOB model.
 *
 * Zdroje (rozsahy, nie individuálne meranie):
 * - Diabetes Canada CPG Appendix 6 (onset/peak/duration)
 * - SmPC / Merck tabuľky
 * - OpenAPS/AAPS peak presets (NovoRapid 75′, Fiasp 55′, Lyumjev 45′)
 *
 * [durationHours] je kompromis pre lineárny IOB v logu (nie AID loop):
 * o niečo bližšie k praktickému DIA než k spodnej hranici „3–5 h“ z letáku.
 */
data class InsulinPreset(
    val id: String,
    val name: String,
    val genericName: String,
    val category: InsulinCategory,
    val colorHex: String,
    /** Orientačná dĺžka pôsobenia pre IOB (hodiny). */
    val durationHours: Double,
    /** Čas vrcholu aktivity (minúty) — info / budúci model. */
    val peakMinutes: Int?,
    val note: String = "",
)

object InsulinCatalog {
    val presets: List<InsulinPreset> = listOf(
        // —— Bolus / rýchle ——
        InsulinPreset(
            id = "novorapid",
            name = "NovoRapid",
            genericName = "insulin aspart",
            category = InsulinCategory.BOLUS,
            colorHex = "#E53935",
            durationHours = 5.0,
            peakMinutes = 75,
            note = "Ako Humalog/Apidra — rapid analog",
        ),
        InsulinPreset(
            id = "novolog",
            name = "NovoLog",
            genericName = "insulin aspart",
            category = InsulinCategory.BOLUS,
            colorHex = "#E53935",
            durationHours = 5.0,
            peakMinutes = 75,
        ),
        InsulinPreset(
            id = "humalog",
            name = "Humalog",
            genericName = "insulin lispro",
            category = InsulinCategory.BOLUS,
            colorHex = "#D81B60",
            durationHours = 5.0,
            peakMinutes = 75,
        ),
        InsulinPreset(
            id = "apidra",
            name = "Apidra",
            genericName = "insulin glulisine",
            category = InsulinCategory.BOLUS,
            colorHex = "#8E24AA",
            durationHours = 5.0,
            peakMinutes = 72,
        ),
        InsulinPreset(
            id = "fiasp",
            name = "Fiasp",
            genericName = "faster insulin aspart",
            category = InsulinCategory.BOLUS,
            colorHex = "#FB8C00",
            durationHours = 4.5,
            peakMinutes = 55,
            note = "Ultra-rapid",
        ),
        InsulinPreset(
            id = "lyumjev",
            name = "Lyumjev",
            genericName = "insulin lispro-aabc",
            category = InsulinCategory.BOLUS,
            colorHex = "#F4511E",
            durationHours = 4.5,
            peakMinutes = 45,
            note = "Ultra-rapid",
        ),
        InsulinPreset(
            id = "actrapid",
            name = "Actrapid",
            genericName = "regular / short-acting",
            category = InsulinCategory.BOLUS,
            colorHex = "#6D4C41",
            durationHours = 6.5,
            peakMinutes = 150,
            note = "Klasický krátkodobý (Humulin R / Novolin R)",
        ),
        InsulinPreset(
            id = "humulin_r",
            name = "Humulin R",
            genericName = "regular insulin",
            category = InsulinCategory.BOLUS,
            colorHex = "#6D4C41",
            durationHours = 6.5,
            peakMinutes = 150,
        ),
        // —— Bazál ——
        InsulinPreset(
            id = "lantus",
            name = "Lantus",
            genericName = "insulin glargine U100",
            category = InsulinCategory.BASAL,
            colorHex = "#43A047",
            durationHours = 24.0,
            peakMinutes = null,
            note = "Bez výrazného vrcholu",
        ),
        InsulinPreset(
            id = "abasaglar",
            name = "Abasaglar",
            genericName = "insulin glargine U100",
            category = InsulinCategory.BASAL,
            colorHex = "#43A047",
            durationHours = 24.0,
            peakMinutes = null,
        ),
        InsulinPreset(
            id = "toujeo",
            name = "Toujeo",
            genericName = "insulin glargine U300",
            category = InsulinCategory.BASAL,
            colorHex = "#2E7D32",
            durationHours = 36.0,
            peakMinutes = null,
            note = "Dlhšie než Lantus",
        ),
        InsulinPreset(
            id = "tresiba",
            name = "Tresiba",
            genericName = "insulin degludec",
            category = InsulinCategory.BASAL,
            colorHex = "#1E88E5",
            durationHours = 42.0,
            peakMinutes = null,
            note = "Ultra-dlhodobý (až ~42 h)",
        ),
        InsulinPreset(
            id = "levemir",
            name = "Levemir",
            genericName = "insulin detemir",
            category = InsulinCategory.BASAL,
            colorHex = "#039BE5",
            durationHours = 24.0,
            peakMinutes = null,
        ),
        InsulinPreset(
            id = "nph",
            name = "NPH / Insulatard",
            genericName = "NPH / isophane",
            category = InsulinCategory.BASAL,
            colorHex = "#546E7A",
            durationHours = 18.0,
            peakMinutes = 360,
            note = "Stredne dlhodobý, má vrchol",
        ),
    )

    fun defaultDuration(category: InsulinCategory): Double =
        if (category == InsulinCategory.BASAL) 24.0 else 5.0

    fun findByName(name: String): InsulinPreset? {
        val key = name.trim().lowercase()
        if (key.isEmpty()) return null
        return presets.firstOrNull { it.name.equals(key, ignoreCase = true) }
            ?: presets.firstOrNull { key.contains(it.name.lowercase()) }
            ?: presets.firstOrNull { it.name.lowercase().contains(key) }
    }

    fun matchDurationForName(name: String, category: InsulinCategory): Double =
        findByName(name)?.durationHours ?: defaultDuration(category)
}
