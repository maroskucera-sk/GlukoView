package sk.glukoview.insulinlog.ui.history

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import sk.glukoview.insulinlog.data.repository.CatalogRepository
import sk.glukoview.insulinlog.data.repository.DoseRepository
import sk.glukoview.insulinlog.data.repository.SyncRepository
import sk.glukoview.insulinlog.domain.DoseRecord
import sk.glukoview.insulinlog.domain.InsulinCategory
import sk.glukoview.insulinlog.domain.InsulinType
import sk.glukoview.insulinlog.domain.Profile
import sk.glukoview.insulinlog.domain.Scene
import sk.glukoview.insulinlog.ui.util.Formatters
import java.time.LocalDate

data class DayGroup(
    val date: LocalDate,
    val title: String,
    val bolusTotal: Double,
    val basalTotal: Double,
    val doses: List<DoseRecord>,
)

data class HistoryUiState(
    val groups: List<DayGroup> = emptyList(),
    val profiles: Map<Long, Profile> = emptyMap(),
    val insulins: Map<Long, InsulinType> = emptyMap(),
    val scenes: Map<Long, Scene> = emptyMap(),
)

class HistoryViewModel(
    private val doseRepository: DoseRepository,
    private val catalogRepository: CatalogRepository,
    private val syncRepository: SyncRepository,
) : ViewModel() {
    val state: StateFlow<HistoryUiState> = combine(
        doseRepository.observeRecent(500),
        catalogRepository.observeInsulins(),
        catalogRepository.observeScenes(),
        catalogRepository.observeProfiles(),
    ) { doses, insulins, scenes, profiles ->
        val insulinMap = insulins.associateBy { it.id }
        val active = doses.filter { it.deletedAt == null }
        val grouped = active
            .groupBy { Formatters.toLocalDate(it.administeredAt) }
            .toSortedMap(compareByDescending { it })
            .map { (date, dayDoses) ->
                val sorted = dayDoses.sortedByDescending { it.administeredAt }
                var bolus = 0.0
                var basal = 0.0
                for (d in sorted) {
                    when (insulinMap[d.insulinId]?.category) {
                        InsulinCategory.BASAL -> basal += d.units
                        else -> bolus += d.units
                    }
                }
                DayGroup(
                    date = date,
                    title = Formatters.dayHeader(date),
                    bolusTotal = (bolus * 100).toInt() / 100.0,
                    basalTotal = (basal * 100).toInt() / 100.0,
                    doses = sorted,
                )
            }
        HistoryUiState(
            groups = grouped,
            profiles = profiles.associateBy { it.id },
            insulins = insulinMap,
            scenes = scenes.associateBy { it.id },
        )
    }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), HistoryUiState())

    fun delete(uuid: String) {
        viewModelScope.launch {
            doseRepository.softDeleteLocal(uuid)
            syncRepository.syncNow()
        }
    }

    companion object {
        fun factory(
            doseRepository: DoseRepository,
            catalogRepository: CatalogRepository,
            syncRepository: SyncRepository,
        ): ViewModelProvider.Factory = object : ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : ViewModel> create(modelClass: Class<T>): T =
                HistoryViewModel(doseRepository, catalogRepository, syncRepository) as T
        }
    }
}
