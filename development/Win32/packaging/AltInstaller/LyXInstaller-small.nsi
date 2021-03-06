# Lyx for Windows, NSIS v2 series installer script

# File LyXInstaller-small.nsi
# This file is part of LyX, the document processor.
# http://www.lyx.org/
# Licence details can be found in the file COPYING or copy at
# http://www.lyx.org/about/license.php3

# Author: Uwe Stöhr based on the work of Angus Leeming and ideas of Joost Verburg
# Full author contact details are available in file CREDITS or copy at
# http://www.lyx.org/about/credits.php

# This script requires NSIS 2.45 or newer
# http://nsis.sourceforge.net/
#--------------------------------

# Do a Cyclic Redundancy Check to make sure the installer
# was not corrupted by the download.
CRCCheck force

# Make the installer as small as possible.
SetCompressor lzma

#--------------------------------
# You should need to change only these macros...

!define INSTALLER_VERSION "Small"
!define INSTALLER2_VERSION "Complete"
!define INSTALLER3_VERSION "Update"
!define INSTALLER_TYPE "NotUpdate"

# load the settings
!include "Settings.nsh"

#--------------------------------
# variables only used in this installer version

Var AppFiles
Var AiksaurusPath
Var AspellPath
Var Acrobat
Var BibTeXEditorPath
Var DelGSDir
Var DelPythonFiles
Var DelWMFDir
Var EditorPath
Var GhostscriptPath
Var ImageEditorPath
Var LangEncoding
Var LangSysEncoding
Var LaTeXName
Var LangNameLyX
Var MiKTeXVersion
Var MiKTeXUser
Var MissedProg
Var PathPrefix
Var PSVPath
Var State
Var SVGPath
Var WMFPath

# Variables used by all installer versions
!include "Variables.nsh"

#--------------------------------
# load some NSIS libraries
!include "MUI.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "StrFunc.nsh"
!include "TextFunc.nsh"
!include "WinVer.nsh"

# Set of various macros and functions
!include "LyXUtils.nsh"

# Functions to check and configure the LaTeX-system
!include "LaTeX.nsh"

# Function to check if needed programs are missing or not
!include "MissingPrograms.nsh"

# Function for page to manually select LaTeX's installation folder
!include "LaTeXFolder.nsh"

# Functions for page to set installer language
# and LyX's menu language
!include "LanguageSettings.nsh"

# Function to configure LyX
!include "ConfigLyX.nsh"

# Functions to configure needed third-party programs
!include "InstallThirdPartyProgs.nsh"

# Function for page to install Aspell dictionaries
!include "Aspell.nsh"

#--------------------------------
# Remember the installer language
!define MUI_LANGDLL_REGISTRY_ROOT "HKCU"
!define MUI_LANGDLL_REGISTRY_KEY "${PRODUCT_UNINST_KEY}"
!define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"

!define MUI_ABORTWARNING
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "${PRODUCT_DIR}\icons\lyx_logo_hi.bmp"
!define MUI_ICON "${PRODUCT_DIR}\icons\lyx.ico"
!define MUI_UNICON "${PRODUCT_DIR}\icons\lyx.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${VERSION_BITMAP}"

# Welcome page
!define MUI_WELCOMEPAGE_TEXT "$(WelcomePageText)"
!insertmacro MUI_PAGE_WELCOME

# Show the license.
!insertmacro MUI_PAGE_LICENSE "${PRODUCT_LICENSE_FILE}"

# Specify the installation directory.
!insertmacro MUI_PAGE_DIRECTORY

# Specify LyX's menu language.
Page custom SelectMenuLanguage SelectMenuLanguage_LeaveFunction

# Define which components to install.
!insertmacro MUI_PAGE_COMPONENTS

# Specify where to install program shortcuts.
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "${PRODUCT_ROOT_KEY}"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "${PRODUCT_UNINST_KEY}"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "LyX ${PRODUCT_VERSION}"
!insertmacro MUI_PAGE_STARTMENU ${PRODUCT_NAME} $StartmenuFolder

# Select latex.exe manually
Page custom LatexFolder LatexFolder_LeaveFunction

# Check for needed programs
Page custom MissingProgramsPage MissingProgramsPage_LeaveFunction

# Watch the components being installed.
!insertmacro MUI_PAGE_INSTFILES

