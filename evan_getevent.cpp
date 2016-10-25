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
static int state;
int key_num=5;
struct gpio_key
{
	int keycode;
	bool ispressed;	
	char *keyname;
};

static struct gpio_key *wistron_keys;

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

typedef int KEY;
struct  {
    KEY reset_btn;
    KEY video_slide;
    KEY picture_slide;
    KEY wifi_btn;
    KEY capture_btn;	
}LUCID_KEYS;

enum {
	init_mode,
	video_mode,
	picture_mode,
	off_mode,
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
            case EV_SYN:
                code_label = get_label(syn_labels, code);
                break;
            case EV_KEY:
		code_label = get_label(key_labels, code);
                value_label = get_label(key_value_labels, value);
                break;
#if 1
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
	       	    printf(" %-12.12s", type_label);
	        else
	            printf("%04x        ", type);
                if (code_label)
                    printf(" %-20.20s", code_label);
	        else
	            printf(" %04x                ", code);
                if (value_label)
                    printf(" %-20.20s", value_label);
	        else
        	    printf(" %08x            ", value);
    		printf("\n");
        }
    } else {
        printf("%04x %04x %08x", type, code, value);
    }
}

static int print_possible_events(int fd, int print_flags)
{
    struct gpio_key *wistron_keys_new;
    wistron_keys_new=(struct gpio_key*)calloc(5,sizeof(gpio_key[5]));
    char *state;    
 
    uint8_t *bits = NULL;
    ssize_t bits_size = 0;
    const char* label;
    int i, j, k;
    int res, res2;
    struct label* bit_labels;
    const char *bit_label;
    for(i = EV_KEY; i <= EV_MAX; i++) { // skip EV_SYN since we cannot query its available codes
        int count = 0;
        while(1) {
            res = ioctl(fd, EVIOCGBIT(i, bits_size), bits);
            if(res < bits_size)
                break;
            bits_size = res + 16;
            bits = (unsigned char*)realloc(bits, bits_size * 2);
            if(bits == NULL) {
                fprintf(stderr, "failed to allocate buffer of size %d\n", (int)bits_size);
                return 1;
            }
        }
        res2 = 0;

        switch(i) {
            case EV_KEY:
                res2 = ioctl(fd, EVIOCGKEY(res), bits + bits_size);
                label = "KEY";
                bit_labels = key_labels;
                break;
            case EV_REL:
                label = "REL";
                bit_labels = rel_labels;
                break;
            case EV_ABS:
                label = "ABS";
                bit_labels = abs_labels;
                break;
            case EV_MSC:
                label = "MSC";
                bit_labels = msc_labels;
                break;
            case EV_LED:
                res2 = ioctl(fd, EVIOCGLED(res), bits + bits_size);
                label = "LED";
                bit_labels = led_labels;
                break;
            case EV_SND:
                res2 = ioctl(fd, EVIOCGSND(res), bits + bits_size);
                label = "SND";
                bit_labels = snd_labels;
                break;
            case EV_SW:
                res2 = ioctl(fd, EVIOCGSW(bits_size), bits + bits_size);
                label = "SW ";
                bit_labels = sw_labels;
                break;
            case EV_REP:
                label = "REP";
                bit_labels = rep_labels;
                break;
            case EV_FF:
                label = "FF ";
                bit_labels = ff_labels;
                break;
            case EV_PWR:
                label = "PWR";
                bit_labels = NULL;
                break;
            case EV_FF_STATUS:
                label = "FFS";
                bit_labels = ff_status_labels;
                break;
            default:
                res2 = 0;
                label = "???";
                bit_labels = NULL;
        }
//	printf("res=%d\n",res);
        for(j = 0; j < res; j++) {
            for(k = 0; k < 8; k++)
                if(bits[j] & 1 << k) {
                    char down;
                    if(j < res2 && (bits[j + bits_size] & 1 << k))
                    {
			down = '*';
			wistron_keys_new[count].ispressed=true;
		    }
                    else
		    {
			wistron_keys_new[count].ispressed=false;
                        down = ' ';
		    }
                    if(count == 0)
                        printf("    %s (%04x):", label, i);
                    else if((count & (print_flags & PRINT_LABELS ? 0x3 : 0x7)) == 0 || i == EV_ABS)
                        printf("\n               ");
                    if(bit_labels && (print_flags & PRINT_LABELS)) {
                        bit_label = get_label(bit_labels, j * 8 + k);
                        if(bit_label)
                            printf(" %.20s%c%*s", bit_label, down, 20 - strlen(bit_label), "");
			else
                            printf(" %04x%c                ", j * 8 + k, down);
                    } else {
			wistron_keys_new[count].keycode=j*8+k;
                        printf(" %04x%c", j * 8 + k, down);
                    }
                    if(i == EV_ABS) {
                        struct input_absinfo abs;
                        if(ioctl(fd, EVIOCGABS(j * 8 + k), &abs) == 0) {
                            printf(" : value %d, min %d, max %d, fuzz %d, flat %d, resolution %d",
                                abs.value, abs.minimum, abs.maximum, abs.fuzz, abs.flat,
                                abs.resolution);
                        }
                    }
                    count++;
                }

        }
        if(count)
            printf("\n");
    }
    wistron_keys=wistron_keys_new;
    free(bits);
    return 0;
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
    int print_flags=0;

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

//print current key states.
    print_flags |= PRINT_ALL_INFO;
    print_possible_events(fd,print_flags);


    return 0;
}

