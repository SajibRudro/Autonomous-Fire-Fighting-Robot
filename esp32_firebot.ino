#include <WiFi.h>
#include <WebServer.h>

// ===== WiFi Access Point =====
// Phone থেকে "FireBot" নেটওয়ার্কে connect করো
// তারপর browser এ যাও: 192.168.4.1
const char* ssid     = "FireBot";
const char* password = "12345678";

WebServer server(80);

// ===== Arduino Serial2 =====
// GPIO17 (TX2) → Arduino Pin 0 (RX)
// GPIO16 (RX2) → খালি, connect করবে না
#define arduinoSerial Serial2

// ===== Flame sensor pins (ESP32 নিজেও দেখতে পারে) =====
#define FLAME_L 34
#define FLAME_C 35
#define FLAME_R 32

void setup() {
  Serial.begin(115200); // PC debugging এর জন্য

  // Arduino এর সাথে Serial2
  // RX=GPIO16 (unused), TX=GPIO17 → Arduino Pin 0
  arduinoSerial.begin(9600, SERIAL_8N1, 16, 17);

  // ===== WiFi AP mode =====
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP()); // 192.168.4.1

  // ===== Web routes =====
  server.on("/",         handleRoot);
  server.on("/forward",  []() { sendCmd("FORWARD");    server.send(200, "text/plain", "OK"); });
  server.on("/backward", []() { sendCmd("BACKWARD");   server.send(200, "text/plain", "OK"); });
  server.on("/left",     []() { sendCmd("TURN_LEFT");  server.send(200, "text/plain", "OK"); });
  server.on("/right",    []() { sendCmd("TURN_RIGHT"); server.send(200, "text/plain", "OK"); });
  server.on("/stop",     []() { sendCmd("STOP");       server.send(200, "text/plain", "OK"); });
  server.on("/pump",     []() { sendCmd("PUMP_ON");    server.send(200, "text/plain", "OK"); });
  server.on("/pumpoff",  []() { sendCmd("PUMP_OFF");   server.send(200, "text/plain", "OK"); });
  server.on("/auto",     []() { sendCmd("AUTO");       server.send(200, "text/plain", "OK"); });
  server.on("/sensors",  handleSensors);

  server.begin();
  Serial.println("Web server started!");
  Serial.println("Connect to WiFi: FireBot");
  Serial.println("Then open: http://192.168.4.1");
}

void loop() {
  server.handleClient();
}

// ===== Arduino তে command পাঠাও =====
void sendCmd(String cmd) {
  arduinoSerial.println(cmd);
  Serial.println("Sent: " + cmd); // debugging
}

