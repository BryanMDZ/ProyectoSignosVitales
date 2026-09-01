#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>

#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"

// =====================================================
// CONFIGURACIÓN GENERAL
// =====================================================

// Pines ESP32-C3 Super Mini
#define SDA_PIN 8
#define SCL_PIN 9

// AD8232
#define ECG_PIN 4
#define ECG_SDN 1
#define LO_PLUS 2
#define LO_MINUS 3

// TMP117
#define TMP117_ADDR 0x48
#define TMP117_TEMP_REG 0x00

// MAX30102
MAX30105 particleSensor;

// Servidor local
WebServer server(80);

// WiFi
Preferences prefs;

String ssidGuardado = "";
String passGuardada = "";

// Estado de conexión
bool hayWiFiGuardado = false;
bool wifiDisponible = false;
bool internetDisponible = false;
bool modoAP = false;
bool modoConfigWiFi = false;
bool modoNube = false;

// ===========================
//  MEDICIÓN DE BATERÍA
// ===========================

#define BATTERY_PIN 0 // GPIO0 ADC

const float R1_BAT = 100000.0; // Resistencia superior del divisor
const float R2_BAT = 100000.0; // Resistencia inferior del divisor

const float FACTOR_DIVISOR_BAT = (R1_BAT + R2_BAT) / R2_BAT;

// Ajuste fino por calibración.
// Si mides con multímetro 4.00 V y el ESP32 calcula 3.92 V,
// factor = 4.00 / 3.92 = 1.020
const float FACTOR_CALIBRACION_BAT = 1.00;

float voltajeBateria = 0.0;
int porcentajeBateria = 0;

bool bateriaBaja = false;
bool bateriaCritica = false;

unsigned long lastBatteryRead = 0;
const unsigned long BATTERY_INTERVAL = 3000;

// =====================================================
// CONTROL DE MONITOREO DE RED
// =====================================================

bool monitoreoActivo = false;

// mDNS
const char *MDNS_NAME = "monitorsignos";

// Datos del SoftAP
const char *AP_SSID = "BioMonitor_Config";
const char *AP_PASS = "Monitor12345";

// =====================================================
// SUPERVISIÓN DE CONECTIVIDAD
// =====================================================

unsigned long lastWiFiCheck = 0;
unsigned long inicioDesconexion = 0;
unsigned long lastReconnectAttempt = 0;

const unsigned long WIFI_CHECK_INTERVAL = 5000;
const unsigned long WIFI_LOST_TIMEOUT = 15000;
const unsigned long WIFI_RECONNECT_INTERVAL = 10000;

bool softAPRespaldoActivo = false;
bool reinicioPendiente = false;
unsigned long inicioReinicio = 0;
const unsigned long TIEMPO_REINICIO = 2000;

// URL de Apps Script
const char *serverURL = "https://script.google.com/macros/s/AKfycbxknks8RtbI2pyyO2e9mVIfOQn6btbYjlNiKgSMi1vpl44Sn3kGFgqlN7rd8NMLTs-K/exec";

// ID de paciente
// Si después implementas paciente activo desde web, este valor se puede actualizar dinámicamente
String pacienteID = "001";

// =====================================================
// VARIABLES DE DISPONIBILIDAD DE SENSORES
// =====================================================

bool maxDisponible = false;
bool tmpDisponible = false;
bool ecgDisponible = true;

// Estado físico
bool dedoDetectado = false;
bool electrodosConectados = false;

// =====================================================
// SELECCIÓN DE VARIABLES A MONITOREAR
// =====================================================

bool medirBPM = false;
bool medirSpO2 = false;
bool medirTemp = false;
bool medirECG = false;

// =====================================================
// VARIABLES DE SIGNOS VITALES
// =====================================================

float temperatura = 0.0;
float temperaturaCalibrada = 0.0;
float offsetTemp = 0.0; // Ajusta aquí tu offset de calibración

int bpmInstantaneo = 0;
int bpmPromedio = 0;
int bpmSeguro = 0;

int spo2Valor = 0;
int spo2Seguro = 0;

int ecgValue = 0;

// =====================================================
// VARIABLES PARA BPM
// =====================================================

const byte RATE_SIZE = 8;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;

float beatsPerMinute = 0;
float filteredBPM = 0;

// =====================================================
// VARIABLES PARA SpO2
// =====================================================

#define BUFFER_SIZE 100

uint32_t irBuffer[BUFFER_SIZE];
uint32_t redBuffer[BUFFER_SIZE];

int32_t spo2;
int8_t validSPO2;

int32_t heartRateDummy;
int8_t validHeartRateDummy;

byte bufferIndex = 0;

const byte SPO2_SIZE = 8;
int spo2Buffer[SPO2_SIZE];
byte spo2Spot = 0;

float filteredSpO2 = 95.0;

// =======================================================
// VARIABLES PARA ECG
// =======================================================

#define ECG_PIN 4      // OUT del AD8232
#define ECG_LO_PLUS 2  // LO+
#define ECG_LO_MINUS 3 // LO-
#define ECG_SDN 1      // SDN

#define MODO_ECG 1

const unsigned long ECG_SAMPLE_PERIOD_US = 2000; // 500 Hz
unsigned long lastECGSample = 0;

int ecgRaw = 0;

float baseline = 2048.0;
float ecgLP1 = 0.0;
float ecgLP2 = 0.0;

const float ALPHA_BASELINE = 0.998;
const float ALPHA_LP1 = 0.12;
const float ALPHA_LP2 = 0.08;
const float GANANCIA_VISUAL = 4.0;

// Buffer reducido para nube
const int ECG_CLOUD_SIZE = 350;       // 250 muestras
const byte ECG_DECIMACION_NUBE = 1.5; // 500 Hz / 5 = 100 Hz

int ecgCloudBuffer[ECG_CLOUD_SIZE];
int ecgCloudIndex = 0;
bool ecgCloudFull = false;
byte ecgDecimador = 0;

// =====================================================
// TEMPORIZACIÓN
// =====================================================

unsigned long lastTempRead = 0;
unsigned long lastSerialPrint = 0;
unsigned long lastCloudSend = 0;

const unsigned long TEMP_INTERVAL = 1000;
const unsigned long SERIAL_INTERVAL = 1000;
const unsigned long CLOUD_INTERVAL = 5000;

// =====================================================
// FUNCIONES AUXILIARES
// =====================================================

String boolToString(bool value)
{
  return value ? "true" : "false";
}

// =====================================================
// LECTURA TMP117
// =====================================================

bool leerTMP117(float &temperaturaC)
{
  Wire.beginTransmission(TMP117_ADDR);
  Wire.write(TMP117_TEMP_REG);

  byte error = Wire.endTransmission(false);

  if (error != 0)
  {
    return false;
  }

  byte bytesRecibidos = Wire.requestFrom(TMP117_ADDR, (uint8_t)2);

  if (bytesRecibidos != 2)
  {
    return false;
  }

  byte msb = Wire.read();
  byte lsb = Wire.read();

  int16_t valorCrudo = (int16_t)((msb << 8) | lsb);

  temperaturaC = valorCrudo * 0.0078125f;

  return true;
}

// =====================================================
// VERIFICAR TMP117
// =====================================================

void verificarTMP117()
{
  Wire.beginTransmission(TMP117_ADDR);
  byte error = Wire.endTransmission();

  if (error == 0)
  {
    tmpDisponible = true;
    Serial.println("TMP117 detectado correctamente");
  }
  else
  {
    tmpDisponible = false;
    Serial.println("TMP117 no detectado");
  }
}

// =====================================================
// VERIFICAR MAX30102
// =====================================================

void verificarMAX30102()
{
  if (particleSensor.begin(Wire, I2C_SPEED_STANDARD))
  {
    maxDisponible = true;
    Serial.println("MAX30102 detectado correctamente");

    byte ledBrightness = 60;
    byte sampleAverage = 4;
    byte ledMode = 2; // Rojo + IR
    int sampleRate = 100;
    int pulseWidth = 411;
    int adcRange = 4096;

    particleSensor.setup(
        ledBrightness,
        sampleAverage,
        ledMode,
        sampleRate,
        pulseWidth,
        adcRange);

    particleSensor.setPulseAmplitudeRed(0x1F);
    particleSensor.setPulseAmplitudeIR(0x1F);
    particleSensor.setPulseAmplitudeGreen(0);
  }
  else
  {
    maxDisponible = false;
    Serial.println("MAX30102 no detectado");
  }
}

