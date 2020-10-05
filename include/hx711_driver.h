#ifndef HX711_DRIVER_H
#define HX711_DRIVER_H

void initialize_hx711();
void mass_read_task(void* pvParameters);
double get_weight();
void tare(int iterations);

#endif
