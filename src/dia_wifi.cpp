#include "dia_wifi.h"
#include "dia_portal.h"
#include "dia_pin.h"
#include "dia_board.h"
#include "dia_carelink.h"
#include "dia_i18n.h"
#include "dia_config.h"
#include "dia_fetch.h"
#include "dia_glucose.h"
#include "dia_secret.h"
#include "dia_types.h"
#include "dia_ui.h"
#include "dia_version.h"

#include <WiFi.h>
#include <time.h>
#include <vector>

WiFiManager wm;
WiFiMulti wifiMulti;

WiFiManagerParameter *custom_source = nullptr;
WiFiManagerParameter *custom_ns_url = nullptr;
WiFiManagerParameter *custom_ns_token = nullptr;
WiFiManagerParameter *custom_llu_user = nullptr;
WiFiManagerParameter *custom_llu_pwd = nullptr;
WiFiManagerParameter *custom_llu_region = nullptr;
WiFiManagerParameter *custom_libre_patient = nullptr;
WiFiManagerParameter *custom_dx_user = nullptr;
WiFiManagerParameter *custom_dx_pwd = nullptr;
WiFiManagerParameter *custom_dx_region = nullptr;
WiFiManagerParameter *custom_units = nullptr;
WiFiManagerParameter *custom_tls = nullptr;
WiFiManagerParameter *custom_t_low = nullptr;
WiFiManagerParameter *custom_t_high = nullptr;
WiFiManagerParameter *custom_t_crit = nullptr;
WiFiManagerParameter *custom_cl_patient = nullptr;
WiFiManagerParameter *custom_p1_cl_pref = nullptr;
WiFiManagerParameter *custom_lang = nullptr;
WiFiManagerParameter *custom_disp_mode = nullptr;
WiFiManagerParameter *custom_p0_name = nullptr;
WiFiManagerParameter *custom_p1_name = nullptr;
WiFiManagerParameter *custom_p1_source = nullptr;
WiFiManagerParameter *custom_p1_ns_url = nullptr;
WiFiManagerParameter *custom_p1_ns_token = nullptr;
WiFiManagerParameter *custom_p1_llu_user = nullptr;
WiFiManagerParameter *custom_p1_llu_pwd = nullptr;
WiFiManagerParameter *custom_p1_llu_reg = nullptr;
WiFiManagerParameter *custom_p1_libre_pref = nullptr;
WiFiManagerParameter *custom_p1_dx_user = nullptr;
WiFiManagerParameter *custom_p1_dx_pwd = nullptr;
WiFiManagerParameter *custom_p1_dx_reg = nullptr;
WiFiManagerParameter *custom_p1_t_low = nullptr;
WiFiManagerParameter *custom_p1_t_high = nullptr;
WiFiManagerParameter *custom_p1_t_crit = nullptr;

static String portalSavedNetsHtml;

static bool isPortalApSsid(const String &ssid) {
    return ssid == "GlukoView" || ssid == "DIA_Monitor";
}

static const int kMaxWifiNets = 5;
static const uint32_t kPerNetTimeoutMs = 12000;
static const int kConnectRounds = 2;
static const uint32_t kPortalTimeoutSec = 180; // po timeout znova skúsi STA

struct WifiCred {
    String ssid;
    String pwd;
};

static bool s_portalRestart = true;

void saveConfigCallback() {
    shouldSaveConfig = true;
    s_portalRestart = true;
    if (wm.server && String(wm.server->uri()) == "/paramsave") s_portalRestart = false;
}

/** Slot má použiteľný zdroj (nie len meno v selecte bez prihlásenia). */
static bool diaSourceReady(int slot) {
    if (slot < 0 || slot > 1) return false;
    String s = profiles[slot].dsSource;
    s.trim();
    s.toLowerCase();
    if (s.length() == 0 || s == "off") return false;
    if (s == "nightscout") return profiles[slot].nsUrl.length() > 0;
    if (s == "libre") return profiles[slot].libreUser.length() > 0 && profiles[slot].librePwd.length() > 0;
    if (s == "dexcom") return profiles[slot].dexUser.length() > 0 && profiles[slot].dexPwd.length() > 0;
    if (s == "guardian") return carelinkTokensReadyForSlot(slot);
    return false;
}

/**
 * Na displeji držať IP, kým nie je čo sťahovať.
 * (prázdny/off zdroj, nedokončený wizard, chýbajúce heslo/token → inak by svietilo len „Config“)
 */
static bool diaNeedsSourceSetup() {
    if (preferences.isKey("wiz") && preferences.getUChar("wiz", 255) != 255) return true;
    if (!diaSourceReady(0)) return true;
    if (!diaBoardIsRound() && display_mode >= 2) {
        String s1 = profiles[1].dsSource;
        s1.trim();
        s1.toLowerCase();
        // Dual: B ešte nevybrané, alebo vybrané ale nepripravené (off = zámerne vypnuté)
        if (s1.length() == 0) return true;
        if (s1 != "off" && !diaSourceReady(1)) return true;
    }
    return false;
}

bool diaWifiNeedsSetupHint() { return diaNeedsSourceSetup(); }

static void showConnectedPortalHint() {
    // IP ako titulok (najväčší text) – bez hotového zdroja musí byť stále viditeľná
    String ip = WiFi.localIP().toString();
    if (ip == "0.0.0.0" || ip.length() == 0) ip = "—.—.—.—";
    showBootStatus(ip.c_str(), S(Str::BootPortalHint));
}

/** Captive portal / úvod: v AP rovno WiFi výber, v STA rovno nastavenie zdroja. */
static void handlePortalHome() {
    if (!wm.server) return;
    const wifi_mode_t mode = WiFi.getMode();
    if (mode == WIFI_AP || mode == WIFI_AP_STA) {
        wm.server->sendHeader(F("Location"), F("/wifi"), true);
        wm.server->send(302, F("text/plain"), F(""));
        return;
    }
    wm.server->sendHeader(F("Location"), F("/param"), true);
    wm.server->send(302, F("text/plain"), F(""));
}

// ==========================================
// Jednotný zoznam sietí (aktuálna + zálohy, bez duplikátov)
// NVS: wifi_ssid/wifi_pwd = MRU (posledná úspešná)
//      ssid1..4 / pwd1..4 = ostatné
// ==========================================

static void syncBackupGlobalsFromCreds(const WifiCred *nets, int count) {
    backup_ssid1 = (count > 1) ? nets[1].ssid : "";
    backup_pwd1  = (count > 1) ? nets[1].pwd  : "";
    backup_ssid2 = (count > 2) ? nets[2].ssid : "";
    backup_pwd2  = (count > 2) ? nets[2].pwd  : "";
    backup_ssid3 = (count > 3) ? nets[3].ssid : "";
    backup_pwd3  = (count > 3) ? nets[3].pwd  : "";
    backup_ssid4 = (count > 4) ? nets[4].ssid : "";
    backup_pwd4  = (count > 4) ? nets[4].pwd  : "";
}

static void persistCreds(const WifiCred *nets, int count) {
    if (count > 0) {
        preferences.putString("wifi_ssid", nets[0].ssid);
        diaNvsPutSecret("wifi_pwd", nets[0].pwd);
    } else {
        preferences.remove("wifi_ssid");
        preferences.remove("wifi_pwd");
    }

    const char *sk[] = {"ssid1", "ssid2", "ssid3", "ssid4"};
    const char *pk[] = {"pwd1", "pwd2", "pwd3", "pwd4"};
    for (int i = 0; i < 4; i++) {
        int src = i + 1;
        if (src < count) {
            preferences.putString(sk[i], nets[src].ssid);
            diaNvsPutSecret(pk[i], nets[src].pwd);
        } else {
            preferences.remove(sk[i]);
            preferences.remove(pk[i]);
        }
    }
    syncBackupGlobalsFromCreds(nets, count);
}

/** Zoberie všetky známe siete, dedupe podľa SSID (neprázdne heslo má prednosť). Index 0 = MRU. */
static int gatherWifiCreds(WifiCred *out, int maxOut) {
    WifiCred raw[8];
    int rawN = 0;

    auto pushRaw = [&](String ssid, String pwd) {
        ssid.trim();
        pwd.trim();
        if (ssid.length() == 0 || isPortalApSsid(ssid)) return;
        for (int i = 0; i < rawN; i++) {
            if (raw[i].ssid == ssid) {
                if (pwd.length() > 0 && (raw[i].pwd.length() == 0 || raw[i].pwd != pwd)) {
                    raw[i].pwd = pwd;
                }
                return;
            }
        }
        if (rawN < 8) {
            raw[rawN].ssid = ssid;
            raw[rawN].pwd = pwd;
            rawN++;
        }
    };

    // MRU najprv (poradie pri dedupe zachováme cez druhý prechod)
    String mruSsid = preferences.isKey("wifi_ssid") ? preferences.getString("wifi_ssid", "") : "";
    String mruPwd = diaNvsGetSecret("wifi_pwd");
    pushRaw(mruSsid, mruPwd);
    pushRaw(backup_ssid1, backup_pwd1);
    pushRaw(backup_ssid2, backup_pwd2);
    pushRaw(backup_ssid3, backup_pwd3);
    pushRaw(backup_ssid4, backup_pwd4);
    pushRaw(wm.getWiFiSSID(true), wm.getWiFiPass(true));

    // Doplň heslá z iných zdrojov ak chýbajú
    for (int i = 0; i < rawN; i++) {
        if (raw[i].pwd.length() > 0) continue;
        if (raw[i].ssid == backup_ssid1 && backup_pwd1.length() > 0) raw[i].pwd = backup_pwd1;
        else if (raw[i].ssid == backup_ssid2 && backup_pwd2.length() > 0) raw[i].pwd = backup_pwd2;
        else if (raw[i].ssid == backup_ssid3 && backup_pwd3.length() > 0) raw[i].pwd = backup_pwd3;
        else if (raw[i].ssid == backup_ssid4 && backup_pwd4.length() > 0) raw[i].pwd = backup_pwd4;
        else if (raw[i].ssid == mruSsid && mruPwd.length() > 0) raw[i].pwd = mruPwd;
        else if (raw[i].ssid == wm.getWiFiSSID(true)) {
            String wp = wm.getWiFiPass(true);
            if (wp.length() > 0) raw[i].pwd = wp;
        }
    }

    // Zoraď: MRU prvá, potom ostatné v poradí nájdenia
    int n = 0;
    auto take = [&](const String &ssid) {
        if (ssid.length() == 0 || n >= maxOut) return;
        for (int i = 0; i < rawN; i++) {
            if (raw[i].ssid != ssid) continue;
            // už pridané?
            for (int j = 0; j < n; j++) if (out[j].ssid == ssid) return;
            out[n++] = raw[i];
            return;
        }
    };

    take(mruSsid);
    for (int i = 0; i < rawN && n < maxOut; i++) take(raw[i].ssid);
    return n;
}

