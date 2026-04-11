"""
Build and send CAN messages using DBC signal encoding.

Demonstrates:
  - cangaroo.encode()        — build a Message from named signal values
  - cangaroo.signal_value()  — extract a single signal's physical value
  - cangaroo.find_message()  — look up a DBC message definition by name or ID

Usage: Load demo.dbc in Measurement Setup, start the measurement, then run
this script.  Adjust interface_id to match your setup.
"""
import cangaroo
import time

INTERFACE_ID = 0   # change to match your setup

# ---- inspect a message definition before encoding ----
defn = cangaroo.find_message("EngineData")
if defn is None:
    print("EngineData not found — is demo.dbc loaded?")
else:
    print(f"Message: {defn['message']}  ID=0x{defn['id']:03X}  DLC={defn['dlc']}")
    for sig in defn["signals"]:
        print(f"  {sig['name']:20s}  [{sig['start_bit']}|{sig['length']}]"
              f"  factor={sig['factor']}  offset={sig['offset']}"
              f"  unit={sig['unit']!r}")
    print()

# ---- encode and send EngineData ----
engine_msg = cangaroo.encode("EngineData", {
    "EngineSpeed": 3500.0,   # rpm
    "EngineTemp":  90.0,     # degC
    "OilPressure": 4.2,      # bar
})
cangaroo.send(engine_msg, interface_id=INTERFACE_ID)
print(f"Sent EngineData:  {engine_msg}")

# Verify round-trip: decode the just-sent message
speed = cangaroo.signal_value(engine_msg, "EngineSpeed")
temp  = cangaroo.signal_value(engine_msg, "EngineTemp")
oil   = cangaroo.signal_value(engine_msg, "OilPressure")
print(f"  EngineSpeed={speed} rpm  EngineTemp={temp} degC  OilPressure={oil} bar")
print()

# ---- encode and send TransmissionData ----
# GearPos has value names: 0=Neutral 1=First … 7=Reverse
for gear_pos, speed_kmh in [(1, 15.0), (2, 35.0), (3, 60.0), (0, 0.0)]:
    tx = cangaroo.encode("TransmissionData", {
        "GearPos":      float(gear_pos),
        "VehicleSpeed": speed_kmh,
    })
    cangaroo.send(tx, interface_id=INTERFACE_ID)
    decoded = cangaroo.decode(tx)
    gear_name = decoded["signals"]["GearPos"].get("value_name", str(gear_pos))
    print(f"Sent TransmissionData: gear={gear_name}  speed={speed_kmh} km/h  {tx}")
    time.sleep(0.05)

print()

# ---- encode by raw ID instead of name ----
ambient_msg = cangaroo.encode(768, {   # 0x300 = AmbientData
    "OutsideTemp": 21.5,   # degC
    "Humidity":    65.0,   # %
})
cangaroo.send(ambient_msg, interface_id=INTERFACE_ID)
print(f"Sent AmbientData (by ID):  {ambient_msg}")
print(f"  OutsideTemp={cangaroo.signal_value(ambient_msg, 'OutsideTemp')} degC"
      f"  Humidity={cangaroo.signal_value(ambient_msg, 'Humidity')} %")

print("\nDone.")
