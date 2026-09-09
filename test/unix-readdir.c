/* Exercise the directory helper with a captured response and wire checks. */
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ixp.h>
typedef void *IxpFileIdU;
#include <ixp_srvutil.h>

static unsigned version, expected;
static char expected_name;

uint
ixp_req_getversion(Ixp9Req *req) {
	(void)req;
	return version;
}

void
ixp_respond(Ixp9Req *req, const char *error) {
	unsigned char *p = (unsigned char*)req->ofcall.io.data;
	assert(!error);
	assert(req->ofcall.io.count == expected);
	assert((p[0] | (unsigned)p[1] << 8) == expected - 2);
	assert(p[41] == 1 && p[42] == 0 && p[43] == expected_name);
	if(version == IXP_V9P2000U) {
		assert(p[50] == 0 && p[51] == 0);
		assert(p[52] == 0xff && p[63] == 0xff);
	}
	free(p);
}

static IxpFileId*
lookup(IxpFileId *file, char *name) {
	IxpFileId *root, *a, *b;
	(void)file;
	(void)name;
	root = ixp_srv_getfile();
	a = ixp_srv_getfile();
	b = ixp_srv_getfile();
	root->next = a;
	root->tab.name = NULL;
	a->next = b;
	a->tab.name = malloc(2);
	b->tab.name = malloc(2);
	assert(a->tab.name && b->tab.name);
	strcpy(a->tab.name, "a");
	strcpy(b->tab.name, "b");
	return root;
}

static void
dostat(IxpStat *stat, IxpFileId *file) {
	stat->name = file->tab.name;
	stat->uid = stat->gid = stat->muid = "";
}

int
main(void) {
	Ixp9Req req = {0};
	IxpFid fid = {0};
	IxpFileId file = {0};
	req.fid = &fid;
	fid.aux = &file;
	for(version = IXP_V9P2000; version <= IXP_V9P2000U; version++) {
		expected = version == IXP_V9P2000U ? 64 : 50;
		fid.iounit = req.ifcall.io.count = expected;
		req.ifcall.io.offset = 0;
		expected_name = 'a';
		ixp_srv_readdir(&req, lookup, dostat);
		req.ifcall.io.offset = expected;
		expected_name = 'b';
		ixp_srv_readdir(&req, lookup, dostat);
	}
	ixp_srv_freefilepool();
	return 0;
}