static void rebuildWifiMulti(const WifiCred *nets, int count) {
    wifiMulti = WiFiMulti();
    for (int i = 0; i < count; i++) {
        if (nets[i].ssid.length() == 0) continue;
        wifiMulti.addAP(nets[i].ssid.c_str(), nets[i].pwd.c_str());
        Serial.printf("WiFiMulti + '%s' (pwd %d znakov)\n",
                      nets[i].ssid.c_str(), nets[i].pwd.length());
    }
}

static String lookupStoredPassword(const String &ssid) {
    WifiCred nets[kMaxWifiNets];
    int n = gatherWifiCreds(nets, kMaxWifiNets);
    for (int i = 0; i < n; i++) {
        if (nets[i].ssid == ssid && nets[i].pwd.length() > 0) return nets[i].pwd;
    }
    return "";
}

/** Ulož / aktualizuj sieť; MRU na začiatok; pri plnom zozname vyhodí najstaršiu (FIFO). */
void saveNetworkToBackup(String ssid, String pwd) {
    ssid.trim();
    pwd.trim();
    if (ssid.length() == 0 || isPortalApSsid(ssid)) return;

    if (pwd.length() == 0) {
        pwd = lookupStoredPassword(ssid);
    }

    WifiCred nets[kMaxWifiNets];
    int n = gatherWifiCreds(nets, kMaxWifiNets);

    // Odstráň existujúci záznam tej istej SSID
    int dst = 0;
    for (int i = 0; i < n; i++) {
        if (nets[i].ssid == ssid) continue;
        nets[dst++] = nets[i];
    }
    n = dst;

    // Ak je plno, zahod najstaršiu (poslednú)
    if (n >= kMaxWifiNets) n = kMaxWifiNets - 1;

    // Posuň doprava a vlož MRU na index 0
    for (int i = n; i > 0; i--) nets[i] = nets[i - 1];
    nets[0].ssid = ssid;
    nets[0].pwd = pwd;
    n++;

    persistCreds(nets, n);
    rebuildWifiMulti(nets, n);
    Serial.printf("WiFi ulozena MRU='%s' (celkom %d)\n", ssid.c_str(), n);
}

void autoSaveNetwork() {
    String ssid = WiFi.SSID();
    String pwd = WiFi.psk();

    // WiFi.psk() na ESP32 často vráti "" – doplň z WM / NVS
    if (ssid.length() == 0) ssid = wm.getWiFiSSID(true);
    if (pwd.length() == 0) pwd = wm.getWiFiPass(true);
    if (pwd.length() == 0 && ssid.length() > 0) pwd = lookupStoredPassword(ssid);

    if (ssid.length() > 0 && !isPortalApSsid(ssid)) {
        saveNetworkToBackup(ssid, pwd);
    }
}

static String buildSavedNetsListHtml() {
    WifiCred nets[kMaxWifiNets];
    int n = gatherWifiCreds(nets, kMaxWifiNets);

    String html;
    html.reserve(320);
    if (n == 0) {
        html = "<em>";
        html += S(Str::PortalNoneYet);
        html += "</em><br/>";
        return html;
    }
    for (int i = 0; i < n; i++) {
        html += "• ";
        html += nets[i].ssid;
        if (i == 0) {
            html += " <small>";
            html += S(Str::PortalLast);
            html += "</small>";
        }
        if (nets[i].pwd.length() == 0) {
            html += " <small style=\"color:#f66\">";
            html += S(Str::PortalNoPwd);
            html += "</small>";
        }
        html += "<br/>";
    }
    return html;
}

void refreshPortalSavedNetsHtml() {
    portalSavedNetsHtml = "<div class='msg'><strong>";
    portalSavedNetsHtml += S(Str::PortalSavedTitle);
    portalSavedNetsHtml += "</strong><br/>";
    portalSavedNetsHtml += buildSavedNetsListHtml();
    portalSavedNetsHtml += "<br/><form action='/nets' method='get'><button type='submit'>";
    portalSavedNetsHtml += S(Str::PortalSavedWifi);
    portalSavedNetsHtml += "</button></form></div><br/>";
    wm.setCustomMenuHTML(portalSavedNetsHtml.c_str());
}

static String diaHtmlAttr(const String &in) {
    String out;
    out.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == '&') out += F("&amp;");
        else if (c == '"') out += F("&quot;");
        else if (c == '\'') out += F("&#39;");
        else if (c == '<') out += F("&lt;");
        else out += c;
    }
    return out;
}

static String diaSelOpt(const char *val, const char *label, const String &cur) {
    String o = "<option value=\"";
    o += val;
    o += "\"";
    if (cur.equalsIgnoreCase(val)) o += " selected";
    o += ">";
    o += label;
    o += "</option>";
    return o;
}

static void diaField(String &page, const char *label, const char *name, const String &value, bool isPassword = false) {
    page += F("<label>");
    page += label;
    page += F("</label><input name=\"");
    page += name;
    page += F("\" type=\"");
    page += isPassword ? F("password") : F("text");
    page += F("\" autocomplete=off");
    if (isPassword) {
        if (value.length() > 0) page += F(" placeholder=\"••••\"");
    } else {
        page += F(" value=\"");
        page += diaHtmlAttr(value);
        page += F("\"");
    }
    page += F(">");
}

static void diaApplySecret(String &dst, const char *incoming) {
    if (incoming && incoming[0]) dst = incoming;
}

static void diaPortalPatientSelect(String &page, const char *label, const char *fieldName,
                                   const String &cur, const String &list) {
    if (list.length() == 0) {
        diaField(page, label, fieldName, cur);
        return;
    }
    page += F("<label>");
    page += label;
    page += F("</label><select name=\"");
    page += fieldName;
    page += F("\"><option value=\"\">—</option>");
    int from = 0;
    while (from < (int)list.length()) {
        int nl = list.indexOf('\n', from);
        if (nl < 0) nl = list.length();
        String line = list.substring(from, nl);
        int tab = line.indexOf('\t');
        String id = tab >= 0 ? line.substring(0, tab) : line;
        String name = tab >= 0 ? line.substring(tab + 1) : line;
        page += F("<option value=\"");
        page += diaHtmlAttr(id);
        page += F("\"");
        if (cur.length() && (cur == id || cur == name)) page += F(" selected");
        page += F(">");
        page += diaHtmlAttr(name);
        page += F("</option>");
        from = nl + 1;
    }
    page += F("</select>");
}

static void diaPortalLibrePatientSelect(String &page, const char *fieldName, const String &vLibre,
                                        const String &list) {
    diaPortalPatientSelect(page, S(Str::PortalPatient), fieldName, vLibre, list);
}

/** CareLink pacient: viditeľný select len pri Guardian; inak hidden (WM by inak vymazal buffer). */
static void diaPortalCarelinkPatientFields(String &page, bool userB, bool isGuardian,
                                           const String &cur, const String &list) {
    const char *field = userB ? "p1_cpf" : "cl_pref";
    if (!isGuardian) {
        page += F("<input type=hidden name=\"");
        page += field;
        page += F("\" value=\"");
        page += diaHtmlAttr(cur);
        page += F("\">");
        return;
    }
    diaPortalPatientSelect(page, S(Str::PortalGuardianPatient), field, cur, list);
    page += F("<p class=h>");
    page += S(Str::PortalGuardianPatientHint);
    page += F("</p>");
}

