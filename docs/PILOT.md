# Collaudo guidato: due PC Windows 10 x64

Solo PC di prova con accesso fisico. Salvare prima configurazione e chiavi Veyon.
Non pubblicare chiavi private, password o esportazioni di configurazione.

## Preparazione

1. Confrontare lo SHA-256 dell’installer con quello della release. La alpha non
   è firmata. Installare come amministratore su docente e client; accettare lo
   stop/disabilitazione del servizio originale solo sui PC di prova.
2. Configurare autenticazione a chiave nel Configurator: privata solo sul docente,
   pubblica sul client, diritti della privata limitati al docente. Non copiare la
   privata sui client. Riusare aule, PC e autorizzazioni già configurati.
3. Nel docente aggiungere il client all’aula di prova per nome/IP. Non selezionare
   il docente come destinatario di InternetGuard.
4. Nel client impostare School/DiscoveryMaster al nome/IP del docente, per provare
   anche HTTP sulla stessa subnet. Profilo rete Privato/Dominio; riavviare il
   servizio dopo modifiche. Nel Master la miniatura deve aggiornarsi.
5. Annotare build OS, antivirus/firewall, latenza e carico CPU a riposo.

## 1.0.0-alpha.19: filtro dei client già configurati

1. Esportare il JSON del Configurator e conservare le chiavi esistenti. Chiudere
   le applicazioni sul MATEBOOK e aggiornare con il pacchetto completo come
   amministratore, senza disinstallare o rigenerare `netlink_test`. Se richiesto,
   riavviare Windows e rieseguire lo stesso installer. Verificare la versione
   **1.0.0-alpha.19** in Informazioni e la configurazione/chiave conservate.
2. Con il DELL acceso e già presente in Posizioni e computer, aprire Trova client.
   Il filtro deve essere attivo: il DELL non deve apparire tra i nuovi risultati,
   ma deve essere incluso nel conteggio dei PC già configurati dopo gli annunci.
3. Disattivare il filtro: il DELL deve ricomparire con il proprio IPv4 e la voce
   Già configurato. Tentare l'importazione non deve creare una seconda riga.
   Riattivare il filtro: la selezione di un PC nascosto non deve spostarsi su un
   altro risultato. Il docente stesso continua a essere escluso.
4. Se un PC è configurato per hostname, attendere il controllo DNS; errori o
   timeout devono lasciare il risultato visibile e aumentare i non risolti, senza
   bloccare la Console. Chiudere durante il controllo e riaprire senza crash.
   Con DHCP la verifica dipende dalla risoluzione corretta e aggiornata del nome.
5. Se è disponibile un ulteriore PC non configurato, verificarne l'importazione
   esplicita: deve sparire dai nuovi risultati subito dopo il salvataggio.
   Verificare nuovamente miniatura, controllo remoto, Appello e InternetGuard.

Per questo filtro è sufficiente aggiornare il Master; il DELL può restare alla
versione precedente. Nessun nuovo prerequisito o configurazione sui client.
La ricerca continua a includere solo risultati IPv4: non è un collaudo IPv6-only.

## 1.0.0-alpha.18: puntina permanente e un solo bordo attivo

1. Conservare JSON e chiavi; chiudere le applicazioni e aggiornare il Master con
   il pacchetto completo. Se richiesto, riavviare e rilanciare l'installer. Non
   rigenerare chiavi. Il client non richiede modifiche per il nuovo menu.
2. Aprire il controllo remoto del DELL: inizialmente solo il bordo superiore deve
   richiamare la barra. Provare gli altri tre bordi senza provocarne lo spostamento.
3. Uscire dalla barra: i comandi si richiudono ma la puntina resta nel medesimo
   punto. Dopo circa 1,8 s si attenua, senza scomparire. Tornando sopra la puntina
   deve diventare subito nitida e richiamare i comandi dopo una breve sosta.
4. Clic sinistro sulla puntina: fissare e sbloccare la barra aperta. Clic destro
   sulla puntina, sullo sfondo della barra e sui suoi pulsanti: devono spostarsi
   insieme alto → sinistra → basso → destra → alto, una volta per clic e senza
   inviare comandi al DELL. Provare anche quando la barra è chiusa o fissata.
