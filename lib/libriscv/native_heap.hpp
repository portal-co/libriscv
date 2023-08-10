//
// C++ Header-Only Separate Address-Space Allocator
// by fwsGonzo, originally based on allocator written in C by Snaipe
//
#pragma once
#include "common.hpp"
#include <cstddef>
#include <cassert>
#include <deque>
#include "util/function.hpp"

namespace riscv
{
struct Arena;

struct ArenaChunk
{
	using PointerType = uint32_t;

	ArenaChunk() = default;
	ArenaChunk(ArenaChunk* n, ArenaChunk* p, size_t s, bool f, PointerType d)
		: next(n), prev(p), size(s), free(f), data(d) {}

	ArenaChunk* next = nullptr;
	ArenaChunk* prev = nullptr;
	size_t size = 0;
	bool   free = false;
	PointerType data = 0;

	ArenaChunk* find(PointerType ptr);
	ArenaChunk* find_free(size_t size);
	void merge_next(Arena&);
	void split_next(Arena&, size_t size);
	void subsume_next(Arena&, size_t extra);
};

struct Arena
{
	static constexpr size_t ALIGNMENT = 8u;
	using PointerType = ArenaChunk::PointerType;
	using ReallocResult = std::tuple<PointerType, size_t>;
	using unknown_realloc_func_t = Function<ReallocResult(PointerType, size_t)>;
	using unknown_free_func_t = Function<int(PointerType, ArenaChunk *)>;

	Arena(const Arena& other);
	Arena(PointerType base, PointerType end);

	PointerType malloc(size_t size);
	ReallocResult realloc(PointerType old, size_t size);
	size_t      size(PointerType src, bool allow_free = false);
	signed int  free(PointerType);

	PointerType seq_alloc_aligned(size_t size, size_t alignment);

	size_t bytes_free() const;
	size_t bytes_used() const;
	size_t chunks_used() const noexcept { return m_chunks.size(); }

	void transfer(Arena& dest) const;

	void on_unknown_free(unknown_free_func_t func) {
		m_free_unknown_chunk = std::move(func);
	}
	void on_unknown_realloc(unknown_realloc_func_t func) {
		m_realloc_unknown_chunk = std::move(func);
	}

	/** Internal usage **/
	inline ArenaChunk& base_chunk() {
		return m_base_chunk;
	}
	template <typename... Args>
	ArenaChunk* new_chunk(Args&&... args);
	void   free_chunk(ArenaChunk*);
	ArenaChunk* find_chunk(PointerType ptr);

	static size_t word_align(size_t size) {
		return (size + (ALIGNMENT-1)) & ~(ALIGNMENT-1);
	}
	static size_t fixup_size(size_t size) {
		// The minimum allocation is 8 bytes
		return std::max(ALIGNMENT, word_align(size));
	}
private:
	void internal_free(ArenaChunk* ch);
	void foreach(Function<void(const ArenaChunk&)>) const;

	std::deque<ArenaChunk> m_chunks;
	std::vector<ArenaChunk*> m_free_chunks;
	ArenaChunk  m_base_chunk;

