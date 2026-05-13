/*
 * Laboratório 3 - Monitoramento de Temperatura
 * Etapa 1: Classificação por Faixas e Serial Plotter
 * Enzo Januário
 */

// ===== Pinos =====
const uint8_t PIN_LM35   = A1;
const uint8_t PIN_LED_B  = 11;  // Azul     - Frio    (< 20 °C)
const uint8_t PIN_LED_G  = 12;  // Verde    - Normal  (20 a 30 °C)
const uint8_t PIN_LED_R  = 13;  // Vermelho - Crítico (>= 30 °C)

// ===== Limites de temperatura (°C) =====
float L = 20.0;  // Limite inferior (abaixo disso = Frio)
float H = 30.0;  // Limite superior (a partir disso = Crítico)

// ===== Configurações de leitura =====
const float VREF        = 5.0;     // Tensão de referência do ADC
const int   ADC_RES     = 1023;    // Resolução do ADC (10 bits)
const unsigned long INTERVALO = 1000; // Intervalo entre leituras (ms)

unsigned long ultimaLeitura = 0;

void setup() {
  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);

  Serial.begin(9600);
}

void loop() {
  if (millis() - ultimaLeitura >= INTERVALO) {
    ultimaLeitura = millis();

    float temp = lerTemperatura();
    atualizarLEDs(temp);
    plotarSerial(temp);
  }
}

// Lê o LM35 e converte para °C.
// LM35: 10 mV por °C  =>  T = Vout * 100
float lerTemperatura() {
  int leitura = analogRead(PIN_LM35);
  float tensao = (leitura * VREF) / ADC_RES;  // tensão em Volts
  float tempC  = tensao * 100.0;              // °C
  return tempC;
}

// Acende somente o LED correspondente à faixa atual.
void atualizarLEDs(float temp) {
  // Desliga todos primeiro para garantir exclusividade
  digitalWrite(PIN_LED_B, LOW);
  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_R, LOW);

  if (temp < L) {
    digitalWrite(PIN_LED_B, HIGH);  // Frio
  } else if (temp >= H) {
    digitalWrite(PIN_LED_R, HIGH);  // Crítico
  } else {
    digitalWrite(PIN_LED_G, HIGH);  // Normal
  }
}

// Envia no formato esperado pelo Serial Plotter:
// temperatura, limite inferior, limite superior
void plotarSerial(float temp) {
  Serial.print(temp);
  Serial.print(",");
  Serial.print(L);
  Serial.print(",");
  Serial.println(H);
}
