; SPDX-License-Identifier: GPL-2.0-or-later
; No service, registry, configuration or reboot-state operations. Only inert
; files under this fixture executable's private directory are manipulated.
Unicode true
RequestExecutionLevel user
SilentInstall silent
AutoCloseWindow true
OutFile "${OUTPUT}\netlink-upgrade-fixture.exe"
!define NETLINK_UPGRADE_FIXTURE
!include "${REPOROOT}\packaging\NetLinkUpgrade.nsh"

!macro CheckCase CASE EXPECTED
 StrCpy $INSTDIR "$EXEDIR\NetLinkUpgradeFixtureData\${CASE}"
 !insertmacro NetLinkSelectUpgradeCli $R9
 ${If} $R9 != "${EXPECTED}"
   SetErrorLevel 10
   Quit
 ${EndIf}
 !insertmacro NetLinkRetireLegacyExecutables
!macroend

Section
 !insertmacro CheckCase "fresh" ""
 !insertmacro CheckCase "alpha8" "$INSTDIR\veyon-wcli.exe"
 !insertmacro CheckCase "alpha9" "$INSTDIR\netlink-wcli.exe"
 !insertmacro CheckCase "mixed" "$INSTDIR\netlink-wcli.exe"
 SetErrorLevel 0
SectionEnd
