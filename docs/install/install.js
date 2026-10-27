import { ESPLoader, Transport } from "./vendor/esptool-js-0.5.7.js?v=5";

const BOARDS = {
  solo: { vid: 0x1a86, pid: 0x55d3, name: "Solo" },
  duo: { vid: 0x303a, pid: 0x1001, name: "Duo" },
};

const statusEl = document.getElementById("status");
const barEl = document.getElementById("bar");
const logEl = document.getElementById("log");
const eraseEl = document.getElementById("erase");
const buttons = [document.getElementById("btn-solo"), document.getElementById("btn-duo")];
let busy = false;

function say(text) {
  statusEl.textContent = text;
}

function log(line) {
  const text = String(line ?? "").replace(/\u001b\[[0-9;]*m/g, "");
  if (!text.trim()) return;
  logEl.hidden = false;
  logEl.textContent += (logEl.textContent ? "\n" : "") + text.trim();
  logEl.scrollTop = logEl.scrollHeight;
}

function md5hex(bytes) {
  let buffer;
  if (typeof bytes === "string") {
    const out = new Uint8Array(bytes.length);
    for (let i = 0; i < bytes.length; i++) out[i] = bytes.charCodeAt(i) & 255;
    buffer = out.buffer;
  } else {
    buffer = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
  }
  return SparkMD5.ArrayBuffer.hash(buffer);
}

function setBusy(on) {
  busy = on;
  buttons.forEach((button) => {
    button.disabled = on;
  });
  eraseEl.disabled = on;
}

async function loadParts(boardKey) {
  const manifestUrl = new URL(`${boardKey}/manifest.json`, import.meta.url);
  const manifest = await (await fetch(manifestUrl)).json();
  const parts = [];
  for (const part of manifest.builds[0].parts) {
    const response = await fetch(new URL(part.path, manifestUrl));
    if (!response.ok) throw new Error(`Chýba súbor ${part.path}. / Missing ${part.path}.`);
    parts.push({
      data: new Uint8Array(await response.arrayBuffer()),
      address: part.offset,
    });
  }
  return parts;
}

async function showVersions() {
  for (const key of ["solo", "duo"]) {
    try {
      const manifest = await (await fetch(`${key}/manifest.json`)).json();
      document.getElementById(`ver-${key}`).textContent = manifest.version;
      document.getElementById(`ver-${key}-en`).textContent = manifest.version;
    } catch {
      document.getElementById(`ver-${key}`).textContent = "—";
      document.getElementById(`ver-${key}-en`).textContent = "—";
    }
  }
}

async function flash(boardKey) {
  if (busy) return;
  if (!navigator.serial) {
    say("Otvor túto stránku v Chrome alebo Edge na počítači. / Open this page in Chrome or Edge on a computer.");
    return;
  }
  const board = BOARDS[boardKey];
  setBusy(true);
  logEl.hidden = true;
  logEl.textContent = "";
  barEl.hidden = false;
  barEl.value = 0;
  let transport = null;
  let wrote = false;
  try {
    say("Načítavam súbory. / Loading files.");
    const parts = await loadParts(boardKey);
    const total = parts.reduce((sum, part) => sum + part.data.length, 0);
    say(`Vyber port ${board.name}. / Choose the ${board.name} port.`);
    const port = await navigator.serial.requestPort({
      filters: [{ usbVendorId: board.vid, usbProductId: board.pid }],
    });
    const info = port.getInfo();
    if (info.usbVendorId !== board.vid || info.usbProductId !== board.pid) {
      throw new Error(`Toto nie je ${board.name}. / This is not the ${board.name} board.`);
    }
    transport = new Transport(port, false);
    const loader = new ESPLoader({
      transport,
      baudrate: 115200,
      romBaudrate: 115200,
      // Predvolených 255 B na CH343 pretečie hneď po štarte stubu a zápis sa už nepohne.
      serialOptions: { bufferSize: 131072 },
      terminal: {
        clean() {},
        writeLine(line) { log(line); },
        write(line) { log(line); },
      },
    });
    say("Pripájam dosku. / Connecting.");
    const chip = await loader.main();
    if (!String(chip).includes("ESP32-S3")) {
      throw new Error("Pripojený čip nie je ESP32-S3. / The connected chip is not an ESP32-S3.");
    }
    if (eraseEl.checked) {
      say("Mažem uložené nastavenia. / Erasing saved settings.");
      await loader.eraseFlash();
    }
    say("Pripravujem zápis. Prehliadač môže chvíľu nereagovať. / Preparing the write. The browser may pause briefly.");
    log("Stub beží, začínam zápis. / Stub is running, starting the write.");
    await new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    say("Nahrávam. Neodpájaj kábel. / Flashing. Do not unplug.");
    await loader.writeFlash({
      fileArray: parts,
      flashSize: "16MB",
      flashMode: "dio",
      flashFreq: "80m",
      eraseAll: false,
      compress: true,
      reportProgress(_fileIndex, written, fileTotal) {
        const before = parts.slice(0, _fileIndex).reduce((sum, part) => sum + part.data.length, 0);
        barEl.value = Math.min(100, ((before + written) / total) * 100);
        if (fileTotal) say(`Nahrávam ${Math.round(barEl.value)} %. / Flashing ${Math.round(barEl.value)}%.`);
      },
      calculateMD5Hash: md5hex,
    });
    wrote = true;
    barEl.value = 100;
    say("Reštartujem dosku. / Restarting the board.");
    await Promise.race([
      loader.after(),
      new Promise((_, reject) => setTimeout(() => reject(new Error("reset-timeout")), 4000)),
    ]);
    transport = null;
    say(eraseEl.checked
      ? "Hotovo. Nastavenia sú zmazané, po štarte znova WiFi a súhlas. / Done. Settings were erased."
      : "Hotovo. WiFi a účet ostali. / Done. Wi-Fi and the account were kept.");
  } catch (error) {
    const message = error && error.message ? error.message : String(error);
    if (/No port selected|cancel/i.test(message)) {
      say("Zrušené. / Cancelled.");
    } else if (wrote) {
      say("Súbory sú nahraté, ale reštart zlyhal. Odpoj a znova pripoj kábel. / Files are written, but the restart failed. Unplug and reconnect.");
      log(message);
    } else {
      const hint = boardKey === "duo"
        ? " Pri Duo podrž BOOT, stlač RESET a pusti BOOT. / On Duo, hold BOOT, press RESET, release BOOT."
        : " Pri Solo skontroluj kábel a ovládač CH343. / On Solo, check the cable and the CH343 driver.";
      say(message + hint);
      log(message);
    }
    if (transport) {
      try { await transport.disconnect(); } catch { /* port už môže byť zatvorený */ }
    }
  } finally {
    setBusy(false);
  }
}

if (md5hex(new TextEncoder().encode("abc")) !== "900150983cd24fb0d6963f7d28e17f72") {
  say("Kontrola súčtu na tejto stránke nesedí. Nahrávanie je zastavené. / The checksum on this page is wrong. Flashing is stopped.");
  setBusy(true);
} else {
  document.getElementById("btn-solo").addEventListener("click", () => flash("solo"));
  document.getElementById("btn-duo").addEventListener("click", () => flash("duo"));
  showVersions();
}
