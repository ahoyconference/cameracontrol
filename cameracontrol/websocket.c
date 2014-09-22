#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "libwebsockets.h"
#include "cJSON.h"
#include "log.h"
#include "websocket.h"
#include "uvc.h"

#define MAX_WEBSOCKET_CONTEXTS 32

struct per_session_data__cameracontrol {
	struct libwebsocket_context *context;
	struct libwebsocket *wsi;
};

static int websocket_is_shutdown = 0;
static int websocket_is_shutting_down = 0;
static int allow_new_connections = 1;

static int startsWith(char *string, char *pattern) {
    return !strncasecmp(string, pattern, strlen(pattern));
}

static 	struct libwebsocket_context *context;


static void build_mime_type(char *mime_type, int len, char *file_ext) {
    if (startsWith(file_ext, ".html")) {
	snprintf(mime_type, len, "text/html");
    } else if (startsWith(file_ext, ".htm")) {
	snprintf(mime_type, len, "text/html");
    } else if (startsWith(file_ext, ".gif")) {
	snprintf(mime_type, len, "image/gif");
    } else if (startsWith(file_ext, ".jpg")) {
	snprintf(mime_type, len, "image/jpeg");
    } else if (startsWith(file_ext, ".png")) {
	snprintf(mime_type, len, "image/png");
    } else if (startsWith(file_ext, ".css")) {
	snprintf(mime_type, len, "text/css");
    } else if (startsWith(file_ext, ".xml")) {
	snprintf(mime_type, len, "text/xml");
    } else if (startsWith(file_ext, ".js")) {
	snprintf(mime_type, len, "text/javascript");
    } else {
	snprintf(mime_type, len, "text/html");
    }
}


static int callback_http(struct libwebsocket_context * context,
		struct libwebsocket *wsi,
		enum libwebsocket_callback_reasons reason, void *user,
							   void *in, size_t len)
{
	char *tmp = NULL;
	char *file_ext = NULL;
	char file_name[1024];
	char new_file_name[1024];
	FILE *file;
	char *query_string = NULL;
	char mime_type[255] = "text/html";

	switch (reason) {
	case LWS_CALLBACK_HTTP:
		    // LOG_DEBUG("serving HTTP URI %s, len %d\n", (char *)in, len);
		    tmp = (char *)in;

		    if (!tmp) return -1;
	
		    query_string = strchr(tmp, '?');
		    if (query_string) {
			query_string[0] = '\0'; /* remove the query string from the uri */
		    }

		    if (strrchr(tmp, '/') == (tmp + strlen(tmp) - 1)) {
			snprintf(file_name, sizeof(file_name) - 1, "./htdocs/%sindex.html", tmp + 1);
		    } else {
			snprintf(file_name, sizeof(file_name) - 1, "./htdocs/%s", tmp + 1);
		        snprintf(new_file_name, sizeof(new_file_name) - 1, "%s/index.html", file_name);
			// the last '/' in the uri was omitted, but a corresponding index file is present
		        if ((file = fopen(new_file_name, "r"))) {
			    // replace requested uri with another having a '/index.html' suffix
			    strcpy(file_name, new_file_name);
			    fclose(file);
			}
		    }
		
		    file_ext = strrchr(tmp, '.');
		    if (file_ext) {
			build_mime_type(mime_type, sizeof(mime_type) - 1, file_ext);
		    }
	
		    LOG_DEVDEBUG("serving file_name '%s'\n", file_name);

		    if (libwebsockets_serve_http_file(context, wsi, file_name, mime_type, NULL)) {
			return -1;
		    }

	    break;

	case LWS_CALLBACK_HTTP_FILE_COMPLETION:
		return -1;
	    break;

	default:
		break;
	}

	return 0;
}

int send_json_cameras(struct libwebsocket *wsi, cJSON *cameras) {
    unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 32000 + LWS_SEND_BUFFER_POST_PADDING];
    char *json = NULL;
    int idx = 0;
    int res = 0;

    cJSON *root = cJSON_CreateObject();
    if (root) {
	if (cameras) {
	    cJSON_AddItemToObject(root, "cameras", cameras);
	}
	
	json = cJSON_Print(root);
	idx = sprintf((char *)&buf[LWS_SEND_BUFFER_PRE_PADDING + idx], "%s", json);
	LOG_VERBOSE("send_json_cameras: JSON '%s'\n", json);
	res = libwebsocket_write(wsi, buf + LWS_SEND_BUFFER_PRE_PADDING, idx, LWS_WRITE_TEXT);
	if (res == -1) {
	    LOG_DEBUG("send_json_cameras: libwebsocket_write returned %d. closing connection.\n", res);
	}
    }
    if (json) free (json);
    if (root) cJSON_Delete(root);
    return res;
}

