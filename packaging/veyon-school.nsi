Unicode true
!ifndef NETLINK_BUILD_SUFFIX
 !define NETLINK_BUILD_SUFFIX ""
!endif
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"
!include "nsDialogs.nsh"
!include "Sections.nsh"
!include "${REPOROOT}\packaging\NetLinkUpgrade.nsh"
Var PurgeSettings
Var MaintenanceMutex
Var InstallationRole
Var InstallCompanion
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
Name "NetLink 1.0.0 alpha.25${NETLINK_BUILD_SUFFIX}"
OutFile "${OUTPUT}\NetLink-1.0.0-alpha.25${NETLINK_BUILD_SUFFIX}-win64-setup.exe"
InstallDir "$PROGRAMFILES64\Veyon School"
SetCompressor /SOLID lzma
AllowSkipFiles off
!define MUI_ICON "${REPOROOT}\branding\netlink.ico"
!define MUI_UNICON "${REPOROOT}\branding\netlink.ico"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${REPOROOT}\COPYING.GPL3"
Page custom RolePageCreate RolePageLeave
!define MUI_COMPONENTSPAGE_TEXT_TOP "Seleziona le applicazioni da installare. Service e Configurator sono obbligatori. Per aggiungere Companion in seguito basta rieseguire questo installer: configurazione e chiavi sono conservate."
!define MUI_COMPONENTSPAGE_TEXT_COMPLIST "Applicazioni NetLink:"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
; Standard MUI completion action: checked by default, only on the interactive
; success page. Silent installs and pending-reboot exits never launch the GUI.
!define MUI_FINISHPAGE_RUN "$INSTDIR\netlink-configurator.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Avvia NetLink Configurator"
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Italian"
VIProductVersion "1.0.0.25"
VIAddVersionKey /LANG=1040 "ProductName" "NetLink"
VIAddVersionKey /LANG=1040 "FileDescription" "NetLink installer (pre-release)"
VIAddVersionKey /LANG=1040 "FileVersion" "1.0.0-alpha.25${NETLINK_BUILD_SUFFIX}"
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
 ; Retain installed optional components. Metadata is written only on success;
 ; older installations can be recognized by their existing executable.
 ReadRegDWORD $InstallCompanion ${APPREGROOT} "${APPREGKEY}" "InstallCompanion"
 ${If} ${Errors}
   StrCpy $InstallCompanion 0
   !ifdef NETLINK_INSTALLER_FIXTURE
     StrCpy $R2 "${FIXTURE_ROOT}\NetLinkFixtureApp"
   !else
     StrCpy $R2 "$PROGRAMFILES64\Veyon School"
   !endif
   ${If} ${FileExists} "$R2\netlink-companion.exe"
     StrCpy $InstallCompanion 1
   ${EndIf}
 ${ElseIf} $InstallCompanion != 1
   StrCpy $InstallCompanion 0
 ${EndIf}
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
 ClearErrors
 ${GetOptions} $R0 "/Companion=" $R1
 ${IfNot} ${Errors}
   ${If} $R1 == "1"
     StrCpy $InstallCompanion 1
   ${ElseIf} $R1 == "0"
     StrCpy $InstallCompanion 0
   ${Else}
     SetErrorLevel 2
     IfSilent +2
     MessageBox MB_OK|MB_ICONSTOP "Opzione Companion non valida. Usare /Companion=1 oppure /Companion=0. Nessun file o impostazione è stato modificato."
     Quit
   ${EndIf}
 ${EndIf}
 Call ApplyComponentSelection
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
 ${NSD_CreateLabel} 0 0 100% 24u "Service e Configurator sono sempre inclusi. Nella pagina successiva puoi scegliere le applicazioni facoltative. Configurazione e chiavi sono conservate."
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
 Call ApplyComponentSelection
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
; Native component rows, in the agreed order. The common install section below
; performs all copying so verification always sees the complete selected payload.
Section "NetLink Service" SEC_SERVICE
 SectionIn RO
SectionEnd
Section "NetLink Configurator" SEC_CONFIGURATOR
 SectionIn RO
SectionEnd
Section /o "NetLink Console docente" SEC_MASTER
SectionEnd
Section /o "NetLink Companion" SEC_COMPANION
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
 !insertmacro MUI_DESCRIPTION_TEXT ${SEC_SERVICE} "Servizio e runtime necessari per il controllo remoto, InternetGuard, Appello e discovery. Sempre installati."
 !insertmacro MUI_DESCRIPTION_TEXT ${SEC_CONFIGURATOR} "Configurazione locale, chiavi di autenticazione e autorizzazioni. Sempre installato."
 !insertmacro MUI_DESCRIPTION_TEXT ${SEC_MASTER} "Console docente per visualizzare e controllare i PC. Selezionata per Master, esclusa per Client."
 !insertmacro MUI_DESCRIPTION_TEXT ${SEC_COMPANION} "Diagnostica locale e collegamento protetto temporaneo. Facoltativo; aggiungibile rieseguendo l'installer senza disinstallare."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function ApplyComponentSelection
 ${If} $InstallationRole == "Master"
   !insertmacro SelectSection ${SEC_MASTER}
 ${Else}
   !insertmacro UnselectSection ${SEC_MASTER}
 ${EndIf}
 ${If} $InstallCompanion == 1
   !insertmacro SelectSection ${SEC_COMPANION}
 ${Else}
   !insertmacro UnselectSection ${SEC_COMPANION}
 ${EndIf}