// =====================================================
// INICIALIZAR ECG
// =====================================================

void inicializarECG()
{
  pinMode(LO_PLUS, INPUT);
  pinMode(LO_MINUS, INPUT);

  pinMode(ECG_SDN, OUTPUT);
  digitalWrite(ECG_SDN, HIGH);

  analogReadResolution(12);
  analogSetPinAttenuation(ECG_PIN, ADC_11db);

  ecgDisponible = true;

  Serial.println("AD8232 configurado");
}

// =====================================================
// ACTUALIZAR MAX30102
// =====================================================

void actualizarMAX30102()
{
  if (!maxDisponible)
  {
    dedoDetectado = false;
    bpmSeguro = 0;
    spo2Seguro = 0;
    return;
  }

  particleSensor.check();

  if (!particleSensor.available())
  {
    return;
  }

  long irValue = particleSensor.getIR();
  long redValue = particleSensor.getRed();

  particleSensor.nextSample();

  if (irValue < 50000)
  {
    dedoDetectado = false;
    bpmSeguro = 0;
    spo2Seguro = 0;
    return;
  }

  dedoDetectado = true;

  // ------------------------------
  // BPM
  // ------------------------------
  if (medirBPM)
  {
    if (checkForBeat(irValue))
    {
      long delta = millis() - lastBeat;
      lastBeat = millis();

      beatsPerMinute = 60.0 / (delta / 1000.0);

      if (beatsPerMinute > 20 && beatsPerMinute < 200)
      {
        if (filteredBPM == 0)
        {
          filteredBPM = beatsPerMinute;
        }
        else
        {
          filteredBPM = 0.7 * filteredBPM + 0.3 * beatsPerMinute;
        }

        bpmInstantaneo = (int)filteredBPM;

        rates[rateSpot++] = bpmInstantaneo;
        rateSpot %= RATE_SIZE;

        int suma = 0;
        int conteo = 0;

        for (byte i = 0; i < RATE_SIZE; i++)
        {
          if (rates[i] > 0)
          {
            suma += rates[i];
            conteo++;
          }
        }

        if (conteo > 0)
        {
          bpmPromedio = suma / conteo;
          bpmSeguro = bpmPromedio;
        }
      }
    }
  }
  else
  {
    bpmSeguro = 0;
  }

  // ------------------------------
  // SpO2
  // ------------------------------
  if (medirSpO2)
  {
    irBuffer[bufferIndex] = irValue;
    redBuffer[bufferIndex] = redValue;
    bufferIndex++;

    if (bufferIndex >= BUFFER_SIZE)
    {
      maxim_heart_rate_and_oxygen_saturation(
          irBuffer,
          BUFFER_SIZE,
          redBuffer,
          &spo2,
          &validSPO2,
          &heartRateDummy,
          &validHeartRateDummy);

      if (validSPO2 && spo2 > 60 && spo2 <= 100)
      {
        filteredSpO2 = 0.8 * filteredSpO2 + 0.2 * spo2;

        spo2Buffer[spo2Spot++] = (int)filteredSpO2;
        spo2Spot %= SPO2_SIZE;

        int suma = 0;
        int conteo = 0;

        for (byte i = 0; i < SPO2_SIZE; i++)
        {
          if (spo2Buffer[i] > 0)
          {
            suma += spo2Buffer[i];
            conteo++;
          }
        }

        if (conteo > 0)
        {
          spo2Valor = suma / conteo;
          spo2Seguro = spo2Valor;
        }
      }

      bufferIndex = 0;
    }
  }
  else
  {
    spo2Seguro = 0;
  }
}

// =====================================================
// ACTUALIZAR TEMPERATURA
// =====================================================

void actualizarTemperatura()
{
  if (!medirTemp || !tmpDisponible)
  {
    return;
  }

  if (millis() - lastTempRead >= TEMP_INTERVAL)
  {
    lastTempRead = millis();

    float tempLeida;

    if (leerTMP117(tempLeida))
    {
      temperatura = tempLeida;
      temperaturaCalibrada = temperatura + offsetTemp;
    }
    else
    {
      tmpDisponible = false;
      Serial.println("Error al leer TMP117");
    }
  }
}

// =====================================================
// ACTUALIZAR ECG
// =====================================================

void actualizarECG()
{
  if (micros() - lastECGSample < ECG_SAMPLE_PERIOD_US)
    return;

  lastECGSample = micros();

  bool electrodoDesconectado =
      digitalRead(ECG_LO_PLUS) == HIGH ||
      digitalRead(ECG_LO_MINUS) == HIGH;

  if (electrodoDesconectado)
  {
    ecgRaw = 2048;
    ecgValue = 0;
    ecgLP1 = 0.0;
    ecgLP2 = 0.0;
    electrodosConectados = false;
    return;
  }

  electrodosConectados = true;

  ecgRaw = analogRead(ECG_PIN);

  baseline = ALPHA_BASELINE * baseline + (1.0 - ALPHA_BASELINE) * ecgRaw;

  float ecgCentrado = ecgRaw - baseline;

#if MODO_ECG == 0

  ecgValue = (int)(ecgCentrado * GANANCIA_VISUAL);

#elif MODO_ECG == 1

  ecgLP1 = ecgLP1 + ALPHA_LP1 * (ecgCentrado - ecgLP1);
  ecgLP2 = ecgLP2 + ALPHA_LP2 * (ecgLP1 - ecgLP2);

  ecgValue = (int)(ecgLP2 * GANANCIA_VISUAL);

#elif MODO_ECG == 2

  float rectificada = abs(ecgCentrado);

  ecgLP1 = ecgLP1 + ALPHA_LP1 * (rectificada - ecgLP1);
  ecgLP2 = ecgLP2 + ALPHA_LP2 * (ecgLP1 - ecgLP2);

  ecgValue = (int)(ecgLP2 * GANANCIA_VISUAL);

#endif

  // Decimación para nube: guarda solo 1 de cada 5 muestras
  ecgDecimador++;

  if (ecgDecimador >= ECG_DECIMACION_NUBE)
  {
    ecgDecimador = 0;

    ecgCloudBuffer[ecgCloudIndex] = ecgValue;
    ecgCloudIndex++;

    if (ecgCloudIndex >= ECG_CLOUD_SIZE)
    {
      ecgCloudIndex = 0;
      ecgCloudFull = true;
    }
  }
}

String obtenerBufferECG()
{
  if (!electrodosConectados)
    return "";

  int total = ecgCloudFull ? ECG_CLOUD_SIZE : ecgCloudIndex;

  if (total <= 0)
    return "";

  int inicio = ecgCloudFull ? ecgCloudIndex : 0;

  String buffer = "";
  buffer.reserve(total * 7);

  for (int i = 0; i < total; i++)
  {
    int idx = (inicio + i) % ECG_CLOUD_SIZE;

    buffer += String(ecgCloudBuffer[idx]);

    if (i < total - 1)
    {
      buffer += ",";
    }
  }

  return buffer;
}

// =====================================================
// MODO DE CONEXIÓN
// =====================================================
String obtenerIPActual()
{
  if (modoAP)
  {
    return WiFi.softAPIP().toString();
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    return WiFi.localIP().toString();
  }

  return "Sin IP";
}

String obtenerSSIDActual()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return WiFi.SSID();
  }

  if (modoAP)
  {
    return String(AP_SSID);
  }

  if (ssidGuardado.length() > 0)
  {
    return ssidGuardado;
  }

  return "Sin red";
}

String obtenerModoConexion()
{
  if (modoNube)
  {
    return "WiFi + nube";
  }

  if (wifiDisponible && !internetDisponible)
  {
    return "WiFi local sin internet";
  }

  if (softAPRespaldoActivo)
  {
    return "SoftAP de respaldo";
  }

  if (modoAP && modoConfigWiFi)
  {
    return "SoftAP configuracion";
  }

  if (modoAP)
  {
    return "SoftAP local";
  }

  return "Sin conexion";
}

void aplicarReglasDisponibilidad()
{
  if (!maxDisponible)
  {
    medirBPM = false;
    medirSpO2 = false;
  }

  if (!tmpDisponible)
  {
    medirTemp = false;
  }

  if (!ecgDisponible)
  {
    medirECG = false;
  }
}

