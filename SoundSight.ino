#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "driver/i2s.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Visual_Alarm_System_inferencing.h>


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
const float GEN_CONFIDENCE_THRESHOLD = 0.8;
const float SPCL_CONFIDENCE_THRESHOLD = 0.6;


// ==================== EDGE IMPULSE ====================
signal_t audio_signal;


// ==================== FUNCTION PROTOTYPES ====================
static bool microphone_inference_start(uint32_t n_samples);
static bool microphone_inference_record(void);
static int i2s_init(uint32_t sampling_rate);
static void capture_samples(void* arg);
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr);
static void audio_inference_callback(uint32_t n_bytes);
void classifySound();
void switchLight(int color);
void deactivateAlarm();
void waitWithServer(unsigned long ms);


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
 .card{ background:var(--card); border-radius:16px; padding:28px; width:100%; max-width:480px; box-shadow:0 20px 50px rgba(0,0,0,0.4); }
 h1{ font-size:1.6rem; margin-bottom:6px; display:flex; align-items:center; gap:10px; }
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
 .hidden{ display:none; }
</style>
</head>
<body>
<div class="card">
 <h1><span id="statusDot" class="status"></span> Sound Sight</h1>
 <div class="subtitle">Monitoring for important sounds...</div>


 <div class="row">
   <div class="box">
     <div class="box-label">Status</div>
     <div class="box-value" id="alarmState" style="color:var(--success)">Idle</div>
   </div>
   <div class="box">
     <div class="box-label">Last Detection</div>
     <div class="box-value" id="lastDetect" style="color:var(--muted)">—</div>
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


<script>
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
     const last = document.getElementById('lastDetect');


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


     last.textContent = d.last_label !== 'None' ? d.last_label : '—';
     if(d.last_label === 'smoke') last.style.color = '#ef4444';
     else if(d.last_label === 'glass') last.style.color = '#22c55e';
     else if(d.last_label === 'baby') last.style.color = '#38bdf8';
     else last.style.color = '#94a3b8';


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


 fetchStatus();
 fetchHistory();
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


 // Init LEDs
 strip.begin();
 strip.show();


 // Start WiFi AP
 WiFi.mode(WIFI_AP);
 WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
 WiFi.softAP(AP_SSID, AP_PASS);
 Serial.println("WiFi AP started:");
 Serial.print("  SSID: "); Serial.println(AP_SSID);
 Serial.print("  IP:   "); Serial.println(WiFi.softAPIP());


 // Web server routes
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
 server.begin();
 Serial.println("[READY] Dashboard at http://192.168.4.1");


 // Init Edge Impulse audio pipeline
 audio_signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
 audio_signal.get_data = &microphone_audio_signal_get_data;
 microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT);
}


