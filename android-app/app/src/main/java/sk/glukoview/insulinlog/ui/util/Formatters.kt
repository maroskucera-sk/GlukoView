package sk.glukoview.insulinlog.ui.util

import java.time.Duration
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.Locale

object Formatters {
    private val zone: ZoneId = ZoneId.systemDefault()
    private val timeFmt = DateTimeFormatter.ofPattern("H:mm", Locale("sk"))
    private val dayFmt = DateTimeFormatter.ofPattern("EEEE d. M. yyyy", Locale("sk"))
    private val dayShortFmt = DateTimeFormatter.ofPattern("d. M. yyyy", Locale("sk"))
    private val dateTimeFmt = DateTimeFormatter.ofPattern("d. M. H:mm", Locale("sk"))

    fun units(value: Double): String {
        val s = if (value == value.toLong().toDouble()) {
            value.toLong().toString()
        } else {
            String.format(Locale.US, "%.2f", value).trimEnd('0').trimEnd('.')
        }
        return "${s}j"
    }

    fun relative(from: Instant, now: Instant = Instant.now()): String {
        val minutes = Duration.between(from, now).toMinutes()
        return when {
            minutes < 0 -> "o chvíľu"
            minutes < 1 -> "pred chvíľou"
            minutes == 1L -> "pred 1 min"
            minutes < 60 -> "pred $minutes min"
            minutes < 24 * 60 -> {
                val h = minutes / 60
                val m = minutes % 60
                if (m == 0L) "pred ${h}h" else "pred ${h}h ${m}m"
            }
            else -> localDateTime(from)
        }
    }

    fun localTime(instant: Instant): String =
        instant.atZone(zone).toLocalTime().format(timeFmt)

    fun localDateTime(instant: Instant): String =
        instant.atZone(zone).format(dateTimeFmt)

    fun dayHeader(date: LocalDate, today: LocalDate = LocalDate.now(zone)): String =
        when (date) {
            today -> "Dnes · ${date.format(dayShortFmt)}"
            today.minusDays(1) -> "Včera · ${date.format(dayShortFmt)}"
            else -> date.format(dayFmt).replaceFirstChar { it.titlecase(Locale("sk")) }
        }

    fun toLocalDate(instant: Instant): LocalDate =
        instant.atZone(zone).toLocalDate()

    fun startOfDay(date: LocalDate): Instant =
        date.atStartOfDay(zone).toInstant()

    fun endOfDay(date: LocalDate): Instant =
        date.plusDays(1).atStartOfDay(zone).toInstant()

    fun siteLabel(site: String?): String {
        if (site.isNullOrBlank()) return ""
        return when (site.lowercase()) {
            "abdomen" -> "Brucho"
            "thigh" -> "Stehno"
            "arm" -> "Rameno"
            "buttock" -> "Zadok"
            else -> site
        }
    }
}
