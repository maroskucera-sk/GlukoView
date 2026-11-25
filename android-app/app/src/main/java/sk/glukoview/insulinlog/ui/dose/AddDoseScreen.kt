package sk.glukoview.insulinlog.ui.dose

import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource
import sk.glukoview.insulinlog.R
import sk.glukoview.insulinlog.ui.components.PlaceholderScreen

@Composable
@Suppress("UNUSED_PARAMETER")
fun AddDoseScreen(onClose: () -> Unit) {
    // onClose použije krok 4 (uloženie / zrušenie)
    PlaceholderScreen(title = stringResource(R.string.nav_add_dose))
}