5. Dopo ogni spostamento solo il nuovo bordo deve richiamare la barra. Verificare
   orientamento, posizione della puntina invariata tra apertura/chiusura, fullscreen,
   finestre strette, DPI 100/150/200%, lingua e cambio di aspetto in tempo reale.
6. Tenere aperto un menu o navigare nei pulsanti con la tastiera: barra e puntina
   devono restare leggibili. Provare contrasto elevato Windows: niente attenuazione.
   Richiudere la sessione e riaprirla: si riparte dall'alto, senza nuove impostazioni.
7. In Informazioni e nei dettagli dell'icona verificare **1.0.0-alpha.18**. La base
   tecnica e il protocollo restano Veyon 4.7.5: le versioni tecniche nei rapporti/CLI
   conservano quel prefisso. Non equivale al passaggio a una versione stabile.

## Alpha.16: barra compatta del controllo remoto

1. Salvare JSON e chiavi; chiudere le applicazioni e aggiornare con il pacchetto
   completo. Se richiesto, riavviare e rieseguire l'installer prima del collaudo.
   La modifica del menu riguarda il Master; non cambia il protocollo del client.
2. Dal MATEBOOK controllare il DELL: passare sui quattro bordi e attendere circa
   350 ms. Verificare posizione centrata e orientamento, assenza di oscuramento,
   linguetta, scomparsa dopo l'uscita e richiamo senza attivazioni accidentali.
3. Attivare **Fissa barra**, spostare il puntatore sul desktop remoto, attendere e
   passare tra finestra e fullscreen: deve restare aperta. Disattivare la puntina:
   deve tornare la chiusura automatica. La scelta vale per la sessione corrente.
4. Aprire **Invia combinazione di tasti** e, se disponibile, **Seleziona schermo**:
   la barra non deve sparire mentre il menu è aperto. Verificare che dopo il menu
   la tastiera torni al desktop remoto e che non restino tasti/modificatori premuti.
5. Provare screenshot, sola visualizzazione/controllo e **Disconnetti**: il DELL
   deve restare acceso, senza chiudere le applicazioni. Ridurre la finestra e
   ripetere a 100/150/200%, con due monitor se disponibili, anche spostando la
   finestra tra monitor con scale diverse. Controllare lingua italiana/inglese.
6. Riprovare Appello con Invio, miniatura, InternetGuard e discovery. Le prove
   automatiche della sola barra non sostituiscono il collaudo remoto su Windows.

## Alpha.15: aggiornamento e file ancora in uso

1. Conservare il JSON del Configurator e le chiavi. Chiudere Console, Configurator
   e Companion, poi avviare alpha.15 come amministratore nella normale cartella.
2. Se compare **Riavvio necessario**, la copia non è iniziata: chiudere l'installer,
   scegliere **Riavvia** in Windows e rilanciare lo stesso installer. Non cancellare
   DLL, non usare installer vecchi per aggirare il controllo e non rigenerare chiavi.
   Il servizio precedente può essere fermo fino al riavvio, ma resta registrato.
3. Verificare configurazione, `netlink_test`, icona, miniatura e controllo remoto;
   poi Appello con Invio e didascalie. Ripetere sul DELL con lo stesso pacchetto.
4. Per il collaudo di disinstallazione, soltanto su PC di prova: prima salvare il
   JSON e le chiavi, poi disinstallare conservando le impostazioni. Se Windows
   segnala il riavvio, non saltarlo. Una reinstallazione anticipata con file ancora
   occupati deve fermarsi prima della copia, non proporre di ignorare una DLL.
5. Un messaggio di autorizzazioni/sola lettura non è la prova di un file occupato:
   conservare il testo e il registro `C:\ProgramData\NetLink\maintenance-error.txt`.
   Non disattivare antivirus o firewall e non cambiare permessi alla cieca.

La nuova alpha rende esplicito il riavvio necessario, non lo elimina. Il controllo
non è una transazione atomica né garantisce rollback dopo qualunque errore di copia.

