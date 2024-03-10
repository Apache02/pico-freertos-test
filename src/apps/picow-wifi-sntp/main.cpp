#include <stdio.h>
#include <stdlib.h>
#include <map>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/unique_id.h"
#include "FreeRTOS.h"
#include "task.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/sntp.h"
#include <time.h>
#include "hardware/rtc.h"


static const char *const ssid = WIFI_SSID;
static const char *const password = WIFI_PASSWORD;
static const char *const SNTP_servers[] = {
        "time.google.com",
        "time1.google.com",
        "time2.google.com",
        "time3.google.com",
        "time.cloudflare.com",
        "time.facebook.com",
        "time.windows.com",
        "time.apple.com",
        "time.euro.apple.com",
};

static void print_hello() {
    printf("FreeRTOS started at %d cores\n", configNUMBER_OF_CORES);
    char id[64];
    pico_get_unique_board_id_string(id, sizeof(id));
    printf("Pico unique_id: %s\n", id);
}

static void print_ifconfig(int itf) {
    char *ip = ipaddr_ntoa(netif_ip4_addr(&cyw43_state.netif[itf]));
    char *gw = ipaddr_ntoa(netif_ip4_gw(&cyw43_state.netif[itf]));
    printf("      IP: %s\n", ip);
    printf(" Gateway: %s\n", gw);

    uint8_t mac[6];
    if (cyw43_wifi_get_mac(&cyw43_state, itf, mac) == 0) {
        char mac_str[20];
        sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        printf("     MAC: %s\n", mac_str);
    }
}

void taskStatusLed(__unused void *pvParams) {
    while (true) {
        auto status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

        switch (status) {
            case CYW43_LINK_UP:
                // connected
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            case CYW43_LINK_JOIN:
            case CYW43_LINK_NOIP:
                // connecting
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(200));
                break;
            case CYW43_LINK_DOWN:
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                break;
            case CYW43_LINK_FAIL:
            case CYW43_LINK_NONET:
            case CYW43_LINK_BADAUTH:
                for (auto i = 3; i > 0; i--) {
                    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                vTaskDelay(pdMS_TO_TICKS(200));
                break;
        }
    }
}

void taskWifi(__unused void *pvParams) {
    vTaskDelay(pdMS_TO_TICKS(2000));
    int tryNumber = 0;

    while (true) {
        tryNumber++;

        cyw43_arch_enable_sta_mode();

        size_t password_length = std::min((size_t) 16, strlen(password));
        int wifi_connect_status = 0;

        if (password_length == 0) {
            printf("Connecting to Wi-Fi [open] (%s)...\n", ssid);
            wifi_connect_status = cyw43_arch_wifi_connect_timeout_ms(ssid, NULL, CYW43_AUTH_OPEN, 30000);
        } else {
            char password_masked[password_length + 1];
            memset(password_masked, '*', password_length);
            password_masked[password_length] = '\0';
            printf("Connecting to Wi-Fi [WPA2] (%s, %s)...\n", ssid, password_masked);
            wifi_connect_status = cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 30000);
        }

        if (wifi_connect_status == PICO_ERROR_BADAUTH) {
            printf("failed to connect. Invalid password.\n");
            cyw43_arch_disable_sta_mode();
            tryNumber = 0;
            vTaskDelay(pdMS_TO_TICKS(60000));
        } else if (wifi_connect_status != PICO_OK) {
            printf("failed to connect.\n");

            auto delay = std::min(3, tryNumber) * 10000;
            vTaskDelay(pdMS_TO_TICKS(delay));
        } else {
            tryNumber = 0;
            printf("wifi connected.\n");

            for (auto i = 0; i < 10; i++) {
                auto tcp_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
                if (tcp_status == CYW43_LINK_UP) {
                    print_ifconfig(CYW43_ITF_STA);
                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            while (true) {
                vTaskDelay(pdMS_TO_TICKS(2000));

                auto tcp_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
                if (tcp_status != CYW43_LINK_UP) {
                    break;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void taskSNTP(__unused void *pvParams) {
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (true) {
        // wait connection
        auto tcp_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
        if (tcp_status == CYW43_LINK_UP) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    printf("connection ready\n");

    // init servers
    vTaskDelay(pdMS_TO_TICKS(1000));

    int i = 0;
    for (auto domain: SNTP_servers) {
        if (i >= SNTP_MAX_SERVERS) {
            break;
        }
        sntp_setservername(i++, domain);
        printf("added server %s\n", domain);
    }

    rtc_init();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_init();

    auto xLastTime = xTaskGetTickCount();
    while (true) {
        datetime_t d;

        if (rtc_get_datetime(&d)) {
            printf("RTC: %02d-%02d-%02d %02d:%02d:%02d\n",
                   d.year,
                   d.month,
                   d.day,
                   d.hour,
                   d.min,
                   d.sec);
        }

        vTaskDelayUntil(&xLastTime, pdMS_TO_TICKS(1000));
    }
}

void sntpSetTimeSec(uint32_t sec) {
    datetime_t date;
    struct tm *timeinfo;

    // offset
    time_t t = sec + (60 * 0);

    timeinfo = gmtime(&t);

    memset(&date, 0, sizeof(date));
    date.sec = timeinfo->tm_sec;
    date.min = timeinfo->tm_min;
    date.hour = timeinfo->tm_hour;
    date.day = timeinfo->tm_mday;
    date.month = timeinfo->tm_mon + 1;
    date.year = timeinfo->tm_year + 1900;

    rtc_set_datetime(&date);
}

int main() {
    stdio_init_all();
    sleep_ms(2000);
    print_hello();

    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed");
        return 1;
    }

    TaskHandle_t xWifiTask;

    xTaskCreate(taskStatusLed, "status_led", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(taskWifi, "wifi", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY, &xWifiTask);
    xTaskCreate(taskSNTP, "sntp", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY, NULL);

#if NO_SYS && configUSE_CORE_AFFINITY
    vTaskCoreAffinitySet(xWifiTask, 1);
#endif

    vTaskStartScheduler();

    return 0;
}
