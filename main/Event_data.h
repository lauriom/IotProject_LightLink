#ifndef EVENT_DATA_H
#define EVENT_DATA_H

typedef enum { sensor_ambient, sensor_motion, sensor_ultra, bt_color, bt_bright, bt_sett } MODE;

//event_data struct has three variables mode, value and rgb.
//Mode identifies the events type.
//Value is the number that sensors give.
//Rgb is color values that we get from bluetooth

xQueueHandle sensor_queue;
EventGroupHandle_t xEventBits;
#define BIT_0 (1<<0)
#define BIT_1 (1<<1)
#define BIT_2 (1<<2)
#define BIT_3 (1<<3)


struct event_data {
    MODE mode;
    int value;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};
#endif
