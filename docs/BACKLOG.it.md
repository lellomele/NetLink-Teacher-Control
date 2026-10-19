# Consolidamento e sviluppi futuri

[English](BACKLOG.md) · [Italiano](BACKLOG.it.md)

Le funzionalità disponibili sono descritte nel [README](../README.it.md)
e nella [guida d'uso](USAGE.it.md). Questo elenco riguarda soltanto attività
ancora da valutare o completare, non la cronologia delle versioni.

## Priorità di consolidamento

- Collaudo prolungato con più client, misurando latenza, CPU e memoria.
- Recupero WFP con connessioni preesistenti, perdita rete, riavvii,
  IPv6/QUIC, VPN e policy firewall/antivirus dell'ambiente di destinazione.
- Revisione dei percorsi ereditati con arresto forzato dei thread,
  DNS bloccante e attese native lunghe.
- Verifica aggiornata delle dipendenze e chiusura delle lacune di analisi.
- Installazione, aggiornamento, file in uso, riavvio e recupero su PC dedicati.
- Validazione fisica di DPI misti, accessibilità, input e icona di servizio.

## Scenari da validare separatamente

- Discovery tra subnet/VLAN e ACL reali.
- Reti solo IPv6 e variazioni degli indici delle schede link-local.
- Interoperabilità delle funzioni comuni con client Veyon originali.
- Raccolta Companion con account standard, amministratore alternativo,
  dominio/RDP, più schede e criteri scolastici.

## Evoluzioni non incluse

Il canale Companion scambia rapporti diagnostici con abbinamento esplicito.
Eventuali riparazioni assistite richiedono una proposta specifica, consenso
locale, privilegi minimi, verifica dell'esito e ripristino. Non aggiungere
comandi remoti o listener permanenti al canale diagnostico senza tale progetto.

Nuovi driver di input, accelerazione della cattura o ampliamenti del protocollo
richiedono valutazione separata; non sono prerequisiti dell'installer attuale.

[Checklist di collaudo](PILOT.it.md) · [Protezioni e criteri beta](AUDIT.it.md)
