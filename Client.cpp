//Code de Jean-Félix - Laboratoire 3
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include "fstream"
#include "string"
#include "vector"
#include <iostream>
#include <filesystem>
#include <sstream>  
#include <bitset>


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015" //port par defaut du serveur

using namespace std;
namespace fs = filesystem;

WSADATA wsaData;
int iResult;
string OutFilePath = "";
char buff[4096];

struct addrinfo* result = NULL,
    * ptr = NULL,
    hints;

bool Connection(SOCKET clientSocket);
void ReceiveDataFolderNames(SOCKET clientSocket);
void ShowListAgain(string list);
void ReceiveFile(string fileName, SOCKET clientSocket);

bool replace(string& str, const string& from, const string& to) {
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

int __cdecl main(int argc, char** argv) {
    cout << "Laboratoire 3 sur les protocoles -> Client" << endl;
    
    string ExecPath = argv[0];
    replace(ExecPath, "Client.exe", "Out\\"); //modification du lien vers l'exécutable pour avoir le lien vers le fichier Out du client
    OutFilePath = ExecPath;

    string ServerIPAddress = "127.0.0.1"; //Addresse IP du serveur

    //Initialisation du Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cerr << "Can't start winsock, error #" << iResult << endl;
        return 1;
    }

    //Creation de la socket
    SOCKET Clientsocket = socket(AF_INET, SOCK_STREAM, 0);
    if (Clientsocket == INVALID_SOCKET) {
        cerr << "Can't create the Client socket, error #" << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    //Remplir la structure
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(27015);
    inet_pton(AF_INET, ServerIPAddress.c_str(), &hint.sin_addr);

    //Connection au serveur
    int connectionResult = connect(Clientsocket, (sockaddr*)&hint, sizeof(hint));
    if (connectionResult == SOCKET_ERROR) {
        cerr << "Can't connect to the server , error #" << WSAGetLastError() << endl;
        closesocket(Clientsocket);
        WSACleanup();
        return 0;
    }

    //Connection au serveur avec le nom d'utilisateur et le mot de passe
    bool isConnectionSuccesfull = false;
    while (isConnectionSuccesfull != true) {
        isConnectionSuccesfull = Connection(Clientsocket);
    }

    cout << endl;
    
    ReceiveDataFolderNames(Clientsocket); //Réception du contenu du data folder

    //boucle l'envoie et la reception des donnees
    char buff[4096];
    string userInput;
    bool ShowList = false;
    do {
        //recupere ce que l'utilisateur va ecrire
        cout << "CLIENT> ";
        cout << "Pour choisir le fichier a telecharger, indiquer le numero correspondant ('list' pour revoir les fichier disponible et 'q' pour quitter) :";

        // Attendre une reponse
        getline(cin, userInput);

        if (userInput.size() > 0) { //verifie que l'utilisateur a ecrit quelque chose

            // Envoie le texte
            int sendResult = send(Clientsocket, userInput.c_str(), userInput.size() + 1, 0);

            if (userInput == "q") {
                break;
            }
            else if (userInput == "list") {
                ShowList = true;
            }

            if (sendResult != SOCKET_ERROR) {
                // Attendre une reponse
                ZeroMemory(buff, 4096);
                int bytesReceived = recv(Clientsocket, buff, 4096, 0);

                if (bytesReceived > 0) {

                    if (ShowList == true) {
                        cout << "SERVEUR>" << endl;
                        ShowListAgain(string(buff, 0, bytesReceived));
                        ShowList = false;
                    }
                    else if ((string(buff, 0, bytesReceived) == "La commande est valide")) {
                        
                        //attend de recevoir le contenu du fichier
                        ZeroMemory(buff, 4096);
                        int bytesReceived = recv(Clientsocket, buff, 4096, 0);
                        if (bytesReceived > 0) {
                           
                            ReceiveFile(string(buff, 0, bytesReceived), Clientsocket); //Creer le fichier selon le type obtenu
                            cout << "SERVEUR> Le fichier a ete telecharger" << endl;
                        }
                    }
                    else {
                        // Ecrit la reponse du serveur a l'ecran
                        cout << "SERVEUR>" << string(buff, 0, bytesReceived) << endl;
                    }
                }
            }
        }
    } while (userInput.size() > 0);

    //fermeture
    closesocket(Clientsocket);
    WSACleanup();

    return 0;
}

bool Connection(SOCKET clientSocket)
{
    // Attendre une reponse
    ZeroMemory(buff, 4096);
    int bytesReceived = recv(clientSocket, buff, 4096, 0);
    if (bytesReceived > 0) {
        // Ecrit la reponse du serveur a l'ecran
        cout << "SERVEUR>" << string(buff, 0, bytesReceived) << endl;
    }
    string userInput;
    //recupere ce que l'utilisateur va ecrire
    cout << "CLIENT> ";
    getline(cin, userInput);
    if (userInput.size() > 0) { //verifie que l'utilisateur a ecrit quelque chose
        // Envoie le texte
        int sendResult = send(clientSocket, userInput.c_str(), userInput.size() + 1, 0);
    }

    //pareil pour le password
    ZeroMemory(buff, 4096);
    bytesReceived = recv(clientSocket, buff, 4096, 0);
    if (bytesReceived > 0) {
        cout << "SERVEUR>" << string(buff, 0, bytesReceived) << endl;
    }

    //recupere ce que l'utilisateur va ecrire
    cout << "CLIENT> ";
    getline(cin, userInput);

    if (userInput.size() > 0) { //verifie que l'utilisateur a ecrit quelque chose
        // Envoie le texte
        int sendResult = send(clientSocket, userInput.c_str(), userInput.size() + 1, 0);
    }

    // Attendre une reponse
    ZeroMemory(buff, 4096);
    bytesReceived = recv(clientSocket, buff, 4096, 0);

    if (bytesReceived > 0) {
        cout << "SERVEUR>" << string(buff, 0, bytesReceived) << endl;
    }

    if (string(buff, 0, bytesReceived) == "Le client est maintenant connecter au serveur :)") {
        return true;
    }

    return false;
}

void ReceiveFile(string fileName, SOCKET clientSocket) //OutFilePath = path du fichier Out dans lequel crer le fichier
{
    ZeroMemory(buff, 4096);
    ofstream fileSelected;
    int fileRequestedsize;

    int byteReceived = recv(clientSocket, (char*)&fileRequestedsize, sizeof(long), 0);

    //creer un fichier selon le nom et le type récupéré avec le path du fichier Out 
    fileSelected.open(OutFilePath + "/" + fileName, ios::binary | ios::trunc);
    int fileDownloaded = 0;

    //écrit le fichier
    do {
        memset(buff, 0, 4096);
        int bytesReceived = recv(clientSocket, buff, 4096, 0);
        if (bytesReceived == 0 || bytesReceived == -1) {
            closesocket(clientSocket);
            cout << "Erreur lors de la reception du fichier " << endl;
            WSACleanup();
            return;
        }

        fileSelected.write(buff, bytesReceived);
        fileDownloaded += bytesReceived;
    } while (fileDownloaded < fileRequestedsize);
    fileSelected.close();
    cout << "telechargement reussi" << endl;

    ZeroMemory(buff, 4096);
}

void ReceiveDataFolderNames(SOCKET clientSocket)
{
    char buff[4096];
    while(true) {
        // Attendre une reponse
        ZeroMemory(buff, 4096);
        int bytesReceived = recv(clientSocket, buff, 4096, 0);

        if (bytesReceived > 0) {
            string s, str;
            s = string(buff, 0, bytesReceived);

            stringstream ss(s);

            while (getline(ss, str, ' '))
                
                cout << str << endl;

            break;
        }   
    }
}

void ShowListAgain(string list)
{
    string s, str;
    s = list;
    
    stringstream ss(s);

    while (getline(ss, str, ' '))
        
        cout << str << endl;
}
