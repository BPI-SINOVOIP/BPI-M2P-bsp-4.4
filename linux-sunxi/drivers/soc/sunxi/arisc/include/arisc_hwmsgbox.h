#ifndef __ARISC_HWMSGBOX_H__
#define __ARISC_HWMSGBOX_H__

#include <linux/arisc/arisc.h>

int arisc_hwmsgbox_init(void);
irqreturn_t arisc_hwmsgbox_int_handler(int irq, void *dev);
int arisc_hwmsgbox_send_message(struct arisc_message *pmessage, unsigned int timeout);
int arisc_hwmsgbox_standby_resume(void);
int arisc_hwmsgbox_standby_suspend(void);
int arisc_hwmsgbox_message_feedback(struct arisc_message *pmessage);
int arisc_hwmsgbox_wait_message_feedback(struct arisc_message *pmessage);
struct arisc_message *arisc_hwmsgbox_query_message(void);
int arisc_hwmsgbox_clear_receiver_pending(int queue, int user);
int arisc_hwmsgbox_query_receiver_pending(int queue, int user);
int arisc_hwmsgbox_disable_receiver_int(int queue, int user);
int arisc_hwmsgbox_enable_receiver_int(int queue, int user);
int arisc_hwmsgbox_feedback_message(struct arisc_message *pmessage, unsigned int timeout);
int arisc_hwmsgbox_exit(void);

#endif
