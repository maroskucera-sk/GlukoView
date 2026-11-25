package sk.glukoview.insulinlog.ui.settings

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import sk.glukoview.insulinlog.data.local.SessionPrefs
import sk.glukoview.insulinlog.data.local.UserPreferences
import sk.glukoview.insulinlog.data.repository.AuthRepository
import sk.glukoview.insulinlog.data.repository.CatalogRepository
import sk.glukoview.insulinlog.data.repository.DoctorExportFormat
import sk.glukoview.insulinlog.data.repository.ExportRepository
import sk.glukoview.insulinlog.data.repository.SyncRepository
import sk.glukoview.insulinlog.domain.FamilySettings
import sk.glukoview.insulinlog.domain.GlucoseUnit
import sk.glukoview.insulinlog.domain.InjectionSite
import sk.glukoview.insulinlog.domain.InsulinCategory
import sk.glukoview.insulinlog.domain.InsulinType
import sk.glukoview.insulinlog.domain.Profile
import sk.glukoview.insulinlog.domain.Scene
import sk.glukoview.insulinlog.domain.ThemeMode
import sk.glukoview.insulinlog.ui.util.Formatters
import java.io.File
import java.time.LocalDate

data class SettingsUiState(
    val session: SessionPrefs = SessionPrefs(),
    val profiles: List<Profile> = emptyList(),
    val insulins: List<InsulinType> = emptyList(),
    val scenes: List<Scene> = emptyList(),
    val injectionSites: List<InjectionSite> = emptyList(),
    val nightscoutUrl: String = "",
    val nightscoutToken: String = "",
    val basalReminderTime: String = "",
    val glucoseUnit: GlucoseUnit = GlucoseUnit.MMOL,
    val duplicateMinutes: String = "20",
    val currentInjectionSite: String = "",
    val message: String? = null,
    val error: String? = null,
    val busy: Boolean = false,
    val exportFile: File? = null,
)

