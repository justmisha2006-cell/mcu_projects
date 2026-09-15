#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <EEPROM.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Update.h>

// ================= НАСТРОЙКИ =================
#define WIFI_SSID               "DIR-842-a29f"
#define WIFI_PASS               "39673305"

#define BOT_TOKEN               "8941366974:AAEnFNrODdBUG6RQaf3VpnLbx7cTjx18rR4"

// ================= OTA =================
#define FW_VERSION        "2.1.2"

#define OTA_VERSION_URL   "https://raw.githubusercontent.com/nexorkiev-lgtm/TargetiX_OS_TG/main/version.txt"
#define OTA_BIN_URL       "https://raw.githubusercontent.com/nexorkiev-lgtm/TargetiX_OS_TG/main/firmware.bin"

// ================= ПІНИ =================
#define RED_LED                 2
#define GREEN_LED               4
#define BUTTON_PIN              19

// ================= EEPROM =================
#define EEPROM_SIZE             512
#define PIN_ADDR                0
#define LAST_GREEN_ADDR         8
#define UPTIME_ADDR             16
#define CONFIG_FLAG_ADDR        32   // 1 байт (0xAA = налаштовано)
#define NAME_ADDR               40
#define SURNAME_ADDR            80
#define COUNTRY_ADDR            120
#define REGION_ADDR             160
#define WIFI_SSID_ADDR          200
#define WIFI_PASS_ADDR          240
#define CHAT_ID_ADDR            300

// ================= ЧАСИ =================
#define RED_TIME                7000UL

// ================= БАТАРЕЯ =================
#define BAT_ADC_PIN             35   // ADC для батареи
#define R1                      100.0 // кОм
#define R2                      100.0 // кОм

// ================= PROTOTYPES =================
void tg(String msg);
void setupWebServer();
void startProgram(unsigned long greenSec);
void stopProgram();
void startMP5();
void loadEEPROM();
float readBatteryVoltage();
int batteryPercent();
void handlePhysicalButton();

// ================= Wi-Fi =================
String wifiSSID = WIFI_SSID;
String wifiPASS = WIFI_PASS;
String oldWifiSSID = "";
String oldWifiPASS = "";

// ================= Telegram =================
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

WebServer server(80);
bool configMode = false;

String userName, userSurname, userCountry, userRegion;

bool tgActive = false;   // 💬 пользователь начал диалог

bool wifiSelectMode = false;
bool wifiPassMode   = false;

bool otaChecked = false;
bool otaAvailable = false;
bool otaInProgress = false;
String remoteVersion = "";

String chatID = "";
bool chatBound = false;

uint8_t mp5GreenCount = 0;   // сколько зелёных уже было

bool mp5Mode = false;
uint8_t mp5Step = 0;
bool lastWasMP5 = false;

unsigned long lastUptimeSave = 0;

String wifiPassInput = "";

unsigned long wifiMenuTimer = 0;
unsigned long wifiReconnectTimer = 0;

// ====== MULTI PROGRAM ======
unsigned int mpCycles = 0;       // скільки циклів потрібно
unsigned int mpCyclesDone = 0;   // скільки вже виконано

// ================= СТАНИ =================
enum ProgramState {
  PS_IDLE,
  PS_RED,
  PS_GREEN
};

bool finalRedStage = false;   // 🔴 финальный красный (МП8/МП10)

ProgramState progState = PS_IDLE;
bool programRunning = false;

unsigned long stageStart = 0;
unsigned long greenTime  = 0;
unsigned long lastGreenTime = 5;

// ================= PIN =================
bool authorized = false;
bool pinExists  = false;

String storedPin  = "";
String enteredPin = "";

// ================= Telegram =================
unsigned long tgTimer = 0;
bool onlineSent = false;

unsigned long otaCheckTimer = 0;  // таймер для проверки OTA

// ================= UPTIME =================
unsigned long totalUptimeSec = 0;
unsigned long lastSecondTick = 0;

// ================= КНОПКА (DEBOUNCE) =================
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; // 50 мс защита от дребезга

String readEEPROMString(int addr, int maxLen) {
  char buf[maxLen + 1];
  for (int i = 0; i < maxLen; i++) {
    uint8_t b = EEPROM.read(addr + i);
    if (b == 0xFF || b == 0) {
      buf[i] = 0;
      break;
    }
    buf[i] = (char)b;
  }
  buf[maxLen] = 0;
  return String(buf);
}

void showSavedData() {
  if (!chatBound) return;

  String msg = "📋 Збережені дані:\n\n";
  msg += "👤 Імʼя: " + readEEPROMString(NAME_ADDR, 40) + "\n";
  msg += "👤 Прізвище: " + readEEPROMString(SURNAME_ADDR, 40) + "\n";
  msg += "🌍 Країна: " + readEEPROMString(COUNTRY_ADDR, 40) + "\n";
  msg += "📶 Wi-Fi SSID: " + readEEPROMString(WIFI_SSID_ADDR, 32) + "\n";
  msg += "🔑 Wi-Fi Пароль: " + readEEPROMString(WIFI_PASS_ADDR, 64);

  bot.sendMessage(chatID, msg, "");
}

// 6. ОПТИМИЗАЦИЯ ADC (Усреднение измерений)
float readBatteryVoltage() {
  uint32_t rawSum = 0;
  const int samples = 16; // 16 выборок для сглаживания шумов

  for (int i = 0; i < samples; i++) {
    rawSum += analogRead(BAT_ADC_PIN);
    delayMicroseconds(100);
  }
  float raw = (float)rawSum / samples;

  float v_adc = raw * (3.3f / 4095.0f);      // напряжение на ADC
  float voltage = v_adc * (R1 + R2) / R2;  // восстанавливаем напряжение LiPo
  return voltage;
}

