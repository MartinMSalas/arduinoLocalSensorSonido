#include <SPI.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <math.h>

// =====================================
// DISPLAY MAX7219 FC-16
// =====================================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 1
#define CS_PIN 53

MD_Parola display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// =====================================
// LED ONBOARD - MAQUINA DE ESTADOS
// =====================================
const byte LED_PIN = LED_BUILTIN;

const unsigned long DURACION_ESTADO_LED = 2000;
const unsigned long INTERVALO_LENTO = 500;
const unsigned long INTERVALO_RAPIDO = 150;

enum EstadoLED {
  APAGADO,
  PARPADEO_LENTO,
  PARPADEO_RAPIDO,
  PRENDIDO
};

EstadoLED estadoLED = PARPADEO_LENTO;
unsigned long tiempoCambioEstadoLED = 0;
unsigned long tiempoUltimoParpadeo = 0;
bool ledEstadoFisico = LOW;

// =====================================
// MICROFONO / ANALISIS
// =====================================
const byte MIC_PIN = A0;
const unsigned long REPORT_MS = 10000;   // RMS promedio cada 10 segundos
const int BLOCK_SIZE = 200;

// =====================================
// CALIBRACION INICIAL
// =====================================
const unsigned long CALIBRATION_MS = 8000;
bool calibrando = true;
unsigned long calibrationStart = 0;

// valores base aprendidos al inicio
double centroBase = 0.0;
double rmsBase = 0.0;
double picoBase = 0.0;

// acumuladores para calibracion
unsigned long calibSamples = 0;
double calibSum = 0.0;
double calibSumSquaresCentered = 0.0;
int calibMin = 1023;
int calibMax = 0;

// =====================================
// ACUMULADORES DE MEDICION NORMAL
// =====================================
unsigned long lastReport = 0;

unsigned long totalSamples = 0;
double totalSum = 0.0;
double totalSumSquaresCentered = 0.0;

int globalMin = 1023;
int globalMax = 0;

int buffer[BLOCK_SIZE];
int bufferIndex = 0;

// Texto del display
char textoDisplay[16] = "CAL";

// =====================================
// SETUP
// =====================================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);

  // Display
  display.begin();
  display.setIntensity(2);   // 0..15
  display.displayClear();
  display.displayText(textoDisplay, PA_CENTER, 60, 500, PA_SCROLL_LEFT, PA_SCROLL_LEFT);

  unsigned long ahora = millis();
  tiempoCambioEstadoLED = ahora;
  tiempoUltimoParpadeo = ahora;
  lastReport = ahora;
  calibrationStart = ahora;

  Serial.println("Iniciando calibracion de base... mantener en silencio relativo.");
}

// =====================================
// LOOP
// =====================================
void loop() {
  unsigned long ahora = millis();

  actualizarMaquinaEstadosLED(ahora);
  capturarAudio();

  if (calibrando) {
    procesarCalibracionSiCorresponde(ahora);
  } else {
    reportarAudioSiCorresponde(ahora);
  }

  // animacion del display sin bloquear
  if (display.displayAnimate()) {
    display.displayReset();
  }
}

// =====================================
// LED FSM
// =====================================
void actualizarMaquinaEstadosLED(unsigned long ahora) {
  switch (estadoLED) {
    case APAGADO:
      digitalWrite(LED_PIN, LOW);
      break;

    case PRENDIDO:
      digitalWrite(LED_PIN, HIGH);
      break;

    case PARPADEO_LENTO:
      if (ahora - tiempoUltimoParpadeo >= INTERVALO_LENTO) {
        tiempoUltimoParpadeo = ahora;
        ledEstadoFisico = !ledEstadoFisico;
        digitalWrite(LED_PIN, ledEstadoFisico);
      }
      break;

    case PARPADEO_RAPIDO:
      if (ahora - tiempoUltimoParpadeo >= INTERVALO_RAPIDO) {
        tiempoUltimoParpadeo = ahora;
        ledEstadoFisico = !ledEstadoFisico;
        digitalWrite(LED_PIN, ledEstadoFisico);
      }
      break;
  }

  if (ahora - tiempoCambioEstadoLED >= DURACION_ESTADO_LED) {
    cambiarEstadoLED();
    tiempoCambioEstadoLED = ahora;
    tiempoUltimoParpadeo = ahora;
  }
}

void cambiarEstadoLED() {
  switch (estadoLED) {
    case APAGADO:
      estadoLED = PARPADEO_LENTO;
      ledEstadoFisico = LOW;
      break;
    case PARPADEO_LENTO:
      estadoLED = PARPADEO_RAPIDO;
      ledEstadoFisico = LOW;
      break;
    case PARPADEO_RAPIDO:
      estadoLED = PRENDIDO;
      ledEstadoFisico = HIGH;
      break;
    case PRENDIDO:
      estadoLED = APAGADO;
      ledEstadoFisico = LOW;
      break;
  }
}

// =====================================
// CAPTURA DE AUDIO
// =====================================
void capturarAudio() {
  int raw = analogRead(MIC_PIN);

  buffer[bufferIndex++] = raw;

  if (bufferIndex >= BLOCK_SIZE) {
    if (calibrando) {
      procesarBloqueCalibracion();
    } else {
      procesarBloqueNormal();
    }
  }
}