## Alpha.14: didascalie e Invio nell'Appello

1. Esportare il JSON dal Configurator e conservare le chiavi esistenti; non
   pubblicare questi file. Chiudere Console, Configurator e Companion sul MATEBOOK,
   aggiornare con l'installer completo come amministratore, senza disinstallare
   e senza rigenerare `netlink_test`. Riavviare e verificare miniatura/controllo del DELL.
2. Aggiornare anche il DELL con lo stesso installer e riavviare: la correzione di
   Invio è nella finestra del client, non soltanto nella Console docente.
3. Richiedere Appello sul solo DELL e digitare un nome fittizio, poi premere Invio.
   La finestra deve chiudersi e il nome arrivare al Master. Ripetere con Invio del
   tastierino, OK, Annulla, Esc e nome vuoto; non devono arrivare risposte doppie.
4. Verificare `[nome] - utente Windows - nome PC`, sfondo solo sul nome,
   assenza della dicitura di origine nella didascalia e stato InternetGuard ancora
   leggibile. Con un profilo caricato verificare sfondo tenue/corsivo del nome
   precaricato e origine/differenza nel tooltip. Provare temi chiaro/scuro,
   selezione e dimensioni miniature abituali; la ricerca deve trovare nome, account e PC.

I file classe–aula rimangono compatibili. L'installer preserva configurazione e
chiavi nei percorsi già configurati; non occorre reimportare il JSON durante un
aggiornamento normale. Le prove isolate non sostituiscono questo collaudo reale.

## Alpha.13: profili classe–aula sul MATEBOOK

Aggiornare prima solo il MATEBOOK, chiudendo tutte le app NetLink e usando
l’installer completo come amministratore, senza disinstallare né rigenerare chiavi.
Conservare prima il JSON del Configurator e le chiavi. Questa funzione non richiede
modifiche al client DELL, che può restare alla alpha già funzionante.

1. Verificare `netlink_test`, miniatura e controllo remoto del DELL.
2. In **Posizioni e computer** controllare i PC desiderati e aprire **Profili
   classe–aula…**. Inserire classe e aula, modificare il nome assegnato al DELL.
   Controllare elenco laterale, miniatura e tooltip: deve indicare nome precaricato.
3. **Salva JSON**, scegliere una cartella personale protetta. Chiudere e riaprire
   il Master: i nomi non si caricano automaticamente. Usare **Carica JSON** dal
   pannello e verificare la stessa associazione. Il file non è il backup del Configurator.
4. Selezionare il DELL nella Console, usare **Appello → Richiedi nomi** e inserire
   sul client un nome diverso. Verificare nome dichiarato e differenza dall’assegnazione,
   senza conferme di presenza. Il JSON non deve cambiare. **Usa nomi dichiarati** nel
   profilo copia la risposta nelle assegnazioni soltanto su richiesta; poi salvare.
5. Modificare un’assegnazione e chiudere il pannello: la bozza deve restare in memoria.
   Per sostituirla con un altro profilo confermare esplicitamente lo scarto o salvare.
   Caricare una copia del file con un’altra classe/nome mentre il client deve ancora
   rispondere: la risposta tardiva della vecchia richiesta non deve cambiare la classe nuova.
6. Deselezionare e riselezionare il DELL nel gruppo: deve tornare il nome precaricato,
   non una vecchia dichiarazione. Riavviare il servizio DELL durante una dichiarazione:
   dopo la riconnessione resta soltanto l’assegnazione salvata, non una presenza verificata.
7. Esportare **CSV** e controllare classe, aula, PC, nome assegnato, dichiarazione,
   origine e connessione. Le colonne devono distinguere precaricamento da risposta.
8. Caricare un file JSON incompleto/non valido e un backup del Configurator: entrambi
   devono essere rifiutati senza perdere i nomi attivi. Provare **Nuovo / scarica profilo**:
   spariscono le etichette attive, ma i file JSON rimangono sul disco.

Cambio IP: con lo stesso UUID il profilo conserva l’assegnazione, ma non corregge
un vecchio IP numerico nel Configurator. PC cancellati/ricreati risultano non associati.
Verificare leggibilità del pannello a 100/150/200%, lingua italiana/inglese e
reattività durante selezione file, caricamento, salvataggio e chiusura.

