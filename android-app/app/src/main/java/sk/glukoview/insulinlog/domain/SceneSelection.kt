package sk.glukoview.insulinlog.domain

/**
 * Určí id scény pre formulár dávky.
 * Default (prvá scéna) len kým používateľ/load ešte nevybral — inak by
 * `selected ?: first` vždy prepísalo „Bez scény“ (null) späť na Raňajky.
 */
object SceneSelection {
    fun resolve(
        selectedSceneId: Long?,
        sceneUserSet: Boolean,
        isEdit: Boolean,
        firstSceneId: Long?,
    ): Long? = when {
        sceneUserSet || isEdit -> selectedSceneId
        else -> selectedSceneId ?: firstSceneId
    }
}
