#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "btstack_config.h"
#include "pico/cyw43_arch.h"
#include "btstack.h"

#include "DualSense.h"


static const char *const dualsense_addr_string = DUALSENSE_ADDR;

#if 1
#define log_error(format, ...)      printf("[ERROR] " format, ## __VA_ARGS__)
#define log_warning(format, ...)    printf("[WARNING] " format, ## __VA_ARGS__)
#define log_info(format, ...)       printf("[INFO] " format, ## __VA_ARGS__)
#define log_debug(format, ...)       printf("[DEBUG] " format, ## __VA_ARGS__)

static void log_debug_hexdump(const char *message, uint8_t *buffer, size_t size) {
    if (message) {
        printf("[DEBUG] %s\n", message);
    }
    for (auto i = 0; i < size; i++) {
        if ((i & 0x0f) == 0) {
            printf("%08p | +%02X |", buffer, i);
        }
        printf(" %02X", buffer[i]);
        if ((i & 0x0f) == 0x0f && (i + 1) < size) {
            printf("\n");
        }
    }
    printf("\n");
}

#else
#define log_error(...) (void)(0)
#define log_warning(...) (void)(0)
#define log_info(...) (void)(0)
#define log_debug(...) (void)(0)
#define log_debug_hexdump(...) (void)(0)
#endif


struct DualSense_InputReport_t {
    unsigned char header: 8;    // = A1
    unsigned char id: 8;        // = 01
    DualSense_State_t state;
} __packed;

DualSense_State_t last_state;

// SDP
static uint8_t hid_descriptor_storage[300];

static bd_addr_t dualsense_addr;
static btstack_packet_callback_registration_t hci_event_callback_registration;

static uint16_t hid_host_cid = 0;
static bool hid_host_descriptor_available = false;
//static hid_protocol_mode_t hid_host_report_mode = HID_PROTOCOL_MODE_REPORT_WITH_FALLBACK_TO_BOOT;
static hid_protocol_mode_t hid_host_report_mode = HID_PROTOCOL_MODE_REPORT;

enum {
    NONE = 0,
    CONNECTING,
    CONNECTED,
} app_state = NONE;

static void print_hello() {
    printf("FreeRTOS started at %d cores\n", configNUMBER_OF_CORES);
}

static void hid_host_handle_interrupt_report(const uint8_t *report, uint16_t report_len) {
    // check if HID Input Report
    if (report_len < 1) return;
    if (report[0] != 0xa1) return;

    // check id?
    if (report[1] != 0x01) return;

    memcpy(static_cast<void *>(&last_state), &report[2], sizeof(last_state));
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
//    portENTER_CRITICAL();

    UNUSED(channel);
    UNUSED(size);

    uint8_t event;
    uint8_t status;

    /* LISTING_RESUME */
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            event = hci_event_packet_get_type(packet);

            switch (event) {
#ifndef HAVE_BTSTACK_STDIN
                /* @text When BTSTACK_EVENT_STATE with state HCI_STATE_WORKING
                 * is received and the example is started in client mode, the remote SDP HID query is started.
                 */
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                        status = hid_host_connect(dualsense_addr, hid_host_report_mode, &hid_host_cid);
                        if (status != ERROR_CODE_SUCCESS) {
                            log_info("HID host connect failed, status 0x%02x.\n", status);
                        }
                    }
                    break;
#endif
                case HCI_EVENT_HID_META:
                    switch (hci_event_hid_meta_get_subevent_code(packet)) {

                        case HID_SUBEVENT_INCOMING_CONNECTION:
                            log_debug_hexdump("HID_SUBEVENT_INCOMING_CONNECTION", packet, size);
                            // There is an incoming connection: we can accept it or decline it.
                            // The hid_host_report_mode in the hid_host_accept_connection function
                            // allows the application to request a protocol mode.
                            // For available protocol modes, see hid_protocol_mode_t in btstack_hid.h file.
                            hid_host_accept_connection(hid_subevent_incoming_connection_get_hid_cid(packet),
                                                       hid_host_report_mode);
                            break;

                        case HID_SUBEVENT_CONNECTION_OPENED:
                            log_debug_hexdump("HID_SUBEVENT_CONNECTION_OPENED", packet, size);
                            // The status field of this event indicates if the control and interrupt
                            // connections were opened successfully.
                            status = hid_subevent_connection_opened_get_status(packet);
                            if (status != ERROR_CODE_SUCCESS) {
                                log_info("Connection failed, status 0x%02x\n", status);
                                app_state = NONE;
                                hid_host_cid = 0;
                                break;
                            }
                            app_state = CONNECTED;
                            hid_host_descriptor_available = false;
                            hid_host_cid = hid_subevent_connection_opened_get_hid_cid(packet);
                            log_info("HID Host connected.\n");
                            break;

                        case HID_SUBEVENT_DESCRIPTOR_AVAILABLE:
                            log_debug_hexdump("HID_SUBEVENT_DESCRIPTOR_AVAILABLE", packet, size);
                            // This event will follows HID_SUBEVENT_CONNECTION_OPENED event.
                            // For incoming connections, i.e. HID Device initiating the connection,
                            // the HID_SUBEVENT_DESCRIPTOR_AVAILABLE is delayed, and some HID
                            // reports may be received via HID_SUBEVENT_REPORT event. It is up to
                            // the application if these reports should be buffered or ignored until
                            // the HID descriptor is available.
                            status = hid_subevent_descriptor_available_get_status(packet);
                            if (status == ERROR_CODE_SUCCESS) {
                                hid_host_descriptor_available = true;
                                log_info("HID Descriptor available, please start typing.\n");
                            } else {
                                log_info("Cannot handle input report, HID Descriptor is not available, status 0x%02x\n",
                                         status);
                            }
                            break;

                        case HID_SUBEVENT_REPORT:
                            // Handle input report.
                            if (app_state == CONNECTED) {
                                hid_host_handle_interrupt_report(hid_subevent_report_get_report(packet),
                                                                 hid_subevent_report_get_report_len(packet) + 1);
                            }
                            break;

                        case HID_SUBEVENT_CONNECTION_CLOSED:
                            // The connection was closed.
                            hid_host_cid = 0;
                            hid_host_descriptor_available = false;
                            app_state = NONE;
                            log_info("HID Host disconnected.\n");
                            break;
                    }
                    break;
                default:
//                    log_debug("event = %d\n", event);
//                    log_debug_hexdump(NULL, packet, size);
                    break;
            }
            break;
        default:
            break;
    }

