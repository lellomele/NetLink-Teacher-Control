Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"
!include "nsDialogs.nsh"
!include "${REPOROOT}\packaging\NetLinkUpgrade.nsh"
Var PurgeSettings
Var MaintenanceMutex
Var InstallationRole
Var RoleDialog
Var MasterRadio
Var ClientRadio
Var StartMenuDir
!ifdef NETLINK_INSTALLER_FIXTURE
 !define APPREGROOT HKCU
 !define APPREGKEY "${FIXTURE_REGKEY}\Uninstall"
 !define MAINTARGS ' "$INSTDIR"'
 !define LOCKNAME "Local\NetLinkFixture-${FIXTURE_ID}"
 !define NETLINK_UPGRADE_FIXTURE
 RequestExecutionLevel user
!else
 !define APPREGROOT HKLM
 !define APPREGKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\VeyonSchool"
 !define MAINTARGS ""
 !define LOCKNAME "Global\NetLinkInstallerMaintenance"
 RequestExecutionLevel admin
!endif
Name "NetLink 1.0.0 alpha.23"
OutFile "${OUTPUT}\NetLink-1.0.0-alpha.23-win64-setup.exe"
InstallDir "$PROGRAMFILES64\Veyon School"
SetCompressor /SOLID lzma
AllowSkipFiles off
!define MUI_ICON "${REPOROOT}\branding\netlink.ico"
!define MUI_UNICON "${REPOROOT}\branding\netlink.ico"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${REPOROOT}\COPYING.GPL3"
Page custom RolePageCreate RolePageLeave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Italian"
VIProductVersion "1.0.0.23"
VIAddVersionKey /LANG=1040 "ProductName" "NetLink"
VIAddVersionKey /LANG=1040 "FileDescription" "NetLink installer (pre-release)"
VIAddVersionKey /LANG=1040 "FileVersion" "1.0.0-alpha.23"
VIAddVersionKey /LANG=1040 "LegalCopyright" "Copyright (c) 2025-26 prof. Ing. Raffaele Mele; Veyon e contributori"
Function .onInit
 System::Call 'kernel32::CreateMutexW(p 0, i 0, w "${LOCKNAME}") p .r0 ?e'
 Pop $1
 StrCpy $MaintenanceMutex $0
 ${If} $1 == 183
 ${OrIf} $0 == 0
   Abort "Un'altra installazione o disinstallazione di NetLink è già in corso."
 ${EndIf}
 ${IfNot} ${RunningX64}
   MessageBox MB_ICONSTOP "Questa build richiede Windows a 64 bit."
   Abort
 ${EndIf}
 SetRegView 64
 ; Retain the installed role. Older full installations are Masters; a fresh
 ; installation defaults to Client so the teacher UI is not deployed by mistake.
 ReadRegStr $InstallationRole ${APPREGROOT} "${APPREGKEY}" "InstallationRole"
 ${If} $InstallationRole != "Master"
 ${AndIf} $InstallationRole != "Client"
   StrCpy $InstallationRole "Client"
   !ifdef NETLINK_INSTALLER_FIXTURE
     StrCpy $R2 "${FIXTURE_ROOT}\NetLinkFixtureApp"
   !else
     StrCpy $R2 "$PROGRAMFILES64\Veyon School"
   !endif
   ${If} ${FileExists} "$R2\netlink-master.exe"
   ${OrIf} ${FileExists} "$R2\veyon-master.exe"
     StrCpy $InstallationRole "Master"
   ${EndIf}
 ${EndIf}
 ${GetParameters} $R0
 ClearErrors
 ${GetOptions} $R0 "/Role=" $R1
 ${IfNot} ${Errors}
   ${If} $R1 == "Master"
     StrCpy $InstallationRole "Master"
   ${ElseIf} $R1 == "Client"
     StrCpy $InstallationRole "Client"
   ${Else}
     SetErrorLevel 2
     IfSilent +2
     MessageBox MB_OK|MB_ICONSTOP "Ruolo non valido. Usare /Role=Master oppure /Role=Client. Nessun file o impostazione è stato modificato."
     Quit
   ${EndIf}
 ${EndIf}
 ; Fixed protected destination: student accounts must not be able to replace SYSTEM executables.
 !ifdef NETLINK_INSTALLER_FIXTURE
 StrCpy $INSTDIR "${FIXTURE_ROOT}\NetLinkFixtureApp"
 !else
 StrCpy $INSTDIR "$PROGRAMFILES64\Veyon School"
 ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\Services\VeyonService" "ImagePath"
 ${If} $0 != ""
   IfSilent refuse_legacy
   MessageBox MB_YESNO|MB_ICONEXCLAMATION "Veyon originale è presente. Fermare il suo servizio e disabilitarne l’avvio automatico per evitare conflitti? Installazione e configurazione saranno conservate; il tipo di avvio potrà essere ripristinato disinstallando NetLink. Versione preliminare: usare PC di prova." IDYES stop_legacy
   refuse_legacy:
   Abort "Il servizio Veyon originale deve essere fermato prima dell’installazione."
   stop_legacy:
   ReadRegDWORD $1 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VeyonSchool" "OriginalServiceStart"
   ${If} ${Errors}
     ReadRegDWORD $1 HKLM "SYSTEM\CurrentControlSet\Services\VeyonService" "Start"
     WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VeyonSchool" "OriginalServiceStart" $1
     ReadRegStr $1 HKLM "SYSTEM\CurrentControlSet\Services\VeyonService" "ImagePath"
     WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VeyonSchool" "OriginalServiceImage" $1
   ${EndIf}
   ExecWait '"$SYSDIR\sc.exe" stop VeyonService' $0
   ExecWait '"$SYSDIR\sc.exe" config VeyonService start= disabled' $0
   ${If} $0 != 0
     Abort "Impossibile disabilitare il servizio originale: installazione interrotta."
   ${EndIf}
 ${EndIf}
 !endif