// ===== Sensor data JSON =====
void handleSensors() {
  int L = analogRead(FLAME_L);
  int C = analogRead(FLAME_C);
  int R = analogRead(FLAME_R);

  // কম মানে বেশি আগুন (IR sensor)
  bool fireL = (L < 800);
  bool fireC = (C < 800);
  bool fireR = (R < 800);

  String json = "{";
  json += "\"L\":" + String(L) + ",";
  json += "\"C\":" + String(C) + ",";
  json += "\"R\":" + String(R) + ",";
  json += "\"fireL\":" + String(fireL ? "true" : "false") + ",";
  json += "\"fireC\":" + String(fireC ? "true" : "false") + ",";
  json += "\"fireR\":" + String(fireR ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}

// ===== Web Control Panel =====
void handleRoot() {
  String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1">
<title>FireBot Control</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: #0a0a0a;
    color: #eee;
    font-family: sans-serif;
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 20px;
    min-height: 100vh;
  }
  h2 { color: #ff6b35; font-size: 22px; margin-bottom: 6px; }
  .subtitle { color: #888; font-size: 13px; margin-bottom: 14px; }

  /* Mode indicator */
  .mode-bar {
    display: flex;
    gap: 10px;
    margin-bottom: 16px;
    width: 100%;
    max-width: 320px;
  }
  .mode-btn {
    flex: 1;
    height: 44px;
    border-radius: 10px;
    border: 2px solid #333;
    background: #1e1e1e;
    color: #888;
    font-size: 14px;
    font-weight: bold;
    cursor: pointer;
    transition: all 0.2s;
    -webkit-tap-highlight-color: transparent;
  }
  .mode-btn.active-auto {
    background: #0f2d3d;
    border-color: #3498db;
    color: #3498db;
  }
  .mode-btn.active-manual {
    background: #2d1f0f;
    border-color: #e67e22;
    color: #e67e22;
  }
  .mode-btn:active { transform: scale(0.96); }

  /* Mode label */
  .mode-label {
    font-size: 12px;
    color: #555;
    margin-bottom: 14px;
  }
  .mode-label.auto  { color: #3498db; }
  .mode-label.manual { color: #e67e22; }

  /* Manual controls — disabled when auto */
  .manual-section {
    opacity: 1;
    transition: opacity 0.3s;
    display: flex;
    flex-direction: column;
    align-items: center;
  }
  .manual-section.disabled {
    opacity: 0.35;
    pointer-events: none;
  }

  .sensor-bar {
    display: flex;
    gap: 10px;
    margin-bottom: 20px;
    width: 100%;
    max-width: 320px;
  }
  .sensor {
    flex: 1;
    background: #1a1a1a;
    border: 1px solid #333;
    border-radius: 10px;
    padding: 10px;
    text-align: center;
    font-size: 12px;
    color: #888;
    transition: all 0.3s;
  }
  .sensor.fire {
    background: #3a1a00;
    border-color: #ff6b35;
    color: #ff6b35;
  }
  .sensor-val { font-size: 18px; font-weight: bold; margin-bottom: 2px; }

  .controls {
    display: grid;
    grid-template-columns: repeat(3, 90px);
    grid-template-rows: repeat(3, 80px);
    gap: 10px;
    margin-bottom: 16px;
  }
  .btn {
    background: #1e1e1e;
    border: 2px solid #333;
    border-radius: 14px;
    color: #fff;
    font-size: 13px;
    font-weight: bold;
    cursor: pointer;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 4px;
    transition: all 0.15s;
    user-select: none;
    -webkit-tap-highlight-color: transparent;
  }
  .btn:active { transform: scale(0.93); }
  .btn .icon { font-size: 24px; }
  .btn.fwd  { grid-column: 2; grid-row: 1; border-color: #2ecc71; }
  .btn.fwd:active  { background: #1a3d2b; }
  .btn.left { grid-column: 1; grid-row: 2; border-color: #3498db; }
  .btn.left:active { background: #1a2a3d; }
  .btn.stop { grid-column: 2; grid-row: 2; background: #c0392b; border-color: #e74c3c; font-size: 15px; }
  .btn.stop:active { background: #922b21; }
  .btn.right{ grid-column: 3; grid-row: 2; border-color: #3498db; }
  .btn.right:active{ background: #1a2a3d; }
  .btn.back { grid-column: 2; grid-row: 3; border-color: #e67e22; }
  .btn.back:active { background: #3d2a1a; }

  .pump-row {
    display: flex;
    gap: 10px;
    width: 280px;
    margin-bottom: 4px;
  }
  .pump-btn {
    flex: 1;
    height: 50px;
    background: #0f3d22;
    border: 2px solid #27ae60;
    border-radius: 14px;
    color: #2ecc71;
    font-size: 14px;
    font-weight: bold;
    cursor: pointer;
    transition: all 0.15s;
    -webkit-tap-highlight-color: transparent;
  }
  .pump-btn:active { background: #0a2a17; transform: scale(0.97); }
  .pump-off-btn {
    flex: 1;
    height: 50px;
    background: #3d0f0f;
    border: 2px solid #c0392b;
    border-radius: 14px;
    color: #e74c3c;
    font-size: 14px;
    font-weight: bold;
    cursor: pointer;
    transition: all 0.15s;
    -webkit-tap-highlight-color: transparent;
  }
  .pump-off-btn:active { background: #2a0a0a; transform: scale(0.97); }

  .status {
    margin-top: 14px;
    font-size: 12px;
    color: #555;
  }
  .status.ok  { color: #27ae60; }
  .status.err { color: #e74c3c; }
</style>
</head>
<body>

<h2>🔥 FireBot</h2>
<p class="subtitle">192.168.4.1</p>

<!-- Mode selector -->
<div class="mode-bar">
  <button class="mode-btn" id="btnAuto" onclick="setMode('auto')">🤖 AUTO</button>
  <button class="mode-btn" id="btnManual" onclick="setMode('manual')">🕹 MANUAL</button>
</div>
<div class="mode-label" id="modeLabel">Mode: AUTO</div>

<!-- Sensor bar -->
<div class="sensor-bar">
  <div class="sensor" id="sL"><div class="sensor-val" id="vL">---</div>LEFT</div>
  <div class="sensor" id="sC"><div class="sensor-val" id="vC">---</div>CENTER</div>
  <div class="sensor" id="sR"><div class="sensor-val" id="vR">---</div>RIGHT</div>
</div>

<!-- Manual controls -->
<div class="manual-section disabled" id="manualSection">
  <div class="controls">
    <div class="btn fwd"   ontouchstart="cmd('forward')"  onclick="cmd('forward')">
      <span class="icon">▲</span>Forward
    </div>
    <div class="btn left"  ontouchstart="cmd('left')"     onclick="cmd('left')">
      <span class="icon">◀</span>Left
    </div>
    <div class="btn stop"  ontouchstart="cmd('stop')"     onclick="cmd('stop')">
      ⏹ STOP
    </div>
    <div class="btn right" ontouchstart="cmd('right')"    onclick="cmd('right')">
      <span class="icon">▶</span>Right
    </div>
    <div class="btn back"  ontouchstart="cmd('backward')" onclick="cmd('backward')">
      <span class="icon">▼</span>Back
    </div>
  </div>

  <div class="pump-row">
    <button class="pump-btn"     ontouchstart="cmd('pump')"    onclick="cmd('pump')">💧 Pump ON</button>
    <button class="pump-off-btn" ontouchstart="cmd('pumpoff')" onclick="cmd('pumpoff')">🚫 Pump OFF</button>
  </div>
</div>

<div class="status" id="status">AUTO mode active</div>

<script>
  let currentMode = 'auto';

  function setMode(mode) {
    currentMode = mode;
    const manualSection = document.getElementById('manualSection');
    const btnAuto   = document.getElementById('btnAuto');
    const btnManual = document.getElementById('btnManual');
    const modeLabel = document.getElementById('modeLabel');

    if (mode === 'auto') {
      fetch('/auto').catch(() => {});
      manualSection.classList.add('disabled');
      btnAuto.className   = 'mode-btn active-auto';
      btnManual.className = 'mode-btn';
      modeLabel.className = 'mode-label auto';
      modeLabel.textContent = 'Mode: AUTO — Robot searching for fire';
      document.getElementById('status').textContent = 'AUTO mode active';
      document.getElementById('status').className = 'status ok';
    } else {
      fetch('/stop').catch(() => {});
      manualSection.classList.remove('disabled');
      btnAuto.className   = 'mode-btn';
      btnManual.className = 'mode-btn active-manual';
      modeLabel.className = 'mode-label manual';
      modeLabel.textContent = 'Mode: MANUAL — You are in control';
      document.getElementById('status').textContent = 'MANUAL mode active';
      document.getElementById('status').className = 'status ok';
    }
  }

  function cmd(action) {
    fetch('/' + action)
      .then(() => {
        document.getElementById('status').textContent = 'Sent: ' + action.toUpperCase();
        document.getElementById('status').className = 'status ok';
      })
      .catch(() => {
        document.getElementById('status').textContent = 'Connection error!';
        document.getElementById('status').className = 'status err';
      });
  }

  // Default: AUTO mode on load
  setMode('auto');

  // Sensor update every 800ms
  setInterval(() => {
    fetch('/sensors')
      .then(r => r.json())
      .then(d => {
        document.getElementById('vL').textContent = d.L;
        document.getElementById('vC').textContent = d.C;
        document.getElementById('vR').textContent = d.R;
        document.getElementById('sL').className = 'sensor' + (d.fireL ? ' fire' : '');
        document.getElementById('sC').className = 'sensor' + (d.fireC ? ' fire' : '');
        document.getElementById('sR').className = 'sensor' + (d.fireR ? ' fire' : '');
      })
      .catch(() => {});
  }, 800);
</script>

</body>
</html>
)rawhtml";

  server.send(200, "text/html", html);
}
