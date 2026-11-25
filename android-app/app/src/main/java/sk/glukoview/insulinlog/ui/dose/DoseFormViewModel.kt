package sk.glukoview.insulinlog.ui.dose

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import sk.glukoview.insulinlog.data.local.UserPreferences
import sk.glukoview.insulinlog.data.nightscout.NightscoutClient
import sk.glukoview.insulinlog.data.repository.CatalogRepository
import sk.glukoview.insulinlog.data.repository.DoseRepository
import sk.glukoview.insulinlog.data.repository.SyncRepository
import sk.glukoview.insulinlog.domain.DoseRecord
import sk.glukoview.insulinlog.domain.DuplicateBolusChecker
import sk.glukoview.insulinlog.domain.DuplicateBolusWarning
import sk.glukoview.insulinlog.domain.GlucoseUnit
import sk.glukoview.insulinlog.domain.GlucoseUnits
import sk.glukoview.insulinlog.domain.InjectionSite
import sk.glukoview.insulinlog.domain.InsulinType
import sk.glukoview.insulinlog.domain.Profile
import sk.glukoview.insulinlog.domain.Scene
import java.time.Instant
import java.util.UUID

data class DoseFormUiState(
    val loading: Boolean = true,
    val saving: Boolean = false,
    val isEdit: Boolean = false,
    val uuid: String = "",
    val insulins: List<InsulinType> = emptyList(),
    val scenes: List<Scene> = emptyList(),
    val injectionSites: List<InjectionSite> = emptyList(),
    val profiles: Map<Long, Profile> = emptyMap(),
    val selectedInsulinId: Long? = null,
    val selectedSceneId: Long? = null,
    val unitsText: String = "1",
    val administeredAt: Instant = Instant.now(),
    val glucoseText: String = "",
    val glucoseUnit: GlucoseUnit = GlucoseUnit.MMOL,
    val glucoseTrend: String? = null,
    val carbsSjText: String = "",
    val carbsGramsText: String = "",
    val injectionSite: String? = null,
    val currentInjectionSite: String? = null,
    val note: String = "",
    val profileId: Long? = null,
    val warning: DuplicateBolusWarning? = null,
    val pendingSaveAfterWarning: Boolean = false,
    val error: String? = null,
    val saved: Boolean = false,
    val existingDoses: List<DoseRecord> = emptyList(),
    /** Posledných 10 dávok (celá rodina) na rýchle opakovanie. */
    val recentForRepeat: List<DoseRecord> = emptyList(),
    val glucoseRefreshing: Boolean = false,
)

