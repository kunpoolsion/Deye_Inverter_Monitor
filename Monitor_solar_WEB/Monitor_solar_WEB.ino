#include <WiFi.h>
#include <ESPmDNS.h>
#include <time.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "SolarmanV5.h"
#include "DeyeInverter.h"

// CONFIGURACIÓN
const char* ssid = "wifissid"; // SSID de la wifi
const char* password = "wifipass"; // Pass de la wifi
const unsigned long update_interval = 10; // Frecuencia de actualizacion en segundos
const char* datalogger_ip = "192.168.1.10"; // IP del datalogger Solarman
uint32_t datalogger_sn = 1234567890; // Número de serie del Solarman

// === WEB
WebServer server(80);
SolarmanV5 *solarman = nullptr;
DeyeInverter *inverter = nullptr;
InverterData inv_data;

void connectWiFi() {
  WiFi.setHostname("monitor_solar");
  WiFi.begin(ssid, password);
  Serial.print("📶 Conectando a WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Conectado a WiFi!");
    Serial.print("📡 IP del ESP32: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ No se pudo conectar a WiFi");
  }
}

void initializeInverter() {
  if (solarman) delete solarman;
  if (inverter) delete inverter;
  solarman = new SolarmanV5(datalogger_ip, datalogger_sn);
  inverter = new DeyeInverter(solarman);
  solarman->begin();
  Serial.println("🔌 Comunicación con inversor inicializada");
  Serial.printf("   IP: %s\n", datalogger_ip);
  Serial.printf("   SN: %lu\n", datalogger_sn);
}

void readInverterData() {
  Serial.println("\n🔄 Actualizando datos del inversor...");
  if (inverter && inverter->readAllData(&inv_data)) {
    Serial.println("✅ Datos leídos correctamente");
    printInverterData();
  } else {
    Serial.println("❌ Error leyendo datos del inversor");
    inv_data.data_valid = false;
  }
}

void printInverterData() {
  if (!inv_data.data_valid) return;
  Serial.println("\n=== DATOS DEL INVERSOR ===");
  Serial.printf("🕒 Timestamp: %lu\n", inv_data.timestamp);
  Serial.println("\n🌞 SOLAR:");
  Serial.printf("   PV1: %.1fV, %.1fA, %.0fW\n", inv_data.pv1_voltage, inv_data.pv1_current, inv_data.pv1_power);
  Serial.printf("   PV2: %.1fV, %.1fA, %.0fW\n", inv_data.pv2_voltage, inv_data.pv2_current, inv_data.pv2_power);
  Serial.printf("   Producción diaria: %.1f kWh\n", inv_data.daily_production);
  Serial.println("\n🔋 BATERÍA:");
  Serial.printf("   SOC: %.0f%%, %.2fV, %.2fA, %.0fW\n", inv_data.battery_soc, inv_data.battery_voltage,
                inv_data.battery_current, inv_data.battery_power);
  Serial.printf("   Estado: %s, Temp: %.1f°C\n", inv_data.battery_status.c_str(), inv_data.battery_temperature);
  Serial.println("\n⚡ RED:");
  Serial.printf("   Potencia: %.0fW\n", inv_data.grid_power);
  Serial.printf("   Voltaje L1: %.1fV\n", inv_data.grid_voltage_l1);
  Serial.printf("   Corriente L1: %.2fA\n", inv_data.grid_current_l1);
  Serial.printf("   Frecuencia: %.2f Hz\n", inv_data.grid_frequency);
  Serial.printf("   Energía comprada hoy: %.1f kWh\n", inv_data.daily_energy_bought);
  Serial.printf("   Energía vendida hoy: %.1f kWh\n", inv_data.daily_energy_sold);
  Serial.println("\n🏠 CARGA:");
  Serial.printf("   Total: %.0fW, L1: %.0fW\n", inv_data.load_power, inv_data.load_l1_power);
  Serial.printf("   Consumo hoy: %.1f kWh\n", inv_data.daily_load_consumption);
  Serial.printf("   Estado inversor: %s\n", inv_data.running_status.c_str());
  Serial.printf("   Modo trabajo: %s\n", inv_data.work_mode.c_str());
  Serial.printf("   Temp inversor: %.1f°C\n", inv_data.inverter_temperature);
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/update", handleUpdate);
  server.on("/status", handleStatus);
  server.on("/reboot", handleReboot);
  server.begin();
  Serial.println("🌐 Servidor web iniciado en http://" + WiFi.localIP().toString());
}

