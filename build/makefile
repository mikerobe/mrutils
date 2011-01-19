PLATFORM=$(shell uname -s)
ifeq ($(PLATFORM),Darwin)
	FAST=-fast
else
	FAST=-O3
endif

DEBUG = $(FAST)
SOURCES= \
	hmac_md5.cpp \
	hmac_sha1.cpp \
	md5.cpp \
	memxor.cpp \
	mr_beta.cpp \
	mr_betamult.cpp \
	mr_bufferedreader.cpp \
	mr_bufferedwriter.cpp \
	mr_delim.cpp \
	mr_encoder.cpp \
	mr_expavg.cpp \
	mr_exppois.cpp \
	mr_expvar.cpp \
	mr_filecpr.cpp \
	mr_filermr.cpp \
	mr_files.cpp \
	mr_gnuplot.cpp \
	mr_holdingprd.cpp \
	mr_mysql.cpp \
	mr_sql.cpp \
	mr_sqlite.cpp \
	mr_db.cpp \
	mr_numerical.cpp \
	mr_probit.cpp \
	mr_prog.cpp \
	mr_selinterrupt.cpp \
	mr_sfileh.cpp \
	mr_signal.cpp \
	mr_sockets.cpp \
	mr_strutils.cpp \
	mr_time.cpp \
	mr_timerqueue.cpp \
	mr_xml.cpp \
	mr_xmlbuilder.cpp \
	mysql_sha1.cpp \
	sha1.cpp \
	mr_guiimgterm.cpp \
	mr_xwin.cpp \
	mr_dialog.cpp \
	mr_audio.cpp \
	mr_display.cpp \
	mr_decoder.cpp \
	mr_colchooser.cpp \
	mr_termline.cpp \
	mr_termutils.cpp \
	mr_img.cpp \
	mr_curl.cpp \
	mr_x.cpp \
	mr_gui.cpp \
	mr_guieditbox.cpp \
	mr_guifilechooser.cpp \
	mr_guifilesave.cpp \
	mr_guilist.cpp \
	mr_guiscroll.cpp \
	mr_guiimgscroll.cpp \
	mr_guiselect.cpp \
	mr_guiviewer.cpp \
	mr_gzipreader.cpp \

SOURCES_OTHER = 

S = ../src/
O = objects/$(shell uname -s)

CC=g++
CFLAGS=-pipe -D BUILD_DLL -D OK_OUTSIDE_LIBS $(DEBUG) -Wall -Wextra -Woverloaded-virtual
LFLAGS= -L"${HOME}/lib" -fpic -shared -fvisibility=hidden -lX11 -lncurses -lX11 -ljpeg -lpng -lz -lsqlite3

OUTPUT_DIR=${HOME}/lib
ifneq ($(findstring cygwin,${OSTYPE}),)
	OUTPUT_DIR=/usr/bin
endif

######### AUDIO
ifeq ($(PLATFORM),Darwin)
	SOURCES_OTHER += \
		/Developer/Extras/CoreAudio/PublicUtility/CAStreamBasicDescription.cpp \
		/Developer/Extras/CoreAudio/PublicUtility/CAXException.cpp \
		/Developer/Extras/CoreAudio/PublicUtility/CAAudioUnit.cpp \
		/Developer/Extras/CoreAudio/PublicUtility/CAComponent.cpp \
		/Developer/Extras/CoreAudio/PublicUtility/CAComponentDescription.cpp \
		/Developer/Extras/CoreAudio/PublicUtility/AUOutputBL.cpp \
		/Developer/Extras/CoreAudio/PublicUtility/CAAudioChannelLayout.cpp \
		/Developer/Extras/CoreAudio/PublicUtility/CAAudioChannelLayoutObject.cpp \
		stacktrace_mac.cpp \

	CFLAGS+= -I/Developer/Extras/CoreAudio/PublicUtility/
	LFLAGS+= -prebind -framework OpenCL -framework AudioToolbox -framework CoreServices -framework AudioUnit 
else 
	CFLAGS+= -fno-strict-aliasing
	# do SOX based audio on linux
	ifeq ($(shell type sox > /dev/null 2>&1 && echo 1 || echo 0),1)
		LFLAGS+= -lsox
	endif
endif

######### GSL
ifeq ($(shell type gsl-config > /dev/null 2>&1 && echo 1 || echo 0),1)
	CFLAGS+= -D _MR_USE_GSL
	LFLAGS+= -lgsl
endif

######## GNU TLS (secure sockets)
	SOURCES_OTHER += \
		mr_sendmail.cpp \
		mr_imap.cpp \

	LFLAGS+= -L/usr/bin -lgcrypt -lgsasl -lgnutls -lgnutls-extra -lgpg-error #-liconv 
	CFLAGS+= -D MR_GNUTLS

# determine whether to compile 64 bit or 32
ifeq ($(shell uname -m),i386)
	CFLAGS+= -m64
else
	CFLAGS+= -m32
endif

OBJECTS = $(SOURCES:%.cpp=$(O)/%.o)
OBJECTS+= $(SOURCES_OTHER:%.cpp=$(O)/%.o)

all: $(OBJECTS)
	$(CC) $(OBJECTS) $(LFLAGS) -o "$(OUTPUT_DIR)/libmrutils.${DYLIB}"
	cd ../src && ./maketags.sh && cd -

$(O)//Developer/Extras/CoreAudio/PublicUtility/CAStreamBasicDescription.o:  
	$(CC) $(CFLAGS) -c /Developer/Extras/CoreAudio/PublicUtility/CAStreamBasicDescription.cpp $< -o $@
$(O)//Developer/Extras/CoreAudio/PublicUtility/CAXException.o:  
	$(CC) $(CFLAGS) -c /Developer/Extras/CoreAudio/PublicUtility/CAXException.cpp $< -o $@
$(O)//Developer/Extras/CoreAudio/PublicUtility/CAAudioUnit.o:  
	$(CC) $(CFLAGS) -c /Developer/Extras/CoreAudio/PublicUtility/CAAudioUnit.cpp $< -o $@
$(O)//Developer/Extras/CoreAudio/PublicUtility/CAComponent.o:  
	$(CC) $(CFLAGS) -c /Developer/Extras/CoreAudio/PublicUtility/CAComponent.cpp $< -o $@
$(O)//Developer/Extras/CoreAudio/PublicUtility/CAComponentDescription.o:  
	$(CC) $(CFLAGS) -c /Developer/Extras/CoreAudio/PublicUtility/CAComponentDescription.cpp $< -o $@
$(O)//Developer/Extras/CoreAudio/PublicUtility/AUOutputBL.o:  
	$(CC) $(CFLAGS) -c /Developer/Extras/CoreAudio/PublicUtility/AUOutputBL.cpp $< -o $@
$(O)//Developer/Extras/CoreAudio/PublicUtility/CAAudioChannelLayout.o:  
	$(CC) $(CFLAGS) -c /Developer/Extras/CoreAudio/PublicUtility/CAAudioChannelLayout.cpp $< -o $@
$(O)//Developer/Extras/CoreAudio/PublicUtility/CAAudioChannelLayoutObject.o:  
	$(CC) $(CFLAGS) -c /Developer/Extras/CoreAudio/PublicUtility/CAAudioChannelLayoutObject.cpp $< -o $@

$(O)/%.o: $(S)/%.cpp
	$(CC) $(CFLAGS) -c $< -o "$@"

clean:
	rm -f $(OBJECTS)
