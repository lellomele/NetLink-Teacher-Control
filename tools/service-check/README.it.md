[Italiano](README.it.md) | [English](README.md)

# NetLink · Controllo servizio

Utilità portatile Windows x64 per analizzare il servizio locale NetLink, correggerne il tipo di avvio e avviarlo su richiesta. Versione 1.1.0, interfaccia italiana. Non serve installare Qt.

## Uso

1. Avvia `NetLink-Service-Check.exe` e conferma la richiesta di amministratore. La prima analisi è in sola lettura.
2. Leggi le righe colorate: verde = controllo positivo/informazione, giallo = attenzione, rosso = problema o correzioni bloccate. Passa il mouse o fai doppio clic su una riga per l'indicazione completa.
3. Se necessario scegli **Automatico senza ritardo** oppure **Automatico ritardato**, poi **Imposta avvio…**. La conferma riepiloga ogni modifica. Il recupero dopo errore è facoltativo e viene impostato solo se spuntato.
4. **Avvia servizio…** avvia il servizio adesso, senza cambiare il suo tipo di avvio. Non è equivalente a correggere l'avvio al prossimo riavvio.
5. **Salva rapporto…** esporta testo o JSON da confrontare fra prima/dopo la correzione e dopo un riavvio. Non serve riavviare il PC per usare l'utilità; il riavvio necessario alla prova va eseguito dall'utente, dopo aver salvato il lavoro.

L'aggiornamento ogni 15 secondi è facoltativo e in sola lettura: non è un servizio residente e non ripara nulla in background.

**Controllo dopo il boot:** scegli 3 minuti, 5 minuti o Disattivato, quindi **Imposta controllo…**.
Questo non ritarda il normale avvio Windows e non assegna priorità ad altri servizi.
Installa `netlink-startup-check.exe` in `Program Files\NetLink Service Tools` e registra
l'attività **NetLink Service Startup Check**, eseguita come SYSTEM senza utente connesso.
Se NetLink è attivo non fa nulla; se è fermo e Automatico chiede a Windows di avviarlo.
Riprova al massimo tre volte, con pause di due minuti dopo gli errori. Rispetta Manuale,
Disabilitato e manutenzione. Windows può eseguirlo più tardi su un PC molto occupato.
Funziona anche con installazioni precedenti ad alpha.38 nel percorso standard
`Program Files\Veyon School`; non sostituisce il servizio NetLink e non corregge da solo
i difetti del vecchio eseguibile. I rapporti sono in `C:\ProgramData\NetLink\StartupCheck`.
La scelta Disattivato rimuove l'attività ma conserva il piccolo controllo nativo protetto.

## Come interpretare il mancato avvio

- **Manuale o disabilitato:** il servizio non è configurato per partire normalmente al boot. Il codice 1077 segnala che Windows non ne ha tentato l'avvio dopo l'ultimo riavvio.
- **Automatico ma arrestato:** non basta riscrivere Automatico. Controlla gli eventi: timeout, dipendenze, accesso dell'account, eseguibile mancante o arresto successivo all'avvio sono casi diversi.
- **Parte dal Configurator ma non al boot:** l'avvio manuale riuscito non identifica da solo la causa. In caso di timeout un avvio ritardato può essere più adatto di uno anticipato; prova su quel PC e verifica dopo un riavvio.
- **Torna su Manuale dopo una correzione:** salva due rapporti e controlla gli eventi 7040. Un programma o un criterio può cambiare il tipo di avvio, ma l'utilità non ne identifica con certezza l'autore.

Windows ordina i servizi usando gruppi e dipendenze. L'app **non cambia l'ordine globale, la priorità CPU, servizi Windows non essenziali, account o dipendenze**. Senza ritardo evita la coda dei servizi ritardati, non garantisce precedenza sugli altri servizi automatici. Non imposta mai NetLink come driver di boot né aumenta il timeout globale di Windows.

## Protezioni e limiti

Le correzioni riguardano soltanto `VeyonSchoolService`, nome tecnico conservato da NetLink. Sono consentite solo con eseguibile NetLink riconosciuto, presente in `Program Files\Veyon School` o `Program Files\NetLink School`, senza collegamenti nel percorso. Un'installazione non standard resta diagnosticabile, ma non viene modificata. I metadati dell'eseguibile non sostituiscono una verifica della firma digitale.

Prima di modificare o avviare viene salvata una copia JSON dei parametri del servizio in `%LOCALAPPDATA%\NetLink\ServiceCheck\Backups`, con accesso limitato all'utente, agli amministratori e a SYSTEM. Se il backup fallisce, l'azione non parte. La copia serve come riferimento: non viene reimportata automaticamente. Le modifiche SCM non sono una transazione unica; se una fase fallisce, il rapporto indica l'errore e lo stato viene riletto, senza annullamenti alla cieca.

Il profilo di recupero opzionale riprova dopo 10, 30 e 60 secondi, poi non riprova ulteriormente fino all'azzeramento del conteggio (24 ore senza errori). Vale anche per arresti con errore dichiarato. Non riavvia il computer e non esegue programmi esterni. Un arresto volontario senza errori non viene trattato come un guasto.

Gli eventi Sistema vengono letti negli ultimi 7 giorni, entro un limite di 1.500 eventi SCM e 30 corrispondenze. Sono evidenze storiche, non prova della causa attuale. Se il registro è assente, incompleto o non leggibile, l'app lo segnala. Il controllo non dimostra il funzionamento di video, discovery o accesso remoto.

Configurazione NetLink, chiavi di autenticazione e firewall non vengono modificati. Non si crea un servizio mancante e non si chiudono processi. Non vengono aperte porte aggiuntive. I rapporti non contengono chiavi o password, ma possono contenere nomi del PC, account e percorsi: controllali prima di condividerli.

## Requisiti e compilazione

Windows 10/11 x64, .NET Framework 4.8 e autorizzazione amministratore. Il file è portatile e non firmato digitalmente. Per compilare: esegui `build.ps1` da PowerShell; il controllo nativo usa MSYS2 MinGW-w64 (`C:/msys64/mingw64`), la GUI il compilatore .NET Framework di Windows. In alternativa passa `-NativeHelper` indicando il controllo corrispondente compilato dai sorgenti della suite. I test isolati usano un servizio simulato. I sorgenti dell'icona sono in `branding`, con SVG, PNG e ICO.

Riferimenti: [ordine di avvio Microsoft](https://learn.microsoft.com/en-us/windows/win32/services/automatically-starting-services), [avvio ritardato](https://learn.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_delayed_auto_start_info), [recupero dopo errore](https://learn.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_failure_actionsw).

© 2026 prof. Ing. Raffaele Mele. GPL-2.0-or-later; vedi `LICENSE.txt`.
