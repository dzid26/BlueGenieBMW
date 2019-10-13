# BlueGenie BMW <br>
Bluetooth audio for BMW with CAN-bus support for steering wheel buttons control. Supported cars should be E90, E91, E92, E93, E82, E81, (also probably 5, 7-series, maybe E46).   <br> <br>

Hardware thingies: https://github.com/dzid26/BlueGenieBMW/tree/master/Hardware <br>
-ESP8266 - (Wemos D1 mini) ($3) <br>
-BK3254 (BT4.1 A2DP audio) - with UART enabled firmware (the ebay ones should be ok) ($2) <br>
-MCP2515 board with some transciever ($2) <br> <br>

~~Since there is no PCB project yet, for lazy people I recommend buying $3 BK8000L with a board and audio jack. Desoldier the BK8000L and replace with BK3254. Why not use BK8000L directly? - the chinese chips seem to have UART disabled and it is unclear how to easly change firmware.~~
 <br> <br>
Connect things together. Use this PCB for convenience:
https://oshpark.com/shared_projects/S17iaoNp
 <br> <br>


Features: <br>
-Next / Previous - (CAN-bus) steering buttons  <br>
-OTA over wifi <br>
 <br>
 <br>
Installation: <br>
Find K-CAN ("chasis" CAN-bus) - twisted green (CAN HIGH) and orange (CAN LOW) cables behind the radio and splice it and connect to MCP2515.  <br>
Make some USB power for the NodeMCU.



Some info regarding K-CAN - it is @100kbps  <br>
https://us.autologic.com/news/bmw-instrument-cluster-lights-and-the-k-bus-failure  <br>
For CAN messages, refer to https://github.com/dzid26/opendbc-BMW-E8x-E9x/blob/master/bmw_e9x_e8x.dbc.
