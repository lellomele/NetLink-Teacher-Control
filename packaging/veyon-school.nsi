Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"
!include "${REPOROOT}\packaging\NetLinkUpgrade.nsh"
Var UpgradeCli
Name "NetLink 0.1.0 alpha.10"
OutFile "${OUTPUT}\NetLink-0.1.0-alpha.10-win64-setup.exe"
InstallDir "$PROGRAMFILES64\Veyon School"
RequestExecutionLevel admin
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
VIProductVersion "4.7.5.10"
VIAddVersionKey /LANG=1040 "ProductName" "NetLink"
VIAddVersionKey /LANG=1040 "FileDescription" "NetLink installer (pre-release)"
VIAddVersionKey /LANG=1040 "FileVersion" "0.1.0-alpha.10"
VIAddVersionKey /LANG=1040 "LegalCopyright" "Copyright (c) 2025-26 prof. Ing. Raffaele Mele; Veyon e contributori"
Function .onInit
 ${IfNot} ${RunningX64}
   MessageBox MB_ICONSTOP "Questa build richiede Windows a 64 bit."
   Abort
 ${EndIf}
 SetRegView 64
 ; Fixed protected destination: student accounts must not be able to replace SYSTEM executables.
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
FunctionEnd
Function un.onInit
 StrCpy $INSTDIR "$PROGRAMFILES64\Veyon School"
FunctionEnd
Section "NetLink"
 SetShellVarContext all
 SetOutPath "$INSTDIR"
 CreateDirectory "$INSTDIR\Backups"
 ExecWait '"$SYSDIR\icacls.exe" "$INSTDIR\Backups" /inheritance:r /grant:r "*S-1-5-18:(OI)(CI)F" "*S-1-5-32-544:(OI)(CI)F"' $0
 ${If} $0 != 0
   Abort "Impossibile proteggere la directory dei backup."
 ${EndIf}
 !insertmacro NetLinkSelectUpgradeCli $UpgradeCli
 ReadRegStr $1 HKLM "SYSTEM\CurrentControlSet\Services\VeyonSchoolService" "ImagePath"
 ${If} $UpgradeCli == ""
   ${If} $1 != ""
     Abort "Servizio del fork registrato ma CLI precedente assente: aggiornamento interrotto prima di sostituire file. Ripristinare l'installazione precedente."
   ${EndIf}
   Goto fresh
 ${EndIf}
 ClearErrors
 ExecWait '"$UpgradeCli" config export "$INSTDIR\Backups\configuration-before-upgrade.json"' $0
 ${If} ${Errors}
 ${OrIf} $0 != 0
   Abort "Backup prima dell’aggiornamento non riuscito."
 ${EndIf}
 ${If} $1 != ""
   ClearErrors
   ExecWait '"$UpgradeCli" service stop' $0
   ${If} ${Errors}
   ${OrIf} $0 != 0
     Abort "Impossibile fermare il servizio del fork; nessun file è stato sostituito."
   ${EndIf}
   ClearErrors
   ExecWait '"$UpgradeCli" service unregister' $0
   ${If} ${Errors}
   ${OrIf} $0 != 0
     Abort "Impossibile rimuovere la precedente registrazione del servizio."
   ${EndIf}
 ${EndIf}
 fresh:
 ClearErrors
 File /r "${PAYLOAD}\*"
 ${If} ${Errors}
   Abort "Copia dei file incompleta. Chiudere le applicazioni, riavviare Windows e ripetere l'installer completo; non sostituire singole DLL."
 ${EndIf}
 ; Keep recovery available even if later configuration or service startup fails.
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VeyonSchool" "DisplayName" "NetLink (installazione incompleta)"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VeyonSchool" "UninstallString" '"$INSTDIR\Uninstall.exe"'
 IfFileExists "$INSTDIR\Backups\configuration-original.json" backup_done
 ClearErrors
 ExecWait '"$INSTDIR\netlink-wcli.exe" config export "$INSTDIR\Backups\configuration-original.json"' $0
 ${If} ${Errors}
 ${OrIf} $0 != 0
   Abort "Backup della configurazione non riuscito. Nessuna configurazione è stata sovrascritta."
 ${EndIf}
 backup_done:
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
 CreateDirectory "$SMPROGRAMS\NetLink"
 CreateShortcut "$SMPROGRAMS\NetLink\Console docente.lnk" "$INSTDIR\netlink-master.exe"
 CreateShortcut "$SMPROGRAMS\NetLink\Configurator.lnk" "$INSTDIR\netlink-configurator.exe"
 CreateShortcut "$SMPROGRAMS\NetLink\Companion.lnk" "$INSTDIR\netlink-companion.exe"
 Delete "$SMPROGRAMS\Veyon School\Master.lnk"
 Delete "$SMPROGRAMS\Veyon School\Configurator.lnk"
 RMDir "$SMPROGRAMS\Veyon School"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VeyonSchool" "DisplayName" "NetLink 0.1.0 alpha.10"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VeyonSchool" "UninstallString" '"$INSTDIR\Uninstall.exe"'
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VeyonSchool" "DisplayIcon" "$INSTDIR\netlink-master.exe"
SectionEnd
Section "Uninstall"
 SetRegView 64
 SetShellVarContext all
 ; Do not recursively delete the directory: preserve configuration backups and unknown user files.
 ExecWait '"$INSTDIR\netlink-wcli.exe" service stop' $0
 ExecWait '"$INSTDIR\netlink-wcli.exe" service unregister' $0
 ExecWait '"$INSTDIR\netlink-wcli.exe" school firewall_remove' $0
 !include "${OUTPUT}\uninstall-files.nsh"
 Delete "$INSTDIR\Uninstall.exe"
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
 DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VeyonSchool"
 ; Veyon registry configuration, keys, backups and original service are deliberately retained.
SectionEnd
