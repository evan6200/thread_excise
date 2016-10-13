#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <iostream>
#include <sys/time.h>

#include <string.h>
#include <fcntl.h>

#include <unistd.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <stdlib.h>


#include <string.h>
#include <fcntl.h>
#include <sys/time.h>

#include <errno.h>
#include "power_supply.h"

void *print_message_function( void *ptr );
void write_trig(char *trig);

struct ply
{
	int a;
};

bool check_usb_online()
{
        //USB_ONLINE_PATH
         int led_fd=0;
        int ret=0;
        char buf[128];
        printf("USB_ONLINE_PATH %s \n", USB_ONLINE_PATH);
        led_fd=open(USB_ONLINE_PATH,O_RDONLY);
//	led_fd=open(DRY_ONLINE_PATH,O_RDONLY);  
      if (led_fd>0)
        {
                ret=read(led_fd,buf,5);
                if (ret==-1)
                {
                        printf("read ret = %d\n",ret);
                        printf("print read string  %x\n",buf);

                }else
		{
			printf("read ret = %d\n",ret);
			printf("print read string  %c %x\n",buf[0],buf[1]);
				printf("this is digit %s\n",buf);
				printf("atoi value %d\n",atoi(buf) );
			
		}
        }
         close(led_fd);

	printf("BATTERY_CAPACITY_PATH %s \n", BATTERY_CAPACITY_PATH);
        led_fd=open(BATTERY_CAPACITY_PATH,O_RDONLY);
//      led_fd=open(DRY_ONLINE_PATH,O_RDONLY);  
      if (led_fd>0)
        {
                ret=read(led_fd,buf,5);
                if (ret==-1)
                {
                        printf("read ret = %d\n",ret);
                        printf("print read string  %x\n",buf);

                }else
                {
                        printf("read ret = %d\n",ret);
                        printf("print read char  %c %x %x %x\n",buf[0],buf[1],buf[2],buf[3]);
                	printf("print string %s\n",buf);
                               printf("this is digit %s\n",buf);
                                printf("atoi value %d\n",atoi(buf) );
                }
		
        }
         close(led_fd);

}
int main()
{
     pthread_t thread1, thread2;
     const char *message1 = "Thread 1";
     const char *message2 = "Thread 2";
     int  iret1, iret2;	
     struct ply my_ply;
	my_ply.a=10;
	int i =0;
     //power_supply my ;
//check_usb_online();
//return 0;

    //iret1 = pthread_create( &thread1, NULL, print_message_function, (void*) message1);
//	printf("TTTTTTTTT\n");
//	printf(" my_ply.a %d\n",my_ply.a);
    iret1 = pthread_create( &thread1, NULL, print_message_function,&my_ply); 
    if(iret1)
     {
	
         fprintf(stderr,"Error - pthread_create() return code: %d\n",iret1);
         exit(EXIT_FAILURE);
     }
	printf("111:test\n");
          pthread_join( thread1, NULL);
	printf("wait thread terminate\n");
	while(i<1)
	{
		i++;
		sleep(1);
	}
	printf("verify value %d\n",my_ply.a);
	return 0;
}

void *print_message_function( void *ptr )
{
  //   char *message;
  //   message = (char *) ptr;
  //  printf("%s \n", message);
     int i = 0;
      struct ply *in_ply=(struct ply*)ptr;
    printf("test in while\n"); 
  while(i<100000000)
  { 
	i++; 
    printf("in FUNC %d \n",i);
	sleep(1);
     printf("print arg %d", (*in_ply).a);
	if (i%2==0)
	{
		write_trig("lucid_blink_fast");
	}else
	{
		write_trig("none");
	}	

  }
//write_trig("lucid_blink_fast");		
	(*in_ply).a=9999;
//	return NULL;
}
//write_trig("lucid_blink_fast");
void write_trig(char *trig)
{
	int led_fd=0;
        char buf[128];
        led_fd=open("/sys/class/leds/battery_led_g/trigger",O_WRONLY);
        if (led_fd>0)
        {
                sprintf(buf,"%s",trig);
                write(led_fd,buf,strlen(buf));
        }
         close(led_fd);

}


#if 0
pthread_attr_t tattr;
pthread_t tid;
int ret;
int newprio = 99;
sched_param param;

/* initialized with default attributes */
ret = pthread_attr_init (&tattr);

/* safe to get existing scheduling param */
ret = pthread_attr_getschedparam (&tattr, &param);

/* set the priority; others are unchanged */
param.sched_priority = newprio;

/* setting the new scheduling param */
ret = pthread_attr_setschedparam (&tattr, &param);  
pthread_create( &pow_supply_thread, &tattr, pow_supply_ThreadFunc,&my );
#endif