FunctionEnd
Function ReadComponentSelection
 SectionGetFlags ${SEC_MASTER} $0
 IntOp $0 $0 & ${SF_SELECTED}
 ${If} $0 != 0
   StrCpy $InstallationRole "Master"
 ${Else}
   StrCpy $InstallationRole "Client"
 ${EndIf}
 SectionGetFlags ${SEC_COMPANION} $0
 IntOp $InstallCompanion $0 & ${SF_SELECTED}
FunctionEnd
Function .onSelChange
 Call ReadComponentSelection
FunctionEnd

Section "-NetLink installation"
 Call ReadComponentSelection
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
 ${If} $InstallCompanion != 1
   ClearErrors
   Delete "$INSTDIR\netlink-companion.exe"
   ${If} ${Errors}
     Abort "Impossibile rimuovere Companion. Chiudi NetLink, riavvia e ripeti l'installer. Configurazione e chiavi sono conservate."
   ${EndIf}
 ${EndIf}
 SetOutPath "$INSTDIR"
 ClearErrors
 File /r /x netlink-master.exe /x netlink-companion.exe /x payload.sha256 /x payload-client.sha256 /x payload-master-no-companion.sha256 /x payload-client-no-companion.sha256 "${PAYLOAD}\*"
 ${If} $InstallCompanion == 1
   File "${PAYLOAD}\netlink-companion.exe"
 ${EndIf}
 ${If} $InstallationRole == "Master"
   File "${PAYLOAD}\netlink-master.exe"
   ${If} $InstallCompanion == 1
     File /oname=payload.sha256 "${PAYLOAD}\payload.sha256"
   ${Else}
     File /oname=payload.sha256 "${PAYLOAD}\payload-master-no-companion.sha256"
   ${EndIf}
 ${Else}
   ${If} $InstallCompanion == 1
     File /oname=payload.sha256 "${PAYLOAD}\payload-client.sha256"
   ${Else}
     File /oname=payload.sha256 "${PAYLOAD}\payload-client-no-companion.sha256"
   ${EndIf}
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
 ; Retire unprefixed labels, including optional applications no longer selected.
 Delete "$StartMenuDir\Console docente.lnk"
 Delete "$StartMenuDir\Configurator.lnk"
 Delete "$StartMenuDir\Companion.lnk"
 ${If} $InstallationRole == "Master"
   CreateShortcut "$StartMenuDir\NetLink Console docente.lnk" "$INSTDIR\netlink-master.exe"
 ${Else}
   Delete "$StartMenuDir\NetLink Console docente.lnk"
 ${EndIf}
 CreateShortcut "$StartMenuDir\NetLink Configurator.lnk" "$INSTDIR\netlink-configurator.exe"
 ${If} $InstallCompanion == 1
   CreateShortcut "$StartMenuDir\NetLink Companion.lnk" "$INSTDIR\netlink-companion.exe"
 ${Else}
   Delete "$StartMenuDir\NetLink Companion.lnk"
 ${EndIf}
 ${If} ${Errors}
   Abort "Aggiornamento collegamenti Start non riuscito. Ripetere l'installer completo."
 ${EndIf}
 !ifndef NETLINK_INSTALLER_FIXTURE
 Delete "$SMPROGRAMS\Veyon School\Master.lnk"
 Delete "$SMPROGRAMS\Veyon School\Configurator.lnk"
 RMDir "$SMPROGRAMS\Veyon School"
 !endif
 WriteRegStr ${APPREGROOT} "${APPREGKEY}" "DisplayName" "NetLink 1.0.0 alpha.25${NETLINK_BUILD_SUFFIX}"
 WriteRegStr ${APPREGROOT} "${APPREGKEY}" "DisplayVersion" "1.0.0-alpha.25${NETLINK_BUILD_SUFFIX}"
 WriteRegStr ${APPREGROOT} "${APPREGKEY}" "InstallationRole" "$InstallationRole"
 WriteRegDWORD ${APPREGROOT} "${APPREGKEY}" "InstallCompanion" $InstallCompanion
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
 Delete "$SMPROGRAMS\NetLink\NetLink Console docente.lnk"
 Delete "$SMPROGRAMS\NetLink\NetLink Configurator.lnk"
 Delete "$SMPROGRAMS\NetLink\NetLink Companion.lnk"
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
