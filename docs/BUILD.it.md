# Compilazione Windows x64

[English](BUILD.md) · [Italiano](BUILD.it.md)

La base applicativa resta Veyon 4.7.5; tutti i moduli vengono ricompilati insieme.
Non mescolare DLL/plugin di installer upstream o ABI Qt/compilatore differenti.

L'installer include `netlink-maintenance.exe`, collegato staticamente al runtime
C++ e basato soltanto sulle API Windows, senza Qt. Salva la configurazione macchina
dal registro prima di arrestare/sostituire un'installazione danneggiata, verifica
l'elenco SHA-256 prima di avviare la CLI e gestisce la disinstallazione.
I test nativi e la prova completa NSIS usano directory private e sottochiavi HKCU
esatte. Servizio/firewall sono simulati: non è una prova privilegiata di
installazione/riavvio. Eseguire PILOT.md su PC Windows isolati.

## Ambiente e compilazione

Installare i pacchetti MSYS2 MinGW64:

```
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-qt5-base mingw-w64-x86_64-qt5-tools mingw-w64-x86_64-qca-qt5 mingw-w64-x86_64-openssl mingw-w64-x86_64-openldap mingw-w64-x86_64-lzo2 mingw-w64-x86_64-libjpeg-turbo mingw-w64-x86_64-libpng mingw-w64-x86_64-zlib mingw-w64-x86_64-nsis
```

Se l'indice MinGW64 non offre più `qca-qt5` o `openldap`, installare prima gli
altri pacchetti e poi eseguire nella shell MSYS2 MinGW64:
`bash .github/scripts/install-school-build-libraries.sh`.
Lo script scarica gli archivi ufficiali QCA 2.3.10-2 e OpenLDAP 2.6.12-1,
verifica gli SHA-256 fissati e lascia a pacman la verifica di firme e dipendenze.
È un passaggio sviluppatore/CI: l'installer include già il runtime necessario.
Non mescolare UCRT64 e MinGW64.

In PowerShell aggiungere `C:\msys64\mingw64\bin` a PATH e configurare:

Per il runtime Qt/KDE verificato eseguire prima `./packaging/prepare-qt51519.ps1`.
Controlla gli SHA-256 dei due pacchetti MSYS2 fissati, riutilizzando la cache
pacman se disponibile oppure recuperandoli dagli endpoint HTTPS ufficiali.
Estrae Qt base 5.15.19+kde+r96-1 e tools 5.15.19-1 nel nuovo prefisso `build-qt51519/mingw64`,
senza installare o sostituire pacchetti globali. Anteporre la sua cartella `bin`
a quella del compilatore in PATH e usare
`"-DCMAKE_PREFIX_PATH=$PWD/build-qt51519/mingw64;C:/msys64/mingw64"`
al posto del singolo prefisso indicato sotto. Usare una nuova cartella di build.
La configurazione Windows rifiuta Qt precedente a 5.15.19; il pacchetto richiede
esattamente il runtime 5.15.19 verificato e non ripiega su vecchie DLL Qt.

```
cmake -S . -B build-school -G Ninja -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=x86_64 -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 -DWITH_QT6=OFF -DWITH_BUILTIN_LIBVNC=ON -DWITH_TESTS=ON -DWITH_PCH=OFF -DWITH_UNITY_BUILD=OFF -DWITH_LTO=OFF -DWITH_TRANSLATIONS=OFF -DWITH_INTERCEPTION=OFF -DCMAKE_BUILD_TYPE=Release -DMINGW_PREFIX=C:/msys64/mingw64 -DMINGW_PLATFORM=win64 -DMINGW_TARGET=x86_64-w64-mingw32
cmake --build build-school -j 4
```

Eseguire `ctest --test-dir build-school --output-on-failure` con
`QCA_PLUGIN_PATH=C:\msys64\mingw64\share\qt5\plugins` per le prove crittografiche
nell'albero di build e il `bin` Qt isolato prima di quello globale in PATH.
Le applicazioni distribuite trovano il provider incluso tramite `qt.conf` e
non richiedono questa variabile d'ambiente. Mantenere inoltre
`build-school/core` e la directory bin della toolchain in PATH.
I test devono passare prima del pacchetto. Per la release creare una nuova
`build-release` con le stesse opzioni ma `WITH_TESTS=OFF`: esclude percorsi
plugin di sviluppo e hook runtime dei test.

## Discovery e risorse grafiche