bool haySeleccionValida()
{
  aplicarReglasDisponibilidad();

  return medirBPM || medirSpO2 || medirTemp || medirECG;
}

void leerSeleccionDesdeServidor()
{
  medirBPM = server.hasArg("bpm") && server.arg("bpm").toInt() == 1;
  medirSpO2 = server.hasArg("spo2") && server.arg("spo2").toInt() == 1;
  medirTemp = server.hasArg("temp") && server.arg("temp").toInt() == 1;
  medirECG = server.hasArg("ecg") && server.arg("ecg").toInt() == 1;

  aplicarReglasDisponibilidad();
}

void iniciarSoftAPRespaldo()
{
  if (softAPRespaldoActivo)
  {
    return;
  }

  Serial.println("Activando SoftAP de respaldo...");

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);

  modoAP = true;
  modoConfigWiFi = false;
  softAPRespaldoActivo = true;

  wifiDisponible = false;
  internetDisponible = false;
  modoNube = false;

  Serial.print("Red SoftAP de respaldo: ");
  Serial.println(AP_SSID);

  Serial.print("IP SoftAP: ");
  Serial.println(WiFi.softAPIP());
}

void detenerSoftAPRespaldo()
{
  detenerSoftAP();
}

void supervisarConectividad()
{
  if (millis() - lastWiFiCheck < WIFI_CHECK_INTERVAL)
  {
    return;
  }

  lastWiFiCheck = millis();

  // Si no hay credenciales guardadas, no tiene sentido intentar STA.
  // En ese caso se mantiene el SoftAP de configuración.
  if (!hayWiFiGuardado)
  {
    return;
  }

  // ==============================
  // CASO 1: WiFi conectado
  // ==============================
  if (WiFi.status() == WL_CONNECTED)
  {
    detenerSoftAP();
    if (!wifiDisponible)
    {
      Serial.println("WiFi recuperado");
      Serial.print("IP local: ");
      Serial.println(WiFi.localIP());
    }

    wifiDisponible = true;
    inicioDesconexion = 0;

    // Si estaba activo el SoftAP de respaldo, se puede apagar.
    if (softAPRespaldoActivo)
    {
      detenerSoftAPRespaldo();
    }

    // Verificar internet solo si no estaba disponible.
    if (!internetDisponible)
    {
      if (verificarInternet())
      {
        modoNube = true;
        Serial.println("Internet recuperado, se reactiva envio a nube");
      }
      else
      {
        modoNube = false;
        Serial.println("WiFi conectado, pero sin internet");
      }
    }

    return;
  }

  // ==============================
  // CASO 2: WiFi desconectado
  // ==============================

  if (inicioDesconexion == 0)
  {
    inicioDesconexion = millis();
    Serial.println("WiFi perdido, iniciando temporizador de respaldo...");
  }

  wifiDisponible = false;
  internetDisponible = false;
  modoNube = false;

  // Intentar reconexión periódica
  if (millis() - lastReconnectAttempt >= WIFI_RECONNECT_INTERVAL)
  {
    lastReconnectAttempt = millis();

    Serial.println("Intentando reconectar a WiFi guardado...");

    WiFi.mode(softAPRespaldoActivo ? WIFI_AP_STA : WIFI_STA);
    WiFi.begin(ssidGuardado.c_str(), passGuardada.c_str());
  }

  // Si la desconexión dura más del tiempo permitido, activar SoftAP.
  if (millis() - inicioDesconexion >= WIFI_LOST_TIMEOUT)
  {
    iniciarSoftAPRespaldo();
  }
}

// =====================================================
// GENERAR JSON LOCAL
// =====================================================
String generarJSON()
{
  String ipActual;

  if (modoAP)
  {
    ipActual = WiFi.softAPIP().toString();
  }
  else
  {
    ipActual = WiFi.localIP().toString();
  }

  String modoConexion;

  if (modoNube)
  {
    modoConexion = "WiFi + nube";
  }
  else if (wifiDisponible && !internetDisponible)
  {
    modoConexion = "WiFi local sin internet";
  }
  else if (modoAP && modoConfigWiFi)
  {
    modoConexion = "SoftAP configuracion";
  }
  else if (modoAP)
  {
    modoConexion = "SoftAP local";
  }
  else
  {
    modoConexion = "sin conexion";
  }
  String json = "{";
  json += "\"paciente\":\"" + pacienteID + "\",";

  json += "\"maxDisponible\":" + boolToString(maxDisponible) + ",";
  json += "\"tmpDisponible\":" + boolToString(tmpDisponible) + ",";
  json += "\"ecgDisponible\":" + boolToString(ecgDisponible) + ",";

  json += "\"dedoDetectado\":" + boolToString(dedoDetectado) + ",";
  json += "\"electrodosConectados\":" + boolToString(electrodosConectados) + ",";

  json += "\"medirBPM\":" + boolToString(medirBPM) + ",";
  json += "\"medirSpO2\":" + boolToString(medirSpO2) + ",";
  json += "\"medirTemp\":" + boolToString(medirTemp) + ",";
  json += "\"medirECG\":" + boolToString(medirECG) + ",";

  json += "\"bpm\":" + String(monitoreoActivo ? bpmSeguro : 0) + ",";
  json += "\"spo2\":" + String(monitoreoActivo ? spo2Seguro : 0) + ",";
  json += "\"temp\":" + String(monitoreoActivo ? temperaturaCalibrada : 0.0, 2) + ",";
  json += "\"ecg\":" + String(monitoreoActivo ? ecgValue : 0) + ",";

  json += "\"monitoreoActivo\":" + boolToString(monitoreoActivo) + ",";

  json += "\"wifiDisponible\":" + boolToString(wifiDisponible) + ",";
  json += "\"internetDisponible\":" + boolToString(internetDisponible) + ",";
  json += "\"modoAP\":" + boolToString(modoAP) + ",";
  json += "\"modoConfigWiFi\":" + boolToString(modoConfigWiFi) + ",";
  json += "\"modoNube\":" + boolToString(modoNube) + ",";

  json += "\"wifi\":\"" + obtenerModoConexion() + "\",";
  json += "\"ssid\":\"" + obtenerSSIDActual() + "\",";
  json += "\"ip\":\"" + obtenerIPActual() + "\",";

  json += "\"voltajeBateria\":" + String(voltajeBateria, 2) + ",";
  json += "\"porcentajeBateria\":" + String(porcentajeBateria) + ",";
  json += "\"bateriaBaja\":" + boolToString(bateriaBaja) + ",";
  json += "\"bateriaCritica\":" + boolToString(bateriaCritica) + ",";

  json += "\"mdns\":\"http://" + String(MDNS_NAME) + ".local\"";
  json += "}";
  return json;
}

// =====================================================
// PÁGINA WEB LOCAL
// =====================================================