int batteryPercent() {
  float v = readBatteryVoltage();
  if (v >= 4.2f) return 100;
  if (v <= 3.2f) return 0;
  return (int)((v - 3.2f) / (4.2f - 3.2f) * 100.0f);
}

void saveChatID(String id) {
  for (int i = 0; i < id.length(); i++) {
    EEPROM.write(CHAT_ID_ADDR + i, id[i]);
  }
  EEPROM.write(CHAT_ID_ADDR + id.length(), 0);
  EEPROM.commit();

  chatID = id;
  chatBound = true;
}

String remoteChangelog = "";  // глобально

bool checkOTAUpdate() {

  WiFiClientSecure otaClient;
  otaClient.setInsecure();

  HTTPClient https;
  Serial.println("[OTA] Checking version...");

  // Получаем версию
  if (!https.begin(otaClient, OTA_VERSION_URL)) {
    Serial.println("[OTA] HTTPS begin failed");
    return false;
  }
  int httpCode = https.GET();
  if (httpCode != 200) {
    Serial.println("[OTA] Version GET failed");
    https.end();
    return false;
  }
  remoteVersion = https.getString();
  remoteVersion.trim();
  https.end();

  // Получаем список изменений
  if (!https.begin(otaClient, "https://raw.githubusercontent.com/mihajloviperua-coder/esp32-ota/main/changelog.txt")) {
    Serial.println("[OTA] Changelog GET failed");
    return remoteVersion != FW_VERSION;
  }
  httpCode = https.GET();
  if (httpCode == 200) {
    remoteChangelog = https.getString();
  } else {
    remoteChangelog = "📝 Список змін недоступний";
  }
  https.end();

  Serial.println("[OTA] Local: " + String(FW_VERSION));
  Serial.println("[OTA] Remote: " + remoteVersion);

  return remoteVersion != FW_VERSION;
}

void pressButtonSimulation() {
  pinMode(BUTTON_PIN, OUTPUT);    // перевести в выход
  digitalWrite(BUTTON_PIN, LOW);  // имитация нажатия
  delay(200);                     // удерживаем 200 мс
  digitalWrite(BUTTON_PIN, HIGH); // отпускаем
  pinMode(BUTTON_PIN, INPUT_PULLUP); // вернуть обратно как вход
}

void otaKeyboard() {

  String kb =
    "[[\"⬇️ Завантажити\"],"
    "[\"⏭ Пропустити\"]]";

  bot.sendMessageWithReplyKeyboard(
    chatID,
    "🆕 Доступне оновлення\n"
    "📦 Версія: " + remoteVersion +
    "\n\n📝 Зміни:\n" + remoteChangelog,
    "",
    kb,
    true
  );
}

void startOTA() {

  otaInProgress = true;

  bot.sendMessage(
    chatID,
    "⏳ Зачекайте, йде завантаження оновлення…",
    ""
  );

  WiFiClientSecure otaClient;
  otaClient.setInsecure();

  HTTPClient https;
  https.begin(otaClient, OTA_BIN_URL);

  int httpCode = https.GET();
  if (httpCode != 200) {
    tg("❌ Помилка OTA");
    https.end();
    otaInProgress = false;
    return;
  }

  int contentLength = https.getSize();
  bool canBegin = Update.begin(contentLength);

  if (!canBegin) {
    tg("❌ Недостатньо памʼяті");
    https.end();
    otaInProgress = false;
    return;
  }

  WiFiClient * stream = https.getStreamPtr();
  size_t written = Update.writeStream(*stream);

  if (written == contentLength && Update.end(true)) {
    tg("✅ Оновлення завершено\n🔄 Перезавантаження…");
    delay(1500);
    ESP.restart();
  }
  else {
    tg("❌ Помилка запису OTA");
    Update.end(false);
  }

  https.end();
  otaInProgress = false;
}

void resetUptime() {

  totalUptimeSec = 0;
  lastSecondTick = millis();

  EEPROM.put(UPTIME_ADDR, totalUptimeSec);
  EEPROM.commit();

  tg("⏱ Час роботи скинуто до 0");
  Serial.println("[UPTIME] Reset");
}

void factoryReset() {

  // 📢 уведомляем владельца
  tg("⚠️ Скидання до заводських налаштувань...\n🔄 Перезавантаження");
  delay(1000);

  // 🔥 полная очистка EEPROM
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();

  // 🔥 СБРОС TELEGRAM
  chatID = "";
  chatBound = false;
  tgActive = false;

  // 🔥 СБРОС PIN
  authorized = false;
  pinExists  = false;
  storedPin  = "";
  enteredPin = "";

  // 🔥 СБРОС WI-FI (важно!)
  wifiSSID = "";
  wifiPASS = "";

  // 🔥 выходы
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);

  delay(1500);
  ESP.restart();
}

bool isConfigured() {
  return EEPROM.read(CONFIG_FLAG_ADDR) == 0xAA;
}

void setConfigured() {
  EEPROM.write(CONFIG_FLAG_ADDR, 0xAA);
  EEPROM.commit();
}

void startAPMode() {
  configMode = true;

  WiFi.mode(WIFI_AP);
  WiFi.softAP("NALASHTUVANNYA");

  IPAddress ip = WiFi.softAPIP();
  Serial.println("[AP] IP: " + ip.toString());

  setupWebServer();
}

