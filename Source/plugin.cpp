/*
MIT License

Copyright (c) 2016-2017

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once
#include "plugin.h"
#include "api-base.h"
#include "amf.h"
#include "amf-capabilities.h"
#include <sstream>

#ifdef WITH_AVC
#include "enc-h264.h"
#endif
#ifdef WITH_HEVC
#include "enc-h265.h"
#endif
#ifdef _WIN32
#include <windows.h>
#endif

using namespace Plugin;
using namespace Plugin::AMD;

//////////////////////////////////////////////////////////////////////////
// Code
//////////////////////////////////////////////////////////////////////////
BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
	return TRUE;
}

OBS_DECLARE_MODULE();
OBS_MODULE_AUTHOR("Michael Fabian Dirks");
OBS_MODULE_USE_DEFAULT_LOCALE("enc-amf", "en-US");

#ifdef _DEBUG
#include "components/VideoEncoderVCE.h"
#include "components/VideoEncoderHEVC.h"

static std::string fastPrintVariant(const char* text, amf::AMFVariantStruct variant) {
	std::vector<char> buf(1024);
	switch (variant.type) {
		case amf::AMF_VARIANT_EMPTY:
			std::snprintf(buf.data(), buf.size(), "%s%s", text, "Empty");
			break;
		case amf::AMF_VARIANT_BOOL:
			std::snprintf(buf.data(), buf.size(), "%s%s", text, variant.boolValue ? "true" : "false");
			break;
		case amf::AMF_VARIANT_INT64:
			std::snprintf(buf.data(), buf.size(), "%s%lld", text, variant.int64Value);
			break;
		case amf::AMF_VARIANT_DOUBLE:
			std::snprintf(buf.data(), buf.size(), "%s%f", text, variant.doubleValue);
			break;
		case amf::AMF_VARIANT_RECT:
			std::snprintf(buf.data(), buf.size(), "%s[%ld,%ld,%ld,%ld]", text,
				variant.rectValue.top, variant.rectValue.left,
				variant.rectValue.bottom, variant.rectValue.right);
			break;
		case amf::AMF_VARIANT_SIZE:
			std::snprintf(buf.data(), buf.size(), "%s%ldx%ld", text,
				variant.sizeValue.width, variant.sizeValue.height);
			break;
		case amf::AMF_VARIANT_POINT:
			std::snprintf(buf.data(), buf.size(), "%s[%ld,%ld]", text,
				variant.pointValue.x, variant.pointValue.y);
			break;
		case amf::AMF_VARIANT_RATE:
			std::snprintf(buf.data(), buf.size(), "%s%ld/%ld", text,
				variant.rateValue.num, variant.rateValue.den);
			break;
		case amf::AMF_VARIANT_RATIO:
			std::snprintf(buf.data(), buf.size(), "%s%ld:%ld", text,
				variant.ratioValue.num, variant.ratioValue.den);
			break;
		case amf::AMF_VARIANT_COLOR:
			std::snprintf(buf.data(), buf.size(), "%s(%d,%d,%d,%d)", text,
				variant.colorValue.r,
				variant.colorValue.g,
				variant.colorValue.b,
				variant.colorValue.a);
			break;
		case amf::AMF_VARIANT_STRING:
			std::snprintf(buf.data(), buf.size(), "%s'%s'", text,
				variant.stringValue);
			break;
		case amf::AMF_VARIANT_WSTRING:
			std::snprintf(buf.data(), buf.size(), "%s'%ls'", text,
				variant.wstringValue);
			break;
	}
	return std::string(buf.data());
};

static void printDebugInfo(amf::AMFComponentPtr m_AMFEncoder) {
	amf::AMFPropertyInfo* pInfo;
	size_t propCount = m_AMFEncoder->GetPropertyCount();
	PLOG_INFO("-- Internal AMF Encoder Properties --");
	for (size_t propIndex = 0; propIndex < propCount; propIndex++) {
		static const char* typeToString[] = {
			"Empty",
			"Boolean",
			"Int64",
			"Double",
			"Rect",
			"Size",
			"Point",
			"Rate",
			"Ratio",
			"Color",
			"String",
			"WString",
			"Interface"
		};

		AMF_RESULT res = m_AMFEncoder->GetPropertyInfo(propIndex, (const amf::AMFPropertyInfo**) &pInfo);
		if (res != AMF_OK)
			continue;

		amf::AMFVariantStruct curStruct = amf::AMFVariantStruct();
		m_AMFEncoder->GetProperty(pInfo->name, &curStruct);

		auto vcur = fastPrintVariant("Current: ", curStruct);
		auto vdef = fastPrintVariant("Default: ", pInfo->defaultValue);
		auto vmin = fastPrintVariant("Minimum: ", pInfo->minValue);
		auto vmax = fastPrintVariant("Maximum: ", pInfo->maxValue);
		std::stringstream venum;
		if (pInfo->pEnumDescription) {
			const amf::AMFEnumDescriptionEntry* pEnumEntry = pInfo->pEnumDescription;
			while (pEnumEntry->name != nullptr) {
				QUICK_FORMAT_MESSAGE(tmp, "%ls[%ld]", pEnumEntry->name, pEnumEntry->value);
				venum << tmp.c_str() << "; ";
				pEnumEntry++;
			}
		}

		PLOG_INFO("%ls(Description: %ls, Type: %s, Index %d, Content Type: %d, Access: %s%s%s, Values: {%s, %s, %s, %s%s%s})",
			pInfo->name,
			pInfo->desc,
			typeToString[pInfo->type],
			propIndex,
			pInfo->contentType,
			(pInfo->accessType & amf::AMF_PROPERTY_ACCESS_READ) ? "R" : "",
			(pInfo->accessType & amf::AMF_PROPERTY_ACCESS_WRITE) ? "W" : "",
			(pInfo->accessType & amf::AMF_PROPERTY_ACCESS_WRITE_RUNTIME) ? "X" : "",
			vcur.c_str(), vdef.c_str(), vmin.c_str(), vmax.c_str(),
			(venum.str().length() > 0) ? ", Enum: " : "", venum.str().c_str()
		);
	}
}
#endif

/**
* Required: Called when the module is loaded.  Use this function to load all
* the sources/encoders/outputs/services for your module, or anything else that
* may need loading.
*
* @return           Return true to continue loading the module, otherwise
*                   false to indicate failure and unload the module
*/
MODULE_EXPORT bool obs_module_load(void) {
	PLOG_DEBUG("<" __FUNCTION_NAME__ "> Loading...");

	// AMF
	try {
		Plugin::AMD::AMF::Initialize();
	} catch (const std::exception& e) {
		PLOG_ERROR("Encountered Exception during AMF initialization: %s", e.what());
		return false;
	} catch (...) {
		PLOG_ERROR("Unexpected Exception during AMF initialization.");
		return false;
	}

	// Initialize Graphics APIs
	Plugin::API::InitializeAPIs();

	// AMF Capabilities
	try {
		Plugin::AMD::CapabilityManager::Initialize();
	} catch (const std::exception& e) {
		PLOG_ERROR("Encountered Exception during Capability Manager initialization: %s", e.what());
		return false;
	} catch (...) {
		PLOG_ERROR("Unexpected Exception during Capability Manager initialization.");
		return false;
	}

	// Register Encoders
	#ifdef WITH_AVC
	Plugin::Interface::H264Interface::encoder_register();
	#endif
	#ifdef WITH_HEVC
	Plugin::Interface::H265Interface::encoder_register();
	#endif

	#ifdef _DEBUG
	{
		PLOG_INFO("Dumping Parameter Information...");
		const wchar_t* encoders[] = {
			AMFVideoEncoderVCE_AVC,
			AMFVideoEncoder_HEVC
		};
		auto m_AMF = AMF::Instance();
		auto m_AMFFactory = m_AMF->GetFactory();
		amf::AMFTrace* m_AMFTrace;
		m_AMFFactory->GetTrace(&m_AMFTrace);
		amf::AMFDebug* m_AMFDebug;
		m_AMFFactory->GetDebug(&m_AMFDebug);
		m_AMFDebug->AssertsEnable(true);
		m_AMFDebug->EnablePerformanceMonitor(true);
		m_AMFTrace->EnableWriter(AMF_TRACE_WRITER_FILE, true);
		m_AMFTrace->EnableWriter(AMF_TRACE_WRITER_DEBUG_OUTPUT, true);
		m_AMFTrace->SetWriterLevel(AMF_TRACE_WRITER_FILE, 99);
		m_AMFTrace->SetWriterLevel(AMF_TRACE_WRITER_DEBUG_OUTPUT, 99);
		m_AMFTrace->SetPath(L"C:\\AMFTrace.log");
		m_AMFTrace->TraceEnableAsync(true);
		m_AMFTrace->SetGlobalLevel(99);
		for (auto enc : encoders) {
			amf::AMFContextPtr m_AMFContext;
			if (m_AMFFactory->CreateContext(&m_AMFContext) == AMF_OK) {
				m_AMFContext->InitDX11(nullptr);
				amf::AMFComponentPtr m_AMFComponent;
				if (m_AMFFactory->CreateComponent(m_AMFContext, enc, &m_AMFComponent) == AMF_OK) {
					PLOG_INFO("-- %ls --", enc);
					printDebugInfo(m_AMFComponent);
					m_AMFComponent->Terminate();
				}
				m_AMFContext->Terminate();
			}
		}
	}
	#endif

	PLOG_DEBUG("<" __FUNCTION_NAME__ "> Loaded.");
	return true;
}

/** Optional: Called when the module is unloaded.  */
MODULE_EXPORT void obs_module_unload(void) {
	Plugin::AMD::CapabilityManager::Finalize();
	Plugin::API::FinalizeAPIs();
	Plugin::AMD::AMF::Finalize();
}

/** Optional: Returns the full name of the module */
MODULE_EXPORT const char* obs_module_name() {
	return "AMD Media Framework Plugin";
}

/** Optional: Returns a description of the module */
MODULE_EXPORT const char* obs_module_description() {
	return "AMD Media Framework Plugin";
}

