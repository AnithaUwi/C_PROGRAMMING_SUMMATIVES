enum SignalState {
  STATE_RED = 0,
  STATE_GREEN = 1,
  STATE_YELLOW = 2
};

struct LightPins {
  byte red;
  byte yellow;
  byte green;
};

struct SensorPins {
  byte vehicleButton;
};

struct IntersectionConfig {
  const char* name;
  LightPins lights;
  SensorPins sensor;
};

struct IntersectionRuntime {
  SignalState state;
  unsigned long stateStartedAt;
  unsigned long redDurationMs;
  unsigned long yellowDurationMs;
  unsigned long greenDurationMs;
  unsigned long vehiclesDetected;
  unsigned long cycleCount;
  bool vehicleWaiting;
  bool lastButtonLevel;
};

struct TrafficSystem {
  IntersectionConfig* configs;
  IntersectionRuntime* runtime;
  int count;
  bool manualOverride;
  int manualGreenIndex;
  unsigned long lastLogAt;
  unsigned long logIntervalMs;
  int activeIntersection;
  SignalState phase;
  unsigned long phaseStartedAt;
};

TrafficSystem* systemState = NULL;

const unsigned long BASE_GREEN_MS = 5000;
const unsigned long HEAVY_GREEN_MS = 8000;
const unsigned long BASE_RED_MS = 5000;
const unsigned long YELLOW_MS = 2000;
const unsigned long MIN_LOG_INTERVAL_MS = 250;

const char* stateToText(int s) {
  if (s == STATE_RED) return "RED";
  if (s == STATE_GREEN) return "GREEN";
  if (s == STATE_YELLOW) return "YELLOW";
  return "UNKNOWN";
}

void setLightsByIndex(int idx, int state) {
  if (systemState == NULL) return;
  if (idx < 0 || idx >= systemState->count) return;

  LightPins* p = &systemState->configs[idx].lights;
  digitalWrite(p->red, (state == STATE_RED) ? HIGH : LOW);
  digitalWrite(p->yellow, (state == STATE_YELLOW) ? HIGH : LOW);
  digitalWrite(p->green, (state == STATE_GREEN) ? HIGH : LOW);
}

bool setIntersectionStateSafe(int idx, int nextState, unsigned long nowMs) {
  if (systemState == NULL) return false;
  if (idx < 0 || idx >= systemState->count) return false;

  if (!(nextState == STATE_RED || nextState == STATE_GREEN || nextState == STATE_YELLOW)) {
    Serial.println("ERROR: invalid signal state requested.");
    return false;
  }

  systemState->runtime[idx].state = (SignalState)nextState;
  systemState->runtime[idx].stateStartedAt = nowMs;
  setLightsByIndex(idx, nextState);
  return true;
}

void refreshDynamicTiming(int idx) {
  if (systemState == NULL) return;
  if (idx < 0 || idx >= systemState->count) return;

  IntersectionRuntime* r = &systemState->runtime[idx];
  if (r->vehicleWaiting) {
    r->greenDurationMs = HEAVY_GREEN_MS;
  } else {
    r->greenDurationMs = BASE_GREEN_MS;
  }
  r->redDurationMs = BASE_RED_MS;
  r->yellowDurationMs = YELLOW_MS;
}

void readSensors() {
  if (systemState == NULL) return;

  for (int i = 0; i < systemState->count; i++) {
    IntersectionRuntime* r = &systemState->runtime[i];
    IntersectionConfig* c = &systemState->configs[i];

    bool pressed = (digitalRead(c->sensor.vehicleButton) == LOW);
    if (pressed && !r->lastButtonLevel) {
      r->vehiclesDetected++;
      r->vehicleWaiting = true;
    }
    r->lastButtonLevel = pressed;
  }
}

void applyManualOverride(unsigned long nowMs) {
  if (systemState == NULL) return;
  if (!systemState->manualOverride) return;

  for (int i = 0; i < systemState->count; i++) {
    if (i == systemState->manualGreenIndex) {
      setIntersectionStateSafe(i, STATE_GREEN, nowMs);
    } else {
      setIntersectionStateSafe(i, STATE_RED, nowMs);
    }
  }
}

void applyAutoStateFromPhase(unsigned long nowMs) {
  if (systemState == NULL) return;

  int active = systemState->activeIntersection;
  if (active < 0 || active >= systemState->count) {
    active = 0;
    systemState->activeIntersection = 0;
  }

  for (int i = 0; i < systemState->count; i++) {
    if (i == active) {
      setIntersectionStateSafe(i, systemState->phase, nowMs);
    } else {
      setIntersectionStateSafe(i, STATE_RED, nowMs);
    }
  }
}

