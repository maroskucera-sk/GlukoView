package sk.glukoview.insulinlog.ui.glychart

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import sk.glukoview.insulinlog.domain.ChartMetrics
import sk.glukoview.insulinlog.domain.DoseRecord
import sk.glukoview.insulinlog.domain.GlucoseUnit
import sk.glukoview.insulinlog.domain.GlucoseUnits
import sk.glukoview.insulinlog.domain.InsulinCategory
import sk.glukoview.insulinlog.domain.InsulinType
import sk.glukoview.insulinlog.domain.NightscoutReading
import sk.glukoview.insulinlog.ui.theme.BlueBasal
import sk.glukoview.insulinlog.ui.theme.CoralBolus
import sk.glukoview.insulinlog.ui.theme.TealPrimary
import sk.glukoview.insulinlog.ui.util.Formatters
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.Locale
import kotlin.math.abs
import kotlin.math.max

data class ChartSelection(
    val kind: String,
    val label: String,
    val at: Instant,
)

@Composable
fun GlucoseInsulinChart(
    glucose: List<NightscoutReading>,
    doses: List<DoseRecord>,
    insulins: Map<Long, InsulinType>,
    unit: GlucoseUnit,
    from: Instant,
    to: Instant,
    modifier: Modifier = Modifier,
    onSelect: (ChartSelection?) -> Unit = {},
) {
    val density = LocalDensity.current
    val gridColor = MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.7f)
    val labelColor = MaterialTheme.colorScheme.onSurfaceVariant
    val bandColor = Color(0xFF43A047).copy(alpha = 0.14f)
    val glucoseColor = TealPrimary
    val axisLabelSizePx = with(density) { 10.sp.toPx() }
    val timeFmt = remember {
        DateTimeFormatter.ofPattern("H:mm", Locale("sk")).withZone(ZoneId.systemDefault())
    }

    var selectedMs by remember { mutableStateOf<Long?>(null) }

    Canvas(
        modifier = modifier
            .fillMaxWidth()
            .height(260.dp)
            .pointerInput(glucose, doses, from, to) {
                detectTapGestures { offset ->
                    val left = 48f
                    val right = size.width - 12f
                    if (offset.x < left || offset.x > right) {
                        selectedMs = null
                        onSelect(null)
                        return@detectTapGestures
                    }
                    val fromMs = from.toEpochMilli().toDouble()
                    val toMs = to.toEpochMilli().toDouble().coerceAtLeast(fromMs + 1)
                    val ratio = ((offset.x - left) / (right - left)).toDouble().coerceIn(0.0, 1.0)
                    val tapMs = (fromMs + ratio * (toMs - fromMs)).toLong()

                    val gHit = glucose.minByOrNull { abs(it.date.toEpochMilli() - tapMs) }
                    val dHit = doses.filter { it.deletedAt == null }
                        .minByOrNull { abs(it.administeredAt.toEpochMilli() - tapMs) }

                    val gDist = gHit?.let { abs(it.date.toEpochMilli() - tapMs) } ?: Long.MAX_VALUE
                    val dDist = dHit?.let { abs(it.administeredAt.toEpochMilli() - tapMs) } ?: Long.MAX_VALUE
                    // ~8 % šírky grafu ako tolerancia
                    val tol = ((toMs - fromMs) * 0.04).toLong().coerceAtLeast(5 * 60_000L)

                    when {
                        dHit != null && dDist <= tol && dDist <= gDist -> {
                            selectedMs = dHit.administeredAt.toEpochMilli()
                            val insulin = insulins[dHit.insulinId]
                            val kind = if (insulin?.category == InsulinCategory.BASAL) "bazál" else "bolus"
                            onSelect(
                                ChartSelection(
                                    kind = "dose",
                                    label = "${Formatters.units(dHit.units)} ${insulin?.name ?: "inzulín"} ($kind)",
                                    at = dHit.administeredAt,
                                ),
                            )
                        }
                        gHit != null && gDist <= tol -> {
                            selectedMs = gHit.date.toEpochMilli()
                            val v = GlucoseUnits.convert(gHit.sgvMgdl, GlucoseUnit.MGDL, unit)
                            onSelect(
                                ChartSelection(
                                    kind = "glucose",
                                    label = "${GlucoseUnits.format(v, unit)} ${if (unit == GlucoseUnit.MMOL) "mmol/L" else "mg/dL"}",
                                    at = gHit.date,
                                ),
                            )
                        }
                        else -> {
                            selectedMs = null
                            onSelect(null)
                        }
                    }
                }
            },
    ) {
        val left = 48f
        val right = size.width - 12f
        val top = 16f
        val bottom = size.height - 28f
        if (right <= left || bottom <= top) return@Canvas

        val fromMs = from.toEpochMilli().toDouble()
        val toMs = to.toEpochMilli().toDouble().coerceAtLeast(fromMs + 1)
        val spanMs = toMs - fromMs

        val gValues = glucose.map { GlucoseUnits.convert(it.sgvMgdl, GlucoseUnit.MGDL, unit) }
        val yMin: Double
        val yMax: Double
        if (unit == GlucoseUnit.MMOL) {
            yMin = 2.0
            yMax = max(12.0, (gValues.maxOrNull() ?: 10.0) + 1.0)
        } else {
            yMin = 40.0
            yMax = max(220.0, (gValues.maxOrNull() ?: 180.0) + 20.0)
        }
        val ySpan = (yMax - yMin).coerceAtLeast(1.0)

        fun xOf(ms: Long): Float =
            (left + ((ms - fromMs) / spanMs) * (right - left)).toFloat()

        fun yOf(v: Double): Float =
            (bottom - ((v - yMin) / ySpan) * (bottom - top)).toFloat()

        val bandLo = ChartMetrics.targetLow(unit)
        val bandHi = ChartMetrics.targetHigh(unit)
        drawRect(
            color = bandColor,
            topLeft = Offset(left, yOf(bandHi.coerceIn(yMin, yMax))),
            size = Size(
                right - left,
                yOf(bandLo.coerceIn(yMin, yMax)) - yOf(bandHi.coerceIn(yMin, yMax)),
            ),
        )

        val steps = 4
        val textPaint = android.graphics.Paint().apply {
            color = android.graphics.Color.argb(
                (labelColor.alpha * 255).toInt(),
                (labelColor.red * 255).toInt(),
                (labelColor.green * 255).toInt(),
                (labelColor.blue * 255).toInt(),
            )
            textSize = axisLabelSizePx
            isAntiAlias = true
        }
        for (i in 0..steps) {
            val v = yMin + ySpan * i / steps
            val gy = yOf(v)
            drawLine(gridColor, Offset(left, gy), Offset(right, gy), strokeWidth = 1f)
            drawContext.canvas.nativeCanvas.drawText(
                GlucoseUnits.format(v, unit),
                4f,
                gy + axisLabelSizePx / 3f,
                textPaint,
            )
        }

        // časové značky
        for (i in 0..4) {
            val ms = (fromMs + spanMs * i / 4).toLong()
            val x = xOf(ms)
            drawLine(gridColor, Offset(x, bottom), Offset(x, bottom + 4f), strokeWidth = 1.5f)
            val label = timeFmt.format(Instant.ofEpochMilli(ms))
            val tw = textPaint.measureText(label)
            drawContext.canvas.nativeCanvas.drawText(
                label,
                (x - tw / 2f).coerceIn(left, right - tw),
                size.height - 4f,
                textPaint,
            )
        }

        // CGM čiara
        val sortedG = glucose.sortedBy { it.date }
        if (sortedG.size >= 2) {
            val path = Path()
            sortedG.forEachIndexed { idx, g ->
                val v = GlucoseUnits.convert(g.sgvMgdl, GlucoseUnit.MGDL, unit)
                val p = Offset(xOf(g.date.toEpochMilli()), yOf(v.coerceIn(yMin, yMax)))
                if (idx == 0) path.moveTo(p.x, p.y) else path.lineTo(p.x, p.y)
            }
            drawPath(
                path,
                color = glucoseColor,
                style = Stroke(width = 3.2f, cap = StrokeCap.Round),
            )
        } else if (sortedG.size == 1) {
            val g = sortedG.first()
            val v = GlucoseUnits.convert(g.sgvMgdl, GlucoseUnit.MGDL, unit)
            drawCircle(
                glucoseColor,
                radius = 4f,
                center = Offset(xOf(g.date.toEpochMilli()), yOf(v.coerceIn(yMin, yMax))),
            )
        }

        // inzulínové markery
        for (d in doses) {
            if (d.deletedAt != null) continue
            val px = xOf(d.administeredAt.toEpochMilli())
            if (px < left - 4 || px > right + 4) continue
            val isBasal = insulins[d.insulinId]?.category == InsulinCategory.BASAL
            val color = if (isBasal) BlueBasal else CoralBolus
            val markerH = 14f
            val path = Path().apply {
                moveTo(px, bottom)
                lineTo(px - 6f, bottom - markerH)
                lineTo(px + 6f, bottom - markerH)
                close()
            }
            drawPath(path, color)
            drawLine(
                color = color.copy(alpha = 0.35f),
                start = Offset(px, bottom - markerH),
                end = Offset(px, top + 2f),
                strokeWidth = 1.5f,
            )
        }

        // výber
        selectedMs?.let { ms ->
            val x = xOf(ms)
            drawLine(
                color = labelColor.copy(alpha = 0.55f),
                start = Offset(x, top),
                end = Offset(x, bottom),
                strokeWidth = 2f,
            )
        }
    }
}
