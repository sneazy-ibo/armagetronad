#include "doctest.h"
#include "tLinkedList.h"

// Tests for tListItem (linked list functionality)
// Purpose: Document the status quo behavior and detect regressions

// Mock class that inherits from tListItem for testing
class MockLinkedListItem : public tListItem<MockLinkedListItem> {
public:
    int value;
    MockLinkedListItem(int v = 0) : value(v) {}
    
    // For sorting
    static bool Compare(const MockLinkedListItem* a, const MockLinkedListItem* b) {
        return a->value < b->value;
    }
};

TEST_CASE("tListItem default construction") {
    MockLinkedListItem item;
    CHECK(item.Next() == nullptr);
    CHECK(item.IsInList() == false);
}

TEST_CASE("tListItem Insert and Remove") {
    MockLinkedListItem* anchor = nullptr;
    
    MockLinkedListItem item1(1);
    item1.Insert(anchor);
    CHECK(anchor == &item1);
    CHECK(item1.IsInList() == true);
    CHECK(item1.Next() == nullptr);
    
    MockLinkedListItem item2(2);
    item2.Insert(anchor);
    CHECK(anchor == &item2);
    CHECK(item2.Next() == &item1);
    CHECK(item1.IsInList() == true);
    
    item2.Remove();
    CHECK(anchor == &item1);
    CHECK(item2.IsInList() == false);
}

TEST_CASE("tListItem Len static method") {
    MockLinkedListItem* anchor = nullptr;
    
    MockLinkedListItem item1(1);
    item1.Insert(anchor);
    CHECK(MockLinkedListItem::Len(anchor) == 1);
    
    MockLinkedListItem item2(2);
    item2.Insert(anchor);
    CHECK(MockLinkedListItem::Len(anchor) == 2);
}

TEST_CASE("tListItem InsertAfter") {
    MockLinkedListItem* anchor = nullptr;
    
    MockLinkedListItem item1(1);
    item1.Insert(anchor);
    
    MockLinkedListItem item2(2);
    item2.InsertAfter(item1);
    CHECK(item1.Next() == &item2);
    CHECK(MockLinkedListItem::Len(anchor) == 2);
}

TEST_CASE("tListItem SwapLists") {
    MockLinkedListItem* list1 = nullptr;
    MockLinkedListItem* list2 = nullptr;
    
    MockLinkedListItem item1(1);
    item1.Insert(list1);
    
    MockLinkedListItem item2(2);
    item2.Insert(list2);
    
    MockLinkedListItem::SwapLists(list1, list2);
    CHECK(list1 == &item2);
    CHECK(list2 == &item1);
}
