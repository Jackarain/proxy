/**
 *   Copyright (C) 2024 T. Zachary Laine
 *
 *   Distributed under the Boost Software License, Version 1.0. (See
 *   accompanying file LICENSE_1_0.txt or copy at
 *   http://www.boost.org/LICENSE_1_0.txt)
 */

#include <boost/parser/parser.hpp>
#include <boost/parser/transcode_view.hpp>

#if __has_include(<boost/optional/optional.hpp>)
#define TEST_BOOST_OPTIONAL 1
#include <boost/optional/optional.hpp>
#else
#define TEST_BOOST_OPTIONAL 0
#endif

#include <set>

#include <boost/core/lightweight_test.hpp>


using namespace boost::parser;


void github_issue_36()
{
    namespace bp = boost::parser;

    auto id = bp::lexeme[+bp::char_(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz0123456789")];
    auto ids = +id;

    std::string str;
    std::vector<std::string> vec;

    bp::parse("id1", id, bp::ws, str); // (1)
    BOOST_TEST(str == "id1");
    str.clear();
    bp::parse("id1 id2", ids, bp::ws, vec); // (2)
    BOOST_TEST(vec == std::vector<std::string>({"id1", "id2"}));

    // Intentionally ill-formed.
    // bp::parse("i1 i2", ids, bp::ws, str);   // (3)
}

namespace issue_50 {
    struct X
    {
        char a;
        int b;

        bool operator<(X rhs) const { return a < rhs.a; }
    };

    struct Y
    {
        std::vector<X> x;
        int c;
    };

    struct Y2
    {
        std::set<X> x;
        int c;
    };

    static_assert(
        boost::parser::detail::is_struct_compatible<
            Y,
            boost::parser::
                tuple<std::vector<boost::parser::tuple<char, int>>, int>>());

    static_assert(
        boost::parser::detail::is_struct_compatible<
            Y2,
            boost::parser::
                tuple<std::vector<boost::parser::tuple<char, int>>, int>>());
}

void github_issue_50()
{
    using namespace issue_50;

    namespace bp = boost::parser;

    {
        auto parse_x = bp::char_ >> bp::int_;
        auto parse_y = +parse_x >> bp::int_;

        Y y;
        auto b = bp::parse("d 3 4", parse_y, bp::ws, y);
        BOOST_TEST(b);

        BOOST_TEST(y.x[0].a == 'd');
        BOOST_TEST(y.x[0].b == 3);
        BOOST_TEST(y.c == 4);
    }

    {
        auto parse_x = bp::char_ >> bp::int_;
        auto parse_y = +parse_x >> bp::int_;

        Y2 y;
        auto b = bp::parse("d 3 4", parse_y, bp::ws, y);
        BOOST_TEST(b);

        BOOST_TEST(y.x.begin()->a == 'd');
        BOOST_TEST(y.x.begin()->b == 3);
        BOOST_TEST(y.c == 4);
    }
}

namespace issue_52 {
    struct X
    {
        char a;
        int b;
    };

    struct Y
    {
        std::vector<X> x;
        int c;
    };

    struct Z
    {
        std::vector<Y> y;
        int d;
    };

    struct W
    {
        std::vector<Z> z;
        int e;
    };
}

void github_issue_52()
{
    using namespace issue_52;

    namespace bp = boost::parser;
    auto parse_x = bp::char_ >> bp::int_;
    auto parse_y = +parse_x >> bp::int_;
    auto parse_z = +parse_y >> bp::char_;
    auto parse_w = +parse_z >> bp::int_;

    {
        Z z;
        auto b = bp::parse("d 2 3 c", parse_z, bp::ws, z);
        BOOST_TEST(b);

        BOOST_TEST(z.y[0].x[0].a == 'd');
        BOOST_TEST(z.y[0].x[0].b == 2);
        BOOST_TEST(z.y[0].c == 3);
        BOOST_TEST(z.d == 'c');
    }
    {
        W w;
        auto b = bp::parse("d 2 3 c 4", parse_w, bp::ws, w);
        BOOST_TEST(b);

        BOOST_TEST(w.z[0].y[0].x[0].a == 'd');
        BOOST_TEST(w.z[0].y[0].x[0].b == 2);
        BOOST_TEST(w.z[0].y[0].c == 3);
        BOOST_TEST(w.z[0].d == 'c');
        BOOST_TEST(w.e == 4);
    }
}

void github_issue_78()
{
    namespace bp = boost::parser;
    std::vector<int> result;
    auto b = bp::parse("3 4 c", +bp::int_, bp::ws, result);
    BOOST_TEST(!b);
    BOOST_TEST(result.empty());
}

namespace issue_90 { namespace parser {
    const auto string =
        ('"' >> boost::parser::lexeme[*(boost::parser::char_ - '"')]) > '"';
    const auto specifier = string > ':' > string;
}}

void github_issue_90()
{
    using namespace issue_90;
    namespace bp = boost::parser;

    std::string input = R"( "dd" : "2" )";
    std::pair<std::string, std::string> result;

    auto b = bp::parse(input, parser::specifier, bp::ws, result);
    BOOST_TEST(b);
    BOOST_TEST(result.first == "dd");
    BOOST_TEST(result.second == "2");
}

namespace github_issue_125_ {
    namespace bp = boost::parser;
    constexpr bp::
        rule<struct replacement_field_rule, std::optional<unsigned short>>
            replacement_field = "replacement_field";
    constexpr auto replacement_field_def = bp::lit('{') >> -bp::ushort_;
    BOOST_PARSER_DEFINE_RULES(replacement_field);
}

void github_issue_125()
{
    namespace bp = boost::parser;
    using namespace github_issue_125_;

    unsigned short integer_found = 99;
    auto print_repl_field = [&](auto & ctx) {
        const std::optional<unsigned short> & val = bp::_attr(ctx);
        if (val)
            integer_found = *val;
        else
            integer_found = 77;
    };

    {
        integer_found = 99;
        auto result = bp::parse("{9", replacement_field[print_repl_field]);
        BOOST_TEST(result);
        BOOST_TEST(integer_found == 9);
    }
    {
        integer_found = 99;
        auto result = bp::parse("{", replacement_field[print_repl_field]);
        BOOST_TEST(result);
        BOOST_TEST(integer_found == 77);
    }
}

void github_issue_209()
{
    namespace bp = boost::parser;

    BOOST_TEST(std::is_sorted(
        std::begin(bp::detail::char_set<detail::punct_chars>::chars),
        std::end(bp::detail::char_set<detail::punct_chars>::chars)));

    BOOST_TEST(std::is_sorted(
        std::begin(bp::detail::char_set<detail::symb_chars>::chars),
        std::end(bp::detail::char_set<detail::symb_chars>::chars)));

    BOOST_TEST(std::is_sorted(
        std::begin(bp::detail::char_set<detail::lower_case_chars>::chars),
        std::end(bp::detail::char_set<detail::lower_case_chars>::chars)));

    BOOST_TEST(std::is_sorted(
        std::begin(bp::detail::char_set<detail::upper_case_chars>::chars),
        std::end(bp::detail::char_set<detail::upper_case_chars>::chars)));
}

void github_issue_223()
{
    namespace bp = boost::parser;

    // failing case
    {
        std::vector<char> v;
        const auto parser = *('x' | bp::char_('y'));
        bp::parse("xy", parser, bp::ws, v);

        BOOST_TEST(v.size() == 1);
        BOOST_TEST(v == std::vector<char>({'y'}));

        // the assert fails since there are two elements in the vector: '\0'
        // and 'y'. Seems pretty surprising to me
    }

    // working case
    {
        const auto parser = *('x' | bp::char_('y'));
        const auto result = bp::parse("xy", parser, bp::ws);

        BOOST_TEST(result->size() == 1);
        BOOST_TEST(*(*result)[0] == 'y');

        // success, the vector has only one 'y' element
    }
}

namespace github_issue_248_ {
    namespace bp = boost::parser;

    static constexpr bp::rule<struct symbol, int> symbol = "//";
    static constexpr bp::rule<struct vector, std::vector<int>> list =
        "<int>(,<int>)*";
    static constexpr bp::rule<struct working, std::vector<int>> working =
        "working";
    static constexpr bp::rule<struct failing, std::vector<int>> failing =
        "failing";

    static auto const symbol_def = bp::symbols<int>{{"//", 0}};
    static constexpr auto list_def = bp::int_ % ',';
    static constexpr auto working_def = -symbol >> (bp::int_ % ',');
    static constexpr auto failing_def = -symbol >> list;

    BOOST_PARSER_DEFINE_RULES(symbol, list, working, failing);
}

void github_issue_248()
{
    namespace bp = boost::parser;

    using namespace github_issue_248_;

    {
        auto const result = bp::parse("//1,2,3", working, bp::ws);
        auto const expected = std::vector<int>{0, 1, 2, 3};
        BOOST_TEST(result.has_value());
        bool const equal = std::equal(
            result->begin(), result->end(), expected.begin(), expected.end());
        BOOST_TEST(equal);
        if (!equal) {
            std::cout << "contents of *result:\n";
            for (auto x : *result) {
                std::cout << x << '\n';
            }
            std::cout << '\n';
        }
    }
    {
        auto const result = bp::parse("//1,2,3", failing, bp::ws);
        auto const expected = std::vector<int>{0, 1, 2, 3};
        BOOST_TEST(result.has_value());
        bool const equal = std::equal(
            result->begin(), result->end(), expected.begin(), expected.end());
        BOOST_TEST(equal);
        if (!equal) {
            std::cout << "contents of *result:\n";
            for (auto x : *result) {
                std::cout << x << '\n';
            }
            std::cout << '\n';
        }
    }
}

#if BOOST_PARSER_USE_CONCEPTS
namespace github_issue_268_ {
    namespace bp = boost::parser;
    constexpr bp::rule<struct name, std::string_view> name = "name";
    auto name_def = bp::string_view[bp::lexeme[+(
        bp::lower | bp::upper | bp::digit | bp::char_("_"))]];
    BOOST_PARSER_DEFINE_RULES(name)
    constexpr bp::rule<struct qd_vec, std::vector<double>> qd_vec = "qd_vec";
    auto qd_vec_def = bp::lit("\"") >>
                      bp::double_ %
                          (bp::lit(",") |
                           (bp::lit("\"") >> bp::lit(",") >> bp::lit("\""))) >>
                      bp::lit('\"');
    BOOST_PARSER_DEFINE_RULES(qd_vec)
    struct lu_table_template_1
    {
        std::vector<double> index_1;
        std::string_view variable_1;
    };
    constexpr boost::parser::
        rule<struct lu_table_template_1_tag, lu_table_template_1>
            lu_table_template_1_rule = "lu_table_template_1";
    auto lu_table_template_1_rule_def = (bp::lit("index_1") >> '(' >> qd_vec >>
                                         ')' >> ';') >>
                                        (bp::lit("variable_1") >> ':' >> name >>
                                         ';');
    BOOST_PARSER_DEFINE_RULES(lu_table_template_1_rule)

    constexpr boost::parser::
        rule<struct lu_table_template_1_permut_tag, lu_table_template_1>
            lu_table_template_1_permut_rule = "lu_table_template_1";
    auto lu_table_template_1_permut_rule_def =
        (bp::lit("index_1") >> '(' >> qd_vec >> ')' >> ';') ||
        (bp::lit("variable_1") >> ':' >> name >> ';');
    BOOST_PARSER_DEFINE_RULES(lu_table_template_1_permut_rule)
}
#endif

void github_issue_268()
{
#if BOOST_PARSER_USE_CONCEPTS
    namespace bp = boost::parser;
    using namespace github_issue_268_;
    std::string inputstring = "index_1 ( \"1\" ) ; variable_1 : bier;";

    auto const def_result = bp::parse(
        inputstring, lu_table_template_1_rule_def, bp::blank, bp::trace::off);
    std::cout << "seq_parser generates this type:\n"
              << typeid(def_result.value()).name() << std::endl;
    BOOST_TEST(def_result);

    auto const permut_def_result = bp::parse(
        inputstring,
        lu_table_template_1_permut_rule_def,
        bp::blank,
        bp::trace::off);
    std::cout << "permut_parser generates this type:\n"
              << typeid(permut_def_result.value()).name() << std::endl;
    BOOST_TEST(permut_def_result);

    auto const result = bp::parse(
        inputstring, lu_table_template_1_rule, bp::blank, bp::trace::off);
    std::cout << "seq_parser in rule generates this type:\n"
              << typeid(result.value()).name() << std::endl;
    BOOST_TEST(result);

    auto const permut_result = bp::parse(
        inputstring,
        lu_table_template_1_permut_rule,
        bp::blank,
        bp::trace::off);
    std::cout << "permut_parser generates this type:\n"
              << typeid(permut_result.value()).name() << std::endl;
    BOOST_TEST(permut_result);
#endif
}

void github_issue_279()
{
    namespace bp = boost::parser;

    {
        constexpr auto condition_clause =
            bp::lit(U"while") > bp::lit(U"someexpression") >> bp::attr(true);

        constexpr auto do_statement =
            bp::lexeme[bp::lit(U"do") >> &bp::ws] > -condition_clause > bp::eol;

        auto const result =
            bp::parse(U"do\n", do_statement, bp::blank, bp::trace::off);
        BOOST_TEST(result);
        std::optional<bool> const & condition = result.value();
        BOOST_TEST(!condition.has_value());
    }

    {
        constexpr auto condition_clause =
            bp::lit(U"while") > bp::lit(U"someexpression") >> bp::attr(true);

        constexpr auto do_statement_reverse =
            -condition_clause > bp::lexeme[bp::lit(U"do") >> &bp::ws] > bp::eol;

        auto const result =
            bp::parse(U"do\n", do_statement_reverse, bp::blank, bp::trace::off);
        BOOST_TEST(result);
        std::optional<bool> const & condition = result.value();
        BOOST_TEST(!condition.has_value());
    }
}

namespace github_issue_285_ {
    namespace bp = boost::parser;

    struct Content
    {
        ~Content()
        {
            int setbreakpointhere = 0;
            (void)setbreakpointhere;
        }
    };
    constexpr bp::rule<struct content_tag, std::shared_ptr<Content>> content =
        "content";
    constexpr auto content_action = [](auto & ctx) {
        std::shared_ptr<Content> & result = _val(ctx);
        result = std::make_shared<Content>();
    };
    constexpr auto content_def =
        (bp::lit(U"content") >> bp::eol)[content_action];
    BOOST_PARSER_DEFINE_RULES(content);
}

void github_issue_285()
{
    using namespace github_issue_285_;
    namespace bp = boost::parser;

    constexpr auto prolog = bp::lit(U"prolog") >> bp::eol;

    constexpr auto epilog =
        bp::no_case[bp::lexeme[bp::lit(U"epi") >> bp::lit(U"log")]] >> bp::eol;

    constexpr auto full_parser = prolog >> content >> epilog;

    std::string teststring =
        "prolog\n"
        "content\n"
        "epilog\n";

    // "content" produces a shared_ptr with the result.
    // The "epilog" parser must not delete the result.

    auto const result = bp::parse(teststring, full_parser, bp::blank);
    BOOST_TEST(result);
    BOOST_TEST(result.value().get() != nullptr);
}

void github_pr_290()
{
    namespace bp = boost::parser;

    auto const pTest = bp::lit("TEST:") > -bp::quoted_string;

    auto result = bp::parse("TEST: \"foo\"", pTest, bp::blank);
    BOOST_TEST(result);
    BOOST_TEST(*result == "foo");
}

namespace github_issue_294_ {
    namespace bp = boost::parser;
    struct Foo
    {};
    constexpr bp::rule<struct foo_parser_tag, std::shared_ptr<Foo>> foo_parser =
        "foo_parser";
    constexpr auto foo_parser_action = [](auto & ctx) {
        std::shared_ptr<Foo> & val = _val(ctx);
        val = std::shared_ptr<Foo>(new Foo{});
    };
    constexpr auto foo_parser_def = bp::eps[foo_parser_action];
    struct Bar
    {
        std::shared_ptr<Foo> foo;
    };
    constexpr bp::rule<struct bar_parser_tag, std::shared_ptr<Bar>> bar_parser =
        "bar_parser";
    constexpr auto bar_parser_action = [](auto & ctx) {
        std::shared_ptr<Bar> & val = _val(ctx);
        val = std::shared_ptr<Bar>(new Bar{});
        std::optional<std::shared_ptr<Foo>> & attr = _attr(ctx);
        if (attr) {
            val->foo = attr.value();
        }
    };
    constexpr auto bar_parser_def =
        (bp::lit("(") > -foo_parser > bp::lit(")"))[bar_parser_action];

    BOOST_PARSER_DEFINE_RULES(bar_parser, foo_parser);
}

void github_issue_294()
{
    namespace bp = boost::parser;
    using namespace github_issue_294_;

    bp::parse("()", bar_parser, bp::blank);
}

namespace github_pr_297_ {
    namespace bp = boost::parser;
    constexpr auto bar_required_f = [](auto& ctx) -> bool {
        const bool& argument = bp::_p<0>(ctx);
        return argument;
    };
    constexpr bp::rule<struct foobar_parser_tag> foobar_parser = "foobar_parser";
    constexpr auto foobar_parser_def =
        bp::lit("foo")
        >> bp::switch_(bar_required_f)
            (true, bp::lit("bar"))
            (false, -bp::lit("bar"));
    BOOST_PARSER_DEFINE_RULES(foobar_parser);
}

void github_pr_297()
{
    namespace bp = boost::parser;
    using namespace github_pr_297_;

    {
        const bool bar_required = true;
        const bool result =
            bp::parse("foo bar", foobar_parser.with(bar_required), bp::blank);
        BOOST_TEST(result);
    }
    {
        const bool bar_required = true;
        const bool result =
            bp::parse("foo", foobar_parser.with(bar_required), bp::blank);
        BOOST_TEST(!result);
    }
    {
        const bool bar_required = false;
        const bool result =
            bp::parse("foo bar", foobar_parser.with(bar_required), bp::blank);
        BOOST_TEST(result);
    }
    {
        const bool bar_required = false;
        const bool result =
            bp::parse("foo", foobar_parser.with(bar_required), bp::blank);
        BOOST_TEST(result);
    }
}

namespace github_issue_312_ {
    /*
     * Recursive descent parser for expressions.
     * Supports addition (+), multiplication (*) and
     * parethesized expressions, nothing else.
     *
     * Creates a tree of "evaluatable" objects which
     * own their downstream objects in a unique_ptr
     */

    // base class for all tree nodes
    struct evaluatable
    {
        virtual double evaluate() = 0;
        virtual ~evaluatable() = default;
    };

    namespace bp = boost::parser;

    // top level parser
    constexpr bp::rule<struct expression_tag, std::unique_ptr<evaluatable>> expression_parser = "expression_parser";

    /*
     *      LITERAL EXPRESSION
     */
    struct literal_evaluatable : evaluatable
    {
        explicit literal_evaluatable(double v) : value_(v) {}
        double evaluate() override
        {
            return value_;
        }
        double value_;
    };
    constexpr bp::rule<struct literal_tag, std::unique_ptr<evaluatable>> literal_parser = "literal_parser";
    constexpr auto literal_parser_action = [](auto& ctx) {
        std::unique_ptr<evaluatable>& val = _val(ctx);
        double& parsed_value = _attr(ctx);
        val = std::make_unique<literal_evaluatable>(parsed_value);
    };
    constexpr auto literal_parser_def =
        bp::double_[literal_parser_action];

    /*
     *      PARENTHESIZED EXPRESSION
     */
    struct parenthesized_evaluatable : evaluatable
    {
        explicit parenthesized_evaluatable(std::unique_ptr<evaluatable>&& e) : evaluatable_(std::move(e)) {}
        double evaluate() override
        {
            return evaluatable_->evaluate();
        }
        std::unique_ptr<evaluatable> evaluatable_;
    };
    constexpr bp::rule<struct parenthesized_tag, std::unique_ptr<evaluatable>> parenthesized_parser = "parenthesized_parser";
    constexpr auto parenthesized_action = [](auto& ctx) {
        std::unique_ptr<evaluatable>& val = _val(ctx);
        std::unique_ptr<evaluatable>& attr = _attr(ctx);
        val = std::make_unique<parenthesized_evaluatable>(std::move(attr));
    };
    constexpr auto parenthesized_parser_def =
        (
        bp::lit('(') > expression_parser > bp::lit(')')
        )[parenthesized_action];

    /*
     *      ATOM EXPRESSION
     */
    struct atom_evaluatable : evaluatable
    {
        explicit atom_evaluatable(std::unique_ptr<evaluatable>&& e) : evaluatable_(std::move(e)) {}
        double evaluate() override
        {
            return evaluatable_->evaluate();
        }
        std::unique_ptr<evaluatable> evaluatable_;
    };
    constexpr bp::rule<struct atom_tag, std::unique_ptr<evaluatable>> atom_parser = "atom_parser";
    constexpr auto atom_action = [](auto& ctx) {
        std::unique_ptr<evaluatable>& val = _val(ctx);
        std::unique_ptr<evaluatable>& attr = _attr(ctx);
        val = std::make_unique<atom_evaluatable>(std::move(attr));
    };
    constexpr auto atom_parser_def =
        (
            parenthesized_parser
            |
            literal_parser
        )[atom_action];

    /*
     *      MULTIPLICATION EXPRESSION
     */
    struct multiplication_evaluatable : evaluatable
    {
        multiplication_evaluatable(std::vector<std::unique_ptr<evaluatable>>&& e)
            : evaluatables_(std::move(e))
        {}
        double evaluate() override
        {
            double result = 1;
            for (const auto& e : evaluatables_) {
                result *= e->evaluate();
            }
            return result;
        }
        std::vector<std::unique_ptr<evaluatable>> evaluatables_;
    };
    constexpr bp::rule<struct mult_tag, std::unique_ptr<evaluatable>> mult_parser = "mult_parser";
    constexpr auto mult_parser_action = [](auto& ctx) {
        std::unique_ptr<evaluatable>& val = _val(ctx);
        std::vector<std::unique_ptr<evaluatable>>& operands = _attr(ctx);
        val = std::make_unique<multiplication_evaluatable>(std::move(operands));
    };
    constexpr auto mult_parser_def =
        (atom_parser % bp::lit('*'))[mult_parser_action];

    /*
     *      ADDITION EXPRESSION
     */
    struct addition_evaluatable : evaluatable
    {
        addition_evaluatable(std::vector<std::unique_ptr<evaluatable>>&& e)
            : evaluatables_(std::move(e))
        {}
        double evaluate() override
        {
            double result = 0;
            for (const auto& e : evaluatables_) {
                result += e->evaluate();
            }
            return result;
        }
        std::vector<std::unique_ptr<evaluatable>> evaluatables_;
    };
    constexpr bp::rule<struct add_tag, std::unique_ptr<evaluatable>> add_parser = "add_parser";
    constexpr auto add_parser_action = [](auto& ctx) {
        std::unique_ptr<evaluatable>& val = _val(ctx);
        std::vector<std::unique_ptr<evaluatable>>& operands = _attr(ctx);
        val = std::make_unique<addition_evaluatable>(std::move(operands));
    };
    constexpr auto add_parser_def =
        (mult_parser % bp::lit('+'))[add_parser_action];

    constexpr auto expression_parser_action = [](auto& ctx) {
        std::unique_ptr<evaluatable>& val = _val(ctx);
        std::unique_ptr<evaluatable>& attr = _attr(ctx);
        val = std::move(attr);
    };

    /*
     *      EXPRESSION
     */
    constexpr auto expression_parser_def =
        add_parser[expression_parser_action];

    BOOST_PARSER_DEFINE_RULES(
        literal_parser,
        mult_parser,
        add_parser,
        expression_parser,
        parenthesized_parser,
        atom_parser);
}

void github_issue_312()
{
    namespace bp = boost::parser;
    using namespace github_issue_312_;

    auto result = bp::parse("(2 + 3) + 3.1415 * 2", expression_parser, bp::blank);
    BOOST_TEST(result);
    BOOST_TEST(result.value()->evaluate() == (2 + 3) + 3.1415 * 2);

    result = bp::parse("((2*0.1) + 33.) * (2 + 3) + 3.1415 * 2", expression_parser, bp::blank);
    BOOST_TEST(result);
    BOOST_TEST(result.value()->evaluate() == ((2*0.1) + 33.) * (2 + 3) + 3.1415 * 2);
}


int main()
{
    github_issue_36();
    github_issue_50();
    github_issue_52();
    github_issue_78();
    github_issue_90();
    github_issue_125();
    github_issue_209();
    github_issue_223();
    github_issue_248();
    github_issue_268();
    github_issue_279();
    github_issue_285();
    github_pr_290();
    github_issue_294();
    github_pr_297();
    github_issue_312();
    return boost::report_errors();
}
