# Veyon School 0.1.0-alpha.2 — Windows 10 x64

Prerelease per PC di prova, non destinata al laboratorio in uso. Installer non
firmato, basato su Veyon 4.7.5. Dipendenze incluse; nessun software aggiuntivo
richiesto sui PC docente e studenti.

Corregge la pagina **Veyon School** del Configurator:

- **Applica** si abilita modificando indirizzo del Master, WS-Discovery o risorse.
- Le impostazioni vengono salvate prima del riavvio del servizio e restano
  disponibili chiudendo e riaprendo il Configurator.
- Le risorse non valide o oltre il limite di 128 bloccano il salvataggio prima
  che siano modificate impostazioni, servizi o firewall.

Verifiche automatiche: 13 test di protocollo/input e 12 test della configurazione
grafica superati, inclusi salvataggio e riapertura su archivi isolati JSON e
LocalStore/QSettings. I sei messaggi discovery sono validati con gli schemi
ufficiali OASIS/W3C. Queste prove non sostituiscono il collaudo dell'aggiornamento
e dei servizi sui due PC Windows.

Chiudere Master e Configurator prima di aggiornare. Installare come amministratore
nella stessa cartella della alpha precedente su entrambi i PC di prova.
Configurazione e chiavi vengono conservate; non distribuirne la parte privata
sui client. Il servizio originale Veyon non deve funzionare contemporaneamente
a quello del fork.

La correzione del Configurator **non risolve automaticamente errori di
autenticazione**. La coppia di chiavi di Veyon 4.7.5 è riutilizzabile, purché
nome, parte pubblica e accessibilità della privata siano corretti. Se sul Master
ci sono più private, viene scelta la prima leggibile in ordine alfabetico,
salvo selezione esplicita con `VEYON_AUTH_KEY_NAME`.

Audit e collaudi WFP/Appello, resilienza, interoperabilità e discovery tra subnet
restano parziali. Vedere [README](https://github.com/lellomele/veyon-school/blob/school-0.1.0-alpha.2/README.md),
[collaudo guidato](https://github.com/lellomele/veyon-school/blob/school-0.1.0-alpha.2/docs/PILOT.md)
e [limiti dell'audit](https://github.com/lellomele/veyon-school/blob/school-0.1.0-alpha.2/docs/AUDIT.md).

Asset: installer, manifest del contenuto, sorgenti completi con submodule,
sorgenti delle dipendenze copyleft e SHA-256. Usare l'asset source:
gli archivi automatici GitHub non includono i submodule.
