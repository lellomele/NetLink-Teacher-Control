# NetLink

Fork di **Veyon 4.7.5** per aule Windows: visualizzazione e controllo dei PC,
InternetGuard basato su Windows Filtering Platform, Appello con il nome dichiarato
dallo studente sulle miniature e ricerca dei client tramite WS-Discovery 1.1.
Le funzioni sono integrate: non occorre installare un plugin esterno.

**Versione preliminare 0.1.0-alpha.4: destinata a PC di prova, non al laboratorio in uso.**

## Installazione

Scaricare l’installer x64 dalla [pagina delle release](https://github.com/lellomele/veyon-school/releases).
Installarlo come amministratore sul PC docente e sui client. Windows 10 a 64 bit è
il sistema di riferimento; le librerie necessarie sono incluse nell’installer.
Non servono Python, .NET aggiuntivo, DDS, Zenoh o un nuovo driver kernel.

Il programma usa la configurazione Veyon esistente e installa il servizio separato
`VeyonSchoolService`. Non eseguire contemporaneamente i due servizi: usano le stesse
porte. L’installazione originale, le chiavi e la configurazione non vengono eliminate.
L’installer salva la configurazione in una cartella Backups accessibile solo agli
amministratori. Se il servizio originale è presente, ne disabilita l’avvio automatico
con conferma e ripristina il tipo di avvio alla disinstallazione.
La configurazione è condivisa: il fork disabilita Interception e DeskDupEngine.
La disinstallazione non reimporta automaticamente il backup della configurazione;
per tornare alle impostazioni precedenti usare il Configurator originale.

Nel Configurator mantenere le aule, i PC, l’autenticazione e le regole di accesso
già configurati. È consigliata l’autenticazione a chiave con diritti limitati ai
docenti. Non distribuire la chiave privata ai client.

## Uso

- **Aspetto:** temi Sistema, Chiaro e Scuro; palette Oceano, Bosco, Prugna o colore
  personalizzato. Il pulsante Aspetto nel Master e il menu Aspetto nel Configurator
  salvano preferenze personali, senza elevazione o riavvio del servizio.
  Eventuali colori delle miniature già personalizzati nel Configurator restano prioritari.
- **InternetGuard:** selezionare i PC, aprire InternetGuard e scegliere Blocca,
  Sblocca o Verifica stato. La console mostra conferme, errori e richieste senza risposta.
  Il blocco è rinnovato ogni 20 secondi e scade dopo 90 secondi senza rinnovo.
- **Appello:** selezionare i PC e richiedere nome e cognome. I nomi sono visualizzati
  sulle miniature, ricercabili e ordinabili. Si può esportare il risultato in CSV
  oppure terminare l’appello e cancellare i nomi.
- **Trova client:** vedere i PC rilevati, verificarli e importare il PC selezionato.
  L’importazione richiede diritti di scrittura sulla configurazione e il riavvio
  del Master. Non modifica automaticamente le chiavi o le autorizzazioni.

## Configurazione aggiuntiva

### NetLink Companion

Il terzo programma della suite si apre da **Start → NetLink → Companion**.
Raccoglie localmente le informazioni utili per configurare la comunicazione:
Windows, account e gruppi del processo, schede/IP/prefissi, DNS/DHCP/gateway,
servizi, porte TCP, profili firewall, regole discovery del fork, impostazioni
NetLink e PC già inseriti nel Configurator. Delle chiavi private legge soltanto
nomi, percorsi e permessi, mai il contenuto; calcola l'impronta dei soli file pubblici.

Eseguirlo prima con lo stesso account e livello di privilegi del Master. Il pulsante
**Apri come amministratore** è facoltativo: i suoi risultati non provano i permessi
dell'utente docente non elevato. Companion non cambia rete, servizi, firewall,
chiavi o autorizzazioni; non scansiona automaticamente la rete.

Selezionare il ruolo docente/client. Per confrontare due PC, esportare su ciascuno
un rapporto JSON completo selezionando **Includi identificativi…**; aprire il rapporto
dell'altro PC con **Confronta con altro PC**. Senza questa opzione il rapporto è ridotto
e omette identificativi, indirizzi e chiavi. Il rapporto completo contiene dati
personali e della rete: conservarlo e condividerlo solo quando necessario.

**Verifica TCP** apre una sola connessione al nome/IP e alla porta scelti, per massimo
8 secondi. Non verifica autenticazione, multicast UDP o navigazione. Sul servizio
NetLink/Veyon questa prova può provocare una notifica di autenticazione incompleta.
Le impronte uguali attestano file pubblici uguali, non la validità della coppia
con la chiave privata. I rapporti non sono firmati e non vengono importati come
configurazione fidata. Routing, ACL, isolamento Wi-Fi e autenticazione vanno
confermati con una connessione reale dal Master.

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

Le destinazioni autorizzate restano raggiungibili su tutte le porte; proxy o VPN su
quelle destinazioni possono permettere l’accesso esterno. Non autorizzare intere
subnet senza motivo. Gli studenti non devono avere diritti amministrativi locali.
Un altro docente non può sostituire una policy attiva di una diversa connessione:
in caso di riconnessione può essere necessario aspettare la scadenza di 90 secondi.

Appello è un’autodichiarazione, non un’identificazione verificata. I nomi restano
nella memoria del Master finché termina l’appello o si chiude il programma; il CSV
contiene dati personali e va conservato soltanto quando necessario. Gli annunci
discovery non trasmettono i nomi degli studenti e non sono autenticati.

La comunicazione eredita il modello di sicurezza Veyon 4.7.5: non viene aggiunto un
canale TLS generalizzato. Non esporre i servizi a Internet o a reti non fidate.
Limiti: 256 client per operazione, 255 destinazioni totali, 128 risorse aggiuntive,
1.024 candidati discovery; le risposte SOAP sono limitate a 32 KiB e 64 record.

Non sono inclusi Interception né la DLL binaria DeskDupEngine. Si usa l’acquisizione
schermo software: verificare fluidità e carico CPU sui PC di destinazione. Il blocco
degli input non ha la stessa forza della modalità con driver Interception.
Interception è il filtro di tastiera/mouse che rafforza il blocco degli input,
incluse combinazioni speciali; non serve per rete, autenticazione o InternetGuard.
Le nuove funzioni richiedono il fork NetLink (già Veyon School) sui client.

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
