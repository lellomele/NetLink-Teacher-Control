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

## InternetGuard

1. Sul client, terminale amministrativo nella cartella di installazione:
   `veyon-cli.exe school selftest`. Il test usa solo 192.0.2.254 e 2001:db8::ffff,
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
2. Usare nomi fittizi: miniatura con nome + PC, account Windows distinto nel
   tooltip, ricerca e ordinamento funzionanti.
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

Non proseguire un’installazione segnalata come incompleta. Conservare il messaggio
e il backup protetto in `C:\Program Files\Veyon School\Backups`. Dopo la copia dei
file è disponibile `Uninstall.exe` anche se la configurazione successiva fallisce.
Il rollback completo di un aggiornamento non è automatico né ancora collaudato.

Per tornare a Veyon originale: disinstallare il fork, importare il backup della
configurazione nel Configurator originale avviato come amministratore e avviare
il servizio originale. La configurazione è condivisa e le due impostazioni dei
driver/acquisizione vengono disabilitate dal fork; rimuovere il programma non
ripristina automaticamente quelle impostazioni. Le chiavi restano al loro posto.
