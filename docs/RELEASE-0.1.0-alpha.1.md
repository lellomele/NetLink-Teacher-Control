# Veyon School 0.1.0-alpha.1 — Windows 10 x64

Prerelease per PC di prova, non destinata al laboratorio in uso. Installer non
firmato. Basata su Veyon 4.7.5; tutte le DLL del programma sono ricompilate insieme.

- InternetGuard WFP integrato, con conferme/errori e scadenza dopo 90 secondi senza
  rinnovo. La conferma indica filtri applicati, non navigazione Internet verificata.
- Appello con nome dichiarato dallo studente sulle miniature, ricerca, ordinamento
  ed esportazione CSV. Non identifica automaticamente chi usa l’account condiviso.
- WS-Discovery 1.1: multicast nella subnet e annunci HTTP al Master configurato
  per altre subnet. Importazione esplicita; l’autenticazione Veyon resta obbligatoria.

Le dipendenze di esecuzione sono incluse; nessun driver o plugin esterno richiesto.
Configurazione e chiavi Veyon vengono conservate. Il servizio originale non può
funzionare contemporaneamente al servizio del fork, perché usano le stesse porte.

Build Windows 10 x64 completata. 13 test Qt passati e sei messaggi discovery
validati con gli schemi ufficiali OASIS/W3C. Audit parziale: non sono ancora
conclusi i collaudi reali WFP, Appello, installazione/disinstallazione, resilienza,
prestazioni di classe, interoperabilità e discovery tra subnet. Il test WFP locale
senza elevazione ha restituito accesso negato, non un esito positivo.

Leggere [README](https://github.com/lellomele/veyon-school/blob/school-0.1.0-alpha.1/README.md),
[collaudo guidato](https://github.com/lellomele/veyon-school/blob/school-0.1.0-alpha.1/docs/PILOT.md) e
[audit/punti aperti](https://github.com/lellomele/veyon-school/blob/school-0.1.0-alpha.1/docs/AUDIT.md)
prima dell’installazione. Non distribuire sui client
le chiavi private del docente. Non esporre i servizi a Internet.

Asset: installer, manifest del contenuto, sorgenti completi dell’applicazione
con submodule, sorgenti delle dipendenze copyleft e SHA-256 dei file della release.
Gli archivi automatici GitHub non includono i submodule: usare l’asset source.
