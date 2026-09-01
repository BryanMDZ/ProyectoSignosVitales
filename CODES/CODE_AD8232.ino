// ===============================
// PRUEBA ECG AD8232 - ESP32-C3
// Visualización de la señal ECG en Serial Plotter
// ===============================

// ===============================
// PINES DEL AD8232
// ===============================

// Pin analógico donde se conecta la salida OUT del AD8232
#define ECG_PIN      4

// Pines digitales para detectar si los electrodos están desconectados
#define ECG_LO_PLUS  2
#define ECG_LO_MINUS 3

// Pin de habilitación del AD8232
#define ECG_SDN      1


// ===============================
// MODO DE VISUALIZACIÓN ECG
// ===============================
// 0 = señal cruda centrada en 0
// 1 = señal filtrada centrada en 0

#define MODO_ECG 1


// ===============================
// CONFIGURACIÓN DE MUESTREO
// ===============================

// Periodo de muestreo de la señal ECG.
// 10000 us equivale aproximadamente a 100 Hz.
// Para 250 Hz se usarían 4000 us.
const unsigned long ECG_SAMPLE_PERIOD_US = 10000;

// Variable para controlar el instante de la última muestra
unsigned long lastECGSample = 0;


// ===============================
// VARIABLES DE SEÑAL ECG
// ===============================

// Valor crudo leído desde el ADC del ESP32
int ecgRaw = 0;

// Variable reservada para graficar la señal
int ecgPlot = 0;

// Valor de referencia o línea base de la señal
float baseline = 0.0;

// Señal ECG después del filtrado digital
float ecgFiltrado = 0.0;


// ===============================
// AJUSTES DE FILTRADO DIGITAL
// ===============================

// Factor usado para estimar la línea base.
// Valores cercanos a 1 hacen que la línea base cambie más lento.
// Con 0.5 la línea base se actualiza relativamente rápido.
const float ALPHA_BASELINE = 0.5;

// Factor de suavizado pasa bajas.
// Valores mayores responden más rápido, pero dejan pasar más ruido.
// Valores menores suavizan más la señal.
const float ALPHA_LP = 0.05;


// ===============================
// SETUP
// ===============================

void setup() {

  // Inicializa la comunicación serial para visualizar la señal
  // en el Monitor Serial o Serial Plotter.
  Serial.begin(115200);

  // Configura los pines de detección de electrodos como entradas.
  pinMode(ECG_LO_PLUS, INPUT);
  pinMode(ECG_LO_MINUS, INPUT);

  // Configura el pin SDN como salida y habilita el AD8232.
  pinMode(ECG_SDN, OUTPUT);
  digitalWrite(ECG_SDN, HIGH);

  // Configura el ADC del ESP32 a 12 bits.
  analogReadResolution(12);

  // Ajusta la atenuación del ADC para permitir un mayor rango de lectura.
  analogSetPinAttenuation(ECG_PIN, ADC_11db);

  // Pequeña espera inicial para estabilizar el sistema.
  delay(1000);
}


// ===============================
// LOOP PRINCIPAL
// ===============================

void loop() {

  // Controla que la lectura del ECG se realice cada cierto periodo.
  if (micros() - lastECGSample >= ECG_SAMPLE_PERIOD_US) {

    // Actualiza el tiempo de la última muestra.
    lastECGSample = micros();

    // Verifica si alguno de los electrodos está desconectado.
    bool electrodoDesconectado =
      digitalRead(ECG_LO_PLUS) == HIGH ||
      digitalRead(ECG_LO_MINUS) == HIGH;

    // Si hay un electrodo desconectado, se reinicia la señal
    // y se imprime cero para mantener la gráfica centrada.
    if (electrodoDesconectado) {
      ecgRaw = 0;
      ecgFiltrado = 0;

      Serial.println(0);
      return;
    }

    // Lee la señal analógica proveniente del AD8232.
    ecgRaw = analogRead(ECG_PIN);

    // Calcula una línea base de referencia para centrar la señal.
    baseline = ALPHA_BASELINE * baseline + (1.0 - ALPHA_BASELINE) * ecgRaw;

    // Resta la línea base para obtener una señal centrada en cero.
    float ecgRawCentrado = ecgRaw - baseline;

    // Aplica un filtro digital pasa bajas para suavizar la señal.
    ecgFiltrado = ecgFiltrado + ALPHA_LP * (ecgRawCentrado - ecgFiltrado);

    // Ganancia aplicada solo para mejorar la visualización.
    // No modifica la señal física, únicamente su escala en pantalla.
    float gananciaVisual = 4.0;

    // Según el modo seleccionado, se imprime la señal cruda centrada
    // o la señal filtrada centrada.
    #if MODO_ECG == 0

      // Muestra la señal cruda centrada en cero.
      Serial.println(ecgRawCentrado * gananciaVisual);

    #elif MODO_ECG == 1

      // Muestra la señal filtrada centrada en cero.
      Serial.println(ecgFiltrado * gananciaVisual);

    #endif
  }
}