## Ripristino e aggiornamento ad alpha.12

Sul MATEBOOK con l'installer alpha.11 bloccato: chiudere errore e installer,
chiudere tutte le app della suite e avviare direttamente l'installer alpha.12
come amministratore. Non cancellare manualmente la cartella e non disinstallare
di nuovo prima del ripristino. Il backup non dipende dal vecchio eseguibile Qt.
L'installer verifica gli hash dei file prima di avviare la nuova CLI.

Controllare versione, PC configurati e chiave `netlink_test`, quindi miniatura,
controllo remoto, Appello, discovery e InternetGuard. Annotare eventuali errori
mostrati nei dettagli dell'installer; il dettaglio dell'ultimo errore nativo è
in `C:\ProgramData\NetLink\maintenance-error.txt` (cartella protetta).

### Prova guidata di disinstallazione/reinstallazione su PC di prova

1. Dopo il ripristino esportare il JSON dal Configurator. Conservare separatamente
   le chiavi esistenti: il JSON e i `.reg` non contengono le chiavi private.
2. Rilevare nel Configurator i percorsi di chiavi pubbliche/private; normalmente
   sono sotto `C:\ProgramData\Veyon\keys`. Le chiavi non devono stare nella
   cartella del programma: in quel caso la rimozione si ferma prima di cancellare.
3. Disinstallare da Windows. **No** conserva le impostazioni; **Sì** le elimina
   mantenendo solo i percorsi delle chiavi. Entrambe le scelte eliminano l'intera
   cartella del programma, inclusi file aggiunti manualmente. La copia di recupero
   delle impostazioni di sistema e dei vecchi backup resta protetta in
   `C:\ProgramData\NetLink\Recovery`.
4. Riavviare se richiesto. Verificare l'assenza di `C:\Program Files\Veyon School`,
   del servizio `VeyonSchoolService`, delle scorciatoie e delle regole firewall
   riferite a quel percorso. Non basta che l'app non compaia più nell'elenco Windows.
5. Con rimozione completa verificare nelle viste registro 64/32 bit la rimozione
   delle impostazioni della suite. Sono intenzionalmente conservati i percorsi
   `Authentication/PrivateKeyBaseDir` e `PublicKeyBaseDir`, se personalizzati,
   le impostazioni condivise con Veyon originale e i profili non caricati.
   Non cancellare a mano voci trovate cercando genericamente "Veyon" o "NetLink".
6. Reinstallare alpha.12. Dopo la rimozione completa reimportare il JSON e applicare
   dal Configurator; dopo la rimozione con conservazione le impostazioni devono
   essere già presenti. Verificare l'identità delle chiavi e la connessione al DELL.

I test automatizzati eseguono il flusso NSIS reale con file e HKCU isolati,
simulando servizio/firewall. Il ciclo amministrativo sui PC reali, i file in uso e
il riavvio richiedono questa prova: non vengono dichiarati collaudati dai test isolati.

## Procedura generale di aggiornamento

Su entrambi i PC esportare prima la configurazione dal Configurator e conservare
al sicuro le chiavi esistenti. Chiudere Console, Configurator e Companion.
Installare l’installer completo come amministratore prima sul DELL client,
quindi sul MATEBOOK docente, senza disinstallare o rigenerare chiavi. Riavviare
entrambi. Aprire le applicazioni dalla cartella **Start → NetLink** e verificare
la versione alpha.12. La chiave `netlink_test` e i PC configurati devono restare
invariati. Confermare miniatura e controllo remoto prima delle prove successive.

## Collaudo alpha.11: MATEBOOK docente e DELL client

Riferimenti attuali: MATEBOOK14S-RMELE, 192.168.1.195 (docente), e DELL,
192.168.1.109 (client). Gli indirizzi DHCP possono cambiare: verificarli prima
delle prove. Entrambi sono Windows 10 x64 su Wi-Fi nella stessa subnet.
Eseguire soltanto dopo aver aggiornato entrambi con il nuovo installer completo.
Non copiare singoli eseguibili o DLL dalla
cartella di compilazione sopra l'installazione esistente.

