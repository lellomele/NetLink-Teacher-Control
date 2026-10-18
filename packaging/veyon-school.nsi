Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"
!include "${REPOROOT}\packaging\NetLinkUpgrade.nsh"
Var PurgeSettings
Var MaintenanceMutex
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
Name "NetLink 0.1.0 alpha.13"
OutFile "${OUTPUT}\NetLink-0.1.0-alpha.13-win64-setup.exe"
InstallDir "$PROGRAMFILES64\Veyon School"
SetCompressor /SOLID lzma
!define MUI_ICON "${REPOROOT}\branding\netlink.ico"
!define MUI_UNICON "${REPOROOT}\branding\netlink.ico"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${REPOROOT}\COPYING.GPL3"
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Italian"
VIProductVersion "4.7.5.13"
VIAddVersionKey /LANG=1040 "ProductName" "NetLink"
VIAddVersionKey /LANG=1040 "FileDescription" "NetLink installer (pre-release)"
VIAddVersionKey /LANG=1040 "FileVersion" "0.1.0-alpha.13"
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
 MessageBox MB_YESNOCANCEL|MB_DEFBUTTON2|MB_ICONQUESTION "Rimuovere anche le impostazioni di NetLink? $\r$\n$\r$\nSì: rimuove programma, cartella e impostazioni. Conserva le chiavi di autenticazione e i loro percorsi.$\r$\nNo: rimuove programma e cartella, ma conserva le impostazioni per reinstallare.$\r$\nAnnulla: interrompe.$\r$\n$\r$\nLe impostazioni condivise con Veyon originale non vengono eliminate. Una copia di recupero resta protetta in ProgramData\NetLink\Recovery." IDYES un_purge IDNO un_choice_done
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
Section "NetLink"
 SetShellVarContext all
 ; Never execute a leftover Qt CLI before restoring its runtime. This helper
 ; is statically linked and uses only Windows APIs, from the installer temp dir.
 InitPluginsDir
 SetOutPath "$PLUGINSDIR"
 File /oname=netlink-maintenance.exe "${PAYLOAD}\netlink-maintenance.exe"
 !insertmacro RunMaintenance "check-pending" "Riavviare Windows prima di reinstallare: rimozioni precedenti ancora in sospeso"
 !insertmacro RunMaintenance "backup" "Backup delle impostazioni non riuscito; nessun file è stato sostituito"
 !insertmacro RunMaintenance "stop" "Impossibile arrestare/rimuovere il servizio precedente; chiudere le applicazioni e riavviare"
 SetOutPath "$INSTDIR"
 ClearErrors
 File /r "${PAYLOAD}\*"
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
   MessageBox MB_ICONSTOP "Registrazione servizio non riuscita. Installazione incompleta."
   Abort
 ${EndIf}
 ClearErrors
 ExecWait '"$INSTDIR\netlink-wcli.exe" config set Windows/UseInterceptionDriver false' $0
 ${If} ${Errors}
 ${OrIf} $0 != 0
   MessageBox MB_ICONSTOP "Configurazione/firewall non riusciti. Verifica nel Configurator."
   Abort
 ${EndIf}
 ClearErrors
 ExecWait '"$INSTDIR\netlink-wcli.exe" config set UltraVNC/DeskDupEngine false' $0
 ${If} ${Errors}
 ${OrIf} $0 != 0
   MessageBox MB_ICONSTOP "Configurazione acquisizione schermo non riuscita."
   Abort
 ${EndIf}
 ${GetParameters} $R0
 ${GetOptions} $R0 "/Master=" $R1
 ${If} $R1 != ""
   ClearErrors
   ExecWait '"$INSTDIR\netlink-wcli.exe" config set School/DiscoveryMaster "$R1"' $0
   ${If} ${Errors}
   ${OrIf} $0 != 0
     MessageBox MB_ICONSTOP "Nome del Master non configurato."
     Abort
   ${EndIf}
 ${EndIf}
 ClearErrors
 ExecWait '"$INSTDIR\netlink-wcli.exe" service start' $0
 ${If} ${Errors}
 ${OrIf} $0 != 0
   MessageBox MB_ICONSTOP "Avvio servizio non riuscito. Verifica nel Configurator."
   Abort
 ${EndIf}
 ; New registration/start succeeded. Retire only the seven known old filenames;
 ; an in-use legacy EXE is removed on reboot, never by killing a user process.
 !insertmacro NetLinkRetireLegacyExecutables
 ; Preserve installation path/service identity for existing settings and keys.
 !ifndef NETLINK_INSTALLER_FIXTURE
 CreateDirectory "$SMPROGRAMS\NetLink"
 CreateShortcut "$SMPROGRAMS\NetLink\Console docente.lnk" "$INSTDIR\netlink-master.exe"
 CreateShortcut "$SMPROGRAMS\NetLink\Configurator.lnk" "$INSTDIR\netlink-configurator.exe"
 CreateShortcut "$SMPROGRAMS\NetLink\Companion.lnk" "$INSTDIR\netlink-companion.exe"
 Delete "$SMPROGRAMS\Veyon School\Master.lnk"
 Delete "$SMPROGRAMS\Veyon School\Configurator.lnk"
 RMDir "$SMPROGRAMS\Veyon School"
 !endif
 WriteRegStr ${APPREGROOT} "${APPREGKEY}" "DisplayName" "NetLink 0.1.0 alpha.13"
 WriteRegStr ${APPREGROOT} "${APPREGKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
 WriteRegStr ${APPREGROOT} "${APPREGKEY}" "DisplayIcon" "$INSTDIR\netlink-master.exe"
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
