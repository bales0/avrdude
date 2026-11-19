# HVprog2 / ScratchMonkey – stabilizace 19. 11. 2026

## Audit a změny

Výchozí i současný HEAD tohoto projektu B je
`cd794b01819b3f3b77614ff824a08dd2f7e884c7`, větev `main`.
Pracovní strom byl před touto etapou čistý. Nevznikla nová větev ani commit.
Firmware A má HEAD `c04c7e463645c6db5fc309ec5296c125aed2dd49`.
Podrobný společný report: [STABILIZATION.md](../ScratchMonkey-orangkucing/STABILIZATION.md).

| Soubor B | Změna / důkaz |
|---|---|
| `src/stk500v2.c` | Dva clampy v JTAGmkII/3 receive: `maxsize + 1` zahrnuje bajt obálky; následné kopírování `rv - 1` dříve zahodilo poslední bajt padded odpovědi. Test selhal před opravou a prošel po ní. |
| `tests/stk500v2_transport.c` | Test skutečného backendu s virtuálním serial/USB/JTAG transportem: délky/statusy, chybné odpovědi a zákaz opakování erase/write po ztrátě ACK. |
| `tests/run_stk500v2_tests.py` | Samostatné sestavení a spuštění testu proti existujícímu MSYS2 CMake buildu. Bez produkčních testovacích hooků. |
| `README.md`, `HVPROG2.md` | Odkaz a tato přejímací dokumentace. |

Ostatní změny potřebné pro ScratchMonkey již byly v uvedeném HEAD: podmíněný
`CMD_XPROG_SETMODE` pouze pro TPI/PDI, validace odpovědí, fragmentace PDI,
parametr velikosti stránky a zákaz replay. Nebyly znovu implementovány.
Nedochází ke změně generického AVRDUDE ani identifikace programátoru.

## Profily a závislosti

| Režim | `-c` | Veřejný AVRDUDE 8.3 | Tento fork |
|---|---|---|---|
| ISP | `stk500v2` | PASS, SIMULATED | PASS, SIMULATED |
| HVSP ATtiny13 | `stk500hvsp` | PASS, SIMULATED | PASS, SIMULATED |
| HVPP ATmega32 | `stk500pp` | PASS, SIMULATED | PASS, SIMULATED |
| TPI | `stk500v2` | Toto HVprog2 rozšíření není deklarováno jako podporované | PASS, SIMULATED |
| PDI | `stk500v2` | Toto HVprog2 rozšíření není deklarováno jako podporované | PASS, SIMULATED |

Aliasy `scratchmonkey`, `scratchmonkey_hvsp`, `scratchmonkey_pp` jsou stejné
profily, nikoli jiný handshake nebo důkaz XPROG. Firmware se stále hlásí
`STK500_2`. Standardní režimy neposílají XPROG probe; odmítnutý probe u
TPI/PDI vede k chybě bez ISP fallbacku. Standardní STK500 bez XPROG prošel
ISP testem a správně odmítl TPI i PDI.

Historický Microtherionův patch vázal XPROG na `SCRATCHMONKEY`; Orangkucingův
patch jej dovoloval pro obecný STK500. Současná detekce ověřuje schopnost
příkazem, pouze když ji vyžaduje zvolený cílový čip.

Bezpečné 512B PDI stránky vyžadují správné příznaky posledního fragmentu.
Starý klient potvrzující každý 256B fragment tuto podmínku nesplňuje.
TPI LV/HV se volí existujícím adaptérem, nikoli novým profilem. TPI firmware
zapíná generátor v obou variantách; RESET5V/RESET12V odděluje konektor adaptéru.
PDI vyžaduje správný 3,3V adaptér. Hlášené napětí FW není fyzické měření.

## Ukončení a zapojení

