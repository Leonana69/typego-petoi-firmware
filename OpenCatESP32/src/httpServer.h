#ifndef PETOI_HTTP_SERVER_H
#define PETOI_HTTP_SERVER_H

// Plain HTTP control surface for the Petoi firmware. Runs alongside the
// WebSocket server in webServer.h — shares the same command-dispatch globals
// (token / newCmd / cmdLen / newCmdIdx / cmdFromWeb / webTaskActive /
// webResponse) and waits for reaction() on the main loop task to finish
// before replying.

#include <WebServer.h>
#include <ArduinoJson.h>

#define HTTP_PORT 80
#define HTTP_CMD_TIMEOUT_MS 30000
#define HTTP_TASK_STACK 4096
// Pin to core 1 (Arduino loop core). Core 0 is shared by BLE + WiFi stacks —
// leaving heap/CPU headroom there avoids std::bad_alloc during BLE scan.
#define HTTP_TASK_CORE 1

static WebServer httpServer(HTTP_PORT);
static TaskHandle_t httpTaskHandle = NULL;
static portMUX_TYPE httpDispatchMux = portMUX_INITIALIZER_UNLOCKED;

// --- Dispatch primitives ----------------------------------------------------

// Atomically check-and-claim the shared web dispatch slot. Returns false if
// another web/WS task already owns it.
static bool httpTryClaim() {
  portENTER_CRITICAL(&httpDispatchMux);
  if (webTaskActive) {
    portEXIT_CRITICAL(&httpDispatchMux);
    return false;
  }
  webTaskActive = true;
  cmdFromWeb = true;
  currentWebTaskId = "";  // keep webTasks map untouched; completeWebTask() will no-op the map path
  portEXIT_CRITICAL(&httpDispatchMux);
  return true;
}

static void httpForceRelease() {
  portENTER_CRITICAL(&httpDispatchMux);
  webTaskActive = false;
  cmdFromWeb = false;
  portEXIT_CRITICAL(&httpDispatchMux);
}

// Populate dispatch globals and block (on HTTP task, core 0) until the main
// loop's reaction() calls finishWebCommand()→completeWebTask(), which clears
// webTaskActive. Returns false on timeout or if argLen doesn't fit newCmd.
static bool runHttpCommandBlocking(char tk, const char *args, size_t argLen,
                                   String &outResponse,
                                   unsigned long timeoutMs = HTTP_CMD_TIMEOUT_MS) {
  if (argLen > (size_t)BUFF_LEN) return false;
  if (!httpTryClaim()) return false;

  token = tk;
  if (argLen > 0) memcpy(newCmd, args, argLen);
  cmdLen = argLen;
  newCmd[argLen] = (tk >= 'A' && tk <= 'Z') ? '~' : '\0';
  newCmdIdx = 4;
  webResponse = "";

  unsigned long start = millis();
  while (webTaskActive && (millis() - start) < timeoutMs) {
    vTaskDelay(pdMS_TO_TICKS(5));
  }

  bool ok = !webTaskActive;
  if (!ok) httpForceRelease();
  outResponse = webResponse;
  return ok;
}

// --- Response helpers -------------------------------------------------------

static void httpSendJson(int code, const String &body) {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.send(code, "application/json", body);
}

static void httpSendOk(char tk, const String &cmd, unsigned long durationMs,
                      const String &response) {
  JsonDocument doc;
  doc["ok"] = true;
  char ts[2] = {tk, 0};
  doc["token"] = ts;
  doc["cmd"] = cmd;
  doc["durationMs"] = (uint32_t)durationMs;
  doc["response"] = response;
  String out;
  serializeJson(doc, out);
  httpSendJson(200, out);
}

static void httpSendError(int code, const char *error, const String &detail = "") {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = error;
  if (detail.length()) doc["detail"] = detail;
  String out;
  serializeJson(doc, out);
  httpSendJson(code, out);
}

// Run a command and produce the standard JSON response. Used by most routes.
static void httpRunAndReply(char tk, const String &args) {
  unsigned long t0 = millis();
  String resp;
  bool ok = runHttpCommandBlocking(tk, args.c_str(), args.length(), resp);
  if (ok) {
    httpSendOk(tk, args, millis() - t0, resp);
  } else if (webTaskActive) {
    httpSendError(409, "busy", "another web task is running");
  } else {
    httpSendError(408, "timeout", "command did not complete in time");
  }
}

static String httpBody() {
  if (httpServer.hasArg("plain")) return httpServer.arg("plain");
  return String();
}

// --- Route handlers ---------------------------------------------------------

static const char *httpModelName() {
#if defined(BITTLE)
  return "Bittle";
#elif defined(NYBBLE)
  return "Nybble";
#elif defined(CUB)
  return "Cub";
#else
  return "Unknown";
#endif
}

