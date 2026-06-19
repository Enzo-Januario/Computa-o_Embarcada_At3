#define USAR_TMP36

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#ifndef USAR_TMP36
  #include <DHT.h>
  DHT dht(2, DHT11);
#endif

#define COOLER 9
#define BUZZER 8
#define BENTER 4
#define BUP    5
#define BDOWN  6
#define LR 11
#define LG 12
#define LB 13

LiquidCrystal_I2C lcd(0x27, 16, 2);   
const byte ASSIN = 0xA7;              

float amostras[5]; int iAmostra = 0, qAmostra = 0;
float temperatura = 0, tMaxReg = -100, tMinReg = 150, limMax = 35, limMin = 20;
float setpoint = 28, Kp = 20, Ki = 2, integral = 0; int pwm = 0;
float logBuf[10]; int iLog = 0, qLog = 0;

enum Estado { EXIBICAO, SET_MAX, SET_MIN } estado = EXIBICAO;
bool telaExtremos = false, blinkOn = true;
volatile bool fEnter = false, fUp = false, fDown = false;
volatile byte portdAnt = 0;
enum Nivel { OK, FRIO, CRITICO } nivel = OK, nivelAnt = OK;

unsigned long tLei = 0, tPlot = 0, tBlink = 0, tBuz = 0, tEnter = 0, tDeb = 0;
bool buzOn = false, enterDown = false, limpar = true, atualizar = true;
String linha = "";

// ISR dos botoes (Etapa 2): so seta flags volatile
ISR(PCINT2_vect) {
  byte a = PIND, m = a ^ portdAnt;
  if ((m & (1 << BENTER)) && (a & (1 << BENTER))) fEnter = true;
  if ((m & (1 << BUP))    && (a & (1 << BUP)))    fUp = true;
  if ((m & (1 << BDOWN))  && (a & (1 << BDOWN)))  fDown = true;
  portdAnt = a;
}

void setup() {
  Serial.begin(9600);
  pinMode(COOLER, OUTPUT); pinMode(BUZZER, OUTPUT);
  pinMode(LR, OUTPUT); pinMode(LG, OUTPUT); pinMode(LB, OUTPUT);
  pinMode(BENTER, INPUT); pinMode(BUP, INPUT); pinMode(BDOWN, INPUT);
#ifndef USAR_TMP36
  dht.begin();
#endif
  lcd.init(); lcd.backlight();
  carregarEEPROM();
  portdAnt = PIND;
  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT20) | (1 << PCINT21) | (1 << PCINT22);
  lcd.print("Sist. Ambiental"); lcd.setCursor(0, 1); lcd.print("Etapa Final");
  delay(1500); lcd.clear();
  Serial.println(F("Comandos: SETPOINT x | KP x | KI x | STATUS | LOG"));
}

void loop() {
  unsigned long t = millis();
  if (t - tLei >= 1000) {
    tLei = t;
    temperatura = media(lerSensor());
    extremos(); regLog(temperatura); controlePI(); alerta();
    if (estado == EXIBICAO) atualizar = true;
  }
  botoes();
  if (estado != EXIBICAO && t - tBlink >= 350) { tBlink = t; blinkOn = !blinkOn; atualizar = true; }
  if (buzOn && t - tBuz >= 150) { noTone(BUZZER); buzOn = false; }
  if (t - tPlot >= 500) { tPlot = t; plotter(); }
  serial();
  if (atualizar) { atualizar = false; mostrarLCD(); }
}

float lerSensor() {
#ifdef USAR_TMP36
  return (analogRead(A0) * 5.0 / 1024.0 - 0.5) * 100.0;
#else
  float v = dht.readTemperature(); return isnan(v) ? temperatura : v;
#endif
}

float media(float n) {
  amostras[iAmostra] = n; iAmostra = (iAmostra + 1) % 5; if (qAmostra < 5) qAmostra++;
  float s = 0; for (int i = 0; i < qAmostra; i++) s += amostras[i]; return s / qAmostra;
}

void extremos() {
  bool rec = false;
  if (temperatura > tMaxReg) { tMaxReg = temperatura; rec = true; }
  if (temperatura < tMinReg) { tMinReg = temperatura; rec = true; }
  if (rec) { EEPROM.put(1, tMaxReg); EEPROM.put(5, tMinReg); }
}

void salvarLimites() { EEPROM.put(9, limMax); EEPROM.put(13, limMin); }

void carregarEEPROM() {
  byte a; EEPROM.get(0, a);
  if (a != ASSIN) {
    EEPROM.put(0, ASSIN);
    EEPROM.put(1, tMaxReg); EEPROM.put(5, tMinReg); salvarLimites();
  } else {
    EEPROM.get(1, tMaxReg); EEPROM.get(5, tMinReg);
    EEPROM.get(9, limMax);  EEPROM.get(13, limMin);
  }
}

void controlePI() {
  float erro = temperatura - setpoint;
  float s = Kp * erro + integral;
  if (s > 0 && s < 255) integral = constrain(integral + Ki * erro, 0, 255); 
  pwm = constrain((int)(Kp * erro + integral), 0, 255);
  analogWrite(COOLER, pwm);
}