int handle_json_message(struct per_session_data__cameracontrol *pss, struct libwebsocket *wsi, char *message) {
    int res = 0;
    cJSON *root = NULL;
    if (message) root = cJSON_Parse(message);

    LOG_DEBUG("handle_json_message: '%s'\n", message?message:"WS has been closed");
    if (root) {
	cJSON *control = cJSON_GetObjectItem(root, "control");
	cJSON *cameras = cJSON_GetObjectItem(root, "cameras");
	if (control) {
	    int camera_int = 0;
	    cJSON *camera = cJSON_GetObjectItem(control, "camera");
	    if (camera) {
		camera_int = camera->valueint;
	    }
	    cJSON *reset = cJSON_GetObjectItem(control, "reset");
	    cJSON *move = cJSON_GetObjectItem(control, "move");
	    cJSON *led = cJSON_GetObjectItem(control, "led");
	    cJSON *zoom = cJSON_GetObjectItem(control, "zoom");
	    if (reset) {
		res = uvc_reset(camera_int);
	    }
	    if (led) {
		cJSON *led_state = cJSON_GetObjectItem(led, "state");
		if (led_state && led_state->valuestring && strlen(led_state->valuestring)) {
		    int state = -1;
		    if (!strcasecmp("on", led_state->valuestring)) {
			state = UVC_LED_STATE_ON;
		    } else if (!strcasecmp("off", led_state->valuestring)) {
			state = UVC_LED_STATE_OFF;
		    } else if (!strcasecmp("blink", led_state->valuestring)) {
			state = UVC_LED_STATE_BLINK;
		    }
		    if (state >= 0) {
			uvc_led(camera_int, state, 77);
		    }
		}
	    }
	    if (move) {
		cJSON *left = cJSON_GetObjectItem(move, "left");
		cJSON *right = cJSON_GetObjectItem(move, "right");
		cJSON *down = cJSON_GetObjectItem(move, "down");
		cJSON *up = cJSON_GetObjectItem(move, "up");
		if (left) {
		    res = uvc_move(camera_int, -1, 0);
		} else if (right) {
		    res = uvc_move(camera_int, 1, 0);
		} else if (down) {
		    res = uvc_move(camera_int, 0, -1);
		} else if (up) {
		    res = uvc_move(camera_int, 0, 1);
		}
	    }
	    if (zoom) {
		cJSON *in = cJSON_GetObjectItem(zoom, "in");
		cJSON *out = cJSON_GetObjectItem(zoom, "out");
		if (in) {
		    res = uvc_zoom(camera_int, 1);
		} else if (out) {
		    res = uvc_zoom(camera_int, -1);
		}
	    }
	} else if (cameras) {
	    cJSON *cameras = uvc_get_cameras();
	    send_json_cameras(wsi, cameras);
	}
	cJSON_Delete(root);
    }

    return res;
}

int callback_cameracontrol(struct libwebsocket_context * context, struct libwebsocket *wsi, enum libwebsocket_callback_reasons reason, void *user, void *in, size_t len) {
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 2000 + LWS_SEND_BUFFER_POST_PADDING];
	struct per_session_data__cameracontrol *pss = user;
	int res = -1;
	int idx = 0;

	switch (reason) {
	    case LWS_CALLBACK_ESTABLISHED:
		if (!websocket_is_shutting_down) {
		    LOG_DEBUG("callback_cameracontrol: WS established.\n");
		    pss->wsi = wsi;
		    pss->context = context;
		} else {
		    char *error_message = "{\"messageType\":\"ERROR_indication\",\"error\":\"out_of_licenses\"}";
		    idx = sprintf((char *)&buf[LWS_SEND_BUFFER_PRE_PADDING], "%s", error_message);
		    libwebsocket_write(wsi, buf + LWS_SEND_BUFFER_PRE_PADDING, idx, LWS_WRITE_TEXT);

		    int fd = libwebsocket_get_socket_fd(wsi);
		    if (fd > 0) {
			close(fd);
		    }
		    return -1;
		}
		break;

	    case LWS_CALLBACK_RECEIVE:
		if (len > 0) {
		    res = handle_json_message(pss, wsi, (char *)in);
		    if (res != 0) {
		        /* destroy the websocket */
		        return -1;
		    }
		} else {
		    return -1;
		}
		break;

	    case LWS_CALLBACK_CLOSED:
		if (pss->wsi) {
		    LOG_DEBUG("callback_cameracontrol: WS has been closed\n");
		}
		break;

	    default:
		break;
	}

	return 0;
}



static struct libwebsocket_protocols protocols[] = {
    {
	"cameracontrol-protocol",
	callback_cameracontrol,
	sizeof(struct per_session_data__cameracontrol),
	256*1024,
    },
    {
	NULL, NULL, 0, 0
    }
};

int websocket_service(int ms) {
    if (context) {
        libwebsocket_service(context, ms);
    }
    return 0;
}


struct libwebsocket_extension libwebsocket_no_extensions[] = {
    {
	NULL, NULL, 0
    }
};

void websocket_log(int level, const char *line) {
    if (level == LLL_INFO) {
	LOG_VERBOSE((char *)line);
    } else  {
	LOG_DEBUG((char *)line);
    }
}

int websocket_init(int port) {
	struct lws_context_creation_info info;

	lws_set_log_level(LLL_ERR, websocket_log);

	memset(&info, 0, sizeof(info));
	info.port = port;
	info.protocols = protocols;
	info.iface = "lo";
	info.extensions = libwebsocket_no_extensions;

	LOG_VERBOSE("websocket_init: port %d\n", port);

	context = libwebsocket_create_context(&info);
	if (context == NULL) {
		lwsl_err("libwebsocket init failed\n");
		return -1;
	}



	return 0;
}

void websocket_disallow_new_connections() {
    allow_new_connections = 0;
}

void websocket_shutdown() {
    if (websocket_is_shutting_down) {
	return;
    }    
    LOG_DEBUG("websocket_shutdown: shutting down.\n");
    websocket_is_shutting_down = 1;

    if (context) {
	libwebsocket_context_destroy(context);
	context = NULL;
    }
    websocket_is_shutdown = 1;
}

int websocket_is_running(void) {
    return websocket_is_shutdown?0:1;
}
