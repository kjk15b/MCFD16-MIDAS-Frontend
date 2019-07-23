'''
This program serves as the main routine for I/O on the MCFD16

Kolby Kiesling
kjk15b@acu.edu
07 / 22 / 2019
'''

import mcfd_serial as mcfd

def main():
  udev = str(input("User device? "))
  dev = mcfd.MCFD(udev) # construct the object
  dev.clear_pulser()
  dev.initialize() # initialize the device
  print dev.exit()
main()