String paginaHTML()
{
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="es">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>BioMonitor Local</title>
  
    <style>
      :root {
        --primary: #0f4c75;
        --primary-dark: #0b354f;
        --accent: #00a6a6;
        --bg: #f4f8fb;
        --card: #ffffff;
        --soft: #eef6f9;
        --border: #d9e2ec;
        --text: #17212b;
        --muted: #6b7c8f;
        --danger: #d9534f;
        --ok: #2e8b57;
        --warn: #f4a261;
      }
  
      * {
        box-sizing: border-box;
      }
  
      body {
        margin: 0;
        font-family: Arial, Helvetica, sans-serif;
        background: var(--bg);
        color: var(--text);
      }
  
      .topbar {
        height: 60px;
        background: var(--primary);
        color: white;
        display: flex;
        align-items: center;
        padding: 0 18px;
        box-shadow: 0 2px 8px rgba(0,0,0,0.15);
        position: sticky;
        top: 0;
        z-index: 10;
      }
  
      .menu-btn {
        font-size: 28px;
        cursor: pointer;
        margin-right: 16px;
        user-select: none;
      }
  
      .topbar-title {
        font-size: 18px;
        font-weight: bold;
        letter-spacing: 0.4px;
      }
  
      #menuToggle {
        display: none;
      }
  
      .sidebar {
        position: fixed;
        top: 0;
        left: -280px;
        width: 260px;
        height: 100vh;
        background: white;
        box-shadow: 2px 0 14px rgba(0,0,0,0.18);
        z-index: 20;
        transition: left 0.25s ease;
        padding-top: 70px;
      }
  
      #menuToggle:checked ~ .sidebar {
        left: 0;
      }
  
      .sidebar-header {
        position: absolute;
        top: 0;
        left: 0;
        height: 60px;
        width: 100%;
        background: var(--primary-dark);
        color: white;
        display: flex;
        align-items: center;
        padding: 0 18px;
        font-weight: bold;
      }
  
      .nav-item {
        display: block;
        padding: 15px 20px;
        color: var(--text);
        text-decoration: none;
        border-bottom: 1px solid var(--border);
        font-weight: bold;
      }
  
      .nav-item:hover,
      .nav-item.active {
        background: var(--soft);
        color: var(--primary);
      }
  
      .overlay {
        display: none;
        position: fixed;
        inset: 0;
        background: rgba(0,0,0,0.25);
        z-index: 15;
      }
  
      #menuToggle:checked ~ .overlay {
        display: block;
      }
  
      main {
        max-width: 1180px;
        margin: 22px auto;
        padding: 0 16px 30px;
      }
  
      .page-title {
        margin-bottom: 18px;
      }
  
      .page-title h1 {
        margin: 0;
        color: var(--primary);
        font-size: 26px;
      }
  
      .page-title p {
        margin: 6px 0 0;
        color: var(--muted);
      }
  
      .grid {
        display: grid;
        grid-template-columns: repeat(12, 1fr);
        gap: 16px;
      }
  
      .card {
        background: var(--card);
        border: 1px solid var(--border);
        border-radius: 14px;
        padding: 18px;
        box-shadow: 0 2px 8px rgba(0,0,0,0.06);
      }
  
      .span-4 { grid-column: span 4; }
      .span-6 { grid-column: span 6; }
      .span-8 { grid-column: span 8; }
      .span-12 { grid-column: span 12; }
  
      .card-title {
        margin: 0 0 14px;
        font-size: 15px;
        text-transform: uppercase;
        letter-spacing: 1px;
        color: var(--primary);
        font-weight: bold;
      }
  
      .check-grid {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
        gap: 10px;
        margin-bottom: 16px;
      }
  
      .check-card {
        border: 1px solid var(--border);
        background: var(--soft);
        border-radius: 10px;
        padding: 12px;
        display: flex;
        align-items: center;
        gap: 8px;
        font-weight: bold;
        cursor: pointer;
      }
  
      .check-card input {
        transform: scale(1.15);
      }
  
      .check-card input:disabled + span {
        color: var(--muted);
      }
  
      .actions {
        display: flex;
        gap: 10px;
        flex-wrap: wrap;
      }
  
      button,
      .btn {
        border: none;
        border-radius: 10px;
        padding: 12px 16px;
        font-weight: bold;
        cursor: pointer;
        background: var(--primary);
        color: white;
        transition: 0.2s;
      }
  
      button:hover,
      .btn:hover {
        background: var(--primary-dark);
      }
  
      .btn-secondary {
        background: var(--accent);
      }
  
      .btn-danger {
        background: var(--danger);
      }
  
      .btn-light {
        background: var(--soft);
        color: var(--primary);
        border: 1px solid var(--border);
      }
  
      .monitor-grid {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(185px, 1fr));
        gap: 14px;
      }
  
      .value-card {
        background: var(--soft);
        border-radius: 12px;
        padding: 16px;
        border: 1px solid var(--border);
      }
  
      .label {
        font-size: 13px;
        color: var(--muted);
        font-weight: bold;
        text-transform: uppercase;
        letter-spacing: 0.8px;
      }
  
      .value {
        margin-top: 10px;
        font-size: 34px;
        color: var(--primary);
        font-weight: bold;
      }
  
      .unit {
        font-size: 15px;
        color: var(--muted);
        margin-left: 4px;
      }
  
      .status-grid {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(210px, 1fr));
        gap: 12px;
      }
  
      .status-row {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 10px;
        border: 1px solid var(--border);
        border-radius: 10px;
        padding: 12px;
        background: #fff;
      }
  
      .chip {
        border-radius: 999px;
        padding: 6px 10px;
        font-size: 12px;
        font-weight: bold;
        white-space: nowrap;
      }
  
      .chip-ok {
        background: rgba(46,139,87,0.14);
        color: var(--ok);
      }
  
      .chip-bad {
        background: rgba(217,83,79,0.14);
        color: var(--danger);
      }
  
      .chip-warn {
        background: rgba(244,162,97,0.18);
        color: #b06421;
      }
  
      .info-list {
        display: grid;
        grid-template-columns: 170px 1fr;
        gap: 10px;
        font-size: 14px;
      }
  
      .info-label {
        font-weight: bold;
        color: var(--primary);
      }
  
      .info-value {
        color: var(--text);
        word-break: break-word;
      }
  
      @media (max-width: 850px) {
        .span-4,
        .span-6,
        .span-8,
        .span-12 {
          grid-column: span 12;
        }
  
        .info-list {
          grid-template-columns: 1fr;
        }
  
        .value {
          font-size: 30px;
        }
      }
    </style>
  </head>
  
  <body>
    <input type="checkbox" id="menuToggle">
  
    <header class="topbar">
      <label for="menuToggle" class="menu-btn">☰</label>
      <div class="topbar-title">BioMonitor Local</div>
    </header>
  
    <aside class="sidebar">
      <div class="sidebar-header">Menú local</div>
      <a class="nav-item active" href="/monitor" onclick="cerrarMenu()">Monitoreo</a>
      <a class="nav-item" href="/wifi" onclick="cerrarMenu()">Credenciales WiFi</a>
    </aside>
  
    <label for="menuToggle" class="overlay"></label>
  
    <main>
      <section class="page-title">
        <h1>Monitoreo local</h1>
        <p>Visualización sin conexión y control de variables activas.</p>
      </section>
  
      <div class="grid">
  
        <section class="card span-6">
          <h2 class="card-title">Selección de variables</h2>
  
          <div class="check-grid">
            <label class="check-card">
              <input type="checkbox" id="bpm">
              <span>LPM</span>
            </label>
  
            <label class="check-card">
              <input type="checkbox" id="spo2">
              <span>SpO2</span>
            </label>
  
            <label class="check-card">
              <input type="checkbox" id="temp">
              <span>Temperatura</span>
            </label>
  
            <label class="check-card">
              <input type="checkbox" id="ecg">
              <span>ECG</span>
            </label>
          </div>
  
          <div class="actions">
            <button class="btn-secondary" onclick="comenzarMonitoreo()">Comenzar monitoreo</button>
            <button class="btn-danger" onclick="detenerMonitoreo()">Detener</button>
            <button class="btn-light" onclick="guardarSeleccion()">Actualizar selección</button>
          </div>
  
          <br>
          <div class="status-row">
            <strong>Estado del monitoreo</strong>
            <span id="estadoMonitoreo" class="chip chip-warn">--</span>
          </div>
        </section>
  
        <section class="card span-6">
          <h2 class="card-title">Estado de conectividad</h2>
          <div class="info-label">Batería</div>
            <div class="info-value">
              <span id="porcentajeBateria">--</span> %
              (<span id="voltajeBateria">--</span> V)
            </div>
          <div class="info-list">
            <div class="info-label">Modo</div>
            <div class="info-value" id="modoConexion">--</div>
  
            <div class="info-label">Red actual</div>
            <div class="info-value" id="ssidActual">--</div>
  
            <div class="info-label">Dirección IP</div>
            <div class="info-value" id="ipLocal">--</div>
  
            <div class="info-label">mDNS</div>
            <div class="info-value" id="mdnsLocal">--</div>
  
            <div class="info-label">WiFi</div>
            <div class="info-value" id="estadoWiFi">--</div>
  
            <div class="info-label">Internet / nube</div>
            <div class="info-value" id="estadoInternet">--</div>
          </div>
        </section>
  
        <section class="card span-12">
          <h2 class="card-title">Monitoreo</h2>
  
          <div class="monitor-grid">
            <div class="value-card">
              <div class="label">Frecuencia cardíaca</div>
              <div class="value"><span id="valorBPM">--</span><span class="unit">LPM</span></div>
            </div>
  
            <div class="value-card">
              <div class="label">Saturación de oxígeno</div>
              <div class="value"><span id="valorSpO2">--</span><span class="unit">%</span></div>
            </div>
  
            <div class="value-card">
              <div class="label">Temperatura</div>
              <div class="value"><span id="valorTemp">--</span><span class="unit">°C</span></div>
            </div>
  
            <div class="value-card">
              <div class="label">ECG</div>
              <div class="value"><span id="valorECG">--</span><span class="unit">ADC</span></div>
            </div>
          </div>
        </section>
  
        <section class="card span-12">
          <h2 class="card-title">Estado físico de sensores</h2>
  
          <div class="status-grid">
            <div class="status-row">
              <strong>MAX30102</strong>
              <span id="estadoMAX" class="chip chip-warn">--</span>
            </div>
  
            <div class="status-row">
              <strong>Presencia de dedo</strong>
              <span id="estadoDedo" class="chip chip-warn">--</span>
            </div>
  
            <div class="status-row">
              <strong>TMP117</strong>
              <span id="estadoTMP" class="chip chip-warn">--</span>
            </div>
  
            <div class="status-row">
              <strong>Electrodos ECG</strong>
              <span id="estadoElectrodos" class="chip chip-warn">--</span>
            </div>
          </div>
        </section>
  
      </div>
    </main>
  
    <script>
      function cerrarMenu() {
        const menu = document.getElementById('menuToggle');
        if (menu) menu.checked = false;
      }
  
      function setTexto(id, texto) {
        const elemento = document.getElementById(id);
        if (elemento) elemento.textContent = texto;
      }
  
      function setChecked(id, valor) {
        const elemento = document.getElementById(id);
        if (elemento) elemento.checked = valor;
      }
  
      function setDisabled(id, valor) {
        const elemento = document.getElementById(id);
        if (elemento) elemento.disabled = valor;
      }
  
      function setChip(id, estado, textoOk, textoBad) {
        const elemento = document.getElementById(id);
        if (!elemento) return;
  
        elemento.classList.remove('chip-ok', 'chip-bad', 'chip-warn');
  
        if (estado) {
          elemento.textContent = textoOk || 'Correcto';
          elemento.classList.add('chip-ok');
        } else {
          elemento.textContent = textoBad || 'No disponible';
          elemento.classList.add('chip-bad');
        }
      }
  
      function setChipTexto(id, texto, tipo) {
        const elemento = document.getElementById(id);
        if (!elemento) return;
  
        elemento.textContent = texto;
        elemento.classList.remove('chip-ok', 'chip-bad', 'chip-warn');
  
        if (tipo === 'ok') elemento.classList.add('chip-ok');
        else if (tipo === 'bad') elemento.classList.add('chip-bad');
        else elemento.classList.add('chip-warn');
      }
  
      function obtenerSeleccionQuery() {
        const bpm = document.getElementById('bpm').checked ? 1 : 0;
        const spo2 = document.getElementById('spo2').checked ? 1 : 0;
        const temp = document.getElementById('temp').checked ? 1 : 0;
        const ecg = document.getElementById('ecg').checked ? 1 : 0;
  
        return `bpm=${bpm}&spo2=${spo2}&temp=${temp}&ecg=${ecg}`;
      }
  
      function guardarSeleccion() {
        fetch(`/set?${obtenerSeleccionQuery()}`)
          .then(response => response.json())
          .then(data => {
            actualizar();
          })
          .catch(error => console.log("Error actualizando selección:", error));
      }
  
      function comenzarMonitoreo() {
        fetch(`/start?${obtenerSeleccionQuery()}`)
          .then(response => response.json())
          .then(data => {
            actualizar();
          })
          .catch(error => console.log("Error iniciando monitoreo:", error));
      }
  
      function detenerMonitoreo() {
        fetch('/stop')
          .then(response => response.json())
          .then(data => {
            actualizar();
          })
          .catch(error => console.log("Error deteniendo monitoreo:", error));
      }
  
      function actualizar() {
        fetch('/data')
          .then(response => response.json())
          .then(data => {
  
            if (data.monitoreoActivo) {
              setTexto('valorBPM', data.bpm > 0 ? data.bpm : '--');
              setTexto('valorSpO2', data.spo2 > 0 ? data.spo2 : '--');
              setTexto('valorTemp', data.temp > 0 ? Number(data.temp).toFixed(2) : '--');
              setTexto('valorECG', data.ecg > 0 ? data.ecg : '--');
              setChipTexto('estadoMonitoreo', 'Activo', 'ok');
            } else {
              setTexto('valorBPM', '--');
              setTexto('valorSpO2', '--');
              setTexto('valorTemp', '--');
              setTexto('valorECG', '--');
              setChipTexto('estadoMonitoreo', 'Detenido', 'warn');
            }
            setTexto('porcentajeBateria', data.porcentajeBateria !== undefined ? data.porcentajeBateria : '--');
            setTexto('voltajeBateria', data.voltajeBateria !== undefined ? Number(data.voltajeBateria).toFixed(2) : '--');
            if (data.bateriaCritica) {
              setTexto('estadoInternet', 'Batería crítica');
            } else if (data.bateriaBaja) {
              setTexto('estadoInternet', 'Batería baja');
            }
            
            setChip('estadoMAX', data.maxDisponible, 'Detectado', 'No detectado');
            setChip('estadoDedo', data.dedoDetectado, 'Detectado', 'Sin dedo');
            setChip('estadoTMP', data.tmpDisponible, 'Detectado', 'No detectado');
            setChip('estadoElectrodos', data.electrodosConectados, 'Conectados', 'Desconectados');
  
            setTexto('modoConexion', data.wifi || '--');
            setTexto('ssidActual', data.ssid || '--');
            setTexto('ipLocal', data.ip || '--');
            setTexto('mdnsLocal', data.mdns || '--');
  
            setTexto('estadoWiFi', data.wifiDisponible ? 'Conectado' : 'No conectado');
            setTexto('estadoInternet', data.internetDisponible ? 'Disponible' : 'No disponible');
  
            setChecked('bpm', data.medirBPM);
            setChecked('spo2', data.medirSpO2);
            setChecked('temp', data.medirTemp);
            setChecked('ecg', data.medirECG);
  
            setDisabled('bpm', !data.maxDisponible || data.monitoreoActivo);
            setDisabled('spo2', !data.maxDisponible || data.monitoreoActivo);
            setDisabled('temp', !data.tmpDisponible || data.monitoreoActivo);
            setDisabled('ecg', !data.ecgDisponible || data.monitoreoActivo);
          })
          .catch(error => {
            console.log("Error al actualizar datos locales:", error);
            setTexto('modoConexion', 'Sin respuesta del dispositivo');
          });
      }
  
      document.addEventListener("keydown", function(event) {
        if (event.key === "Escape") cerrarMenu();
      });
  
      actualizar();
      setInterval(actualizar, 1500);
    </script>
  </body>
  </html>

  )rawliteral";
  return html;
}

