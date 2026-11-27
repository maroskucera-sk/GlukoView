package sk.glukoview.insulinlog.ui.onboarding

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import sk.glukoview.insulinlog.R
import sk.glukoview.insulinlog.domain.Profile

@Composable
fun OnboardingScreen(
    viewModel: OnboardingViewModel,
    onFinished: () -> Unit,
) {
    val state by viewModel.state.collectAsStateWithLifecycle()

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    listOf(
                        MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.55f),
                        MaterialTheme.colorScheme.background,
                    ),
                ),
            ),
    ) {
        when (state.step) {
            OnboardingStep.CREDENTIALS -> CredentialsStep(
                state = state,
                onUrl = viewModel::onServerUrlChange,
                onKey = viewModel::onAccessKeyChange,
                onContinue = viewModel::verify,
            )
            OnboardingStep.PROFILE -> ProfileStep(
                profiles = state.profiles,
                onPick = { id -> viewModel.selectProfile(id, onFinished) },
            )
        }
    }
}

@Composable
private fun CredentialsStep(
    state: OnboardingUiState,
    onUrl: (String) -> Unit,
    onKey: (String) -> Unit,
    onContinue: () -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        verticalArrangement = Arrangement.Center,
    ) {
        Text(
            text = "GlukoView",
            style = MaterialTheme.typography.headlineMedium,
            color = MaterialTheme.colorScheme.primary,
        )
        Text(
            text = stringResource(R.string.onboarding_title),
            style = MaterialTheme.typography.headlineMedium,
            modifier = Modifier.padding(top = 8.dp),
        )
        Text(
            text = stringResource(R.string.medical_disclaimer),
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.padding(top = 8.dp, bottom = 24.dp),
        )

        OutlinedTextField(
            value = state.serverUrl,
            onValueChange = onUrl,
            label = { Text(stringResource(R.string.onboarding_server_url)) },
            placeholder = { Text("https://insulin.vasadomena.sk") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth(),
        )
        Box(Modifier.height(12.dp))
        OutlinedTextField(
            value = state.accessKey,
            onValueChange = onKey,
            label = { Text(stringResource(R.string.onboarding_access_key)) },
            singleLine = true,
            visualTransformation = PasswordVisualTransformation(),
            modifier = Modifier.fillMaxWidth(),
        )

        val errorText = state.error
        if (errorText != null) {
            Text(
                text = errorText,
                color = MaterialTheme.colorScheme.error,
                style = MaterialTheme.typography.bodyMedium,
                modifier = Modifier.padding(top = 12.dp),
            )
        }

        Box(Modifier.height(20.dp))
        Button(
            onClick = onContinue,
            enabled = !state.loading,
            modifier = Modifier
                .fillMaxWidth()
                .height(52.dp),
            shape = RoundedCornerShape(14.dp),
        ) {
            if (state.loading) {
                CircularProgressIndicator(
                    modifier = Modifier.size(22.dp),
                    color = MaterialTheme.colorScheme.onPrimary,
                    strokeWidth = 2.dp,
                )
            } else {
                Text(stringResource(R.string.onboarding_continue))
            }
        }
    }
}

@Composable
private fun ProfileStep(
    profiles: List<Profile>,
    onPick: (Long) -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
    ) {
        Text(
            text = stringResource(R.string.onboarding_pick_profile),
            style = MaterialTheme.typography.headlineMedium,
            modifier = Modifier.padding(top = 48.dp, bottom = 16.dp),
        )
        LazyColumn(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            items(profiles, key = { it.id }) { profile ->
                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickable { onPick(profile.id) },
                    shape = RoundedCornerShape(16.dp),
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.surface,
                    ),
                    elevation = CardDefaults.cardElevation(defaultElevation = 1.dp),
                ) {
                    Row(
                        modifier = Modifier.padding(16.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(14.dp),
                    ) {
                        Box(
                            modifier = Modifier
                                .size(40.dp)
                                .background(parseColor(profile.colorHex), CircleShape),
                        )
                        Text(profile.name, style = MaterialTheme.typography.titleLarge)
                    }
                }
            }
        }
    }
}

private fun parseColor(hex: String): Color =
    runCatching { Color(android.graphics.Color.parseColor(hex)) }
        .getOrDefault(Color.Gray)
