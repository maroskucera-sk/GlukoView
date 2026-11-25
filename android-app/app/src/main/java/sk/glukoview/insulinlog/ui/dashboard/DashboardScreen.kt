package sk.glukoview.insulinlog.ui.dashboard

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.pulltorefresh.PullToRefreshBox
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import sk.glukoview.insulinlog.R
import sk.glukoview.insulinlog.di.AppContainer
import sk.glukoview.insulinlog.ui.components.DoseRow
import sk.glukoview.insulinlog.ui.util.Formatters

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DashboardScreen(
    container: AppContainer,
    onOpenDose: (String) -> Unit,
) {
    val vm: DashboardViewModel = viewModel(
        factory = DashboardViewModel.factory(
            container.doseRepository,
            container.catalogRepository,
            container.syncRepository,
            container.nightscout,
            container.preferences,
        ),
    )
    val state by vm.state.collectAsStateWithLifecycle()

    Column(modifier = Modifier.fillMaxSize()) {
        TopAppBar(
            title = {
                Column {
                    Text("GlukoView Inzulín")
                    state.profileName?.let {
                        Text(
                            text = it,
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                }
            },
            actions = {
                IconButton(onClick = vm::refresh) {
                    Icon(Icons.Default.Refresh, contentDescription = "Obnoviť")
                }
            },
        )

        PullToRefreshBox(
            isRefreshing = state.refreshing,
            onRefresh = vm::refresh,
            modifier = Modifier.fillMaxSize(),
        ) {
            if (state.loading) {
                Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    CircularProgressIndicator()
                }
            } else {
                LazyColumn(
                    contentPadding = PaddingValues(16.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    item {
                        GlucoseIobCard(state)
                    }
                    if (state.basalReminder) {
                        item {
                            Surface(
                                shape = RoundedCornerShape(14.dp),
                                color = MaterialTheme.colorScheme.tertiaryContainer,
                            ) {
                                Text(
                                    text = stringResource(R.string.basal_reminder),
                                    modifier = Modifier.padding(14.dp),
                                    style = MaterialTheme.typography.bodyMedium,
                                )
                            }
                        }
                    }
                    state.lastDose?.let { last ->
                        item {
                            LastDoseCard(state, last)
                        }
                    }
                    item {
                        Text(
                            text = stringResource(R.string.today_doses),
                            style = MaterialTheme.typography.titleLarge,
                            modifier = Modifier.padding(top = 8.dp),
                        )
                    }
                    if (state.todayDoses.isEmpty()) {
                        item {
                            Text(
                                text = "Dnes zatiaľ žiadny záznam.",
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    } else {
                        items(state.todayDoses, key = { it.uuid }) { dose ->
                            DoseRow(
                                dose = dose,
                                insulin = state.insulins[dose.insulinId],
                                scene = dose.sceneId?.let { state.scenes[it] },
                                profile = state.profiles[dose.profileId],
                                onClick = { onOpenDose(dose.uuid) },
                            )
                        }
                    }
                    item { Box(Modifier.height(72.dp)) }
                }
            }
        }
    }
}

@Composable
private fun GlucoseIobCard(state: DashboardUiState) {
    Surface(
        shape = RoundedCornerShape(20.dp),
        color = MaterialTheme.colorScheme.primaryContainer,
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(modifier = Modifier.padding(18.dp)) {
            Text(
                text = stringResource(R.string.glucose),
                style = MaterialTheme.typography.labelLarge,
                color = MaterialTheme.colorScheme.onPrimaryContainer,
            )
            val g = state.glucose
            if (g != null) {
                Row(verticalAlignment = Alignment.Bottom, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    Text(
                        text = g.valueLabel,
                        fontSize = 48.sp,
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.onPrimaryContainer,
                    )
                    Text(
                        text = g.trendArrow,
                        fontSize = 32.sp,
                        modifier = Modifier.padding(bottom = 8.dp),
                    )
                    Column(modifier = Modifier.padding(bottom = 10.dp)) {
                        Text(g.unitLabel, style = MaterialTheme.typography.bodyMedium)
                        Text(g.ageLabel, style = MaterialTheme.typography.bodyMedium)
                    }
                }
                val delta = g.deltaLabel
                if (delta != null) {
                    Text(
                        text = "$delta · ${g.trendLabel}",
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onPrimaryContainer.copy(alpha = 0.8f),
                    )
                }
            } else {
                Text(
                    text = state.error ?: "Nightscout nie je nastavený",
                    style = MaterialTheme.typography.bodyLarge,
                    color = MaterialTheme.colorScheme.onPrimaryContainer,
                )
            }
            Box(Modifier.height(12.dp))
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(
                        MaterialTheme.colorScheme.surface.copy(alpha = 0.55f),
                        RoundedCornerShape(12.dp),
                    )
                    .padding(12.dp),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(stringResource(R.string.iob), style = MaterialTheme.typography.titleMedium)
                Text(
                    text = Formatters.units(state.iob),
                    style = MaterialTheme.typography.headlineMedium,
                    fontWeight = FontWeight.SemiBold,
                )
            }
        }
    }
}

@Composable
private fun LastDoseCard(state: DashboardUiState, last: sk.glukoview.insulinlog.domain.DoseRecord) {
    val insulin = state.insulins[last.insulinId]
    val scene = last.sceneId?.let { state.scenes[it] }
    val profile = state.profiles[last.profileId]
    Surface(
        shape = RoundedCornerShape(16.dp),
        color = MaterialTheme.colorScheme.surfaceVariant,
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(modifier = Modifier.padding(14.dp)) {
            Text(stringResource(R.string.last_dose), style = MaterialTheme.typography.labelLarge)
            Text(
                text = buildString {
                    append(Formatters.units(last.units))
                    append(" ")
                    append(insulin?.name ?: "inzulín")
                    scene?.let { append(" – "); append(it.name) }
                    append(", ")
                    append(Formatters.relative(last.administeredAt))
                    profile?.let { append(", podal: "); append(it.name) }
                },
                style = MaterialTheme.typography.bodyLarge,
                fontWeight = FontWeight.Medium,
                modifier = Modifier.padding(top = 4.dp),
            )
        }
    }
}
