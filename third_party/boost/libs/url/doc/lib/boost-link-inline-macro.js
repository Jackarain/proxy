/*
    Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    Official repository: https://github.com/boostorg/site-docs
*/

/**
 * Converts a string from camel case or pascal case to snake case.
 * @param {string} str - The input string in camel case or pascal case.
 * @returns {string} The string in snake case.
 */
function toSnakeCase(str) {
    // Replace all uppercase letters with an underscore followed by their lowercase version
    // If the uppercase letter is the first character, only return the lowercase letter
    return str.replace(/[A-Z]/g, (letter, index) => {
        return index === 0 ? letter.toLowerCase() : '_' + letter.toLowerCase();
    }).replace('-', '_');
}

/**
 * Convert a string to PascalCase.
 * @param {string} inputString - The input string to convert.
 * @returns {string} The input string converted to PascalCase.
 */
function toPascalCase(inputString) {
    // Replace all occurrences of separator followed by a character with uppercase version of the character
    // First argument to replace() is a regex to match the separator and character
    // Second argument to replace() is a callback function to transform the matched string
    const pascalCaseString = inputString.replace(/(_|-|\s)(.)/g, function (match, separator, char) {
        return char.toUpperCase();
    });

    // Uppercase the first character of the resulting string
    return pascalCaseString.replace(/(^.)/, function (char) {
        return char.toUpperCase();
    });
}

/**
 * Registers an inline macro named "boost" with the given Asciidoctor registry.
 * This macro creates a link to the corresponding Boost C++ library.
 *
 * @param {Object} registry - The Asciidoctor registry to register the macro with.
 * @throws {Error} If registry is not defined.
 * @example
 * const asciidoctor = require('asciidoctor');
 * const registry = asciidoctor.Extensions.create();
 * registerBoostMacro(registry);
 */
module.exports = function (registry) {
    // Make sure registry is defined
    if (!registry) {
        throw new Error('registry must be defined');
    }

    /**
     * Processes the "boost" inline macro.
     * If the "title" attribute is specified, use it as the link text.
     * Otherwise, use the capitalized target as the link text.
     * The link URL is constructed based on the snake_case version of the target.
     *
     * @param {Object} parent - The parent node of the inline macro.
     * @param {string} target - The target of the inline macro.
     * @param {Object} attr - The attributes of the inline macro.
     * @returns {Object} An inline node representing the link.
     */
    registry.inlineMacro('boost', function () {
        const self = this;
        self.process(function (parent, target, attr) {
            let title = attr.$positional ? attr.$positional[0] : `Boost.${toPascalCase(target)}`;
            let is_tool = ['auto_index', 'bcp', 'boostbook', 'boostdep', 'boost_install', 'build', 'check_build', 'cmake', 'docca', 'inspect', 'litre', 'quickbook'].includes(toSnakeCase(target));
            let text = `https://www.boost.org/${is_tool ? 'tools' : 'libs'}/${toSnakeCase(target)}[${title}]`;
            return self.createInline(parent, 'quoted', text);
        });
    });
}

