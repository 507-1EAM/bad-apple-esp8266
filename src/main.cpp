#include <Wire.h>
#include <LittleFS.h>

#define OLED_ADDR 0x3C // Some modules use 0x3D
#define WIDTH 128
#define HEIGHT 64
#define FRAME_SIZE 1024

uint8_t frameBuffer[1024]; 
File videoFile;
uint32_t totalFrames = 0;
uint32_t frameIndex = 0;

// Function to send a command to the OLED
void sendCommand(uint8_t cmd) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00); // Co = 0, D/C# = 0 (Command)
  Wire.write(cmd);
  Wire.endTransmission();
}

// Initialize SSD1306 from scratch (no libraries)
void initOLED() {
  Wire.setClock(400000); // Boost I2C speed to 400kHz (Fast Mode)
  Wire.begin(4, 5);      // SDA = D2 (GPIO4), SCL = D1 (GPIO5)
  delay(100);

  sendCommand(0xAE); // Display OFF
  sendCommand(0xD5); // Set OSC Frequency
  sendCommand(0x80);
  sendCommand(0xA8); // Set Multiplex Ratio
  sendCommand(0x3F); // 1/64 duty
  sendCommand(0xD3); // Set Display Offset
  sendCommand(0x00);
  sendCommand(0x40); // Set Start Line (0x00)
  sendCommand(0x8D); // Charge Pump Setting
  sendCommand(0x14); // Enable Charge Pump (CRITICAL for OLED!)
  sendCommand(0x20); // Set Memory Addressing Mode
  sendCommand(0x00); // Horizontal Addressing Mode
  sendCommand(0xA1); // Segment Remap
  sendCommand(0xC8); // COM Output Scan Direction
  sendCommand(0xDA); // Set COM Pins Hardware Configuration
  sendCommand(0x12);
  sendCommand(0x81); // Set Contrast
  sendCommand(0xCF);
  sendCommand(0xD9); // Set Pre-Charge Period
  sendCommand(0xF1);
  sendCommand(0xDB); // Set VCOMH Deselect Level
  sendCommand(0x40);
  sendCommand(0xA4); // Entire Display ON
  sendCommand(0xA6); // Normal Display
  sendCommand(0xAF); // Display ON
}

// Push the 1024-byte buffer to the OLED
void drawFrame() {
  // Set column address (0-127) and page address (0-7)
  sendCommand(0x21); sendCommand(0); sendCommand(127); 
  sendCommand(0x22); sendCommand(0); sendCommand(7);   

  // Send data (ESP8266 I2C buffer is 128 bytes, so we send in chunks of 32)
  for (int i = 0; i < 1024; i += 32) {
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x40); // Co = 0, D/C# = 1 (Data)
    for (int j = 0; j < 32; j++) {
      Wire.write(frameBuffer[i + j]);
    }
    Wire.endTransmission();
  }
}

// Read and decompress RLE frame directly from LittleFS
void readAndDecompressFrame() {
  int outIdx = 0;
  while (outIdx < FRAME_SIZE && videoFile.available()) {
    uint8_t control = videoFile.read();
    if (control & 0x80) {
      // Run-length
      uint8_t count = control & 0x7F;
      uint8_t val = videoFile.read();
      for (int i = 0; i < count; i++) frameBuffer[outIdx++] = val;
    } else {
      // Unique bytes
      uint8_t count = control;
      for (int i = 0; i < count; i++) frameBuffer[outIdx++] = videoFile.read();
    }
  }
}

void setup() {
  Serial.begin(115200);
  initOLED();
  
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }
  
  videoFile = LittleFS.open("/badapple.bin", "r");
  if (!videoFile) {
    Serial.println("File not found! Did you upload the FS?");
    return;
  }
  
  char magic[4];
  videoFile.readBytes(magic, 4);
  if (strncmp(magic, "BAPP", 4) != 0) {
    Serial.println("Invalid file format");
    return;
  }
  
  videoFile.readBytes((char*)&totalFrames, 4);
  Serial.printf("Total frames loaded: %d\n", totalFrames);
  frameIndex = 0;
}

void loop() {
  if (frameIndex >= totalFrames) {
    frameIndex = 0; // Loop the video
  }

  uint32_t offset;
  // Read frame offset from the index (index starts at byte 8)
  videoFile.seek(8 + (frameIndex * 4)); 
  videoFile.readBytes((char*)&offset, 4);
  
  // Jump to the actual frame data
  uint32_t dataStart = 8 + (totalFrames * 4);
  videoFile.seek(dataStart + offset);
  
  uint32_t startTime = millis();
  readAndDecompressFrame();
  drawFrame();
  
  frameIndex++;
  
  // Delay to maintain ~15 FPS (66ms), minus draw time
  uint32_t elapsed = millis() - startTime;
  if (elapsed < 66) {
    delay(66 - elapsed);
  }
}
