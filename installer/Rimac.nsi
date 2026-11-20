;-----------------------------------------------------------------------------
; Rimac — instalador para Windows (VST3 + Standalone)
;-----------------------------------------------------------------------------
Unicode true

!include "MUI2.nsh"
!include "FileFunc.nsh"

!define APP_NAME      "Rimac"
!define APP_VERSION   "1.0.0"
!define APP_PUBLISHER "aMii5"
!define APP_URL       "https://github.com/aMii5/rimac"

!define VST3_DIR  "..\build\Rimac_artefacts\Release\VST3\Rimac.vst3"
!define EXE_FILE  "..\build\Rimac_artefacts\Release\Standalone\Rimac.exe"
!define ICON_FILE "..\build\Rimac_artefacts\Release\VST3\Rimac.vst3\Plugin.ico"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "..\dist\Rimac-${APP_VERSION}-Windows-Setup.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"
InstallDirRegKey HKLM "Software\${APP_NAME}" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName"     "${APP_NAME}"
VIAddVersionKey "ProductVersion"  "${APP_VERSION}"
VIAddVersionKey "FileDescription" "${APP_NAME} installer"
VIAddVersionKey "FileVersion"     "${APP_VERSION}.0"
VIAddVersionKey "CompanyName"     "${APP_PUBLISHER}"
VIAddVersionKey "LegalCopyright"  "MIT Licence"

!define MUI_ICON   "${ICON_FILE}"
!define MUI_UNICON "${ICON_FILE}"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\INSTALL.txt"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Ver notas de instalación"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Spanish"
!insertmacro MUI_LANGUAGE "English"

;-----------------------------------------------------------------------------
Section "Plugin VST3 (obligatorio)" SEC_VST3
  SectionIn RO
  SetOutPath "$COMMONFILES64\VST3\${APP_NAME}.vst3"
  File /r "${VST3_DIR}\*.*"

  SetOutPath "$INSTDIR"
  File "LICENSE.txt"
  File "INSTALL.txt"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\${APP_NAME}" "InstallDir" "$INSTDIR"
SectionEnd

Section "Aplicación Standalone" SEC_APP
  SetOutPath "$INSTDIR"
  File "${EXE_FILE}"

  CreateDirectory "$SMPROGRAMS\${APP_NAME}"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_NAME}.exe"
  CreateShortcut "$SMPROGRAMS\${APP_NAME}\Desinstalar ${APP_NAME}.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Acceso directo en el escritorio" SEC_DESKTOP
  CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_NAME}.exe"
SectionEnd

;-----------------------------------------------------------------------------
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_VST3}    "Instala el plugin en la carpeta común de VST3 para usarlo en cualquier DAW."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_APP}     "Copia el ejecutable independiente y crea las entradas del menú Inicio."
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP} "Crea un acceso directo a la aplicación en el escritorio."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;-----------------------------------------------------------------------------
Section "Uninstall"
  SetRegView 64
  Delete "$COMMONFILES64\VST3\${APP_NAME}.vst3\Contents\x86_64-win\${APP_NAME}.vst3"
  Delete "$COMMONFILES64\VST3\${APP_NAME}.vst3\Contents\Resources\moduleinfo.json"
  Delete "$COMMONFILES64\VST3\${APP_NAME}.vst3\Plugin.ico"
  Delete "$COMMONFILES64\VST3\${APP_NAME}.vst3\desktop.ini"
  RMDir "$COMMONFILES64\VST3\${APP_NAME}.vst3\Contents\x86_64-win"
  RMDir "$COMMONFILES64\VST3\${APP_NAME}.vst3\Contents\Resources"
  RMDir "$COMMONFILES64\VST3\${APP_NAME}.vst3\Contents"
  RMDir "$COMMONFILES64\VST3\${APP_NAME}.vst3"

  Delete "$INSTDIR\${APP_NAME}.exe"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\INSTALL.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
  Delete "$SMPROGRAMS\${APP_NAME}\Desinstalar ${APP_NAME}.lnk"
  RMDir "$SMPROGRAMS\${APP_NAME}"
  Delete "$DESKTOP\${APP_NAME}.lnk"

  DeleteRegKey HKLM "Software\${APP_NAME}"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
SectionEnd

;-----------------------------------------------------------------------------
Function .onInit
  SetRegView 64
  ; Registro para "Aplicaciones y características".
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                   "DisplayName" "${APP_NAME} ${APP_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                   "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                   "Publisher" "${APP_PUBLISHER}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                   "URLInfoAbout" "${APP_URL}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                   "DisplayIcon" "$INSTDIR\${APP_NAME}.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                   "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                   "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                   "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" \
                   "NoRepair" 1
FunctionEnd