// =====================================================
// PÁGINA WEB REMOTA
// =====================================================

String paginaConfigWiFi()
{
  String modo = obtenerModoConexion();
  String red = obtenerSSIDActual();
  String ip = obtenerIPActual();

  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="es">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Credenciales WiFi</title>
  
    <style>
      :root {
        --primary: #0f4c75;
        --primary-dark: #0b354f;
        --accent: #00a6a6;
        --bg: #f4f8fb;
        --card: #ffffff;
        --soft: #eef6f9;
        --border: #d9e2ec;
        --text: #17212b;
        --muted: #6b7c8f;
        --danger: #d9534f;
        --ok: #2e8b57;
        --warn: #f4a261;
      }
  
      * {
        box-sizing: border-box;
      }
  
      body {
        margin: 0;
        font-family: Arial, Helvetica, sans-serif;
        background: var(--bg);
        color: var(--text);
      }
  
      .topbar {
        height: 60px;
        background: var(--primary);
        color: white;
        display: flex;
        align-items: center;
        padding: 0 18px;
        box-shadow: 0 2px 8px rgba(0,0,0,0.15);
        position: sticky;
        top: 0;
        z-index: 10;
      }
  
      .menu-btn {
        font-size: 28px;
        cursor: pointer;
        margin-right: 16px;
        user-select: none;
      }
  
      .topbar-title {
        font-size: 18px;
        font-weight: bold;
        letter-spacing: 0.4px;
      }
  
      #menuToggle {
        display: none;
      }
  
      .sidebar {
        position: fixed;
        top: 0;
        left: -280px;
        width: 260px;
        height: 100vh;
        background: white;
        box-shadow: 2px 0 14px rgba(0,0,0,0.18);
        z-index: 20;
        transition: left 0.25s ease;
        padding-top: 70px;
      }
  
      #menuToggle:checked ~ .sidebar {
        left: 0;
      }
  
      .sidebar-header {
        position: absolute;
        top: 0;
        left: 0;
        height: 60px;
        width: 100%;
        background: var(--primary-dark);
        color: white;
        display: flex;
        align-items: center;
        padding: 0 18px;
        font-weight: bold;
      }
  
      .nav-item {
        display: block;
        padding: 15px 20px;
        color: var(--text);
        text-decoration: none;
        border-bottom: 1px solid var(--border);
        font-weight: bold;
      }
  
      .nav-item:hover,
      .nav-item.active {
        background: var(--soft);
        color: var(--primary);
      }
  
      .overlay {
        display: none;
        position: fixed;
        inset: 0;
        background: rgba(0,0,0,0.25);
        z-index: 15;
      }
  
      #menuToggle:checked ~ .overlay {
        display: block;
      }
  
      main {
        max-width: 1050px;
        margin: 22px auto;
        padding: 0 16px 30px;
      }
  
      .page-title {
        margin-bottom: 18px;
      }
  
      .page-title h1 {
        margin: 0;
        color: var(--primary);
        font-size: 26px;
      }
  
      .page-title p {
        margin: 6px 0 0;
        color: var(--muted);
      }
  
      .grid {
        display: grid;
        grid-template-columns: repeat(12, 1fr);
        gap: 16px;
      }
  
      .card {
        background: var(--card);
        border: 1px solid var(--border);
        border-radius: 14px;
        padding: 18px;
        box-shadow: 0 2px 8px rgba(0,0,0,0.06);
      }
  
      .span-6 { grid-column: span 6; }
      .span-12 { grid-column: span 12; }
  
      .card-title {
        margin: 0 0 14px;
        font-size: 15px;
        text-transform: uppercase;
        letter-spacing: 1px;
        color: var(--primary);
        font-weight: bold;
      }
  
      .form-group {
        margin-bottom: 14px;
      }
  
      label {
        display: block;
        font-weight: bold;
        margin-bottom: 6px;
        color: var(--text);
      }
  
      input {
        width: 100%;
        border: 1px solid var(--border);
        background: #fff;
        border-radius: 10px;
        padding: 13px;
        font-size: 15px;
        outline: none;
      }
  
      input:focus {
        border-color: var(--accent);
        box-shadow: 0 0 0 3px rgba(0,166,166,0.14);
      }
  
      button,
      .btn {
        border: none;
        border-radius: 10px;
        padding: 12px 16px;
        font-weight: bold;
        cursor: pointer;
        background: var(--primary);
        color: white;
        transition: 0.2s;
        text-decoration: none;
        display: inline-block;
      }
  
      button:hover,
      .btn:hover {
        background: var(--primary-dark);
      }
  
      .btn-danger {
        background: var(--danger);
      }
  
      .btn-light {
        background: var(--soft);
        color: var(--primary);
        border: 1px solid var(--border);
      }
  
      .actions {
        display: flex;
        gap: 10px;
        flex-wrap: wrap;
        margin-top: 10px;
      }
  
      .info-list {
        display: grid;
        grid-template-columns: 160px 1fr;
        gap: 10px;
        font-size: 14px;
      }
  
      .info-label {
        font-weight: bold;
        color: var(--primary);
      }
  
      .info-value {
        color: var(--text);
        word-break: break-word;
      }
  
      .warning-box {
        background: rgba(244,162,97,0.16);
        border: 1px solid rgba(244,162,97,0.45);
        color: #8a4a16;
        padding: 12px;
        border-radius: 10px;
        font-size: 14px;
        margin-top: 12px;
      }
  
      @media (max-width: 850px) {
        .span-6,
        .span-12 {
          grid-column: span 12;
        }
  
        .info-list {
          grid-template-columns: 1fr;
        }
      }
    </style>
  </head>
  
  <body>
    <input type="checkbox" id="menuToggle">
  
    <header class="topbar">
      <label for="menuToggle" class="menu-btn">☰</label>
      <div class="topbar-title">BioMonitor Local</div>
    </header>
  
    <aside class="sidebar">
      <div class="sidebar-header">Menú local</div>
      <a class="nav-item" href="/monitor" onclick="cerrarMenu()">Monitoreo</a>
      <a class="nav-item active" href="/wifi" onclick="cerrarMenu()">Credenciales WiFi</a>
    </aside>
  
    <label for="menuToggle" class="overlay"></label>
  
    <main>
      <section class="page-title">
        <h1>Credenciales WiFi</h1>
        <p>Configuración de red para conexión local y envío de datos a la nube.</p>
      </section>
  
      <div class="grid">
  
        <section class="card span-6">
          <h2 class="card-title">Agregar o actualizar credenciales</h2>
  
          <form action="/guardarWiFi" method="POST">
            <div class="form-group">
              <label for="ssid">Nombre de red WiFi</label>
              <input type="text" id="ssid" name="ssid" placeholder="SSID de la red" required>
            </div>
  
            <div class="form-group">
              <label for="pass">Contraseña</label>
              <input type="password" id="pass" name="pass" placeholder="Contraseña de la red">
            </div>
  
            <div class="actions">
              <button type="submit">Guardar credenciales</button>
              <a class="btn btn-light" href="/monitor">Volver al monitoreo</a>
            </div>
          </form>
  
          <div class="warning-box">
            Al guardar nuevas credenciales, el dispositivo se reiniciará para intentar conectarse a la red indicada.
          </div>
        </section>
  
        <section class="card span-6">
          <h2 class="card-title">Borrar credenciales</h2>
  
          <p>
            Esta opción elimina la red guardada. Después del reinicio, el dispositivo podrá iniciar en modo de configuración local.
          </p>
  
          <form action="/borrarWiFi" method="POST" onsubmit="return confirm('¿Seguro que deseas borrar las credenciales WiFi guardadas?');">
            <button class="btn-danger" type="submit">Borrar credenciales guardadas</button>
          </form>
  
          <div class="warning-box">
            Usa esta opción si cambiaste de red, si la contraseña ya no es válida o si deseas volver a configurar el dispositivo desde cero.
          </div>
        </section>
  
        <section class="card span-12">
          <h2 class="card-title">Estado de conexión de red</h2>
          <div class="info-list">
            <div class="info-label">Modo</div>
            <div class="info-value" id="modoConexion">--</div>
  
            <div class="info-label">Red actual</div>
            <div class="info-value" id="ssidActual">--</div>
  
            <div class="info-label">Dirección IP</div>
            <div class="info-value" id="ipLocal">--</div>
  
            <div class="info-label">mDNS</div>
            <div class="info-value" id="mdnsLocal">--</div>
  
            <div class="info-label">WiFi disponible</div>
            <div class="info-value" id="wifiDisponible">--</div>
  
            <div class="info-label">Internet / nube</div>
            <div class="info-value" id="internetDisponible">--</div>
          </div>
        </section>
  
      </div>
    </main>
  
    <script>
      function cerrarMenu() {
        const menu = document.getElementById('menuToggle');
        if (menu) menu.checked = false;
      }
  
      function setTexto(id, texto) {
        const elemento = document.getElementById(id);
        if (elemento) elemento.textContent = texto;
      }
  
      function actualizarConexion() {
        fetch('/data')
          .then(response => response.json())
          .then(data => {
            setTexto('modoConexion', data.wifi || '--');
            setTexto('ssidActual', data.ssid || '--');
            setTexto('ipLocal', data.ip || '--');
            setTexto('mdnsLocal', data.mdns || '--');
            setTexto('wifiDisponible', data.wifiDisponible ? 'Sí' : 'No');
            setTexto('internetDisponible', data.internetDisponible ? 'Sí' : 'No');
          })
          .catch(error => {
            console.log("Error consultando estado de conexión:", error);
            setTexto('modoConexion', 'Sin respuesta del dispositivo');
          });
      }
  
      document.addEventListener("keydown", function(event) {
        if (event.key === "Escape") cerrarMenu();
      });
  
      actualizarConexion();
      setInterval(actualizarConexion, 2500);
    </script>
  </body>
  </html>

  )rawliteral";
  return html;
}

