// 文件：src/monitor/RingBuffer.h
// 高性能环形缓冲区模板类
// 用于固定窗口数据缓存，支持高效的增量追加和淘汰

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <vector>
#include <cstddef>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <cassert>

namespace Monitor {

/**
 * @brief RingBuffer
 * 
 * 固定容量的环形缓冲区，支持：
 * - O(1) 时间复杂度的追加和淘汰
 * - 高效的批量追加
 * - 迭代器访问
 * - 线程不安全（调用方需自行加锁）
 * 
 * @tparam T 元素类型
 */
template<typename T>
class RingBuffer
{
public:
    using value_type = T;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;

    /**
     * @brief 前向迭代器
     */
    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        iterator(RingBuffer* buf, size_type pos) : m_buffer(buf), m_pos(pos) {}

        reference operator*() { return (*m_buffer)[m_pos]; }
        pointer operator->() { return &(*m_buffer)[m_pos]; }

        iterator& operator++() { ++m_pos; return *this; }
        iterator operator++(int) { iterator tmp = *this; ++m_pos; return tmp; }

        bool operator==(const iterator& other) const { return m_pos == other.m_pos; }
        bool operator!=(const iterator& other) const { return m_pos != other.m_pos; }

    private:
        RingBuffer* m_buffer;
        size_type m_pos;
    };

    class const_iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        const_iterator(const RingBuffer* buf, size_type pos) : m_buffer(buf), m_pos(pos) {}

        reference operator*() const { return (*m_buffer)[m_pos]; }
        pointer operator->() const { return &(*m_buffer)[m_pos]; }

        const_iterator& operator++() { ++m_pos; return *this; }
        const_iterator operator++(int) { const_iterator tmp = *this; ++m_pos; return tmp; }

        bool operator==(const const_iterator& other) const { return m_pos == other.m_pos; }
        bool operator!=(const const_iterator& other) const { return m_pos != other.m_pos; }

    private:
        const RingBuffer* m_buffer;
        size_type m_pos;
    };

public:
    /**
     * @brief 构造函数
     * @param capacity 缓冲区容量（必须 > 0）
     */
    explicit RingBuffer(size_type capacity = 1000)
        : m_capacity(capacity > 0 ? capacity : 1000)
        , m_head(0)
        , m_size(0)
    {
        m_data.resize(m_capacity);
    }

    /// 获取容量
    size_type capacity() const { return m_capacity; }

    /// 获取当前元素数量
    size_type size() const { return m_size; }

    /// 是否为空
    bool empty() const { return m_size == 0; }

    /// 是否已满
    bool full() const { return m_size == m_capacity; }

    /// 清空缓冲区
    void clear()
    {
        m_head = 0;
        m_size = 0;
    }

    /**
     * @brief 追加单个元素
     * 如果缓冲区已满，自动淘汰最老的元素
     */
    void push_back(const T& value)
    {
        size_type writePos = (m_head + m_size) % m_capacity;
        m_data[writePos] = value;

        if (m_size < m_capacity) {
            ++m_size;
        } else {
            // 缓冲区已满，移动 head
            m_head = (m_head + 1) % m_capacity;
        }
    }

    void push_back(T&& value)
    {
        size_type writePos = (m_head + m_size) % m_capacity;
        m_data[writePos] = std::move(value);

        if (m_size < m_capacity) {
            ++m_size;
        } else {
            m_head = (m_head + 1) % m_capacity;
        }
    }

    /**
     * @brief 批量追加元素
     * @param first 起始迭代器
     * @param last 结束迭代器
     */
    template<typename InputIt>
    void append(InputIt first, InputIt last)
    {
        for (; first != last; ++first) {
            push_back(*first);
        }
    }

    /**
     * @brief 批量追加（从 vector）
     */
    void append(const std::vector<T>& values)
    {
        append(values.begin(), values.end());
    }

    /**
     * @brief 按索引访问（0 = 最老的元素）
     */
    reference operator[](size_type index)
    {
        return m_data[(m_head + index) % m_capacity];
    }

    const_reference operator[](size_type index) const
    {
        return m_data[(m_head + index) % m_capacity];
    }

    reference at(size_type index)
    {
        if (index >= m_size) {
            throw std::out_of_range("RingBuffer index out of range");
        }
        return (*this)[index];
    }

    const_reference at(size_type index) const
    {
        if (index >= m_size) {
            throw std::out_of_range("RingBuffer index out of range");
        }
        return (*this)[index];
    }

    /// 获取最老的元素（调用方需确保 !empty()）
    reference front() { assert(m_size > 0 && "RingBuffer::front() called on empty buffer"); return (*this)[0]; }
    const_reference front() const { assert(m_size > 0 && "RingBuffer::front() called on empty buffer"); return (*this)[0]; }

    /// 获取最新的元素（调用方需确保 !empty()）
    reference back() { assert(m_size > 0 && "RingBuffer::back() called on empty buffer"); return (*this)[m_size - 1]; }
    const_reference back() const { assert(m_size > 0 && "RingBuffer::back() called on empty buffer"); return (*this)[m_size - 1]; }

    /// 迭代器
    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, m_size); }
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, m_size); }
    const_iterator cbegin() const { return const_iterator(this, 0); }
    const_iterator cend() const { return const_iterator(this, m_size); }

    /**
     * @brief 转换为 vector（按时间顺序）
     */
    std::vector<T> toVector() const
    {
        std::vector<T> result;
        result.reserve(m_size);
        for (size_type i = 0; i < m_size; ++i) {
            result.push_back((*this)[i]);
        }
        return result;
    }

    /**
     * @brief 获取最近 N 个元素
     */
    std::vector<T> last(size_type count) const
    {
        if (count > m_size) {
            count = m_size;
        }
        std::vector<T> result;
        result.reserve(count);
        size_type start = m_size - count;
        for (size_type i = start; i < m_size; ++i) {
            result.push_back((*this)[i]);
        }
        return result;
    }

    /**
     * @brief 修改容量（会清空现有数据）
     */
    void setCapacity(size_type newCapacity)
    {
        if (newCapacity == 0) {
            newCapacity = 1;
        }
        m_capacity = newCapacity;
        m_data.resize(m_capacity);
        clear();
    }

    /**
     * @brief 淘汰最老的 count 个元素
     */
    void dropOldest(size_type count)
    {
        if (count >= m_size) {
            clear();
        } else {
            m_head = (m_head + count) % m_capacity;
            m_size -= count;
        }
    }

private:
    std::vector<T> m_data;      ///< 底层存储
    size_type m_capacity;       ///< 容量
    size_type m_head;           ///< 头部位置（最老元素）
    size_type m_size;           ///< 当前元素数量
};

} // namespace Monitor

#endif // RING_BUFFER_H
