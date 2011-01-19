#ifndef _MR_CPPLIB_AUDIO_H_
#define _MR_CPPLIB_AUDIO_H_

#include <iostream>
#include "mr_threads.h"

#if defined(__APPLE__)
    // mac includes
    #include <AudioToolbox/AudioToolbox.h>
    #include "CAXException.h"
    #include "CAStreamBasicDescription.h"
    #include "CAAudioUnit.h"
#elif defined(__LINUX__)
    #include <sox.h>
#endif

namespace mrutils {

class AudioFile {
    public:
        AudioFile(const char * path = NULL);

        ~AudioFile();

        bool open(const char * path);
        void play();

        inline bool good() { return open_; }

    private:
        bool open_;

    private:
    #if defined(__APPLE__)

       /****************
        * Mac
        ****************/

        double PrepareFileAU (CAAudioUnit &au, CAStreamBasicDescription &fileFormat, AudioFileID audioFile);
        void MakeSimpleGraph (AUGraph &theGraph, CAAudioUnit &fileAU, CAStreamBasicDescription &fileFormat, AudioFileID audioFile);

        AudioFileID audioFile;
        AUGraph theGraph;
        CAAudioUnit fileAU;
        Float64 fileDuration;
        CAStreamBasicDescription fileFormat;
        AudioTimeStamp position;

    #elif defined(__LINUX__)

       /****************
        * SOX
        ****************/

        sox_format_t * in, * out;
        char * buffer; size_t buffSz;
        static const size_t maxSamples=2048;
        sox_sample_t samples[maxSamples];

    #endif
};

}

#endif
