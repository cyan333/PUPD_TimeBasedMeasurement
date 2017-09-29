# invoke SourceDir generated makefile for pinInterrupt.pem3
pinInterrupt.pem3: .libraries,pinInterrupt.pem3
.libraries,pinInterrupt.pem3: package/cfg/pinInterrupt_pem3.xdl
	$(MAKE) -f /Users/Fangming/Documents/PUPD_Project/uC/PUPD_TimeBasedMeasurement/src/makefile.libs

clean::
	$(MAKE) -f /Users/Fangming/Documents/PUPD_Project/uC/PUPD_TimeBasedMeasurement/src/makefile.libs clean

