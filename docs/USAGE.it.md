# Funzionalità e configurazione

[English](USAGE.md) · [Italiano](USAGE.it.md)

## Console docente e controllo remoto

La Console riutilizza le postazioni del Configurator. La connessione reale e
la miniatura aggiornata verificano uno scambio autenticato; un test TCP positivo
non basta. Per le nuove funzioni devono essere presenti client NetLink compatibili.

Passando il mouse su una miniatura connessa, **Diagnostica del servizio client**
mostra plugin VNC, porta interna del backend, stato della negoziazione VNC e
tempo dall'ultimo fotogramma inoltrato su quella specifica connessione.
Non si contano i fotogrammi di altre sessioni. Console e client devono entrambi
supportare l'estensione (alpha.29 o successiva). Si usa il collegamento NetLink
già autenticato, senza aprire nuove porte. I rapporti si aggiornano circa ogni
15 secondi; mancata risposta, dati non validi e disconnessione invalidano il
riepilogo, con tentativi più distanziati per i client precedenti.
Un client precedente, offline o non ancora connesso è indicato con diagnostica
non disponibile, non come guasto VNC. Se la negoziazione iniziale non può
completarsi restano necessari i dettagli locali del servizio: la diagnostica
non aggira tale negoziazione. I fotogrammi inoltrati non provano la visualizzazione
sul Master; un desktop immutato può non inviarne di nuovi. Non si eseguono
sonde video né modifiche della configurazione.

Il doppio clic su una miniatura esegue l'azione configurata (visualizzazione
remota per impostazione predefinita). **Ctrl+doppio clic** avvia il controllo
remoto soltanto di quella postazione, anche se erano selezionate altre miniature.
Restano validi i permessi e le funzioni disabilitate: la scorciatoia non concede
ulteriori autorizzazioni.

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

I pulsanti dei pannelli, compresi i profili classe–aula, mostrano un effetto
temporaneo di pressione con mouse e barra spaziatrice. Al rilascio tornano allo
stato normale/selezionato, senza ritardi di animazione o comandi aggiuntivi.
In alto contrasto si mantiene il disegno accessibile nativo di Windows.

**Lock e Demo** rileggono le miniature selezionate a ogni richiesta, sia dalla
barra strumenti sia dal menu contestuale. Selezionare almeno una postazione:
nessuna selezione non significa tutti i PC. Una selezione mista avvia la modalità
sul gruppo; se tutti i selezionati hanno già quella modalità, il pulsante la
ferma solo per loro. Gli altri destinatari continuano a ricevere Demo. Per
condividere lo schermo di uno studente, selezionare sorgente e destinatari,
poi scegliere la sorgente tra questi PC. Il cambio sorgente è rifiutato se
PC non selezionati stanno ricevendo una Demo diversa.

## Configurator

- **Facile:** accesso, chiavi, impostazioni essenziali del servizio, gruppi/PC
  e discovery.
- **Normale/Avanzata:** accesso alle altre impostazioni.
- Cambiare modalità non deve modificare i valori nascosti.
- Nelle tabelle dei gruppi/PC i comandi di aggiunta/rimozione sono sopra le
  tabelle e restano accessibili senza massimizzare la finestra.

Con autenticazione a chiave, privata leggibile soltanto dagli utenti docenti
autorizzati e pubblica sui client. Non distribuire la privata.
Una prova che richiede una privata non valida la sola pubblica di un client.

### Chiavi dei laboratori e spostamento dei PC

1. Sul Master, **Chiavi di autenticazione → Chiave di autenticazione del Master**
   permette di scegliere la privata predefinita. Resta disponibile la scelta
   automatica per compatibilità; con più chiavi è preferibile sceglierne una esplicita.
2. In **Posizioni e computer**, evidenziare un gruppo e scegliere **Chiave privata
   del gruppo selezionato**, oppure mantenere **Usa la chiave predefinita del Master**.
   Applicare e riavviare la Console dopo aver terminato Lock, Demo e InternetGuard.
3. La chiave specifica del gruppo ha priorità. Altrimenti la variabile di avvio
   `VEYON_AUTH_KEY_NAME` prevale sulla predefinita; in assenza di entrambe si usa
   la prima privata leggibile, come prima. Una chiave esplicita non valida o non
   disponibile causa un errore senza provare la chiave di un altro laboratorio.