static void diaPortalSourceBlock(String &page, bool userB, const String &src,
                                 const String &vUrl, const String &vTok,
                                 const String &vLluUser, const String &vLluPwd, const String &vLibre,
                                 const String &lluReg, const String &vDxUser, const String &vDxPwd,
                                 const String &dxReg, const String &libreList,
                                 const String &clPref, const String &clList, bool clTokOk) {
    const char *srcName = userB ? "p1_src" : "ds_source";
    const char *pfx = userB ? "p1" : "p0";
    const char *nsUrlN = userB ? "p1_nsu" : "ns_url";
    const char *nsTokN = userB ? "p1_nst" : "ns_token";
    const char *lluUserN = userB ? "p1_llu" : "llu_user";
    const char *lluPwdN = userB ? "p1_llp" : "llu_pwd";
    const char *libreN = userB ? "p1_lpf" : "libre_pref";
    const char *lluRegN = userB ? "p1_llr" : "llu_reg";
    const char *dxUserN = userB ? "p1_dxu" : "dx_user";
    const char *dxPwdN = userB ? "p1_dxp" : "dx_pwd";
    const char *dxRegN = userB ? "p1_dxr" : "dx_reg";
    const bool isGuardian = src.equalsIgnoreCase("guardian");

    page += F("<label>");
    page += S(Str::PortalPickSource);
    page += F("</label><select name=\"");
    page += srcName;
    page += F("\" onchange=\"diaSrc('");
    page += pfx;
    page += F("',this.value)\">");
    if (userB) page += diaSelOpt("off", S(Str::PortalSrcOff), src);
    page += diaSelOpt("nightscout", "Nightscout", src);
    page += diaSelOpt("libre", "Libre LinkUp", src);
    page += diaSelOpt("dexcom", "Dexcom Share", src);
    page += diaSelOpt("guardian", "Guardian (CareLink)", src);
    page += F("</select>");

    // Len CSS hide – polia ostávajú v POST (WM pri chýbajúcom arg vymaže buffer)
    page += F("<div id=\"");
    page += pfx;
    page += F("_nightscout\" class=srcblk>");
    page += F("<h4 style=\"color:#8cf;margin:10px 0 4px\">");
    page += S(Str::PortalNs);
    page += F("</h4><p class=h>");
    page += S(Str::PortalNsHint);
    page += F("</p>");
    diaField(page, S(Str::PortalNsUrl), nsUrlN, vUrl);
    diaField(page, S(Str::PortalNsToken), nsTokN, vTok, true);
    page += F("</div>");

    page += F("<div id=\"");
    page += pfx;
    page += F("_libre\" class=srcblk>");
    page += F("<h4 style=\"color:#8cf;margin:10px 0 4px\">");
    page += S(Str::PortalAccount);
    page += F("</h4><p class=h>");
    page += S(Str::PortalAccountHint);
    page += F("</p>");
    diaField(page, S(Str::PortalEmail), lluUserN, vLluUser);
    diaField(page, S(Str::PortalPassword), lluPwdN, vLluPwd, true);
    diaPortalLibrePatientSelect(page, libreN, vLibre, libreList);
    page += F("<p class=h>");
    page += S(Str::PortalPatientHint);
    page += F("</p><label>");
    page += S(Str::PortalRegion);
    page += F("</label><select name=\"");
    page += lluRegN;
    page += F("\">");
    page += diaSelOpt("eu", "EU", lluReg);
    page += diaSelOpt("eu2", "EU2", lluReg);
    page += diaSelOpt("us", "US", lluReg);
    page += diaSelOpt("jp", "JP", lluReg);
    page += F("</select></div>");

    page += F("<div id=\"");
    page += pfx;
    page += F("_dexcom\" class=srcblk>");
    page += F("<h4 style=\"color:#8cf;margin:10px 0 4px\">");
    page += S(Str::PortalDexcom);
    page += F("</h4><p class=h>");
    page += S(Str::PortalDexcomHint);
    page += F("</p>");
    diaField(page, S(Str::PortalEmail), dxUserN, vDxUser);
    diaField(page, S(Str::PortalPassword), dxPwdN, vDxPwd, true);
    page += F("<label>");
    page += S(Str::PortalRegion);
    page += F("</label><select name=\"");
    page += dxRegN;
    page += F("\">");
    page += diaSelOpt("ous", "EU / OUS", dxReg);
    page += diaSelOpt("us", "US", dxReg);
    page += diaSelOpt("jp", "JP", dxReg);
    page += F("</select></div>");

    page += F("<div id=\"");
    page += pfx;
    page += F("_guardian\" class=srcblk><p class=h>");
    page += S(Str::PortalGuardianHint);
    page += F(" <a href=/carelink");
    if (userB) page += F("?slot=1");
    page += F(" style=color:#8cf>/carelink</a></p>");
    if (clTokOk) {
        page += F("<p style=color:#6d6>");
        page += S(Str::PortalTokensOk);
        page += F("</p>");
    } else {
        page += F("<p style=color:#fc6>");
        page += S(Str::PortalTokensMissing);
        page += F("</p>");
    }
    // Viditeľný patient select len v guardian bloku; hidden mimo (nižšie mimo div)
    if (isGuardian) {
        diaPortalCarelinkPatientFields(page, userB, true, clPref, clList);
        if (!userB || carelinkSlotOwnsTokens(1)) {
            page += F("<p><a href=/carelink");
            if (userB) page += F("?slot=1");
            page += F(">");
            page += S(Str::ClPickPatientBtn);
            page += F("</a></p>");
        }
        if (userB) {
            page += F("<p class=h>");
            page += S(Str::PortalDualClHint);
            page += F("</p>");
        }
    }
    page += F("</div>");
    if (!isGuardian) diaPortalCarelinkPatientFields(page, userB, false, clPref, clList);
    page += F("<script>diaSrc('");
    page += pfx;
    page += F("','");
    page += src;
    page += F("');</script>");
}

static void handleDiaParamPage() {
    if (!wm.server) return;
    if (!wm.server->hasArg("all")) {
        const bool mid = preferences.isKey("wiz") && preferences.getUChar("wiz", 255) != 255;
        const bool fresh = !preferences.isKey("wiz") && profiles[0].dsSource.length() == 0;
        if (mid || fresh) {
            wm.server->sendHeader("Location", "/setup", true);
            wm.server->send(302, "text/plain", "");
            return;
        }
    }

    String src0 = custom_source ? String(custom_source->getValue()) : profiles[0].dsSource;
    String src1 = custom_p1_source ? String(custom_p1_source->getValue()) : profiles[1].dsSource;
    String units = use_mgdl ? "mgdl" : "mmol";
    if (custom_units) units = custom_units->getValue();
    String tls = tls_insecure ? "ano" : "nie";
    if (custom_tls) tls = custom_tls->getValue();
    String modeCur = String(display_mode >= 2 ? 2 : 1);
    if (custom_disp_mode) modeCur = custom_disp_mode->getValue();

    char low0[16], high0[16], crit0[16], low1[16], high1[16], crit1[16];
    snprintf(low0, sizeof(low0), "%.1f", custom_t_low ? atof(custom_t_low->getValue()) : profiles[0].targetLow);
    snprintf(high0, sizeof(high0), "%.1f", custom_t_high ? atof(custom_t_high->getValue()) : profiles[0].targetHigh);
    snprintf(crit0, sizeof(crit0), "%.1f", custom_t_crit ? atof(custom_t_crit->getValue()) : profiles[0].targetCrit);
    snprintf(low1, sizeof(low1), "%.1f", custom_p1_t_low ? atof(custom_p1_t_low->getValue()) : profiles[1].targetLow);
    snprintf(high1, sizeof(high1), "%.1f", custom_p1_t_high ? atof(custom_p1_t_high->getValue()) : profiles[1].targetHigh);
    snprintf(crit1, sizeof(crit1), "%.1f", custom_p1_t_crit ? atof(custom_p1_t_crit->getValue()) : profiles[1].targetCrit);

    String page;
    page.reserve(14000);
    String langCur = String(diaI18nCode());
    langCur.toLowerCase();
    page += F(
        "<!DOCTYPE html><html><head><meta charset=UTF-8>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>GlukoView</title><style>"
        "body{font-family:system-ui,Verdana,sans-serif;margin:0;padding:16px;background:#0b0f14;color:#eee}"
        ".brand{font-size:28px;font-weight:700;letter-spacing:-0.5px;color:#fff;margin:0 0 4px}"
        ".brand span{color:#1fa3ec}"
        ".sub{color:#6a7a8a;font-size:13px;margin:0 0 18px}"
        "input,select,textarea{width:100%;padding:10px;margin:4px 0 12px;box-sizing:border-box;"
        "border-radius:8px;border:1px solid #243040;background:#151c26;color:#eee}"
        "label{font-size:13px;color:#9aa8b5;display:block}"
        "h3{margin:22px 0 8px;padding:10px 12px;background:#151c26;border-radius:10px;"
        "border-left:4px solid #1fa3ec;color:#fff;font-size:16px}"
        "h3.b{border-left-color:#7c5cff}"
        "button{width:100%;padding:14px;background:linear-gradient(135deg,#1fa3ec,#0d7ab8);"
        "color:#fff;border:0;border-radius:10px;font-size:16px;font-weight:600;margin-top:8px}"
        "a{color:#8cf}.h{color:#6a7a8a;font-size:12px;margin:0 0 8px}"
        ".srcblk{display:none}"
        "</style><script>"
        "function diaSrc(pfx,v){"
        "['nightscout','libre','dexcom','guardian'].forEach(function(k){"
        "var el=document.getElementById(pfx+'_'+k);"
        "if(el)el.style.display=(k===v)?'block':'none';"
        "});}"
        "function diaMode(v){"
        "var b=document.getElementById('userB');"
        "if(b)b.style.display=(v==='2')?'block':'none';"
        "var a=document.getElementById('userATitle');"
        "if(a)a.style.display=(v==='2')?'block':'none';"
        "}"
        "</script></head><body>"
        "<div class=brand>Gluko<span>View</span></div>"
        "<p class=sub>");
    page += S(Str::PortalTitle);
    page += F("</p><p><a href=\"/setup?s=0\">");
    page += S(Str::WizLink);
    page += F("</a></p><form method=POST action=/paramsave>");

    page += F("<label>");
    page += S(Str::PortalLang);
    page += F("</label><select name=ui_lang>");
    {
        static const char *kOpt[] = {"sk", "cs", "en", "de", "es", "fr"};
        for (int i = 0; i < diaI18nCount(); i++) {
            page += diaSelOpt(kOpt[i], diaI18nNativeNameAt(i), langCur.c_str());
        }
    }
    page += F("</select>");
    // Kruhový 1.28: len 1 používateľ – bez prepínača a bez sekcie B
    if (!diaBoardIsRound()) {
        page += F("<label>");
        page += S(Str::PortalDispMode);
        page += F("</label><select name=disp_mode onchange=\"diaMode(this.value)\">");
        page += diaSelOpt("1", S(Str::PortalMode1), modeCur);
        page += diaSelOpt("2", S(Str::PortalMode2), modeCur);
        page += F("</select>");
    } else {
        page += F("<input type=hidden name=disp_mode value=1>");
    }
    page += F("<label>");
    page += S(Str::PortalUnits);
    page += F("</label><select name=units>");
    page += diaSelOpt("mmol", "mmol/L", units);
    page += diaSelOpt("mgdl", "mg/dL", units);
    page += F("</select><label>");
    page += S(Str::PortalTls);
    page += F("</label><select name=tls_skip>");
    page += diaSelOpt("ano", S(Str::PortalTlsYes), tls);
    page += diaSelOpt("nie", S(Str::PortalTlsNo), tls);
    page += F("</select>");

    // 3.5B: nadpis „Používateľ A“ len v dual režime
    if (!diaBoardIsRound()) {
        page += F("<h3 id=userATitle");
        if (modeCur != "2") page += F(" style=display:none");
        page += F(">");
        page += S(Str::PortalUserA);
        page += F("</h3>");
        diaField(page, S(Str::PortalName), "p0_name",
                 custom_p0_name ? String(custom_p0_name->getValue()) : profiles[0].name);
    }
    {
        String listA = profiles[0].carelink_patient_list;
        String curA = custom_cl_patient ? String(custom_cl_patient->getValue()) : profiles[0].carelink_patient_pref;
        diaPortalSourceBlock(
            page, false, src0,
            custom_ns_url ? String(custom_ns_url->getValue()) : profiles[0].nsUrl,
            profiles[0].nsToken,
            custom_llu_user ? String(custom_llu_user->getValue()) : profiles[0].libreUser,
            profiles[0].librePwd,
            custom_libre_patient ? String(custom_libre_patient->getValue()) : profiles[0].libre_patient_pref,
            custom_llu_region ? String(custom_llu_region->getValue()) : profiles[0].libreRegion,
            custom_dx_user ? String(custom_dx_user->getValue()) : profiles[0].dexUser,
            profiles[0].dexPwd,
            custom_dx_region ? String(custom_dx_region->getValue()) : profiles[0].dexRegion,
            profiles[0].libre_patient_list,
            curA, listA, carelinkTokensReadyForSlot(0));
    }
    diaField(page, S(Str::PortalLow), "t_low", String(low0));
    diaField(page, S(Str::PortalHigh), "t_high", String(high0));
    diaField(page, S(Str::PortalCrit), "t_crit", String(crit0));

    if (!diaBoardIsRound()) {
        // User B len pri režime 2 — JS diaMode() prepína viditeľnosť
        page += F("<div id=userB");
        if (modeCur != "2") page += F(" style=display:none");
        page += F("><h3 class=b>");
        page += S(Str::PortalUserB);
        page += F("</h3>");
        diaField(page, S(Str::PortalName), "p1_name",
                 custom_p1_name ? String(custom_p1_name->getValue()) : profiles[1].name);
        {
            const bool ownB = carelinkSlotOwnsTokens(1);
            String listB = profiles[1].carelink_patient_list;
            if (!ownB && listB.length() == 0 && src1.equalsIgnoreCase("guardian"))
                listB = profiles[0].carelink_patient_list;
            String curB = custom_p1_cl_pref ? String(custom_p1_cl_pref->getValue()) : profiles[1].carelink_patient_pref;
            diaPortalSourceBlock(
                page, true, src1,
                custom_p1_ns_url ? String(custom_p1_ns_url->getValue()) : profiles[1].nsUrl,
                profiles[1].nsToken,
                custom_p1_llu_user ? String(custom_p1_llu_user->getValue()) : profiles[1].libreUser,
                profiles[1].librePwd,
                custom_p1_libre_pref ? String(custom_p1_libre_pref->getValue()) : profiles[1].libre_patient_pref,
                custom_p1_llu_reg ? String(custom_p1_llu_reg->getValue()) : profiles[1].libreRegion,
                custom_p1_dx_user ? String(custom_p1_dx_user->getValue()) : profiles[1].dexUser,
                profiles[1].dexPwd,
                custom_p1_dx_reg ? String(custom_p1_dx_reg->getValue()) : profiles[1].dexRegion,
                profiles[1].libre_patient_list,
                curB, listB, carelinkTokensReadyForSlot(ownB ? 1 : 0));
        }
        diaField(page, S(Str::PortalLow), "p1_t_low", String(low1));
        diaField(page, S(Str::PortalHigh), "p1_t_high", String(high1));
        diaField(page, S(Str::PortalCrit), "p1_t_crit", String(crit1));
        page += F("</div>");
    }

    page += F("<button type=submit>");
    page += S(Str::PortalSave);
    page += F("</button></form><p><a href=/>");
    page += S(Str::PortalBack);
    page += F("</a> | <a href=/nets>");
    page += S(Str::PortalSavedWifi);
    page += F("</a></p>");
    if (!diaBoardIsRound()) {
        page += F("<script>diaMode(document.querySelector('[name=disp_mode]').value);</script>");
    }
    page += F("</body></html>");

    Serial.printf("[portal] /param page len=%u freeheap=%u\n", (unsigned)page.length(), (unsigned)ESP.getFreeHeap());
    wm.server->send(200, "text/html; charset=utf-8", page);
}

