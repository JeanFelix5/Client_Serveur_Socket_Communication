#undef UNICODE
//Code de Jean-Félix - Laboratoire 3
#define WIN32_LEAN_AND_MEAN

//socket libraries
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

//general libraries
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>

#pragma comment (lib, "Ws2_32.lib") // Need to link with Ws2_32.lib

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"

using namespace std;
namespace fs = filesystem;  //seulement disponible dans la version c++ 17 si ne fonctionne pas aller dans l'option langage
                            //(change the language standard in Project properties) -> Configuration Properties -> C/C++ -> 
                            //Language -> C++ Language Standard to at least ISO C++17 Standard (/std:c++17)
                            //https://stackoverflow.com/questions/62256738/visual-studio-2019-c-and-stdfilesystem


//Socket variables
WSADATA wsaData;
int iResult;
char buff[4096];

//general variables
string ExecPath;
string OutFolderPath;
int FileToSendToClient = 0;
vector<string> Credentials;
int NumberOfFiles = 0;

void GetCredentials(string AdminPath);
bool VerifyConnection(SOCKET, string, string);
void ShowDataFolder(SOCKET);
void SendFile(string, SOCKET clientSocket);


bool replace(string& str, const string& from, const string& to) {
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

bool isNumber(const string& s)
{
    for (char const& ch : s) {
        if (std::isdigit(ch) == 0)
            return false;
    }
    return true;
}

int __cdecl main(int argc, char* argv[])
{
    cout << "Laboratoire 3 sur les protocoles -> Serveur" << endl;

    ExecPath = argv[0];                         //get the exe path
    replace(ExecPath, "Serveur.exe", "Data\\"); //je modifie le path courant qui mène à l'exécutable et le modifie pour avoir le path qui mène au dossier Data

    //Je récupère le path vers le fichier qui contient les crédentials et je récupère nom d'utilisateurs et les mots de passes qu'il contient
    string AdminPath = ExecPath;
    replace(AdminPath, "Data\\", "Accounts.txt");
    GetCredentials(AdminPath);


    //Initialisation de la winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cerr << "Can't start winsock on the server" << endl;
        return 1;
    }

    //Creation de la socket
    SOCKET listening = socket(AF_INET, SOCK_STREAM, 0);
    if (listening == INVALID_SOCKET) {
        cerr << "Can't create the socket on the server" << endl;
        return 1;
    }

    //Lier l'IP avec le port a la socket
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(27015);
    hint.sin_addr.S_un.S_addr = INADDR_ANY;

    bind(listening, (sockaddr*)&hint, sizeof(hint));

    //Dire au winsock d'ecouter
    listen(listening, SOMAXCONN);

    //Je crée un master file descriptor et je le set a zero
    fd_set master;
    FD_ZERO(&master);

    FD_SET(listening, &master);

    bool running = true;

    while (running) {
       
        fd_set copy = master;

        //regarde quel client est en train de nous parler
        int socketCount = select(0, &copy, nullptr, nullptr, nullptr);
        
        //boucle entre toutes les connections courantes
        for (int i = 0; i < socketCount; i++)
        {
            
            SOCKET sock = copy.fd_array[i];

            if (sock == listening)//si c'est un nouveau client qui se connecte
            {
                sockaddr_in clientSockAddress;
                int clientSize = sizeof(clientSockAddress);

                //accepte la nouvelle connection
                SOCKET clientSocket = accept(listening, (sockaddr*)&clientSockAddress, &clientSize);
                
                //j'ajoute la nouvelle connection a la liste des clients connectés
                FD_SET(clientSocket, &master);

                char host[NI_MAXHOST];      //nom du client
                char service[NI_MAXSERV];   //Service (IP:PORT) le client quand il se connecte

                ZeroMemory(host, NI_MAXHOST);
                ZeroMemory(service, NI_MAXSERV);

                if (getnameinfo((sockaddr*)&clientSockAddress, sizeof(clientSockAddress), host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0) {
                    cout << host << " connected on port " << service << endl;
                }
                else {
                    inet_ntop(AF_INET, &clientSockAddress.sin_addr, host, NI_MAXHOST);
                    cout << host << " connected on port " << ntohs(clientSockAddress.sin_port) << endl;
                }

                // Verification 
                //demande nom d'utilisateur et le mot de passe
                //en envoyant la question au client et en ecoutant la reponse
                string UserNameQuestion = "Veuillez entrer votre nom d'utilisateur:";
                string PasswordQuestion = "Veuillez entrer votre mot de passe:";
                bool isConnectionSuccesfull = false;
                while (isConnectionSuccesfull != true) {
                    isConnectionSuccesfull = VerifyConnection(clientSocket, UserNameQuestion, PasswordQuestion);
                }

                ShowDataFolder(clientSocket); //montre le contenu du fichier data sur le serveur
            }
            else //Si c'est un message d'un client qui est actuellement connecté
            {
                ZeroMemory(buff, 4096);

                //attendre de recevoir les données du client
                int byteReceived = recv(sock, buff, 4096, 0);
                if (byteReceived == SOCKET_ERROR) {
                    //cerr << "Error in recv(). Exiting" << endl;
                    break;
                }
              
                if (byteReceived <= 0) //si la réponse est nulle j'enlève le client de la liste
                {
                    //enlève le client
                    closesocket(sock);
                    FD_CLR(sock, &master);
                }
                else 
                {
                    //Si la commande reçu est la commande list, le serveur affiche le contenu du data folder
                    if (string(buff, 0, byteReceived) == "list") {
                        ShowDataFolder(sock);
                    }
                    else if (string(buff, 0, byteReceived) == "q") { //si la commande est q, déconnecte le client du serveur
                        closesocket(sock);
                        FD_CLR(sock, &master);

                        cout << "Client de la socket: " << sock << " s'est deconnecter." << endl;
                    }
                    else {
                        //Si cela ne correspond pas au deux commandes précédantes, le serveur vérifie si la commande est valide
                     
                        bool bisNumber = isNumber(string(buff, 0, byteReceived));//vérifie si la commande est un nombre
                        if (bisNumber == true) {
                            int command = stoi(string(buff, 0, byteReceived));

                            if (command <= NumberOfFiles && command > -1) {   //condition si le choix est un nombre valide (le nombre doit correspondre a un fichier dans la liste présente à l'écran. Il doit être plus petit ou égal au nombre de fichier présent et ne pas être négatif) 
                                FileToSendToClient = stoi(string(buff, 0, byteReceived));

                                string validCommand = "La commande est valide";
                                cout << validCommand << endl;

                                // envoie un message de reponse au client, pour lui dire que sa commande est valide
                                send(sock, validCommand.c_str(), validCommand.size() + 1, 0);

                                //Envoie les informations nécessaire au client du fichier choisi pour que le client puisse le télécharger
                                SendFile(string(buff, 0, byteReceived), sock);

                                //affiche a l'écran du serveur
                                //cout << string(buff, 0, byteReceived) << endl;
                            }
                            else {//commande est invalide, car le nombre reçu est trop petit ou trop grand et ne fait donc pas de sens
                                //affiche a l'écran du serveur si la commande est invalide
                                string invalidCommand = "La commande: " + string(buff, 0, byteReceived) + " est invalide";
                                cout << invalidCommand << endl;
                                // envoie un message de reponse au client, pour lui dire que sa commande est invalide
                                send(sock, invalidCommand.c_str(), invalidCommand.size() + 1, 0);
                            }
                        }
                        else {//la commande est invalide car la commande reçu n'est pas un nombre
                            //affiche a l'écran du serveur si la commande est invalide
                            string invalidCommand = "La commande: " + string(buff, 0, byteReceived) + " est invalide";
                            cout << invalidCommand << endl;
                            // envoie un message de reponse au client, pour lui dire que sa commande est invalide
                            send(sock, invalidCommand.c_str(), invalidCommand.size() + 1, 0);
                        }
                    }
                }
            }
        }
    }
    
    FD_CLR(listening, &master);
    closesocket(listening);

    string msg = "Server is shutting down. Goodbye\r\n";

    //nettoyage de la socket
    WSACleanup();

    system("pause");
    return 0;
}

void GetCredentials(string AdminPath) 
{
    string line;
    ifstream myfile(AdminPath);
    if (myfile.is_open())
    {
        while (getline(myfile, line))
        {
            Credentials.push_back(line);    //mets chaque ligne du fichier Accounts.txt dans mon vecteur qui contient les crédentials
        }
        myfile.close();
    }

    else cout << "Unable to open file";
}

bool VerifyConnection(SOCKET clientSocket, string UserNameQuestion, string PasswordQuestion)
{
    //send username question
    send(clientSocket, UserNameQuestion.c_str(), UserNameQuestion.size() + 1, 0);

    //recv username
    string username;
    string password;
    ZeroMemory(buff, 4096);
    int byteReceived = recv(clientSocket, buff, 4096, 0);

    if (byteReceived > 0) {
        username = string(buff, 0, byteReceived);
    }

    //verify username with credentieal vector
    for (int i = 0; i < Credentials.size(); i++) {
        if (Credentials.at(i) == username) {
            //username correct
            password = Credentials.at(i + 1);
            cout << "Le nom d'utilisateur est valide" << endl;
            break;
        }
        else {
            i++;    //incremente i parce que le prochain element est un mot de passe et on ne regarde pas les mots de passe pour cette boucle
        }
    }
    
    //send password question
    send(clientSocket, PasswordQuestion.c_str(), PasswordQuestion.size() + 1, 0);

    //recv password
    ZeroMemory(buff, 4096);
    byteReceived = recv(clientSocket, buff, 4096, 0);
    //verify password with credential 
    if (string(buff, 0, byteReceived) == password && byteReceived > 0) {

        cout << "Le mot de passe est valide" << endl;
        string ConnectionValid = "Le client est maintenant connecter au serveur :)";
        send(clientSocket, ConnectionValid.c_str(), ConnectionValid.size() + 1, 0);
        return true;  //return true if ok, false otherwise
    }

    string ConnectionFailed = "Le mot de passe ou le nom d'utilisateur est invalide, veuillez recommencer :(";
    send(clientSocket, ConnectionFailed.c_str(), ConnectionFailed.size() + 1, 0);

    return false;
}

void ShowDataFolder(SOCKET clientSocket) {  //Send the content of the data folder
    //Affiche et numerote les elements du fichier Data
    string DataFolderContent = "Voici_la_liste_des_fichier_contenu_dans_le_serveur: ";
    
    string Filename = "";
    int numElemFolder = 0;
    for (auto & entry : fs::directory_iterator(ExecPath)) {
      
        Filename = "-" + to_string(numElemFolder) + "->" + entry.path().filename().generic_string() + " ";
        DataFolderContent += Filename;
        NumberOfFiles = numElemFolder;
        numElemFolder++;
    }
    
    send(clientSocket, DataFolderContent.c_str(), DataFolderContent.size() + 1, 0);
}

void SendFile(string Data, SOCKET clientSocket)
{
    ZeroMemory(buff, 4096);
    int dirPosition = 0;
    string Filename = "";
    string FilenamePath = "";

    //get the path of the chosen file
    for (const auto& entry : fs::directory_iterator(ExecPath)) {

        if (dirPosition == FileToSendToClient) {
            FilenamePath = entry.path().generic_string();       
            Filename = entry.path().filename().generic_string();//obtien le nom du fichier choisi avec son type indique a la fin
        }
        dirPosition++;
    }

    send(clientSocket, Filename.c_str(), Filename.size(), 0); //send the title and the type of the file


    //Trouve la taille du fichier en utilisant seekg et tellg
    ifstream Chosenfile(FilenamePath, ios::binary);
    if (Chosenfile.is_open()) {
        Chosenfile.seekg(0, ios::end);
        long ChosenfileSize = Chosenfile.tellg();

        //envoyer la size du fichier au serveur
        int bytesReceived = send(clientSocket, (char*)&ChosenfileSize, sizeof(long), 0);
        if (bytesReceived == 0 || bytesReceived == -1) {
            closesocket(clientSocket);
            cout << "Erreur d'envoie de la taille du fichier " << endl;
            WSACleanup();
            return;
        }

        Chosenfile.seekg(0, ios::beg);

        //lire le fichier
        do {
            Chosenfile.read(buff, 4096);
            if (Chosenfile.gcount() > 0) {
                int bytesReceived = send(clientSocket, buff, Chosenfile.gcount(), 0);
            }
        } while (Chosenfile.gcount() > 0);
        Chosenfile.close();
        cout << "Envoi du fichier reussi" << endl;
    }
}
