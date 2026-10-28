# Architettura, protezioni e stato delle verifiche

[English](AUDIT.md) · [Italiano](AUDIT.it.md)

L'audit di NetLink è **parziale**. Questo documento descrive i meccanismi presenti,
i limiti e le prove necessarie; non certifica l'assenza di vulnerabilità,
la conformità completa WS-Discovery o l'impiego in produzione.

## Comunicazione e ciclo di vita

- L'avvio del servizio Windows contatta Service Control Manager prima di
  inizializzare IPC, sessioni e driver di input, segnalando progressi effettivi.
  Errori di avvio o del punto d'ingresso producono un codice di uscita non nullo;
  arresti/spegnimenti espliciti restituiscono successo. Registrazione e avvio
  propagano gli errori nativi: la sola presenza del servizio non è un successo.
- La registrazione ripara solo l'eseguibile NetLink/precedente atteso nella stessa
  cartella. Avvio automatico e ripristino nativo limitato sono riletti dopo la
  configurazione. Il client SMB facoltativo non è una dipendenza d'avvio.
  Nessun watchdog esterno o riavvio del PC. Riferimenti Microsoft:
  [tempi del dispatcher](https://learn.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-startservicectrldispatcherw),
  [azioni di ripristino](https://learn.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_failure_actionsw)
  e [errori senza crash](https://learn.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_failure_actions_flag).
  Quest'ultima opzione diventa effettiva dopo il riavvio del sistema. I test con
  SCM simulato coprono registrazione, riparazione, errori, rilettura e arresto
  esplicito, ma non riproducono l'avvio di un client fisico né identificano la
  causa di un errore d'avvio di cui non sono stati raccolti i dati.
- Autenticazione e controllo degli accessi precedono l'esecuzione delle funzioni.
  L'adattatore RFB distingue letture bloccanti esatte da frame asincroni parziali.
- Frame, dimensioni delle immagini, code, comandi e lotti di eventi hanno limiti.
  Il decoder pubblica snapshot delle miniature separati dal buffer del worker;
  gli aggiornamenti vuoti non provocano copie o ridisegni inutili.
- Stop e riconnessione scartano comandi e callback obsoleti. Scritture TCP/OpenSSL
  usano scadenze monotone assolute, anche se il peer non legge o TLS ritenta.
- I worker locali hanno nonce monouso, verifica della proprietà del socket,
  timeout di avvio/autenticazione e code limitate. I timer di recupero sono
  legati alla vita degli oggetti; i messaggi non autenticati hanno limiti più stretti.
- Le finestre InternetGuard/Appello/discovery sono non modali, riutilizzabili
  e conservano l'insieme dei client selezionati all'apertura.
- DNS delle nuove funzioni è asincrono e cancellabile. Una policy non viene
  applicata usando soltanto la parte risolta di una lista incompleta.

**Punti aperti:** il codice ereditato contiene ancora DNS bloccante, arresto
forzato del thread VNC in condizioni eccezionali e attese native senza limite
in alcuni percorsi di chiusura. Occorre una revisione del ciclo di vita,
non un timeout che lasci callback attive su memoria liberata.
Gli avvisi di ritardo GUI vengono emessi dopo il recupero: non provano
un deadlock e possono includere sospensione/ripresa di Windows.

## InternetGuard e servizio privilegiato

I filtri WFP sono installati in una sessione dinamica con transazioni e lease
di 90 secondi. Commit, stato e rinnovo verificano gli ID dei filtri installati.
La pipe verifica SYSTEM e i percorsi attesi di servizio/server; i worker grafici
non possono richiedere direttamente modifiche privilegiate alla rete.

Le richieste hanno limiti di concorrenza; una connessione può conservare una
richiesta esplicita di sblocco pendente. Se la chiusura WFP fallisce,
handle, proprietario e stato di recupero sono conservati per il ritentativo;
un nuovo blocco non può nascondere la pulizia incompleta.
Lo stato non viene presentato come libero se Windows non lo conferma.

Il feedback conferma filtri applicati, non Internet irraggiungibile.
Destinazioni autorizzate su tutte le porte, resolver DNS, proxy, VPN, QUIC,
connessioni preesistenti, cambi di scheda e altri fornitori firewall richiedono
prove reali. Gli IPv6 link-local scoped non sono accettati come risorse.
Chiamate BFE/RPC bloccate possono ritardare scadenza o arresto del servizio.

Limiti: 256 client per operazione, 255 destinazioni e 128 risorse aggiuntive.

## Appello e profili

Richieste e risposte sono correlate da identificativi; risposte obsolete,
cambi di profilo e disconnessioni non devono riutilizzare vecchi nomi.
La didascalia mantiene separati nome Appello, account Windows e PC.
I testi sono limitati e trattati come testo, non HTML.

I profili sono solo sul Master: schema JSON versionato, massimo 1 MiB e
256 assegnazioni, UUID validi/univoci e stringhe limitate. IP e hostname sono
indicazioni, non identità di associazione. Caricamenti non validi non sostituiscono
il profilo attivo. I nomi precaricati sono distinti dalle dichiarazioni e
non costituiscono conferme di presenza.

Letture e scritture JSON/CSV sono fuori dal thread GUI. QSaveFile evita fallback
a scritture dirette; i prefissi di formule CSV sono neutralizzati.
Su Windows i permessi Qt non sostituiscono le ACL NTFS: proteggere la cartella.
Una scrittura già avviata su una risorsa di rete può completarsi dopo la chiusura
del pannello. I file restano dati personali in chiaro, non attestazioni di identità.

## Discovery IPv4/IPv6

La modalità automatica preferisce IPv4 osservato di recente, poi IPv6 instradabile
e link-local. Solo IPv4 resta selezionabile. È una scelta degli annunci,
non un test di raggiungibilità TCP o un failover delle connessioni.

La cache limita osservazioni totali e per famiglia/identità, con scadenza di
95 secondi, sequenze e rilevamento duplicati per sorgente. Un Bye rimuove solo
l'osservazione della propria sorgente. Gli indirizzi reali sono quelli del mittente,
non alias arbitrari dichiarati nei metadati; nessun URL remoto viene seguito.

Gli annunci della stessa EPR producono una riga. Nomi/porte contraddittori
restano visibili e non importabili automaticamente. Identità e dati identici
su immagini clonate non sono distinguibili da questi annunci non autenticati.
Le nuove EPR locali calcolano un hash da campi con lunghezza esplicita: ID del
sistema, nome normalizzato, ruolo e MAC permanenti ordinati e deduplicati delle
schede fisiche Ethernet/Wi-Fi. Nessuna ricerca in rete, nuova porta o scrittura
di impostazioni. Senza MAC si usa ID del sistema + nome, mai un'identità casuale
per ogni annuncio. Modifiche a hardware, sistema o nome possono cambiare EPR;
UUID delle postazioni/profili e autenticazione sono separati e non cambiano.
Windows usa [GetIfTable2](https://learn.microsoft.com/en-us/windows/win32/api/netioapi/nf-netioapi-getiftable2)
e indirizzo permanente/flag hardware di
[MIB_IF_ROW2](https://learn.microsoft.com/en-us/windows/win32/api/netioapi/ns-netioapi-mib_if_row2),
non MAC attivo/casuale o indice di interfaccia. Questi dati del driver non sono
prove crittografiche; un hash non rende segreto un MAC indovinabile.
Il docente è escluso mediante i suoi indirizzi su tutte le schede, non per nome.

I link-local usano l'indice dell'interfaccia ricevente, mai il numero remoto.
Il filtro dei PC noti usa tutti gli indirizzi osservati contro destinazioni
configurate e DNS A/AAAA. Sono ammesse otto verifiche DNS concorrenti e una
scadenza di cinque secondi; gli errori lasciano i risultati visibili.
L'importazione riconvalida modalità, freschezza, conflitti e duplicati.

XML/HTTP hanno limiti di dimensione, profondità, nodi, connessioni, frequenza
e durata. Le risposte SOAP sono limitate a 32 KiB e 64 record; la cache ha
al massimo 1.024 osservazioni. La conformità allo schema XML non certifica
l'intero protocollo WS-Discovery. Il multicast IPv6 è locale al collegamento;
routing, ACL e scenari fra subnet richiedono collaudo distinto.

## Interfaccia, input e acquisizione

La barra remota è un widget compatto, non un overlay su tutto il desktop.
Timer/animazioni appartengono al widget; il clic destro cambia bordo localmente,
senza inoltrare comandi al client. Focus, menu, DPI e contrasto elevato sono
coperti da test grafici, ma rimane necessaria una verifica su monitor reali.

Il blocco input usa il meccanismo software delle scorciatoie ordinarie.
Eventi di sessione e osservazioni del desktop protetto sono raggruppati,
limitati e inoltrati soltanto al Master interessato. Non raccolgono testo
digitato né identificano con certezza Ctrl+Alt+Canc; non esiste un registro
forense o recupero degli eventi durante la disconnessione.

La DLL di aggancio distribuita importa soltanto KERNEL32/USER32, senza
inizializzazione CRT/pthread, TLS o dipendenze ritardate. I callback e la coda
regioni hanno limiti e sincronizzazione dedicata.
Interception e DeskDupEngine binario non sono distribuiti. Acquisizione software,
input e chiusura dei componenti ereditati devono essere provati sotto carico;
non è dichiarata equivalenza con modalità accelerate o driver.

## Companion e protezione dei rapporti

La raccolta avviene in un processo figlio limitato per tempo e dimensione:
è una scelta di reattività, non una sandbox di sicurezza.
La diagnostica distingue listener, profilo rete, regole configurate e prove TCP;
non certifica la policy effettiva del firewall né l'autenticazione.
I rapporti completi contengono identificativi; quelli ridotti li escludono.

Il collegamento temporaneo usa TLS, impronta SHA-256 verificata e segreto
monouso a 256 bit confrontato in tempo costante. Ha scadenze e limiti per
frame, tentativi, connessioni e singola consegna. Le notifiche sono legate
alla generazione della sessione, così non raggiungono un abbinamento successivo.
Non avvia listener automatici, non legge la privata Veyon e non modifica
firewall o configurazione. I rapporti ricevuti restano dichiarazioni non firmate.

Il controllo remoto eredita il modello di sicurezza Veyon 4.7.5:
il TLS del Companion non estende TLS/integrità generalizzati a RFB.
Credenziali persistenti richiedono una revisione specifica dello storage;
non assumere che la cifratura ereditata con chiave incorporata le protegga
da un soggetto con accesso al sistema. Non esporre la suite a reti non fidate.

## Installer e distribuzione

Un helper senza Qt salva impostazioni e backup, controlla percorsi e file
sostituibili, gestisce il servizio del fork e verifica SHA-256 prima di
avviare gli strumenti appena installati. Esclude percorsi reindirizzati e
non modifica i file delle chiavi esterne. Operazioni concorrenti sono escluse.

File occupati richiedono riavvio senza copia parziale volontaria; errori di
permessi sono distinti e nessun file può essere saltato. Non è un aggiornamento
atomico: il controllo non impedisce aperture successive né fornisce rollback completo.

La disinstallazione rimuove l'intero albero del programma senza seguire junction,
protegge percorsi delle chiavi e consente conservazione o pulizia delle impostazioni.
Mantiene le impostazioni condivise rilevate e non elimina dati OS o profili
non caricati. Dettagli e ripristino: [INSTALLATION.md](INSTALLATION.it.md).

## Copertura e criteri beta

I test coprono framing, timeout TCP/TLS, concorrenza, teardown, autenticazione,
stato WFP simulato, input simulato, callback, Appello/profili, DNS/discovery,
Companion e interfaccia a più scale. Sei tipi di messaggio discovery vengono
validati con gli schemi ufficiali. Il test IPv6 UDP usa loopback reale;
se IPv6 non è disponibile dichiara lo skip.

I test dell'installer usano file/sottochiavi isolate e simulano servizio/firewall.
Le build di distribuzione escludono hook di test e percorsi plugin di sviluppo.
Verifica delle importazioni, manifesto e sorgenti delle dipendenze non equivalgono
a un audit completo di tali librerie.

L'analisi statica ha lacune di preprocessing Qt/MinGW: non rappresenta copertura
completa. Restano necessari revisione delle dipendenze/CVE e backport, sanitizers
dove applicabili, prove prolungate di carico/rientranza e interoperabilità.
Non tutte le librerie ereditate sono allineate alle versioni upstream più recenti.

Per una beta controllata: congelare le funzionalità, chiudere difetti critici/alti,
verificare installazione/recupero reali, blocco e sblocco nativi, resilienza WFP,
uso prolungato con più client, lingua/DPI e protezione dei dati.
Dichiarare supportati altri scenari di rete soltanto dopo collaudo.
La firma digitale è distinta dalla correttezza e non sostituisce queste prove.

[Checklist operativa](PILOT.it.md) · [Dipendenze](DEPENDENCIES.it.md) ·
[WS-Discovery 1.1](https://docs.oasis-open.org/ws-dd/discovery/1.1/os/wsdd-discovery-1.1-spec-os.html) ·
[Arbitraggio WFP](https://learn.microsoft.com/en-us/windows/win32/fwp/filter-arbitration)
