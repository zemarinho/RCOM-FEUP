No GTKTerm

/dev/ttyS0 115200

Reset:
    system reset-configuration

user: admin
pass: [blank]

Criar Bridge:
    interface bridge add name=[bridge_name]

Remover portas para adicionar à bridge desejada:
    interface bridge port remove [find interface =[PORT]]

Adicionar as portas à bridge:
    interfate bridge port add bridge=[bridge_name] interface=[PORT]

Imprimir as ligações entre bridges e portas:
    interface bridge port print

---------------------------------------------------------------------