// =====================================
// BLOQUE DE CALIBRACION
// =====================================
void procesarBloqueCalibracion() {
  if (bufferIndex == 0) return;

  double blockSum = 0.0;
  int blockMin = 1023;
  int blockMax = 0;

  for (int i = 0; i < bufferIndex; i++) {
    blockSum += buffer[i];
    if (buffer[i] < blockMin) blockMin = buffer[i];
    if (buffer[i] > blockMax) blockMax = buffer[i];
  }

  double blockMean = blockSum / bufferIndex;

  for (int i = 0; i < bufferIndex; i++) {
    double centered = buffer[i] - blockMean;
    calibSumSquaresCentered += centered * centered;
  }

  calibSamples += bufferIndex;
  calibSum += blockSum;

  if (blockMin < calibMin) calibMin = blockMin;
  if (blockMax > calibMax) calibMax = blockMax;

  bufferIndex = 0;
}

// =====================================
// BLOQUE NORMAL
// =====================================
void procesarBloqueNormal() {
  if (bufferIndex == 0) return;

  double blockSum = 0.0;
  int blockMin = 1023;
  int blockMax = 0;

  for (int i = 0; i < bufferIndex; i++) {
    blockSum += buffer[i];
    if (buffer[i] < blockMin) blockMin = buffer[i];
    if (buffer[i] > blockMax) blockMax = buffer[i];
  }

  double blockMean = blockSum / bufferIndex;

  for (int i = 0; i < bufferIndex; i++) {
    double centered = buffer[i] - blockMean;
    totalSumSquaresCentered += centered * centered;
  }

  totalSamples += bufferIndex;
  totalSum += blockSum;

  if (blockMin < globalMin) globalMin = blockMin;
  if (blockMax > globalMax) globalMax = blockMax;

  bufferIndex = 0;
}

// =====================================
// FIN DE CALIBRACION
// =====================================
void procesarCalibracionSiCorresponde(unsigned long ahora) {
  if (ahora - calibrationStart >= CALIBRATION_MS) {
    procesarBloqueCalibracion();

    if (calibSamples > 0) {
      centroBase = calibSum / calibSamples;
      rmsBase = sqrt(calibSumSquaresCentered / calibSamples);
      picoBase = (calibMax - calibMin) / 2.0;
    } else {
      centroBase = 512.0;
      rmsBase = 0.0;
      picoBase = 0.0;
    }

    calibrando = false;
    resetMedicionNormal();
    lastReport = ahora;

    Serial.println("Calibracion finalizada.");
    Serial.print("CentroBase=");
    Serial.print(centroBase, 2);
    Serial.print("  RMSBase=");
    Serial.print(rmsBase, 2);
    Serial.print("  PicoBase=");
    Serial.println(picoBase, 2);

    snprintf(textoDisplay, sizeof(textoDisplay), "OK");
    display.displayClear();
    display.displayText(textoDisplay, PA_CENTER, 60, 500, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  }
}

// =====================================
// REPORTE NORMAL CADA 10s
// =====================================
void reportarAudioSiCorresponde(unsigned long ahora) {
  if (ahora - lastReport >= REPORT_MS) {
    procesarBloqueNormal();

    if (totalSamples > 0) {
      double center = totalSum / totalSamples;
      double rms = sqrt(totalSumSquaresCentered / totalSamples);
      double peakToPeak = globalMax - globalMin;
      double peak = peakToPeak / 2.0;

      double rmsDelta = rms - rmsBase;
      double peakDelta = peak - picoBase;
      double centerDelta = center - centroBase;

      if (rmsDelta < 0) rmsDelta = 0;
      if (peakDelta < 0) peakDelta = 0;

      Serial.print("LED=");
      Serial.print(nombreEstadoLED());

      Serial.print("  Centro=");
      Serial.print(center, 2);

      Serial.print("  CentroBase=");
      Serial.print(centroBase, 2);

      Serial.print("  CentroDelta=");
      Serial.print(centerDelta, 2);

      Serial.print("  P2P=");
      Serial.print(peakToPeak, 2);

      Serial.print("  Pico=");
      Serial.print(peak, 2);

      Serial.print("  PicoBase=");
      Serial.print(picoBase, 2);

      Serial.print("  PicoDelta=");
      Serial.print(peakDelta, 2);

      Serial.print("  RMS_PROM=");
      Serial.print(rms, 2);

      Serial.print("  RMSBase=");
      Serial.print(rmsBase, 2);

      Serial.print("  RMSDelta=");
      Serial.println(rmsDelta, 2);

      // Mostrar RMS entero en display
      int rmsEntero = (int)round(rms);
      snprintf(textoDisplay, sizeof(textoDisplay), "R%d", rmsEntero);

      display.displayClear();
      display.displayText(textoDisplay, PA_CENTER, 60, 500, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
    }

    resetMedicionNormal();
    lastReport = ahora;
  }
}

// =====================================
// RESET MEDICION NORMAL
// =====================================
void resetMedicionNormal() {
  totalSamples = 0;
  totalSum = 0.0;
  totalSumSquaresCentered = 0.0;
  globalMin = 1023;
  globalMax = 0;
  bufferIndex = 0;
}

// =====================================
// UTILIDAD
// =====================================
const char* nombreEstadoLED() {
  switch (estadoLED) {
    case APAGADO: return "APAGADO";
    case PARPADEO_LENTO: return "PARPADEO_LENTO";
    case PARPADEO_RAPIDO: return "PARPADEO_RAPIDO";
    case PRENDIDO: return "PRENDIDO";
    default: return "DESCONOCIDO";
  }
}