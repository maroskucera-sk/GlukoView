package sk.glukoview.insulinlog.ui.dose

import android.app.DatePickerDialog
import android.app.TimePickerDialog
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Remove
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import sk.glukoview.insulinlog.R
import sk.glukoview.insulinlog.di.AppContainer
import sk.glukoview.insulinlog.domain.InjectionSite
import sk.glukoview.insulinlog.domain.InsulinCategory
import sk.glukoview.insulinlog.ui.util.Formatters
import java.time.ZoneId
import java.time.temporal.ChronoUnit

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AddDoseScreen(
    container: AppContainer,
    editUuid: String? = null,
    onClose: () -> Unit,
) {
    val vm: DoseFormViewModel = viewModel(
        factory = DoseFormViewModel.factory(
            editUuid,
            container.doseRepository,
            container.catalogRepository,
            container.syncRepository,
            container.nightscout,
            container.preferences,
        ),
    )
    val state by vm.state.collectAsStateWithLifecycle()

    LaunchedEffect(state.saved) {
        if (state.saved) onClose()
    }

    val warning = state.warning
    if (warning != null) {
        val recent = warning.recent
        val insulinName = state.insulins.firstOrNull { it.id == recent.insulinId }?.name ?: "inzulín"
        val profileName = state.profiles[recent.profileId]?.name ?: "?"
        AlertDialog(
            onDismissRequest = vm::dismissWarning,
            title = { Text(stringResource(R.string.duplicate_bolus_title)) },
            text = {
                Text(
                    "Nedávny bolus: ${Formatters.units(recent.units)} $insulinName " +
                        "(${Formatters.relative(recent.administeredAt)}, podal: $profileName).\n\n" +
                        "Naozaj chcete pridať ďalší záznam?",
                )
            },
            confirmButton = {
                TextButton(onClick = vm::confirmWarningAndSave) { Text("Aj tak uložiť") }
            },
            dismissButton = {
                TextButton(onClick = vm::dismissWarning) { Text(stringResource(R.string.cancel)) }
            },
        )
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Text(
                        if (state.isEdit) stringResource(R.string.edit)
                        else stringResource(R.string.nav_add_dose),
                    )
                },
                navigationIcon = {
                    IconButton(onClick = onClose) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = null)
                    }
                },
                actions = {
                    if (state.isEdit) {
                        TextButton(onClick = { vm.delete(onClose) }) {
                            Text(stringResource(R.string.delete), color = MaterialTheme.colorScheme.error)
                        }
                    }
                },
            )
        },
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp),
        ) {
            SectionLabel(stringResource(R.string.insulin_bolus) + " / " + stringResource(R.string.insulin_basal))
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                state.insulins.forEach { insulin ->
                    FilterChip(
                        selected = state.selectedInsulinId == insulin.id,
                        onClick = { vm.selectInsulin(insulin.id) },
                        label = {
                            Text(
                                "${insulin.name} · ${
                                    if (insulin.category == InsulinCategory.BASAL) "bazál"
                                    else "bolus"
                                }",
                            )
                        },
                    )
                }
            }

            SectionLabel(stringResource(R.string.units))
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                IconButton(onClick = { vm.nudgeUnits(-0.5) }) {
                    Icon(Icons.Default.Remove, contentDescription = "-0.5")
                }
                OutlinedTextField(
                    value = state.unitsText,
                    onValueChange = vm::setUnits,
                    modifier = Modifier.weight(1f),
                    singleLine = true,
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                    suffix = { Text("j") },
                )
                IconButton(onClick = { vm.nudgeUnits(0.5) }) {
                    Icon(Icons.Default.Add, contentDescription = "+0.5")
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(onClick = { vm.nudgeUnits(1.0) }) { Text("+1") }
                OutlinedButton(onClick = { vm.nudgeUnits(-1.0) }) { Text("-1") }
            }

            SectionLabel(stringResource(R.string.scene))
            Row(
                modifier = Modifier.horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                state.scenes.forEach { scene ->
                    FilterChip(
                        selected = state.selectedSceneId == scene.id,
                        onClick = { vm.selectScene(scene.id) },
                        label = { Text(scene.name) },
                    )
                }
            }

            SectionLabel(stringResource(R.string.time))
            val context = LocalContext.current
            val zone = ZoneId.systemDefault()
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                Text(Formatters.localDateTime(state.administeredAt), modifier = Modifier.weight(1f))
                OutlinedButton(onClick = vm::setNow) { Text("Teraz") }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedButton(
                    onClick = {
                        val zdt = state.administeredAt.atZone(zone)
                        DatePickerDialog(
                            context,
                            { _, y, m, d ->
                                val updated = zdt.withYear(y).withMonth(m + 1).withDayOfMonth(d).toInstant()
                                vm.setAdministeredAt(updated)
                            },
                            zdt.year,
                            zdt.monthValue - 1,
                            zdt.dayOfMonth,
                        ).show()
                    },
                ) { Text("Dátum") }
                OutlinedButton(
                    onClick = {
                        val zdt = state.administeredAt.atZone(zone)
                        TimePickerDialog(
                            context,
                            { _, hour, minute ->
                                vm.setAdministeredAt(zdt.withHour(hour).withMinute(minute).withSecond(0).toInstant())
                            },
                            zdt.hour,
                            zdt.minute,
                            true,
                        ).show()
                    },
                ) { Text("Čas") }
                OutlinedButton(
                    onClick = {
                        vm.setAdministeredAt(state.administeredAt.minus(15, ChronoUnit.MINUTES))
                    },
                ) { Text("-15 min") }
            }

            SectionLabel(stringResource(R.string.glucose))
            OutlinedTextField(
                value = state.glucoseText,
                onValueChange = vm::setGlucose,
                modifier = Modifier.fillMaxWidth(),
                singleLine = true,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                suffix = {
                    Text(if (state.glucoseUnit.name == "MMOL") "mmol/L" else "mg/dL")
                },
                supportingText = {
                    state.glucoseTrend?.let { Text("Trend: $it") }
                },
            )

            SectionLabel(stringResource(R.string.carbs))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(
                    value = state.carbsSjText,
                    onValueChange = vm::setCarbsSj,
                    modifier = Modifier.weight(1f),
                    label = { Text("SJ") },
                    singleLine = true,
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                )
                OutlinedTextField(
                    value = state.carbsGramsText,
                    onValueChange = vm::setCarbsGrams,
                    modifier = Modifier.weight(1f),
                    label = { Text("g") },
                    singleLine = true,
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                )
            }

            SectionLabel(stringResource(R.string.injection_site))
            Row(
                modifier = Modifier.horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                InjectionSite.entries.forEach { site ->
                    FilterChip(
                        selected = state.injectionSite == site,
                        onClick = {
                            vm.setSite(if (state.injectionSite == site) null else site)
                        },
                        label = { Text(Formatters.siteLabel(site)) },
                    )
                }
            }

            SectionLabel(stringResource(R.string.note))
            OutlinedTextField(
                value = state.note,
                onValueChange = vm::setNote,
                modifier = Modifier.fillMaxWidth(),
                minLines = 2,
            )

            state.error?.let {
                Text(it, color = MaterialTheme.colorScheme.error)
            }

            Button(
                onClick = { vm.save() },
                enabled = !state.saving,
                modifier = Modifier
                    .fillMaxWidth()
                    .height(52.dp),
                shape = RoundedCornerShape(14.dp),
            ) {
                Text(if (state.saving) "Ukladám…" else stringResource(R.string.save))
            }
            Box(Modifier.height(24.dp))
        }
    }
}

@Composable
private fun SectionLabel(text: String) {
    Text(text, style = MaterialTheme.typography.titleMedium)
}
