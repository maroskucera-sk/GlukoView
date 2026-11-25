package sk.glukoview.insulinlog

import android.app.Application
import sk.glukoview.insulinlog.di.AppContainer

class InsulinLogApp : Application() {
    lateinit var container: AppContainer
        private set

    override fun onCreate() {
        super.onCreate()
        container = AppContainer(this)
    }
}
