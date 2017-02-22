#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <stdlib.h>
#include "BatteryInfo.h"
#include "MessageIOController.h"
#include "wDebug.h"

#include <fcntl.h>
#include <sys/time.h>

#include <string.h>

#include <sys/select.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>

//#include <linux/types.h>
#include <linux/netlink.h>

#include <hardware_legacy/power.h>

using namespace std;
using namespace wistronAPIs;

#include "LEDController.h"
extern LEDController *g_ledController;

//set timout 1 sec
PowerOffSubTimer::PowerOffSubTimer():SubTimerHandler(10,SUBTIMER_ONCE)
{

}

void PowerOffSubTimer::subTimerHandler()
{
    g_ledController->setLED( POWER_LED_GREEN, LED_OFF );
    g_ledController->setLED( WIFI_LED, LED_OFF );
    g_ledController->setLED( SDCARD_LED, LED_OFF );
    g_ledController->setLED( RECORD_LED, LED_OFF );
}

void BatteryInfo::off_led_blink_once()
{
  //Turn off all LED.
  g_ledController->setLED( POWER_LED_GREEN, LED_OFF );
  g_ledController->setLED( WIFI_LED, LED_OFF );
  g_ledController->setLED( SDCARD_LED, LED_OFF );
  g_ledController->setLED( RECORD_LED, LED_OFF );
  g_ledController->setLED( POWER_LED_AMBER, LED_OFF );

  //Turn on and wait.
  g_ledController->setLED( POWER_LED_GREEN, LED_ON );
  g_ledController->setLED( WIFI_LED, LED_ON );
  g_ledController->setLED( SDCARD_LED, LED_ON );
  g_ledController->setLED( RECORD_LED, LED_ON );
  mPowerOffSubTimer = new PowerOffSubTimer;

  if( g_Timer != NULL )
  {
     g_Timer->registerSubTimer( mPowerOffSubTimer );
  }

}

void BatteryInfo::executePowerOff()
{
	off_led_blink_once();
	system("am start -a android.intent.action.ACTION_REQUEST_SHUTDOWN --ez KEY_CONFIRM true --activity-clear-task");
}

int BatteryInfo::check_capacity()
{
	int led_fd=0;
        int ret=0;
        char buf[64];
        int capacity=0;
        led_fd=open(BATTERY_CAPACITY_PATH,O_RDONLY);
        if (led_fd>0)
        {
                ret=read(led_fd,buf,64);
                if (ret!=-1)
                {
			capacity=atoi(buf);
		}
        }
	close(led_fd);
	return capacity;
}
bool BatteryInfo::check_charging_state()
{
	int led_fd=0;
        int ret=0;
        char buf[64];
	bool isCharging=false;
	led_fd=open(CHARGING_STATE_PATH,O_RDONLY);
        if (led_fd>0)
        {
                ret=read(led_fd,buf,64);
		if (ret!=-1)
		{
			if(0==strncasecmp(buf,"Charging",8))
				isCharging=true;
			else
				isCharging=false;
		}
	}
	close(led_fd);
	return isCharging;
}

BatteryInfo::BatteryInfo(bool is_WAPI_existed)
{

  mPowerOffSubTimer = NULL;
  mCapacity = 0;
  mVolt = 0.0;
  wistron_pow.capacity=0;
  wistron_pow.isChanged=false;
  wistron_pow.start=true;

  if (is_WAPI_existed)
  {
	DBG("wistron_apis already existed! start BatteryMointor only\n");
	setBatteryMonitor();
  }
}

void BatteryInfo::stop_thread()
{
  wistron_pow.start=false;
  pthread_join(wistron_pow.pow_supply_thread,NULL);
  return ;
}

void BatteryInfo::write_trig(char *trig)
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

void* BatteryInfo::pow_supply_ThreadFunc(void* ptr)
{
	struct power_supply *in_data=(struct power_supply*)ptr;
	int capacity=0;
	POWER_SUPPLY_MODE oldmode,mode;
	oldmode=mode=POWER_SUPPLY_INIT;

	int count=0;
	DBG("Start Battery Monitor\n");
	while((*in_data).start)
	{
		if( count % 40 == 0) // 10 seconds
			DBG("Thread alive!\n");

		if(oldmode != mode )
		{
			(*in_data).isChanged=true;
			oldmode=mode;
		}
		if (check_charging_state())
		{
			mode=POWRR_SUPPLY_CHARGING;
		}
		else
		{
			capacity=check_capacity();

			if(capacity > 20 )
			{
				mode=POWER_SUPPLY_CAPACITY_HIGER_THAN_20;
			}else
			{
				if(capacity<20 && capacity >5)
					mode=POWER_SUPPLY_CAPACITY_BTWN_20_5;
				else if(capacity<5)
					mode=POWER_SUPPLY_CAPACITY_LOWER_THAN_5;
			}
		}
		(*in_data).mode=mode;
		if((*in_data).isChanged==true)
	        {
			(*in_data).isChanged=false;
			switch((*in_data).mode)
			{
				case POWRR_SUPPLY_CHARGING:
					g_ledController->setLED( POWER_LED_GREEN, LED_OFF );
					g_ledController->setLED( POWER_LED_AMBER, LED_ON );
					break;
				case POWER_SUPPLY_CAPACITY_HIGER_THAN_20:
					g_ledController->setLED( POWER_LED_GREEN, LED_ON );
					g_ledController->setLED( POWER_LED_AMBER, LED_OFF );
					break;
				case POWER_SUPPLY_CAPACITY_BTWN_20_5:
					g_ledController->setLED( POWER_LED_GREEN, LED_OFF );
					g_ledController->setLED( POWER_LED_AMBER, LED_BLINK1 );
					break;
				case POWER_SUPPLY_CAPACITY_LOWER_THAN_5:
					g_ledController->setLED( POWER_LED_GREEN, LED_OFF);
					g_ledController->setLED( POWER_LED_AMBER, LED_FAST_BLINK );
					break;
			}
		}
		usleep(250000);
		count++;
	}
	return 0;
}

