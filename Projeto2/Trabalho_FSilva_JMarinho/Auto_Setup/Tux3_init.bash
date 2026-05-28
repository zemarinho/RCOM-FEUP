#!/bin/bash

systemctl restart networking

ifconfig if_e1 172.16.120.1/24

iptables -P FORWARD ACCEPT

iptables -F FORWARD

# add routes
    # to bridge121 through Tux124E1
route add -net 172.16.121.0/24 gw 172.16.120.254
    # to FTP Server through Router
route add -net 172.16.1.0/24 gw 172.16.120.254
