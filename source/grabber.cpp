#include "grabber.h"
#include <initguid.h>
#include <Processing.NDI.Lib.h>
//#include "dbg.h"

//######################################
// Defines
//######################################

// Define ASYNC_MODE to activate asynchronous mode. In async mode sample data is sent asynchronously,
// which unfortunately requires to copy it in memory, but performance is still better.

#define ASYNC_MODE

//######################################
// Globals
//######################################
NDIlib_send_instance_t g_pNDI_send = NULL;
NDIlib_video_frame_v2_t g_NDI_video_frame;

#ifdef ASYNC_MODE
PBYTE g_data = NULL;
int g_data_size;
#endif

//######################################
// GUIDs
//######################################

// {850F8FE5-A357-4668-9D11-41EDF1642741}
DEFINE_GUID(CLSID_NDIGrabber,
	0x850f8fe5, 0xa357, 0x4668, 0x9d, 0x11, 0x41, 0xed, 0xf1, 0x64, 0x27, 0x41);

// {D643AA54-4B89-4F28-AB75-D2EF964A5D09}
DEFINE_GUID(IID_INDIGrabber,
	0xd643aa54, 0x4b89, 0x4f28, 0xab, 0x75, 0xd2, 0xef, 0x96, 0x4a, 0x5d, 0x9);

//######################################
// Setup data
//######################################

const AMOVIESETUP_PIN psudNDIGrabberPins[] = {
  { L"Input"            // strName
  , FALSE               // bRendered
  , FALSE               // bOutput
  , FALSE               // bZero
  , FALSE               // bMany
  , &CLSID_NULL         // clsConnectsToFilter
  , L""                 // strConnectsToPin
  , 0                   // nTypes
  , NULL                // lpTypes
  }
, { L"Output"           // strName
  , FALSE               // bRendered
  , TRUE                // bOutput
  , FALSE               // bZero
  , FALSE               // bMany
  , &CLSID_NULL         // clsConnectsToFilter
  , L""                 // strConnectsToPin
  , 0                   // nTypes
  , NULL                // lpTypes
  }
};

const AMOVIESETUP_FILTER sudNDIGrabber = {
  &CLSID_NDIGrabber   // clsID
, L"NDIGrabber"       // strName
, MERIT_DO_NOT_USE      // dwMerit
, 2                     // nPins
, psudNDIGrabberPins  // lpPin
};

// Needed for the CreateInstance mechanism
CFactoryTemplate g_Templates[] = {
	{ L"NDIGrabber"
		, &CLSID_NDIGrabber
		, CNDIGrabber::CreateInstance
		, NULL
		, &sudNDIGrabber }
};

int g_cTemplates = sizeof(g_Templates)/sizeof(g_Templates[0]);

//######################################
// Notify about errors
//######################################
void ErrorMessage (const char * msg) {
	// print to debug log
	OutputDebugStringA(msg);

	// optional: show blocking message box?
	MessageBoxA(NULL, msg, "Error", MB_OK);
}

//######################################
// CreateInstance
// Provide the way for COM to create a CNDIGrabber object
//######################################
CUnknown * WINAPI CNDIGrabber::CreateInstance (LPUNKNOWN punk, HRESULT *phr) {
	ASSERT(phr);

	// assuming we don't want to modify the data
	CNDIGrabber *pNewObject = new CNDIGrabber(punk, phr, FALSE);

	if(pNewObject == NULL) {
		if (phr) *phr = E_OUTOFMEMORY;
	}

	return pNewObject;
}

//######################################
// Constructor
//######################################
CNDIGrabber::CNDIGrabber (IUnknown * pOuter, HRESULT * phr, BOOL ModifiesData)
		: CTransInPlaceFilter( TEXT("NDIGrabber"), (IUnknown*) pOuter, CLSID_NDIGrabber, phr, (BOOL)ModifiesData )
{
	// this is used to override the input pin with our own
	m_pInput = (CTransInPlaceInputPin*) new CNDIGrabberInPin(this, phr);
	if (!m_pInput) {
		if (phr) *phr = E_OUTOFMEMORY;
	}

	// Not required, but "correct" (see the SDK documentation.
	if (!NDIlib_initialize()) {
		ErrorMessage("Initializing NDILib failed");
	}

	// We create the NDI sender
	NDIlib_send_create_t params;
	params.p_ndi_name = "NDIGrabber";
	params.p_groups = NULL;
	params.clock_video = TRUE;
	params.clock_audio = FALSE;
	g_pNDI_send = NDIlib_send_create(&params);

	if (!g_pNDI_send) {
		ErrorMessage("Creating NDI sender failed");
	}

}

