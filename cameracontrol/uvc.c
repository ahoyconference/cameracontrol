#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>

#include "libusb.h"
#include "cJSON.h"
#include "log.h"
#include "uvc.h"

typedef struct camera_info {
    int id;
    uint16_t vendor_id;
    uint16_t product_id;
    int type;
    char *name;
    void (*reset)(struct camera_info *camera, int pan, int tilt);
    void (*move)(struct camera_info *camera, int x, int y);
    void (*led)(struct camera_info *camera, int state, int freq);
    void (*zoom)(struct camera_info *camera, int zoom);
} camera_info_t;


static camera_info_t *cameras[UVC_MAX_CAMERAS];
static int camera_count = 0;

cJSON *camera_to_json(camera_info_t *camera) {
    cJSON *root = cJSON_CreateObject();
    if (root) {
        cJSON_AddItemToObject(root, "id", cJSON_CreateNumber((double)camera->id));
        cJSON_AddItemToObject(root, "vendor_id", cJSON_CreateNumber((double)camera->vendor_id));
        cJSON_AddItemToObject(root, "product_id", cJSON_CreateNumber((double)camera->product_id));
        cJSON_AddItemToObject(root, "name", cJSON_CreateString(camera->name));
    }
    return root;
}

cJSON *uvc_get_cameras(void) {
    int i = 0;
    cJSON *cameras_json = NULL;
    cJSON *camera_json = NULL;

    cameras_json = cJSON_CreateArray();
    for (i = 0; i < camera_count; i++) {
        if (cameras[i]) {
    	    camera_json = camera_to_json(cameras[i]);
    	    if (camera_json) {
                cJSON_AddItemToArray(cameras_json, camera_json);
    	    }
        }
    }

    return cameras_json;
}

struct libusb_device_handle *uvc_claim_device(camera_info_t *camera) {
    libusb_device_handle *dh = NULL;
    dh = libusb_open_device_with_vid_pid(NULL, camera->vendor_id, camera->product_id);
    if (dh) {
        libusb_claim_interface(dh, 0);
    }
    return dh;
}

void uvc_release_device(struct libusb_device_handle *dh) {
    if (dh) {
        libusb_release_interface(dh, 0);
        libusb_close(dh);
    }
}

int uvc_send_control(camera_info_t *camera, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t *data, int data_len, int timeout) {
    libusb_device_handle *dh = NULL;
    int res = 0;

    dh = uvc_claim_device(camera);

    if (dh) {
        libusb_control_transfer(dh, bmRequestType, bRequest, wValue , wIndex, data, data_len, timeout);
        uvc_release_device(dh);
    }
    return res;
}

int uvc_send_control_noclaim(libusb_device_handle *dh, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, uint8_t *data, int data_len, int timeout) {
    int res = 0;
    
    
    if (dh) {
        libusb_control_transfer(dh, bmRequestType, bRequest, wValue , wIndex, data, data_len, timeout);
    }
    return res;
}

void uvc_pan_tilt_rel(libusb_device_handle *dh, uint8_t pan_direction, uint8_t pan_speed, uint8_t tilt_direction, uint8_t tilt_speed) {
    uint8_t data[4];

    data[0] = pan_direction;
    data[1] = pan_speed;
    data[2] = tilt_direction;
    data[3] = tilt_speed;

    uvc_send_control_noclaim(dh, 0x21, 0x01, 0x0e << 8 , 1 << 8, data, sizeof(data), 0);
}

void uvc_pan_tilt_abs(camera_info_t *camera, uint8_t pan_left, uint8_t pan_right, uint8_t tilt_down, uint8_t tilt_up) {
    uint8_t data[4];

    data[0] = pan_left;
    data[1] = pan_right;
    data[2] = tilt_down;
    data[3] = tilt_up;

    uvc_send_control(camera, 0x21, 0x01, 0x01 << 8 , 9 << 8, data, sizeof(data), 0);
}


