#include "mr_audio.h"
#include "mr_files.h"
#include "mr_threads.h"

using namespace mrutils;

AudioFile::AudioFile(const char * path) 
:open_(false)
#if defined(_MR_AUDIO_SOX)
,in(NULL),out(NULL),buffer(NULL)
#endif
{
    #if defined(_MR_AUDIO_SOX)
        static MUTEX mutex = mrutils::mutexCreate();
        static bool calledSoxInit = false;
        mrutils::mutexAcquire(mutex);
        if (!calledSoxInit) {
            if (sox_init() != SOX_SUCCESS) 
                perror("error initializing sox");
        } calledSoxInit = true;
        mrutils::mutexRelease(mutex);
    #endif

    if (path != NULL) 
        open_ = open(path);
}

AudioFile::~AudioFile() {
    #if defined(__APPLE__)
        if (   0== AUGraphStop (theGraph)
            && 0== AUGraphUninitialize (theGraph)
            && 0== AudioFileClose (audioFile)
            && 0== AUGraphClose (theGraph)
            ) {}
    #elif defined(_MR_AUDIO_SOX)
        if (out != NULL) sox_close(out);
        free(buffer);
    #endif
}

bool AudioFile::open(const char * path) {
    #if defined(__APPLE__)
        if (!fileExists(path)) return false;

        CFURLRef theURL = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (UInt8*)path, strlen(path), false);

        if (0!=AudioFileOpenURL (theURL, kAudioFileReadPermission, 0, &audioFile)) 
            return false;
                
        // get the number of channels of the file
        CAStreamBasicDescription fileFormat;
        UInt32 propsize = sizeof(CAStreamBasicDescription);
        if (0!=AudioFileGetProperty(audioFile, kAudioFilePropertyDataFormat, &propsize, &fileFormat)) 
            return false;

        //printf ("playing file: %s\n", path);
        //printf ("format: "); fileFormat.Print();

        MakeSimpleGraph (theGraph, fileAU, fileFormat, audioFile);
        fileDuration = PrepareFileAU (fileAU, fileFormat, audioFile);
    #elif defined(_MR_AUDIO_SOX)
        static const char * outFormat = "alsa";
        //static const char * outPath = "/dev/snd/pcmC0D0p";
        static const char * outPath = "default";

        if (NULL == (in = sox_open_read(path,NULL,NULL,NULL))) {
            fprintf(stderr,"Unable to open file at %s\n",path); return false;
        }

        if (NULL == (out = sox_open_memstream_write(&buffer,&buffSz,&in->signal,NULL,"sox",NULL))) {
            fprintf(stderr,"Unable to allocate stream for file at %s\n",path); return false;
        }

        /** read into memory **/
        for (size_t r; 0 != (r=sox_read(in,samples,maxSamples))
            ;sox_write(out,samples,r));
        sox_close(out); sox_close(in);

        /** open connection to speakers **/
        in=sox_open_mem_read(buffer,buffSz,NULL,NULL,NULL);
        if (NULL == (out=sox_open_write(outPath,&in->signal,NULL,outFormat,NULL,NULL))) {
            fprintf(stderr,"Unable to open output (%s, %s)\n",outPath,outFormat);
            return false;
        } sox_close(in);
    #else
        return false;
    #endif

    return true;
}

void AudioFile::play() {
    if (!open_) return;

    #if defined(__APPLE__)
        if (0!=AUGraphStart (theGraph)) return;
        mrutils::sleep(fileDuration * 1000.);
        AUGraphStop (theGraph);
    #elif defined(_MR_AUDIO_SOX)
        in=sox_open_mem_read(buffer,buffSz,NULL,NULL,NULL);

        for (size_t r; 0 != (r=sox_read(in,samples,maxSamples))
            ;sox_write(out,samples,r));

        // flush remainder
        sox_close(out);
        static const char * outFormat = "alsa";
        static const char * outPath = "default";
        if (NULL == (out=sox_open_write(outPath,&in->signal,NULL,outFormat,NULL,NULL))) {
            fprintf(stderr,"Unable to open output (%s, %s)\n",outPath,outFormat);
            open_ = false;
        }

        sox_close(in);
    #endif
}