//######################################
// Destructor
//######################################
CNDIGrabber::~CNDIGrabber () {

	if (g_pNDI_send) {

		// Destroy the NDI sender
		NDIlib_send_destroy(g_pNDI_send);
		g_pNDI_send = NULL;

		// Not required, but nice
		NDIlib_destroy();
	}

#ifdef ASYNC_MODE
	if (g_data) {
		free(g_data);
		g_data = NULL;
	}
#endif

	m_pInput = NULL;
}

//######################################
//
//######################################
STDMETHODIMP CNDIGrabber::NonDelegatingQueryInterface (REFIID riid, void ** ppv) {
	CheckPointer(ppv, E_POINTER);

	if(riid == IID_INDIGrabber) {
		return GetInterface((INDIGrabber *) this, ppv);
	}
	else {
		return CTransInPlaceFilter::NonDelegatingQueryInterface(riid, ppv);
	}
}

//######################################
// This is where you force the sample grabber to connect with one type
// or the other. What you do here is crucial to what type of data your
// app will be dealing with in the sample grabber's callback. For instance,
// if you don't enforce right-side-up video in this call, you may not get
// right-side-up video in your callback. It all depends on what you do here.
//######################################
HRESULT CNDIGrabber::CheckInputType( const CMediaType * pMediaType) {
	CheckPointer(pMediaType, E_POINTER);
	CAutoLock lock( &m_Lock );

	// Does this have a VIDEOINFOHEADER format block
	const GUID *pFormatType = pMediaType->FormatType();
	if (*pFormatType != FORMAT_VideoInfo) {
		NOTE("Format GUID not a VIDEOINFOHEADER");
		return E_INVALIDARG;
	}
	ASSERT(pMediaType->Format());

	// Check the format looks reasonably ok
	ULONG Length = pMediaType->FormatLength();
	if (Length < SIZE_VIDEOHEADER) {
		NOTE("Format smaller than a VIDEOHEADER");
		return E_FAIL;
	}

	// Check if the media major type is MEDIATYPE_Video
	const GUID *pMajorType = pMediaType->Type();
	if (*pMajorType != MEDIATYPE_Video) {
		NOTE("Major type not MEDIATYPE_Video");
		return E_INVALIDARG;
	}

	// Check if the media subtype is supported
	const GUID *pSubType = pMediaType->Subtype();
	if (
		*pSubType == MEDIASUBTYPE_UYVY      // NDIlib_FourCC_type_UYVY
		|| *pSubType == MEDIASUBTYPE_NV12      // NDIlib_FourCC_type_NV12
		|| *pSubType == MEDIASUBTYPE_RGB32     // NDIlib_FourCC_type_BGRX
		|| *pSubType == MEDIASUBTYPE_ARGB32    // NDIlib_FourCC_type_BGR
		//|| * pSubType != MEDIASUBTYPE_YV12
		) return NOERROR;

	NOTE("Invalid video media subtype");
	return E_INVALIDARG;
}

//######################################
// This bit is almost straight out of the base classes.
// We override this so we can handle Transform( )'s error
// result differently.
//######################################
HRESULT CNDIGrabber::Receive (IMediaSample * pMediaSample) {
	CheckPointer(pMediaSample, E_POINTER);

	AM_SAMPLE2_PROPERTIES * const pProps = m_pInput->SampleProps();
	if (pProps->dwStreamId != AM_STREAM_MEDIA) {
		if( m_pOutput->IsConnected() )
			return m_pOutput->Deliver(pMediaSample);
		else
			return NOERROR;
	}

	if (g_pNDI_send) {

		CAutoLock lock(&m_Lock);

		PBYTE pbData;
		HRESULT hr = pMediaSample->GetPointer(&pbData);
		if (FAILED(hr)) return hr;

		//send the frame via NDI
#ifdef ASYNC_MODE
		memcpy(g_data, pbData, g_data_size);
		g_NDI_video_frame.p_data = g_data;
		NDIlib_send_send_video_async_v2(g_pNDI_send, &g_NDI_video_frame);
#else
		g_NDI_video_frame.p_data = pbData;
		NDIlib_send_send_video_v2(g_pNDI_send, &g_NDI_video_frame);
#endif

	}

	return m_pOutput->Deliver(pMediaSample);
}

//######################################
// SetAcceptedMediaType
//######################################
STDMETHODIMP CNDIGrabber::SetAcceptedMediaType (const CMediaType * pMediaType) {
	CAutoLock lock(&m_Lock);
	if (!pMediaType) {
		m_mtAccept = CMediaType();
		return NOERROR;
	}
	return CopyMediaType(&m_mtAccept, pMediaType);
}

//######################################
// GetAcceptedMediaType
//######################################
STDMETHODIMP CNDIGrabber::GetConnectedMediaType (CMediaType * pMediaType) {
	if (!m_pInput || !m_pInput->IsConnected()) return VFW_E_NOT_CONNECTED;
	return m_pInput->ConnectionMediaType(pMediaType);
}

