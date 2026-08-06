#include <gtest/gtest.h>

#include <algorithm>
#include <format>
#include <concepts>

#include "../utility/transform_hierarchy.hpp"

struct pos2d {
  int x, y;
};

constexpr auto operator==(const pos2d &left, const pos2d &right) noexcept
    -> bool {
  return left.x == right.x && left.y == right.y;
}

constexpr auto operator+(const pos2d &left, const pos2d &right) noexcept
    -> pos2d {
  return pos2d{left.x + right.x, left.y + right.y};
}

constexpr auto operator-(const pos2d &left, const pos2d &right) noexcept
    -> pos2d {
  return pos2d{left.x - right.x, left.y - right.y};
}

constexpr auto operator*(const pos2d &left, auto scalar) noexcept
    -> pos2d {
  return pos2d{left.x * scalar, left.y * scalar};
}

constexpr auto pos2d_child_global_from_local(pos2d parent_global,
                                             pos2d child_local) -> pos2d {
  return (parent_global * -1) + child_local;
}

constexpr auto pos2d_child_local_from_global(pos2d parent_global,
                                             pos2d child_global) -> pos2d {
  return child_global + parent_global;
}

template <> struct std::formatter<pos2d> {
  constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }

  auto format(const pos2d &obj, std::format_context &ctx) const {
    return std::format_to(ctx.out(), "[{}, {}]", obj.x, obj.y);
  }
};

template <> class testing::internal::UniversalPrinter<pos2d> {
public:
  static void Print(const pos2d &val, ::std::ostream *os) {
    *os << std::format("{}", val);
  }
};

using transform_hierarchy_pos2d_t = transform_hierarchy::transform_hierarchy_t<
    pos2d, pos2d_child_global_from_local, pos2d_child_local_from_global>;

TEST(transform_id, construction_destruction) {
  transform_hierarchy_pos2d_t hierarchy(32);
  pos2d p1{3, 4};
  auto t1 = hierarchy.add(p1);
  ASSERT_TRUE(t1.has_value());
  EXPECT_TRUE(hierarchy.is_valid(*t1));
  hierarchy.remove_and_preserve_children(*t1);
  EXPECT_FALSE(hierarchy.is_valid(*t1));
}

TEST(transform_id, add_argument_reference_qualifiers) {
  transform_hierarchy_pos2d_t hierarchy(32);

  // lvalue
  pos2d p1{3, 4};
  auto t1 = hierarchy.add(p1);
  ASSERT_TRUE(t1.has_value());

  // rvalue
  auto t2 = hierarchy.add(pos2d{5, 6});
  ASSERT_TRUE(t2.has_value());
}

TEST(transform_id, set_local_location_argument_reference_qualifiers) {
  transform_hierarchy_pos2d_t hierarchy(32);
  auto t1 = hierarchy.add(pos2d{0, 0});
  ASSERT_TRUE(t1.has_value());

  // lvalue
  pos2d p1{3, 4};
  hierarchy.set_local_location(*t1, p1);

  // rvalue
  hierarchy.set_local_location(*t1, pos2d{5, 6});
}

TEST(transform_id, parent_removed_marks_child_as_dirty) {
  transform_hierarchy_pos2d_t hierarchy(32);
  auto t1 = hierarchy.add(pos2d{3, 4});
  auto t2 = hierarchy.add_child_local_location(pos2d{5, 6}, *t1);

  hierarchy.remove_and_preserve_children(*t1);
  auto *p2 = hierarchy.unsafe_find(*t2);
  ASSERT_NE(p2, nullptr);

  EXPECT_EQ(p2->parent, transform_hierarchy::invalid_transform_id);
  EXPECT_TRUE(p2->dirty);
}

TEST(transform_id, multiple_creation_updates_index) {
  transform_hierarchy_pos2d_t hierarchy(32);
  pos2d p1{3, 4};
  pos2d p2{3, 4};
  auto t1 = hierarchy.add(p1);
  auto t2 = hierarchy.add(p2);
  ASSERT_TRUE(t1.has_value());
  ASSERT_TRUE(t2.has_value());
  EXPECT_NE(t1->index(), t2->index());
  EXPECT_EQ(t1->generation(), t2->generation());
}

TEST(transform_id, generation_update_on_refill) {
  transform_hierarchy_pos2d_t hierarchy(32);
  pos2d p1{3, 4};
  auto t1 = hierarchy.add(p1);
  ASSERT_TRUE(t1.has_value());

  hierarchy.remove_and_preserve_children(*t1);

  pos2d p2{3, 4};
  auto t2 = hierarchy.add(p2);
  ASSERT_TRUE(t2.has_value());
  EXPECT_EQ(t1->index(), t2->index());
  EXPECT_NE(t1->generation(), t2->generation());
}

