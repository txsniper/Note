#ifndef XTHREAD_BASE_LINKED_LIST_H
#define XTHREAD_BASE_LINKED_LIST_H
#include "noncopyable.h"
namespace xthread
{
namespace base
{
template <typename T>
class LinkNode : NonCopyable {
    public:
        LinkNode(): previous_(this), next_(this) {}
        LinkNode(LinkNode<T>* previous, LinkNode<T>* next) : previous_(previous), next_(next) {}

        void InsertBefore(LinkNode<T>* e)
        {
            this->next_ = e;
            this->previous_ = e->previous_;
            e->previous_->next_ = this;
            e->previous_ = this;
        }

        // Insert |this| as a circular linked list into the linked list, before |e|.
        void InsertBeforeAsList(LinkNode<T>* e) {
            LinkNode<T>* prev = this->previous_;
            prev->next_ = e;
            this->previous_ = e->previous_;
            e->previous_->next_ = this;
            e->previous_ = prev;
          }

        // Insert |this| into the linked list, after |e|.
        void InsertAfter(LinkNode<T>* e) {
            this->next_ = e->next_;
            this->previous_ = e;
            e->next_->previous_ = this;
            e->next_ = this;
          }

        // Insert |this| as a circular list into the linked list, after |e|.
        void InsertAfterAsList(LinkNode<T>* e) {
            LinkNode<T>* prev = this->previous_;
            prev->next_ = e->next_;
            this->previous_ = e;
            e->next_->previous_ = prev;
            e->next_ = this;
          }

        // Remove |this| from the linked list.
        void RemoveFromList() {
            this->previous_->next_ = this->next_;
            this->next_->previous_ = this->previous_;
            this->next_ = this;
            this->previous_ = this;
        }

        LinkNode<T>* previous() const {
            return previous_;
        }

        LinkNode<T>* next() const {
            return next_;
        }

        const T* value() const {
            return static_cast<const T*>(this);
        }

        T* value() {
            return static_cast<T*>(this);
        }
    private:
        LinkNode<T>* previous_;
        LinkNode<T>* next_;
};

template <typename T>
class LinkedList : NonCopyable{
    public:
    LinkedList() {}

    void Append(LinkNode<T>* e) {
        e->InsertBefore(&root_);
    }

    LinkNode<T>* head() const {
        return root_.next();
    }

    LinkNode<T>* tail() const {
        return root_.previous();
    }

    const LinkNode<T>* end() const {
        return &root_;
    }

    bool empty() const { return head() == end(); }

    private:
    LinkNode<T> root_;
};
}
}
#endif