//    portEXIT_CRITICAL();
}


void taskDualSense(__unused void *pvParams) {
    // Initialize L2CAP
    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    // Initialize HID Host
    hid_host_init(hid_descriptor_storage, sizeof(hid_descriptor_storage));
    hid_host_register_packet_handler(packet_handler);

    // Allow sniff mode requests by HID device and support role switch
    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE | LM_LINK_POLICY_ENABLE_ROLE_SWITCH);

    // try to become master on incoming connections
    hci_set_master_slave_policy(HCI_ROLE_MASTER);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // Disable stdout buffering
    setvbuf(stdin, NULL, _IONBF, 0);

    // parse human readable Bluetooth address
    sscanf_bd_addr(dualsense_addr_string, dualsense_addr);

    // Turn on the device
    hci_power_control(HCI_POWER_ON);

    app_state = CONNECTING;

    while (true) {
        switch (app_state) {
            case NONE:
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                break;
            case CONNECTING:
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(200));
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                break;
            case CONNECTED:
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                vTaskDelay(pdMS_TO_TICKS(200));
                break;
        }
    }
}

void taskPrintButtons(__unused void *pvParams) {
    TickType_t xPrevWakeUpTime = xTaskGetTickCount();

    for (;; xTaskDelayUntil(&xPrevWakeUpTime, pdMS_TO_TICKS(1000))) {
        if (app_state != CONNECTED) {
            continue;
        }

        log_debug_hexdump(
                "------------------------------------------------",
                reinterpret_cast<uint8_t *>(&last_state),
                sizeof(last_state)
        );

        printf(
                "left: %d;%d  |  right: %d;%d  |  l2: %d  | r2: %d | dpad: %x\n",
                last_state.left_stick.x,
                last_state.left_stick.y,
                last_state.right_stick.x,
                last_state.right_stick.y,
                last_state.l2_stick,
                last_state.r2_stick,
                last_state.buttons.dpad
        );
        printf(
                "sq %d, x %d, O %d, tri %d | "
                "l1 %d, r1 %d, l2 %d, r2 %d, l3 %d, r3 %d | "
                "create %d, options %d, PS %d"
                "\n",
                last_state.buttons.square,
                last_state.buttons.cross,
                last_state.buttons.circle,
                last_state.buttons.triangle,
                last_state.buttons.l1,
                last_state.buttons.r1,
                last_state.buttons.l2,
                last_state.buttons.r2,
                last_state.buttons.l3,
                last_state.buttons.r3,
                last_state.buttons.create,
                last_state.buttons.options,
                last_state.ps_button
        );
    }
}

void taskMain(__unused void *pvParams) {
    if (cyw43_arch_init()) {
        log_error("failed to initialise cyw43_arch\n");
        return;
    }

    TaskHandle_t xTask = 0;

    xTaskCreate(taskDualSense, "dualsense", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY, &xTask);
    xTaskCreate(taskPrintButtons, "print", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY, NULL);

#if configUSE_CORE_AFFINITY
    if (xTask) vTaskCoreAffinitySet(xTask, 1 << 0);
#endif

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    cyw43_arch_deinit();
}

int main() {
    stdio_init_all();

    sleep_ms(2000);
    print_hello();

    TaskHandle_t xTask = 0;
    xTaskCreate(taskMain, "main", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY + 2, &xTask);
#if configUSE_CORE_AFFINITY && configNUM_CORES > 1
    vTaskCoreAffinitySet(task, 1 << 0);
#endif

    vTaskStartScheduler();

    return 0;
}
