package sk.glukoview.insulinlog.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import sk.glukoview.insulinlog.domain.DoseRecord
import sk.glukoview.insulinlog.domain.InsulinCategory
import sk.glukoview.insulinlog.domain.InsulinType
import sk.glukoview.insulinlog.domain.Profile
import sk.glukoview.insulinlog.domain.Scene
import sk.glukoview.insulinlog.ui.theme.BlueBasal
import sk.glukoview.insulinlog.ui.theme.CoralBolus
import sk.glukoview.insulinlog.ui.util.Formatters

@Composable
fun DoseRow(
    dose: DoseRecord,
    insulin: InsulinType?,
    scene: Scene?,
    profile: Profile?,
    onClick: () -> Unit,
) {
    val accent = when (insulin?.category) {
        InsulinCategory.BASAL -> BlueBasal
        else -> runCatching { Color(android.graphics.Color.parseColor(insulin?.colorHex ?: "#D84315")) }
            .getOrDefault(CoralBolus)
    }

    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick),
        shape = RoundedCornerShape(16.dp),
        color = MaterialTheme.colorScheme.surface,
        tonalElevation = 1.dp,
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 14.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Box(
                modifier = Modifier
                    .size(10.dp)
                    .background(accent, CircleShape),
            )
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = "${Formatters.units(dose.units)} · ${insulin?.name ?: "Inzulín"}",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.SemiBold,
                )
                val meta = listOfNotNull(
                    scene?.name,
                    Formatters.localTime(dose.administeredAt),
                    profile?.let { "podal: ${it.name}" },
                    dose.glucoseValue?.let { "G: $it" },
                    dose.carbsSj?.let { "SJ: $it" },
                ).joinToString(" · ")
                Text(
                    text = meta,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
                if (dose.syncPending) {
                    Text(
                        text = "Čaká na synchronizáciu",
                        style = MaterialTheme.typography.labelLarge,
                        color = MaterialTheme.colorScheme.tertiary,
                    )
                }
            }
        }
    }
}
