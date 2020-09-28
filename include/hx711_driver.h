#ifndef HX711_DRIVER_H
#define HX711_DRIVER_H

void initialize_hx711();
void sck_task(void* pvParameters);
double get_weight();
void tare();

#endif
