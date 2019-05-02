extern "C"
{
	#include <stdio.h>
	#include <string.h>
	#include <stdlib.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <arpa/inet.h>
	#include "helpers.h"
}

#include <vector>
#include <utility>
#include <map>
#include <string>
#include <sstream>
#include <iostream>
#include <iterator>

using namespace std;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s <server_port>\n", file);
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

// functie care verifica validitatea input-ului
bool check_tokens(int sockfd, vector<string> tokens)
{
	if (tokens.size() != 3)
	{
		send(sockfd, INPUT_ERR, sizeof(INPUT_ERR), 0);
		return false;
	}
	else if ((tokens[2] != "0") && (tokens[2] != "1"))
	{
		send(sockfd, SF_ERR, sizeof(SF_ERR), 0);
		return false;
	}
	else if ((tokens[0] != "subscribe") && (tokens[0] != "unsubscribe"))
	{
		send(sockfd, INPUT_ERR, sizeof(INPUT_ERR), 0);
		return false;
	}
	return true;
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret;
	socklen_t clilen;
	
	// folosit pentru asocierea sockfd-ului cu id-ul clientului conectat
	map<int, string> cli_ids;
	// TODO: folosit pentru obtinerea topic-urilor la care este abonat clientul

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 2)
	{
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(sockfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;

	// dezactivez algoritmul Nagle
	int b = 1;
	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)(&b), sizeof(b));

	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		for (i = 0; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &tmp_fds))
			{
				if (i == sockfd)
				{
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					clilen = sizeof(cli_addr);
					newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd < 0, "accept");

					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax)
					{ 
						fdmax = newsockfd;
					}

				}
				else 
				{
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(buffer), 0);
					DIE(n < 0, "recv");

					if (n == 0)
					{
						// conexiunea s-a inchis
						string id = cli_ids.find(i)->second;
						cout << "Client " << id << " disconnected.\n";
						close(i);
						
						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &read_fds);
						cli_ids[i] = "\0\0";
					}
					else
					{
						// daca e un socket nou, trebuie sa adaug un element nou in lista id-urilor
						if ((cli_ids.find(i) == cli_ids.end()) || (cli_ids[i] == "\0\0"))
						{
							string id(buffer);
							cli_ids[i] = id;

							printf("New client %s connected from %s:%d.\n",
									buffer, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
						}
						else if (strstr(buffer, "unsubscribe"))
						{
							// TODO: ce fac cand da unsubscribe
							string str(buffer);
							vector<string> tokens = tokenize_input(str);

							if (check_tokens(i, tokens) == true)
							{
								// TODO: cazul in care input-ul este ok
								string send_msg = "You have unsubscribed from " + tokens[1] + "!";
								send(i, send_msg.c_str(), send_msg.size(), 0);
							}
						}
						else if (strstr(buffer, "subscribe"))
						{
							// TODO: ce fac cand da subscribe
							string str(buffer);
							vector<string> tokens = tokenize_input(str);

							if (check_tokens(i, tokens) == true)
							{
								// TODO: cazul in care input-ul este ok
								string send_msg = "You have subscribed to " + tokens[1] + "!";
								send(i, send_msg.c_str(), send_msg.size(), 0);
							}
						}
						// input-ul este incorect
						else
						{							
							send(i, INPUT_ERR, sizeof(INPUT_ERR), 0);
						}
					}
				}
			}
		}
	}

	close(sockfd);

	return 0;
}
