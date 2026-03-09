/* Edge Impulse Arduino examples
 * Copyright (c) 2022 EdgeImpulse Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define EIDSP_QUANTIZE_FILTERBANK 0

#include <aedes_wingsbeat_inferencing.h>
#include <I2S.h>

#define SAMPLE_RATE 16000U
#define SAMPLE_BITS 16
#define CONFIDENCE_THRESHOLD 0.80  // Minimum confidence to trigger LED

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
static bool debug_nn = false;
static bool record_status = true;

/**
 * @brief Blink the built-in LED a specified number of times
 * @param times   Number of blinks
 * @param on_ms   LED on duration in ms
 * @param off_ms  LED off duration in ms
 */
void blinkLED(int times, int on_ms = 200, int off_ms = 200) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_BUILTIN, LOW);   // LOW = ON for XIAO ESP32-S3
    delay(on_ms);
    digitalWrite(LED_BUILTIN, HIGH);  // HIGH = OFF for XIAO ESP32-S3
    delay(off_ms);
  }
}

/**
 * @brief Arduino setup function
 */
void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("Edge Impulse Inferencing Demo - Aedes Mosquito Detector");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // Turn off LED initially

  I2S.setAllPins(-1, 42, 41, -1, -1);
  if (!I2S.begin(PDM_MONO_MODE, SAMPLE_RATE, SAMPLE_BITS)) {
    Serial.println("Failed to initialize I2S!");
    while (1);
  }

  // Summary of inferencing settings
  ei_printf("Inferencing settings:\n");
  ei_printf("\tInterval: ");
  ei_printf_float((float)EI_CLASSIFIER_INTERVAL_MS);
  ei_printf(" ms.\n");
  ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
  ei_printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
  ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) / sizeof(ei_classifier_inferencing_categories[0]));

  ei_printf("\nLED Guide:\n");
  ei_printf("  1 blink  = Aedes Aegypti detected\n");
  ei_printf("  2 blinks = Aedes Albopictus detected\n");
  ei_printf("  No blink = Noise / Other Species / Low confidence\n");

  ei_printf("\nStarting continuous inference in 2 seconds...\n");
  ei_sleep(2000);

  if (microphone_inference_start(EI_CLASSIFIER_RAW_SAMPLE_COUNT) == false) {
    ei_printf("ERR: Could not allocate audio buffer (size %d)\r\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT);
    return;
  }

  ei_printf("Recording...\n");
}

/**
 * @brief Arduino main function
 */
void loop() {
  bool m = microphone_inference_record();
  if (!m) {
    ei_printf("ERR: Failed to record audio...\n");
    return;
  }

  signal_t signal;
  signal.total_length = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
  signal.get_data = &microphone_audio_signal_get_data;
  ei_impulse_result_t result = { 0 };

  EI_IMPULSE_ERROR r = run_classifier(&signal, &result, debug_nn);
  if (r != EI_IMPULSE_OK) {
    ei_printf("ERR: Failed to run classifier (%d)\n", r);
    return;
  }

  // Find highest confidence prediction
  int pred_index = 0;
  float pred_value = 0;

  ei_printf("Predictions (DSP: %d ms., Classification: %d ms.):\n",
            result.timing.dsp, result.timing.classification);

  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    ei_printf("    %s: ", result.classification[ix].label);
    ei_printf_float(result.classification[ix].value);
    ei_printf("\n");

    if (result.classification[ix].value > pred_value) {
      pred_index = ix;
      pred_value = result.classification[ix].value;
    }
  }

  // -------------------------------------------------------
  // LED Blink Logic:
  //   pred_index 0 = Aedes Aegypti    → blink ONCE
  //   pred_index 1 = Aedes Albopictus → blink TWICE
  //   pred_index 2 = Noise            → no blink
  //   pred_index 3 = Other Species    → no blink, serial only
  // -------------------------------------------------------
  if (pred_value >= CONFIDENCE_THRESHOLD) {
    if (pred_index == 0) {
      Serial.println(">> Aedes Aegypti detected! (1 blink)");
      blinkLED(1);
    } 
    else if (pred_index == 1) {
      Serial.println(">> Aedes Albopictus detected! (2 blinks)");
      blinkLED(2);
    } 
    else if (pred_index == 2) {
      Serial.println(">> Noise detected. No blink.");
      digitalWrite(LED_BUILTIN, HIGH);
    }
    else if (pred_index == 3) {
      Serial.println(">> Other species detected. No blink.");
      digitalWrite(LED_BUILTIN, HIGH);
    }
  } else {
    Serial.print(">> Low confidence (");
    Serial.print(pred_value);
    Serial.println("). No action.");
    digitalWrite(LED_BUILTIN, HIGH);
  }

#if EI_CLASSIFIER_HAS_ANOMALY == 1
  ei_printf("    anomaly score: ");
  ei_printf_float(result.anomaly);
  ei_printf("\n");
#endif
}

static void audio_inference_callback(uint32_t n_bytes) {
  for (int i = 0; i < n_bytes >> 1; i++) {
    inference.buffer[inference.buf_count++] = sampleBuffer[i];

    if (inference.buf_count >= inference.n_samples) {
      inference.buf_count = 0;
      inference.buf_ready = 1;
    }
  }
}

static void capture_samples(void *arg) {
  const int32_t i2s_bytes_to_read = (uint32_t)arg;
  size_t bytes_read = i2s_bytes_to_read;

  while (record_status) {
    esp_i2s::i2s_read(esp_i2s::I2S_NUM_0, (void *)sampleBuffer, i2s_bytes_to_read, &bytes_read, 100);

    if (bytes_read <= 0) {
      ei_printf("Error in I2S read : %d", bytes_read);
    } else {
      if (bytes_read < i2s_bytes_to_read) {
        ei_printf("Partial I2S read");
      }

      for (int x = 0; x < i2s_bytes_to_read / 2; x++) {
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

static bool microphone_inference_start(uint32_t n_samples) {
  inference.buffer = (int16_t *)malloc(n_samples * sizeof(int16_t));

  if (inference.buffer == NULL) {
    return false;
  }

  inference.buf_count = 0;
  inference.n_samples = n_samples;
  inference.buf_ready = 0;

  ei_sleep(100);
  record_status = true;

  xTaskCreate(capture_samples, "CaptureSamples", 1024 * 32, (void *)sample_buffer_size, 10, NULL);

  return true;
}

static bool microphone_inference_record(void) {
  while (inference.buf_ready == 0) {
    delay(10);
  }
  inference.buf_ready = 0;
  return true;
}

static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
  numpy::int16_to_float(&inference.buffer[offset], out_ptr, length);
  return 0;
}

static void microphone_inference_end(void) {
  free(sampleBuffer);
  ei_free(inference.buffer);
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif