/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   chunked_test.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jucoelho <jucoelho@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/21 18:12:42 by jucoelho          #+#    #+#             */
/*   Updated: 2026/07/21 20:00:40 by jucoelho         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */



#include "http/RequestParser.hpp"
#include "http/HttpRequest.hpp"
#include "utils/Logger.hpp"
#include <iostream>
#include <string>
#include <sstream>

// Helper para converter int para string (C++98 compatible)
static std::string intToString(int value)
{
    std::stringstream ss;
    ss << value;
    return ss.str();
}

void testChunkedParser()
{
	/*
    Logger::info("\n=== Teste DEBUG: O que está acontecendo em prs_chunked_data ===");
    {
        RequestParser parser;
        std::string request = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n0\r\n\r\n";
        
        parser.feed(request.c_str(), request.length());
        
        Logger::info("State: " + intToString(parser.get_psr_state()) + " (esperado 6=COMPLETE)");
        Logger::info("Body received: '" + parser.getRequest().getBody() + "'");
        Logger::info("Body expected: 'Hello'");
        Logger::info("Body length: " + intToString(parser.getRequest().getBody().length()));
    }

    Logger::info("\n=== Teste 1: Chunked simples (inteiro de uma vez) ===");
    {
        RequestParser parser1;
        std::string request1 = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n";
        
        parser1.feed(request1.c_str(), request1.length());
        Logger::info("Buffer size: " + intToString(parser1.getRequest().getBody().length()));
        Logger::info("State: " + intToString(parser1.get_psr_state()));
        if (parser1.get_psr_state() != COMPLETE) {
            Logger::error("Expected COMPLETE state (6)");
        } else {
            const HttpRequest& req = parser1.getRequest();
            Logger::info("Body: '" + req.getBody() + "'");
            Logger::info("Expected: 'Hello World'");
        }
    }

    Logger::info("\n=== Teste 2: Chunked em pedaços (3 recv) ===");
    {
        RequestParser parser2;
        
        std::string chunk1 = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHel";
        std::string chunk2 = "lo\r\n6\r\n Wor";
        std::string chunk3 = "ld\r\n0\r\n\r\n";
        
        parser2.feed(chunk1.c_str(), chunk1.length());
        parser2.feed(chunk2.c_str(), chunk2.length());
        parser2.feed(chunk3.c_str(), chunk3.length());
        
        if (parser2.get_psr_state() == COMPLETE) {
            const HttpRequest& req = parser2.getRequest();
            Logger::info("Body: '" + req.getBody() + "'");
            Logger::info("Expected: 'Hello World'");
        } else {
            Logger::error("Expected COMPLETE state (6) but got: " + intToString(parser2.get_psr_state()));
        }
    }

    Logger::info("\n=== Teste 3: Chunked + Content-Length (deve erro) ===");
    {
        RequestParser parser3;
        std::string badRequest = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\nContent-Length: 11\r\n\r\n5\r\nHello\r\n6\r\n World\r\n0\r\n\r\n";
        parser3.feed(badRequest.c_str(), badRequest.length());
        if (parser3.get_psr_state() == ERROR)
            Logger::info("✓ Correctly detected error. Error code: " + intToString(parser3.get_error_code()));
        else
            Logger::warning("✗ Expected ERROR state (7) but got: " + intToString(parser3.get_psr_state()));
    }

    Logger::info("\n=== Teste 4: Chunked malformado (dados != size) ===");
    {
        RequestParser parser4;
        std::string malformed = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHelloWorld\r\n0\r\n\r\n";
        parser4.feed(malformed.c_str(), malformed.length());
        if (parser4.get_psr_state() == ERROR)
            Logger::info("✓ Correctly detected malformed chunk. Error code: " + intToString(parser4.get_error_code()));
        else
            Logger::warning("✗ Expected ERROR state (7) but got: " + intToString(parser4.get_psr_state()));
    }

    Logger::info("\n=== Teste 5: Chunked vazio ===");
    {
        RequestParser parser5;
        std::string empty = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n";
        parser5.feed(empty.c_str(), empty.length());
        if (parser5.get_psr_state() == COMPLETE)
            Logger::info("✓ Body: '" + parser5.getRequest().getBody() + "' (empty as expected)");
        else
            Logger::error("✗ Expected COMPLETE but got: " + intToString(parser5.get_psr_state()));
    }

    Logger::info("\n=== Teste 6: Múltiplos chunks ===");
    {
        RequestParser parser6;
        std::string request = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nTest\r\n1\r\n!\r\n5\r\nHello\r\n0\r\n\r\n";
        parser6.feed(request.c_str(), request.length());
        if (parser6.get_psr_state() == COMPLETE) {
            Logger::info("✓ Body: '" + parser6.getRequest().getBody() + "'");
            Logger::info("Expected: 'Test!Hello'");
        } else {
            Logger::error("✗ Expected COMPLETE but got: " + intToString(parser6.get_psr_state()));
        }
    }

    Logger::info("\n=== Teste 7: Chunked com CRLF dentro do payload (Julia) ===");
    {
        RequestParser parser7;
        std::string request = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n11\r\njulia\r\nlalalala\r\n\r\n0\r\n\r\n";
        parser7.feed(request.c_str(), request.length());
        
        if (parser7.get_psr_state() == COMPLETE) {
            std::string body = parser7.getRequest().getBody();
            if (body == "julia\r\nlalalala\r\n")
                Logger::info("✓ Sucesso: O CRLF interno foi preservado perfeitamente!");
            else
                Logger::error("✗ Falha: O corpo está diferente do esperado.");
        } else {
            Logger::error("✗ Expected COMPLETE but got: " + intToString(parser7.get_psr_state()));
        }
    }
*/
    // =========================================================================
    // BATERIA DE STRESS (Orientada a Dados)
    // =========================================================================
    Logger::info("\n=== BATERIA DE STRESS: Casos de Borda Chunked ===");

    struct ChunkedTest {
        std::string name;
        std::string payload;
        t_psr_state expected_state;
        std::string expected_body;
    };

    ChunkedTest tests[] = {
        {"1. Único caractere", "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n1\r\na\r\n0\r\n\r\n", COMPLETE, "a"},
        {"2. Chunk Vazio", "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n", COMPLETE, ""},
        {"3. Dois Chunks pequenos", "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n1\r\na\r\n1\r\nb\r\n0\r\n\r\n", COMPLETE, "ab"},
        {"4. Hex Maiúsculo", "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\nA\r\n0123456789\r\n0\r\n\r\n", COMPLETE, "0123456789"},
        {"5. Hex Minúsculo", "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\na\r\n0123456789\r\n0\r\n\r\n", COMPLETE, "0123456789"},
        {"6. Hex com zeros à esquerda", "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n000000005\r\nHello\r\n0\r\n\r\n", COMPLETE, "Hello"},
        {"7. Hex muito grande", "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n10\r\n1234567890123456\r\n0\r\n\r\n", COMPLETE, "1234567890123456"},
        {"8. CRLF no Início", "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n4\r\n\r\nAB\r\n0\r\n\r\n", COMPLETE, "\r\nAB"},
        {"9. CRLF no Fim", "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nAB\r\n\r\n0\r\n\r\n", COMPLETE, "AB\r\n"},
        {"10. Apenas CRLFs", "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n4\r\n\r\n\r\n\r\n0\r\n\r\n", COMPLETE, "\r\n\r\n"},
		{"11. Nulos (Binary)", std::string("POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n3\r\na") + std::string("\0b", 2) + std::string("\r\n0\r\n\r\n"), COMPLETE, std::string("a\0b", 3)},        {"12. Hexadecimal Inválido", "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\nG\r\n1234567890123456\r\n0\r\n\r\n", ERROR, ""},
        {"12. Hexadecimal Inválido", "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\nG\r\n1234567890123456\r\n0\r\n\r\n", ERROR, ""},
		{"13. Faltando CRLF apos tamanho", "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n5Hello\r\n0\r\n\r\n", ERROR, ""},
        {"14. Faltando CRLF apos dados", "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nHello0\r\n\r\n", ERROR, ""},
        {"15. Julia CRLF Interno", "POST / HTTP/1.1\r\nHost: a\r\nTransfer-Encoding: chunked\r\n\r\n11\r\njulia\r\nlalalala\r\n\r\n0\r\n\r\n", COMPLETE, "julia\r\nlalalala\r\n"}
    };

    int num_tests = sizeof(tests) / sizeof(tests[0]);
    int passed = 0;

    for (int i = 0; i < num_tests; i++) {
        RequestParser p;
        p.feed(tests[i].payload.c_str(), tests[i].payload.length());
        
        bool state_ok = (p.get_psr_state() == tests[i].expected_state);
        bool body_ok = true;
        
        if (tests[i].expected_state == COMPLETE) {
            body_ok = (p.getRequest().getBody() == tests[i].expected_body);
        }

        if (state_ok && body_ok) {
            Logger::info("✓ Passou: " + tests[i].name);
            passed++;
        } else {
            Logger::error("✗ FALHOU: " + tests[i].name);
        }
    }

    Logger::info("\n=== RESULTADO STRESS: " + intToString(passed) + "/" + intToString(num_tests) + " PASSARAM ===");
}

// A função MAIN exclusiva deste arquivo de teste
int main()
{
    Logger::info("Iniciando bateria de testes do RequestParser...");
    testChunkedParser();
    Logger::info("Testes finalizados.");
    return 0;
}