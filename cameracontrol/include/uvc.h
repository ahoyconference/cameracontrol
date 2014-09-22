#ifndef _UVC_H
#define _UVC_H

extern int uvc_init(void);
extern void uvc_shutdown(void);
extern int uvc_reset(int cam);
extern int uvc_move(int cam, int x, int y);
extern int uvc_led(int cam, int state, int freq);
extern int uvc_zoom(int cam, int direction);
extern cJSON *uvc_get_cameras(void);

#define UVC_CAMERA_LOGITECH_SPHERE_AF	1
#define UVC_CAMERA_LOGITECH_BCC950	2
#define UVC_CAMERA_LOGITECH_CC3000E	3

#define UVC_LED_STATE_OFF	0
#define UVC_LED_STATE_ON	1
#define UVC_LED_STATE_BLINK	2

#define UVC_MAX_CAMERAS 8

#endif