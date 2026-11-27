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
        val unitLabel = if (unit == GlucoseUnit.MMOL) "mmol/L" else "mg/dL"

        val gValues = glucose.map {
            GlucoseUnits.convert(it.sgvMgdl, GlucoseUnit.MGDL, unit)
        }
        val bolusTotal = doseRows.filter { insulins[it.insulinId]?.category != "basal" }.sumOf { it.units }
        val basalTotal = doseRows.filter { insulins[it.insulinId]?.category == "basal" }.sumOf { it.units }

        val byDayDoses = doseRows.groupBy {
            Instant.ofEpochMilli(it.administeredAtEpochMs).atZone(zone).toLocalDate()
        }
        val byDayGlucose = glucose.groupBy { it.date.atZone(zone).toLocalDate() }
        val days = (byDayGlucose.keys + byDayDoses.keys).toSortedSet()

        val doc = PdfDocument()
        val pageWidth = 595
        val pageHeight = 842
        val titlePaint = Paint().apply {
            textSize = 15f
            typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
            isAntiAlias = true
            color = 0xFF1A2332.toInt()
        }
        val body = Paint().apply {
            textSize = 10f
            isAntiAlias = true
            color = 0xFF1A2332.toInt()
        }
        val small = Paint().apply {
            textSize = 8.5f
            isAntiAlias = true
            color = 0xFF555555.toInt()
        }
        val axisPaint = Paint().apply {
            strokeWidth = 1f
            color = 0xFFCCCCCC.toInt()
            isAntiAlias = true
            style = Paint.Style.STROKE
        }
        val gridPaint = Paint().apply {
            strokeWidth = 0.6f
            color = 0xFFEEEEEE.toInt()
            isAntiAlias = true
            style = Paint.Style.STROKE
        }
        val glucosePaint = Paint().apply {
            strokeWidth = 1.6f
            color = 0xFF2E7D32.toInt()
            isAntiAlias = true
            style = Paint.Style.STROKE
            strokeJoin = Paint.Join.ROUND
            strokeCap = Paint.Cap.ROUND
        }
        val targetBand = Paint().apply {
            color = 0x222E7D32
            style = Paint.Style.FILL
            isAntiAlias = true
        }
        val bolusPaint = Paint().apply {
            color = 0xFFE53935.toInt()
            style = Paint.Style.FILL
            isAntiAlias = true
        }
        val basalPaint = Paint().apply {
            color = 0xFF1E88E5.toInt()
            style = Paint.Style.FILL
            isAntiAlias = true
        }

        var pageNumber = 1
        fun newPage(): Pair<PdfDocument.Page, Canvas> {
            val pageInfo = PdfDocument.PageInfo.Builder(pageWidth, pageHeight, pageNumber++).create()
            val page = doc.startPage(pageInfo)
            return page to page.canvas
        }

        var (page, canvas) = newPage()
        var y = 36f

        fun ensureSpace(needed: Float) {
            if (y + needed > pageHeight - 36) {
                doc.finishPage(page)
                val next = newPage()
                page = next.first
                canvas = next.second
                y = 36f
            }
        }

        fun line(text: String, paint: Paint = body, gap: Float = 13f) {
            ensureSpace(gap + 2)
            canvas.drawText(text, 40f, y, paint)
            y += gap
        }

        // —— Strana 1: súhrn + graf ——
        line("GlukoView Inzulín — správa pre diabetológa", titlePaint, 20f)
        line("Obdobie: ${rangeFmt.format(from)} – ${rangeFmt.format(to)}", body)
        line(
            if (nsOk) "Nightscout CGM: ${glucose.size} meraní"
            else "Nightscout: nedostupné (${nsError ?: "—"})",
            small,
        )
        if (gValues.isNotEmpty()) {
            line(
                "Glykémia ($unitLabel): priemer ${GlucoseUnits.format(gValues.average(), unit)}, " +
                    "min ${GlucoseUnits.format(gValues.min(), unit)}, " +
                    "max ${GlucoseUnits.format(gValues.max(), unit)}",
                body,
            )
        }
        line(
            "Inzulín: ${doseRows.size} dávok · bolus ${Formatters.units(bolusTotal)} · bazál ${Formatters.units(basalTotal)}",
            body,
        )
        line("Toto nie je zdravotnícka pomôcka.", small, 16f)

        // Graf
        val chartH = 220f
        ensureSpace(chartH + 50f)
        val left = 50f
        val right = pageWidth - 40f
        val top = y
        val bottom = y + chartH
        val fromMs = from.toEpochMilli().toDouble()
        val toMs = to.toEpochMilli().toDouble().coerceAtLeast(fromMs + 1.0)
        val spanMs = toMs - fromMs

        val yMin: Double
        val yMax: Double
        if (unit == GlucoseUnit.MMOL) {
            yMin = 2.0
            yMax = maxOf(12.0, (gValues.maxOrNull() ?: 10.0) + 1.0)
        } else {
            yMin = 40.0
            yMax = maxOf(220.0, (gValues.maxOrNull() ?: 180.0) + 20.0)
        }
        val ySpan = (yMax - yMin).coerceAtLeast(1.0)

        fun xOf(ms: Long): Float = (left + ((ms - fromMs) / spanMs) * (right - left)).toFloat()
        fun yOf(v: Double): Float = (bottom - ((v - yMin) / ySpan) * (bottom - top)).toFloat()

        // cieľové pásmo ~3.9–10 mmol alebo 70–180 mg/dl
        val bandLo = if (unit == GlucoseUnit.MMOL) 3.9 else 70.0
        val bandHi = if (unit == GlucoseUnit.MMOL) 10.0 else 180.0
        canvas.drawRect(left, yOf(bandHi.coerceIn(yMin, yMax)), right, yOf(bandLo.coerceIn(yMin, yMax)), targetBand)

        // mriežka + osi
        val gridSteps = 4
        for (i in 0..gridSteps) {
            val v = yMin + ySpan * i / gridSteps
            val gy = yOf(v)
            canvas.drawLine(left, gy, right, gy, gridPaint)
            canvas.drawText(GlucoseUnits.format(v, unit), 12f, gy + 3f, small)
        }
        canvas.drawRect(left, top, right, bottom, axisPaint)

        // glykémia
        if (glucose.size >= 2) {
            val path = android.graphics.Path()
            glucose.sortedBy { it.date }.forEachIndexed { idx, g ->
                val v = GlucoseUnits.convert(g.sgvMgdl, GlucoseUnit.MGDL, unit)
                val px = xOf(g.date.toEpochMilli())
                val py = yOf(v.coerceIn(yMin, yMax))
                if (idx == 0) path.moveTo(px, py) else path.lineTo(px, py)
            }
            canvas.drawPath(path, glucosePaint)
        } else if (glucose.size == 1) {
            val g = glucose.first()
            val v = GlucoseUnits.convert(g.sgvMgdl, GlucoseUnit.MGDL, unit)
            canvas.drawCircle(xOf(g.date.toEpochMilli()), yOf(v.coerceIn(yMin, yMax)), 2.5f, glucosePaint)
        }

        // inzulínové markery
        for (d in doseRows) {
            val px = xOf(d.administeredAtEpochMs)
            if (px < left || px > right) continue
            val isBasal = insulins[d.insulinId]?.category == "basal"
            val paint = if (isBasal) basalPaint else bolusPaint
            val path = android.graphics.Path().apply {
                moveTo(px, bottom)
                lineTo(px - 4f, bottom - 10f)
                lineTo(px + 4f, bottom - 10f)
                close()
            }
            canvas.drawPath(path, paint)
            // jemna ciarka hore
            canvas.drawLine(px, bottom - 10f, px, top + 4f, Paint(paint).apply {
                style = Paint.Style.STROKE
                strokeWidth = 0.7f
                alpha = 90
            })
        }

        y = bottom + 14f
        canvas.drawText("Zelená = CGM · červený △ = bolus · modrý △ = bazál · pásmo = cieľ", 40f, y, small)
        y += 10f
        canvas.drawText("Čas →", right - 30f, bottom + 12f, small)
        y += 18f

        // —— Kompaktný zoznam dávok ——
        line("Podaný inzulín", titlePaint, 18f)
        if (doseRows.isEmpty()) {
            line("Žiadne dávky v období.", small)
        } else {
            for (day in days) {
                val dDay = byDayDoses[day].orEmpty().sortedBy { it.administeredAtEpochMs }
                if (dDay.isEmpty()) continue
                ensureSpace(28f)
                line(dayFmt.format(day.atStartOfDay(zone)), body, 14f)
                for (d in dDay) {
                    val i = insulins[d.insulinId]
                    val p = profiles[d.profileId]?.name.orEmpty()
                    val s = d.sceneId?.let { scenes[it]?.name }
                    val site = Formatters.siteLabel(d.injectionSite)
                    val text = "  ${timeFmt.format(Instant.ofEpochMilli(d.administeredAtEpochMs))}  " +
                        "${Formatters.units(d.units)} ${i?.name ?: "?"} " +
                        "(${if (i?.category == "basal") "bazál" else "bolus"})" +
                        (s?.let { " · $it" } ?: "") +
                        (if (site.isNotBlank()) " · $site" else "") +
                        " · $p"
                    line(text, small, 11f)
                }
                y += 4f
            }
        }

        // denné priemery G (stručne)
        if (byDayGlucose.isNotEmpty()) {
            ensureSpace(40f)
            line("Denné súhrny glykémie ($unitLabel)", titlePaint, 16f)
            for (day in days) {
                val gDay = byDayGlucose[day].orEmpty()
                if (gDay.isEmpty()) continue
                val vals = gDay.map { GlucoseUnits.convert(it.sgvMgdl, GlucoseUnit.MGDL, unit) }
                line(
                    "  ${dayFmt.format(day.atStartOfDay(zone))}  " +
                        "n=${vals.size}  avg=${GlucoseUnits.format(vals.average(), unit)}  " +
                        "min=${GlucoseUnits.format(vals.min(), unit)}  " +
                        "max=${GlucoseUnits.format(vals.max(), unit)}",
                    small,
                    11f,
                )
            }
        }

        doc.finishPage(page)
        val file = File(context.cacheDir, "glukoview-lekar-export.pdf")
        FileOutputStream(file).use { doc.writeTo(it) }
        doc.close()
        return file
    }
}