static void handleDiaNetsPage() {
    if (!wm.server) return;
    refreshPortalSavedNetsHtml();

    String page;
    page.reserve(1400);
    page += F(
        "<!DOCTYPE html><html><head><meta charset=UTF-8>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>GlukoView WiFi</title><style>"
        "body{font-family:system-ui,Verdana,sans-serif;margin:12px;background:#0b0f14;color:#eee}"
        "button{width:100%;padding:12px;background:#1fa3ec;color:#fff;border:0;border-radius:8px;font-size:16px;margin-top:8px}"
        "a{color:#8cf}.box{background:#151c26;padding:14px;border-radius:8px;margin:12px 0}"
        "</style></head><body><div style=\"font-size:22px;font-weight:700;margin-bottom:4px\">"
        "Gluko<span style=color:#1fa3ec>View</span></div><h2>");
    page += S(Str::PortalNetsTitle);
    page += F("</h2><div class=box>");
    page += buildSavedNetsListHtml();
    page += F("</div><p style=\"color:#888\">");
    page += S(Str::PortalNetsHint);
    page += F("</p><form action=/wifi method=get><button type=submit>");
    page += S(Str::PortalConfigWifi);
    page += F("</button></form><p><a href=/>");
    page += S(Str::PortalBackMenu);
    page += F("</a></p></body></html>");
    wm.server->send(200, "text/html; charset=utf-8", page);
}

static void diaPinSendForm(WebServer &server, bool bad) {
    String page;
    page.reserve(700);
    page += F("<!DOCTYPE html><html><head><meta charset=utf-8>"
              "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
              "<title>GlukoView</title><style>"
              "body{font-family:sans-serif;background:#111;color:#eee;text-align:center;padding:2rem}"
              "input{font-size:2rem;letter-spacing:.35rem;width:9rem;text-align:center}"
              "button{font-size:1.2rem;margin-top:1rem;padding:.5rem 1.4rem}"
              ".err{color:#f66}</style></head><body><h1>GlukoView</h1><p>");
    page += S(Str::PinWebHint);
    page += F("</p>");
    if (diaPinLockedOut()) {
        page += F("<p class=err>");
        page += S(Str::PinWebWait);
        page += F("</p>");
    } else if (bad) {
        page += F("<p class=err>");
        page += S(Str::PinWebBad);
        page += F("</p>");
    }
    page += F("<form method=post action=/pin>"
              "<input name=pin inputmode=numeric pattern=\"[0-9]{4}\" maxlength=4 "
              "autocomplete=off required autofocus><br>"
              "<button type=submit>OK</button></form></body></html>");
    server.send(200, "text/html; charset=utf-8", page);
}

static void diaPinRedirectHome(WebServer &server, bool withCookie) {
    if (withCookie) {
        String cookie = String(F("gvpin=")) + diaPinToken() + F("; Path=/; Max-Age=28800; HttpOnly");
        server.sendHeader(F("Set-Cookie"), cookie, true);
    }
    server.sendHeader(F("Location"), F("/"), !withCookie);
    server.send(302, "text/plain", "OK");
}

static void handlePinGet() {
    if (!wm.server) return;
    if (!diaPinIsSet() || diaPinCookieOk(wm.server->header("Cookie"))) {
        diaPinRedirectHome(*wm.server, false);
        return;
    }
    diaPinSendForm(*wm.server, false);
}

static void handlePinPost() {
    if (!wm.server) return;
    if (!diaPinIsSet()) {
        diaPinRedirectHome(*wm.server, false);
        return;
    }
    if (diaPinLockedOut()) {
        diaPinSendForm(*wm.server, false);
        return;
    }
    String pin = wm.server->arg("pin");
    pin.trim();
    if (diaPinCheck(pin.c_str())) {
        diaPinRedirectHome(*wm.server, true);
        return;
    }
    diaPinSendForm(*wm.server, true);
}

/** Brána pred ostatnými stránkami. canHandle beží ešte pred Cookie, preto
 *  sa relácia overí až v handle() a pri platnom kľúči sa požiadavka posunie ďalej. */
class DiaPinGate : public RequestHandler {
public:
    bool canHandle(HTTPMethod method, String uri) override {
        (void)method;
        (void)uri;
        return diaPinIsSet();
    }
    bool canUpload(String uri) override {
        (void)uri;
        return diaPinIsSet();
    }
    bool handle(WebServer &server, HTTPMethod requestMethod, String requestUri) override {
        if (!diaPinIsSet() || diaPinCookieOk(server.header("Cookie"))) {
            for (RequestHandler *h = next(); h; h = h->next()) {
                if (h->canHandle(requestMethod, requestUri))
                    return h->handle(server, requestMethod, requestUri);
            }
            return false;
        }
        diaPinSendForm(server, false);
        return true;
    }
    void upload(WebServer &server, String requestUri, HTTPUpload &up) override {
        if (!diaPinIsSet() || !diaPinCookieOk(server.header("Cookie"))) return;
        for (RequestHandler *h = next(); h; h = h->next()) {
            if (h->canUpload(requestUri)) {
                h->upload(server, requestUri, up);
                return;
            }
        }
    }
};

static void wizSet(WiFiManagerParameter *p, const String &v) {
    if (!p) return;
    if ((int)v.length() > p->getValueLength()) return;
    p->setValue(v.c_str(), p->getValueLength());
}

static int wizAdvance(int step) {
    if (step <= 0) return 1;
    if (step == 1) {
        if (profiles[0].dsSource == "off" || profiles[0].dsSource.length() == 0) {
            if (diaBoardIsRound() || display_mode < 2) return 6;
            return 4;
        }
        return 2;
    }
    if (step == 2) {
        if (diaBoardIsRound() || display_mode < 2) return 6;
        if (profiles[0].dsSource == "guardian") return 3;
        return 4;
    }
    if (step == 3) return preferences.getUChar("cl_same", 0) == 1 ? 6 : 4;
    if (step == 4) return (profiles[1].dsSource == "off" || profiles[1].dsSource.length() == 0) ? 6 : 5;
    return 6;
}