String htmlPage() {
return R"rawliteral(
<!DOCTYPE html>
<html lang="uk">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 Setup</title>

<style>
/* ================= CORE ================= */
*{
 box-sizing:border-box;
 font-family:-apple-system,BlinkMacSystemFont,"SF Pro Text",system-ui
}
html,body{
 margin:0;height:100%;overflow:hidden
}
body{
 display:flex;align-items:center;justify-content:center;
 background:
  radial-gradient(1200px at 10% 10%,#ffffffcc,transparent),
  radial-gradient(900px at 90% 30%,#e3e9ff,transparent),
  linear-gradient(180deg,#f4f6fb,#dde3ef);
 animation:bg 18s infinite alternate;
}
@keyframes bg{
 from{filter:hue-rotate(0deg)}
 to{filter:hue-rotate(12deg)}
}

/* ================= CARD ================= */
.card{
 width:92%;max-width:440px;
 background:rgba(255,255,255,.72);
 backdrop-filter:blur(36px) saturate(1.4);
 border-radius:36px;
 padding:32px;
 box-shadow:0 45px 100px rgba(0,0,0,.28)
}

/* ================= STATUS ================= */
.status{
 margin-bottom:20px
}
.status-text{
 font-size:14px;
 text-align:center;
 margin-bottom:8px;
 color:#555
}
.status-bar{
 height:6px;
 background:#d1d1d6;
 border-radius:6px;
 overflow:hidden
}
.status-bar div{
 height:100%;
 width:0%;
 background:linear-gradient(90deg,#0a84ff,#34c759);
 transition:.4s cubic-bezier(.4,0,.2,1)
}

/* ================= STEPS ================= */
.step{display:none}
.step.active{
 display:block;
 animation:slide .45s cubic-bezier(.4,0,.2,1)
}
@keyframes slide{
 from{opacity:0;transform:translateX(30px)}
 to{opacity:1}
}

h1{text-align:center;font-size:22px;margin:8px 0}
p{text-align:center;color:#666}

/* ================= INPUT ================= */
.field{position:relative;margin-top:18px}
.field span{
 position:absolute;left:14px;top:50%;
 transform:translateY(-50%);
 font-size:18px
}
input,select{
 width:100%;
 padding:14px 14px 14px 44px;
 border-radius:18px;
 border:1px solid #ccc;
 background:rgba(255,255,255,.96);
 font-size:16px;
 transition:.25s
}
input:focus,select:focus{
 outline:none;
 border-color:#007aff;
 box-shadow:0 0 0 4px rgba(0,122,255,.18)
}

/* ================= ERROR ================= */
.error{
 border-color:#ff3b30!important;
 animation:shake .32s
}
@keyframes shake{
 0%{transform:translateX(0)}
 25%{transform:translateX(-6px)}
 50%{transform:translateX(6px)}
}

/* ================= BUTTONS ================= */
.buttons{
 display:flex;gap:14px;margin-top:28px
}
button{
 flex:1;
 padding:16px;
 border-radius:22px;
 border:none;
 font-size:16px;
 background:#d1d1d6;
 color:#fff;
 cursor:pointer;
 transition:.25s;
 position:relative;
 overflow:hidden
}
button.active{
 background:linear-gradient(180deg,#0a84ff,#006ee6);
 box-shadow:0 14px 32px rgba(0,122,255,.45)
}
button:disabled{opacity:.45}
button:active{transform:scale(.96)}

/* ================= LOADER ================= */
.loader{
 width:46px;height:46px;
 border-radius:50%;
 border:4px solid #ddd;
 border-top-color:#007aff;
 animation:spin 1s linear infinite;
 margin:36px auto
}
@keyframes spin{to{transform:rotate(360deg)}}

.success{text-align:center;font-size:64px}
</style>
</head>

<body>

<div class="card">

<div class="status">
 <div class="status-text" id="statusText">Готово до старту</div>
 <div class="status-bar">
  <div id="statusProgress"></div>
 </div>
</div>

<form method="POST" action="/save">

<div class="step active">
 <h1>Вітаємо 👋</h1>
 <p>SIUS OS HL 2.1</p>
 <p>Розпочнемо налаштування пристрою</p>
 <div class="buttons">
  <button type="button" class="active" onclick="next()">Почати</button>
 </div>
</div>

<div class="step">
 <h1>Користувач</h1>
 <div class="field"><span>👤</span><input name="name" placeholder="Імʼя" oninput="validateUserStep()"></div>
 <div class="field"><span>👤</span><input name="surname" placeholder="Прізвище" oninput="validateUserStep()"></div>
 <div class="buttons">
  <button type="button" onclick="back()">⬅️</button>
  <button type="button" class="next" disabled onclick="next()">Далі</button>
 </div>
</div>

<div class="step">
 <h1>Країна</h1>
 <div class="field">
  <span>🌍</span>
  <select name="country" onchange="validate(this)">
   <option value=""></option>
   <option value="Україна">🇺🇦 Україна</option>
   <option value="Польща">🇵🇱 Польща</option>
   <option value="Німеччина">🇩🇪 Німеччина</option>
   <option value="Чехія">🇨🇿 Чехія</option>
   <option value="Італія">🇮🇹 Італія</option>
   <option value="Іспанія">🇪🇸 Іспанія</option>
   <option value="Франція">🇫🇷 Франція</option>
  </select>
 </div>
 <div class="buttons">
  <button type="button" onclick="back()">⬅️</button>
  <button type="button" class="next" disabled onclick="next()">Далі</button>
 </div>
</div>

<div class="step">
 <h1>Wi-Fi</h1>
 <div class="field">
  <span>📶</span>
  <select id="ssid" name="ssid" onchange="wifiChanged();validate(this)"></select>
 </div>
 <div class="buttons">
  <button type="button" onclick="back()">⬅️</button>
  <button type="button" class="next" disabled onclick="next()">Далі</button>
 </div>
</div>

<div class="step">
 <h1>Пароль</h1>
 <div class="field">
  <span onclick="togglePass()">👁</span>
  <input id="pass" type="password" name="pass" oninput="validate(this)">
 </div>
 <div class="buttons">
  <button type="button" onclick="back()">⬅️</button>
  <button type="button" class="next" disabled onclick="next()">Підключитись</button>
 </div>
</div>

<div class="step">
 <h1>Підключення…</h1>
 <div class="loader"></div>
</div>


<div class="step">
  <div class="success">✅</div>
  <h1>Готово</h1>
  <p>Налаштування збережено</p>

  <!-- Telegram-бот: рамка с ссылкой -->
  <p style="text-align:center; margin-top:16px;">
    <span id="botLinkBox" onclick="copyLink()" 
          style="
            display: inline-block;
            padding: 10px 16px;
            border: 2px solid #007aff;
            border-radius: 8px;
            cursor: pointer;
            user-select: all;
            color:#007aff;
            font-weight:bold;
          ">
      https://t.me/targetmaster_osBot
    </span>
  </p>

  <div class="buttons" style="text-align:center; margin-top:20px;">
    <button type="submit" class="active">Завершити</button>
  </div>

  <!-- Сообщение о копировании -->
  <p id="copiedMessage" style="
        text-align:center;
        color:#28a745;
        font-weight:bold;
        margin-top:10px;
        opacity:0;
        transition: opacity 0.3s;
      ">
    ✔ Скопійовано!
  </p>
</div>

<script>
function copyLink() {
  const text = document.getElementById("botLinkBox").innerText;
  navigator.clipboard.writeText(text).then(() => {
    const message = document.getElementById("copiedMessage");
    message.style.opacity = 1;
    setTimeout(() => { message.style.opacity = 0; }, 2000);
  }).catch(err => {
    console.error('Не удалось скопировать ссылку:', err);
  });
}
</script>

</form>
</div>

<script>
let step=0;
const steps=document.querySelectorAll(".step");
const statusText=document.getElementById("statusText");
const statusProgress=document.getElementById("statusProgress");
const ssid=document.getElementById("ssid");
const pass=document.getElementById("pass");

function setStatus(text,percent){
 statusText.textContent=text;
 statusProgress.style.width=percent+"%";
}

function show(){
 if(step<0) step=0;
 if(step>=steps.length) step=steps.length-1;

 steps.forEach(s=>s.classList.remove("active"));
 steps[step].classList.add("active");

 if(step===0){
  setStatus("SIUS OS HL 2.0",0);
 }else{
  setStatus(
   "Крок "+step+" з "+(steps.length-1),
   Math.round(step/(steps.length-1)*100)
  );
 }

 if(step===3) scanWiFi();
 if(step===5) setTimeout(()=>{step++;show()},3000);
}

function next(){step++;show()}
function back(){step--;show()}

function validate(el){
 const btn=el.closest(".step").querySelector(".next");
 if(!el.value){
  el.classList.add("error");
  btn.disabled=true;
  return;
 }
 el.classList.remove("error");
 btn.disabled=false;
 btn.classList.add("active");
}

function scanWiFi(){
 setStatus("Сканування Wi-Fi…",40);
 fetch("/scan").then(r=>r.json()).then(list=>{
  ssid.innerHTML='<option value=""></option>';
  list.forEach(w=>{
   let o=document.createElement("option");
   o.value=w.ssid;
   o.text=w.ssid+(w.open?" 🔓":" 🔒");
   o.dataset.secure=!w.open;
   ssid.add(o);
  });
  setStatus("Wi-Fi готово",55);
 });
}

function wifiChanged(){
 const s=ssid.selectedOptions[0];
 pass.parentElement.style.display =
  s && s.dataset.secure==="true" ? "block" : "none";
}

function togglePass(){
 pass.type = pass.type==="password" ? "text" : "password";
 setTimeout(()=>pass.type="password",2500);
}

function validateUserStep(){
 const stepEl = steps[1]; // шаг "Користувач"
 const name = stepEl.querySelector('input[name="name"]');
 const surname = stepEl.querySelector('input[name="surname"]');
 const btn = stepEl.querySelector(".next");

 let ok = true;

 if(!name.value){
  name.classList.add("error");
  ok = false;
 } else name.classList.remove("error");

 if(!surname.value){
  surname.classList.add("error");
  ok = false;
 } else surname.classList.remove("error");

 btn.disabled = !ok;
 btn.classList.toggle("active", ok);
}

</script>

</body>
</html>
)rawliteral";
}

void handleSave() {
    if (server.hasArg("country")) userCountry = server.arg("country");
    if (server.hasArg("name")) userName = server.arg("name");
    if (server.hasArg("surname")) userSurname = server.arg("surname");
    if (server.hasArg("ssid")) wifiSSID = server.arg("ssid");
    if (server.hasArg("pass")) wifiPASS = server.arg("pass");

    EEPROM.put(COUNTRY_ADDR, userCountry);
    EEPROM.put(NAME_ADDR, userName);
    EEPROM.put(SURNAME_ADDR, userSurname);
    EEPROM.put(WIFI_SSID_ADDR, wifiSSID);
    EEPROM.put(WIFI_PASS_ADDR, wifiPASS);
    EEPROM.commit();

    setConfigured();   // помечаем, что устройство настроено
    loadEEPROM();      // сразу обновляем переменные

    server.send(200, "text/html", "<h2>✅ Збережено! Перезавантаження...</h2>");
    delay(2000);
    ESP.restart();
}

// === Настройка веб-сервера ===
void setupWebServer() {

    // Главная страница
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/html", htmlPage());
    });

    // Сохранение данных с формы
    server.on("/save", HTTP_POST, handleSave);

    // Запуск последней программы
    server.on("/start_last", HTTP_GET, []() {
        if (programRunning) {
            server.send(200, "text/plain", "Program already running");
            return;
        }

        if (lastWasMP5) {
            startMP5();
            server.send(200, "text/plain", "MP5 started");
            return;
        }

        if (lastGreenTime > 0) {
            startProgram(lastGreenTime);
            server.send(200, "text/plain", "Last program started");
        } else {
            server.send(400, "text/plain", "No last program");
        }
    });

    // Остановка программы
    server.on("/stop", HTTP_GET, []() {
        if (!programRunning) {
            server.send(200, "text/plain", "Program not running");
            return;
        }

        stopProgram();
        server.send(200, "text/plain", "Program stopped");
    });

    // Переключение запуска/остановки программы
    server.on("/toggle", HTTP_GET, []() {
        if (programRunning) {
            stopProgram();
            server.send(200, "text/plain", "STOP");
        } else {
            if (lastWasMP5) {
                startMP5();
                server.send(200, "text/plain", "START MP5");
                return;
            }

            if (lastGreenTime > 0) {
                startProgram(lastGreenTime);
                server.send(200, "text/plain", "START LAST");
            } else {
                server.send(400, "text/plain", "NO LAST PROGRAM");
            }
        }
    });

    // Сканирование Wi-Fi
    server.on("/scan", HTTP_GET, []() {
        int n = WiFi.scanNetworks();
        String json = "[";
        for (int i = 0; i < n && i < 10; i++) {
            if (i) json += ",";
            json += "{";
            json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
            json += "\"open\":" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
            json += "}";
        }
        json += "]";
        server.send(200, "application/json", json);
    });

    // Запуск веб-сервера
    server.begin();
}

// =================================================
// ================= UTILS =========================
// =================================================
void tg(String msg) {
  if (!chatBound) return;
  if (!tgActive) return;   // 🔥 ВАЖНО
  bot.sendMessage(chatID, msg, "");
}

String rssiText() {
  int r = WiFi.RSSI();
  if (r > -60) return String(r) + " dBm 📶📶📶";
  if (r > -75) return String(r) + " dBm 📶📶";
  return String(r) + " dBm 📶";
}

void clearPin() {
  for (int i = 0; i < 4; i++) EEPROM.write(PIN_ADDR + i, 0);
  EEPROM.commit();

  storedPin = "";
  pinExists = false;
}

// =================================================
// ================= KEYBOARDS =====================
// =================================================
void pinKeyboard() {
  String kb =
    "[[\"1\",\"2\",\"3\"],"
    "[\"4\",\"5\",\"6\"],"
    "[\"7\",\"8\",\"9\"],"
    "[\"0\"]]";
  bot.sendMessageWithReplyKeyboard(chatID, "🔐 Введіть PIN", "", kb, true);
}

void mainKeyboard() {

String kb =
  "[[\"▶️ МП5\",\"▶️ МП8\"],"
  "[\"▶️ МП10\",\"▶️ Остання\"],"
  "[\"⏱ Час роботи\",\"📶 Wi-Fi\"],"
  "[\"🔄 Змінити PIN\",\"⛔ СТОП\"],"
  "[\"🔋 Батарея\",\"📋 Дані\"]]";


  bot.sendMessageWithReplyKeyboard(
    chatID,
    "📋 Меню",
    "",
    kb,
    true
  );
}

void mp8Keyboard() {
  bot.sendMessageWithReplyKeyboard(
    chatID,
    "▶️ МП8",
    "",
    "[[\"8 сек\",\"6 сек\",\"4 сек\"],[\"⬅️ Назад\"]]",
    true
  );
}

void mp10Keyboard() {
  bot.sendMessageWithReplyKeyboard(
    chatID,
    "▶️ МП10",
    "",
    "[[\"150 сек\",\"20 сек\",\"10 сек\"],[\"⬅️ Назад\"]]",
    true
  );
}

// =================================================
// ================= LOAD EEPROM ==================
// =================================================
void loadEEPROM() {
  userCountry = readEEPROMString(COUNTRY_ADDR, 32);
  userName    = readEEPROMString(NAME_ADDR, 32);
  userSurname = readEEPROMString(SURNAME_ADDR, 32);
  wifiSSID    = readEEPROMString(WIFI_SSID_ADDR, 32);
  wifiPASS    = readEEPROMString(WIFI_PASS_ADDR, 64);

  chatID = readEEPROMString(CHAT_ID_ADDR, 15);
  chatBound = (chatID.length() >= 5);

  Serial.println("[EEPROM] Loaded user data:");
  Serial.println("Country: " + userCountry);
  Serial.println("Name: " + userName);
  Serial.println("Surname: " + userSurname);
  Serial.println("WiFi SSID: " + wifiSSID);
}

void savePin(String pin) {
  for (int i = 0; i < 4; i++) EEPROM.write(PIN_ADDR + i, pin[i]);
  EEPROM.commit();
  storedPin = pin;
  pinExists = true;
}

// =================================================
// ================= Wi-Fi ========================
// =================================================
bool connectSTA() {

  Serial.println("[WiFi] Reset & connect...");

  WiFi.scanDelete();          // 🔥 ОЧЕНЬ ВАЖНО
  WiFi.disconnect(true, true);
  delay(500);

  WiFi.mode(WIFI_STA);
  delay(200);

  WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());

  unsigned long start = millis();

  while (millis() - start < 25000) {

    wl_status_t st = WiFi.status();

    if (st == WL_CONNECTED) {
      Serial.println("\n[WiFi] CONNECTED");
      Serial.print("[WiFi] IP: ");
      Serial.println(WiFi.localIP());
      return true;
    }

    Serial.print(".");
    delay(500);
    yield(); // Предотвращает сброс сторожевого таймера (WDT)
  }

  Serial.println("\n[WiFi] FAILED, status = " + String(WiFi.status()));
  return false;
}

// =================================================
// ================= PROGRAM ======================
// =================================================
void startProgram(unsigned long greenSec) {

lastGreenTime = greenSec;

EEPROM.put(LAST_GREEN_ADDR, lastGreenTime);
EEPROM.commit();

lastWasMP5 = false;

  if (programRunning) {
    tg("⚠️ Програма вже запущена");
    return;
  }
lastWasMP5 = false;   // ✅ это обычная программа

  mp5Mode = false;
  mpCycles = 0;
  mpCyclesDone = 0;

  greenTime = greenSec * 1000UL;
  lastGreenTime = greenSec;

finalRedStage = false;   // 🔴 сброс финального этапа

  programRunning = true;
  progState = PS_RED;
  stageStart = millis();

  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);

  tg("▶️ Старт\n🟢 " + String(greenSec) + " сек");
}

void stopProgram() {

  if (!programRunning) {
    tg("⚠️ Програма ще не запущена");
    return;
  }

  programRunning = false;
  progState = PS_IDLE;

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);

  Serial.println("[PROGRAM] STOP");
  tg("⛔ Програму зупинено");
}