1. Nel Configurator selezionare English US, Applica, poi chiudere e riaprire le
   applicazioni: Console, Attendance, Find clients e Appearance devono essere
   coerenti. Ripetere con italiano. I dialoghi nativi Windows possono mantenere
   la lingua del sistema operativo. Il titolo della Console non deve contenere
   Veyon né duplicare NetLink; restano le attribuzioni legali in Informazioni.
2. Provare Spectrum/Spettro, chiaro/scuro, scale 100/150/200%, finestra stretta e
   ridimensionamento. Le etichette devono essere complete; la freccia di overflow
   deve rendere utilizzabili anche gli ultimi comandi, espandendo righe o menu.
3. Dal MATEBOOK attivare Lock sul DELL. Sul DELL provare Alt+Tab, Alt+F4,
   Ctrl+Esc, Ctrl+Maiusc+Esc e tasto Windows. Nella Console verificare il pannello
   non bloccante Eventi durante il blocco, con PC, orario di ricezione e contatore.
   Gli avvisi sono raggruppati circa ogni due secondi e non raccolgono testo.
4. Provare Ctrl+Alt+Canc mantenendo visibile la schermata per almeno due secondi:
   attendersi un evento di schermata protetta (oppure desktop non verificabile),
   NON l'affermazione che siano stati identificati quei tasti. Tornare al desktop:
   deve ricomparire la copertura NetLink e l'avviso di ritorno. Ripetere con Win+L
   se consentito da Windows: gli eventi di sessione non provano la combinazione.
5. Aprire volontariamente una richiesta UAC durante il blocco, se possibile:
   deve essere distinta come transizione Windows dalla pressione osservata di una
   scorciatoia. Nessun avviso deve accusare lo studente di aver premuto Ctrl+Alt+Canc.
6. Ripetere rapidamente le scorciatoie, chiudere/riaprire il riepilogo tramite un
   nuovo evento, poi Unlock dal docente. Nessun blocco della Console; tastiera,
   mouse e normali combinazioni devono tornare operativi. Gli eventi devono
   cessare con Unlock. Il riepilogo conserva solo 200 righe in memoria.
7. Scollegare il Wi-Fi del DELL e ricollegarlo: non promettere recupero di eventi
   persi. Dopo riconnessione, Unlock/Lock riavvia le segnalazioni del blocco nuovo.
   Verificare miniatura, Appello, InternetGuard e controllo remoto anche dopo
   diversi cicli. Un client Veyon originale non invia queste nuove segnalazioni.

## Ulteriori prove dell’interfaccia

1. In **NetLink → Aspetto, colori e icone** provare Organico, Lineare, Spettro e Classico,
   le tre dimensioni e i temi chiaro/scuro. Salva deve persistere al riavvio;
   Annulla deve annullare anche il cambio di icone/dimensioni. Nessuna nuova chiave.
2. Provare display Windows al 100%, 150% e 200%, poi spostare la Console fra
   monitor con scale diverse. Testare tastiera/focus, contrasto elevato, menu
   della toolbar e tutti i comandi anche quando la barra entra in overflow.
3. Configurator: **Facile** mostra accesso, chiavi, servizio essenziale, gruppi/PC
   e discovery; **Normale** e **Avanzata** mantengono le altre opzioni. Passare
   ripetutamente fra le viste non deve abilitare Applica né alterare valori
   nascosti. Non modificare le chiavi per la sola verifica grafica.
4. Gruppi/PC: con finestra non massimizzata i comandi devono essere sopra le
   tabelle. Verificare modifica di celle, aggiunta/rimozione righe, selezione,
   conferma del gruppo con tutti i PC, Applica e riapertura della configurazione.
5. Icona di servizio: verificare assenza/presenza di un Master autorizzato, porta
   locale, TCP in ascolto e menu **Stato del servizio**. Chiudere il pannello più
   volte: l’icona deve restare attiva. Il testo è un’istantanea, non certifica
   firewall, autenticazione di un nuovo PC, discovery o blocco Internet.