// =====================================================
// MANEJADORES HTTP LOCALES
// =====================================================

void redirigir(const char *ruta)
{
  server.sendHeader("Location", ruta, true);
  server.sendHeader("Cache-Control", "no-store");
  server.send(303, "text/plain", "");
}

void handleRoot()
{
  if (modoConfigWiFi)
  {
    server.send(200, "text/html", paginaConfigWiFi());
  }
  else
  {
    server.send(200, "text/html", paginaHTML());
  }
}

void handleMonitor()
{
  server.send(200, "text/html", paginaHTML());
}

void handleWiFiConfig()
{
  server.send(200, "text/html", paginaConfigWiFi());
}

void handleGuardarWiFi()
{
  if (server.method() != HTTP_POST)
  {
    redirigir("/wifi");
    return;
  }

  if (!server.hasArg("ssid"))
  {
    server.send(400, "text/plain", "Falta SSID");
    return;
  }

  String nuevoSSID = server.arg("ssid");
  String nuevoPass = server.arg("pass");

  nuevoSSID.trim();
  nuevoPass.trim();

  if (nuevoSSID.length() == 0)
  {
    server.send(400, "text/plain", "SSID invalido");
    return;
  }

  guardarCredencialesWiFi(nuevoSSID, nuevoPass);

  reinicioPendiente = true;
  inicioReinicio = millis();

  redirigir("/reiniciando?accion=guardar");
}