static void wizApply(int step) {
    if (!wm.server) return;
    WebServer &sv = *wm.server;
    if (step == 0) {
        int m = diaBoardIsRound() ? 1 : sv.arg("disp_mode").toInt();
        display_mode = (m >= 2) ? 2 : 1;
        wizSet(custom_disp_mode, display_mode >= 2 ? "2" : "1");
        diaConfigSaveProfiles();
        return;
    }
    if (step == 1 || step == 4) {
        const int slot = (step == 4) ? 1 : 0;
        String name = sv.arg("name");
        name.trim();
        if (name.length() > 24) name.remove(24);
        String src = sv.arg("source");
        src.trim();
        if (src != "nightscout" && src != "libre" && src != "dexcom" && src != "guardian" && src != "off")
            src = "off";
        profiles[slot].name = name;
        profiles[slot].dsSource = src;
        if (slot == 0) {
            wizSet(custom_p0_name, name);
            wizSet(custom_source, src);
        } else {
            wizSet(custom_p1_name, name);
            wizSet(custom_p1_source, src);
        }
        diaConfigSaveProfiles();
        diaConfigPushSlot(slot);
        forceFetch = true;
        return;
    }
    if (step == 2 || step == 5) {
        const int slot = (step == 5) ? 1 : 0;
        auto &p = profiles[slot];
        if (p.dsSource == "nightscout") {
            p.nsUrl = sv.arg("ns_url");
            p.nsUrl.trim();
            diaApplySecret(p.nsToken, sv.arg("ns_token").c_str());
            wizSet(slot ? custom_p1_ns_url : custom_ns_url, p.nsUrl);
        } else if (p.dsSource == "libre") {
            p.libreUser = sv.arg("user");
            p.libreUser.trim();
            diaApplySecret(p.librePwd, sv.arg("pwd").c_str());
            p.libreRegion = sv.arg("region");
            if (p.libreRegion != "us") p.libreRegion = "eu";
            p.dsUser = p.libreUser;
            p.dsPwd = p.librePwd;
            p.dsRegion = p.libreRegion;
            wizSet(slot ? custom_p1_llu_user : custom_llu_user, p.libreUser);
            wizSet(slot ? custom_p1_llu_reg : custom_llu_region, p.libreRegion);
        } else if (p.dsSource == "dexcom") {
            p.dexUser = sv.arg("user");
            p.dexUser.trim();
            diaApplySecret(p.dexPwd, sv.arg("pwd").c_str());
            p.dexRegion = sv.arg("region");
            if (p.dexRegion != "us" && p.dexRegion != "jp") p.dexRegion = "ous";
            p.dsUser = p.dexUser;
            p.dsPwd = p.dexPwd;
            p.dsRegion = p.dexRegion;
            wizSet(slot ? custom_p1_dx_user : custom_dx_user, p.dexUser);
            wizSet(slot ? custom_p1_dx_reg : custom_dx_region, p.dexRegion);
        }
        diaConfigSaveProfiles();
        diaConfigPushSlot(slot);
        forceFetch = true;
        return;
    }
    if (step == 3) {
        const bool same = sv.arg("same") == "1";
        preferences.putUChar("cl_same", same ? 1 : 0);
        if (same) {
            profiles[1].dsSource = "guardian";
            wizSet(custom_p1_source, "guardian");
            diaConfigSaveProfiles();
            diaConfigPushSlot(1);
        }
        return;
    }
    if (step == 6) {
        if (sv.hasArg("pa")) {
            profiles[0].carelink_patient_pref = sv.arg("pa");
            profiles[0].carelink_patient_pref.trim();
            wizSet(custom_cl_patient, profiles[0].carelink_patient_pref);
        }
        if (sv.hasArg("pb")) {
            profiles[1].carelink_patient_pref = sv.arg("pb");
            profiles[1].carelink_patient_pref.trim();
            wizSet(custom_p1_cl_pref, profiles[1].carelink_patient_pref);
        }
        diaConfigSaveProfiles();
        diaConfigPushSlot(0);
        if (!diaBoardIsRound() && display_mode >= 2) diaConfigPushSlot(1);
        forceFetch = true;
    }
}

static void wizSourceSelect(String &page, const String &cur) {
    page += F("<label>");
    page += S(Str::PortalPickSource);
    page += F("</label><select name=source>");
    page += diaSelOpt("off", S(Str::PortalSrcOff), cur);
    page += diaSelOpt("nightscout", "Nightscout", cur);
    page += diaSelOpt("libre", "Libre LinkUp", cur);
    page += diaSelOpt("dexcom", "Dexcom Share", cur);
    page += diaSelOpt("guardian", "Guardian (CareLink)", cur);
    page += F("</select>");
}

static void wizPatients(String &page, const char *field, const String &list, const String &cur) {
    if (!list.length()) return;
    page += F("<select name=");
    page += field;
    page += F(">");
    int from = 0;
    while (from < (int)list.length()) {
        int nl = list.indexOf('\n', from);
        if (nl < 0) nl = list.length();
        String line = list.substring(from, nl);
        line.trim();
        if (line.length()) {
            int tab = line.indexOf('\t');
            String id = tab >= 0 ? line.substring(0, tab) : line;
            String name = tab >= 0 ? line.substring(tab + 1) : line;
            page += F("<option value=\"");
            page += diaHtmlAttr(id);
            page += F("\"");
            if (cur == id || cur == name) page += F(" selected");
            page += F(">");
            page += diaHtmlAttr(name);
            page += F("</option>");
        }
        from = nl + 1;
    }
    page += F("</select>");
}

static String wizWhen(int slot) {
    const DiaSlotReading &r = slotReadings[slot];
    if (!r.has_valid_reading || r.timestamp <= 0 || time(nullptr) < 1700000000) return "—";
    time_t ts = (time_t)r.timestamp;
    struct tm ti;
    if (!localtime_r(&ts, &ti)) return "—";
    long age = time(nullptr) - r.timestamp;
    if (age < 0) age = 0;
    char b[40];
    snprintf(b, sizeof(b), "%02d:%02d (%ld min)", ti.tm_hour, ti.tm_min, age / 60);
    return b;
}

