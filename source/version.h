#pragma once								

#define DVER_BUILDNUM 0							
#define DVER_BUILDNUM_STRING "0"					

#define DVER_COUNTRY DVER_COUNTRY_US			

#define DVER_MAJORVERSION 0				    
#define DVER_MAJORVERSION_STRING "0"

#define DVER_MINORVERSION 1
#define DVER_MINORVERSION_STRING "1"     
														
#define DVER_BUGFIXVERSION 0
#define DVER_BUGFIXVERSION_STRING "0"

#if DVER_BUGFIXVERSION > 0
	#define	DVER_VERSION_STRING	DVER_MAJORVERSION_STRING "." DVER_MINORVERSION_STRING "." DVER_BUGFIXVERSION_STRING 
#else
	#define	DVER_VERSION_STRING	DVER_MAJORVERSION_STRING "." DVER_MINORVERSION_STRING 
#endif

// following are used in version info for the windows resource 
#define DVER_ORIGINALFILENAME	"SpoutGrabber.ax"
#define DVER_COMPANY			"Valentin Schmidt"
#define DVER_FILEDESCRIPTION	"SpoutGrabber DirectShow Filter"
#define DVER_PRODUCTNAME		"SpoutGrabber"
#define DVER_INTERNALNAME		"SpoutGrabber.ax"

#define DVER_COPYRIGHT "(c) 2018"
#define DVER_YEAR		"2018"