// ==================== LOOP ====================
void loop() {
 server.handleClient();
 classifySound();
 server.handleClient();

 // LED alarm sequence (non-blocking server handling inside)
 if (state.alarmActive) {
   for (int i = 0; i < 3; i++) {
     if (!state.alarmActive) break;

     if (alerts[i]) {
       switchLight(i);          // on
       waitWithServer(500);
       switchLight(3);          // off
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

   // ── PRINT ALL CLASSES ──
   Serial.print("[");
   for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
       Serial.print(result.classification[i].label);
       Serial.print(": ");
       Serial.print(result.classification[i].value, 3);
       if (i < EI_CLASSIFIER_LABEL_COUNT - 1) Serial.print(" | ");
   }
   Serial.println("]");

   // ── CHECK FOR ALERTS ──
   bool anyNewAlert = false;

   for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
       String label = result.classification[i].label;
       float  value = result.classification[i].value;

       // Skip background
       if (label.equalsIgnoreCase("Background")) {
           continue;
       }

       int alertIdx = -1;

       // Case-insensitive matching for labels
       if (value > GEN_CONFIDENCE_THRESHOLD && label.equalsIgnoreCase("smoke alarm")) {
         alertIdx = 0;  // red
       } else if (value > SPCL_CONFIDENCE_THRESHOLD && label.equalsIgnoreCase("Glass Crashing")) {
         alertIdx = 1;  // green
       } else if (value > GEN_CONFIDENCE_THRESHOLD && label.equalsIgnoreCase("Baby Crying"))  {
         alertIdx = 2;  // blue
       }

       if (alertIdx >= 0) {
           if (!alerts[alertIdx]) anyNewAlert = true;
           alerts[alertIdx] = true;
           state.alarmActive = true;
           state.lastLabel   = label;
           state.confidence  = value;
           Serial.printf("[ALERT SET] idx=%d label=%s val=%.3f\n", alertIdx, label.c_str(), value);
       }
   }

   if (anyNewAlert) {
       state.history[state.histIndex] = String(state.lastLabel) + " @ " + String(state.confidence * 100, 1) + "%";
       state.histIndex = (state.histIndex + 1) % 10;
       Serial.printf("[DETECT] %s (%.1f%%)\n", state.lastLabel.c_str(), state.confidence * 100);
   }
}


// ==================== LEDS ====================
void switchLight(int color) {
   uint32_t rgb = strip.Color(0, 0, 0);  // default off

   if (color == 0) {
       rgb = strip.Color(255, 0, 0);      // red    = smoke alarm
   } else if (color == 1) {
       rgb = strip.Color(0, 255, 0);      // green  = glass crashing
   } else if (color == 2) {
       rgb = strip.Color(0, 0, 255);      // blue   = baby crying
   } else if (color == 3) {
       rgb = strip.Color(0, 0, 0);        // off
   }

   for (int i = 0; i < num; i++) {
       strip.setPixelColor(i, rgb);
   }
   strip.show();

   // Debug so you know the LED command fired
   Serial.printf("[LED] color=%d  RGB=%06X\n", color, (unsigned int)rgb);
}


void deactivateAlarm() {
 state.alarmActive = false;
 state.lastLabel   = "None";
 state.confidence  = 0.0f;
 for (int i = 0; i < 3; i++) {
   alerts[i] = false;
 }
 switchLight(3); // all off
}


void waitWithServer(unsigned long ms) {
 unsigned long start = millis();
 while (millis() - start < ms) {
   server.handleClient();
   delay(10);
 }
}




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


// ==================== AUDIO FUNCTIONS ====================
static bool microphone_inference_start(uint32_t n_samples)
{
   inference.buffer = (int16_t *)malloc(n_samples * sizeof(int16_t));
   if(inference.buffer == NULL) {
       return false;
   }

   inference.buf_count  = 0;
   inference.n_samples  = n_samples;
   inference.buf_ready  = 0;

   if (i2s_init(EI_CLASSIFIER_FREQUENCY)) {
       ei_printf("Failed to start I2S!");
   }

   ei_sleep(100);
   record_status = true;

   xTaskCreate(capture_samples, "CaptureSamples", 1024 * 32, (void*)sample_buffer_size, 10, NULL);
   return true;
}


static bool microphone_inference_record(void)
{
   while (inference.buf_ready == 0) {
       delay(10);
       server.handleClient();  // keep web responsive while waiting for audio buffer
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
       .bck_io_num = 26,    // IIS_SCLK
       .ws_io_num = 32,     // IIS_LCLK
       .data_out_num = -1,  // IIS_DSIN
       .data_in_num = 33,   // IIS_DOUT
   };
   esp_err_t ret = 0;

   ret = i2s_driver_install((i2s_port_t)1, &i2s_config, 0, NULL);
   if (ret != ESP_OK) {
       ei_printf("Error in i2s_driver_install");
   }

   ret = i2s_set_pin((i2s_port_t)1, &pin_config);
   if (ret != ESP_OK) {
       ei_printf("Error in i2s_set_pin");
   }

   ret = i2s_zero_dma_buffer((i2s_port_t)1);
   if (ret != ESP_OK) {
       ei_printf("Error in initializing dma buffer with 0");
   }

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
           if (bytes_read < i2s_bytes_to_read) {
               ei_printf("Partial I2S read");
           }

           // scale the data (otherwise the sound is too quiet)
           for (int x = 0; x < i2s_bytes_to_read/2; x++) {
               sampleBuffer[x] = (int16_t)(sampleBuffer[x]) * 8;
           }

           if (record_status) {
               audio_inference_callback(i2s_bytes_to_read);
           } else {
               break;
           }
       }
   }
   vTaskDelete(NULL);
}


static void audio_inference_callback(uint32_t n_bytes)
{
   for(int i = 0; i < n_bytes>>1; i++) {
       inference.buffer[inference.buf_count++] = sampleBuffer[i];
       if(inference.buf_count >= inference.n_samples) {
           inference.buf_count = 0;
           inference.buf_ready = 1;
       }
   }
}


static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
   numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);
   return 0;
}
