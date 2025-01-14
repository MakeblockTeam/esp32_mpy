//include from sensor.c
#include <stdbool.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_camera.h"
#include "omv_sensor.h"

static const char *TAG = "esp32_sensors";

//extern esp_err_t camera_subpart_reinit(camera_config_t *config);
//extern esp_err_t  camera_subpart_deinit(void);
 #define camera_subpart_deinit esp_camera_deinit 
 #define camera_subpart_reinit esp_camera_init 
#define BOARD_MATATALAB
// WROVER-KIT PIN Map
#if defined BOARD_WROVER_KIT

#define CAM_PIN_PWDN -1  //power down is not used
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 21
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 19
#define CAM_PIN_D2 18
#define CAM_PIN_D1 5
#define CAM_PIN_D0 4
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

// ESP32Cam (AiThinker) PIN Map
#elif defined BOARD_ESP32CAM_AITHINKER

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

// ESP32Cam (AiThinker) PIN Map
#elif defined BOARD_MATATALAB

#define CAM_PIN_PWDN -1
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 40
#define CAM_PIN_SIOD 34
#define CAM_PIN_SIOC 33

#define CAM_PIN_D7 39
#define CAM_PIN_D6 41
#define CAM_PIN_D5 42
#define CAM_PIN_D4 12
#define CAM_PIN_D3 3
#define CAM_PIN_D2 14
#define CAM_PIN_D1 47
#define CAM_PIN_D0 13
#define CAM_PIN_VSYNC 21
#define CAM_PIN_HREF 38
#define CAM_PIN_PCLK 11
#else
#error "At least one Camera board defined!"
#endif


static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_GRAYSCALE, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 1       //if more than one, i2s runs in continuous mode. Use only with JPEG
};

#if CAM_PIN_PWDN == -1
#define DCMI_PWDN_HIGH()  gpio_set_level(CAM_PIN_PWDN, 1)
#define DCMI_PWDN_LOW()  gpio_set_level(CAM_PIN_PWDN, 0)
#else
#define DCMI_PWDN_HIGH()  
#define DCMI_PWDN_LOW()  
#endif

static int omv_reset(omv_sensor_t *sensor)
{
  sensor_t *esp_sensor = esp_camera_sensor_get();
  return esp_sensor->reset(esp_sensor);
}

#define PFOMV2ESP32(x)  case OMV_##x: pixformat = x;break;
#define OMV_PIXFORMAT_RAW   OMV_PIXFORMAT_BAYER
static int omv_set_pixformat(omv_sensor_t *sensor, omv_pixformat_t _pixformat)
{
  sensor_t *esp_sensor = esp_camera_sensor_get();
  pixformat_t pixformat;
  
  switch(_pixformat) {
    PFOMV2ESP32(PIXFORMAT_RGB565)
    PFOMV2ESP32(PIXFORMAT_YUV422)
    PFOMV2ESP32(PIXFORMAT_JPEG)
    PFOMV2ESP32(PIXFORMAT_RAW)
    PFOMV2ESP32(PIXFORMAT_GRAYSCALE)
    default: ESP_LOGE(TAG, "invalid pixel format 0x%X\n", _pixformat); return -1;
  }
  camera_config.pixel_format = pixformat;
//  if(pixformat == PIXFORMAT_JPEG)
//    camera_config.fb_count = 2;
  return esp_sensor->set_pixformat(esp_sensor, pixformat);
}

#define FSOMV2ESP32(x)  case OMV_##x: framesize = x;break;
static int omv_set_framesize(omv_sensor_t *sensor, omv_framesize_t _framesize)
{
  sensor_t *esp_sensor = esp_camera_sensor_get();
  framesize_t framesize;
  
  switch(_framesize) {
    FSOMV2ESP32(FRAMESIZE_QQVGA)
    FSOMV2ESP32(FRAMESIZE_QCIF)
    FSOMV2ESP32(FRAMESIZE_HQVGA)
    FSOMV2ESP32(FRAMESIZE_QVGA)
    FSOMV2ESP32(FRAMESIZE_CIF)
    FSOMV2ESP32(FRAMESIZE_VGA)
    FSOMV2ESP32(FRAMESIZE_SVGA)
    FSOMV2ESP32(FRAMESIZE_XGA)
    FSOMV2ESP32(FRAMESIZE_HD)
    FSOMV2ESP32(FRAMESIZE_SXGA)
    FSOMV2ESP32(FRAMESIZE_UXGA)
    default: ESP_LOGE(TAG, "invalid framesize 0x%X\n", _framesize); return -1;
  }
  camera_config.frame_size = framesize;
  return esp_sensor->set_framesize(esp_sensor, framesize);
}

