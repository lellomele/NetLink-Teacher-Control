# Funzionalità e configurazione

[English](USAGE.md) · [Italiano](USAGE.it.md)

## Console docente e controllo remoto

La Console riutilizza le postazioni del Configurator. La connessione reale e
la miniatura aggiornata verificano uno scambio autenticato; un test TCP positivo
non basta. Per le nuove funzioni devono essere presenti client NetLink compatibili.

Il menu remoto è una barra compatta che lascia visibile il desktop. Ogni nuova
sessione parte dal bordo superiore. La puntina resta visibile e si attenua dopo
un breve periodo di inattività; passando il mouse torna nitida.
Clic sinistro fissa o libera la barra. Clic destro su puntina o barra sposta
i controlli nell'ordine alto → sinistra → basso → destra.
Il bordo attivo è uno solo. Menù e focus da tastiera mantengono accessibili i comandi.

Aspetto, colori, dimensioni e set di icone sono preferenze personali.
La barra remota segue il set della Console. In contrasto elevato la puntina
non si attenua. La lingua si sceglie nel Configurator: applicare e riavviare
le applicazioni. I dialoghi di Windows possono usare la lingua del sistema.

## Configurator

- **Facile:** accesso, chiavi, impostazioni essenziali del servizio, gruppi/PC
  e discovery.
- **Normale/Avanzata:** accesso alle altre impostazioni.
- Cambiare modalità non deve modificare i valori nascosti.
- Nelle tabelle dei gruppi/PC i comandi di aggiunta/rimozione sono sopra le
  tabelle e restano accessibili senza massimizzare la finestra.

Con autenticazione a chiave, privata leggibile soltanto dagli utenti docenti
autorizzati e pubblica sui client. Non distribuire la privata.
Se sono presenti più private, la scelta e i relativi permessi vanno verificati:
`VEYON_AUTH_KEY_NAME` consente una selezione esplicita.
Una prova che richiede una privata non valida la sola pubblica di un client.

## InternetGuard WFP

1. Selezionare i client, aprire InternetGuard e scegliere **Blocca Internet**.
2. Leggere il feedback per ciascun client: conferma, errore o nessuna risposta.
3. Per rilasciare i filtri usare **Sblocca**. Chiudere il pannello non sblocca.
4. **Verifica stato** interroga il servizio, ma non esegue una prova di navigazione.

Il Master rinnova ogni 20 secondi; la scadenza prevista è 90 secondi senza rinnovo.
La lista autorizzata riutilizza i PC selezionati e l'indirizzo effettivo del Master.
DNS/DHCP delle interfacce attive sono rilevati dal client; risorse extra IP/CIDR
si aggiungono nella pagina NetLink del Configurator, soltanto quando necessarie.
Non autorizzare intere subnet senza motivo.

Le destinazioni autorizzate sono raggiungibili su tutte le porte: proxy, VPN
o servizi di inoltro possono consentire accessi esterni. Una policy di un'altra
connessione docente non è sostituibile; una riconnessione può dover attendere
la scadenza precedente. Se Windows non conferma la chiusura della sessione WFP,
lo stato resta sconosciuto e il servizio ritenta; non viene dichiarato un falso sblocco.

Limiti: 256 client per operazione, 255 destinazioni totali e 128 risorse extra.
Non usare come destinazioni InternetGuard IPv6 link-local con indicazione
della scheda (`%numero`): preferire IP instradabili o hostname risolvibili.

## Appello e profili classe–aula

**Appello → Richiedi nomi** apre il dialogo nelle postazioni selezionate.
Lo studente risponde con OK o Invio. La didascalia è
`[nome Appello] - account Windows - PC`, con sfondo distinto sul nome.
Non sono previste conferme di presenza e il nome non è un'identità verificata.

In **Posizioni e computer → Profili classe–aula…**:

1. Assegnare classe, aula e studente alle postazioni. L'editor parte dai PC
   selezionati, oppure da quelli visualizzati in assenza di selezione.
2. **Salva JSON** conserva le assegnazioni; **Carica JSON** le ricarica.
   Non è il JSON del Configurator e non viene caricato automaticamente all'avvio.
3. Modificare il nome assegnato oppure usare **Usa nomi dichiarati** sulle righe
   scelte. Le risposte non riscrivono il file: salvarlo esplicitamente.
4. **Esporta CSV** produce una copia leggibile con assegnazioni e dichiarazioni,
   non ricaricabile come profilo.
5. **Termina richieste e cancella nomi dichiarati** mantiene il profilo;
   **Nuovo / scarica profilo** rimuove le etichette attive, non i file sul disco.

I nomi precaricati sono distinti dalle dichiarazioni correnti. Alla disconnessione
le dichiarazioni vengono eliminate, le assegnazioni restano.
L'associazione usa l'UUID della postazione, non l'IP: con DHCP usare hostname
risolvibili o aggiornare il Configurator. Il profilo non corregge vecchi IP.
PC cancellati e ricreati vanno riassegnati. JSON/CSV contengono dati personali
in chiaro e vanno conservati in cartelle protette, fuori dalla cartella del programma.
Una scrittura su rete può finire dopo la chiusura del pannello.

