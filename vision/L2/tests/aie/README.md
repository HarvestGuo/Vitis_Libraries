## Steps to run AIE testcases using Makefile

The following steps need to be followed to run all Vitis Vision AIE testcases using Makefile. These are to performed only after all the prerequisites mentioned in top-level [README](https://github.com/Xilinx/Vitis_Libraries/blob/master/vision/README.md#prerequisites) are met.

1. Open a bash terminal
2. *cd* to any testcase under *L2/tests/aie/< function-name>/* (example: L2/tests/aie/Filter2D/16bit_aie_8bit_pl)
3. Setup the environment using below commands:
	* *source < path-to-Vitis-installation-directory >/settings64.sh*
		- Vitis™ 2022.2 or later version supported.
	* *export PLATFORM=< path-to-vck190 platform-directory >/< vck190 platform >.xpfm*
	* *source < path-to-XRT-installation-directory >/setup.sh*
	* *export SYSROOT=< path-to-versal-platform-sysroot >*
4. Hardware Emulation:<br/>
   
   	*make run TARGET=hw_emu*

   x86 Simulation:<br/>
   
   	*make run TARGET=x86sim*

   AIE Simulation:<br/>
   
   	*make run TARGET=aiesim*
   
   Hardware run:<br/>
   
   	*make run TARGET=hw* (This command only builds the testcase, doesn't run it)

   After the build for hardware target completes, sd_card.img file will be generated in the build directory.<br/>

   a. Use a software like Etcher to flash the sd_card.img file on to a SD Card<br/>
   b. After flashing is complete, insert the SD card in the SD card slot on board and power on the board<br/>
   c. Use Teraterm to connect to COM port and wait for the system to boot up<br/>
   d. After the boot up is done, goto /run/media/sd-mmcblk0p1 directory and run the script ./run_script.sh. This will invoke the application executable<br/>



## Steps to run AIE testcases in Vitis GUI

The following steps need to be followed to run all Vitis Vision AIE testcases (except AIE-PL cases) in Vitis GUI. These are to performed only after all the prerequisites mentioned in top-level [README](https://github.com/Xilinx/Vitis_Libraries/blob/master/vision/README.md#prerequisites) are met.

1. Open a bash terminal
2. *cd* to any testcase under *L2/tests/aie/< function-name>/* 
3. Setup the environment using below commands:
	* *source < path-to-Vitis-installation-directory >/settings64.sh*
		- Vitis™ 2022.2 or later version supported.
	* *export PLATFORM=< path-to-vck190 platform-directory >/< vck190 platform >.xpfm*
	* *source < path-to-XRT-installation-directory >/setup.sh*
	* *export SYSROOT=< path-to-versal-platform-sysroot >*
	
4. Open *description.json* file present in the same directory and change value of attribute "gui" to *true*.
5. Open Vitis GUI from the same terminal using the command *vitis* and create application project for that aie function.
	* Add platform and related details:
		- Add platform to repository: < path-to-vck190 platform-directory >/< vck190 platform >.xpfm
		- Sysroot path: < path-to-versal-platform-sysroot >
		- Root FS: < path-to-versal-platform-rootfs >
		- Kernel Image:  < path-to-versal-platform-kernel-Image >

6. After project creation, open *host.cpp* from GUI ProjName[xrt] -> src folder. If *host.cpp* has *#include "graph.cpp"* then replace *graph.cpp* with its absolute path i.e. the absolute path for *graph.cpp* present in GUI ProjName_aie[aiengine] -> src folder. To get absolute path, right click on *graph.cpp* and select Properties. Under Properties, location gives its absolute path.
7. Right click on ProjName[xrt] and select *C/C++ Build Settings -> C/C++ Build -> Settings -> Tool Settings -> GCC Host Linker -> Libraries -> Library search path*
8. Under *Library search path* section, add *${workspace_loc:${ProjName}/libs/xf_opencv/L1/lib/sw/aarch64-linux/}*
9. Open a new bash terminal, *cd* to the same testcase directory and repeat step 3
10. Run the command : *make x86sim TARGET=hw_emu * (ignore simulation failures)

	This command generates *aie_control_xrt.cpp* that is needed to build AIE GUI project, at < current-directory>/Work/ps/c_rts/
11. Copy *aie_control_xrt.cpp* generated in above step and paste it in the GUI project folder where *host.cpp* is present.
12. Go to GUI project
	* Build Emulation-HW
	* Run hwemu-launch
