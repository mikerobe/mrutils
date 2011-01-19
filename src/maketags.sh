#!/bin/bash


CMD="ctags"
ARGS=""

if [[ -e /opt/local/bin/ctags || -e /usr/local/bin/ctags.exe || -e /usr/bin/ctags.exe ]]; then 
    ARGS="-f.tags -R --c++-kinds=+p --fields=+iaS --extra=+q ./"
else
    ARGS="-f.tags -R -I --globals --declarations --members *"
fi

if [[ -e /opt/local/bin/ctags ]]; then 
    CMD="/opt/local/bin/ctags"
elif [[ -e /usr/local/bin/ctags.exe ]]; then 
    CMD="/usr/local/bin/ctags.exe"
fi

$CMD $ARGS



