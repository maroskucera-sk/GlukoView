package sk.glukoview.insulinlog.data.local

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

@Database(
    entities = [
        ProfileEntity::class,
        InsulinEntity::class,
        SceneEntity::class,
        InjectionSiteEntity::class,
        DoseEntity::class,
        SettingsEntity::class,
        SyncMetaEntity::class,
    ],
    version = 2,
    exportSchema = false,
)
abstract class AppDatabase : RoomDatabase() {
    abstract fun profiles(): ProfileDao
    abstract fun insulins(): InsulinDao
    abstract fun scenes(): SceneDao
    abstract fun injectionSites(): InjectionSiteDao
    abstract fun doses(): DoseDao
    abstract fun settings(): SettingsDao
    abstract fun syncMeta(): SyncMetaDao

    companion object {
        fun create(context: Context): AppDatabase =
            Room.databaseBuilder(
                context.applicationContext,
                AppDatabase::class.java,
                "glukoview_insulin.db",
            ).fallbackToDestructiveMigration().build()
    }
}