TEST(transform_id, transforms_are_correcly_initialized) {
  transform_hierarchy_pos2d_t hierarchy(32);
  pos2d p1{3, 4};
  auto t1 = hierarchy.add(p1);
  ASSERT_TRUE(t1.has_value());

  pos2d p2{3, 4};
  auto t2 = hierarchy.add_child_local_location(p2, *t1);
  ASSERT_TRUE(t2.has_value());

  auto pt1 = hierarchy.unsafe_find(*t1);
  auto pt2 = hierarchy.unsafe_find(*t2);
  ASSERT_NE(pt1, nullptr);
  ASSERT_NE(pt2, nullptr);
  EXPECT_NE(pt1, pt2);

  EXPECT_TRUE(pt1->active);
  EXPECT_TRUE(pt2->active);
  EXPECT_FALSE(pt1->dirty);
  EXPECT_TRUE(pt2->dirty);

  EXPECT_EQ(pt1->generation, 0);
  EXPECT_EQ(pt2->generation, 0);
  EXPECT_EQ(pt1->local_location, p1);
  EXPECT_EQ(pt2->local_location, p2);
  EXPECT_EQ(pt1->parent, transform_hierarchy::invalid_transform_id);
  EXPECT_EQ(pt2->parent, *t1);
}

TEST(transform_id, parent_translation_is_propagated_to_child) {
  transform_hierarchy_pos2d_t hierarchy(32);
  pos2d p1{2, 4};
  auto t1 = hierarchy.add(p1);
  ASSERT_TRUE(t1.has_value());

  pos2d p2{5, 10};
  auto t2 = hierarchy.add_child_local_location(p2, *t1);
  ASSERT_TRUE(t2.has_value());

  auto local1 = hierarchy.local_location(*t1);
  auto local2 = hierarchy.local_location(*t2);
  ASSERT_TRUE(local1.has_value());
  ASSERT_TRUE(local1.has_value());
  EXPECT_EQ(*local1, p1);
  EXPECT_EQ(*local2, p2);

  auto global1 = hierarchy.global_location(*t1);
  auto global2 = hierarchy.global_location(*t2);
  ASSERT_TRUE(global1.has_value());
  ASSERT_TRUE(global2.has_value());
  EXPECT_EQ(*global1, p1);
  EXPECT_EQ(*global2, p1 + p2);
}

TEST(transform_id, unparenting_preserves_former_parent_translations) {
  transform_hierarchy_pos2d_t hierarchy(32);
  pos2d p1{1, 2};
  auto t1 = hierarchy.add(p1);

  pos2d p2{4, 4};
  auto t2 = hierarchy.add_child_local_location(p2, *t1);
  pos2d movement = {4, 0};
  hierarchy.set_local_location(*t1, movement);
  pos2d newp2 = hierarchy.global_location(*t2).value_or(pos2d{0, 0});
  ASSERT_EQ(newp2, p2 + movement);
}

TEST(transform_id, reparenting) { GTEST_SKIP(); }

TEST(transform_id, get_children) {
  transform_hierarchy_pos2d_t hierarchy(32);
  pos2d p1{1, 2};
  auto t1 = hierarchy.add(p1);

  pos2d p2{4, 4};
  auto t2 = hierarchy.add_child_local_location(p2, *t1);

  pos2d p3{2, 2};
  auto t3 = hierarchy.add_child_local_location(p3, *t1);

  std::vector<transform_hierarchy::transform_id_t> t1_children =
      hierarchy.children(*t1);

  ASSERT_EQ(t1_children.size(), 2);
  EXPECT_EQ(t1_children[0], *t2);
  EXPECT_EQ(t1_children[1], *t3);
}

TEST(transform_id, local_parenting_and_global_parenting) {
  transform_hierarchy_pos2d_t hierarchy(32);
  pos2d p1{1, 2};
  auto t1 = hierarchy.add(p1);

  pos2d offset{4, 4};
  auto local = hierarchy.add_child_local_location(offset, *t1);
  auto global = hierarchy.add_child_global_location(offset, *t1);

  pos2d local_child = hierarchy.global_location(*local).value_or(pos2d{0, 0});
  EXPECT_EQ(local_child, offset + p1);

  pos2d global_child = hierarchy.global_location(*global).value_or(pos2d{0, 0});
  EXPECT_EQ(global_child, offset);
}

