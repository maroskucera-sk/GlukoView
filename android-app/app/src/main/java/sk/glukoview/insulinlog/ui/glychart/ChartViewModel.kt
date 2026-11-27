package sk.glukoview.insulinlog.ui.glychart

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import sk.glukoview.insulinlog.data.nightscout.NightscoutClient
import sk.glukoview.insulinlog.data.repository.CatalogRepository
import sk.glukoview.insulinlog.data.repository.DoseRepository
import sk.glukoview.insulinlog.data.repository.SyncRepository
import sk.glukoview.insulinlog.domain.ChartMetrics
import sk.glukoview.insulinlog.domain.DoseRecord
import sk.glukoview.insulinlog.domain.GlucoseUnit
import sk.glukoview.insulinlog.domain.InsulinType
import sk.glukoview.insulinlog.domain.IobCalculator
import sk.glukoview.insulinlog.domain.NightscoutReading
import sk.glukoview.insulinlog.domain.Profile
import sk.glukoview.insulinlog.domain.Scene
import java.time.Instant
import java.time.temporal.ChronoUnit

enum class ChartRange(val hours: Long, val label: String) {
    H3(3, "3 h"),
    H6(6, "6 h"),
    H12(12, "12 h"),
    H24(24, "24 h"),
}

data class ChartUiState(
    val loading: Boolean = true,
    val refreshing: Boolean = false,
    val range: ChartRange = ChartRange.H6,
    val from: Instant = Instant.now().minus(6, ChronoUnit.HOURS),
    val to: Instant = Instant.now(),
    val glucose: List<NightscoutReading> = emptyList(),
    val doses: List<DoseRecord> = emptyList(),
    val insulins: Map<Long, InsulinType> = emptyMap(),
    val profiles: Map<Long, Profile> = emptyMap(),
    val scenes: Map<Long, Scene> = emptyMap(),
    val unit: GlucoseUnit = GlucoseUnit.MMOL,
    val metrics: ChartMetrics = ChartMetrics(),
    val iob: Double = 0.0,
    val selection: ChartSelection? = null,
    val error: String? = null,
    val nsConfigured: Boolean = false,
)

class ChartViewModel(
    private val doseRepository: DoseRepository,
    private val catalogRepository: CatalogRepository,
    private val syncRepository: SyncRepository,
    private val nightscout: NightscoutClient,
) : ViewModel() {
    private val _range = MutableStateFlow(ChartRange.H6)
    private val _glucose = MutableStateFlow<List<NightscoutReading>>(emptyList())
    private val _refreshing = MutableStateFlow(false)
    private val _error = MutableStateFlow<String?>(null)
    private val _selection = MutableStateFlow<ChartSelection?>(null)
    private val _from = MutableStateFlow(Instant.now().minus(6, ChronoUnit.HOURS))
    private val _to = MutableStateFlow(Instant.now())

    private var loadJob: Job? = null

    private val nsSlice = combine(_range, _glucose, _refreshing) { range, glucose, refreshing ->
        Triple(range, glucose, refreshing)
    }
    private val metaSlice = combine(_error, _selection, _from, _to) { error, selection, from, to ->
        MetaSlice(error, selection, from, to)
    }
    private val catalogSlice = combine(
        doseRepository.observeRecent(500),
        catalogRepository.observeInsulins(),
        catalogRepository.observeProfiles(),
    ) { doses, insulins, profiles ->
        Triple(doses, insulins, profiles)
    }
    private val scenesSettings = combine(
        catalogRepository.observeScenes(),
        catalogRepository.observeSettings(),
    ) { scenes, settings ->
        scenes to settings
    }

    val state: StateFlow<ChartUiState> = combine(
        combine(nsSlice, metaSlice) { ns, meta -> ns to meta },
        combine(catalogSlice, scenesSettings) { catalog, scenesSet -> catalog to scenesSet },
    ) { nsMeta, catalogScenes ->
        val (ns, meta) = nsMeta
        val (range, glucose, refreshing) = ns
        val (doses, insulins, profiles) = catalogScenes.first
        val (scenes, settings) = catalogScenes.second
        val unit = settings?.glucoseUnit ?: GlucoseUnit.MMOL
        val insulinMap = insulins.associateBy { it.id }
        val inWindow = doses.filter {
            it.deletedAt == null &&
                !it.administeredAt.isBefore(meta.from) &&
                !it.administeredAt.isAfter(meta.to)
        }.sortedBy { it.administeredAt }
        ChartUiState(
            loading = false,
            refreshing = refreshing,
            range = range,
            from = meta.from,
            to = meta.to,
            glucose = glucose,
            doses = inWindow,
            insulins = insulinMap,
            profiles = profiles.associateBy { it.id },
            scenes = scenes.associateBy { it.id },
            unit = unit,
            metrics = ChartMetrics.compute(glucose, inWindow, insulinMap, unit),
            iob = IobCalculator.activeBolusIob(doses, insulinMap),
            selection = meta.selection,
            error = meta.error,
            nsConfigured = !settings?.nightscoutUrl.isNullOrBlank(),
        )
    }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), ChartUiState())

    init {
        refresh(fullSync = true)
        viewModelScope.launch {
            while (isActive) {
                delay(AUTO_REFRESH_MS)
                refresh(fullSync = false)
            }
        }
    }

    fun setRange(range: ChartRange) {
        _range.value = range
        _selection.value = null
        refresh(fullSync = false)
    }

    fun select(selection: ChartSelection?) {
        _selection.value = selection
    }

    fun refresh(fullSync: Boolean = true) {
        loadJob?.cancel()
        loadJob = viewModelScope.launch {
            _refreshing.value = true
            _error.value = null
            if (fullSync) {
                runCatching { syncRepository.syncNow() }
            }
            val now = Instant.now()
            val from = now.minus(_range.value.hours, ChronoUnit.HOURS)
            _from.value = from
            _to.value = now

            val settings = catalogRepository.getSettings()
            val url = settings?.nightscoutUrl
            if (url.isNullOrBlank()) {
                _glucose.value = emptyList()
                _error.value = "Nightscout URL nie je nastavené — graf ukáže len inzulín."
                _refreshing.value = false
                return@launch
            }
            nightscout.fetchRange(url, settings.nightscoutToken, from, now)
                .onSuccess { readings ->
                    _glucose.value = readings
                    _error.value = if (readings.isEmpty()) {
                        "Nightscout nevrátil merania za zvolené obdobie."
                    } else {
                        null
                    }
                }
                .onFailure { e ->
                    _glucose.value = emptyList()
                    _error.value = e.message ?: "Nightscout nedostupný"
                }
            _refreshing.value = false
        }
    }

    private data class MetaSlice(
        val error: String?,
        val selection: ChartSelection?,
        val from: Instant,
        val to: Instant,
    )

    companion object {
        private const val AUTO_REFRESH_MS = 60_000L

        fun factory(
            doseRepository: DoseRepository,
            catalogRepository: CatalogRepository,
            syncRepository: SyncRepository,
            nightscout: NightscoutClient,
        ): ViewModelProvider.Factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T =
                ChartViewModel(
                    doseRepository, catalogRepository, syncRepository, nightscout,
                ) as T
        }
    }
}
