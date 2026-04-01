#!/bin/bash
# Setup two virtual CAN interfaces with bidirectional gateway
# Messages sent on vcan0 appear on vcan1 and vice versa
# Requires: can-utils (cangw), iproute2

set -e

modprobe vcan
modprobe can-gw

# Create interfaces
ip link add dev vcan0 type vcan
ip link add dev vcan1 type vcan

ip link set up vcan0
ip link set up vcan1

# Bidirectional gateway: vcan0 <-> vcan1
cangw -A -s vcan0 -d vcan1 -e
cangw -A -s vcan1 -d vcan0 -e

echo "vcan0 <-> vcan1 gateway active"
echo "Test: cansend vcan0 123#DEADBEEF  ->  candump vcan1"
