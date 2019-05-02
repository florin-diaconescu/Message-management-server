extern "C"
{
	#include <stdio.h>
	#include <stdlib.h>
	#include <unistd.h>
	#include <string.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
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

void usage(char *file)
{
	fprintf(stderr, "Usage: %s <client_id> <server_address> <server_port>\n", file);
	exit(0);
}

vector<string> tokenize_input(string input)
{
	stringstream ss(input);
	string buf;
	vector<string> tokens;

	while (ss >> buf)
	{
		tokens.push_back(buf);
	}

	return tokens;
}

int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar

	if (argc < 4)
	{
		usage(argv[0]);
	}

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));

	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	FD_SET(sockfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);

	n = send(sockfd, argv[1], sizeof(argv[1]), 0);
	DIE(n < 0, "send");

	while (1) 
	{
		tmp_fds = read_fds;

		ret = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		// primesc mesaj de la server
		if (FD_ISSET(sockfd, &tmp_fds))
		{
			recv(sockfd, buffer, sizeof(buffer), 0);
			printf("%s\n", buffer);
		}

		if (FD_ISSET(STDIN_FILENO, &tmp_fds))
		{
			// se citeste de la tastatura
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);

			if (strncmp(buffer, "exit", 4) == 0) 
			{
				break;
			}

			// se trimite mesaj la server
			n = send(sockfd, buffer, sizeof(buffer), 0);
			DIE(n < 0, "send");

			string str(buffer);
			vector<string> tokens = tokenize_input(str);

			
			/*if (tokens.size() == 3)
			{
				if ((tokens[0] == "subscribe" || tokens[0] == "unsubscribe") &&
					(tokens[2] == "0" || tokens[2] == "1"))
				{
					cout << "You have " << tokens[0] << "d " << tokens[1] << "!\n";
				}
			}*/
		}

	}
  		

	close(sockfd);

	return 0;
}
