// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

package cjpm.toml

import std.collection.*

public class LinkedListNode<T> {
    var _next: ?LinkedListNode<T>
    var _prev: ?LinkedListNode<T>
    private var _item: T
    protected var _list: ?LinkedList<T>

    init(element: T, prev: ?LinkedListNode<T>, next: ?LinkedListNode<T>, belongs: ?LinkedList<T>) {
        _item = element
        _prev = prev
        _next = next
        _list = belongs
    }

    public mut prop value: T {
        get() {
            checkNodeState()
            _item
        }
        set(v) {
            checkNodeState()
            _item = v
        }
    }

    public prop next: Option<LinkedListNode<T>> {
        get() {
            checkNodeState()
            _next
        }
    }

    public prop prev: Option<LinkedListNode<T>> {
        get() {
            checkNodeState()
            _prev
        }
    }

    func insertBefore(element: T): LinkedListNode<T> {
        var node = LinkedListNode<T>(element, _prev, this, _list)
        if (let Some(prev) <- _prev) {
            prev._next = node
        } else {
            _list.getOrThrow()._head = node
        }
        _prev = Some(node)
        return node
    }

    func insertAfter(element: T): LinkedListNode<T> {
        var node = LinkedListNode<T>(element, this, _next, _list)
        if (let Some(next) <- _next) {
            next._prev = node
        } else {
            _list.getOrThrow()._tail = node
        }
        _next = Some(node)
        return node
    }

    func remove(): T {
        match ((_prev, _next)) {
            case (Some(p), Some(n)) =>
                p._next = n
                n._prev = p
            case (Some(p), None) =>
                p._next = None
                _list.getOrThrow()._tail = p
            case (None, Some(n)) =>
                n._prev = None
                _list.getOrThrow()._head = n
            case _ =>
                _list.getOrThrow()._tail = None
                _list.getOrThrow()._head = None
        }

        _list = None
        return _item
    }

    public func forward(): Iterator<T> {
        if (let Some(list) <- _list) {
            return LinkedListIterator<T>(list, this, true)
        } else {
            throw IllegalStateException()
        }
    }

    public func backward(): Iterator<T> {
        if (let Some(list) <- _list) {
            return LinkedListIterator<T>(list, this, false)
        } else {
            throw IllegalStateException()
        }
    }

    func reverse(): Unit {
        let tmp = _prev
        _prev = _next
        _next = tmp
    }

    private func checkNodeState() {
        if (let None <- _list) {
            throw IllegalStateException()
        }
    }
}

public class LinkedList<T> <: Collection<T> {
    var _size: Int64 = 0
    var _head: ?LinkedListNode<T> = None
    var _tail: ?LinkedListNode<T> = None
    var _version: Int64 = 0

    public init() {}

    public init(elements: Collection<T>) {
        for (value in elements) {
            append(value)
        }
    }

    public init(elements: Array<T>) {
        for (element in elements) {
            append(element)
        }
    }

    public init(size: Int64, initElement: (Int64) -> T) {
        if (size < 0) {
            throw IllegalArgumentException("Invalid size of LinkedList: ${size}")
        }
        for (i in 0..size) {
            append(initElement(i))
        }
    }

    public prop size: Int64 {
        get() {
            return _size
        }
    }

    prop version: Int64 {
        get() {
            return _version
        }
    }

    public func prepend(element: T): LinkedListNode<T> {
        if (let Some(node) <- _head) {
            _head = node.insertBefore(element)
        } else {
            _head = LinkedListNode<T>(element, None, None, this)
            _tail = _head
        }
        _size++
        _version++
        return _head.getOrThrow()
    }

    public func append(element: T): LinkedListNode<T> {
        if (let Some(node) <- _tail) {
            _tail = node.insertAfter(element)
        } else {
            _tail = LinkedListNode<T>(element, None, None, this)
            _head = _tail
        }
        _size++
        _version++
        return _tail.getOrThrow()
    }

    public func toArray(): Array<T> {
        if (size == 0) {
            return Array<T>()
        }

        let arr = Array<T>(size, repeat: unsafe { zeroValue<T>() })
        var index = size - 1
        var node = _tail.getOrThrow()
        while (true) {
            arr[index] = node.value
            index--
            node = node.prev ?? break
        }
        return arr
    }

    public func insertBefore(node: LinkedListNode<T>, element: T): LinkedListNode<T> {
        checkNode(node)
        _size++
        _version++
        return node.insertBefore(element)
    }

    public func insertAfter(node: LinkedListNode<T>, element: T): LinkedListNode<T> {
        checkNode(node)
        _size++
        _version++
        return node.insertAfter(element)
    }

    public func remove(node: LinkedListNode<T>): T {
        checkNode(node)
        _size--
        _version++
        return node.remove()
    }

    public func removeIf(predicate: (T) -> Bool): Unit {
        var node = _head
        while (let Some(n) <- node) {
            node = n.next
            if (predicate(n.value)) {
                _size--
                _version++
                n.remove()
            }
        }
    }