// USB video calls pan/tilt relative specific functions (Logitech BCC950, CC3000E)

void uvc_move_rel(struct camera_info *camera, int x, int y) {
    libusb_device_handle *dh = NULL;
    int pan_direction = 0x00;
    if (x == 1) pan_direction = 0x01;
    if (x == -1) pan_direction = 0xff;
    int pan_speed = 1;

    int tilt_direction = 0x00;
    if (y == -1) tilt_direction = 0xff;
    if (y == 1) tilt_direction = 0x01;
    int tilt_speed = 1;

    dh = uvc_claim_device(camera);
    
    if (dh) {
        
        uvc_pan_tilt_rel(dh, pan_direction, pan_speed, tilt_direction, tilt_speed);
        if (tilt_direction == 0) {
            usleep(30 * 1000);
        } else {
            usleep(60 * 1000);
        }
        uvc_pan_tilt_rel(dh, 0, 0, 0, 0);
    
        uvc_release_device(dh);

    }
}

// Logitech Orbit Sphere AF specific functions

static void logitech_sphere_led(struct camera_info *camera, int state, int freq) {
    uint8_t data[3];
    if (state == UVC_LED_STATE_OFF) {
        data[0] = 0x00;
        data[1] = 0x95;
        data[2] = 0x00;
    } else if (state == UVC_LED_STATE_ON) {
        data[0] = 0x01;
        data[1] = 0x95;
        data[2] = 0x00;
    } else if (state == UVC_LED_STATE_BLINK) {
        data[0] = 0x02;
        data[1] = 0x95;
        data[2] = (uint8_t)freq;
    }

    uvc_send_control(camera, 0x21, 0x01, 0x01 << 8 , 0x0d << 8, data, sizeof(data), 0);
}

static void logitech_sphere_reset(struct camera_info *camera, int pan, int tilt) {
    uint8_t data[1];

    data[0] = 0x00;
    if (pan) {
        data[0] |= 0x1;
    }
    if (tilt) {
        data[0] |= 0x2;
    }
    uvc_send_control(camera, 0x21, 0x01, 0x02 << 8 , 9 << 8, data, sizeof(data), 0);
}

static void logitech_sphere_move(struct camera_info *camera, int x, int y) {
    int pan_left = (x == -1?0xff:0x00);
    int pan_right = (x == 1?0xff:0x00);
    int pan_down = (y == -1?0xff:0x00);
    int pan_up = (y == 1?0xff:0x00);

    uvc_pan_tilt_abs(camera, pan_left, pan_right, pan_down, pan_up);
}

// reset pan/tilt
int uvc_reset(int cam) {
    if (cameras[cam]) {
        if (cameras[cam]->reset) {
	    cameras[cam]->reset(cameras[cam], 1, 1);
            return 0;
        }
        return 0;
    } 
    return 1;
}


// led control
int uvc_led(int cam, int state, int freq) {
    if (cameras[cam]) {
	if (cameras[cam]->led) {
	    cameras[cam]->led(cameras[cam], state, freq);
	    return 0;
	}
    } 
    return 1;
}

// move one step
int uvc_move(int cam, int x, int y) {
    if (cameras[cam]) {
        if (cameras[cam]->move) {
            cameras[cam]->move(cameras[cam], x, y);
            return 0;
        }
    } 
    return 1;
}

// zoom one step
int uvc_zoom(int cam, int zoom) {
    if (cameras[cam]) {
        if (cameras[cam]->zoom) {
            return 0;
        }
    } 
    return 1;
}


