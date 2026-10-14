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

1. Trova client: verificare nome/IP/porta e assenza dei nomi degli studenti.
2. La scoperta NON deve concedere accesso senza autenticazione Veyon.
3. Client spento: il candidato deve scadere entro circa 95 secondi.
4. Una sola subnet verifica multicast/HTTP locale, NON il routing tra VLAN.
   Ripetere a scuola con Wi-Fi/cavo su subnet diverse, hostname bootstrap e
   TCP 3702 consentito dalle ACL.
5. Verificare reboot, aggiornamento e disinstallazione. Backup/chiavi conservati;
   tipo di avvio originale ripristinabile. Il servizio originale non viene
   riavviato automaticamente dalla disinstallazione.

Riportare passaggio, passato/non passato ed errore testuale, senza credenziali.
Prima della stabile servono anche carico prolungato su una classe e chiusura
dei punti ancora aperti in AUDIT.md.

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