La preferenza Master per utente è `SchoolDiscovery/AddressMode`
(`Automatic` o `IPv4Only`). I test del ciclo di vita iniettano uno store in memoria,
annunci simulati e DNS asincrono. Il test UDP IPv6 usa soltanto socket loopback
effimeri e dichiara lo skip se IPv6 non è disponibile: non apre regole scolastiche
né interroga PC configurati. Impostare `VEYON_SCHOOL_SCHEMA_FIXTURES` su una
directory di build prima di school-tests per esportare sei esempi SOAP;
eseguire poi `tests/validate-discovery.ps1 -Fixtures` con quella directory.
Le stesse specifiche ufficiali coprono i metadati delle due famiglie.

Se cambia il disegno vettoriale, rigenerare le icone:
`powershell -NoProfile -ExecutionPolicy Bypass -File branding/render-icon.ps1`.
Il rasterizzatore usa API grafiche Windows integrate; l'SVG originale resta disponibile.
`netlink.ico` è incorporata in eseguibili Windows e installer.
Gli eseguibili sono `netlink-*.exe`; target CMake e nomi DLL restano invariati.
Sono mantenuti directory protetta `Veyon School`, percorsi registro/chiavi,
identificativi funzionalità/protocollo e servizio `VeyonSchoolService`.
NetLinkIdentity centralizza avvio locale e percorsi rigidi dei peer WFP.
I nomi legacy sono accettati solo nei rapporti diagnostici e nella selezione
dell'aggiornamento, mai come alias fidati della pipe WFP.
Questa alpha Windows non cambia i nomi degli eseguibili non Windows.

## Aspetto e prove grafiche

Le preferenze sono separate per utente in
`HKCU\Software\Veyon\NetLinkAppearance`, non nelle impostazioni di sistema/autenticazione.
I test offscreen accettano `NETLINK_APPEARANCE_SCREENSHOTS` per PNG;
su Windows impostare `QT_QPA_FONTDIR=C:\Windows\Fonts` per i font di sistema.
Nessuna variabile è richiesta dalla GUI installata.

Set Organico/Lineare/Spettro/Classico, dimensioni toolbar 28/36/44 pixel logici
e viste Facile/Normale/Avanzata del Configurator usano lo stesso store separato.
Cambiare vista non azzera valori nascosti. I glifi vettoriali sono percorsi QPainter
in `core/src/NetLinkUi.cpp`, senza font esterni o nuove librerie SVG/icone.
Le risorse del marchio restano invariate. Lo scaling DPI è abilitato soltanto
negli ingressi Master/Configurator/Companion, non in server o worker acquisizione/input.

Il tema condiviso usa popup a elenco, non righe in stile menu.
`netlink-combo-popup-scale-1`, `-1_5` e `-2` aprono i veri popup Qt con temi
chiaro/scuro e caratteri applicativi normali/ingranditi. Verificano righe delle
liste brevi, destinatari modificabili, conferma/annullamento da tastiera,
scorrimento e bordi schermo. `NETLINK_COMBO_SCREENSHOTS` abilita PNG di finestra
e popup. Usano preferenze isolate, senza inizializzare autenticazione o rete.

`netlink-ui-scale-1`, `netlink-ui-scale-1_5` e `netlink-ui-scale-2` provano
la pagina directory reale con modello JSON isolato, rendering vettoriale,
pulsanti nativi accessibili, conservazione vista, conferma rimozione gruppo e
dialoghi tray che non terminano il processo. `NETLINK_UI_SCREENSHOTS` abilita i PNG.
Le anteprime Console usano controlli reali con schede sintetiche, non un Master connesso.
Non sostituiscono prove native di tray/DPI/input remoto.

`netlink-remote-toolbar-scale-1`, `-1_5` e `-2` provano il pannello remoto
compatto reale senza connessione VNC, servizio o lettura chiavi.
`NETLINK_REMOTE_SCREENSHOTS` abilita le anteprime. La stessa sottoclasse è usata
in produzione; i collegamenti VNC restano in RemoteAccessWidget.cpp.
Focus/grab tastiera, DPI misti e interazione remota richiedono collaudo reale.

`netlink-lock-notification-tests` usa widget inerti, worker isolati e fixture
di canali autenticati. Classifica scorciatoie senza installare hook reali.
Gli eventi di sessione usano WTS; le verifiche del desktop input sono in sola
lettura e non prendono controllo del desktop protetto.
Il core collega Wtsapi32 di sistema: nessun redistribuibile aggiuntivo.
Vedere PILOT.md e AUDIT.md per collaudi nativi e criteri beta.

## Companion

