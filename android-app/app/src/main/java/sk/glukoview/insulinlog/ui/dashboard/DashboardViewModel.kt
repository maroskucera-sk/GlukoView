package sk.glukoview.insulinlog.ui.dashboard

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import sk.glukoview.insulinlog.data.local.SessionPrefs
import sk.glukoview.insulinlog.data.local.UserPreferences
import sk.glukoview.insulinlog.data.nightscout.NightscoutClient
import sk.glukoview.insulinlog.data.repository.CatalogRepository
import sk.glukoview.insulinlog.data.repository.DoseRepository
import sk.glukoview.insulinlog.data.repository.SyncRepository
import sk.glukoview.insulinlog.domain.DoseRecord
import sk.glukoview.insulinlog.domain.FamilySettings
import sk.glukoview.insulinlog.domain.GlucoseUnit
import sk.glukoview.insulinlog.domain.GlucoseUnits
import sk.glukoview.insulinlog.domain.InsulinCategory
import sk.glukoview.insulinlog.domain.InsulinType
import sk.glukoview.insulinlog.domain.IobCalculator
import sk.glukoview.insulinlog.domain.Profile
import sk.glukoview.insulinlog.domain.Scene
import sk.glukoview.insulinlog.ui.util.Formatters
import java.time.LocalDate
import java.time.LocalTime
import java.time.ZoneId

data class GlucoseUi(
    val valueLabel: String,
    val unitLabel: String,
    val trendArrow: String,
    val trendLabel: String,
    val ageLabel: String,
    val deltaLabel: String?,
)

data class DashboardUiState(
    val loading: Boolean = true,
    val refreshing: Boolean = false,
    val error: String? = null,
    val glucose: GlucoseUi? = null,
    val iob: Double = 0.0,
    val lastDose: DoseRecord? = null,
    val todayDoses: List<DoseRecord> = emptyList(),
    val profiles: Map<Long, Profile> = emptyMap(),
    val insulins: Map<Long, InsulinType> = emptyMap(),
    val scenes: Map<Long, Scene> = emptyMap(),
    val settings: FamilySettings? = null,
    val basalReminder: Boolean = false,
    val profileName: String? = null,
)

class DashboardViewModel(
    private val doseRepository: DoseRepository,
    private val catalogRepository: CatalogRepository,
    private val syncRepository: SyncRepository,
    private val nightscout: NightscoutClient,
    private val preferences: UserPreferences,
) : ViewModel() {
    private val zone = ZoneId.systemDefault()
    private val _glucose = MutableStateFlow<GlucoseUi?>(null)
    private val _refreshing = MutableStateFlow(false)
    private val _nsError = MutableStateFlow<String?>(null)

    private val dosesAndCatalog = combine(
        doseRepository.observeRecent(300),
        catalogRepository.observeInsulins(),
        catalogRepository.observeScenes(),
        catalogRepository.observeProfiles(),
        catalogRepository.observeSettings(),
    ) { doses, insulins, scenes, profiles, settings ->
        DosesCatalog(doses, insulins, scenes, profiles, settings)
    }

    private val sessionAndNs = combine(
        preferences.session,
        _glucose,
        _refreshing,
        _nsError,
    ) { session, glucose, refreshing, nsError ->
        SessionNs(session, glucose, refreshing, nsError)
    }

    val state: StateFlow<DashboardUiState> = combine(dosesAndCatalog, sessionAndNs) { catalog, sessionNs ->
        val insulinMap = catalog.insulins.associateBy { it.id }
        val today = LocalDate.now(zone)
        val from = Formatters.startOfDay(today)
        val to = Formatters.endOfDay(today)
        val todayDoses = catalog.doses.filter {
            it.deletedAt == null && !it.administeredAt.isBefore(from) && it.administeredAt.isBefore(to)
        }.sortedByDescending { it.administeredAt }

        val last = catalog.doses.firstOrNull { it.deletedAt == null }
        val iob = IobCalculator.activeBolusIob(catalog.doses, insulinMap)

        val basalGiven = todayDoses.any { insulinMap[it.insulinId]?.category == InsulinCategory.BASAL }
        val reminderTime = catalog.settings?.basalReminderTime
        val showBasalReminder = if (reminderTime.isNullOrBlank() || basalGiven) {
            false
        } else {
            runCatching {
                val parts = reminderTime.split(":")
                val reminder = LocalTime.of(parts[0].toInt(), parts[1].toInt())
                !LocalTime.now(zone).isBefore(reminder)
            }.getOrDefault(false)
        }

        DashboardUiState(
            loading = false,
            refreshing = sessionNs.refreshing,
            error = sessionNs.nsError,
            glucose = sessionNs.glucose,
            iob = iob,
            lastDose = last,
            todayDoses = todayDoses,
            profiles = catalog.profiles.associateBy { it.id },
            insulins = insulinMap,
            scenes = catalog.scenes.associateBy { it.id },
            settings = catalog.settings,
            basalReminder = showBasalReminder,
            profileName = catalog.profiles.firstOrNull { it.id == sessionNs.session.profileId }?.name,
        )
    }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), DashboardUiState())

    init {
        refresh()
    }

    fun refresh() {
        viewModelScope.launch {
            _refreshing.value = true
            syncRepository.syncNow()
            fetchNightscout()
            _refreshing.value = false
        }
    }

    private suspend fun fetchNightscout() {
        val settings = catalogRepository.getSettings()
        val url = settings?.nightscoutUrl
        if (url.isNullOrBlank()) {
            _glucose.value = null
            _nsError.value = null
            return
        }
        nightscout.fetchLatest(url, settings.nightscoutToken)
            .onSuccess { (latest, previous) ->
                val unit = settings.glucoseUnit
                val value = GlucoseUnits.convert(latest.sgvMgdl, GlucoseUnit.MGDL, unit)
                _glucose.value = GlucoseUi(
                    valueLabel = GlucoseUnits.format(value, unit),
                    unitLabel = if (unit == GlucoseUnit.MMOL) "mmol/L" else "mg/dL",
                    trendArrow = latest.trend.arrow,
                    trendLabel = latest.trend.labelSk,
                    ageLabel = nightscout.formatAge(latest),
                    deltaLabel = nightscout.formatDelta(latest, previous, unit),
                )
                _nsError.value = null
            }
            .onFailure { e ->
                _nsError.value = e.message ?: "Nightscout nedostupný"
            }
    }

    private data class DosesCatalog(
        val doses: List<DoseRecord>,
        val insulins: List<InsulinType>,
        val scenes: List<Scene>,
        val profiles: List<Profile>,
        val settings: FamilySettings?,
    )

    private data class SessionNs(
        val session: SessionPrefs,
        val glucose: GlucoseUi?,
        val refreshing: Boolean,
        val nsError: String?,
    )

    companion object {
        fun factory(
            doseRepository: DoseRepository,
            catalogRepository: CatalogRepository,
            syncRepository: SyncRepository,
            nightscout: NightscoutClient,
            preferences: UserPreferences,
        ): ViewModelProvider.Factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T =
                DashboardViewModel(
                    doseRepository, catalogRepository, syncRepository, nightscout, preferences,
                ) as T
        }
    }
}
