# Sviluppi pianificati di NetLink

## Companion e visibilità degli esiti TCP

Richiesta del 15 ottobre 2026. Implementato per alpha.5; resta il collaudo visivo
interattivo sui PC di destinazione.

Rendere immediatamente riconoscibile l'esito della prova di collegamento, in
particolare il messaggio di connessione TCP riuscita, attraverso uno sfondo
distinto o colori del testo coerenti con il tema selezionato.

Criteri di accettazione:

- Distinguere visivamente prova in corso, TCP raggiungibile, collegamento non
  riuscito e tempo scaduto.
- Accompagnare sempre il colore con un'etichetta testuale e un indicatore di
  stato, mantenendo un contrasto leggibile nei temi chiaro, scuro e ad alto
  contrasto. Non affidare il significato al solo colore.
- Mantenere ben visibili destinatario effettivo e porta, senza confondere la
  direzione del collegamento tra docente e client.
- Separare il successo TCP dall'autenticazione: un collegamento TCP riuscito
  deve continuare a indicare chiaramente che l'autenticazione non è verificata.
- Aggiornare o cancellare l'esito precedente all'avvio di una nuova prova.
- Verificare i messaggi e lo stile dei diversi esiti con test dei widget e
  un controllo visivo sui temi supportati.

Questa richiesta non prevede, da sola, modifiche automatiche a firewall,
servizi, rete o chiavi.

## Cartella delle applicazioni nel menu Start di Windows

Richiesta del 15 ottobre 2026. Cartella prevista: **NetLink**, salvo un diverso
nome indicato dall'utente. L'installer attuale prevede già questa cartella;
la corretta presenza dei collegamenti resta da verificare sui PC di prova.

Criteri di accettazione:

- Raggruppare Console docente, Configurator e Companion in un'unica cartella
  NetLink nel menu Start, con nomi chiari e le icone delle applicazioni.
- Verificare destinazioni, visibilità e assenza di duplicati dopo una nuova
  installazione e dopo l'aggiornamento da una alpha precedente.
- Rimuovere soltanto i collegamenti obsoleti appartenenti al fork, conservando
  quelli di eventuali installazioni Veyon originali o di altre applicazioni.
- Verificare che la disinstallazione rimuova i collegamenti di NetLink senza
  eliminare elementi estranei.

## Diagnosi e riparazioni assistite

Il canale temporaneo richiesto il 15 ottobre 2026 scambia solo rapporti con
abbinamento esplicito. I suggerimenti locali e il confronto dei due rapporti
devono distinguere osservazioni, ipotesi e verifiche ancora necessarie.

Le riparazioni automatiche restano fuori da questa alpha: richiedono una proposta
specifica, consenso locale, privilegi minimi, verifica dell'esito e ripristino.
Non aggiungere un servizio permanente o comandi remoti al canale diagnostico.
