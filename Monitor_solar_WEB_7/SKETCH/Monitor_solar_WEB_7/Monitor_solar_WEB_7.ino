#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <time.h>
#define LV_CONF_INCLUDE_SIMPLE 1
#include "lv_conf.h"
#include "lvgl.h"
#include "Waveshare_ST7262_LVGL.h"
#include "SolarmanV5.h"
#include "DeyeInverter.h"

// ===== CONFIGURACIÓN POR DEFECTO
const char* DEFAULT_SSID = "wifissid";               // nombre de la wifi
const char* DEFAULT_PASS = "wifipass";               // pass de la wifi
const uint32_t DEFAULT_DATALOGGER_SN = 1234567890;   // SN del datalogger
const char* DEFAULT_DATALOGGER_IP = "192.168.1.10";  // ip del datalogger
const int16_t DEFAULT_POTENCIA = 6000;               // potencia del inversor, W
const int16_t DEFAULT_ESPERA = 15;                   // espera hasta apagar pantalla,minutos
const uint32_t DEFAULT_READ_INTERVAL = 10;           // intervalo entre lecturas del inversor, segundos

// ===== VARIABLES DE CONFIGURACIÓN
String config_ssid = DEFAULT_SSID;
String config_password = DEFAULT_PASS;
uint32_t config_datalogger_sn = DEFAULT_DATALOGGER_SN;
String config_datalogger_ip = DEFAULT_DATALOGGER_IP;
int16_t potencia = DEFAULT_POTENCIA;
int16_t config_espera = DEFAULT_ESPERA;
uint32_t config_read_interval = DEFAULT_READ_INTERVAL;
const char* datalogger_ip = DEFAULT_DATALOGGER_IP;

// ===== GESTIÓN DE BACKLIGHT =====
uint32_t SCREEN_OFF_TIMEOUT_MS = config_espera * 60 * 1000;
unsigned long lastUserActivity = 0;
bool screenOn = true;

// ===== MODO AP =====
const char* AP_SSID = "MonitorSolar";
bool inApMode = false;
DNSServer dnsServer;

SolarmanV5* solarman = nullptr;
DeyeInverter* inverter = nullptr;
InverterData inv_data;
bool systemRunning = true;

lv_obj_t *arc_solar = nullptr;
lv_obj_t *arc_bat = nullptr;
lv_obj_t *arc_red = nullptr;
lv_obj_t *arc_home = nullptr;
lv_obj_t *label_solar = nullptr;
lv_obj_t *label_bat = nullptr;
lv_obj_t *label_red = nullptr;
lv_obj_t *label_home = nullptr;
lv_obj_t *label_pv1_pv2 = nullptr;
lv_obj_t *label_bat_power = nullptr;
lv_obj_t *label_red_daily = nullptr;
lv_obj_t *label_casa_daily = nullptr;
lv_obj_t *label_datetime = nullptr;
lv_obj_t *label_url = nullptr;
lv_obj_t *label_bat_temp = nullptr;
lv_obj_t *label_inverter_temp = nullptr;

lv_color_t color_primary = lv_color_hex(0x00AFFF);
lv_color_t color_success = lv_color_hex(0x39FF14);
lv_color_t color_danger = lv_color_hex(0xFF6666);
lv_color_t color_warn = lv_color_hex(0xFFAA00);

WebServer server(80);

// ===== FUNCIONES DE CONFIGURACIÓN =====
void loadConfig() {
    Preferences prefs;
    prefs.begin("solar", true);
    config_ssid = prefs.getString("ssid", DEFAULT_SSID);
    config_password = prefs.getString("pass", DEFAULT_PASS);
    config_datalogger_sn = prefs.getUInt("sn", DEFAULT_DATALOGGER_SN);
    config_datalogger_ip = prefs.getString("ip", DEFAULT_DATALOGGER_IP);
    config_espera = prefs.getShort("espera", DEFAULT_ESPERA);
    config_read_interval = prefs.getUInt("interval", DEFAULT_READ_INTERVAL);
    prefs.end();

    SCREEN_OFF_TIMEOUT_MS = config_espera * 60 * 1000;
}

