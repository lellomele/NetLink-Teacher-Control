Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"
Name "Veyon School 0.1.0 alpha.1"
OutFile "${OUTPUT}\VeyonSchool-0.1.0-alpha.1-win64-setup.exe"
InstallDir "$PROGRAMFILES64\Veyon School"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
!define MUI_ICON "${REPOROOT}\branding\veyon-school.ico"
!define MUI_UNICON "${REPOROOT}\branding\veyon-school.ico"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${REPOROOT}\COPYING"
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Italian"
VIProductVersion "4.7.5.1"
VIAddVersionKey /LANG=1040 "ProductName" "Veyon School"
VIAddVersionKey /LANG=1040 "FileDescription" "Veyon School installer (pre-release)"
VIAddVersionKey /LANG=1040 "FileVersion" "0.1.0-alpha.1"
VIAddVersionKey /LANG=1040 "LegalCopyright" "Veyon and Veyon School contributors"
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
   MessageBox MB_YESNO|MB_ICONEXCLAMATION "Veyon originale è presente. Fermare il suo servizio e disabilitarne l’avvio automatico per evitare conflitti? Installazione e configurazione saranno conservate; il tipo di avvio potrà essere ripristinato disinstallando Veyon School. Versione preliminare: usare PC di prova." IDYES stop_legacy
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
Section "Veyon School"
 SetShellVarContext all
 SetOutPath "$INSTDIR"
 CreateDirectory "$INSTDIR\Backups"
 ExecWait '"$SYSDIR\icacls.exe" "$INSTDIR\Backups" /inheritance:r /grant:r "*S-1-5-18:(OI)(CI)F" "*S-1-5-32-544:(OI)(CI)F"' $0
 ${If} $0 != 0
   Abort "Impossibile proteggere la directory dei backup."
 ${EndIf}
 IfFileExists "$INSTDIR\veyon-wcli.exe" 0 fresh
 ExecWait '"$INSTDIR\veyon-wcli.exe" config export "$INSTDIR\Backups\configuration-before-upgrade.json"' $0
 ExecWait '"$INSTDIR\veyon-wcli.exe" service stop' $0
 ExecWait '"$INSTDIR\veyon-wcli.exe" service unregister' $0
 fresh:
 File /r "${PAYLOAD}\*"
 IfFileExists "$INSTDIR\Backups\configuration-original.json" backup_done
 ExecWait '"$INSTDIR\veyon-wcli.exe" config export "$INSTDIR\Backups\configuration-original.json"' $0
 ${If} $0 != 0
   Abort "Backup della configurazione non riuscito. Nessuna configurazione è stata sovrascritta."
 ${EndIf}
 backup_done:
 ExecWait '"$INSTDIR\veyon-wcli.exe" service register' $0
 ${If} $0 != 0
   MessageBox MB_ICONSTOP "Registrazione servizio non riuscita. Installazione incompleta."
   Abort
 ${EndIf}
 ExecWait '"$INSTDIR\veyon-wcli.exe" config set Windows/UseInterceptionDriver false' $0
 ${If} $0 != 0
   MessageBox MB_ICONSTOP "Configurazione/firewall non riusciti. Verifica nel Configurator."
   Abort
 ${EndIf}
 ExecWait '"$INSTDIR\veyon-wcli.exe" config set UltraVNC/DeskDupEngine false' $0
 ${If} $0 != 0
   MessageBox MB_ICONSTOP "Configurazione acquisizione schermo non riuscita."
   Abort
 ${EndIf}
 ${GetParameters} $R0
 ${GetOptions} $R0 "/Master=" $R1
 ${If} $R1 != ""
   ExecWait '"$INSTDIR\veyon-wcli.exe" config set School/DiscoveryMaster "$R1"' $0
   ${If} $0 != 0
     MessageBox MB_ICONSTOP "Nome del Master non configurato."
     Abort
   ${EndIf}
 ${EndIf}
 ExecWait '"$INSTDIR\veyon-wcli.exe" service start' $0
 ${If} $0 != 0
   MessageBox MB_ICONSTOP "Avvio servizio non riuscito. Verifica nel Configurator."
   Abort
 ${EndIf}
 CreateDirectory "$SMPROGRAMS\Veyon School"
 CreateShortcut "$SMPROGRAMS\Veyon School\Master.lnk" "$INSTDIR\veyon-master.exe"
 CreateShortcut "$SMPROGRAMS\Veyon School\Configurator.lnk" "$INSTDIR\veyon-configurator.exe"
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VeyonSchool" "DisplayName" "Veyon School 0.1.0 alpha.1"
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VeyonSchool" "UninstallString" '"$INSTDIR\Uninstall.exe"'
 WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\VeyonSchool" "DisplayIcon" "$INSTDIR\veyon-master.exe"
SectionEnd
Section "Uninstall"
 SetRegView 64
 SetShellVarContext all
 ; Do not recursively delete the directory: preserve configuration backups and unknown user files.
 ExecWait '"$INSTDIR\veyon-wcli.exe" service stop' $0
 ExecWait '"$INSTDIR\veyon-wcli.exe" service unregister' $0
 ExecWait '"$INSTDIR\veyon-wcli.exe" school firewall_remove' $0
 !include "${OUTPUT}\uninstall-files.nsh"
 Delete "$INSTDIR\Uninstall.exe"
 Delete "$SMPROGRAMS\Veyon School\Master.lnk"
 Delete "$SMPROGRAMS\Veyon School\Configurator.lnk"
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
