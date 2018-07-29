::::::::::::::::::::::::::::::::::::::::
:: NDIGrabber Demo (360p)
::::::::::::::::::::::::::::::::::::::::
@echo off
cd "%~dp0"

:: CLSID constants
set CLSID_LAVSplitterSource={B98D13E7-55DB-4385-A33D-09FD1BA26338}
set CLSID_LAVVideoDecoder={EE30215D-164F-4A92-A4EB-9D4C13390F9F}
set CLSID_NDIGrabber={850F8FE5-A357-4668-9D11-41EDF1642741}
set CLSID_VideoRendererDefault={6BC1CFFA-8FC1-4261-AC22-CFB4CC38DB50}

:: make sure that also LAV's DLLs are found
set PATH=filters;%PATH%

:: render 360p MP4 video with NDIRenderer
bin\dscmd^
 -graph ^
%CLSID_LAVSplitterSource%;src=..\assets\bbb_360p_10sec.mp4;file=filters\LAVSplitter.ax,^
%CLSID_LAVVideoDecoder%;file=filters\LAVVideo.ax,^
%CLSID_NDIGrabber%;file=NDIGrabber.ax,^
%CLSID_VideoRendererDefault%^
!0:1,1:2,2:3^
 -windowCaption "NDIGrabber Demo (360p)"^
 -loop -1^
 -i

echo.
pause
