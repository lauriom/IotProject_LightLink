#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "freertos/timers.h"
 #include "freertos/event_groups.h"

#include "Event_data.h"

#include "esp_log.h"


#define GPIO_OUTPUT_IO_0    18
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0))
#define GPIO_INPUT_IO_0     19
#define GPIO_INPUT_PIN_SEL  ((1ULL<<GPIO_INPUT_IO_0))

SemaphoreHandle_t twomic, tenmic;
static void twoMicTimerCallback(void* arg)
{
    xSemaphoreGive(twomic);
}
static void tenMicTimerCallback(void* arg)
{
    xSemaphoreGive(tenmic);
}

//ultra_task uses SRF05 ultrasonic sensor for measuring distance.
//Timers and semaphores are used for creating a delay that is very small(microseconds)
//gpio_set_level AND gpio_get_level functions are used for writing and reading from gpio pins
//sensor values range from 1 to 360(rough estimate)

static void ultra_task(void *arg)
{
    struct event_data ultra;
    ultra.mode = sensor_ultra;
    ultra.value = 0;
    gpio_config_t io_conf;
    //disable interrupt

    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);



    twomic = xSemaphoreCreateBinary();
    tenmic = xSemaphoreCreateBinary();

    const esp_timer_create_args_t twoMicTimer_args = {
            .callback = &twoMicTimerCallback,
            /* name is optional, but may help identify the timer when debugging */
            .name = "twomictimer"
    };
    const esp_timer_create_args_t tenMicTimer_args = {
            .callback = &tenMicTimerCallback,
            /* name is optional, but may help identify the timer when debugging */
            .name = "tenmictimer"
    };
    esp_timer_handle_t twoMicTimer;
    esp_timer_handle_t tenMicTimer;
    esp_timer_create(&twoMicTimer_args, &twoMicTimer);
    esp_timer_create(&tenMicTimer_args, &tenMicTimer);

    int echoState = 0;
    int counter = 0;

	int averageIrData[10] = {0};
    int arrPos = 0;
    bool arrPopulated = false;

    xEventGroupSync( xEventBits,BIT_2,( BIT_0 | BIT_1 | BIT_2 | BIT_3), portMAX_DELAY );
    vTaskDelay(10);

    while (1) {
        //The queue is used for getting the toggle settings from the website via controller_task.
        //active is an array of three boolean values
            gpio_set_level(18, 0);
            esp_timer_start_once(twoMicTimer, 2);
            if (xSemaphoreTake(twomic, portMAX_DELAY) == pdTRUE) {
                esp_timer_stop(twoMicTimer);
                gpio_set_level(18, 1);
                esp_timer_start_once(tenMicTimer, 10);
                if (xSemaphoreTake(tenmic, portMAX_DELAY) == pdTRUE) {
                    esp_timer_stop(tenMicTimer);
                    gpio_set_level(18, 0);

                    echoState = gpio_get_level(19);
                    while (echoState == 0) {
                        echoState = gpio_get_level(19);
                    }
                    while (echoState == 1) {
                        esp_timer_start_once(tenMicTimer, 10);
                        if (xSemaphoreTake(tenmic, portMAX_DELAY) == pdTRUE) {
                            esp_timer_stop(tenMicTimer);
                            counter++;
                            echoState = gpio_get_level(19);
                        }
                    }
                    ultra.value = counter;
                  //  printf("Ultra distance: %d\r\n", ultra.value);
                    //sensor_queue is used for sending the sensorvalues to the controller_task
                    xQueueSend(sensor_queue, (void *)&ultra, portMAX_DELAY);
					
					
					averageIrData[arrPos++] = counter;
                    if(arrPos == 10){
                        arrPos = 0;
                        arrPopulated = true;
                    }
                    int avg = 0;
                    for(int i = 0; i < 10; i++){
                        avg += averageIrData[i];
                    }
                    avg /= 10;
                    if (abs(counter - avg) > 50 && arrPopulated){
                        ultra.value = 1;
                        xQueueSendToBack(sensor_queue, (void *)&ultra, portMAX_DELAY);
                        vTaskDelay(500);
                    
                    }
                    ESP_LOGI("IR SENSOR", "val = %d avg = %d, abs = %d",counter, avg,(abs(counter - avg)));
                    counter = 0;
                }
            }
            vTaskDelay(100); 
    }
}