## Trova client

**Automatico — preferisci IPv4** sceglie un annuncio IPv4 recente, altrimenti
un IPv6 instradabile e infine link-local. **Solo IPv4** nasconde i risultati
soltanto IPv6. La preferenza è personale e viene salvata chiudendo la Console;
non disabilita IPv6 in Windows e non esegue failover TCP.

Una riga raggruppa gli indirizzi osservati della stessa identità discovery.
Il tooltip li elenca; gli indirizzi semplicemente dichiarati nei metadati
non vengono usati come prova. Il docente è escluso su tutte le schede.
Annunci contraddittori restano visibili e non sono importabili automaticamente;
PC clonati con identità e dati uguali possono essere indistinguibili.

**Nascondi i computer già presenti in Posizioni e computer** confronta gli
indirizzi osservati con IP configurati e risposte DNS A/AAAA degli hostname.
Considera tutte le aule. Nomi non risolti lasciano i risultati visibili;
con DHCP un IP vecchio non prova l'identità di un PC.
Disattivare il filtro per vedere anche i PC già noti.

Verificare il destinatario prima dell'importazione. Servono diritti di scrittura
sulla configurazione e riavvio del Master per visualizzare il nuovo PC.
L'importazione non modifica chiavi o autorizzazioni.
Un link-local conserva il numero della scheda del docente: vale su quel
collegamento e può richiedere aggiornamento se la scheda cambia.

### Rete e porte

- Stesso collegamento: multicast WS-Discovery su UDP 3702.
- Altre subnet: indicare sui client nome/IP del docente in
  `School/DiscoveryMaster`. Gli annunci diretti usano HTTP su TCP 3702;
  routing e ACL devono consentirlo. L'installer accetta anche `/Master=NOME-PC`.
- Le eccezioni discovery gestite sono limitate ai profili Privato/Dominio.
  Il multicast non attraversa autonomamente VLAN/subnet; quello IPv6 è link-local.

## Blocco input e stato del servizio

Il blocco software intercetta scorciatoie ordinarie e mostra una copertura.
Le schermate protette restano accessibili: al ritorno la copertura viene ripristinata.
Gli eventi segnalano osservazioni di sessione/desktop, non provano Ctrl+Alt+Canc.
Transizioni brevissime possono sfuggire. Gli avvisi sono raggruppati circa ogni
due secondi, inviati al Master che ha richiesto più recentemente il blocco e
conservati in un riepilogo volatile di massimo 200 righe. Nessun testo digitato
viene raccolto; dopo una riconnessione ripetere Unlock/Lock.

L'icona del servizio mostra un riepilogo locale e il menu apre i dettagli.
Controllare anche le icone nascoste di Windows. “In ascolto” non certifica
raggiungibilità, autenticazione, discovery o blocco Internet.

## Companion

Da **Start → NetLink → Companion**, scegliere il ruolo ed eseguire **Analizza PC**.
Le prove distinguono:

- **Controllo remoto:** docente → client, normalmente TCP 11100, con servizio
  attivo sul client; opzionalmente verifica il messaggio iniziale RFB.
- **Discovery diretto:** client → docente, TCP 3702 e Console aperta. Prova
  soltanto l'apertura TCP, non il multicast o un annuncio applicativo.
- **TCP personalizzato:** destinazione/porta esplicite, anche loopback per il
  confronto fra accesso locale e remoto.

Le prove durano al massimo 8 secondi. Il rapporto completo conserva fino a
20 risultati della sessione e dati identificativi; il ridotto li omette.
TCP raggiungibile, banner RFB e impronte pubbliche uguali non provano che
l'autenticazione completa funzioni. Un test può produrre un avviso di
autenticazione incompleta sul servizio.

### Collegamento protetto fra Companion

1. Sul ricevente: Analizza PC, **Ricevo il rapporto**, interfaccia della rete
   e porta temporanea (TCP 11110 o 0 per automatica), **Genera invito e attendi**.
2. Trasferire l'invito riservato con **Copia invito** o **Salva invito su file**.
   Non è una chiave Veyon e non va importato nel Configurator.
3. Sull'altro PC: Analizza PC, **Invio il mio rapporto**, incollare/aprire l'invito,
   **Verifica destinatario e invia il mio rapporto**, confermare i dati condivisi.
4. Attendere **CONSEGNATO** e **RICEVUTO**. Per invertire la direzione serve
   un nuovo invito. **Annulla collegamento** chiude l'attesa.

Il canale è TLS 1.2 o superiore, con certificato temporaneo, impronta e segreto
monouso a 256 bit. Il formato compatto NL2 mantiene i segreti completi; accetta
inviti NL1, mentre i client vecchi non leggono NL2. L'invito scade entro cinque
minuti e consente un solo rapporto. Nessun listener parte automaticamente,
nessuna regola firewall viene modificata e nessun comando remoto è eseguito.
Un'eventuale autorizzazione firewall deve essere locale e mirata al ricevente.
I rapporti sono istantanee non firmate, non attestazioni fidate del sistema.