static void handleSetup() {
    if (!wm.server) return;
    if (diaBoardIsRound()) display_mode = 1;
    WebServer &sv = *wm.server;
    if (sv.method() == HTTP_POST) {
        int step = sv.arg("step").toInt();
        if (diaBoardIsRound() && step == 0) step = 1;
        wizApply(step);
        int next = wizAdvance(step);
        preferences.putUChar("wiz", next >= 6 ? 255 : (uint8_t)next);
        String loc = String("/setup?s=") + String(next);
        sv.sendHeader("Location", loc, true);
        sv.send(302, "text/plain", "");
        return;
    }

    int step = 0;
    if (sv.hasArg("s")) step = sv.arg("s").toInt();
    else if (preferences.isKey("wiz") && preferences.getUChar("wiz", 255) != 255)
        step = preferences.getUChar("wiz", 0);
    if (diaBoardIsRound() && step == 0) step = 1;
    if (step < 0 || step > 6) step = diaBoardIsRound() ? 1 : 0;

    String page;
    page.reserve(4200);
    page += F(
        "<!DOCTYPE html><html><head><meta charset=UTF-8>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>GlukoView</title><style>"
        "body{font-family:system-ui,Verdana,sans-serif;margin:0;padding:16px;background:#0b0f14;color:#eee}"
        "h1{font-size:22px;margin:0 0 12px}label{font-size:13px;color:#9aa8b5;display:block}"
        "input,select{width:100%;padding:10px;margin:4px 0 12px;box-sizing:border-box;border-radius:8px;"
        "border:1px solid #243040;background:#151c26;color:#eee}"
        "button,a.btn{display:block;width:100%;padding:14px;background:#1fa3ec;color:#fff;border:0;"
        "border-radius:10px;font-size:16px;text-align:center;text-decoration:none;box-sizing:border-box;margin-top:8px}"
        "a{color:#8cf}.muted{color:#9ab;font-size:14px;line-height:1.4}"
        "</style></head><body><h1>");
    page += S(Str::WizLink);
    page += F("</h1>");

    int back = -1;
    if (step == 1 && !diaBoardIsRound()) back = 0;
    else if (step == 2) back = 1;
    else if (step == 3) back = 2;
    else if (step == 4) back = (profiles[0].dsSource == "guardian") ? 3 : 1;
    else if (step == 5) back = 4;
    if (back >= 0) {
        page += F("<p><a href=\"/setup?s=");
        page += String(back);
        page += F("\">");
        page += S(Str::Back);
        page += F("</a></p>");
    }

    if (step == 0) {
        page += F("<form method=POST action=/setup><input type=hidden name=step value=0><p>");
        page += S(Str::WizCount);
        page += F("</p><select name=disp_mode>");
        String modeCur = String(display_mode >= 2 ? 2 : 1);
        page += diaSelOpt("1", S(Str::PortalMode1), modeCur);
        page += diaSelOpt("2", S(Str::PortalMode2), modeCur);
        page += F("</select><button type=submit>");
        page += S(Str::PortalSave);
        page += F("</button></form>");
    } else if (step == 1 || step == 4) {
        const int slot = (step == 4) ? 1 : 0;
        page += F("<form method=POST action=/setup><input type=hidden name=step value=");
        page += String(step);
        page += F("><h2>");
        page += S(slot ? Str::PortalUserB : Str::PortalUserA);
        page += F("</h2>");
        diaField(page, S(Str::PortalName), "name", profiles[slot].name);
        wizSourceSelect(page, profiles[slot].dsSource);
        page += F("<button type=submit>");
        page += S(Str::PortalSave);
        page += F("</button></form>");
    } else if (step == 2 || step == 5) {
        const int slot = (step == 5) ? 1 : 0;
        const String &src = profiles[slot].dsSource;
        page += F("<h2>");
        page += S(slot ? Str::PortalUserB : Str::PortalUserA);
        page += F("</h2>");
        if (src == "guardian") {
            page += F("<p class=muted>");
            page += S(Str::ClFollowerHint);
            page += F("</p><a class=btn href=\"/carelink?slot=");
            page += String(slot);
            page += F("\">");
            page += S(Str::ClOpenBtn);
            page += F("</a><form method=POST action=/setup><input type=hidden name=step value=");
            page += String(step);
            page += F("><button type=submit>");
            page += S(Str::PortalSave);
            page += F("</button></form>");
        } else if (src == "off") {
            page += F("<form method=POST action=/setup><input type=hidden name=step value=");
            page += String(step);
            page += F("><button type=submit>");
            page += S(Str::PortalSave);
            page += F("</button></form>");
        } else {
            page += F("<form method=POST action=/setup><input type=hidden name=step value=");
            page += String(step);
            page += F(">");
            if (src == "nightscout") {
                diaField(page, S(Str::PortalNsUrl), "ns_url", profiles[slot].nsUrl);
                diaField(page, S(Str::PortalNsToken), "ns_token", "", true);
            } else if (src == "libre") {
                diaField(page, S(Str::PortalEmail), "user", profiles[slot].libreUser);
                diaField(page, S(Str::PortalPassword), "pwd", "", true);
                page += F("<label>");
                page += S(Str::PortalRegion);
                page += F("</label><select name=region>");
                page += diaSelOpt("eu", "EU", profiles[slot].libreRegion);
                page += diaSelOpt("us", "US", profiles[slot].libreRegion);
                page += F("</select>");
            } else {
                diaField(page, S(Str::PortalEmail), "user", profiles[slot].dexUser);
                diaField(page, S(Str::PortalPassword), "pwd", "", true);
                page += F("<label>");
                page += S(Str::PortalRegion);
                page += F("</label><select name=region>");
                page += diaSelOpt("ous", "EU / OUS", profiles[slot].dexRegion);
                page += diaSelOpt("us", "US", profiles[slot].dexRegion);
                page += diaSelOpt("jp", "JP", profiles[slot].dexRegion);
                page += F("</select>");
            }
            page += F("<button type=submit>");
            page += S(Str::PortalSave);
            page += F("</button></form>");
        }
    } else if (step == 3) {
        page += F("<p>");
        page += S(Str::WizSame);
        page += F("</p><form method=POST action=/setup><input type=hidden name=step value=3>");
        page += F("<button type=submit name=same value=1>");
        page += S(Str::PortalTlsYes);
        page += F("</button><button type=submit name=same value=0>");
        page += S(Str::PortalTlsNo);
        page += F("</button></form>");
    } else {
        preferences.putUChar("wiz", 255);
        page += F("<p>");
        page += (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : String("—");
        page += F("</p><p>");
        page += String(FIRMWARE_VERSION_MAJOR);
        page += F(".");
        page += String(FIRMWARE_VERSION_MINOR);
        page += F("</p><p>");
        page += profiles[0].name.length() ? profiles[0].name : String("A");
        page += F(": ");
        page += wizWhen(0);
        page += F("</p>");
        if (!diaBoardIsRound() && display_mode >= 2) {
            page += F("<p>");
            page += profiles[1].name.length() ? profiles[1].name : String("B");
            page += F(": ");
            page += wizWhen(1);
            page += F("</p>");
        }
        const bool showA = profiles[0].dsSource == "guardian";
        const bool showB = !diaBoardIsRound() && display_mode >= 2 && profiles[1].dsSource == "guardian";
        const String listA = profiles[0].carelink_patient_list;
        String listB = profiles[1].carelink_patient_list;
        if (showB && !carelinkSlotOwnsTokens(1) && listB.length() == 0) listB = listA;
        if (showA || showB) {
            page += F("<form method=POST action=/setup><input type=hidden name=step value=6>");
            if (showA) {
                page += F("<label>");
                page += S(Str::PortalUserA);
                page += F("</label>");
                if (listA.length()) wizPatients(page, "pa", listA, profiles[0].carelink_patient_pref);
                else {
                    page += F("<a class=btn href=\"/carelink?slot=0\">");
                    page += S(Str::ClPickPatientBtn);
                    page += F("</a>");
                }
            }
            if (showB) {
                page += F("<label>");
                page += S(Str::PortalUserB);
                page += F("</label>");
                if (listB.length() && !(carelinkSlotOwnsTokens(1) && profiles[1].carelink_patient_list.length() == 0))
                    wizPatients(page, "pb", listB, profiles[1].carelink_patient_pref);
                else {
                    page += F("<a class=btn href=\"/carelink?slot=1\">");
                    page += S(Str::ClPickPatientBtn);
                    page += F("</a>");
                }
            }
            if ((showA && listA.length()) || (showB && listB.length())) {
                page += F("<button type=submit>");
                page += S(Str::PortalSave);
                page += F("</button>");
            }
            page += F("</form>");
        }
        page += F("<p class=muted>");
        page += S(Str::WizUnplug);
        page += F("</p><p><a href=\"/param?all=1\">");
        page += S(Str::ClParams);
        page += F("</a></p>");
    }
    page += F("</body></html>");
    sv.send(200, "text/html", page);
}

static void bindWmWebServer() {
    if (!wm.server) return;
    const char *hdrs[] = {"Cookie"};
    wm.server->collectHeaders(hdrs, 1);
    // Prepíše WM root: captive portal → /wifi, po pripojení → /param
    wm.server->on("/", HTTP_GET, handlePortalHome);
    wm.server->on("/pin", HTTP_GET, handlePinGet);
    wm.server->on("/pin", HTTP_POST, handlePinPost);
    wm.server->addHandler(new DiaPinGate());
    wm.server->on("/param", handleDiaParamPage);
    wm.server->on("/setup", HTTP_GET, handleSetup);
    wm.server->on("/setup", HTTP_POST, handleSetup);
    wm.server->on("/nets", handleDiaNetsPage);
    diaPortalRegisterCarelinkRoutes();
}

static void configModeCallback(WiFiManager *myWiFiManager) {
    (void)myWiFiManager;
    showBootStatus("WiFi SETUP", S(Str::BootSetupHint));
}

void diaWifiSetupManager() {
    wm.setSaveConfigCallback(saveConfigCallback);
    wm.setSaveParamsCallback(saveConfigCallback);
    wm.setWebServerCallback(bindWmWebServer);
    wm.setTitle("GlukoView");
    wm.setClass("invert"); // tmavý GlukoView štýl (wm_strings_sk.h)
    std::vector<const char *> portalMenu = {"wifi", "param", "custom", "info", "sep", "exit"};
    wm.setMenu(portalMenu);
    wm.setShowInfoUpdate(false);
    wm.setCustomHeadElement("");
}

void diaWifiAddParameters() {
    if (custom_source) return; // už alokované – neleakovať WM buffery
    const bool dualOk = !diaBoardIsRound();
    char modeBuf[4];
    snprintf(modeBuf, sizeof(modeBuf), "%u", dualOk ? (unsigned)display_mode : 1u);

    // disp_mode / profil B len na 3.5B – inak ich WM default stránka ukáže
    custom_disp_mode = dualOk
        ? new WiFiManagerParameter("disp_mode", "Deti (1 alebo 2)", modeBuf, 2)
        : nullptr;
    custom_p0_name = new WiFiManagerParameter("p0_name", "Dieta A meno", profiles[0].name.c_str(), 24);
    custom_source = new WiFiManagerParameter("ds_source", "A zdroj (libre/ns/dexcom/guardian)", profiles[0].dsSource.c_str(), 20);
    custom_ns_url = new WiFiManagerParameter("ns_url", "A Nightscout URL", profiles[0].nsUrl.c_str(), 100);
    custom_ns_token = new WiFiManagerParameter("ns_token", "A Nightscout token", "", 64);
    custom_llu_user = new WiFiManagerParameter("llu_user", "A Libre email", profiles[0].libreUser.c_str(), 64);
    custom_llu_pwd = new WiFiManagerParameter("llu_pwd", "A Libre heslo", "", 64);
    custom_libre_patient = new WiFiManagerParameter("libre_pref", "A Libre pacient", profiles[0].libre_patient_pref.c_str(), 64);
    custom_cl_patient = new WiFiManagerParameter("cl_pref", "A Guardian pacient", profiles[0].carelink_patient_pref.c_str(), 64);
    custom_llu_region = new WiFiManagerParameter("llu_reg", "A Libre region", profiles[0].libreRegion.c_str(), 8);
    custom_dx_user = new WiFiManagerParameter("dx_user", "A Dexcom email", profiles[0].dexUser.c_str(), 64);
    custom_dx_pwd = new WiFiManagerParameter("dx_pwd", "A Dexcom heslo", "", 64);
    custom_dx_region = new WiFiManagerParameter("dx_reg", "A Dexcom region", profiles[0].dexRegion.c_str(), 8);
    custom_t_low = new WiFiManagerParameter("t_low", "A Spodny", String(profiles[0].targetLow, 1).c_str(), 10);
    custom_t_high = new WiFiManagerParameter("t_high", "A Vrchny", String(profiles[0].targetHigh, 1).c_str(), 10);
    custom_t_crit = new WiFiManagerParameter("t_crit", "A Kriticke", String(profiles[0].targetCrit, 1).c_str(), 10);

    if (dualOk) {
        custom_p1_name = new WiFiManagerParameter("p1_name", "Dieta B meno", profiles[1].name.c_str(), 24);
        custom_p1_source = new WiFiManagerParameter("p1_src", "B zdroj (off/libre/ns/dexcom/guardian)", profiles[1].dsSource.c_str(), 20);
        custom_p1_ns_url = new WiFiManagerParameter("p1_nsu", "B Nightscout URL", profiles[1].nsUrl.c_str(), 100);
        custom_p1_ns_token = new WiFiManagerParameter("p1_nst", "B Nightscout token", "", 64);
        custom_p1_llu_user = new WiFiManagerParameter("p1_llu", "B Libre email", profiles[1].libreUser.c_str(), 64);
        custom_p1_llu_pwd = new WiFiManagerParameter("p1_llp", "B Libre heslo", "", 64);
        custom_p1_libre_pref = new WiFiManagerParameter("p1_lpf", "B Libre pacient", profiles[1].libre_patient_pref.c_str(), 64);
        custom_p1_cl_pref = new WiFiManagerParameter("p1_cpf", "B Guardian pacient", profiles[1].carelink_patient_pref.c_str(), 64);
        custom_p1_llu_reg = new WiFiManagerParameter("p1_llr", "B Libre region", profiles[1].libreRegion.c_str(), 8);
        custom_p1_dx_user = new WiFiManagerParameter("p1_dxu", "B Dexcom email", profiles[1].dexUser.c_str(), 64);
        custom_p1_dx_pwd = new WiFiManagerParameter("p1_dxp", "B Dexcom heslo", "", 64);
        custom_p1_dx_reg = new WiFiManagerParameter("p1_dxr", "B Dexcom region", profiles[1].dexRegion.c_str(), 8);
        custom_p1_t_low = new WiFiManagerParameter("p1_t_low", "B Spodny", String(profiles[1].targetLow, 1).c_str(), 10);
        custom_p1_t_high = new WiFiManagerParameter("p1_t_high", "B Vrchny", String(profiles[1].targetHigh, 1).c_str(), 10);
        custom_p1_t_crit = new WiFiManagerParameter("p1_t_crit", "B Kriticke", String(profiles[1].targetCrit, 1).c_str(), 10);
    }

    custom_units = new WiFiManagerParameter("units", "Jednotky", use_mgdl ? "mgdl" : "mmol", 8);
    custom_tls = new WiFiManagerParameter("tls_skip", "TLS", tls_insecure ? "ano" : "nie", 8);
    // cl_json NIE cez WiFiManagerParameter – 7000 B buffer zje internal heap a Dexcom/Libre TLS padá (connect -1)
    {
        static char langInit[4];
        String c = String(diaI18nCode());
        c.toLowerCase();
        strncpy(langInit, c.c_str(), sizeof(langInit) - 1);
        langInit[sizeof(langInit) - 1] = '\0';
        custom_lang = new WiFiManagerParameter("ui_lang", S(Str::PortalLang), langInit, 4);
    }

    if (dualOk) wm.addParameter(custom_disp_mode);
    wm.addParameter(custom_p0_name);
    wm.addParameter(custom_source);
    wm.addParameter(custom_ns_url);
    wm.addParameter(custom_ns_token);
    wm.addParameter(custom_llu_user);
    wm.addParameter(custom_llu_pwd);
    wm.addParameter(custom_libre_patient);
    wm.addParameter(custom_cl_patient);
    wm.addParameter(custom_llu_region);
    wm.addParameter(custom_dx_user);
    wm.addParameter(custom_dx_pwd);
    wm.addParameter(custom_dx_region);
    wm.addParameter(custom_t_low);
    wm.addParameter(custom_t_high);
    wm.addParameter(custom_t_crit);
    if (dualOk) {
        wm.addParameter(custom_p1_name);
        wm.addParameter(custom_p1_source);
        wm.addParameter(custom_p1_ns_url);
        wm.addParameter(custom_p1_ns_token);
        wm.addParameter(custom_p1_llu_user);
        wm.addParameter(custom_p1_llu_pwd);
        wm.addParameter(custom_p1_libre_pref);
        if (custom_p1_cl_pref) wm.addParameter(custom_p1_cl_pref);
        wm.addParameter(custom_p1_llu_reg);
        wm.addParameter(custom_p1_dx_user);
        wm.addParameter(custom_p1_dx_pwd);
        wm.addParameter(custom_p1_dx_reg);
        wm.addParameter(custom_p1_t_low);
        wm.addParameter(custom_p1_t_high);
        wm.addParameter(custom_p1_t_crit);
    }
    wm.addParameter(custom_units);
    wm.addParameter(custom_tls);
    wm.addParameter(custom_lang);

    refreshPortalSavedNetsHtml();
}

static bool tryConnectOne(const String &ssid, const String &pwd, uint32_t timeoutMs, bool showUi) {
    if (ssid.length() == 0) return false;

    Serial.printf("WiFi skusam '%s' (pwd %d, timeout %u ms)\n",
                  ssid.c_str(), pwd.length(), (unsigned)timeoutMs);

    if (showUi) {
        char detail[80];
        snprintf(detail, sizeof(detail), "%s", ssid.c_str());
        showBootStatus("WiFi", detail);
    }

    // Dôležité: po neúspechu predchádzajúcej siete uvoľniť STA, inak Multi/begin zlyhá
    WiFi.disconnect(false, false);
    delay(150);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pwd.c_str());

    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        wl_status_t st = WiFi.status();
        if (st == WL_CONNECTED) {
            Serial.printf("WiFi OK: %s RSSI=%d\n", WiFi.SSID().c_str(), WiFi.RSSI());
            return true;
        }
        if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL) {
            // rýchly fail – SSID mimo dosah / zlé heslo
            delay(300);
            if (WiFi.status() != WL_CONNECTED) break;
        }
        delay(200);
        diaUiTick(200);
    }

    WiFi.disconnect(false, false);
    delay(100);
    return false;
}

