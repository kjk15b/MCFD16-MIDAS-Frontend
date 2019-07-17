//********************************************************************
//
//  Name:         dd_mcfd16.cxx
//  Created by:   Kolby Kiesling
//
//  Contents:     Device driver for Mesytec MCFD16
//
//  $Id: $
//
//********************************************************************

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <iostream>

#include "midas.h"

#undef calloc



#define DEFAULT_TIMEOUT 1000     // milliseconds



#define DD_MCFD_SETTINGS_STR "\
BWL = INT : 1\n\
CFD = INT : 1\n\
set_mask = INT : 0\n\
set_coincidence = INT : 36\n\
set_veto = INT : 0\n\
gate_selector = INT : 0\n\
gate_timing = INT : 255\n\
pulser = INT : 0\n\
Read Period ms = FLOAT : 200\n\
set_polarity = INT[8] :\n\
[0] 1\n\
[1] 1\n\
[2] 1\n\
[3] 1\n\
[4] 1\n\
[5] 1\n\
[6] 1\n\
[7] 1\n\
set_gain = INT[8] :\n\
[0] 1\n\
[1] 1\n\
[2] 1\n\
[3] 1\n\
[4] 1\n\
[5] 1\n\
[6] 1\n\
[7] 1\n\
set_threshold = INT[16] :\n\
[0] 0\n\
[1] 0\n\
[2] 0\n\
[3] 0\n\
[4] 0\n\
[5] 0\n\
[6] 0\n\
[7] 0\n\
[8] 0\n\
[9] 0\n\
[10] 0\n\
[11] 0\n\
[12] 0\n\
[13] 0\n\
[14] 0\n\
[15] 0\n\
set_width = INT[8] :\n\
[0] 16\n\
[1] 16\n\
[2] 16\n\
[3] 16\n\
[4] 16\n\
[5] 16\n\
[6] 16\n\
[7] 16\n\
set_dead_time = INT[8] :\n\
[0] 27\n\
[1] 27\n\
[2] 27\n\
[3] 27\n\
[4] 27\n\
[5] 27\n\
[6] 27\n\
[7] 27\n\
set_delay_line = INT[8] :\n\
[0] 1\n\
[1] 1\n\
[2] 1\n\
[3] 1\n\
[4] 1\n\
[5] 1\n\
[6] 1\n\
[7] 1\n\
set_fraction = INT[8] :\n\
[0] 40\n\
[1] 40\n\
[2] 40\n\
[3] 40\n\
[4] 40\n\
[5] 40\n\
[6] 40\n\
[7] 40\n\
trigger_source = INT[3] :\n\
[0] 1\n\
[1] 1\n\
[2] 1\n\
trigger_monitor = INT[2] :\n\
[0] 0\n\
[1] 1\n\
trigger_pattern = INT[2] :\n\
[0] 0\n\
[1] 255\n\
set_multiplicity = INT[2] :\n\
[0] 1\n\
[1] 16\n\
paired_coincidence = INT[16] :\n\
[0] 255\n\
[0] 255\n\
[0] 255\n\
[0] 255\n\
[0] 255\n\
[0] 255\n\
[0] 255\n\
[0] 255\n\
[0] 255\n\
[0] 255\n\
[0] 255\n\
[0] 255\n\
[0] 255\n\
[0] 255\n\
[0] 255\n\
[0] 255\n\
"


typedef struct {
  int BWL; // Bandwidth limit
  int CFD; // turn on CFD
  int set_mask; // set mask for registers pairings
  int set_coincidence; // global coincidence time
  int set_veto; // fast veto input
  int gate_selector; // gate selector
  int gate_timing; // Gate allowed timing, hard coding to falling edge right now b/c  of our equipment
  int pulser; // test pulser status
  int readPeriod_ms; // maybe need?
  
  int set_polarity[8]; // pair
  int set_gain[8]; // pair
  int set_threshold[16]; // individual
  int set_width[8]; // pair 
  int set_dead_time[8]; // pair
  int set_delay_line[8]; // pair
  int set_fraction[8]; // pair
  int trigger_source[3]; // 3 values
  int trigger_monitor[2]; // 2 values
  int trigger_pattern[2]; // 2 values
  int set_multiplicity[2]; // Upper & lower
  int paired_coincidence[16]; // 1->15
  
//   bool manual_control; // maybe...
} DD_MCFD_SETTINGS;


