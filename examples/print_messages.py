"""
Continuously print all received CAN messages.

Usage: Paste into the Script window and click Run while a measurement is active.
Press Stop to end the script.
"""
import cangaroo

# Print available interfaces
for iface in cangaroo.interfaces():
    print(f"Interface {iface['id']}: {iface['name']}")

print("Waiting for messages...\n")

while True:
    # receive() blocks up to timeout (seconds), returns a list of messages
    messages = cangaroo.receive(timeout=1.0)

    for msg in messages:
        iface = cangaroo.interface_name(msg.interface_id)
        data_hex = msg.get_data().hex(' ')

        if msg.extended:
            id_str = f"0x{msg.id:08X}"
        else:
            id_str = f"0x{msg.id:03X}"

        flags = []
        if msg.fd:
            flags.append("FD")
        if msg.brs:
            flags.append("BRS")
        if msg.rtr:
            flags.append("RTR")
        flag_str = f" [{','.join(flags)}]" if flags else ""

        direction = "RX" if msg.is_rx else "TX"

        print(f"{msg.timestamp:.6f}  {iface}  {direction}  {id_str}  "
              f"[{msg.dlc}]{flag_str}  {data_hex}")
