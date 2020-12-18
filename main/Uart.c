/* UART Echo Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"


/**
 * This is an example which echos any data it receives on configured UART back to the sender,
 * with hardware flow control turned off. It does not use UART driver event queue.
 *
 * - Port: configured UART
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below (See Kconfig)
 */
QueueHandle_t zigbeeFrameQueue = NULL;

#define ECHO_TEST_TXD 1
#define ECHO_TEST_RXD 3

#define ECHO_UART_PORT_NUM 0
#define ECHO_UART_BAUD_RATE 115200
#define ECHO_TASK_STACK_SIZE (CONFIG_EXAMPLE_TASK_STACK_SIZE)

#define BUF_SIZE (1024)

enum command
{
    power,
    luminocity,
    RGB
};
struct ZigbeeFrameCommand
{
    enum command cmd;
    uint8_t cmdVal;
    uint8_t REDvalue;
    uint8_t GREENvalue;
    uint8_t BLUEvalue;
};

static void uartSender(void *arg)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    // ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));


    // Zigbee frame for light toggle
    uint8_t toggler[] = {0x7E, 0x00, 0x19, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
                         0xFE, 0xE8, 0x01, 0x00, 0x06, 0x01, 0x04, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x10, 0x00};
    // frame for brightness adjustment
    uint8_t brightness[] = {0x7E, 0x00, 0x1B, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0xE8,
                            0x01, 0x00, 0x08, 0x01, 0x04, 0x00, 0x00, 0x01, 0x00, 0x04, 0x41, 0x10, 0x00, 0x10, 0x00};
    // frame for color adjustment
    uint8_t colour[] = {0x7E, 0x00, 0x1F, 0x11, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFE, 0xE8, 0x01,
                        0x03, 0x00, 0x01, 0x04, 0x00, 0x00, 0x01, 0x00, 0x07, 0xAF, 0x00, 0xCF, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00};

    int togglerSize = sizeof(toggler) / sizeof(toggler[0]) - 1;
    int brightnesSize = sizeof(brightness) / sizeof(brightness[0]) - 1;
    int colourSize = sizeof(colour) / sizeof(colour[0]) - 1;

    struct ZigbeeFrameCommand frameCmd;
    float cieX;
    float cieY;
    float cieZ;
    while (1)
    {
        // executes commands, builds and sends zigbee frame over uart pins 1 tx & 3 rx
        xQueueReceive(zigbeeFrameQueue, &(frameCmd), portMAX_DELAY);

        int value = 0;
        int i = 3;
        switch (frameCmd.cmd)
        {
        case power:
            
            // first 3 values are ignored, and last value is checksum
            if (frameCmd.cmdVal > 0){
                toggler[25] = 0x01; // on
            }
            else{
                toggler[25] = 0x00; //off
            }

            for (; i < togglerSize; i++)
            {
                value += toggler[i];
            } // i++ 
            toggler[i] = 0xFF - (uint8_t) value; // set checksum
            uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)toggler, togglerSize+1);
            break;
        case luminocity: // brightness was taken

            brightness[26] = frameCmd.cmdVal;

            for (; i < brightnesSize; i++)
            {
                value += brightness[i];
                
            }
            brightness[i] = 0xFF - (uint8_t)value; // set checksum
           
            uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)brightness, brightnesSize+1);
            break;
        case RGB:
            // calculates rgb hex into cie xy color
            cieX = 0.4123866 * (frameCmd.REDvalue/255.0) + 0.3575915 * (frameCmd.GREENvalue/255.0) + 0.1804505 * (frameCmd.BLUEvalue/255.0);
            cieY = 0.2126368 * (frameCmd.REDvalue/255.0) + 0.7151830 * (frameCmd.GREENvalue/255.0) + 0.0721802 * (frameCmd.BLUEvalue/255.0);
            cieZ = 0.0193306 * (frameCmd.REDvalue/255.0) + 0.1191972 * (frameCmd.GREENvalue/255.0) + 0.9503726 * (frameCmd.BLUEvalue/255.0);

            uint16_t holder1 = ( cieX / (cieX + cieY + cieZ)*255);
            uint16_t holder2 = ( cieY / (cieX + cieY + cieZ)*255);
            colour[26] = (holder1 >> 8); 
            colour[27] = holder1 & 0xff;
            colour[28] = (holder2 >> 8);
            colour[29] = holder2 & 0xff; 

            for (; i < colourSize; i++)
            {
                value += colour[i];
            }
            colour[i] = 0xFF - (uint8_t)value; // set checksum
            uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)colour, colourSize+1);
            break;

        default:
            break;
        }
    }
}
