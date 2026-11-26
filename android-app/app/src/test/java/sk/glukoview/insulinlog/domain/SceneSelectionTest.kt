package sk.glukoview.insulinlog.domain

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SceneSelectionTest {
    @Test
    fun newForm_defaultsToFirstScene() {
        assertEquals(
            1L,
            SceneSelection.resolve(
                selectedSceneId = null,
                sceneUserSet = false,
                isEdit = false,
                firstSceneId = 1L,
            ),
        )
    }

    @Test
    fun bezSceny_staysNull() {
        assertNull(
            SceneSelection.resolve(
                selectedSceneId = null,
                sceneUserSet = true,
                isEdit = false,
                firstSceneId = 1L,
            ),
        )
    }

    @Test
    fun editWithNullScene_staysNull() {
        assertNull(
            SceneSelection.resolve(
                selectedSceneId = null,
                sceneUserSet = false,
                isEdit = true,
                firstSceneId = 1L,
            ),
        )
    }

    @Test
    fun userPickedScene_kept() {
        assertEquals(
            3L,
            SceneSelection.resolve(
                selectedSceneId = 3L,
                sceneUserSet = true,
                isEdit = false,
                firstSceneId = 1L,
            ),
        )
    }
}
