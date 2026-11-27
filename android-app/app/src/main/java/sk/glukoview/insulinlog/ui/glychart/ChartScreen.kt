package sk.glukoview.insulinlog.ui.glychart

import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.pulltorefresh.PullToRefreshBox
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import sk.glukoview.insulinlog.R
import sk.glukoview.insulinlog.di.AppContainer
import sk.glukoview.insulinlog.domain.GlucoseUnits
import sk.glukoview.insulinlog.ui.components.DoseRow
import sk.glukoview.insulinlog.ui.theme.BlueBasal
import sk.glukoview.insulinlog.ui.theme.CoralBolus
import sk.glukoview.insulinlog.ui.theme.TealPrimary
import sk.glukoview.insulinlog.ui.util.Formatters

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ChartScreen(
    container: AppContainer,
    onOpenDose: (String) -> Unit,
) {
    val vm = viewModel<ChartViewModel>(
        factory = ChartViewModel.factory(
            container.doseRepository,
            container.catalogRepository,
            container.syncRepository,
            container.nightscout,
        ),
    )
    val state by vm.state.collectAsStateWithLifecycle()
    val lifecycleOwner = LocalLifecycleOwner.current
    DisposableEffect(lifecycleOwner) {
        val observer = LifecycleEventObserver { _, event ->
            if (event == Lifecycle.Event.ON_RESUME) {
                vm.refresh(fullSync = false)
            }
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
    }

    Column(modifier = Modifier.fillMaxSize()) {
        TopAppBar(
            title = { Text(stringResource(R.string.nav_chart)) },
            actions = {
                IconButton(onClick = { vm.refresh(fullSync = true) }) {
                    Icon(Icons.Default.Refresh, contentDescription = "Obnoviť")
                }
            },
        )
        PullToRefreshBox(
            isRefreshing = state.refreshing,
            onRefresh = { vm.refresh(fullSync = true) },
            modifier = Modifier.fillMaxSize(),
        ) {
            if (state.loading) {
                Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    CircularProgressIndicator()
                }
            } else {
                ChartList(state = state, vm = vm, onOpenDose = onOpenDose)
            }
        }
    }
}

@Composable
private fun ChartList(
    state: ChartUiState,
    vm: ChartViewModel,
    onOpenDose: (String) -> Unit,
) {
    val doseItems = state.doses.sortedByDescending { it.administeredAt }
    LazyColumn(
        contentPadding = PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        item {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                for (range in ChartRange.values()) {
                    FilterChip(
                        selected = state.range == range,
                        onClick = { vm.setRange(range) },
                        label = { Text(range.label) },
                    )
                }
            }
        }
        item {
            Surface(
                shape = RoundedCornerShape(20.dp),
                color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.55f),
                modifier = Modifier.fillMaxWidth(),
            ) {
                Column(modifier = Modifier.padding(12.dp)) {
                    Text(
                        text = "CGM + inzulín · ${Formatters.localDateTime(state.from)} – ${Formatters.localTime(state.to)}",
                        style = MaterialTheme.typography.labelLarge,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    Box(Modifier.height(8.dp))
                    GlucoseInsulinChart(
                        glucose = state.glucose,
                        doses = state.doses,
                        insulins = state.insulins,
                        unit = state.unit,
                        from = state.from,
                        to = state.to,
                        onSelect = { vm.select(it) },
                    )
                    Box(Modifier.height(8.dp))
                    LegendRow()
                    val sel = state.selection
                    if (sel != null) {
                        Box(Modifier.height(8.dp))
                        Text(
                            text = "${Formatters.localDateTime(sel.at)} · ${sel.label}",
                            style = MaterialTheme.typography.bodyMedium,
                            fontWeight = FontWeight.Medium,
                        )
                    }
                }
            }
        }
        item { MetricsGrid(state) }
        val err = state.error
        if (!err.isNullOrBlank()) {
            item {
                Text(
                    text = err,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.error,
                )
            }
        }
        item {
            Text(
                text = "Dávky v grafe (${state.doses.size})",
                style = MaterialTheme.typography.titleLarge,
                modifier = Modifier.padding(top = 4.dp),
            )
        }
        if (doseItems.isEmpty()) {
            item {
                Text(
                    text = "V tomto období nie sú žiadne dávky.",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        } else {
            items(doseItems, key = { it.uuid }) { dose ->
                DoseRow(
                    dose = dose,
                    insulin = state.insulins[dose.insulinId],
                    scene = dose.sceneId?.let { id -> state.scenes[id] },
                    profile = state.profiles[dose.profileId],
                    onClick = { onOpenDose(dose.uuid) },
                )
            }
        }
        item { Box(Modifier.height(72.dp)) }
    }
}

@Composable
private fun LegendRow() {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(14.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        LegendDot(TealPrimary, "CGM")
        LegendDot(CoralBolus, "Bolus")
        LegendDot(BlueBasal, "Bazál")
        Text(
            text = "pásmo = cieľ",
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }
}

@Composable
private fun LegendDot(color: Color, label: String) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Box(
            modifier = Modifier
                .size(width = 14.dp, height = 10.dp)
                .background(color, RoundedCornerShape(3.dp)),
        )
        Text(label, style = MaterialTheme.typography.labelMedium)
    }
}

