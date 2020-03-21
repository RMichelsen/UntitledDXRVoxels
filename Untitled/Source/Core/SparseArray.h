#pragma once

#include "Core/Logging.h"
#include "EABase/eabase.h"

template<typename T>
class SparseHandle
{
public:
	SparseHandle() : value(eastl::numeric_limits<uint32_t>::max())
	{
	}

private:
	template<typename T, size_t Size>
	friend class SparseArray;

	SparseHandle(uint32_t value_) : value(value_) {}
	uint32_t value;
};

template<typename T, size_t Size>
class SparseArray
{
public:
	SparseArray()
	{
		// 0xFFFFFFFF - 1 handles, the last handle is reserved for the invalid handle (null handle)
		static_assert(Size < (eastl::numeric_limits<uint32_t>::max() - 1));
	}

	inline T& operator [](SparseHandle<T> handle)
	{
		UNTITLED_ASSERT(handle.value != eastl::numeric_limits<uint32_t>::max() && "Null handle passed!");

		return dense[lowerSparse[handle.value]];
	}

	[[nodiscard]] inline SparseHandle<T> Insert(T&& element)
	{
		uint32_t index;
		if (freelist.empty())
		{
			index = sparseIndex++;
		}
		else
		{
			index = freelist.back();
			freelist.pop_back();
		}

		// Insert the element into the dense array
		auto denseIndex = static_cast<uint32_t>(dense.size());
		dense.emplace_back(eastl::forward<T>(element));

		// The lower sparse array now maps the handle to the dense index
		// The upper sparse array maps the dense index to the lowerSparse index
		lowerSparse[index] = denseIndex;
		upperSparse[denseIndex] = index;
		return SparseHandle<T>(index);
	}

	inline void Remove(SparseHandle<T>& handle)
	{
		UNTITLED_ASSERT(handle.value != eastl::numeric_limits<uint32_t>::max() && "Null handle passed!");

		auto denseIndex = lowerSparse[handle.value];
		auto sparseIndex = upperSparse[denseIndex];

		// Swap the two elements
		auto& a = dense[denseIndex];
		auto& b = dense.back();
		eastl::swap(a, b);
		dense.pop_back();

		// Get the lowerSparse index for the end of the dense array
		auto endSparseIndex = upperSparse[dense.size()];

		// Change the lowerSparse entry to now point to the moved element
		// Change the upperSparse entry to point to the new lowerSparse entry
		lowerSparse[endSparseIndex] = denseIndex;
		upperSparse[denseIndex] = endSparseIndex;

		// Invalidate lower sparse dense index
		// and add it to the free list
		lowerSparse[handle.value] = eastl::numeric_limits<uint32_t>::max();
		freelist.push_back(handle.value);

		// Invalidate handle
		handle = SparseHandle<T>(0xFFFFFFFF);
	}

	// From a user point of view, the sparse array will be iterated
	// and accessed directly into the dense array
	inline auto data() { return dense.data(); }
	inline auto size() const { return dense.size(); }
	inline auto begin() { return dense.begin(); }
	inline auto end() { return dense.end(); }
	inline auto cbegin() const { return dense.cbegin(); }
	inline auto cend() const { return dense.cend(); }

private:
	uint32_t sparseIndex = 0;

	// The upper sparse array maps dense indices to lowerSparse indices.
	// The lower sparse array maps handles to dense indices
	eastl::array<uint32_t, Size> upperSparse;
	eastl::array<uint32_t, Size> lowerSparse;
	eastl::fixed_vector<T, Size> dense;
	eastl::fixed_vector<uint32_t, Size> freelist;
};