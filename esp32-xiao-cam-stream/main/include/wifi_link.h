#ifndef __WIFI_LINK_H__
#define __WIFI_LINK_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "sys/socket.h"

typedef struct {
    int socket;
    int clientSocket;
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen;
    bool enabled;
    TaskHandle_t rxTaskHandle;
    TaskHandle_t txTaskHandle;
    QueueHandle_t txQueue;
    PodtpPacket rxPacket;
    // Add other members as needed
} WifiLink;

void wifiInit(int8_t configIndex);
void wifiLinkInit();
void wifiLinkEnableStream(bool enable);
void wifiLinkSendImage(uint8_t *data, uint32_t length);

#endif // __WIFI_LINK_H__