void handleBorrarWiFi()
{
  if (server.method() != HTTP_POST)
  {
    redirigir("/wifi");
    return;
  }

  borrarCredencialesWiFi();

  monitoreoActivo = false;
  modoNube = false;
  wifiDisponible = false;
  internetDisponible = false;

  reinicioPendiente = true;
  inicioReinicio = millis();

  redirigir("/reiniciando?accion=borrar");
}

void handleData()
{
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", generarJSON());
}

void handleSet()
{
  leerSeleccionDesdeServidor();

  server.send(200, "application/json",
              "{\"ok\":true,\"mensaje\":\"Seleccion actualizada\"}");
}

void handleStart()
{
  leerSeleccionDesdeServidor();

  if (!haySeleccionValida())
  {
    monitoreoActivo = false;

    server.send(400, "application/json",
                "{\"ok\":false,\"mensaje\":\"No hay variables disponibles seleccionadas\"}");
    return;
  }

  monitoreoActivo = true;
  lastCloudSend = millis();

  server.send(200, "application/json",
              "{\"ok\":true,\"mensaje\":\"Monitoreo iniciado\"}");
}

void handleStop()
{
  monitoreoActivo = false;

  server.send(200, "application/json",
              "{\"ok\":true,\"mensaje\":\"Monitoreo detenido\"}");
}

void handleReiniciando()
{
  String accion = server.arg("accion");

  String mensaje = "Configuracion aplicada";

  if (accion == "guardar")
  {
    mensaje = "Credenciales WiFi guardadas";
  }
  else if (accion == "borrar")
  {
    mensaje = "Credenciales WiFi borradas";
  }

  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <title>Reiniciando</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background: #f4f6f8;
      color: #222;
      padding: 20px;
      text-align: center;
    }
    .card {
      max-width: 430px;
      margin: 60px auto;
      background: white;
      padding: 24px;
      border-radius: 12px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.12);
    }
    h1 {
      color: #134b70;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>Reiniciando dispositivo</h1>
)rawliteral";

  html += "<p>" + mensaje + ".</p>";

  html += R"rawliteral(
    <p>Espera unos segundos y vuelve a ingresar a la página principal.</p>
    <p>Si el dispositivo entra en modo SoftAP, usa:</p>
    <b>http://192.168.4.1</b>
    <p>Si se conecta al router, intenta:</p>
    <b>http://biomonitor.local</b>
  </div>

  <script>
    history.replaceState(null, "", "/");
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// =====================================================
// CONEXIÓN WIFI
// =====================================================
bool cargarCredencialesWiFi()
{
  prefs.begin("wifi", true);

  ssidGuardado = prefs.getString("ssid", "");
  passGuardada = prefs.getString("pass", "");

  prefs.end();

  hayWiFiGuardado = ssidGuardado.length() > 0;

  if (hayWiFiGuardado)
  {
    Serial.println("Credenciales WiFi encontradas");
    Serial.print("SSID guardado: ");
    Serial.println(ssidGuardado);
  }
  else
  {
    Serial.println("No hay credenciales WiFi guardadas");
  }

  return hayWiFiGuardado;
}

void guardarCredencialesWiFi(String ssid, String pass)
{
  prefs.begin("wifi", false);

  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);

  prefs.end();

  Serial.println("Credenciales WiFi guardadas");
}

void borrarCredencialesWiFi()
{
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();

  Serial.println("Credenciales WiFi borradas");
}

bool conectarWiFiGuardado()
{
  Serial.println("Intentando conectar con credenciales guardadas...");

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssidGuardado.c_str(), passGuardada.c_str());

  unsigned long inicio = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    detenerSoftAP();
    wifiDisponible = true;
    modoAP = false;

    Serial.println("WiFi conectado");
    Serial.print("IP local: ");
    Serial.println(WiFi.localIP());

    return true;
  }

  wifiDisponible = false;

  Serial.println("No se pudo conectar al WiFi guardado");
  return false;
}

void configurarConectividad()
{
  modoAP = false;
  modoConfigWiFi = false;
  modoNube = false;
  wifiDisponible = false;
  internetDisponible = false;

  if (cargarCredencialesWiFi())
  {

    if (conectarWiFiGuardado())
    {

      if (verificarInternet())
      {
        modoNube = true;

        Serial.println("Modo operativo: WiFi + nube + monitoreo local");
      }
      else
      {
        modoNube = false;

        Serial.println("Modo operativo: WiFi local sin internet");
      }
    }
    else
    {
      Serial.println("Hay credenciales, pero no se encontro la red WiFi");
      Serial.println("Se inicia SoftAP de respaldo para monitoreo local");

      iniciarModoAP(false);
    }
  }
  else
  {
    Serial.println("Sin credenciales WiFi");
    Serial.println("Se inicia SoftAP de configuracion");

    iniciarModoAP(true);
  }
}

bool verificarInternet()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    internetDisponible = false;
    return false;
  }

  HTTPClient http;

  String url = String(serverURL) + "?ping=1";

  http.begin(url);
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int codigo = http.GET();

  http.end();

  Serial.print("Verificacion de internet HTTP: ");
  Serial.println(codigo);

  if (codigo > 0)
  {
    internetDisponible = true;
    Serial.println("Internet disponible");
    return true;
  }

  internetDisponible = false;
  Serial.println("Sin acceso a internet");
  return false;
}
// =====================================================
// INICIAR SOFTAP
// =====================================================
void iniciarModoAP(bool modoConfiguracion)
{
  WiFi.mode(WIFI_AP);

  WiFi.softAP(AP_SSID, AP_PASS);

  modoAP = true;
  wifiDisponible = false;
  internetDisponible = false;
  modoNube = false;
  modoConfigWiFi = modoConfiguracion;

  Serial.println("Modo SoftAP iniciado");
  Serial.print("Red WiFi creada: ");
  Serial.println(AP_SSID);
  Serial.print("Password: ");
  Serial.println(AP_PASS);
  Serial.print("IP SoftAP: ");
  Serial.println(WiFi.softAPIP());
}

void iniciarMDNS()
{
  if (MDNS.begin(MDNS_NAME))
  {
    MDNS.addService("http", "tcp", 80);

    Serial.print("mDNS iniciado: http://");
    Serial.print(MDNS_NAME);
    Serial.println(".local");
  }
  else
  {
    Serial.println("No se pudo iniciar mDNS");
  }
}

void detenerSoftAP()
{
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA || modoAP || softAPRespaldoActivo)
  {
    Serial.println("Deteniendo SoftAP activo...");

    WiFi.softAPdisconnect(true);
    delay(300);

    WiFi.mode(WIFI_STA);

    modoAP = false;
    modoConfigWiFi = false;
    softAPRespaldoActivo = false;

    Serial.println("SoftAP detenido. Modo actual: WIFI_STA");
  }
}
// =====================================================
// INICIAR SERVIDOR LOCAL
// =====================================================
void iniciarServidorLocal()
{
  server.on("/", HTTP_GET, handleRoot);
  server.on("/monitor", HTTP_GET, handleMonitor);
  server.on("/wifi", HTTP_GET, handleWiFiConfig);

  server.on("/guardarWiFi", HTTP_POST, handleGuardarWiFi);
  server.on("/guardarWiFi", HTTP_GET, []()
            { redirigir("/wifi"); });

  server.on("/borrarWiFi", HTTP_POST, handleBorrarWiFi);
  server.on("/borrarWiFi", HTTP_GET, []()
            { redirigir("/wifi"); });

  server.on("/reiniciando", HTTP_GET, handleReiniciando);

  server.on("/data", HTTP_GET, handleData);
  server.on("/set", HTTP_GET, handleSet);
  server.on("/start", HTTP_GET, handleStart);
  server.on("/stop", HTTP_GET, handleStop);

  server.begin();

  Serial.println("Servidor HTTP local iniciado");
}

