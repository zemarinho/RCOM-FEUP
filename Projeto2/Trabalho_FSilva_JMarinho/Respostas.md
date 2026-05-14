# Exp 1- Configure an IP Network

## What are the ARP packets and what are they used for?

### Quais são

    Request->   Who has 172.16.120.1? Tell 172.16.120.254
    Reply->     172.16.120.254 is at ec:75:0c:c2:3c:ac
    Request->   Who has 172.16.120.254? Tell 172.16.120.1
    Reply->     172.16.120.1 is at ec:75:0c:c2:3c:75

### Para que servem

    Estes pacotes servem para fazer o mapeamento entre os endereços IP e
    os endereços MAC dentro de uma rede local

## What are the MAC and IP addresses of ARP packets and why?

### Quais são

    Tux3:
        - IP: 172.16.120.1
        - MAC: ec:75:0c:c2:3c:75

    Tux4:
        - IP: 172.16.120.254
        - MAC: ec:75:0c:c2:3c:ac

### Porquê

    Porque são os que aparecem no ficheiro de log do WireShark e porque
    coincidem com os endereços registados ao fazer o setup da rede

## What packets does the ping command generate?

    Pacotes ICMP. Pacotes que ajudam a identificar problemas na rede

## What are the MAC and IP addresses of the ping packets?

    Tux3:
        - IP: 172.16.120.1
        - MAC: ec:75:0c:c2:3c:75

    Tux4:
        - IP: 172.16.120.254
        - MAC: ec:75:0c:c2:3c:ac

## How to determine if a receiving Ethernet frame is ARP, IP, ICMP?

    Na coluna Protocol, no WireShark, está indicado o tipo de pacote utilizado

## How to determine the length of a receiving frame?

    Na coluna Length, no WireShark, está indicado o tamanho do pacote

## What is the loopback interface and why is it important?

### O que é

    É uma interface de rede virtual reservada que permite ao computador comunicar
    consigo próprio.

### Porque é importante

    Com isto é possível que diferentes serviços ou aplicações a correr
    no mesmo computador comuniquem entre si utilizando protocolos de redes standart

# Exp 2 - Implement two bridges in a switch

## How to configure bridge120?

    No MTKterm:
    - /interface bridge add name=bridgeY0
    - /interface bridge port remove [find interface =ether1]
    - /interface bridge port add bridge=bridgeY0 interface=ether1.

## How many broadcast domains are there?

    2

## How can you conclude it from the logs?

    Ao fazer broadcast a aprtir do tux3 apenas o tux 4 ouve
    Ao fazer broadcast a partir do tux2 mais nenhum tux ouve

# Exp 3 - Configure a Router in Linux

## What routes are there in the tuxes? What are their meaning?

    tux2:
        - Destination: 172.16.120.0; Gateway: 172.16.121.253
        . O tux2 consegue aceder à rede do tux3 (172.16.120.0) através
        da gateway 172.16.121.253 (ip do tux4, que establece a comunicação entre as duas redes)

    tux3:
        - Destination: 172.16.121.0; Gateway: 172.16.120.254
        . O tux3 consegue aceder à rede do tux2 (172.16.121.0) através
        da gateway 172.16.120.254 (ip do tux4, que establece a comunicação entre as duas redes)

## What information does an entry of the forwarding table contain?

    A tabela contém as rotas existente onde, para cada uma, podemos ver
    o Destino, a Gateway de acesso ao Destino, a Máscara da rede de destino,
    as Flags associada à rota, e as Interface que a rota usa.

## What ARP messages, and associated MAC addresses, are observed and why?

### Mensagens e endereços MAC

    tux3:
        16 - Who has 172.16.120.1? Tell 172.16.120.254
        17 - 172.16.120.1 is at ec:75:0c:c2:3c:ac
        18 - Who has 172.16.120.254? Tell 172.16.120.1
        19 - 172.16.120.254 is at ec:75:0c:c2:3c:75

        72 - Who has 172.16.120.1? Tell 172.16.120.254
        73 - 172.16.120.1 is at ec:75:0c:c2:3c:ac

    tux4 if_e1:
        25 - Who has 172.16.120.254? Tell 172.16.120.1
        26 - 172.16.120.254 is at ec:75:0c:c2:3c:75
        27 - Who has 12.16.120.1? Tell 172.16.120.254
        28 - 172.16.120.1 is at ec:75:0c:c2:3c:ac

    tux4 if_e2:
        31 - Who has 172.16.121.253? Tell 172.16.121.1
        32 - 172.16.121.253 is at ec:75:0c:c2:10:6b
        33 - Who has 12.16.121.1? Tell 172.16.121.253
        34 - 172.16.121.1 is at ec:75:0c:c2:31:73

### Why

    As mensagens ARP permitem mapear a que endereço MAC está associado cada endereço IP

## What ICMP packets are observed and why?

    Vários pacotes "Echo (ping) request" e "Echo (ping) reply"
    Inicialmente o ping do tux3 com destino ao tux2 aparece como uma resposta
    do tux4, após serem limpas as tabelas arp, as respostas ao ping do tux3 são
    registadas como tendo origem no IP associado ao tux2; Isto acontece porque
    ao ser redirecionada a mensagem de ping recebe o IP do tux4, que a reenvia
    Depois de serem apagadas as tabelas arp, os campos de Destino e Origem das
    mensagens já aparecem com os valores corretos de IP do tux2 e do tux3

## What are the IP and MAC addresses associated to ICMP packets and why?

    tux3 if_e1 172.16.120.1 - ec:75:0c:c2:3c:ac
    tux4 if_e1 172.16.120.254 - ec:75:0c:c2:3c:75
    tux4 if_e2 172.16.121.253 - ec:75:0c:c2:10:6b
    tux2 if_e1 172.16.121.1 - ec:75:0c:c2:31:73
