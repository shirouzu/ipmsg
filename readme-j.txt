======================================================================
        IP Messenger for Win 全ソース 取り扱い説明書    2019/02/12

                                               H.Shirouzu（白水啓章）
                                               https://ipmsg.org
======================================================================

目次

  1. 概要
  2. 使用上の注意
  3. サポートについて
  4. コンパイル方法
  5. ディレクトリ構成
  6. 自己展開インストーラ形式について

■ 1. 概要

・IP Messenger for Win の全ソースコードです。

・MFC を使用せず、オリジナルの簡易クラスライブラリを使っています。
  簡易クラス部分は、tlib.h と t*.cpp になります。

・VS2015以降 でビルドできます。


■ 2. 使用上の注意

・「IP Messenger for Win」は実行ファイル・ソースコード共に
  フリーソフトウェアです。

・アーカイブの転載・再配布は自由です。

・ライセンスは以下の通りです。(BSD License)

/* ==============================================================
  Copyright (c) 1996-2019 SHIROUZU Hiroaki  All rights reserved.
  Copyright (c) 2015-2018 Asahi Net, Inc. All rights reserved.
  Copyright (c) 2018-2019 FastCopy Lab, LLC. All rights reserved.

  ソースとバイナリ形式の再配布および使用は、「変更の有無、商用/
  非商用にかかわらず」、以下の条件を満たす場合に許可されます:

  1. ソースコード形式で再配布する場合、上記著作権表示、本条件書、
     および下記免責表示を必ず含めてください。

  2. バイナリ形式で再配布する場合、上記著作権表示、本条件書、
     および下記免責表示を、配布物とともに提供される文書及び
     他の資料に必ず含めてください。

 「このソフトウエアは、作者により「あるがままの状態」で提供され、
   あらゆる明示的または暗黙の保証を否認し、いかなる損害に対しても
   責任を負いません。」

=============================================================== */


■ 3. サポートについて

・サポートは下記のURLに記載している掲示板で行っています。
　また、次期バージョンへ向けての提案なども歓迎です。
 （最新版は常にここにあります）。
    https://ipmsg.org/

・潜在バグの指摘や、よりよいソースコードへのアドバイスを歓迎します。


■ 4. コンパイル方法（VS2017 以降）

  VS2017 以降でビルドします。
  （VS2015の場合、toast以外の全プロジェクトのプラットフォームセット
    を v141_xp -> v140_xp に変更します。toastプロジェクトだけは
    _xp なしの v140 を指定します）

  ipmsg本体だけでなく、インストーラ形式としてビルドするためには、
  下記の準備が必要になります。

  1) Python2.x がインストールされ、python.exe に PATH が通った
     状態になっていること
     （instdataプロジェクトのビルド前イベントで使用されます）

  2) ヘルプビルド用 HTML Help Workshop が
      C:\Program Files (x86)\HTML Help Workshop\hhc
     にインストールされていること


■ 5. ディレクトリ構成

	IPMsg----+-IPMsg.sln ... Project file for VS2017以降
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

■ 6. 新しい自己展開インストーラ形式について

・v4.00以降の自己展開形式インストーラは、下記のステップで作成しています。
　（installプロジェクトのビルド前プロセスに記述されています。要python）

　0. （前提）ipmsg.exe, uninst.exe ipmsg.chm が既にビルド済みであること

　1. installerプロジェクトのビルド前プロセスにより、
　   gendat.py files_data(64).dat ipmsg.exe uninst.exe ipmsg.chm が実行
　   される。files_data(64).dat の中には、ファイル内容が zlib圧縮された、
　   C言語のstatic BYTE配列データが書かれる。
　   具体的には、
　    static BYTE ipmsg_exe[]={...};
　    static BYTE uninst_exe[]={...};
　    static BYTE ipmsg_chm[]={...};
　   といった内容。

  2. install.cpp にある #include "files_data(64).dat" により、それらが
     static変数として取り込まれ、ビルドされる。

  3. それらのデータは、実行時に zlib inflateされ、元のipmsg.exe等として
     インストール先に展開される。


