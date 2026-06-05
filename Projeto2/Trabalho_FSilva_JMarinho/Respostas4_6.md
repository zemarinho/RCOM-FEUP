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

    Na primeira conexão

## What are the phases of a TCP connection?

    Abertura da ligação: conexão é establecida entre cliente e o servidor e são trocados parâmetros como a janela inicial de receção, o tamanho máximo de cada segmento, o fator de escala da janela, ou a permisão (ou não) de usar SACK

    Transferência dos dados: é realizada a troca de dados do cliente para o servidor ou do servidor para o cliente

    Fecho da ligação: a conexão é terminada

## How does the ARQ TCP mechanism work? What are the relevant TCP fields? What relevant information can be observed in the logs?

### Como funciona?

    Através de dois processos diferentes:
        -Faz uma retransmissão caso o temporizador de retransmissão terminar antes de ser recebido um ACK. Neste caso é retransmitido o segmento não confirmado com o número de segmento mais baixo

        -Faz a retransmissão do segmento N sem esperar o timeout no caso de receber 3 ACK's consecutivos para o segmento N-1

### Quais são os campos relevantes?

    Os campos relevantes para o mecanismo ARQ são:
        -Número de sequência
        -Número de confirmação
        -Flag ACK
        -Opções (SACK)

### Que informação relevante se observa nos logs?

    -Hand-shake de 3 vias
    -Os ACK's comulativos
    -Retransmissão após timeout
    -Variação da janela deslizante
    -Reenvio de pacotes perdidos

## How does the TCP congestion control mechanism work? What are the relevant fields. How did the throughput of the data connection evolve along the time? Is it according to the TCP congestion control mechanism?

### Como funciona o mecanismo?

    É gerido pelo emissor, que tem em conta o valor mínimo entre a janela de congestão e a janela de receção.
    O emissor controla o valor da janela de congestão, que inicialmente aumenta de forma multiplicativa, começando em 1 MSS/RTT (fase de slow start), e passando para auemtno de forma aditiva, +1 MSS/RTT, assim que a dimensão da janela atinge o valor do threshold de referêcnia (fase de congestion avoidance). Isto acontece na ausência de perdas.
    Quando há perdas, a janela de congestão diminui de forma multiplicativa.

### Quais são os campos reelvantes?

    -Número de confirmação
    -Flag ACK
    -Tamanho da janela (menor entre rwnd e cwnd)
    -Opções (SACK) (importante para fast recovery)

### Como evoluiu o throughput?

    O throuput médio aumentou de forma rápida e depois estabilizou em volta de 12.5 pacotes por milissegundo até ao fim da transmissão

### Está de acordo com o mecanimsmo de controlo de congesão?

    Sim, o número de pacotes enviados por unidade de tempo é limitado pela rede após o slow start seguido do crescimento exponencial

## Is the throughput of a TCP data connections disturbed by the appearance of a second TCP connection? How?

    Não. Pode ser explicado por eventualmente a segunda comunicação ter sido começada demasiado cedo o que levou a saturação e devido a isso não se nota decrescimento. Outra explicação possível é que a segunda transmissãosimplesmente ocupou o espaço deixado pela primeira, em vez de ambas se equilibrarem em percentagem do uso da rede