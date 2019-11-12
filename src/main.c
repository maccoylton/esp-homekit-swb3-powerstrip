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
#include <custom_characteristics.h>
#include <shared_functions.h>


void socket_one_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void socket_two_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void socket_three_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void socket_usb_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context);

// add this section to make your device OTA capable
// create the extra characteristic &ota_trigger, at the end of the primary service (before the NULL)
// it can be used in Eve, which will show it, where Home does not
// and apply the four other parameters in the accessories_information section

#include <ota-api.h>


homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t wifi_reset   = HOMEKIT_CHARACTERISTIC_(CUSTOM_WIFI_RESET, false, .setter=wifi_reset_set);
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
const int LED_GPIO = 0;
const int LED_2_GPIO = 2;
const int BUTTON_GPIO = 13;
const int SOCKET_THREE_GPIO = 5;
const int SOCKET_ONE_GPIO = 12;
const int SOCKET_TWO_GPIO = 14;
const int SOCKET_USB_GPIO = 15;
int led_off_value=0; /* global varibale to support LEDs set to 0 where the LED is connected to GND, 1 where +3.3v */


const int status_led_gpio = 0; /*set the gloabl variable for the led to be sued for showing status */


void socket_one_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context){
    printf("Socket one callback\n");
    relay_write(socket_one.value.bool_value, SOCKET_ONE_GPIO);
}

void socket_two_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context){
    printf("Socket two callback\n");
    relay_write(socket_two.value.bool_value, SOCKET_TWO_GPIO);
}

void socket_three_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context){
    printf("Socket three callback\n");
    relay_write(socket_three.value.bool_value, SOCKET_THREE_GPIO);
}

void socket_usb_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context){
    printf("Socket usb callback\n");
    relay_write(socket_usb.value.bool_value, SOCKET_USB_GPIO);
}

void button_single_press_callback(uint8_t gpio, void* args, uint8_t param) {
    
    printf("Button event single press on GPIO : %d\n", gpio);
/*    printf("Toggling relay\n");
    switch_on.value.bool_value = !switch_on.value.bool_value;
    relay_write(switch_on.value.bool_value);
    led_write(switch_on.value.bool_value);
    homekit_characteristic_notify(&switch_on, switch_on.value);
 */
    
}


void button_double_press_callback(uint8_t gpio, void* args, uint8_t param) {
    
    printf("Button event double press on GPIO : %d\n", gpio);
    
}


void button_long_press_callback(uint8_t gpio, void* args, uint8_t param) {
    
    printf("Button event long press on GPIO : %d\n", gpio);

}

void button_very_long_press_callback(uint8_t gpio, void* args, uint8_t param) {
    
    printf("Button event very long press on GPIO : %d\n", gpio);
    reset_configuration();
    
}


void gpio_init() {


    adv_button_set_evaluate_delay(10);

    /* GPIO for button, pull-up resistor, inverted */
    printf("Initialising buttons\n");
    adv_button_create(BUTTON_GPIO, true, false);
    adv_button_register_callback_fn(BUTTON_GPIO, button_single_press_callback, SINGLEPRESS_TYPE, NULL, 0);
    adv_button_register_callback_fn(BUTTON_GPIO, button_double_press_callback, DOUBLEPRESS_TYPE, NULL, 0);
    adv_button_register_callback_fn(BUTTON_GPIO, button_long_press_callback, LONGPRESS_TYPE, NULL, 0);
    adv_button_register_callback_fn(BUTTON_GPIO, button_very_long_press_callback, VERYLONGPRESS_TYPE, NULL, 0);

    
    
    gpio_enable(LED_GPIO, GPIO_OUTPUT);
    led_write(false, LED_GPIO);
    
    gpio_enable(LED_2_GPIO, GPIO_OUTPUT);
    led_write(false, LED_2_GPIO);
    
    gpio_enable(SOCKET_ONE_GPIO, GPIO_OUTPUT);
    relay_write(socket_one.value.bool_value, SOCKET_ONE_GPIO);

    gpio_enable(SOCKET_TWO_GPIO, GPIO_OUTPUT);
    relay_write(socket_two.value.bool_value, SOCKET_TWO_GPIO);

    gpio_enable(SOCKET_THREE_GPIO, GPIO_OUTPUT);
    relay_write(socket_three.value.bool_value, SOCKET_THREE_GPIO);

    gpio_enable(SOCKET_USB_GPIO, GPIO_OUTPUT);
    relay_write(socket_usb.value.bool_value, SOCKET_USB_GPIO);

    
}



homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            &manufacturer,
            &serial,
            &model,
            &revision,
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Socket 1"),
            &socket_one,
            &ota_trigger,
            &wifi_reset,
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=false, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Socket 2"),
            &socket_two,
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=false, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Socket 3"),
            &socket_three,
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=false, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Socket USB"),
            &socket_usb,
            NULL
        }),
        NULL
    }),
    NULL
};


void accessory_init (void ){
/* initalise anything you don't want started until wifi and pairing is confirmed */
    
}

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111",
    .setupId = "1234",
    .on_event = on_homekit_event
};


void user_init(void) {

    standard_init (&name, &manufacturer, &model, &serial, &revision);

    gpio_init();

    wifi_config_init("AirQualitySensor", NULL, on_wifi_ready);
}
