#!/bin/bash

systemctl restart networking

ifconfig if_e1 172.16.120.254/24

ifconfig if_e2 172.16.121.253/24

sysctl net.ipv4.ip_forward=1
 
sysctl net.ipv4.icmp_echo_ignore_broadcasts=0

iptables -P FORWARD ACCEPT

iptables -F FORWARD

# add route to FTP Server through Router
route add -net 172.16.1.0/24 gw 172.16.121.254
