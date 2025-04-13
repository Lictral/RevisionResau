I. Concepts Clés UDP
1. Caractéristiques d’UDP
Non-connecté : Pas de handshake (vs TCP).

Non fiable : Pas d’acquittement, pas de retransmission.

Sans état : Chaque datagramme est indépendant.

Faible latence : Idéal pour VoIP, jeux en ligne.

2. Différences TCP vs UDP
Critère	TCP	UDP
Fiabilité	✅ Garantie	❌ Aucune garantie
Ordre des paquets	✅ Séquence conservée	❌ Non garanti
Contrôle de flux	✅ Géré	❌ Absent
Utilisation typique	HTTP, FTP	DNS, Streaming, Jeux
II. API Winsock (Windows)
1. Initialisation
cpp
Copy
WSADATA wsaData;
if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    std::cerr << "Échec de WSAStartup" << std::endl;
}
Note : Toujours appeler WSACleanup() à la fin.

2. Création d’un socket UDP
cpp
Copy
SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
AF_INET : IPv4.

SOCK_DGRAM : Mode datagramme (UDP).

3. Liaison (Bind) – Côté Serveur
cpp
Copy
sockaddr_in serverAddr;
serverAddr.sin_family = AF_INET;
serverAddr.sin_addr.s_addr = INADDR_ANY; // Écoute toutes les IPs
serverAddr.sin_port = htons(12345);      // Port en réseau byte order

bind(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));
III. Fonctions Principales
1. Envoi (sendto)
cpp
Copy
sockaddr_in destAddr;
destAddr.sin_family = AF_INET;
inet_pton(AF_INET, "127.0.0.1", &destAddr.sin_addr); // IP cible
destAddr.sin_port = htons(12345);                    // Port cible

sendto(sock, buffer, strlen(buffer), 0, 
       (sockaddr*)&destAddr, sizeof(destAddr));
2. Réception (recvfrom)
cpp
Copy
char buffer[1024];
sockaddr_in clientAddr;
int clientAddrSize = sizeof(clientAddr);

int bytesReceived = recvfrom(sock, buffer, sizeof(buffer), 0,
                            (sockaddr*)&clientAddr, &clientAddrSize);
Attention : recvfrom est bloquant par défaut.

IV. Gestion des Erreurs Courantes
1. Erreurs de socket
INVALID_SOCKET : Vérifier après socket().

SOCKET_ERROR : Retourné par sendto/recvfrom en cas d’échec.

2. Conversion d’adresses
Texte → Binaire : inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr).

Binaire → Texte : inet_ntop(AF_INET, &addr.sin_addr, ipStr, INET_ADDRSTRLEN).

3. Gestion multi-clients
Utiliser un std::unordered_map pour tracker les clients :

cpp
Copy
std::unordered_map<std::string, ClientInfo> clients; // Clé = "IP:Port"
Mutex : Protéger l’accès concurrent avec std::mutex.

V. Bonnes Pratiques
1. Timeout des clients
cpp
Copy
time_t currentTime = time(nullptr);
if (currentTime - client.lastActivity > 300) { // 5 minutes
    // Supprimer le client inactif
}
2. Sécurité
Valider les données reçues (buffer overflow).

Ne pas faire confiance à l’adresse IP (spoofing possible).

3. Performances
Éviter les copies inutiles (réutiliser les buffers).

Utiliser select() ou poll() pour gérer plusieurs sockets.

VI. Exemple de Code Résumé
Serveur UDP Minimal
cpp
Copy
// Initialisation Winsock
// Création socket
// Bind
while (true) {
    recvfrom(...); // Recevoir un message
    // Rediriger à tous les autres clients
    for (auto& client : clients) {
        sendto(...); // Envoyer le message
    }
}
Client UDP Minimal
cpp
Copy
// Initialisation Winsock
// Création socket
std::thread(receive_messages).detach(); // Écoute en arrière-plan
while (true) {
    std::string msg;
    std::getline(std::cin, msg);
    sendto(...); // Envoyer au serveur
}
VII. Questions Possibles en Partiel
Q : Pourquoi choisir UDP plutôt que TCP pour une application temps réel ?
R : Latence réduite (pas de handshake), tolérance aux pertes de paquets (ex : streaming).

Q : Comment garantir l’ordre des messages en UDP ?
R : Ajouter un numéro de séquence côté application et réordonner à la réception.

Q : Comment détecter la déconnexion d’un client en UDP ?
R : Timeout applicatif (ex : dernier message reçu il y a > 30s).