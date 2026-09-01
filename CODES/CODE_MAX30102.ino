// =======================================================
// PRUEBA BÁSICA DEL SENSOR MAX30102 CON ESP32-C3
// Lectura de valores crudos IR y RED por comunicación I2C
// =======================================================

// Biblioteca para comunicación I2C
#include <Wire.h>

// Biblioteca de SparkFun para sensores de la familia MAX3010x.
#include "MAX30105.h"

// Biblioteca para detección de latidos.
#include "heartRate.h"

// Biblioteca con algoritmo para cálculo de SpO2.
#include "spo2_algorithm.h"

// Se crea un objeto llamado particleSensor.
// Este objeto permite comunicarse con el MAX30102 y controlar sus funciones.
MAX30105 particleSensor;

// =========================
//  VARIABLES DE CONTROL FC
// =========================

// Cantidad de muestras usadas para promediar la frecuencia cardíaca.
const byte RATE_SIZE = 8;

// Arreglo donde posteriormente se podrían guardar lecturas de BPM.
byte rates[RATE_SIZE];

// Posición actual dentro del arreglo de BPM.
byte rateSpot = 0;

// Guarda el tiempo del último latido detectado.
// En esta prueba no se usa porque no se calcula BPM.
long lastBeat = 0;

// Variable para guardar la frecuencia cardíaca instantánea.
float beatsPerMinute = 0;

// Variable para guardar una versión filtrada de BPM.
float filteredBPM = 0;

// Variable para guardar el promedio de BPM.
int beatAvg = 0;

// ===========================
//  VARIABLES DE CONTROL SpO2
// ===========================

// Tamaño del buffer usado para almacenar muestras de IR y RED.
// Estas muestras se usarían para calcular SpO2 en una versión posterior.
#define BUFFER_SIZE 50

// Buffer para guardar muestras del LED infrarrojo.
uint32_t irBuffer[BUFFER_SIZE];

// Buffer para guardar muestras del LED rojo.
uint32_t redBuffer[BUFFER_SIZE];

// Variable donde se almacenaría el valor calculado de SpO2.
int32_t spo2;

// Bandera que indicaría si el cálculo de SpO2 fue válido.
int8_t validSPO2;

// Cantidad de muestras usadas para promediar SpO2.
const byte SPO2_SIZE = 8;

// Arreglo donde posteriormente se podrían guardar valores de SpO2.
int spo2Buffer[SPO2_SIZE];

// Posición actual dentro del arreglo de SpO2.
byte spo2Spot = 0;

// Valor inicial mínimo para el filtro de SpO2.
float filteredSpO2 = 70;

// Valor promedio inicial de SpO2.
int avgSpO2 = 70;

// Variables auxiliares para el algoritmo de SpO2.
// Se declaran porque la función de SpO2 también puede calcular frecuencia cardíaca.
int32_t dummyHR;
int8_t dummyValid;

// Índice para recorrer los buffers de muestras.
byte bufferIndex = 0;

// =======================
//          SETUP
// =======================

void setup()
{

  // Inicializa la comunicación serial con la computadora.
  // El Monitor Serial debe configurarse a 115200 baudios.
  Serial.begin(115200);

  // Inicializa el bus I2C en el ESP32-C3.
  // GPIO8 se usa como SDA.
  // GPIO9 se usa como SCL.
  Wire.begin(8, 9);

  // Intenta inicializar el sensor MAX30102 usando el bus I2C.
  // I2C_SPEED_STANDARD normalmente corresponde a 100 kHz.
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD))
  {

    // Si el sensor no responde, se imprime un mensaje de error.
    Serial.println("MAX30102 no encontrado");

    // El programa se detiene aquí para evitar continuar sin sensor.
    while (1)
      ;
  }

  // Si el sensor responde correctamente, se confirma por Monitor Serial.
  Serial.println("MAX30102 encontrado");

  // =======================================================
  // CONFIGURACIÓN MANUAL DEL MAX30102
  // =======================================================

  // Intensidad inicial de los LEDs.
  // Rango aproximado: 0 a 255.
  byte ledBrightness = 60;

  // Número de muestras que el sensor promedia internamente.
  // Un promedio mayor suaviza la señal, pero reduce respuesta rápida.
  byte sampleAverage = 4;

  // Modo de operación de los LEDs.
  // 2 significa que se activan LED rojo e infrarrojo.
  byte ledMode = 2;

  // Frecuencia de muestreo interna del sensor en muestras por segundo.
  int sampleRate = 100;

  // Ancho de pulso de los LEDs en microsegundos.
  // Un ancho mayor permite mayor resolución, pero incrementa consumo.
  int pulseWidth = 411;

  // Rango del ADC interno del sensor.
  int adcRange = 4096;

  // Aplica la configuración anterior al sensor.
  particleSensor.setup(
      ledBrightness, // Intensidad LED
      sampleAverage, // Promedio de muestras
      ledMode,       // Modo LED: rojo + infrarrojo
      sampleRate,    // Frecuencia de muestreo
      pulseWidth,    // Ancho de pulso
      adcRange       // Rango ADC
  );

  // Ajusta manualmente la amplitud del LED rojo.
  // 0x32 equivale a 50 en decimal.
  particleSensor.setPulseAmplitudeRed(0x32);

  // Ajusta manualmente la amplitud del LED infrarrojo.
  // 0x32 equivale a 50 en decimal.
  particleSensor.setPulseAmplitudeIR(0x32);
}

