# Installazione, aggiornamento e recupero

## Preparazione

Usare Windows 10 x64 e l'installer completo della
[pagina delle release](https://github.com/lellomele/NetLink-Teacher-Control/releases).
Confrontare lo SHA-256; le alpha non sono firmate.

Esportare il JSON dal Configurator e conservare separatamente le chiavi:
JSON e backup del registro non contengono la privata.
La privata deve restare sul docente con permessi limitati, la pubblica sui client.
Non collocare chiavi o documenti personali nella cartella del programma.

## Installazione e aggiornamento

1. Chiudere Console docente, Configurator e Companion.
2. Eseguire l'installer come amministratore sul docente e sui client.
   Non occorre disinstallare prima né rigenerare chiavi già funzionanti.
3. Il programma verifica i file prima della copia. Se sono occupati o vi sono
   rimozioni pendenti, chiudere l'installer, scegliere Riavvia in Windows e
   rieseguire il pacchetto completo. Nessuna DLL può essere ignorata.
4. Verificare versione, servizio, icona, configurazione, autenticazione e miniatura.

Gli eseguibili sono `netlink-master.exe`, `netlink-configurator.exe`,
`netlink-service.exe`, `netlink-server.exe`, `netlink-worker.exe`,
`netlink-cli.exe`, `netlink-wcli.exe` e `netlink-companion.exe`.
Non rinominarli manualmente. L'installer gestisce i vecchi `veyon-*.exe`
del fork; alcuni file caricati in altri processi richiedono il riavvio.

Restano invariati la cartella `C:\Program Files\Veyon School`, configurazione,
directory delle chiavi, DLL e identificativo `VeyonSchoolService`.
Le regole firewall della suite sono aggiornate dall'installer; script,
regole personalizzate o criteri basati sui vecchi percorsi vanno adeguati a parte.

## Coesistenza con Veyon originale

Il fork riutilizza la configurazione Veyon. Non avviare contemporaneamente i due
servizi: usano le stesse porte. Se rilevato, l'avvio automatico del servizio
originale viene disabilitato con conferma; il tipo di avvio è ripristinabile
alla disinstallazione. Il servizio originale non viene riavviato automaticamente.

Il fork disabilita Interception e DeskDupEngine nella configurazione condivisa.
Disinstallare non reimporta la configurazione precedente: per tornare
alle impostazioni originarie usare il Configurator originale e il proprio backup.

## Backup e riparazione

Il componente di manutenzione non dipende dal vecchio runtime Qt installato.
Prima dell'aggiornamento salva impostazioni del registro e backup preesistenti
in `C:\ProgramData\NetLink\Recovery`, protetto per amministratori e SYSTEM.
Può riparare un'installazione incompleta usando il pacchetto intero:
non copiare singole DLL e non avviare vecchi strumenti da un'installazione danneggiata.

File occupati richiedono riavvio; errori di permessi o sola lettura sono distinti.
I controlli non sono un aggiornamento transazionale: un file può essere riaperto
dopo la verifica e un errore successivo non dispone di rollback completo.
Conservare messaggio e backup, correggere la causa e rieseguire il pacchetto.

## Disinstallazione

Chiudere le applicazioni e usare la disinstallazione di Windows. Scegliere
se mantenere le impostazioni per reinstallare oppure eliminarle.

In entrambi i casi viene rimossa l'intera cartella `C:\Program Files\Veyon School`,
inclusi file aggiunti manualmente. I file in uso sono rimossi al riavvio:
se richiesto, riavviare prima di reinstallare.
Le chiavi esterne non vengono aperte né eliminate; se sono dentro la cartella
del programma, la disinstallazione si ferma per consentirne prima il trasferimento.

La pulizia delle impostazioni riguarda le voci gestite dalla suite nelle viste
registro 32/64 bit e nei profili caricati. Sono conservati:
percorsi delle chiavi, impostazioni condivise con Veyon originale rilevato,
profili non caricati, backup protetti, file personali esterni e dati gestiti da Windows.
Non cancellare genericamente tutte le voci che contengono “Veyon” o “NetLink”.

Dopo rimozione e riavvio verificare cartella, servizio, collegamenti Start e
regole firewall riferite al percorso del fork.
Per reinstallare da zero, installare e reimportare il JSON nel Configurator.
Dopo una rimozione con conservazione le impostazioni devono essere già presenti.

## Collaudo dopo l'installazione

Seguire [PILOT.md](PILOT.md) su postazioni dedicate con accesso fisico.
I test automatici dell'installer usano file e sottochiavi di prova e simulano
servizio/firewall: non sostituiscono installazione amministrativa, riavvio
e recupero nell'ambiente reale.
