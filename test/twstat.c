/* Check the wire lengths directly, independently of libixp's decoder. */
#include <assert.h>
#include <string.h>
#include <ixp.h>

static unsigned
le16(const unsigned char *p) {
	return p[0] | (unsigned)p[1] << 8;
}

static void
check(const char *name, unsigned version) {
	unsigned char buf[512];
	IxpFcall f = {0};
	IxpMsg msg;
	unsigned n, statlen;

	f.hdr.type = P9_TWStat;
	f.hdr.fid = 1;
	f.twstat.stat.name = (char*)name;
	f.twstat.stat.uid = "";
	f.twstat.stat.gid = "";
	f.twstat.stat.muid = "";
	f.twstat.stat.extension = "target";
	msg = ixp_message((char*)buf, sizeof buf, MsgPack);
	msg.version = version;
	n = ixp_fcall2msg(&msg, &f);
	/* Stat: size, type, dev, qid, mode, times, length, strings. */
	statlen = 2 + 2 + 4 + 13 + 4 + 4 + 4 + 8 + 8 + strlen(name);
	if(version == IXP_V9P2000U)
		statlen += 2 + strlen("target") + 12;
	assert(n == 4 + 1 + 2 + 4 + 2 + statlen);
	assert(le16(buf + 11) == statlen);
	assert(le16(buf + 13) == statlen - 2);
}

int
main(void) {
	check("", IXP_V9P2000);
	check("renamed-file", IXP_V9P2000);
	check("", IXP_V9P2000U);
	check("renamed-file", IXP_V9P2000U);
	return 0;
}