4. Il Master può collegarsi contemporaneamente a gruppi con chiavi diverse.
   I client devono avere la pubblica corrispondente con lo stesso nome e i normali
   permessi di accesso. Nessuna privata viene inviata ai client e il protocollo
   di autenticazione non cambia.

Nel Configurator selezionare una o più righe di PC (Ctrl/Maiusc), poi **Sposta
nel gruppo…**. Scegliere la destinazione e confermarne la chiave. **Applica** salva
la modifica; riavviare la Console quando non ci sono funzioni attive. Cambia solo
il gruppo sul Master: UUID, nome, indirizzo e MAC rimangono. Alla successiva
connessione della Console il PC usa la chiave di destinazione, senza ripiegare su
quella di origine o sostituire chiavi remote. Le chiavi di destinazione mancanti
vengono segnalate, non generate. Clic destro su **Posizioni/computer** nella Console
offre il collegamento per aprire Configurator.

I profili classe salvati mantengono i nomi associati agli UUID. Il caricamento di
un vecchio profilo avvisa dei PC ora in altri gruppi, mantiene gruppi/chiavi attuali
e non riporta indietro i PC né riscrive il file salvato. Un PC ricreato con un nuovo
UUID non viene associato per nome/IP. Le chiavi dei gruppi riguardano la directory
integrata, non i gruppi LDAP. Le connessioni remote manuali senza identità nella
directory usano la chiave predefinita del Master.

Il tooltip della miniatura mostra la chiave scelta da questo Master e gli errori
locali di lettura: non è una prova di autorizzazione sul client. Su un client
aggiornato, i dettagli dello stato elencano le **chiavi pubbliche in uso dalle
connessioni autorizzate** e, separatamente, l'**ultima chiave pubblica autorizzata
dall'avvio del servizio**. Nel tooltip breve compare nome/numero delle chiavi se
c'è spazio. Senza connessioni autorizzate non c'è una chiave attualmente in uso,
anche se alcune sono installate. I tentativi rifiutati non cambiano l'ultima chiave
autorizzata. Non viene aperta nessuna porta aggiuntiva.

## InternetGuard WFP

1. Selezionare i client nella Console e aprire InternetGuard: tutte le righe sono
   inizialmente selezionate. Ctrl-clic/Maiusc-clic modificano la selezione;
   **Blocca Internet**, **Sblocca** e **Verifica stato** agiscono solo sulle righe
   evidenziate. Senza selezione i comandi sono disabilitati. Il pannello segue
   le miniature selezionate nella Console e le ricontrolla prima di ogni comando,
   anche se già aperto. Le righe possono restringere ulteriormente i destinatari,
   mai aggiungere miniature non selezionate.
2. Leggere il feedback per ciascun client: conferma, errore o nessuna risposta.
3. Per rilasciare i filtri usare **Sblocca**. Chiudere il pannello non sblocca.
4. **Verifica stato** interroga il servizio, ma non esegue una prova di navigazione.

Il filtro attivo confermato aggiunge **[web locked]** sulla miniatura, bianco su
rosso opaco. Lo sblocco confermato rimuove l'etichetta; attese ed errori rimangono
nel tooltip e nel pannello InternetGuard. È lo stato dei filtri, non un test web.

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
La disconnessione invalida le vecchie conferme. Chiudendo la Console si fermano
i rinnovi e si inviano gli sblocchi: una risposta non confermata non prova lo sblocco.

Limiti: 256 client per operazione, 255 destinazioni totali e 128 risorse extra.
Non usare come destinazioni InternetGuard IPv6 link-local con indicazione
della scheda (`%numero`): preferire IP instradabili o hostname risolvibili.

## Appello e profili classe–aula

**Appello → Richiedi nomi** apre il dialogo nelle postazioni selezionate.
Lo studente risponde con OK o Invio. La didascalia è
`[nome Appello] - account Windows - PC`, con sfondo distinto sul nome.
Non sono previste conferme di presenza e il nome non è un'identità verificata.

In **Posizioni e computer → Profili classe–aula…**:

1. Evidenziare una riga di gruppo/aula oppure di un suo PC in **Posizioni e computer**.
   È la riga evidenziata a scegliere il gruppo, non le spunte o le miniature selezionate.
   **Usa gruppo evidenziato** riempie l'editor con tutti i PC configurati di quel gruppo,
   anche senza spunta o nascosti dalla ricerca testuale. Impostare classe, aula e nomi.
