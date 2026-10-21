#include "dia_portal.h"
#include "dia_wifi.h"
#include "dia_carelink.h"
#include "dia_config.h"
#include "dia_fetch.h"
#include "dia_i18n.h"
#include "dia_ui.h"

#include <WebServer.h>

static String diaHtmlAttr(const String &in) {
    String out;
    out.reserve(in.length() + 8);
    for (unsigned i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c == '&') out += F("&amp;");
        else if (c == '"') out += F("&quot;");
        else if (c == '\'') out += F("&#39;");
        else if (c == '<') out += F("&lt;");
        else out += c;
    }
    return out;
}

static void handleCarelinkPage() {
    if (!wm.server) return;
    const bool ready = carelinkTokensReady();
    const bool pending = carelinkBrowserLoginPending();
    String page;
    page.reserve(4800);
    page += F(
        "<!DOCTYPE html><html><head><meta charset=UTF-8>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>CareLink</title><style>"
        "body{font-family:system-ui,Verdana,sans-serif;margin:14px;background:#0b0f14;color:#eee;line-height:1.45}"
        "h1{font-size:1.35rem;margin:0 0 8px}h2{font-size:1.05rem;margin:18px 0 8px;color:#8cf}"
        ".box{background:#151c26;padding:14px;border-radius:10px;margin:12px 0}"
        ".ok{color:#6d6}.warn{color:#fc6}.err{color:#f66}"
        "ol{padding-left:1.2rem}li{margin:8px 0}"
        "button,.btn{display:block;width:100%;padding:14px;background:#1fa3ec;color:#fff;border:0;"
        "border-radius:10px;font-size:16px;margin-top:10px;text-align:center;text-decoration:none;box-sizing:border-box}"
        "button.sec{background:#2a3544}select,textarea{width:100%;box-sizing:border-box;margin-top:6px;"
        "padding:10px;border-radius:8px;border:1px solid #334;background:#0e141c;color:#eee;font-size:15px}"
        "textarea{min-height:90px}a{color:#8cf}.muted{color:#99a;font-size:0.92rem}"
        "label.pat{display:block;padding:10px;margin:6px 0;background:#0e141c;border-radius:8px;border:1px solid #334}"
        "</style></head><body>"
        "<div style=\"font-size:20px;font-weight:700\">Gluko<span style=color:#1fa3ec>View</span></div>"
        "<h1>");
    page += S(Str::ClTitle);
    page += F("</h1>");
    if (ready) {
        page += F("<p class=ok>");
        page += S(Str::ClTokensOk);
        page += F("</p>");
        int slot = 0;
        if (wm.server->hasArg("slot")) slot = wm.server->arg("slot").toInt() == 1 ? 1 : 0;
        String list, err;
        if (carelinkFetchPatientList(slot, list, err)) {
            page += F("<div class=box><h2>");
            page += S(Str::ClPickPatient);
            page += F("</h2><form method=GET action=/carelink><label>");
            page += S(Str::ClForUser);
            page += F("</label><select name=slot onchange=\"this.form.submit()\">"
                      "<option value=0");
            if (slot == 0) page += F(" selected");
            page += F(">A</option><option value=1");
            if (slot == 1) page += F(" selected");
            page += F(">B</option></select></form><p class=muted>");
            page += S(Str::ClSharedHint);
            page += F("</p><form method=POST action=/carelink/patient>"
                      "<input type=hidden name=slot value=\"");
            page += String(slot);
            page += F("\">");
            String cur = (slot == 1) ? profiles[1].carelink_patient_pref : profiles[0].carelink_patient_pref;
            cur.trim();
            int from = 0;
            int n = 0;
            while (from < (int)list.length()) {
                int nl = list.indexOf('\n', from);
                if (nl < 0) nl = list.length();
                String line = list.substring(from, nl);
                int tab = line.indexOf('\t');
                String id = tab >= 0 ? line.substring(0, tab) : line;
                String name = tab >= 0 ? line.substring(tab + 1) : line;
                if (id.length()) {
                    page += F("<label class=pat><input type=radio name=patient value=\"");
                    page += diaHtmlAttr(id);
                    page += F("\"");
                    if (cur.equalsIgnoreCase(id) || (cur.length() == 0 && n == 0)) page += F(" checked");
                    page += F("> <b>");
                    page += diaHtmlAttr(name);
                    page += F("</b> <span class=muted>(");
                    page += diaHtmlAttr(id);
                    page += F(")</span></label>");
                    n++;
                }
                from = nl + 1;
            }
            page += F("<button type=submit>");
            page += S(Str::ClSavePatient);
            page += (slot == 1) ? F("B") : F("A");
            page += F("</button></form></div>");
            if (n == 0) {
                page += F("<p class=err>");
                page += S(Str::ClEmptyList);
                page += F("</p>");
            }
        } else {
            page += F("<p class=err>");
            page += S(Str::ClLoadFail);
            page += err;
            page += F("</p>");
        }
    } else {
        page += F("<p class=warn>");
        page += S(Str::ClTokensMissing);
        page += F("</p>");
    }
    if (pending) {
        page += F("<p class=warn>");
        page += S(Str::ClPendingCode);
        page += F("</p>");
    }
    page += F("<div class=box><h2>");
    page += S(Str::ClHowTitle);
    page += F("</h2><ol>");
    page += S(Str::ClHowHtml);
    page += F("</ol><p class=muted>");
    page += S(Str::ClFollowerHint);
    page += F("</p></div><div class=box><form method=POST action=/carelink/start><label>");
    page += S(Str::ClRegion);
    page += F("</label><select name=region><option value=EU selected>");
    page += S(Str::ClRegionEu);
    page += F("</option><option value=US>");
    page += S(Str::ClRegionUs);
    page += F("</option></select><label>");
    page += S(Str::ClSaveFor);
    page += F("</label><select name=slot><option value=0 selected>");
    page += S(Str::ClSlotA);
    page += F("</option><option value=1>");
    page += S(Str::ClSlotB);
    page += F("</option></select><button type=submit>");
    page += S(Str::ClBtnStart);
    page += F("</button></form></div><div class=box><form method=POST action=/carelink/finish><label>");
    page += S(Str::ClCbLabel);
    page += F("</label><textarea name=cb placeholder=\"com.medtronic.carepartner:/sso?code=...\"></textarea>"
              "<button type=submit>");
    page += S(Str::ClBtnFinish);
    page += F("</button></form></div><p><a href=/param>");
    page += S(Str::ClBackParam);
    page += F("</a> · <a href=/>");
    page += S(Str::ClMenu);
    page += F("</a></p></body></html>");
    wm.server->send(200, "text/html; charset=utf-8", page);
}

