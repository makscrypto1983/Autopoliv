/*
 * Система управления водой для Arduino Nano
 * Версия с поплавковым цифровым датчиком (замкнуто/разомкнуто)
 * 
 * Компоненты:
 * - Кнопка включения насоса (пин 2)
 * - Реле насоса (пин 7)
 * - Поплавковый датчик уровня воды (пин 3, digital)
 * - Насос воды (управляется через реле)
 */

const int BUTTON_PIN = 2;         // Кнопка включения насоса
const int RELAY_PIN = 7;          // Реле управления насосом  
const int WATER_SENSOR_PIN = 3;   // Поплавковый датчик (цифровой пин)
const int LED_PIN = 13;           // Встроенный светодиод

const unsigned long WEEK_INTERVAL = 7UL * 24UL * 60UL * 60UL * 1000UL; // Неделя
const unsigned long PUMP_TIMEOUT = 30000; // Максимум 30 секунд

// Состояния
bool pumpState = false;
bool buttonPressed = false;
bool lastButtonState = false;
unsigned long lastWeeklyCheck = 0;
unsigned long pumpStartTime = 0;
int waterLevel = 0; // 0 = нет воды, 1023 = вода есть (для совместимости)

void setup() {
  Serial.begin(9600);
  Serial.println("=== Система управления водой запущена ===");

  pinMode(BUTTON_PIN, INPUT_PULLUP);       // Кнопка
  pinMode(RELAY_PIN, OUTPUT);              // Реле насоса
  pinMode(WATER_SENSOR_PIN, INPUT_PULLUP); // Поплавковый датчик (размыкает на GND)
  pinMode(LED_PIN, OUTPUT);                // Светодиод

  digitalWrite(RELAY_PIN, LOW); // насос выключен
  digitalWrite(LED_PIN, LOW);   // светодиод выключен

  Serial.println("Система готова к работе!");
}

void loop() {
  readSensors();
  handleButtonPress();
  handleWaterLevel();
  handleWeeklyCheck();
  handlePumpTimeout();
  updateIndicators();
  printStatus();
  serialCommands(); // Обработка команд через Serial
  delay(500);
}

// === ОСНОВНЫЕ ФУНКЦИИ ===

void readSensors() {
  // LOW = контакт замкнут = вода есть
  bool waterDetected = digitalRead(WATER_SENSOR_PIN) == LOW;

  // Обновляем waterLevel для логики (совместимо с прошлой реализацией)
  waterLevel = waterDetected ? 1023 : 0;

  // Кнопка с подтяжкой (нажатие = LOW)
  bool currentButtonState = !digitalRead(BUTTON_PIN);
  if (currentButtonState && !lastButtonState) {
    buttonPressed = true;
  }
  lastButtonState = currentButtonState;
}

void handleButtonPress() {
  if (buttonPressed) {
    pumpState = !pumpState;
    if (pumpState) {
      startPump("Ручное включение");
    } else {
      stopPump("Ручное выключение");
    }
    buttonPressed = false;
  }
}

void handleWaterLevel() {
  if (waterLevel >= 1023 && pumpState) {
    stopPump("Бак полный");
  }

  if (waterLevel <= 0 && !pumpState) {
    Serial.println("⚠️  ВНИМАНИЕ: Низкий уровень воды!");
  }
}

void handleWeeklyCheck() {
  unsigned long currentTime = millis();
  if (currentTime - lastWeeklyCheck >= WEEK_INTERVAL) {
    Serial.println("🔄 Еженедельная автоматическая проверка...");
    if (waterLevel < 1023) {
      startPump("Еженедельная доливка");
    } else {
      Serial.println("✅ Уровень воды в норме");
    }
    lastWeeklyCheck = currentTime;
  }
}

void handlePumpTimeout() {
  if (pumpState && (millis() - pumpStartTime > PUMP_TIMEOUT)) {
    stopPump("ТАЙМАУТ - принудительное выключение");
    Serial.println("🚨 ВНИМАНИЕ: Насос выключен по таймауту!");
  }
}

void startPump(String reason) {
  if (!pumpState) {
    pumpState = true;
    digitalWrite(RELAY_PIN, HIGH);
    pumpStartTime = millis();
    Serial.println("🟢 Насос ВКЛЮЧЕН: " + reason);
  }
}

void stopPump(String reason) {
  if (pumpState) {
    pumpState = false;
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("🔴 Насос ВЫКЛЮЧЕН: " + reason);
  }
}

void updateIndicators() {
  if (pumpState) {
    digitalWrite(LED_PIN, (millis() / 500) % 2); // Мигание
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}

void printStatus() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    Serial.println("--- СОСТОЯНИЕ СИСТЕМЫ ---");
    Serial.println("💧 Уровень воды: " + String(waterLevel == 1023 ? "ПОЛНЫЙ" : "НИЗКИЙ"));
    Serial.println("🔌 Насос: " + String(pumpState ? "ВКЛЮЧЕН" : "ВЫКЛЮЧЕН"));
    Serial.println("⏱️  До еженедельной проверки: " + String((WEEK_INTERVAL - (millis() - lastWeeklyCheck)) / 1000 / 60) + " минут");
    Serial.println("🕐 Время работы: " + String(millis() / 1000) + " секунд");
    Serial.println("------------------------");
    lastPrint = millis();
  }
}

void serialCommands() {
  if (Serial.available()) {
    String command = Serial.readString();
    command.trim();

    if (command == "status") {
      printStatus();
    } else if (command == "pump_on") {
      startPump("Команда Serial");
    } else if (command == "pump_off") {
      stopPump("Команда Serial");
    } else if (command == "help") {
      Serial.println("Доступные команды:");
      Serial.println("status - показать состояние");
      Serial.println("pump_on - включить насос");
      Serial.println("pump_off - выключить насос");
    } else {
      Serial.println("Неизвестная команда. Введите 'help' для списка.");
    }
  }
}
