#ifndef SMBUS_H
#define SMBUS_H

int smbus_init(void);
void battery_poll(void);

extern int battery_present;
extern int battery_percent;
extern int battery_charging;

#endif
