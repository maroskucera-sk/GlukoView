package sk.glukoview.insulinlog.di

import android.content.Context
import sk.glukoview.insulinlog.data.local.AppDatabase
import sk.glukoview.insulinlog.data.local.UserPreferences
import sk.glukoview.insulinlog.data.nightscout.NightscoutClient
import sk.glukoview.insulinlog.data.repository.AuthRepository
import sk.glukoview.insulinlog.data.repository.DoseRepository
import sk.glukoview.insulinlog.data.repository.SyncRepository

/** Jednoduchý manuálny DI kontajner (bez Hiltu) — stačí pre MVVM základ. */
class AppContainer(context: Context) {
    private val appContext = context.applicationContext

    val database: AppDatabase = AppDatabase.create(appContext)
    val preferences: UserPreferences = UserPreferences(appContext)
    val nightscout: NightscoutClient = NightscoutClient()

    val authRepository = AuthRepository(database, preferences)
    val syncRepository = SyncRepository(database, preferences)
    val doseRepository = DoseRepository(database)
}
