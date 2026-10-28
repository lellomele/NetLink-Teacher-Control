# Collaudo funzionale e di resilienza

[English](PILOT.md) · [Italiano](PILOT.it.md)

Usare postazioni dedicate con accesso fisico: un **PC docente** e uno o più
**client studenti**, senza riferimenti a dispositivi o reti personali.
Annotare versione, sistema operativo, antivirus/firewall e risultato di ogni prova.
Non condividere chiavi private, password o rapporti completi non anonimizzati.

## Preparazione e installazione

1. Esportare il JSON del Configurator e conservare separatamente le chiavi.
   Verificare checksum dell'installer e installare il pacchetto completo come
   amministratore su docente/client. Se richiesto, riavviare e rieseguirlo.
2. Mantenere le chiavi esistenti: privata soltanto sul docente, pubblica sui
   client, permessi limitati. Riutilizzare aule, PC e regole di accesso.
3. Controllare configurazione, servizio, icona visibile/nascosta e Start → NetLink.
   Non avviare insieme servizio originale e fork sulle stesse porte.
4. Dal Master verificare miniatura aggiornata e controllo remoto autenticato.
   Un TCP positivo non basta. Annotare carico CPU/memoria e orario iniziale.
5. Usare gli indirizzi effettivi delle postazioni o hostname risolvibili:
   DHCP può cambiare gli IP. Non inserire nella configurazione esempi documentali.
6. Installare **Master** sul docente e **Client** sugli studenti. Verificare sul Client
   Configurator/Companion/servizio, assenza di EXE/collegamento docente e controllo
   remoto funzionante. Provare aggiornamenti con ruolo conservato e cambio in entrambe
   le direzioni, senza variazioni di configurazione/chiavi. Ripetere rimozione e
   reinstallazione per ciascun ruolo.

## Console, aspetto e controllo remoto

1. Provare italiano/inglese impostati dal Configurator, temi chiaro/scuro,
   set Organico/Lineare/Spettro/Classico e dimensioni dei pulsanti.
   Salva deve persistere; Annulla deve ripristinare la scelta precedente.
2. Provare scala 100/150/200%, finestre strette e monitor con DPI diversi.
   Tutti i comandi devono restare accessibili anche in overflow; verificare
   focus/tastiera, contrasto elevato, puntamento remoto e copyright nel titolo.
3. Nel controllo remoto verificare attivazione iniziale dal solo bordo superiore.
   Clic sinistro sulla puntina fissa/libera la barra; il clic destro ruota
   alto → sinistra → basso → destra, una volta e senza comandi inoltrati al client.
4. La puntina non deve scomparire: si attenua quando inattiva, torna nitida
   con mouse/focus e non si attenua in contrasto elevato.
   Provare menu, fullscreen, ridimensionamento e disconnessione.
5. Aprire/chiudere InternetGuard, Appello e Trova client almeno 20 volte anche
   senza inviare comandi. Console e miniature devono restare utilizzabili,
   senza nero a tutto schermo, crash o nuovi errori del desktop di Windows.
6. Un secondo clic deve riaprire la stessa finestra; i destinatari devono
   restare quelli selezionati alla prima apertura.

## Configurator e icona del servizio

1. Passare fra Facile, Normale e Avanzata senza cambiare i valori nascosti.
   Aggiungere/modificare/rimuovere gruppi e PC senza massimizzare la finestra.
2. Modificare discovery, indirizzo Master e risorse: Applica deve abilitarsi,
   validare e conservare i valori dopo chiusura/riapertura.
   Input errati non devono produrre salvataggi parziali.
3. Controllare anche le icone nascoste e l'impostazione HideTrayIcon.
   Il riepilogo e Stato del servizio devono essere aggiornati, chiudibili
   senza terminare il worker e non presentare “in ascolto” come prova di rete.
4. Su un client isolato con avvio automatico abilitato, aggiornare sul posto e
   riavviare Windows due volte: NetLink Service deve essere Automatico e In
   esecuzione senza aprire Configurator. Verificare il ritorno della miniatura.
   Eseguire `netlink-cli.exe service verify` da terminale amministrativo e
   conservarne il risultato.
5. Solo su un PC di prova sacrificabile, verificare il ripristino nativo limitato
   dopo un errore imprevisto del processo, e che un arresto amministrativo esplicito
   rimanga tale. Ripristinare poi il normale funzionamento. Una scelta manuale
   intenzionale nel Configurator deve superare l'upgrade senza essere presentata
   come avvio automatico. In caso di avvio fallito annotare gli eventi Sistema /
   Service Control Manager: i test simulati del controllo servizi non dimostrano
   l'affidabilità all'avvio sull'hardware fisico dei client.

## InternetGuard

