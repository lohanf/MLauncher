# ============================================================================
#  Name     : Icons_scalable_dc.mk
#  Part of  : MLauncher
#
#  Description: This is file for creating .mif file (scalable icon)
# 
# ============================================================================


ifeq (WINS,$(findstring WINS, $(PLATFORM)))
ZDIR=$(EPOCROOT)epoc32\release\$(PLATFORM)\$(CFG)\Z
else
ZDIR=$(EPOCROOT)epoc32\data\z
endif

TARGETDIR=$(ZDIR)\resource\apps
ICONTARGETFILENAME=$(TARGETDIR)\MLauncher.mif

ICONDIR=..\gfx

do_nothing :
	@rem do_nothing

MAKMAKE : do_nothing

BLD : do_nothing

CLEAN : do_nothing

LIB : do_nothing

CLEANLIB : do_nothing

RESOURCE :	
	mifconv $(ICONTARGETFILENAME) /H$(EPOCROOT)epoc32\include\MLauncher.mbg /c32 $(ICONDIR)\MLauncherAif.svg /c8,1 $(ICONDIR)\b_checked_svgt.svg /c8,1 $(ICONDIR)\b_partially_checked_svgt.svg /c8,1 $(ICONDIR)\b_empty_svgt.svg /c8,1 $(ICONDIR)\checkbox_checked.svg /c8,1 $(ICONDIR)\checkbox_partially_checked.svg /c8,1 $(ICONDIR)\checkbox_empty.svg /c8,1 $(ICONDIR)\sources_used.svg /c8,1 $(ICONDIR)\sources_ignored.svg /c8,1 $(ICONDIR)\sel_pls.svg /c8,1 $(ICONDIR)\empty.svg /c32,1 $(ICONDIR)\arrows_left_right_green.svg /c32,1 $(ICONDIR)\arrows_left_right_blue.svg /c32,1 $(ICONDIR)\arrows_left_right_orange.svg /c32,1 $(ICONDIR)\arrows_left_right_red.svg /c32,1 $(ICONDIR)\arrows_left_right_violet.svg /c32,1 $(ICONDIR)\play_white.svg /c32,1 $(ICONDIR)\pause_white.svg /c32,1 $(ICONDIR)\ff_white.svg /c32,1 $(ICONDIR)\fr_white.svg /c32,1 $(ICONDIR)\play_black.svg /c32,1 $(ICONDIR)\pause_black.svg /c32,1 $(ICONDIR)\ff_black.svg /c32,1 $(ICONDIR)\fr_black.svg /c32,1 $(ICONDIR)\into_folder.svg /c32,1 $(ICONDIR)\outof_folder.svg /c32,1 $(ICONDIR)\shuffleplay.svg /c32,1 $(ICONDIR)\preview.svg /c32,1 $(ICONDIR)\sources_music.svg /c32,1 $(ICONDIR)\sources_audiobooks.svg
		 
	xcopy $(ICONDIR)\cover* $(ZDIR)\private\E388D98A /i /y
FREEZE : do_nothing

SAVESPACE : do_nothing

RELEASABLES :
	@echo $(ICONTARGETFILENAME) 

FINAL : do_nothing