static int omv_set_contrast(omv_sensor_t *sensor, int level)
{
  sensor_t *esp_sensor = esp_camera_sensor_get();
  return esp_sensor->set_contrast(esp_sensor, level);
}

static int omv_set_brightness(omv_sensor_t *sensor, int level)
{
  sensor_t *esp_sensor = esp_camera_sensor_get();
  return esp_sensor->set_brightness(esp_sensor, level);
}

static int omv_set_saturation(omv_sensor_t *sensor, int level)
{
  sensor_t *esp_sensor = esp_camera_sensor_get();
  return esp_sensor->set_saturation(esp_sensor, level);
}

static int omv_set_gainceiling(omv_sensor_t *sensor, gainceiling_t gainceiling)
{
  sensor_t *esp_sensor = esp_camera_sensor_get();
  return esp_sensor->set_gainceiling(esp_sensor, gainceiling);
}

static int omv_set_quality(omv_sensor_t *sensor, int quality)
{
  sensor_t *esp_sensor = esp_camera_sensor_get();
  return esp_sensor->set_quality(esp_sensor, quality);
}

static int omv_set_colorbar(omv_sensor_t *sensor, int enable)
{
  sensor_t *esp_sensor = esp_camera_sensor_get();
  return esp_sensor->set_colorbar(esp_sensor, enable);
}

static int omv_set_auto_exposure(omv_sensor_t *sensor, int enable, int exposure_us)
{
  sensor_t *esp_sensor = esp_camera_sensor_get();
  return esp_sensor->set_exposure_ctrl(esp_sensor, enable);
}

static int omv_set_auto_whitebal(omv_sensor_t *sensor, int enable, float r_gain_db, float g_gain_db, float b_gain_db)
{
  sensor_t *esp_sensor = esp_camera_sensor_get();
  return esp_sensor->set_whitebal(esp_sensor, enable);
}

static int omv_set_hmirror(omv_sensor_t *sensor, int enable)
{
  sensor_t *esp_sensor = esp_camera_sensor_get();
  return esp_sensor->set_hmirror(esp_sensor, enable);
}

static int omv_set_vflip(omv_sensor_t *sensor, int enable)
{
  sensor_t *esp_sensor = esp_camera_sensor_get();
  return esp_sensor->set_vflip(esp_sensor, enable);
}

static int omv_set_special_effect(omv_sensor_t *sensor, sde_t sde)
{
  sensor_t *esp_sensor = esp_camera_sensor_get();
  return esp_sensor->set_special_effect(esp_sensor, sde);
}

static void esp32_sensor_init(omv_sensor_t *sensor)
{
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Camera Init Failed");
    return (int) err;
  }
  
  sensor->reset = omv_reset;
  sensor->set_pixformat = omv_set_pixformat;
  sensor->set_framesize = omv_set_framesize;
  sensor->set_contrast = omv_set_contrast;
  sensor->set_brightness = omv_set_brightness;
  sensor->set_saturation = omv_set_saturation;
  sensor->set_gainceiling = omv_set_gainceiling;
  sensor->set_quality = omv_set_quality;
  sensor->set_colorbar = omv_set_colorbar;
  sensor->set_auto_exposure = omv_set_auto_exposure;
  sensor->set_auto_whitebal = omv_set_auto_whitebal;
  sensor->set_hmirror = omv_set_hmirror;
  sensor->set_vflip = omv_set_vflip;    
  
  sensor->slv_addr = 0x42;
  sensor->chip_id = ESP32CAM_CHIP_ID;
}
