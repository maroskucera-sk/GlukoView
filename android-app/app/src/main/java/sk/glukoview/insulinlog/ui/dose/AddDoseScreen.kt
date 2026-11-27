package sk.glukoview.insulinlog.ui.dose

import android.app.DatePickerDialog
import android.app.TimePickerDialog
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.ExpandMore
import androidx.compose.material.icons.filled.Remove
import androidx.compose.material.icons.filled.Schedule
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedCard
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import sk.glukoview.insulinlog.R
import sk.glukoview.insulinlog.di.AppContainer
import sk.glukoview.insulinlog.domain.DoseRecord
import sk.glukoview.insulinlog.domain.InjectionSite
import sk.glukoview.insulinlog.domain.InsulinCategory
import sk.glukoview.insulinlog.domain.InsulinType
import sk.glukoview.insulinlog.domain.Profile
import sk.glukoview.insulinlog.domain.Scene
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
            if (!state.isEdit && state.recentForRepeat.isNotEmpty()) {
                SectionLabel("Rýchle opakovanie")
                Text(
                    text = "Posledných ${state.recentForRepeat.size} dávok — ťuknutím predvyplníš formulár (čas = teraz).",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    state.recentForRepeat.forEach { dose ->
                        RecentDoseChip(
                            dose = dose,
                            insulin = state.insulins.firstOrNull { it.id == dose.insulinId },
                            scene = state.scenes.firstOrNull { it.id == dose.sceneId },
                            profile = state.profiles[dose.profileId],
                            onClick = { vm.applyFromRecent(dose) },
                        )
                    }
                }
            }

            SectionLabel(stringResource(R.string.insulin_bolus) + " / " + stringResource(R.string.insulin_basal))
            InsulinDropdown(
                insulins = state.insulins,
                selectedId = state.selectedInsulinId,
                onSelect = vm::selectInsulin,
            )

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
            SceneDropdown(
                scenes = state.scenes,
                selectedId = state.selectedSceneId,
                onSelect = vm::selectScene,
            )

            SectionLabel(stringResource(R.string.time))
            TimeBlock(
                administeredAt = state.administeredAt,
                onNow = vm::setNow,
                onMinus15 = {
                    vm.setAdministeredAt(state.administeredAt.minus(15, ChronoUnit.MINUTES))
                },
                onPick = vm::setAdministeredAt,
            )

            SectionLabel(stringResource(R.string.glucose))
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                OutlinedTextField(
                    value = state.glucoseText,
                    onValueChange = vm::setGlucose,
                    modifier = Modifier.weight(1f),
                    singleLine = true,
                    enabled = !state.glucoseRefreshing,
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                    suffix = {
                        Text(if (state.glucoseUnit.name == "MMOL") "mmol/L" else "mg/dL")
                    },
                    supportingText = {
                        val hint = state.glucoseSourceHint
                        when {
                            state.glucoseRefreshing -> Text("Načítavam z Nightscoutu…")
                            !hint.isNullOrBlank() -> Text(hint)
                            state.glucoseTrend != null -> Text("Trend: ${state.glucoseTrend}")
                        }
                    },
                )
                OutlinedButton(
                    onClick = vm::refreshGlucoseFromNightscout,
                    enabled = !state.glucoseRefreshing,
                ) {
                    Text(if (state.glucoseRefreshing) "…" else "NS")
                }
            }
            Text(
                text = "Pri zmene času sa G doplní z NS k tomu momentu. Tlačidlo NS = vždy aktuálna.",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
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
            if (!state.currentInjectionSite.isNullOrBlank()) {
                Text(
                    text = "Aktuálne nasadené: ${Formatters.siteLabel(state.currentInjectionSite)}",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    FilterChip(
                        selected = state.injectionSite == state.currentInjectionSite,
                        onClick = vm::useCurrentSite,
                        label = { Text("Použiť aktuálne") },
                    )
                    if (!state.injectionSite.isNullOrBlank() &&
                        state.injectionSite != state.currentInjectionSite
                    ) {
                        FilterChip(
                            selected = false,
                            onClick = vm::markSelectedAsCurrent,
                            label = { Text("Nastav ako aktuálne") },
                        )
                    }
                }
            }
            SiteDropdown(
                sites = state.injectionSites,
                selectedName = state.injectionSite,
                onSelect = vm::setSite,
            )

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

@Composable
private fun RecentDoseChip(
    dose: DoseRecord,
    insulin: InsulinType?,
    scene: Scene?,
    profile: Profile?,
    onClick: () -> Unit,
) {
    Surface(
        onClick = onClick,
        shape = RoundedCornerShape(14.dp),
        tonalElevation = 2.dp,
        modifier = Modifier.widthIn(min = 140.dp, max = 200.dp),
    ) {
        Column(modifier = Modifier.padding(horizontal = 12.dp, vertical = 10.dp)) {
            Text(
                text = "${Formatters.units(dose.units)} · ${insulin?.name ?: "?"}",
                style = MaterialTheme.typography.titleSmall,
                fontWeight = FontWeight.SemiBold,
            )
            Text(
                text = listOfNotNull(
                    scene?.name,
                    Formatters.relative(dose.administeredAt),
                    profile?.name,
                ).joinToString(" · "),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun InsulinDropdown(
    insulins: List<InsulinType>,
    selectedId: Long?,
    onSelect: (Long) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    val selected = insulins.firstOrNull { it.id == selectedId }
    val label = selected?.let {
        "${it.name} · ${if (it.category == InsulinCategory.BASAL) "bazál" else "bolus"}"
    }.orEmpty()

    ExposedDropdownMenuBox(
        expanded = expanded,
        onExpandedChange = { expanded = it },
    ) {
        OutlinedTextField(
            value = label.ifBlank { "Vyber inzulín" },
            onValueChange = {},
            readOnly = true,
            modifier = Modifier
                .menuAnchor()
                .fillMaxWidth(),
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
            singleLine = true,
        )
        ExposedDropdownMenu(
            expanded = expanded,
            onDismissRequest = { expanded = false },
        ) {
            insulins.forEach { insulin ->
                DropdownMenuItem(
                    text = {
                        Text(
                            "${insulin.name} · ${
                                if (insulin.category == InsulinCategory.BASAL) "bazál" else "bolus"
                            }",
                        )
                    },
                    onClick = {
                        onSelect(insulin.id)
                        expanded = false
                    },
                )
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SceneDropdown(
    scenes: List<Scene>,
    selectedId: Long?,
    onSelect: (Long?) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    val selected = scenes.firstOrNull { it.id == selectedId }

    ExposedDropdownMenuBox(
        expanded = expanded,
        onExpandedChange = { expanded = it },
    ) {
        OutlinedTextField(
            value = selected?.name ?: "Bez scény",
            onValueChange = {},
            readOnly = true,
            modifier = Modifier
                .menuAnchor()
                .fillMaxWidth(),
            trailingIcon = {
                Icon(Icons.Default.ExpandMore, contentDescription = null)
            },
            singleLine = true,
        )
        ExposedDropdownMenu(
            expanded = expanded,
            onDismissRequest = { expanded = false },
        ) {
            DropdownMenuItem(
                text = { Text("Bez scény") },
                onClick = {
                    onSelect(null)
                    expanded = false
                },
            )
            scenes.forEach { scene ->
                DropdownMenuItem(
                    text = { Text(scene.name) },
                    onClick = {
                        onSelect(scene.id)
                        expanded = false
                    },
                )
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SiteDropdown(
    sites: List<InjectionSite>,
    selectedName: String?,
    onSelect: (String?) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    val label = when {
        selectedName.isNullOrBlank() -> "Bez miesta"
        else -> Formatters.siteLabel(selectedName)
    }

    ExposedDropdownMenuBox(
        expanded = expanded,
        onExpandedChange = { expanded = it },
    ) {
        OutlinedTextField(
            value = label,
            onValueChange = {},
            readOnly = true,
            modifier = Modifier
                .menuAnchor()
                .fillMaxWidth(),
            trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded = expanded) },
            singleLine = true,
        )
        ExposedDropdownMenu(
            expanded = expanded,
            onDismissRequest = { expanded = false },
        ) {
            DropdownMenuItem(
                text = { Text("Bez miesta") },
                onClick = {
                    onSelect(null)
                    expanded = false
                },
            )
            sites.forEach { site ->
                DropdownMenuItem(
                    text = { Text(site.name) },
                    onClick = {
                        onSelect(site.name)
                        expanded = false
                    },
                )
            }
            // Legacy hodnoty (abdomen…) ak ešte nie sú v katalógu
            if (!selectedName.isNullOrBlank() && sites.none { it.name.equals(selectedName, true) }) {
                DropdownMenuItem(
                    text = { Text(Formatters.siteLabel(selectedName)) },
                    onClick = {
                        onSelect(selectedName)
                        expanded = false
                    },
                )
            }
        }
    }
}

@Composable
private fun TimeBlock(
    administeredAt: java.time.Instant,
    onNow: () -> Unit,
    onMinus15: () -> Unit,
    onPick: (java.time.Instant) -> Unit,
) {
    val context = LocalContext.current
    val zone = ZoneId.systemDefault()

    fun openPickers() {
        val zdt = administeredAt.atZone(zone)
        DatePickerDialog(
            context,
            { _, y, m, d ->
                val afterDate = zdt.withYear(y).withMonth(m + 1).withDayOfMonth(d)
                TimePickerDialog(
                    context,
                    { _, hour, minute ->
                        onPick(
                            afterDate.withHour(hour).withMinute(minute).withSecond(0).toInstant(),
                        )
                    },
                    afterDate.hour,
                    afterDate.minute,
                    true,
                ).show()
            },
            zdt.year,
            zdt.monthValue - 1,
            zdt.dayOfMonth,
        ).show()
    }

    OutlinedCard(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = ::openPickers),
        shape = RoundedCornerShape(14.dp),
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 14.dp, vertical = 14.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Icon(
                Icons.Default.Schedule,
                contentDescription = null,
                tint = MaterialTheme.colorScheme.primary,
            )
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = Formatters.localDateTime(administeredAt),
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold,
                )
                Text(
                    text = "Ťukni pre výber dátumu a času",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        FilterChip(selected = false, onClick = onNow, label = { Text("Teraz") })
        FilterChip(selected = false, onClick = onMinus15, label = { Text("−15 min") })
    }
}
