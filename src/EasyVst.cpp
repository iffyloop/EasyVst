#include <EasyVst.h>

Steinberg::Vst::HostApplication *EasyVst::_standardPluginContext = nullptr;
int EasyVst::_standardPluginContextRefCount = 0;

using namespace Steinberg;
using namespace Steinberg::Vst;

EasyVst::EasyVst()
{}

EasyVst::~EasyVst()
{
	destroy();
}

bool EasyVst::init(const std::string &path, int sampleRate, int maxBlockSize, int symbolicSampleSize, bool realtime)
{
	_destroy(false);

	++_standardPluginContextRefCount;
	if (!_standardPluginContext) {
		_standardPluginContext = owned(NEW HostApplication());
		PluginContextFactory::instance().setPluginContext(_standardPluginContext);
	}

	_path = path;
	_sampleRate = sampleRate;
	_maxBlockSize = maxBlockSize;
	_symbolicSampleSize = symbolicSampleSize;
	_realtime = realtime;

	_processSetup.processMode = _realtime;
	_processSetup.symbolicSampleSize = _symbolicSampleSize;
	_processSetup.sampleRate = _sampleRate;
	_processSetup.maxSamplesPerBlock = _maxBlockSize;

	_processData.numSamples = 0;
	_processData.symbolicSampleSize = _symbolicSampleSize;
	_processData.processContext = &_processContext;

	std::string error;
	_module = VST3::Hosting::Module::create(path, error);
	if (!_module) {
		_printError(error);
		return false;
	}

	VST3::Hosting::PluginFactory factory = _module->getFactory();
	for (auto &classInfo : factory.classInfos()) {
		if (classInfo.category() == kVstAudioEffectClass) {
			_plugProvider = owned(NEW PlugProvider(factory, classInfo, true));
			if (!_plugProvider) {
				_printError("No PlugProvider found");
				return false;
			}

			_vstPlug = _plugProvider->getComponent();

			_audioEffect = FUnknownPtr<IAudioProcessor>(_vstPlug);
			if (!_audioEffect) {
				_printError("Could not get audio processor from VST");
				return false;
			}

			_editController = _plugProvider->getController();

			_name = classInfo.name();

			FUnknownPtr<IProcessContextRequirements> contextRequirements(_audioEffect);
			if (contextRequirements) {
				auto flags = contextRequirements->getProcessContextRequirements();

#define PRINT_FLAG(x) if (flags & IProcessContextRequirements::Flags::x) { _printDebug(#x); }
				PRINT_FLAG(kNeedSystemTime)
					PRINT_FLAG(kNeedContinousTimeSamples)
					PRINT_FLAG(kNeedProjectTimeMusic)
					PRINT_FLAG(kNeedBarPositionMusic)
					PRINT_FLAG(kNeedCycleMusic)
					PRINT_FLAG(kNeedSamplesToNextClock)
					PRINT_FLAG(kNeedTempo)
					PRINT_FLAG(kNeedTimeSignature)
					PRINT_FLAG(kNeedChord)
					PRINT_FLAG(kNeedFrameRate)
					PRINT_FLAG(kNeedTransportState)
#undef PRINT_FLAG
			}

			_numInAudioBuses = _vstPlug->getBusCount(MediaTypes::kAudio, BusDirections::kInput);
			_numOutAudioBuses = _vstPlug->getBusCount(MediaTypes::kAudio, BusDirections::kOutput);
			_numInEventBuses = _vstPlug->getBusCount(MediaTypes::kEvent, BusDirections::kInput);
			_numOutEventBuses = _vstPlug->getBusCount(MediaTypes::kEvent, BusDirections::kOutput);

			std::ostringstream debugOss;
			debugOss << "Buses: " << _numInAudioBuses << " audio and " << _numInEventBuses << " event inputs; ";
			debugOss << _numOutAudioBuses << " audio and " << _numOutEventBuses << " event outputs";
			_printDebug(debugOss.str());

			for (int i = 0; i < _numInAudioBuses; ++i) {
				BusInfo info;
				_vstPlug->getBusInfo(kAudio, kInput, i, info);
				_inAudioBusInfos.push_back(info);
				setBusActive(kAudio, kInput, i, false);

				SpeakerArrangement speakerArr;
				_audioEffect->getBusArrangement(kInput, i, speakerArr);
				_inSpeakerArrs.push_back(speakerArr);
			}

			for (int i = 0; i < _numInEventBuses; ++i) {
				BusInfo info;
				_vstPlug->getBusInfo(kEvent, kInput, i, info);
				_inEventBusInfos.push_back(info);
				setBusActive(kEvent, kInput, i, false);
			}

			for (int i = 0; i < _numOutAudioBuses; ++i) {
				BusInfo info;
				_vstPlug->getBusInfo(kAudio, kOutput, i, info);
				_outAudioBusInfos.push_back(info);
				setBusActive(kAudio, kOutput, i, false);

				SpeakerArrangement speakerArr;
				_audioEffect->getBusArrangement(kOutput, i, speakerArr);
				_outSpeakerArrs.push_back(speakerArr);
			}

			for (int i = 0; i < _numOutEventBuses; ++i) {
				BusInfo info;
				_vstPlug->getBusInfo(kEvent, kOutput, i, info);
				_outEventBusInfos.push_back(info);
				setBusActive(kEvent, kOutput, i, false);
			}

			tresult res = _audioEffect->setBusArrangements(_inSpeakerArrs.data(), _numInAudioBuses, _outSpeakerArrs.data(), _numOutAudioBuses);
			if (res != kResultTrue) {
				_printError("Failed to set bus arrangements");
				return false;
			}

			res = _audioEffect->setupProcessing(_processSetup);
			if (res == kResultOk) {
				_processData.prepare(*_vstPlug, _maxBlockSize, _processSetup.symbolicSampleSize);
				if (_numInEventBuses > 0) {
					_processData.inputEvents = new EventList[_numInEventBuses];
				}
				if (_numOutEventBuses > 0) {
					_processData.outputEvents = new EventList[_numOutEventBuses];
				}
			} else {
				_printError("Failed to setup VST processing");
				return false;
			}

			if (_vstPlug->setActive(true) != kResultTrue) {
				_printError("Failed to activate VST component");
				return false;
			}
		}
	}

	return true;
}