void handleProgram() {

  if (!programRunning) return;

  unsigned long now = millis();

  // 🔴 RED
  if (progState == PS_RED) {

    if (now - stageStart >= RED_TIME) {

      // ===== MP5 =====
      if (mp5Mode) {

        // если уже было 5 зелёных — это финальный красный
        if (mp5GreenCount >= 5) {

          programRunning = false;
          progState = PS_IDLE;

          digitalWrite(RED_LED, LOW);
          digitalWrite(GREEN_LED, LOW);

          tg("✅ МП5 завершена");
          return;
        }

        // 🔴 → 🟢
        progState = PS_GREEN;
        stageStart = now;

        digitalWrite(RED_LED, LOW);
        digitalWrite(GREEN_LED, HIGH);
        return;
      }

      // ===== MP8 / MP10 =====
      if (finalRedStage) {

        programRunning = false;
        progState = PS_IDLE;
        finalRedStage = false;

        digitalWrite(RED_LED, LOW);
        digitalWrite(GREEN_LED, LOW);

        tg("✅ Програма завершена");
        return;
      }

      // 🔴 → 🟢
      progState = PS_GREEN;
      stageStart = now;

      digitalWrite(RED_LED, LOW);
      digitalWrite(GREEN_LED, HIGH);
    }
  }

  // 🟢 GREEN
  else if (progState == PS_GREEN) {

    unsigned long duration = mp5Mode ? 3000 : greenTime;

    if (now - stageStart >= duration) {

      // ===== MP5 =====
      if (mp5Mode) {

        mp5GreenCount++;   // ✅ считаем ТОЛЬКО зелёные

        // 🟢 → 🔴
        progState = PS_RED;
        stageStart = now;

        digitalWrite(GREEN_LED, LOW);
        digitalWrite(RED_LED, HIGH);
        return;
      }

      // ===== MP8 / MP10 =====
      finalRedStage = true;

      progState = PS_RED;
      stageStart = now;

      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, HIGH);
    }
  }
}

