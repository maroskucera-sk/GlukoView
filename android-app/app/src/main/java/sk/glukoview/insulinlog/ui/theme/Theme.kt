package sk.glukoview.insulinlog.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import sk.glukoview.insulinlog.domain.ThemeMode

private val LightColors = lightColorScheme(
    primary = TealPrimary,
    onPrimary = TealOnPrimary,
    primaryContainer = TealContainer,
    onPrimaryContainer = TealOnContainer,
    secondary = BlueBasal,
    tertiary = CoralBolus,
    background = LightBackground,
    surface = LightSurface,
    onBackground = LightOnSurface,
    onSurface = LightOnSurface,
    surfaceVariant = Color(0xFFDCE8ED),
    onSurfaceVariant = Color(0xFF3E4F56),
)

private val DarkColors = darkColorScheme(
    primary = Color(0xFF7AD0E8),
    onPrimary = Color(0xFF003544),
    primaryContainer = Color(0xFF0A5570),
    onPrimaryContainer = Color(0xFFB8E5F2),
    secondary = Color(0xFF90CAF9),
    tertiary = Color(0xFFFFAB91),
    background = DarkBackground,
    surface = DarkSurface,
    onBackground = DarkOnSurface,
    onSurface = DarkOnSurface,
    surfaceVariant = Color(0xFF2A3A40),
    onSurfaceVariant = Color(0xFFB8C9D0),
)

@Composable
fun InsulinLogTheme(
    themeMode: ThemeMode = ThemeMode.SYSTEM,
    content: @Composable () -> Unit,
) {
    val dark = when (themeMode) {
        ThemeMode.SYSTEM -> isSystemInDarkTheme()
        ThemeMode.LIGHT -> false
        ThemeMode.DARK -> true
    }
    MaterialTheme(
        colorScheme = if (dark) DarkColors else LightColors,
        typography = InsulinTypography,
        content = content,
    )
}
