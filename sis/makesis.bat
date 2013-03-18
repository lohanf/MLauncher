REM set SAVEPATH=%PATH%
REM cd ..\group

REM set EPOCROOT=\Symbian\9.1\S60_3rd_MR\
REM set PATH=%EPOCROOT%\epoc32\tools;%SAVEPATH%
REM call abld clean
REM call abld build gcce urel
REM set EPOCROOT=\S60\devices\S60_3rd_FP2_SDK_v1.1\
REM set PATH=%EPOCROOT%\epoc32\tools;%SAVEPATH%
REM call abld clean
REM call abld build gcce urel
REM cd ..\sis

del cert-gen.cer
del key-gen.key

REM makekeys -cert -expdays 3650 -password HarryPotter -len 2048 -dname "CN=Florin Lohan C=FI EM=florin.lohan@mlauncher.org" fl_key.key fl_cert.cer
createsis sign   -cert fl_cert.cer -key fl_key.key -pass HarryRonHermione OggPlayController.sis OggPlayController.sisx
createsis create -cert fl_cert.cer -key fl_key.key -pass HarryRonHermione Mlauncher_NoSisInside.pkg 
createsis create -cert fl_cert.cer -key fl_key.key -pass HarryRonHermione Mlauncher.pkg

REM set PATH=%SAVEPATH%

rename Mlauncher.SIS MLauncher_sis.sisx
rename Mlauncher_NoSisInside.SIS MLauncher.sisx
