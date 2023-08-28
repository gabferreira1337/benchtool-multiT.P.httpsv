#CLIENT.C

### Implementação de um cliente multithread para benchmarking de servidores web. Cada thread envia várias solicitações HTTP GET e o programa coleta e analisa os tempos de resposta para avaliar o desempenho do servidor.


#TWS.C
### Implementação de um servidor web  .Recorrendo ao problema do produtor-consumidor, onde a main thread configura um socket , escuta conexões de entrada e gera threads para lidar com solicitações de clientes (PontoB). E Recorrendo também a uma máquina de estados finitos (FSM) , onde a main thread cria uma pool de threads ,onde cada thread que atende o pedido HTTP executa todos os passos até ao término da ligação (PontoC.a).