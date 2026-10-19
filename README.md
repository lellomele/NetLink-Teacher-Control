# NetLink

Fork di **Veyon 4.7.5** per aule Windows: visualizzazione e controllo dei PC,
InternetGuard basato su Windows Filtering Platform, Appello con il nome dichiarato
dallo studente sulle miniature e ricerca dei client tramite WS-Discovery 1.1.
Le funzioni sono integrate: non occorre installare un plugin esterno.

**Versione preliminare 1.0.0-alpha.19: destinata a PC di prova, non al laboratorio in uso.**

## Installazione

Scaricare l’installer x64 dalla [pagina delle release](https://github.com/lellomele/veyon-school/releases).
Installarlo come amministratore sul PC docente e sui client. Windows 10 a 64 bit è
il sistema di riferimento; le librerie necessarie sono incluse nell’installer.
Non servono Python, .NET aggiuntivo, DDS, Zenoh o un nuovo driver kernel.

Per aggiornare, chiudere Console docente, Configurator e Companion e usare
l'installer completo su entrambi i PC, conservando le chiavi esistenti. Dopo
l'aggiornamento dalle alpha precedenti riavviare Windows prima del collaudo:
le vecchie librerie di aggancio possono essere ancora caricate in altri processi.
Non sostituire o cancellare singole DLL.

Prima della copia l'installer controlla le rimozioni in attesa di riavvio e i file
da sostituire, dopo aver fermato il servizio senza ancora rimuoverne la registrazione.
Se un file è occupato, si ferma con **Riavvio necessario**: chiudere l'installer,
scegliere **Riavvia** in Windows e rieseguire il pacchetto completo. Nessun riavvio
viene forzato. Configurazione e chiavi restano conservate; non è disponibile
**Ignora** per saltare una DLL. Errori di permessi o file in sola lettura sono
distinti dai file occupati e richiedono di verificare il messaggio diagnostico.

Gli eseguibili Windows si chiamano `netlink-master.exe`, `netlink-configurator.exe`,
`netlink-service.exe`, `netlink-server.exe`, `netlink-worker.exe`, `netlink-cli.exe`,
`netlink-wcli.exe` e `netlink-companion.exe`. L'installer aggiorna anche le alpha con
i vecchi nomi `veyon-*.exe`: non rinominarli manualmente. I sette vecchi eseguibili
del fork vengono rimossi dopo l'avvio del nuovo servizio, eventualmente al riavvio.
Le regole firewall gestite dalla suite sono aggiornate; regole personalizzate,
script o criteri scolastici basati sui vecchi percorsi vanno adeguati separatamente.
Restano invariati DLL, cartella `C:\Program Files\Veyon School`, configurazione,
directory delle chiavi e identificativo interno `VeyonSchoolService`.

Il programma usa la configurazione Veyon esistente e installa il servizio separato
`VeyonSchoolService`. Non eseguire contemporaneamente i due servizi: usano le stesse
porte. L’installazione originale, le chiavi e la configurazione non vengono eliminate.
L’installer salva la configurazione del registro e i vecchi backup in
`C:\ProgramData\NetLink\Recovery`, accessibile solo ad amministratori e SYSTEM.
Ripristina anche un'installazione incompleta senza avviare i vecchi eseguibili:
verifica tutti i file prima di avviare gli strumenti di configurazione.
Se il servizio originale è presente, ne disabilita l’avvio automatico
con conferma e ripristina il tipo di avvio alla disinstallazione.
La configurazione è condivisa: il fork disabilita Interception e DeskDupEngine.
La disinstallazione non reimporta automaticamente il backup della configurazione;
per tornare alle impostazioni precedenti usare il Configurator originale.

### Disinstallazione

Chiudere le applicazioni e avviare la disinstallazione da Windows. Si può scegliere
se conservare le impostazioni per reinstallare oppure rimuoverle. In entrambi i casi
vengono rimossi i file e la cartella `C:\Program Files\Veyon School`, comprese le
vecchie DLL e i file aggiunti manualmente: non usarla per documenti personali.
I file in uso e la cartella vengono eliminati al riavvio; reinstallare solo dopo
aver riavviato se richiesto. Le chiavi di autenticazione esterne alla cartella del
programma non vengono lette né eliminate; restano anche i loro percorsi configurati.
Se le chiavi sono state collocate nella cartella del programma, la disinstallazione
si ferma: trasferirle prima in una posizione protetta tramite Configurator.

La rimozione delle impostazioni riguarda le voci gestite dalla suite (registro
32/64 bit e preferenze dei profili caricati). Non cancella configurazioni condivise
con un Veyon originale rilevato, profili Windows non caricati, cronologia/eventi
gestiti da Windows, criteri di sistema condivisi (come SoftwareSASGeneration)
o file personali fuori dalla cartella del programma.
Le copie di recupero protette restano disponibili in ProgramData; i file `.reg`
contengono impostazioni, non una copia delle chiavi private. Per una reinstallazione
da zero conservare anche un'esportazione JSON dal Configurator e reimportarla dopo.

Nel Configurator mantenere le aule, i PC, l’autenticazione e le regole di accesso
già configurati. È consigliata l’autenticazione a chiave con diritti limitati ai
docenti. Non distribuire la chiave privata ai client.

## Uso

- **Controllo remoto:** barra compatta, senza oscurare il desktop. Inizia sul solo
  bordo superiore: fermare lì il puntatore oppure passarlo sulla puntina per
  richiamare i comandi. La puntina resta visibile anche a barra chiusa e si attenua
  dopo circa 1,8 secondi di inattività; torna nitida al passaggio del mouse.
  Il clic sinistro sulla puntina fissa/sblocca la barra aperta. Il clic destro su
  puntina o barra sposta entrambe: **alto → sinistra → basso → destra → alto**.
  Solo il bordo scelto richiama la barra; la scelta vale per la sessione corrente.
  I menu aperti e l'uso dei controlli da tastiera impediscono la chiusura accidentale.
  In modalità Windows a contrasto elevato la puntina non si attenua.
  **Disconnetti** termina solo la sessione remota, non spegne il PC.
- **Aspetto:** temi Sistema, Chiaro e Scuro; palette Oceano, Bosco, Prugna o colore
  personalizzato; set coordinati di icone Organico, Lineare, Spettro (colorato) e Classico, con tre
  dimensioni dei pulsanti. Il menu **NetLink → Aspetto, colori e icone** salva
  preferenze personali, senza elevazione o riavvio del servizio.
  Eventuali colori delle miniature già personalizzati nel Configurator restano prioritari.
- **Configurator:** viste Facile, Normale e Avanzata. Facile mostra autenticazione,
  chiavi, servizio essenziale, gruppi/PC e discovery senza alterare le opzioni
  nascoste. I comandi per aggiungere e rimuovere gruppi o PC sono sopra le tabelle.
- **Lingua:** scegliere la lingua nel Configurator, applicare e riavviare le
  applicazioni. Console, Configurator e pannelli NetLink condividono la selezione;
  le nuove voci sono disponibili in italiano e inglese. Dialoghi nativi Windows
  e alcune descrizioni diagnostiche del Companion possono seguire altre regole.
- **Blocco schermo:** copertura software con filtro delle scorciatoie ordinarie,
  attivo solo durante il blocco. Il pannello non bloccante **Eventi durante il
  blocco** indica PC, orario di ricezione e conteggio degli eventi osservabili.
  Richiede questa versione su entrambi i PC; un client Veyon originale non
  produce gli eventi aggiuntivi. Nessun testo digitato viene raccolto.
- **Stato del servizio:** l’icona nell’area di notifica mostra un riepilogo locale;
  il suo menu apre i dettagli. “In ascolto” non prova la raggiungibilità da altri PC.
- **InternetGuard:** selezionare i PC, aprire InternetGuard e scegliere Blocca,
  Sblocca o Verifica stato. La console mostra conferme, errori e richieste senza risposta.
  Il blocco è rinnovato ogni 20 secondi e scade dopo 90 secondi senza rinnovo.
  Le finestre InternetGuard, Appello e Trova client lasciano utilizzabile la
  console e conservano i PC selezionati all'apertura. Chiudere la finestra
  InternetGuard non sblocca i client: usare esplicitamente **Sblocca**.
- **Appello e profili classe–aula:** in **Posizioni e computer** usare **Profili
  classe–aula…** per assegnare uno studente a ogni postazione, indicando classe e
  aula. I nomi appaiono nell’elenco e sulle miniature, ricercabili e ordinabili.
  L’editor parte dai PC selezionati nelle miniature, oppure da tutti quelli
  visualizzati se non c’è una selezione; si possono aggiungere altre postazioni.
  **Salva JSON** conserva le assegnazioni; **Carica JSON** le ricarica in una nuova
  lezione. Il file è scelto dal docente (inizialmente in Documenti, oppure in
  `%APPDATA%\NetLink\Classi` se la cartella esiste già). Non è il JSON del Configurator.
  Modificare la colonna dello studente per cambiare assegnazione; selezionare righe
  e usare **Usa nomi dichiarati** per copiare le risposte dell’Appello nel profilo.
  Le risposte non sovrascrivono automaticamente il file: salvarlo esplicitamente.
  **Esporta CSV** produce una copia leggibile con postazioni, assegnazioni e
  dichiarazioni, non ricaricabile come profilo.
  Per rileggere un nome, selezionare solo i PC interessati nella Console e usare
  **Appello → Richiedi nomi**. Nessuna conferma di presenza è richiesta o prevista.
  Sul client confermare il nome con **OK** oppure **Invio**. La miniatura mostra
  `[nome Appello] - account Windows - PC`, con sfondo distinto solo sul nome;
  l'origine resta nel tooltip. Il nome precaricato ha sfondo tenue e corsivo.
  I nomi **precaricati** sono distinti da quelli **dichiarati dal client**; un nome
  diverso dall’assegnazione è segnalato. **Termina richieste e cancella nomi
  dichiarati** mantiene il profilo; **Nuovo / scarica profilo** elimina le etichette
  attive e consente di costruire un altro profilo, senza cancellare i file salvati.
- **Trova client:** mostra soltanto PC remoti con indirizzi IPv4, escludendo il
  computer docente e tutte le sue schede, anche virtuali. IPv6 di Windows resta
  attivo. **Nascondi i computer già presenti in Posizioni e computer**, attivo
  all'apertura, esclude gli IP già configurati in tutte le aule e gli IPv4
  risolti dai nomi host configurati. Disattivarlo per vedere anche i PC già noti.
  Il conteggio distingue risultati visibili, già configurati e nomi non risolti;
  questi ultimi non fanno nascondere un PC. Il solo nome dichiarato dal client
  non determina una corrispondenza. Con DHCP usare nomi host risolvibili:
  un vecchio IP salvato non identifica stabilmente lo stesso computer.
  Un PC con IPv4 e IPv6 viene elencato tramite IPv4; un PC raggiungibile soltanto
  via IPv6 non entra nei risultati. La tabella del Configurator accetta IPv6
  inseriti manualmente, ma l'intera suite non è collaudata su una rete solo IPv6.
  Verificare i PC rilevati prima di importare quello selezionato.
  L’importazione richiede diritti di scrittura sulla configurazione e il riavvio
  del Master. Non modifica automaticamente le chiavi o le autorizzazioni.

## Configurazione aggiuntiva

### NetLink Companion

Il terzo programma della suite si apre da **Start → NetLink → Companion**.
La stessa cartella contiene Console docente e Configurator.
Raccoglie localmente le informazioni utili per configurare la comunicazione:
Windows, account e gruppi del processo, schede/IP/prefissi, DNS/DHCP/gateway,
servizi, porte TCP, profili firewall, regole discovery del fork, impostazioni
NetLink e PC già inseriti nel Configurator. Delle chiavi private legge soltanto
nomi, percorsi e permessi, mai il contenuto; calcola l'impronta dei soli file pubblici.

La sezione **Stabilità e arresti Windows** cerca, con limiti di tempo e quantità,
gli arresti del desktop e delle applicazioni della suite nelle ultime 24 ore e
le finestre che Windows segnala come non rispondenti. Mostra orari UTC e modulo
coinvolto, senza raccogliere dump di memoria, titoli di finestre o eventi di altre
applicazioni. I vecchi eventi restano visibili dopo l'aggiornamento: confrontarne
gli orari con le nuove prove. L'assenza di eventi non certifica la stabilità.

Eseguirlo prima con lo stesso account e livello di privilegi del Master. Il pulsante
**Apri come amministratore** è facoltativo: i suoi risultati non provano i permessi
dell'utente docente non elevato. Companion non cambia rete, servizi, firewall,
chiavi o autorizzazioni; non scansiona automaticamente la rete.

Selezionare il ruolo docente/client. Per confrontare due PC, esportare su ciascuno
un rapporto JSON completo selezionando **Includi identificativi…**; aprire il rapporto
dell'altro PC con **Confronta con altro PC**. Senza questa opzione il rapporto è ridotto
e omette identificativi, indirizzi e chiavi. Il rapporto completo contiene dati
personali e della rete: conservarlo e condividerlo solo quando necessario.

Le **Prove di comunicazione** distinguono tre casi, senza scansioni della rete:

- **Controllo remoto:** dal docente al client, abitualmente TCP 11100. Sul client
  deve essere attivo il servizio NetLink. È possibile verificare anche la risposta
  iniziale RFB. Il collegamento inverso client → docente non è necessario per
  controllare il client.
- **Discovery diretto:** dal client al docente su TCP 3702, con la Console docente
  aperta. Verifica solo l'apertura della porta degli annunci diretti, non il
  multicast WS-Discovery su UDP 3702 o lo scambio di un annuncio applicativo.
- **TCP personalizzato:** nome/IP e porta scelti esplicitamente per una diagnosi
  avanzata, anche su 127.0.0.1 per confrontare accesso locale e remoto.

Ogni prova dura al massimo 8 secondi e mostra l'indirizzo effettivo quando connessa.
Il rapporto completo conserva fino a 20 esiti della sessione, con orari, direzione,
destinatario e porta; quello ridotto li omette. Gli esiti hanno testo e sfondo
distinti. Nessuna di queste prove verifica autenticazione o navigazione. Sul servizio
NetLink/Veyon una prova può provocare una notifica di autenticazione incompleta.
Le impronte uguali attestano file pubblici uguali, non la validità della coppia
con la chiave privata. I rapporti non sono firmati e non vengono importati come
configurazione fidata. Routing, ACL, isolamento Wi-Fi e autenticazione vanno
confermati con una connessione reale dal Master.

La scheda **Collegamento protetto** trasferisce un rapporto fra due Companion,
anche quando il servizio NetLink non è raggiungibile. La Console docente può
restare chiusa; i due programmi vanno aggiornati alla stessa alpha.

1. Sul **docente**, eseguire **Analizza PC** e scegliere il ruolo docente. Nella
   scheda Collegamento protetto scegliere **Ricevo il rapporto**, l'interfaccia
   della rete utilizzata e la porta separata (TCP 11110, oppure 0 per automatica).
   Premere **Genera invito e attendi**.
2. Trasferire l'invito riservato al client con **Copia invito** oppure **Salva
   invito su file**. Su IPv4 l'invito compatto contiene 104 caratteri: non è una
   chiave di autenticazione Veyon e non deve essere importato nel Configurator.
3. Sul **client**, eseguire **Analizza PC**, scegliere ruolo client e **Invio il
   mio rapporto**. Incollare l'invito oppure aprire il file `.netlink-invite`.
   Premere **Verifica destinatario e invia il mio rapporto** e confermare i dati
   personali da condividere.
4. Attendere **CONSEGNATO** sul client, poi tornare sul docente: **RICEVUTO** indica
   che il confronto automatico è disponibile sotto il pannello. Lo scambio è
   monodirezionale; per inviare anche nell'altra direzione occorre un nuovo invito
   a ruoli invertiti. **Annulla collegamento** chiude la ricezione temporanea.

Il canale usa TLS 1.2 o successivo, certificato temporaneo verificato mediante
l'impronta nel codice e un segreto casuale monouso. Il formato compatto conserva
l'impronta e il segreto completi a 256 bit; non usa un PIN breve facilmente
indovinabile. Accetta anche i precedenti inviti NL1, ma le alpha vecchie non
leggono i nuovi inviti NL2. Il conto alla rovescia indica la scadenza entro 5 minuti;
la ricezione termina dopo un rapporto. File e codice sono riservati; un file
salvato non prolunga la validità. Non
vengono aperte porte all'avvio né modificate regole firewall: se il traffico è
filtrato serve un'autorizzazione locale mirata sul ricevente. Non è un canale di
amministrazione remota, non esegue riparazioni e non usa le chiavi Veyon. I rapporti
rimangono dichiarazioni dell'altro programma, non attestazioni del sistema.

### Configurator

Pagina **NetLink** del Configurator:

- Stessa subnet: discovery multicast su UDP 3702, senza indirizzi da inserire.
- Subnet diverse: indicare sui client il nome/IP del docente in `DiscoveryMaster`.
  Il Master riceve gli annunci via HTTP su TCP 3702. Il routing e le ACL della rete
  devono consentire il collegamento. In alternativa l’installer accetta `/Master=NOME-PC`.
- Risorse scolastiche extra: aggiungere solo gli IP o CIDR necessari, uno per riga.
  I PC selezionati nel Master e il suo indirizzo effettivo vengono riutilizzati;
  DNS e DHCP delle interfacce attive vengono rilevati dal client.

Le eccezioni discovery sono limitate ai profili Windows **Privato/Dominio**.
Il multicast non attraversa automaticamente le subnet. Il nome del Master può
essere distribuito insieme alla normale configurazione Veyon.

## Limiti operativi

Il feedback InternetGuard conferma l’installazione dei filtri WFP, **non** una prova
di navigazione. Serve ancora il collaudo su Windows 10 con firewall/antivirus reali,
connessioni già aperte, IPv6, VPN e cambi di interfaccia. La scadenza è volutamente
fail-open: questo non è un firewall aziendale o un controllo di sicurezza assoluto.
Se Windows non conferma la chiusura della sessione WFP, NetLink conserva lo stato
di recupero, segnala **sconosciuto** e ritenta: non dichiara falsamente lo sblocco.
Un blocco delle chiamate native BFE/RPC può ritardare anche la scadenza prevista.

Le destinazioni autorizzate restano raggiungibili su tutte le porte; proxy o VPN su
quelle destinazioni possono permettere l’accesso esterno. Non autorizzare intere
subnet senza motivo. Gli studenti non devono avere diritti amministrativi locali.
Un altro docente non può sostituire una policy attiva di una diversa connessione:
in caso di riconnessione può essere necessario aspettare la scadenza di 90 secondi.

Appello non verifica identità o presenze. I profili sono solo del Master e non
richiedono aggiornamenti dei client già compatibili con Appello. Il JSON salva
nomi assegnati e identificativi delle postazioni, non dichiarazioni o presenze.
Le dichiarazioni restano in memoria e vengono eliminate alla disconnessione o al
cambio profilo; i nomi precaricati restano fino allo scaricamento del profilo o
alla chiusura del Master. Il caricamento usa l’identificativo interno del PC, non
l’IP: non ripristina vecchi indirizzi né aggiorna automaticamente un IP cambiato
nel Configurator. Usare hostname risolvibili o aggiornare gli indirizzi nella
configurazione. PC cancellati e ricreati devono essere riassegnati esplicitamente;
i PC non visualizzati/configurati sono segnalati nel pannello dei profili.
JSON e CSV contengono dati personali in chiaro: conservarli in cartelle protette,
non nella cartella del programma, e non pubblicarli su GitHub. Non vengono inclusi
nel backup JSON del Configurator né eliminati dalla disinstallazione. Gli annunci
discovery non trasmettono i nomi degli studenti e non sono autenticati.

La comunicazione Master/client eredita il modello di sicurezza Veyon 4.7.5: il TLS
del solo Companion non aggiunge un canale TLS generalizzato alla suite.
Non esporre i servizi a Internet o a reti non fidate.
Limiti: 256 client per operazione, 255 destinazioni totali, 128 risorse aggiuntive,
1.024 candidati discovery; le risposte SOAP sono limitate a 32 KiB e 64 record.

Non sono inclusi Interception né la DLL binaria DeskDupEngine. Si usa l’acquisizione
schermo software: verificare fluidità e carico CPU sui PC di destinazione. Il blocco
software non è una modalità chiosco inviolabile: le schermate protette di Windows
restano accessibili. Un evento di schermata protetta non identifica con certezza
Ctrl+Alt+Canc: può dipendere anche da UAC o dal sistema. I cambi molto brevi possono
non essere rilevati. Gli avvisi sono raggruppati circa ogni due secondi, inviati
solo al Master che ha richiesto più recentemente il blocco e conservati in un
riepilogo di massimo 200 righe in memoria. Non sono un registro forense e non
recuperano i periodi disconnessi. Dopo una riconnessione, ripetere Unlock/Lock.
Le nuove funzioni richiedono il fork NetLink (già Veyon School) sui client.

I ritardi dell'interfaccia superiori a due secondi vengono segnalati nel normale
registro diagnostico dopo il recupero, con avvisi limitati nel tempo. Anche una
sospensione di Windows può produrre un ritardo: non è una prova di deadlock.
Non vengono forzati riavvii, chiusure o riparazioni. Le operazioni JSON/CSV
dell’Appello avvengono fuori dal thread dell’interfaccia: su cartelle di rete non
disponibili Windows può ritardare l’esito. Chiudere il pannello durante una scrittura
non annulla necessariamente il salvataggio già iniziato.

## Compilazione

Clonare con `git clone --recurse-submodules https://github.com/lellomele/veyon-school.git`.
Su Windows usare MSYS2 MinGW64 con GCC, CMake, Ninja, Qt 5, QCA, OpenSSL, OpenLDAP,
LZO, libjpeg-turbo, libpng, zlib e NSIS. Le istruzioni complete sono in
[BUILD.md](docs/BUILD.md). I sorgenti dell’icona sono in `branding/`.

## Copyright e licenza

NetLink: Copyright © 2025-26 prof. Ing. Raffaele Mele, per le modifiche del fork.
Veyon © Tobias Junghans, Veyon Solutions e contributori. Non è una release
ufficiale del progetto Veyon. Copyright e attribuzioni originali sono conservati.
Il codice Veyon è GPL-2.0-or-later; le licenze delle librerie sono conservate.
Vedere [COPYING](COPYING) e [DEPENDENCIES.md](docs/DEPENDENCIES.md).
