package sk.glukoview.insulinlog.ui.onboarding

import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import sk.glukoview.insulinlog.data.repository.AuthRepository
import sk.glukoview.insulinlog.domain.Profile

data class OnboardingUiState(
    val serverUrl: String = "",
    val accessKey: String = "",
    val loading: Boolean = false,
    val error: String? = null,
    val profiles: List<Profile> = emptyList(),
    val step: OnboardingStep = OnboardingStep.CREDENTIALS,
)

enum class OnboardingStep { CREDENTIALS, PROFILE }

class OnboardingViewModel(
    private val authRepository: AuthRepository,
) : ViewModel() {
    private val _state = MutableStateFlow(OnboardingUiState())
    val state: StateFlow<OnboardingUiState> = _state.asStateFlow()

    fun onServerUrlChange(value: String) = _state.update { it.copy(serverUrl = value, error = null) }
    fun onAccessKeyChange(value: String) = _state.update { it.copy(accessKey = value, error = null) }

    fun verify() {
        val url = _state.value.serverUrl.trim()
        val key = _state.value.accessKey.trim()
        if (url.isEmpty() || key.isEmpty()) {
            _state.update { it.copy(error = "Vyplňte URL aj prístupový kľúč.") }
            return
        }
        viewModelScope.launch {
            _state.update { it.copy(loading = true, error = null) }
            authRepository.verifyAndStore(url, key)
                .onSuccess { profiles ->
                    _state.update {
                        it.copy(
                            loading = false,
                            profiles = profiles,
                            step = OnboardingStep.PROFILE,
                        )
                    }
                }
                .onFailure { e ->
                    _state.update {
                        it.copy(
                            loading = false,
                            error = e.message ?: "Prihlásenie zlyhalo.",
                        )
                    }
                }
        }
    }

    fun selectProfile(profileId: Long, onDone: () -> Unit) {
        viewModelScope.launch {
            authRepository.selectProfile(profileId)
            onDone()
        }
    }

    companion object {
        fun factory(authRepository: AuthRepository): ViewModelProvider.Factory =
            object : ViewModelProvider.Factory {
                @Suppress("UNCHECKED_CAST")
                override fun <T : ViewModel> create(modelClass: Class<T>): T =
                    OnboardingViewModel(authRepository) as T
            }
    }
}