void saveConfig() {
    Preferences prefs;
    prefs.begin("solar", false);
    prefs.putString("ssid", config_ssid);
    prefs.putString("pass", config_password);
    prefs.putUInt("sn", config_datalogger_sn);
    prefs.putString("ip", config_datalogger_ip);
    prefs.putShort("espera", config_espera);
    prefs.putUInt("interval", config_read_interval);
    prefs.end();
}

// ===== CONEXIÓN WIFI CON FALLO A AP =====
void connectWiFiWithFallback() {
    Preferences prefs;
    prefs.begin("solar", true);
    config_ssid = prefs.getString("ssid", DEFAULT_SSID);
    config_password = prefs.getString("pass", DEFAULT_PASS);
    config_datalogger_ip = prefs.getString("ip", DEFAULT_DATALOGGER_IP);
    config_datalogger_sn = prefs.getUInt("sn", DEFAULT_DATALOGGER_SN);
    potencia  = prefs.getShort("potencia", DEFAULT_POTENCIA);
    config_espera = prefs.getShort("espera", DEFAULT_ESPERA);
    config_read_interval = prefs.getUInt("interval", DEFAULT_READ_INTERVAL);
    prefs.end();

    SCREEN_OFF_TIMEOUT_MS = config_espera * 60 * 1000;

    Serial.printf("Intentando conectar a WiFi: %s\n", config_ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(config_ssid.c_str(), config_password.c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 10000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi conectado. IP: " + WiFi.localIP().toString());
        inApMode = false;
    } else {
        Serial.println("\nFalló conexión. Modo AP activado.");
        WiFi.disconnect();
        WiFi.mode(WIFI_AP);
        WiFi.softAP("MonitorSolar");
        WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
        dnsServer.start(53, "*", WiFi.softAPIP());
        inApMode = true;
    }
}

// ===== WEB SETUP
void handleSetupPage() {
    Preferences prefs;
    prefs.begin("solar", true);
    String ssid_val = prefs.getString("ssid", DEFAULT_SSID);
    String pass_val = prefs.getString("pass", DEFAULT_PASS);
    String ip_val = prefs.getString("ip", DEFAULT_DATALOGGER_IP);
    uint32_t sn_val = prefs.getUInt("sn", DEFAULT_DATALOGGER_SN);
    int16_t potencia_val = prefs.getShort("potencia", DEFAULT_POTENCIA);
    int16_t espera_val = prefs.getShort("espera", DEFAULT_ESPERA);
    uint32_t interval_val = prefs.getUInt("interval", DEFAULT_READ_INTERVAL);
    prefs.end();

    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Configuración - Monitor Solar</title>
    <meta charset="UTF-8">
    <style>
        body { font-family: Arial; background: #111; color: white; padding: 20px; }
        input, button { padding: 10px; margin: 8px 0; width: 100%; box-sizing: border-box; }
        button { background: #00AFFF; color: white; border: none; cursor: pointer; }
    </style>
</head>
<body>
    <h2>Configuración del Monitor Solar</h2>
    <form method="POST" action="/save">
        <label>SSID WiFi:</label>
        <input name="ssid" value=")rawliteral" + ssid_val + R"rawliteral(" required>
        <label>Contraseña WiFi:</label>
        <input name="pass" type="password" value=")rawliteral" + pass_val + R"rawliteral(" required>
        <label>Datalogger IP:</label>
        <input name="ip" type="text" value=")rawliteral" + ip_val + R"rawliteral(" pattern="^(\d{1,3}\.){3}\d{1,3}$" required>
        <label>Datalogger SN:</label>
        <input name="sn" type="number" value=")rawliteral" + String(sn_val) + R"rawliteral(" required>
        <label>Potencia del inversor (W):</label>
        <input name="potencia" type="number" value=")rawliteral" + String(potencia_val) + R"rawliteral(" min="1000" max="20000" required>
        <label>Apagado pantalla (minutos):</label>
        <input name="espera" type="number" value=")rawliteral" + String(espera_val) + R"rawliteral(" min="1" max="60" required>
        <label>Intervalo entre lectura (segundos):</label>
        <input name="interval" type="number" value=")rawliteral" + String(interval_val) + R"rawliteral(" min="5" max="60" required>
        <button type="submit">Guardar y Reiniciar</button>
    </form>
</body>
</html>
    )rawliteral";
    server.send(200, "text/html", html);
}

void handleSaveConfig() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Método no permitido");
        return;
    }

    String ssid_val = server.arg("ssid");
    String pass_val = server.arg("pass");
    String ip_val = server.arg("ip");
    uint32_t sn_val = strtoul(server.arg("sn").c_str(), NULL, 10);
    int16_t potencia_val = server.arg("potencia").toInt();
    int16_t espera_val = server.arg("espera").toInt();
    uint32_t interval_val = server.arg("interval").toInt();

    Preferences prefs;
    prefs.begin("solar", false);
    prefs.putString("ssid", ssid_val);
    prefs.putString("pass", pass_val);
    prefs.putString("ip", ip_val);
    prefs.putUInt("sn", sn_val);
    prefs.putShort("potencia", potencia_val);
    prefs.putShort("espera", espera_val);
    prefs.putUInt("interval", interval_val);
    prefs.end();

    server.send(200, "text/html", "<html><body><h2>Guardado. Reiniciando...</h2></body></html>");
    delay(1000);
    ESP.restart();
}