FunctionEnd
Function RolePageCreate
 !insertmacro MUI_HEADER_TEXT "Tipo di installazione" "Scegli il ruolo di questo PC: Master oppure Client."
 nsDialogs::Create 1018
 Pop $RoleDialog
 ${If} $RoleDialog == error
   Abort
 ${EndIf}
 ${NSD_CreateLabel} 0 0 100% 24u "Entrambe le opzioni includono Configurator, Companion e il servizio NetLink. Configurazione e chiavi esistenti vengono conservate."
 Pop $0
 ${NSD_CreateRadioButton} 0 34u 100% 14u "Master"
 Pop $MasterRadio
 ${NSD_CreateLabel} 12u 52u 94% 24u "Include anche la Console docente, per visualizzare e controllare i client."
 Pop $0
 ${NSD_CreateRadioButton} 0 86u 100% 14u "Client"
 Pop $ClientRadio
 ${NSD_CreateLabel} 12u 104u 94% 36u "Non installa la Console docente. Se già presente, la rimuove insieme al suo collegamento. Il PC resta controllabile dal Master."
 Pop $0
 ${If} $InstallationRole == "Master"
   ${NSD_Check} $MasterRadio
 ${Else}
   ${NSD_Check} $ClientRadio
 ${EndIf}
 nsDialogs::Show
FunctionEnd
Function RolePageLeave
 ${NSD_GetState} $MasterRadio $0
 ${If} $0 == ${BST_CHECKED}
   StrCpy $InstallationRole "Master"
 ${Else}
   StrCpy $InstallationRole "Client"
 ${EndIf}
FunctionEnd
Function un.onInit
 System::Call 'kernel32::CreateMutexW(p 0, i 0, w "${LOCKNAME}") p .r0 ?e'
 Pop $1
 StrCpy $MaintenanceMutex $0
 ${If} $1 == 183
 ${OrIf} $0 == 0
   Abort "Un'altra installazione o disinstallazione di NetLink è già in corso."
 ${EndIf}
 !ifdef NETLINK_INSTALLER_FIXTURE
 StrCpy $INSTDIR "${FIXTURE_ROOT}\NetLinkFixtureApp"
 !else
 StrCpy $INSTDIR "$PROGRAMFILES64\Veyon School"
 !endif
 StrCpy $PurgeSettings 0
 ${GetParameters} $R0
 ClearErrors
 ${GetOptions} $R0 "/PURGE" $R1
 ${IfNot} ${Errors}
   StrCpy $PurgeSettings 1
 ${EndIf}
 IfSilent un_choice_done
 MessageBox MB_YESNOCANCEL|MB_DEFBUTTON2|MB_ICONQUESTION "Vuoi cancellare anche le impostazioni di NetLink?$\r$\n$\r$\nSÌ = disinstalla NetLink e cancella le sue impostazioni. Alla reinstallazione dovrai riconfigurarlo o importare il tuo backup.$\r$\n$\r$\nNO = disinstalla NetLink ma conserva le impostazioni, così puoi reinstallarlo senza riconfigurarlo. Consigliato per una reinstallazione.$\r$\n$\r$\nIn ENTRAMBI i casi il programma e la sua cartella vengono rimossi; le chiavi di autenticazione e i loro percorsi sono conservati.$\r$\nLe impostazioni condivise con Veyon originale, se presente, non vengono cancellate.$\r$\n$\r$\nANNULLA = non disinstallare." IDYES un_purge IDNO un_choice_done
 Abort
 un_purge:
 StrCpy $PurgeSettings 1
 un_choice_done:
FunctionEnd
!macro RunMaintenance ACTION MESSAGE
 ClearErrors
 nsExec::ExecToLog /TIMEOUT=45000 '"$PLUGINSDIR\netlink-maintenance.exe" ${ACTION}${MAINTARGS}'
 Pop $0
 ${If} ${Errors}
 ${OrIf} $0 != 0
   Abort "${MESSAGE} (codice $0). Dettagli: C:\ProgramData\NetLink\maintenance-error.txt"
 ${EndIf}
!macroend
!macro CheckInstallerReady ACTION EXTRA
 ClearErrors
 nsExec::ExecToLog /TIMEOUT=45000 '"$PLUGINSDIR\netlink-maintenance.exe" ${ACTION}${MAINTARGS}${EXTRA}'
 Pop $0
 ${If} $0 == 3010
   DetailPrint "Riavvio necessario prima della copia dei file. Configurazione e chiavi conservate."
   SetRebootFlag true
   SetErrorLevel 3010
   IfSilent +2
   MessageBox MB_OK|MB_ICONEXCLAMATION "Riavvio necessario.$\r$\n$\r$\nAlcuni file di NetLink sono ancora in uso oppure attendono la rimozione al riavvio.$\r$\nNessun file del programma è stato sovrascritto. Configurazione e chiavi NetLink sono conservate.$\r$\n$\r$\nChiudi questo installer, scegli Riavvia in Windows e poi esegui nuovamente l'installer completo.$\r$\nNon cancellare DLL e non rigenerare le chiavi."
   Quit
 ${ElseIf} ${Errors}
 ${OrIf} $0 != 0
   DetailPrint "Controllo preventivo non riuscito (codice $0). Copia non iniziata."
   SetErrorLevel 1
   IfSilent +2
   MessageBox MB_OK|MB_ICONSTOP "Impossibile verificare i file da sostituire (codice $0).$\r$\nLa copia non è iniziata.$\r$\n$\r$\nChiudi le applicazioni NetLink e verifica le autorizzazioni. Dettagli: C:\ProgramData\NetLink\maintenance-error.txt"
   Quit
 ${EndIf}
