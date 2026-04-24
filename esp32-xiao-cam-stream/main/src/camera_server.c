#include "camera_server.h"
#include "wifi_link.h"

#include "esp_camera.h"
#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
#include "camera_pins.h"

void cameraServerTask(void *pvParameters) {
    camera_fb_t * fb = NULL;
    int count = 0;
    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            printf("Camera capture [FAILED]\n");
            continue;
        }

        wifiLinkSendImage(fb->buf, fb->len);
        
        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
        }
        count++;
    }
}

static camera_config_t initConfig = {
    .ledc_channel = LEDC_CHANNEL_0,
    .ledc_timer = LEDC_TIMER_0,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d7 = Y9_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_sccb_sda = SIOD_GPIO_NUM,
    .pin_sccb_scl = SIOC_GPIO_NUM,
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .xclk_freq_hz = 20000000,
    .frame_size = FRAMESIZE_HD,
    .pixel_format = PIXFORMAT_JPEG, // for streaming
    .fb_location = CAMERA_FB_IN_PSRAM,
    .jpeg_quality = 6,
    .fb_count = 2,
    .grab_mode = CAMERA_GRAB_LATEST
};

void cameraConfig(_camera_config_t *config) {  
    sensor_t *s = esp_camera_sensor_get();
    if (s == NULL && config->on) {
        esp_err_t err = esp_camera_init(&initConfig);
        if (err != ESP_OK) {
            printf("Camera init [FAILED]: 0x%x\n", err);
            return;
        }
    }

    if (s != NULL && !config->on) {
        esp_camera_deinit();
        return;
    }

    if (config->on) {
        printf("Camera config: %d, %d, %d, %d, %d, %d\n", config->frame_size, config->quality, config->brighness, config->contrast, config->saturation, config->sharpness);
        s->set_framesize(s, (framesize_t)config->frame_size);
        s->set_quality(s, config->quality);
        s->set_brightness(s, config->brighness);
        s->set_contrast(s, config->contrast);
        s->set_saturation(s, config->saturation);
        s->set_sharpness(s, config->sharpness);
    }
}

void cameraInit() {
    printf("Camera init [START]\n");
    // camera init
    esp_err_t err = esp_camera_init(&initConfig);
    if (err != ESP_OK) {
        printf("Camera init [FAILED]: 0x%x\n", err);
        return;
    }
    sensor_t *s = esp_camera_sensor_get();
    // initial sensors are flipped vertically and colors are a bit saturated
    printf("Camera id: 0x%x\n", s->id.PID);
    xTaskCreatePinnedToCore(cameraServerTask, "camera_server_task", 8192, NULL, 10, NULL, 0);
}