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
[Zdrojový kód snifferu](https://github.com/Foyceek/ESP32-Wi-Fi-sniffer/tree/main/ESP-IDF/radiotap_csv_pcap_3)

[Zdrojový kód aplikace](https://github.com/Foyceek/ESP32-Wi-Fi-sniffer/blob/main/Python/README.md)

Obě tlačítka mají každé 4 funkce:
- Krátký stisk (50-500ms)
- Střední stisk (500-1000ms)
- Dlouhý stisk (>1000ms)
- Dvojtý stisk

Časy se dají měnit, současné stisknutí obou tlačítek po dobu alespoň 2s restartuje celé ESP32.

### Funkce pravého tlačítka

Procházení informací na OLED displeji

- Krátký stisk posun vpřed
- Střední stisk návrat na první záznam
- Dlouhý stisk přepínání mezi dobou vypnutí OLED při nečinnosti 5s, 30s nebo 1h
- Dvojitý stisk posun vzad

### Funkce pravého tlačítka

Uprava počtu záznamů na OLED displeji

- Krátký stisk +1 záznam
- Střední stisk obnovetní původního počtu záznamů
- Dlouhý stisk přepíná mezi zachytáváním probe requestů a spuštění webserveru s možností stažení sesbíraných dat.
- Dvojitý stisk -1 záznam

### Prostor pro zlepšení
- Debug tvorby ZIP archivu
- Python - přidat funkce

### Fotodokumentace

![cirkit_radiotap_1](https://github.com/user-attachments/assets/558d47b1-5c94-41e6-a4a4-1bc2c05e1fe9)

Příklad captive portálu po krátkém spuštění snifferu

![CAP_AP](https://github.com/user-attachments/assets/8d05ecc0-101e-42ca-87ed-d28ffb53db6c)

![Prototyp1](https://github.com/user-attachments/assets/c92969a9-bd53-4d98-9b29-4a28ecc01424)

![Prototyp2](https://github.com/user-attachments/assets/553a48cf-5b35-4249-b05f-6d622c1f183f)

![Prototyp3](https://github.com/user-attachments/assets/bb4b56c4-5db7-492e-9b2a-b4e0cf100bb0)

## Závěrečná práce
- Název: Vývoj embedded zařízení pro monitorování a sběr Wi-Fi probe requestů
- Název anglicky: Development of embedded device for monitoring and collecting Wi-Fi probe requests
## Základní literární prameny
[1] BRAVENEC, T. ESP32 Probe Request Sniffer. Online. GitLab. Dostupné z: https://gitlab.com/tbravenec/esp32-probe-sniffer. [cit. 2024-05-27]
[2] RUSCA, R., SANSOLDO, F., CASETTI, C., GIACCONE, P. What WiFi Probe Requests can tell you, 2023 IEEE 20th Consumer Communications & Networking Conference (CCNC), Las Vegas, NV, USA, 2023, pp. 1086-1091, doi: 10.1109/CCNC51644.2023.10060447
