package sk.glukoview.insulinlog.data.repository

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Typeface
import android.graphics.pdf.PdfDocument
import sk.glukoview.insulinlog.data.local.AppDatabase
import sk.glukoview.insulinlog.data.nightscout.NightscoutClient
import sk.glukoview.insulinlog.domain.GlucoseUnit
import sk.glukoview.insulinlog.domain.GlucoseUnits
import sk.glukoview.insulinlog.domain.NightscoutReading
import sk.glukoview.insulinlog.ui.util.Formatters
import java.io.File
import java.io.FileOutputStream
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter

enum class DoctorExportFormat { CSV, PDF }

data class DoctorExportResult(
    val file: File,
    val glucoseCount: Int,
    val doseCount: Int,
    val nightscoutOk: Boolean,
    val nightscoutError: String?,
)

/**
 * Kombinovaný export pre diabetológa:
 * - inzulínové dávky z lokálnej/sync DB
 * - glykémie stiahnuté on-demand z Nightscoutu za rovnaké obdobie (neukladajú sa natrvalo)
 */
class ExportRepository(
    private val context: Context,
    private val db: AppDatabase,
    private val nightscout: NightscoutClient,
) {
    suspend fun buildDoctorExport(
        from: Instant,
        to: Instant,
        format: DoctorExportFormat,
    ): DoctorExportResult {
        val settings = db.settings().get()
        val unit = GlucoseUnit.fromApi(settings?.glucoseUnit)
        val doseRows = db.doses().listBetween(from.toEpochMilli(), to.toEpochMilli())
        val profiles = db.profiles().getAll().associateBy { it.id }
        val insulins = db.insulins().getAll().associateBy { it.id }
        val scenes = db.scenes().getAll().associateBy { it.id }

        var nsOk = false
        var nsError: String? = null
        val glucose: List<NightscoutReading> = if (!settings?.nightscoutUrl.isNullOrBlank()) {
            nightscout.fetchRange(settings!!.nightscoutUrl!!, settings.nightscoutToken, from, to)
                .fold(
                    onSuccess = {
                        nsOk = true
                        it
                    },
                    onFailure = { e ->
                        nsError = e.message
                        emptyList()
                    },
                )
        } else {
            nsError = "Nightscout URL nie je nastavené — export obsahuje len inzulín."
            emptyList()
        }

        val file = when (format) {
            DoctorExportFormat.CSV -> writeCsv(
                from, to, unit, glucose, doseRows, profiles, insulins, scenes, nsOk, nsError,
            )
            DoctorExportFormat.PDF -> writePdf(
                from, to, unit, glucose, doseRows, profiles, insulins, scenes, nsOk, nsError,
            )
        }

        return DoctorExportResult(
            file = file,
            glucoseCount = glucose.size,
            doseCount = doseRows.size,
            nightscoutOk = nsOk,
            nightscoutError = nsError,
        )
    }

    private fun writeCsv(
        from: Instant,
        to: Instant,
        unit: GlucoseUnit,
        glucose: List<NightscoutReading>,
        doseRows: List<sk.glukoview.insulinlog.data.local.DoseEntity>,
        profiles: Map<Long, sk.glukoview.insulinlog.data.local.ProfileEntity>,
        insulins: Map<Long, sk.glukoview.insulinlog.data.local.InsulinEntity>,
        scenes: Map<Long, sk.glukoview.insulinlog.data.local.SceneEntity>,
        nsOk: Boolean,
        nsError: String?,
    ): File {
        val zone = ZoneId.systemDefault()
        val fmt = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm").withZone(zone)
        val sb = StringBuilder()
        sb.append('\uFEFF')
        sb.append("# GlukoView Inzulín — export pre diabetológa\n")
        sb.append("# Obdobie: ${fmt.format(from)} – ${fmt.format(to)}\n")
        sb.append("# Nightscout: ${if (nsOk) "OK (${glucose.size} meraní)" else "NEDOSTUPNÉ — ${nsError.orEmpty()}"}\n")
        sb.append("# Toto nie je zdravotnícka pomôcka.\n")
        sb.append(
            "Typ;Datum_cas;Gly_mmol;Gly_mgdl;Trend;Inzulin;Kategoria;Jednotky;Profil;Scena;SJ;Sacharidy_g;Miesto_vpichu;Poznamka;Gly_pri_davke\n",
        )

        data class Event(val at: Instant, val line: String)
        val events = mutableListOf<Event>()

        for (g in glucose) {
            val mmol = GlucoseUnits.convert(g.sgvMgdl, GlucoseUnit.MGDL, GlucoseUnit.MMOL)
            val mgdl = g.sgvMgdl
            events += Event(
                g.date,
                listOf(
                    "GLYKEMIA",
                    fmt.format(g.date),
                    GlucoseUnits.format(mmol, GlucoseUnit.MMOL),
                    GlucoseUnits.format(mgdl, GlucoseUnit.MGDL),
                    g.directionRaw.orEmpty(),
                    "", "", "", "", "", "", "", "", "", "",
                ).joinToString(";"),
            )
        }

        for (d in doseRows) {
            val p = profiles[d.profileId]?.name.orEmpty()
            val i = insulins[d.insulinId]
            val s = d.sceneId?.let { scenes[it]?.name }.orEmpty()
            val cat = if (i?.category == "basal") "bazalny" else "rychly"
            val site = Formatters.siteLabel(d.injectionSite)
            val doseGly = d.glucoseValue?.toString().orEmpty()
            events += Event(
                Instant.ofEpochMilli(d.administeredAtEpochMs),
                listOf(
                    "INZULIN",
                    fmt.format(Instant.ofEpochMilli(d.administeredAtEpochMs)),
                    "", "",
                    d.glucoseTrend.orEmpty(),
                    i?.name.orEmpty(),
                    cat,
                    d.units.toString(),
                    p,
                    s,
                    d.carbsSj?.toString().orEmpty(),
                    d.carbsGrams?.toString().orEmpty(),
                    site,
                    (d.note ?: "").replace(';', ','),
                    doseGly,
                ).joinToString(";") { it.replace("\n", " ") },
            )
        }

        events.sortedBy { it.at }.forEach { sb.append(it.line).append('\n') }

        // Preferovaná jednotka UI len v poznámke hlavičky — stĺpce majú obe
        sb.append("# Preferovana_jednotka_UI=${unit.toApi()}\n")

        val file = File(context.cacheDir, "glukoview-lekar-export.csv")
        file.writeText(sb.toString(), Charsets.UTF_8)
        return file
    }

    private fun writePdf(
        from: Instant,
        to: Instant,
        unit: GlucoseUnit,
        glucose: List<NightscoutReading>,
        doseRows: List<sk.glukoview.insulinlog.data.local.DoseEntity>,
        profiles: Map<Long, sk.glukoview.insulinlog.data.local.ProfileEntity>,
        insulins: Map<Long, sk.glukoview.insulinlog.data.local.InsulinEntity>,
        scenes: Map<Long, sk.glukoview.insulinlog.data.local.SceneEntity>,
        nsOk: Boolean,
        nsError: String?,
    ): File {
        val zone = ZoneId.systemDefault()
        val dayFmt = DateTimeFormatter.ofPattern("d. M. yyyy").withZone(zone)
        val timeFmt = DateTimeFormatter.ofPattern("HH:mm").withZone(zone)
        val rangeFmt = DateTimeFormatter.ofPattern("d. M. yyyy").withZone(zone)

        val byDayGlucose = glucose.groupBy { it.date.atZone(zone).toLocalDate() }
        val byDayDoses = doseRows.groupBy {
            Instant.ofEpochMilli(it.administeredAtEpochMs).atZone(zone).toLocalDate()
        }
        val days = (byDayGlucose.keys + byDayDoses.keys).toSortedSet()

        val doc = PdfDocument()
        val pageWidth = 595 // A4 pts
        val pageHeight = 842
        val titlePaint = Paint().apply {
            textSize = 16f
            typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
            isAntiAlias = true
        }
        val body = Paint().apply {
            textSize = 10f
            isAntiAlias = true
        }
        val small = Paint().apply {
            textSize = 9f
            isAntiAlias = true
            color = 0xFF444444.toInt()
        }

        var pageNumber = 1
        fun newPage(): Pair<PdfDocument.Page, Canvas> {
            val pageInfo = PdfDocument.PageInfo.Builder(pageWidth, pageHeight, pageNumber++).create()
            val page = doc.startPage(pageInfo)
            return page to page.canvas
        }

        var (page, canvas) = newPage()
        var y = 40f

        fun ensureSpace(needed: Float) {
            if (y + needed > pageHeight - 40) {
                doc.finishPage(page)
                val next = newPage()
                page = next.first
                canvas = next.second
                y = 40f
            }
        }

        fun line(text: String, paint: Paint = body, gap: Float = 14f) {
            ensureSpace(gap + 4)
            canvas.drawText(text, 40f, y, paint)
            y += gap
        }

        line("GlukoView Inzulín — správa pre diabetológa", titlePaint, 22f)
        line("Obdobie: ${rangeFmt.format(from)} – ${rangeFmt.format(to)}", body)
        line(
            if (nsOk) "Nightscout: ${glucose.size} meraní CGM"
            else "Nightscout: nedostupné (${nsError ?: "—"})",
            small,
        )
        line("Inzulínové dávky: ${doseRows.size}", small)
        line("Upozornenie: Toto nie je zdravotnícka pomôcka.", small, 18f)

        if (days.isEmpty()) {
            line("Za zvolené obdobie nie sú žiadne dáta.")
        }

        for (day in days) {
            ensureSpace(80f)
            line(dayFmt.format(day.atStartOfDay(zone)), titlePaint, 18f)

            val gDay = byDayGlucose[day].orEmpty()
            if (gDay.isNotEmpty()) {
                val values = gDay.map {
                    GlucoseUnits.convert(it.sgvMgdl, GlucoseUnit.MGDL, unit)
                }
                val avg = values.average()
                val min = values.min()
                val max = values.max()
                val unitLabel = if (unit == GlucoseUnit.MMOL) "mmol/L" else "mg/dL"
                line(
                    "Glykémia: n=${gDay.size}, priemer=${GlucoseUnits.format(avg, unit)}, " +
                        "min=${GlucoseUnits.format(min, unit)}, max=${GlucoseUnits.format(max, unit)} $unitLabel",
                    body,
                )
            } else {
                line("Glykémia: bez dát z Nightscoutu", small)
            }

            val dDay = byDayDoses[day].orEmpty().sortedBy { it.administeredAtEpochMs }
            if (dDay.isEmpty()) {
                line("Inzulín: žiadny záznam", small, 16f)
            } else {
                line("Inzulín:", body, 14f)
                for (d in dDay) {
                    val i = insulins[d.insulinId]
                    val p = profiles[d.profileId]?.name.orEmpty()
                    val s = d.sceneId?.let { scenes[it]?.name }
                    val gly = d.glucoseValue?.let { " G=$it" }.orEmpty()
                    val text = "  ${timeFmt.format(Instant.ofEpochMilli(d.administeredAtEpochMs))}  " +
                        "${d.units}j ${i?.name ?: "?"} (${if (i?.category == "basal") "bazál" else "bolus"})" +
                        (s?.let { " · $it" } ?: "") +
                        " · $p$gly"
                    line(text, small, 12f)
                }
                y += 6f
            }
        }

        doc.finishPage(page)
        val file = File(context.cacheDir, "glukoview-lekar-export.pdf")
        FileOutputStream(file).use { doc.writeTo(it) }
        doc.close()
        return file
    }
}