static void httpHandleStatus() {
  JsonDocument doc;
  doc["model"] = httpModelName();
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["uptimeMs"] = (uint32_t)millis();
  doc["busy"] = webTaskActive;
  doc["wsClients"] = (int)connectedClients.size();
  doc["freeHeap"] = (uint32_t)ESP.getFreeHeap();
  String out;
  serializeJson(doc, out);
  httpSendJson(200, out);
}

static void httpHandleSkills() {
  const size_t nSkills = sizeof(progmemPointer) / sizeof(progmemPointer[0]);
  JsonDocument doc;
  JsonArray postures = doc["postures"].to<JsonArray>();
  JsonArray gaits = doc["gaits"].to<JsonArray>();
  JsonArray behaviors = doc["behaviors"].to<JsonArray>();

  char nameBuf[24];
  for (size_t i = 0; i < nSkills; i++) {
    const char *raw = skillNameWithType[i];
    size_t len = strlen(raw);
    if (len == 0 || len >= sizeof(nameBuf)) continue;
    // drop the trailing type char (I = instinct / PROGMEM, N = newbility, etc.)
    memcpy(nameBuf, raw, len - 1);
    nameBuf[len - 1] = '\0';

    int8_t period = (int8_t)pgm_read_byte(progmemPointer[i]);
    if (period == 1) postures.add((const char *)nameBuf);
    else if (period > 1) gaits.add((const char *)nameBuf);
    else behaviors.add((const char *)nameBuf);
  }
  doc["model"] = httpModelName();
  doc["count"] = (uint32_t)nSkills;
  String out;
  serializeJson(doc, out);
  httpSendJson(200, out);
}

static void httpHandleSkill() {
  JsonDocument req;
  if (deserializeJson(req, httpBody())) {
    httpSendError(400, "bad_request", "invalid JSON");
    return;
  }
  const char *name = req["name"] | (const char *)nullptr;
  if (!name || !*name) {
    httpSendError(400, "bad_request", "missing 'name'");
    return;
  }
  String args = name;
  if (req["arg"].is<int>()) {
    args += " ";
    args += String((int)req["arg"]);
  }
  httpRunAndReply('k', args);
}

static void httpHandleCmd() {
  JsonDocument req;
  if (deserializeJson(req, httpBody())) {
    httpSendError(400, "bad_request", "invalid JSON");
    return;
  }
  const char *raw = req["raw"] | (const char *)nullptr;
  if (raw && *raw) {
    char tk = raw[0];
    String args = String(raw + 1);
    httpRunAndReply(tk, args);
    return;
  }
  const char *tokStr = req["token"] | (const char *)nullptr;
  if (!tokStr || !*tokStr) {
    httpSendError(400, "bad_request", "missing 'token' or 'raw'");
    return;
  }
  String args = req["args"].is<const char *>() ? String((const char *)req["args"]) : String();
  httpRunAndReply(tokStr[0], args);
}

static void httpHandleMove() {
  JsonDocument req;
  if (deserializeJson(req, httpBody())) {
    httpSendError(400, "bad_request", "invalid JSON");
    return;
  }
  JsonArrayConst joints = req["joints"];
  if (joints.isNull() || joints.size() == 0) {
    httpSendError(400, "bad_request", "missing 'joints' array");
    return;
  }
  String mode = req["mode"] | "simultaneous";
  char tk = (mode == "sequential") ? 'm' : 'i';

  String args;
  bool first = true;
  for (JsonVariantConst j : joints) {
    if (!first) args += " ";
    args += String((int)j["index"]);
    args += " ";
    args += String((int)j["angle"]);
    first = false;
  }
  httpRunAndReply(tk, args);
}

static void httpHandleTilt() {
  JsonDocument req;
  if (deserializeJson(req, httpBody())) {
    httpSendError(400, "bad_request", "invalid JSON");
    return;
  }
  int roll = req["roll"] | 0;
  int pitch = req["pitch"] | 0;
  String args = String(roll) + " " + String(pitch);
  httpRunAndReply('t', args);
}

// POST /euler {"roll": <rad>, "pitch": <rad>} — yaw ignored (balance loop only
// corrects roll/pitch). Emits "t 1 <pitch_deg> 2 <roll_deg>" so axis indices
// match the firmware's T_TILT parser; angles clamped to int8_t (yprTilt range).
static void httpHandleEuler() {
  JsonDocument req;
  if (deserializeJson(req, httpBody())) {
    httpSendError(400, "bad_request", "invalid JSON");
    return;
  }
  float rollRad = req["roll"] | 0.0f;
  float pitchRad = req["pitch"] | 0.0f;
  int rollDeg = constrain((int)lroundf(rollRad * RAD_TO_DEG), -127, 127);
  int pitchDeg = constrain((int)lroundf(pitchRad * RAD_TO_DEG), -127, 127);
  String args = "1 " + String(pitchDeg) + " 2 " + String(rollDeg);
  httpRunAndReply('t', args);
}