static void handleCarelinkPatient() {
    if (!wm.server) return;
    String pat = wm.server->hasArg("patient") ? wm.server->arg("patient") : "";
    int slot = wm.server->hasArg("slot") && wm.server->arg("slot").toInt() == 1 ? 1 : 0;
    pat.trim();
    String page;
    page.reserve(900);
    page += F("<!DOCTYPE html><html><head><meta charset=UTF-8><meta name=viewport "
              "content=\"width=device-width,initial-scale=1\"><title>CareLink</title>"
              "<style>body{font-family:system-ui;margin:14px;background:#0b0f14;color:#eee}"
              ".ok{color:#6d6}.err{color:#f66}a{color:#8cf}</style></head><body><h1>CareLink</h1>");
    if (pat.length() < 2) {
        page += F("<p class=err>");
        page += S(Str::ClNoPatient);
        page += F("</p><p><a href=/carelink>");
        page += S(Str::Back);
        page += F("</a></p></body></html>");
        wm.server->send(200, "text/html; charset=utf-8", page);
        return;
    }
    profiles[slot].carelink_patient_pref = pat;
    profiles[slot].dsSource = "guardian";
    // Meno z CareLink zoznamu (id\tmeno) – hneď na ciferník, bez čakania na fetch
    {
        String list = profiles[slot].carelink_patient_list;
        if (list.length() == 0 && slot == 1 && !carelinkSlotOwnsTokens(1))
            list = profiles[0].carelink_patient_list;
        String display = pat;
        int from = 0;
        while (from < (int)list.length()) {
            int nl = list.indexOf('\n', from);
            if (nl < 0) nl = list.length();
            String line = list.substring(from, nl);
            int tab = line.indexOf('\t');
            String id = tab >= 0 ? line.substring(0, tab) : line;
            String nm = tab >= 0 ? line.substring(tab + 1) : line;
            if (id.equalsIgnoreCase(pat) || nm.equalsIgnoreCase(pat)) {
                display = nm.length() ? nm : id;
                break;
            }
            from = nl + 1;
        }
        if (display.length() > 24) display.remove(24);
        profiles[slot].name = display;
    }
    if (slot == 0) {
        carelink_patient_pref = pat;
        preferences.putString("cl_pref", pat);
    }
    char key[12];
    snprintf(key, sizeof(key), "p%d_cpf", slot);
    preferences.putString(key, pat);
    diaConfigSaveProfiles();
    forceFetch = true;
    diaUiApplyDisplayMode();
    page += F("<p class=ok>");
    page += S(Str::ClPatientSaved);
    page += (slot == 1) ? F("B") : F("A");
    page += F("): <b>");
    page += profiles[slot].name;
    page += F("</b></p><p><a href=/carelink?slot=");
    page += String(slot);
    page += F(">");
    page += S(Str::ClBackCarelink);
    page += F("</a> · <a href=/param>");
    page += S(Str::ClParams);
    page += F("</a></p></body></html>");
    wm.server->send(200, "text/html; charset=utf-8", page);
}