void startMP5() {

  if (programRunning) {
    tg("⚠️ Програма вже запущена");
    return;
  }

  mp5Mode = true;
  mp5GreenCount = 0;   // 🔥 СБРОС
  finalRedStage = false;

  lastWasMP5 = true;

  programRunning = true;
  progState = PS_RED;
  stageStart = millis();

  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);

  tg("▶️ Старт МП5");
}

void wifiListKeyboard() {

  Serial.println("[WIFI] Scan start");
  int n = WiFi.scanNetworks();

  String kb = "[";

  if (n <= 0) {
    kb += "[\"🔁 Повторити скан\"],";
  } else {
    for (int i = 0; i < n && i < 8; i++) {
      if (i) kb += ",";
      kb += "[\"" + WiFi.SSID(i) + "\"]";
      Serial.println("[WIFI] Found: " + WiFi.SSID(i));
    }
    kb += ",";
  }

  kb += "[\"🔁 Повторити скан\"],[\"⬅️ Назад\"]]";

  bot.sendMessageWithReplyKeyboard(
    chatID,
    "📡 Оберіть Wi-Fi мережу",
    "",
    kb,
    true
  );
}

void wifiPassKeyboard() {

String kb =
  "[[\"1\",\"2\",\"3\"],"
  "[\"4\",\"5\",\"6\"],"
  "[\"7\",\"8\",\"9\"],"
  "[\"0\",\"⌫\",\"OK\"],"
  "[\"⬅️ Назад\"]]";


  bot.sendMessageWithReplyKeyboard(
    chatID,
    "🔐 Введіть пароль Wi-Fi:\n" + wifiPassInput,
    "",
    kb,
    true
  );
}

