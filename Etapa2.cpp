/*
 * Laboratório 3 - Monitoramento de Temperatura
 * Etapa 2: Controle Interativo via Serial
 * Enzo Januário
 */

// ===== Pinos =====
const uint8_t PIN_LM35   = A1;
const uint8_t PIN_LED_B  = 11;  // Azul     - Frio    (< L)
const uint8_t PIN_LED_G  = 12;  // Verde    - Normal  (L a H)
const uint8_t PIN_LED_R  = 13;  // Vermelho - Crítico (>= H)

// ===== Limites de temperatura (°C) =====
float L = 20.0;
float H = 30.0;

// ===== Configurações de leitura =====
const float VREF        = 5.0;
const int   ADC_RES     = 1023;
const unsigned long INTERVALO = 1000;

unsigned long ultimaLeitura = 0;

// ===== Buffer para comandos da Serial =====
String comando = "";
float ultimaTemp = 0.0;

void setup() {
  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);

  Serial.begin(9600);
  comando.reserve(32);  // Pré-aloca memória pra evitar fragmentação

  Serial.println("Sistema iniciado. Comandos: L=XX | H=YY | T | S");
}

void loop() {
  // Tarefa 1: leitura periódica + atualização dos LEDs + plot
  if (millis() - ultimaLeitura >= INTERVALO) {
    ultimaLeitura = millis();
    ultimaTemp = lerTemperatura();
    atualizarLEDs(ultimaTemp);
    plotarSerial(ultimaTemp);
  }

  // Tarefa 2: leitura de comandos da Serial (não-bloqueante)
  processarSerial();
}

// =========================================================
//                  LEITURA E CLASSIFICAÇÃO
// =========================================================
float lerTemperatura() {
  int leitura = analogRead(PIN_LM35);
  float tensao = (leitura * VREF) / ADC_RES;
  return tensao * 100.0;
}

void atualizarLEDs(float temp) {
  digitalWrite(PIN_LED_B, LOW);
  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_R, LOW);

  if (temp < L) {
    digitalWrite(PIN_LED_B, HIGH);
  } else if (temp >= H) {
    digitalWrite(PIN_LED_R, HIGH);
  } else {
    digitalWrite(PIN_LED_G, HIGH);
  }
}

void plotarSerial(float temp) {
  Serial.print(temp);
  Serial.print(",");
  Serial.print(L);
  Serial.print(",");
  Serial.println(H);
}

// =========================================================
//             COMUNICAÇÃO BIDIRECIONAL VIA SERIAL
// =========================================================

// Lê caracteres da Serial sem bloquear o loop.
// Quando recebe '\n', interpreta o comando completo.
void processarSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (comando.length() > 0) {
        interpretarComando(comando);
        comando = "";
      }
    } else {
      comando += c;
    }
  }
}

// Interpreta o comando recebido e executa a ação correspondente.
void interpretarComando(String cmd) {
  cmd.trim();          // remove espaços e quebras de linha
  cmd.toUpperCase();   // aceita "l=18" ou "L=18"

  if (cmd.startsWith("L=")) {
    float novoL = cmd.substring(2).toFloat();
    if (validarLimite(novoL, H, true)) {
      L = novoL;
      Serial.print("OK: L=");
      Serial.println(L);
    }
  }
  else if (cmd.startsWith("H=")) {
    float novoH = cmd.substring(2).toFloat();
    if (validarLimite(L, novoH, false)) {
      H = novoH;
      Serial.print("OK: H=");
      Serial.println(H);
    }
  }
  else if (cmd == "T") {
    Serial.print("Temp: ");
    Serial.print(ultimaTemp);
    Serial.println(" C");
  }
  else if (cmd == "S") {
    Serial.print("Limites: [");
    Serial.print(L);
    Serial.print(", ");
    Serial.print(H);
    Serial.print("] | T: ");
    Serial.println(ultimaTemp);
  }
  else {
    Serial.print("ERRO: comando invalido -> ");
    Serial.println(cmd);
  }
}

// Garante que L < H e que os valores estão num range plausível pro LM35.
bool validarLimite(float novoL, float novoH, bool ajustandoL) {
  if (novoL >= novoH) {
    Serial.println("ERRO: L deve ser menor que H");
    return false;
  }
  float valor = ajustandoL ? novoL : novoH;
  if (valor < 0.0 || valor > 150.0) {
    Serial.println("ERRO: valor fora da faixa do LM35 (0 a 150 C)");
    return false;
  }
  return true;
}