# Finish page
!define MUI_FINISHPAGE_RUN 
!define MUI_FINISHPAGE_TEXT "$(FinishPageMessage)"
!define MUI_FINISHPAGE_RUN_TEXT "$(FinishPageRun)"
!define MUI_FINISHPAGE_RUN_FUNCTION "LaunchProduct"
!insertmacro MUI_PAGE_FINISH

# The uninstaller.
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES

#--------------------------------
# List of languages the installers are translated to

!include "TranslatedLanguages.nsh"

LicenseData "$(LyXLicenseData)"

#--------------------------------
# Reserve Files

# These files are inserted before other files in the data block
!insertmacro MUI_RESERVEFILE_LANGDLL
ReserveFile "io_ui_language.ini"
!insertmacro MUI_RESERVEFILE_INSTALLOPTIONS

#--------------------------------
# Installer sections

Section "!${PRODUCT_NAME}" SecCore
  SectionIn RO
SectionEnd
Section "$(SecAllUsersTitle)" SecAllUsers
  SetShellVarContext all
  StrCpy $ProductRootKey "HKLM"
SectionEnd
# remove the /o for LyX 2.0 final!
Section /o "$(SecFileAssocTitle)" SecFileAssoc
  StrCpy $CreateFileAssociations "true"
SectionEnd
Section /o "$(SecDesktopTitle)" SecDesktop
  StrCpy $CreateDesktopIcon "true"
SectionEnd

# Section descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${SecCore} "$(SecCoreDescription)"
!insertmacro MUI_DESCRIPTION_TEXT ${SecAllUsers} "$(SecAllUsersDescription)"
!insertmacro MUI_DESCRIPTION_TEXT ${SecFileAssoc} "$(SecFileAssocDescription)"
!insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "$(SecDesktopDescription)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

# the installation section
!include "InstallActions-complete-small.nsh"

#--------------------------------
# This hook function is called internally by NSIS on installer startup
Function .onInit

  # set the installer language to the Windows locale language
  System::Call "kernel32::GetUserDefaultLangID()i.a"

  # check that the installer is not currently running
  System::Call 'kernel32::CreateMutexA(i 0, i 0, t "${INSTALLER_EXE}.Instance") i .r1 ?e'
  Pop $R0
  ${if} $R0 != "0"
   MessageBox MB_OK|MB_ICONSTOP "$(InstallRunning)"
   Abort
  ${endif}
  FindProcDLL::FindProc "${INSTALLER2_EXE}"
  ${if} $R0 == "1"
   MessageBox MB_OK|MB_ICONSTOP "$(InstallRunning)"
   Abort
  ${endif}

  # check if LyX is already installed
  ReadRegStr $0 HKLM "${PRODUCT_UNINST_KEY}" "Publisher"
  ${if} $0 != ""
   MessageBox MB_OK|MB_ICONSTOP "$(StillInstalled)"
   Abort
  ${endif}
  
  # printer settings, needed to install the Metafile2eps printer
  !insertmacro PrinterInit # macro from InstallThirdPartyProgs.nsh

  # default settings
  # these can be reset to "all" in section SecAllUsers
  SetShellVarContext current
  StrCpy $ProductRootKey "HKCU"

  # this can be reset to "true" in section SecDesktop
  StrCpy $CreateDesktopIcon "false"
  StrCpy $CreateFileAssociations "false"

  # if the user does *not* have administrator privileges, abort
  StrCpy $Answer ""
  StrCpy $UserName ""
  !insertmacro IsUserAdmin $Answer $UserName # macro from LyXUtils.nsh
  ${if} $Answer == "yes"
    !define ENABLE 0x00000001
    SectionGetFlags ${SecAllUsers} $0
    IntOp $0 $0 | ${ENABLE}
    SectionSetFlags ${SecAllUsers} $0
    !undef ENABLE
  ${else}
    MessageBox MB_OK|MB_ICONSTOP "$(NotAdmin)"
    Abort
  ${endif}

  # check the LaTeX-system
  Call LaTeXActions # Function from LaTeX.nsh

  # check which programs are installed or not
  Call MissingPrograms # function from MissingPrograms.nsh

  # used later in the function LatexFolder
  StrCpy $State "0"

  ClearErrors
FunctionEnd

#--------------------------------

Function LaunchProduct
  Exec ${PRODUCT_BAT}
FunctionEnd

#--------------------------------
# The Uninstaller

!include "Uninstall.nsh"

# eof
