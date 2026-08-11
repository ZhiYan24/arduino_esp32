#include <Adafruit_NeoPixel.h>
#include "driver/i2s.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Visual_Alarm_System_inferencing.h>

const int BTN_pin = 27;

const int LED_pin = 18;
const int num = 75;

Adafruit_NeoPixel strip(num, LED_pin, NEO_GRB + NEO_KHZ800);

signal_t audio_signal;

const float CONFIDENCE_THRESHOLD = 0.8;

// function prototypes
static void capture_samples(void* arg);
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr);

void setup() {
  // put your setup code here, to run once:
  pinMode(BTN_pin, INPUT_PULLUP);

  strip.begin();
  strip.show();

  Serial.begin(115200);

  audio_signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
  audio_signal.get_data = &microphone_audio_signal_get_data;

  microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT);
 }

bool alerts[3] = {false, false, false};

bool activate = false;

void loop() {
  // put your main code here, to run repeatedly:
  classifySound();

  if (activate == true) {
    // for each alert triggered, flash
    for (int i = 0; i < 3; i++) {
      if (alerts[i] == true) {
        switchLight(i);
        delay(500);
        switchLight(3); // off
        delay(500);
        if (checkDeactivation() == true) {
          break;
        }
      }
    }
  }
}

void classifySound() {
  microphone_inference_record();

  ei_impulse_result_t result = {};

  EI_IMPULSE_ERROR err = run_classifier(&audio_signal, &result, false);

  if (err != EI_IMPULSE_OK) {
    Serial.println("inference failed");
    return;
  }

  float results[3] = {result.classification[3].value, result.classification[2].value, result.classification[0].value}; // smoke, glass, baby

  for (int i = 0; i < 3; i++) {
    if (results[i] > CONFIDENCE_THRESHOLD) {
      alerts[i] = true;
      activate = true;
    }
  }
}

void switchLight(int color) {
  uint32_t rgb;

  if (color == 0) {
    rgb = strip.Color(255, 0, 0);
  }
  else if (color == 1) {
    rgb = strip.Color(0, 255, 0);
  }
  else if (color == 2) {
    rgb = strip.Color(0, 0, 255);
  }
  else if (color == 3) {
    rgb = strip.Color(0, 0, 0);
  }

  for (int i = 0; i < num; i++) {
    strip.setPixelColor(i, rgb);
  }

  strip.show();
}

bool checkDeactivation() {
  if(digitalRead(BTN_pin)== LOW) {
    activate = false;
    for (int i = 0; i < 3; i++) {
      alerts[i] = false;
    }
    return true;
  }
  return false;
}



// pasted from example

/** Audio buffers, pointers and selectors */
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

/**
 * @brief      Init inferencing struct and setup/start PDM
 *
 * @param[in]  n_samples  The n samples
 *
 * @return     { description_of_the_return_value }
 */
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

/**
 * @brief      Wait on new data
 *
 * @return     True when finished
 */
static bool microphone_inference_record(void)
{
    bool ret = true;

    while (inference.buf_ready == 0) {
        delay(10);
    }

    inference.buf_ready = 0;
    return ret;
}

static int i2s_init(uint32_t sampling_rate) {
  // Start listening for audio: MONO @ 8/16KHz
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

    /* read data at once from i2s */
    i2s_read((i2s_port_t)1, (void*)sampleBuffer, i2s_bytes_to_read, &bytes_read, 100);

    if (bytes_read <= 0) {
      ei_printf("Error in I2S read : %d", bytes_read);
    }
    else {
        if (bytes_read < i2s_bytes_to_read) {
        ei_printf("Partial I2S read");
        }

        // scale the data (otherwise the sound is too quiet)
        for (int x = 0; x < i2s_bytes_to_read/2; x++) {
            sampleBuffer[x] = (int16_t)(sampleBuffer[x]) * 8;
        }

        if (record_status) {
            audio_inference_callback(i2s_bytes_to_read);
        }
        else {
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