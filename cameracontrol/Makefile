CFLAGS=-O2 -Wall -g -I. -I/usr/local/include -I/usr/local/include/libusb-1.0 -I./include -I../
EXTRALDFLAGS=-L/usr/local/lib -L../

%.lo : %.c
	$(CC) -fPIC $(CFLAGS) -o $@ -c $<

cameracontrol: main.o log.o log_syslog.o cJSON.o websocket.o uvc.o
	$(CC) -o $@ $^ $(EXTRALDFLAGS) -lwebsockets -lm -lusb -lssl -lcrypto

all: cameracontrol

clean:
	rm -f *.o *.so *.lo *.so.1 *.so.1.0 cameracontrol

