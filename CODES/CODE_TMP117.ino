// =======================================================
// PRUEBA BÁSICA DEL SENSOR TMP117 CON ESP32-C3 SUPERMINI
// Lectura de temperatura por comunicación I2C
// =======================================================

#include <Wire.h>

// ===============================
// PINES I2C ESP32-C3 SUPERMINI
// ===============================

// Pines usados para el bus I2C
#define SDA_PIN 8
#define SCL_PIN 9

// ===============================
// DIRECCIONES DEL TMP117
// ===============================

// Dirección I2C típica del TMP117.
// Puede cambiar a 0x49, 0x4A o 0x4B dependiendo del pin ADDR.
#define TMP117_ADDRESS 0x48

// Registro de temperatura del TMP117.
#define TMP117_TEMP_REG 0x00

// Resolución del TMP117.
// Cada bit equivale a 0.0078125 °C.
#define TMP117_RESOLUTION 0.0078125

// ===============================
// SETUP
// ===============================

void setup() {
  // Inicializa el Monitor Serial.
  Serial.begin(115200);

  // Pequeña espera para permitir abrir el Monitor Serial.
  delay(1000);

  Serial.println("Prueba TMP117 - ESP32-C3 SuperMini");

  // Inicializa el bus I2C usando GPIO8 como SDA y GPIO9 como SCL.
  Wire.begin(SDA_PIN, SCL_PIN);

  // Configura la velocidad del bus I2C a 100 kHz.
  Wire.setClock(100000);

  // Verifica si el TMP117 responde en la dirección configurada.
  Wire.beginTransmission(TMP117_ADDRESS);

  if (Wire.endTransmission() == 0) {
    Serial.println("TMP117 encontrado correctamente");
  } else {
    Serial.println("TMP117 no encontrado");
    Serial.println("Revisar conexion VCC, GND, SDA, SCL o direccion I2C");
    while (1);
  }
}

// ===============================
// LOOP
// ===============================

void loop() {
  float temperatura = 0.0;

  // Se llama a la función de lectura.
  bool lecturaCorrecta = leerTemperaturaTMP117(temperatura);

  if (lecturaCorrecta) {
    // Si la lectura fue correcta, se muestra la temperatura.
    Serial.print("Temperatura: ");
    Serial.print(temperatura, 2);
    Serial.println(" °C");
  } else {
    // Si hubo error de comunicación, se muestra advertencia.
    Serial.println("Error al leer el TMP117");
  }

  // Espera 1 segundo antes de realizar una nueva lectura.
  delay(1000);
}

// =======================================================
// FUNCIÓN DE LECTURA DEL TMP117
// =======================================================

bool leerTemperaturaTMP117(float &temperaturaC) {
  // Se inicia comunicación con el TMP117.
  Wire.beginTransmission(TMP117_ADDRESS);

  // Se selecciona el registro de temperatura.
  Wire.write(TMP117_TEMP_REG);

  // Se finaliza la escritura sin liberar completamente el bus.
  // Esto permite realizar una lectura inmediatamente después.
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  // Se solicitan 2 bytes al TMP117.
  // El registro de temperatura está formado por MSB y LSB.
  Wire.requestFrom(TMP117_ADDRESS, 2);

  // Se verifica que se hayan recibido los 2 bytes esperados.
  if (Wire.available() < 2) {
    return false;
  }

  // Se lee el byte más significativo.
  uint8_t msb = Wire.read();

  // Se lee el byte menos significativo.
  uint8_t lsb = Wire.read();

  // Se combinan ambos bytes en un entero de 16 bits con signo.
  int16_t rawTemp = (int16_t)((msb << 8) | lsb);

  // Se convierte el valor crudo a grados Celsius.
  temperaturaC = rawTemp * TMP117_RESOLUTION;

  // Se indica que la lectura fue correcta.
  return true;
}
