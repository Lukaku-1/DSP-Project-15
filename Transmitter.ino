#include <Keypad.h>
#include <driver/i2s.h>
#include <math.h>

// -----------------------------------------
// I2S (MAX98357A) Configuration
// -----------------------------------------
#define I2S_NUM         I2S_NUM_0
#define I2S_BCLK        26
#define I2S_LRC         25
#define I2S_DOUT        22

const int SAMPLE_RATE = 16000; 
const int TONE_DURATION_MS = 200; 
const float VOLUME = 0.5; 

// -----------------------------------------
// Keypad Configuration (4x4)
// -----------------------------------------
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {19, 18, 5, 17};

// FIXED: Swapped the pin order for the columns to match your keypad's behavior
// Old order: {16, 4, 2, 15}
// New order: {2, 4, 16, 15}
byte colPins[COLS] = {2, 4, 16, 15}; 

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// DTMF Frequencies (Hz)
const float rowFreqs[ROWS] = {697.0, 770.0, 852.0, 941.0};
const float colFreqs[COLS] = {1209.0, 1336.0, 1477.0, 1633.0};

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 DTMF Transmitter starting...");

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, 
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM, &pin_config);
  
  i2s_zero_dma_buffer(I2S_NUM);
  
  Serial.println("Ready. Press a key on the keypad.");
}

void loop() {
  char key = keypad.getKey();

  if (key) {
    Serial.print("Key Pressed: ");
    Serial.println(key);
    playDTMF(key, TONE_DURATION_MS);
  }
}

void playDTMF(char key, int duration_ms) {
  int row_idx = -1;
  int col_idx = -1;

  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      if (keys[r][c] == key) {
        row_idx = r;
        col_idx = c;
        break;
      }
    }
  }

  if (row_idx == -1 || col_idx == -1) return;

  float f_low = rowFreqs[row_idx];
  float f_high = colFreqs[col_idx];

  uint32_t total_samples = (SAMPLE_RATE * duration_ms) / 1000;
  
  int16_t sample_buffer[256];
  size_t bytes_written;
  uint32_t samples_generated = 0;
  
  while (samples_generated < total_samples) {
    uint32_t remaining = total_samples - samples_generated;
    uint32_t batch_size = (remaining > 256) ? 256 : remaining;
    
    for (uint32_t i = 0; i < batch_size; i++) {
      float t = (float)(samples_generated + i) / SAMPLE_RATE;
      
      // FIXED: Restored the missing * symbols for multiplication
      float signal = sin(2.0 * PI * f_low * t) + sin(2.0 * PI * f_high * t);
      
      // FIXED: Restored the missing * symbol for volume scaling
      signal = (signal / 2.0) * 32767.0 * VOLUME;
      
      sample_buffer[i] = (int16_t)signal;
    }
    
    i2s_write(I2S_NUM, sample_buffer, batch_size * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    samples_generated += batch_size;
  }
  
  memset(sample_buffer, 0, sizeof(sample_buffer));
  for(int i = 0; i < 10; i++) { 
     i2s_write(I2S_NUM, sample_buffer, sizeof(sample_buffer), &bytes_written, portMAX_DELAY);
  }
}
