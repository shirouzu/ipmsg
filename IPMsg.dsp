# Microsoft Developer Studio Project File - Name="IPMsg" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** 編集しないでください **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=IPMsg - Win32 Debug
!MESSAGE これは有効なﾒｲｸﾌｧｲﾙではありません。 このﾌﾟﾛｼﾞｪｸﾄをﾋﾞﾙﾄﾞするためには NMAKE を使用してください。
!MESSAGE [ﾒｲｸﾌｧｲﾙのｴｸｽﾎﾟｰﾄ] ｺﾏﾝﾄﾞを使用して実行してください
!MESSAGE 
!MESSAGE NMAKE /f "IPMsg.mak".
!MESSAGE 
!MESSAGE NMAKE の実行時に構成を指定できます
!MESSAGE ｺﾏﾝﾄﾞ ﾗｲﾝ上でﾏｸﾛの設定を定義します。例:
!MESSAGE 
!MESSAGE NMAKE /f "IPMsg.mak" CFG="IPMsg - Win32 Debug"
!MESSAGE 
!MESSAGE 選択可能なﾋﾞﾙﾄﾞ ﾓｰﾄﾞ:
!MESSAGE 
!MESSAGE "IPMsg - Win32 Release" ("Win32 (x86) Application" 用)
!MESSAGE "IPMsg - Win32 Debug" ("Win32 (x86) Application" 用)
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "IPMsg - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "IPMsg___Win32_Release"
# PROP BASE Intermediate_Dir "IPMsg___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Obj\Release"
# PROP Intermediate_Dir "Obj\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /FAcs /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "NDEBUG"
# ADD RSC /l 0x411 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib Comctl32.lib imm32.lib wsock32.lib external\lib\libpng.lib external\lib\zlib.lib lib\tlib.lib /nologo /subsystem:windows /map /machine:I386 /out:"Release/IPMsg.exe"

!ELSEIF  "$(CFG)" == "IPMsg - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "IPMsg___Win32_Debug"
# PROP BASE Intermediate_Dir "IPMsg___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Obj\Debug"
# PROP Intermediate_Dir "Obj\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x411 /d "_DEBUG"
# ADD RSC /l 0x411 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib Comctl32.lib imm32.lib wsock32.lib external\lib\libpng_d.lib external\lib\zlib_d.lib lib\tlib_d.lib /nologo /subsystem:windows /debug /machine:I386 /out:"Debug/IPMsg.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "IPMsg - Win32 Release"
# Name "IPMsg - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\aes.cpp
# End Source File
# Begin Source File

SOURCE=.\src\blowfish.cpp
# End Source File
# Begin Source File

SOURCE=.\src\cfg.cpp
# End Source File
# Begin Source File

SOURCE=.\src\ipmsg.cpp
# End Source File
# Begin Source File

SOURCE=.\src\ipmsg.h
# End Source File
# Begin Source File

SOURCE=.\src\ipmsg.rc
# End Source File
# Begin Source File

SOURCE=.\src\logmng.cpp
# End Source File
# Begin Source File

SOURCE=.\src\mainwin.cpp
# End Source File
# Begin Source File

SOURCE=.\src\miscdlg.cpp
# End Source File
# Begin Source File

SOURCE=.\src\miscfunc.cpp
# End Source File
# Begin Source File

SOURCE=.\src\msgmng.cpp
# End Source File
# Begin Source File

SOURCE=.\src\plugin.cpp
# End Source File
# Begin Source File

SOURCE=.\src\pubkey.cpp
# End Source File
# Begin Source File

SOURCE=.\src\recvdlg.cpp
# End Source File
# Begin Source File

SOURCE=.\src\resource.h
# End Source File
# Begin Source File

SOURCE=.\src\richedit.cpp
# End Source File
# Begin Source File

SOURCE=.\src\senddlg.cpp
# End Source File
# Begin Source File

SOURCE=.\src\setupdlg.cpp
# End Source File
# Begin Source File

SOURCE=.\src\share.cpp
# End Source File
# Begin Source File

SOURCE=.\src\version.cpp
# End Source File
# Begin Source File

SOURCE=.\src\version.h
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\src\absence.ico
# End Source File
# Begin Source File

SOURCE=.\src\dummypic.bmp
# End Source File
# Begin Source File

SOURCE=.\src\file.ico
# End Source File
# Begin Source File

SOURCE=.\src\fileabs.ico
# End Source File
# Begin Source File

SOURCE=.\src\ipmsg.ico
# End Source File
# Begin Source File

SOURCE=.\src\ipmsgrev.ico
# End Source File
# Begin Source File

SOURCE=.\src\ipmsgv1a.ico
# End Source File
# Begin Source File

SOURCE=.\src\ipmsgv2.ico
# End Source File
# Begin Source File

SOURCE=.\src\ipmsgv2_.ico
# End Source File
# Begin Source File

SOURCE=.\src\ipmsgv2a.ico
# End Source File
# Begin Source File

SOURCE=.\src\menu.ico
# End Source File
# Begin Source File

SOURCE=.\src\v1.ico
# End Source File
# Begin Source File

SOURCE=.\src\v1abs.ico
# End Source File
# End Group
# Begin Source File

SOURCE=.\src\ipmsg.exe.manifest
# End Source File
# End Target
# End Project
