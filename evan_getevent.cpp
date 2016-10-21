#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <sys/poll.h>
#include <linux/input.h>
#include <errno.h>

#include "getevent.h"

static struct pollfd *ufds;
static char **device_names;
static int nfds;

enum {
    PRINT_DEVICE_ERRORS     = 1U << 0,
    PRINT_DEVICE            = 1U << 1,
    PRINT_DEVICE_NAME       = 1U << 2,
    PRINT_DEVICE_INFO       = 1U << 3,
    PRINT_VERSION           = 1U << 4,
    PRINT_POSSIBLE_EVENTS   = 1U << 5,
    PRINT_INPUT_PROPS       = 1U << 6,
    PRINT_HID_DESCRIPTOR    = 1U << 7,

    PRINT_ALL_INFO          = (1U << 8) - 1,

    PRINT_LABELS            = 1U << 16,
};

static const char *get_label(const struct label *labels, int value)
{
    while(labels->name && value != labels->value) {
        labels++;
    }
    return labels->name;
}

static void print_event(int type, int code, int value, int print_flags)
{
    const char *type_label, *code_label, *value_label;

    if (print_flags & PRINT_LABELS) {
        type_label = get_label(ev_labels, type);
        code_label = NULL;
        value_label = NULL;

        switch(type) {
//            case EV_SYN:
//                code_label = get_label(syn_labels, code);
//                break;
            case EV_KEY:
		code_label = get_label(key_labels, code);
                value_label = get_label(key_value_labels, value);
                break;
#if 0
            case EV_REL:
                code_label = get_label(rel_labels, code);
                break;
            case EV_ABS:
                code_label = get_label(abs_labels, code);
                switch(code) {
                    case ABS_MT_TOOL_TYPE:
                        value_label = get_label(mt_tool_labels, value);
                }
                break;
            case EV_MSC:
                code_label = get_label(msc_labels, code);
                break;
            case EV_LED:
                code_label = get_label(led_labels, code);
                break;
            case EV_SND:
                code_label = get_label(snd_labels, code);
                break;
            case EV_SW:
                code_label = get_label(sw_labels, code);
                break;
            case EV_REP:
                code_label = get_label(rep_labels, code);
                break;
            case EV_FF:
                code_label = get_label(ff_labels, code);
                break;
            case EV_FF_STATUS:
                code_label = get_label(ff_status_labels, code);
                break;
#endif        
	}
	if(type==EV_KEY)
	{
	        if (type_label)
       	           printf("%-12.12s", type_label);
 //       else
 //           printf("%04x        ", type);
                if (code_label)
                   printf(" %-20.20s", code_label);
 //       else
 //           printf(" %04x                ", code);
                if (value_label)
                   printf(" %-20.20s", value_label);
 //       else
 //           printf(" %08x            ", value);
    		printf("\n");
        }
    } else {
	printf("Evan here\n");
        printf("%04x %04x %08x", type, code, value);
    }
}


static int open_device(const char *device)
{
    int version;
    int fd;
    struct pollfd *new_ufds;
    char **new_device_names;
    char name[80];
    char location[80];
    char idstr[80];
    struct input_id id;

    fd = open(device, O_RDWR);
    if(fd < 0) {
            fprintf(stderr, "could not open %s\n", device);
        return -1;
    }
    new_ufds = (struct pollfd*)realloc(ufds, sizeof(ufds[0]) * (nfds + 1));
    if(new_ufds == NULL) {
        fprintf(stderr, "out of memory\n");
        return -1;
    }

    ufds = new_ufds;
	
    new_device_names = (char**)realloc(device_names, sizeof(device_names[0]) * (nfds + 1));
    if(new_device_names == NULL) {
        fprintf(stderr, "out of memory\n");
        return -1;
    }
    device_names = new_device_names;

    ufds[nfds].fd = fd;
    ufds[nfds].events = POLLIN;
    device_names[nfds] = strdup(device);
    nfds++;

    return 0;
}

int main ()
{
	int c;
	int i;
	int res;
	int pollres;
	int get_time = 0;
	int print_device = 0;
	char *newline = "\n";
	uint16_t get_switch = 0;
	struct input_event event;
	int version;
	int print_flags = 0;
	int print_flags_set = 0;
	int dont_block = -1;
	int event_count = 0;
	int sync_rate = 0;
	int64_t last_sync_time = 0;
	const char *device = NULL;
	const char *device_path = "/dev/input/event2";
	
	int gpio_fd_num=1;

	nfds = 1;
	ufds = (struct pollfd*)calloc(1, sizeof(ufds[0]));
	ufds[0].fd = inotify_init();
	ufds[0].events = POLLIN;
	
	device=device_path;

	res = open_device(device);
        
	if(res < 0) {
		return 1;
	}
	while(1) {
		pollres = poll(ufds, nfds, -1);
		if(ufds[gpio_fd_num].revents) {
                 if(ufds[gpio_fd_num].revents & POLLIN) {
                    res = read(ufds[gpio_fd_num].fd, &event, sizeof(event));
                    if(res < (int)sizeof(event)) {
                        fprintf(stderr, "could not get event\n");
                        return 1;
                    }
		
        //            printf("[%8ld.%06ld] ", event.time.tv_sec, event.time.tv_usec);
        //            printf("%s: ", device_names[gpio_fd_num]);
	//	    printf("evant.type=%d,event.code=%d,event.value=%d\n",event.type,event.code,event.value);
		    print_flags=PRINT_LABELS;
		    print_event(event.type, event.code, event.value, print_flags);
	//	    printf("\n");
                }
            }
        


	}	
	

	return 0;
}
