// https://github.com/blitcrush/EasyVst
#include <EasyVst.h>

// https://github.com/cameron314/concurrentqueue
#include <concurrentqueue.h>
// http://portaudio.com/
#include <portaudio.h>
//
// https://www.music.mcgill.ca/~gary/rtmidi/
//
// Add __WINDOWS_MM__ or __MACOSX_CORE__ to your preprocessor definitions depending on your target platform
// This is necessary for RtMidi to use the correct backend
//
#include <RtMidi.h>

#include <iostream>
#include <chrono>

struct MidiNoteMessage {
        int noteNum = -1;
        float velocity = 0.0f;
        bool isNoteOn = false;
};

struct UserData {
        EasyVst vst;
        uint64_t continuousSamples = 0;
        moodycamel::ConcurrentQueue<MidiNoteMessage> notesQueue;
};

static const double TEMPO = 120.0;
static const int SAMPLE_RATE = 44100;
static const int MAX_BLOCK_SIZE = 1024;

static int audioCallback(const void *inputBuffer, void *pOutputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *pUserData)
{
        float *outputBuffer = static_cast<float *>(pOutputBuffer);
        UserData *userData = static_cast<UserData *>(pUserData);

        double currentBeat = userData->continuousSamples / ((60.0 / TEMPO) * static_cast<double>(SAMPLE_RATE));

        Steinberg::Vst::ProcessContext *processContext = userData->vst.processContext();
        processContext->state = Steinberg::Vst::ProcessContext::kPlaying;
        processContext->sampleRate = SAMPLE_RATE;
        processContext->projectTimeSamples = userData->continuousSamples;
        processContext->state |= Steinberg::Vst::ProcessContext::kTempoValid;
        processContext->tempo = TEMPO;
        processContext->state |= Steinberg::Vst::ProcessContext::kTimeSigValid;
        processContext->timeSigNumerator = 4;
        processContext->timeSigDenominator = 4;
        processContext->state |= Steinberg::Vst::ProcessContext::kContTimeValid;
        processContext->continousTimeSamples = userData->continuousSamples;
        processContext->state |= Steinberg::Vst::ProcessContext::kSystemTimeValid;
        processContext->systemTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        processContext->state |= Steinberg::Vst::ProcessContext::kProjectTimeMusicValid;
        processContext->projectTimeMusic = currentBeat;
        userData->continuousSamples += framesPerBuffer;

        Steinberg::Vst::EventList *eventList = userData->vst.eventList(Steinberg::Vst::kInput, 0);
        while (true) {
                MidiNoteMessage note;
                bool hasNotes = userData->notesQueue.try_dequeue(note);
                if (!hasNotes) {
                        break;
                }
                
                Steinberg::Vst::Event evt = {};
                evt.busIndex = 0;
                evt.sampleOffset = 0;
                evt.ppqPosition = currentBeat;
                evt.flags = Steinberg::Vst::Event::EventFlags::kIsLive;
                if (note.isNoteOn) {
                        evt.type = Steinberg::Vst::Event::EventTypes::kNoteOnEvent;
                        evt.noteOn.channel = 0;
                        evt.noteOn.pitch = note.noteNum;
                        evt.noteOn.tuning = 0.0f;
                        evt.noteOn.velocity = note.velocity;
                        evt.noteOn.length = 0;
                        evt.noteOn.noteId = -1;
                } else {
                        evt.type = Steinberg::Vst::Event::EventTypes::kNoteOffEvent;
                        evt.noteOff.channel = 0;
                        evt.noteOff.pitch = note.noteNum;
                        evt.noteOff.tuning = 0.0f;
                        evt.noteOff.velocity = note.velocity;
                        evt.noteOff.noteId = -1;
                }
                eventList->addEvent(evt);
        }

        if (!userData->vst.process(framesPerBuffer)) {
                std::cerr << "VST process() failed" << std::endl;
                return 1;
        }

        eventList->clear();

        float *left = userData->vst.channelBuffer32(Steinberg::Vst::kOutput, 0);
        float *right = userData->vst.channelBuffer32(Steinberg::Vst::kOutput, 0);
        for (unsigned long i = 0; i < framesPerBuffer; ++i) {
                outputBuffer[i * 2 + 0] = left[i];
                outputBuffer[i * 2 + 1] = right[i];
        }

        return 0;
}