void autoSchedule(unsigned long nowMs) {
  if (systemState == NULL) return;
  if (systemState->manualOverride) return;

  int active = systemState->activeIntersection;
  if (active < 0 || active >= systemState->count) {
    active = 0;
    systemState->activeIntersection = 0;
  }

  refreshDynamicTiming(active);
  IntersectionRuntime* current = &systemState->runtime[active];
  unsigned long elapsed = nowMs - systemState->phaseStartedAt;

  if (systemState->phase == STATE_GREEN) {
    if (elapsed >= current->greenDurationMs) {
      systemState->phase = STATE_YELLOW;
      systemState->phaseStartedAt = nowMs;
      applyAutoStateFromPhase(nowMs);
    }
  } else if (systemState->phase == STATE_YELLOW) {
    if (elapsed >= current->yellowDurationMs) {
      current->cycleCount++;
      current->vehicleWaiting = false;
      systemState->activeIntersection = (active + 1) % systemState->count;
      systemState->phase = STATE_GREEN;
      systemState->phaseStartedAt = nowMs;
      applyAutoStateFromPhase(nowMs);
    }
  }
  bool aGreen = (systemState->runtime[0].state == STATE_GREEN);
  bool bGreen = (systemState->runtime[1].state == STATE_GREEN);
  if (aGreen && bGreen) {
    setIntersectionStateSafe(1, STATE_RED, nowMs);
    Serial.println("ERROR GUARD: both GREEN detected, forcing B to RED.");
  }
}

void printStatus() {
  if (systemState == NULL) return;

  Serial.println("-Traffic System Status -");
  Serial.print("Mode: ");
  Serial.println(systemState->manualOverride ? "MANUAL" : "AUTO");

  for (int i = 0; i < systemState->count; i++) {
    IntersectionConfig* c = &systemState->configs[i];
    IntersectionRuntime* r = &systemState->runtime[i];

    Serial.print(c->name);
    Serial.print(" | State: ");
    Serial.print(stateToText(r->state));
    Serial.print(" | Vehicles: ");
    Serial.print(r->vehiclesDetected);
    Serial.print(" | Cycles: ");
    Serial.print(r->cycleCount);
    Serial.print(" | Waiting: ");
    Serial.println(r->vehicleWaiting ? "YES" : "NO");
  }
  Serial.println("-----------------------------------");
}

void printMenu() {
  Serial.println("");
  Serial.println("=== Smart Traffic Menu ===");
  Serial.println("1 -> Show status");
  Serial.println("2 -> Enable AUTO mode");
  Serial.println("3 -> Enable MANUAL mode (A green)");
  Serial.println("4 -> Enable MANUAL mode (B green)");
  Serial.println("5 -> Clear counters");
  Serial.println("6 -> Show menu");
  Serial.println("7 -> Toggle log interval");
  Serial.println("8 -> Show timing config");
}

void printTimingConfig() {
  Serial.print("BASE_GREEN_MS=");
  Serial.print(BASE_GREEN_MS);
  Serial.print(", HEAVY_GREEN_MS=");
  Serial.print(HEAVY_GREEN_MS);
  Serial.print(", BASE_RED_MS=");
  Serial.print(BASE_RED_MS);
  Serial.print(", YELLOW_MS=");
  Serial.println(YELLOW_MS);
  Serial.print("Current log interval: ");
  Serial.println(systemState->logIntervalMs);
}

void cycleLogInterval() {
  if (systemState == NULL) return;

  if (systemState->logIntervalMs <= 1000) {
    systemState->logIntervalMs = 2000;
  } else if (systemState->logIntervalMs <= 2000) {
    systemState->logIntervalMs = 5000;
  } else {
    systemState->logIntervalMs = 1000;
  }

  if (systemState->logIntervalMs < MIN_LOG_INTERVAL_MS) {
    systemState->logIntervalMs = MIN_LOG_INTERVAL_MS;
  }

  Serial.print("Updated log interval to ");
  Serial.println(systemState->logIntervalMs);
}

void clearCounters() {
  if (systemState == NULL) return;

  for (int i = 0; i < systemState->count; i++) {
    systemState->runtime[i].vehiclesDetected = 0;
    systemState->runtime[i].cycleCount = 0;
  }
  Serial.println("Counters reset.");
}