typedef struct {
  DD_MCFD_SETTINGS settings;
  DD_MCFD_SETTINGS settingsIncoming;

  INT num_channels;
  INT(*bd)(INT cmd, ...);      // bus driver entry function
  void *bd_info;               // private info of bus driver
  HNDLE hkey;                  // ODB key for bus driver info


  float *channel_frequency; // 0-15 standard channels 16-18 are trig0,1,2 and 19 is total 
  
  
  float *array;                // Most recent measurement or NaN, one for each channel
  DWORD *update_time;          // seconds

  INT get_label_calls;

} DD_MCFD_INFO;


// Should probably call this every time the fe is started.  This would log PID parameters to the midas.log so they can be recovered later...
//int recall_pid_settings(bool saveToODB=false); // read from Arduino, print to messages/stdout, optionally save to ODB


int mcfd_apply_settings(DD_MCFD_INFO* info) {
  /* Format of most commands to R / W from the MCFD:
   * (char) + (int) + (opt. modifier) 
   * Where char is the identifying register, int is the data write and optional modifier is for special cases...
   */
  
  
  char cmd[256];
  char str[256];
  memset(cmd, 0, sizeof(cmd));
  // want to write for loops to automate the r / w process...
  
  for (int i=0; i<16; ++i) { // These are the only commands to write that are 16 wide...
	snprintf(cmd, sizeof(cmd)-1, "st %d %d\r\n", i, info->settings.set_threshold[i]); // set threshold
	BD_PUTS(cmd);
	BD_GETS(str, sizeof(str)-1, "\n", DEFAULT_TIMEOUT); // read echo

	if (i < 3) {
	  snprintf(cmd, sizeof(cmd)-1, "tr %d %d\r\n", i, info->settings.trigger_source[i]); // set trigger sources
	  BD_PUTS(cmd);
	  BD_GETS(str, sizeof(str)-1, "\n", DEFAULT_TIMEOUT); // read echo
	}

	if (i < 8) {
	  snprintf(cmd, sizeof(cmd)-1, "sp %d %d\r\n", i, info->settings.set_polarity[i]); // set polarity
	  BD_PUTS(cmd);
	  BD_GETS(str, sizeof(str)-1, "\n", DEFAULT_TIMEOUT); // read echo
	
	  snprintf(cmd, sizeof(cmd)-1, "sg %d %d\r\n", i, info->settings.set_gain[i]); // set gain
	  BD_PUTS(cmd);
	  BD_GETS(str, sizeof(str)-1, "\n", DEFAULT_TIMEOUT); // read echo
	
	  snprintf(cmd, sizeof(cmd)-1, "sw %d %d\r\n", i, info->settings.set_width[i]); // set width
	  BD_PUTS(cmd);
	  BD_GETS(str, sizeof(str)-1, "\n", DEFAULT_TIMEOUT); // read echo
	
	  snprintf(cmd, sizeof(cmd)-1, "sy %d %d\r\n", i, info->settings.set_delay_line[i]); // set delay
	  BD_PUTS(cmd);
	  BD_GETS(str, sizeof(str)-1, "\n", DEFAULT_TIMEOUT); // read echo
	
	  snprintf(cmd, sizeof(cmd)-1, "sd %d %d\r\n", i, info->settings.set_dead_time[i]); // set dead time
	  BD_PUTS(cmd);
	  BD_GETS(str, sizeof(str)-1, "\n", DEFAULT_TIMEOUT); // read echo
	
	  snprintf(cmd, sizeof(cmd)-1, "sf %d %d\r\n", i, info->settings.set_fraction[i]); // set fraction
	  BD_PUTS(cmd);
	  BD_GETS(str, sizeof(str)-1, "\n", DEFAULT_TIMEOUT); // read echo
	}
	if (i < 15) {
	  snprintf(cmd, sizeof(cmd)-1, "pa %d %d\r\n", (i+1), info->settings.paired_coincidence[i]); // paired_coincidence
	  BD_PUTS(cmd);
	  BD_GETS(str, sizeof(str)-1, "\n", DEFAULT_TIMEOUT); // read echo	  
	}
  }
  
  
  
  snprintf(cmd, sizeof(cmd)-1, "tm %d %d\r\n", info->settings.trigger_monitor[0], info->settings.trigger_monitor[1]); // set trigger monitor
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\n", DEFAULT_TIMEOUT); // read echo
  
  snprintf(cmd, sizeof(cmd)-1, "sm %d %d\r\n", info->settings.set_multiplicity[0], info->settings.set_multiplicity[1]); // set multiplicity
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
  
  snprintf(cmd, sizeof(cmd)-1, "bwl %d\r\n", info->settings.BWL); // set bandwidth limit
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
  
  snprintf(cmd, sizeof(cmd)-1, "cfd %d\r\n", info->settings.CFD); // set CFD mode
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
  
  snprintf(cmd, sizeof(cmd)-1, "sk %d\r\n", info->settings.set_mask); // set mask registers
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
  
  snprintf(cmd, sizeof(cmd)-1, "sc %d\r\n", info->settings.set_coincidence); // set coincidence
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
  
  snprintf(cmd, sizeof(cmd)-1, "sv %d\r\n", info->settings.set_veto); // set veto
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
  
  snprintf(cmd, sizeof(cmd)-1, "gs %d\r\n", info->settings.gate_selector); // set gate selection
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
  
  snprintf(cmd, sizeof(cmd)-1, "ga 1 %d\r\n", info->settings.gate_timing); // set gate timing (NEGATIVE EDGE)
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
  
  snprintf(cmd, sizeof(cmd)-1, "p%d\r\n", info->settings.pulser); // set pulser
  BD_PUTS(cmd);
  BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
  
  
  return FE_SUCCESS; // TODO: make sure things applied okay...
}


