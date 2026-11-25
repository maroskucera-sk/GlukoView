package sk.glukoview.insulinlog.data.repository

import android.content.Context
import sk.glukoview.insulinlog.data.local.AppDatabase
import sk.glukoview.insulinlog.domain.InjectionSite
import sk.glukoview.insulinlog.ui.util.Formatters
import java.io.File
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter

class ExportRepository(
    private val context: Context,
    private val db: AppDatabase,
) {
    suspend fun buildCsvFile(from: Instant, to: Instant): File {
        val rows = db.doses().listBetween(from.toEpochMilli(), to.toEpochMilli())
        val profiles = db.profiles().getAll().associateBy { it.id }
        val insulins = db.insulins().getAll().associateBy { it.id }
        val scenes = db.scenes().getAll().associateBy { it.id }

        val zone = ZoneId.systemDefault()
        val fmt = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm").withZone(zone)
        val sb = StringBuilder()
        sb.append('\uFEFF')
        sb.append("Datum_cas;Profil;Inzulin;Kategoria;Jednotky;Scena;Glykemia;Jednotka_glykemie;Trend;SJ;Sacharidy_g;Miesto_vpichu;Poznamka\n")
        for (d in rows) {
            val p = profiles[d.profileId]?.name.orEmpty()
            val i = insulins[d.insulinId]
            val s = d.sceneId?.let { scenes[it]?.name }.orEmpty()
            val cat = if (i?.category == "basal") "bazalny" else "rychly"
            val site = d.injectionSite?.let { raw ->
                InjectionSite.fromApi(raw)?.let { Formatters.siteLabel(it) } ?: raw
            }.orEmpty()
            sb.append(
                listOf(
                    fmt.format(Instant.ofEpochMilli(d.administeredAtEpochMs)),
                    p,
                    i?.name.orEmpty(),
                    cat,
                    d.units.toString(),
                    s,
                    d.glucoseValue?.toString().orEmpty(),
                    d.glucoseUnit.orEmpty(),
                    d.glucoseTrend.orEmpty(),
                    d.carbsSj?.toString().orEmpty(),
                    d.carbsGrams?.toString().orEmpty(),
                    site,
                    (d.note ?: "").replace(';', ','),
                ).joinToString(";") { it.replace("\n", " ") },
            ).append('\n')
        }

        val file = File(context.cacheDir, "glukoview-inzulin-export.csv")
        file.writeText(sb.toString(), Charsets.UTF_8)
        return file
    }
}
