// clang-format off

#include <functional>
#include <iostream>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>

using namespace std;

namespace nodes {

struct Number;
struct Plus;
struct Times;

struct Node {
  virtual ~Node() {}
  virtual auto value() const -> int = 0;
  virtual auto to_rpn() const -> string = 0;

  struct Visitor {
    virtual void accept(const Number& expr) = 0;
    virtual void accept(const Plus& expr) = 0;
    virtual void accept(const Times& expr) = 0;
  };

  virtual void visit(Visitor& viz) const = 0;
};

struct Number : Node {
  explicit Number(int value) : val(value) { }
  auto value() const -> int override { return val; }
  auto to_rpn() const -> string override { return to_string(val); }
  void visit(Visitor& viz) const override { viz.accept(*this); }
  int val;
};

struct Plus : Node {
  Plus(const Node& left, const Node& right) : left(left), right(right) { }
  auto value() const -> int override { return left.value() + right.value(); }
  auto to_rpn() const -> string override { return left.to_rpn() + " " + right.to_rpn() + " +"; }
  void visit(Visitor& viz) const override { viz.accept(*this); }
  const Node& left; const Node& right;
};

struct Times : Node {
  Times(const Node& left, const Node& right) : left(left), right(right) { }
  auto value() const -> int override { return left.value() * right.value(); }
  auto to_rpn() const -> string override { return left.to_rpn() + " " + right.to_rpn() + " &"; }
  void visit(Visitor& viz) const override { viz.accept(*this); }
  const Node& left; const Node& right;
};

}

namespace typeswitch {

using namespace nodes;

auto to_rpn(const Node& node) -> string {
  if (auto expr = dynamic_cast<const Number*>(&node)) {
    return to_string(expr->value());
  } else if (auto expr = dynamic_cast<const Plus*>(&node)) {
    return to_rpn(expr->left) + " " + to_rpn(expr->right) + " +";
  } else if (auto expr = dynamic_cast<const Times*>(&node)) {
    return to_rpn(expr->left) + " " + to_rpn(expr->right) + " *";
  }
  throw runtime_error("unknown node type");
}
}

namespace visitor {

using namespace nodes;

struct RPNVisitor : Node::Visitor {
  void accept(const Number& expr) {
    result = to_string(expr.val);
  }
  void accept(const Plus& expr) {
    expr.left.visit(*this);
    string l = result;
    expr.right.visit(*this);
    result = l + " " + result + " +";
  }
  void accept(const Times& expr) {
    expr.left.visit(*this);
    string l = result;
    expr.right.visit(*this);
    result = l + " " + result + " *";
  }
  string result;
};

auto to_rpn(const Node& node) -> string {
  RPNVisitor viz;
  node.visit(viz);
  return viz.result;
}

}

namespace funtable {

using namespace nodes;

unordered_map<type_index, string (*)(const Node&)> RPNformatters;

auto to_rpn(const Node& node) -> string {
  return RPNformatters[typeid(node)](node);
}

struct Init {
  Init() {
    RPNformatters[typeid(Number)] = [](const Node& node) {
      return to_string(static_cast<const Number&>(node).val); };
    RPNformatters[typeid(Plus)] = [](const Node& node) {
      auto expr = static_cast<const Plus&>(node);
      return to_rpn(expr.left) + " " + to_rpn(expr.right) + " +"; };
    RPNformatters[typeid(Times)] = [](const Node& node) {
      auto expr = static_cast<const Times&>(node);
      return to_rpn(expr.left) + " " + to_rpn(expr.right) + " *"; };
  }
} init;
}

#include <boost/openmethod.hpp>
#include <boost/openmethod/interop/std_unique_ptr.hpp>
#include <boost/openmethod/initialize.hpp>

namespace openmethods {

using namespace nodes;
using boost::openmethod::virtual_ptr;

BOOST_OPENMETHOD_CLASSES(Node, Number, Plus, Times);

BOOST_OPENMETHOD(to_rpn, (virtual_ptr<const Node>), string);

BOOST_OPENMETHOD_OVERRIDE(to_rpn, (virtual_ptr<const Number> expr), string) {
  return std::to_string(expr->val);
}

BOOST_OPENMETHOD_OVERRIDE(to_rpn, (virtual_ptr<const Plus> expr), string) {
  return to_rpn(expr->left) + " " + to_rpn(expr->right) + " +";
}

BOOST_OPENMETHOD_OVERRIDE(to_rpn, (virtual_ptr<const Times> expr), string) {
  return to_rpn(expr->left) + " " + to_rpn(expr->right) + " *";
}

BOOST_OPENMETHOD(value, (virtual_ptr<const Node>), int);

BOOST_OPENMETHOD_OVERRIDE(value, (virtual_ptr<const Number> expr), int) {
  return expr->val;
}

BOOST_OPENMETHOD_OVERRIDE(value, (virtual_ptr<const Plus> expr), int) {
  return value(expr->left) + value(expr->right);
}

BOOST_OPENMETHOD_OVERRIDE(value, (virtual_ptr<const Times> expr), int) {
  return value(expr->left) * value(expr->right);
}

}

