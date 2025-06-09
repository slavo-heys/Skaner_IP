# Skaner IP i Portów w Sieci Lokalnej

Proste, ale wydajne narzędzie napisane w C++ do skanowania sieci lokalnej w poszukiwaniu aktywnych hostów oraz otwartych, popularnych portów TCP. Program wykorzystuje wielowątkowość do przyspieszenia procesu skanowania i jest kompatybilny zarówno z systemami Windows, jak i Linux/macOS.

## Funkcjonalności

*   **Skanowanie zakresu IP:** Umożliwia zdefiniowanie zakresu adresów IP do przeskanowania w ramach podanej podsieci (np. `192.168.1.1` do `192.168.1.254`).
*   **Wykrywanie otwartych portów:** Sprawdza listę predefiniowanych, popularnych portów TCP (m.in. 20, 21, 22, 23, 25, 53, 80, 443, 3306, 3389, 8080) na każdym aktywnym hoście.
*   **Rozpoznawanie nazw hostów:** Próbuje uzyskać nazwę hosta dla każdego aktywnego adresu IP.
*   **Skanowanie wielowątkowe:** Wykorzystuje wątki do równoczesnego skanowania wielu hostów, co znacząco skraca czas oczekiwania na wyniki.
*   **Konfigurowalne parametry:** Użytkownik może określić:
    *   Bazowy adres IP (np. `192.168.1`).
    *   Początkowy i końcowy numer w ostatnim oktecie adresu IP.
    *   Liczbę wątków skanujących.
    *   Timeout dla operacji sprawdzania portu (w milisekundach).
*   **Przejrzyste wyniki:** Wyświetla listę aktywnych hostów wraz z ich adresami IP, nazwami (jeśli dostępne) oraz listą otwartych portów.
*   **Wsparcie dla wielu platform:** Działa na systemach Windows (z Winsock2) oraz systemach POSIX (Linux, macOS) dzięki odpowiednim dyrektywom preprocesora.

## Kompilacja

Program można skompilować przy użyciu kompilatora g++ (lub innego wspierającego C++17).

**Linux/macOS:**
```bash
g++ -std=c++17 -o skaner_ip skaner_ip.cpp -lpthread

***Windows:**
```bash
g++ -std=c++17 -o skaner_ip.exe skaner_ip.cpp -lws2_32 -lpthread


-------------------------------------------------------------------------------
```bash
--- Wyniki Skanowania ---
Adres IP          Nazwa Hosta                   Otwarte Porty
----------------------------------------------------------------------
192.168.1.1       router.local                  53,80
192.168.1.15      DESKTOP-XYZ.local             135,139,445
192.168.1.22      raspberrypi.local             22,8080
...
