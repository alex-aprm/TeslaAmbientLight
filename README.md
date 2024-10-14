# Overview
I want ambient light in my Tesla Model 3. More, I want it to react to car controls.
## Ideas:
- Show blind spot alerts when changing lanes
- Show turn signals
- Show gear by short animation
- Indicate open doors
- Indicate seat heat
- Indicate unlatched seatbelt
- Whatever...
It should also response to day/night ambient light, don't disrupt or disturb, and look fancy.
Installation without cutting any wire.

# Demo
![image](https://github.com/user-attachments/assets/cbf0db33-2318-4a28-8039-147283e58a8a)

https://www.youtube.com/shorts/Xc-fdAo0e8E

# Concept

ESP32 as master module, connected to Vehicle CAN and Chassis CAN with two MCP2515.

4 ESP32 in each door connected to master module and controlling LED strips.
I've decided to put ESP32 in each door because central ESP32 can't control address LED strip due to electric noise. Too long wires to work with shift register.
Reasonable solution is to use CAN, but I don't want to put another wires.

My TM3 is RWD, it has door pocket lights but they are inactive. I took these wires for power delivery. See "wiring" below.

# Wiring

## Car connectors and pins
## Master module power
## Door module power
## CAN wiring

# Materials required

# Used projects
https://github.com/joshwardell/model3dbc

https://github.com/coryjfowler/MCP_CAN_lib

https://github.com/nmullaney/candash
