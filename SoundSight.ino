#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "driver/i2s.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Visual_Alarm_System_v1_inferencing.h>


// ==================== HARDWARE ====================
const int LED_pin = 18;
const int num     = 75;
Adafruit_NeoPixel strip(num, LED_pin, NEO_GRB + NEO_KHZ800);


// ==================== WiFi AP ====================
const char* AP_SSID = "SoundSight";
const char* AP_PASS = "soundsight";
IPAddress     apIP(192, 168, 4, 1);
WebServer server(80);


// ==================== ALARM STATE ====================
struct {
 bool alarmActive = false;
 String lastLabel = "None";
 float confidence = 0.0f;
 String history[10];
 int    histIndex = 0;
} state;


bool alerts[3] = {false, false, false};
const float CONFIDENCE_THRESHOLD = 0.8;
const float GLASS_THRESHOLD      = 0.65;

uint32_t alarmColors[3];


// ==================== EDGE IMPULSE ====================
signal_t audio_signal;


// ==================== AUDIO INFERENCE STRUCTS ====================
typedef struct {
   int16_t *buffer;
   uint8_t buf_ready;
   uint32_t buf_count;
   uint32_t n_samples;
} inference_t;


static inference_t inference;
static const uint32_t sample_buffer_size = 2048;
static signed short sampleBuffer[sample_buffer_size];
static bool record_status = true;


// ==================== FUNCTION PROTOTYPES ====================
static void capture_samples(void* arg);
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr);
static bool microphone_inference_start(uint32_t n_samples);
static bool microphone_inference_record(void);
static int i2s_init(uint32_t sampling_rate);
static void audio_inference_callback(uint32_t n_bytes);
void classifySound();
void switchLight(int color);
void deactivateAlarm();
void waitWithServer(unsigned long ms);
String colorToHex(uint32_t c);
uint32_t parseHexColor(String s);


