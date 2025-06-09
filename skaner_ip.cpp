// skaner Ip i portów w sieci lokalnej
// wersja 1.3
// autor slavoheys & gemini

// polecenie kompilacji:
// g++ -std=c++17 -o skaner_ip skaner_ip.cpp -lpthread

#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>
#include <iomanip> // Dla std::setw
#include <cstring> // Dla memset

// Platform-specific socket includes
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib") // Link with Winsock library
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> // Dla close()
#include <netdb.h>  // Dla gethostbyaddr, getnameinfo
#include <fcntl.h>  // Dla fcntl (non-blocking sockets)
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

// Struktura do przechowywania informacji o hoście
struct HostInfo
{
    std::string ip;
    std::string hostname;
    std::vector<int> openPorts;
    bool alive = false;
};

// Funkcja inicjalizująca Winsock (tylko Windows)
void initializeWinsock()
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "Błąd inicjalizacji WSAStartup." << std::endl;
        exit(EXIT_FAILURE);
    }
#endif
}

// Funkcja czyszcząca Winsock (tylko Windows)
void cleanupWinsock()
{
#ifdef _WIN32
    WSACleanup();
#endif
}

// Funkcja próby połączenia TCP z określonym portem z timeoutem
bool checkPortWithTimeout(const std::string &ip, int port, int timeout_ms = 200)
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
    {
        return false;
    }

    sockaddr_in hint;
    memset(&hint, 0, sizeof(hint)); // Zerowanie struktury
    hint.sin_family = AF_INET;
    hint.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &hint.sin_addr) <= 0)
    {
        // std::cerr << "Nieprawidłowy adres IP: " << ip << std::endl; // Opcjonalny log
        closesocket(sock);
        return false;
    }

    // Ustawienie gniazda na nieblokujące
#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != 0)
    {
        closesocket(sock);
        return false;
    }
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1)
    {
        closesocket(sock);
        return false;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        closesocket(sock);
        return false;
    }
#endif

    int conn_res = connect(sock, (sockaddr *)&hint, sizeof(hint));

    if (conn_res == 0)
    { // Połączono natychmiast
        closesocket(sock);
        return true;
    }

#ifdef _WIN32
    if (conn_res == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
    {
        closesocket(sock);
        return false;
    }
#else
    if (conn_res == SOCKET_ERROR && errno != EINPROGRESS)
    {
        closesocket(sock);
        return false;
    }
#endif

    // Połączenie w toku (EINPROGRESS lub WSAEWOULDBLOCK)
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(sock, &writefds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    // Na Windows pierwszy parametr select (nfds) jest ignorowany.
    // Na POSIX, nfds to najwyższy numer deskryptora + 1.
    int select_res = select(sock + 1, NULL, &writefds, NULL, &tv);

    if (select_res <= 0)
    { // Timeout lub błąd select
        closesocket(sock);
        return false;
    }

    // select_res > 0, gniazdo jest zapisywalne. Sprawdź błąd połączenia.
    int optval;
    socklen_t optlen = sizeof(optval);
#ifdef _WIN32
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&optval, &optlen) == SOCKET_ERROR)
    {
#else
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &optval, &optlen) == SOCKET_ERROR)
    {
#endif
        closesocket(sock);
        return false;
    }

    if (optval == 0)
    { // Brak błędu, połączenie udane
        closesocket(sock);
        return true;
    }
    else
    { // Błąd połączenia
        closesocket(sock);
        return false;
    }
}

// Funkcja pobierająca nazwę hosta
std::string getHostnameByIp(const std::string &ip)
{
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV]; // Nieużywane, ale wymagane przez getnameinfo

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa)); // Zerowanie struktury
    sa.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip.c_str(), &sa.sin_addr) <= 0)
    {
        return "N/A";
    }
    // sa.sin_port nie jest ściśle potrzebne dla NI_NAMEREQD, jeśli chcemy tylko hosta

    if (getnameinfo((struct sockaddr *)&sa, sizeof(sa),
                    host, NI_MAXHOST,
                    serv, NI_MAXSERV, NI_NAMEREQD) == 0)
    { // NI_NAMEREQD: Błąd, jeśli nazwa nie może być znaleziona
        return std::string(host);
    }
    return "N/A";
}
// Funkcja robocza skanująca pojedynczy host (porty)
void scanHostWorker(HostInfo &hostInfo, const std::vector<int> &portsToScan, int portTimeoutMs)
{
    std::vector<int> foundOpenPorts;
    for (int port : portsToScan)
    {
        if (checkPortWithTimeout(hostInfo.ip, port, portTimeoutMs))
        {
            foundOpenPorts.push_back(port);
        }
    }

    if (!foundOpenPorts.empty())
    {
        hostInfo.alive = true;
        hostInfo.openPorts = foundOpenPorts;
        std::sort(hostInfo.openPorts.begin(), hostInfo.openPorts.end());
        hostInfo.hostname = getHostnameByIp(hostInfo.ip);
    }
    else
    {
        hostInfo.alive = false;
    }
}