class SettingsViewModel(
    private val catalogRepository: CatalogRepository,
    private val syncRepository: SyncRepository,
    private val authRepository: AuthRepository,
    private val exportRepository: ExportRepository,
    private val preferences: UserPreferences,
) : ViewModel() {
    private val _draft = MutableStateFlow(SettingsDraft())
    private val _flash = MutableStateFlow<Flash?>(null)
    private val _busy = MutableStateFlow(false)
    private val _export = MutableStateFlow<File?>(null)

    private data class SettingsDraft(
        val nightscoutUrl: String = "",
        val nightscoutToken: String = "",
        val basalReminderTime: String = "",
        val glucoseUnit: GlucoseUnit = GlucoseUnit.MMOL,
        val duplicateMinutes: String = "20",
        val currentInjectionSite: String = "",
        val hydrated: Boolean = false,
    )

    private data class Flash(val message: String? = null, val error: String? = null)

    private data class CatalogSlice(
        val profiles: List<Profile>,
        val insulins: List<InsulinType>,
        val scenes: List<Scene>,
        val injectionSites: List<InjectionSite>,
    )

    private data class DraftBundle(
        val draft: SettingsDraft,
        val flash: Flash?,
        val busy: Boolean,
        val export: File?,
        val settings: FamilySettings?,
    )

    val state: StateFlow<SettingsUiState> = combine(
        preferences.session,
        combine(
            catalogRepository.observeProfiles(),
            catalogRepository.observeAllInsulins(),
            catalogRepository.observeAllScenes(),
            catalogRepository.observeAllInjectionSites(),
        ) { profiles, insulins, scenes, sites ->
            CatalogSlice(profiles, insulins, scenes, sites)
        },
        combine(_draft, _flash, _busy, _export, catalogRepository.observeSettings()) { draft, flash, busy, export, settings ->
            DraftBundle(draft, flash, busy, export, settings)
        },
    ) { session, catalog, bundle ->
        val draft = bundle.draft
        SettingsUiState(
            session = session,
            profiles = catalog.profiles,
            insulins = catalog.insulins,
            scenes = catalog.scenes,
            injectionSites = catalog.injectionSites,
            nightscoutUrl = draft.nightscoutUrl,
            nightscoutToken = draft.nightscoutToken,
            basalReminderTime = draft.basalReminderTime,
            glucoseUnit = draft.glucoseUnit,
            duplicateMinutes = draft.duplicateMinutes,
            currentInjectionSite = draft.currentInjectionSite,
            message = bundle.flash?.message,
            error = bundle.flash?.error,
            busy = bundle.busy,
            exportFile = bundle.export,
        )
    }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), SettingsUiState())

    init {
        viewModelScope.launch {
            catalogRepository.observeSettings().collect { settings ->
                if (settings != null && !_draft.value.hydrated) {
                    _draft.value = SettingsDraft(
                        nightscoutUrl = settings.nightscoutUrl.orEmpty(),
                        nightscoutToken = settings.nightscoutToken.orEmpty(),
                        basalReminderTime = settings.basalReminderTime.orEmpty(),
                        glucoseUnit = settings.glucoseUnit,
                        duplicateMinutes = settings.duplicateBolusMinutes.toString(),
                        currentInjectionSite = settings.currentInjectionSite.orEmpty(),
                        hydrated = true,
                    )
                }
            }
        }
    }

    fun setNsUrl(v: String) = _draft.update { it.copy(nightscoutUrl = v) }
    fun setNsToken(v: String) = _draft.update { it.copy(nightscoutToken = v) }
    fun setBasalTime(v: String) = _draft.update { it.copy(basalReminderTime = v) }
    fun setGlucoseUnit(u: GlucoseUnit) = _draft.update { it.copy(glucoseUnit = u) }
    fun setDuplicateMinutes(v: String) = _draft.update { it.copy(duplicateMinutes = v.filter { ch -> ch.isDigit() }) }
    fun setCurrentInjectionSiteDraft(v: String) = _draft.update { it.copy(currentInjectionSite = v) }

    fun setCurrentInjectionSiteNow(name: String?) {
        viewModelScope.launch {
            val cleared = name.isNullOrBlank()
            _draft.update { it.copy(currentInjectionSite = if (cleared) "" else name!!.trim()) }
            catalogRepository.setCurrentInjectionSite(if (cleared) null else name!!.trim())
                .onSuccess { saved ->
                    _draft.update {
                        it.copy(currentInjectionSite = saved.currentInjectionSite.orEmpty())
                    }
                    _flash.value = Flash(
                        message = if (saved.currentInjectionSite.isNullOrBlank()) {
                            "Aktuálne miesto zrušené."
                        } else {
                            "Aktuálne miesto: ${saved.currentInjectionSite}"
                        },
                    )
                }
                .onFailure { e -> _flash.value = Flash(error = e.message) }
        }
    }

    fun setTheme(mode: ThemeMode) {
        viewModelScope.launch { preferences.setTheme(mode) }
    }

    fun selectProfile(id: Long) {
        viewModelScope.launch { authRepository.selectProfile(id) }
    }

    fun addProfile(name: String) {
        viewModelScope.launch {
            _busy.value = true
            val colors = listOf("#1976D2", "#C2185B", "#7B1FA2", "#00897B", "#F57C00", "#5D4037", "#455A64")
            val color = colors[state.value.profiles.size % colors.size]
            catalogRepository.createProfile(name.trim(), color)
                .onSuccess {
                    _flash.value = Flash(message = "Profil „${name.trim()}“ pridaný. Po synci ho uvidia aj ostatné zariadenia.")
                    syncRepository.syncNow()
                }
                .onFailure { e ->
                    _flash.value = Flash(error = e.message ?: "Profil sa nepodarilo pridať.")
                }
            _busy.value = false
        }
    }

    fun deactivateProfile(profile: Profile) {
        viewModelScope.launch {
            catalogRepository.updateProfile(
                id = profile.id,
                name = profile.name,
                colorHex = profile.colorHex,
                isActive = false,
            ).onSuccess {
                _flash.value = Flash(message = "Profil „${profile.name}“ deaktivovaný.")
                syncRepository.syncNow()
            }.onFailure { e ->
                _flash.value = Flash(error = e.message)
            }
        }
    }

    fun saveFamilySettings() {
        viewModelScope.launch {
            _busy.value = true
            val d = _draft.value
            catalogRepository.saveSettings(
                nightscoutUrl = d.nightscoutUrl.ifBlank { null },
                nightscoutToken = d.nightscoutToken.ifBlank { null },
                basalReminderTime = d.basalReminderTime.ifBlank { null },
                glucoseUnit = d.glucoseUnit,
                duplicateBolusMinutes = d.duplicateMinutes.toIntOrNull() ?: 20,
                currentInjectionSite = d.currentInjectionSite.ifBlank { null },
            ).onSuccess {
                _flash.value = Flash(message = "Nastavenia uložené.")
            }.onFailure { e ->
                _flash.value = Flash(error = e.message ?: "Uloženie zlyhalo.")
            }
            _busy.value = false
        }
    }

    fun syncNow() {
        viewModelScope.launch {
            _busy.value = true
            syncRepository.syncNow()
                .onSuccess { _flash.value = Flash(message = "Synchronizácia hotová.") }
                .onFailure { e -> _flash.value = Flash(error = e.message ?: "Sync zlyhal.") }
            _busy.value = false
        }
    }

    fun addInsulin(name: String, category: InsulinCategory) {
        viewModelScope.launch {
            _busy.value = true
            catalogRepository.createInsulin(
                name = name,
                category = category,
                colorHex = if (category == InsulinCategory.BASAL) "#1E88E5" else "#E53935",
                durationHours = if (category == InsulinCategory.BASAL) 24.0 else 4.0,
            ).onSuccess {
                _flash.value = Flash(message = "Inzulín pridaný.")
            }.onFailure { e ->
                _flash.value = Flash(error = e.message)
            }
            _busy.value = false
        }
    }

    fun archiveInsulin(insulin: InsulinType) {
        viewModelScope.launch {
            catalogRepository.updateInsulin(
                id = insulin.id,
                name = insulin.name,
                category = insulin.category,
                colorHex = insulin.colorHex,
                durationHours = insulin.durationHours,
                isArchived = true,
            )
        }
    }

    fun addScene(name: String) {
        viewModelScope.launch {
            catalogRepository.createScene(name, "meal")
                .onSuccess { _flash.value = Flash(message = "Scéna pridaná.") }
                .onFailure { e -> _flash.value = Flash(error = e.message) }
        }
    }

    fun archiveScene(scene: Scene) {
        viewModelScope.launch {
            catalogRepository.updateScene(scene.id, scene.name, scene.iconKey, isArchived = true)
        }
    }

    fun addInjectionSite(name: String) {
        viewModelScope.launch {
            catalogRepository.createInjectionSite(name.trim())
                .onSuccess {
                    _flash.value = Flash(message = "Miesto „${name.trim()}“ pridané.")
                    syncRepository.syncNow()
                }
                .onFailure { e -> _flash.value = Flash(error = e.message) }
        }
    }

    fun archiveInjectionSite(site: InjectionSite) {
        viewModelScope.launch {
            catalogRepository.updateInjectionSite(site.id, site.name, isArchived = true)
                .onSuccess { syncRepository.syncNow() }
                .onFailure { e -> _flash.value = Flash(error = e.message) }
        }
    }

    fun exportForDoctor(days: Long, format: DoctorExportFormat) {
        viewModelScope.launch {
            _busy.value = true
            runCatching {
                val to = Formatters.endOfDay(LocalDate.now())
                val from = Formatters.startOfDay(LocalDate.now().minusDays(days - 1))
                exportRepository.buildDoctorExport(from, to, format)
            }.onSuccess { result ->
                _export.value = result.file
                val nsPart = when {
                    result.nightscoutOk -> "CGM ${result.glucoseCount} bodov"
                    else -> "CGM nedostupné"
                }
                val kind = if (format == DoctorExportFormat.PDF) "PDF" else "CSV"
                _flash.value = Flash(
                    message = "Export $kind: ${result.doseCount} dávok, $nsPart — pripravené na zdieľanie.",
                )
            }.onFailure { e ->
                _flash.value = Flash(error = e.message)
            }
            _busy.value = false
        }
    }

    fun clearExport() {
        _export.value = null
    }

    fun logout(onDone: () -> Unit) {
        viewModelScope.launch {
            authRepository.logout()
            onDone()
        }
    }

    companion object {
        fun factory(
            catalogRepository: CatalogRepository,
            syncRepository: SyncRepository,
            authRepository: AuthRepository,
            exportRepository: ExportRepository,
            preferences: UserPreferences,
        ): ViewModelProvider.Factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T =
                SettingsViewModel(
                    catalogRepository, syncRepository, authRepository, exportRepository, preferences,
                ) as T
        }
    }
}
