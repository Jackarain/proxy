// library

struct Matrix {
    virtual ~Matrix() = default;
};

struct SquareMatrix : Matrix {};
struct SymmetricMatrix : Matrix {};
struct DiagonalMatrix : SymmetricMatrix {};

// application

#include <array>
#include <iostream>
#include <memory>

#include <boost/openmethod.hpp>
#include <boost/openmethod/initialize.hpp>

using boost::openmethod::virtual_ptr;

BOOST_OPENMETHOD_CLASSES(Matrix, SquareMatrix, SymmetricMatrix, DiagonalMatrix);

BOOST_OPENMETHOD(to_json, (virtual_ptr<const Matrix>, std::ostream& os), void);

BOOST_OPENMETHOD_OVERRIDE(to_json, (virtual_ptr<const SquareMatrix>, std::ostream& os), void) {
    os << "all the elements\n";
}

BOOST_OPENMETHOD_OVERRIDE(to_json, (virtual_ptr<const SymmetricMatrix>, std::ostream& os), void) {
    os << "elements above and including the diagonal\n";
}

BOOST_OPENMETHOD_OVERRIDE(to_json, (virtual_ptr<const DiagonalMatrix>, std::ostream& os), void) {
    os << "just the diagonal\n";
}

int main() {
    std::array<std::unique_ptr<Matrix>, 3> matrices = {
        std::make_unique<SquareMatrix>(),
        std::make_unique<SymmetricMatrix>(),
        std::make_unique<DiagonalMatrix>(),
    };

    boost::openmethod::initialize();

    for (const auto& m : matrices) {
        to_json(*m, std::cout);
    }
}

// output:
// all the elements
// elements above and including the diagonal
// just the diagonal