// Prosta walidacja formatu bazowego adresu IP (np. "192.168.1")
bool isValidBaseIpFormat(const std::string &baseIp)
{
    std::string s = baseIp;
    std::vector<std::string> parts;
    size_t start = 0;
    size_t end = s.find('.');
    int dotCount = 0;

    while (end != std::string::npos)
    {
        parts.push_back(s.substr(start, end - start));
        start = end + 1;
        end = s.find('.', start);
        dotCount++;
    }
    parts.push_back(s.substr(start)); // Ostatnia część

    if (dotCount != 2)
    { // Oczekujemy dwóch kropek dla formatu X.Y.Z
        return false;
    }
    if (parts.size() != 3)
    {
        return false;
    }

    for (const std::string &part : parts)
    {
        if (part.empty())
            return false;
        for (char c : part)
        {
            if (!isdigit(c))
                return false;
        }
        try
        {
            int num = std::stoi(part);
            if (num < 0 || num > 255)
                return false;
        }
        catch (const std::out_of_range &)
        {
            return false; // Numer poza zakresem int lub za duży/mały dla oktetu
        }
        catch (const std::invalid_argument &)
        {
            return false; // Nie można przekonwertować na liczbę
        }
    }
    return true;
}

int main()
{
    initializeWinsock();

    std::string baseIp;
    int startRange, endRange;
    int numThreads;
    int portTimeoutMs;
    std::vector<int> commonPorts = {20, 21, 22, 23, 25, 53, 80, 110, 135, 137, 138, 139, 143, 443, 445, 993, 995, 1723, 3306, 3389, 5900, 8080};
    // Można też zapytać użytkownika o porty
    // wyczysc konsole
    std::cout << "\033[2J\033[H"; // ANSI escape code to clear console (works on most terminals)
    std::cout << "Podaj bazowy adres IP (np. 192.168.1): ";
    std::cin >> baseIp;
    if (!isValidBaseIpFormat(baseIp))
    {
        std::cerr << "Nieprawidłowy format bazowego adresu IP. Oczekiwano formatu X.Y.Z (np. 192.168.1)." << std::endl;
        cleanupWinsock();
        return 1;
    }

    std::cout << "Podaj początek zakresu IP (np. 1): ";
    std::cin >> startRange;
    std::cout << "Podaj koniec zakresu IP (np. 254): ";
    std::cin >> endRange;
    std::cout << "Podaj liczbę równoczesnych wątków skanujących hosty (np. 10): ";
    std::cin >> numThreads;
    std::cout << "Podaj timeout dla skanowania portów w milisekundach (np. 200): ";
    std::cin >> portTimeoutMs;

    if (startRange < 1 || endRange > 254 || startRange > endRange)
    {
        std::cerr << "Nieprawidłowy zakres IP. Start/Koniec musi być między 1 a 254, a start <= koniec." << std::endl;
        cleanupWinsock();
        return 1;
    }
    if (numThreads < 1)
    {
        std::cerr << "Liczba wątków musi być co najmniej 1." << std::endl;
        cleanupWinsock();
        return 1;
    }
    if (portTimeoutMs < 10)
    {
        std::cerr << "Timeout portu musi być co najmniej 10 ms." << std::endl;
        cleanupWinsock();
        return 1;
    }

    // wyczysc konsole
    std::cout << "\033[2J\033[H"; // ANSI escape code to clear console (works on most terminals)
    std::cout << "\nSkanowanie sieci " << baseIp << "." << startRange << " - " << baseIp << "." << endRange << std::endl;
    std::cout << "Porty do przeskanowania: ";
    for (size_t i = 0; i < commonPorts.size(); ++i)
    {
        std::cout << commonPorts[i] << (i == commonPorts.size() - 1 ? "" : ", ");
    }
    std::cout << std::endl
              << std::endl;

    std::vector<std::string> ipsToScan;
    for (int i = startRange; i <= endRange; ++i)
    {
        ipsToScan.push_back(baseIp + "." + std::to_string(i));
    }

    std::vector<HostInfo> hostInfos(ipsToScan.size());
    for (size_t i = 0; i < ipsToScan.size(); ++i)
    {
        hostInfos[i].ip = ipsToScan[i]; // Inicjalizacja IP
    }

    std::vector<std::thread> activeThreads;
    for (size_t i = 0; i < hostInfos.size();)
    {
        for (int t = 0; t < numThreads && i < hostInfos.size(); ++t, ++i)
        {
            activeThreads.emplace_back(scanHostWorker, std::ref(hostInfos[i]), std::cref(commonPorts), portTimeoutMs);
        }
        for (auto &th : activeThreads)
        {
            if (th.joinable())
            {
                th.join();
            }
        }
        activeThreads.clear();
    }

    std::vector<HostInfo> results;
    for (const auto &hi : hostInfos)
    {
        if (hi.alive)
        {
            results.push_back(hi);
        }
    }
    // wyczysc konsole
    std::cout << "\033[2J\033[H"; // ANSI escape code to clear console (works on most terminals)
    // Wyświetlanie wyników
    std::cout << "\n--- Wyniki Skanowania ---" << std::endl;
    std::cout << std::left << std::setw(18) << "Adres IP"
              << std::setw(30) << "Nazwa Hosta"
              << "Otwarte Porty" << std::endl;
    std::cout << std::string(70, '-') << std::endl; // Dostosowana długość linii

    if (results.empty())
    {
        std::cout << "Nie znaleziono aktywnych hostów lub brak otwartych portów na przeskanowanych hostach." << std::endl;
    }
    else
    {
        for (const auto &host : results)
        {
            std::cout << std::left << std::setw(18) << host.ip
                      << std::setw(30) << host.hostname;

            if (host.openPorts.empty())
            {
                std::cout << "Brak";
            }
            else
            {
                for (size_t i = 0; i < host.openPorts.size(); ++i)
                {
                    std::cout << host.openPorts[i] << (i == host.openPorts.size() - 1 ? "" : ",");
                }
            }
            std::cout << std::endl;
        }
    }

    cleanupWinsock();
    return 0;
}
