#include <driver/i2s.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// -----------------------------------------
// OLED Configuration (Swapped Pins)
// -----------------------------------------
#define I2C_SDA 22
#define I2C_SCL 21

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -----------------------------------------
// I2S (INMP441 Microphone) Configuration
// -----------------------------------------
#define I2S_NUM         I2S_NUM_0

// PINS ON THE RIGHT SIDE OF THE ESP32
#define I2S_SD          19  // DIN (Data)
#define I2S_WS          18  // LRC (Word Select)
#define I2S_SCK         5   // BCLK (Serial Clock)

const int SAMPLE_RATE = 16000;
const int BLOCK_SIZE = 640; 
const float THRESHOLD = 500.0; // High threshold to ignore background noise

// -----------------------------------------
// DTMF Configuration
// -----------------------------------------
const float rowFreqs[4] = {697.0, 770.0, 852.0, 941.0};
const float colFreqs[4] = {1209.0, 1336.0, 1477.0, 1633.0};

char keys[4][4] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

char last_key = 0; 
int32_t i2s_raw_buffer[BLOCK_SIZE];
float audio_buffer[BLOCK_SIZE];

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 DTMF Receiver starting...");

  // Force I2C to use the swapped pins
  Wire.begin(I2C_SDA, I2C_SCL);

  // Initialize OLED (Address 0x3C is standard for 0.96" I2C OLEDs)
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println("OLED allocation failed");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(10, 25);
    display.println("Listening for DTMF...");
    display.display();
  }

  // Setup I2S for Microphone
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, 
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = BLOCK_SIZE,
    .use_apll = false
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM, &pin_config);
  
  Serial.println("Ready.");
}

float runGoertzel(float target_freq) {
  int k = (int)(0.5 + ((BLOCK_SIZE * target_freq) / SAMPLE_RATE));
  
  float omega = (2.0 * PI * k) / BLOCK_SIZE;
  float cosine = cos(omega);
  float coeff = 2.0 * cosine;
  
  float q0 = 0, q1 = 0, q2 = 0;

  for (int i = 0; i < BLOCK_SIZE; i++) {
    q0 = coeff * q1 - q2 + audio_buffer[i];
    q2 = q1;
    q1 = q0;
  }
  
  return sqrt(q1 * q1 + q2 * q2 - q1 * q2 * coeff);
}

void loop() {
  size_t bytes_read;
  
  i2s_read(I2S_NUM, &i2s_raw_buffer, BLOCK_SIZE * sizeof(int32_t), &bytes_read, portMAX_DELAY);
  
  for (int i = 0; i < BLOCK_SIZE; i++) {
    audio_buffer[i] = (float)(i2s_raw_buffer[i] >> 14) / 1000.0; 
  }

  float max_row_mag = 0;
  int max_row_idx = -1;
  float max_col_mag = 0;
  int max_col_idx = -1;

  for (int i = 0; i < 4; i++) {
    float mag = runGoertzel(rowFreqs[i]);
    if (mag > max_row_mag) {
      max_row_mag = mag;
      max_row_idx = i;
    }
  }

  for (int i = 0; i < 4; i++) {
    float mag = runGoertzel(colFreqs[i]);
    if (mag > max_col_mag) {
      max_col_mag = mag;
      max_col_idx = i;
    }
  }

  // If the sound crosses the threshold, register a key
  if (max_row_mag > THRESHOLD && max_col_mag > THRESHOLD) {
    char detected_key = keys[max_row_idx][max_col_idx];
    
    // Only print if it's a new key press
    if (detected_key != last_key) {
      Serial.print("Key Detected: ");
      Serial.println(detected_key);
      last_key = detected_key; 
      
      // Update OLED Display
      display.clearDisplay();
      
      // Print "Detected:" label
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.print("DTMF Detected:");
      
      // Print the character large in the center
      display.setTextSize(5);
      display.setCursor(50, 20);
      display.print(detected_key);
      
      display.display();
    }
  } else {
    // Reset when silence or background noise is detected
    last_key = 0; 
  }
}
