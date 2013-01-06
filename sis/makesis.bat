del cert-gen.cer
del key-gen.key

REM makekeys -cert -expdays 3650 -password yourPw -len 2048 -dname "CN=Your Name C=FI EM=you@somewhere" fl_key.key fl_cert.cer
createsis sign   -cert fl_cert.cer -key fl_key.key -pass yourPw OggPlayController.sis OggPlayController.sisx
createsis create -cert fl_cert.cer -key fl_key.key -pass yourPw Mlauncher_NoSisInside.pkg 
createsis create -cert fl_cert.cer -key fl_key.key -pass yourPw Mlauncher.pkg


rename Mlauncher.SIS MLauncher_sis.sisx
rename Mlauncher_NoSisInside.SIS MLauncher.sisx
