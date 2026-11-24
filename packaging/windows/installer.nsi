; EdgeDrop Windows NSIS Installer Script
; Modern UI 2 with user-level execution (no admin elevation required)

!include "MUI2.nsh"
!define PRODUCT_NAME "EdgeDrop"
!define PRODUCT_VERSION "5.0.0"
!define PRODUCT_PUBLISHER "EdgeDrop Open Source"
!define PRODUCT_WEB_SITE "https://edgedrop.app"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\EdgeDrop.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKCU"

SetCompressor /SOLID lzma
RequestExecutionLevel user

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "..\..\dist\EdgeDrop-Windows-x64-Setup.exe"
InstallDir "$LOCALAPPDATA\Programs\EdgeDrop"
InstallDirRegKey HKCU "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

!define MUI_ABORTWARNING
!define MUI_ICON "..\..\assets\icons\EdgeDrop.ico"
!define MUI_UNICON "..\..\assets\icons\EdgeDrop.ico"

; Welcome Page with Friendly-Formal SmartScreen Notice
!define MUI_WELCOMEPAGE_TITLE "Welcome to EdgeDrop"
!define MUI_WELCOMEPAGE_TEXT "EdgeDrop is a minimalist, local-first offline file compression utility.$\r$\n$\r$\nA quick note before you install:$\r$\nEdgeDrop is built by an independent creator and currently distributed as an unsigned, open-source binary. SmartScreen may display 'Windows protected your PC'. Simply click 'More info' and 'Run anyway'.$\r$\n$\r$\nEdgeDrop makes zero network calls and runs entirely offline on your device.$\r$\n$\r$\nClick Next to continue."

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_RUN "$INSTDIR\EdgeDrop.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch EdgeDrop now"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Section "MainSection" SEC01
  SetOutPath "$INSTDIR"
  SetOverwrite ifnewer
  File /r "..\..\build\Release\*.*"
  
  CreateDirectory "$SMPROGRAMS\EdgeDrop"
  CreateShortcut "$SMPROGRAMS\EdgeDrop\EdgeDrop.lnk" "$INSTDIR\EdgeDrop.exe"
  CreateShortcut "$SMPROGRAMS\EdgeDrop\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortcut "$DESKTOP\EdgeDrop.lnk" "$INSTDIR\EdgeDrop.exe"
SectionEnd

Section -Post
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\EdgeDrop.exe"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\EdgeDrop.exe"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
SectionEnd

Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "$(^Name) was successfully removed from your computer."
FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to completely remove $(^Name) and all of its components?" IDYES +2
  Abort
FunctionEnd

Section Uninstall
  Delete "$DESKTOP\EdgeDrop.lnk"
  Delete "$SMPROGRAMS\EdgeDrop\EdgeDrop.lnk"
  Delete "$SMPROGRAMS\EdgeDrop\Uninstall.lnk"
  RMDir "$SMPROGRAMS\EdgeDrop"
  
  RMDir /r "$INSTDIR"
  
  DeleteRegKey HKCU "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKCU "${PRODUCT_DIR_REGKEY}"
  SetAutoClose true
SectionEnd
