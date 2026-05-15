#ifndef APPS_H
#define APPS_H

void app_close(void);
void cb_term(void);
void cb_calc(void);
void cb_about(void);
void cb_reboot(void);
void cb_shutdown(void);
void msgbox(const char *title, const char *msg);

#endif