bool diaWifiTryConnect() {
    WifiCred nets[kMaxWifiNets];
    int n = gatherWifiCreds(nets, kMaxWifiNets);

    Serial.printf("WiFi ulozenych sieti: %d\n", n);
    for (int i = 0; i < n; i++) {
        Serial.printf("  [%d] '%s' pwd=%d\n", i, nets[i].ssid.c_str(), nets[i].pwd.length());
    }

    if (n == 0) {
        Serial.println("Ziadna ulozene WiFi.");
        return false;
    }

    rebuildWifiMulti(nets, n);
    showBootStatus("WiFi", S(Str::BootConnecting));

    // Krátky settle bez plného scanu (scan na 3.5B predĺžil boot a občas nechal rádio „hluché“)
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.disconnect(false, false);
    delay(200);

    // 1) Postupne každá sieť (spoľahlivé pri zmene lokality)
    for (int round = 0; round < kConnectRounds; round++) {
        for (int i = 0; i < n; i++) {
            char detail[96];
            snprintf(detail, sizeof(detail), "%d/%d: %s", i + 1, n, nets[i].ssid.c_str());
            showBootStatus(round == 0 ? "WiFi" : S(Str::BootWifiAgain), detail);

            if (tryConnectOne(nets[i].ssid, nets[i].pwd, kPerNetTimeoutMs, false)) {
                saveNetworkToBackup(nets[i].ssid, nets[i].pwd.length() > 0 ? nets[i].pwd : lookupStoredPassword(nets[i].ssid));
                return true;
            }
        }
    }

    // 2) Posledný pokus: WiFiMulti podľa RSSI (ak je viac sietí v dosahu)
    showBootStatus("WiFi", S(Str::BootScanning));
    WiFi.disconnect(false, false);
    delay(200);
    for (int attempt = 0; attempt < 3; attempt++) {
        Serial.printf("WiFiMulti.run attempt %d\n", attempt + 1);
        if (wifiMulti.run(10000) == WL_CONNECTED) {
            Serial.printf("WiFiMulti OK: %s\n", WiFi.SSID().c_str());
            autoSaveNetwork();
            return true;
        }
        delay(500);
        diaUiTick(500);
    }

    Serial.println("WiFi: ziadna zo ulozenych sieti nedostupna.");
    return false;
}

static void restore_ui_after_wifi() {
    restoreMainScreenChrome();
    if (has_valid_reading && last_mmol_value > 0.0f) {
        showCachedReadingOnUi();
    }
    apply_watchface();
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
}

/** Po WiFi: ak chýba zdroj, držať návod s IP; inak bežný ciferník. */
static void afterWifiConnectedUi() {
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
    if (diaNeedsSourceSetup()) {
        showConnectedPortalHint();
        return;
    }
    showBootStatus(S(Str::BootConnected), S(Str::Loading));
    restore_ui_after_wifi();
}

bool diaWifiEnsureConnected() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false); // riadime sami cez zoznam sietí
    diaWifiSetupManager();
    diaWifiAddParameters();
    wm.setCaptivePortalEnable(true);

    WifiCred nets[kMaxWifiNets];
    int savedCount = gatherWifiCreds(nets, kMaxWifiNets);
    rebuildWifiMulti(nets, savedCount);

    // Opakuj: STA všetky siete → (ak treba) AP s timeoutom → znova STA
    for (int cycle = 0; cycle < 6; cycle++) {
        if (diaWifiTryConnect()) {
            autoSaveNetwork();
            wifi_was_connected = true;
            afterWifiConnectedUi();
            refreshPortalSavedNetsHtml();
            wm.startWebPortal();
            return true;
        }

        Serial.printf("WiFi: spustam AP portal (cycle %d, saved=%d)\n", cycle, savedCount);
        showBootStatus("WiFi SETUP", S(Str::BootSetupHint));

        wm.setAPCallback(configModeCallback);
        wm.setAPStaticIPConfig(IPAddress(10, 0, 0, 1), IPAddress(10, 0, 0, 1), IPAddress(255, 255, 255, 0));
        wm.setConfigPortalBlocking(true);
        wm.setSaveConnect(true);
        wm.setCaptivePortalEnable(true);
        // Ak už máme uložené siete – timeout, potom znova skús STA (presun do dielne)
        // Prvé nastavenie (0 sietí) – čakaj dlhšie
        wm.setConfigPortalTimeout(savedCount > 0 ? kPortalTimeoutSec : 0);
        refreshPortalSavedNetsHtml();

        if (wm.startConfigPortal("GlukoView")) {
            autoSaveNetwork();
            Serial.printf("Portal OK, SSID='%s'\n", WiFi.SSID().c_str());
            wifi_was_connected = true;
            afterWifiConnectedUi();
            refreshPortalSavedNetsHtml();
            wm.startWebPortal();
            return true;
        }

        Serial.println("Config portal timeout / ukonceny – znova skusam ulozene siete.");
        showBootStatus("WiFi", S(Str::BootRetry));
        savedCount = gatherWifiCreds(nets, kMaxWifiNets);
        rebuildWifiMulti(nets, savedCount);
    }

    Serial.println("WiFi: nepodarilo sa pripojit ani cez portal.");
    return false;
}

void diaWifiProcess() {
    wm.process();
}

