#pragma once

#include <sstream>
#include <iostream>

#include <public.sdk/source/vst/hosting/plugprovider.h>
#include <public.sdk/source/vst/hosting/module.h>
#include <public.sdk/source/vst/hosting/hostclasses.h>
#include <public.sdk/source/vst/hosting/eventlist.h>
#include <public.sdk/source/vst/hosting/parameterchanges.h>
#include <public.sdk/source/vst/hosting/processdata.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstprocesscontext.h>
#include <pluginterfaces/gui/iplugview.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

class EasyVst {
public:
	EasyVst();
	~EasyVst();

	bool init(const std::string &path, int sampleRate, int maxBlockSize, int symbolicSampleSize, bool realtime);
	void destroy();

	Steinberg::Vst::ProcessContext *processContext();
	void setProcessing(bool processing);
	bool process(int numSamples);

	const Steinberg::Vst::BusInfo *busInfo(Steinberg::Vst::MediaType type, Steinberg::Vst::BusDirection direction, int which);
	int numBuses(Steinberg::Vst::MediaType type, Steinberg::Vst::BusDirection direction);
	void setBusActive(Steinberg::Vst::MediaType type, Steinberg::Vst::BusDirection direction, int which, bool active);

	Steinberg::Vst::Sample32 *channelBuffer32(Steinberg::Vst::BusDirection direction, int which);
	Steinberg::Vst::Sample64 *channelBuffer64(Steinberg::Vst::BusDirection direction, int which);

	Steinberg::Vst::EventList *eventList(Steinberg::Vst::BusDirection direction, int which);
	Steinberg::Vst::ParameterChanges *parameterChanges(Steinberg::Vst::BusDirection direction, int which);

	bool createView();
	void destroyView();
	static void processSdlEvent(const SDL_Event &event);

	const std::string &name();

private:
	void _destroy(bool decrementRefCount);

	void _printDebug(const std::string &info);
	void _printError(const std::string &error);

	std::vector<Steinberg::Vst::BusInfo> _inAudioBusInfos, _outAudioBusInfos;
	int _numInAudioBuses = 0, _numOutAudioBuses = 0;

	std::vector<Steinberg::Vst::BusInfo> _inEventBusInfos, _outEventBusInfos;
	int _numInEventBuses = 0, _numOutEventBuses = 0;

	std::vector<Steinberg::Vst::SpeakerArrangement> _inSpeakerArrs, _outSpeakerArrs;

	VST3::Hosting::Module::Ptr _module = nullptr;
	Steinberg::IPtr<Steinberg::Vst::PlugProvider> _plugProvider = nullptr;

	Steinberg::IPtr<Steinberg::Vst::IComponent> _vstPlug = nullptr;
	Steinberg::IPtr<Steinberg::Vst::IAudioProcessor> _audioEffect = nullptr;
	Steinberg::IPtr<Steinberg::Vst::IEditController> _editController = nullptr;
	Steinberg::Vst::HostProcessData _processData = {};
	Steinberg::Vst::ProcessSetup _processSetup = {};
	Steinberg::Vst::ProcessContext _processContext = {};

	Steinberg::IPtr<Steinberg::IPlugView> _view = nullptr;
	SDL_Window *_window = nullptr;

	int _sampleRate = 0, _maxBlockSize = 0, _symbolicSampleSize = 0;
	bool _realtime = false;

	std::string _path;
	std::string _name;

	static Steinberg::Vst::HostApplication *_standardPluginContext;
	static int _standardPluginContextRefCount;
};
