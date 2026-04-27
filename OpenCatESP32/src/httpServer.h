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

// True once /euler has set a non-zero pitch or roll. Cleared by
// resetTiltIfActive(), which gait-bound dispatches (/nav, /skill) call so a
// leftover lean from a prior /euler doesn't fight the gait.
static bool tiltActive = false;

// PID state for the closed-loop pitch task. Layered on top of the open-loop
// fold from eulerPitchToFoldDeg(): bias = Kp*err + Ki*∫err + Kd*d_err in
// degrees, added to the open-loop fold each tick. Active iff pitchPidActive.
static volatile float pitchTargetRad = 0.0f;
static volatile bool pitchPidActive = false;
static volatile float pidKp = 0.5f;
static volatile float pidKi = 0.1f;
static volatile float pidKd = 0.0f;

static void resetTiltIfActive() {
  if (!tiltActive) return;
  tiltActive = false;
  pitchPidActive = false;
  pitchTargetRad = 0.0f;
  balanceSlope[1] = 1;
  yprTilt[0] = yprTilt[1] = yprTilt[2] = 0;
  // Wait briefly for any in-flight PID dispatch to release the slot. Without
  // this the caller's own dispatch races PID and gets a 409 (busy).
  unsigned long deadline = millis() + 250;
  while (webTaskActive && (long)(millis() - deadline) < 0) {
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  // Knees don't need an explicit reset: the gait /skill being loaded next
  // will overwrite all leg joints.
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

static void httpHandleVersion() {
  JsonDocument doc;
  doc["buildDate"] = __DATE__;
  doc["buildTime"] = __TIME__;
  doc["model"] = httpModelName();
  String out;
  serializeJson(doc, out);
  httpSendJson(200, out);
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
  resetTiltIfActive();
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

// Bittle balance posture: hips and knees rest at 29° (per `j` query). /euler
// pins all 8 leg joints to absolute targets so leftover currentAdjust[] from
// the IMU loop can't bake itself into the new posture.
#define EULER_BALANCE_HIP 29
#define EULER_BALANCE_KNEE 29
#define EULER_PITCH_GAIN 1.5f      // joint deg of fold per deg of commanded pitch
#define EULER_PITCH_CLAMP_DEG 30   // knee min ≈ EULER_BALANCE_KNEE - clamp = -1 → 0 in hardware

// Signed open-loop fold: positive = front squat (head down), negative = rear.
static int eulerPitchToFoldDeg(float pitchRad) {
  return constrain((int)lroundf(pitchRad * RAD_TO_DEG * EULER_PITCH_GAIN),
                   -EULER_PITCH_CLAMP_DEG, EULER_PITCH_CLAMP_DEG);
}

// Build the `i`-token argument string that pins all 8 leg joints for a given
// signed fold. Front knees+hips fold for positive fold, rear for negative.
static String eulerJointArgs(int foldDeg) {
  int frontFold = max(0, foldDeg);
  int rearFold = max(0, -foldDeg);
  int frontKnee = EULER_BALANCE_KNEE - frontFold;
  int rearKnee = EULER_BALANCE_KNEE - rearFold;
  int frontHip = EULER_BALANCE_HIP - frontFold;
  int rearHip = EULER_BALANCE_HIP + rearFold;
  return "8 " + String(frontHip) + " 9 " + String(frontHip) +
         " 10 " + String(rearHip) + " 11 " + String(rearHip) +
         " 12 " + String(frontKnee) + " 13 " + String(frontKnee) +
         " 14 " + String(rearKnee) + " 15 " + String(rearKnee);
}

// PID tick: runs from the HTTP server task's main loop (see httpServerTask)
// rather than a separate FreeRTOS task. Sharing the HTTP task naturally
// serializes PID dispatches with HTTP request dispatches — no contention on
// webTaskActive, and no risk of starting a PID task that breaks scheduling.
//
// Reads ypr[1] (degrees, updated by IMU loop in skill.h:373) and dispatches
// an `i`-token frame. CAUTION: do NOT write directly into skill->dutyAngles
// — that pointer aliases into newCmd (skill.h:140), and concurrent writes
// corrupt command parsing badly enough to crash the WiFi stack.
#define PID_PERIOD_MS 200
static unsigned long pidLastTickMs = 0;

static void runPidTick() {
  static float lastErrorDeg = 0;
  static float integralDeg = 0;
  static unsigned long lastTime = 0;

  if (!pitchPidActive) {
    lastErrorDeg = 0;
    integralDeg = 0;
    lastTime = millis();
    return;
  }

  unsigned long now = millis();
  float dt = lastTime == 0 ? (PID_PERIOD_MS / 1000.0f)
                            : max(0.001f, (now - lastTime) / 1000.0f);
  lastTime = now;

  float currentDeg = ypr[1];
  float targetDeg = pitchTargetRad * RAD_TO_DEG;
  float errorDeg = targetDeg - currentDeg;
  integralDeg += errorDeg * dt;
  float iCap = EULER_PITCH_CLAMP_DEG / max((float)pidKi, 0.01f);
  integralDeg = constrain(integralDeg, -iCap, iCap);
  float derivDeg = (errorDeg - lastErrorDeg) / dt;
  lastErrorDeg = errorDeg;

  float biasDeg = pidKp * errorDeg + pidKi * integralDeg + pidKd * derivDeg;
  int openFold = eulerPitchToFoldDeg(pitchTargetRad);
  int totalFold = constrain((int)lroundf(openFold + biasDeg),
                            -EULER_PITCH_CLAMP_DEG, EULER_PITCH_CLAMP_DEG);

  String iArgs = eulerJointArgs(totalFold);
  String unused;
  runHttpCommandBlocking('i', iArgs.c_str(), iArgs.length(), unused, 1000);
}

// POST /euler {"roll": <rad>, "pitch": <rad>} — yaw ignored.
// Roll uses the IMU balance loop via yprTilt[2] (direct write — no dispatch).
// Pitch arms the PID task; this handler dispatches the initial open-loop pose
// so the user sees motion immediately, then the PID continues at 20 Hz.
static void httpHandleEuler() {
  JsonDocument req;
  if (deserializeJson(req, httpBody())) {
    httpSendError(400, "bad_request", "invalid JSON");
    return;
  }
  float rollRad = req["roll"] | 0.0f;
  float pitchRad = req["pitch"] | 0.0f;
  int rollDeg = constrain((int)lroundf(rollRad * RAD_TO_DEG), -127, 127);

  pitchTargetRad = pitchRad;
  pitchPidActive = (fabsf(pitchRad) > 0.01f);
  tiltActive = pitchPidActive || (rollDeg != 0);
  balanceSlope[1] = pitchPidActive ? 0 : 1;

  yprTilt[1] = 0;
  yprTilt[2] = rollDeg;

  int openFold = eulerPitchToFoldDeg(pitchRad);
  String iArgs = eulerJointArgs(openFold);
  httpRunAndReply('i', iArgs);
}

static void httpSendPidJson() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["kp"] = pidKp;
  doc["ki"] = pidKi;
  doc["kd"] = pidKd;
  doc["active"] = pitchPidActive;
  doc["targetDeg"] = pitchTargetRad * RAD_TO_DEG;
  doc["currentDeg"] = ypr[1];
  String out;
  serializeJson(doc, out);
  httpSendJson(200, out);
}

static void httpHandlePidGet() { httpSendPidJson(); }

static void httpHandlePidPost() {
  JsonDocument req;
  if (deserializeJson(req, httpBody())) {
    httpSendError(400, "bad_request", "invalid JSON");
    return;
  }
  if (!req["kp"].isNull()) pidKp = req["kp"].as<float>();
  if (!req["ki"].isNull()) pidKi = req["ki"].as<float>();
  if (!req["kd"].isNull()) pidKd = req["kd"].as<float>();
  httpSendPidJson();
}

// POST /nav {"vx": <m/s>, "vy": <m/s>, "vyaw": <rad/s>}
// Maps a body-frame velocity command to the closest gait skill. vy is ignored
// — quadruped gaits in this firmware have no native lateral translation.
// Magnitudes are only used as a dead-zone; speed is set separately via /speed.
static void httpHandleNav() {
  JsonDocument req;
  if (deserializeJson(req, httpBody())) {
    httpSendError(400, "bad_request", "invalid JSON");
    return;
  }
  float vx = req["vx"] | 0.0f;
  float vyaw = req["vyaw"] | 0.0f;

  const float LIN_DEAD = 0.05f;
  const float ANG_DEAD = 0.1f;
  const char *skill;
  if (fabsf(vx) < LIN_DEAD && fabsf(vyaw) < ANG_DEAD) {
    skill = "balance";
  } else if (vx > LIN_DEAD) {
    skill = (vyaw > ANG_DEAD) ? "wkL" : (vyaw < -ANG_DEAD) ? "wkR" : "wkF";
  } else if (vx < -LIN_DEAD) {
    skill = (vyaw > ANG_DEAD) ? "bkL" : (vyaw < -ANG_DEAD) ? "bkR" : "bk";
  } else {
    skill = (vyaw > 0) ? "wkL" : "wkR";
  }
  resetTiltIfActive();
  httpRunAndReply('k', String(skill));
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

// Convenience skill-load handlers — must reset tilt state so PID stops
// dispatching `i`-tokens that would override the recovery posture.
static void httpHandleStop()    { resetTiltIfActive(); httpRunAndReply('k', "rest"); }
static void httpHandleRest()    { resetTiltIfActive(); httpRunAndReply('k', "rest"); }
static void httpHandleBalance() { resetTiltIfActive(); httpRunAndReply('k', "balance"); }
static void httpHandleUp()      { resetTiltIfActive(); httpRunAndReply('k', "up"); }
static void httpHandleZero()    { resetTiltIfActive(); httpRunAndReply('k', "zero"); }

static void httpHandleOptions() {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  httpServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  httpServer.send(204);
}

static void httpHandleRoot() {
  String body = String("Petoi ") + httpModelName() +
                " HTTP control. Endpoints: GET /status, GET /version, GET /skills, "
                "POST /skill, POST /cmd, POST /move, POST /tilt, POST /euler, POST /nav, "
                "GET /pid, POST /pid, POST /beep, "
                "POST /gyro, POST /speed, POST /stop, POST /rest, POST /balance, "
                "POST /up, POST /zero";
  httpServer.send(200, "text/plain", body);
}

// --- Server task ------------------------------------------------------------

static void httpServerTask(void *) {
  httpServer.on("/", HTTP_GET, httpHandleRoot);
  httpServer.on("/status", HTTP_GET, httpHandleStatus);
  httpServer.on("/version", HTTP_GET, httpHandleVersion);
  httpServer.on("/skills", HTTP_GET, httpHandleSkills);
  httpServer.on("/skill", HTTP_POST, httpHandleSkill);
  httpServer.on("/cmd", HTTP_POST, httpHandleCmd);
  httpServer.on("/move", HTTP_POST, httpHandleMove);
  httpServer.on("/tilt", HTTP_POST, httpHandleTilt);
  httpServer.on("/euler", HTTP_POST, httpHandleEuler);
  httpServer.on("/nav", HTTP_POST, httpHandleNav);
  httpServer.on("/pid", HTTP_GET, httpHandlePidGet);
  httpServer.on("/pid", HTTP_POST, httpHandlePidPost);
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
    if (millis() - pidLastTickMs >= PID_PERIOD_MS) {
      pidLastTickMs = millis();
      runPidTick();
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void startHttpServer() {
  if (httpTaskHandle) return;
  xTaskCreatePinnedToCore(httpServerTask, "http", HTTP_TASK_STACK, NULL, 1,
                          &httpTaskHandle, HTTP_TASK_CORE);
}

#endif  // PETOI_HTTP_SERVER_H
