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

#define SAVE_DELAY 1000

homekit_characteristic_t wifi_reset   = HOMEKIT_CHARACTERISTIC_(CUSTOM_WIFI_RESET, false, .setter=wifi_reset_set);
homekit_characteristic_t wifi_check_interval   = HOMEKIT_CHARACTERISTIC_(CUSTOM_WIFI_CHECK_INTERVAL, 10, .setter=wifi_check_interval_set);
/* checks the wifi is connected and flashes status led to indicated connected */
homekit_characteristic_t task_stats   = HOMEKIT_CHARACTERISTIC_(CUSTOM_TASK_STATS, false , .setter=task_stats_set);

homekit_characteristic_t ota_beta     = HOMEKIT_CHARACTERISTIC_(CUSTOM_OTA_BETA, false, .setter=ota_beta_set);
homekit_characteristic_t lcm_beta    = HOMEKIT_CHARACTERISTIC_(CUSTOM_LCM_BETA, false, .setter=lcm_beta_set);
homekit_characteristic_t preserve_state   = HOMEKIT_CHARACTERISTIC_(CUSTOM_PRESERVE_STATE, false, .setter=preserve_state_set);


homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t name         = HOMEKIT_CHARACTERISTIC_(NAME, DEVICE_NAME);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  DEVICE_MANUFACTURER);
homekit_characteristic_t serial       = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, DEVICE_SERIAL);
homekit_characteristic_t model        = HOMEKIT_CHARACTERISTIC_(MODEL,         DEVICE_MODEL);
homekit_characteristic_t revision     = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  FW_VERSION);

homekit_characteristic_t socket_one   = HOMEKIT_CHARACTERISTIC_(
                                                             ON, false, .description ="socket one", .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(socket_one_callback)
                                                             );
homekit_characteristic_t socket_two  = HOMEKIT_CHARACTERISTIC_(
                                                                ON, false, .description ="socket two",.callback=HOMEKIT_CHARACTERISTIC_CALLBACK(socket_two_callback)
                                                                );
homekit_characteristic_t socket_three   = HOMEKIT_CHARACTERISTIC_(
                                                                ON, false, .description ="socket three",.callback=HOMEKIT_CHARACTERISTIC_CALLBACK(socket_three_callback)
                                                                );
homekit_characteristic_t socket_usb   = HOMEKIT_CHARACTERISTIC_(
                                                                ON, false, .description ="socket usb",.callback=HOMEKIT_CHARACTERISTIC_CALLBACK(socket_usb_callback)
                                                                );
const int LED_GPIO = 0;
const int LED_2_GPIO = 2;
const int BUTTON_GPIO = 13;
const int SOCKET_THREE_GPIO = 5;
const int SOCKET_ONE_GPIO = 12;
const int SOCKET_TWO_GPIO = 14;
const int SOCKET_USB_GPIO = 15;

int led_off_value=0; /* global varibale to support LEDs set to 0 where the LED is connected to GND, 1 where +3.3v */

const int status_led_gpio = 0; /*set the gloabl variable for the led to be used for showing status */


void socket_one_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context){
    printf("%s:\n", __func__);
    relay_write(socket_one.value.bool_value, SOCKET_ONE_GPIO);
}

void socket_two_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context){
    printf("Socket two callback\n");
    relay_write(socket_two.value.bool_value, SOCKET_TWO_GPIO);
    sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
}

void socket_three_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context){
    printf("Socket three callback\n");
    relay_write(socket_three.value.bool_value, SOCKET_THREE_GPIO);
    sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
}

void socket_usb_callback (homekit_characteristic_t *_ch, homekit_value_t on, void *context){
    printf("Socket usb callback\n");
    relay_write(socket_usb.value.bool_value, SOCKET_USB_GPIO);
    sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );

}

void button_single_press_callback(uint8_t gpio, void* args, uint8_t param) {
    
    printf("Button event single press on GPIO : %d\n", gpio);
    printf("Toggling switch one\n");
    socket_one.value.bool_value = !socket_one.value.bool_value;
    relay_write(socket_one.value.bool_value, SOCKET_ONE_GPIO);
    homekit_characteristic_notify(&socket_one, socket_one.value);
    sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );
    
}


