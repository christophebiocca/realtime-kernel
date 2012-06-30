#ifndef USER_SENSOR_H
#define USER_SENSOR_H   1

void sensorInit(void);
void sensorQuit(void);

void sensorInterrupt(int train_number, int sensor, int sensor_number);

#endif /* USER_SENSOR_H */
