package sk.glukoview.insulinlog.ui.navigation

object Routes {
    const val ONBOARDING = "onboarding"
    const val PROFILE_PICK = "profile_pick"
    const val DASHBOARD = "dashboard"
    const val CHART = "chart"
    const val HISTORY = "history"
    const val SETTINGS = "settings"
    const val ADD_DOSE = "add_dose"
    const val EDIT_DOSE = "edit_dose/{uuid}"

    fun editDose(uuid: String) = "edit_dose/$uuid"
}
