#define FS_QUEUE_IMPLEMENTATION
//#define FS_QUEUE_SINGLE_THREADED
#include "fs-queue.hpp"

#include <iostream>

#include <gtest/gtest.h>


#pragma region TESTS

const char *TESTS_SAMPLE_TEXT = "Lorem ipsum dolor sit amet";

TEST(FS_QueueTest, push_peek_no_wrap) {
	FS_Queue queue(10);

	ASSERT_EQ(queue.push(TESTS_SAMPLE_TEXT, 20), 10);

	char peeked[6]{ 0 };
	size_t peekedSize = queue.peek(peeked, 5);
	ASSERT_EQ(peeked[5], 0);

	ASSERT_STREQ(peeked, "Lorem");
}

TEST(FS_QueueTest, push_pop_wrap) {
	FS_Queue queue(10);

	queue.push(TESTS_SAMPLE_TEXT, 8);
	queue.pop(nullptr, 8);
	queue.push(TESTS_SAMPLE_TEXT, 5);

	char popped[6]{ 0 };
	queue.pop(popped, 3);
	queue.pop(popped + 3, 2);
	ASSERT_EQ(popped[5], 0);

	ASSERT_STREQ(popped, "Lorem");

	queue.empty();
	ASSERT_EQ(queue.pop(nullptr, 11), 0);

}

TEST(FS_QueueTest, resize) {
	FS_Queue queue(5);
	queue.resize(10);
	ASSERT_EQ(queue.capacity(), 10);

	ASSERT_EQ(queue.push(TESTS_SAMPLE_TEXT, 20), 10);
}

#ifndef FS_QUEUE_SINGLE_THREADED

TEST(FS_QueueTest, push_and_pop_blocking) {
	FS_Queue queue(10);

	std::atomic<bool> completed_pushing = false;

	std::thread pushThread([&]() {
		ASSERT_EQ(queue.pushBlocking(TESTS_SAMPLE_TEXT, strlen(TESTS_SAMPLE_TEXT)), strlen(TESTS_SAMPLE_TEXT));
		printf("completed pushing\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		completed_pushing = true;
		printf("interrupting\n");
		queue.interrupt();
	});

	char popped[strlen(TESTS_SAMPLE_TEXT) + 1]{ 0 };
	for(int i = 0; !completed_pushing || queue.size();) {
		int ps = queue.popBlocking(popped + i, queue.awaitData());
		i += ps;
		printf("popped %i bytes, text: %.*s\n", ps, i, popped);
	}

	printf("joining\n");
	pushThread.join();

	printf("asserts\n");
	ASSERT_EQ(popped[strlen(TESTS_SAMPLE_TEXT)], 0);
	ASSERT_STREQ(popped, TESTS_SAMPLE_TEXT);

}

#endif

#pragma endregion


int main(int argc, char **argv) {

	testing::InitGoogleTest(&argc, argv);
	int testsResult = RUN_ALL_TESTS();

	if(testsResult) return testsResult;

	FS_Queue tc(5);

	bool stillPushing = true;
	const char *sample_text = "hello xworld! abcdefgh";

	std::thread th([&] {
		tc.pushBlocking(sample_text, strlen(sample_text));
		printf("\npush blocking ended");
		stillPushing = false;
	});

	char ch = 0;
	while(tc.size() || stillPushing) {
		tc.popBlocking(&ch, 1);
		printf("popped [%c]", ch);
		if(ch == 'x') {
			printf("\ninterrupting\n");
			tc.interrupt();
		}
		ch = 0;
		std::cin.get();
	}

	th.join();
	return 0;
}