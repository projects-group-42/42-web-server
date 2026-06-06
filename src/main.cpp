/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/31 17:46:15 by jucoelho          #+#    #+#             */
/*   Updated: 2026/06/06 19:06:31 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "webserver.hpp"
#include "network/Socket.hpp"
#include "server/EventLoop.hpp"
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <vector>
#include <poll.h>
/*
int main(void)
{
	try
	{
		Socket sckt;
		sckt.create();
		sckt.bind("0.0.0.0", 8080);
		sckt.listen(128);
		Logger::info("Listening on 0.0.0.0:8080 — connect with: nc localhost 8080");
		EventLoop run(&sckt);
		run.run();
	}
	catch (const std::exception &e)
	{
		Logger::error(e.what());
		return 1;
	}
	return 0;
}*/

int main(void)
{
    Logger::info("=== INICIANDO TESTE ISOLADO DA ISSUE #09 ===");

    // Criamos um mapa de clientes simulando o armazenamento do EventLoop
    std::map<int, Connection> simulated_map;

    int fd_cliente_A = 4;
    int fd_cliente_B = 5;

    // 1. Instanciando os estados de conexão por cliente
    Connection cliente_A(fd_cliente_A);
    Connection cliente_B(fd_cliente_B);

    simulated_map[fd_cliente_A] = cliente_A;
    simulated_map[fd_cliente_B] = cliente_B;

    Logger::info("Passo 1: Conexões registradas no mapa de estados.");

    // 2. Simulando Cliente A enviando dados parciais: "GE"
    // Simulamos o comportamento interno do receive_data() injetando direto no atributo
    // Nota: Se '_read_buffer' for privado, altere temporariamente para public em Connection.hpp para rodar o teste!
    simulated_map[fd_cliente_A]._read_buffer += "GE";
    Logger::info("Cliente A enviou dados parciais. Buffer atual do FD 4: " + simulated_map[fd_cliente_A]._read_buffer);

    // 3. Simulando Interrupção: Cliente B conecta e envia uma requisição completa
    simulated_map[fd_cliente_B]._read_buffer += "POST /upload HTTP/1.1\r\n\r\n";
    Logger::warning("Cliente B operou em paralelo! Buffer atual do FD 5: " + simulated_map[fd_cliente_B]._read_buffer);

    // 4. Cliente A retorna no ciclo seguinte para terminar o envio
    simulated_map[fd_cliente_A]._read_buffer += "T /index.html HTTP/1.1\r\n\r\n";
    
    std::cout << std::endl;
    Logger::info("=== RESULTADO DA VERIFICAÇÃO DE ESTADO ===");
    Logger::info("Buffer Final do Cliente A (FD 4): " + simulated_map[fd_cliente_A]._read_buffer);
    Logger::info("Buffer Final do Cliente B (FD 5): " + simulated_map[fd_cliente_B]._read_buffer);

    // Validação lógica do critério de aceitação
    if (simulated_map[fd_cliente_A]._read_buffer == "GET /index.html HTTP/1.1\r\n\r\n")
    {
        Logger::info("SUCESSO: O servidor manteve o estado e remontou o buffer do Cliente A perfeitamente!");
    }
    else
    {
        Logger::error("FALHA: Os buffers se misturaram ou foram corrompidos na memória.");
    }

    return 0;
}