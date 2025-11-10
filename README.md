# SWRMeter
PCB files, 3D printing files, and Arduino program to replace the traditional crossed needles SWR meter.

This project was originally done to replace the crossed needle meter on my LDG AT-11MP Tuner, as the meter was broken and replacement parts no longer available.

<img width="715" height="499" alt="image" src="https://github.com/user-attachments/assets/6fffd180-17fd-496f-b042-2c2ed28dae18" />

# To Calibrate
1: Compile with _CALIBRATE_SHOW_COUNTS == 1

2: Attach meter to DUT, transmit into dummy load using FM at 5W and record number on display.

3: Repeat for power values listed in LUT_FWD[] (you can extrapolate if your transmitter only goes so high)

4: Transmit backwards (INTO antenna port with dummy load on TX port) and record number for Ref Pwr. (May want to limit power and extrapolate)

5: Enter recorded values into LUT_FWD[] and LUT_REF[], set _CALIBRATE_SHOW_COUNTS == 0, and compile.
