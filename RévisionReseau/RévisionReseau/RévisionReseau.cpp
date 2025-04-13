#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ctime>

#pragma comment(lib, "ws2_32.lib")

#define PORT 12345
#define BUFFER_SIZE 1024
#define TIMEOUT_SECONDS 300

// Structure pour stocker les infos client (côté serveur)
struct ClientInfo {
    std::string username;
    time_t lastActivity;
};

// Variables globales pour le serveur
std::unordered_map<std::string, ClientInfo> clients;
std::mutex clientsMutex;
bool serverRunning = false;
SOCKET serverSocket = INVALID_SOCKET;

// Variables globales pour le client
bool clientRunning = false;
SOCKET clientSocket = INVALID_SOCKET;
std::string username;

// Fonction de nettoyage des clients inactifs (serveur)
void cleanup_clients() {
    while (serverRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(60));

        std::lock_guard<std::mutex> lock(clientsMutex);
        time_t currentTime = time(nullptr);

        for (auto it = clients.begin(); it != clients.end(); ) {
            if (currentTime - it->second.lastActivity > TIMEOUT_SECONDS) {
                std::cout << "Client expired: " << it->second.username << std::endl;
                it = clients.erase(it);
            }
            else {
                ++it;
            }
        }
    }
}

// Fonction de réception des messages (client)
void receive_messages() {
    char buffer[BUFFER_SIZE];
    sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);

    while (clientRunning) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recvfrom(clientSocket, buffer, BUFFER_SIZE, 0,
            (sockaddr*)&fromAddr, &fromLen);

        if (bytesReceived == SOCKET_ERROR) {
            if (clientRunning) {
                std::cerr << "recvfrom error: " << WSAGetLastError() << std::endl;
            }
            continue;
        }

        std::cout << std::string(buffer, bytesReceived) << std::endl;
    }
}

// Fonction pour démarrer le serveur
void start_server() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return;
    }

    serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        WSACleanup();
        return;
    }

    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    serverRunning = true;
    std::thread cleaner(cleanup_clients);
    cleaner.detach();

    std::cout << "Server started on port " << PORT << std::endl;

    char buffer[BUFFER_SIZE];
    sockaddr_in clientAddr;
    int addrLen = sizeof(clientAddr);

    while (serverRunning) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytesReceived = recvfrom(serverSocket, buffer, BUFFER_SIZE, 0,
            (sockaddr*)&clientAddr, &addrLen);

        if (bytesReceived == SOCKET_ERROR) {
            std::cerr << "recvfrom error: " << WSAGetLastError() << std::endl;
            continue;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
        std::string clientKey = std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port));

        std::lock_guard<std::mutex> lock(clientsMutex);

        if (clients.find(clientKey) == clients.end()) {
            clients[clientKey] = { std::string(buffer, bytesReceived), time(nullptr) };
            std::cout << "New client: " << clients[clientKey].username << std::endl;
            continue;
        }

        clients[clientKey].lastActivity = time(nullptr);
        std::string message = clients[clientKey].username + ": " + std::string(buffer, bytesReceived);

        for (const auto& client : clients) {
            if (client.first != clientKey) {
                size_t colonPos = client.first.find(':');
                std::string ip = client.first.substr(0, colonPos);
                int port = std::stoi(client.first.substr(colonPos + 1));

                sockaddr_in destAddr;
                memset(&destAddr, 0, sizeof(destAddr));
                destAddr.sin_family = AF_INET;
                inet_pton(AF_INET, ip.c_str(), &destAddr.sin_addr);
                destAddr.sin_port = htons(port);

                sendto(serverSocket, message.c_str(), message.size(), 0,
                    (sockaddr*)&destAddr, sizeof(destAddr));
            }
        }
    }

    closesocket(serverSocket);
    WSACleanup();
}

// Fonction pour démarrer le client
void start_client(const std::string& serverIp) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return;
    }

    clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
        WSACleanup();
        return;
    }

    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr);
    serverAddr.sin_port = htons(PORT);

    std::cout << "Enter username: ";
    std::getline(std::cin, username);

    sendto(clientSocket, username.c_str(), username.size(), 0,
        (sockaddr*)&serverAddr, sizeof(serverAddr));

    clientRunning = true;
    std::thread receiver(receive_messages);
    receiver.detach();

    std::cout << "Connected to server. Type messages (Ctrl+C to quit):" << std::endl;

    std::string message;
    while (clientRunning) {
        std::getline(std::cin, message);
        sendto(clientSocket, message.c_str(), message.size(), 0,
            (sockaddr*)&serverAddr, sizeof(serverAddr));
    }

    closesocket(clientSocket);
    WSACleanup();
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " server | client <server_ip>" << std::endl;
        return 1;
    }

    std::string mode(argv[1]);

    if (mode == "server") {
        start_server();
    }
    else if (mode == "client" && argc == 3) {
        start_client(argv[2]);
    }
    else {
        std::cerr << "Invalid arguments" << std::endl;
        std::cerr << "For server: " << argv[0] << " server" << std::endl;
        std::cerr << "For client: " << argv[0] << " client <server_ip>" << std::endl;
        return 1;
    }

    return 0;
}