void EasyVst::destroy()
{
	_destroy(true);
}

bool EasyVst::process(int numSamples)
{
	if (numSamples > _maxBlockSize) {
#ifdef _DEBUG
		_printError("numSamples > _maxBlockSize");
#endif
		numSamples = _maxBlockSize;
	}

	_processData.numSamples = numSamples;
	tresult result = _audioEffect->process(_processData);
	if (result != kResultOk) {
#ifdef _DEBUG
		std::cerr << "VST process failed" << std::endl;
#endif
		return false;
	}

	return true;
}

const Steinberg::Vst::BusInfo *EasyVst::busInfo(Steinberg::Vst::MediaType type, Steinberg::Vst::BusDirection direction, int which)
{
	if (type == kAudio) {
		if (direction == kInput) {
			return &_inAudioBusInfos[which];
		} else if (direction == kOutput) {
			return &_outAudioBusInfos[which];
		} else {
			return nullptr;
		}
	} else if (type == kEvent) {
		if (direction == kInput) {
			return &_inEventBusInfos[which];
		} else if (direction == kOutput) {
			return &_outEventBusInfos[which];
		} else {
			return nullptr;
		}
	} else {
		return nullptr;
	}
}

int EasyVst::numBuses(Steinberg::Vst::MediaType type, Steinberg::Vst::BusDirection direction)
{
	if (type == kAudio) {
		if (direction == kInput) {
			return _numInAudioBuses;
		} else if (direction == kOutput) {
			return _numOutAudioBuses;
		} else {
			return 0;
		}
	} else if (type == kEvent) {
		if (direction == kInput) {
			return _numInEventBuses;
		} else if (direction == kOutput) {
			return _numOutEventBuses;
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}

void EasyVst::setBusActive(MediaType type, BusDirection direction, int which, bool active)
{
	_vstPlug->activateBus(type, direction, which, active);
}

void EasyVst::setProcessing(bool processing)
{
	_audioEffect->setProcessing(processing);
}

Steinberg::Vst::ProcessContext *EasyVst::processContext()
{
	return &_processContext;
}

Steinberg::Vst::Sample32 *EasyVst::channelBuffer32(BusDirection direction, int which)
{
	if (direction == kInput) {
		return _processData.inputs->channelBuffers32[which];
	} else if (direction == kOutput) {
		return _processData.outputs->channelBuffers32[which];
	} else {
		return nullptr;
	}
}

Steinberg::Vst::Sample64 *EasyVst::channelBuffer64(BusDirection direction, int which)
{
	if (direction == kInput) {
		return _processData.inputs->channelBuffers64[which];
	} else if (direction == kOutput) {
		return _processData.outputs->channelBuffers64[which];
	} else {
		return nullptr;
	}
}

Steinberg::Vst::EventList *EasyVst::eventList(Steinberg::Vst::BusDirection direction, int which)
{
	if (direction == kInput) {
		return static_cast<Steinberg::Vst::EventList *>(&_processData.inputEvents[which]);
	} else if (direction == kOutput) {
		return static_cast<Steinberg::Vst::EventList *>(&_processData.outputEvents[which]);
	} else {
		return nullptr;
	}
}

Steinberg::Vst::ParameterChanges *EasyVst::parameterChanges(Steinberg::Vst::BusDirection direction, int which)
{
	if (direction == kInput) {
		return static_cast<Steinberg::Vst::ParameterChanges *>(&_processData.inputParameterChanges[which]);
	} else if (direction == kOutput) {
		return static_cast<Steinberg::Vst::ParameterChanges *>(&_processData.outputParameterChanges[which]);
	} else {
		return nullptr;
	}
}

bool EasyVst::createView()
{
	if (!_editController) {
		_printError("VST does not provide an edit controller");
		return false;
	}

	if (_view || _window) {
		_printDebug("Editor view or window already exists");
		return false;
	}

	_view = _editController->createView(ViewType::kEditor);
	if (!_view) {
		_printError("EditController does not provide its own view");
		return false;
	}

	ViewRect viewRect = {};
	if (_view->getSize(&viewRect) != kResultOk) {
		_printError("Failed to get editor view size");
		return false;
	}

#ifdef _WIN32
	if (_view->isPlatformTypeSupported(Steinberg::kPlatformTypeHWND) != Steinberg::kResultTrue) {
		_printError("Editor view does not support HWND");
		return false;
}
#else
	_printError("Platform is not supported yet");
	return false;
#endif

	_window = SDL_CreateWindow(_name.data(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, viewRect.getWidth(), viewRect.getHeight(), SDL_WINDOW_SHOWN);
	SDL_SetWindowData(_window, "EasyVstInstance", this);

	SDL_SysWMinfo wmInfo = {};
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(_window, &wmInfo);

#ifdef _WIN32
	if (_view->attached(wmInfo.info.win.window, Steinberg::kPlatformTypeHWND) != Steinberg::kResultOk) {
		_printError("Failed to attach editor view to HWND");
		return false;
	}
#endif

	return true;
	}

void EasyVst::destroyView()
{
	if (_window) {
		SDL_DestroyWindow(_window);
		_window = nullptr;
	}

	if (_view) {
		_view = nullptr;
	}
}

void EasyVst::processSdlEvent(const SDL_Event &event)
{
	if (event.type == SDL_WINDOWEVENT) {
		if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
			SDL_Window *window = SDL_GetWindowFromID(event.window.windowID);
			EasyVst *target = static_cast<EasyVst *>(SDL_GetWindowData(window, "EasyVstInstance"));
			if (target) {
				target->destroyView();
			}
		}
	}
}

const std::string &EasyVst::name()
{
	return _name;
}

void EasyVst::_destroy(bool decrementRefCount)
{
	destroyView();

	_editController = nullptr;
	_audioEffect = nullptr;
	_vstPlug = nullptr;
	_plugProvider = nullptr;
	_module = nullptr;

	_inAudioBusInfos.clear();
	_outAudioBusInfos.clear();
	_numInAudioBuses = 0;
	_numOutAudioBuses = 0;

	_inEventBusInfos.clear();
	_outEventBusInfos.clear();
	_numInEventBuses = 0;
	_numOutEventBuses = 0;

	_inSpeakerArrs.clear();
	_outSpeakerArrs.clear();

	if (_processData.inputEvents) {
		delete[] static_cast<Steinberg::Vst::EventList *>(_processData.inputEvents);
	}
	if (_processData.outputEvents) {
		delete[] static_cast<Steinberg::Vst::EventList *>(_processData.outputEvents);
	}
	_processData.unprepare();
	_processData = {};

	_processSetup = {};
	_processContext = {};

	_sampleRate = 0;
	_maxBlockSize = 0;
	_symbolicSampleSize = 0;
	_realtime = false;

	_path = "";
	_name = "";

	if (decrementRefCount) {
		if (_standardPluginContextRefCount > 0) {
			--_standardPluginContextRefCount;
		}
		if (_standardPluginContext && _standardPluginContextRefCount == 0) {
			PluginContextFactory::instance().setPluginContext(nullptr);
			_standardPluginContext->release();
			delete _standardPluginContext;
			_standardPluginContext = nullptr;
		}
	}
}

void EasyVst::_printDebug(const std::string &info)
{
	std::cout << "Debug info for VST3 plugin \"" << _path << "\": " << info << std::endl;
}

void EasyVst::_printError(const std::string &error)
{
	std::cerr << "Error loading VST3 plugin \"" << _path << "\": " << error << std::endl;
}