// ==================== DASHBOARD HTML ====================
const char index_html[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Sound Sight</title>
<style>
 :root{ --bg:#0f172a; --card:#1e293b; --text:#e2e8f0; --muted:#94a3b8;
        --accent:#38bdf8; --danger:#ef4444; --success:#22c55e; }
 *{box-sizing:border-box; margin:0; padding:0;}
 body{ background:var(--bg); color:var(--text); font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
       min-height:100vh; display:flex; align-items:center; justify-content:center; padding:20px; }
 .card{ background:var(--card); border-radius:16px; padding:28px; width:100%; max-width:480px; box-shadow:0 20px 50px rgba(0,0,0,0.4); position:relative; }
 h1{ font-size:1.6rem; margin-bottom:6px; display:flex; align-items:center; gap:10px; }
 .settings-btn{ margin-left:auto; cursor:pointer; font-size:1.3rem; opacity:0.6; transition:opacity 0.2s; user-select:none; }
 .settings-btn:hover{ opacity:1; }
 .status{ display:inline-block; width:12px; height:12px; border-radius:50%; background:var(--success); }
 .status.alarm{ background:var(--danger); animation:pulse 1s infinite; }
 @keyframes pulse{ 0%{opacity:1} 50%{opacity:0.4} 100%{opacity:1} }
 .subtitle{ color:var(--muted); font-size:0.95rem; margin-bottom:22px; }
 .row{ display:flex; gap:12px; margin-bottom:14px; }
 .box{ flex:1; background:rgba(255,255,255,0.04); border-radius:10px; padding:14px; text-align:center; }
 .box-label{ font-size:0.75rem; color:var(--muted); text-transform:uppercase; letter-spacing:0.05em; margin-bottom:6px; }
 .box-value{ font-size:1.3rem; font-weight:700; }
 .alarm-box{ background:rgba(239,68,68,0.1); border:1px solid rgba(239,68,68,0.25); }
 .btn{ width:100%; padding:14px; border:none; border-radius:10px; font-size:1rem; font-weight:600; cursor:pointer;
       color:#fff; background:linear-gradient(135deg,#3b82f6,#8b5cf6); margin-top:10px; }
 .btn:active{ transform:scale(0.98); }
 .btn-danger{ background:linear-gradient(135deg,#ef4444,#f97316); }
 .btn:disabled{ opacity:0.5; cursor:not-allowed; }
 .history{ margin-top:18px; max-height:160px; overflow-y:auto; }
 .history-item{ font-size:0.85rem; padding:8px 0; border-bottom:1px solid rgba(255,255,255,0.06); color:var(--muted); }
 .footer{ margin-top:18px; font-size:0.8rem; color:var(--muted); text-align:center; }
 .hidden{ display:none !important; }
 .modal{ position:fixed; inset:0; background:rgba(0,0,0,0.7); display:flex; align-items:center; justify-content:center; z-index:100; padding:20px; }
 .modal-content{ background:var(--card); border-radius:16px; padding:24px; width:100%; max-width:360px; box-shadow:0 20px 50px rgba(0,0,0,0.5); position:relative; }
 .modal-close{ position:absolute; top:12px; right:16px; font-size:1.4rem; cursor:pointer; color:var(--muted); line-height:1; }
 .modal-close:hover{ color:var(--text); }
 .modal-content h2{ margin-bottom:16px; font-size:1.2rem; padding-right:24px; }
 .color-row{ display:flex; align-items:center; justify-content:space-between; margin:16px 0; }
 .color-row label{ color:var(--text); font-size:0.95rem; font-weight:500; }
 .color-row input[type="color"]{ width:52px; height:40px; border:none; border-radius:8px; cursor:pointer; background:none; padding:0; }
 .modal-actions{ display:flex; gap:10px; margin-top:20px; }
 .modal-actions .btn{ flex:1; margin-top:0; }
 .modal-actions .btn-secondary{ background:#475569; }
 .toast{ position:fixed; bottom:20px; left:50%; transform:translateX(-50%); background:var(--success); color:#fff; padding:10px 20px; border-radius:8px; font-weight:600; opacity:0; transition:opacity 0.3s; pointer-events:none; z-index:200; }
 .toast.show{ opacity:1; }
</style>
</head>
<body>


<div class="card">
 <h1>
   <span id="statusDot" class="status"></span>
   Sound Sight
   <span class="settings-btn" onclick="openSettings()" title="Alarm Colors">⚙️</span>
 </h1>
 <div class="subtitle">Monitoring for important sounds...</div>


 <div class="row">
   <div class="box">
     <div class="box-label">Status</div>
     <div class="box-value" id="alarmState" style="color:var(--success)">Idle</div>
   </div>
 </div>


 <div id="alarmBanner" class="box alarm-box hidden">
   <div class="box-label" style="color:#fca5a5">Active Alert</div>
   <div class="box-value" id="alarmLabel" style="color:#ef4444">—</div>
   <div style="font-size:0.85rem;color:#fca5a5;margin-top:4px" id="alarmConf">Confidence: —</div>
 </div>


 <button class="btn btn-danger" id="resetBtn" onclick="resetAlarm()" disabled>Disable Alarm</button>


 <div class="history" id="historyBox">
   <div class="box-label" style="text-align:left;margin-bottom:8px">Recent Detections</div>
   <div id="historyList"></div>
 </div>


 <div class="footer">Auto-refreshing every 2s • WiFi: SoundSight-Setup</div>
</div>


<div id="settingsModal" class="modal hidden" onclick="if(event.target===this) closeSettings()">
 <div class="modal-content">
   <span class="modal-close" onclick="closeSettings()">✕</span>
   <h2>Alarm Colors</h2>
   <div class="color-row">
     <label>Smoke Alarm</label>
     <input type="color" id="colorSmoke" value="#ff0000">
   </div>
   <div class="color-row">
     <label>Glass Crashing</label>
     <input type="color" id="colorGlass" value="#00ff00">
   </div>
   <div class="color-row">
     <label>Baby Crying</label>
     <input type="color" id="colorBaby" value="#0000ff">
   </div>
   <div class="modal-actions">
     <button class="btn" onclick="saveColors()">Save</button>
     <button class="btn btn-secondary" onclick="closeSettings()">Cancel</button>
   </div>
 </div>
</div>


<div id="toast" class="toast">Saved</div>


<script>
 function openSettings(){
   document.getElementById('settingsModal').classList.remove('hidden');
 }
 function closeSettings(){
   document.getElementById('settingsModal').classList.add('hidden');
 }
 function showToast(msg){
   const t = document.getElementById('toast');
   t.textContent = msg;
   t.classList.add('show');
   setTimeout(() => t.classList.remove('show'), 1500);
 }


 async function fetchStatus(){
   try{
     const r = await fetch('/api/status');
     const d = await r.json();
     const dot = document.getElementById('statusDot');
     const stateTxt = document.getElementById('alarmState');
     const banner = document.getElementById('alarmBanner');
     const label = document.getElementById('alarmLabel');
     const conf = document.getElementById('alarmConf');
     const btn = document.getElementById('resetBtn');


     if(d.alarm_active){
       dot.classList.add('alarm');
       stateTxt.textContent = 'ALERT';
       stateTxt.style.color = '#ef4444';
       banner.classList.remove('hidden');
       label.textContent = d.last_label.toUpperCase();
       conf.textContent = 'Confidence: ' + (d.confidence * 100).toFixed(1) + '%';
       btn.disabled = false;
     } else {
       dot.classList.remove('alarm');
       stateTxt.textContent = 'Idle';
       stateTxt.style.color = '#22c55e';
       banner.classList.add('hidden');
       btn.disabled = true;
     }
   }catch(e){ console.error(e); }
 }


 async function fetchHistory(){
   try{
     const r = await fetch('/api/history');
     const d = await r.json();
     const box = document.getElementById('historyList');
     if(d.history.length === 0){ box.innerHTML = '<div class="history-item">No detections yet</div>'; return; }
     box.innerHTML = d.history.map(h => `<div class="history-item">${h}</div>`).join('');
   }catch(e){}
 }


 async function resetAlarm(){
   try{
     await fetch('/api/alarm/off', {method:'POST'});
     fetchStatus();
   }catch(e){ alert('Failed to reset alarm'); }
 }


 async function fetchColors(){
   try{
     const r = await fetch('/api/colors');
     const d = await r.json();
     document.getElementById('colorSmoke').value = d.smoke;
     document.getElementById('colorGlass').value = d.glass;
     document.getElementById('colorBaby').value  = d.baby;
   }catch(e){ console.error(e); }
 }


 async function saveColors(){
   const smoke = document.getElementById('colorSmoke').value;
   const glass = document.getElementById('colorGlass').value;
   const baby  = document.getElementById('colorBaby').value;
   closeSettings();
   showToast('Saving...');
   try{
     await fetch('/api/colors?smoke='+encodeURIComponent(smoke)+'&glass='+encodeURIComponent(glass)+'&baby='+encodeURIComponent(baby), {method:'POST'});
     showToast('Saved');
   }catch(e){
     showToast('Save failed');
     console.error(e);
   }
 }


 fetchStatus();
 fetchHistory();
 fetchColors();
 setInterval(fetchStatus, 2000);
 setInterval(fetchHistory, 5000);
</script>
</body>
</html>
)HTML";


// ==================== SETUP ====================
void setup() {
 Serial.begin(115200);
 delay(500);
 Serial.println("\n=== SOUND SIGHT ===");


 strip.begin();
 strip.show();


 alarmColors[0] = strip.Color(255, 0, 0);
 alarmColors[1] = strip.Color(0, 255, 0);
 alarmColors[2] = strip.Color(0, 0, 255);


 WiFi.mode(WIFI_AP);
 WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
 WiFi.softAP(AP_SSID, AP_PASS);
 Serial.println("WiFi AP started:");
 Serial.print("  SSID: "); Serial.println(AP_SSID);
 Serial.print("  IP:   "); Serial.println(WiFi.softAPIP());


 server.on("/", HTTP_GET, []() {
   server.send(200, "text/html", index_html);
 });
 server.on("/api/status", HTTP_GET, []() {
   String json = "{";
   json += "\"alarm_active\":" + String(state.alarmActive ? "true" : "false") + ",";
   json += "\"last_label\":\"" + state.lastLabel + "\",";
   json += "\"confidence\":" + String(state.confidence, 3) + "}";
   server.send(200, "application/json", json);
 });
 server.on("/api/history", HTTP_GET, []() {
   String json = "{\"history\":[";
   bool first = true;
   for (int i = 0; i < 10; i++) {
     int idx = (state.histIndex + i) % 10;
     if (state.history[idx].length() > 0) {
       if (!first) json += ",";
       json += "\"" + state.history[idx] + "\"";
       first = false;
     }
   }
   json += "]}";
   server.send(200, "application/json", json);
 });
 server.on("/api/alarm/off", HTTP_POST, []() {
   deactivateAlarm();
   Serial.println("[ALARM] Disabled via web");
   server.send(200, "application/json", "{\"ok\":true}");
 });
 server.on("/api/colors", HTTP_GET, []() {
   String json = "{";
   json += "\"smoke\":\"" + colorToHex(alarmColors[0]) + "\",";
   json += "\"glass\":\"" + colorToHex(alarmColors[1]) + "\",";
   json += "\"baby\":\"" + colorToHex(alarmColors[2]) + "\"}";
   server.send(200, "application/json", json);
 });
 server.on("/api/colors", HTTP_POST, []() {
   if (server.hasArg("smoke")) alarmColors[0] = parseHexColor(server.arg("smoke"));
   if (server.hasArg("glass")) alarmColors[1] = parseHexColor(server.arg("glass"));
   if (server.hasArg("baby"))  alarmColors[2] = parseHexColor(server.arg("baby"));
   Serial.println("[COLORS] Updated via web");
   server.send(200, "application/json", "{\"ok\":true}");
 });
 server.begin();
 Serial.println("[READY] Dashboard at http://192.168.4.1");


 audio_signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
 audio_signal.get_data = &microphone_audio_signal_get_data;
 microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT);
}


// ==================== LOOP ====================
void loop() {
 server.handleClient();
 classifySound();
 server.handleClient();


 if (state.alarmActive) {
   for (int i = 0; i < 3; i++) {
     server.handleClient();
     if (!state.alarmActive) break;
     if (alerts[i]) {
       switchLight(i);
       waitWithServer(500);
       switchLight(3);
       waitWithServer(500);
     }
   }
 }
}


// ==================== ALARM LOGIC ====================
void classifySound() {
   microphone_inference_record();


   ei_impulse_result_t result = {};
   EI_IMPULSE_ERROR err = run_classifier(&audio_signal, &result, false);


   if (err != EI_IMPULSE_OK) {
       Serial.println("Inference failed");
       return;
   }


   // ── DEBUG: print index map so you can verify label order ──
   Serial.println("--- INDEX MAP ---");
   for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
       Serial.printf("  [%d] %s = %.3f\n", i, result.classification[i].label, result.classification[i].value);
   }
   Serial.println("-----------------");


   // ── READ BY INDEX (update these if the map above shows different order) ──
   float baby  = result.classification[0].value;
   float glass = result.classification[2].value;
   float smoke = result.classification[3].value;

 
   bool anyNewAlert = false;


   if (smoke > CONFIDENCE_THRESHOLD) {
       if (!alerts[0]) anyNewAlert = true;
       alerts[0] = true;
       state.alarmActive = true;
       state.lastLabel   = "smoke alarm";
       state.confidence  = smoke;
       Serial.printf("[ALERT SET] idx=0 label=smoke alarm val=%.3f\n", smoke);
   }


   if (glass > GLASS_THRESHOLD) {
       if (!alerts[1]) anyNewAlert = true;
       alerts[1] = true;
       state.alarmActive = true;
       state.lastLabel   = "Glass Crashing";
       state.confidence  = glass;
       Serial.printf("[ALERT SET] idx=1 label=Glass Crashing val=%.3f\n", glass);
   }


   if (baby > CONFIDENCE_THRESHOLD) {
       if (!alerts[2]) anyNewAlert = true;
       alerts[2] = true;
       state.alarmActive = true;
       state.lastLabel   = "Baby Crying";
       state.confidence  = baby;
       Serial.printf("[ALERT SET] idx=2 label=Baby Crying val=%.3f\n", baby);
   }


   if (anyNewAlert) {
       state.history[state.histIndex] = String(state.lastLabel) + " @ " + String(state.confidence * 100, 1) + "%";
       state.histIndex = (state.histIndex + 1) % 10;
       Serial.printf("[DETECT] %s (%.1f%%)\n", state.lastLabel.c_str(), state.confidence * 100);
   }
}


// ==================== LEDS ====================
void switchLight(int color) {
   uint32_t rgb = strip.Color(0, 0, 0);
   if (color >= 0 && color <= 2) rgb = alarmColors[color];
   for (int i = 0; i < num; i++) strip.setPixelColor(i, rgb);
   strip.show();
   Serial.printf("[LED] color=%d\n", color);
}


void deactivateAlarm() {
 state.alarmActive = false;
 state.lastLabel   = "None";
 state.confidence  = 0.0f;
 for (int i = 0; i < 3; i++) alerts[i] = false;
 switchLight(3);
}


void waitWithServer(unsigned long ms) {
 unsigned long start = millis();
 while (millis() - start < ms) {
   server.handleClient();
   delay(10);
 }
}


// ==================== COLOR HELPERS ====================
String colorToHex(uint32_t c) {
 uint8_t r = (c >> 16) & 0xFF;
 uint8_t g = (c >> 8) & 0xFF;
 uint8_t b = c & 0xFF;
 const char* hex = "0123456789ABCDEF";
 String s = "#";
 s += hex[r >> 4]; s += hex[r & 0x0F];
 s += hex[g >> 4]; s += hex[g & 0x0F];
 s += hex[b >> 4]; s += hex[b & 0x0F];
 return s;
}


uint32_t parseHexColor(String s) {
 if (s.startsWith("#")) s = s.substring(1);
 long val = strtol(s.c_str(), NULL, 16);
 uint8_t r = (val >> 16) & 0xFF;
 uint8_t g = (val >> 8) & 0xFF;
 uint8_t b = val & 0xFF;
 return strip.Color(r, g, b);
}


// ==================== AUDIO PIPELINE ====================
static bool microphone_inference_start(uint32_t n_samples) {
   inference.buffer = (int16_t *)malloc(n_samples * sizeof(int16_t));
   if(inference.buffer == NULL) return false;
   inference.buf_count = 0;
   inference.n_samples = n_samples;
   inference.buf_ready = 0;
   if (i2s_init(EI_CLASSIFIER_FREQUENCY)) ei_printf("Failed to start I2S!");
   ei_sleep(100);
   record_status = true;
   xTaskCreate(capture_samples, "CaptureSamples", 1024 * 32, (void*)sample_buffer_size, 10, NULL);
   return true;
}


static bool microphone_inference_record(void) {
   while (inference.buf_ready == 0) {
       delay(10);
       server.handleClient();
   }
   inference.buf_ready = 0;
   return true;
}


static int i2s_init(uint32_t sampling_rate) {
   i2s_config_t i2s_config = {
       .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
       .sample_rate = sampling_rate,
       .bits_per_sample = (i2s_bits_per_sample_t)16,
       .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
       .communication_format = I2S_COMM_FORMAT_I2S,
       .intr_alloc_flags = 0,
       .dma_buf_count = 8,
       .dma_buf_len = 512,
       .use_apll = false,
       .tx_desc_auto_clear = false,
       .fixed_mclk = -1,
   };
   i2s_pin_config_t pin_config = {
       .bck_io_num = 26,
       .ws_io_num = 32,
       .data_out_num = -1,
       .data_in_num = 33,
   };
   esp_err_t ret = 0;
   ret = i2s_driver_install((i2s_port_t)1, &i2s_config, 0, NULL);
   if (ret != ESP_OK) ei_printf("Error in i2s_driver_install");
   ret = i2s_set_pin((i2s_port_t)1, &pin_config);
   if (ret != ESP_OK) ei_printf("Error in i2s_set_pin");
   ret = i2s_zero_dma_buffer((i2s_port_t)1);
   if (ret != ESP_OK) ei_printf("Error in initializing dma buffer with 0");
   return int(ret);
}


static void capture_samples(void* arg) {
   const int32_t i2s_bytes_to_read = (uint32_t)arg;
   size_t bytes_read = i2s_bytes_to_read;
   while (record_status) {
       i2s_read((i2s_port_t)1, (void*)sampleBuffer, i2s_bytes_to_read, &bytes_read, 100);
       if (bytes_read <= 0) {
           ei_printf("Error in I2S read : %d", bytes_read);
       } else {
           if (bytes_read < i2s_bytes_to_read) ei_printf("Partial I2S read");
           for (int x = 0; x < i2s_bytes_to_read/2; x++) {
               int32_t scaled = (int32_t)sampleBuffer[x] * 8;
               if (scaled > 32767) scaled = 32767;
               if (scaled < -32768) scaled = -32768;
               sampleBuffer[x] = (int16_t)scaled;
           }
           if (record_status) audio_inference_callback(i2s_bytes_to_read);
           else break;
       }
   }
   vTaskDelete(NULL);
}


static void audio_inference_callback(uint32_t n_bytes) {
   for(int i = 0; i < n_bytes>>1; i++) {
       inference.buffer[inference.buf_count++] = sampleBuffer[i];
       if(inference.buf_count >= inference.n_samples) {
           inference.buf_count = 0;
           inference.buf_ready = 1;
       }
   }
}


static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
   numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);
   return 0;
}
