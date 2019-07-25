'''
This program serves as a functional controller to the MCFD-16

Kolby Kiesling
07 / 22 / 2019
'''

# Modules used
import serial # NO LONGER MAINTAINED, find alternative?
import re
import datetime
import time
import matplotlib.pyplot as plt

# GLOBAL VARIABLES
chn_frq = [['''Chn 0'''], ['''Chn 1'''], ['''Chn 2'''], ['''Chn 3'''], ['''Chn 4'''], # records all of the channel frequencies for later analysis
	   ['''Chn 5'''], ['''Chn 6'''], ['''Chn 7'''], ['''Chn 8'''], ['''Chn 9'''],
	   ['''Chn 10'''], ['''Chn 11'''], ['''Chn 12'''], ['''Chn 13'''], ['''Chn 14'''],
	   ['''Chn 15'''], ['''Trg 0'''], ['''Trg 1'''], ['''Trg 2'''], ['''Total''']]

tmp_frq = [['''Chn 0'''], ['''Chn 1'''], ['''Chn 2'''], ['''Chn 3'''], ['''Chn 4'''], # saves only the past N number of events for real-time analysis
	   ['''Chn 5'''], ['''Chn 6'''], ['''Chn 7'''], ['''Chn 8'''], ['''Chn 9'''],
	   ['''Chn 10'''], ['''Chn 11'''], ['''Chn 12'''], ['''Chn 13'''], ['''Chn 14'''],
	   ['''Chn 15'''], ['''Trg 0'''], ['''Trg 1'''], ['''Trg 2'''], ['''Total''']]
tmp_index = 0

tmp_time_log = ['', '', '', '', '', '', '', '', '', ''] # to keep track of timestamps for posting, used for overwritting xticks in mpl