void mcfd_settings_updated(INT hDB, INT hkey, void* vinfo)
{
  printf("Settings updated\n");

  DD_MCFD_INFO* info = (DD_MCFD_INFO*) vinfo;

  bool changed=false;

  
  if (info->settingsIncoming.kP != info->settings.kP) {
    std::cout << "   kP changed from ``" << info->settings.kP << "'' to ``" << info->settingsIncoming.kP << "''" << std::endl;
    info->settings.kP = info->settingsIncoming.kP;
    //cm_msg(MINFO, "pid_settings_updated", "This frontend must be restarted for changes to     ``kP'' to take effect.");
    changed=true;
  }
  
  if (info->settingsIncoming.kI != info->settings.kI) {
    std::cout << "   kI changed from ``" << info->settings.kI << "'' to ``" << info->settingsIncoming.kI << "''" << std::endl;
    info->settings.kI = info->settingsIncoming.kI;
    changed=true;
  }

  if (info->settingsIncoming.kD != info->settings.kD) {
    std::cout << "   kD changed from ``" << info->settings.kD << "'' to ``" << info->settingsIncoming.kD << "''" << std::endl;
    info->settings.kD = info->settingsIncoming.kD;
    changed=true;
  }

  if (info->settingsIncoming.out_Min != info->settings.out_Min) {
    std::cout << "   out_Min changed from ``" << info->settings.out_Min << "'' to ``" << info->settingsIncoming.out_Min << "''" << std::endl;
    info->settings.out_Min = info->settingsIncoming.out_Min;
    changed=true;
  }

  if (info->settingsIncoming.out_Max != info->settings.out_Max) {
    std::cout << "   out_Max changed from ``" << info->settings.out_Max << "'' to ``" << info->settingsIncoming.out_Max << "''" << std::endl;
    info->settings.out_Max = info->settingsIncoming.out_Max;
    changed=true;
  }

  if (info->settingsIncoming.readPeriod_ms != info->settings.readPeriod_ms) {
    std::cout << "   readPeriod_ms changed from ``" << info->settings.readPeriod_ms << "'' to ``" << info->settingsIncoming.readPeriod_ms << "''" << std::endl;
    info->settings.readPeriod_ms = info->settingsIncoming.readPeriod_ms;
    changed=true;
  }

  if (changed) mcfd_apply_settings(info); // TODO: only apply the changed ones...
}


//---- standard device driver routines -------------------------------

