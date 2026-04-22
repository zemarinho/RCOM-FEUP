# Exp 1- Configure an IP Network

## What are the ARP packets and what are they used for?

### Quais são

    Request->   Who has 172.16.120.1? Tell 172.16.120.254
    Reply->     172.16.120.254 is at ec:75:0c:c2:3c:ac
    Request->   Who has 172.16.120.254? Tell 172.16.120.1
    Reply->     172.16.120.1 is at ec:75:0c:c2:3c:75

### Para que servem

    Estes pacotes servem para fazer o mapeamento entre os endereços IP e os endereços MAC dentro de uma rede local

## What are the MAC and IP addresses of ARP packets and why?

### Quais são

    Tux3:
        - IP: 172.16.120.1
        - MAC: ec:75:0c:c2:3c:75
    
    Tux4:
        - IP: 172.16.120.254
        - MAC: ec:75:0c:c2:3c:ac

### Porquê

    Porque são os que aparecem no ficheiro de log do WireShark e porque coincidem com os endereços registados ao fazer o setup da rede

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

    É uma interface de rede virtual reservada que permite ao computador comunicar consigo próprio.

### Porque é importante

    Com isto é possível que diferentes serviços ou aplicações a correr no mesmo computador comuniquem entre si utilizando protocolos de redes standart
<<<<<<< HEAD
    
=======

# haha

    eu mesmo
>>>>>>> 3abf45c57692f1fb6b1a8ad07e4277570b81d6c4