TEST(transform_id, nested_hierarchy_translations) {
  /*
   * p1
   * |__p2
   * |__p3
   *    |__p4
   *       |__p5
   */
  transform_hierarchy_pos2d_t hierarchy(32);

  pos2d p1{1, 2};
  auto t1 = hierarchy.add(p1);

  pos2d p2{4, 4};
  auto t2 = hierarchy.add_child_local_location(p2, *t1);

  pos2d p3{-4, 0};
  auto t3 = hierarchy.add_child_local_location(p3, *t1);

  pos2d p4{2, 2};
  auto t4 = hierarchy.add_child_local_location(p4, *t3);

  pos2d p5{-3, -2};
  auto t5 = hierarchy.add_child_local_location(p5, *t4);

  EXPECT_EQ(*hierarchy.global_location(*t1), p1);
  EXPECT_EQ(*hierarchy.local_location(*t1), p1);

  auto const translate_locally =
      [&hierarchy](transform_hierarchy::transform_id_t id,
                   pos2d offset) -> void {
    pos2d local_location = hierarchy.local_location(id).value_or(pos2d{0, 0});
    hierarchy.set_local_location(id, local_location + offset);
  };

  // Translate t1 and validate all nodes are translated accordingly
  pos2d offset1{2, 2};
  translate_locally(*t1, offset1);
  EXPECT_EQ(hierarchy.local_location(*t1), p1 + offset1);
  EXPECT_EQ(hierarchy.global_location(*t1), p1 + offset1);

  EXPECT_EQ(hierarchy.local_location(*t2), p2);
  EXPECT_EQ(hierarchy.global_location(*t2), p1 + p2 + offset1);

  EXPECT_EQ(hierarchy.local_location(*t3), p3);
  EXPECT_EQ(hierarchy.global_location(*t3), p1 + p3 + offset1);

  EXPECT_EQ(hierarchy.local_location(*t4), p4);
  EXPECT_EQ(hierarchy.global_location(*t4), p1 + p3 + p4 + offset1);

  EXPECT_EQ(hierarchy.local_location(*t5), p5);
  EXPECT_EQ(hierarchy.global_location(*t5), p1 + p3 + p4 + p5 + offset1);

  // Translate t3 and validate all its children nodes are translated accordingly
  // and the unrelated nodes are not translated
  pos2d offset2{10, -10};
  translate_locally(*t3, offset2);

  EXPECT_EQ(hierarchy.local_location(*t1), p1 + offset1);
  EXPECT_EQ(hierarchy.global_location(*t1), p1 + offset1);

  EXPECT_EQ(hierarchy.local_location(*t2), p2);
  EXPECT_EQ(hierarchy.global_location(*t2), p1 + p2 + offset1);

  EXPECT_EQ(hierarchy.local_location(*t3), p3 + offset2);
  EXPECT_EQ(hierarchy.global_location(*t3), p1 + p3 + offset1 + offset2);

  EXPECT_EQ(hierarchy.local_location(*t4), p4);
  EXPECT_EQ(hierarchy.global_location(*t4), p1 + p3 + p4 + offset1 + offset2);

  EXPECT_EQ(hierarchy.local_location(*t5), p5);
  EXPECT_EQ(hierarchy.global_location(*t5),
            p1 + p3 + p4 + p5 + offset1 + offset2);
}

TEST(transform_id, roots) {
  /*
   * p1
   * |__p2
   * |__p3
   *
   * p4
   * |__p5
   *
   * p6
   */
  transform_hierarchy_pos2d_t hierarchy(32);

  auto t1 = hierarchy.add(pos2d{1, 2});
  auto t2 = hierarchy.add_child_local_location(pos2d{4, 4}, *t1);
  auto t3 = hierarchy.add_child_local_location(pos2d{-4, 0}, *t1);

  auto t4 = hierarchy.add(pos2d{2, 2});

  auto t5 = hierarchy.add_child_local_location(pos2d{-3, -2}, *t4);

  auto t6 = hierarchy.add(pos2d{0, 0});

  std::vector<transform_hierarchy::transform_id_t> roots = hierarchy.roots();

  ASSERT_FALSE(roots.empty());
  EXPECT_TRUE(std::ranges::contains(roots, *t1));
  EXPECT_TRUE(std::ranges::contains(roots, *t4));
  EXPECT_TRUE(std::ranges::contains(roots, *t6));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