namespace virtual_ptr_demo {

using namespace boost::openmethod;
using namespace nodes;

BOOST_OPENMETHOD(value, (virtual_ptr<const Node>), int);

auto call_via_vptr(virtual_ptr<const Node> node) -> int {
  return value(node);
}

BOOST_OPENMETHOD_OVERRIDE(value, (virtual_ptr<const Plus> expr), int) {
  return value(expr->left) + value(expr->right);
}

auto make_node_ptr(const Node& node) {
  return virtual_ptr(node);
}

auto make_final_node_ptr(const Plus& node) {
  return final_virtual_ptr(node);
}

}

namespace unique_virtual_ptr_demo {

using namespace boost::openmethod;

struct Number;
struct Plus;
struct Times;

struct Node {
  virtual ~Node() {}
};

struct Number : Node {
  explicit Number(int value) : val(value) { }
  int val;
};

struct Plus : Node {
  Plus(unique_virtual_ptr<Node>&& left, unique_virtual_ptr<Node>&& right)
    : left(std::move(left)), right(std::move(right)) { }
  unique_virtual_ptr<Node> left, right;
};

struct Times : Node {
  Times(unique_virtual_ptr<Node>&& left, unique_virtual_ptr<Node>&& right)
    : left(std::move(left)), right(std::move(right)) { }
  unique_virtual_ptr<Node> left, right;
};

BOOST_OPENMETHOD_CLASSES(Node, Number, Plus, Times);

BOOST_OPENMETHOD(value, (virtual_ptr<Node>), int);

BOOST_OPENMETHOD_OVERRIDE(value, (virtual_ptr<Plus> expr), int) {
  return value(expr->left) + value(expr->right);
}

BOOST_OPENMETHOD_OVERRIDE(value, (virtual_ptr<Number> expr), int) {
  return expr->val;
}

BOOST_OPENMETHOD_OVERRIDE(value, (virtual_ptr<Times> expr), int) {
  return value(expr->left) * value(expr->right);
}

BOOST_OPENMETHOD(to_rpn, (virtual_ptr<Node>), string);

BOOST_OPENMETHOD_OVERRIDE(to_rpn, (virtual_ptr<Number> expr), string) {
  return std::to_string(expr->val);
}

BOOST_OPENMETHOD_OVERRIDE(to_rpn, (virtual_ptr<Plus> expr), string) {
  return to_rpn(expr->left) + " " + to_rpn(expr->right) + " +";
}

BOOST_OPENMETHOD_OVERRIDE(to_rpn, (virtual_ptr<Times> expr), string) {
  return to_rpn(expr->left) + " " + to_rpn(expr->right) + " *";
}

}

namespace core_api {

using namespace boost::openmethod;
using namespace nodes;

use_classes<Node, Number, Plus, Times> use_node_classes;

struct value_id;
using value = method<value_id, auto (virtual_ptr<const Node>)->int>;

auto number_value(virtual_ptr<const Number> node) -> int {
  return node->val;
}
value::override<number_value> add_number_value;

template<class NodeClass, class Op>
auto binary_op(virtual_ptr<const NodeClass> expr) -> int {
    return Op()(value::fn(expr->left), value::fn(expr->right));
}

BOOST_OPENMETHOD_REGISTER(value::override<binary_op<Plus, std::plus<int>>>);
BOOST_OPENMETHOD_REGISTER(value::override<binary_op<Times, std::multiplies<int>>>);

}

auto main() -> int {
  boost::openmethod::initialize();

  {
    using namespace nodes;

    Number n2(2), n3(3), n4(4);
    Plus sum(n3, n4);
    Times product(n2, sum);

    const Node& expr = product;
    cout << expr.value() << "\n";

    cout << expr.value() << "\n";
    cout << typeswitch::to_rpn(expr) << " = " << expr.value() << "\n";
    cout << visitor::to_rpn(expr) << " = " << expr.value() << "\n";
    cout << funtable::to_rpn(expr) << " = " << expr.value() << "\n";

    cout << openmethods::to_rpn(expr) << " = " << expr.value() << "\n";

    cout << core_api::value::fn(expr) << "\n";
  }

  {
    using boost::openmethod::make_unique_virtual;
    using namespace unique_virtual_ptr_demo;

    auto expr = make_unique_virtual<Times>(
      make_unique_virtual<Number>(2),
      make_unique_virtual<Plus>(
        make_unique_virtual<Number>(3), make_unique_virtual<Number>(4)
      ));

    cout << to_rpn(expr) << " = " << value(expr) << "\n";
  }

  return 0;
}