// find all supported cameras
void uvc_find_devices() {
    libusb_device_handle *dh = NULL;

    // Logitech Orbit Sphere AF
    dh = libusb_open_device_with_vid_pid(NULL, 0x046d, 0x0994);
    if (dh) {
        cameras[camera_count] = malloc(sizeof(camera_info_t));
        if (cameras[camera_count]) {
            memset(cameras[camera_count], 0, sizeof(camera_info_t));
            cameras[camera_count]->vendor_id = 0x046d;
            cameras[camera_count]->product_id = 0x0994;
            cameras[camera_count]->type = UVC_CAMERA_LOGITECH_SPHERE_AF;
            cameras[camera_count]->name = strdup("Logitech Orbit Sphere AF");
            cameras[camera_count]->id = camera_count;
            cameras[camera_count]->reset = logitech_sphere_reset;
            cameras[camera_count]->move = logitech_sphere_move;
            cameras[camera_count]->led = logitech_sphere_led;
            cameras[camera_count]->zoom = NULL;
            camera_count++;
        }
        libusb_close(dh);
    }

    // Logitech BCC950 Conference Cam
    dh = libusb_open_device_with_vid_pid(NULL, 0x046d, 0x0837);
    if (dh) {
        cameras[camera_count] = malloc(sizeof(camera_info_t));
        if (cameras[camera_count]) {
            memset(cameras[camera_count], 0, sizeof(camera_info_t));
            cameras[camera_count]->vendor_id = 0x046d;
            cameras[camera_count]->product_id = 0x0837;
            cameras[camera_count]->type = UVC_CAMERA_LOGITECH_BCC950;
            cameras[camera_count]->name = strdup("Logitech BCC950 Conference Cam");
            cameras[camera_count]->id = camera_count;
            cameras[camera_count]->reset = NULL;
            cameras[camera_count]->led = NULL;
            cameras[camera_count]->move = uvc_move_rel;
            cameras[camera_count]->zoom = NULL;
            camera_count++;
        }
        libusb_close(dh);
    } 

    // Logitech CC3000E
    dh = libusb_open_device_with_vid_pid(NULL, 0x046d, 0x0845);
    if (dh) {
        cameras[camera_count] = malloc(sizeof(camera_info_t));
        if (cameras[camera_count]) {
            memset(cameras[camera_count], 0, sizeof(camera_info_t));
            cameras[camera_count]->vendor_id = 0x046d;
            cameras[camera_count]->product_id = 0x0845;
            cameras[camera_count]->type = UVC_CAMERA_LOGITECH_BCC950;
            cameras[camera_count]->name = strdup("Logitech CC3000E");
            cameras[camera_count]->id = camera_count;
            cameras[camera_count]->reset = NULL;
            cameras[camera_count]->led = NULL;
            cameras[camera_count]->move = uvc_move_rel;
            cameras[camera_count]->zoom = NULL;
            camera_count++;
        }
        libusb_close(dh);
    } 
}

int uvc_init() {
    int i = 0;
    LOG_DEBUG("uvc_init: initializing libusb\n");

    if (libusb_init(NULL)) {
        LOG_ERROR("uvc_init: unable to initialize libusb.\n");
        return -1;
    }
    
    uvc_find_devices();

    if (camera_count) {
        LOG_VERBOSE("uvc_init: found %d supported cameras.\n", camera_count);
        for (i = 0; i < camera_count; i++) {
            uvc_move(i, -1 , 0);
            uvc_move(i, 1 , 0);
            uvc_move(i, 1 , 0);
            uvc_move(i, -1 , 0);
            uvc_move(i, 0 , 1);
            uvc_move(i, 0 , -1);
            uvc_move(i, 0 , -1);
            uvc_move(i, 0 , 1);
        }
    } else {
	LOG_ERROR("uvc_init: unable to find a supported camera.\n");
    }
    return 0;
}

void uvc_shutdown() {
    LOG_DEBUG("uvc_shutdown: shutting down libusb\n");
    int i = 0;
    for (i = 0; i < camera_count; i ++) {
        if (cameras[i]) {
            if (cameras[i]->name) free(cameras[i]->name);
            free(cameras[i]);
        }
    }
    libusb_exit(NULL);
}