2. **Salva JSON** salva tutti i PC di quel gruppo, non gli altri gruppi spuntati.
   **Esporta CSV** usa lo stesso criterio. Mantiene le assegnazioni modificabili già
   presenti; per i PC non ancora assegnati usa il nome Appello corrente, se disponibile.
   **Usa nomi dichiarati** copia esplicitamente le risposte più recenti.
   Cancellare un nome non elimina il PC configurato.
3. **Carica JSON** riepiloga classe, gruppo e numero di PC riconosciuti. Confermando,
   spunta solo i PC salvati e ripristina i nomi. Gli altri PC sono deselezionati, non
   eliminati: albero, appartenenza ai gruppi, indirizzi attuali, chiavi e autorizzazioni
   restano invariati. I PC mancanti/nascosti sono elencati, non creati né associati
   tramite IP o nome riutilizzato. Se nessun PC è disponibile non cambia nulla.
   Prima del caricamento terminare Lock, Demo e InternetGuard, compresi gli sblocchi
   non confermati. Annulla conserva il profilo attivo. I vecchi JSON classe–aula sono
   leggibili; i nuovi file con gruppo richiedono alpha.31 o successiva.
   Non è il JSON del Configurator e non viene caricato automaticamente all'avvio.
4. Modificare il nome assegnato oppure usare **Usa nomi dichiarati** sulle righe
   scelte. Le risposte non riscrivono il file: salvarlo esplicitamente.
5. **Esporta CSV** produce una copia leggibile con assegnazioni e dichiarazioni,
   non ricaricabile come profilo.
6. **Termina richieste e cancella nomi dichiarati** mantiene il profilo;
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
Se gli annunci discordano, lo stato distingue nomi differenti, porte NetLink
differenti oppure entrambi. Selezionare la riga per vedere ciascun nome, indirizzo
e porta nel pannello dei dettagli; il tooltip mostra la stessa spiegazione.
L'importazione automatica resta bloccata. Più indirizzi IPv4/IPv6 con nomi e porte
coerenti non costituiscono un conflitto.
Il tooltip li elenca; gli indirizzi semplicemente dichiarati nei metadati
non vengono usati come prova. Il docente è escluso su tutte le schede.
Annunci contraddittori restano visibili e non sono importabili automaticamente;
L'identità discovery combina ID del sistema, nome del computer normalizzato e,
su Windows, MAC permanenti disponibili delle schede fisiche Ethernet/Wi-Fi.
Sono incluse le schede installate anche se scollegate; interfacce virtuali/filtro
e MAC inutilizzabili sono ignorati. Non dipende da connessione attiva, indirizzo IP
o MAC corrente/casuale. I MAC entrano nel calcolo locale dell'hash, non sono aggiunti
in chiaro agli annunci. Se non disponibili, ID del sistema + nome distinguono
comunque i PC clonati con nomi diversi. Non è autenticazione: input identici o
contraffatti possono ancora coincidere.

Rinominare un PC, reinstallare Windows o aggiungere/rimuovere/sostituire una scheda
inclusa può cambiare l'identità discovery. Non cambia chiavi di autenticazione,
UUID delle postazioni già configurate o profili classe. L'identità resta fissa
per ciascun agente discovery in esecuzione: dopo tali modifiche riavviare il servizio.
Aggiornare i client interessati, non soltanto il Master. I vecchi client mantengono
i precedenti annunci; dopo l'upgrade attendere circa 95 secondi per la scadenza
dei dati obsoleti. Nessun identificatore Windows o parametro delle schede è modificato.

**Nascondi i computer già presenti in Posizioni e computer** confronta gli
indirizzi osservati con IP configurati e risposte DNS A/AAAA degli hostname.
Considera tutte le aule. Nomi non risolti lasciano i risultati visibili;
con DHCP un IP vecchio non prova l'identità di un PC.
Disattivare il filtro per vedere anche i PC già noti.