1. Sul client di prova, da terminale amministrativo nella cartella del programma,
   eseguire `netlink-cli.exe school selftest`. Annotare risultato e codici
   d'errore. Usa destinazioni riservate ai test, non un blocco Internet generale.
2. Verificare navigazione libera e aprire un download prima del blocco.
   Selezionare soltanto i client interessati e usare Blocca Internet.
3. Controllare feedback, miniatura, nuove connessioni Internet, download già
   avviato, DNS e risorse autorizzate. Verificare il traffico reale, non solo
   lo stato dei filtri. Chiudere/riaprire il pannello non deve sbloccare.
4. Usare Sblocca: attendere conferma e verificare il ritorno della navigazione.
   Ripetere con IPv6/QUIC, VPN e policy di sicurezza effettivamente previste.
5. Bloccare, chiudere il Master e attendere almeno 95 secondi: verificare
   il recupero. Ripetere con perdita del Wi-Fi docente, riavvio del servizio
   e riavvio client. Non devono restare filtri indesiderati.
6. Provare riconnessione e sblocco durante una richiesta in corso.
   Nessuna conferma non significa successo; una nuova connessione può
   dover attendere la scadenza del proprietario precedente.

## Appello e profili classe–aula

1. Richiedere il nome sul client selezionato. Provare OK, Invio normale e
   tastierino, anche dopo focus su Annulla. Deve arrivare una sola risposta.
   Nome vuoto/spazi non deve chiudere con Invio; Esc/Annulla devono annullare.
2. Verificare `[nome Appello] - account Windows - PC`, sfondo del nome,
   tooltip, ricerca e nomi lunghi nei temi/scale supportati.
3. Creare un profilo con classe, aula e assegnazioni; salvare JSON e ricaricarlo
   dopo il riavvio del Master. Non confonderlo con il backup del Configurator.
4. Richiedere un nome diverso: distinguere precaricato e dichiarato senza
   conferme di presenza né modifica automatica del file.
   Usare nomi dichiarati deve copiare solo le assegnazioni selezionate.
5. Cambiare classe con una risposta ancora pendente; la vecchia risposta
   non deve alterare il profilo nuovo. Provare disconnessione/riavvio client:
   cancellare la dichiarazione, non l'assegnazione precaricata.
6. Provare JSON errato/incompleto, UUID duplicati, file troppo grande e backup
   del Configurator: rifiuto senza perdita del profilo attivo.
   Nuovo/scarica profilo non deve cancellare file sul disco.
7. Cambiare l'IP mantenendo lo stesso UUID: l'assegnazione resta, ma il
   Configurator non ripristina o corregge automaticamente indirizzi.
   PC cancellati/ricreati devono richiedere riassegnazione.
8. Esportare CSV con Unicode, virgolette e valori che iniziano con `=`.
   Distinguere assegnazioni e dichiarazioni; verificare reattività durante
   salvataggio/caricamento e chiusura del pannello.
9. Spuntare due gruppi, evidenziare un PC di un solo gruppo, lasciare un altro PC
   di quel gruppo senza spunta e nasconderlo con la ricerca: JSON/CSV devono
   includere entrambi i PC dell'aula, non l'altro gruppo. Evidenziare la radice
   deve dare lo stesso risultato. Nessuna riga evidenziata o aula vuota devono
   produrre un avviso senza scrivere file.
10. Caricare con altri gruppi presenti: verificare che siano spuntati solo gli UUID
    salvati e disponibili, senza sostituire righe, gruppi, appartenenze, indirizzi o chiavi.
    Provare Annulla, PC mancanti/ricreati, vecchi JSON, Lock/Demo/InternetGuard pendenti
    e cambiamenti nell'elenco o nelle funzioni mentre è aperta la conferma.

## Discovery e filtro dei PC noti

1. In Automatico, un client dual-stack deve produrre una sola riga,
   preferibilmente IPv4; il docente non deve comparire su alcuna scheda.
   Il tooltip deve mostrare gli indirizzi osservati.
2. Con il client già configurato per IP o hostname risolvibile, attivare il
   filtro: deve sparire dai nuovi risultati. Disattivarlo: deve essere
   Già configurato e un'importazione non deve duplicarlo.
3. Provare un client configurato solo mediante un alias IPv6 della stessa
   identità e verificare il filtro anche se la riga preferisce IPv4.
   DNS non risolto deve lasciare visibile il risultato e non bloccare la GUI.
4. Passare Automatico/Solo IPv4, chiudere normalmente la Console e riaprirla:
   la preferenza deve restare, senza modificare IPv6 in Windows.
5. In una rete di prova appropriata, un client che annuncia solo IPv6 deve
   apparire in Automatico e non in Solo IPv4. Non disabilitare IPv4 su un
   PC controllato da remoto senza accesso fisico.
