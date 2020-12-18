#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"


TaskHandle_t ambientTaskHandle, motionTaskHandle, irTaskHandle;

#include "gatts_table.c"
#include "Uart.c"
#include "Event_data.h"
#include "Ambient_light.c"
#include "Pir_Motion.c"
#include "Ultra_sonic.c"



static void controller_task(void *arg)
{
    bool lightSwitchState = true;
    struct ZigbeeFrameCommand command;
    struct event_data data;
   
    xEventGroupSync( xEventBits,BIT_0,( BIT_0 | BIT_1 | BIT_2 | BIT_3), portMAX_DELAY );
    vTaskSuspend(ambientTaskHandle);
    vTaskSuspend(irTaskHandle);
    vTaskSuspend(motionTaskHandle);




    while (1) {
        xQueueReceive(sensor_queue, &data, portMAX_DELAY);
        switch (data.mode) {
        //Funcionality for the ambient light sensor
        case sensor_ambient:
            if (data.value < 500 && !lightSwitchState) {
                lightSwitchState = true;
                command.cmd = power; // turn light on
                command.cmdVal = 1;
                xQueueSendToBack(zigbeeFrameQueue, &(command), portMAX_DELAY);
            }
            if (data.value > 3500 && lightSwitchState) {
                lightSwitchState = false;
                command.cmd = power; // turn light on
                command.cmdVal = 0;
                xQueueSendToBack(zigbeeFrameQueue, &(command), portMAX_DELAY);
            }
           
            break;
        //Funcionality for the motion sensor.
        case sensor_motion:
            if (data.value == 1 && !lightSwitchState) {
                //light is off and motion sensor detects movement
                lightSwitchState = true;
                command.cmd = power; // turn light on
                command.cmdVal = 1;
                xQueueSendToBack(zigbeeFrameQueue, &(command), portMAX_DELAY);
            }
            else {
                //light is off or should be turned off
                if(lightSwitchState){ // light is on and the pir motions task has finished its timer
                    lightSwitchState = false;
                    command.cmd = power; // turn light on
                    command.cmdVal = 0;
                    xQueueSendToBack(zigbeeFrameQueue, &(command), portMAX_DELAY);
                }
            }
            break;
        //Funcionality for the ultrasonic sensor.
        case sensor_ultra:
            if (data.value == 1) { // if object is moved infront of ir sensor
                if(lightSwitchState){ // if on turn off, 
                    command.cmd = power; // turn light on
                    command.cmdVal = 0;
                }else{ // if off turn on
                    command.cmd = power; // turn light on
                    command.cmdVal = 1;
                }
                xQueueSendToBack(zigbeeFrameQueue, &(command), portMAX_DELAY);
                lightSwitchState = !lightSwitchState;
            }
            break;
        //Gets the color info from the website.
        //This is then used for changing the color of the light.
        case bt_color:
            command.cmd = RGB;
            command.REDvalue = data.red;
            command.GREENvalue = data.green;
            command.BLUEvalue = data.blue;
            
            xQueueSendToBack(zigbeeFrameQueue, &(command), portMAX_DELAY);
            break;
        //Gets the brightness info from the website.
        //This is then used for changing the brightness of the light.
        case bt_bright:
        if(data.value > 0) { // adjust brightness
            command.cmd =  luminocity;
            command.cmdVal = data.value;
            xQueueSendToBack(zigbeeFrameQueue, &(command), portMAX_DELAY);
        }else { // brightness if 0, turn light off / on
                lightSwitchState = !lightSwitchState;
                command.cmd = power; // turn light on
                command.cmdVal = (int)lightSwitchState;
                xQueueSendToBack(zigbeeFrameQueue, &(command), portMAX_DELAY);
        }
        
            break;
        //Gets the sensor activity data.
        //Then it send them to the sensor tasks.
        case bt_sett:
        // disable or enable task
            if(((data.value) & (1<<(0)))) {
                vTaskResume(ambientTaskHandle);
            }else{
                vTaskSuspend(ambientTaskHandle);
            }
            if(((data.value) & (1<<(1)))) {
                vTaskResume(irTaskHandle);
            }else{
                vTaskSuspend(irTaskHandle);
            }
            if(((data.value) & (1<<(2)))) {
                vTaskResume(motionTaskHandle);
            }else{
                vTaskSuspend(motionTaskHandle);
            }
            
            break;
        }
       
    }
}
void app_main(void)
{
    sensor_queue = xQueueCreate(40, sizeof(struct event_data));
    zigbeeFrameQueue = xQueueCreate(10, sizeof(struct ZigbeeFrameCommand));
    xEventBits = xEventGroupCreate();


    setupBLE(); // start BLE
    xTaskCreate(uartSender, "uart_Sender", 3*1024, NULL, 7, NULL); // sends zigbee frame over uart
    xTaskCreate(ultra_task, "ultra_task", 2*1048, NULL, 5, &irTaskHandle); 
    xTaskCreate(ambient_task, "ambient_task", 2*1048, NULL, 5, &ambientTaskHandle);
    xTaskCreate(motion_task, "motion_task", 2*1048, NULL, 5, &motionTaskHandle);
    xTaskCreate(controller_task, "controller_task", 3*1048, NULL, 6, NULL);

}