static void handleCarelinkStart() {
    if (!wm.server) return;
    String region = wm.server->hasArg("region") ? wm.server->arg("region") : "EU";
    int slot = wm.server->hasArg("slot") && wm.server->arg("slot").toInt() == 1 ? 1 : 0;
    String authUrl, err;
    if (!carelinkBeginBrowserLogin(region, slot, authUrl, err)) {
        String page;
        page.reserve(900);
        page += F("<!DOCTYPE html><html><head><meta charset=UTF-8><meta name=viewport "
                  "content=\"width=device-width,initial-scale=1\"><title>CareLink</title>"
                  "<style>body{font-family:system-ui;margin:14px;background:#0b0f14;color:#eee}"
                  "a{color:#8cf}.err{color:#f66}</style></head><body><h1>CareLink</h1><p class=err>");
        page += err;
        page += F("</p><p><a href=/carelink>");
        page += S(Str::Back);
        page += F("</a></p></body></html>");
        wm.server->send(200, "text/html; charset=utf-8", page);
        return;
    }

    String href = authUrl;
    href.replace("&", "&amp;");
    String page;
    page.reserve(2400);
    page += F(
        "<!DOCTYPE html><html><head><meta charset=UTF-8>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<title>CareLink login</title><style>"
        "body{font-family:system-ui;margin:14px;background:#0b0f14;color:#eee;line-height:1.45}"
        ".box{background:#151c26;padding:14px;border-radius:10px;margin:12px 0}"
        "a.btn{display:block;padding:14px;background:#1fa3ec;color:#fff;border-radius:10px;"
        "text-align:center;text-decoration:none;font-size:16px;margin:10px 0}"
        "textarea{width:100%;min-height:90px;box-sizing:border-box;padding:10px;border-radius:8px;"
        "border:1px solid #334;background:#0e141c;color:#eee}"
        "button{width:100%;padding:14px;background:#2ecc71;color:#fff;border:0;border-radius:10px;font-size:16px}"
        "a{color:#8cf}.muted{color:#99a;font-size:0.92rem}"
        "</style></head><body><h1>");
    page += S(Str::ClStep1Done);
    page += F("</h1><div class=box><p>");
    page += S(Str::ClOpenLogin);
    page += F("</p><a class=btn href=\"");
    page += href;
    page += F("\">");
    page += S(Str::ClOpenBtn);
    page += F("</a><p class=muted>");
    page += S(Str::ClAfterLoginHint);
    page += F("</p></div><div class=box><form method=POST action=/carelink/finish><label>");
    page += S(Str::ClPasteLabel);
    page += F("</label><textarea name=cb></textarea>"
              "<button type=submit style=\"margin-top:10px\">");
    page += S(Str::ClBtnFinish);
    page += F("</button></form></div><p><a href=/carelink>");
    page += S(Str::Back);
    page += F("</a></p></body></html>");
    wm.server->send(200, "text/html; charset=utf-8", page);
}

static void handleCarelinkFinish() {
    if (!wm.server) return;
    String cb = wm.server->hasArg("cb") ? wm.server->arg("cb") : "";
    String err;
    bool ok = carelinkCompleteBrowserLogin(cb, err);
    String page;
    page.reserve(1000);
    page += F("<!DOCTYPE html><html><head><meta charset=UTF-8><meta name=viewport "
              "content=\"width=device-width,initial-scale=1\"><title>CareLink</title>"
              "<style>body{font-family:system-ui;margin:14px;background:#0b0f14;color:#eee}"
              ".ok{color:#6d6}.err{color:#f66}a{color:#8cf}"
              "a.btn{display:inline-block;margin-top:12px;padding:12px 16px;background:#1fa3ec;"
              "color:#fff;border-radius:8px;text-decoration:none}</style></head><body><h1>CareLink</h1>");
    if (ok) {
        const int tokSlot = carelinkPendingTokenSlot();
        profiles[tokSlot].dsSource = "guardian";
        profiles[tokSlot].carelink_patient_pref = "";
        if (tokSlot == 0) {
            if (dia_active_slot == 0) dsSource = "guardian";
            carelink_patient_pref = "";
            preferences.putString("cl_pref", "");
        }
        diaConfigSaveProfiles();
        forceFetch = true;
        page += F("<p class=ok>");
        page += S(Str::ClDoneTokens);
        page += (tokSlot == 1) ? F("B") : F("A");
        page += F(".</p><p><a class=btn href=/carelink?slot=");
        page += String(tokSlot);
        page += F(">");
        page += S(Str::ClPickPatientBtn);
        page += F("</a></p><p><a href=/>");
        page += S(Str::ClHome);
        page += F("</a> · <a href=/param>");
        page += S(Str::ClParams);
        page += F("</a></p>");
    } else {
        page += F("<p class=err>");
        page += err;
        page += F("</p><p><a href=/carelink>");
        page += S(Str::ClRetry);
        page += F("</a></p>");
    }
    page += F("</body></html>");
    wm.server->send(200, "text/html; charset=utf-8", page);
}


void diaPortalRegisterCarelinkRoutes() {
    if (!wm.server) return;
    wm.server->on("/carelink", HTTP_GET, handleCarelinkPage);
    wm.server->on("/carelink/start", HTTP_POST, handleCarelinkStart);
    wm.server->on("/carelink/finish", HTTP_POST, handleCarelinkFinish);
    wm.server->on("/carelink/patient", HTTP_POST, handleCarelinkPatient);
}