void handleRoot() {
  server.send(200, "text/html", getWebInterface());
}

void handleData() {
  DynamicJsonDocument doc(2048);
  if (inv_data.data_valid) {
    doc["status"] = "success";
    doc["timestamp"] = inv_data.timestamp;

    // --- Campos requeridos por la interfaz web ---
    float solar_total = inv_data.pv1_power + inv_data.pv2_power;
    doc["solar"] = solar_total;
    doc["home"] = inv_data.load_power;
    doc["grid"] = inv_data.grid_power;
    doc["daily_bought"] = inv_data.daily_energy_bought;
    doc["daily_load"] = inv_data.daily_load_consumption;
    doc["daily_production"] = inv_data.daily_production;
    doc["pv1"] = inv_data.pv1_power;
    doc["pv2"] = inv_data.pv2_power;
    doc["bat_power"] = inv_data.battery_power;
    doc["soc"] = inv_data.battery_soc;
    doc["bat_temp"] = inv_data.battery_temperature;
    doc["inv_temp"] = inv_data.inverter_temperature;
    doc["pv1_voltage"] = inv_data.pv1_voltage;
    doc["pv1_current"] = inv_data.pv1_current;
    doc["pv2_voltage"] = inv_data.pv2_voltage;
    doc["pv2_current"] = inv_data.pv2_current;
    doc["battery_voltage"] = inv_data.battery_voltage;
    doc["battery_current"] = inv_data.battery_current;
    doc["battery_status"] = inv_data.battery_status;
    doc["grid_voltage_l1"] = inv_data.grid_voltage_l1;
    doc["grid_current_l1"] = inv_data.grid_current_l1;
    doc["grid_frequency"] = inv_data.grid_frequency;
    doc["daily_energy_sold"] = inv_data.daily_energy_sold;
    doc["load_l1_power"] = inv_data.load_l1_power;
    doc["running_status"] = inv_data.running_status;
    doc["work_mode"] = inv_data.work_mode;
  } else {
    doc["status"] = "error";
    doc["message"] = "Datos no disponibles";
  }
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleUpdate() {
  readInverterData();
  server.send(200, "application/json", "{\"status\":\"updated\"}");
}

void handleStatus() {
  DynamicJsonDocument doc(512);
  doc["status"] = "online";
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["datalogger_ip"] = datalogger_ip;
  doc["datalogger_sn"] = datalogger_sn;
  doc["data_valid"] = inv_data.data_valid;
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleReboot() {
  server.send(200, "text/html", "<html><body><h1>Reiniciando ESP32...</h1></body></html>");
  delay(1000);
  ESP.restart();
}

String getWebInterface() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Monitor Solar</title>
    <style>
        @font-face {
            font-family: 'Montserrat';
            font-weight: 400;
            font-style: normal;
            font-display: swap;
            src: url(data:font/woff2;base64,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fnh4eHm5ub66urq8vLi/OLi/OLi/OLi4vzy6uLi6vrq7u7+8fHp+fn59fX17e39/f3j8+Pz8+vz6+vr6+Pj4/39/e3t7fX19eXl5fnp6fHh/u7u9vr6+uri4uLs7Ozs9PT05Pj4+Ojo6ODg4Oho/+Dw8PDw8Ojo6OT05PT09Ozs7Pz8/OLi4vLy8urq+vr65ubm5vb29u7u7v7+4eHh8fHh4eH+/u7u7vb29vb25ub6+vr66vr66urq6vLq6urq6vLi4uLs/PL04vLi4vz07PL84vry6ubi7ubu8eHp5en59fXt/ePj6/sboO7u7m5vbq6urm5vrm9vrm9vb+/v7x8eHh8eHh4fHx8eH+/v7+/v7+7u7u7ubm9vb66vr6+vL6+uLi8vLi8vLi4uLs7Pz8/OTk9PT05Ojo+PDg/3D/Z39/b29nZ3dnZ2tra2Nzc3NzfXN7bWNza3Nre2trd3dvf2Dw4Oj46OT07Pzi8urm9u7+8fnl5e397ePz+/vj5/vr6/v7+9vb29vr29vr29vb2/vn5+fn5+fX59f3x/vr29v759fn5+fn5/fr6/vr6+vr6+vr2+vr29vL+/v729PT08Pd0/3N3d39/f3t/f3d3d3d3d39/f3T08PD0/Pz+/vHx+fn99fX9/f3z8/Pz+D9/f318/Pz8/3j8+vr8/Pz8+Pj8+P9/f397e3t7e3t7fXt9eXd7enl5enx8fHh8eH+/v7+7vb+7u729u7u9u7h7uHh4enp6eXl5dXV1c3t3cPD48vL6/voL29f3x9f//6+f3z8/v79/fv379+/wJ+fv78/fv38/Hx/vH+/v729vb68vLy9PT08PDw8PDw8PDw8PDw+Pj4+Pj4+Pj4+Pj4+PDw9PT09PT0/Pz8/Pz8/Pz8/Pz8/Pz8/Pz8/P///wAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD//0oAAAD///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////yH5BAEAAAEALAAAAAAQABAAAAj/AAEIHEjwIA8ePXr46NGDB48cOXLgwJEDx40bN27YsGHDhg0bNWrUqFGjRo0aNGjQkCFDhg0bN27gyJFDx44dPXr48PEDyJAiR5IsKVKkR5QpVapUuZLly5UwY8qMKXMmzZU2ceq0yXMnT549ffr8CTQo0KFECR49mjSp0qVMmzqFKXMq1apWr2LVylUmzZpZs2rl2tXrV7BdwdK0mXMszZs5c+rs6ZPsTrI81ZJl23YtwrVv57qFGzeu3Ll059LFmxevXr599fbl6/ev4L6F/wIeXNgw4sSI7SJuvHix5MuYL1e+jDlzZ82bOXfu7PmzaNGhSY8uTdp0adN4U6tuvbr1a9euY8uGXft27tu7e/fm/ds3cODChQ8nXvw48uTGlytXznz5c+fRn0uXXj369OrYtW/n3t37d/Dhw4snX948efPo16tn3969+/fw38eXP36+fPr27ePPv7/7/v7///sPQMAABFDBBReEEMIIJYwQQgoqvNBCCzHMcEMLNeQwQw497DBEEUcs8UQUUzzRRBRXNDFGFGdU0UYVcWzRRh1z7DFHIX8kskgjjUTSSCOXRNJJJp+EMsopn5QySip9vDLHLXf0sksvwQxTTDHJNHNMNNFMU8012WzTzTfhjFNOOeekM04677QTTz315NPPPAENlE9B/yx0UEILNfRQRBNNVNFEGW3U0UYffdRSSy/FlFJNLd0U00017ZRTTz0FNVRRRR2V1FJNPfXUVElV9VRVU2XV1VZfhTVWWWe9FVZZc53V1lx15dVXYIMldlhii0W2WGSTVXZZZpt19llnoZV2WmqntfZabLHFlttrs9W2W2295fZbbsH91ttwwR2X3HLNPfdcdNU9V11112X3XXbjfVfefN3NV95+7d33Xn7v9fdfgPcFON+ABSa4YIIPRjhheg9WOF+GF35Y4YgjjnhhgCmmuOKHLcY444o1vjhjjjX2OGOQPx5Z5JJBPplkkE8OWWSUS1655JVZRrlml1t+GWaZZaZ5ZpttxjlnnHfWeWeeefZ5Z6B9Fjpon4suGuikjS76aKWTTjrpppt+Guqnn446aqmrprpqqa3GOmqtq9Y6a6617nrrr78Ge2yxxya7bLLPNlvtstM+22y2zU7bbLPdPhtuuOV+O2645467brrtttvuuu+2G2+79c5777v5vnvvvv3W2+/A9wYccMIBR5xwwQ0nnHDEC0/ccMYNd9xxyCFnnHLJHbec8sgt19xzzTnnPHPQPef8c9FFJ3100k0v/fTTUU/9dNVVJ531112PHfbZaZ+99tlvt7123HPXfXfde+e9d99/B1544IMXnnjijUe++OSTV5555pl/3nnmm3/e+eifh3566aunXnrqp7e+euyr1/567bHXnvrtte+ee+69//778MEPX/jxyS/f/PTRV1999dl33/324X8//vjln39++uO3//7678c//9z7359///v3H0ABDvCACExgAheowAUykIEMbKADHwhBCEZQghOkIAUtOEEKVtCCFcRgBTNoQQ1eEIMX1KAGN8jBDXbwoAEBADs=) format('woff2');
        }
        body {
            background: #0A0A0A;
            color: white;
            font-family: 'Montserrat', sans-serif;
            margin: 0;
            padding: 10px;
        }
        .header {
            text-align: center;
            margin-bottom: 15px;
            font-size: 1.2em;
            color: #AAAAAA;
        }
        .grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
        }
        .card {
            background: #1A1A1A;
            border-radius: 20px;
            padding: 15px;
            position: relative;
            text-align: center;
        }
        .title {
            position: absolute;
            top: 10px;
            left: 12px;
            font-size: 16px;
            font-weight: 600;
        }
        .arc-container {
            height: 120px;
            display: flex;
            align-items: center;
            justify-content: center;
            margin: 10px 0;
        }
        .value-container {
            margin-top: -30px;
        }
        .value {
            font-size: 32px;
            margin: 0;
        }
        .unit {
            font-size: 20px;
            color: #AAAAAA;
        }
        .detail {
            font-size: 14px;
            color: #AAAAAA;
            margin-top: 8px;
        }
        .danger { color: #FF6666; }
        .success { color: #39FF14; }
        .warn { color: #FFAA00; }
    </style>
</head>
<body>
    <div style="text-align:center; margin-bottom:10px; font-size:1.1em; color:#AAAAAA;">
        <div id="datetime">--/--/---- --:--</div><div id="inverter_temp">Inv: --°C</div>
    </div>
    <div class="grid">
        <div class="card">
            <div class="title">SOLAR</div>
            <div class="arc-container">
                <svg viewBox="0 0 120 120" width="120" height="120">
                    <circle cx="60" cy="60" r="50" fill="none" stroke="#222" stroke-width="10"/>
                    <path id="solar-arc" d="" fill="none" stroke="#00AFFF" stroke-width="10" stroke-linecap="round"/>
                </svg>
            </div>
            <div class="value-container">
                <div class="value" id="solar">0.00</div>
                <div class="unit">kW</div>
            </div>
            <div class="detail" id="pv1pv2">0W y 0W - Hoy: 0.00 kWh</div>
        </div>
        <div class="card">
            <div class="title">RED</div>
            <div class="arc-container">
                <svg viewBox="0 0 120 120" width="120" height="120">
                    <circle cx="60" cy="60" r="50" fill="none" stroke="#222" stroke-width="10"/>
                    <path id="grid-arc" d="" fill="none" stroke="#FFAA00" stroke-width="10" stroke-linecap="round"/>
                </svg>
            </div>
            <div class="value-container">
                <div class="value" id="grid">0.00</div>
                <div class="unit">kW</div>
            </div>
            <div class="detail" id="bought">Hoy: 0.00 kWh</div>
        </div>
        <div class="card">
            <div class="title">BATERÍA</div>
            <div class="arc-container">
                <svg viewBox="0 0 120 120" width="120" height="120">
                    <circle cx="60" cy="60" r="50" fill="none" stroke="#222" stroke-width="10"/>
                    <path id="bat-arc" d="" fill="none" stroke="#39FF14" stroke-width="10" stroke-linecap="round"/>
                </svg>
            </div>
            <div class="value-container">
                <div class="value" id="soc">0</div>
                <div class="unit">%</div>
            </div>
            <div class="detail" id="batpower">0W</div><div class="detail" id="bat_temp">--°C</div>
        </div>
        <div class="card">
            <div class="title">CASA</div>
            <div class="arc-container">
                <svg viewBox="0 0 120 120" width="120" height="120">
                    <circle cx="60" cy="60" r="50" fill="none" stroke="#222" stroke-width="10"/>
                    <path id="home-arc" d="" fill="none" stroke="white" stroke-width="10" stroke-linecap="round"/>
                </svg>
            </div>
            <div class="value-container">
                <div class="value" id="home">0.00</div>
                <div class="unit">kW</div>
            </div>
            <div class="detail" id="load">Hoy: 0.00 kWh</div>
        </div>
    </div>
    <div style="text-align:center; margin-top:20px;">
        <button onclick="fetch('/reset', {method:'POST'});"
                style="background:#FF3333; color:white; border:none; padding:12px 30px; font-size:18px; border-radius:8px; cursor:pointer;">
            Reiniciar dispositivo
        </button>
    </div>
    <script>
        function drawArc(id, value, max) {
            const radius = 50;
            const centerX = 60;
            const centerY = 60;
            const startAngle = -120;
            const angleRange = 240;
            const angle = startAngle + (value / max) * angleRange;
            const start = angleToCoord(centerX, centerY, radius, startAngle);
            const end = angleToCoord(centerX, centerY, radius, angle);
            const largeArc = (angle - startAngle) <= 180 ? "0" : "1";
            const d = `M ${start.x} ${start.y} A ${radius} ${radius} 0 ${largeArc} 1 ${end.x} ${end.y}`;
            document.getElementById(id).setAttribute("d", d);
        }
        function angleToCoord(cx, cy, r, angleDeg) {
            const angleRad = (angleDeg - 90) * Math.PI / 180;
            return {
                x: cx + r * Math.cos(angleRad),
                y: cy + r * Math.sin(angleRad)
            };
        }
        function updateColors() {
            fetch('/data')
                .then(r => r.json())
                .then(d => {
                    drawArc('solar-arc', d.solar, 6000);
                    document.getElementById('solar').textContent = (d.solar / 1000).toFixed(2);
                    const absGrid = Math.abs(d.grid);
                    drawArc('grid-arc', absGrid, 6000);
                    document.getElementById('grid').textContent = (d.grid / 1000).toFixed(2);
                    const gridPath = document.getElementById('grid-arc');
                    gridPath.setAttribute('stroke', d.grid > 0 ? '#FF6666' : '#39FF14');
                    document.getElementById('grid').className = d.grid > 0 ? 'value danger' : 'value success';
                    document.getElementById('bought').textContent = 'Hoy: ' + d.daily_bought.toFixed(2) + ' kWh';
                    document.getElementById('bought').className = 'detail ' + (d.daily_bought > 0 ? 'danger' : '');
                    drawArc('bat-arc', d.soc, 100);
                    document.getElementById('soc').textContent = d.soc;
                    const batPath = document.getElementById('bat-arc');
                    if (d.soc <= 30) batPath.setAttribute('stroke', '#FF6666');
                    else if (d.soc <= 70) batPath.setAttribute('stroke', '#FFAA00');
                    else batPath.setAttribute('stroke', '#39FF14');
                    drawArc('home-arc', d.home, 6000);
                    document.getElementById('home').textContent = (d.home / 1000).toFixed(2);
                    document.getElementById('pv1pv2').textContent = 
                        d.pv1 + 'W y ' + d.pv2 + 'W - Hoy: ' + d.daily_production.toFixed(2) + ' kWh';
                    const batPowerEl = document.getElementById('batpower');
                    batPowerEl.textContent = d.bat_power + 'W';
                    batPowerEl.className = 'detail ' + (d.bat_power < 0 ? 'success' : 'danger');
                    document.getElementById('load').textContent = 'Hoy: ' + d.daily_load.toFixed(2) + ' kWh';
                    document.getElementById('inverter_temp').textContent = 'Inv: ' + d.inv_temp.toFixed(1) + '°C';
                    document.getElementById('bat_temp').textContent = d.bat_temp.toFixed(1) + '°C';
                })
                .catch(err => console.error('Error:', err));
        }
        setInterval(updateColors, 10000);
        updateColors();
        setInterval(() => {
            const now = new Date();
            const str = ('0'+now.getDate()).slice(-2) + '/' + ('0'+(now.getMonth()+1)).slice(-2) + '/' + now.getFullYear() + 
                        ' ' + ('0'+now.getHours()).slice(-2) + ':' + ('0'+now.getMinutes()).slice(-2);
            document.getElementById('datetime').textContent = str;
        }, 60000);
    </script>
</body>
</html>
  )rawliteral";
  return html;
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== MONITOR SOLAR ===");
  connectWiFi();
  delay(1000);
  if (MDNS.begin("solar")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS responder iniciado → http://solar.local");
  } else {
    Serial.println("Error al iniciar mDNS");
  }
  initializeInverter();
  setupWebServer();
  delay(2000);
  readInverterData();
}

void loop() {
  server.handleClient();
  static unsigned long last_read = 0;
  if (millis() - last_read > update_interval * 1000) {
    readInverterData();
    last_read = millis();
  }
}