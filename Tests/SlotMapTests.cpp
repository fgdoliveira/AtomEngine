#include "Core/SlotMap.h"

#include <doctest/doctest.h>

#include <string>

using Atom::Handle;
using Atom::SlotMap;

TEST_CASE("A default handle is null and finds nothing")
{
    SlotMap<int> map;
    Handle handle{};
    CHECK(handle.IsNull());
    CHECK(map.Get(handle) == nullptr);
    CHECK_FALSE(map.Remove(handle));
}

TEST_CASE("Inserted values are reachable through their handle")
{
    SlotMap<std::string> map;
    const Handle a = map.Insert("torii");
    const Handle b = map.Insert("hokora");

    REQUIRE(map.Get(a) != nullptr);
    CHECK(*map.Get(a) == "torii");
    CHECK(*map.Get(b) == "hokora");
    CHECK(map.Size() == 2);
    CHECK_FALSE(a == b);
}

TEST_CASE("A removed object's handle goes stale instead of dangling")
{
    SlotMap<int> map;
    const Handle old = map.Insert(1);
    REQUIRE(map.Remove(old));

    CHECK(map.Get(old) == nullptr);
    CHECK_FALSE(map.Contains(old));
    CHECK_FALSE(map.Remove(old)); // double remove is harmless

    // The slot is reused, but with a new generation: the old handle must
    // not see the new object.
    const Handle reused = map.Insert(2);
    CHECK(reused.index == old.index);
    CHECK(reused.generation != old.generation);
    CHECK(map.Get(old) == nullptr);
    CHECK(*map.Get(reused) == 2);
}

TEST_CASE("Clear removes everything and invalidates all handles")
{
    SlotMap<int> map;
    const Handle a = map.Insert(1);
    const Handle b = map.Insert(2);
    map.Clear();

    CHECK(map.Size() == 0);
    CHECK(map.Get(a) == nullptr);
    CHECK(map.Get(b) == nullptr);
}

TEST_CASE("ForEach visits only live objects")
{
    SlotMap<int> map;
    map.Insert(1);
    const Handle removed = map.Insert(2);
    map.Insert(3);
    map.Remove(removed);

    int sum = 0;
    int count = 0;
    map.ForEach([&](Handle, int& value) {
        sum += value;
        ++count;
    });
    CHECK(count == 2);
    CHECK(sum == 4);
}
