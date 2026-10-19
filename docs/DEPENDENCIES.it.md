# Dipendenze e sorgenti corrispondenti

[English](DEPENDENCIES.md) · [Italiano](DEPENDENCIES.it.md)

NetLink (in precedenza Veyon School) usa la base applicativa Veyon 4.7.5:
`f24b48f5acea2a5728ba9f45c9d4df382d094b33` (GPL-2.0-or-later).
Copyright e licenze nei sorgenti ereditati sono conservati.

Repository e archivio sorgenti della release includono sorgenti applicativi,
script di compilazione/distribuzione e librerie interne fissate a revisioni precise.
Le correzioni UltraVNC sono applicate in modo riproducibile da
`patches-ultravnc.diff` durante la configurazione.
L'eseguibile è distribuito secondo GPL-3.0 quando richiesto dalla combinazione
delle dipendenze; i singoli sorgenti mantengono le concessioni GPL-2.0-or-later originali.
Non sono imposte restrizioni allo studio, modifica, ricompilazione o sostituzione
delle librerie collegate dinamicamente.

## Sorgenti interni

| Componente | Upstream | Revisione fissata |
|---|---|---|
| UltraVNC | https://github.com/veyon/ultravnc | 6b734d107f1e0cc0d7254028a54d9a2eac9d81f6 + patch |
| LibVNCServer/Client | https://github.com/veyon/libvncserver | 27c2b554d7c5a9258f8049f77c5531d61fc34ea9 + patch |
| KLdap | https://invent.kde.org/pim/kldap | 23abaa68c3c43770e86998af38b3c8b2ffde4667 |
| KLdap Qt compatibility | https://invent.kde.org/pim/kldap | 4974359470669f8a2fcb017a2cbc6974aa8e6f3a |
| Qt HTTP server | https://github.com/veyon/qthttpserver | f80feff79a3d698ca3f23cfa240bd763d87cf0ba + patch |

Qt HTTP server usa il proprio sottomodulo http-parser fissato.
L'albero dei sottomoduli Git registra tutte le revisioni esatte, comprese le
librerie solo Linux. Consultare COPYING/LICENSE di ogni componente per licenza e copyright.

L'integrazione Interception è disattivata in questo installer.
Non sono distribuiti DLL Interception, driver kernel o installer del driver.
Il sorgente opzionale resta fissato a `39eecbbc46a52e0402f783b872ef62b0254a896a`;
verificarne la licenza prima di abilitarlo. La DLL binaria legacy DeskDupEngine
non è distribuita.

## Runtime collegato dinamicamente

La build Windows x64 usa pacchetti MSYS2 MinGW64: sono dipendenze di compilazione,
NON software aggiuntivo richiesto agli utilizzatori. Le DLL runtime sono incluse.

| Componente | Versione pacchetto |
|---|---|
| Qt 5 base | 5.15.18+kde+r109-6 |
| QCA | 2.3.10-2 |
| OpenSSL | 3.6.2-2 |
| OpenLDAP | 2.6.12-1 |
| LZO | 2.10-3 |
| libjpeg-turbo | 3.1.4.1-3 |
| zlib | 1.3.2-2 |
| libpng | 1.6.58-1 |

Nomi e SHA-256 delle DLL risolte ricorsivamente sono in `package-manifest.json`.
Le licenze dei pacchetti accompagnano l'installer.
La release offre anche i sorgenti runtime di Qt, QCA, GLib, Graphite, libiconv,
gettext, LZO e GCC, con ricette/patch MSYS2 esatte.
Il manifesto fissa URL sorgenti, commit delle ricette e SHA-256.
Queste librerie non sono modificate: le modifiche applicative/vendor sono
nell'archivio sorgenti applicativo. Le altre librerie conservano le licenze permissive.

Ricette e patch MSYS2: https://github.com/msys2/MINGW-packages ;
URL e versioni dei sorgenti upstream sono nei rispettivi PKGBUILD.
Modifiche Qt: https://invent.kde.org/qt/qt/qtbase (branch 5.15);
QCA: https://invent.kde.org/libraries/qca ;
OpenSSL: https://github.com/openssl/openssl ;
OpenLDAP: https://git.openldap.org/openldap/openldap .

Questo inventario non certifica l'assenza di vulnerabilità. Alcune librerie ereditate
precedono le ultime release upstream; backport di sicurezza e verifiche
d'interoperabilità restano necessari prima della release (vedere AUDIT.md).