`branding/render-companion-icon.ps1` genera l'icona da `companion.svg`
con API Windows. L'output è `companion/netlink-companion.exe`, con runtime
Qt/core esistente e sole API Windows. L'installer aggiunge il terzo collegamento Start.
Il collector è un processo figlio separato in sola lettura, limitato dalla GUI
a 30 secondi/2 MiB, senza VeyonCore né inizializzazione servizio/plugin.
`--collect` emette un rapporto completo su stdout con identificativi:
non reindirizzarlo/condividerlo senza protezione appropriata dei dati.
`--version` emette solo la versione senza raccolta.

## Pacchetto e installer

Da payload `dist` nuovo:
`powershell -NoProfile -ExecutionPolicy Bypass -File packaging/build-package.ps1 -Build build-release`.
Compila le traduzioni senza modificare i TS, risolve ricorsivamente le importazioni
DLL, conserva le licenze e genera NSIS. Il target legacy `windows-binaries`
non è usato su Windows.

Prima di sostituire file l'installer esegue l'helper estratto senza Qt, non la vecchia
CLI installata. Controlla rimozioni pendenti, salva impostazioni, arresta il servizio
di proprietà senza deregistrarlo e verifica accesso esclusivo in scrittura alle
destinazioni del nuovo manifesto, senza modificarne il contenuto.
Solo dopo esito positivo consente deregistrazione/copia.
Un file occupato termina con 3010 e richiesta esplicita di riavvio; i problemi
di permessi sono distinti. Nessun riavvio/arresto processi forzato o opzione Ignora.
Il controllo non impedisce riaperture successive e non offre rollback transazionale.
Il pacchetto usa una lista esplicita di otto EXE ignorando output obsoleti.
L'installer offre i ruoli Master/Client conservati negli aggiornamenti. Client
omette solo EXE/collegamento Master. Genera manifesti SHA-256 completo/client;
installa solo quello scelto come payload.sha256. Il controllo completo verifica
anche l'EXE Master esistente prima della rimozione in modalità Client. Le prove
isolate coprono cambi di ruolo, persistenza, argomenti errati e collegamenti.

Solo i sette EXE legacy noti nella directory fissa del fork sono rimossi dopo
l'avvio del nuovo servizio. La reinstallazione alpha.9 o successiva preferisce
la CLI corrente. La fixture NSIS prova la stessa selezione/rimozione su file inerti
privati, senza operazioni reali su servizio, registro o riavvio.
Non dimostra il successo di installazione/aggiornamento/disinstallazione privilegiati:
resta necessario il collaudo guidato Windows.

## Sorgenti e dipendenze

Con il prefisso Qt isolato passare anche
`-QtPrefix "$PWD/build-qt51519/mingw64"` a `packaging/build-package.ps1`.
`verify-package.ps1` verifica le versioni delle DLL Qt e dei plugin Windows/JPEG,
oltre agli hash, per impedire pacchetti misti 5.15.18/5.15.19.

Le modifiche UltraVNC sono in `patches-ultravnc.diff`, applicate solo alla
revisione fissata. La configurazione fallisce se i sorgenti non sono originali
o esattamente corretti dalla patch. Dopo modifiche vendor rigenerare/ispezionare
la patch prima del commit; il riferimento del sottomodulo resta invariato.

L'archivio completo deve includere sottomoduli e patch; lo ZIP automatico GitHub
non include i contenuti dei sottomoduli. Verifica importazioni DLL e test unitari
non sostituiscono prove installer su VM pulita né collaudi WFP/rete.

Dopo il commit della release, `packaging/build-source-package.ps1` crea
l'archivio con tutti i sottomoduli fissati. I sorgenti vendor sono originali;
CMake applica le tre patch. Binari legacy opachi sono esclusi dagli export Git.

`packaging/build-runtime-sources.ps1` raccoglie sorgenti copyleft runtime e
ricette esatte. Richiede GitHub CLI e archivi originali MSYS2 nella cache locale.
Accetta `-QtBasePackage` per l'archivio Qt isolato e `-Output` per una cartella
sorgenti separata: la versione distribuita non viene dedotta dal pacman globale
rimasto invariato. `netlink-qt-runtime-tests` verifica firma RSA PUBBLICA e
messaggi prodotti con Qt 5.15.18, rifiuto delle alterazioni, importazione PEM,
firma corrente e OAEP, senza leggere chiavi o configurazione reali.
Il generatore autonomo è `tests/QtCompatibilityFixture.cpp`; il JSON incluso
non contiene chiavi private. Vedere la [valutazione Qt/KDE](QT-RUNTIME.it.md).
Le ricette sono confrontate con SHA-256 BUILDINFO senza eseguire script scaricati.
L'archivio runtime è un asset separato; questi strumenti di sviluppo non servono sui client.