6. Verificare gli IPv6 link-local con indice della scheda ricevente.
   Non usarli come destinazioni InternetGuard; per quella prova usare
   IP instradabili o hostname risolvibili.
7. Provare un nuovo client, importazione esplicita e rimozione dai risultati
   nuovi dopo il salvataggio. Il riavvio del Master deve mostrare una sola voce.
   La scoperta non deve concedere accesso senza autenticazione.
8. Spegnere il client: il candidato deve scadere entro circa 95 secondi.
   Chiudere la finestra durante DNS/annunci e riaprirla senza crash.
9. Verificare separatamente subnet/VLAN diverse: configurare DiscoveryMaster
   e consentire TCP 3702 secondo le ACL. Una prova nella stessa subnet
   non verifica routing o isolamento Wi-Fi. Il multicast non attraversa
   autonomamente le VLAN; quello IPv6 è locale al collegamento.
10. Con due gruppi, evidenziare il secondo gruppo o un suo PC e importare:
    verificare la destinazione mostrata e il gruppo padre. Nessun gruppo evidenziato,
    gruppo rimosso/rinominato o selezione cambiata durante la conferma devono
    fermare l'importazione, senza scegliere o creare un altro gruppo. Annulla,
    errore di salvataggio e duplicati non devono aggiungere PC né riscrivere altre impostazioni.
11. Annunciare la stessa identità con nomi e/o porte differenti: riga e dettagli
    devono indicare la discordanza precisa e gli indirizzi osservati.
    Importazione bloccata; annunci dual-stack coerenti non sono conflitti.
    Verificare inglese/italiano e 100%, 150%, 200%, anche con nomi simili a markup.

## Blocco input ed eventi

1. Attivare Lock. Provare Alt+Tab, Alt+F4, Ctrl+Esc, Ctrl+Maiusc+Esc e tasto
   Windows; verificare avvisi limitati e nessuna raccolta di testo.
2. Provare Win+L, Ctrl+Alt+Canc e UAC: attendere una segnalazione di sessione
   o schermata protetta, non l'affermazione che una combinazione sia stata
   identificata con certezza. Al ritorno deve ricomparire la copertura.
3. Ripetere gli eventi e usare Unlock: tastiera e mouse devono tornare operativi,
   senza blocchi/flicker. Gli eventi devono cessare; riepilogo massimo 200 righe.
4. Dopo perdita della connessione non attendersi recupero degli eventi persi.
   Riconnettere, Unlock/Lock e riprovare. Client Veyon originali non producono
   i messaggi aggiuntivi: l'interoperabilità richiede un test separato.

## Companion

1. Selezionare ruolo e controllare esiti distinti per TCP riuscito, rifiutato,
   timeout, RFB valido e porta aperta senza RFB.
   Docente → client usa normalmente TCP 11100; client → docente, per discovery
   diretto con Console aperta, TCP 3702. Non confondere le direzioni.
2. Confrontare listener, profilo della scheda effettiva e regole configurate.
   Non dedurre l'autenticazione da una notifica con utente vuoto o dal solo TCP.
3. Il ricevente genera l'invito temporaneo; il mittente lo apre/incolla,
   verifica il destinatario e conferma l'invio dei dati. Attendere CONSEGNATO
   e RICEVUTO/confronto. Nessuna riparazione deve essere eseguita.
4. Provare invito alterato, scaduto, riutilizzato, annullamento, perdita rete
   e chiusura dell'app. Il canale non deve ascoltare all'avvio o dopo la scadenza.
5. Non disattivare il firewall per il collaudo: eventuali autorizzazioni
   amministrative devono essere limitate a Companion/porta/rete necessari.
   Condividere soltanto rapporti appropriati e anonimizzati.

## Disinstallazione e recupero

Su un sistema dedicato, verificare aggiornamento su installazione esistente,
file occupati/riavvio, installazione pulita e recupero da errore.
Seguire [INSTALLATION.md](INSTALLATION.it.md), senza prove distruttive sui PC in uso.

Controllare entrambe le scelte di conservazione/pulizia delle impostazioni,
rimozione di cartella, servizio, collegamenti e regole del fork,
conservazione delle chiavi esterne e loro permessi. Reinstallare e verificare
ripristino dal JSON e connessione reale. Non cancellare manualmente voci
di registro trovate con ricerche generiche.

## Carico e risultati

Eseguire sessioni prolungate con più client, PC offline e riconnessioni.
Registrare latenza, CPU/memoria e passaggi riproducibili; raccogliere rapporti
Companion privi di dati non necessari. I log GUI segnalano ritardi dopo
il recupero e possono includere sleep/resume.

Non dichiarare completa la verifica di classi numerose, reti IPv6-only,
subnet diverse o interoperabilità sulla base del solo test con due PC.
Criteri di avanzamento e limiti: [AUDIT.md](AUDIT.it.md).
