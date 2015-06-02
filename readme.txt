---------------------------------------------------------------------
	Source code of IP Messenger for Win version 3.21
			H.Shirouzu June 27, 2011

		Copyright (C) 1996-2011 SHIROUZU Hiroaki
			All Rights Reserved.
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

----------------------------------------------------------------------
2. License (BSD License)

  Copyright (c) 1996-2011 SHIROUZU Hiroaki All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer. 

    Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

    Neither the name of the SHIROUZU Hiroaki nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission. 

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

----------------------------------------------------------------------
3. Requirements

 - Visual C++ 6.0 or later
 - IP Messenger ver3.0 or later, use zlib/lipng for embedded image.
    http://www.libpng.org/pub/png/libpng.html
    http://zlib.net/

----------------------------------------------------------------------
4. Directory

	IPMsg----+-IPMsg.dsw ... Project file for VC6
		|      IPMsg.sln ... Project file for VS2005 or later
		|
		+-Src------+-ipmsg.cpp
		|          |     :
		|          +-install-+- install.cpp
		|                    |       :
		|
		+-External-+-Zlib---+-zlib.dsw
		|          |            :
		|          +-Libpng-+-libpng.dsw
		|                       :
		+-Release--+-
		|
		+-Obj------+-Release-+-
		           |
		           +-Debug---+-

----------------------------------------------------------------------
5. Support

 - ipmsg-ML is opened. If you want to subscribe for this ML,
   http://ipmsg.org/ (Japanese site)
   http://ipmsg.org/index.html.en (English site)

----------------------------------------------------------------------
6. About self-extract installer

 - v3.10 or later, self-extract installer format is supported.
   This format is install.exe with additional files.
   Additional files are "ipmsg.exe", "ipmsg.chm", "setup.exe".
   Additional file format is this.
 　 \n===(70 characters)===\n
 　 filesize filename\n
 　 (Zlib compressed file)

 - I have used this python script for generate self-extract installer.

=====================================================
import sys, zlib

def add_file(f, fname):
	data = zlib.compress(open(fname, "rb").read())
	f.write("\n%s\n" % ("=" * 70))
	f.write("%d %s\n" % (len(data), fname))
	f.write(data)

def gen_inst(installer_name, installer_base, files):
	f = open(installer_name, "wb")
	f.write(open(installer_base, "rb").read())
	for i in files:
		add_file(f, i)
	f.close()

gen_inst("ipmsgXXX_installer.exe", "install.exe", ["ipmsg.exe", "ipmsg.chm", "setup.exe"])
=====================================================

