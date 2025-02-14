#include <Arduino.h>
#include <Wire.h>
#include <rom/rtc.h>
#include "wristband-tft.hpp"
#include "wristband-ota.hpp"
#include "clock.hpp"
#include "pages.hpp"
#include "mpu.hpp"
#include "bt.hpp"
#include "BluetoothSerial.h"
#include "Sparkfun_DRV2605L.h"
#include "battery.hpp"
#include "esp_log.h"
#include "esp_spiffs.h"

int16_t *accelbuff;
int16_t *gyrobuff;

FILE *f;

static const char *LOG_TAG = "JumBLE";

SFE_HMD_DRV2605L drv;
bool vibrate = true;
bool drv_initialised = false;
extern bool vibe_request;
void scanI2Cdevice(void)
{
    uint8_t err, addr;
    int nDevices = 0;
    for (addr = 1; addr < 127; addr++) {
        Wire1.beginTransmission(addr);
        err = Wire1.endTransmission();
        if (err == 0) {
            SerialBT.print("I2C device found at address 0x");
            if (addr < 16)
                SerialBT.print("0");
            SerialBT.print(addr, HEX);
            SerialBT.println(" !");
            nDevices++;
        } else if (err == 4) {
            SerialBT.print("Unknown error at address 0x");
            if (addr < 16)
                SerialBT.print("0");
            SerialBT.println(addr, HEX);
        }
    }
    if (nDevices == 0)
        SerialBT.println("No I2C devices found\n");
    else
        SerialBT.println("Done\n");
}

void setupSpiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
    .base_path = "/spiffs",
    .partition_label = NULL,
    .max_files = 5,
    .format_if_mount_failed = true
  };

  esp_err_t ret = esp_vfs_spiffs_register(&conf);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(LOG_TAG, "Failed to mount or format filesystem");
    } else if (ret == ESP_ERR_NOT_FOUND) {
      ESP_LOGE(LOG_TAG, "Failed to find SPIFFS partition");
    } else {
      ESP_LOGE(LOG_TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    }
    return;
  }

  size_t total = 0, used = 0;
  ret = esp_spiffs_info(conf.partition_label, &total, &used);
  if (ret != ESP_OK) {
    ESP_LOGE(LOG_TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    // esp_spiffs_format(conf.partition_label);
  } else {
    ESP_LOGI(LOG_TAG, "Partition size: total: %d, used: %d", total, used);
  }
}

void setup() {

  Serial.begin(115200);
  Serial.printf("Begin setup\n");
  ESP_LOGI(LOG_TAG, "Begin setup");
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  Wire1.begin(DRV_SDA_PIN, DRV_SCL_PIN);
  Wire1.setClock(400000);
  initClock();
  
  bt_init();
  tftInit();
  // deactivateWifi();
  setupADC();
#ifndef IMU_SKIP
  initMPU();
#else
  mpuDeepSleep();
#endif
  initButton();
  setupBattery();

  // enable drv2605
  digitalWrite(14, HIGH); // Enable high

  scanI2Cdevice();
  setupSpiffs();

  f = fopen("/spiffs/jumble_imu.bin", "wb+");
  if (!f)
    Serial.printf("Failed to open spiffs\n");

  Serial.printf("Finish setup\n");
}

void loop() 
{
  // enable drv2605
  digitalWrite(14, HIGH); // Enable high

  accelbuff=getAccel();
  gyrobuff=getGyro();

  if (f)
    fwrite(accelbuff, sizeof(accelbuff), 1, f);

  if (SerialBT.connected())
  {
    SerialBT.printf("%6.6d, %6.6d, %6.6d, %6.6d, %6.6d, %6.6d\n\r", accelbuff[0], accelbuff[1],accelbuff[2], 
      gyrobuff[0], gyrobuff[1],gyrobuff[2]);
  }

//  Serial.printf("%6.6d, %6.6d, %6.6d, %6.6d, %6.6d, %6.6d\n\r", accelbuff[0], accelbuff[1],accelbuff[2], 
//      gyrobuff[0], gyrobuff[1],gyrobuff[2]);

  handleUi();
  updateBatteryChargeStatus();
  if (! isCharging());
    bt_loop();

  if (sqrt(accelbuff[0]*accelbuff[0]+ accelbuff[1]*accelbuff[1]+accelbuff[2]*accelbuff[2])>17500)
    vibrate = true;

  if (getVibeReq()== true)
  {
    vibrate = true;
  }

  if (vibrate == true)
  {
    // swa rfc - Tidy up the digital writes above this, once we know there's no need for a delay after EN.
    digitalWrite(14, HIGH); // Enable high
    drv.begin();
    drv.MotorSelect(0x0A);
    drv.Library(7); //change to 6 for LRA motors 

    Serial.printf("Vibrate start:\n");
    drv.Mode(0); // This takes the device out of sleep mode

    for (int i = 0; i < 1; i++)
    {
      drv.Waveform(1, 16);  
      drv.Waveform(2, 0);  
      drv.Waveform(3, 16);  
      drv.Waveform(4, 0);  
      drv.Waveform(1, 16);  
      drv.Waveform(2, 0);  
      drv.Waveform(3, 16);  
      drv.Waveform(4, 0);  
      drv.Waveform(1, 16);  
      drv.Waveform(2, 0);  
      drv.Waveform(3, 16);  
      drv.Waveform(4, 0);  
      drv.go();
    }
    vibrate = false;
    Serial.printf("Vibrate finish\n");
  }
}