// =================================================
// ================= TELEGRAM =====================
// =================================================
void handleTelegram() {

  if (millis() - tgTimer < 1200) return;
  tgTimer = millis();

  int n = bot.getUpdates(bot.last_message_received + 1);
  if (!n) return;

  Serial.println("[TG] Messages: " + String(n));

  for (int i = 0; i < n; i++) {

    String text = bot.messages[i].text;
    String incomingChat = bot.messages[i].chat_id;

    Serial.println("[TG] TEXT: " + text);

  // 🔐 ПЕРВАЯ ПРИВЯЗКА — ТОЛЬКО ПО /start
if (!chatBound) {

  if (text != "/start") {
    // ❌ игнорируем всё, кроме /start
    return;
  }

  saveChatID(incomingChat);
  tgActive = true;

  bot.sendMessage(
    chatID,
    "✅ Ви стали власником пристрою\n\n🔐 Введіть PIN",
    ""
  );

  authorized = false;
  enteredPin = "";
  pinKeyboard();
  return;
}


if (text == "/start") {

  // ❌ если это не владелец — просто игнор
  if (incomingChat != chatID) {
    bot.sendMessage(
      incomingChat,
      "⛔ Цей пристрій вже привʼязаний до іншого користувача",
      ""
    );
    return;
  }

  // ✅ владелец
  tgActive = true;

  bot.sendMessage(
    chatID,
    "🤖 Пристрій готовий до роботи",
    ""
  );

  if (!authorized) {
    pinKeyboard();
  } else {
    mainKeyboard();
  }

  return;
}


    // ================= PIN =================
 if (!authorized) {

    if (text.length() == 1 && isDigit(text[0])) {
        enteredPin += text;

        if (enteredPin.length() == 4) {

            if (!pinExists) {
                savePin(enteredPin);
                authorized = true;
                tg("✅ PIN збережено");
            }
            else if (enteredPin == storedPin) {
                authorized = true;
                tg("✅ Доступ дозволено");
            }
            else {
                tg("❌ Невірний PIN");
                pinKeyboard();
                enteredPin = "";
                return;
            }

            enteredPin = "";

            // ===== СЮДА добавляем проверку OTA сразу после PIN =====
            if (!otaChecked) {
                otaChecked = true;
                if (checkOTAUpdate()) {
                    otaAvailable = true;
                    otaKeyboard();
                    return; // показываем клавиатуру обновления
                }
            }

            // Если OTA нет — показываем главное меню
            mainKeyboard();
        }
    }
    continue;
}


    // ===== ДАЛЬШЕ — ТВОЯ ТЕКУЩАЯ ЛОГИКА МЕНЮ =====

if (otaAvailable) {

  if (text == "⬇️ Завантажити") {
    otaAvailable = false;
    startOTA();
    return;
  }

  if (text == "⏭ Пропустити") {
    otaAvailable = false;
    mainKeyboard();
    return;
  }
}

if (wifiSelectMode) {

  if (text == "🔁 Повторити скан") {
    WiFi.scanDelete();
    wifiListKeyboard();
    return;
  }

  if (text == "⬅️ Назад") {
    wifiSelectMode = false;
    mainKeyboard();
    return;
  }

 // 🔒 сохраняем текущий рабочий Wi-Fi
oldWifiSSID = wifiSSID;
oldWifiPASS = wifiPASS;

// 🔁 выбираем новый
wifiSSID = text;

  wifiPassInput = "";
  wifiSelectMode = false;
  wifiPassMode = true;

  Serial.println("[WIFI] Selected: " + wifiSSID);
  wifiPassKeyboard();
  return;
}

if (wifiPassMode) {

  // ⬅️ Назад
  if (text == "⬅️ Назад") {
    wifiPassMode = false;
    wifiSelectMode = true;
    wifiPassInput = "";
    wifiListKeyboard();
    return;
  }

  // ⌫ удалить символ
  if (text == "⌫" && wifiPassInput.length()) {
    wifiPassInput.remove(wifiPassInput.length() - 1);
    wifiPassKeyboard();
  }
  // OK — подключение
  else if (text == "OK") {

    wifiPASS = wifiPassInput;
    Serial.println("[WIFI] Try connect: " + wifiSSID);

  bot.sendMessage(
    chatID,
    "⏳ Підключення до Wi-Fi...\n📡 Мережа: " + wifiSSID,
    ""
  );

  if (connectSTA()) {

  // 💾 сохраняем успешный Wi-Fi
  EEPROM.put(WIFI_SSID_ADDR, wifiSSID);
  EEPROM.put(WIFI_PASS_ADDR, wifiPASS);
  EEPROM.commit();

  tg("✅ Wi-Fi підключено\n📡 Мережа: " + wifiSSID);

  wifiPassMode = false;
  mainKeyboard();
}

 else {

  // 🔙 возвращаем старый Wi-Fi
  wifiSSID = oldWifiSSID;
  wifiPASS = oldWifiPASS;

  WiFi.disconnect(true);
  delay(500);
  WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());

  tg("❌ Не вдалося підключитися\n📡 Залишаємося в мережі: " + wifiSSID);

  wifiPassInput = "";
  wifiPassKeyboard();
}

  }
  // ввод цифр
  else if (text.length() == 1) {
    wifiPassInput += text;
    wifiPassKeyboard();
  }

  return;
}


else if (text == "📶 Wi-Fi") {

  wifiSelectMode = true;

  // 🔄 очистка предыдущего скана
  WiFi.scanDelete();

  // 📡 сообщение в Telegram
  bot.sendMessage(
    chatID,
    "📡 Сканування Wi-Fi мереж…",
    ""
  );

  // небольшая пауза для UX
  delay(600);

  // 📋 показать список сетей
  wifiListKeyboard();
}


else if (text == "⏱ Час роботи") {

  unsigned long s = totalUptimeSec;

  unsigned long days  = s / 86400;
  unsigned long hours = (s % 86400) / 3600;
  unsigned long mins  = (s % 3600) / 60;
  unsigned long secs  = s % 60;

  String msg = "⏱ Час роботи пристрою:\n";
  msg += "📅 Днів: " + String(days) + "\n";
  msg += "🕒 Годин: " + String(hours) + "\n";
  msg += "🕑 Хвилин: " + String(mins) + "\n";
  msg += "⏲ Секунд: " + String(secs);

  bot.sendMessage(chatID, msg, "");
  Serial.println(msg);
}

// 2. ВІДОБРАЖЕННЯ РІВНЯ ЗАРЯДУ АКУМУЛЯТОРА
else if (text == "🔋 Батарея") {
  float voltage = readBatteryVoltage();
  int percent = batteryPercent();

  String icon = "🔋";
  if (percent <= 20) icon = "🪫";

  String msg = icon + " Стан батареї:\n\n";
  msg += "⚡ Напруга: " + String(voltage, 2) + " В\n";
  msg += "📊 Рівень заряду: " + String(percent) + "%";

  bot.sendMessage(chatID, msg, "");
}

// ===== FACTORY RESET =====
if (text == "FACTORY_RESET") {
  factoryReset();
  return;
}

// ===== RESET UPTIME =====
if (text == "RESET_UPTIME") {
  resetUptime();
  return;
}

else if (text == "📋 Дані") {
    showSavedData();
    return;
}

    // ---- MENU ----
if (text == "▶️ МП5") startMP5();
    else if (text == "▶️ МП8") mp8Keyboard();
    else if (text == "▶️ МП10") mp10Keyboard();
else if (text == "▶️ Остання") {

  if (lastWasMP5) {
    startMP5();
    return;
  }

  if (lastGreenTime == 0) {
    tg("⚠️ Немає попередньої програми");
    return;
  }

  startProgram(lastGreenTime);
   }

    else if (text == "8 сек") startProgram(8);
    else if (text == "6 сек") startProgram(6);
    else if (text == "4 сек") startProgram(4);

    else if (text == "150 сек") startProgram(150);
    else if (text == "20 сек") startProgram(20);
    else if (text == "10 сек") startProgram(10);

    else if (text == "📊 Час роботи") {
      tg("📊 Час роботи: " + String(totalUptimeSec) + " сек");
    }
   else if (text == "🔄 Змінити PIN") {

  clearPin();                // ❗ стереть старый PIN
  authorized = false;
  enteredPin = "";

  tg("🔐 Введіть новий PIN");
  pinKeyboard();
}

    else if (text == "⛔ СТОП") stopProgram();
    else if (text == "⬅️ Назад") mainKeyboard();
  }
}