6. Dopo la nuova scalatura della GUI verificare puntamento mouse sul client,
   miniatura, Appello e apertura/chiusura ripetuta dei pannelli School. Server e
   worker non adottano la nuova scalatura della GUI, ma il test fisico è richiesto.
7. Verificare il testo **© 2026 prof. Ing. Raffaele Mele** nelle barre del titolo,
   senza righe di copyright sotto i pannelli operativi. Carattere e colore del
   titolo sono quelli standard di Windows, senza sfumature. Le attribuzioni
   nella finestra Informazioni restano disponibili e non vengono rimosse.

## InternetGuard

1. Sul client, terminale amministrativo nella cartella di installazione:
   `netlink-cli.exe school selftest`. Il test usa solo 192.0.2.254 e 2001:db8::ffff,
   senza blocco generale. Atteso `selftest-passed`, 16 filtri temporanei; riportare
   eventuali codici d’errore.
2. Verificare navigazione libera e aprire un download PRIMA del blocco.
3. Nel Master selezionare solo il client, InternetGuard → Blocca. Attesi feedback
   “filtri attivi”, miniatura aggiornata e nuove connessioni Internet negate.
   Verificare separatamente il download già aperto, DNS e risorse autorizzate.
4. Sblocca: attesi “libero” e navigazione ripristinata. Ripetere con IPv6/QUIC
   se disponibili. Il feedback non sostituisce queste prove.
5. Bloccare, chiudere il Master, attendere almeno 95 secondi: il client deve
   tornare libero. Ripetere togliendo Wi-Fi al docente e riavviando il servizio.
6. Provare riavvio client e riconnessione Master; nessun filtro deve permanere.
   Un nuovo proprietario può attendere la precedente scadenza. “Nessuna conferma”
   non equivale a successo.

## Appello

1. Richiedere nome: il dialogo deve comparire nella sessione dello studente.
2. Usare nomi fittizi: da alpha.14 la miniatura mostra
   `[nome appello] - account Windows - PC`, con sfondo distinto solo sul nome e
   senza la dicitura «Nome dichiarato dal Client». Verificare tema chiaro/scuro,
   selezione, 100/150/200%, nomi lunghi e ricerca per nome, account e PC. I nomi
   precaricati hanno sfondo più tenue e corsivo; origine/differenze restano nel
   tooltip e nel pannello dei profili, senza conferme di presenza.
   Ripetere sul client con Invio normale, Invio del tastierino e clic su OK:
   deve arrivare un'unica risposta con lo stesso nome. Provare anche dopo aver
   spostato il focus su Annulla e poi di nuovo nel campo nome. Nome vuoto/spazi
   non devono chiudere il dialogo con Invio; Esc e Annulla devono annullare.
   Questa correzione della finestra richiede l'aggiornamento anche del client.
3. Provare Annulla, richiesta ripetuta, offline, perdita Wi-Fi e riavvio. Nessuna
   risposta vecchia deve aggiornare una richiesta nuova.
4. Esportare CSV, verificare Unicode/virgolette/valori che iniziano con `=`;
   terminare l’appello e verificare cancellazione dei nomi.

## Discovery e installazione

1. Trova client: verificare nome/IPv4/porta e assenza dei nomi degli studenti.
   Sul MATEBOOK deve comparire il DELL remoto, non il MATEBOOK stesso, né il suo
   Wi-Fi né le schede virtuali. Non devono apparire righe IPv6; IPv6 di Windows
   deve restare attivo. I client remoti su altre subnet IPv4 non sono esclusi.
2. La scoperta NON deve concedere accesso senza autenticazione Veyon.
3. Client spento: il candidato deve scadere entro circa 95 secondi.
4. Una sola subnet verifica multicast/HTTP locale, NON il routing tra VLAN.
   Ripetere a scuola con Wi-Fi/cavo su subnet diverse, hostname bootstrap e
   TCP 3702 consentito dalle ACL.
5. Verificare reboot, aggiornamento e disinstallazione. Backup/chiavi conservati;
   tipo di avvio originale ripristinabile. Il servizio originale non viene
   riavviato automaticamente dalla disinstallazione.