//######################################
// used to help speed input pin connection times. We return a partially
// specified media type - only the main type is specified. If we return
// anything BUT a major type, some codecs written improperly will crash
//######################################
HRESULT CNDIGrabberInPin::GetMediaType (int iPosition, CMediaType * pMediaType) {
	CheckPointer(pMediaType, E_POINTER);

	if (iPosition < 0) return E_INVALIDARG;
	if (iPosition > 0) return VFW_S_NO_MORE_ITEMS;

	*pMediaType = CMediaType( );
	pMediaType->SetType( ((CNDIGrabber*)m_pFilter)->m_mtAccept.Type( ) );

	return S_OK;
}

//######################################
// override the CTransInPlaceInputPin's method, and return a new enumerator
// if the input pin is disconnected. This will allow GetMediaType to be
// called. If we didn't do this, EnumMediaTypes returns a failure code
// and GetMediaType is never called.
//######################################
STDMETHODIMP CNDIGrabberInPin::EnumMediaTypes (IEnumMediaTypes **ppEnum) {
	CheckPointer(ppEnum, E_POINTER);
	ValidateReadWritePtr(ppEnum,sizeof(IEnumMediaTypes *));

	// if the output pin isn't connected yet, offer the possibly
	// partially specified media type that has been set by the user
	if( !((CNDIGrabber*)m_pTIPFilter)->OutputPin( )->IsConnected() ) {
		// Create a new reference counted enumerator
		*ppEnum = new CEnumMediaTypes( this, NULL );
		return (*ppEnum) ? NOERROR : E_OUTOFMEMORY;
	}

	// if the output pin is connected, offer it's fully qualified media type
	return ((CNDIGrabber*)m_pTIPFilter)->OutputPin( )->GetConnected()->EnumMediaTypes( ppEnum );
}

//######################################
//
//######################################
HRESULT CNDIGrabberInPin::SetMediaType (const CMediaType *pMediaType) {
	m_bMediaTypeChanged = TRUE;

	const GUID *pSubType = pMediaType->Subtype();
	if      (*pSubType == MEDIASUBTYPE_UYVY)   g_NDI_video_frame.FourCC = NDIlib_FourCC_type_UYVY;
	else if (*pSubType == MEDIASUBTYPE_NV12)   g_NDI_video_frame.FourCC = NDIlib_FourCC_type_NV12;
	else if (*pSubType == MEDIASUBTYPE_RGB32)  g_NDI_video_frame.FourCC = NDIlib_FourCC_type_BGRX; // result vertically flipped
	else if (*pSubType == MEDIASUBTYPE_ARGB32) g_NDI_video_frame.FourCC = NDIlib_FourCC_type_BGRA; // result vertically flipped
	//else if (*pSubType == MEDIASUBTYPE_YV12)  g_NDI_video_frame.FourCC = NDIlib_FourCC_type_YV12; // not working

	else {
		NOTE("Invalid video media subtype");
		return E_INVALIDARG;
	}

	// Has the video size changed between connections?
	if ((pMediaType->formattype == FORMAT_VideoInfo) && (pMediaType->cbFormat == sizeof(VIDEOINFOHEADER) && (pMediaType->pbFormat != NULL))) {
		VIDEOINFOHEADER *pVideoInfo = (VIDEOINFOHEADER *)pMediaType->Format();

		g_NDI_video_frame.xres = pVideoInfo->bmiHeader.biWidth;
		g_NDI_video_frame.yres = pVideoInfo->bmiHeader.biHeight;
		if (g_NDI_video_frame.yres < 0) g_NDI_video_frame.yres = -g_NDI_video_frame.yres; // do we need this?

#ifdef ASYNC_MODE
		g_data_size = pVideoInfo->bmiHeader.biSizeImage;
		if (g_data) {
			g_data = (PBYTE)realloc(g_data, g_data_size);
		}
		else {
			g_data = (PBYTE)malloc(g_data_size);
		}
		if (!g_data) return E_OUTOFMEMORY;
#endif

		return CTransInPlaceInputPin::SetMediaType(pMediaType);
	}

	return E_INVALIDARG;
}

////////////////////////////////////////////////////////////////////////
// Exported entry points for registration and unregistration
// (in this case they only call through to default implementations).
////////////////////////////////////////////////////////////////////////

//######################################
// DllRegisterSever
//######################################
STDAPI DllRegisterServer () {
	return AMovieDllRegisterServer2(TRUE);
}

//######################################
// DllUnregisterServer
//######################################
STDAPI DllUnregisterServer () {
	return AMovieDllRegisterServer2(FALSE);
}

//######################################
// DllEntryPoint
//######################################
extern "C" BOOL WINAPI DllEntryPoint (HINSTANCE, ULONG, LPVOID);
BOOL APIENTRY DllMain(HANDLE hModule, DWORD  dwReason, LPVOID lpReserved) {
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}