void diaWifiSavePortalConfigIfNeeded() {
    if (!shouldSaveConfig) return;

    // Reštart len keď sa mení WiFi. Uloženie mena, zdroja alebo pacienta
    // nechá fetch bežať, inak by sa CareLink token prerušil v polovici.
    if (s_portalRestart) {
        esp_rom_printf("[wifi] save caka na fetch\n");
        diaFetchQuiesce(40000);
    }

    if (diaBoardIsRound()) {
        display_mode = 1;
    } else if (custom_disp_mode) {
        int m = String(custom_disp_mode->getValue()).toInt();
        display_mode = (m >= 2) ? 2 : 1;
    }
    // Jedna osoba: vypnúť B, aby sa polia nevracali „duchmi“
    if (display_mode < 2) {
        profiles[1].dsSource = "off";
        if (custom_p1_source) wizSet(custom_p1_source, "off");
    }
    if (custom_p0_name) profiles[0].name = custom_p0_name->getValue();
    profiles[0].dsSource = custom_source->getValue();
    profiles[0].nsUrl = custom_ns_url->getValue();
    diaApplySecret(profiles[0].nsToken, custom_ns_token->getValue());
    profiles[0].libreUser = custom_llu_user ? String(custom_llu_user->getValue()) : "";
    if (custom_llu_pwd) diaApplySecret(profiles[0].librePwd, custom_llu_pwd->getValue());
    profiles[0].libreRegion = custom_llu_region ? String(custom_llu_region->getValue()) : "eu";
    profiles[0].libre_patient_pref = custom_libre_patient ? String(custom_libre_patient->getValue()) : "";
    profiles[0].libre_patient_pref.trim();
    // CareLink pacient len ak je A Guardian – inak nechať uložené (Dexcom/Libre A nesmie dostať CL pacienta z B)
    if (profiles[0].dsSource.equalsIgnoreCase("guardian") && custom_cl_patient) {
        profiles[0].carelink_patient_pref = custom_cl_patient->getValue();
        profiles[0].carelink_patient_pref.trim();
    }
    profiles[0].dexUser = custom_dx_user ? String(custom_dx_user->getValue()) : "";
    if (custom_dx_pwd) diaApplySecret(profiles[0].dexPwd, custom_dx_pwd->getValue());
    profiles[0].dexRegion = custom_dx_region ? String(custom_dx_region->getValue()) : "ous";
    if (profiles[0].dsSource == "dexcom") {
        profiles[0].dsUser = profiles[0].dexUser;
        profiles[0].dsPwd = profiles[0].dexPwd;
        profiles[0].dsRegion = profiles[0].dexRegion;
    } else {
        profiles[0].dsUser = profiles[0].libreUser;
        profiles[0].dsPwd = profiles[0].librePwd;
        profiles[0].dsRegion = profiles[0].libreRegion;
    }
    profiles[0].targetLow = String(custom_t_low->getValue()).toFloat();
    profiles[0].targetHigh = String(custom_t_high->getValue()).toFloat();
    profiles[0].targetCrit = String(custom_t_crit->getValue()).toFloat();

    if (custom_p1_name) profiles[1].name = custom_p1_name->getValue();
    if (custom_p1_source) profiles[1].dsSource = custom_p1_source->getValue();
    if (custom_p1_ns_url) profiles[1].nsUrl = custom_p1_ns_url->getValue();
    if (custom_p1_ns_token) diaApplySecret(profiles[1].nsToken, custom_p1_ns_token->getValue());
    if (custom_p1_llu_user) profiles[1].libreUser = custom_p1_llu_user->getValue();
    if (custom_p1_llu_pwd) diaApplySecret(profiles[1].librePwd, custom_p1_llu_pwd->getValue());
    if (custom_p1_llu_reg) profiles[1].libreRegion = custom_p1_llu_reg->getValue();
    if (custom_p1_libre_pref) {
        profiles[1].libre_patient_pref = custom_p1_libre_pref->getValue();
        profiles[1].libre_patient_pref.trim();
    }
    if (custom_p1_cl_pref && profiles[1].dsSource.equalsIgnoreCase("guardian")) {
        profiles[1].carelink_patient_pref = custom_p1_cl_pref->getValue();
        profiles[1].carelink_patient_pref.trim();
    }
    if (custom_p1_dx_user) profiles[1].dexUser = custom_p1_dx_user->getValue();
    if (custom_p1_dx_pwd) diaApplySecret(profiles[1].dexPwd, custom_p1_dx_pwd->getValue());
    if (custom_p1_dx_reg) profiles[1].dexRegion = custom_p1_dx_reg->getValue();
    if (custom_p1_t_low) profiles[1].targetLow = String(custom_p1_t_low->getValue()).toFloat();
    if (custom_p1_t_high) profiles[1].targetHigh = String(custom_p1_t_high->getValue()).toFloat();
    if (custom_p1_t_crit) profiles[1].targetCrit = String(custom_p1_t_crit->getValue()).toFloat();
    if (profiles[1].dsSource == "dexcom") {
        profiles[1].dsUser = profiles[1].dexUser;
        profiles[1].dsPwd = profiles[1].dexPwd;
        profiles[1].dsRegion = profiles[1].dexRegion;
    } else {
        profiles[1].dsUser = profiles[1].libreUser;
        profiles[1].dsPwd = profiles[1].librePwd;
        profiles[1].dsRegion = profiles[1].libreRegion;
    }
    String unitsVal = custom_units->getValue();
    unitsVal.toLowerCase();
    use_mgdl = (unitsVal == "mgdl" || unitsVal == "mg/dl");
    String tlsVal = custom_tls->getValue();
    tlsVal.toLowerCase();
    tls_insecure = !(tlsVal == "nie" || tlsVal == "no" || tlsVal == "0");

    diaConfigClearSessions();
    diaConfigPushSlot(diaShownSlot());
    preferences.putBool("use_mgdl", use_mgdl);
    preferences.putBool("tls_insec", tls_insecure);
    diaConfigSaveProfiles();

    {
        String langVal;
        if (wm.server && wm.server->hasArg("ui_lang")) langVal = wm.server->arg("ui_lang");
        else if (custom_lang) langVal = custom_lang->getValue();
        if (langVal.length()) {
            langVal.toLowerCase();
            diaI18nSet(diaI18nFromCode(langVal.c_str()));
            diaI18nMarkSetupDone();
        }
    }

    autoSaveNetwork();
    shouldSaveConfig = false;
    if (!s_portalRestart) {
        diaConfigPushSlot(0);
        if (!diaBoardIsRound() && display_mode >= 2) diaConfigPushSlot(1);
        diaUiApplyDisplayMode();
        // Ciferník až keď je zdroj skutočne pripravený – inak IP (nie „Config“/Sync)
        if (diaNeedsSourceSetup()) {
            forceFetch = false;
            showConnectedPortalHint();
        } else {
            forceFetch = true;
            restore_ui_after_wifi();
        }
        Serial.println("[wifi] param saved, bez restartu");
        return;
    }
    preferences.end();

    showBootStatus("OK", S(Str::BootSaving));

    // Portál/AP + plné LVGL flushe pred ESP.restart() na S3 často „visia“
    wm.stopWebPortal();
    wm.stopConfigPortal();
    WiFi.disconnect(true, false);
    delay(50);
    WiFi.mode(WIFI_OFF);
    Serial.println("[wifi] config saved – restart");
    Serial.flush();
    delay(200);
    ESP.restart();
    while (true) { delay(1000); } // ak by restart zlyhal
}

void diaWifiMaintain(uint32_t currentMillis) {
    static uint32_t lastWifiCheck = 0;
    static uint32_t lastFullRetry = 0;

    if (currentMillis - lastWifiCheck <= 5000) return;
    lastWifiCheck = currentMillis;

    // Počas HTTP fetchu neskúšať reconnect (kolízia na WiFi stacku)
    if (diaFetchBusy()) return;

    static uint32_t wifiDownSince = 0;
    static String lastHintIp;
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected) {
        if (!wifi_was_connected || wifiDownSince != 0) {
            autoSaveNetwork();
            if (diaNeedsSourceSetup()) {
                forceFetch = false;
                showConnectedPortalHint();
                lastHintIp = WiFi.localIP().toString();
            } else {
                forceFetch = true;
            }
        }
        // Obnoviť hint len ak sa zmenila IP – nie periodicky (Sync by blikal)
        if (diaNeedsSourceSetup()) {
            forceFetch = false;
            String ip = WiFi.localIP().toString();
            if (ip.length() && ip != "0.0.0.0" && ip != lastHintIp) {
                lastHintIp = ip;
                showConnectedPortalHint();
            }
        }
        wifiDownSince = 0;
        wifi_was_connected = true;
        return;
    }

    // Rýchly pokus cez Multi (už má všetky AP)
    if (wifiMulti.run(5000) == WL_CONNECTED) {
        Serial.printf("WiFi reconnect Multi: %s\n", WiFi.SSID().c_str());
        forceFetch = true;
        autoSaveNetwork();
        wifiDownSince = 0;
        wifi_was_connected = true;
        return;
    }

    // Každých ~45 s plný prechod všetkých uložených sietí
    if (currentMillis - lastFullRetry > 45000) {
        lastFullRetry = currentMillis;
        Serial.println("WiFi maintain: plny retry vsetkych sieti");
        WifiCred nets[kMaxWifiNets];
        int n = gatherWifiCreds(nets, kMaxWifiNets);
        rebuildWifiMulti(nets, n);
        for (int i = 0; i < n; i++) {
            if (tryConnectOne(nets[i].ssid, nets[i].pwd, 8000, false)) {
                saveNetworkToBackup(nets[i].ssid, nets[i].pwd);
                forceFetch = true;
                wifiDownSince = 0;
                wifi_was_connected = true;
                return;
            }
        }
    }

    if (wifi_was_connected) {
        if (wifiDownSince == 0) wifiDownSince = currentMillis;
        // Krátky výpadok pri štarte adresy nenechá padnúť. Presun do inej izby áno.
        if ((uint32_t)(currentMillis - wifiDownSince) >= 20000UL) {
            carelinkForgetEndpoints();
            markFetchError(FetchError::Wifi);
            wifi_was_connected = false;
            wifiDownSince = 0;
        }
    }
}
