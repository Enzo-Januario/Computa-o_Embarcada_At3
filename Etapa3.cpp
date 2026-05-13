/*
 * Laboratório 3 - Monitoramento de Temperatura
 * Etapa 3: Histórico, Estatísticas e Exportação CSV
 * Enzo Januário
 */

// ===== Pinos =====
const uint8_t PIN_LM35   = A1;
const uint8_t PIN_LED_B  = 11;  // Azul     - Frio    (< L)
const uint8_t PIN_LED_G  = 12;  // Verde    - Normal  (L a H)
const uint8_t PIN_LED_R  = 13;  // Vermelho - Crítico (>= H)

// ===== Limites =====
float L = 20.0;
float H = 30.0;

// ===== Leitura =====
const float VREF        = 5.0;
const int   ADC_RES     = 1023;
const unsigned long INTERVALO = 1000;
unsigned long ultimaLeitura = 0;
float ultimaTemp = 0.0;

// ===== Buffer circular =====
const uint8_t TAM_HIST = 20;
float historico[TAM_HIST];
uint8_t indiceEscrita = 0;
uint8_t qtdLeituras = 0;
unsigned long timestamps[TAM_HIST];

// ===== Serial =====
String comando = "";
unsigned long ultimoCharRecebido = 0;
const unsigned long TIMEOUT_COMANDO = 50; // ms

void setup() {
  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);

  Serial.begin(9600);
  comando.reserve(32);

  Serial.println("Sistema iniciado.");
  Serial.println("Comandos: L=XX | H=YY | T | S | E | C");
}

void loop() {
  if (millis() - ultimaLeitura >= INTERVALO) {
    ultimaLeitura = millis();
    ultimaTemp = lerTemperatura();
    armazenarLeitura(ultimaTemp);
    atualizarLEDs(ultimaTemp);
    plotarSerial(ultimaTemp);
  }
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

  if (temp < L)        digitalWrite(PIN_LED_B, HIGH);
  else if (temp >= H)  digitalWrite(PIN_LED_R, HIGH);
  else                 digitalWrite(PIN_LED_G, HIGH);
}

void plotarSerial(float temp) {
  Serial.print(temp);
  Serial.print(",");
  Serial.print(L);
  Serial.print(",");
  Serial.println(H);
}

// =========================================================
//                    BUFFER CIRCULAR
// =========================================================
void armazenarLeitura(float temp) {
  historico[indiceEscrita] = temp;
  timestamps[indiceEscrita] = millis();

  indiceEscrita = (indiceEscrita + 1) % TAM_HIST;
  if (qtdLeituras < TAM_HIST) qtdLeituras++;
}

float calcularMedia() {
  if (qtdLeituras == 0) return 0.0;
  float soma = 0.0;
  for (uint8_t i = 0; i < qtdLeituras; i++) {
    soma += historico[i];
  }
  return soma / qtdLeituras;
}

float calcularMaximo() {
  if (qtdLeituras == 0) return 0.0;
  float maximo = historico[0];
  for (uint8_t i = 1; i < qtdLeituras; i++) {
    if (historico[i] > maximo) maximo = historico[i];
  }
  return maximo;
}

float calcularMinimo() {
  if (qtdLeituras == 0) return 0.0;
  float minimo = historico[0];
  for (uint8_t i = 1; i < qtdLeituras; i++) {
    if (historico[i] < minimo) minimo = historico[i];
  }
  return minimo;
}

void exportarCSV() {
  Serial.println(F("---- INICIO CSV ----"));
  Serial.println(F("indice,tempo_ms,temperatura,L,H"));

  uint8_t inicio = (qtdLeituras < TAM_HIST) ? 0 : indiceEscrita;

  for (uint8_t i = 0; i < qtdLeituras; i++) {
    uint8_t idx = (inicio + i) % TAM_HIST;
    Serial.print(i + 1);
    Serial.print(",");
    Serial.print(timestamps[idx]);
    Serial.print(",");
    Serial.print(historico[idx]);
    Serial.print(",");
    Serial.print(L);
    Serial.print(",");
    Serial.println(H);
  }
  Serial.println(F("---- FIM CSV ----"));
}

// =========================================================
//              COMUNICAÇÃO BIDIRECIONAL
// =========================================================
void processarSerial() {
  // Lê todos os caracteres disponíveis
  while (Serial.available() > 0) {
    char c = Serial.read();

    // Aceita \n ou \r como terminador (Arduino IDE real)
    if (c == '\n' || c == '\r') {
      if (comando.length() > 0) {
        interpretarComando(comando);
        comando = "";
      }
    } else {
      comando += c;
      ultimoCharRecebido = millis();
    }
  }

  // Workaround pro Tinkercad: se passou um tempo sem receber nada
  // e tem comando no buffer, assume que o usuário terminou de digitar
  if (comando.length() > 0 && (millis() - ultimoCharRecebido) > TIMEOUT_COMANDO) {
    interpretarComando(comando);
    comando = "";
  }
}

void interpretarComando(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

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
  else if (cmd == "E") {
    Serial.print("Estatisticas (");
    Serial.print(qtdLeituras);
    Serial.print(" leituras) -> Media: ");
    Serial.print(calcularMedia());
    Serial.print(" | Max: ");
    Serial.print(calcularMaximo());
    Serial.print(" | Min: ");
    Serial.println(calcularMinimo());
  }
  else if (cmd == "C") {
    exportarCSV();
  }
  else {
    Serial.print("ERRO: comando invalido -> ");
    Serial.println(cmd);
  }
}

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
