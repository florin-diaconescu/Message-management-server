-------------------------------------------------
-- Diaconescu Florin, 322CB, florin.diaconescu --
-------------------------------------------------


--------Aplicatie client-server TCP si UDP pentru gestionarea mesajelor--------

Am ales sa implementez tema in C++, datorita facilitatilor oferite de containerele
STL, mai exact vector si map, fiindu-mi astfel mai usor sa gestionez userii
conectati la un moment dat, topic-urile la care acestia sunt abonati (cu sau fara
SF setat), cat si a mesajelor ce trebuie sa fie stocate cat timp sunt offline.

Am inceput cu preluarea codului din laboratorul 8 (cel de multiplexare), de unde
am luat fisierul helpers.c, adaugandu-i doua definitii pentru mesajele de eroare,
cat si dimensiunile buff-elor, cat si punctul de plecare pentru implementarea
clientului (subscriber.cpp) si a serverului (server.cpp).

Functionalitatea clientului de TCP este una simpla, avand doar rolul de a trimite
server-ului comenzile (subscribe, unsubscribe, exit sau ceva invalid), primind de
la server un mesaj (ce poate fi atat unul de eroare, cat si un mesaj primit de la
clientii de UDP unde este abonat). Astfel, setul de file descriptori al acestuia
este format doar din socket-ul prin care comunica si STDIN, pentru a prelua
comenzile de la tastatura.

Serverul cuprinde centrul functionalitatii aplicatiei, aici parsandu-se atat
payload-urile primite de la clientii UDP, cat si trimiterea lor mai departe, unde
este necesar, stocarea mesajelor de care vor avea nevoie clientii abonati cu SF
la anumite topic-uri. Inainte de bind, am considerat ca este util sa adaug doua
setsockopt-uri.
In primul rand, folosesc SO_REUSEADDR deoarece aveam nevoie de posibilitatea de
a redeschide imediat server-ul pe acelasi port, daca il inchid cu comanda "exit"
de la tastatura. Prima data, am gasit in resursa [1] diferenta intre close si
shutdown aplicat unui socket, astfel realizand ca ar trebui inchisa conexiunea
intr-un mod mai putin brutal, folosind intai un shutdown, cu how setat pe SHUT_WR,
pentru a-i trimite clientului ca nu voi mai scrie nimic, apoi sa primesc toate
mesajele (cu read), ce nu au fost inca receptionate, apoi sa il inchid cu close,
optiune ce nu a functionat, din pacate. Citind in resursa [2], am aflat de
SO_REUSEADDR, ce mi-a permis repornirea server-ului pe acelasi port.

Structurile de date folosite pentru implementare sunt:
- o mapare intre string si int, folosit pentru asocierea socket-ului pe care
  este (poate) conectat un client cu id-ul dat de string (cli_socks);
- o mapare intre int si string, analoaga celei de mai sus, folosita pentru
  obtinerea id-ului unui client conectat pe socket-ul dat de int (cli_ids);
- doi vectori de perechi, ce contin fiecare un string (numele unui client)
  si un vector de strings (topic-urile la care este clientul abonat),
  una pentru topic-urile generale la care este abonat (topics), iar al doilea 
  (sf_topics), pentru topic-urile la care este abonat cu SF = 1;
- un vector de perechi, ce contin un string (numele utilizatorului) si un vector
  de char* (de mesaje), folosit pentru stocarea mesajelor primite cat timp
  utilizatorul a fost deconectat, de la topic-urile unde era abonat cu SF = 1
  (offline_msgs).

Pentru a nu aduce aplicatia intr-o stare nefunctionala, am ales ca, dupa ce
am impartit input-ul clientului in tokeni (cu functia tokenize_input), sa ii
verific (cu functia check tokens), unde verific fie daca am 2 sau 3 tokeni, in
functie de comanda apelata, dar si daca SF este diferit de 0 sau 1, pentru a-i
trimite un mesaj de eroare sugestiv utilizatorului.

Am ales sa fac parsarea payload-ului primit de la clientii UDP in server, pentru
ca o vad ca pe un fel de unitate principala, ce are rolul de a gestiona toate
comenzile si operatiile dinauntru, clientii vazandu-l ca pe un fel de blackbox,
avand optiunea de a parsa la randul lor output-ul, daca chiar au nevoie pentru
un API sau o alta utilizare. Logica de parsare putea fi mutata cu usurinta in
client-ul TCP, dar am considerat ca nu asta este solutia potrivita pentru aplicatia
imaginata de mine.
In ceea ce priveste detaliile tehnice, memorez numele topic-ului, stiind ca acesta
are maxim 50 de bytes, stiu sigur ca al 51-lea este byte-ul ce imi va spune ce
tip de date va urma in continuare, apoi, pe baza acestuia fac diverse operatii,
cum ar fi calculul bitului de semn, sau impartirea valorii la 100 (pentru
SHORT_REAL), sau afisarea cu format variabil, in functie de puterea negativa
cu care este inmultit modulul numarului de la FLOAT.
Dupa ce am parsat payload-ul, il trimit clientilor TCP abonati la acel topic,
online in acel moment (deci ce se regasesc in vectorul topics), dar si celor
ce au SF = 1 setat, dar nu sunt offline (deci se regasesc in sf_topics), cu
ajutorul functiei check_SF ce adauga, eventual, mesajul dorit in vectorul de
mesaje ce ii vor trebui trimise clientului la conectare (offline_msgs).

La conectarea unui client TCP, memorez ID-ul acestuia in cele doua mapari in
oglinda (cli_ids si cli_socks), ce vor ajuta la identificarea socket-ului pe care
este conectat si a numele utilizatorului conectat pe un socket anume si apoi
verific daca are cumva mesaje primite cat timp a fost offline, eliberand
memoria alocata pentru mesaje si, evident, stergandu-le pe cele care au fost
deja trimise din memorie.

Ar merita mentionat ca, pentru implementarea mea, dupa ce se realizeaza
conexiunea intre client si server, primul mesaj TCP trimis de client va fi
numele de utilizator, in urma acestuia fiind afisat si la ecran mesajul de
conectare al acestuia.

Daca se primeste un mesaj de tip subscribe de la client, in urma verificarii
tokenilor, se vor face actualizarile necesare in lista de topicuri (cu SF
sau nu) la care este abonat acel client. Daca inputul este incorect, i se va
trimite clientului un mesaj de eroare corespunzator, asa cum am mentionat si mai
sus.

[1] = https://stackoverflow.com/questions/4160347/close-vs-shutdown-socket
[2] = https://stackoverflow.com/questions/14388706/socket-options-so-reuseaddr-and-so-reuseport-how-do-they-differ-do-they-mean-t