// ===== FUNCIONES
void setScreenBacklight(bool on) {
    if (on != screenOn) {
        int dummy = screenOn ? 1 : 0;
        toggle_backlight(dummy);
        screenOn = on;
        Serial.println(screenOn ? "Pantalla ENCENDIDA" : "Pantalla APAGADA");
    }
}

void touch_activity_cb(lv_event_t *e) {
    (void)e;
    lastUserActivity = millis();
    if (!screenOn) {
        setScreenBacklight(true);
    }
}

bool init_display() {
    Serial.println("Inicializando pantalla...");
    lcd_init();
    delay(1000);
    screenOn = true;
    if (lv_disp_get_default() == nullptr) {
        Serial.println("ERROR: LVGL no se inicializó");
        return false;
    }
    Serial.println("Pantalla OK. Resolución: " + 
                  String(lv_disp_get_hor_res(nullptr)) + "x" + 
                  String(lv_disp_get_ver_res(nullptr)));
    return true;
}

// === INTERFAZ LCD
void create_ui() {
    Serial.println("Creando interfaz...");
    lvgl_port_lock(-1);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0A0A0A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_event_cb(lv_scr_act(), touch_activity_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(lv_scr_act(), touch_activity_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(lv_scr_act(), touch_activity_cb, LV_EVENT_LONG_PRESSED, NULL);

    // FECHA Y HORA
    label_datetime = lv_label_create(lv_scr_act());
    lv_label_set_text(label_datetime, "00/00/0000 00:00");
    lv_obj_set_style_text_font(label_datetime, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_datetime, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(label_datetime, LV_ALIGN_TOP_LEFT, 20, 5);

    // TEMPERATURA DEL INVERSOR
    label_inverter_temp = lv_label_create(lv_scr_act());
    lv_label_set_text(label_inverter_temp, "Inversor: --°C");
    lv_obj_set_style_text_font(label_inverter_temp, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_inverter_temp, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(label_inverter_temp, LV_ALIGN_TOP_MID, 0, 5);

    // URL O IP
    label_url = lv_label_create(lv_scr_act());
    lv_label_set_text(label_url, inApMode ? "http://192.168.4.1" : "http://solar.local");
    lv_obj_set_style_text_font(label_url, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label_url, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(label_url, LV_ALIGN_TOP_RIGHT, -20, 5);

    // SOLAR
    lv_obj_t* card1 = lv_obj_create(lv_scr_act());
    lv_obj_set_size(card1, 385, 210);
    lv_obj_set_pos(card1, 10, 40);
    lv_obj_set_style_bg_color(card1, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_radius(card1, 20, 0);
    lv_obj_set_style_border_width(card1, 0, 0);
    lv_obj_set_style_border_opa(card1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(card1, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card1, 0, 0);
    lv_obj_set_style_outline_width(card1, 0, 0);
    lv_obj_clear_flag(card1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* title_solar = lv_label_create(card1);
    lv_label_set_text(title_solar, "SOLAR");
    lv_obj_set_style_text_font(title_solar, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title_solar, lv_color_white(), 0);
    lv_obj_align(title_solar, LV_ALIGN_TOP_LEFT, 12, 12);
    arc_solar = lv_arc_create(card1);
    lv_obj_set_size(arc_solar, 180, 180);
    lv_arc_set_range(arc_solar, 0, potencia);
    lv_arc_set_value(arc_solar, 0);
    lv_arc_set_bg_angles(arc_solar, 150, 30);
    lv_obj_set_style_arc_color(arc_solar, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_solar, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_solar, color_primary, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_solar, 20, LV_PART_INDICATOR);
    lv_obj_remove_style(arc_solar, NULL, LV_PART_KNOB);
    lv_obj_center(arc_solar);
    lv_obj_t* cont_solar = lv_obj_create(card1);
    lv_obj_set_size(cont_solar, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont_solar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_solar, 0, 0);
    lv_obj_set_style_border_opa(cont_solar, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(cont_solar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont_solar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(cont_solar);
    label_solar = lv_label_create(cont_solar);
    lv_label_set_text(label_solar, "0");
    lv_obj_set_style_text_font(label_solar, &lv_font_montserrat_38, 0);
    lv_obj_set_style_text_color(label_solar, lv_color_white(), 0);
    lv_obj_t* unit_solar = lv_label_create(cont_solar);
    lv_label_set_text(unit_solar, "kW");
    lv_obj_set_style_text_font(unit_solar, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(unit_solar, lv_color_hex(0xAAAAAA), 0);
    label_pv1_pv2 = lv_label_create(card1);
    lv_label_set_text(label_pv1_pv2, "0W y 0W - Hoy: 0kWh");
    lv_obj_set_style_text_font(label_pv1_pv2, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_pv1_pv2, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(label_pv1_pv2, LV_ALIGN_BOTTOM_MID, 0, -10);

    // BATERIA
    lv_obj_t* card2 = lv_obj_create(lv_scr_act());
    lv_obj_set_size(card2, 385, 210);
    lv_obj_set_pos(card2, 10, 260);
    lv_obj_set_style_bg_color(card2, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_radius(card2, 20, 0);
    lv_obj_set_style_border_width(card2, 0, 0);
    lv_obj_set_style_border_opa(card2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(card2, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card2, 0, 0);
    lv_obj_set_style_outline_width(card2, 0, 0);
    lv_obj_clear_flag(card2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* title_bat = lv_label_create(card2);
    lv_label_set_text(title_bat, "BATERIA");
    lv_obj_set_style_text_font(title_bat, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title_bat, lv_color_white(), 0);
    lv_obj_align(title_bat, LV_ALIGN_TOP_LEFT, 12, 12);
    arc_bat = lv_arc_create(card2);
    lv_obj_set_size(arc_bat, 180, 180);
    lv_arc_set_range(arc_bat, 0, 100);
    lv_arc_set_value(arc_bat, 0);
    lv_arc_set_bg_angles(arc_bat, 150, 30);
    lv_obj_set_style_arc_color(arc_bat, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_bat, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_bat, color_success, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_bat, 20, LV_PART_INDICATOR);
    lv_obj_remove_style(arc_bat, NULL, LV_PART_KNOB);
    lv_obj_center(arc_bat);
    lv_obj_t* cont_bat = lv_obj_create(card2);
    lv_obj_set_size(cont_bat, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont_bat, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_bat, 0, 0);
    lv_obj_set_style_border_opa(cont_bat, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(cont_bat, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont_bat, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(cont_bat);
    label_bat = lv_label_create(cont_bat);
    lv_label_set_text(label_bat, "0");
    lv_obj_set_style_text_font(label_bat, &lv_font_montserrat_38, 0);
    lv_obj_set_style_text_color(label_bat, lv_color_white(), 0);
    lv_obj_t* unit_bat = lv_label_create(cont_bat);
    lv_label_set_text(unit_bat, "%");
    lv_obj_set_style_text_font(unit_bat, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(unit_bat, lv_color_hex(0xAAAAAA), 0);
    label_bat_power = lv_label_create(card2);
    label_bat_temp = lv_label_create(card2);
    lv_label_set_text(label_bat_temp, "--°C ");
    lv_obj_set_style_text_font(label_bat_temp, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_bat_temp, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(label_bat_temp, LV_ALIGN_BOTTOM_LEFT, 12, -10);
    lv_label_set_text(label_bat_power, "0 W");
    lv_obj_set_style_text_font(label_bat_power, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_bat_power, lv_color_white(), 0);
    lv_obj_align(label_bat_power, LV_ALIGN_BOTTOM_MID, 0, -10);

    // RED
    lv_obj_t* card3 = lv_obj_create(lv_scr_act());
    lv_obj_set_size(card3, 385, 210);
    lv_obj_set_pos(card3, 405, 40);
    lv_obj_set_style_bg_color(card3, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_radius(card3, 20, 0);
    lv_obj_set_style_border_width(card3, 0, 0);
    lv_obj_set_style_border_opa(card3, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(card3, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card3, 0, 0);
    lv_obj_set_style_outline_width(card3, 0, 0);
    lv_obj_clear_flag(card3, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* title_red = lv_label_create(card3);
    lv_label_set_text(title_red, "RED");
    lv_obj_set_style_text_font(title_red, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title_red, lv_color_white(), 0);
    lv_obj_align(title_red, LV_ALIGN_TOP_RIGHT, -12, 12);
    arc_red = lv_arc_create(card3);
    lv_obj_set_size(arc_red, 180, 180);
    lv_arc_set_range(arc_red, 0, potencia);
    lv_arc_set_value(arc_red, 0);
    lv_arc_set_bg_angles(arc_red, 150, 30);
    lv_obj_set_style_arc_color(arc_red, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_red, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_red, color_warn, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_red, 20, LV_PART_INDICATOR);
    lv_obj_remove_style(arc_red, NULL, LV_PART_KNOB);
    lv_obj_center(arc_red);
    lv_obj_t* cont_red = lv_obj_create(card3);
    lv_obj_set_size(cont_red, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont_red, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_red, 0, 0);
    lv_obj_set_style_border_opa(cont_red, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(cont_red, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont_red, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(cont_red);
    label_red = lv_label_create(cont_red);
    lv_label_set_text(label_red, "0");
    lv_obj_set_style_text_font(label_red, &lv_font_montserrat_38, 0);
    lv_obj_set_style_text_color(label_red, lv_color_white(), 0);
    lv_obj_t* unit_red = lv_label_create(cont_red);
    lv_label_set_text(unit_red, "kW");
    lv_obj_set_style_text_font(unit_red, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(unit_red, lv_color_hex(0xAAAAAA), 0);
    label_red_daily = lv_label_create(card3);
    lv_label_set_text(label_red_daily, "0 kWh");
    lv_obj_set_style_text_font(label_red_daily, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_red_daily, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(label_red_daily, LV_ALIGN_BOTTOM_MID, 0, -10);

    // CASA
    lv_obj_t* card4 = lv_obj_create(lv_scr_act());
    lv_obj_set_size(card4, 385, 210);
    lv_obj_set_pos(card4, 405, 260);
    lv_obj_set_style_bg_color(card4, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_radius(card4, 20, 0);
    lv_obj_set_style_border_width(card4, 0, 0);
    lv_obj_set_style_border_opa(card4, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(card4, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(card4, 0, 0);
    lv_obj_set_style_outline_width(card4, 0, 0);
    lv_obj_clear_flag(card4, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* title_casa = lv_label_create(card4);
    lv_label_set_text(title_casa, "CASA");
    lv_obj_set_style_text_font(title_casa, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title_casa, lv_color_white(), 0);
    lv_obj_align(title_casa, LV_ALIGN_TOP_RIGHT, -12, 12);
    arc_home = lv_arc_create(card4);
    lv_obj_set_size(arc_home, 180, 180);
    lv_arc_set_range(arc_home, 0, potencia);
    lv_arc_set_value(arc_home, 0);
    lv_arc_set_bg_angles(arc_home, 150, 30);
    lv_obj_set_style_arc_color(arc_home, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_home, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_home, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_home, 20, LV_PART_INDICATOR);
    lv_obj_remove_style(arc_home, NULL, LV_PART_KNOB);
    lv_obj_center(arc_home);
    lv_obj_t* cont_home = lv_obj_create(card4);
    lv_obj_set_size(cont_home, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont_home, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_home, 0, 0);
    lv_obj_set_style_border_opa(cont_home, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(cont_home, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont_home, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_center(cont_home);
    label_home = lv_label_create(cont_home);
    lv_label_set_text(label_home, "0");
    lv_obj_set_style_text_font(label_home, &lv_font_montserrat_38, 0);
    lv_obj_set_style_text_color(label_home, lv_color_white(), 0);
    lv_obj_t* unit_home = lv_label_create(cont_home);
    lv_label_set_text(unit_home, "kW");
    lv_obj_set_style_text_font(unit_home, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(unit_home, lv_color_hex(0xAAAAAA), 0);
    label_casa_daily = lv_label_create(card4);
    lv_label_set_text(label_casa_daily, "0 kWh");
    lv_obj_set_style_text_font(label_casa_daily, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(label_casa_daily, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(label_casa_daily, LV_ALIGN_BOTTOM_MID, 0, -10);

    lvgl_port_unlock();
    Serial.println("Interfaz creada");
}

// === LECTURA DEL DATALOGGER
void inverterReadTask(void *parameter) {
    Serial.println("Tarea de lectura del inversor iniciada en core " + String(xPortGetCoreID()));
    while (systemRunning) {
        if (inverter) {
            bool success = inverter->readAllData(&inv_data);
            if (success) {
                int solar = (int)(inv_data.pv1_power + inv_data.pv2_power);
                int pv1 = (int)inv_data.pv1_power;
                int pv2 = (int)inv_data.pv2_power;
                int soc = (int)inv_data.battery_soc;
                int bat_power = (int)inv_data.battery_power;
                int home = (int)inv_data.load_power;
                int grid = (int)inv_data.grid_power;
                float daily_bought = inv_data.daily_energy_bought;
                float daily_load = inv_data.daily_load_consumption;

                if (lvgl_port_lock(20)) {
                    time_t now = time(nullptr);
                    struct tm local_time;
                    localtime_r(&now, &local_time);
                    char datetime_str[32];
                    strftime(datetime_str, sizeof(datetime_str), "%d/%m/%Y %H:%M", &local_time);
                    if (label_datetime) lv_label_set_text(label_datetime, datetime_str);
                    if (label_inverter_temp) {
                        char temp_str[16];
                        snprintf(temp_str, sizeof(temp_str), "T.Inv %.1f°C ", inv_data.inverter_temperature);
                        lv_label_set_text(label_inverter_temp, temp_str);
                    }

                    if (arc_solar) lv_arc_set_value(arc_solar, solar);
                    if (label_solar) {
                        String val = String(solar / 1000.0f, 2);
                        lv_label_set_text(label_solar, val.c_str());
                    }

                    if (label_pv1_pv2) {
                        char buf[80];
                        snprintf(buf, sizeof(buf), "%dW y %dW - Hoy: %.2f kWh", pv1, pv2, inv_data.daily_production);
                        lv_label_set_text(label_pv1_pv2, buf);
                    }

                    if (arc_bat) {
                        lv_arc_set_value(arc_bat, soc);
                        lv_color_t bat_color = soc > 70 ? color_success : soc > 30 ? color_warn : color_danger;
                        lv_obj_set_style_arc_color(arc_bat, bat_color, LV_PART_INDICATOR);
                    }
                    if (label_bat) {
                        String val = String(soc);
                        lv_label_set_text(label_bat, val.c_str());
                    }
                    if (label_bat_power) {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "%d W", bat_power);
                        lv_label_set_text(label_bat_power, buf);
                        lv_color_t pwr_color = (bat_power < 0) ? color_success : color_danger;
                        lv_obj_set_style_text_color(label_bat_power, pwr_color, 0);
                    }
                    if (label_bat_temp) {
                        char temp_str[16];
                        snprintf(temp_str, sizeof(temp_str), "%.1f°C ", inv_data.battery_temperature);
                        lv_label_set_text(label_bat_temp, temp_str);
                    }
                    if (arc_red) {
                        lv_arc_set_value(arc_red, abs(grid));
                        lv_color_t grid_color = (grid > 0) ? color_danger : color_success;
                        lv_obj_set_style_arc_color(arc_red, grid_color, LV_PART_INDICATOR);
                    }
                    if (label_red) {
                        String val = String(grid / 1000.0f, 2);
                        lv_label_set_text(label_red, val.c_str());
                        lv_color_t num_color = (grid > 0) ? color_danger : color_success;
                        lv_obj_set_style_text_color(label_red, num_color, 0);
                    }
                    if (label_red_daily) {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "Hoy: %.2f kWh", daily_bought);
                        lv_label_set_text(label_red_daily, buf);
                        lv_color_t daily_color = (daily_bought > 0) ? color_danger : lv_color_hex(0xAAAAAA);
                        lv_obj_set_style_text_color(label_red_daily, daily_color, 0);
                    }

                    if (arc_home) lv_arc_set_value(arc_home, home);
                    if (label_home) {
                        String val = String(home / 1000.0f, 2);
                        lv_label_set_text(label_home, val.c_str());
                    }
                    if (label_casa_daily) {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "Hoy: %.2f kWh", daily_load);
                        lv_label_set_text(label_casa_daily, buf);
                    }

                    lvgl_port_unlock();
                    Serial.printf("✓ UI actualizada - Solar: %dW, Bat: %d%%, Casa: %dW\n", solar, soc, home);
                }
            } else {
                Serial.println("✗ Error leyendo datos del inversor");
            }
        }
        vTaskDelay((config_read_interval  * 1000) / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

// === JSON
void handleJson() {
    if (!inv_data.data_valid) {
        server.send(503, "application/json", "{\"error\":\"Datos no disponibles\"}");
        return;
    }
    String json = "{";
    json += "\"inv_temp\":" + String(inv_data.inverter_temperature, 1) + ",";
    json += "\"solar\":" + String((int)(inv_data.pv1_power + inv_data.pv2_power)) + ",";
    json += "\"pv1\":" + String((int)inv_data.pv1_power) + ",";
    json += "\"pv2\":" + String((int)inv_data.pv2_power) + ",";
    json += "\"daily_production\":" + String(inv_data.daily_production, 2) + ",";
    json += "\"soc\":" + String((int)inv_data.battery_soc) + ",";
    json += "\"bat_power\":" + String((int)inv_data.battery_power) + ",";
    json += "\"bat_temp\":" + String(inv_data.battery_temperature, 1) + ",";
    json += "\"home\":" + String((int)inv_data.load_power) + ",";
    json += "\"grid\":" + String((int)inv_data.grid_power) + ",";
    json += "\"daily_bought\":" + String(inv_data.daily_energy_bought, 2) + ",";
    json += "\"daily_load\":" + String(inv_data.daily_load_consumption, 2);
    json += "}";
    server.send(200, "application/json", json);
}

// === WEB
const char WEBSITE[] PROGMEM = R"rawliteral(
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
            src: url(data:font/woff2;base64,@font-face { 
  font-family: "Montserrat Thin SemiBold";
  /* Add other properties here, as needed. For example: */
  /*
  font-weight: 100 900;
  font-style: normal italic;
  */
  src: url(data:application/octet-stream;base64,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) format('woff2');
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
            <div class="detail" id="pv1pv2">0W / 0W / Hoy: 0.00 kWh</div>
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
        <button onclick="window.location.href='/setup';"
                style="background:#00AFFF; color:white; border:none; padding:12px 30px; font-size:18px; border-radius:8px; cursor:pointer; width:280px;">
            Ir a Configuración
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
                        d.pv1 + 'W / ' + d.pv2 + 'W / Hoy: ' + d.daily_production.toFixed(2) + ' kWh';
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

void serveHtml() {
    String html = String(WEBSITE);
    server.send(200, "text/html", html);
}

// === SETUP
void setup() {
    Serial.begin(115200);
    Serial.println("=== MONITOR SOLAR ===");

    connectWiFiWithFallback();
    delay(1000);

    if (!inApMode && MDNS.begin("solar")) {
        MDNS.addService("http", "tcp", 80);
        Serial.println("mDNS responder iniciado → http://solar.local");
    }

    server.on("/data", HTTP_GET, handleJson);
    server.on("/reset", HTTP_POST, []() {
        server.send(200, "text/plain", "Reiniciando...");
        delay(100);
        ESP.restart();
    });
    server.on("/setup", HTTP_GET, handleSetupPage);
    server.on("/save", HTTP_POST, handleSaveConfig);

    if (inApMode) {
        server.onNotFound([]() {
            server.sendHeader("Location", "/setup", true);
            server.send(302, "text/plain", "");
        });
    } else {
        server.onNotFound(serveHtml);
    }

    server.begin();

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    if (!init_display()) {
        Serial.println("FALLO: Display no inicializado");
        while(1) delay(1000);
    }

    create_ui();
    const char* datalogger_ip_used = config_datalogger_ip.c_str();
    solarman = new SolarmanV5(config_datalogger_ip.c_str(), config_datalogger_sn);
    inverter = new DeyeInverter(solarman);
    solarman->begin();

    xTaskCreatePinnedToCore(inverterReadTask, "InverterReader", 10000, NULL, 1, NULL, 1);
    Serial.println("=== SISTEMA LISTO ===");
}

// === LOOP
void loop() {
    server.handleClient();
    if (inApMode) {
        dnsServer.processNextRequest();
    }
    if (screenOn && (millis() - lastUserActivity >= SCREEN_OFF_TIMEOUT_MS)) {
        setScreenBacklight(false);
        //Serial.println("Apagando pantalla LCD");
    }

    static unsigned long last_status = 0;
    //if (millis() - last_status > 30000) {
    //    Serial.printf("Sistema activo - Pantalla: %s - Memoria libre: %d\n",
    //                  screenOn ? "ON" : "OFF", esp_get_free_heap_size());
    //    last_status = millis();
    //}
    delay(10);
}