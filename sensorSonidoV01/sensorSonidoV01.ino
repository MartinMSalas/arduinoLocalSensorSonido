const int LED_PIN = LED_BUILTIN;

// Duración de cada estado
const unsigned long DURACION_ESTADO = 4000;

// Intervalos de parpadeo
const unsigned long INTERVALO_LENTO = 500;
const unsigned long INTERVALO_RAPIDO = 150;

enum Estado {
  APAGADO,
  PARPADEO_LENTO,
  PARPADEO_RAPIDO,
  PRENDIDO
};

Estado estadoActual = PARPADEO_LENTO;  // iniciar en parpadeo lento

unsigned long tiempoCambioEstado = 0;
unsigned long tiempoUltimoParpadeo = 0;

bool ledEstado = LOW;

void setup() {
  pinMode(LED_PIN, OUTPUT);

  // Estado inicial
  digitalWrite(LED_PIN, LOW);
  tiempoCambioEstado = millis();
  tiempoUltimoParpadeo = millis();
}

void loop() {
  unsigned long ahora = millis();

  // 1. Ejecutar comportamiento del estado actual
  switch (estadoActual) {
    case APAGADO:
      digitalWrite(LED_PIN, LOW);
      break;

    case PRENDIDO:
      digitalWrite(LED_PIN, HIGH);
      break;

    case PARPADEO_LENTO:
      if (ahora - tiempoUltimoParpadeo >= INTERVALO_LENTO) {
        tiempoUltimoParpadeo = ahora;
        ledEstado = !ledEstado;
        digitalWrite(LED_PIN, ledEstado);
      }
      break;

    case PARPADEO_RAPIDO:
      if (ahora - tiempoUltimoParpadeo >= INTERVALO_RAPIDO) {
        tiempoUltimoParpadeo = ahora;
        ledEstado = !ledEstado;
        digitalWrite(LED_PIN, ledEstado);
      }
      break;
  }

  // 2. Verificar si ya pasaron 2 segundos para cambiar de estado
  if (ahora - tiempoCambioEstado >= DURACION_ESTADO) {
    cambiarEstado();
    tiempoCambioEstado = ahora;
    tiempoUltimoParpadeo = ahora;
  }
}

void cambiarEstado() {
  switch (estadoActual) {
    case APAGADO:
      estadoActual = PARPADEO_LENTO;
      ledEstado = LOW;
      break;

    case PARPADEO_LENTO:
      estadoActual = PARPADEO_RAPIDO;
      ledEstado = LOW;
      break;

    case PARPADEO_RAPIDO:
      estadoActual = PRENDIDO;
      ledEstado = HIGH;
      break;

    case PRENDIDO:
      estadoActual = APAGADO;
      ledEstado = LOW;
      break;
  }
}