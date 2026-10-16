; SPDX-License-Identifier: GPL-2.0-or-later
; Copyright (c) 2025-26 prof. Ing. Raffaele Mele
!ifndef NETLINK_UPGRADE_INCLUDED
!define NETLINK_UPGRADE_INCLUDED
!include "LogicLib.nsh"

; During upgrade use the installed CLI before replacing its DLLs. Prefer the
; current name on reinstallation, retain support for the alpha.1-alpha.8 name.
!macro NetLinkSelectUpgradeCli RESULT
 StrCpy ${RESULT} ""
 ${If} ${FileExists} "$INSTDIR\netlink-wcli.exe"
   StrCpy ${RESULT} "$INSTDIR\netlink-wcli.exe"
 ${ElseIf} ${FileExists} "$INSTDIR\veyon-wcli.exe"
   StrCpy ${RESULT} "$INSTDIR\veyon-wcli.exe"
 ${EndIf}
!macroend

!macro NetLinkRetireLegacyFile NAME
 ; Tests operate on inert files in a private fixture, never on reboot state.
 !ifdef NETLINK_UPGRADE_FIXTURE
   Delete "$INSTDIR\${NAME}"
 !else
   Delete /REBOOTOK "$INSTDIR\${NAME}"
 !endif
!macroend

; Exact known fork executables only, under the fixed protected installation
; directory. Never remove a Veyon original installation, DLL, key or user file.
!macro NetLinkRetireLegacyExecutables
 !insertmacro NetLinkRetireLegacyFile "veyon-master.exe"
 !insertmacro NetLinkRetireLegacyFile "veyon-configurator.exe"
 !insertmacro NetLinkRetireLegacyFile "veyon-service.exe"
 !insertmacro NetLinkRetireLegacyFile "veyon-server.exe"
 !insertmacro NetLinkRetireLegacyFile "veyon-worker.exe"
 !insertmacro NetLinkRetireLegacyFile "veyon-cli.exe"
 !insertmacro NetLinkRetireLegacyFile "veyon-wcli.exe"
!macroend
!endif