void button_double_press_callback(uint8_t gpio, void* args, uint8_t param) {
    
    printf("Button event double press on GPIO : %d\n", gpio);
    
    printf("Toggling socket one\n");
    socket_one.value.bool_value = !socket_one.value.bool_value;
    relay_write(socket_one.value.bool_value, SOCKET_ONE_GPIO);
    homekit_characteristic_notify(&socket_one, socket_one.value);

    printf("Toggling socket two\n");
    socket_two.value.bool_value = !socket_two.value.bool_value;
    relay_write(socket_two.value.bool_value, SOCKET_TWO_GPIO);
    homekit_characteristic_notify(&socket_two, socket_two.value);

    printf("Toggling socket three\n");
    socket_three.value.bool_value = !socket_three.value.bool_value;
    relay_write(socket_three.value.bool_value, SOCKET_THREE_GPIO);
    homekit_characteristic_notify(&socket_three, socket_three.value);
    
    sdk_os_timer_arm (&save_timer, SAVE_DELAY, 0 );

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
    printf("%s: Initialising buttons\n", __func__);
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
            &ota_trigger,
            &wifi_reset,
            &wifi_check_interval,
            &task_stats,
            &preserve_state,
            NULL
        }),
        NULL
    }),
    NULL
};


void accessory_init_not_paired (void) {
    /* initalise anything you don't want started until wifi and homekit imitialisation is confirmed, but not paired */
    
}

void accessory_init (void ){
/* initalise anything you don't want started until wifi and pairing is confirmed */
    
    printf ("%s:\n", __func__);
    if ( preserve_state.value.bool_value == true){
        printf ("%s:Loading preserved state\n", __func__);
        
        load_characteristic_from_flash(&socket_one);
        relay_write(socket_one.value.bool_value, SOCKET_ONE_GPIO);
        
        load_characteristic_from_flash(&socket_two);
        relay_write(socket_two.value.bool_value, SOCKET_TWO_GPIO);

        
        load_characteristic_from_flash(&socket_three);
        relay_write(socket_three.value.bool_value, SOCKET_THREE_GPIO);

        
        load_characteristic_from_flash(&socket_usb);
        relay_write(socket_usb.value.bool_value, SOCKET_USB_GPIO);

        
        load_characteristic_from_flash(&wifi_check_interval);
    } else {
        printf ("%s:Preserved state is off\n", __func__);
    }
    homekit_characteristic_notify(&preserve_state, preserve_state.value);
    homekit_characteristic_notify(&wifi_check_interval, wifi_check_interval.value);
    homekit_characteristic_notify(&socket_one, socket_one.value);
    homekit_characteristic_notify(&socket_two, socket_two.value);
    homekit_characteristic_notify(&socket_three, socket_three.value);
    homekit_characteristic_notify(&socket_usb, socket_usb.value);

}


void recover_from_reset (int reason){
    /* called if we restarted abnormally */
    printf ("%s: reason %d\n", __func__, reason);
}


void save_characteristics (){
    
    /* called if we restarted abnormally */
    printf ("%s:\n", __func__);
    
    save_characteristic_to_flash(&preserve_state, preserve_state.value);
    if ( preserve_state.value.bool_value == true){
        printf ("%s:Preserving state\n", __func__);
        save_characteristic_to_flash(&socket_one, socket_one.value);
        save_characteristic_to_flash(&socket_two, socket_two.value);
        save_characteristic_to_flash(&socket_three , socket_three.value);
        save_characteristic_to_flash(&socket_usb , socket_usb.value);
        save_characteristic_to_flash(&wifi_check_interval, wifi_check_interval.value);
    } else {
        printf ("%s:Not preserving state\n", __func__);
    }
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

    wifi_config_init(DEVICE_NAME, NULL, on_wifi_ready);
}
