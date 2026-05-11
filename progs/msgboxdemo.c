#include "../lunarsnow.h"

extern void msgbox(const char *title, const char *msg);

void prog_msgbox_demo(void)
{
    msgbox("Message", "Hello, World!");
}
