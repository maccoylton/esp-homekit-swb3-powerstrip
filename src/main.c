/*
 * Copyright 2018 David B Brown (@maccoylton)
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Homekit firmware for SWB3 Power Strip
 */

#define DEVICE_MANUFACTURER "Yagala"
#define DEVICE_NAME "PowerStrip"
#define DEVICE_MODEL "SWB3"
#define DEVICE_SERIAL "12345678"
#define FW_VERSION "1.0"

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>


#include <adv_button.h>
#include <led_codes.h>
#include <udplogger.h>


// add this section to make your device OTA capable
// create the extra characteristic &ota_trigger, at the end of the primary service (before the NULL)
// it can be used in Eve, which will show it, where Home does not
// and apply the four other parameters in the accessories_information section

#include <ota-api.h>

void socket_one_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void socket_two_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void socket_three_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void socket_usb_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context);


homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t name         = HOMEKIT_CHARACTERISTIC_(NAME, DEVICE_NAME);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  DEVICE_MANUFACTURER);
homekit_characteristic_t serial       = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, DEVICE_SERIAL);
homekit_characteristic_t model        = HOMEKIT_CHARACTERISTIC_(MODEL,         DEVICE_MODEL);
homekit_characteristic_t revision     = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  FW_VERSION);
homekit_characteristic_t socket_one   = HOMEKIT_CHARACTERISTIC_(
                                                             ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(socket_one_callback)
                                                             );
homekit_characteristic_t socket_two  = HOMEKIT_CHARACTERISTIC_(
                                                                ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(socket_two_callback)
                                                                );
homekit_characteristic_t socket_three   = HOMEKIT_CHARACTERISTIC_(
                                                                ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(socket_three_callback)
                                                                );
homekit_characteristic_t socket_usb   = HOMEKIT_CHARACTERISTIC_(
                                                                ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(socket_usb_callback)
                                                                );
const int STATUS_LED_GPIO = 0;
const int LED_GPIO = 2;
const int BUTTON_GPIO = 13;
const int SOCKET_THREE_GPIO = 5;
const int SOCKET_ONE_GPIO = 12;
const int SOCKET_TWO_GPIO = 14;
const int SOCKET_USB_GPIO = 15;


int led_off_value=1; /* global varibale to support LEDs set to 0 where the LED is connected to GND, 1 where +3.3v */


void relay_write(bool on, int gpio) {
    gpio_write(gpio, on ? 1 : 0);
}

void led_write(bool on) {
    gpio_write(LED_GPIO, on ? 0 : 1);
}

void reset_configuration_task() {
    //Flash the LED first before we start the reset
    led_code (LED_GPIO, WIFI_CONFIG_RESET);    
    printf("Resetting Wifi Config\n");
    
    wifi_config_reset();
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    printf("Resetting HomeKit Config\n");
    
    homekit_server_reset();
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    printf("Restarting\n");
    
    sdk_system_restart();
    
    vTaskDelete(NULL);
}

void reset_configuration() {
    printf("Resetting Device configuration\n");
    xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
}

void reset_button_callback(uint8_t gpio, void* args) {
    printf("Reset Button event long press on GPIO : %d\n", gpio);
    reset_configuration();
    
}

void socket_one_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context){
    printf("Socket one callback\n");
    relay_write(socket_one.value.bool_value, SOCKET_ONE_GPIO);
}

void socket_two_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context){
    printf("Socket one callback\n");
    relay_write(socket_two.value.bool_value, SOCKET_TWO_GPIO);
}

void socket_three_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context){
    printf("Socket one callback\n");
    relay_write(socket_three.value.bool_value, SOCKET_THREE_GPIO);
}

void socket_usb_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context){
    printf("Socket one callback\n");
    relay_write(socket_usb.value.bool_value, SOCKET_USB_GPIO);
}

