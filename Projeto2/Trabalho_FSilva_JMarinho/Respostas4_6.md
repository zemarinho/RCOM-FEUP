# Exp 4 - Configure a Commercial Router and Implement NAT

## How to configure a static route in a commercial router?

    Depois de configurar as interfaces do router, usar o comando:

        /ip route add dst-address=172.16.120.0/24 gateway=172.16.121.253

## What are the paths followed by the packets, with and without ICMP redirect enabled, in the experiments carried out and why?

    Com redirect enable:
                Tux2        --->    Router    --->              Tux4              --->    Tux3
                172.16.121.1 -> 172.16.121.254 -> 172.16.121.253 -> 172.16.120.254 -> 172.16.120.1
    Com redirect disable:
                Tux2        --->              Tux4              --->    Tux3
                172.16.121.1 -> 172.16.121.253 -> 172.16.120.254 -> 172.16.120.1

## How to configure NAT in a commercial router?

    Para desativar NAT, usar comando:
        /ip firewall nat disable 0

    Para ativar NAT, usar comando:
        /ip firewall nat enable 0

## What does NAT do?

    Faz a tradução entre endereços IP privados e endereços IP públicos

    Exemplo:
        -COM_A: tux2 (IP 172.16.121.1) está a aceder a um site através do seu porto 2001
        -COM_B: tux4 (IP 172.16.121.253) está a aceder a outro site através do seu porto 3020
        -O router (IP 172.16.1.121) troca cada um dos IP dos tux's pelo seu próprio IP e associa a cada ligação uma das suas portas
        As comunicações, vistas da rede pública, ficam algo como:
        -COM_A: IP 172.16.1.121 porto 2100
        -COM_A: IP 172.16.1.121 porto 4444

    No fundo, a NAT mascara os IP locais de origem com o IP do router que faz a comunicação com a rede externa

## What happens when tuxY3 pings the FTP server with the NAT disabled? Why?

    O tux3 envia o ping mas não obtém resposta porque o servidor FTP não sabe como aceder à rede do tux3 sem a NAT para lhe mascarar o IP com o do router
    Para a comunicação poder funcionar sem a NAT seria necessário configurar uma rota no servidor FTP que usasse a interface pública do router como gateway para aceber à rede do tux3

# Exp 5 - DNS

## How to configure the DNS service in a host?

    Editar o ficheiro /etc/resolv.conf para conter:
        "nameserver [IP_do_servidor_DNS]"

## What packets are exchanged by DNS and what information is transported?

    São trocados pacotes DNS entre o cliente e o servidor que transportam:
        -endereço IPV4 e IPV6 associados a "google.com"
        -o cliente pergunta qual é o nome associado ao IP 142.250.181.174 e o servidor responde

# Exp 6 - TCP connections

## How many TCP connections are opened by your FTP application?

    São abertas duas conexões TCP

## In what connection is transported the FTP control information?