static void httpHandleBeep() {
  JsonDocument req;
  if (deserializeJson(req, httpBody())) {
    httpSendError(400, "bad_request", "invalid JSON");
    return;
  }
  JsonArrayConst notes = req["notes"];
  if (notes.isNull() || notes.size() == 0) {
    httpSendError(400, "bad_request", "missing 'notes' array");
    return;
  }
  String args;
  bool first = true;
  for (JsonVariantConst pair : notes) {
    if (!first) args += " ";
    args += String((int)pair[0]);
    args += " ";
    args += String((int)pair[1]);
    first = false;
  }
  httpRunAndReply('b', args);
}

static void httpHandleGyro() {
  JsonDocument req;
  if (deserializeJson(req, httpBody())) {
    httpSendError(400, "bad_request", "invalid JSON");
    return;
  }
  // Token 'g' toggles. If caller specifies enabled and current state already
  // matches, the firmware will flip it anyway — matches existing WS behavior.
  httpRunAndReply('g', String());
}

static void httpHandleSpeed() {
  JsonDocument req;
  if (deserializeJson(req, httpBody())) {
    httpSendError(400, "bad_request", "invalid JSON");
    return;
  }
  int delta = req["delta"] | 1;
  char tk = (delta >= 0) ? '.' : ',';
  int steps = abs(delta);
  if (steps == 0) steps = 1;
  String resp;
  unsigned long t0 = millis();
  bool ok = true;
  for (int i = 0; i < steps && ok; i++) {
    ok = runHttpCommandBlocking(tk, "", 0, resp, 5000);
  }
  if (ok) httpSendOk(tk, String(delta), millis() - t0, resp);
  else httpSendError(409, "busy_or_timeout");
}

static void httpHandleStop()    { httpRunAndReply('k', "rest"); }
static void httpHandleRest()    { httpRunAndReply('k', "rest"); }
static void httpHandleBalance() { httpRunAndReply('k', "balance"); }
static void httpHandleUp()      { httpRunAndReply('k', "up"); }
static void httpHandleZero()    { httpRunAndReply('k', "zero"); }

static void httpHandleOptions() {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  httpServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  httpServer.send(204);
}

static void httpHandleRoot() {
  String body = String("Petoi ") + httpModelName() +
                " HTTP control. Endpoints: GET /status, GET /skills, "
                "POST /skill, POST /cmd, POST /move, POST /tilt, POST /euler, POST /beep, "
                "POST /gyro, POST /speed, POST /stop, POST /rest, POST /balance, "
                "POST /up, POST /zero";
  httpServer.send(200, "text/plain", body);
}

// --- Server task ------------------------------------------------------------

static void httpServerTask(void *) {
  httpServer.on("/", HTTP_GET, httpHandleRoot);
  httpServer.on("/status", HTTP_GET, httpHandleStatus);
  httpServer.on("/skills", HTTP_GET, httpHandleSkills);
  httpServer.on("/skill", HTTP_POST, httpHandleSkill);
  httpServer.on("/cmd", HTTP_POST, httpHandleCmd);
  httpServer.on("/move", HTTP_POST, httpHandleMove);
  httpServer.on("/tilt", HTTP_POST, httpHandleTilt);
  httpServer.on("/euler", HTTP_POST, httpHandleEuler);
  httpServer.on("/beep", HTTP_POST, httpHandleBeep);
  httpServer.on("/gyro", HTTP_POST, httpHandleGyro);
  httpServer.on("/speed", HTTP_POST, httpHandleSpeed);
  httpServer.on("/stop", HTTP_POST, httpHandleStop);
  httpServer.on("/rest", HTTP_POST, httpHandleRest);
  httpServer.on("/balance", HTTP_POST, httpHandleBalance);
  httpServer.on("/up", HTTP_POST, httpHandleUp);
  httpServer.on("/zero", HTTP_POST, httpHandleZero);
  httpServer.onNotFound([]() { httpSendError(404, "not_found"); });
  // Simple CORS preflight for every route.
  httpServer.on("/status", HTTP_OPTIONS, httpHandleOptions);
  httpServer.on("/skills", HTTP_OPTIONS, httpHandleOptions);
  httpServer.on("/skill", HTTP_OPTIONS, httpHandleOptions);
  httpServer.on("/cmd", HTTP_OPTIONS, httpHandleOptions);
  httpServer.on("/move", HTTP_OPTIONS, httpHandleOptions);

  httpServer.begin();
  printToAllPorts("HTTP server started on :" + String(HTTP_PORT));

  for (;;) {
    httpServer.handleClient();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void startHttpServer() {
  if (httpTaskHandle) return;
  xTaskCreatePinnedToCore(httpServerTask, "http", HTTP_TASK_STACK, NULL, 1,
                          &httpTaskHandle, HTTP_TASK_CORE);
}

#endif  // PETOI_HTTP_SERVER_H
