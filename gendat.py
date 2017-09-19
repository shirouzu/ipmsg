# convert file data to compressed BYTE array C source for install.cpp
# H.Shirouzu 2016/01/29
#
# ex) ipmsg.exe ==> zipped data ==> C source like "static char ipmsg_exe[] = { 0x78,0x9c,... }; "
#

import sys, zlib, os

HTMLWORKSHOP = r'"C:\program files (x86)\HTML Help Workshop\hhc"'

def gen_byte_array(f, keyword, data):
	f.write("static BYTE %s[] = {" % keyword)

	for i, c in enumerate(data):
		if (i % 16) == 0:
			f.write("\n\t")
		f.write("0x%02x," % ord(c))

	f.write("\n};\n")

def gen_timestamp(f, keyword, mtime):
	ntfs_prec = 10000000 # 10,000,000 == 1 sec
	unix_epoch_base = 0x019db1ded53e8000L
	t = unix_epoch_base + int(mtime * ntfs_prec)

	f.write("static FILETIME %s_mtime = { 0x%x, 0x%x };\n" %
		(keyword, int(t & 0xffffffff), int((t >> 32) & 0xffffffff)))

def compare_time(dst, src_array):
	try:
		dst_mtime = os.stat(dst).st_mtime
		for src in src_array:
			src_mtime = os.stat(src).st_mtime
			if src_mtime > dst_mtime:
				return	False;
		else:
			return	True

	except:	# path not found or etc
		pass

	return	False

def gen_data(out_name, files):
	if compare_time(out_name, files):
		return	False

	f = open(out_name, "wb")

	for fname in files:
		data = zlib.compress(open(fname, "rb").read())
		keyword = fname.split("\\")[-1].split("/")[-1].replace(".", "_")
		gen_timestamp(f, keyword, os.path.getmtime(fname))
		gen_byte_array(f, keyword, data)

def gen_help(dir):
	hhp = os.path.abspath(os.path.join(dir, r'help\ipmsg.hhp'))
	try:
		hhp_time = os.stat(hhp).st_mtime
		hhc_time = os.stat(os.path.join(dir, r'help\ipmsg.hhc')).st_mtime
		hlp_time = os.stat(os.path.join(dir, r'help\ipmsghlp.htm')).st_mtime
		hlpeng_time = os.stat(os.path.join(dir, r'help\ipmsghlp_eng.htm')).st_mtime
		chm_time = os.stat(os.path.join(dir, r'help\ipmsg.chm')).st_mtime
		if	max(hhp_time, hhc_time, hlp_time, hlpeng_time) <= chm_time:
			return

	except:
		pass

	cmd = HTMLWORKSHOP + " " + hhp
	os.system(cmd)

def main():
	gen_help(os.path.dirname(sys.argv[0]))
	gen_data(sys.argv[1], sys.argv[2:])
	sys.exit(0)

main()

