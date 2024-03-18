#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

#include "pico/cyw43_arch.h"
#include "btstack.h"
#include "hci.h"
#include "pico/btstack_cyw43.h"

#include "DualSense.h"


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

static const char *const dualsense_addr_string = DUALSENSE_ADDR;

static bool can_try_connect = false;
static bool connected = false;
static bool connecting = false;

struct DualSense_InputReport_t {
    unsigned char header: 8;    // = A1
    unsigned char id: 8;        // = 01
    DualSense_State_t state;
} __packed;

DualSense_State_t last_state;

static void print_hello() {
    printf("FreeRTOS started at %d cores\n", configNUMBER_OF_CORES);
}

uint16_t outgoing_control_cid = 0;
uint16_t outgoing_interrupt_cid = 0;
uint16_t incoming_control_cid = 0;
uint16_t incoming_interrupt_cid = 0;

void on_channel_closed(uint16_t cid) {
    if (cid == outgoing_interrupt_cid) {
        outgoing_interrupt_cid = 0;
    }
    if (cid == outgoing_control_cid) {
        outgoing_control_cid = 0;
    }
    if (cid == incoming_interrupt_cid) {
        incoming_interrupt_cid = 0;
    }
    if (cid == incoming_control_cid) {
        incoming_control_cid = 0;
    }
}

void l2cap_event_handler(uint16_t channel, uint8_t *packet, uint16_t size) {
    switch (hci_event_packet_get_type(packet)) {
        case L2CAP_EVENT_INCOMING_CONNECTION:
            switch (l2cap_event_incoming_connection_get_psm(packet)) {
                case PSM_HID_CONTROL:
                    incoming_control_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                    l2cap_accept_connection(channel);
                    break;
                case PSM_HID_INTERRUPT:
                    incoming_interrupt_cid = l2cap_event_incoming_connection_get_local_cid(packet);
                    l2cap_accept_connection(channel);
                    break;
                default:
                    l2cap_decline_connection(channel);
                    break;
            }
            break;

        case L2CAP_EVENT_CHANNEL_OPENED: {
            uint16_t cid = l2cap_event_channel_opened_get_local_cid(packet);
            uint8_t status = l2cap_event_channel_opened_get_status(packet);

            if (status == ERROR_CODE_SUCCESS) {
                connected = true;
                log_info("L2CAP channel opened (cid %d)\n", (int) cid);
            } else {
                connected = false;
                if (status == L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY) {
                    // Assume remote has forgotten link key, delete it and try again
                    log_info("Dropping link key due to L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY\n");
                    bd_addr_t addr;
                    l2cap_event_channel_opened_get_address(packet, addr);
                    gap_drop_link_key_for_bd_addr(addr);
                    gap_disconnect(l2cap_event_channel_opened_get_handle(packet));
                } else {
                    log_info("L2CAP connection failed (cid %d, status 0x%02x)\n", (int) cid, (int) status);
                }
                on_channel_closed(cid);
            }
            break;
        }

        case L2CAP_EVENT_CHANNEL_CLOSED: {
            uint16_t cid = l2cap_event_channel_closed_get_local_cid(packet);

            printf("L2CAP channel closed (cid %d)\n", (int) cid);
            on_channel_closed(cid);
            break;
        }
    }
}

void l2cap_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
//    log_debug("l2cap paket #%d at channel #%d, size = %d\n", packet_type, channel, size);
//    log_debug_hexdump(NULL, packet, size);

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            l2cap_event_handler(channel, packet, size);
            break;
        case L2CAP_DATA_PACKET:
            DualSense_InputReport_t *report = (DualSense_InputReport_t *) packet;
            if (report->header != 0xA1 || report->id != 0x01) {
                break;
            }

            memcpy(reinterpret_cast<uint8_t *>(&last_state), &report->state, sizeof(last_state));
            break;

    }
}

btstack_packet_callback_registration_t dualsense_hci_event_callback_registration;

void hci_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }

//    log_debug("hci paket #%d at channel #%d, size = %d\n", packet_type, channel, size);
//    log_debug_hexdump(NULL, packet, size);

    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            log_info("BTSTACK_EVENT_STATE %d\n", btstack_event_state_get_state(packet));
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                can_try_connect = true;
            }
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            can_try_connect = true;
            log_info("HCI_EVENT_DISCONNECTION_COMPLETE\n");
            connected = false;
            break;
        case HCI_EVENT_CONNECTION_COMPLETE:
            log_debug_hexdump("HCI_EVENT_CONNECTION_COMPLETE", packet, size);
            connecting = false;
    }
}

void taskDualSense(__unused void *pvParams) {

    log_info("%s\n", __PRETTY_FUNCTION__);
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);

    gap_set_default_link_policy_settings(LM_LINK_POLICY_ENABLE_SNIFF_MODE);
    gap_connectable_control(1);

    // l2cap
    sdp_init();
    l2cap_init();
    l2cap_register_service(l2cap_handler, PSM_HID_INTERRUPT, 0xffff, gap_get_security_level());
    l2cap_register_service(l2cap_handler, PSM_HID_CONTROL, 0xffff, gap_get_security_level());

    // hci
    dualsense_hci_event_callback_registration.callback = hci_handler;
    hci_add_event_handler(&dualsense_hci_event_callback_registration);
    hci_set_master_slave_policy(HCI_ROLE_MASTER);
    hci_power_control(HCI_POWER_ON);

    bd_addr_t remote_addr;
    if (sscanf_bd_addr(dualsense_addr_string, remote_addr) != 1) {
        panic("Failed to parse target address!\n");
    }

    // wait for HCI ready
    while (!can_try_connect) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    while (true) {
        if (can_try_connect) {
            can_try_connect = false;
            connecting = true;
            connected = false;
            log_info("Attempting to connect to %s\n", bd_addr_to_str(remote_addr));
            l2cap_create_channel(l2cap_handler, remote_addr, PSM_HID_CONTROL, 0xffff, &outgoing_control_cid);
            l2cap_create_channel(l2cap_handler, remote_addr, PSM_HID_INTERRUPT, 0xffff, &outgoing_interrupt_cid);
        }

        if (connecting) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(500));
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(500));
        } else if (connected) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

void taskPrintButtons(__unused void *pvParams) {
    TickType_t xPrevWakeUpTime = xTaskGetTickCount();

    for (;; xTaskDelayUntil(&xPrevWakeUpTime, pdMS_TO_TICKS(1000))) {
        if (!connected) {
            continue;
        }
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

    xTaskCreate(taskDualSense, "dualsense", configMINIMAL_STACK_SIZE * 8, NULL, tskIDLE_PRIORITY, &xTask);
    xTaskCreate(taskPrintButtons, "print", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY, NULL);

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

    xTaskCreate(taskMain, "main", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

    vTaskStartScheduler();

    return 0;
}