void button_single_press_callback(uint8_t gpio, void* args) {
    
    printf("Button event single press on GPIO : %d\n", gpio);
/*    printf("Toggling relay\n");
    switch_on.value.bool_value = !switch_on.value.bool_value;
    relay_write(switch_on.value.bool_value);
    led_write(switch_on.value.bool_value);
    homekit_characteristic_notify(&switch_on, switch_on.value);
 */
    
}


void button_double_press_callback(uint8_t gpio, void* args) {
    
    printf("Button event double press on GPIO : %d\n", gpio);
    
}


void button_long_press_callback(uint8_t gpio, void* args) {
    
    printf("Button event long press on GPIO : %d\n", gpio);

}


void gpio_init() {

    const uint8_t toggle_press = 0, single_press = 1, double_press = 2,  long_press = 3, very_long_press = 4, hold_press = 5;

    adv_button_set_evaluate_delay(10);

    /* GPIO for button, pull-up resistor, inverted */
    printf("Initialising buttons\n");
    adv_button_create(BUTTON_GPIO, true, false);
    adv_button_register_callback_fn(BUTTON_GPIO, button_single_press_callback, single_press, NULL);
    adv_button_register_callback_fn(BUTTON_GPIO, button_double_press_callback, double_press, NULL);
    adv_button_register_callback_fn(BUTTON_GPIO, button_long_press_callback, long_press, NULL);

    
    
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    led_write(false);
    
    gpio_enable(STATUS_LED_GPIO, GPIO_OUTPUT);
    led_write(false);
    
    gpio_enable(SOCKET_ONE_GPIO, GPIO_OUTPUT);
    relay_write(socket_one.value.bool_value, SOCKET_ONE_GPIO);

    gpio_enable(SOCKET_TWO_GPIO, GPIO_OUTPUT);
    relay_write(socket_two.value.bool_value, SOCKET_TWO_GPIO);

    gpio_enable(SOCKET_THREE_GPIO, GPIO_OUTPUT);
    relay_write(socket_three.value.bool_value, SOCKET_THREE_GPIO);

    gpio_enable(SOCKET_USB_GPIO, GPIO_OUTPUT);
    relay_write(socket_usb.value.bool_value, SOCKET_USB_GPIO);

    
}

void switch_identify_task(void *_args) {
    // We identify the Device by Flashing it's LED.
    led_code( LED_GPIO, IDENTIFY_ACCESSORY);
    vTaskDelete(NULL);
}

void switch_identify(homekit_value_t _value) {
    printf("Switch identify\n");
    xTaskCreate(switch_identify_task, "Switch identify", 128, NULL, 2, NULL);
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            &manufacturer,
            &serial,
            &model,
            &revision,
            HOMEKIT_CHARACTERISTIC(IDENTIFY, switch_identify),
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Switch"),
            &socket_one,
            &socket_two,
            &socket_three,
            &socket_usb,
            &ota_trigger,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void create_accessory_name() {

    int serialLength = snprintf(NULL, 0, "%d", sdk_system_get_chip_id());

    char *serialNumberValue = malloc(serialLength + 1);

    snprintf(serialNumberValue, serialLength + 1, "%d", sdk_system_get_chip_id());
    
    int name_len = snprintf(NULL, 0, "%s-%s-%s",
				DEVICE_NAME,
				DEVICE_MODEL,
				serialNumberValue);

    if (name_len > 63) {
        name_len = 63;
    }

    char *name_value = malloc(name_len + 1);

    snprintf(name_value, name_len + 1, "%s-%s-%s",
		 DEVICE_NAME, DEVICE_MODEL, serialNumberValue);

   
    name.value = HOMEKIT_STRING(name_value);
    serial.value = name.value;
}

void user_init(void) {
    uart_set_baud(0, 115200);
    
    udplog_init(3);
    gpio_init();
    
    create_accessory_name();
    
    
    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
                                 &model.value.string_value,&revision.value.string_value);
    if (c_hash==0) c_hash=1;
    config.accessories[0]->config_number=c_hash;
    
    homekit_server_init(&config);
}
