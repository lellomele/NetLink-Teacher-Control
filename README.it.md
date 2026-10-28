[Italiano](README.it.md) | [English](README.md)

# NetLink Teacher Control

Suite NetLink per la gestione delle aule Windows, basata su **Veyon 4.7.5**.
Integra controllo remoto, InternetGuard WFP, Appello, profili classe–aula,
ricerca automatica dei client e diagnostica Companion, senza plugin esterni.

**Versione preliminare 1.0.0-alpha.36: usare consapevolmente.**

**[Scarica l’installer Windows x64 — ultima release (alpha.36)](https://github.com/lellomele/NetLink-Teacher-Control/releases/download/netlink-1.0.0-alpha.36/NetLink-1.0.0-alpha.36-win64-setup.exe)**

## Funzionalità

- **Console docente:** miniature dei PC, controllo remoto e comandi Veyon.
  Ctrl+doppio clic su una miniatura controlla quella postazione; il doppio clic
  normale mantiene l'azione configurata (visualizzazione remota per impostazione predefinita).
  La barra remota compatta non oscura il desktop: puntina permanente,
  fissaggio aperto e spostamento fra i bordi con clic destro.
- **Aspetto personalizzabile:** temi, colori e set coordinati di icone vettoriali,
  con supporto ai display ad alta risoluzione, descrizioni dei comandi ed effetto di pressione dei pulsanti.
- **InternetGuard WFP:** blocco/sblocco sui client selezionati, feedback di
  applicazione o errore e scadenza dei filtri in assenza di rinnovo.
  La finestra seleziona inizialmente tutte le righe; i comandi agiscono solo su quelle evidenziate.
  InternetGuard, Lock e Demo rileggono le miniature selezionate a ogni richiesta;
  nessuna selezione non significa mai tutta l'aula. Il filtro attivo confermato
  è indicato da `[web locked]` bianco su rosso; se inattivo non aggiunge etichette.
- **Appello:** nome dichiarato dallo studente sulle miniature, insieme ad account
  Windows e nome del PC. Non è una verifica dell'identità o della presenza.
- **Profili classe–aula:** assegnazioni studente/postazione modificabili,
  salvataggio e caricamento JSON sul Master, esportazione CSV e distinzione
  fra nomi precaricati e risposte dei client.
  Il salvataggio include tutti i PC del gruppo della riga evidenziata, anche senza spunta.
  Il caricamento spunta i PC salvati e ripristina i nomi senza sostituire l'albero di rete.
- **Trova client:** WS-Discovery, modalità automatica con IPv4 preferito e IPv6
  come alternativa, opzione Solo IPv4, filtro dei PC già configurati e
  accorpamento degli annunci della stessa identità.
  I conflitti mostrano nomi, indirizzi e porte osservati. Il gruppo di importazione
  si sceglie nella tendina della Discovery; la riga di Posizioni e computer è solo la proposta iniziale.
  Selezione multipla con Ctrl/Maiusc o Seleziona tutti e importazione con un'unica
  conferma; una riga non valida impedisce l'importazione dell'intera selezione.
  Identità adatte ai PC clonati: ID del sistema, nome del PC e MAC permanenti
  disponibili delle schede fisiche; aggiornare anche i client interessati.
- **Configurazione semplificata:** modalità Facile, Normale e Avanzata;
  riutilizzo di aule, PC, chiavi e regole di accesso già configurati.
  Chiave privata predefinita del Master e chiave facoltativa per gruppo/laboratorio
  della directory integrata. Spostamento dei PC selezionati senza ricrearne
  l'identità: usano la chiave di destinazione, senza modificare le chiavi dei client.
  I salvataggi uniscono modifiche indipendenti e rifiutano quelle concorrenti
  in conflitto; un registro protetto permette il recupero di scritture interrotte.
- **Stato e diagnostica:** icona del servizio con riepilogo locale, Companion
  per analizzare PC/rete e distinguere le prove di controllo remoto e discovery.
  Il tooltip preferisce l'IPv4 della rete attiva; nei dettagli restano gli altri
  indirizzi di ascolto, backend VNC e informazioni sui fotogrammi inoltrati.
  Lo stato del client distingue le chiavi pubbliche delle connessioni autorizzate
  dall'ultima usata, senza confonderle con quelle semplicemente installate.
  Le miniature connesse mostrano plugin VNC, porta interna e osservazioni sul
  video della singola connessione, attraverso il canale autenticato esistente.
  La funzione richiede docente e client aggiornati; risposte vecchie, mancanti
  o scadute sono indicate come non disponibili. Non servono altre porte.
  Un canale temporaneo abbinato esplicitamente consente lo scambio di rapporti.
- **Avvio del servizio Windows:** configurazione automatica verificata e ripristino
  nativo limitato dopo crash/errori di avvio. L'installer rileva gli errori di verifica;
  restano supportati l'avvio manuale intenzionale e lo stop regolare dell'amministratore.

## Installazione e aggiornamento

1. Scaricare l'installer Windows x64 dalla
   [pagina delle release](https://github.com/lellomele/NetLink-Teacher-Control/releases)
   e confrontare lo SHA-256 con il file di checksum allegato. L'alpha non è firmata.
2. Conservare un'esportazione JSON del Configurator e, separatamente, le chiavi.
   Il JSON non contiene le chiavi private.
3. Chiudere Console docente, Configurator e Companion. Installare il pacchetto
   completo come amministratore: scegliere **Master** sul docente oppure **Client**
   sugli studenti. Service e Configurator sono obbligatori; la Console docente è
   facoltativa e selezionata per Master. Companion è facoltativo, inizialmente
   deselezionato e aggiungibile rieseguendo l'installer. Gli aggiornamenti conservano
   le scelte installate. Non occorre disinstallare o rigenerare chiavi funzionanti.
4. Se l'installer segnala file in uso o rimozioni pendenti, riavviare Windows e
   rieseguire il pacchetto. Non sostituire singole DLL e non saltare file.

Il riferimento è **Windows 10 a 64 bit**. Le dipendenze sono incluse.
La suite include Qt 5.15.19 con patch KDE: non occorre installare Qt separatamente.

La configurazione Veyon è riutilizzata. La chiave privata resta soltanto sul pc docente;
ai client si distribuisce la chiave pubblica.

L'installer verifica i file prima della copia e conserva configurazione e chiavi.
La disinstallazione può mantenere oppure eliminare le impostazioni; rimuove
l'intera cartella del programma e conserva le chiavi esterne. Se le chiavi sono
dentro la cartella da rimuovere, si ferma: trasferirle prima in un percorso protetto.
I backup delle impostazioni sono in `C:\ProgramData\NetLink\Recovery`.

Procedura completa: [installazione, disinstallazione e recupero](docs/INSTALLATION.it.md).

## Uso rapido

- Nel Configurator impostare chiavi e autorizzazioni, mantenendo aule e PC esistenti.
- Nella Console selezionare le postazioni e scegliere controllo remoto,
  InternetGuard oppure Appello.
- In **Posizioni e computer → Profili classe–aula…** salvare e ricaricare le
  assegnazioni. Prima di salvare evidenziare un gruppo o uno dei suoi PC.
  Il caricamento chiede conferma dei PC riconosciuti; segnala quelli mancanti senza crearli.
  Prima terminare Lock, Demo e InternetGuard. Il profilo non modifica gruppi,
  indirizzi di rete o chiavi del Configurator.
- In **Trova client** disattivare il filtro dei PC noti per visualizzare anche
  quelli già configurati. Verificare i destinatari prima di importarli.
  Per importare occorre che la Console stessa possa scrivere la configurazione
  di sistema: il Configurator amministratore non eleva una Console già aperta.
- Se installato, da **Start → NetLink → NetLink Companion** scegliere ruolo e prova:
  controllo remoto dal docente al client, discovery diretto dal client al docente.

[Guida alle funzionalità e alla configurazione](docs/USAGE.it.md) ·
[Checklist di collaudo](docs/PILOT.it.md)

## Limiti operativi

- Usare reti fidate e account studenti senza privilegi amministrativi.
  La base Veyon 4.7.5 conserva il proprio modello di sicurezza: il TLS del
  Companion non aggiunge TLS generalizzato al controllo remoto.
- Il feedback WFP conferma i filtri, non il risultato di una prova di navigazione.
  Le destinazioni autorizzate restano raggiungibili su tutte le porte; proxy/VPN
  e altre policy richiedono verifiche. I filtri scadono dopo 90 secondi senza rinnovo,
  ma un blocco delle chiamate native di Windows può ritardare il recupero.
- La discovery non autentica i PC e la preferenza IPv4 usa annunci recenti,
  non test TCP o failover delle sessioni. Il multicast non attraversa da solo
  VLAN/subnet. Per InternetGuard usare IP instradabili o hostname risolvibili,
  non IPv6 link-local con `%numero`.
- Il blocco software non è una modalità chiosco inviolabile: le schermate
  protette di Windows restano accessibili. Le segnalazioni non provano quale
  combinazione speciale sia stata premuta.
- Profili JSON/CSV e rapporti completi Companion contengono dati personali:
  conservarli in cartelle protette e non pubblicarli.
- Interception e la DLL binaria DeskDupEngine non sono distribuiti.
  Prestazioni di classe, reti IPv6-only e interoperabilità vanno collaudate
  nell'ambiente di destinazione; l'audit non è completo.

[Architettura, protezioni e verifiche ancora necessarie](docs/AUDIT.it.md)

## Compilazione

Clonare con
`git clone --recurse-submodules https://github.com/lellomele/NetLink-Teacher-Control.git`.
La build Windows usa MSYS2 MinGW64, GCC, CMake, Ninja, Qt 5, QCA, OpenSSL,
OpenLDAP, LZO, libjpeg-turbo, libpng, zlib e NSIS. Questi strumenti servono
solo a chi compila, non agli utilizzatori dell'installer.

[Istruzioni di compilazione e distribuzione](docs/BUILD.it.md) ·
[Dipendenze e licenze](docs/DEPENDENCIES.it.md).
I sorgenti delle icone sono in `branding/`. Gli archivi sorgenti completi
delle release includono i submodule; gli ZIP automatici di GitHub non li includono.

## Copyright e licenza

NetLink: Copyright © 2025-26 prof. Ing. Raffaele Mele, per le modifiche del fork.
Veyon © Tobias Junghans, Veyon Solutions e contributori.
Non è una release ufficiale del progetto Veyon. Attribuzioni originali conservate.
Il codice Veyon è GPL-2.0-or-later; vedere [COPYING](COPYING) e le licenze incluse.
