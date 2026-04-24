#include "wifi.h"
#include "camera_stream.h"

void app_main(void)
{
    wifi_init();
    camera_init();
    camera_stream_start();
}
