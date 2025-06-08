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
[Zdrojový kód snifferu](https://github.com/Foyceek/ESP32-Wi-Fi-sniffer/tree/main/ESP-IDF/ESP32_Wi-Fi_SNIFFER)

[Zdrojový kód aplikace](https://github.com/Foyceek/ESP32-Wi-Fi-sniffer/blob/main/Probe_Request_Analysis_App/README.md)

Obě tlačítka mají každé 4 funkce:
- Krátký stisk (50-500ms)
- Střední stisk (500-1000ms)
- Dlouhý stisk (>1000ms)
- Dvojtý stisk

Časy se dají měnit, současné stisknutí obou tlačítek po dobu alespoň 2s zastaví aplikaci aby bylo možné bezpečně odpojit napájení.

### Funkce horního tlačítka

Procházení informací na OLED displeji

- Krátký stisk posun vpřed
- Střední stisk návrat na první záznam
- Dlouhý stisk přepínání mezi dobou vypnutí OLED při nečinnosti 5s, 30s nebo 1h
- Dvojitý stisk posun vzad

### Funkce dolního tlačítka

Uprava počtu záznamů na OLED displeji

- Krátký stisk +1 záznam
- Střední stisk obnovetní původního počtu záznamů
- Dlouhý stisk přepíná mezi zachytáváním probe requestů a spuštění webserveru s možností stažení sesbíraných dat.
- Dvojitý stisk -1 záznam

### Fotodokumentace

![zapojeni](https://github.com/user-attachments/assets/65e7d7c3-b3d8-4aaf-93ca-ba332e7c353a)

![brd](https://github.com/user-attachments/assets/0920484b-47d3-49ab-9737-f8c021d58a87)

Příklad captive portálu po krátkém spuštění snifferu

![server1](https://github.com/user-attachments/assets/6a84b53f-39b7-4a1f-9683-0bcdcdfa014b)

![server2](https://github.com/user-attachments/assets/f39c349e-f5ec-4509-88af-fab7db7d953b)

![server3](https://github.com/user-attachments/assets/3365ed92-6d4b-4bcf-a7f9-df0f376b8b1d)

![cover](https://github.com/user-attachments/assets/2015fd8f-3917-49bc-b3e5-515400cd47ba)

![front_left](https://github.com/user-attachments/assets/cd16d14e-1ad9-4449-83fd-e3f99a581656)

![back](https://github.com/user-attachments/assets/ba73f930-5efb-40a1-aba5-03c768b71ad5)



## Závěrečná práce
- Název: Vývoj embedded zařízení pro monitorování a sběr Wi-Fi probe requestů
- Název anglicky: Development of embedded device for monitoring and collecting Wi-Fi probe requests
## Základní literární prameny
[1] BRAVENEC, T. ESP32 Probe Request Sniffer. Online. GitLab. Dostupné z: https://gitlab.com/tbravenec/esp32-probe-sniffer. [cit. 2024-05-27]
[2] RUSCA, R., SANSOLDO, F., CASETTI, C., GIACCONE, P. What WiFi Probe Requests can tell you, 2023 IEEE 20th Consumer Communications & Networking Conference (CCNC), Las Vegas, NV, USA, 2023, pp. 1086-1091, doi: 10.1109/CCNC51644.2023.10060447
