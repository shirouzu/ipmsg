---------------------------------------------------------------------
	Source code of IP Messenger for Win version 4.80
			H.Shirouzu Sep 19, 2017

	Copyright (C) 1996-2017 SHIROUZU Hiroaki All Rights Reserved.
	Copyright (C) 2015-2017 Asahi Net, Inc. All Rights Reserved.
---------------------------------------------------------------------

Index.

  1. About IP Messenger
  2. License
  3. Requirements
  4. Directory
  5. Support
  6. About self-extract installer

----------------------------------------------------------------------
1. About IP Messenger

 - IP Messenger is a pop up style message communication software for
   multi platforms. It is based on TCP/IP(UDP).

 - This software don't need server machine.

 - Simple, lightweight, and free software :-)

 - Win, Win16, Mac/MacOSX, X11R6/GTK/GNOME, Java, Div version and
   all source is open to public. You can get in the following URL.
       http://ipmsg.org/index.html.en

 - iOS, Android version is available, but source code is not opened.
       http://ipmsg.org/index.html.en

----------------------------------------------------------------------
2. License (BSD License)

 /* ==============================================================
  Copyright (c) 1996-2017 SHIROUZU Hiroaki All rights reserved.
  Copyright (C) 2015-2017 Asahi Net, Inc. All Rights Reserved.

  Redistribution and use in source and binary forms, with or
  without modification, are permitted provided that the following
  conditions are met:

  1. Redistributions of source code must retain the above
     copyright notice, this list of conditions and the following
     disclaimer.

  2. Redistributions in binary form must reproduce the above
     copyright notice, this list of conditions and the following
     disclaimer in the documentation and/or other materials
     provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY SHIROUZU Hiroaki and Asahi Net, Inc.
  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
  BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
  AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
  IN NO EVENT SHALL SHIROUZU Hiroaki OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
  OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
  OF SUCH DAMAGE.
 =============================================================== */

----------------------------------------------------------------------
3. Requirements

 - VS2017 or later

----------------------------------------------------------------------
4. Directory

	IPMsg----+-IPMsg.sln
		|
		+-Src------+-ipmsg.cpp
		|          |     :
		|          +-install-+- install.cpp
		|                    |       :
		|
		+-external-+-zlib (for installer)
		|          |     :
		|          +-sqlite3
		|                :
		+-Lib
		|
		+-Release--+-
		|
		+-Obj------+-Release-+-
		|          |
		|          +-Debug---+-
		|
		+-x64---+-Release
		        |
		        +-obj---+-Relase
		                |   :

----------------------------------------------------------------------
5. Support

 - ipmsg-ML is opened. If you want to subscribe for this ML,
   http://ipmsg.org/ (Japanese site)
   http://ipmsg.org/index.html.en (English site)

----------------------------------------------------------------------
6. About self-extract installer

 - v4.00 or later, self-extract installer format is supported.
   (it requires python)

