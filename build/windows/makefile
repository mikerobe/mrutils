.SUFFIXES: .c .cpp .o .obj

VC_ROOT = C:\Program Files\Microsoft Visual Studio 9.0\VC
VC_INCS = "$(VC_ROOT)\include"
VC_LIBS = "$(VC_ROOT)\lib"

#MYSQL_ROOT = C:\Program Files\MySQL\MySQL Server 5.1
#MYSQL_LIBS = "$(MYSQL_ROOT)\lib\opt"
#MYSQL_LIBS = "mysql"
#MYSQL_INCS = "$(MYSQL_ROOT)\include"

INC_GNUTLS = "C:\Program Files\GnuTLS-2.9.9\include"

## Libs
#LIB_CURL = curllib.lib ws2_32.lib winmm.lib
#LIB_MYSQL = libmysql.lib mysqlclient.lib 
LIB_Z = zlib1.lib zdll.lib zlibwapi.lib
#LIB_Z = zlib1.lib
LIB_DBGHELP = dbghelp.lib
LIB_GNUTLS = libgnutls-26.lib libgnutls-extra-26.lib

## Flags
CFLAGS = /D"BUILD_DLL" 
LFLAGS = /I"./" /I$(INC_GNUTLS)
LIBDIRS = /LIBPATH:"libs"
LIBS = $(LIB_Z) $(LIB_GNUTLS)
#$(LIB_DBGHELP)

## Std Flags
EHsc     = /EHsc             # C functions never throw C++ exceptions
MD       = /D "_AFXDLL" /MD  # Creates a multithreaded DLL using MSVCRT.lib. 
MDd      = /D "_AFXDLL" /MDd # Creates a multithreaded DLL using MSVCRT.lib. 
W1       = /W1               # warning level
W3       = /W3               # warning level
Od       = /Od               # disable optimization
ZI       = /ZI               # Includes debug information in a program database compatible with Edit and Continue.
RTC      = /RTC1             # perform runtime checks

OPTIM    = /D "NDEBUG" $(MD) /Ox /Oi /Ot /GL
DEBUG    = $(MDd) $(ZI) $(Od) $(RTC)

DEBUG_L  = /PDB:"mrutils.pdb"
OPTIM_L  = /OPT:REF /OPT:ICF

CC = cl
STDLIBS = 
LOPTS = $(OPTIM_L)
STDCFLAGS = \
	/D "_MBCS"   \
	$(EHsc)      \
	$(W3)        \
	$(OPTIM)

O = objects
S = ../mrutils/
Obj = \
	$O\critsectex.obj \
	$O\mr_audio.obj \
	$O\mr_beta.obj \
	$O\mr_betamult.obj \
	$O\mr_probit.obj \
	$O\mr_betaseq.obj \
	$O\mr_bufferedreader.obj \
	$O\mr_decoder.obj \
	$O\mr_delim.obj \
	$O\mr_encoder.obj \
	$O\mr_expavg.obj \
	$O\mr_exppois.obj \
	$O\mr_expvar.obj \
	$O\mr_filecpr.obj \
	$O\mr_filermr.obj \
	$O\mr_selinterrupt.obj \
	$O\mr_files.obj \
	$O\mr_gnuplot.obj \
	$O\mr_gzipreader.obj \
	$O\mr_holdingprd.obj \
	$O\mr_imap.obj \
	$O\mr_mysql.obj \
	$O\mr_numerical.obj \
	$O\mr_sendmail.obj \
	$O\mr_sfileh.obj \
	$O\mr_signal.obj \
	$O\mr_sockets.obj \
	$O\mr_strutils.obj \
	$O\mr_termutils.obj \
	$O\mr_time.obj \
	$O\mr_timerqueue.obj \
	$O\mysql_sha1.obj \
	#$O\stacktrace_win.obj \

all: $(Obj)
	$(CC) $(STDCFLAGS) $(CFLAGS) $(LFLAGS) $(Obj) /link /DLL /out:mrutils.dll $(LOPTS) $(LIBDIRS) $(STDLIBS) $(LIBS)

{$S}.cpp{$O}.obj::
	$(CC) $(STDCFLAGS) $(CFLAGS) $(LFLAGS) -Fo$O\ -Fd$O\ -c $<

clean:
	@for %D in ($(Obj)) do \
		@if exist %D @del /F /Q %D 

$(Obj) :