    public func clear(): Unit {
        _size = 0
        _head = None
        _tail = None
        _version++
    }

    public func reverse(): Unit {
        _version++
        var node = _head
        while (let Some(n) <- node) {
            node = n.next
            n.reverse()
        }
        let tmp = _head
        _head = _tail
        _tail = tmp
    }

    public func isEmpty(): Bool {
        return _size == 0
    }

    public func iterator(): Iterator<T> {
        return LinkedListIterator<T>(this, _head, false)
    }

    public prop first: ?T {
        get() {
            match (_head) {
                case Some(v) => return v.value
                case _ => return None
            }
        }
    }

    public prop last: ?T {
        get() {
            match (_tail) {
                case Some(v) => return v.value
                case _ => return None
            }
        }
    }

    public func popFirst(): ?T {
        if (let Some(v) <- _head) {
            _version++
            _size--
            return v.remove()
        }

        return None
    }

    public func popLast(): ?T {
        if (let Some(v) <- _tail) {
            _version++
            _size--
            return v.remove()
        }

        return None
    }

    public func firstNode(): Option<LinkedListNode<T>> {
        return _head
    }

    public func lastNode(): Option<LinkedListNode<T>> {
        return _tail
    }

    public func nodeAt(index: Int64): Option<LinkedListNode<T>> {
        if (index < 0 || index >= _size) {
            return None
        }

        if (index > (_size >> 1)) {
            var count = _size - index - 1
            var node = _tail
            while (count > 0) {
                node = node.getOrThrow().prev
                count--
            }
            return node
        } else {
            var count = index
            var node = _head
            while (count > 0) {
                node = node.getOrThrow().next
                count--
            }
            return node
        }
    }

    public func splitOff(node: LinkedListNode<T>): LinkedList<T> {
        checkNode(node)
        _version++

        var subList = LinkedList<T>()
        subList._head = node
        subList._tail = this._tail
        subList._size = 1
        node._list = subList

        var cursor = node
        while (let Some(n) <- cursor.next) {
            subList._size++
            n._list = subList
            cursor = n
        }

        this._tail = node.prev
        node._prev = None
        match (this._tail) {
            case Some(v) => v._next = None
            case None => ()
        }

        this._size = this._size - subList._size
        return subList
    }

    private func checkNode(node: LinkedListNode<T>) {
        if (let Some(v) <- node._list) {
            if (refEq(this, v)) {
                return
            }
        }
        throw IllegalArgumentException("The node is not a member of this LinkedList.")
    }
}

extend<T> LinkedList<T> <: ToString where T <: ToString {
    public func toString(): String {
        return collectionToString<LinkedList<T>, T>(this)
    }
}

extend<T> LinkedList<T> <: Equatable<LinkedList<T>> where T <: Equatable<T> {
    /** overloaded determination == method. */
    public operator func ==(right: LinkedList<T>): Bool {
        if (refEq(this, right)) {
            return true
        }
        if (this.size != right.size) {
            return false
        } else if (this.size == 0) {
            return true
        }

        var thisCursor = this._head
        var rightCursor = right._head

        while (let (Some(l), Some(r)) <- (thisCursor, rightCursor)) {
            if (l.value == r.value) {
                thisCursor = l.next
                rightCursor = r.next
            } else {
                return false
            }
        }

        return true
    }

    /** overloaded determination != method. */
    public operator func !=(right: LinkedList<T>): Bool {
        return !(this == right)
    }
}

class LinkedListIterator<T> <: Iterator<T> {
    private let _lockVersion: Int64
    private let _data: LinkedList<T>
    private var _currentNode: LinkedListNode<T>
    private let _backward: Bool

    init(data: LinkedList<T>, node: ?LinkedListNode<T>, backward: Bool) {
        _lockVersion = data.version
        _data = data
        _currentNode = LinkedListNode<T>(unsafe { zeroValue<T>() }, node, node, data)
        _backward = backward
    }

    public func next(): Option<T> {
        if (_lockVersion != _data.version) {
            throw ConcurrentModificationException()
        }
        if (_backward) {
            _currentNode = _currentNode.prev ?? return None
            return _currentNode.value
        } else {
            _currentNode = _currentNode.next ?? return None
            return _currentNode.value
        }
    }
}

@OverflowWrapping
func collectionToString<C, T>(col: C): String where C <: Collection<T>, T <: ToString {
    if (col.size == 0) {
        return "[]"
    }
    let sb = StringBuilder("[")
    let it = col.iterator()
    var tmp: T = it.next().getOrThrow()
    while (let Some(next) <- it.next()) {
        sb.append(tmp)
        unsafe { sb.appendFromUtf8Unchecked(", ".toArray()) }
        tmp = next
    }
    sb.append(tmp)
    sb.append(r']')
    return sb.toString()
}