	unknown_free_func_t m_free_unknown_chunk
		= [] (auto, auto*) { return -1; };
	unknown_realloc_func_t m_realloc_unknown_chunk
		= [] (auto, auto) { return ReallocResult{0, 0}; };
};

// find exact free chunk that matches ptr
inline ArenaChunk* ArenaChunk::find(PointerType ptr)
{
	ArenaChunk* ch = this;
	while (ch != nullptr) {
		if (!ch->free && ch->data == ptr)
			return ch;
		ch = ch->next;
	}
	return nullptr;
}
// find free chunk that has at least given size
inline ArenaChunk* ArenaChunk::find_free(size_t size)
{
    ArenaChunk* ch = this;
	while (ch != nullptr) {
		if (ch->free && ch->size >= size)
			return ch;
		ch = ch->next;
	}
	return nullptr;
}
// merge this and next into this chunk
inline void ArenaChunk::merge_next(Arena& arena)
{
	ArenaChunk* freech = this->next;
	this->size += freech->size;
	this->next = freech->next;
	if (this->next) {
		this->next->prev = this;
	}
	arena.free_chunk(freech);
}

inline void ArenaChunk::subsume_next(Arena& arena, size_t newlen)
{
	assert(this->size < newlen);
	ArenaChunk* ch = this->next;
	assert(ch);

	if (this->size + ch->size < newlen)
		return;

	const size_t subsume = newlen - this->size;
	ch->size -= subsume;
	ch->data += subsume;
	this->size = newlen;

	// Free the next chunk if we ate all of it
	if (ch->size == 0) {
		this->next = ch->next;
		if (this->next) {
			this->next->prev = this;
		}
		arena.free_chunk(ch);
	}
}

inline void ArenaChunk::split_next(Arena& arena, size_t size)
{
	ArenaChunk* newch = arena.new_chunk(
		this->next,
		this,
		this->size - size,
		true,
		this->data + (PointerType) size
	);
	if (this->next) {
		this->next->prev = newch;
	}
	this->next = newch;
	this->size = size;
}

template <typename... Args>
inline ArenaChunk* Arena::new_chunk(Args&&... args)
{
	if (UNLIKELY(m_free_chunks.empty())) {
		m_chunks.emplace_back(std::forward<Args>(args)...);
		return &m_chunks.back();
	}
	auto* chunk = m_free_chunks.back();
	m_free_chunks.pop_back();
	return new (chunk) ArenaChunk {std::forward<Args>(args)...};
}
inline void Arena::free_chunk(ArenaChunk* chunk)
{
	m_free_chunks.push_back(chunk);
}
inline ArenaChunk* Arena::find_chunk(PointerType ptr)
{
	for (auto& chunk : m_chunks) {
		if (!chunk.free && chunk.data == ptr)
			return &chunk;
	}
	return nullptr;
}

inline void Arena::internal_free(ArenaChunk* ch)
{
	ch->free = true;
	// merge chunks ahead and behind us
	if (ch->next && ch->next->free) {
		ch->merge_next(*this);
	}
	if (ch->prev && ch->prev->free) {
		ch = ch->prev;
		ch->merge_next(*this);
	}
}

inline Arena::PointerType Arena::malloc(size_t size)
{
	const size_t length = fixup_size(size);
	ArenaChunk* ch = base_chunk().find_free(length);

	if (ch != nullptr) {
		ch->split_next(*this, length);
		ch->free = false;
		return ch->data;
	}
	return 0;
}

// Guarantee that allocation is sequential in memory
// when accessed outside of emulation. A single page
// has fully sequential memory within itself, so we can
// always allocate sequential memory within a single page.
inline Arena::PointerType Arena::seq_alloc_aligned(size_t size, size_t alignment)
{
	assert(alignment != 0x0);
	(void)alignment;

	// XXX: Alignment is ignored for now,
	// but 8-byte alignment is guaranteed.
	const size_t objectsize = fixup_size(size);
	const size_t oversized = fixup_size(size * 2);

	// Find memory that can always cover the object sequentially
	ArenaChunk* ch = base_chunk().find_free(oversized);

	if (ch != nullptr) {
		// XXX: Assume that alignment of data is OK
		// Check if the allocation crosses a page
		if ((ch->data & ~(RISCV_PAGE_SIZE-1)) !=
			((ch->data + size) & ~(RISCV_PAGE_SIZE-1)))
		{
			// The second page boundary
			PointerType boundary = (ch->data + size) & ~(RISCV_PAGE_SIZE-1);
			// Split at the page boundary
			ch->split_next(*this, boundary - ch->data);

			// The data after the page boundary is usable,
			// but it must be split to the object size
			auto* final_ch = ch->next;
			final_ch->free = false;

			// Free the data before the boundary
			this->internal_free(ch);

			// XXX: Always split?
			final_ch->split_next(*this, objectsize);
			return final_ch->data;
		}
		else
		{
			ch->split_next(*this, objectsize);
			ch->free = false;
			return ch->data;
		}
	}
	return 0;
}

inline Arena::ReallocResult
	Arena::realloc(PointerType ptr, size_t newsize)
{
	if (ptr == 0x0) // Regular malloc
		return {malloc(newsize), 0};

	ArenaChunk* ch = base_chunk().find(ptr);
	if (UNLIKELY(ch == nullptr || ch->free)) {
		// Realloc failure handler
		return m_realloc_unknown_chunk(ptr, newsize);
	}

	newsize = fixup_size(newsize);
	if (ch->size >= newsize) // Already long enough?
		return {ch->data, 0};

	// We return the old length to aid memcpy
	const size_t old_len = ch->size;
	// Try to eat from the next chunk
	if (ch->next && ch->next->free) {
		ch->subsume_next(*this, newsize);
		if (ch->size >= newsize)
			return {ch->data, 0};
	}

	// Fallback to malloc, then free the old chunk
	ptr = malloc(newsize);
	if (ptr != 0x0) {
		this->internal_free(ch);
		return {ptr, old_len};
	}

	return {0x0, 0x0};
}

inline size_t Arena::size(PointerType ptr, bool allow_free)
{
	ArenaChunk* ch = base_chunk().find(ptr);
	if (UNLIKELY(ch == nullptr || (ch->free && !allow_free)))
		return 0;
	return ch->size;
}

inline int Arena::free(PointerType ptr)
{
	ArenaChunk* ch = base_chunk().find(ptr);
	if (UNLIKELY(ch == nullptr || ch->free))
		return m_free_unknown_chunk(ptr, ch);

	this->internal_free(ch);
	return 0;
}

inline Arena::Arena(PointerType arena_base, PointerType arena_end)
{
	m_base_chunk.size = arena_end - arena_base;
	m_base_chunk.data = arena_base;
	m_base_chunk.free = true;
}

inline void Arena::foreach(Function<void(const ArenaChunk&)> callback) const
{
	const ArenaChunk* ch = &this->m_base_chunk;
    while (ch != nullptr) {
		callback(*ch);
		ch = ch->next;
	}
}

inline size_t Arena::bytes_free() const
{
	size_t size = 0;
	foreach([&size] (const ArenaChunk& chunk) {
		if (chunk.free) size += chunk.size;
	});
	return size;
}
inline size_t Arena::bytes_used() const
{
	size_t size = 0;
	foreach([&size] (const ArenaChunk& chunk) {
		if (!chunk.free) size += chunk.size;
	});
	return size;
}

inline Arena::Arena(const Arena& other)
{
	other.transfer(*this);
}

inline void Arena::transfer(Arena& dest) const
{
	dest.m_base_chunk = m_base_chunk;
	dest.m_chunks.clear();
	dest.m_free_chunks.clear();

	ArenaChunk* last = &dest.m_base_chunk;

	const ArenaChunk* chunk = m_base_chunk.next;
	while (chunk != nullptr)
	{
		dest.m_chunks.push_back(*chunk);
		auto& new_chunk = dest.m_chunks.back();
		new_chunk.prev = last;
		new_chunk.next = nullptr;
		last->next = &new_chunk;
		/* New last before next iteration */
		last = &new_chunk;

		chunk = chunk->next;
	}
}

} // namespace riscv
