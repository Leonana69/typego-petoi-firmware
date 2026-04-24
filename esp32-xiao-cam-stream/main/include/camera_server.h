#ifndef __CAMERA_SERVER_H__
#define __CAMERA_SERVER_H__

#include <stdint.h>

#define CAMERA_START_BYTE_1 0xAE
#define CAMERA_START_BYTE_2 0x6D

typedef struct {
    uint8_t on;
    uint8_t frame_size;
    int8_t quality;
    int8_t brighness;
    int8_t contrast;
    int8_t saturation;
    int8_t sharpness;
} __attribute__((packed)) _camera_config_t;

void cameraInit();
void cameraConfig(_camera_config_t *config);

#endif // __CAMERA_SERVER_H__