// =======================
//           LOOP
// =======================

void loop()
{

  // Lee el valor crudo correspondiente al LED infrarrojo.
  long irValue = particleSensor.getIR();

  // Lee el valor crudo correspondiente al LED rojo.
  // Esta señal se usa junto con la infrarroja para estimar SpO2.
  long redValue = particleSensor.getRed();

  // =======================
  // VALIDACIÓN DE DEDO
  // =======================

  // Con ayuda del valor del del infrarrojo se puede detectar si el dedo está colocado sobre el sensor.
  bool dedoDetectado = (irValue > 50000);

  // Se pregunta si se detecta un dedo colocado, si no lo hay, se limpian los valores.
  //  Esto evita mostrar valores anteriores cuando el usuario ya retiró el dedo.
  if (!dedoDetectado)
  {
    beatAvg = 0;
    avgSpO2 = 0;
    bpmSafe = 0;
    spo2Safe = 0;
  }

  // checkForBeat(irValue) analiza la señal infrarroja y detecta cambios
  // asociados al pulso sanguíneo.
  if (dedoDetectado && checkForBeat(irValue))
  {

    // Se calcula el tiempo transcurrido desde el último latido detectado.
    // millis() entrega el tiempo actual desde que inició el programa.
    long delta = millis() - lastBeat;

    // Se actualiza el tiempo del último latido.
    lastBeat = millis();

    // Se calcula la frecuencia cardíaca en latidos por minuto.
    // delta está en milisegundos, por eso se divide entre 1000.0
    // para convertirlo a segundos.
    // BPM = 60 / periodo_en_segundos
    float bpm = 60 / (delta / 1000.0);

    // Se validan los BPM calculados.
    // Solo se aceptan valores entre 50 y 150 LPM para descartar
    // lecturas erróneas o picos falsos.
    if (bpm > 50 && bpm < 150)
    {

      // Filtro exponencial para suavizar la lectura de BPM.
      // 65 % corresponde al valor filtrado anterior.
      // 35 % corresponde al nuevo valor calculado.
      //
      // Esto reduce saltos bruscos en la medición.
      filteredBPM = (0.65 * filteredBPM) + (0.35 * bpm);

      // Se guarda el valor filtrado dentro del arreglo de muestras.
      // Convierte el valor a entero de 8 bits.
      rates[rateSpot++] = (byte)filteredBPM;

      // Se reinicia el índice cuando llega al tamaño máximo del arreglo.
      // Esto permite usar el arreglo como buffer circular.
      rateSpot %= RATE_SIZE;

      // Variable para sumar los valores almacenados en el arreglo.
      int sum = 0;

      // Se recorren las muestras guardadas en el arreglo rates[].
      for (byte i = 0; i < RATE_SIZE; i++)
      {
        sum += rates[i];
      }

      // Esta línea calcula un promedio simple de BPM.
      beatAvg = sum / RATE_SIZE;

      // Se asigna el BPM filtrado como valor seguro de frecuencia cardíaca.
      bpmSafe = beatAvg;
    }
  }

    // =======================
    // SpO2 BUFFER
    // =======================

    // Índice estático para guardar muestras en los buffers.
    // Al ser static, conserva su valor entre iteraciones del loop().
    static byte sampleIndex = 0;

    // Se guarda la lectura infrarroja actual en el buffer IR.
    irBuffer[sampleIndex] = irValue;

    // Se guarda la lectura roja actual en el buffer RED.
    redBuffer[sampleIndex] = redValue;

    // Se avanza a la siguiente posición del buffer.
    sampleIndex++;

    // Cuando se han almacenado 25 muestras, se ejecuta el cálculo de SpO2.
    if (sampleIndex >= 25)
    {

      // Se reinicia el índice para volver a llenar el buffer desde el inicio.
      sampleIndex = 0;

      // Función del algoritmo de Maxim para estimar frecuencia cardíaca y SpO2.
      //
      // Parámetros:
      // irBuffer        → arreglo con muestras infrarrojas
      // 25              → cantidad de muestras usadas
      // redBuffer       → arreglo con muestras rojas
      // &spo2           → variable donde se guarda el valor calculado de SpO2
      // &validSPO2      → bandera que indica si la SpO2 calculada es válida
      // &heartRate      → variable donde se guarda la frecuencia cardíaca calculada por el algoritmo
      // &validHeartRate → bandera que indica si la frecuencia cardíaca calculada es válida
      maxim_heart_rate_and_oxygen_saturation(
          irBuffer,
          25,
          redBuffer,
          &spo2,
          &validSPO2,
          &heartRate,
          &validHeartRate);

      // Se valida el valor de SpO2 calculado.
      // Solo se aceptan valores dentro de un rango fisiológico aproximado.
      // Esto ayuda a descartar resultados erróneos.
      if (spo2 > 85 && spo2 < 100)
      {

        // Filtro exponencial para suavizar la lectura de SpO2.
        // 60 % corresponde al promedio anterior.
        // 40 % corresponde al nuevo valor calculado.
        avgSpO2 = (0.6 * avgSpO2) + (0.4 * spo2);

        // Se asigna el valor filtrado como SpO2 segura.
        spo2Safe = avgSpO2;
      }
      // Espera 100 ms antes de volver a leer e imprimir.
      // Esto produce aproximadamente 10 lecturas visibles por segundo.
      delay(100);
    }
  }
