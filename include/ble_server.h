#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include "main_util.h"
#include "queue.h"
#include <stdint.h>

#define DIAGNOSTIC_TASKS_STACK_SIZE configMINIMAL_STACK_SIZE*10

#define ADC_PAYLOAD_LENGTH 20
#define MAX_TEXT_PAYLOAD_LENGTH 500
#define STATE_PAYLOAD_LENGTH 500

enum diagnostic_tasks {NONE, TEXT, ADC, STATE, STATS, COMMAND, COMMAND_SR, NETWORK, CALIBRATE};

int task_manager(struct Data_Queues* data_queues);

int text_task_stdout_redirect(void* c,const char* data,int size);

int text_payload_get_current_index();
void clear_text_payload();

void add_state(uint8_t state);

struct adc_args
{
    char* payload;
    int payload_size;
    QueueHandle_t* adc_queue;

    enum diagnostic_tasks* active_task; //TODO: Remove Unused
};

struct text_args
{
    char* payload;
    int payload_size;
    QueueHandle_t* text_queue;
    bool* ack;

    enum diagnostic_tasks* active_task; //TODO: Remove unused
};

struct stats_args
{
    char* payload;
    int payload_size;

    //this ack tells the stats task which data was received.
    //0x41 = task names received
    //0x42 = runtime received
    //0x43 = memory usage received
    int* ack; 
};

struct cmd_sr_args
{
    QueueHandle_t* adc_queue;
    char* result;
};

#endif