class MCFD():
  def __init__ (self, dev):
    self.dev = serial.Serial(dev, 9600, timeout=1) # string to hold user device
    return
  
  def readout(self): # debugging to see if we made the program right...
    for i in range(5):
      print self.dev.readline()

  def clear(self):
    #time.sleep(0.25)
    #self.dev.write("\r\n")
    self.readout()
    #time.sleep(0.25)
  
  def set_Polarity(self, channel_pair, val): # pair
     cmd = "sp " + str(channel_pair) + " " + str(val) + "\r\n" # construct the command
     self.dev.write(cmd) 
     self.clear()
  def set_Gain(self, channel_pair, val): # pair
    cmd = "sg " + str(channel_pair) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
  
  def set_Bandwidth_Limit(self, val): # single use
    cmd = "bwl " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
  
  def set_CFD(self, val): # single use
    cmd = "cfd " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
  
  def set_Threshold(self, channel, val): # all channels
   cmd = "st " + str(channel) + " " + str(val) + "\r\n"
   self.dev.write(cmd)
   self.clear()
  
  def set_Width(self, channel_pair, val): # pairs
    cmd = "sw " + str(channel_pair) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Dead_Time(self, channel_pair, val): # pairs
    cmd = "sd " + str(channel_pair) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Delay_Line(self, channel_pair, val): # pairs
    cmd = "sy " + str(channel_pair) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Fraction(self, channel_pair, val): # pairs
    cmd = "sf " + str(channel_pair) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Coincidence(self, val): # single use
    cmd = "sc " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Trigger_Source(self, trigger, val): # three values
    cmd = "tr " + str(trigger) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Trigger_Monitor(self, monitor, channel):
    cmd = "tm " + str(monitor) + " " + str(channel) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Trigger_Pattern(self, byte_pattern, val):
    cmd = "tp " + str(byte_pattern) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Veto(self, val):
    cmd = "sv " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Gate_Source(self, val):
    cmd = "gs " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Gate_Timing(self, edge, val):
    cmd = "ga " + str(edge) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
  
  def set_Multiplicity(self, lower_channel, upper_channel):
    cmd = "sm " + str(lower_channel) + " " + str(upper_channel) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Pairings(self, chnID, val):
    cmd = "pa " + str(chnID) + " " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def set_Mask(self, val): # single use
    cmd = "sk " + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
  
  def set_Pulser(self, val):
    cmd = "p" + str(val) + "\r\n"
    self.dev.write(cmd)
    self.clear()
    
  def clear_pulser(self):
    self.dev.write("p0\r\n")
    self.clear()
  
  def initialize(self): # initialize the MCFD 16
    #cmd = ["sp ", "sg "] TODO: make this all in a cmd for loop...
    self.set_Bandwidth_Limit(0) # leave bandwidth limit off...
    self.set_CFD(1)
    self.set_Mask(0) # unmask all registers
    self.set_Coincidence(36) # set the coincidence timeout
    self.set_Veto(0) # turn veto off
    self.set_Gate_Source(0) # set the source as zero
    self.set_Gate_Timing(1, 5) # negative edge (FALLING)
    self.set_Pulser(0) # turn pulser off
    
    trgPat=[1, 4, 18] # OR -> PAIRED COINCIDENCE -> PATTERN & MULTIPLICITY
    trgMntr=[0, 3] # monitor channesl 0 and 3
    mltChn=[0, 3] # multiplicity channels
    
    for i in range(1):
      self.set_Multiplicity(mltChn[i], mltChn[i + 1])
      
    for i in range(2):
      self.set_Trigger_Monitor(i, trgMntr[i])
    
    for i in range(3):
      self.set_Trigger_Source(i, trgPat[i]) # setup the triggers
    
    for i in range(4):
      self.set_Trigger_Pattern(i, 255) # setup the patter for triggering
    
    for i in range(8):
      self.set_Polarity(i, 1) # initialize all polarities to negative
      self.set_Gain(i, 1) # set common gain to 1
      self.set_Width(i, 16) # start out with shortest width
      self.set_Dead_Time(i, 27) # shortest amount of deadtime
      self.set_Delay_Line(i, 1) # shortest delay lines
      self.set_Fraction(i, 40) # use 40% of signal
      
    for i in range(16):
      self.set_Threshold(i, 0) # initialize all thresholds to zero
      self.set_Pairings(i, 255) # global pairing
    return 1
  
  def mcfd_get(self): # fetch data from the MCFD-16
    chn_header = re.compile('rate channel [0-9]*:|trigger rate[0-9]*:|sum rate :') # pattern to search for headers
    chn_units = re.compile('Hz|kHz|MHz')
    factor = 1 # dividing factor to have all units be in kHz
    for i in range(20):
      self.dev.write("ra {0}".format(i)+"\r\n") # request data
      for k in range(5):
	line = self.dev.readline()
	match_chn_header = re.search('rate channel [0-9]*:|trigger rate[0-9]*:|sum rate :', line)
	match_chn_units = re.search('Hz|kHz|MHz', line)
	if match_chn_header:
	  if match_chn_units:
	    
	    if re.search('Hz', line) and not re.search('k', line):
	      factor = 1000 # unit is in Hz
	    elif re.search('MHz', line):
	      factor = 0.001 # unit is in MHz
	    elif re.search('kHz', line):
	      factor = 1
	    else:
	      factor = 1 # default catch all... to be changed later
	    
	    line = re.sub(chn_header, '', line) # omit the header
	    print "Channel: {0}\t{1}".format(i, line) # debugging, print what we found and extracted
	    line = re.sub(chn_units, '', line) # cut the units
	    try:
	      chn_frq[i].append(float(line)) # try to see if we cut correctly
	      tmp_frq[i][tmp_index] = float(line) # super dangerous, but I do not know how else to implement without knowing the index of chn_frq...
	    except:
	      print 'Error, return not cut properly, could not convert string to float\n--------------------\nError: {0}'.format(line)
	      return -1
    tmp_index = (tmp_index + 1) % 10 # lets say we save the last 10 values?...
    return tmp_index # return the index so I know when to call the plotter to update image...
  
  def build_event(self):
    for i in range(20):
      for j in range(10): # make an array of 20X100 to store temporary data into to present visually
	tmp_frq[i].append(0) # dummy data to only hold ten values
	    
  def refresh_tmp():
    for i in range(20):
      for j in range(10):
	tmp_frq[i][j] = 0 # reset values on the 
  
  def exit(self):
    print "-------------------\nClosing the connection\n-------------------\n"
    self.dev.close()
    return -1