INT dd_mcfd16_init(HNDLE hkey, void **pinfo, INT channels, INT(*bd)(INT cmd, ...))
{
  int status;
  HNDLE hDB, hkeydd;
  DD_MCFD_INFO *info;
  printf("dd_mcfd16_init: channels = %d\n", channels);

  info = new DD_MCFD_INFO;
  *pinfo = info;

  cm_get_experiment_database(&hDB, NULL);

  info->array = (float*) calloc(channels, sizeof(float));
  info->update_time = (DWORD*) calloc(channels, sizeof(DWORD));
  info->channel_frequency = (float*) calloc(channels, sizeof(float));
  
  for (int i=0; i<channels; ++i) { // TODO: obviously change this to be back to channels, just making notes for the meantime...
    info->channel_frequency[i] = ss_nan(); // initialize the channel readouts to nan
    info->array[i] = ss_nan();
    info->update_time[i] = 0;
  }
  
  info->get_label_calls=0;  
  
  info->num_channels = channels;  // TODO: make sure it is 19 channel readout
  info->bd = bd;
  info->hkey = hkey;

  // DD Settings
  status = db_create_record(hDB, hkey, "DD", DD_MCFD_SETTINGS_STR); // should make the database correctly now...
  if (status != DB_SUCCESS)
     return FE_ERR_ODB;

  status = db_find_key(hDB, hkey, "DD", &hkeydd);
  if (status != DB_SUCCESS) {
    return FE_ERR_ODB;
  }
  int size = sizeof(info->settingsIncoming);
  status = db_get_record(hDB, hkeydd, &info->settingsIncoming, &size, 0);
  if (status != DB_SUCCESS) return FE_ERR_ODB;

  status = db_open_record(hDB, hkeydd, &info->settingsIncoming,
                          size, MODE_READ, mcfd_settings_updated, info);
  if (status != DB_SUCCESS) {
    return FE_ERR_ODB;
  }
  memcpy(&info->settings, &info->settingsIncoming, sizeof(info->settingsIncoming));

  // Initialize bus driver
  status = info->bd(CMD_INIT, info->hkey, &info->bd_info);
  if (status != SUCCESS) return status;
  
  printf("Sending initialization commands to MCFD16\n");
  

  char str[256];
  memset(str, 0, sizeof(str));
  int len=0;
  // TODO: Check to see if MCFD16 is outputing data
  BD_PUTS("ra 0\r\n"); // "get" measurement
  int tries=0;
  for (tries=0; tries<500; ++tries) {
    int len = BD_GETS(str, sizeof(str)-1, "\n", 2); // will have to format this for MCFD, will need heavy testing
    if (len==0 && tries > 4) break;
  }
  printf("read tries=%d, len=%d, str=``%s''\n", tries, len, str); // debugging
  if (len == 0) {
  }


  BD_PUTS("ra 0\r\n");
  len = BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT);  // EXTRA dummy read to readback "echo?"
  len = BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT);
  printf("ra 0 response = ``%s''\n",str);

  for (tries=0; tries<500; ++tries) {
    int len = BD_GETS(str, sizeof(str)-1, "\r\n", 2);
    if (len==0 && tries > 2) break;
  }


  mcfd_apply_settings(info); // settings are probably not functional, but they appear to be getting there...

  return FE_SUCCESS;
}

//--------------------------------------------------------------------

INT dd_mcfd_exit(DD_MCFD_INFO * info)
{
  printf("Running dd_mcfd_exit\n");

  // Close serial
  info->bd(CMD_EXIT, info->bd_info);

//   if (info->array) free(info->array);
//   if (info->update_time) free(info->update_time);
//   delete info->recent;
  delete info;

  return FE_SUCCESS;
}

//--------------------------------------------------------------------

INT dd_mcfd_set(DD_MCFD_INFO * info, INT channel, float value)
{
  if (channel < 0 || channel >= info->num_channels)
    return FE_ERR_DRIVER;

  channel+=info->num_channels;

  char cmd[256];
  char str[256];
  memset(cmd, 0, sizeof(cmd));

  printf("Set channel %d to %.2f\n", channel, value);
  
  switch (channel) {
    case CHAN_OUT_SP: // Setpoint
      // TODO: make sure "value" is reasonable
      info->set_point = value;
      snprintf(cmd, sizeof(cmd)-1, "s%.2f\n", info->set_point);
      BD_PUTS(cmd);
      BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); // read echo
      printf("Setpoint to %.2f\n", info->set_point);
      //cmd << "s" << value << "\n";
      //BD_PUTS(cmd.str().c_str());
      // TODO: make sure it was applied correctly...
      break;
    case CHAN_OUT_RE:
      // FIXME: not implemented
      //int ivalue = (int) value;
      //if (ivalue < 0 || ivalue > 255) {
        //std::cerr << "[dd_arduino_fan_pid_set] Error: Value = " << value << " is out of range for Channel["<< channel <<"]" << std::endl;
        //return FE_ERR_DRIVER;
      //}
      break;
  }

  return FE_SUCCESS;
}

//--------------------------------------------------------------------