void regLog(float t) { logBuf[iLog] = t; iLog = (iLog + 1) % 10; if (qLog < 10) qLog++; }

void alerta() {
  nivel = temperatura >= limMax ? CRITICO : (temperatura <= limMin ? FRIO : OK);
  setRGB(nivel == CRITICO, nivel == OK, nivel == FRIO);
  if (nivel == CRITICO && nivelAnt != CRITICO) { tone(BUZZER, 2000); buzOn = true; tBuz = millis(); }
  nivelAnt = nivel;
}
void setRGB(bool r, bool g, bool b) { digitalWrite(LR, r); digitalWrite(LG, g); digitalWrite(LB, b); }

void botoes() {
  unsigned long t = millis();
  if (fEnter) { fEnter = false; if (!enterDown && t - tDeb > 200) { tDeb = t; tEnter = t; enterDown = true; } }
  if (enterDown) {
    if (digitalRead(BENTER) == LOW) { enterDown = false; enterCurto(); }
    else if (t - tEnter >= 900)     { enterDown = false; enterLongo(); }
  }
  if (fUp)   { fUp = false;   if (t - tDeb > 200) { tDeb = t; ajusta(+0.5); } }
  if (fDown) { fDown = false; if (t - tDeb > 200) { tDeb = t; ajusta(-0.5); } }
}

void enterCurto() {
  if (estado == EXIBICAO) telaExtremos = !telaExtremos; 
  else { salvarLimites(); estado = (estado == SET_MAX) ? SET_MIN : EXIBICAO; }
  limpar = true; atualizar = true;
}
void enterLongo() { if (estado == EXIBICAO) { estado = SET_MAX; limpar = true; atualizar = true; } }

void ajusta(float d) {
  if (estado == SET_MAX)      limMax = constrain(limMax + d, limMin + 0.5, 80);
  else if (estado == SET_MIN) limMin = constrain(limMin + d, -10, limMax - 0.5);
  atualizar = true;
}

void pTemp(float v) { lcd.print(v, 1); lcd.print((char)223); lcd.print('C'); }

void mostrarLCD() {
  if (limpar) { lcd.clear(); limpar = false; }
  if (estado == EXIBICAO) {
    if (!telaExtremos) {
      lcd.setCursor(0, 0); lcd.print("T:"); lcd.print(temperatura, 1);
      lcd.print("C SP:"); lcd.print(setpoint, 1); lcd.print(' ');
      lcd.setCursor(0, 1); lcd.print("Cooler: ");
      lcd.print(map(pwm, 0, 255, 0, 100)); lcd.print("%  ");
    } else {
      lcd.setCursor(0, 0); lcd.print("Max:"); pTemp(tMaxReg); lcd.print("  ");
      lcd.setCursor(0, 1); lcd.print("Min:"); pTemp(tMinReg); lcd.print("  ");
    }
  } else {
    lcd.setCursor(0, 0); lcd.print(estado == SET_MAX ? "Limite MAXIMO " : "Limite MINIMO ");
    lcd.setCursor(0, 1);
    if (blinkOn) { pTemp(estado == SET_MAX ? limMax : limMin); lcd.print("     "); }
    else lcd.print("                ");
  }
}

void plotter() {
  Serial.print("Temp:"); Serial.print(temperatura, 1);
  Serial.print(",Setpoint:"); Serial.print(setpoint, 1);
  Serial.print(",PWM:"); Serial.println(pwm);
}

void serial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') { if (linha.length()) { comando(linha); linha = ""; } }
    else linha += c;
  }
}

void comando(String c) {
  c.trim(); c.toUpperCase();
  if (c.startsWith("SETPOINT")) { setpoint = c.substring(8).toFloat(); integral = 0; Serial.print(F("SP=")); Serial.println(setpoint, 1); }
  else if (c.startsWith("KP"))  { Kp = c.substring(2).toFloat(); Serial.print(F("Kp=")); Serial.println(Kp, 1); }
  else if (c.startsWith("KI"))  { Ki = c.substring(2).toFloat(); Serial.print(F("Ki=")); Serial.println(Ki, 1); }
  else if (c == "STATUS") status();
  else if (c == "LOG")    logSerial();
  else Serial.println(F("Comando invalido"));
}

void status() {
  Serial.print(F("T="));   Serial.print(temperatura, 1);
  Serial.print(F(" SP=")); Serial.print(setpoint, 1);
  Serial.print(F(" Kp=")); Serial.print(Kp, 1);
  Serial.print(F(" Ki=")); Serial.print(Ki, 1);
  Serial.print(F(" PWM="));Serial.print(pwm);
  Serial.print(F(" Lim="));Serial.print(limMin, 1); Serial.print('/'); Serial.print(limMax, 1);
  Serial.print(F(" Rec="));Serial.print(tMinReg, 1); Serial.print('/'); Serial.println(tMaxReg, 1);
}

void logSerial() {
  Serial.print(F("Log: "));
  for (int i = 0; i < qLog; i++) { Serial.print(logBuf[(iLog - qLog + i + 10) % 10], 1); Serial.print(' '); }
  Serial.println();
}
