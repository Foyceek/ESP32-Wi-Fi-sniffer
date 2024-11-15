# ESP32-Wi-Fi-sniffer
## Užitečné odkazy
- [ESP32 One overview](https://www.waveshare.com/wiki/ESP32_One)
- [Overleaf](https://www.overleaf.com/project)
- [VUT závěrečné práce](https://www.vut.cz/studenti/zav-prace)
- [IEEE Xplore](https://ieeexplore.ieee.org/Xplore/home.jsp)
- [How to install MicroPython on an ESP32 microcontroller](https://pythonforundergradengineers.com/how-to-install-micropython-on-an-esp32.html)
- [ESP-IDF course](https://github.com/tomas-fryza/esp-idf)
- [ESP-Arduino course](https://github.com/tomas-fryza/esp-arduino)
- [ESP-Micropython course](https://github.com/tomas-fryza/esp-micropython)
- [Probe request project](https://github.com/tomas-fryza/probe-request-project)
- [ESP32 Probe Sniffer-tbravenec](https://gitlab.com/tbravenec/esp32-probe-sniffer)
## Současný prototyp
[Zdrojový kód](https://github.com/Foyceek/ESP32-Wi-Fi-sniffer/tree/main/ESP-IDF/radiotap_csv_pcap_1)

Obě tlačítka mají každé 4 funkce:
- Krátký stisk (50-500ms)
- Střední stisk (500-1000ms)
- Dlouhý stisk (>1000ms)
- Dvojtý stisk

Časy se dají měnit

### Funkce pravého tlačítka

Procházení informací na OLED displeji

- Krátký stisk posun vpřed
- Střední stisk návrat na první záznam
- Dlouhý stisk vymazání všech záznamů
- Dvojitý stisk posun vzad

### Funkce pravého tlačítka

Uprava počtu záznamů na OLED displeji

- Krátký stisk +1 záznam
- Střední stisk obnovetní původního počtu záznamů
- Dlouhý stisk ukončí sniffer a aktivuje captive portál, další restartuje celé ESP32
- Dvojitý stisk -1 záznam

![cirkit_radiotap_1](https://github.com/user-attachments/assets/558d47b1-5c94-41e6-a4a4-1bc2c05e1fe9)

Příklad captive portálu po krátkém spuštění snifferu

![CAP_AP](https://github.com/user-attachments/assets/8d05ecc0-101e-42ca-87ed-d28ffb53db6c)

## Závěrečná práce
- Název: Vývoj embedded zařízení pro monitorování a sběr Wi-Fi probe requestů
- Název anglicky: Development of embedded device for monitoring and collecting Wi-Fi probe requests
## Základní literární prameny
[1] BRAVENEC, T. ESP32 Probe Request Sniffer. Online. GitLab. Dostupné z: https://gitlab.com/tbravenec/esp32-probe-sniffer. [cit. 2024-05-27]
[2] RUSCA, R., SANSOLDO, F., CASETTI, C., GIACCONE, P. What WiFi Probe Requests can tell you, 2023 IEEE 20th Consumer Communications & Networking Conference (CCNC), Las Vegas, NV, USA, 2023, pp. 1086-1091, doi: 10.1109/CCNC51644.2023.10060447
