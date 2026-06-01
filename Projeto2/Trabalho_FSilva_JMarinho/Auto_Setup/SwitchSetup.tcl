On GTKTerm

/dev/ttyS0 115200

Reset:
    /system reset-configuration

user: admin
pass: [blank]

Create bridge:
    interface bridge add name=[bridge_name]

Remove ports from bridge:
    interface bridge port remove [find interface=[PORT]]

Add ports to bridge:
    interface bridge port add bridge=[bridge_name] interface=[PORT]

Show bridges and ports:
    interface bridge port print

---------------------------------------------------------------------

# reset
/system reset-configuration

# remover ports default
/interface bridge port remove [find interface=ether8]
/interface bridge port remove [find interface=ether16]
/interface bridge port remove [find interface=ether21]
/interface bridge port remove [find interface=ether23]
/interface bridge port remove [find interface=ether24]

# criar bridges e ports
    # bridge120
        # Tux123E1
        # Tux124E1
/interface bridge add name=bridge120
/interface bridge port add bridge=bridge120 interface=ether16
/interface bridge port add bridge=bridge120 interface=ether24
    # bridge121
        # Tux122E1
        # Tux124E2
        # Tux124E2
/interface bridge add name=bridge121
/interface bridge port add bridge=bridge121 interface=ether8
/interface bridge port add bridge=bridge121 interface=ether23
/interface bridge port add bridge=bridge121 interface=ether21


teste