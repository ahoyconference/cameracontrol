#ifndef _WEBSOCKET_H
#define _WEBSOCKET_H

extern int websocket_init(int port);
extern int websocket_service(int ms);
extern void websocket_shutdown();
extern int websocket_is_running(void);

#endif