#!/usr/bin/expect -f

set timeout 5

spawn screen /dev/ttyUSB0 115200

expect {
    -re ".*Login:.*" {}
    timeout { exit 1 }
}

send "admin\r"

expect {
    -re ".*Password:.*" {}
    timeout { exit 1 }
}

send "\r"

expect {
    -re ".*>.*" {}
    timeout { exit 1 }
}

send "system reset-configuration\r"

expect {
    -re ".*Do you want to continue.*" {}
    timeout { exit 1 }
}

send "y\r"

expect {
    -re ".*Press any key.*" {}
    timeout { exit 1 }
}

# fechar comunicação completamente
close
wait
exit 0