#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
 #include "freertos/event_groups.h"

#include "freertos/queue.h"
#include "driver/gpio.h"
#include "Event_data.h"


#define GPIO_INPUT_IO_1     17
#define GPIO_INPUT_PIN_SEL1  ((1ULL<<GPIO_INPUT_IO_1))

//motion_task uses Pir motion sensor for measuring movement in infrared.
//gpio_get_level function is used for and reading from gpio pins
//sensor values range from 0 to 1
static void motion_task(void *arg)
{
    struct event_data motion;
    motion.mode = sensor_motion;
    motion.value = 0;

    gpio_config_t io_conf;
    //disable interrupt

    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL1;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    int val = 0;

    xEventGroupSync( xEventBits, BIT_1, ( BIT_0 | BIT_1 | BIT_2 | BIT_3), portMAX_DELAY );
    vTaskDelay(10);
    while (1) {
            val = gpio_get_level(17);// read input value
            motion.value = val;
            if (val == 1) {            // check if the input is HIGH
            xQueueSendToBack(sensor_queue, (void *)&motion, portMAX_DELAY);

                for(int i = 0; i < 60; i++){ // 10 min timeout on movement
                    vTaskDelay(10); // 10 times a sec, check for movement
                    if(gpio_get_level(17)){
                        i = 0;
                    }
                }

            motion.value = 0; // no movement detected for 10 min, turn off
            xQueueSendToBack(sensor_queue, (void *)&motion, portMAX_DELAY);

            }
             vTaskDelay(100);
        }
}
