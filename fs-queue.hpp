#pragma once
#ifndef _FS_QUEUE_HPP
#define _FS_QUEUE_HPP

#include <cctype>
#include <vector>
#include <algorithm>

#ifndef FS_QUEUE_SINGLE_THREADED
#include <thread>
#include <mutex>
#include <condition_variable>
#endif

class FS_Queue {
public:
	FS_Queue(size_t bufferSize = 1 << 10);
	~FS_Queue();
	void resize(size_t newBufferSize);

	size_t push(const void *const data, size_t size);
	size_t peek(void *const data, size_t size);
	size_t pop(void *const data, size_t size);
	inline char pop_one() { char c = 0; pop(&c, 1); return c; }

#ifndef FS_QUEUE_SINGLE_THREADED
	size_t pushBlocking(const void *const data, size_t size);
	size_t popBlocking(void *const data, size_t size);
	inline size_t pop_one_b() { char c = 0; popBlocking(&c, 1); return c; }
	size_t wait();
	void interrupt();
#endif

	inline char &at(size_t index) { return m_buffer[offs(index)]; }
	inline char at(size_t index) const { return m_buffer[offs(index)]; }

	inline size_t size() const { return m_size; }
	inline size_t capacity() const { return m_buffer.size(); }
	inline bool full() const { return m_size == capacity(); }
	inline bool empty() const { return m_size == 0; }

private:
	size_t _push(const void *const data, size_t size);
	size_t _pop(void *const data, size_t size);
	std::vector<char> m_buffer;
	size_t m_size{ 0 }, m_head{ 0 };

#ifndef FS_QUEUE_SINGLE_THREADED
	std::mutex m_evtmux;
	std::condition_variable m_evt;
	size_t m_currentlyWaiting = 0;
	bool m_interrupt = false;
	void waitForInterrupt(std::unique_lock<std::mutex> &);

	size_t blockingCall(char * data, size_t size, bool isPush);
#endif

	inline size_t offs(size_t index) const { return m_head + index < m_buffer.size() ? m_head + index : m_head + index - m_buffer.size(); }
};


#pragma region implementation
#ifdef FS_QUEUE_IMPLEMENTATION

#pragma push_macro("min")
#undef min

FS_Queue::FS_Queue(size_t s) {
	resize(s);
}

FS_Queue::~FS_Queue() {
#ifndef FS_QUEUE_SINGLE_THREADED
	interrupt();
#endif
}

void FS_Queue::resize(size_t s) {
#ifndef FS_QUEUE_SINGLE_THREADED
	interrupt();
#endif
	m_buffer.resize(s);
}

size_t FS_Queue::push(const void *const data, size_t size) {
	if(!data) return 0;

#ifndef FS_QUEUE_SINGLE_THREADED
	std::unique_lock<std::mutex> lk(m_evtmux);
	waitForInterrupt(lk);
	m_evt.notify_all();
#endif

	return _push(data, size);
}

size_t FS_Queue::_push(const void *const data, size_t size) {
	size_t pushsize = std::min(size, m_buffer.size() - m_size);
	for(size_t i = 0; i < pushsize; i++) {
		at(m_size + i) = reinterpret_cast<const char *const>(data)[i];
	}
	m_size += pushsize;
	return pushsize;
}

size_t FS_Queue::pop(void *const data, size_t size) {
	if(!size) return 0;

#ifndef FS_QUEUE_SINGLE_THREADED
	std::unique_lock<std::mutex> lk(m_evtmux);
	waitForInterrupt(lk);
	m_evt.notify_all();
#endif

	return _pop(data, size);
}

size_t FS_Queue::_pop(void *const data, size_t size) {
	size_t popsize = peek(data, size);
	m_head = offs(popsize);
	m_size -= popsize;
	return popsize;
}

size_t FS_Queue::peek(void *const data, size_t size) {
	if(!size) return 0;
	size_t peeksize = std::min(size, m_size);
	if(data) {
		for(size_t i = 0; i < peeksize; i++) {
			reinterpret_cast<char *const>(data)[i] = at(i);
		}
	}
	return peeksize;
}


#ifndef FS_QUEUE_SINGLE_THREADED

void FS_Queue::waitForInterrupt(std::unique_lock<std::mutex> &lk) {
	if(m_interrupt) {
		m_evt.wait(lk, [&] { return m_currentlyWaiting == 0; });
		m_interrupt = false;
	}
}

size_t FS_Queue::blockingCall(char *data, size_t size, bool isPush) {
	size_t done_size = 0;
	std::unique_lock<std::mutex> lk(m_evtmux);
	waitForInterrupt(lk);
	while(done_size != size) {
		m_currentlyWaiting++;
		m_evt.wait(lk, [&] { return m_interrupt || !(isPush ? full() : empty()); });
		m_currentlyWaiting--;
		m_evt.notify_all();
		if(m_interrupt) break;
		done_size += (isPush ? _push(data + done_size, size - done_size) : _pop(data + done_size, size - done_size));
	}
	return done_size;
}

size_t FS_Queue::pushBlocking(const void *data, size_t size) {
	return blockingCall(const_cast<char *>(reinterpret_cast<const char *>(data)), size, true);
}

size_t FS_Queue::popBlocking(void *data, size_t size) {
	return blockingCall(reinterpret_cast<char *>(data), size, false);
}

void FS_Queue::interrupt() {
	std::unique_lock<std::mutex> lk(m_evtmux);
	waitForInterrupt(lk);
	m_interrupt = true;
	m_evt.notify_all();
}

size_t FS_Queue::wait() {
	std::unique_lock<std::mutex> lk(m_evtmux);
	waitForInterrupt(lk);
	m_currentlyWaiting++;
	m_evt.wait(lk, [&] { return m_interrupt || !empty(); });
	m_currentlyWaiting--;
	return m_size;
}

#endif

#pragma pop_macro("min")

#endif	// FS_QUEUE_IMPLEMENTATION
#pragma endregion

#endif	// _FS_QUEUE_HPP