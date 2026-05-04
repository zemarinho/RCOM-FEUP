#!/bin/bash

systemctl restart networking

ifconfig if_e1 172.16.121.1/24