Verificare il destinatario prima dell'importazione. Servono diritti di scrittura
sulla configurazione e riavvio del Master per visualizzare il nuovo PC.
Prima di importare, evidenziare il gruppo di destinazione o la riga di uno dei
suoi PC in **Posizioni e computer**. Le spunte e le miniature selezionate non
scelgono il gruppo. Discovery mostra la destinazione e chiede conferma con
indirizzo e porta. Nessuna selezione significa nessuna importazione: non sceglie
il primo gruppo e non crea gruppi automaticamente. Se il gruppo viene cambiato,
rinominato o eliminato durante la conferma, occorre ripetere l'importazione.
L'importazione non modifica chiavi o autorizzazioni.
Selezionare più righe della discovery con **Ctrl** o **Maiusc**, oppure usare
**Seleziona tutti**; **Deseleziona** azzera la selezione. **Importa PC selezionati**
presenta una sola conferma con numero, gruppo ed elenco completo in Mostra dettagli.
Importa soltanto la selezione confermata, con un unico salvataggio; nuove righe
trovate nel frattempo non vengono aggiunte automaticamente. Dopo la conferma
ricontrolla tutte le righe: se una è già configurata, scaduta, in conflitto, usa
una porta diversa o condivide un indirizzo rilevato con un'altra riga selezionata,
non aggiunge alcun PC. Risolvere/deselezionare la riga interessata e riprovare.
La selezione sopravvive agli aggiornamenti per identità discovery; le righe
nascoste o scadute vengono rimosse dalla selezione visibile.
I permessi devono appartenere al processo Console: un Configurator amministratore
non la eleva. Un annuncio cambiato, scaduto o in conflitto va selezionato nuovamente.
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
Il tooltip breve mostra un solo indirizzo, preferendo l'IPv4 usato da una
connessione NetLink autenticata. In assenza usa la scelta di instradamento di
Windows, poi gli indirizzi Ethernet/Wi-Fi attivi. Se non esiste IPv4 resta IPv6.
Gli altri indirizzi locali rimangono nei dettagli: non sono client collegati e
la loro presenza non dimostra raggiungibilità da remoto. La scelta non invia
sonde di rete.
L'icona del Worker riporta il listener del Server locale: non è il Worker ad aprire
una porta di controllo remoto separata. I dettagli elencano indirizzo di ascolto,
IP delle interfacce, plugin VNC, porta interna e fotogrammi inoltrati per connessione.
Il controllo TCP interno è asincrono e solo locale: non verifica l'autenticazione VNC.
Una connessione autorizzata senza fotogrammi non dimostra che il video funzioni.
Controllare anche le icone nascoste di Windows. “In ascolto” non certifica
raggiungibilità, autenticazione, discovery o blocco Internet.

### Avvio e ripristino su Windows

Abilitare **Avvio automatico del servizio** nel Configurator → Servizio per
l'avvio dopo un riavvio. L'installer conserva questa scelta, compresa un'eventuale
impostazione manuale intenzionale; non la sovrascrive silenziosamente.
Con l'opzione abilitata l'avvio Windows è Automatico, non ritardato.
L'installazione verifica servizio in esecuzione, modalità di avvio e ripristino;
se il controllo fallisce segnala un'installazione incompleta.

Windows ritenta dopo un errore imprevisto del servizio a distanza di 10, 30 e
60 secondi, poi interrompe i tentativi fino all'azzeramento del conteggio dopo
un giorno senza errori. Un normale arresto amministrativo o lo spegnimento di
Windows non è un errore. Non ci sono attività di sorveglianza pianificate,
riavvii automatici del PC o garanzie contro errori persistenti di avvio.
Riavviare Windows dopo l'aggiornamento per attivare tutte le opzioni di ripristino.

Se un client rimane offline dopo il riavvio, controllare prima NetLink Service
nei Servizi di Windows. Se è arrestato, usare Companion → Analizza PC: il rapporto
completo include modalità di avvio effettiva, codici di uscita, dipendenze e stato
del ripristino. Controllare anche Visualizzatore eventi → Registri di Windows →
Sistema → Service Control Manager, all'ora dell'avvio. Una prova di ascolto o
firewall non spiega un servizio arrestato. Da terminale amministrativo nella
cartella di installazione, `netlink-cli.exe service verify` esegue lo stesso
controllo dell'avvio senza modificare impostazioni.

## Companion

Se installato, da **Start → NetLink → NetLink Companion**, scegliere il ruolo ed eseguire **Analizza PC**.
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
