package sk.glukoview.insulinlog.data.local

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.Query
import kotlinx.coroutines.flow.Flow

@Dao
interface ProfileDao {
    @Query("SELECT * FROM profiles WHERE isActive = 1 ORDER BY sortOrder, id")
    fun observeActive(): Flow<List<ProfileEntity>>

    @Query("SELECT * FROM profiles ORDER BY sortOrder, id")
    suspend fun getAll(): List<ProfileEntity>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsertAll(items: List<ProfileEntity>)
}

@Dao
interface InsulinDao {
    @Query("SELECT * FROM insulins WHERE isArchived = 0 ORDER BY sortOrder, id")
    fun observeActive(): Flow<List<InsulinEntity>>

    @Query("SELECT * FROM insulins ORDER BY sortOrder, id")
    suspend fun getAll(): List<InsulinEntity>

    @Query("SELECT * FROM insulins WHERE id = :id LIMIT 1")
    suspend fun getById(id: Long): InsulinEntity?

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsertAll(items: List<InsulinEntity>)
}

@Dao
interface SceneDao {
    @Query("SELECT * FROM scenes WHERE isArchived = 0 ORDER BY sortOrder, id")
    fun observeActive(): Flow<List<SceneEntity>>

    @Query("SELECT * FROM scenes ORDER BY sortOrder, id")
    suspend fun getAll(): List<SceneEntity>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsertAll(items: List<SceneEntity>)
}

@Dao
interface DoseDao {
    @Query(
        """
        SELECT * FROM doses
        WHERE deletedAtEpochMs IS NULL
          AND administeredAtEpochMs >= :fromMs
          AND administeredAtEpochMs < :toMs
        ORDER BY administeredAtEpochMs DESC
        """,
    )
    fun observeBetween(fromMs: Long, toMs: Long): Flow<List<DoseEntity>>

    @Query(
        """
        SELECT * FROM doses
        WHERE deletedAtEpochMs IS NULL
        ORDER BY administeredAtEpochMs DESC
        LIMIT :limit
        """,
    )
    fun observeRecent(limit: Int = 200): Flow<List<DoseEntity>>

    @Query("SELECT * FROM doses WHERE uuid = :uuid LIMIT 1")
    suspend fun getByUuid(uuid: String): DoseEntity?

    @Query("SELECT * FROM doses WHERE syncPending = 1")
    suspend fun getPending(): List<DoseEntity>

    @Query(
        """
        SELECT * FROM doses
        WHERE deletedAtEpochMs IS NULL
        ORDER BY administeredAtEpochMs DESC
        LIMIT 1
        """,
    )
    fun observeLatest(): Flow<DoseEntity?>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsert(item: DoseEntity): Long

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsertAll(items: List<DoseEntity>)

    @Query("UPDATE doses SET syncPending = 0, serverId = :serverId, updatedAtEpochMs = :updatedAt WHERE uuid = :uuid")
    suspend fun markSynced(uuid: String, serverId: Long?, updatedAt: Long)
}

@Dao
interface SettingsDao {
    @Query("SELECT * FROM family_settings WHERE id = 1 LIMIT 1")
    fun observe(): Flow<SettingsEntity?>

    @Query("SELECT * FROM family_settings WHERE id = 1 LIMIT 1")
    suspend fun get(): SettingsEntity?

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsert(item: SettingsEntity)
}

@Dao
interface SyncMetaDao {
    @Query("SELECT * FROM sync_meta WHERE id = 1 LIMIT 1")
    suspend fun get(): SyncMetaEntity?

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun upsert(item: SyncMetaEntity)
}