!macroend
Section "NetLink"
 SetShellVarContext all
 ; Never execute a leftover Qt CLI before restoring its runtime. This helper
 ; is statically linked and uses only Windows APIs, from the installer temp dir.
 InitPluginsDir
 SetOutPath "$PLUGINSDIR"
 File /oname=netlink-maintenance.exe "${PAYLOAD}\netlink-maintenance.exe"
 File /oname=replacement.sha256 "${PAYLOAD}\payload.sha256"
 !insertmacro CheckInstallerReady "check-pending" ""
 !insertmacro RunMaintenance "backup" "Backup delle impostazioni non riuscito; nessun file è stato sostituito"
 !insertmacro RunMaintenance "quiesce" "Impossibile arrestare il servizio per verificare i file; chiudere le applicazioni e riavviare"
 !insertmacro CheckInstallerReady "check-files" ' "$PLUGINSDIR\replacement.sha256"'
 !insertmacro RunMaintenance "stop" "Impossibile arrestare/rimuovere il servizio precedente; chiudere le applicazioni e riavviare"
 ; The full replacement plan also probes the Master EXE before removing it.
 ; Do not defer this removal to reboot: a Client install must not report success
 ; while leaving an executable teacher console behind.
 ${If} $InstallationRole == "Client"
   ClearErrors
   Delete "$INSTDIR\netlink-master.exe"
   Delete "$INSTDIR\veyon-master.exe"
   ${If} ${Errors}
     Abort "Impossibile rimuovere la Console docente. Chiudi NetLink, riavvia e ripeti l'installer scegliendo Client. Configurazione e chiavi sono conservate."
   ${EndIf}
 ${EndIf}
 SetOutPath "$INSTDIR"
 ClearErrors
 File /r /x netlink-master.exe /x payload.sha256 /x payload-client.sha256 "${PAYLOAD}\*"
 ${If} $InstallationRole == "Master"
   File "${PAYLOAD}\netlink-master.exe"
   File /oname=payload.sha256 "${PAYLOAD}\payload.sha256"
 ${Else}
   File /oname=payload.sha256 "${PAYLOAD}\payload-client.sha256"
 ${EndIf}
 ${If} ${Errors}
   Abort "Copia dei file incompleta. Chiudere le applicazioni, riavviare Windows e ripetere l'installer completo; non sostituire singole DLL."
 ${EndIf}
 ; Keep recovery available even if later configuration or service startup fails.
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 WriteRegStr ${APPREGROOT} "${APPREGKEY}" "DisplayName" "NetLink (installazione incompleta)"
 WriteRegStr ${APPREGROOT} "${APPREGKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
 !insertmacro RunMaintenance "verify" "File del programma mancanti o alterati; ripetere l'installer completo"
 ClearErrors
 ExecWait '"$INSTDIR\netlink-wcli.exe" service register' $0
 ${If} ${Errors}
 ${OrIf} $0 != 0
   MessageBox MB_ICONSTOP "Registrazione servizio non riuscita. Installazione incompleta." /SD IDOK
   Abort
 ${EndIf}
 ClearErrors
 ExecWait '"$INSTDIR\netlink-wcli.exe" config set Windows/UseInterceptionDriver false' $0
 ${If} ${Errors}
 ${OrIf} $0 != 0
   MessageBox MB_ICONSTOP "Configurazione/firewall non riusciti. Verifica nel Configurator." /SD IDOK
   Abort
 ${EndIf}
 ClearErrors
 ExecWait '"$INSTDIR\netlink-wcli.exe" config set UltraVNC/DeskDupEngine false' $0
 ${If} ${Errors}
 ${OrIf} $0 != 0
   MessageBox MB_ICONSTOP "Configurazione acquisizione schermo non riuscita." /SD IDOK
   Abort
 ${EndIf}
 ${GetParameters} $R0
 ${GetOptions} $R0 "/Master=" $R1
 ${If} $R1 != ""
   ClearErrors
   ExecWait '"$INSTDIR\netlink-wcli.exe" config set School/DiscoveryMaster "$R1"' $0
   ${If} ${Errors}
   ${OrIf} $0 != 0
     MessageBox MB_ICONSTOP "Nome del Master non configurato." /SD IDOK
     Abort
   ${EndIf}
 ${EndIf}
 ClearErrors
 ExecWait '"$INSTDIR\netlink-wcli.exe" service start' $0
 ${If} ${Errors}
 ${OrIf} $0 != 0
   MessageBox MB_ICONSTOP "Avvio servizio non riuscito. Verifica nel Configurator." /SD IDOK
   Abort
 ${EndIf}
 ; New registration/start succeeded. Retire only the seven known old filenames;
 ; an in-use legacy EXE is removed on reboot, never by killing a user process.
 !insertmacro NetLinkRetireLegacyExecutables
 ; Preserve installation path/service identity for existing settings and keys.
 !ifdef NETLINK_INSTALLER_FIXTURE
 StrCpy $StartMenuDir "$INSTDIR\StartMenu"
 !else
 StrCpy $StartMenuDir "$SMPROGRAMS\NetLink"
 !endif
 ClearErrors
 CreateDirectory "$StartMenuDir"
 ${If} $InstallationRole == "Master"
   CreateShortcut "$StartMenuDir\Console docente.lnk" "$INSTDIR\netlink-master.exe"
 ${Else}
   Delete "$StartMenuDir\Console docente.lnk"
 ${EndIf}
 CreateShortcut "$StartMenuDir\Configurator.lnk" "$INSTDIR\netlink-configurator.exe"
 CreateShortcut "$StartMenuDir\Companion.lnk" "$INSTDIR\netlink-companion.exe"
 ${If} ${Errors}
   Abort "Aggiornamento collegamenti Start non riuscito. Ripetere l'installer completo."
 ${EndIf}
 !ifndef NETLINK_INSTALLER_FIXTURE
 Delete "$SMPROGRAMS\Veyon School\Master.lnk"
 Delete "$SMPROGRAMS\Veyon School\Configurator.lnk"
 RMDir "$SMPROGRAMS\Veyon School"
 !endif
 WriteRegStr ${APPREGROOT} "${APPREGKEY}" "DisplayName" "NetLink 1.0.0 alpha.23"
 WriteRegStr ${APPREGROOT} "${APPREGKEY}" "DisplayVersion" "1.0.0-alpha.23"
 WriteRegStr ${APPREGROOT} "${APPREGKEY}" "InstallationRole" "$InstallationRole"
 WriteRegStr ${APPREGROOT} "${APPREGKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
 WriteRegStr ${APPREGROOT} "${APPREGKEY}" "DisplayIcon" "$INSTDIR\netlink-configurator.exe"