class DoseFormViewModel(
    private val editUuid: String?,
    private val doseRepository: DoseRepository,
    private val catalogRepository: CatalogRepository,
    private val syncRepository: SyncRepository,
    private val nightscout: NightscoutClient,
    private val preferences: UserPreferences,
) : ViewModel() {
    private val _form = MutableStateFlow(DoseFormUiState(uuid = editUuid ?: UUID.randomUUID().toString(), isEdit = editUuid != null))

    private data class CatalogBundle(
        val insulins: List<InsulinType>,
        val scenes: List<Scene>,
        val sites: List<InjectionSite>,
        val profiles: List<Profile>,
        val doses: List<DoseRecord>,
        val currentSite: String?,
    )

    val state: StateFlow<DoseFormUiState> = combine(
        _form,
        combine(
            catalogRepository.observeInsulins(),
            catalogRepository.observeScenes(),
            catalogRepository.observeInjectionSites(),
            catalogRepository.observeProfiles(),
            combine(
                doseRepository.observeRecent(100),
                catalogRepository.observeSettings(),
            ) { doses, settings -> doses to settings?.currentInjectionSite },
        ) { insulins, scenes, sites, profiles, doseSite ->
            CatalogBundle(
                insulins, scenes, sites, profiles, doseSite.first, doseSite.second,
            )
        },
    ) { form, catalog ->
        form.copy(
            insulins = catalog.insulins,
            scenes = catalog.scenes,
            injectionSites = catalog.sites,
            profiles = catalog.profiles.associateBy { it.id },
            existingDoses = catalog.doses,
            recentForRepeat = if (form.isEdit) emptyList() else catalog.doses.take(10),
            selectedInsulinId = form.selectedInsulinId ?: catalog.insulins.firstOrNull()?.id,
            selectedSceneId = form.selectedSceneId ?: catalog.scenes.firstOrNull()?.id,
            currentInjectionSite = catalog.currentSite,
            loading = false,
        )
    }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), DoseFormUiState())

    init {
        viewModelScope.launch {
            val session = preferences.session.first()
            val settings = catalogRepository.getSettings()
            _form.update {
                it.copy(
                    profileId = session.profileId,
                    glucoseUnit = settings?.glucoseUnit ?: GlucoseUnit.MMOL,
                )
            }
            if (editUuid != null) {
                loadExisting(editUuid)
            } else {
                _form.update { it.copy(injectionSite = settings?.currentInjectionSite) }
                prefillNightscout()
            }
        }
    }

    private suspend fun loadExisting(uuid: String) {
        val dose = doseRepository.getByUuid(uuid) ?: return
        _form.update {
            it.copy(
                uuid = dose.uuid,
                selectedInsulinId = dose.insulinId,
                selectedSceneId = dose.sceneId,
                unitsText = dose.units.toString(),
                administeredAt = dose.administeredAt,
                glucoseText = dose.glucoseValue?.toString().orEmpty(),
                glucoseUnit = dose.glucoseUnit ?: it.glucoseUnit,
                glucoseTrend = dose.glucoseTrend,
                carbsSjText = dose.carbsSj?.toString().orEmpty(),
                carbsGramsText = dose.carbsGrams?.toString().orEmpty(),
                injectionSite = dose.injectionSite,
                note = dose.note.orEmpty(),
                profileId = dose.profileId,
            )
        }
    }

    private suspend fun prefillNightscout() {
        val settings = catalogRepository.getSettings() ?: return
        val url = settings.nightscoutUrl ?: return
        _form.update { it.copy(glucoseRefreshing = true) }
        nightscout.fetchLatest(url, settings.nightscoutToken)
            .onSuccess { (latest, _) ->
                val value = GlucoseUnits.convert(latest.sgvMgdl, GlucoseUnit.MGDL, settings.glucoseUnit)
                _form.update {
                    it.copy(
                        glucoseText = GlucoseUnits.format(value, settings.glucoseUnit),
                        glucoseUnit = settings.glucoseUnit,
                        glucoseTrend = latest.directionRaw,
                        glucoseRefreshing = false,
                    )
                }
            }
            .onFailure {
                _form.update { it.copy(glucoseRefreshing = false) }
            }
    }

    /** Znova stiahne aktuálnu glykémiu z Nightscoutu do poľa. */
    fun refreshGlucoseFromNightscout() {
        viewModelScope.launch { prefillNightscout() }
    }

    fun selectInsulin(id: Long) = _form.update { it.copy(selectedInsulinId = id, error = null) }
    fun selectScene(id: Long?) = _form.update { it.copy(selectedSceneId = id) }
    fun setUnits(text: String) = _form.update { it.copy(unitsText = text.filter { ch -> ch.isDigit() || ch == '.' || ch == ',' }) }
    fun nudgeUnits(delta: Double) {
        val current = _form.value.unitsText.replace(',', '.').toDoubleOrNull() ?: 0.0
        val next = ((current + delta).coerceAtLeast(0.0) * 100).toInt() / 100.0
        _form.update { it.copy(unitsText = if (next == next.toLong().toDouble()) next.toLong().toString() else next.toString()) }
    }
    fun setGlucose(text: String) = _form.update { it.copy(glucoseText = text.filter { ch -> ch.isDigit() || ch == '.' || ch == ',' }) }
    fun setCarbsSj(text: String) = _form.update { it.copy(carbsSjText = text.filter { ch -> ch.isDigit() || ch == '.' || ch == ',' }) }
    fun setCarbsGrams(text: String) = _form.update { it.copy(carbsGramsText = text.filter { ch -> ch.isDigit() || ch == '.' || ch == ',' }) }
    fun setNote(text: String) = _form.update { it.copy(note = text.take(280)) }
    fun setSite(site: String?) = _form.update { it.copy(injectionSite = site) }
    fun useCurrentSite() = _form.update { it.copy(injectionSite = it.currentInjectionSite) }
    fun setAdministeredAt(instant: Instant) = _form.update { it.copy(administeredAt = instant) }
    fun setNow() = _form.update { it.copy(administeredAt = Instant.now()) }
    fun dismissWarning() = _form.update { it.copy(warning = null, pendingSaveAfterWarning = false) }

    fun markSelectedAsCurrent() {
        val site = state.value.injectionSite ?: return
        viewModelScope.launch {
            catalogRepository.setCurrentInjectionSite(site)
        }
    }

    /** Predvyplní inzulín, dávku, scénu, SJ/g, miesto a poznámku. Čas = teraz; glykémiu nechá (NS). */
    fun applyFromRecent(dose: DoseRecord) {
        _form.update {
            it.copy(
                selectedInsulinId = dose.insulinId,
                selectedSceneId = dose.sceneId,
                unitsText = formatUnitsField(dose.units),
                administeredAt = Instant.now(),
                carbsSjText = dose.carbsSj?.let { v -> formatUnitsField(v) }.orEmpty(),
                carbsGramsText = dose.carbsGrams?.let { v -> formatUnitsField(v) }.orEmpty(),
                injectionSite = dose.injectionSite,
                note = dose.note.orEmpty(),
                error = null,
            )
        }
    }

    private fun formatUnitsField(value: Double): String =
        if (value == value.toLong().toDouble()) value.toLong().toString() else value.toString()

    fun save(force: Boolean = false) {
        viewModelScope.launch {
            val form = state.value
            val units = form.unitsText.replace(',', '.').toDoubleOrNull()
            val insulinId = form.selectedInsulinId
            val profileId = form.profileId
            if (units == null || units <= 0.0) {
                _form.update { it.copy(error = "Zadajte platné množstvo jednotiek.") }
                return@launch
            }
            if (insulinId == null || profileId == null) {
                _form.update { it.copy(error = "Vyberte inzulín a profil.") }
                return@launch
            }
            val insulin = form.insulins.firstOrNull { it.id == insulinId }
            if (insulin == null) {
                _form.update { it.copy(error = "Inzulín neexistuje.") }
                return@launch
            }

            if (!force) {
                val settings = catalogRepository.getSettings()
                val warning = DuplicateBolusChecker.findRecentBolus(
                    candidateInsulin = insulin,
                    candidateAt = form.administeredAt,
                    candidateUuid = form.uuid,
                    existingDoses = form.existingDoses,
                    insulins = form.insulins.associateBy { it.id },
                    minutesThreshold = settings?.duplicateBolusMinutes ?: 20,
                )
                if (warning != null) {
                    _form.update { it.copy(warning = warning, pendingSaveAfterWarning = true) }
                    return@launch
                }
            }

            _form.update { it.copy(saving = true, error = null, warning = null) }
            val dose = DoseRecord(
                uuid = form.uuid,
                profileId = profileId,
                insulinId = insulinId,
                sceneId = form.selectedSceneId,
                units = units,
                administeredAt = form.administeredAt,
                glucoseValue = form.glucoseText.replace(',', '.').toDoubleOrNull(),
                glucoseUnit = form.glucoseUnit,
                glucoseTrend = form.glucoseTrend,
                carbsSj = form.carbsSjText.replace(',', '.').toDoubleOrNull(),
                carbsGrams = form.carbsGramsText.replace(',', '.').toDoubleOrNull(),
                injectionSite = form.injectionSite,
                note = form.note.ifBlank { null },
                syncPending = true,
            )
            doseRepository.saveLocal(dose)
            syncRepository.syncNow()
            _form.update { it.copy(saving = false, saved = true) }
        }
    }

    fun confirmWarningAndSave() = save(force = true)

    fun delete(onDone: () -> Unit) {
        viewModelScope.launch {
            doseRepository.softDeleteLocal(state.value.uuid)
            syncRepository.syncNow()
            onDone()
        }
    }

    companion object {
        fun factory(
            editUuid: String?,
            doseRepository: DoseRepository,
            catalogRepository: CatalogRepository,
            syncRepository: SyncRepository,
            nightscout: NightscoutClient,
            preferences: UserPreferences,
        ): ViewModelProvider.Factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T =
                DoseFormViewModel(
                    editUuid, doseRepository, catalogRepository, syncRepository, nightscout, preferences,
                ) as T
        }
    }
}
