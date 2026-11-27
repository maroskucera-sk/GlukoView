package sk.glukoview.insulinlog.ui.history

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import sk.glukoview.insulinlog.R
import sk.glukoview.insulinlog.di.AppContainer
import sk.glukoview.insulinlog.ui.components.DoseRow
import sk.glukoview.insulinlog.ui.util.Formatters

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HistoryScreen(
    container: AppContainer,
    onOpenDose: (String) -> Unit,
) {
    val vm: HistoryViewModel = viewModel(
        factory = HistoryViewModel.factory(
            container.doseRepository,
            container.catalogRepository,
            container.syncRepository,
        ),
    )
    val state by vm.state.collectAsStateWithLifecycle()
    var pendingDelete by remember { mutableStateOf<String?>(null) }

    pendingDelete?.let { uuid ->
        AlertDialog(
            onDismissRequest = { pendingDelete = null },
            title = { Text("Zmazať záznam?") },
            text = { Text("Záznam sa soft-zmaže a synchronizuje s rodinou.") },
            confirmButton = {
                TextButton(
                    onClick = {
                        vm.delete(uuid)
                        pendingDelete = null
                    },
                ) { Text(stringResource(R.string.delete)) }
            },
            dismissButton = {
                TextButton(onClick = { pendingDelete = null }) {
                    Text(stringResource(R.string.cancel))
                }
            },
        )
    }

    Column(modifier = Modifier.fillMaxSize()) {
        TopAppBar(title = { Text(stringResource(R.string.nav_history)) })
        LazyColumn(
            contentPadding = PaddingValues(16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
            modifier = Modifier.fillMaxSize(),
        ) {
            if (state.groups.isEmpty()) {
                item {
                    Text(
                        "Zatiaľ žiadna história.",
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
            state.groups.forEach { group ->
                item(key = "h-${group.date}") {
                    Text(
                        text = group.title,
                        style = MaterialTheme.typography.titleLarge,
                        fontWeight = FontWeight.SemiBold,
                        modifier = Modifier.padding(top = 8.dp, bottom = 4.dp),
                    )
                }
                item(key = "s-${group.date}") {
                    Surface(
                        shape = RoundedCornerShape(12.dp),
                        color = MaterialTheme.colorScheme.surfaceVariant,
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text(
                            text = "${group.doses.size} dávok · bolus ${Formatters.units(group.bolusTotal)} · " +
                                "bazál ${Formatters.units(group.basalTotal)}",
                            modifier = Modifier.padding(12.dp),
                            style = MaterialTheme.typography.bodyMedium,
                        )
                    }
                }
                items(group.doses, key = { it.uuid }) { dose ->
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
