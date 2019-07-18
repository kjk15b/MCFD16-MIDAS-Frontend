# This is a simple script to read and write data via USB from the Mesytec MCFD-16 NIM Module
# By: Kolby Kiesling
# For: The ACU Nuclear Physics Research Group
# August 16 2018
# Revised February 18 2019
# V_1.02

# Modules used
import serial
#import matplotlib.pyplot as plt substituted by ROOT
import re
#import numpy as np substituted by ROOT
import datetime
import time

# Initial setup

mcfd = serial.Serial('/dev/ttyUSB0', 9600, timeout=1)
try:
    print(mcfd.name)
    mcfd.write('v\r\n')
    try:
      i = 0
      for i in range(5):
        line = mcfd.readline()
        match = re.search('Firmware version: 2.13|Software version: 2.19', line)
        if match:
          print('Successfully connected')
          print(line)
    except: print('Failed to connect')
except: print('Failed to connect to Mesytec CFD-16')

print('This is a simple script that logs frequencies of the channels to the MCFD-16 NIM Module;')
print("How many times would you like to record (# of times to Write / Read MCFD-16)?")
duration = float(input())

time_log = list()
i = 0
dt_log = list() # datetime log
for i in range(int(duration)):
    dt_log.append('')

chan_0_log = list()  # only need the first three channels right now (TESTING)
chan_1_log = list()
chan_2_log = list()
trig_0_log = list()  # Trigger recording
trig_1_log = list()
trig_2_log = list()
total_log = list()
line_write = list()

'''
r_chan_0_1 = list()
r_chan_0_4 = list()
r_chan_0_6 = list() ratio stuff, needs debugging for div / 0
r_chan_1_4 = list()
r_chan_1_6 = list()
r_chan_4_6 = list()
'''

log = int(0)


def serial_read():
  
	'''
	For parsing the return strings from the MCFD-16
	'''
	r_chan = re.compile('rate channel [0-9]*:|trigger rate[0-9]*:|sum rate :|mcfd-16>|ERROR!')
        r_freq = re.compile('Hz|kHz|MHz')
        r_num = re.compile('[0-9]*')
        r_unit = re.compile('Hz')
        r_dec = re.compile('. ')
        channel = 0
        div = 1
        save_line = ''
        chn = ['0', '1', '2', '16', '17', '18', '19']  # reminder to reappend to all channels later
        crrg = '\r\n'
        chan_log = ['', '', '', '', '', '', '', '']
        while channel <= 6:
                # Writing to Mesytec CFD-16
                msg = 'ra ' + chn[channel] + crrg
                print(msg)
                mcfd.write(msg)
                i = 0
                for i in range(5):
                     try:
                       line = mcfd.readline()
                       match_chn = re.search('rate channel [0-9]*:|trigger rate[0-9]*:|sum rate :', line)
                       match_frq = re.search('Hz|kHz|MHz', line)
                       if match_chn:
                         if match_frq:
                            print('Fetching data')  # successful match in terminal
                            save_line = line
                            mod_line = re.sub(r_chan, '', line)
                            print(save_line)
                            unit_frq = re.sub(r_num, '', mod_line)
                            unit_frq = re.sub(r_unit, '', unit_frq)
                            unit_frq = re.sub(r_dec, '', unit_frq)
                            print(unit_frq)
                            kHz = re.search('k', unit_frq)
                            if kHz:  # check if in units of kHz
			      print('Unit in kHz')
                              div = 1
                            else:
                              div = 1000  # convert from Hz to kHz (not expecting anything atm of MHz range
                     except: print('Error processing lines')
                # Converting string to floats
                chan_log[channel] = re.sub(r_chan, '', save_line)
                chan_log[channel] = re.sub(r_freq, '', chan_log[channel])
                try:
                    chan_log[channel] = float(chan_log[channel])
                    print(chan_log[channel])
                    #time.sleep(3)
                    chan_log[channel] = chan_log[channel] / div
                except: 
		  print('Error, could not convert string to float.', '\n', chan_log[channel])
                channel += 1
        # Appending to lists
        # In essence, one list collects all of the data in one cycle and that data is separated into separate bins externally dubbed logs for each channel
        '''
        chan_0_log.append(chan_log[0])
        chan_1_log.append(chan_log[1])
        chan_2_log.append(chan_log[2])
        chan_3_log.append(chan_log[3])
        trig_0_log.append(chan_log[4])
        trig_1_log.append(chan_log[5])
        trig_2_log.append(chan_log[6])
        total_log.append(chan_log[7])
        '''
        # replace with a single line to export
        l = str(chan_log[0])
        k = 1
        for k in range(7):
	  l = l + "\t" + str(chan_log[k])
	  
	line_write.append(l) # save the event as one line
	
        '''
        #data = open(smpfile, 'w+')  sample textfile writing (very easy example) (Data analysis will be in ROOT)
        #i = 0
        #for i in range(19):
            #data.write(chan_log[i])
            #data.write('\n')
        #data.close()
       '''


def write_file():
  data = open("mcfd.txt", "w+")
  i = 0
  for i in range(duration):
    data.write(line_write[i])
    data.write("\n")
    
  data.close()
      

# Main Function
def serial_log():
        timer = 0
        real_time = ''
        while timer <= duration:
                if timer % 2 == 0: # check in every 2 minutes to read frequencies
                        serial_read()  # read frequencies
                        time_log.append(timer)  # think about what to do with time logs
                        real_time = datetime.datetime.utcnow()
                        real_time = str(real_time)
                        dt_log[timer] = real_time
                timer += 1
                time.sleep(60)
        write_file()


serial_log()
