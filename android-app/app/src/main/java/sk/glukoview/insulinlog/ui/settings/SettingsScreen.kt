package sk.glukoview.insulinlog.ui.settings

import android.content.Intent
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
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.core.content.FileProvider
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import sk.glukoview.insulinlog.R
import sk.glukoview.insulinlog.data.repository.DoctorExportFormat
import sk.glukoview.insulinlog.di.AppContainer
import sk.glukoview.insulinlog.domain.GlucoseUnit
import sk.glukoview.insulinlog.domain.InsulinCategory
import sk.glukoview.insulinlog.domain.ThemeMode

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    container: AppContainer,
    onLoggedOut: () -> Unit,
) {
    val vm: SettingsViewModel = viewModel(
        factory = SettingsViewModel.factory(
            container.catalogRepository,
            container.syncRepository,
            container.authRepository,
            container.exportRepository,
            container.preferences,
        ),
    )
    val state by vm.state.collectAsStateWithLifecycle()
    val context = LocalContext.current
    var newInsulin by remember { mutableStateOf("") }
    var newInsulinBasal by remember { mutableStateOf(false) }
    var newScene by remember { mutableStateOf("") }
    var newProfile by remember { mutableStateOf("") }
    var newSite by remember { mutableStateOf("") }

    LaunchedEffect(state.exportFile) {
        val file = state.exportFile ?: return@LaunchedEffect
        val uri = FileProvider.getUriForFile(
            context,
            "${context.packageName}.fileprovider",
            file,
        )
        val mime = when {
            file.name.endsWith(".pdf", ignoreCase = true) -> "application/pdf"
            else -> "text/csv"
        }
        val intent = Intent(Intent.ACTION_SEND).apply {
            type = mime
            putExtra(Intent.EXTRA_STREAM, uri)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        context.startActivity(Intent.createChooser(intent, "Export pre lekára"))
        vm.clearExport()
    }

    Column(modifier = Modifier.fillMaxSize()) {
        TopAppBar(title = { Text(stringResource(R.string.nav_settings)) })
        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Section("Rodina — kto zapisuje")
            Text(
                text = "Vyber aktívny profil na tomto telefóne. Nového člena pridáš nižšie — uvidia ho všetky zariadenia po synci.",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Row(
                modifier = Modifier.horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                state.profiles.forEach { profile ->
                    FilterChip(
                        selected = state.session.profileId == profile.id,
                        onClick = { vm.selectProfile(profile.id) },
                        label = { Text(profile.name) },
                    )
                }
            }
            state.profiles.forEach { profile ->
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    Text(profile.name, style = MaterialTheme.typography.bodyLarge)
                    if (state.session.profileId != profile.id) {
                        TextButton(onClick = { vm.deactivateProfile(profile) }) {
                            Text("Deaktivovať")
                        }
                    }
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(
                    value = newProfile,
                    onValueChange = { newProfile = it },
                    modifier = Modifier.weight(1f),
                    label = { Text("Nový člen rodiny") },
                    singleLine = true,
                    placeholder = { Text("napr. Mama") },
                )
                OutlinedButton(
                    onClick = {
                        if (newProfile.isNotBlank()) {
                            vm.addProfile(newProfile.trim())
                            newProfile = ""
                        }
                    },
                    enabled = !state.busy,
                ) { Text("Pridať") }
            }

            Section("Vzhľad")
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                ThemeMode.entries.forEach { mode ->
                    val label = when (mode) {
                        ThemeMode.SYSTEM -> stringResource(R.string.theme_system)
                        ThemeMode.LIGHT -> stringResource(R.string.theme_light)
                        ThemeMode.DARK -> stringResource(R.string.theme_dark)
                    }
                    FilterChip(
                        selected = state.session.themeMode == mode,
                        onClick = { vm.setTheme(mode) },
                        label = { Text(label) },
                    )
                }
            }

            Section("Nightscout (zdieľané)")
            OutlinedTextField(
                value = state.nightscoutUrl,
                onValueChange = vm::setNsUrl,
                modifier = Modifier.fillMaxWidth(),
                label = { Text("Nightscout URL") },
                singleLine = true,
                placeholder = { Text("https://….herokuapp.com") },
            )
            OutlinedTextField(
                value = state.nightscoutToken,
                onValueChange = vm::setNsToken,
                modifier = Modifier.fillMaxWidth(),
                label = { Text("API Secret / Token") },
                singleLine = true,
                visualTransformation = PasswordVisualTransformation(),
            )
            OutlinedTextField(
                value = state.basalReminderTime,
                onValueChange = vm::setBasalTime,
                modifier = Modifier.fillMaxWidth(),
                label = { Text("Pripomienka bazálu (HH:MM)") },
                singleLine = true,
                placeholder = { Text("21:30") },
            )
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(
                    selected = state.glucoseUnit == GlucoseUnit.MMOL,
                    onClick = { vm.setGlucoseUnit(GlucoseUnit.MMOL) },
                    label = { Text("mmol/L") },
                )
                FilterChip(
                    selected = state.glucoseUnit == GlucoseUnit.MGDL,
                    onClick = { vm.setGlucoseUnit(GlucoseUnit.MGDL) },
                    label = { Text("mg/dL") },
                )
            }
            OutlinedTextField(
                value = state.duplicateMinutes,
                onValueChange = vm::setDuplicateMinutes,
                modifier = Modifier.fillMaxWidth(),
                label = { Text("Varovanie duplicitného bolusu (min)") },
                singleLine = true,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number),
            )
            Button(
                onClick = vm::saveFamilySettings,
                enabled = !state.busy,
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(12.dp),
            ) { Text("Uložiť nastavenia rodiny") }

            Section("Inzulíny")
            state.insulins.filter { !it.isArchived }.forEach { insulin ->
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    Text(
                        "${insulin.name} · ${
                            if (insulin.category == InsulinCategory.BASAL) "bazál" else "bolus"
                        } · ${insulin.durationHours}h",
                    )
                    TextButton(onClick = { vm.archiveInsulin(insulin) }) {
                        Text("Archivovať")
                    }
                }
            }
            OutlinedTextField(
                value = newInsulin,
                onValueChange = { newInsulin = it },
                modifier = Modifier.fillMaxWidth(),
                label = { Text("Nový inzulín") },
                singleLine = true,
            )
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(
                    selected = !newInsulinBasal,
                    onClick = { newInsulinBasal = false },
                    label = { Text("Bolus") },
                )
                FilterChip(
                    selected = newInsulinBasal,
                    onClick = { newInsulinBasal = true },
                    label = { Text("Bazál") },
                )
                OutlinedButton(
                    onClick = {
                        if (newInsulin.isNotBlank()) {
                            vm.addInsulin(
                                newInsulin.trim(),
                                if (newInsulinBasal) InsulinCategory.BASAL else InsulinCategory.BOLUS,
                            )
                            newInsulin = ""
                        }
                    },
                ) { Text("Pridať") }
            }

            Section("Scény")
            state.scenes.filter { !it.isArchived }.forEach { scene ->
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    Text(scene.name)
                    TextButton(onClick = { vm.archiveScene(scene) }) {
                        Text("Archivovať")
                    }
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(
                    value = newScene,
                    onValueChange = { newScene = it },
                    modifier = Modifier.weight(1f),
                    label = { Text("Nová scéna") },
                    singleLine = true,
                )
                OutlinedButton(
                    onClick = {
                        if (newScene.isNotBlank()) {
                            vm.addScene(newScene.trim())
                            newScene = ""
                        }
                    },
                ) { Text("Pridať") }
            }

            Section("Miesta vpichu")
            Text(
                text = "Katalóg miest (Brucho, iPort, …). „Aktuálne nasadené“ je zdieľané v rodine — napr. kde máte teraz iPort.",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            state.injectionSites.filter { !it.isArchived }.forEach { site ->
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                ) {
                    val isCurrent = state.currentInjectionSite.equals(site.name, ignoreCase = true)
                    Text(
                        text = if (isCurrent) "${site.name} · aktuálne" else site.name,
                    )
                    Row {
                        if (!isCurrent) {
                            TextButton(onClick = { vm.setCurrentInjectionSiteNow(site.name) }) {
                                Text("Aktuálne")
                            }
                        }
                        TextButton(onClick = { vm.archiveInjectionSite(site) }) {
                            Text("Archivovať")
                        }
                    }
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(
                    value = newSite,
                    onValueChange = { newSite = it },
                    modifier = Modifier.weight(1f),
                    label = { Text("Nové miesto") },
                    singleLine = true,
                    placeholder = { Text("napr. iPort ľavé brucho") },
                )
                OutlinedButton(
                    onClick = {
                        if (newSite.isNotBlank()) {
                            vm.addInjectionSite(newSite.trim())
                            newSite = ""
                        }
                    },
                ) { Text("Pridať") }
            }
            if (state.currentInjectionSite.isNotBlank()) {
                TextButton(onClick = { vm.setCurrentInjectionSiteNow(null) }) {
                    Text("Zrušiť aktuálne miesto (${state.currentInjectionSite})")
                }
            }

            Section("Export pre diabetológa")
            Text(
                text = "Pri exporte sa z Nightscoutu on-demand stiahne históriá glykémií za obdobie " +
                    "a zlúči sa s inzulínovými dávkami do jedného súboru. CGM sa v telefóne natrvalo neukladá.",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            OutlinedButton(
                onClick = { vm.exportForDoctor(14, DoctorExportFormat.CSV) },
                enabled = !state.busy,
                modifier = Modifier.fillMaxWidth(),
            ) { Text(if (state.busy) "Pripravujem…" else "CSV: gly + inzulín (14 dní)") }
            OutlinedButton(
                onClick = { vm.exportForDoctor(30, DoctorExportFormat.CSV) },
                enabled = !state.busy,
                modifier = Modifier.fillMaxWidth(),
            ) { Text("CSV: gly + inzulín (30 dní)") }
            OutlinedButton(
                onClick = { vm.exportForDoctor(14, DoctorExportFormat.PDF) },
                enabled = !state.busy,
                modifier = Modifier.fillMaxWidth(),
            ) { Text("PDF správa (14 dní)") }
            OutlinedButton(
                onClick = { vm.exportForDoctor(30, DoctorExportFormat.PDF) },
                enabled = !state.busy,
                modifier = Modifier.fillMaxWidth(),
            ) { Text("PDF správa (30 dní)") }

            Section("Dáta")
            OutlinedButton(
                onClick = vm::syncNow,
                enabled = !state.busy,
                modifier = Modifier.fillMaxWidth(),
            ) { Text("Synchronizovať teraz") }

            state.message?.let { Text(it, color = MaterialTheme.colorScheme.primary) }
            state.error?.let { Text(it, color = MaterialTheme.colorScheme.error) }

            Text(
                text = stringResource(R.string.medical_disclaimer),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )

            TextButton(onClick = { vm.logout(onLoggedOut) }) {
                Text("Odhlásiť zariadenie", color = MaterialTheme.colorScheme.error)
            }
            Box(Modifier.height(48.dp))
        }
    }
}

@Composable
private fun Section(title: String) {
    Text(
        text = title,
        style = MaterialTheme.typography.titleLarge,
        modifier = Modifier.padding(top = 8.dp),
    )
}
