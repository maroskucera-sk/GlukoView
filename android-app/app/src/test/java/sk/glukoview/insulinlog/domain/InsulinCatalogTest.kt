package sk.glukoview.insulinlog.domain

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class InsulinCatalogTest {
    @Test
    fun presets_coverCommonBolusAndBasal() {
        val names = InsulinCatalog.presets.map { it.name.lowercase() }.toSet()
        assertTrue(names.contains("novorapid"))
        assertTrue(names.contains("fiasp"))
        assertTrue(names.contains("lyumjev"))
        assertTrue(names.contains("tresiba"))
        assertTrue(names.contains("lantus"))
    }

    @Test
    fun findByName_matchesCaseInsensitive() {
        val p = InsulinCatalog.findByName("fiasp")
        assertNotNull(p)
        assertEquals(InsulinCategory.BOLUS, p!!.category)
        assertEquals(4.5, p.durationHours, 0.01)
        assertEquals(55, p.peakMinutes)
    }

    @Test
    fun matchDuration_fallsBackByCategory() {
        assertEquals(5.0, InsulinCatalog.matchDurationForName("Neznámy", InsulinCategory.BOLUS), 0.01)
        assertEquals(24.0, InsulinCatalog.matchDurationForName("Neznámy", InsulinCategory.BASAL), 0.01)
    }

    @Test
    fun tresiba_isUltraLong() {
        val t = InsulinCatalog.findByName("Tresiba")!!
        assertEquals(InsulinCategory.BASAL, t.category)
        assertEquals(42.0, t.durationHours, 0.01)
    }

    @Test
    fun allDurations_withinApiRange() {
        InsulinCatalog.presets.forEach { p ->
            assertTrue("${p.name} duration", p.durationHours > 0 && p.durationHours <= 48)
        }
    }
}
