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
#include <cmath>

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

// functie auxiliara, folosita pentru verificarea subscribe-ului cu SF = 1
// pentru clientul cu user_id pentru topic-ul topic_name
void check_SF(vector<pair<string, vector<char *> > > &offline_msgs,
  vector<pair<string, vector<string> > > sf_topics, char *topic_name,
  string user_id, char *output)
{
  for (int k = 0; k < offline_msgs.size(); k++)
  {
    if (user_id == offline_msgs[k].first)
    {
      for (int l = 0; l < sf_topics.size(); k++)
      {
        if (strcmp(sf_topics[l].first.c_str(), topic_name) == 0)
        {
          for (int m = 0; m < sf_topics[l].second.size(); m++)
          {
            if (sf_topics[l].second[m] == user_id)
            {
              char *output_cpy = (char *)malloc(BUFLEN);
              memset(output_cpy, 0, BUFLEN);
              memcpy(output_cpy, output, strlen(output) + 1);
              offline_msgs[k].second.push_back(output_cpy);

              return;
            }
          }
        }
      }
    }
  }
  return;
}


int main(int argc, char *argv[])
{
	int sockfd, udpfd, newsockfd, send_sock_fd, portno;
	char buffer[BUFLEN];
	char output[BUFLEN];
	char string_value[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, j, ret;
	socklen_t clilen;
  uint8_t found;
	
	// folosit pentru asocierea sockfd-ului cu id-ul clientului conectat
	map<int, string> cli_ids;

  // folosit pentru asocierea unui client cu socket-ul pe care (eventual) este
  // conectat si un iterator folosit pentru parcurgerea cli_socks
  map<string, int> cli_socks;
  map<string, int>::iterator it;

	// folosit pentru obtinerea topic-urilor la care este abonat clientul
  vector<pair<string, vector<string> > > topics;
  // folosit pentru obtinerea topic-urilor cu SF = 1, la care este abonat clientul
  vector<pair<string, vector<string> > > sf_topics;
  // folosit pentru topicuri noi
  pair<string, vector<string> > new_topic;

  //folosit pentru stocarea mesajelor offline ale unui client
  vector<pair<string, vector<char *> > > offline_msgs;

	fd_set read_fds;	 // multimea de citire folosita in select()
	fd_set tmp_fds;		 // multime folosita temporar
	int fdmax;			 // valoare maxima fd din multimea read_fds

	if (argc < 2)
	{
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea
  // temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// socket-ul pentru conexiunea TCP
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind_TCP");

	ret = listen(sockfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	// socket-ul pentru conexiunea UDP
	udpfd = socket(AF_INET, SOCK_DGRAM, 0);
	ret = bind(udpfd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind_UDP");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni)
	// in multimea read_fds
	FD_SET(sockfd, &read_fds);
	FD_SET(udpfd, &read_fds);
	fdmax = max(sockfd, udpfd);

	// dezactivez algoritmul Nagle
	int b = 1;
	ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)(&b), sizeof(b));
  DIE(ret < 0, "setsockopt");

	while (1) {
		tmp_fds = read_fds; 

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		// am primit date de la socket-ul UDP
		if (FD_ISSET(udpfd, &tmp_fds))
		{
			clilen = sizeof(cli_addr);
			ret = recvfrom(udpfd, buffer, BUFLEN, 0, (struct sockaddr*)&cli_addr, 
				&clilen);
			DIE(ret < 0, "recvfrom_UDP");

			char data_type[1];

			sprintf(data_type, "%u", buffer[50]);

			string string_buffer(buffer);
			stringstream strValue;
			strValue << string_buffer;

			char topic_name[50];
			strValue >> topic_name;
			
			// daca este un INT
			if (strcmp(data_type, "0") == 0)
			{
				uint32_t value = 0;

				char sign[1];
				sprintf(sign, "%hhu", buffer[51]);

    		memcpy(&value, &buffer[52], sizeof(value));

    		memset(output, 0, BUFLEN);
				sprintf(output, "%s:%d - %s - INT - ", inet_ntoa(cli_addr.sin_addr),
					ntohs(cli_addr.sin_port), topic_name);

				// daca numarul e negativ, adaug un minus
				if (sign[0] == '1')
				{
					sprintf(output + strlen(output), "-");
				}

				sprintf(output + strlen(output), "%u", htonl(value));
			}
			// daca este un SHORT_REAL
			else if ((strcmp(data_type, "1") == 0))
			{
				uint16_t value = 0;
				memcpy(&value, &buffer[51], sizeof(value));

				memset(output, 0, BUFLEN);
				sprintf(output, "%s:%d - %s - SHORT_REAL - %g", 
					inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port),
					topic_name, ((float)htons(value) / 100));
			}
			// daca este un FLOAT
			else if ((strcmp(data_type, "2") == 0))
			{
				char sign[1];
				sprintf(sign, "%hhu", buffer[51]);

				memset(output, 0, BUFLEN);
				sprintf(output, "%s:%d - %s - FLOAT - ", inet_ntoa(cli_addr.sin_addr),
					ntohs(cli_addr.sin_port), topic_name);

				// daca numarul este negativ, adaug un minus
				if (sign[0] == '1')
				{
					sprintf(output + strlen(output), "-");
				}

				// modulul numarului obtinut prin alipire
				uint32_t value = 0;
				memcpy(&value, &buffer[52], sizeof(value));

				// modulul puterii negative a lui 10
				uint8_t neg_pow = 0;
				memcpy(&neg_pow, &buffer[52 + sizeof(value)], sizeof(neg_pow));

        // folosesc numar variabil de zecimale, dat de neg_pow
				sprintf(output + strlen(output), "%.*f", neg_pow,
					((float)htonl(value) / pow(10, neg_pow)));
			}

			//daca este un STRING
			else if ((strcmp(data_type, "3") == 0))
			{
				memset(string_value, 0, BUFLEN);
				memcpy(string_value, &buffer[51], STRINGLEN);

				memset(output, 0, BUFLEN);
				sprintf(output, "%s:%d - %s - STRING - %s", 
					inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port),
					topic_name, string_value);
			}

			// in momentul asta, am output-ul, pe care trebuie sa il trimit la clientii
			// abonati(sau sa il pastrez, pentru cei cu SF = 1)
      for (auto j : topics)
      {
        // daca exista clienti abonati la topic-ul la care s-a postat un mesaj
        if (strcmp(topic_name, j.first.c_str()) == 0)
        {
          for (i = 0; i < j.second.size(); i++)
          {
            it = cli_socks.find(j.second[i]);
            if (it != cli_socks.end() && it->second > 0)
            {
              send_sock_fd = it->second;
              ret = send(send_sock_fd, output, sizeof(output), 0);
              DIE(ret < 0, "send_udp_msg");
            }

            // altfel, verific daca clientul are SF setat pe 1, caz in care adaug
            // mesajul in vector, pentru a i-l trimite la conectare
            else
            {
              check_SF(offline_msgs, sf_topics, topic_name, j.second[i], output);
            }
          }   
        }
      }
			continue;
		}

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

					// se adauga noul socket intors de accept() la multimea descriptorilor
          // de citire
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
            cli_socks[id] = -1;
					}
					else
					{
						// daca e un socket nou, trebuie sa adaug un element nou in lista id-urilor
						if ((cli_ids.find(i) == cli_ids.end()) || (cli_ids[i] == "\0\0"))
						{
							string id(buffer);
							cli_ids[i] = id;
              cli_socks[id] = i;

							printf("New client %s connected from %s:%d.\n",
									buffer, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

              for (j = 0; j < offline_msgs.size(); j++)
              {
                if (id == offline_msgs[j].first)
                {
                  // verific daca are mesaje offline ce trebuiau sa fie trimise
                  int msg_count = offline_msgs[j].second.size();
                  for (int k = 0; k < msg_count; k++)
                  {
                    ret = send(i, offline_msgs[j].second[0], BUFLEN, 0);
                    DIE(ret < 0, "send");
                    // eliberez memoria ocupata de buffer
                    free(offline_msgs[j].second[0]);
                    offline_msgs[j].second.erase(offline_msgs[j].second.begin());
                  }
                }
              }
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
								ret = send(i, send_msg.c_str(), send_msg.size(), 0);
                DIE(ret < 0, "send");
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
								ret = send(i, send_msg.c_str(), send_msg.size(), 0);
                DIE(ret < 0, "send");
							}

              found = 0;
              for (j = 0; j < topics.size(); j++)
              {
                // daca exista deja topicul
                if (topics[j].first == tokens[1])
                {
                  topics[j].second.push_back(cli_ids[i]);
                  found = 1;
                  break;
                }
              }

              // daca topicul nu exista, adaug un topic nou
              if (found == 0)
              {
                new_topic.first = tokens[1];
                new_topic.second = vector<string>(1, cli_ids[i]);
                topics.push_back(new_topic);
              }

              // daca SF este setat pe 1, il adaug intr-o lista suplimentara
              if (tokens[2] == "1")
              {
                pair<string, vector<char *> > new_user;
                new_user.first = cli_ids[i];
                offline_msgs.push_back(new_user);

                found = 0;
                for (j = 0; j < sf_topics.size(); j++)
                {
                  // daca exista deja topicul
                  if (sf_topics[j].first == tokens[1])
                  {
                    sf_topics[j].second.push_back(cli_ids[i]);
                    found = 1;
                    break;
                  }
                }

                // daca topicul nu exista, adaug un topic nou
                if (found == 0)
                {
                  new_topic.first = tokens[1];
                  new_topic.second = vector<string>(1, cli_ids[i]);
                  sf_topics.push_back(new_topic);
                }
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
	close(udpfd);

	return 0;
}