void handleSerialMenu(unsigned long nowMs) {
  if (!Serial.available()) return;

  char cmd = (char)Serial.read();

  if (cmd == '1') {
    printStatus();
  } else if (cmd == '2') {
    systemState->manualOverride = false;
    systemState->activeIntersection = (systemState->manualGreenIndex >= 0) ? systemState->manualGreenIndex : 0;
    systemState->phase = STATE_GREEN;
    systemState->phaseStartedAt = nowMs;
    applyAutoStateFromPhase(nowMs);
    Serial.println("Switched to AUTO mode.");
  } else if (cmd == '3') {
    systemState->manualOverride = true;
    systemState->manualGreenIndex = 0;
    applyManualOverride(nowMs);
    Serial.println("Manual override: A GREEN.");
  } else if (cmd == '4') {
    systemState->manualOverride = true;
    systemState->manualGreenIndex = 1;
    applyManualOverride(nowMs);
    Serial.println("Manual override: B GREEN.");
  } else if (cmd == '5') {
    clearCounters();
  } else if (cmd == '6') {
    printMenu();
  } else if (cmd == '7') {
    cycleLogInterval();
  } else if (cmd == '8') {
    printTimingConfig();
  } else if (cmd == '\n' || cmd == '\r') {
    // Ignore newline characters.
  } else {
    Serial.println("Unknown command. Press 6 for menu.");
  }
}

bool allocateSystem() {
  systemState = (TrafficSystem*)malloc(sizeof(TrafficSystem));
  if (systemState == NULL) return false;

  systemState->count = 2;
  systemState->configs = (IntersectionConfig*)malloc(systemState->count * sizeof(IntersectionConfig));
  systemState->runtime = (IntersectionRuntime*)malloc(systemState->count * sizeof(IntersectionRuntime));

  if (systemState->configs == NULL || systemState->runtime == NULL) {
    free(systemState->configs);
    free(systemState->runtime);
    free(systemState);
    systemState = NULL;
    return false;
  }

  systemState->configs[0].name = "Intersection A";
  systemState->configs[0].lights.red = 2;
  systemState->configs[0].lights.yellow = 3;
  systemState->configs[0].lights.green = 4;
  systemState->configs[0].sensor.vehicleButton = 8;

  systemState->configs[1].name = "Intersection B";
  systemState->configs[1].lights.red = 5;
  systemState->configs[1].lights.yellow = 6;
  systemState->configs[1].lights.green = 7;
  systemState->configs[1].sensor.vehicleButton = 9;

  for (int i = 0; i < systemState->count; i++) {
    systemState->runtime[i].state = STATE_RED;
    systemState->runtime[i].stateStartedAt = 0;
    systemState->runtime[i].redDurationMs = BASE_RED_MS;
    systemState->runtime[i].yellowDurationMs = YELLOW_MS;
    systemState->runtime[i].greenDurationMs = BASE_GREEN_MS;
    systemState->runtime[i].vehiclesDetected = 0;
    systemState->runtime[i].cycleCount = 0;
    systemState->runtime[i].vehicleWaiting = false;
    systemState->runtime[i].lastButtonLevel = false;
  }

  systemState->manualOverride = false;
  systemState->manualGreenIndex = 0;
  systemState->lastLogAt = 0;
  systemState->logIntervalMs = 2000;
  systemState->activeIntersection = 0;
  systemState->phase = STATE_GREEN;
  systemState->phaseStartedAt = 0;

  return true;
}

void setupPins() {
  if (systemState == NULL) return;

  for (int i = 0; i < systemState->count; i++) {
    pinMode(systemState->configs[i].lights.red, OUTPUT);
    pinMode(systemState->configs[i].lights.yellow, OUTPUT);
    pinMode(systemState->configs[i].lights.green, OUTPUT);
    pinMode(systemState->configs[i].sensor.vehicleButton, INPUT_PULLUP);
    setLightsByIndex(i, STATE_RED);
  }
}

void periodicSerialLog(unsigned long nowMs) {
  if (systemState == NULL) return;
  if (nowMs - systemState->lastLogAt < systemState->logIntervalMs) return;

  systemState->lastLogAt = nowMs;

  Serial.print("LOG,");
  Serial.print(nowMs);

  for (int i = 0; i < systemState->count; i++) {
    IntersectionConfig* c = &systemState->configs[i];
    IntersectionRuntime* r = &systemState->runtime[i];

    Serial.print(",");
    Serial.print(c->name);
    Serial.print(",");
    Serial.print(stateToText(r->state));
    Serial.print(",V=");
    Serial.print(r->vehiclesDetected);
    Serial.print(",C=");
    Serial.print(r->cycleCount);
  }
  Serial.println();
}

void setup() {
  Serial.begin(9600);

  if (!allocateSystem()) {
    Serial.println("FATAL: failed to allocate system memory.");
    while (true) {
    }
  }

  setupPins();

  unsigned long nowMs = millis();
  systemState->activeIntersection = 0;
  systemState->phase = STATE_GREEN;
  systemState->phaseStartedAt = nowMs;
  applyAutoStateFromPhase(nowMs);

  printMenu();
  Serial.println("System initialized.");
}

void loop() {
  unsigned long nowMs = millis();

  readSensors();
  handleSerialMenu(nowMs);

  if (systemState->manualOverride) {
    applyManualOverride(nowMs);
  } else {
    autoSchedule(nowMs);
  }

  periodicSerialLog(nowMs);
}
