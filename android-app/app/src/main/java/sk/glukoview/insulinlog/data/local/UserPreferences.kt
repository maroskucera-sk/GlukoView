package sk.glukoview.insulinlog.data.local

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.longPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import sk.glukoview.insulinlog.domain.ThemeMode

private val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "user_prefs")

data class SessionPrefs(
    val serverUrl: String? = null,
    val accessKey: String? = null,
    val profileId: Long? = null,
    val themeMode: ThemeMode = ThemeMode.SYSTEM,
) {
    val isLoggedIn: Boolean
        get() = !serverUrl.isNullOrBlank() && !accessKey.isNullOrBlank() && profileId != null
}

class UserPreferences(private val context: Context) {
    private object Keys {
        val SERVER_URL = stringPreferencesKey("server_url")
        val ACCESS_KEY = stringPreferencesKey("access_key")
        val PROFILE_ID = longPreferencesKey("profile_id")
        val THEME = stringPreferencesKey("theme_mode")
    }

    val session: Flow<SessionPrefs> = context.dataStore.data.map { prefs ->
        SessionPrefs(
            serverUrl = prefs[Keys.SERVER_URL],
            accessKey = prefs[Keys.ACCESS_KEY],
            profileId = prefs[Keys.PROFILE_ID],
            themeMode = when (prefs[Keys.THEME]) {
                "LIGHT" -> ThemeMode.LIGHT
                "DARK" -> ThemeMode.DARK
                else -> ThemeMode.SYSTEM
            },
        )
    }

    suspend fun saveServer(serverUrl: String, accessKey: String) {
        context.dataStore.edit { prefs ->
            prefs[Keys.SERVER_URL] = serverUrl.trim().trimEnd('/')
            prefs[Keys.ACCESS_KEY] = accessKey.trim()
        }
    }

    suspend fun saveProfile(profileId: Long) {
        context.dataStore.edit { prefs ->
            prefs[Keys.PROFILE_ID] = profileId
        }
    }

    suspend fun setTheme(mode: ThemeMode) {
        context.dataStore.edit { prefs ->
            prefs[Keys.THEME] = mode.name
        }
    }

    suspend fun clearSession() {
        context.dataStore.edit { prefs ->
            prefs.remove(Keys.SERVER_URL)
            prefs.remove(Keys.ACCESS_KEY)
            prefs.remove(Keys.PROFILE_ID)
        }
    }
}