## Autenticazione e Companion

Prima dei test aggiornare entrambi i PC con l'installer completo e riavviare
Windows, per scaricare dai processi le librerie delle alpha precedenti.

1. Aggiornare la suite completa sui due PC di prova. Avviare il Master con
   l'account docente normale; una miniatura aggiornata verifica uno scambio reale,
   mentre TCP raggiungibile o un banner RFB non provano l'autenticazione.
2. In Companion verificare esiti distinti per TCP riuscito, rifiutato, timeout,
   risposta RFB valida e porta aperta senza risposta RFB. Non rigenerare le chiavi
   in base al solo testo di una notifica con utente vuoto.
3. Confrontare nel rapporto servizio avviato, listener sulla porta del servizio,
   indirizzo di ascolto ed eccezione firewall configurata. Una regola configurata
   non garantisce che la policy effettiva consenta il traffico.
4. Scegliere il ruolo nella diagnosi locale. Sul PC ricevente, scheda Collegamento
   protetto: scegliere l'interfaccia corretta e TCP 11110 oppure una porta
   automatica, attivare Ricevi un rapporto e trasferire il codice solo all'altro
   operatore. Non eseguire questa prova su una rete non fidata.
5. Sull'altro PC incollare il codice e confermare consapevolmente l'invio del
   rapporto completo. Verificare ricezione, confronto automatico e chiusura del
   canale dopo la conferma. Nessun comando di riparazione deve essere eseguito.
6. Provare codice alterato, scaduto e riutilizzato; nessun rapporto deve essere
   consegnato. Provare Interrompi, chiusura dell'app e perdita del collegamento.
   Il canale non deve ascoltare all'avvio o dopo la scadenza.
7. Se la porta temporanea è filtrata, raccogliere l'esito senza disattivare il
   firewall. Eventuali regole autorizzate dall'amministratore devono essere
   limitate all'eseguibile Companion, alla porta e al PC/rete fidati necessari.
8. Controllare Start → NetLink: Console docente, Configurator e Companion con
   icone e destinazioni corrette, anche dopo aggiornamento. Verificare che gli
   eventuali collegamenti dell'installazione Veyon originale siano conservati.

Riportare passaggio, passato/non passato ed errore testuale, senza credenziali.

## Stabilità della console

1. Annotare l'orario di inizio della prova dopo il riavvio. In Companion aprire
   Stabilità e arresti Windows: gli eventi antecedenti non sono nuovi errori.
2. Aprire e chiudere InternetGuard, Appello e Trova client almeno 20 volte ciascuno,
   anche con Annulla e senza comandi al client. Verificare assenza di nero sull'intero
   schermo, blocchi e nuovi eventi `dwm.exe`/suite nel registro Windows.
3. Tenere aperta una finestra School e verificare che console e miniature restino
   utilizzabili. Un secondo clic deve riportare alla stessa finestra; i PC
   destinatari rimangono quelli selezionati alla prima apertura.
4. Eseguire Blocca, chiudere la finestra e riaprirla: il blocco non deve essere
   rilasciato implicitamente. Sblocca durante una richiesta in corso deve ricevere
   conferma o errore esplicito, mai un falso stato libero.
5. Con il client di prova offline e poi riconnesso, verificare timeout e assenza
   di riproduzione di vecchi comandi. Provare chiusura della console durante DNS,
   appello e ricezione di risposte. Non interrompere il servizio del laboratorio.
6. Raccogliere un nuovo rapporto Companion in caso di pause. Il controllo delle
   finestre non rispondenti è un'osservazione istantanea; gli avvisi GUI nel log
   registrano il ritardo solo dopo il recupero e possono includere sleep/resume.

Prima della stabile servono anche carico prolungato su una classe e chiusura
dei punti ancora aperti in AUDIT.md.

## Appello icona e Companion

Aggiornare entrambi i PC alla stessa alpha, chiudendo prima le applicazioni della
suite. Non rigenerare le chiavi che già consentono il controllo remoto.