// =====================================================
// ENVÍO A GOOGLE APPS SCRIPT
// =====================================================
bool datosValidosParaNube()
{
  if (!monitoreoActivo)
  {
    return false;
  }

  if (!modoNube)
  {
    return false;
  }

  bool hayDatoNube = false;

  if (medirBPM || medirSpO2)
  {
    if (!maxDisponible || !dedoDetectado)
    {
      return false;
    }

    if (medirBPM && bpmSeguro <= 0)
    {
      return false;
    }

    if (medirSpO2 && spo2Seguro <= 0)
    {
      return false;
    }

    hayDatoNube = true;
  }

  if (medirTemp)
  {
    if (!tmpDisponible || temperaturaCalibrada <= 0)
    {
      return false;
    }

    hayDatoNube = true;
  }

  if (medirECG)
  {
    if (!ecgDisponible || !electrodosConectados)
      return false;

    String bufferECG = obtenerBufferECG();

    if (bufferECG.length() == 0)
      return false;

    hayDatoNube = true;
  }

  return hayDatoNube;
}

void enviarDatosNube()
{
  if (!modoNube)
  {
    return;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    wifiDisponible = false;
    internetDisponible = false;
    modoNube = false;

    Serial.println("WiFi perdido, se desactiva envio a nube");
    return;
  }

  if (!datosValidosParaNube())
  {
    return;
  }

  if (millis() - lastCloudSend < CLOUD_INTERVAL)
  {
    return;
  }

  lastCloudSend = millis();

  HTTPClient http;

  String batParam = String(porcentajeBateria);
  String vbatParam = String(voltajeBateria, 2);
  String bpmParam = (medirBPM && bpmSeguro > 0) ? String(bpmSeguro) : "";
  String spo2Param = (medirSpO2 && spo2Seguro > 0) ? String(spo2Seguro) : "";
  String tempParam = (medirTemp && temperaturaCalibrada > 0) ? String(temperaturaCalibrada, 2) : "";
  String ecgParam = "";

  if (medirECG && ecgDisponible && electrodosConectados)
  {
    ecgParam = obtenerBufferECG();
  }

  String url = String(serverURL);
  url += "?accion=registrar";
  url += "&id=" + pacienteID;
  url += "&bpm=" + bpmParam;
  url += "&spo2=" + spo2Param;
  url += "&temp=" + tempParam;
  url += "&bat=" + String(porcentajeBateria);
  url += "&vbat=" + String(voltajeBateria, 2);
  url += "&ecg=" + ecgParam;

  Serial.println("URL enviada:");
  Serial.println(url);

  http.begin(url);
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int httpCode = http.GET();

  Serial.print("Envio nube HTTP: ");
  Serial.println(httpCode);

  if (httpCode > 0)
  {
    String respuesta = http.getString();
    Serial.print("Respuesta: ");
    Serial.println(respuesta);
  }
  else
  {
    Serial.print("Error HTTP: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}

// =====================================================
// IMPRESIÓN SERIAL
// =====================================================

void imprimirSerial()
{
  if (millis() - lastSerialPrint < SERIAL_INTERVAL)
  {
    return;
  }

  lastSerialPrint = millis();

  Serial.println("========== DATOS ==========");
  Serial.print("MAX30102 disponible: ");
  Serial.println(maxDisponible ? "SI" : "NO");

  Serial.print("Dedo detectado: ");
  Serial.println(dedoDetectado ? "SI" : "NO");

  Serial.print("BPM: ");
  Serial.println(bpmSeguro);

  Serial.print("SpO2: ");
  Serial.println(spo2Seguro);

  Serial.print("TMP117 disponible: ");
  Serial.println(tmpDisponible ? "SI" : "NO");

  Serial.print("Temperatura: ");
  Serial.print(temperaturaCalibrada, 2);
  Serial.println(" °C");

  Serial.print("Electrodos conectados: ");
  Serial.println(electrodosConectados ? "SI" : "NO");

  Serial.print("ECG ADC: ");
  Serial.println(ecgValue);

  Serial.print("WiFi disponible: ");
  Serial.println(wifiDisponible ? "SI" : "NO");

  Serial.print("Internet disponible: ");
  Serial.println(internetDisponible ? "SI" : "NO");

  Serial.print("Modo AP: ");
  Serial.println(modoAP ? "SI" : "NO");

  Serial.print("Modo nube: ");
  Serial.println(modoNube ? "SI" : "NO");

  if (modoAP)
  {
    Serial.print("IP SoftAP: ");
    Serial.println(WiFi.softAPIP());
  }
  else
  {
    Serial.print("IP local: ");
    Serial.println(WiFi.localIP());
  }

  Serial.println("===========================");
}

// =====================================================
// MEDICION DE BATERIA
// =====================================================
int estimarPorcentajeLiIon(float voltaje)
{
  const int puntos = 11;

  float voltajes[puntos] = {
      4.20, 4.10, 4.00, 3.92, 3.85,
      3.79, 3.73, 3.68, 3.60, 3.50, 3.30};

  int porcentajes[puntos] = {
      100, 90, 80, 70, 60,
      50, 40, 30, 20, 10, 0};

  if (voltaje >= 4.20)
    return 100;
  if (voltaje <= 3.30)
    return 0;

  for (int i = 0; i < puntos - 1; i++)
  {
    if (voltaje <= voltajes[i] && voltaje >= voltajes[i + 1])
    {
      float v1 = voltajes[i];
      float v2 = voltajes[i + 1];

      int p1 = porcentajes[i];
      int p2 = porcentajes[i + 1];

      float porcentaje = p1 + ((voltaje - v1) * (p2 - p1)) / (v2 - v1);

      return constrain(round(porcentaje), 0, 100);
    }
  }

  return 0;
}

void actualizarBateria()
{
  if (millis() - lastBatteryRead < BATTERY_INTERVAL)
    return;

  lastBatteryRead = millis();

  const int muestras = 30;
  unsigned long sumaMv = 0;

  for (int i = 0; i < muestras; i++)
  {
    sumaMv += analogReadMilliVolts(BATTERY_PIN);
    delay(2);
  }

  float voltajeADC = (sumaMv / (float)muestras) / 1000.0;

  voltajeBateria = voltajeADC * FACTOR_DIVISOR_BAT * FACTOR_CALIBRACION_BAT;

  porcentajeBateria = estimarPorcentajeLiIon(voltajeBateria);

  bateriaBaja = porcentajeBateria <= 20;
  bateriaCritica = porcentajeBateria <= 10;

  Serial.print("Voltaje batería: ");
  Serial.print(voltajeBateria, 2);
  Serial.print(" V | Batería: ");
  Serial.print(porcentajeBateria);
  Serial.println(" %");
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);
  delay(3000);

  Serial.println();
  Serial.println("Iniciando prototipo biomédico...");

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_PIN, ADC_11db);
  analogSetPinAttenuation(ECG_PIN, ADC_11db);

  pinMode(ECG_LO_PLUS, INPUT);
  pinMode(ECG_LO_MINUS, INPUT);

  pinMode(ECG_SDN, OUTPUT);
  digitalWrite(ECG_SDN, HIGH);

  verificarTMP117();
  verificarMAX30102();
  inicializarECG();

  configurarConectividad();
  iniciarServidorLocal();
  iniciarMDNS();

  Serial.println("Sistema iniciado");
}

// =====================================================
// LOOP PRINCIPAL
// =====================================================

void loop()
{
  server.handleClient();

  supervisarConectividad();

  actualizarMAX30102();
  actualizarTemperatura();
  actualizarECG();

  actualizarBateria();

  enviarDatosNube();
  imprimirSerial();

  if (reinicioPendiente && millis() - inicioReinicio >= TIEMPO_REINICIO)
  {
    ESP.restart();
  }
}