// 4. ФІЗИЧНА КНОПКА (BUTTON_PIN = 19) З DEBOUNCE
void handlePhysicalButton() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    static bool buttonState = HIGH;
    if (reading != buttonState) {
      buttonState = reading;

      // Кнопка нажата (LOW из-за INPUT_PULLUP)
      if (buttonState == LOW) {
        Serial.println("[BUTTON] Press detected!");

        if (programRunning) {
          stopProgram();
        } else {
          if (lastWasMP5) {
            startMP5();
          } else if (lastGreenTime > 0) {
            startProgram(lastGreenTime);
          }
        }
      }
    }
  }

  lastButtonState = reading;
}

// =================================================
// ================= SETUP ========================
// =================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("===== ESP32 START =====");

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  // 4. ИНИЦИАЛИЗАЦИЯ ФИЗИЧЕСКОЙ КНОПКИ
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // 6. НАСТРОЙКА ADC ДЛЯ ТОЧНОГО ИЗМЕРЕНИЯ БАТАРЕИ
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db); // Для диапазонов 0 - 3.3В

  // ---------- EEPROM ----------
  EEPROM.begin(EEPROM_SIZE);
  loadEEPROM();

// 🔐 если устройство не настроено — PIN не существует
if (!isConfigured()) {
  clearPin();
  pinExists = false;
  authorized = false;
}

  // ---------- Telegram ----------
  client.setInsecure();

  // ---------- ПЕРШИЙ ЗАПУСК ----------
  if (!isConfigured()) {
    Serial.println("[SETUP] First start → AP MODE");
    startAPMode();        // Wi-Fi AP + Web
    return;               // ❗ далі не йдемо
  }

