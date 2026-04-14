#ifndef UI_MENU_H
#define UI_MENU_H

#include "sc_event_task.h"

void ui_menu_task      (sc_event_t *e);
void ui_brightness_task(sc_event_t *e);
void ui_alarm_task     (sc_event_t *e);
void ui_about_task     (sc_event_t *e);
void ui_debug_task     (sc_event_t *e);
void ui_contact_task   (sc_event_t *e);
void ui_comm_task      (sc_event_t *e);

#endif /* UI_MENU_H */
