package sk.glukoview.insulinlog

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.runtime.getValue
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import sk.glukoview.insulinlog.ui.navigation.AppNavHost
import sk.glukoview.insulinlog.ui.theme.InsulinLogTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        val app = application as InsulinLogApp
        setContent {
            val session by app.container.preferences.session.collectAsStateWithLifecycle(
                initialValue = sk.glukoview.insulinlog.data.local.SessionPrefs(),
            )
            InsulinLogTheme(themeMode = session.themeMode) {
                AppNavHost(container = app.container)
            }
        }
    }
}