if (wifiSSID.length() < 1 || wifiPASS.length() < 1) {
    Serial.println("[WiFi] Нет сохранённых данных → AP MODE");
    startAPMode();
    return;
}

  // ---------- ЗАВАНТАЖЕННЯ Wi-Fi ----------
  EEPROM.get(WIFI_SSID_ADDR, wifiSSID);
  EEPROM.get(WIFI_PASS_ADDR, wifiPASS);

  Serial.println("[SETUP] Wi-Fi SSID: " + wifiSSID);

if (connectSTA()) {
  setupWebServer();
}
else {
  Serial.println("[SETUP] Wi-Fi failed → AP MODE");
  startAPMode();        // fallback
}

}  // ✅ ОЦЕ ЗАКРИВАЄ setup()

// =================================================
// ================= LOOP =========================
// =================================================
void loop() {

  server.handleClient();   // ← ВСЕГДА

  // ===== РЕЖИМ НАЛАШТУВАННЯ (AP + WEB) =====
  if (configMode) {
    server.handleClient();
    yield();
    return;
  }

  // ===== TELEGRAM + ПРОГРАМИ + КНОПКА =====
  handleTelegram();
  handleProgram();
  handlePhysicalButton(); // 4. Опрос физической кнопки каждую итерацию

  // ===== Wi-Fi АВТОПЕРЕПІДКЛЮЧЕННЯ =====
  if (!programRunning &&
    WiFi.status() != WL_CONNECTED &&
    !wifiSelectMode &&
    !wifiPassMode) {

    if (millis() - wifiReconnectTimer > 15000) {
      wifiReconnectTimer = millis();
      Serial.println("[WIFI] Reconnect...");
      connectSTA();
    }
  }

  // ===== UPTIME (РАХУЄМО СЕКУНДИ) =====
  if (millis() - lastSecondTick >= 1000) {
    lastSecondTick = millis();
    totalUptimeSec++;
  }

  // ===== UPTIME (ЗБЕРЕЖЕННЯ В EEPROM) =====
  if (millis() - lastUptimeSave >= 60000) {   // 1 раз в 60 сек
    lastUptimeSave = millis();
    EEPROM.put(UPTIME_ADDR, totalUptimeSec);
    EEPROM.commit();
    Serial.println("[UPTIME] Saved: " + String(totalUptimeSec));
  }

  yield(); // Кормим системный сторожевой таймер
}