#if defined(__APPLE__)
   /************************
    * Mac 
    ************************/

    double AudioFile::PrepareFileAU (CAAudioUnit &au, CAStreamBasicDescription &fileFormat, AudioFileID audioFile)
    {	
    // 
            // calculate the duration
        UInt64 nPackets;
        UInt32 propsize = sizeof(nPackets);
        XThrowIfError (AudioFileGetProperty(audioFile, kAudioFilePropertyAudioDataPacketCount, &propsize, &nPackets), "kAudioFilePropertyAudioDataPacketCount");
            
        Float64 fileDuration = (nPackets * fileFormat.mFramesPerPacket) / fileFormat.mSampleRate;

        ScheduledAudioFileRegion rgn;
        memset (&rgn.mTimeStamp, 0, sizeof(rgn.mTimeStamp));
        rgn.mTimeStamp.mFlags = kAudioTimeStampSampleTimeValid;
        rgn.mTimeStamp.mSampleTime = 0;
        rgn.mCompletionProc = NULL;
        rgn.mCompletionProcUserData = NULL;
        rgn.mAudioFile = audioFile;
        rgn.mLoopCount = 0;
        rgn.mStartFrame = 0;
        rgn.mFramesToPlay = UInt32(nPackets * fileFormat.mFramesPerPacket);
            
            // tell the file player AU to play all of the file
        XThrowIfError (au.SetProperty (kAudioUnitProperty_ScheduledFileRegion, 
                kAudioUnitScope_Global, 0,&rgn, sizeof(rgn)), "kAudioUnitProperty_ScheduledFileRegion");
        
            // prime the fp AU with default values
        UInt32 defaultVal = 0;
        XThrowIfError (au.SetProperty (kAudioUnitProperty_ScheduledFilePrime, 
                kAudioUnitScope_Global, 0, &defaultVal, sizeof(defaultVal)), "kAudioUnitProperty_ScheduledFilePrime");

            // tell the fp AU when to start playing (this ts is in the AU's render time stamps; -1 means next render cycle)
        AudioTimeStamp startTime;
        memset (&startTime, 0, sizeof(startTime));
        startTime.mFlags = kAudioTimeStampSampleTimeValid;
        startTime.mSampleTime = -1;
        XThrowIfError (au.SetProperty(kAudioUnitProperty_ScheduleStartTimeStamp, 
                kAudioUnitScope_Global, 0, &startTime, sizeof(startTime)), "kAudioUnitProperty_ScheduleStartTimeStamp");

        return fileDuration;
    }



    void AudioFile::MakeSimpleGraph (AUGraph &theGraph, CAAudioUnit &fileAU, CAStreamBasicDescription &fileFormat, AudioFileID audioFile)
    {
        XThrowIfError (NewAUGraph (&theGraph), "NewAUGraph");
        
        CAComponentDescription cd;

        // output node
        cd.componentType = kAudioUnitType_Output;
        cd.componentSubType = kAudioUnitSubType_DefaultOutput;
        cd.componentManufacturer = kAudioUnitManufacturer_Apple;

        AUNode outputNode;
        XThrowIfError (AUGraphAddNode (theGraph, &cd, &outputNode), "AUGraphAddNode");
        
        // file AU node
        AUNode fileNode;
        cd.componentType = kAudioUnitType_Generator;
        cd.componentSubType = kAudioUnitSubType_AudioFilePlayer;
        
        XThrowIfError (AUGraphAddNode (theGraph, &cd, &fileNode), "AUGraphAddNode");
        
        // connect & setup
        XThrowIfError (AUGraphOpen (theGraph), "AUGraphOpen");
        
        // install overload listener to detect when something is wrong
        AudioUnit anAU;
        XThrowIfError (AUGraphNodeInfo(theGraph, fileNode, NULL, &anAU), "AUGraphNodeInfo");
        
        fileAU = CAAudioUnit (fileNode, anAU);

    // prepare the file AU for playback
    // set its output channels
        XThrowIfError (fileAU.SetNumberChannels (kAudioUnitScope_Output, 0, fileFormat.NumberChannels()), "SetNumberChannels");

    // set the output sample rate of the file AU to be the same as the file:
        XThrowIfError (fileAU.SetSampleRate (kAudioUnitScope_Output, 0, fileFormat.mSampleRate), "SetSampleRate");

    // load in the file 
        XThrowIfError (fileAU.SetProperty(kAudioUnitProperty_ScheduledFileIDs, 
                            kAudioUnitScope_Global, 0, &audioFile, sizeof(audioFile)), "SetScheduleFile");


        XThrowIfError (AUGraphConnectNodeInput (theGraph, fileNode, 0, outputNode, 0), "AUGraphConnectNodeInput");

    // AT this point we make sure we have the file player AU initialized
    // this also propogates the output format of the AU to the output unit
        XThrowIfError (AUGraphInitialize (theGraph), "AUGraphInitialize");
        
        // workaround a race condition in the file player AU
        usleep (10 * 1000);

    // if we have a surround file, then we should try to tell the output AU what the order of the channels will be
        if (fileFormat.NumberChannels() > 2) {
            UInt32 layoutSize = 0;
            OSStatus err;
            XThrowIfError (err = AudioFileGetPropertyInfo (audioFile, kAudioFilePropertyChannelLayout, &layoutSize, NULL),
                                    "kAudioFilePropertyChannelLayout");
            
            if (!err && layoutSize) {
                char* layout = new char[layoutSize];
                
                err = AudioFileGetProperty(audioFile, kAudioFilePropertyChannelLayout, &layoutSize, layout);
                XThrowIfError (err, "Get Layout From AudioFile");
                
                // ok, now get the output AU and set its layout
                XThrowIfError (AUGraphNodeInfo(theGraph, outputNode, NULL, &anAU), "AUGraphNodeInfo");
                
                err = AudioUnitSetProperty (anAU, kAudioUnitProperty_AudioChannelLayout, 
                                kAudioUnitScope_Input, 0, layout, layoutSize);
                XThrowIfError (err, "kAudioUnitProperty_AudioChannelLayout");
                
                delete [] layout;
            }
        }
    }
#endif