@Composable
private fun MetricsGrid(state: ChartUiState) {
    val m = state.metrics
    val unitLabel = if (state.unit.name == "MMOL") "mmol/L" else "mg/dL"
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            modifier = Modifier.fillMaxWidth(),
        ) {
            StatChip(
                title = "TIR",
                value = m.timeInRangePct?.let { pct -> String.format("%.0f%%", pct) } ?: "—",
                subtitle = "v cieli",
                chipModifier = Modifier.weight(1f),
            )
            StatChip(
                title = "Priemer",
                value = m.average?.let { avg -> GlucoseUnits.format(avg, state.unit) } ?: "—",
                subtitle = unitLabel,
                chipModifier = Modifier.weight(1f),
            )
            StatChip(
                title = "IOB",
                value = Formatters.units(state.iob),
                subtitle = "teraz",
                chipModifier = Modifier.weight(1f),
            )
        }
        Row(
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            modifier = Modifier.fillMaxWidth(),
        ) {
            val minMax = if (m.min != null && m.max != null) {
                "${GlucoseUnits.format(m.min, state.unit)}–${GlucoseUnits.format(m.max, state.unit)}"
            } else {
                "—"
            }
            StatChip(
                title = "Min / max",
                value = minMax,
                subtitle = "$unitLabel · ${m.readingCount} bodov",
                chipModifier = Modifier.weight(1f),
            )
            StatChip(
                title = "Bolus",
                value = Formatters.units(m.bolusTotal),
                subtitle = "v období",
                chipModifier = Modifier.weight(1f),
                accent = CoralBolus,
            )
            StatChip(
                title = "Bazál",
                value = Formatters.units(m.basalTotal),
                subtitle = "v období",
                chipModifier = Modifier.weight(1f),
                accent = BlueBasal,
            )
        }
        if (m.belowPct != null && m.abovePct != null) {
            Text(
                text = "Pod cieľom ${String.format("%.0f%%", m.belowPct)} · nad cieľom ${String.format("%.0f%%", m.abovePct)}",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

@Composable
private fun StatChip(
    title: String,
    value: String,
    subtitle: String,
    chipModifier: Modifier = Modifier,
    accent: Color? = null,
) {
    Surface(
        modifier = chipModifier,
        shape = RoundedCornerShape(14.dp),
        color = MaterialTheme.colorScheme.surface,
        tonalElevation = 1.dp,
    ) {
        Column(modifier = Modifier.padding(horizontal = 12.dp, vertical = 10.dp)) {
            Text(
                text = title,
                style = MaterialTheme.typography.labelMedium,
                color = accent ?: MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Text(
                text = value,
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.SemiBold,
            )
            Text(
                text = subtitle,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}