1. Dal docente richiedere Appello al client, inserire nome e cognome sul client e
   confermare. Verificare nome sulla miniatura e nel suggerimento al passaggio
   del mouse. Ripetere con un nome diverso, annullare una richiesta e terminare
   l'appello: nessun nome precedente deve ricomparire da una risposta tardiva.
2. Sul docente, con icona non nascosta nel Configurator, attendere circa 30 secondi
   dopo l'avvio del servizio/accesso Windows. Controllare anche le icone nascoste
   dell'area di notifica. Se manca, raccogliere un rapporto completo Companion:
   impostazione HideTrayIcon, processi/sessioni, porta IPC locale e registri
   distinguono nascondimento da un worker non avviato; un processo worker presente
   può appartenere a un'altra funzione e non dimostra da solo l'icona visibile.
3. In Companion sul docente scegliere ruolo docente e **Controllo remoto** verso
   il client: normalmente TCP 11100. Sul client scegliere ruolo client e
   **Discovery diretto** verso il docente: TCP 3702, con Console docente aperta.
   Non confrontare questi esiti come se fossero la stessa direzione/porta.
   Esportare i rapporti completi dopo le prove: ora includono gli esiti della
   sessione. Nessun risultato TCP certifica autenticazione o multicast.
4. Sul docente aprire **Collegamento protetto → Ricevo il rapporto**, generare un
   invito, copiarlo o salvarlo su file. Sul client scegliere **Invio il mio
   rapporto**, incollare/aprire l'invito e confermare il destinatario. Verificare
   CONSEGNATO sul client e RICEVUTO/confronto sul docente. La Console può restare
   chiusa. Non disabilitare il firewall: l'eventuale autorizzazione deve riguardare
   solo Companion, la porta temporanea e i PC/rete di prova fidati.
5. Provare annullamento e scadenza dell'invito, rigenerarlo e ripetere. Un invito
   già utilizzato o scaduto non deve trasferire un secondo rapporto; un file
   salvato non prolunga la sua validità. Nessun invito sostituisce le chiavi Veyon.

## Recupero in caso di errore

### Aggiornamento ai nomi NetLink

1. Aggiornare da alpha.8 ad alpha.9 senza disinstallazione preventiva, chiudendo
   prima Console, Configurator e Companion. Riavviare entrambi i PC.
2. In Start → NetLink verificare i tre collegamenti. I processi del fork devono
   chiamarsi `netlink-service.exe`, `netlink-server.exe` e, quando necessario,
   `netlink-worker.exe`; il servizio interno resta `VeyonSchoolService`.
3. Verificare conservazione di PC/aula, chiavi, permessi e preferenze. Riprovare
   autenticazione, controllo remoto, Appello, icona, discovery e InternetGuard.
4. In Companion controllare inventario dei nuovi processi e degli eventi Windows;
   l'apertura di un vecchio rapporto deve continuare a riconoscere i nomi Veyon.
5. Provare una seconda installazione alpha.9 sulla stessa alpha.9, quindi una
   disinstallazione solo sul PC isolato: chiavi, backup e file estranei devono
   essere conservati. La prova automatica su file fittizi non copre questi passi.
6. La compatibilità delle funzioni comuni con un client Veyon originale 4.7.5
   richiede una prova distinta; non è certificata dalle prove fra due NetLink.
   Non avviare contemporaneamente i due servizi sulla stessa porta.

### Installazione incompleta o ritorno a Veyon

Non proseguire un’installazione segnalata come incompleta. Conservare il messaggio
e il backup protetto in `C:\Program Files\Veyon School\Backups`. Dopo la copia dei
file è disponibile `Uninstall.exe` anche se la configurazione successiva fallisce.
Il rollback completo di un aggiornamento non è automatico né ancora collaudato.

Per tornare a Veyon originale: disinstallare il fork, importare il backup della
configurazione nel Configurator originale avviato come amministratore e avviare
il servizio originale. La configurazione è condivisa e le due impostazioni dei
driver/acquisizione vengono disabilitate dal fork; rimuovere il programma non
ripristina automaticamente quelle impostazioni. Le chiavi restano al loro posto.
