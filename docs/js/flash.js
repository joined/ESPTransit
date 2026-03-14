import { ESPLoader, Transport } from "./esptooljs-bundle.f55a63a.js";

const FLASH_FILES = [
  { offset: 0x2000, name: "bootloader.bin" },
  { offset: 0x8000, name: "partition-table.bin" },
  { offset: 0x10000, name: "esptransit.bin" },
];

const BAUD_RATE = 460800;

// DOM elements
const releaseSelect = document.getElementById("release-select");
const boardSelect = document.getElementById("board-select");
const connectBtn = document.getElementById("connect-btn");
const flashBtn = document.getElementById("flash-btn");
const progressSection = document.getElementById("flash-progress");
const progressBar = document.getElementById("progress-bar");
const progressLabel = document.getElementById("progress-label");
const progressPercent = document.getElementById("progress-percent");
const logContainer = document.getElementById("log-container");
const flashLog = document.getElementById("flash-log");
const logClearBtn = document.getElementById("log-clear-btn");

let transport = null;
let esploader = null;
let connected = false;

// --- Logging ---

function log(msg) {
  logContainer.hidden = false;
  flashLog.textContent += msg + "\n";
  flashLog.scrollTop = flashLog.scrollHeight;
}

logClearBtn?.addEventListener("click", () => {
  flashLog.textContent = "";
});

// --- Progress ---

function setProgress(label, pct) {
  progressSection.hidden = false;
  progressLabel.textContent = label;
  progressBar.style.width = pct + "%";
  progressPercent.textContent = Math.round(pct) + "%";
}

// --- Release loading ---

async function loadReleases() {
  try {
    const base = document.querySelector('link[rel="canonical"]')?.href
      ? new URL(".", document.querySelector('link[rel="canonical"]').href).href
      : "";
    const resp = await fetch(base + "firmware/index.json");
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
    const data = await resp.json();

    releaseSelect.innerHTML = "";
    if (data.releases.length === 0) {
      releaseSelect.innerHTML =
        '<option value="">No releases available</option>';
      return;
    }
    for (const rel of data.releases) {
      const opt = document.createElement("option");
      opt.value = rel.tag;
      opt.textContent = rel.tag;
      releaseSelect.appendChild(opt);
    }
    releaseSelect.disabled = false;
    boardSelect.disabled = false;
    updateConnectButton();
  } catch (e) {
    releaseSelect.innerHTML =
      '<option value="">Failed to load releases</option>';
    log("Error loading releases: " + e.message);
  }
}

// --- WebSerial connection ---

function updateConnectButton() {
  if (!("serial" in navigator)) {
    connectBtn.textContent = "WebSerial not supported";
    connectBtn.disabled = true;
    return;
  }
  connectBtn.disabled = !releaseSelect.value;
  connectBtn.textContent = connected ? "Disconnect" : "Connect";
}

connectBtn.addEventListener("click", async () => {
  if (connected) {
    await disconnect();
    return;
  }
  await connect();
});

async function connect() {
  try {
    connectBtn.disabled = true;
    connectBtn.textContent = "Connecting...";
    log("Requesting serial port...");

    const port = await navigator.serial.requestPort();
    transport = new Transport(port, true);

    log("Connecting to ESP32-P4...");
    esploader = new ESPLoader({
      transport,
      baudrate: BAUD_RATE,
      romBaudrate: 115200,
      terminal: { clean: () => {}, writeLine: (msg) => log(msg), write: () => {} },
    });

    const chip = await esploader.main();
    log(`Connected: ${chip}`);

    connected = true;
    flashBtn.disabled = false;
    updateConnectButton();
  } catch (e) {
    log("Connection failed: " + e.message);
    await disconnect();
  }
}

async function disconnect() {
  try {
    if (transport) {
      await transport.disconnect();
    }
  } catch (_) {
    // ignore disconnect errors
  }
  transport = null;
  esploader = null;
  connected = false;
  flashBtn.disabled = true;
  updateConnectButton();
}

// --- Flashing ---

flashBtn.addEventListener("click", async () => {
  const tag = releaseSelect.value;
  const board = boardSelect.value;
  if (!tag || !board || !esploader) return;

  flashBtn.disabled = true;
  connectBtn.disabled = true;

  try {
    const basePath = `firmware/${tag}/${board}/`;
    const fileArray = [];

    // Download all firmware files
    for (let i = 0; i < FLASH_FILES.length; i++) {
      const file = FLASH_FILES[i];
      setProgress(`Downloading ${file.name}...`, (i / FLASH_FILES.length) * 30);
      log(`Downloading ${basePath}${file.name}...`);

      const resp = await fetch(basePath + file.name);
      if (!resp.ok)
        throw new Error(`Failed to download ${file.name}: HTTP ${resp.status}`);

      const data = await resp.arrayBuffer();
      log(`  ${file.name}: ${data.byteLength} bytes`);

      fileArray.push({
        data: new Uint8Array(data),
        address: file.offset,
      });
    }

    setProgress("Flashing...", 30);
    log("Starting flash...");

    const flashOptions = {
      fileArray,
      flashSize: "16MB",
      flashMode: "dio",
      flashFreq: "80m",
      eraseAll: false,
      compress: true,
      reportProgress: (_fileIndex, written, total) => {
        const filePct = (written / total) * 100;
        const overallPct = 30 + (filePct / 100) * 70;
        setProgress("Flashing...", overallPct);
      },
    };

    await esploader.writeFlash(flashOptions);

    setProgress("Done!", 100);
    log("Flash complete! You can now disconnect and reset the device.");
  } catch (e) {
    log("Flash failed: " + e.message);
    setProgress("Failed", 0);
  } finally {
    flashBtn.disabled = false;
    connectBtn.disabled = false;
  }
});

// --- Init ---
loadReleases();
