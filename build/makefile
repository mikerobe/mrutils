PLATFORM=$(shell uname -s)

ifeq ($(PLATFORM),Darwin)
	FAST=-O3 -DNDEBUG
else
	FAST=-O3 -DNDEBUG
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

CC=clang++
CFLAGS=-I$(HOME)/include/stlsoft -pipe -D BUILD_DLL -D OK_OUTSIDE_LIBS $(DEBUG) -Wall -Wextra -Woverloaded-virtual\
	-I/usr/X11/include -std=c++11 -stdlib=libc++ -fcolor-diagnostics
LFLAGS= -fpic -shared -undefined suppress -flat_namespace -fvisibility=hidden -lX11 -lncurses -lX11 -ljpeg -lpng -lz -lsqlite3

OUTPUT_DIR=${HOME}/local/${MACHTYPE_}/lib
ifneq ($(findstring cygwin,${OSTYPE}),)
	OUTPUT_DIR=/usr/bin
endif

######### AUDIO
ifeq ($(PLATFORM),Darwin)
	SOURCES_OTHER += \
		PublicUtility/CAStreamBasicDescription.cpp \
		PublicUtility/CAXException.cpp \
		PublicUtility/CAAudioUnit.cpp \
		PublicUtility/CAComponent.cpp \
		PublicUtility/CAComponentDescription.cpp \
		PublicUtility/AUOutputBL.cpp \
		PublicUtility/CAAudioChannelLayout.cpp \
		PublicUtility/CAAudioChannelLayoutObject.cpp \
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
# stupid mac won't pick up the right location!!!
#ifeq ($(shell type gsl-config > /dev/null 2>&1 && echo 1 || echo 0),1)
#	CFLAGS+= -D _MR_USE_GSL
#	LFLAGS+= -lgsl
#endif

######## GNU TLS (secure sockets)
#	SOURCES_OTHER += \
#		mr_sendmail.cpp \
#		mr_imap.cpp \
#
#	LFLAGS+= -L/usr/bin -lgcrypt -lgsasl -lgnutls -lgnutls-extra -lgpg-error #-liconv 
#	CFLAGS+= -D MR_GNUTLS

# determine whether to compile 64 bit or 32
ifeq ($(shell uname -m),x86_64)
	CFLAGS+= -m64
else
	CFLAGS+= -m32
endif

OBJECTS = $(SOURCES:%.cpp=$(O)/%.o)
OBJECTS+= $(SOURCES_OTHER:%.cpp=$(O)/%.o)

all: $(OBJECTS)
	$(CC) $(OBJECTS) $(LFLAGS) -dynamiclib -o "$(OUTPUT_DIR)/libmrutils.${DYLIB}"
	cd ../src && ./maketags.sh && cd -

$(OBJECTS) : | dirs

.PHONY: dirs

dirs:
	@mkdir -p $O/PublicUtility

$(O)/%.o: $(S)/%.cpp
	$(CC) $(CFLAGS) -c $< -o "$@"

$(O)/PublicUtility/%.o: $(S)/PublicUtility/%.cpp
	$(CC) $(CFLAGS) -c $< -o "$@"

clean:
	rm -f $(OBJECTS)
