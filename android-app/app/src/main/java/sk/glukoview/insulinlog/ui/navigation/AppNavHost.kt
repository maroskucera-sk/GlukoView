package sk.glukoview.insulinlog.ui.navigation

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.NavGraph.Companion.findStartDestination
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import kotlinx.coroutines.flow.first
import sk.glukoview.insulinlog.R
import sk.glukoview.insulinlog.data.local.SessionPrefs
import sk.glukoview.insulinlog.di.AppContainer
import sk.glukoview.insulinlog.ui.dashboard.DashboardScreen
import sk.glukoview.insulinlog.ui.dose.AddDoseScreen
import sk.glukoview.insulinlog.ui.history.HistoryScreen
import sk.glukoview.insulinlog.ui.onboarding.OnboardingScreen
import sk.glukoview.insulinlog.ui.onboarding.OnboardingViewModel
import sk.glukoview.insulinlog.ui.settings.SettingsScreen

@Composable
fun AppNavHost(container: AppContainer) {
    var bootSession by remember { mutableStateOf<SessionPrefs?>(null) }
    LaunchedEffect(Unit) {
        bootSession = container.preferences.session.first()
    }
    val session = bootSession
    if (session == null) {
        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            CircularProgressIndicator()
        }
        return
    }

    val navController = rememberNavController()
    val start = if (session.isLoggedIn) Routes.DASHBOARD else Routes.ONBOARDING

    val backStack by navController.currentBackStackEntryAsState()
    val current = backStack?.destination?.route
    val showChrome = current in setOf(Routes.DASHBOARD, Routes.HISTORY, Routes.SETTINGS)

    Scaffold(
        bottomBar = {
            if (showChrome) {
                NavigationBar {
                    NavigationBarItem(
                        selected = current == Routes.DASHBOARD,
                        onClick = { navController.navigateTop(Routes.DASHBOARD) },
                        icon = { Icon(Icons.Default.Home, contentDescription = null) },
                        label = { Text(stringResource(R.string.nav_dashboard)) },
                    )
                    NavigationBarItem(
                        selected = current == Routes.HISTORY,
                        onClick = { navController.navigateTop(Routes.HISTORY) },
                        icon = { Icon(Icons.Default.History, contentDescription = null) },
                        label = { Text(stringResource(R.string.nav_history)) },
                    )
                    NavigationBarItem(
                        selected = current == Routes.SETTINGS,
                        onClick = { navController.navigateTop(Routes.SETTINGS) },
                        icon = { Icon(Icons.Default.Settings, contentDescription = null) },
                        label = { Text(stringResource(R.string.nav_settings)) },
                    )
                }
            }
        },
        floatingActionButton = {
            if (current == Routes.DASHBOARD || current == Routes.HISTORY) {
                FloatingActionButton(onClick = { navController.navigate(Routes.ADD_DOSE) }) {
                    Icon(Icons.Default.Add, contentDescription = stringResource(R.string.add_dose))
                }
            }
        },
    ) { padding ->
        NavHost(
            navController = navController,
            startDestination = start,
            modifier = Modifier.padding(padding),
        ) {
            composable(Routes.ONBOARDING) {
                val vm: OnboardingViewModel = viewModel(
                    factory = OnboardingViewModel.factory(container.authRepository),
                )
                OnboardingScreen(
                    viewModel = vm,
                    onFinished = {
                        navController.navigate(Routes.DASHBOARD) {
                            popUpTo(Routes.ONBOARDING) { inclusive = true }
                        }
                    },
                )
            }
            composable(Routes.DASHBOARD) { DashboardScreen() }
            composable(Routes.HISTORY) { HistoryScreen() }
            composable(Routes.SETTINGS) { SettingsScreen() }
            composable(Routes.ADD_DOSE) {
                AddDoseScreen(onClose = { navController.popBackStack() })
            }
        }
    }
}

private fun androidx.navigation.NavHostController.navigateTop(route: String) {
    navigate(route) {
        popUpTo(graph.findStartDestination().id) { saveState = true }
        launchSingleTop = true
        restoreState = true
    }
}
