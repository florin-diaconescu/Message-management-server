/*
-------------------------------------------------
-- Diaconescu Florin, 322CB, florin.diaconescu --
-------------------------------------------------
*/

extern "C"
{
	#include <stdio.h>
	#include <stdlib.h>
	#include <unistd.h>
	#include <string.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
  #include <netinet/tcp.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include "helpers.h"	
}

#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <iterator>

using namespace std;

// functie ce trateaza cazul in care se porneste clientul intr-un mod incorect
void usage(char *file)
{
	fprintf(stderr, "Usage: %s <client_id> <server_address> <server_port>\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar

	if (argc < 4)
	{
		usage(argv[0]);
	}

  // ma asigur ca nu va fi introdus un ID cu dimensiune mai mare de 10
  if (strlen(argv[1]) > 10)
  {
    printf(LONG_ID_ERR);
    return 0;
  }

  // golesc multimile de file descriptori
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

  // dezactivez algoritmul Nagle
  int b = 1;
  ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)(&b), sizeof(b));
  DIE(ret < 0, "setsockopt");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));

	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

  // ma conectez la server-ul de la adresa si portul dorite
	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

  // adaug in setul de descriptori socket-ul si stdin, pentru comenzile de la
  // tastatura
	FD_SET(sockfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);

  // trimit client id-ul serverului, pentru a fi identificat
	ret = send(sockfd, argv[1], MAX_ID_SIZE, 0);
	DIE(ret < 0, "send");

	while (1) 
	{
    // salvez multimea de descriptori intr-una temporara
		tmp_fds = read_fds;

		ret = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		// primesc mesaj de la server
		if (FD_ISSET(sockfd, &tmp_fds))
		{
			ret = recv(sockfd, buffer, sizeof(buffer), 0);
      DIE(ret < 0, "recv");

      if (ret == 0)
      {
        break;
      }

			printf("%s\n", buffer);
		}

    // se citeste de la tastatura
		if (FD_ISSET(STDIN_FILENO, &tmp_fds))
		{	
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);

      // ma deconectez daca primesc exit de la tastatura
			if (strncmp(buffer, "exit", 4) == 0) 
			{
				break;
			}

			// se trimite mesaj la server
			ret = send(sockfd, buffer, sizeof(buffer), 0);
			DIE(ret < 0, "send");
		}

	}
  
  // inchid socket-ul ramas deschis		
	close(sockfd);

	return 0;
}
