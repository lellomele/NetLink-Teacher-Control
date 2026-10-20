# Compatibilità del runtime Qt / KDE

[English](QT-RUNTIME.md) · [Italiano](QT-RUNTIME.it.md)

## Versioni esatte

| Componente | Runtime precedente | Runtime attuale |
|---|---|---|
| Qt base MSYS2 MinGW64 | 5.15.18+kde+r109-6 | 5.15.19+kde+r96-1 |
| Revisione KDE qtbase | `bebdfd54917e25d1c100e6bd9f5dd53c2e645fd8` | `fbed962c3195ab3952fa54d40e012ad1a4fdc42b` |
| Tag base upstream | `v5.15.18-lts-lgpl` | `v5.15.19-lts-lgpl` |
| Revisione ricetta MSYS2 | `82bcf47d0ae2621e1cd00fb82c8ee5db02b91955` | `a114954945ef1da8b3fbb12adcbac08f23628c1d` |

I 96 commit sopra il nuovo tag upstream sono equivalenti come patch a 96 commit
della precedente serie KDE (`git range-diff`). Gli altri 13 vecchi commit KDE
sono nella nuova base upstream, non sono stati persi. Dodici hanno correzioni
upstream corrispondenti per D-Bus, tooltip, finestre file native, documentazione,
maiuscole negli header generati, animazioni dei campi testo, lunghezze ODBC,
validazione RandR, codifica QDom, percorsi temporanei Windows, data URL e input X11.
Undici hanno lo stesso identificativo stabile di patch. La patch data URL include
un test upstream aggiuntivo; il sorgente di produzione è identico nelle due revisioni KDE.
Il tredicesimo rinomina una variabile del parser XML: `qxmlstream.g` e
`qxmlstream_p.h` hanno gli stessi identificativi Git nelle revisioni KDE vecchia,
nuova e nella nuova base. Le 29 patch MSYS2 sono identiche byte per byte;
opzioni di compilazione e dipendenze sono invariate. La ricetta cambia soltanto
versione Qt, revisione pacchetto, revisione sorgente e checksum della sorgente Git.

Il vecchio pacchetto includeva già i backport KDE per CVE-2025-30348/QDom e
GetTempPath2 di Windows: non sono protezioni introdotte soltanto ora.
Qt 5.15.19 include ulteriori correzioni upstream di robustezza; gli aggiornamenti
delle librerie interne Qt non incidono necessariamente su questa build, che
collega dinamicamente diverse librerie esterne. Vedere le
[note ufficiali Qt](https://code.qt.io/cgit/qt/qtreleasenotes.git/tree/qt/5.15.19/release-note.md).

## Confini della compatibilità

Tutti i moduli NetLink e le librerie interne sono ricompilati insieme con gli
stessi header e librerie di importazione 5.15.19. L'integrazione QPA Windows usa
gli header privati corrispondenti. I plugin Qt Windows/JPEG provengono dallo
stesso pacchetto; la distribuzione rifiuta versioni Qt miste.
Compilatore, QCA/OpenSSL e gli altri runtime non Qt restano invariati.
Algoritmo delle chiavi (RSA/SHA-512), versione dei messaggi (`Qt_5_5`),
identificativi delle funzionalità e del servizio, percorso d'installazione,
impostazioni e percorsi delle chiavi non cambiano. Non copiare singole DLL in
un'installazione precedente.

Le prove comprendono firma RSA pubblica e messaggi del vecchio runtime,
rifiuto delle firme errate, PEM/firma/OAEP, trasporto asincrono, TLS diagnostico,
ciclo discovery, Appello/profili, controlli grafici alle tre scale e installer
isolato con aggiornamento/disinstallazione/reinstallazione. I test isolati
simulano servizio e firewall: non dimostrano un ciclo privilegiato con riavvio.
Tray nativa, controllo remoto, WFP e interoperabilità in aula richiedono ancora
il [collaudo guidato Windows](PILOT.it.md). Non è una certificazione completa di sicurezza.

Per riprodurre il confronto KDE in un repository qtbase contenente entrambe
le revisioni e i tag:

```
git range-diff --no-patch v5.15.18-lts-lgpl..bebdfd54917e25d1c100e6bd9f5dd53c2e645fd8 v5.15.19-lts-lgpl..fbed962c3195ab3952fa54d40e012ad1a4fdc42b
```

Archivi sorgenti, checksum dei pacchetti e delle ricette e versioni esatte sono
registrati in `packaging/runtime-source-lock.json` e nel manifesto dei sorgenti
runtime allegato alla release.