INT dd_mcfd_get(DD_MCFD_INFO * info, INT channel, float *pvalue)
{
  // Get: PID Output and PV

  INT status = 0;
  char str[256];
  *pvalue = ss_nan();
  memset(str, 0,sizeof(str)-1);


  status = BD_PUTS("g\n");
  if (status < 0) {
    std::cerr << "BD_PUTS error." << std::endl;
    al_trigger_alarm("FanPID", "Fan PID communication failure with Arduino.", 0, "BD_PUTS returns < 0", AT_INTERNAL);
    return FE_ERR_HW;
  }
  //}

  status = BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT);  // EXTRA dummy read to readback "echo?"
  ss_sleep(50);
  status = BD_GETS(str, sizeof(str)-1, "\r\n", DEFAULT_TIMEOUT); 
  if (status <= 0) {
    std::cerr << "BD_GETS error." << std::endl;
  }

  float pv=0,co=0;
  if ( sscanf(str,"%f\t%f",&pv,&co) != 2) {
    std::cerr << "Error: Failed to parse data from Arduino." << std::endl;
    std::cerr << "received: ``" << str << "''" << std::endl;
//    al_trigger_alarm("FanPID", "Fan PID communication failure with Arduino.", 0, "Failed to parse received data", AT_INTERNAL); // TODO: re-enable this...
    return FE_ERR_HW;
  }

   printf("PV=%.2f  CO=%.2f\n", pv, co);

  switch (channel) {
    case CHAN_INP_PV:
      *pvalue = pv;
      info->process_value = pv;
      break;
    case CHAN_INP_CO:
      *pvalue = co;
      info->controlled_output = co;
      break;
    default:
      *pvalue = ss_nan();
      break;
  }

  //ss_sleep(1);
  return FE_SUCCESS;
}

//--------------------------------------------------------------------

INT dd_mcfd_get_label(DD_MCFD_INFO * info, INT channel, char *name)
{

  // Keep track of calls to this function to get right channel labels with multi.c class driver.
  if (info->num_channels==2 && info->get_label_calls > info->num_channels/2) {
    channel+=info->num_channels;
  }

  switch (channel) {
    case CHAN_INP_PV:
      strncpy(name, "Process Value degC", NAME_LENGTH-1);
      break;
    case CHAN_INP_CO:
      strncpy(name, "Controlled Output", NAME_LENGTH-1);
      break;
    case CHAN_OUT_SP:
      strncpy(name, "Setpoint degC", NAME_LENGTH-1);
      break;
    case CHAN_OUT_RE:
      strncpy(name, "reserved", NAME_LENGTH-1);
      break;
    default:
      //snprintf(name,"");
      memset(name, 0, NAME_LENGTH);
      //return FE_ERR_DRIVER;
  }

  //printf("dd_arduino_fan_pid_get_label: chan=%d, Name=``%s''\n",channel,name);
  info->get_label_calls++;
  return FE_SUCCESS;
}

//---- device driver entry point -------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

INT dd_mcfd(INT cmd, ...)
{
  va_list argptr;
  HNDLE hKey;
  INT channel, status;
  DWORD flags=0;
  float value, *pvalue;
  void *info, *bd;
  char* name;

  va_start(argptr, cmd);
  status = FE_SUCCESS;

  switch (cmd) {
    case CMD_INIT:
      hKey = va_arg(argptr, HNDLE);
      info = va_arg(argptr, void *);
      channel = va_arg(argptr, INT);
      flags = va_arg(argptr, DWORD);
      if (flags==0) {} // prevent set-but-unused compile warning
      bd = va_arg(argptr, void *);
      status = dd_mcfd_init(hKey, (void**)info, channel, (INT (*)(INT, ...)) bd);
      break;

    case CMD_EXIT:
      info = va_arg(argptr, void *);
      status = dd_mcfd_exit((DD_MCFD_INFO*) info);
      break;

    case CMD_SET:
      info = va_arg(argptr, void *);
      channel = va_arg(argptr, INT);
      value = (float) va_arg(argptr, double);
      status = dd_mcfd_set((DD_MCFD_INFO*) info, channel, value);
      break;

    case CMD_GET:
      info = va_arg(argptr, void *);
      channel = va_arg(argptr, INT);
      pvalue = va_arg(argptr, float *);
      status = dd_mcfd_get((DD_MCFD_INFO*) info, channel, pvalue);
      break;

    case CMD_GET_LABEL:
      info = va_arg(argptr, void *);
      channel = va_arg(argptr, INT);
      name = va_arg(argptr, char *);
      status = dd_mcfd_get_label((DD_MCFD_INFO*) info, channel, name);
      break;

    default:
      break;
  }

  va_end(argptr);

  return status;
}

#ifdef __cplusplus
}
#endif

//--------------------------------------------------------------------