Klient používá standardní LEAVE; při chybě se o něj pokusí, pokud transport
funguje. Po ztraceném ACK se potenciálně dokončený write/erase neopakuje.
Nový firmware A přidává vlastní 30s idle lease aktivního HVSP/HVPP/TPI:
vypne generátor, stáhne RESET, vypne VTARGET, zastaví periferie a uvolní bus.
Kontrola běží mezi příkazy, takže nepřeruší platnou dlouhou operaci.
PDI/ISP tento HV lease nemají. Při delší interaktivní pauze je třeba nový ENTER.
Lease vyžaduje nový firmware; samotná aktualizace AVRDUDE jej nepřidá.

HVSP ATtiny13 dle ověřeného schématu `HVprog2.jpg`, elektrická čísla IDC HV_PROG:

| Signál | HV_PROG kontakt | ATtiny13 DIP8 |
|---|---:|---:|
| SDI | 1 | 5 / PB0 |
| SII | 2 | 6 / PB1 |
| SDO | 3 | 7 / PB2 |
| SCI | 17 | 2 / PB3 |
| RESET 12 V | 18 | 1 / RESET |
| VTG/VCC | 19 | 8 |
| GND | 20 | 4 |

VCC ověřit 4,5–5,5 V, 12 V pouze RESET. Pohled na protikus/zadní stranu IDC
je zrcadlený: použít skutečný pin 1 a ověřit kontinuitu. Historický obrázek
Arduino kabeláže neurčuje číslování konektoru HVprog2.

## Reprodukce a výsledky

Build: MSYS2 UCRT64 GCC 16.2.0 rev. 3, CMake 4.4.3, Ninja 1.13.2,
Bison 3.8.2, Flex 2.6.4. Výsledný klient hlásí `8.2-20261118 (cd794b01)`;
pracovní oprava není samostatný commit. Standardní protokol byl porovnán
s oficiálním [veřejným AVRDUDE 8.3](https://github.com/avrdudes/avrdude/releases/tag/v8.3).

Z kořene sousedního projektu A:

```powershell
.\Tools\BuildAvrdude.ps1
python ../avrdude/tests/run_stk500v2_tests.py --build-dir .pio/tools/avrdude/build --output-dir .pio/stabilization
python Tests/host/run_tests.py
python Tests/host/acceptance_tests.py
python Tests/host/avrdude_tests.py
```

Všechny uvedené testy prošly. **SIMULOVÁNO / REÁLNĚ NEOVĚŘENO:** skutečné
binárky AVRDUDE komunikují přes loopback se simulátorem nebo přeloženým
firmwarem nad SPI/GPIO modelem. Fyzický COM port nebyl použit. Matice přesných
argv a výsledků je v A `.pio/stabilization/compatibility-matrix.json` a
`xprog-matrix.json`; logy jsou ve stejném adresáři.

Veřejný klient i fork přečetly signatury, fuse, lock a kalibraci přes všech
šest profilů/aliasů. Celé standardní ENTER/LEAVE/control-stack pakety se shodují.
XPROG testy pokrývají stránky 32–512 B, necommitnutý první 256B fragment,
read/verify, erase, fuse, chyby FAILED/COLLISION/TIMEOUT, krátké/duplikované
odpovědi a ztrátu transportu. Standardní transporty testují skutečný backend
bez připojeného USB/JTAG hardwaru.

## Omezení a přejímka

Nový HEX nebyl nahrán. Dřívější ISP ATtiny13 byl fyzicky přečten; uživatel
potvrdil HVSP po opravě kabeláže. To nenahrazuje přejímku nové verze s lease.
Fyzické TPI/PDI, LV/HV TPI, vypnutí napětí a transienty RESET zůstávají
**SKIP, HARDWARE / REÁLNĚ NEOVĚŘENO**. Před vydáním ověřit napětí a kabeláž,
po samostatně schváleném nahrání FW provést čtení standardních režimů a
změřit vypnutí při odmlčení hosta. Žádný automatický flash/erase/fuse write
není součástí této etapy.

Lease není nezávislý hardwarový watchdog a nepřeruší uváznutí uvnitř blokující
knihovní funkce. JTAGmkII nemá XPROG ve své ISP send tabulce; test opravy
receive obálky nerozšiřuje tuto existující schopnost. CRC regrese ověřuje
formát výsledku, nikoli nezávislý výpočet hardwarového algoritmu.