void midiCallback(double deltaTime, std::vector<unsigned char> *message, void *pUserData)
{
        if (message->size() < 3) {
                return;
        }

        unsigned char command = message->at(0);
        unsigned char noteNum = message->at(1);
        unsigned char velocity = message->at(2);

        UserData *userData = static_cast<UserData *>(pUserData);

        if (command == 144) {
                MidiNoteMessage noteOnMsg;
                noteOnMsg.noteNum = noteNum;
                noteOnMsg.velocity = static_cast<float>(velocity) / 127.0f;
                noteOnMsg.isNoteOn = true;
                userData->notesQueue.enqueue(noteOnMsg);
        } else if (command == 128) {
                MidiNoteMessage noteOffMsg;
                noteOffMsg.noteNum = noteNum;
                noteOffMsg.velocity = static_cast<float>(velocity) / 127.0f;
                noteOffMsg.isNoteOn = false;
                userData->notesQueue.enqueue(noteOffMsg);
        }
}

int main(int argc, char *argv[])
{
        if (argc != 2) {
                std::cerr << "Usage: " << argv[0] << " [VST plugin filename].vst3" << std::endl;
                return 1;
        }

        UserData userData;

        if (!userData.vst.init(argv[1], SAMPLE_RATE, MAX_BLOCK_SIZE, Steinberg::Vst::kSample32, true)) {
                std::cerr << "Failed to initialize VST" << std::endl;
                return 1;
        }

        int numEventInBuses = userData.vst.numBuses(Steinberg::Vst::kEvent, Steinberg::Vst::kInput);
        int numAudioOutBuses = userData.vst.numBuses(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput);
        if (numEventInBuses < 1 || numAudioOutBuses < 1) {
                std::cerr << "Incorrect bus configuration" << std::endl;
                return 1;
        }

        const Steinberg::Vst::BusInfo *outBusInfo = userData.vst.busInfo(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput, 0);
        if (outBusInfo->channelCount != 2) {
                std::cerr << "Invalid output channel configuration" << std::endl;
                return 1;
        }

        userData.vst.setBusActive(Steinberg::Vst::kEvent, Steinberg::Vst::kInput, 0, true);
        userData.vst.setBusActive(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput, 0, true);
        userData.vst.setProcessing(true);

        RtMidiIn *midiIn = nullptr;
        try {
                midiIn = new RtMidiIn();
        } catch (RtMidiError &err) {
                err.printMessage();
                return 1;
        }

        unsigned int numMidiPorts = midiIn->getPortCount();
        if (numMidiPorts == 0) {
                std::cerr << "No MIDI inputs available on your system" << std::endl;
                return 1;
        }
        std::cout << "Please type the number that corresponds to the MIDI device you'd like to use and press ENTER" << std::endl;
        for (unsigned int i = 0; i < numMidiPorts; ++i) {
                try {
                        std::string portName = midiIn->getPortName(i);
                        std::cout << (i + 1) << ") " << portName << std::endl;
                } catch (RtMidiError &err) {
                        err.printMessage();
                        return 1;
                }
        }
        int selectedIndex = 0;
        std::cin >> selectedIndex;
        --selectedIndex;
        if (selectedIndex < 0 || selectedIndex >= numMidiPorts) {
                std::cerr << "Invalid MIDI device selection" << std::endl;
                return 1;
        }
        std::cout << "You have selected device: " << selectedIndex + 1 << std::endl;
        try {
                midiIn->openPort(selectedIndex);
                midiIn->setCallback(&midiCallback, &userData);
        } catch (RtMidiError &err) {
                err.printMessage();
                return 1;
        }

        if (!userData.vst.createView()) {
                std::cerr << "Failed to create VST view" << std::endl;
                return 1;
        }

        PaError err = Pa_Initialize();
        if (err != paNoError) {
                std::cerr << "Failed to initialize PortAudio" << std::endl;
                return 1;
        }

        PaStream *stream = nullptr;
        err = Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, SAMPLE_RATE, MAX_BLOCK_SIZE, audioCallback, &userData);
        if (err != paNoError) {
                std::cerr << "Failed to open PortAudio stream" << std::endl;
                return 1;
        }

        err = Pa_StartStream(stream);
        if (err != paNoError) {
                std::cerr << "Failed to start PortAudio stream" << std::endl;
                return 1;
        }

        std::vector<unsigned char> midiMessage;
        bool running = true;
        while (running) {
                SDL_Event evt;
                while (SDL_PollEvent(&evt)) {
                        if (evt.type == SDL_QUIT) {
                                running = false;
                        }
                        userData.vst.processSdlEvent(evt);
                }

                Pa_Sleep(16);
        }

        err = Pa_CloseStream(stream);
        if (err != paNoError) {
                std::cerr << "Failed to close PortAudio stream" << std::endl;
                return 1;
        }

        err = Pa_Terminate();
        if (err != paNoError) {
                std::cerr << "Failed to terminate PortAudio" << std::endl;
                return 1;
        }

        delete midiIn;

        return 0;
}