SectionEnd
Section "Uninstall"
 SetRegView 64
 SetShellVarContext all
 InitPluginsDir
 SetOutPath "$PLUGINSDIR"
 ClearErrors
 CopyFiles /SILENT "$INSTDIR\netlink-maintenance.exe" "$PLUGINSDIR\netlink-maintenance.exe"
 ${If} ${Errors}
   Abort "Strumento di rimozione assente: ripristinare con l'installer completo prima di disinstallare."
 ${EndIf}
 !insertmacro RunMaintenance "check-keys" "Chiavi presenti nella cartella del programma: spostarle prima con il Configurator; nessuna chiave è stata cancellata"
 !insertmacro RunMaintenance "backup" "Backup di recupero non riuscito; disinstallazione interrotta"
 !insertmacro RunMaintenance "stop" "Servizio non arrestabile; chiudere le applicazioni e riprovare dopo il riavvio"
 !insertmacro RunMaintenance "clean-firewall" "Rimozione regole firewall del fork non riuscita"
 !ifndef NETLINK_INSTALLER_FIXTURE
 Delete "$SMPROGRAMS\Veyon School\Master.lnk"
 Delete "$SMPROGRAMS\Veyon School\Configurator.lnk"
 Delete "$SMPROGRAMS\NetLink\Console docente.lnk"
 Delete "$SMPROGRAMS\NetLink\Configurator.lnk"
 Delete "$SMPROGRAMS\NetLink\Companion.lnk"
 RMDir "$SMPROGRAMS\NetLink"
 RMDir "$SMPROGRAMS\Veyon School"
 ReadRegStr $1 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VeyonSchool" "OriginalServiceImage"
 ReadRegStr $2 HKLM "SYSTEM\CurrentControlSet\Services\VeyonService" "ImagePath"
 ${If} $1 != ""
 ${AndIf} $1 == $2
   ReadRegDWORD $1 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VeyonSchool" "OriginalServiceStart"
   ${If} $1 == 2
     ExecWait '"$SYSDIR\sc.exe" config VeyonService start= auto' $0
   ${ElseIf} $1 == 3
     ExecWait '"$SYSDIR\sc.exe" config VeyonService start= demand' $0
   ${EndIf}
 ${EndIf}
 !endif
 ${If} $PurgeSettings == 1
   !insertmacro RunMaintenance "clean-settings" "Pulizia impostazioni non riuscita"
 ${EndIf}
 ; The native remover validates the fixed root and never follows junctions.
 ; Files in use are scheduled, with their directory, for removal at reboot.
 ClearErrors
 nsExec::ExecToLog /TIMEOUT=45000 '"$PLUGINSDIR\netlink-maintenance.exe" clean-files${MAINTARGS}'
 Pop $0
 ${If} $0 == 3010
   SetRebootFlag true
   DetailPrint "Riavvio necessario per completare la rimozione dei file in uso."
 ${ElseIf} $0 != 0
   Abort "Rimozione file incompleta (codice $0). Consultare C:\ProgramData\NetLink\maintenance-error.txt"
 ${EndIf}
 DeleteRegKey ${APPREGROOT} "${APPREGKEY}"
 SetRegView 32
 DeleteRegKey ${APPREGROOT} "${APPREGKEY}"
 SetRegView 64
 ; Keys outside the program directory are never opened, copied or deleted.
SectionEnd