void show_current_state()
{
    struct gpio_key *wistron_keys_new;
    wistron_keys_new=(struct gpio_key*)calloc(5,sizeof(gpio_key[5]));
	wistron_keys_new=wistron_keys;
 	if(wistron_keys_new[LUCID_KEYS.picture_slide].ispressed==false && \
        wistron_keys_new[LUCID_KEYS.video_slide].ispressed==false)
        {
		printf("init state\n");
                state=init_mode;
        }else if(wistron_keys_new[LUCID_KEYS.picture_slide].ispressed==false && \
                wistron_keys_new[LUCID_KEYS.video_slide].ispressed==true)
        {
                printf("video state\n");
                state=video_mode;
        }else if(wistron_keys_new[LUCID_KEYS.picture_slide].ispressed==true && \
                wistron_keys_new[LUCID_KEYS.video_slide].ispressed==false)
        {
                printf("picture state\n");
                state=picture_mode;
        }

}
void set_press(int keycode,int value)
{
        struct gpio_key *wistron_keys_new;
        wistron_keys_new=(struct gpio_key*)calloc(5,sizeof(gpio_key[5]));

	switch (keycode)
	{
		case KEY_F13:
			wistron_keys_new[LUCID_KEYS.video_slide].ispressed=value;
			break;
		case KEY_F14:
			wistron_keys_new[LUCID_KEYS.picture_slide].ispressed=value;
			break;
		case KEY_F15:
			wistron_keys_new[LUCID_KEYS.capture_btn].ispressed=value;
			break;
		case KEY_F16:
			wistron_keys_new[LUCID_KEYS.wifi_btn].ispressed=value;
			break;
		case KEY_F17:
			wistron_keys_new[LUCID_KEYS.wifi_btn].ispressed=value;
			break;
         }
	 wistron_keys=wistron_keys_new;

}
void init_slide_state()
{
#if 1
    struct gpio_key *wistron_keys_new;
    wistron_keys_new=(struct gpio_key*)calloc(5,sizeof(gpio_key[5]));
    wistron_keys_new=wistron_keys;
    int key_num=5;
//	printf("init slide state %d\n",wistron_keys[0].keycode);
//    	wistron_keys_new[0].keycode=999;
//	wistron_keys=wistron_keys_new;
       for(int key=0;key<key_num;key++)
        {
                switch (wistron_keys[key].keycode)
                {
                        case KEY_F13:
                                wistron_keys_new[key].keyname="video_mode";
                                LUCID_KEYS.video_slide=key;
                                break;
                        case KEY_F14:
                                wistron_keys_new[key].keyname="picture_mode";
                                LUCID_KEYS.picture_slide=key;
                                break;
                        case KEY_F15:
                                wistron_keys_new[key].keyname="capture";
                                LUCID_KEYS.capture_btn=key;
                                break;
                        case KEY_F16:
                                wistron_keys_new[key].keyname="wifi";
                                LUCID_KEYS.wifi_btn=key;
                                break;
                        case KEY_F17:
                                wistron_keys_new[key].keyname="reset";
                                LUCID_KEYS.reset_btn=key;
                                break;
                }
        }
    wistron_keys=wistron_keys_new;
#endif
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
	int print_flags = PRINT_LABELS;
	int print_flags_set = 0;
	int dont_block = -1;
	int event_count = 0;
	int sync_rate = 0;
	int64_t last_sync_time = 0;
	const char *device = NULL;
	const char *device_path = "/dev/input/event4";
	
	int gpio_fd_num=1;

	
	wistron_keys=(struct gpio_key*)calloc(5,sizeof(gpio_key[0]));	
	
	nfds = 1;
	ufds = (struct pollfd*)calloc(1, sizeof(ufds[0]));
	ufds[0].fd = inotify_init();
	ufds[0].events = POLLIN;
	
	device=device_path;

	res = open_device(device);
	if(res < 0) {
		return 1;
	}

	    int key_num=5;
#if 0
       for(int key=0;key<key_num;key++)
        {
                switch (wistron_keys[key].keycode)
                {
                        case KEY_F13:
                                wistron_keys[key].keyname="video_mode";
                                LUCID_KEYS.video_slide=key;
                                break;
                        case KEY_F14:
                                wistron_keys[key].keyname="picture_mode";
                                LUCID_KEYS.picture_slide=key;
                                break;
                        case KEY_F15:
                                wistron_keys[key].keyname="capture";
                                LUCID_KEYS.capture_btn=key;
                                break;
                        case KEY_F16:
                                wistron_keys[key].keyname="wifi";
                                LUCID_KEYS.wifi_btn=key;
                                break;
                        case KEY_F17:
                                wistron_keys[key].keyname="reset";
                                LUCID_KEYS.reset_btn=key;
                                break;
                }
        }
#endif
	init_slide_state();
	show_current_state();
#if 0	
	if(wistron_keys[LUCID_KEYS.picture_slide].ispressed==false && \
	wistron_keys[LUCID_KEYS.video_slide].ispressed==false)
	{
		printf("init state\n");
		state=init_mode;
	}else if(wistron_keys[LUCID_KEYS.picture_slide].ispressed==false && \
		wistron_keys[LUCID_KEYS.video_slide].ispressed==true)
	{
		printf("video state\n");
		state=video_mode;
	}else if(wistron_keys[LUCID_KEYS.picture_slide].ispressed==true && \
		wistron_keys[LUCID_KEYS.video_slide].ispressed==false)
        {
                printf("picture state\n");
		state=picture_mode;
        }
#endif
printf("wistron_keys[%d].keycode=%d,is_pressed=%d,name=%s\n",LUCID_KEYS.video_slide,wistron_keys[LUCID_KEYS.video_slide].keycode,wistron_keys[LUCID_KEYS.video_slide].ispressed,wistron_keys[LUCID_KEYS.video_slide].keyname);
printf("wistron_keys[%d].keycode=%d,is_pressed=%d,name=%s\n",LUCID_KEYS.picture_slide,wistron_keys[LUCID_KEYS.picture_slide].keycode,wistron_keys[LUCID_KEYS.picture_slide].ispressed,wistron_keys[LUCID_KEYS.picture_slide].keyname);
	while(1) {
		pollres = poll(ufds, nfds, -1);
		if(ufds[gpio_fd_num].revents) {
                 if(ufds[gpio_fd_num].revents & POLLIN) {
                    res = read(ufds[gpio_fd_num].fd, &event, sizeof(event));
                    if(res < (int)sizeof(event)) {
                        fprintf(stderr, "could not get event\n");
                        return 1;
                    }
		    if(event.type==EV_KEY )
		    {
		    	set_press(event.code,event.value);
			if(event.code==KEY_F13 || event.code==KEY_F14)
		    		show_current_state();		 
			else
				print_event(event.type, event.code, event.value, print_flags);
		    }

                }
            }
        


	}	
	//       printf("wistron_keys[%d].keycode=%d,is_pressed=%d,name=%s\n",key,wistron_keys[key].keycode,wistron_keys[key].ispressed,wistron_keys[key].keyname);
	return 0;
}
