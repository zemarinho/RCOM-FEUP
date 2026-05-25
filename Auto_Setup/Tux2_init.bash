#!/bin/bash

systemctl restart networking

ifconfig if_e1 172.16.121.1/24

iptables -P FORWARD ACCEPT

iptables -F FORWARD

# add routes
    # to bridge120 through Tux124E2
route add -net 172.16.120.0/24 gw 172.16.121.253
    # to FTP Server through Router
route add -net 172.16.1.0/24 gw 172.16.121.254

sysctl net.ipv4.conf.eth1.accept_redirects=0
sysctl net.ipv4.conf.all.accept_redirects=0