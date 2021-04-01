
/*
static void* struct_thread(void* opaque)
{
	MTY_Queue* qctx = (MTY_Queue*)opaque;
	char teststring[] = "This is from the thread!";
	void* buf = NULL;
	size_t size = 0;
	bool r = MTY_QueuePushPtr(qctx, teststring, sizeof(char*));
	test_cmp("MTY_QueuePushPtr", r);
	r = MTY_QueueGetOutputBuffer(qctx, -1, &buf, &size);
	test_cmp("MTY_QueueGetOutputBuffer", r);
	test_cmp("MTY_QueuePush", size == 4 && *(uint64_t*)buf == 0xff00ff00ff);

	r = MTY_QueueGetLastOutputBuffer(qctx, -1, &buf, &size);
	test_cmp("MTY_QueueGetLastOutputBuffer", r);

	return NULL;
};
*/

static bool struct_main(void)
{
	char stringkey[] = "I'm a test string key!";
	void* intvalue = (void *) (uintptr_t) 0xDEADBEEF;

	MTY_Hash* hashctx = MTY_HashCreate(0);
	test_cmp("MTY_HashCreate", hashctx != NULL);

	void *value = MTY_HashSet(hashctx, stringkey, stringkey);
	test_cmp("MTY_HashSet (New)", value == NULL);

	value = MTY_HashSet(hashctx, stringkey, stringkey);
	test_cmp("MTY_HashSet (OW)", value && !strcmp(stringkey, (char *) value));

	value = MTY_HashGet(hashctx, stringkey);
	test_cmp("MTY_HashGet", value && !strcmp(stringkey, value));

	value = MTY_HashSetInt(hashctx, 1, &intvalue);
	test_cmp("MTY_HashSetInt (New)", value == NULL);

	value = MTY_HashSetInt(hashctx, 1, intvalue);
	test_cmp("MTY_HashSetInt (OW)", value && *(void**)value == intvalue);

	value = MTY_HashGetInt(hashctx, 1);
	test_cmp("MTY_HashGetInt", value && value == intvalue);

	uint64_t iter = 0;
	int64_t popintkey = 0;
	const char* popkey;
	bool r = MTY_HashGetNextKey(hashctx, &iter, &popkey);
	test_cmpi64("MTY_HashGetNextKey (I)", r, iter);

	r = MTY_HashGetNextKey(hashctx, &iter, &popkey);
	test_cmpi64("MTY_HashGetNextKey (S)", r, iter);

	iter = 0; //have to reset since the KeyInt function will fail on the normal string key
	r = MTY_HashGetNextKeyInt(hashctx, &iter, &popintkey);
	test_cmpi64("MTY_HashGetNextKeyInt (I)", r, iter);

	r = MTY_HashGetNextKeyInt(hashctx, &iter, &popintkey);
	test_cmpi64("MTY_HashGetNextKeyInt (S)", !r, iter);

	value = MTY_HashPop(hashctx, popkey);
	test_cmp("MTY_HashPop", value && !strcmp(stringkey, value));

	value = MTY_HashPopInt(hashctx, popintkey);
	test_cmp("MTY_HashPopInt", value && value == intvalue);

	MTY_HashDestroy(&hashctx, NULL);
	test_cmp("MTY_HashDestroy", hashctx == NULL);

	MTY_Queue* queuectx = MTY_QueueCreate(2, 4);
	test_cmp("MTY_QueueCreate", queuectx != NULL);

	/*
	MTY_Thread* thread = MTY_ThreadCreate(struct_thread, queuectx);
	size_t size = 0;
	const char* threadstrcheck = "This is from the thread!";

	r = MTY_QueuePopPtr(queuectx, -1, &value, &size);
	test_cmp("MTY_QueuePopPtr", r && !strcmp(value,threadstrcheck));
	value = MTY_QueueGetInputBuffer(queuectx);
	test_cmp("MTY_QueueGetInputBuffer", value);
	*(uint64_t*)value = 0xff00ff00ff;
	MTY_QueuePush(queuectx, 4);


	MTY_Sleep(10);

	MTY_QueueFlush(queuectx, NULL);

	value = NULL;
	value = MTY_QueueGetInputBuffer(queuectx);
	test_cmp("MTY_QueueFlush", value);


	MTY_QueuePushPtr(queuectx, &value, 4);

	uint32_t qlength = MTY_QueueGetLength(queuectx);
	test_cmpi32("MTY_QueueGetLength", qlength > 0, qlength);
	MTY_QueuePop(queuectx);

	qlength = MTY_QueueGetLength(queuectx);
	test_cmp("MTY_QueueGetLength", qlength == 0);
	test_cmp("MTY_QueuePop", true);


	MTY_ThreadDestroy(&thread);
	*/

	MTY_QueueDestroy(&queuectx);
	test_cmp("MTY_QueueDestroy", queuectx == NULL);

	MTY_List* listctx = MTY_ListCreate();
	test_cmp("MTY_ListCreate", listctx != NULL);

	MTY_ListNode* node = MTY_ListGetFirst(listctx);
	test_cmp("MTY_ListGetFirst (Empty)", node == NULL);

	MTY_ListAppend(listctx, intvalue);
	node = MTY_ListGetFirst(listctx);
	test_cmp("MTY_ListAppend", node != NULL);

	value = MTY_ListRemove(listctx, node);
	node = MTY_ListGetFirst(listctx);
	test_cmp("MTY_ListRemove", value && value == intvalue);

	MTY_ListDestroy(&listctx, NULL);
	test_cmp("MTY_ListDestroy", listctx == NULL);

	return true;
}
