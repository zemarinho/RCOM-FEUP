#!/bin/bash

systemctl restart networking

ifconfig if_e1 172.16.121.1/24

iptables -P FORWARD ACCEPT

iptables -F FORWARD

route add -net 172.16.120.0/24 gw 172.16.121.253

route add -net 172.16.1.0/24 gw 172.16.121.254