void* BatteryInfo::usb_detector_ThreadFunc(void* ptr)
{
	struct sockaddr_nl nls;
	struct pollfd pfd;
	char buf[512];
	struct power_supply *in_data=(struct power_supply*)ptr;

	memset(&nls,0,sizeof(struct sockaddr_nl));
	nls.nl_family = AF_NETLINK;
	nls.nl_pid = getpid();
	nls.nl_groups = -1;

	pfd.events = POLLIN;
	pfd.fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);

	DBG("wistron_apis - bind netlink socket\n");
	if(check_charging_state())
		acquire_wake_lock(PARTIAL_WAKE_LOCK,"wistron_apis");


	   fd_set rfds;
           struct timeval tv;
           int retval;

           /* Watch stdin (fd 0) to see when it has input. */
           FD_ZERO(&rfds);
       //    FD_SET(0, &rfds);

	// Listen to netlink socket
	::bind(pfd.fd, (struct sockaddr*) &nls, sizeof(struct sockaddr_nl));
	while(1)
	{
	/* Wait up to five seconds. */
           tv.tv_sec = 5;
           tv.tv_usec = 0;
	   FD_ZERO(&rfds);
	   FD_SET(pfd.fd, &rfds);
		retval = select(pfd.fd+1, &rfds, NULL, NULL, &tv);
		DBG("retval = %d\n",retval);
	   if (retval == -1)
               DBG("select()");
           else if (retval)
           {
		DBG("Data is available now.\n");
		if(FD_ISSET(pfd.fd,&rfds));
		{
			DBG("FD_ISSET == true \n");
                	int i, len = recv(pfd.fd, buf, sizeof(buf), MSG_DONTWAIT);
			i=1;
			while (i<len) {
                        DBG("DATA IN!!\n");
                        DBG("%s\n", buf+i);
			if(0==strcmp(buf+i,"USB_STATE=DISCONNECTED"))
                        {
                                usleep(500000);
                                release_wake_lock("wistron_apis");
                        }else if(0==strcmp(buf+i,"USB_STATE=CONNECTED"))
                        {
                                 acquire_wake_lock(PARTIAL_WAKE_LOCK,"wistron_apis");
                        }
                        i += strlen(buf+i)+1;
                	}
		
		}
		/* FD_ISSET(0, &rfds) will be true. */
           }
	   else
               DBG("No data within five seconds.\n");	
	}

#if 0 
	while (-1!=poll(&pfd, 1, 2000)) {
		int i, len = recv(pfd.fd, buf, sizeof(buf), MSG_DONTWAIT);
		i = 0;
		while (i<len) {
			DBG("DATA IN!!\n");
			if(0==strcmp(buf+i,"USB_STATE=DISCONNECTED"))
			{
				usleep(500000);
				release_wake_lock("wistron_apis");
			}else if(0==strcmp(buf+i,"USB_STATE=CONNECTED"))
			{
				 acquire_wake_lock(PARTIAL_WAKE_LOCK,"wistron_apis");
			}
			i += strlen(buf+i)+1;
		}
	}
#endif
	return 0;
}

void BatteryInfo::parseInfo()
{

  std::ifstream infile( "/sys/class/power_supply/battery/uevent" );

  std::string line;
  int count = 1;
  while( std::getline( infile, line ) )
    {
      string attr;
      std::istringstream iss(line);
      
      iss>>attr;

      size_t found = attr.find( "POWER_SUPPLY_CAPACITY" );
      if( found != string::npos )
	{
	  string foundString = attr.substr( attr.find("=") + 1 );
	  int value = atoi( foundString.c_str() );
	  mCapacity = value;
	}

      found = attr.find( "POWER_SUPPLY_VOLTAGE_NOW" );
      if( found != string::npos )
	{
	  string foundString = attr.substr( attr.find("=") + 1 );
	  int value = atoi( foundString.c_str() );
	  mVolt = (float) value/1000000;
	}
    }
}

void BatteryInfo::setBatteryMonitor()
{
	pthread_create( &wistron_pow.pow_supply_thread, NULL, pow_supply_ThreadFunc,&wistron_pow );
	pthread_create( &wistron_pow.usb_detector_thread,NULL,usb_detector_ThreadFunc,&wistron_pow);
}

float BatteryInfo::getVolt()
{
  char ans[ 16 ] = { 0, };

  parseInfo();

  sprintf( ans, "%.2f\0", mVolt );

  ioController.writeResponse( OUTPUT_WAPIS, ans );
  ioController.writeResponse( OUTPUT_STDOUT, ans );

  return( mVolt );
}

void BatteryInfo::eventHandler( Event &e )
{
  if( e.mType == BATTERY_MONITOR )
    {
      setBatteryMonitor();
    }
  else if( e.mType == BATTERY_VOLT )
    {
      getVolt();
    }
  else if( e.mType == OFF_STATE) //get off event, terminat thread.
    {
      stop_thread();
      executePowerOff();
    }

  return;
}
